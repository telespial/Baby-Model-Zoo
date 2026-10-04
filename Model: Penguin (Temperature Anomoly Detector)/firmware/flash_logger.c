#include "flash_logger.h"

#include <stdbool.h>
#include <string.h>

#include "MCXC162.h"

#define LOGGER_BASE_ADDRESS  0x0000C000U
#define LOGGER_FLASH_SIZE    0x00004000U
#define LOGGER_SECTOR_SIZE   0x00002000U
#define LOGGER_RECORD_MAGIC  0x584C4F47U /* "GOLX" in little-endian memory. */
#define LOGGER_RECORD_SIZE   16U
#define LOGGER_SLOT_COUNT    (LOGGER_FLASH_SIZE / LOGGER_RECORD_SIZE)
#define LOGGER_INVALID_INDEX UINT32_MAX

typedef struct
{
    uint32_t magic;
    uint32_t sequence;
    uint32_t epochSeconds;
    int16_t temperatureCentiC;
    uint16_t checksum;
} flash_record_t;

_Static_assert(sizeof(flash_record_t) == LOGGER_RECORD_SIZE, "Flash record must be one programming phrase");
_Static_assert(LOGGER_SLOT_COUNT == FLASH_LOGGER_CAPACITY, "Public capacity must match flash layout");

/* Official MCUXpresso EEPROM-emulation flash binding for the K4 flash IP. */
int eeprom_emu_flashapi_init(void *api);
int eeprom_emu_flashapi_write(uint32_t offset, const void *src, size_t len);
int eeprom_emu_flashapi_erase(uint32_t offset, size_t len);

static uint32_t s_nextIndex;
static uint32_t s_nextSequence;
static uint32_t s_recordCount;

static const flash_record_t *record_at(uint32_t index)
{
    return (const flash_record_t *)(uintptr_t)(LOGGER_BASE_ADDRESS + (index * LOGGER_RECORD_SIZE));
}

static uint16_t record_checksum(const flash_record_t *record)
{
    const uint8_t *bytes = (const uint8_t *)record;
    uint16_t crc = 0xFFFFU;

    for (size_t index = 0U; index < offsetof(flash_record_t, checksum); index++)
    {
        crc ^= (uint16_t)bytes[index] << 8U;
        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1U) ^ 0x1021U) : (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

static bool record_is_valid(const flash_record_t *record)
{
    return (record->magic == LOGGER_RECORD_MAGIC) && (record->checksum == record_checksum(record));
}

static bool record_is_erased(const flash_record_t *record)
{
    const uint32_t *words = (const uint32_t *)record;
    return (words[0] == UINT32_MAX) && (words[1] == UINT32_MAX) &&
           (words[2] == UINT32_MAX) && (words[3] == UINT32_MAX);
}

static status_t erase_sector_for_index(uint32_t index)
{
    uint32_t byteOffset = index * LOGGER_RECORD_SIZE;
    uint32_t sectorOffset = byteOffset - (byteOffset % LOGGER_SECTOR_SIZE);
    return eeprom_emu_flashapi_erase(LOGGER_BASE_ADDRESS + sectorOffset, LOGGER_SECTOR_SIZE) == 0
               ? kStatus_Success
               : kStatus_Fail;
}

static void scan_records(void)
{
    bool found = false;
    uint32_t newestIndex = 0U;
    uint32_t newestSequence = 0U;

    s_recordCount = 0U;
    for (uint32_t index = 0U; index < LOGGER_SLOT_COUNT; index++)
    {
        const flash_record_t *record = record_at(index);
        if (record_is_valid(record))
        {
            s_recordCount++;
            if (!found || ((int32_t)(record->sequence - newestSequence) > 0))
            {
                found = true;
                newestIndex = index;
                newestSequence = record->sequence;
            }
        }
    }

    s_nextSequence = found ? newestSequence + 1U : 0U;
    s_nextIndex = found ? (newestIndex + 1U) % LOGGER_SLOT_COUNT : 0U;

    /* A reset during phrase programming can leave an invalid, non-erased slot.
       Skip forward within the sector instead of erasing newer valid records. */
    while (!record_is_erased(record_at(s_nextIndex)) &&
           ((s_nextIndex * LOGGER_RECORD_SIZE) % LOGGER_SECTOR_SIZE) != 0U)
    {
        s_nextIndex = (s_nextIndex + 1U) % LOGGER_SLOT_COUNT;
    }
}

status_t FlashLogger_Init(void)
{
    /* Match the access configuration used by NXP's FRDM-MCXC162 EEPROM example. */
    MBC->MBC_INDEX[0].MBC_MEMN_GLBAC[0] = 0x00007777U;
    MBC->MBC_INDEX[0].MBC_DOM0_MEM0_BLK_CFG_W[0] = 0x00000000U;

    if (eeprom_emu_flashapi_init(NULL) != 0)
    {
        return kStatus_Fail;
    }
    scan_records();
    return kStatus_Success;
}

status_t FlashLogger_Append(uint32_t epochSeconds, int16_t temperatureCentiC)
{
    flash_record_t record = {
        .magic = LOGGER_RECORD_MAGIC,
        .sequence = s_nextSequence,
        .epochSeconds = epochSeconds,
        .temperatureCentiC = temperatureCentiC,
        .checksum = 0U,
    };

    if (!record_is_erased(record_at(s_nextIndex)))
    {
        if (erase_sector_for_index(s_nextIndex) != kStatus_Success)
        {
            return kStatus_Fail;
        }
        scan_records();
    }

    record.sequence = s_nextSequence;
    record.checksum = record_checksum(&record);
    if (eeprom_emu_flashapi_write(
            LOGGER_BASE_ADDRESS + (s_nextIndex * LOGGER_RECORD_SIZE), &record, sizeof(record)) != 0)
    {
        return kStatus_Fail;
    }

    s_nextIndex = (s_nextIndex + 1U) % LOGGER_SLOT_COUNT;
    s_nextSequence++;
    if (s_recordCount < LOGGER_SLOT_COUNT)
    {
        s_recordCount++;
    }
    return kStatus_Success;
}

status_t FlashLogger_Clear(void)
{
    if (eeprom_emu_flashapi_erase(LOGGER_BASE_ADDRESS, LOGGER_FLASH_SIZE) != 0)
    {
        return kStatus_Fail;
    }
    s_nextIndex = 0U;
    s_nextSequence = 0U;
    s_recordCount = 0U;
    return kStatus_Success;
}

uint32_t FlashLogger_Count(void)
{
    return s_recordCount;
}

void FlashLogger_ForEach(flash_logger_visit_t visitor, void *context)
{
    bool found = false;
    uint32_t oldestIndex = 0U;
    uint32_t oldestSequence = 0U;

    if (visitor == NULL)
    {
        return;
    }

    for (uint32_t index = 0U; index < LOGGER_SLOT_COUNT; index++)
    {
        const flash_record_t *record = record_at(index);
        if (record_is_valid(record) && (!found || ((int32_t)(record->sequence - oldestSequence) < 0)))
        {
            found = true;
            oldestIndex = index;
            oldestSequence = record->sequence;
        }
    }

    for (uint32_t offset = 0U; found && (offset < LOGGER_SLOT_COUNT); offset++)
    {
        const flash_record_t *record = record_at((oldestIndex + offset) % LOGGER_SLOT_COUNT);
        if (record_is_valid(record))
        {
            flash_logger_sample_t sample = {
                .sequence = record->sequence,
                .epochSeconds = record->epochSeconds,
                .temperatureCentiC = record->temperatureCentiC,
            };
            visitor(&sample, context);
        }
    }
}
