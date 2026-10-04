#ifndef BABY_TEMP_PERSISTENCE_H
#define BABY_TEMP_PERSISTENCE_H

#include <stddef.h>
#include <stdint.h>

#include "baby_temp_ae.h"
#include "baby_temp_predictor.h"

#define BABY_TEMP_MODEL_MAGIC 0x425A544DU
#define BABY_TEMP_MODEL_VERSION 1U
#define BABY_TEMP_MODEL_ARCHITECTURE 0x0102U
#define BABY_TEMP_MODEL_MAX_BYTES 4096U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t architecture;
    uint32_t payloadBytes;
    uint32_t trainingSteps;
    uint32_t checksum;
} baby_temp_model_header_t;

size_t baby_temp_model_pack(uint8_t *buffer, size_t capacity,
                            const baby_temp_ae_params_t *autoencoder,
                            const baby_temp_predictor_t *predictor,
                            uint32_t trainingSteps);
int baby_temp_model_unpack(const uint8_t *buffer, size_t length,
                           baby_temp_ae_params_t *autoencoder,
                           baby_temp_predictor_t *predictor,
                           uint32_t *trainingSteps);

#endif
