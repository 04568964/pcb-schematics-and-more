#ifndef STM32G0XX_HAL_H
#define STM32G0XX_HAL_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    HAL_OK = 0,
    HAL_ERROR = 1
} HAL_StatusTypeDef;

typedef struct {
    uint32_t dummy;
} TIM_TypeDef;

extern TIM_TypeDef SIM_TIM3_INSTANCE;
#define TIM3 (&SIM_TIM3_INSTANCE)
#define TIM_CHANNEL_1 0U

typedef struct {
    TIM_TypeDef *Instance;
} TIM_HandleTypeDef;

#define __HAL_TIM_SET_COUNTER(htim, val) ((void)(htim), (void)(val))

HAL_StatusTypeDef HAL_TIM_PWM_Start_DMA(TIM_HandleTypeDef *htim, uint32_t Channel, uint32_t *pData, uint16_t Length);
HAL_StatusTypeDef HAL_TIM_PWM_Stop_DMA(TIM_HandleTypeDef *htim, uint32_t Channel);
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim);

#endif