
#ifndef LED_H
#define LED_H

#include "main.h"

// Control an LED by its GPIOB pin number
void LED_On(uint16_t pin);
void LED_Off(uint16_t pin);
void LED_Blink(uint16_t pin, uint32_t interval_ms);
void LED_Update(void);

#endif