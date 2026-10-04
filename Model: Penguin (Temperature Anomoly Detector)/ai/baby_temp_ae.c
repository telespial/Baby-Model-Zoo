#include "baby_temp_ae.h"

static float relu(float value) { return value > 0.0f ? value : 0.0f; }

void baby_temp_ae_forward(const baby_temp_ae_params_t *p,
                          const float input[BABY_TEMP_WINDOW],
                          baby_temp_ae_workspace_t *w)
{
    for (size_t j = 0; j < BABY_TEMP_H1; ++j) {
        float sum = p->b1[j];
        for (size_t i = 0; i < BABY_TEMP_WINDOW; ++i) sum += input[i] * p->w1[i][j];
        w->h1[j] = relu(sum);
    }
    for (size_t j = 0; j < BABY_TEMP_LATENT; ++j) {
        float sum = p->b2[j];
        for (size_t i = 0; i < BABY_TEMP_H1; ++i) sum += w->h1[i] * p->w2[i][j];
        w->h2[j] = relu(sum);
    }
    for (size_t j = 0; j < BABY_TEMP_H3; ++j) {
        float sum = p->b3[j];
        for (size_t i = 0; i < BABY_TEMP_LATENT; ++i) sum += w->h2[i] * p->w3[i][j];
        w->h3[j] = relu(sum);
    }
    for (size_t j = 0; j < BABY_TEMP_WINDOW; ++j) {
        float sum = p->b4[j];
        for (size_t i = 0; i < BABY_TEMP_H3; ++i) sum += w->h3[i] * p->w4[i][j];
        w->output[j] = sum;
    }
}

float baby_temp_ae_mse(const float actual[BABY_TEMP_WINDOW],
                       const float predicted[BABY_TEMP_WINDOW])
{
    float sum = 0.0f;
    for (size_t i = 0; i < BABY_TEMP_WINDOW; ++i) {
        const float difference = actual[i] - predicted[i];
        sum += difference * difference;
    }
    return sum / (float)BABY_TEMP_WINDOW;
}
