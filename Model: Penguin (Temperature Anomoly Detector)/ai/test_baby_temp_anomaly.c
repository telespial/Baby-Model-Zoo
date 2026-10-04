#include "baby_temp_service.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    baby_temp_status_t status;
    baby_temp_service_init();
    for (unsigned i = 0; i < 140U; ++i) baby_temp_service_add_sample(21.0f + (float)(i % 3U) * 0.02f);
    for (unsigned i = 0; i < 15U; ++i) baby_temp_service_add_sample(21.0f);
    baby_temp_service_add_sample(42.0f);
    assert(baby_temp_service_get_status(&status));
    assert(status.state == BABY_TEMP_ANOMALY);
    assert(status.anomalyScore >= 0.20f);
    puts("baby_temp_anomaly: PASS");
    return 0;
}
