/*
 * foo.h - TEMPLATE HEADER FILE for adding a new component
 *
 * Every component (sensor, radio, actuator, ...) is split into two files:
 *   foo.h  - the PUBLIC interface: the handle struct and the function
 *            prototypes. Other files (#include "foo.h") only see this.
 *   foo.c  - the PRIVATE implementation: register addresses, bus helpers,
 *            and the function bodies.
 *
 * Every component should provide, at minimum:
 *   FOO_Init()   - called once at startup from main.c (USER CODE 2)
 *   FOO_Update() - the main action, called periodically by a scheduler task, or some other main action
 * plus getters for any results other code needs.
 *
 * To make a new component: copy foo.h/foo.c, rename "foo"/"FOO" to your
 * component name, and fill in the parts marked TODO.
 */

/* Include guard: stops this header from being processed twice if it's
   #included from more than one place in the same .c file. */
#ifndef FOO_H
#define FOO_H

/* main.h pulls in stm32f4xx_hal.h, which gives us I2C_HandleTypeDef,
   HAL_StatusTypeDef, uint8_t/uint16_t, etc. */
#include "main.h"

/*
 * The HANDLE struct holds everything about ONE instance of the component.
 *
 * Why a struct instead of global variables inside foo.c?
 *   - You can have two of the same sensor (e.g. at 0x68 and 0x69) by just
 *     declaring two handles. The driver code doesn't change.
 *   - It's the same pattern ST's HAL uses (I2C_HandleTypeDef, etc.), so
 *     it reads consistently with the rest of the project.
 *
 * Every function takes a pointer to the handle as its first argument.
 * The handle itself is declared as a global in main.c (USER CODE PV).
 */
typedef struct {
    /* --- Bus info: set once in FOO_Init(), never changes --- */
    I2C_HandleTypeDef *hi2c;      /* which I2C peripheral the sensor is on, e.g. &hi2c1, MAYBE YOUR COMPONENT USES DIFFERENT PINS!*/
    uint16_t           addr;      /* 7-bit address shifted left by 1, e.g. (0x68 << 1).
                                     HAL expects the shifted form. Keep it here rather
                                     than in a #define so multiple instances work. */

    /* --- Cached results: written by FOO_Update(), read by the getters ---
       Getters return these instead of talking to the sensor, so reading a
       value is instant and never blocks on the I2C bus. */
    float some_random_data; /* in g */

    /* You may want to add calibration data, config settings, error counters, etc. */
} FOO_HandleTypeDef;

/*
 * FOO_Init - set up the handle and configure the sensor.
 *   dev  : handle to fill in (declared in main.c)
 *   hi2c : I2C peripheral the sensor is wired to (e.g. &hi2c1)
 *   addr : shifted I2C address (e.g. 0x68 << 1)
 * Returns HAL_OK if the chip answered with the expected ID and accepted
 * its config, HAL_ERROR otherwise (usually a wiring or address problem).
 */
HAL_StatusTypeDef FOO_Init(FOO_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c, uint16_t addr);

/*
 * FOO_Update - do ONE bus read and refresh the cached values.
 * Call this from a scheduler task; the task's interval sets the sample rate.
 * Must return quickly - the scheduler is cooperative, so anything slow here
 * delays every other task.
 * Returns HAL_OK on success. On failure the cached values are left at the
 * last good reading.
 */
HAL_StatusTypeDef FOO_Update(FOO_HandleTypeDef *dev);

/* Getters - return the most recent cached reading (no bus access). */
float FOO_GetSomeRandomData(FOO_HandleTypeDef *dev);

#endif /* FOO_H */