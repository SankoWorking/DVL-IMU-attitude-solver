#include "boot_msc_storage.h"

#include "boot_config.h"
#include "main.h"
#include <string.h>

#define FAT_RESERVED_SECTORS      1U
#define FAT_COUNT                 1U
#define FAT_SECTORS               2U
#define FAT_ROOT_ENTRIES          64U
#define FAT_ROOT_DIR_SECTORS      (((FAT_ROOT_ENTRIES * 32U) + (MSC_BLOCK_SIZE - 1U)) / MSC_BLOCK_SIZE)
#define FAT_ROOT_DIR_FIRST_SECTOR (FAT_RESERVED_SECTORS + (FAT_COUNT * FAT_SECTORS))
#define FAT_DATA_FIRST_SECTOR     (FAT_ROOT_DIR_FIRST_SECTOR + FAT_ROOT_DIR_SECTORS)
#define FAT_MEDIA_DESCRIPTOR      0xF8U
#define MSC_WRITE_SETTLE_MS       1500U

static int8_t STORAGE_Init_FS(uint8_t lun);
static int8_t STORAGE_GetCapacity_FS(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
static int8_t STORAGE_IsReady_FS(uint8_t lun);
static int8_t STORAGE_IsWriteProtected_FS(uint8_t lun);
static int8_t STORAGE_Read_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t STORAGE_Write_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t STORAGE_GetMaxLun_FS(void);
static void WriteLe16(uint8_t *dst, uint16_t value);
static void WriteLe32(uint8_t *dst, uint32_t value);
static uint8_t BufferContainsHexRecord(const uint8_t *buf, uint32_t len);

static int8_t STORAGE_Inquirydata_FS[] =
{
    0x00, 0x80, 0x02, 0x02,
    (STANDARD_INQUIRY_DATA_LEN - 5U),
    0x00, 0x00, 0x00,
    'H', '7', ' ', 'B', 'O', 'O', 'T', ' ',
    'H', 'E', 'X', ' ', 'D', 'R', 'I', 'V',
    'E', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    '0', '.', '1', ' '
};

__ALIGN_BEGIN static uint8_t msc_disk[MSC_DISK_SIZE_BYTES] __ALIGN_END;
static volatile uint8_t install_requested = 0U;
static volatile uint32_t last_write_tick = 0U;

USBD_StorageTypeDef USBD_Boot_MSC_fops =
{
    STORAGE_Init_FS,
    STORAGE_GetCapacity_FS,
    STORAGE_IsReady_FS,
    STORAGE_IsWriteProtected_FS,
    STORAGE_Read_FS,
    STORAGE_Write_FS,
    STORAGE_GetMaxLun_FS,
    STORAGE_Inquirydata_FS
};

static void BootMsc_InitDisk(void)
{
    uint8_t *boot_sector;
    uint8_t *fat;
    uint8_t *root_dir;

    memset(msc_disk, 0, sizeof(msc_disk));

    boot_sector = &msc_disk[0];
    boot_sector[0] = 0xEBU;
    boot_sector[1] = 0x3CU;
    boot_sector[2] = 0x90U;
    memcpy(&boot_sector[3], "MSDOS5.0", 8U);
    WriteLe16(&boot_sector[11], MSC_BLOCK_SIZE);
    boot_sector[13] = 1U;
    WriteLe16(&boot_sector[14], FAT_RESERVED_SECTORS);
    boot_sector[16] = FAT_COUNT;
    WriteLe16(&boot_sector[17], FAT_ROOT_ENTRIES);
    WriteLe16(&boot_sector[19], MSC_BLOCK_COUNT);
    boot_sector[21] = FAT_MEDIA_DESCRIPTOR;
    WriteLe16(&boot_sector[22], FAT_SECTORS);
    WriteLe16(&boot_sector[24], 1U);
    WriteLe16(&boot_sector[26], 1U);
    WriteLe32(&boot_sector[28], 0U);
    WriteLe32(&boot_sector[32], 0U);
    boot_sector[36] = 0x80U;
    boot_sector[38] = 0x29U;
    WriteLe32(&boot_sector[39], 0x20260629UL);
    memcpy(&boot_sector[43], "H7BOOT     ", 11U);
    memcpy(&boot_sector[54], "FAT12   ", 8U);
    boot_sector[510] = 0x55U;
    boot_sector[511] = 0xAAU;

    fat = &msc_disk[FAT_RESERVED_SECTORS * MSC_BLOCK_SIZE];
    fat[0] = FAT_MEDIA_DESCRIPTOR;
    fat[1] = 0xFFU;
    fat[2] = 0xFFU;

    root_dir = &msc_disk[FAT_ROOT_DIR_FIRST_SECTOR * MSC_BLOCK_SIZE];
    memcpy(&root_dir[0], "H7BOOT     ", 11U);
    root_dir[11] = 0x08U;
}

static int8_t STORAGE_Init_FS(uint8_t lun)
{
    (void)lun;
    BootMsc_InitDisk();
    return 0;
}

static int8_t STORAGE_GetCapacity_FS(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{
    (void)lun;
    *block_num = MSC_BLOCK_COUNT - 1U;
    *block_size = MSC_BLOCK_SIZE;
    return 0;
}

static int8_t STORAGE_IsReady_FS(uint8_t lun)
{
    (void)lun;
    return 0;
}

static int8_t STORAGE_IsWriteProtected_FS(uint8_t lun)
{
    (void)lun;
    return 0;
}

static int8_t STORAGE_Read_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
    uint32_t offset;
    uint32_t len;

    (void)lun;
    offset = blk_addr * MSC_BLOCK_SIZE;
    len = (uint32_t)blk_len * MSC_BLOCK_SIZE;
    if ((offset + len) > sizeof(msc_disk))
    {
        return -1;
    }

    memcpy(buf, &msc_disk[offset], len);
    return 0;
}

static int8_t STORAGE_Write_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
    uint32_t offset;
    uint32_t len;

    (void)lun;
    offset = blk_addr * MSC_BLOCK_SIZE;
    len = (uint32_t)blk_len * MSC_BLOCK_SIZE;
    if ((offset + len) > sizeof(msc_disk))
    {
        return -1;
    }

    memcpy(&msc_disk[offset], buf, len);
    if ((install_requested != 0U) || (BufferContainsHexRecord(buf, len) != 0U))
    {
        install_requested = 1U;
        last_write_tick = HAL_GetTick();
    }
    return 0;
}

static int8_t STORAGE_GetMaxLun_FS(void)
{
    return 0;
}

uint8_t BootMsc_IsInstallRequested(void)
{
    if (install_requested == 0U)
    {
        return 0U;
    }

    return ((HAL_GetTick() - last_write_tick) >= MSC_WRITE_SETTLE_MS) ? 1U : 0U;
}

void BootMsc_ClearInstallRequest(void)
{
    install_requested = 0U;
    last_write_tick = 0U;
}

const uint8_t *BootMsc_GetDiskData(void)
{
    return msc_disk;
}

uint32_t BootMsc_GetDiskSize(void)
{
    return (uint32_t)sizeof(msc_disk);
}

static void WriteLe16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
}

static void WriteLe32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static uint8_t BufferContainsHexRecord(const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    if (buf == NULL)
    {
        return 0U;
    }

    for (i = 0U; i < len; i++)
    {
        if (buf[i] == ':')
        {
            return 1U;
        }
    }

    return 0U;
}
