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

#include "thermistor.h"

/* TODO: register addresses and values from your sensor's datasheet */

// #define FOO_REG_ID              0x75   /* ID / WHO_AM_I register */
// #define FOO_REG_CONFIG          0x6B   /* config register to write at init */
// #define FOO_REG_DATA            0x3B   /* first byte of the measurement data */

// #define FOO_ID_EXPECTED         0x68   /* value the ID register should return */
// #define FOO_CONFIG_VALUE        0x00   /* value written to FOO_REG_CONFIG */
// #define FOO_SCALE               16384.0f  /* raw counts per unit */

// #define FOO_I2C_TIMEOUT_MS      20     /* keep short - blocks all other tasks while waiting */
//im like 30% none of this matters for the way the thermistor is read
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

HAL_StatusTypeDef THERMISTOR_Init(THERMISTOR_HandleTypeDef *dev, THERMISTOR_HandleTypeDef *hadc, uint32_t adc_channel, uint16_t PULL_UP_R, float VOLTAGE)
{
    //ensure ptrs are valid
    if (dev == NULL || hadc == NULL)
    {
        return HAL_ERROR;
    }
    dev->hadc                    = hadc;                 // which adc bus this sensor is on (e.g. &hadc1), passed in from main.c.
    dev->adc_channel             = adc_channel;          //which channel is mapped to the pin PA1 (double check pin no.)
    dev->PULL_UP_R               = PULL_UP_R;   //(ohms) //value of other resistor in divider, currently is 47kohm
    dev->VOLTAGE                 = VOLTAGE; //(volts)    //value of voltage applied to one end of divider, currently 3V3
    


    //calibration time
    if(HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED) != HAL_OK){
        return HAL_ERROR
    }

    return HAL_OK;
}

HAL_StatusTypeDef THERMISTOR_Update(THERMISTOR_HandleTypeDef *dev)
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

    dev->temperatureK = 67 //DUMMY DATA

    return HAL_OK;
}

float THERMISTOR_GetCelsius(THERMISTOR_HandleTypeDef *dev)
{
    return dev->temperatureK - 273.15;  //fat and lazy method
}

float THERMISTOR_GetKelvin(THERMISTOR_HandleTypeDef *dev){
    return dev->temperatureK;
}