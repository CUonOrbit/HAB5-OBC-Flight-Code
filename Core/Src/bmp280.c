#include "bmp280.h"

/* ---- Register addresses (from Bosch BMP280 datasheet) ---- */
#define BMP280_REG_CHIP_ID      0xD0
#define BMP280_REG_RESET        0xE0
#define BMP280_REG_CTRL_MEAS    0xF4
#define BMP280_REG_CONFIG       0xF5
#define BMP280_REG_PRESS_MSB    0xF7
#define BMP280_REG_CALIB_START  0x88

#define BMP280_CHIP_ID_EXPECTED 0x58

static HAL_StatusTypeDef ReadRegisters(BMP280_HandleTypeDef *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(dev->hi2c, BMP280_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

static HAL_StatusTypeDef WriteRegister(BMP280_HandleTypeDef *dev, uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(dev->hi2c, BMP280_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 100);
}

static void ReadCalibrationData(BMP280_HandleTypeDef *dev)
{
    uint8_t buf[24];
    ReadRegisters(dev, BMP280_REG_CALIB_START, buf, 24);

    dev->dig_T1 = (uint16_t)(buf[1] << 8 | buf[0]);
    dev->dig_T2 = (int16_t)(buf[3] << 8 | buf[2]);
    dev->dig_T3 = (int16_t)(buf[5] << 8 | buf[4]);
    dev->dig_P1 = (uint16_t)(buf[7] << 8 | buf[6]);
    dev->dig_P2 = (int16_t)(buf[9] << 8 | buf[8]);
    dev->dig_P3 = (int16_t)(buf[11] << 8 | buf[10]);
    dev->dig_P4 = (int16_t)(buf[13] << 8 | buf[12]);
    dev->dig_P5 = (int16_t)(buf[15] << 8 | buf[14]);
    dev->dig_P6 = (int16_t)(buf[17] << 8 | buf[16]);
    dev->dig_P7 = (int16_t)(buf[19] << 8 | buf[18]);
    dev->dig_P8 = (int16_t)(buf[21] << 8 | buf[20]);
    dev->dig_P9 = (int16_t)(buf[23] << 8 | buf[22]);
}

HAL_StatusTypeDef BMP280_Init(BMP280_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c, uint32_t interval_ms)
{
    dev->hi2c         = hi2c;
    dev->interval_ms  = interval_ms;
    dev->last_read    = HAL_GetTick();
    dev->t_fine       = 0;
    dev->last_temp_c  = 0.0f;
    dev->last_pressure_pa = 0.0f;

    uint8_t chip_id = 0;
    HAL_StatusTypeDef status = ReadRegisters(dev, BMP280_REG_CHIP_ID, &chip_id, 1);

    if (status != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (chip_id != BMP280_CHIP_ID_EXPECTED)
    {
        return HAL_ERROR;
    }

    ReadCalibrationData(dev);

    WriteRegister(dev, BMP280_REG_CTRL_MEAS, 0x27);
    WriteRegister(dev, BMP280_REG_CONFIG, 0x00);

    return HAL_OK;
}

/* Bosch's official 32-bit integer compensation formulas (from the datasheet) */
static int32_t CompensateTemperature(BMP280_HandleTypeDef *dev, int32_t adc_T)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)dev->dig_T1 << 1))) * ((int32_t)dev->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dev->dig_T1)) * ((adc_T >> 4) - ((int32_t)dev->dig_T1))) >> 12) * ((int32_t)dev->dig_T3)) >> 14;
    dev->t_fine = var1 + var2;
    T = (dev->t_fine * 5 + 128) >> 8;
    return T; /* in 0.01 degC steps, e.g. 5123 = 51.23 C */
}

static uint32_t CompensatePressure(BMP280_HandleTypeDef *dev, int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)dev->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dev->dig_P6;
    var2 = var2 + ((var1 * (int64_t)dev->dig_P5) << 17);
    var2 = var2 + (((int64_t)dev->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dev->dig_P3) >> 8) + ((var1 * (int64_t)dev->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dev->dig_P1) >> 33;

    if (var1 == 0)
    {
        return 0; /* avoid divide-by-zero */
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dev->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dev->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dev->dig_P7) << 4);

    return (uint32_t)p; /* Q24.8 format, i.e. divide by 256 for Pa */
}

void BMP280_Update(BMP280_HandleTypeDef *dev)
{
    uint32_t now = HAL_GetTick();
    if ((now - dev->last_read) < dev->interval_ms)
    {
        return;
    }
    dev->last_read = now;

    uint8_t buf[6];
    HAL_StatusTypeDef status = ReadRegisters(dev, BMP280_REG_PRESS_MSB, buf, 6);
    if (status != HAL_OK)
    {
        return;
    }

    int32_t adc_P = (int32_t)((buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4));
    int32_t adc_T = (int32_t)((buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4));

    int32_t temp_raw = CompensateTemperature(dev, adc_T);
    uint32_t press_raw = CompensatePressure(dev, adc_P);

    dev->last_temp_c      = temp_raw / 100.0f;
    dev->last_pressure_pa = press_raw / 256.0f;

}

float BMP280_GetTempC(BMP280_HandleTypeDef *dev)
{
    return dev->last_temp_c;
}

float BMP280_GetPressurePa(BMP280_HandleTypeDef *dev)
{
    return dev->last_pressure_pa;
}