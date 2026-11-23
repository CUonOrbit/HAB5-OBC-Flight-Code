/*
 * logbuffer.c - RAM buffer of telemetry logs waiting to be written to storage
 *
 * See logbuffer.h for the data flow. Hooking it into main.c (inside USER CODE blocks):
 *   Includes:  #include "logbuffer.h"
 *   PV:        LOGBUF_HandleTypeDef logbuf;
 *   2:         LOGBUF_Init(&logbuf);
 *              Scheduler_AddTask(Task_LogSample, LOG_SAMPLE_PERIOD_MS, "log_sample");
 *              Scheduler_AddTask(Task_LogFlush,  LOG_FLUSH_PERIOD_MS,  "log_flush");
 */

#include "logbuffer.h"
#include <string.h>

void LOGBUF_Init(LOGBUF_HandleTypeDef *buf)
{
    buf->count   = 0;
    buf->dropped = 0;
}

HAL_StatusTypeDef LOGBUF_Add(LOGBUF_HandleTypeDef *buf, const LOG_Entry *entry)
{
    if (buf->count >= LOG_BUFFER_CAPACITY)
    {
        buf->dropped++;
        return HAL_ERROR;
    }
    memcpy(&buf->entries[buf->count], entry, sizeof(LOG_Entry));
    buf->count++;
    return HAL_OK;
}

void LOGBUF_Clear(LOGBUF_HandleTypeDef *buf)
{
    /* Old entries stay in RAM but are ignored - only count matters */
    buf->count = 0;
}

uint16_t LOGBUF_GetCount(const LOGBUF_HandleTypeDef *buf)
{
    return buf->count;
}

uint8_t LOGBUF_IsFull(const LOGBUF_HandleTypeDef *buf)
{
    return buf->count >= LOG_BUFFER_CAPACITY;
}

uint32_t LOGBUF_GetDropped(const LOGBUF_HandleTypeDef *buf)
{
    return buf->dropped;
}

const LOG_Entry *LOGBUF_GetEntry(const LOGBUF_HandleTypeDef *buf, uint16_t index)
{
    if (index >= buf->count)
    {
        return NULL;
    }
    return &buf->entries[index];
}
