#include "microsd.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define ARR_LEN(arr) (sizeof(arr)) / sizeof((arr[0]))

static FATFS FatFs; //Fatfs handle
static FIL fil; 		//File handle
static FRESULT fres; //Result after operationss

FRESULT MICROSD_Init(void){
  fres = f_mount(&FatFs, "",  1);
  if(fres != FR_OK){
    myprintf("f_mount error (%i)\r\n", fres);
    return fres;
  }

  fres = f_open(&fil, "HAB5DATA.csv", FA_WRITE | FA_CREATE_NEW);
  if(fres != FR_OK){
    myprintf("f_open error (%i)\r\n", fres);
    f_mount(NULL, "", 0);
    return fres;
  } 

  UINT bw;
  const char *header = "timestamp_ms,lmt84_temp_c,mcp9808_temp_c,ntc_temp_c,ms5611_pressure_mbar,ms5611_temp_c,ina228_bus_v,ina228_current_ma,ina228_power_mw,imu_accel_g[0],imu_accel_g[1],imu_accel_g[2],imu_gyro_dps[0],imu_gyro_dps[1],imu_gyro_dps[2]\r\n";
  fres = f_write(&fil, header, strlen(header), &bw);
  if(fres != FR_OK || bw < strlen(header)){
    myprintf("CSV header write error\r\n");
    f_close(&fil);
    f_mount(NULL, "", 0);
    return fres;
  }
  f_sync(&fil);

  return FR_OK;
}

FRESULT MICROSD_WriteTo(const LOGBUF_HandleTypeDef *buf){
  char row[256];
  UINT bw;

  for(uint16_t i = 0; i < buf->count; i++){
    int len = snprintf(row, sizeof(row), "%lu,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\r\n", 
      buf->entries[i].timestamp_ms, 
      buf->entries[i].lmt84_temp_c, 
      buf->entries[i].mcp9808_temp_c, 
      buf->entries[i].ntc_temp_c, 
      buf->entries[i].ms5611_pressure_mbar,
      buf->entries[i].ms5611_temp_c, 
      buf->entries[i].ina228_bus_v, 
      buf->entries[i].ina228_current_ma, 
      buf->entries[i].ina228_power_mw, 
      buf->entries[i].imu_accel_g[0],
      buf->entries[i].imu_accel_g[1], 
      buf->entries[i].imu_accel_g[2], 
      buf->entries[i].imu_gyro_dps[0], 
      buf->entries[i].imu_gyro_dps[1], 
      buf->entries[i].imu_gyro_dps[2] 
    );
    
    fres = f_write(&fil, row, (UINT)len, &bw);

    if(fres != FR_OK){
      return fres;
    }
  }

  return FR_OK;
}

FRESULT MICROSD_Sync(void){
  fres = f_sync(&fil);
  if(fres != FR_OK){
    myprintf("f_sync error (%i)\r\n", fres);
  }
  return fres;
}

FRESULT MICROSD_Unmount(void){
  fres = f_close(&fil);
  fres = f_mount(NULL, "", 0);
  return fres;
}