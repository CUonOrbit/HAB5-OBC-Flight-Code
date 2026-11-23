#ifndef BMP280_H
#define BMP280_H

#include "main.h"

/* SDO tied to GND = 0x76, SDO tied to VDD = 0x77.
   Adafruit breakout default is usually 0x77 - verify against board. */
#define BMP280_I2C_ADDR   (0x77 << 1)

typedef struct {
    /* factory calibration coefficients, read once at init */
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    int32_t t_fine;   /* shared intermediate value used by both temp and pressure compensation */

    I2C_HandleTypeDef *hi2c;
    uint32_t            interval_ms;
    uint32_t            last_read;

    float last_temp_c;
    float last_pressure_pa;
} BMP280_HandleTypeDef;

/* Returns HAL_OK on success, or an error code if the sensor didn't respond /
   chip ID didn't match (helps you catch wiring problems early). */
HAL_StatusTypeDef BMP280_Init(BMP280_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c, uint32_t interval_ms);
void               BMP280_Update(BMP280_HandleTypeDef *dev);
float              BMP280_GetTempC(BMP280_HandleTypeDef *dev);
float              BMP280_GetPressurePa(BMP280_HandleTypeDef *dev);

#endif /* BMP280_H */