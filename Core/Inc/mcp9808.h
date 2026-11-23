
#ifndef MCP9808_H
#define MCP9808_H

#include "main.h"

#define MCP9808_ADDR            (0x18 << 1)   /* A0-A2 tied low, shifted for HAL */
#define MCP9808_I2C_TIMEOUT     100           /* ms */

/* Registers */
#define MCP9808_CONFIG_REG      0x01
#define MCP9808_TEMP_REG        0x05
#define MCP9808_MANUF_ID_REG    0x06
#define MCP9808_DEVICE_ID_REG   0x07
#define MCP9808_RESOLUTION_REG  0x08

/* Expected IDs */
#define MCP9808_MANUF_ID        0x0054
#define MCP9808_DEVICE_ID       0x04          /* upper byte of reg 0x07 */

/* Resolution settings (reg 0x08) */
#define MCP9808_RES_0_5C        0x00          /* 30 ms conversion  */
#define MCP9808_RES_0_25C       0x01          /* 65 ms conversion  */
#define MCP9808_RES_0_125C      0x02          /* 130 ms conversion */
#define MCP9808_RES_0_0625C     0x03          /* 250 ms conversion (power-up default) */

/**
 * @brief  Check the sensor is present and is an MCP9808, then set 0.0625 °C resolution.
 * @retval HAL_OK on success, HAL_ERROR if IDs don't match, or the HAL I2C error.
 */
HAL_StatusTypeDef MCP9808_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  Read ambient temperature in °C (handles negative temperatures).
 */
HAL_StatusTypeDef MCP9808_ReadTemp(
    I2C_HandleTypeDef *hi2c,
    float *temperature
);

#endif
