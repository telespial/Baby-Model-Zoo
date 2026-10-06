#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "baby_temp_persistence.h"

int main(void)
{
    baby_temp_ae_params_t source = {0}, restored = {0};
    baby_temp_predictor_t predictor = {0}, predictorRestored = {0};
    uint8_t buffer[BABY_TEMP_MODEL_MAX_BYTES] = {0};
    uint32_t steps = 0U;
    source.w1[0][0] = 1.25f; predictor.bias = 22.5f;
    size_t length = baby_temp_model_pack(buffer, sizeof(buffer), &source, &predictor, 42U);
    assert(length > sizeof(baby_temp_model_header_t));
    assert(baby_temp_model_unpack(buffer, length, &restored, &predictorRestored, &steps) == 0);
    assert(restored.w1[0][0] == source.w1[0][0] && predictorRestored.bias == predictor.bias && steps == 42U);
    buffer[length - 1U] ^= 1U;
    assert(baby_temp_model_unpack(buffer, length, &restored, &predictorRestored, &steps) != 0);
    puts("baby_temp_persistence: PASS");
    return 0;
}
