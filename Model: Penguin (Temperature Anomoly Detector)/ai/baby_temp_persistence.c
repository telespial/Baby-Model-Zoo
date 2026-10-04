#include "baby_temp_persistence.h"

#include <string.h>

static uint32_t model_checksum(const uint8_t *buffer, size_t totalBytes)
{
    baby_temp_model_header_t header;
    memcpy(&header, buffer, sizeof(header));
    header.checksum = 0U;
    uint32_t value = 2166136261U;
    for (size_t i = 0; i < sizeof(header); ++i) value = (value ^ ((const uint8_t *)&header)[i]) * 16777619U;
    for (size_t i = sizeof(header); i < totalBytes; ++i) value = (value ^ buffer[i]) * 16777619U;
    return value;
}

size_t baby_temp_model_pack(uint8_t *buffer, size_t capacity,
                            const baby_temp_ae_params_t *autoencoder,
                            const baby_temp_predictor_t *predictor,
                            uint32_t trainingSteps)
{
    const size_t payloadBytes = sizeof(*autoencoder) + sizeof(*predictor);
    const size_t totalBytes = sizeof(baby_temp_model_header_t) + payloadBytes;
    if (buffer == NULL || autoencoder == NULL || predictor == NULL || capacity < totalBytes) return 0U;
    baby_temp_model_header_t header = {
        .magic = BABY_TEMP_MODEL_MAGIC, .version = BABY_TEMP_MODEL_VERSION,
        .architecture = BABY_TEMP_MODEL_ARCHITECTURE, .payloadBytes = (uint32_t)payloadBytes,
        .trainingSteps = trainingSteps, .checksum = 0U,
    };
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), autoencoder, sizeof(*autoencoder));
    memcpy(buffer + sizeof(header) + sizeof(*autoencoder), predictor, sizeof(*predictor));
    ((baby_temp_model_header_t *)buffer)->checksum = model_checksum(buffer, totalBytes);
    return totalBytes;
}

int baby_temp_model_unpack(const uint8_t *buffer, size_t length,
                           baby_temp_ae_params_t *autoencoder,
                           baby_temp_predictor_t *predictor,
                           uint32_t *trainingSteps)
{
    baby_temp_model_header_t header;
    const size_t payloadBytes = sizeof(*autoencoder) + sizeof(*predictor);
    if (buffer == NULL || autoencoder == NULL || predictor == NULL || trainingSteps == NULL ||
        length < sizeof(header) + payloadBytes) return -1;
    memcpy(&header, buffer, sizeof(header));
    if (header.magic != BABY_TEMP_MODEL_MAGIC || header.version != BABY_TEMP_MODEL_VERSION ||
        header.architecture != BABY_TEMP_MODEL_ARCHITECTURE || header.payloadBytes != payloadBytes ||
        header.checksum != model_checksum(buffer, sizeof(header) + payloadBytes)) return -2;
    memcpy(autoencoder, buffer + sizeof(header), sizeof(*autoencoder));
    memcpy(predictor, buffer + sizeof(header) + sizeof(*autoencoder), sizeof(*predictor));
    *trainingSteps = header.trainingSteps;
    return 0;
}
