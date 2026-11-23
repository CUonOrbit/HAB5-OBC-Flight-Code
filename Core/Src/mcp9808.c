
#include "mcp9808.h"

static HAL_StatusTypeDef MCP9808_ReadReg16(I2C_HandleTypeDef *hi2c, uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(
        hi2c, 
        MCP9808_ADDR, 
        reg, 
        I2C_MEMADD_SIZE_8BIT,
        buf, 
        2, 
        MCP9808_I2C_TIMEOUT);
        
    if (status != HAL_OK)
        return status;

    *value = ((uint16_t)buf[0] << 8) | buf[1];   /* MSB first */
    return HAL_OK;
}

HAL_StatusTypeDef MCP9808_Init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef status;
    uint16_t manuf_id;
    uint16_t device_id;
    uint8_t  res = MCP9808_RES_0_0625C;

    if (hi2c == NULL)
        return HAL_ERROR;

    status = HAL_I2C_IsDeviceReady(hi2c, MCP9808_ADDR, 3, MCP9808_I2C_TIMEOUT);
    if (status != HAL_OK)
        return status;

    status = MCP9808_ReadReg16(hi2c, MCP9808_MANUF_ID_REG, &manuf_id);
    if (status != HAL_OK)
        return status;
    if (manuf_id != MCP9808_MANUF_ID)
        return HAL_ERROR;

    status = MCP9808_ReadReg16(hi2c, MCP9808_DEVICE_ID_REG, &device_id);
    if (status != HAL_OK)
        return status;
    if ((device_id >> 8) != MCP9808_DEVICE_ID)
        return HAL_ERROR;

    /* Set resolution explicitly rather than relying on the power-up default */
    status = HAL_I2C_Mem_Write(hi2c, MCP9808_ADDR, MCP9808_RESOLUTION_REG,
                               I2C_MEMADD_SIZE_8BIT, &res, 1, MCP9808_I2C_TIMEOUT);
    return status;
}

HAL_StatusTypeDef MCP9808_ReadTemp(
    I2C_HandleTypeDef *hi2c,
    float *temperature
)
{
    HAL_StatusTypeDef status;
    uint16_t raw;
    int16_t  val;

    if (hi2c == NULL || temperature == NULL)
        return HAL_ERROR;

    status = MCP9808_ReadReg16(hi2c, MCP9808_TEMP_REG, &raw);
    if (status != HAL_OK)
        return status;

    /* Bits 15-13: alert flags, bit 12: sign, bits 11-0: temperature in 1/16 °C */
    raw &= 0x1FFF;
    val = (raw & 0x1000) ? (int16_t)(raw | 0xE000) : (int16_t)raw;   /* sign-extend 13-bit */

    *temperature = (float)val / 16.0f;
    return HAL_OK;
}
