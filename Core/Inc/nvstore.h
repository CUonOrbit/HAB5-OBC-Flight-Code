/*
 * nvstore.h - Non-volatile variable storage on the W25Q128 flash
 *
 * Keeps a handful of uint32_t values (boot counter, battery usage, ...) that
 * survive resets and power loss. Follows the foo.h template: one handle,
 * Init + actions + getters.
 *
 * Flash layout (W25Q128, 16 MiB):
 *   0x000000 - 0x001FFF   nvstore (2 x 4 KiB sectors)   <- this file
 *   0x002000 - ...        free for data logging (start at NVSTORE_END_ADDR)
 *   last sector           W25Q128 self-test scratch (when FLASH_RUN_SELF_TEST = 1)
 *
 * How it works (so you don't have to worry about flash wear or power cuts):
 *   - Every Save appends a small record (all values + sequence number + CRC)
 *     to a log. A sector holds 128 records, so a sector is only erased once
 *     every 128 saves instead of on every save.
 *   - The log alternates between two sectors. When one fills, the other is
 *     erased and used. The old sector keeps the last good copy until then.
 *   - At Init the newest record with a valid CRC wins. A save cut short by a
 *     power loss just fails its CRC and the previous values are used.
 *   - Wear: ~100k erases per sector x 2 sectors x 128 records = ~25 million
 *     saves. Saving once per second would last most of a year.
 *
 * Usage:
 *   NVSTORE_Set(&nv, NV_BOOT_COUNT, NVSTORE_Get(&nv, NV_BOOT_COUNT) + 1);
 *   NVSTORE_Save(&nv);   // nothing hits flash until Save
 */

#ifndef NVSTORE_H
#define NVSTORE_H

#include "main.h"
#include "w25q128.h"

/* Reserved flash region. Keep other flash users at or above NVSTORE_END_ADDR. */
#define NVSTORE_FLASH_ADDR    0x000000u
#define NVSTORE_NUM_SECTORS   2u
#define NVSTORE_END_ADDR      (NVSTORE_FLASH_ADDR + NVSTORE_NUM_SECTORS * W25Q128_SECTOR_SIZE)

/*
 * The stored variables. To add one: replace a spare (keeps the format), or
 * add a new entry before NV_VAR_COUNT. Up to 5 fit in a record. Adding one
 * changes the record format, so also bump NVSTORE_MAGIC in nvstore.c (old
 * records are then ignored and everything starts from 0).
 */
typedef enum {
    NV_BOOT_COUNT = 0,     /* incremented once per boot in main.c */
    NV_BATTERY_USAGE,      /* e.g. mAh consumed - units up to whoever updates it */
    NV_SPARE_2,
    NV_SPARE_3,
    NV_VAR_COUNT
} NVSTORE_VarId;

typedef struct {
    W25Q128_HandleTypeDef *flash;            /* flash chip, already initialised */

    uint32_t values[NV_VAR_COUNT];           /* current values (RAM copy) */
    uint8_t  dirty;                          /* values changed since last Save */

    /* Log position - internal bookkeeping, don't touch */
    uint32_t seq;                            /* sequence number of last saved record */
    uint8_t  active_sector;                  /* 0 or 1 */
    uint16_t next_slot;                      /* next free record slot in active sector */
} NVSTORE_HandleTypeDef;

/*
 * NVSTORE_Init - load the newest saved values from flash.
 *   nv    : handle to fill in (declared in main.c)
 *   flash : W25Q128 handle that has already passed W25Q128_Init()
 * If nothing has ever been saved (fresh chip) all values start at 0.
 * Returns HAL_OK, or HAL_ERROR if the flash couldn't be read.
 */
HAL_StatusTypeDef NVSTORE_Init(NVSTORE_HandleTypeDef *nv, W25Q128_HandleTypeDef *flash);

/* Get / set a value in RAM. No flash access - call NVSTORE_Save() to persist. */
uint32_t NVSTORE_Get(NVSTORE_HandleTypeDef *nv, NVSTORE_VarId id);
void     NVSTORE_Set(NVSTORE_HandleTypeDef *nv, NVSTORE_VarId id, uint32_t value);

/*
 * NVSTORE_Save - write the current values to flash if anything changed.
 * Usually ~1 ms. Once every 128 saves it also erases a sector, which can
 * block for up to ~400 ms, so don't call it from a time-critical task.
 */
HAL_StatusTypeDef NVSTORE_Save(NVSTORE_HandleTypeDef *nv);

/* Set every value back to 0 and save (e.g. ground reset before flight). */
HAL_StatusTypeDef NVSTORE_ResetAll(NVSTORE_HandleTypeDef *nv);

#endif /* NVSTORE_H */
