/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <math.h>
#include "bmp280.h"
#include "mcp9808.h"
#include "logbuffer.h"
#include "foo.h" //TEMPLATE: need to import our template foo component header file
#include "scheduler.h"
extern void initialise_monitor_handles(void);
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
BMP280_HandleTypeDef baro;
FOO_HandleTypeDef foo; //TEMPLATE: we add a PV for foo
LOGBUF_HandleTypeDef logbuf;   /* logs waiting for the next 5 s storage write */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
static void Task_BMP280(void);
static void Task_MCP9808(void);
static void Task_FOO(void); //TEMPLATE: we add a task for the foo componenet to be run in the main loop
static void Task_LogSample(void);
static void Task_LogFlush(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  initialise_monitor_handles();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  Scheduler_Init();
  /* USER CODE BEGIN 2 */
  // Initialize the BMP280 barometer.
  if (BMP280_Init(&baro, &hi2c1, 1000) == HAL_OK)
  {
    Scheduler_AddTask(Task_BMP280, 1000, "bmp280");
  }
  else
  {
    printf("BMP280 init failed - check wiring/address (0x76 vs 0x77)\r\n");
  }

   /* TEMPLATE: initialize the component.
     &foo     - pointer to the handle that FOO_Init fills in
     &hi2c1   - which I2C bus the sensor is on
     0x68<<1  - sensor's 7-bit I2C address, shifted left 1 because HAL expects that
     Only schedule the task if init succeeded. */
  if (FOO_Init(&foo, &hi2c1, 0x68 << 1) == HAL_OK)
  {
    Scheduler_AddTask(Task_FOO, 100, "foo");
  }
  else
  {
    printf("FOO init failed - check wiring/address\r\n");
  }
  Scheduler_AddTask(Task_BMP280, 1000, "bmp280");

  // Verify MCP9808 is responding
  if (HAL_I2C_IsDeviceReady(&hi2c1, MCP9808_ADDR, 3, 100) != HAL_OK)
  {
      Error_Handler();
  }

    printf("MCP9808 initialized\r\n");
    fflush(stdout);

    Scheduler_AddTask(Task_MCP9808, 1000, "mcp9808");

  
  Scheduler_AddTask(Task_FOO,     100, "foo"); //TEMPLATE: we add the foo task to our schedule

  /* Logging: snapshot every sensor at 12.5 Hz into the buffer, and write the
     buffer to storage every 5 s. Added after the sensor tasks so that when
     they're due in the same loop pass, the sensors update first and the log
     gets the newest values. */
  LOGBUF_Init(&logbuf);
  Scheduler_AddTask(Task_LogSample, LOG_SAMPLE_PERIOD_MS, "log_sample");
  Scheduler_AddTask(Task_LogFlush,  LOG_FLUSH_PERIOD_MS,  "log_flush");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    printf("MCP9808 initialized\r\n");
    Scheduler_Update();
    //printf("Temp: %.2f C  Pressure: %.1f Pa\r\n", BMP280_GetTempC(&baro), BMP280_GetPressurePa(&baro));
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void Task_BMP280(void)
{
    BMP280_Update(&baro);
    printf("Temp: %.2f C  Pressure: %.1f Pa\r\n", BMP280_GetTempC(&baro), BMP280_GetPressurePa(&baro));
}

static void Task_MCP9808(void)
{
    float temperature_c;

    if (MCP9808_ReadTemp(&hi2c1, &temperature_c) == HAL_OK)
    {
        printf("MCP9808 Temp: %.2f C\r\n", temperature_c);
    }
    else
    {
        printf("MCP9808 read failed\r\n");
    }
}
//TEMPLATE: here, we define what happens during the foo task in the main loop. 
static void Task_FOO(void)        
{
  FOO_Update(&foo);
  printf("foo: %.3f\r\n", FOO_GetSomeRandomData(&foo));
}

/*
 * Build one log from the latest cached value of every sensor.
 * Each sensor's own task updates its handle at that sensor's rate; the
 * getters just return the cached value. So a sensor that hasn't produced a
 * new reading since the last log simply repeats its previous one.
 *
 * TODO: as each sensor driver is added, replace its NAN with its getter.
 * NAN (not 0) means "no data" shows up clearly in post-flight analysis.
 */
static void Task_LogSample(void)
{
  LOG_Entry e;

  e.timestamp_ms         = HAL_GetTick();

  e.lmt84_temp_c         = NAN;  /* LMT84_GetTempC(&lmt84)          */
  e.mcp9808_temp_c       = NAN;  /* MCP9808_GetTempC(&mcp9808)      */
  e.ntc_temp_c           = NAN;  /* NTC_GetTempC(&ntc)              */

  e.ms5611_pressure_mbar = NAN;  /* MS5611_GetPressureMbar(&ms5611) */
  e.ms5611_temp_c        = NAN;  /* MS5611_GetTempC(&ms5611)        */

  e.ina228_bus_v         = NAN;  /* INA228_GetBusVoltage(&ina228)   */
  e.ina228_current_ma    = NAN;  /* INA228_GetCurrentmA(&ina228)    */
  e.ina228_power_mw      = NAN;  /* INA228_GetPowermW(&ina228)      */

  for (int i = 0; i < 3; i++)
  {
    e.imu_accel_g[i]     = NAN;  /* IMU_GetAccelG(&imu, i)          */
    e.imu_gyro_dps[i]    = NAN;  /* IMU_GetGyroDps(&imu, i)         */
  }

  LOGBUF_Add(&logbuf, &e);       /* if full, it's dropped and counted */
}

/*
 * Hand the whole buffer to each storage device, then empty it.
 * Expect ~62-63 logs per flush (5000 ms / 80 ms).
 */
static void Task_LogFlush(void)
{
  /* Once they exist, add the writers once the drivers exist on this branch, e.g.
       W25Q128_WriteLogs(&flash, &logbuf);
       SD_WriteLogs(&sd, &logbuf);                                        */

  printf("Log flush: %u logs, %lu dropped total\r\n",
         (unsigned)LOGBUF_GetCount(&logbuf), (unsigned long)LOGBUF_GetDropped(&logbuf));

  LOGBUF_Clear(&logbuf);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
