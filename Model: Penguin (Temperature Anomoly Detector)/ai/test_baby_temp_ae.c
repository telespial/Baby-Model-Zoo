#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "baby_temp_ae.h"

int main(void)
{
    baby_temp_ae_params_t params = {0};
    baby_temp_ae_workspace_t workspace = {0};
    float input[BABY_TEMP_WINDOW] = {0};
    params.b4[0] = 2.0f;
    baby_temp_ae_forward(&params, input, &workspace);
    assert(fabsf(workspace.output[0] - 2.0f) < 0.00001f);
    assert(fabsf(baby_temp_ae_mse(input, workspace.output) - 0.25f) < 0.00001f);
    puts("baby_temp_ae C forward/MSE: PASS");
    return 0;
}
