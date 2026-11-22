#include "ws2812.h"
#include <stddef.h>
#define PWM_HI 51
#define PWM_LO 24
#define RESET_SLOTS 240
#define PWM_BUF_LEN (WS2812_LED_COUNT * 24 + RESET_SLOTS)

static TIM_HandleTypeDef *led_htim = NULL;
static uint16_t pwm_buf[PWM_BUF_LEN];
static volatile uint8_t dma_busy = 0;

void ws2812_set_pixel(uint8_t idx, uint8_t r, uint8_t g, uint8_t b){
    if (idx>=WS2812_LED_COUNT || dma_busy) return;
    uint16_t base = idx * 24;
    for (uint8_t i = 0; i < 8; i++){
        pwm_buf[base + i] = (g & (1U << (7U - i))) ? PWM_HI : PWM_LO;
        pwm_buf[base + i + 8] = (r & (1U << (7U - i))) ? PWM_HI : PWM_LO;
        pwm_buf[base + i + 16] = (b & (1U << (7U - i))) ? PWM_HI : PWM_LO;
    }
}

void ws2812_show(void){
    __HAL_TIM_SET_COUNTER(led_htim, 0);
    if (dma_busy || led_htim == NULL) return;
    dma_busy = 1;
    if (HAL_TIM_PWM_Start_DMA(led_htim,TIM_CHANNEL_1,(uint32_t *)pwm_buf,PWM_BUF_LEN) != HAL_OK){
        dma_busy = 0;
    }
}

uint8_t ws2812_is_busy(void){
    return dma_busy;
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim){
    if (htim->Instance == TIM3){
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
        dma_busy = 0;
    }
}
void ws2812_init(TIM_HandleTypeDef *htim){
    led_htim = htim;
    for(uint16_t i=0; i<PWM_BUF_LEN; i++){
        pwm_buf[i] = 0;
    }
    dma_busy = 0;
    for(uint8_t i=0; i<WS2812_LED_COUNT; i++){
        ws2812_set_pixel(i, 0, 0, 0);
    }
}