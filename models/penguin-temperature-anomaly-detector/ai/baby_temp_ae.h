#ifndef BABY_TEMP_AE_H
#define BABY_TEMP_AE_H

#include <stddef.h>

#define BABY_TEMP_WINDOW 16U
#define BABY_TEMP_H1 8U
#define BABY_TEMP_LATENT 4U
#define BABY_TEMP_H3 8U

typedef struct {
    float w1[BABY_TEMP_WINDOW][BABY_TEMP_H1]; float b1[BABY_TEMP_H1];
    float w2[BABY_TEMP_H1][BABY_TEMP_LATENT]; float b2[BABY_TEMP_LATENT];
    float w3[BABY_TEMP_LATENT][BABY_TEMP_H3]; float b3[BABY_TEMP_H3];
    float w4[BABY_TEMP_H3][BABY_TEMP_WINDOW]; float b4[BABY_TEMP_WINDOW];
} baby_temp_ae_params_t;

typedef struct {
    float h1[BABY_TEMP_H1]; float h2[BABY_TEMP_LATENT];
    float h3[BABY_TEMP_H3]; float output[BABY_TEMP_WINDOW];
} baby_temp_ae_workspace_t;

void baby_temp_ae_forward(const baby_temp_ae_params_t *params,
                          const float input[BABY_TEMP_WINDOW],
                          baby_temp_ae_workspace_t *workspace);
float baby_temp_ae_mse(const float actual[BABY_TEMP_WINDOW],
                       const float predicted[BABY_TEMP_WINDOW]);

#endif
