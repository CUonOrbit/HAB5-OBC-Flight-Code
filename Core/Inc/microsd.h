#ifndef MICROSD_H
#define MICROSD_H

#include "main.h"
#include "fatfs.h"
#include "logbuffer.h"

FRESULT MICROSD_Init(void); // Mount microSD card, then create CSV file and leave open.
FRESULT MICROSD_WriteTo(const LOGBUF_HandleTypeDef *buf); // Write to the CSV.
FRESULT MICROSD_Sync(void); // Flush cached data. Will reset the buffer index?
FRESULT MICROSD_Unmount(void); // Close and unmount volume.

#endif