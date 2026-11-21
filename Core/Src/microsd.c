#include "microsd.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static FATFS FatFs; 	//Fatfs handle
static FIL fil; 		//File handle
static FRESULT fres; //Result after operations

static MICROSD_Buffer current_row = {0};
static MICROSD_Buffer buffer[BUFFER_CAPACITY];
static uint8_t buffer_index = 0;

void MICROSD_Demo(void)
{
  myprintf("\r\n~ SD card demo by kiwih ~\r\n\r\n");

  HAL_Delay(1000); //a short delay is important to let the SD card settle

  //Open the file system
  fres = f_mount(&FatFs, "", 1); //1=mount now
  if (fres != FR_OK) {
    myprintf("f_mount error (%i)\r\n", fres);
    while(1);
  }

  //Let's get some statistics from the SD card
  DWORD free_clusters, free_sectors, total_sectors;

  FATFS* getFreeFs;

  fres = f_getfree("", &free_clusters, &getFreeFs);
  if (fres != FR_OK) {
  myprintf("f_getfree error (%i)\r\n", fres);
  while(1);
  }

  //Formula comes from ChaN's documentation
  total_sectors = (getFreeFs->n_fatent - 2) * getFreeFs->csize;
  free_sectors = free_clusters * getFreeFs->csize;

  myprintf("SD card stats:\r\n%10lu KiB total drive space.\r\n%10lu KiB available.\r\n", total_sectors / 2, free_sectors / 2);

  //Now let's try to open file "test.txt"
  fres = f_open(&fil, "test.txt", FA_READ);
  if (fres != FR_OK) {
  myprintf("f_open error (%i)\r\n", fres);
  while(1);
  }
  myprintf("I was able to open 'test.txt' for reading!\r\n");

  //Read 30 bytes from "test.txt" on the SD card
  BYTE readBuf[30];

  //We can either use f_read OR f_gets to get data out of files
  //f_gets is a wrapper on f_read that does some string formatting for us
  TCHAR* rres = f_gets((TCHAR*)readBuf, 30, &fil);
  if(rres != 0) {
  myprintf("Read string from 'test.txt' contents: %s\r\n", readBuf);
  } else {
  myprintf("f_gets error (%i)\r\n", fres);
  }

  //Be a tidy kiwi - don't forget to close your file!
  f_close(&fil);

  //We're done, so de-mount the drive
  f_mount(NULL, "", 0);
}

void MICROSD_WriteTest(void)
{
  UINT bytesWritten;
  const char *text = "Hello testing testing HAB5\r\n";

  // 1. Mount
  fres = f_mount(&FatFs, "",  1);
  if(fres != FR_OK){
    myprintf("f_mount error (%i)\r\n", fres);
    return;
  }

  // 2. Create new file and overwrite if need be
  fres = f_open(&fil, "flight.txt", FA_WRITE | FA_CREATE_ALWAYS);
  if(fres != FR_OK){
    myprintf("f_open error (%i)\r\n", fres);
    f_mount(NULL, "", 0);
    return;
  } 

  // 3. Write to
  fres = f_write(&fil, text, strlen(text), &bytesWritten);
  if(fres != FR_OK || bytesWritten < strlen(text)) {
    myprintf("f_write error (%i)\r\n", fres);
  }
  else{
    myprintf("Wrote %u bytes successfully!\r\n", bytesWritten);
  }

  //4. Close ("flushes" data to SD?) and unmount
  f_close(&fil);
  f_mount(NULL, "", 0);
}

void MICROSD_CSVTest(void){
  UINT bytesWritten;

  const char *text = "COL1,COL2,COL3,COL4\r\na,b,c,d\r\ne,f,g,h\r\ni,j,k,l\r\n";

  fres = f_mount(&FatFs, "",  1);
  if(fres != FR_OK){
    myprintf("f_mount error (%i)\r\n", fres);
    return;
  }

  fres = f_open(&fil, "data.csv", FA_WRITE | FA_CREATE_ALWAYS);
  if(fres != FR_OK){
    myprintf("f_open error (%i)\r\n", fres);
    f_mount(NULL, "", 0);
    return;
  } 

  fres = f_write(&fil, text, strlen(text), &bytesWritten);
  if(fres != FR_OK || bytesWritten < strlen(text)) {
    myprintf("f_write error (%i)\r\n", fres);
  }
  else{
    myprintf("Wrote %u bytes successfully!\r\n", bytesWritten);
  }

  f_close(&fil);
  f_mount(NULL, "", 0);
}

FRESULT MICROSD_Init(void){
  fres = f_mount(&FatFs, "",  1);
  if(fres != FR_OK){
    myprintf("f_mount error (%i)\r\n", fres);
    return fres;
  }

  fres = f_open(&fil, "HAB5-MICROSD-DATA.csv", FA_WRITE | FA_CREATE_ALWAYS);
  if(fres != FR_OK){
    myprintf("f_open error (%i)\r\n", fres);
    f_mount(NULL, "", 0);
    return fres;
  } 
}

FRESULT MICROSD_Sync(void){
  fres = f_sync();
  if(fres != FR_OK){
    myprintf("f_sync error (%i)\r\n", fres);
    return fres;
  }
}

void MICROSD_SetCol1(float val){
  current_row.col1 = val; 
}

void MICROSD_SetCol2(float val){
  current_row.col2 = val; 
}

void MICROSD_SetCol3(float val){
  current_row.col3 = val; 
}

void MICROSD_SetCol4(float val){
  current_row.col4 = val; 
}

void MICROSD_SaveEntries(void) {
    buffer[buffer_index] = current_row;
    buffer_index++;
    if (buffer_index >= BUFFER_CAPACITY) {
        MICROSD_Sync();
        buffer_index = 0;
    }
}