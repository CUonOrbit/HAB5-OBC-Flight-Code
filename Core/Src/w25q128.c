/*
 * w25q128.c - Winbond W25Q128 SPI NOR flash driver
 *
 * Command set and timings from the Winbond W25Q128JV datasheet.
 *
 * Hooking it into main.c (inside USER CODE blocks):
 *   Includes:  #include "w25q128.h"
 *   PV:        W25Q128_HandleTypeDef flash;
 *   2:         if (W25Q128_Init(&flash, &hspi2, GPIOB, GPIO_PIN_12) == HAL_OK) { ... }
 */

#include "w25q128.h"

/* ---- Commands (W25Q128JV datasheet, section 8) ---- */
#define W25Q_CMD_WRITE_ENABLE       0x06
#define W25Q_CMD_READ_STATUS1       0x05
#define W25Q_CMD_READ_DATA          0x03   /* standard read, fine up to 50 MHz */
#define W25Q_CMD_PAGE_PROGRAM       0x02
#define W25Q_CMD_SECTOR_ERASE_4K    0x20
#define W25Q_CMD_BLOCK_ERASE_64K    0xD8
#define W25Q_CMD_CHIP_ERASE         0xC7
#define W25Q_CMD_RELEASE_POWERDOWN  0xAB
#define W25Q_CMD_JEDEC_ID           0x9F

/* ---- Status register 1 bits ---- */
#define W25Q_SR1_BUSY               0x01   /* program/erase in progress */
#define W25Q_SR1_WEL                0x02   /* write enable latch */

/* ---- Expected JEDEC ID ---- */
#define W25Q_MANUFACTURER_WINBOND   0xEF
#define W25Q_MEMTYPE_SPI            0x40   /* W25Q128JV-IQ/JQ */
#define W25Q_MEMTYPE_QPI            0x70   /* W25Q128JV-IM/JM */
#define W25Q_CAPACITY_128MBIT       0x18   /* 2^0x18 = 16 MiB */

/* ---- Timeouts: datasheet max plus margin (HAL tick is 1 ms) ---- */
#define W25Q_SPI_TIMEOUT_MS         20
#define W25Q_TIMEOUT_PAGE_MS        10     /* tPP max 3 ms */
#define W25Q_TIMEOUT_SECTOR_MS      500    /* tSE max 400 ms */
#define W25Q_TIMEOUT_BLOCK64_MS     2500   /* tBE2 max 2000 ms */
#define W25Q_TIMEOUT_CHIP_MS        250000 /* tCE max 200 s */

/* ---- Private bus helpers ---- */

static void CS_Low(W25Q128_HandleTypeDef *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static void CS_High(W25Q128_HandleTypeDef *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

/* Send a 1-byte command followed by a 24-bit address, MSB first.
   Caller must already have CS low. */
static HAL_StatusTypeDef SendCommandWithAddress(W25Q128_HandleTypeDef *dev, uint8_t cmd, uint32_t addr)
{
    uint8_t hdr[4] = {
        cmd,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr)
    };
    return HAL_SPI_Transmit(dev->hspi, hdr, sizeof(hdr), W25Q_SPI_TIMEOUT_MS);
}

/* Send a single-byte command on its own CS cycle (e.g. write enable). */
static HAL_StatusTypeDef SendCommand(W25Q128_HandleTypeDef *dev, uint8_t cmd)
{
    CS_Low(dev);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(dev->hspi, &cmd, 1, W25Q_SPI_TIMEOUT_MS);
    CS_High(dev);
    return status;
}

static HAL_StatusTypeDef ReadStatus1(W25Q128_HandleTypeDef *dev, uint8_t *sr1)
{
    uint8_t cmd = W25Q_CMD_READ_STATUS1;
    CS_Low(dev);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(dev->hspi, &cmd, 1, W25Q_SPI_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Receive(dev->hspi, sr1, 1, W25Q_SPI_TIMEOUT_MS);
    }
    CS_High(dev);
    return status;
}

/* Poll the BUSY bit until the chip finishes a program/erase or we time out. */
static HAL_StatusTypeDef WaitUntilReady(W25Q128_HandleTypeDef *dev, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t sr1 = 0;

    do
    {
        if (ReadStatus1(dev, &sr1) != HAL_OK)
        {
            return HAL_ERROR;
        }
        if ((sr1 & W25Q_SR1_BUSY) == 0)
        {
            return HAL_OK;
        }
    } while ((HAL_GetTick() - start) < timeout_ms);

    return HAL_TIMEOUT;
}

/* Every program/erase must be preceded by Write Enable. The chip clears
   WEL itself when the operation finishes. We check WEL actually got set -
   if it didn't, the chip is probably write-protected or not responding. */
static HAL_StatusTypeDef WriteEnable(W25Q128_HandleTypeDef *dev)
{
    if (SendCommand(dev, W25Q_CMD_WRITE_ENABLE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    uint8_t sr1 = 0;
    if (ReadStatus1(dev, &sr1) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return (sr1 & W25Q_SR1_WEL) ? HAL_OK : HAL_ERROR;
}

/* Common path for the three erase commands. */
static HAL_StatusTypeDef Erase(W25Q128_HandleTypeDef *dev, uint8_t cmd, uint32_t addr,
                               uint8_t has_addr, uint32_t timeout_ms)
{
    /* Don't start a new operation while a previous one is still running */
    if (WaitUntilReady(dev, timeout_ms) != HAL_OK)
    {
        return HAL_TIMEOUT;
    }
    if (WriteEnable(dev) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status;
    if (has_addr)
    {
        CS_Low(dev);
        status = SendCommandWithAddress(dev, cmd, addr);
        CS_High(dev);   /* the erase starts when CS goes high */
    }
    else
    {
        status = SendCommand(dev, cmd);
    }
    if (status != HAL_OK)
    {
        return status;
    }

    return WaitUntilReady(dev, timeout_ms);
}

/* addr..addr+len must lie inside the chip (written to avoid uint32 overflow) */
static uint8_t InRange(W25Q128_HandleTypeDef *dev, uint32_t addr, uint32_t len)
{
    return (addr < dev->capacity_bytes) && (len <= dev->capacity_bytes - addr);
}

/* ---- Public API ---- */

HAL_StatusTypeDef W25Q128_Init(W25Q128_HandleTypeDef *dev, SPI_HandleTypeDef *hspi,
                               GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
    dev->hspi           = hspi;
    dev->cs_port        = cs_port;
    dev->cs_pin         = cs_pin;
    dev->jedec_id[0]    = 0;
    dev->jedec_id[1]    = 0;
    dev->jedec_id[2]    = 0;
    dev->capacity_bytes = 0;

    CS_High(dev);
    HAL_Delay(5);   /* tVSL: chip needs up to 5 ms after power-up before accepting commands */

    /* If the chip was left in power-down mode it ignores everything except
       this command. Harmless if it's already awake. */
    if (SendCommand(dev, W25Q_CMD_RELEASE_POWERDOWN) != HAL_OK)
    {
        return HAL_ERROR;
    }
    HAL_Delay(1);   /* tRES1 is 3 us */

    /* Confirm the chip is there - catches wiring / CS mistakes early */
    uint8_t cmd = W25Q_CMD_JEDEC_ID;
    CS_Low(dev);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(dev->hspi, &cmd, 1, W25Q_SPI_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Receive(dev->hspi, dev->jedec_id, 3, W25Q_SPI_TIMEOUT_MS);
    }
    CS_High(dev);
    if (status != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (dev->jedec_id[0] != W25Q_MANUFACTURER_WINBOND ||
        (dev->jedec_id[1] != W25Q_MEMTYPE_SPI && dev->jedec_id[1] != W25Q_MEMTYPE_QPI) ||
        dev->jedec_id[2] != W25Q_CAPACITY_128MBIT)
    {
        return HAL_ERROR;
    }
    dev->capacity_bytes = 1UL << dev->jedec_id[2];   /* 16 MiB */

    /* If we reset mid-erase the chip may still be busy */
    if (WaitUntilReady(dev, W25Q_TIMEOUT_SECTOR_MS) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef W25Q128_Read(W25Q128_HandleTypeDef *dev, uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (len == 0)
    {
        return HAL_OK;
    }
    if (!InRange(dev, addr, len))
    {
        return HAL_ERROR;
    }
    if (WaitUntilReady(dev, W25Q_TIMEOUT_PAGE_MS) != HAL_OK)
    {
        return HAL_BUSY;   /* a program/erase is still running */
    }

    CS_Low(dev);
    HAL_StatusTypeDef status = SendCommandWithAddress(dev, W25Q_CMD_READ_DATA, addr);

    /* The chip keeps streaming sequential bytes for as long as CS is low,
       so a long read is just several receives. HAL_SPI_Receive takes a
       uint16_t length, hence the chunking. */
    while (status == HAL_OK && len > 0)
    {
        uint16_t chunk = (len > 0xFFFFu) ? 0xFFFFu : (uint16_t)len;
        status = HAL_SPI_Receive(dev->hspi, buf, chunk, W25Q_SPI_TIMEOUT_MS + chunk / 64u);
        buf += chunk;
        len -= chunk;
    }
    CS_High(dev);

    return status;
}

HAL_StatusTypeDef W25Q128_Write(W25Q128_HandleTypeDef *dev, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    if (len == 0)
    {
        return HAL_OK;
    }
    if (!InRange(dev, addr, len))
    {
        return HAL_ERROR;
    }

    while (len > 0)
    {
        /* A page program that runs past the end of a 256-byte page wraps
           around to the START of the same page, overwriting data. So each
           program must stop at the page boundary. */
        uint32_t page_remaining = W25Q128_PAGE_SIZE - (addr % W25Q128_PAGE_SIZE);
        uint16_t chunk = (uint16_t)((len < page_remaining) ? len : page_remaining);

        if (WaitUntilReady(dev, W25Q_TIMEOUT_PAGE_MS) != HAL_OK)
        {
            return HAL_TIMEOUT;
        }
        if (WriteEnable(dev) != HAL_OK)
        {
            return HAL_ERROR;
        }

        CS_Low(dev);
        HAL_StatusTypeDef status = SendCommandWithAddress(dev, W25Q_CMD_PAGE_PROGRAM, addr);
        if (status == HAL_OK)
        {
            /* HAL_SPI_Transmit takes a non-const pointer but doesn't modify the data */
            status = HAL_SPI_Transmit(dev->hspi, (uint8_t *)buf, chunk, W25Q_SPI_TIMEOUT_MS);
        }
        CS_High(dev);   /* programming starts when CS goes high */
        if (status != HAL_OK)
        {
            return status;
        }

        addr += chunk;
        buf  += chunk;
        len  -= chunk;
    }

    /* Wait for the last page so a Read() straight after sees the new data */
    return WaitUntilReady(dev, W25Q_TIMEOUT_PAGE_MS);
}

HAL_StatusTypeDef W25Q128_EraseSector(W25Q128_HandleTypeDef *dev, uint32_t addr)
{
    if ((addr % W25Q128_SECTOR_SIZE) != 0 || !InRange(dev, addr, W25Q128_SECTOR_SIZE))
    {
        return HAL_ERROR;
    }
    return Erase(dev, W25Q_CMD_SECTOR_ERASE_4K, addr, 1, W25Q_TIMEOUT_SECTOR_MS);
}

HAL_StatusTypeDef W25Q128_EraseBlock64(W25Q128_HandleTypeDef *dev, uint32_t addr)
{
    if ((addr % W25Q128_BLOCK64_SIZE) != 0 || !InRange(dev, addr, W25Q128_BLOCK64_SIZE))
    {
        return HAL_ERROR;
    }
    return Erase(dev, W25Q_CMD_BLOCK_ERASE_64K, addr, 1, W25Q_TIMEOUT_BLOCK64_MS);
}

HAL_StatusTypeDef W25Q128_EraseChip(W25Q128_HandleTypeDef *dev)
{
    return Erase(dev, W25Q_CMD_CHIP_ERASE, 0, 0, W25Q_TIMEOUT_CHIP_MS);
}

uint8_t W25Q128_IsBusy(W25Q128_HandleTypeDef *dev)
{
    uint8_t sr1 = 0;
    if (ReadStatus1(dev, &sr1) != HAL_OK)
    {
        return 1;   /* can't tell - treat as busy so callers don't charge ahead */
    }
    return (sr1 & W25Q_SR1_BUSY) ? 1 : 0;
}

/* ---- Self test ---- */

#define W25Q_TEST_OFFSET   200u   /* start mid-page...                        */
#define W25Q_TEST_LEN      300u   /* ...and run past the 256-byte page boundary */

W25Q128_TestResult W25Q128_SelfTest(W25Q128_HandleTypeDef *dev, uint32_t sector_addr)
{
    /* static so they don't land on the 1 KiB stack */
    static uint8_t tx[W25Q_TEST_LEN];
    static uint8_t rx[W25Q_TEST_LEN];

    if ((sector_addr % W25Q128_SECTOR_SIZE) != 0 || !InRange(dev, sector_addr, W25Q128_SECTOR_SIZE))
    {
        return W25Q128_TEST_BAD_ADDRESS;
    }

    /* 1. Erase */
    if (W25Q128_EraseSector(dev, sector_addr) != HAL_OK)
    {
        return W25Q128_TEST_ERASE_FAILED;
    }

    /* 2. Blank check - the whole sector should now read 0xFF */
    for (uint32_t off = 0; off < W25Q128_SECTOR_SIZE; off += W25Q128_PAGE_SIZE)
    {
        if (W25Q128_Read(dev, sector_addr + off, rx, W25Q128_PAGE_SIZE) != HAL_OK)
        {
            return W25Q128_TEST_READ_FAILED;
        }
        for (uint32_t i = 0; i < W25Q128_PAGE_SIZE; i++)
        {
            if (rx[i] != 0xFF)
            {
                return W25Q128_TEST_NOT_BLANK;
            }
        }
    }

    /* 3. Write a pattern. Seeded from the tick so it differs run to run,
          and non-repeating within 256 bytes so a page-wrap bug shows up. */
    uint8_t seed = (uint8_t)HAL_GetTick();
    for (uint32_t i = 0; i < W25Q_TEST_LEN; i++)
    {
        tx[i] = (uint8_t)(seed + i * 7u + (i >> 8));
    }
    uint32_t test_addr = sector_addr + W25Q_TEST_OFFSET;
    if (W25Q128_Write(dev, test_addr, tx, W25Q_TEST_LEN) != HAL_OK)
    {
        return W25Q128_TEST_WRITE_FAILED;
    }

    /* 4. Read back and compare */
    for (uint32_t i = 0; i < W25Q_TEST_LEN; i++)
    {
        rx[i] = 0;
    }
    if (W25Q128_Read(dev, test_addr, rx, W25Q_TEST_LEN) != HAL_OK)
    {
        return W25Q128_TEST_READ_FAILED;
    }
    for (uint32_t i = 0; i < W25Q_TEST_LEN; i++)
    {
        if (rx[i] != tx[i])
        {
            return W25Q128_TEST_MISMATCH;
        }
    }

    /* 5. Bytes just outside the written range must still be erased -
          catches writes landing in the wrong place */
    uint8_t edge[2];
    if (W25Q128_Read(dev, test_addr - 1, &edge[0], 1) != HAL_OK ||
        W25Q128_Read(dev, test_addr + W25Q_TEST_LEN, &edge[1], 1) != HAL_OK)
    {
        return W25Q128_TEST_READ_FAILED;
    }
    if (edge[0] != 0xFF || edge[1] != 0xFF)
    {
        return W25Q128_TEST_MISMATCH;
    }

    return W25Q128_TEST_OK;
}

const char *W25Q128_TestResultToString(W25Q128_TestResult result)
{
    switch (result)
    {
        case W25Q128_TEST_OK:           return "OK";
        case W25Q128_TEST_BAD_ADDRESS:  return "bad test address";
        case W25Q128_TEST_ERASE_FAILED: return "erase failed";
        case W25Q128_TEST_NOT_BLANK:    return "sector not blank after erase";
        case W25Q128_TEST_WRITE_FAILED: return "write failed";
        case W25Q128_TEST_READ_FAILED:  return "read failed";
        case W25Q128_TEST_MISMATCH:     return "read-back mismatch";
        default:                        return "unknown";
    }
}

/* ---- Getters ---- */

uint32_t W25Q128_GetCapacity(W25Q128_HandleTypeDef *dev)
{
    return dev->capacity_bytes;
}

const uint8_t *W25Q128_GetJedecId(W25Q128_HandleTypeDef *dev)
{
    return dev->jedec_id;
}
