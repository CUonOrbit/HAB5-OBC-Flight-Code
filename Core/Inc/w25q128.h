/*
 * w25q128.h - Driver for the Winbond W25Q128 128 Mbit (16 MiB) SPI NOR flash
 *
 * Follows the foo.h template: one handle struct per chip, Init + actions +
 * getters. Unlike a sensor there's no periodic Update() - the "main actions"
 * are Read / Write / Erase, called by whatever component is logging data.
 *
 * How NOR flash behaves (worth knowing before using this):
 *   - Erased bytes read as 0xFF. Programming can only turn 1-bits into
 *     0-bits, so you must ERASE before re-writing an area.
 *   - The smallest erasable unit is a 4 KiB sector. Erasing sets every byte
 *     in it back to 0xFF.
 *   - Programming works in 256-byte pages. W25Q128_Write() handles splitting
 *     writes across page boundaries for you.
 *
 * Blocking times (datasheet maximums, typical is much lower):
 *   page program  ~3 ms      sector erase  ~400 ms
 *   64K erase     ~2 s       chip erase    ~200 s
 * The scheduler is cooperative, so avoid erasing from a time-critical task -
 * erase ahead of time (e.g. at startup) or erase one sector at a time.
 *
 * Wiring: SPI mode 0 (CPOL low, CPHA 1st edge), 8-bit, MSB first.
 * Chip select is a plain GPIO output driven by this driver, NOT the SPI
 * peripheral's hardware NSS, because each command needs CS held low across
 * several HAL transfers.
 */

#ifndef W25Q128_H
#define W25Q128_H

#include "main.h"

#define W25Q128_PAGE_SIZE      256u
#define W25Q128_SECTOR_SIZE    4096u
#define W25Q128_BLOCK64_SIZE   65536u

typedef struct {
    /* --- Bus info: set once in W25Q128_Init(), never changes --- */
    SPI_HandleTypeDef *hspi;      /* SPI peripheral the chip is on, e.g. &hspi2 */
    GPIO_TypeDef      *cs_port;   /* chip-select GPIO port, e.g. GPIOB */
    uint16_t           cs_pin;    /* chip-select GPIO pin, e.g. GPIO_PIN_12 */

    /* --- Read from the chip at init --- */
    uint8_t  jedec_id[3];         /* manufacturer, memory type, capacity code */
    uint32_t capacity_bytes;      /* 16 MiB for a W25Q128 */
} W25Q128_HandleTypeDef;

/* Result of W25Q128_SelfTest(), so a failure tells you which step broke. */
typedef enum {
    W25Q128_TEST_OK = 0,
    W25Q128_TEST_BAD_ADDRESS,     /* test address not sector-aligned / out of range */
    W25Q128_TEST_ERASE_FAILED,    /* erase command failed or timed out */
    W25Q128_TEST_NOT_BLANK,       /* sector didn't read back as all 0xFF after erase */
    W25Q128_TEST_WRITE_FAILED,    /* page program failed or timed out */
    W25Q128_TEST_READ_FAILED,     /* SPI error while reading */
    W25Q128_TEST_MISMATCH,        /* data read back differs from data written */
} W25Q128_TestResult;

/*
 * W25Q128_Init - set up the handle, wake the chip and check its JEDEC ID.
 *   dev     : handle to fill in (declared in main.c)
 *   hspi    : SPI peripheral the chip is wired to (e.g. &hspi2)
 *   cs_port : chip-select GPIO port (e.g. GPIOB)
 *   cs_pin  : chip-select GPIO pin  (e.g. GPIO_PIN_12)
 * Returns HAL_OK if a Winbond W25Q128 answered, HAL_ERROR otherwise
 * (usually wiring, CS pin, or SPI mode). An ID of FF FF FF or 00 00 00
 * typically means MISO isn't connected or CS never goes low.
 */
HAL_StatusTypeDef W25Q128_Init(W25Q128_HandleTypeDef *dev, SPI_HandleTypeDef *hspi,
                               GPIO_TypeDef *cs_port, uint16_t cs_pin);

/* Read len bytes starting at addr. Any address and length within the chip. */
HAL_StatusTypeDef W25Q128_Read(W25Q128_HandleTypeDef *dev, uint32_t addr, uint8_t *buf, uint32_t len);

/*
 * Program len bytes starting at addr. Splits across 256-byte pages itself.
 * The target area must already be erased (0xFF) or the result is the AND of
 * the old and new data. Blocks ~1-3 ms per page touched.
 */
HAL_StatusTypeDef W25Q128_Write(W25Q128_HandleTypeDef *dev, uint32_t addr, const uint8_t *buf, uint32_t len);

/* Erase the 4 KiB sector at addr. addr must be a multiple of 4096. */
HAL_StatusTypeDef W25Q128_EraseSector(W25Q128_HandleTypeDef *dev, uint32_t addr);

/* Erase the 64 KiB block at addr. addr must be a multiple of 65536. */
HAL_StatusTypeDef W25Q128_EraseBlock64(W25Q128_HandleTypeDef *dev, uint32_t addr);

/* Erase the whole chip. Takes up to ~200 s - ground use only, never in flight. */
HAL_StatusTypeDef W25Q128_EraseChip(W25Q128_HandleTypeDef *dev);

/* Returns 1 while the chip is still busy with a program/erase, 0 when idle. */
uint8_t W25Q128_IsBusy(W25Q128_HandleTypeDef *dev);

/*
 * W25Q128_SelfTest - erase / blank-check / write / read-back / compare.
 *   sector_addr : a 4 KiB-aligned sector that is SAFE TO ERASE. Its contents
 *                 are destroyed. The last sector of the chip is a good choice:
 *                 W25Q128_GetCapacity(dev) - W25Q128_SECTOR_SIZE
 * Writes a 300-byte pattern that deliberately crosses a page boundary, so it
 * also exercises the page-splitting in W25Q128_Write(). Takes up to ~0.5 s.
 */
W25Q128_TestResult W25Q128_SelfTest(W25Q128_HandleTypeDef *dev, uint32_t sector_addr);

/* Human-readable name for a test result, for printf. */
const char *W25Q128_TestResultToString(W25Q128_TestResult result);

/* Getters - values cached at init (no bus access). */
uint32_t       W25Q128_GetCapacity(W25Q128_HandleTypeDef *dev);
const uint8_t *W25Q128_GetJedecId(W25Q128_HandleTypeDef *dev);  /* 3 bytes */

#endif /* W25Q128_H */
