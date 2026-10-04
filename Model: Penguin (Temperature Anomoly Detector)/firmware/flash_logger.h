#ifndef FLASH_LOGGER_H
#define FLASH_LOGGER_H

#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"

#define FLASH_LOGGER_CAPACITY 1024U

typedef struct
{
    uint32_t sequence;
    uint32_t epochSeconds;
    int16_t temperatureCentiC;
} flash_logger_sample_t;

typedef void (*flash_logger_visit_t)(const flash_logger_sample_t *sample, void *context);

status_t FlashLogger_Init(void);
status_t FlashLogger_Append(uint32_t epochSeconds, int16_t temperatureCentiC);
status_t FlashLogger_Clear(void);
uint32_t FlashLogger_Count(void);
void FlashLogger_ForEach(flash_logger_visit_t visitor, void *context);

#endif
