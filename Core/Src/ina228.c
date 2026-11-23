#include "ina228.h"

/* ---- Register map (TI INA228 datasheet, SLYS021) ---- */
#define INA228_REG_CONFIG           0x00   /* 16-bit */
#define INA228_REG_ADC_CONFIG       0x01   /* 16-bit */
#define INA228_REG_SHUNT_CAL        0x02   /* 16-bit */
#define INA228_REG_VBUS             0x05   /* 24-bit, value in bits 23:4 */
#define INA228_REG_DIETEMP          0x06   /* 16-bit signed */
#define INA228_REG_CURRENT          0x07   /* 24-bit, signed value in bits 23:4 */
#define INA228_REG_POWER            0x08   /* 24-bit unsigned */
#define INA228_REG_ENERGY           0x09   /* 40-bit unsigned */
#define INA228_REG_CHARGE           0x0A   /* 40-bit signed */
#define INA228_REG_MANUFACTURER_ID  0x3E
#define INA228_REG_DEVICE_ID        0x3F

#define INA228_MANUFACTURER_ID      0x5449   /* "TI" */
#define INA228_DEVICE_ID            0x228    /* bits 15:4 of DEVICE_ID */

#define CONFIG_RSTACC               (1u << 14)   /* clear energy + charge */
#define CONFIG_ADCRANGE             (1u << 4)

/* ADC_CONFIG: continuous bus + shunt + temp, 1052 us per conversion, 64x averaging.
   One full averaged result every ~0.2 s - plenty for 1 Hz logging, and the
   averaging cuts noise. Charge/energy accumulate on every conversion regardless. */
#define ADC_CONFIG_VALUE            ((0xFu << 12) | (5u << 9) | (5u << 6) | (5u << 3) | 3u)

/* Fixed scaling factors from the datasheet */
#define VBUS_LSB_V                  195.3125e-6f
#define DIETEMP_LSB_C               7.8125e-3f
#define SHUNT_CAL_CONSTANT          13107.2e6f
#define ADCRANGE_0_MAX_V            0.16384f
#define ADCRANGE_1_MAX_V            0.04096f

#define INA228_I2C_TIMEOUT_MS       20

/* ---- Bus helpers (all registers are big-endian) ---- */

static HAL_StatusTypeDef ReadRegisters(INA228_HandleTypeDef *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(dev->hi2c, dev->addr, reg, I2C_MEMADD_SIZE_8BIT, buf, len, INA228_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef ReadRegister16(INA228_HandleTypeDef *dev, uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    HAL_StatusTypeDef status = ReadRegisters(dev, reg, buf, 2);
    *value = (uint16_t)((buf[0] << 8) | buf[1]);
    return status;
}

static HAL_StatusTypeDef WriteRegister16(INA228_HandleTypeDef *dev, uint8_t reg, uint16_t value)
{
    uint8_t buf[2] = { (uint8_t)(value >> 8), (uint8_t)value };
    return HAL_I2C_Mem_Write(dev->hi2c, dev->addr, reg, I2C_MEMADD_SIZE_8BIT, buf, 2, INA228_I2C_TIMEOUT_MS);
}

/* 20-bit value in the top of a 24-bit register. Put it at the top of an int32
   and shift right 12 so the sign bit carries down (two's complement). */
static int32_t Signed20(const uint8_t *b)
{
    int32_t v = (int32_t)(((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8));
    return v >> 12;
}

static uint64_t Unsigned40(const uint8_t *b)
{
    return ((uint64_t)b[0] << 32) | ((uint64_t)b[1] << 24) | ((uint64_t)b[2] << 16) |
           ((uint64_t)b[3] << 8)  |  (uint64_t)b[4];
}

/* ---- Public API ---- */

HAL_StatusTypeDef INA228_Init(INA228_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c, uint16_t addr,
                              float shunt_ohms, float max_current)
{
    dev->hi2c       = hi2c;
    dev->addr       = addr;
    dev->shunt_ohms = shunt_ohms;
    dev->bus_v      = 0.0f;
    dev->current_a  = 0.0f;
    dev->power_w    = 0.0f;
    dev->die_temp_c = 0.0f;
    dev->energy_j   = 0.0;
    dev->charge_c   = 0.0;

    /* Confirm it's an INA228 */
    uint16_t mfr = 0, dev_id = 0;
    if (ReadRegister16(dev, INA228_REG_MANUFACTURER_ID, &mfr) != HAL_OK ||
        ReadRegister16(dev, INA228_REG_DEVICE_ID, &dev_id) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (mfr != INA228_MANUFACTURER_ID || (dev_id >> 4) != INA228_DEVICE_ID)
    {
        return HAL_ERROR;
    }

    /* Use the smaller (4x finer) shunt range if the max shunt voltage fits in it */
    float max_shunt_v = max_current * shunt_ohms;
    if (max_shunt_v > ADCRANGE_0_MAX_V)
    {
        return HAL_ERROR;   /* shunt too big for this max current - would clip */
    }
    dev->adcrange = (max_shunt_v <= ADCRANGE_1_MAX_V) ? 1 : 0;

    /* Datasheet: CURRENT_LSB = max current / 2^19
                  SHUNT_CAL   = 13107.2e6 * CURRENT_LSB * R_shunt  (x4 if ADCRANGE = 1) */
    dev->current_lsb = max_current / 524288.0f;
    float cal = SHUNT_CAL_CONSTANT * dev->current_lsb * shunt_ohms;
    if (dev->adcrange)
    {
        cal *= 4.0f;
    }
    if (cal < 1.0f || cal > 32767.0f)   /* register is 15 bits */
    {
        return HAL_ERROR;
    }

    /* Set range only - no RST/RSTACC, so accumulated charge survives an MCU reset */
    if (WriteRegister16(dev, INA228_REG_CONFIG, dev->adcrange ? CONFIG_ADCRANGE : 0) != HAL_OK ||
        WriteRegister16(dev, INA228_REG_ADC_CONFIG, ADC_CONFIG_VALUE) != HAL_OK ||
        WriteRegister16(dev, INA228_REG_SHUNT_CAL, (uint16_t)(cal + 0.5f)) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef INA228_Update(INA228_HandleTypeDef *dev)
{
    uint8_t vbus[3], current[3], power[3], temp[2], energy[5], charge[5];

    if (ReadRegisters(dev, INA228_REG_VBUS,    vbus,    3) != HAL_OK ||
        ReadRegisters(dev, INA228_REG_CURRENT, current, 3) != HAL_OK ||
        ReadRegisters(dev, INA228_REG_POWER,   power,   3) != HAL_OK ||
        ReadRegisters(dev, INA228_REG_DIETEMP, temp,    2) != HAL_OK ||
        ReadRegisters(dev, INA228_REG_ENERGY,  energy,  5) != HAL_OK ||
        ReadRegisters(dev, INA228_REG_CHARGE,  charge,  5) != HAL_OK)
    {
        return HAL_ERROR;   /* keep last good values */
    }

    /* Bus voltage: positive only, 20 bits */
    uint32_t vbus_raw = ((uint32_t)vbus[0] << 12) | ((uint32_t)vbus[1] << 4) | (vbus[2] >> 4);
    dev->bus_v = vbus_raw * VBUS_LSB_V;

    /* Current: signed 20 bits */
    dev->current_a = Signed20(current) * dev->current_lsb;

    /* Power: unsigned 24 bits, LSB = 3.2 x CURRENT_LSB */
    uint32_t power_raw = ((uint32_t)power[0] << 16) | ((uint32_t)power[1] << 8) | power[2];
    dev->power_w = power_raw * 3.2f * dev->current_lsb;

    /* Die temperature: signed 16 bits */
    dev->die_temp_c = (int16_t)((temp[0] << 8) | temp[1]) * DIETEMP_LSB_C;

    /* Energy: unsigned 40 bits, LSB = 16 x 3.2 x CURRENT_LSB */
    dev->energy_j = (double)Unsigned40(energy) * 16.0 * 3.2 * dev->current_lsb;

    /* Charge: signed 40 bits - shift up to bit 63 and back to sign-extend */
    int64_t charge_raw = (int64_t)(Unsigned40(charge) << 24) >> 24;
    dev->charge_c = (double)charge_raw * dev->current_lsb;

    return HAL_OK;
}

HAL_StatusTypeDef INA228_ResetAccumulators(INA228_HandleTypeDef *dev)
{
    uint16_t config = CONFIG_RSTACC | (dev->adcrange ? CONFIG_ADCRANGE : 0);
    return WriteRegister16(dev, INA228_REG_CONFIG, config);
}

float  INA228_GetBusVoltage(INA228_HandleTypeDef *dev) { return dev->bus_v; }
float  INA228_GetCurrent(INA228_HandleTypeDef *dev)    { return dev->current_a; }
float  INA228_GetPower(INA228_HandleTypeDef *dev)      { return dev->power_w; }
float  INA228_GetDieTemp(INA228_HandleTypeDef *dev)    { return dev->die_temp_c; }
double INA228_GetEnergy(INA228_HandleTypeDef *dev)     { return dev->energy_j; }
double INA228_GetCharge(INA228_HandleTypeDef *dev)     { return dev->charge_c; }

/* 1 mAh = 3.6 C */
float INA228_GetChargeUsed_mAh(INA228_HandleTypeDef *dev) { return (float)(dev->charge_c / 3.6); }