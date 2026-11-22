#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include "ws2812.h"

#define DARK_TH_HIGH 2400U
#define DARK_TH_LOW 1800U
#define EXPECTED_HI 51U
#define EXPECTED_LO 24U
#define EXPECTED_RESET 240U
#define EXPECTED_LEN (WS2812_LED_COUNT * 24U + EXPECTED_RESET)

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

TIM_TypeDef SIM_TIM3_INSTANCE = {0};

static const uint16_t *captured_ptr = NULL;
static uint16_t captured_len = 0;
static uint32_t dma_stop_count = 0;
static uint8_t sim_auto_complete = 0;

HAL_StatusTypeDef HAL_TIM_PWM_Start_DMA(TIM_HandleTypeDef *htim, uint32_t Channel, uint32_t *pData, uint16_t Length) {
    if (htim == NULL || pData == NULL || Channel != TIM_CHANNEL_1) {
        return HAL_ERROR;
    }
    captured_ptr = (const uint16_t *)pData;
    captured_len = Length;
    if (sim_auto_complete) {
        HAL_TIM_PWM_PulseFinishedCallback(htim);
    }
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Stop_DMA(TIM_HandleTypeDef *htim, uint32_t Channel) {
    (void)htim;
    (void)Channel;
    dma_stop_count++;
    return HAL_OK;
}

static uint8_t decode_byte(const uint16_t *slots) {
    uint8_t val = 0;
    for (uint8_t i = 0; i < 8; i++) {
        assert(slots[i] == EXPECTED_HI || slots[i] == EXPECTED_LO);
        if (slots[i] == EXPECTED_HI) {
            val |= (uint8_t)(1U << (7U - i));
        }
    }
    return val;
}

static void decode_frame(rgb_t out[WS2812_LED_COUNT]) {
    assert(captured_ptr != NULL);
    assert(captured_len == EXPECTED_LEN);

    for (uint8_t i = 0; i < WS2812_LED_COUNT; i++) {
        uint16_t base = (uint16_t)(i * 24U);
        out[i].g = decode_byte(&captured_ptr[base]);
        out[i].r = decode_byte(&captured_ptr[base + 8U]);
        out[i].b = decode_byte(&captured_ptr[base + 16U]);
    }

    for (uint16_t i = WS2812_LED_COUNT * 24U; i < EXPECTED_LEN; i++) {
        assert(captured_ptr[i] == 0U);
    }
}

static uint8_t scale8(uint8_t val, uint8_t scale) {
    return (uint8_t)(((uint16_t)val * scale) >> 8);
}

static uint8_t to_srgb(uint8_t lin) {
    return (uint8_t)(sqrt((double)lin * 255.0));
}

static rgb_t color_wheel(uint8_t h) {
    rgb_t c;
    if (h < 85U) {
        c.r = (uint8_t)(255U - h * 3U);
        c.g = (uint8_t)(h * 3U);
        c.b = 0U;
    } else if (h < 170U) {
        h = (uint8_t)(h - 85U);
        c.r = 0U;
        c.g = (uint8_t)(255U - h * 3U);
        c.b = (uint8_t)(h * 3U);
    } else {
        h = (uint8_t)(h - 170U);
        c.r = (uint8_t)(h * 3U);
        c.g = 0U;
        c.b = (uint8_t)(255U - h * 3U);
    }
    return c;
}

static void run_protocol_tests(TIM_HandleTypeDef *htim) {
    sim_auto_complete = 0;
    dma_stop_count = 0;
    ws2812_init(htim);

    const rgb_t expected[WS2812_LED_COUNT] = {
        {0xAA, 0x55, 0xF0},
        {0x00, 0xFF, 0x01},
        {0x80, 0x00, 0x7F},
        {0x12, 0x34, 0x56}
    };

    for (uint8_t i = 0; i < WS2812_LED_COUNT; i++) {
        ws2812_set_pixel(i, expected[i].r, expected[i].g, expected[i].b);
    }

    ws2812_show();
    assert(ws2812_is_busy() == 1U);

    ws2812_set_pixel(0, 0xFF, 0xFF, 0xFF);

    rgb_t decoded[WS2812_LED_COUNT];
    decode_frame(decoded);
    for (uint8_t i = 0; i < WS2812_LED_COUNT; i++) {
        assert(decoded[i].r == expected[i].r);
        assert(decoded[i].g == expected[i].g);
        assert(decoded[i].b == expected[i].b);
    }

    HAL_TIM_PWM_PulseFinishedCallback(htim);
    assert(ws2812_is_busy() == 0U);
    assert(dma_stop_count == 1U);

    printf("[OK] Protocol test passed: %u slots, T0H=%.1f ns, T1H=%.1f ns, RESET=%.1f us\n",
           captured_len,
           EXPECTED_LO * 15.625,
           EXPECTED_HI * 15.625,
           EXPECTED_RESET * 1.25);
}

static uint16_t fake_adc_profile(uint32_t frame) {
    int16_t noise = (int16_t)(((frame * 37U) % 81U) - 40);
    int32_t base;

    if (frame < 100U) {
        base = 1100;
    } else if (frame < 140U) {
        base = 2100;
    } else if (frame < 280U) {
        base = 3100;
    } else if (frame < 320U) {
        base = 2100;
    } else {
        base = 1100;
    }

    int32_t res = base + noise;
    if (res < 0) res = 0;
    if (res > 4095) res = 4095;
    return (uint16_t)res;
}

static void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int main(void) {
    TIM_HandleTypeDef htim3 = {.Instance = TIM3};

    run_protocol_tests(&htim3);

    sim_auto_complete = 1;
    uint16_t adc_filtered = fake_adc_profile(0);
    uint8_t is_dark = (adc_filtered > DARK_TH_HIGH) ? 1U : 0U;
    uint8_t cur_bright = is_dark ? 55U : 200U;
    int16_t breath = 45;
    int8_t breath_dir = 1;
    uint8_t hue = 0;

    printf("Simulating 400 frames (50 FPS): Bright -> Deadzone -> Dark -> Deadzone -> Bright\n");

    for (uint32_t frame = 0; frame < 400U; frame++) {
        uint16_t raw = fake_adc_profile(frame);
        adc_filtered = (uint16_t)(((uint32_t)adc_filtered * 7U + raw + 4U) >> 3);

        if (adc_filtered > DARK_TH_HIGH) {
            is_dark = 1U;
        } else if (adc_filtered < DARK_TH_LOW) {
            is_dark = 0U;
        }

        uint8_t target_bright = is_dark ? 55U : 200U;
        if (cur_bright + 2U < target_bright) {
            cur_bright += 2U;
        } else if (cur_bright > target_bright + 2U) {
            cur_bright -= 2U;
        } else {
            cur_bright = target_bright;
        }

        uint8_t breath_step = is_dark ? 2U : 5U;
        uint8_t hue_step = is_dark ? 1U : 3U;

        breath += (int16_t)breath_dir * (int16_t)breath_step;
        if (breath >= 255) {
            breath = 255;
            breath_dir = -1;
        } else if (breath <= 45) {
            breath = 45;
            breath_dir = 1;
        }

        hue = (uint8_t)(hue + hue_step);

        uint8_t gamma = scale8((uint8_t)breath, (uint8_t)breath);
        uint8_t level = scale8(gamma, cur_bright);

        if (!ws2812_is_busy()) {
            for (uint8_t i = 0; i < WS2812_LED_COUNT; i++) {
                rgb_t c = color_wheel((uint8_t)(hue + i * 48U));
                ws2812_set_pixel(i, scale8(c.r, level), scale8(c.g, level), scale8(c.b, level));
            }
            ws2812_show();
        }

        rgb_t out[WS2812_LED_COUNT];
        decode_frame(out);

        printf("\rF:%03u | ADC:%4u EMA:%4u | Mode:%-5s | Br:%3d Max:%3u Lvl:%3u | ",
               frame, raw, adc_filtered, is_dark ? "DARK" : "LIGHT", breath, cur_bright, level);
        for (uint8_t i = 0; i < WS2812_LED_COUNT; i++) {
            printf("\x1b[48;2;%u;%u;%um    \x1b[0m ",
                   to_srgb(out[i].r), to_srgb(out[i].g), to_srgb(out[i].b));
        }
        fflush(stdout);
        sleep_ms(20);
    }

    printf("\nDone.\n");
    return 0;
}