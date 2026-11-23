/*
 * nvstore.c - Non-volatile variable storage on the W25Q128 flash
 *
 * See nvstore.h for the design. Hooking it into main.c (inside USER CODE blocks):
 *   Includes:  #include "nvstore.h"
 *   PV:        NVSTORE_HandleTypeDef nv;
 *   2:         after W25Q128_Init() succeeds: NVSTORE_Init(&nv, &flash);
 */

#include "nvstore.h"
#include <stddef.h>
#include <string.h>

/* Identifies a valid record. Change it whenever the record format changes
   (e.g. NV_VAR_COUNT changes) so old records are ignored. */
#define NVSTORE_MAGIC         0x4E560401u   /* "NV", format 04, rev 01 */

#define NVSTORE_RECORD_SIZE   32u           /* must divide 256 so records never straddle a page */
#define NVSTORE_SLOTS         (W25Q128_SECTOR_SIZE / NVSTORE_RECORD_SIZE)   /* 128 */

/* One record as stored in flash. raw[] pads it to a fixed size; unused
   bytes stay 0xFF (erased). */
typedef union {
    struct {
        uint32_t magic;
        uint32_t seq;
        uint32_t values[NV_VAR_COUNT];
        uint32_t crc;                       /* CRC-32 of everything before it */
    } f;
    uint8_t raw[NVSTORE_RECORD_SIZE];
} NVSTORE_Record;

_Static_assert(sizeof(((NVSTORE_Record *)0)->f) <= NVSTORE_RECORD_SIZE,
               "Too many NV variables for one record - increase NVSTORE_RECORD_SIZE to 64");
_Static_assert((W25Q128_PAGE_SIZE % NVSTORE_RECORD_SIZE) == 0,
               "NVSTORE_RECORD_SIZE must divide the flash page size");

/* ---- Private helpers ---- */

/* Standard CRC-32 (same as zip/Ethernet), bit by bit - records are tiny. */
static uint32_t Crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static uint32_t RecordCrc(const NVSTORE_Record *rec)
{
    return Crc32(rec->raw, offsetof(NVSTORE_Record, f.crc));
}

static uint8_t RecordIsValid(const NVSTORE_Record *rec)
{
    return rec->f.magic == NVSTORE_MAGIC && rec->f.crc == RecordCrc(rec);
}

static uint32_t SlotAddr(uint8_t sector, uint16_t slot)
{
    return NVSTORE_FLASH_ADDR + (uint32_t)sector * W25Q128_SECTOR_SIZE
           + (uint32_t)slot * NVSTORE_RECORD_SIZE;
}

/* A slot is free only if every byte is still erased. A record cut short by
   a power loss is partly programmed, so it counts as used and is skipped. */
static HAL_StatusTypeDef SlotIsBlank(NVSTORE_HandleTypeDef *nv, uint8_t sector, uint16_t slot, uint8_t *blank)
{
    NVSTORE_Record rec;
    if (W25Q128_Read(nv->flash, SlotAddr(sector, slot), rec.raw, NVSTORE_RECORD_SIZE) != HAL_OK)
    {
        return HAL_ERROR;
    }
    *blank = 1;
    for (uint32_t i = 0; i < NVSTORE_RECORD_SIZE; i++)
    {
        if (rec.raw[i] != 0xFF)
        {
            *blank = 0;
            break;
        }
    }
    return HAL_OK;
}

/* ---- Public API ---- */

HAL_StatusTypeDef NVSTORE_Init(NVSTORE_HandleTypeDef *nv, W25Q128_HandleTypeDef *flash)
{
    static uint8_t page[W25Q128_PAGE_SIZE];   /* static: keep it off the 1 KiB stack */

    nv->flash = flash;
    memset(nv->values, 0, sizeof(nv->values));
    nv->dirty = 0;
    nv->seq   = 0;

    /* Scan both sectors for the valid record with the highest sequence number */
    uint8_t  found = 0;
    uint8_t  best_sector = 0;
    uint16_t best_slot = 0;

    for (uint8_t sector = 0; sector < NVSTORE_NUM_SECTORS; sector++)
    {
        for (uint32_t off = 0; off < W25Q128_SECTOR_SIZE; off += W25Q128_PAGE_SIZE)
        {
            if (W25Q128_Read(flash, SlotAddr(sector, 0) + off, page, W25Q128_PAGE_SIZE) != HAL_OK)
            {
                return HAL_ERROR;
            }
            for (uint32_t r = 0; r < W25Q128_PAGE_SIZE; r += NVSTORE_RECORD_SIZE)
            {
                NVSTORE_Record rec;
                memcpy(rec.raw, &page[r], NVSTORE_RECORD_SIZE);
                if (RecordIsValid(&rec) && (!found || rec.f.seq > nv->seq))
                {
                    found       = 1;
                    nv->seq     = rec.f.seq;
                    best_sector = sector;
                    best_slot   = (uint16_t)((off + r) / NVSTORE_RECORD_SIZE);
                    memcpy(nv->values, rec.f.values, sizeof(nv->values));
                }
            }
        }
    }

    if (!found)
    {
        /* Fresh chip (or format changed): pretend sector 1 is full, so the
           first Save erases sector 0 and starts the log there. */
        nv->active_sector = 1;
        nv->next_slot     = NVSTORE_SLOTS;
        return HAL_OK;
    }

    /* Continue the log after the newest record, at the first fully blank slot */
    nv->active_sector = best_sector;
    nv->next_slot     = NVSTORE_SLOTS;   /* "full" unless we find a blank slot */
    for (uint16_t slot = best_slot + 1; slot < NVSTORE_SLOTS; slot++)
    {
        uint8_t blank = 0;
        if (SlotIsBlank(nv, best_sector, slot, &blank) != HAL_OK)
        {
            return HAL_ERROR;
        }
        if (blank)
        {
            nv->next_slot = slot;
            break;
        }
    }

    return HAL_OK;
}

uint32_t NVSTORE_Get(NVSTORE_HandleTypeDef *nv, NVSTORE_VarId id)
{
    if (id >= NV_VAR_COUNT)
    {
        return 0;
    }
    return nv->values[id];
}

void NVSTORE_Set(NVSTORE_HandleTypeDef *nv, NVSTORE_VarId id, uint32_t value)
{
    if (id >= NV_VAR_COUNT || nv->values[id] == value)
    {
        return;
    }
    nv->values[id] = value;
    nv->dirty = 1;
}

HAL_StatusTypeDef NVSTORE_Save(NVSTORE_HandleTypeDef *nv)
{
    if (!nv->dirty)
    {
        return HAL_OK;   /* nothing changed - don't wear the flash */
    }

    /* Active sector full: move to the other one. Only erase it now, so the
       old sector still holds the last good values if power drops mid-erase. */
    if (nv->next_slot >= NVSTORE_SLOTS)
    {
        uint8_t other = (uint8_t)(nv->active_sector ^ 1u);
        if (W25Q128_EraseSector(nv->flash, SlotAddr(other, 0)) != HAL_OK)
        {
            return HAL_ERROR;
        }
        nv->active_sector = other;
        nv->next_slot     = 0;
    }

    NVSTORE_Record rec;
    memset(rec.raw, 0xFF, sizeof(rec.raw));
    rec.f.magic = NVSTORE_MAGIC;
    rec.f.seq   = nv->seq + 1;
    memcpy(rec.f.values, nv->values, sizeof(rec.f.values));
    rec.f.crc   = RecordCrc(&rec);

    uint32_t addr = SlotAddr(nv->active_sector, nv->next_slot);

    /* The slot is used from here on, even if the write fails part-way */
    nv->next_slot++;

    if (W25Q128_Write(nv->flash, addr, rec.raw, NVSTORE_RECORD_SIZE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Read back to be sure it really landed */
    NVSTORE_Record check;
    if (W25Q128_Read(nv->flash, addr, check.raw, NVSTORE_RECORD_SIZE) != HAL_OK ||
        memcmp(check.raw, rec.raw, NVSTORE_RECORD_SIZE) != 0)
    {
        return HAL_ERROR;
    }

    nv->seq   = rec.f.seq;
    nv->dirty = 0;
    return HAL_OK;
}

HAL_StatusTypeDef NVSTORE_ResetAll(NVSTORE_HandleTypeDef *nv)
{
    for (int i = 0; i < NV_VAR_COUNT; i++)
    {
        NVSTORE_Set(nv, (NVSTORE_VarId)i, 0);
    }
    nv->dirty = 1;   /* save even if everything was already 0 */
    return NVSTORE_Save(nv);
}
