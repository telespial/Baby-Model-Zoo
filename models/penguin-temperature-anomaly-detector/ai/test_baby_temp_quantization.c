#include "baby_temp_service.h"

#include <assert.h>
#include <stdio.h>

static unsigned s_seed = 19U;

static float one_count_sensor_noise(void)
{
    s_seed = (s_seed * 1664525U) + 1013904223U;
    return 21.0f + (float)((s_seed >> 30U) & 1U) * 0.0625f;
}

int main(void)
{
    baby_temp_status_t status;
    baby_temp_service_init();

    /* A perfectly flat baseline followed by ordinary one-count quantization
       previously caused repeated false anomalies after training completed. */
    for (unsigned i = 0U; i < BABY_TEMP_TRAINING_SAMPLES_DEFAULT; ++i)
    {
        baby_temp_service_add_sample(21.0f);
    }
    for (unsigned i = 0U; i < 500U; ++i)
    {
        baby_temp_service_add_sample(one_count_sensor_noise());
        assert(baby_temp_service_get_status(&status));
        assert(status.state != BABY_TEMP_ANOMALY);
        assert(status.anomalyScore == 0.0f);
    }

    baby_temp_service_add_sample(22.0f);
    assert(baby_temp_service_get_status(&status));
    assert(status.state == BABY_TEMP_ANOMALY);

    puts("baby_temp_quantization: PASS");
    return 0;
}
