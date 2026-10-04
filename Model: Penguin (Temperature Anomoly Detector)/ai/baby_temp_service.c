#include "baby_temp_service.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "baby_temp_ae.h"
#include "baby_temp_persistence.h"
#include "baby_temp_predictor.h"

#define AI_WINDOW 16U
#define AI_SENSOR_RESOLUTION_C 0.0625f
#define AI_MIN_ANOMALY_THRESHOLD 0.20f
#define AI_MIN_WATCH_THRESHOLD 0.08f

static baby_temp_ae_params_t s_params;
static baby_temp_ae_workspace_t s_workspace;
static baby_temp_predictor_t s_predictor;
static float s_ring[AI_WINDOW];
static float s_input[AI_WINDOW];
static unsigned s_ringCount;
static unsigned s_ringHead;
static float s_center;
static float s_scale;
static float s_windowRangeC;
static baby_temp_status_t s_status;
static float s_errorMean;
static float s_errorM2;
static float s_watchThreshold;
static float s_anomalyThreshold;
static unsigned s_trainingSamples = BABY_TEMP_TRAINING_SAMPLES_DEFAULT;

static float deterministic_weight(unsigned *seed)
{
    *seed = (*seed * 1664525U) + 1013904223U;
    return (((float)(*seed & 0xFFFFU) / 65535.0f) * 0.10f) - 0.05f;
}

static void initialize_weights(void)
{
    unsigned seed = 7U;
    memset(&s_params, 0, sizeof(s_params));
    memset(&s_predictor, 0, sizeof(s_predictor));
    for (size_t i = 0; i < AI_WINDOW; ++i)
        for (size_t j = 0; j < BABY_TEMP_H1; ++j) s_params.w1[i][j] = deterministic_weight(&seed);
    for (size_t i = 0; i < BABY_TEMP_H1; ++i)
        for (size_t j = 0; j < BABY_TEMP_LATENT; ++j) s_params.w2[i][j] = deterministic_weight(&seed);
    for (size_t i = 0; i < BABY_TEMP_LATENT; ++i)
        for (size_t j = 0; j < BABY_TEMP_H3; ++j) s_params.w3[i][j] = deterministic_weight(&seed);
    for (size_t i = 0; i < BABY_TEMP_H3; ++i)
        for (size_t j = 0; j < AI_WINDOW; ++j) s_params.w4[i][j] = deterministic_weight(&seed);
    for (size_t i = 0; i < BABY_TEMP_PREDICTOR_INPUTS; ++i) s_predictor.weights[i] = deterministic_weight(&seed);
}

static void make_window(void)
{
    float minimum = s_ring[s_ringHead];
    float maximum = minimum;
    s_center = 0.0f;
    for (size_t i = 0; i < AI_WINDOW; ++i) {
        const float sample = s_ring[(s_ringHead + i) % AI_WINDOW];
        s_center += sample;
        if (sample < minimum) minimum = sample;
        if (sample > maximum) maximum = sample;
    }
    s_center /= (float)AI_WINDOW;
    s_windowRangeC = maximum - minimum;
    s_scale = 0.0f;
    for (size_t i = 0; i < AI_WINDOW; ++i) {
        const float difference = s_ring[(s_ringHead + i) % AI_WINDOW] - s_center;
        s_scale += difference * difference;
    }
    s_scale = sqrtf(s_scale / (float)AI_WINDOW);
    if (s_scale < 0.05f) s_scale = 0.05f;
    for (size_t i = 0; i < AI_WINDOW; ++i)
        s_input[i] = (s_ring[(s_ringHead + i) % AI_WINDOW] - s_center) / s_scale;
}

static void train_autoencoder(void)
{
    /* Full 16-8-4-8-16 backpropagation. All buffers are fixed-size so the
       MCU path remains deterministic and heap-free. */
    const float learningRate = 0.003f;
    float d4[BABY_TEMP_WINDOW], d3[BABY_TEMP_H3], d2[BABY_TEMP_LATENT], d1[BABY_TEMP_H1];
    baby_temp_ae_forward(&s_params, s_input, &s_workspace);
    for (size_t j = 0; j < AI_WINDOW; ++j) {
        d4[j] = 2.0f * (s_workspace.output[j] - s_input[j]) / (float)AI_WINDOW;
        for (size_t i = 0; i < BABY_TEMP_H3; ++i) s_params.w4[i][j] -= learningRate * d4[j] * s_workspace.h3[i];
        s_params.b4[j] -= learningRate * d4[j];
    }
    for (size_t i = 0; i < BABY_TEMP_H3; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < AI_WINDOW; ++j) sum += d4[j] * s_params.w4[i][j];
        d3[i] = s_workspace.h3[i] > 0.0f ? sum : 0.0f;
        for (size_t j = 0; j < BABY_TEMP_LATENT; ++j) s_params.w3[j][i] -= learningRate * d3[i] * s_workspace.h2[j];
        s_params.b3[i] -= learningRate * d3[i];
    }
    for (size_t i = 0; i < BABY_TEMP_LATENT; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < BABY_TEMP_H3; ++j) sum += d3[j] * s_params.w3[i][j];
        d2[i] = s_workspace.h2[i] > 0.0f ? sum : 0.0f;
        for (size_t j = 0; j < BABY_TEMP_LATENT; ++j) s_params.w2[i][j] -= learningRate * d2[j] * s_workspace.h1[i];
        s_params.b2[i] -= learningRate * d2[i];
    }
    for (size_t i = 0; i < BABY_TEMP_H1; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < BABY_TEMP_LATENT; ++j) sum += d2[j] * s_params.w2[i][j];
        d1[i] = s_workspace.h1[i] > 0.0f ? sum : 0.0f;
        for (size_t j = 0; j < BABY_TEMP_WINDOW; ++j) s_params.w1[j][i] -= learningRate * d1[i] * s_input[j];
        s_params.b1[i] -= learningRate * d1[i];
    }
    s_status.trainingSteps++;
}

static void train_predictor(void)
{
    const float predictorLearningRate = 0.01f;
    const float predictorError = s_input[AI_WINDOW - 1U] - baby_temp_predictor_forward(&s_predictor, s_input);
    s_predictor.bias += predictorLearningRate * predictorError;
    for (size_t i = 0; i < BABY_TEMP_PREDICTOR_INPUTS; ++i)
        s_predictor.weights[i] += predictorLearningRate * predictorError * s_input[i];
}

static void learn_baseline_error(void)
{
    baby_temp_ae_forward(&s_params, s_input, &s_workspace);
    const float reconstruction = baby_temp_ae_mse(s_input, s_workspace.output);
    const float prediction = baby_temp_predictor_error(&s_predictor, s_input, s_input[AI_WINDOW - 1U]);
    const float error = (reconstruction + prediction) * 0.5f;
    const float count = (float)s_status.trainingSteps;
    const float delta = error - s_errorMean;
    s_errorMean += delta / count;
    s_errorM2 += delta * (error - s_errorMean);
    const float deviation = count > 1.0f ? sqrtf(s_errorM2 / (count - 1.0f)) : 0.0f;
    s_watchThreshold = fmaxf(AI_MIN_WATCH_THRESHOLD, s_errorMean + (2.0f * deviation));
    s_anomalyThreshold = fmaxf(AI_MIN_ANOMALY_THRESHOLD, s_errorMean + (3.0f * deviation));
}

static void guarded_adaptation(void)
{
    /* Continue learning only from windows that are below the learned watch
       threshold. Suspect windows are scored and retained, never used as
       self-training targets. */
    if (s_status.anomalyScore < s_watchThreshold) {
        train_autoencoder();
        train_predictor();
    }
}

void baby_temp_service_init(void)
{
    memset(&s_status, 0, sizeof(s_status));
    memset(s_ring, 0, sizeof(s_ring));
    s_ringCount = 0U;
    s_ringHead = 0U;
    s_errorMean = 0.0f;
    s_errorM2 = 0.0f;
    s_watchThreshold = AI_MIN_WATCH_THRESHOLD;
    s_anomalyThreshold = AI_MIN_ANOMALY_THRESHOLD;
    initialize_weights();
    s_status.state = BABY_TEMP_UNTRAINED;
}

bool baby_temp_service_set_training_samples(unsigned sampleCount)
{
    if ((sampleCount < BABY_TEMP_TRAINING_SAMPLES_MIN) ||
        (sampleCount > BABY_TEMP_TRAINING_SAMPLES_MAX))
    {
        return false;
    }
    if (sampleCount != s_trainingSamples)
    {
        s_trainingSamples = sampleCount;
        baby_temp_service_init();
    }
    return true;
}

unsigned baby_temp_service_get_training_samples(void)
{
    return s_trainingSamples;
}

void baby_temp_service_add_sample(float temperatureC)
{
    s_ring[s_ringHead] = temperatureC;
    s_ringHead = (s_ringHead + 1U) % AI_WINDOW;
    if (s_ringCount < AI_WINDOW) s_ringCount++;
    s_status.rawC = temperatureC;
    s_status.sampleCount++;
    if (s_ringCount < AI_WINDOW) { s_status.state = BABY_TEMP_COLLECTING; return; }

    make_window();
    if (s_status.sampleCount <= s_trainingSamples) {
        s_status.state = BABY_TEMP_TRAINING;
        train_autoencoder();
        train_predictor();
        learn_baseline_error();
        return;
    }
    /* The P3T1755 driver reports temperatures in exact 0.0625 C steps. A
       window spanning no more than one conversion count contains no
       resolvable temperature excursion, so do not amplify that quantization
       into an anomaly through per-window normalization. */
    if (s_windowRangeC <= AI_SENSOR_RESOLUTION_C) {
        s_status.reconstructedC = temperatureC;
        s_status.reconstructionError = 0.0f;
        s_status.predictionError = 0.0f;
        s_status.anomalyScore = 0.0f;
        s_status.state = BABY_TEMP_READY;
        return;
    }
    baby_temp_ae_forward(&s_params, s_input, &s_workspace);
    s_status.reconstructedC = (s_workspace.output[AI_WINDOW - 1U] * s_scale) + s_center;
    s_status.reconstructionError = baby_temp_ae_mse(s_input, s_workspace.output);
    s_status.predictionError = baby_temp_predictor_error(&s_predictor, s_input, s_input[AI_WINDOW - 1U]);
    s_status.anomalyScore = (s_status.reconstructionError + s_status.predictionError) * 0.5f;
    s_status.state = s_status.anomalyScore >= s_anomalyThreshold ? BABY_TEMP_ANOMALY :
                     (s_status.anomalyScore >= s_watchThreshold ? BABY_TEMP_WATCH : BABY_TEMP_READY);
    guarded_adaptation();
}

bool baby_temp_service_get_status(baby_temp_status_t *status)
{
    if (status == NULL) return false;
    *status = s_status;
    return true;
}

void baby_temp_service_reset(void)
{
    baby_temp_service_init();
}

size_t baby_temp_service_save(uint8_t *buffer, size_t capacity)
{
    return baby_temp_model_pack(buffer, capacity, &s_params, &s_predictor, s_status.trainingSteps);
}

bool baby_temp_service_restore(const uint8_t *buffer, size_t length)
{
    uint32_t trainingSteps = 0U;
    if (baby_temp_model_unpack(buffer, length, &s_params, &s_predictor, &trainingSteps) != 0) return false;
    s_status.trainingSteps = trainingSteps;
    s_status.state = BABY_TEMP_READY;
    return true;
}
