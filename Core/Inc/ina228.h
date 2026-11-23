#ifndef INA228_H
#define INA228_H

/*
 * TI INA228 power monitor (I2C).
 * Measures battery voltage, current, power, and accumulates energy and
 * charge in hardware - charge used is what gives you state of charge.
 */

#include "main.h"

typedef struct {
    /* --- Bus info --- */
    I2C_HandleTypeDef *hi2c;
    uint16_t           addr;          /* shifted, e.g. (0x40 << 1) with A0 = A1 = GND */

    /* --- Calibration (set in Init) --- */
    float    shunt_ohms;
    float    current_lsb;             /* amps per CURRENT register count */
    uint8_t  adcrange;                /* 0 = +/-163.84 mV, 1 = +/-40.96 mV shunt range */

    /* --- Cached results, written by INA228_Update() --- */
    float    bus_v;                   /* battery voltage at the VBUS pin */
    float    current_a;
    float    power_w;
    float    die_temp_c;
    double   energy_j;                /* accumulated since INA228 power-up / reset */
    double   charge_c;                /* accumulated since INA228 power-up / reset */
} INA228_HandleTypeDef;

/*
 * shunt_ohms  : value of the shunt resistor on the PCB
 * max_current : largest current you expect to measure (sets resolution)
 * Picks the shunt range automatically. Does NOT clear the energy/charge
 * accumulators, so an MCU-only reset mid-flight keeps the running total.
 */
HAL_StatusTypeDef INA228_Init(INA228_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c, uint16_t addr,
                              float shunt_ohms, float max_current);

/* Read all measurements. Call from a scheduler task (e.g. 1 Hz). */
HAL_StatusTypeDef INA228_Update(INA228_HandleTypeDef *dev);

/* Zero the energy and charge accumulators (e.g. when fresh batteries go in). */
HAL_StatusTypeDef INA228_ResetAccumulators(INA228_HandleTypeDef *dev);

float  INA228_GetBusVoltage(INA228_HandleTypeDef *dev);
float  INA228_GetCurrent(INA228_HandleTypeDef *dev);
float  INA228_GetPower(INA228_HandleTypeDef *dev);
float  INA228_GetDieTemp(INA228_HandleTypeDef *dev);
double INA228_GetEnergy(INA228_HandleTypeDef *dev);
double INA228_GetCharge(INA228_HandleTypeDef *dev);
float  INA228_GetChargeUsed_mAh(INA228_HandleTypeDef *dev);

#endif /* INA228_H */