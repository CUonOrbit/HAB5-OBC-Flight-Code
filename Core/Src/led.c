
#include "led.h"

#define LED_COUNT 8

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t last_toggle;
    uint32_t blinking_interval_ms;
} LED_State;

// {port, pin, last_toggle, blinking_interval_ms}
static LED_State leds[LED_COUNT] = {
    {GPIOA, 11, 0, 0}, // LED0
    {GPIOA, 12, 0, 0}, // LED1
    {GPIOA, 15, 0, 0}, // LED2
    {GPIOA,  8, 0, 0}, // LED3
    {GPIOB,  1, 0, 0}, // LED4
    {GPIOB,  2, 0, 0}, // LED5
    {GPIOB,  8, 0, 0}, // LED6
    {GPIOB,  9, 0, 0}  // LED7
};

static LED_State *LED_Find(uint16_t led_number){
    if (led_number >= LED_COUNT)
        return NULL;

    return &leds[led_number];
}

void LED_On(uint16_t led_number) {
    LED_State *led = LED_Find(led_number);

    if (led == NULL)
        return;

    led->blinking_interval_ms = 0;

    HAL_GPIO_WritePin(led->port, (uint16_t)(1U << led->pin), GPIO_PIN_SET);
}

void LED_Off(uint16_t led_number) {
    LED_State *led = LED_Find(led_number);

    if (led == NULL)
        return;

    led->blinking_interval_ms = 0;

    HAL_GPIO_WritePin(led->port, (uint16_t)(1U << led->pin), GPIO_PIN_RESET);
}

void LED_Blink(uint16_t led_number, uint32_t interval_ms) {
    LED_State *led = LED_Find(led_number);

    if (led == NULL || interval_ms == 0)
        return;

    uint32_t current_tick = HAL_GetTick();

    if (!led->blinking_interval_ms) {
        led->last_toggle = current_tick;
        HAL_GPIO_TogglePin(led->port, (uint16_t)(1U << led->pin));
    }
    
    led->blinking_interval_ms = interval_ms;
}

void LED_Update(void) {
    uint32_t current_tick = HAL_GetTick();
    for (int i = 0; i < LED_COUNT; i++) {
        if (leds[i].blinking_interval_ms > 0) {
            if ((uint32_t)(current_tick - leds[i].last_toggle) >= leds[i].blinking_interval_ms) {
                HAL_GPIO_TogglePin(leds[i].port, (uint16_t)(1U << leds[i].pin));
                leds[i].last_toggle = current_tick;
            }
        }
    }
}
