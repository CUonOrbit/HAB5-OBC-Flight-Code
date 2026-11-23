/*
 * logbuffer.h - RAM buffer of telemetry logs waiting to be written to storage
 *
 * A LOG_Entry is one snapshot: a timestamp plus the latest value from every
 * sensor. A LOGBUF_HandleTypeDef is a list of those entries.
 *
 * Data flow (rates from the duty cycle table):
 *   - Sensor tasks update their own cached values at their own rates.
 *   - A log task runs at the fastest sensor rate (IMU, 12.5 Hz = every 80 ms),
 *     builds a LOG_Entry from the sensor getters and calls LOGBUF_Add().
 *     Slower sensors (1-10 Hz) just repeat their latest value.
 *   - A flush task runs every 5 s, passes the buffer to the NOR flash and
 *     microSD writers (each writes every entry), then calls LOGBUF_Clear().
 *
 * Writers should take a const pointer and read entries[0 .. count-1]:
 *   HAL_StatusTypeDef XXX_WriteLogs(XXX_HandleTypeDef *dev, const LOGBUF_HandleTypeDef *buf);
 *
 * Example (main.c, USER CODE 4):
 *   static void Task_LogSample(void)          // every LOG_SAMPLE_PERIOD_MS
 *   {
 *       LOG_Entry e;
 *       e.timestamp_ms   = HAL_GetTick();
 *       e.mcp9808_temp_c = MCP9808_GetTempC(&mcp9808);
 *       ...one line per field...
 *       LOGBUF_Add(&logbuf, &e);
 *   }
 *   static void Task_LogFlush(void)           // every LOG_FLUSH_PERIOD_MS
 *   {
 *       W25Q128_WriteLogs(&flash, &logbuf);
 *       SD_WriteLogs(&sd, &logbuf);
 *       LOGBUF_Clear(&logbuf);
 *   }
 */

#ifndef LOGBUFFER_H
#define LOGBUFFER_H

#include "main.h"

#define LOG_SAMPLE_PERIOD_MS   80u     /* 12.5 Hz - fastest sensor (IMU) */
#define LOG_FLUSH_PERIOD_MS    5000u   /* storage write burst every 5 s */

/* 5000 / 80 = 62.5 logs per flush. The scheduler is cooperative, so a flush
   can run a little late - leave headroom so no logs are dropped. */
#define LOG_BUFFER_CAPACITY    ((LOG_FLUSH_PERIOD_MS / LOG_SAMPLE_PERIOD_MS) + 8u)   /* 70 */

/*
 * One log: timestamp + latest value of every sensor (60 bytes).
 * Every field is 4 bytes, so there's no padding and the layout in RAM is
 * exactly what gets written to flash. If you change this struct, update the
 * post-flight decoder and the size check below.
 */
typedef struct {
    uint32_t timestamp_ms;          /* ms since boot (HAL_GetTick) */

    float lmt84_temp_c;             /* LMT84LPM internal analog temp, C  (1 Hz) */
    float mcp9808_temp_c;           /* MCP9808 internal digital temp, C  (1 Hz) */
    float ntc_temp_c;               /* USP10981 external NTC air temp, C (1 Hz) */

    float ms5611_pressure_mbar;     /* MS5611 barometer pressure, mbar   (5 Hz) */
    float ms5611_temp_c;            /* MS5611 barometer temp, C          (5 Hz) */

    float ina228_bus_v;             /* INA228 bus voltage, V             (10 Hz) */
    float ina228_current_ma;        /* INA228 shunt current, mA          (10 Hz) */
    float ina228_power_mw;          /* INA228 power, mW                  (10 Hz) */

    float imu_accel_g[3];           /* ISM330DHCX accel X/Y/Z, g         (12.5 Hz) */
    float imu_gyro_dps[3];          /* ISM330DHCX gyro X/Y/Z, deg/s      (12.5 Hz) */
} LOG_Entry;

_Static_assert(sizeof(LOG_Entry) == 60, "LOG_Entry layout changed - update the decoder and this check");

typedef struct {
    LOG_Entry entries[LOG_BUFFER_CAPACITY];
    uint16_t  count;                /* number of valid entries in entries[] */
    uint32_t  dropped;              /* logs lost because the buffer was full (never reset) */
} LOGBUF_HandleTypeDef;

/* Empty the buffer and zero the dropped counter. Call once at startup. */
void LOGBUF_Init(LOGBUF_HandleTypeDef *buf);

/*
 * Copy one log onto the end of the buffer.
 * Returns HAL_OK, or HAL_ERROR if the buffer is full - the log is dropped
 * and counted in buf->dropped (means the flush task isn't keeping up).
 */
HAL_StatusTypeDef LOGBUF_Add(LOGBUF_HandleTypeDef *buf, const LOG_Entry *entry);

/* Empty the buffer after the writers have stored it. Keeps the dropped count. */
void LOGBUF_Clear(LOGBUF_HandleTypeDef *buf);

/* Getters */
uint16_t         LOGBUF_GetCount(const LOGBUF_HandleTypeDef *buf);
uint8_t          LOGBUF_IsFull(const LOGBUF_HandleTypeDef *buf);
uint32_t         LOGBUF_GetDropped(const LOGBUF_HandleTypeDef *buf);
const LOG_Entry *LOGBUF_GetEntry(const LOGBUF_HandleTypeDef *buf, uint16_t index);  /* NULL if out of range */

#endif /* LOGBUFFER_H */
