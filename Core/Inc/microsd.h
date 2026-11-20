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

void MICROSD_Demo(void);
void MICROSD_WriteTest(void);

void MICROSD_CSVTest(void);

void Initialize_CSV(void);

#endif