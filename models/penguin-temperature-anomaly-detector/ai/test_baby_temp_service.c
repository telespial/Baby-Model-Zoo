#include "baby_temp_service.h"

#include <assert.h>
#include <stdio.h>

#include "baby_temp_persistence.h"

int main(void)
{
    uint8_t buffer[BABY_TEMP_MODEL_MAX_BYTES];
    baby_temp_status_t status;
    baby_temp_service_init();
    assert(baby_temp_service_get_training_samples() == BABY_TEMP_TRAINING_SAMPLES_DEFAULT);
    assert(!baby_temp_service_set_training_samples(BABY_TEMP_TRAINING_SAMPLES_MIN - 1U));
    assert(!baby_temp_service_set_training_samples(BABY_TEMP_TRAINING_SAMPLES_MAX + 1U));
    assert(baby_temp_service_set_training_samples(64U));
    assert(baby_temp_service_get_training_samples() == 64U);
    for (unsigned i = 0; i < 140U; ++i) baby_temp_service_add_sample(21.0f + (float)(i % 3U) * 0.02f);
    assert(baby_temp_service_get_status(&status));
    assert(status.trainingSteps > 0U);
    const size_t length = baby_temp_service_save(buffer, sizeof(buffer));
    assert(length > sizeof(baby_temp_model_header_t));
    baby_temp_service_reset();
    assert(baby_temp_service_restore(buffer, length));
    assert(baby_temp_service_get_status(&status));
    assert(status.trainingSteps > 0U);
    puts("baby_temp_service: PASS");
    return 0;
}
