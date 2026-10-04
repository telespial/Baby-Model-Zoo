#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "baby_temp_predictor.h"

int main(void)
{
    baby_temp_predictor_t model = {0};
    float input[BABY_TEMP_PREDICTOR_INPUTS] = {0};
    model.bias = 21.0f;
    assert(fabsf(baby_temp_predictor_forward(&model, input) - 21.0f) < 0.00001f);
    assert(fabsf(baby_temp_predictor_error(&model, input, 22.5f) - 1.5f) < 0.00001f);
    puts("baby_temp_predictor: PASS");
    return 0;
}
