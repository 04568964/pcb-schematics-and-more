#ifndef WS2812_H
#define WS2812_H
#include "stm32g0xx_hal.h"
#include <stdint.h>
#define WS2812_LED_COUNT 4

void ws2812_init(TIM_HandleTypeDef *htim);
void ws2812_set_pixel(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);
void ws2812_show(void);
uint8_t ws2812_is_busy(void);
#endif