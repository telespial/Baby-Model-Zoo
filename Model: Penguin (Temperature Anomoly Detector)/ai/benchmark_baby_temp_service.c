#include "baby_temp_service.h"

#include <stdio.h>
#include <time.h>

int main(void)
{
    struct timespec start, end;
    baby_temp_service_init();
    for (unsigned i = 0; i < 32U; ++i) baby_temp_service_add_sample(21.0f);
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (unsigned i = 0; i < 10000U; ++i) baby_temp_service_add_sample(21.0f + (float)(i % 5U) * 0.01f);
    clock_gettime(CLOCK_MONOTONIC, &end);
    const double elapsedUs = ((double)(end.tv_sec - start.tv_sec) * 1000000.0) +
                             ((double)(end.tv_nsec - start.tv_nsec) / 1000.0);
    printf("baby_temp_service host timing: %.3f us/sample (not MCU timing)\n", elapsedUs / 10000.0);
    return 0;
}
