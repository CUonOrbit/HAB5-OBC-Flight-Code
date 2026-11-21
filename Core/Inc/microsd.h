#ifndef MICROSD_H
#define MICROSD_H

#include "main.h"
#include "fatfs.h"

#define BUFFER_CAPACITY 20

typedef struct {
    float col1;
    float col2;
    float col3;
    float col4;
} MICROSD_Buffer;

// Tests
void MICROSD_Demo(void);
void MICROSD_WriteTest(void);
void MICROSD_CSVTest(void);

// Core 
FRESULT MICROSD_Init(void); // Mount microSD card, then create CSV file and leave open.
FRESULT MICROSD_Sync(void); // Flush cached data.
FRESULT MICROSD_Unmount(void) // Close and unmount volume.
void MICROSD_SaveEntries(void); // 

// Setters
void MICROSD_SetCol1(float val);
void MICROSD_SetCol2(float val);
void MICROSD_SetCol3(float val);
void MICROSD_SetCol4(float val);

#endif