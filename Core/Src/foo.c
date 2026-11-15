/*
 * foo.c - TEMPLATE IMPLEMENTATION FILE for adding a new component
 *
 * Holds the register map, bus helpers, and function bodies for foo.h.
 * Copy alongside foo.h, rename "foo"/"FOO", and fill in the TODOs.
 *
 * Hooking it into main.c (inside USER CODE blocks):
 *   Includes:  #include "foo.h"
 *   PV:        FOO_HandleTypeDef foo;
 *   PFP:       static void Task_FOO(void);
 *   2:         if (FOO_Init(&foo, &hi2c1, 0x68 << 1) != HAL_OK) Error_Handler();
 *              Scheduler_AddTask(Task_FOO, 100, "foo");
 *   4:         static void Task_FOO(void) { FOO_Update(&foo); }
 */

#include "foo.h"

/* TODO: register addresses and values from your sensor's datasheet */
#define FOO_REG_ID              0x75   /* ID / WHO_AM_I register */
#define FOO_REG_CONFIG          0x6B   /* config register to write at init */
#define FOO_REG_DATA            0x3B   /* first byte of the measurement data */

#define FOO_ID_EXPECTED         0x68   /* value the ID register should return */
#define FOO_CONFIG_VALUE        0x00   /* value written to FOO_REG_CONFIG */
#define FOO_SCALE               16384.0f  /* raw counts per unit */

#define FOO_I2C_TIMEOUT_MS      20     /* keep short - blocks all other tasks while waiting */

/* ---- Private bus helpers ---- */

static HAL_StatusTypeDef ReadRegisters(FOO_HandleTypeDef *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(dev->hi2c, dev->addr, reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, FOO_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef WriteRegister(FOO_HandleTypeDef *dev, uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(dev->hi2c, dev->addr, reg, I2C_MEMADD_SIZE_8BIT,
                             &value, 1, FOO_I2C_TIMEOUT_MS);
}

/* ---- Public API ---- */

HAL_StatusTypeDef FOO_Init(FOO_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c, uint16_t addr)
{
    dev->hi2c             = hi2c; // which I2C bus this sensor is on (e.g. &hi2c1), passed in from main.c.
                                   // Other components may use SPI/UART instead - that's set by the handle
                                   // type in foo.h and the HAL calls in the bus helpers
    dev->addr             = addr;
    dev->some_random_data = 0.0f;

    /* Confirm the chip is there - catches wiring/address mistakes early */
    uint8_t id = 0;
    if (ReadRegisters(dev, FOO_REG_ID, &id, 1) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (id != FOO_ID_EXPECTED)
    {
        return HAL_ERROR;
    }

    /* TODO: write whatever config your sensor needs */
    if (WriteRegister(dev, FOO_REG_CONFIG, FOO_CONFIG_VALUE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef FOO_Update(FOO_HandleTypeDef *dev)
{
    /* No timing check here - the scheduler task controls the rate */
    uint8_t buf[2];
    HAL_StatusTypeDef status = ReadRegisters(dev, FOO_REG_DATA, buf, 2);
    if (status != HAL_OK)
    {
        return status;   /* keep the last good value */
    }

    /* TODO: match your sensor's data format (this assumes signed 16-bit, high byte first) */
    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);

    dev->some_random_data = raw / FOO_SCALE;

    return HAL_OK;
}

float FOO_GetSomeRandomData(FOO_HandleTypeDef *dev)
{
    return dev->some_random_data;
}