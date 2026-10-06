#ifndef BABY_TEMP_SERVICE_H
#define BABY_TEMP_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BABY_TEMP_TRAINING_SAMPLES_MIN     32U
#define BABY_TEMP_TRAINING_SAMPLES_MAX     256U
#define BABY_TEMP_TRAINING_SAMPLES_DEFAULT 128U

typedef enum {
    BABY_TEMP_UNTRAINED = 0,
    BABY_TEMP_COLLECTING,
    BABY_TEMP_TRAINING,
    BABY_TEMP_READY,
    BABY_TEMP_WATCH,
    BABY_TEMP_ANOMALY,
    BABY_TEMP_ERROR
} baby_temp_state_t;

typedef struct {
    baby_temp_state_t state;
    float rawC;
    float reconstructedC;
    float reconstructionError;
    float anomalyScore;
    float predictionError;
    unsigned sampleCount;
    unsigned trainingSteps;
} baby_temp_status_t;

void baby_temp_service_init(void);
bool baby_temp_service_set_training_samples(unsigned sampleCount);
unsigned baby_temp_service_get_training_samples(void);
void baby_temp_service_add_sample(float temperatureC);
bool baby_temp_service_get_status(baby_temp_status_t *status);
void baby_temp_service_reset(void);
size_t baby_temp_service_save(uint8_t *buffer, size_t capacity);
bool baby_temp_service_restore(const uint8_t *buffer, size_t length);

#endif
