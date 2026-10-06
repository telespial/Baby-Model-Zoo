#ifndef BABY_TEMP_PREDICTOR_H
#define BABY_TEMP_PREDICTOR_H

#define BABY_TEMP_PREDICTOR_INPUTS 8U

typedef struct {
    float weights[BABY_TEMP_PREDICTOR_INPUTS];
    float bias;
} baby_temp_predictor_t;

float baby_temp_predictor_forward(const baby_temp_predictor_t *model,
                                  const float input[BABY_TEMP_PREDICTOR_INPUTS]);
float baby_temp_predictor_error(const baby_temp_predictor_t *model,
                                const float input[BABY_TEMP_PREDICTOR_INPUTS],
                                float actual);

#endif
