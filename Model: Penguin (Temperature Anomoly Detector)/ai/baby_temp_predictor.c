#include "baby_temp_predictor.h"

float baby_temp_predictor_forward(const baby_temp_predictor_t *model,
                                  const float input[BABY_TEMP_PREDICTOR_INPUTS])
{
    float result = model->bias;
    for (unsigned i = 0U; i < BABY_TEMP_PREDICTOR_INPUTS; ++i) result += model->weights[i] * input[i];
    return result;
}

float baby_temp_predictor_error(const baby_temp_predictor_t *model,
                                const float input[BABY_TEMP_PREDICTOR_INPUTS],
                                float actual)
{
    float difference = actual - baby_temp_predictor_forward(model, input);
    return difference < 0.0f ? -difference : difference;
}
