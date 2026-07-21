#include "boot_flash.h"

#include "boot_config.h"
#include "main.h"
#include "usbd_core.h"

#include <string.h>

#define FLASHWORD_SIZE       32U
#define RAM_START            0x20000000UL
#define RAM_END              0x20020000UL
#define AXI_RAM_START        0x24000000UL
#define AXI_RAM_END          0x24080000UL

typedef void (*AppEntry)(void);

static uint8_t HexNibble(uint8_t ch, uint8_t *value);
static uint8_t HexByte(const uint8_t *text, uint8_t *value);
static BootFlashStatus EraseAppArea(uint32_t min_addr, uint32_t max_addr);
static BootFlashStatus FlashWriter_Begin(void);
static BootFlashStatus FlashWriter_Write(uint32_t addr, const uint8_t *data, uint32_t len);
static BootFlashStatus FlashWriter_Flush(void);
static uint8_t IsAddressInApp(uint32_t addr, uint32_t len);
static uint32_t SectorFromAddress(uint32_t addr);
static uint32_t BankFromAddress(uint32_t addr);
static uint8_t IsStackPointerValid(uint32_t sp);

static uint32_t fw_base_addr;
static uint8_t fw_buf[FLASHWORD_SIZE];
static uint8_t fw_dirty;

BootFlashStatus BootFlash_InstallHexImage(const uint8_t *data, uint32_t len)
{
    uint32_t i = 0U;
    uint32_t ext_linear = 0U;
    uint32_t min_addr = 0xFFFFFFFFUL;
    uint32_t max_addr = 0U;
    uint8_t saw_eof = 0U;
    BootFlashStatus status;

    if ((data == NULL) || (len == 0U))
    {
        return BOOT_FLASH_ERR_FORMAT;
    }

    while (i < len)
    {
        uint8_t count;
        uint8_t type;
        uint8_t checksum;
        uint8_t sum;
        uint16_t offset;
        uint32_t record_addr;
        uint32_t j;

        while ((i < len) && (data[i] != ':'))
        {
            i++;
        }
        if (i >= len)
        {
            break;
        }
        i++;

        if ((i + 10U) > len)
        {
            return BOOT_FLASH_ERR_FORMAT;
        }

        {
            uint8_t hi;
            uint8_t lo;
            if ((HexByte(&data[i], &count) == 0U) ||
                (HexByte(&data[i + 2U], &hi) == 0U) ||
                (HexByte(&data[i + 4U], &lo) == 0U) ||
                (HexByte(&data[i + 6U], &type) == 0U))
            {
                return BOOT_FLASH_ERR_FORMAT;
            }
            offset = (uint16_t)(((uint16_t)hi << 8) | lo);
        }

        sum = count + (uint8_t)(offset >> 8) + (uint8_t)offset + type;
        i += 8U;

        if ((i + ((uint32_t)count * 2U) + 2U) > len)
        {
            return BOOT_FLASH_ERR_FORMAT;
        }

        for (j = 0U; j < count; j++)
        {
            uint8_t b;
            if (HexByte(&data[i + (j * 2U)], &b) == 0U)
            {
                return BOOT_FLASH_ERR_FORMAT;
            }
            sum = (uint8_t)(sum + b);
        }

        if (HexByte(&data[i + ((uint32_t)count * 2U)], &checksum) == 0U)
        {
            return BOOT_FLASH_ERR_FORMAT;
        }
        sum = (uint8_t)(sum + checksum);
        if (sum != 0U)
        {
            return BOOT_FLASH_ERR_FORMAT;
        }

        record_addr = ext_linear + offset;
        if (type == 0x00U)
        {
            if (IsAddressInApp(record_addr, count) == 0U)
            {
                return BOOT_FLASH_ERR_RANGE;
            }

            if (record_addr < min_addr)
            {
                min_addr = record_addr;
            }
            if ((record_addr + count) > max_addr)
            {
                max_addr = record_addr + count;
            }
        }
        else if (type == 0x01U)
        {
            saw_eof = 1U;
            break;
        }
        else if (type == 0x04U)
        {
            uint8_t hi;
            uint8_t lo;

            if (count != 2U)
            {
                return BOOT_FLASH_ERR_FORMAT;
            }
            (void)HexByte(&data[i], &hi);
            (void)HexByte(&data[i + 2U], &lo);
            ext_linear = (((uint32_t)hi << 8) | lo) << 16;
        }

        i += ((uint32_t)count * 2U) + 2U;
    }

    if ((saw_eof == 0U) || (min_addr == 0xFFFFFFFFUL))
    {
        return BOOT_FLASH_ERR_FORMAT;
    }

    status = EraseAppArea(min_addr, max_addr);
    if (status != BOOT_FLASH_OK)
    {
        return status;
    }

    status = FlashWriter_Begin();
    if (status != BOOT_FLASH_OK)
    {
        return status;
    }

    i = 0U;
    ext_linear = 0U;
    while (i < len)
    {
        uint8_t count;
        uint8_t type;
        uint16_t offset;
        uint32_t record_addr;
        uint32_t j;

        while ((i < len) && (data[i] != ':'))
        {
            i++;
        }
        if (i >= len)
        {
            break;
        }
        i++;

        {
            uint8_t hi;
            uint8_t lo;
            (void)HexByte(&data[i], &count);
            (void)HexByte(&data[i + 2U], &hi);
            (void)HexByte(&data[i + 4U], &lo);
            offset = (uint16_t)(((uint16_t)hi << 8) | lo);
            (void)HexByte(&data[i + 6U], &type);
        }
        i += 8U;
        record_addr = ext_linear + offset;

        if (type == 0x00U)
        {
            uint8_t bytes[255];
            for (j = 0U; j < count; j++)
            {
                (void)HexByte(&data[i + (j * 2U)], &bytes[j]);
            }

            status = FlashWriter_Write(record_addr, bytes, count);
            if (status != BOOT_FLASH_OK)
            {
                HAL_FLASH_Lock();
                return status;
            }
        }
        else if (type == 0x01U)
        {
            break;
        }
        else if (type == 0x04U)
        {
            uint8_t hi;
            uint8_t lo;
            (void)HexByte(&data[i], &hi);
            (void)HexByte(&data[i + 2U], &lo);
            ext_linear = (((uint32_t)hi << 8) | lo) << 16;
        }

        i += ((uint32_t)count * 2U) + 2U;
    }

    status = FlashWriter_Flush();
    HAL_FLASH_Lock();
    if (status != BOOT_FLASH_OK)
    {
        return status;
    }

    SCB_InvalidateICache();

    return BootFlash_IsAppValid() ? BOOT_FLASH_OK : BOOT_FLASH_ERR_NO_APP;
}

uint8_t BootFlash_IsAppValid(void)
{
    uint32_t sp = *(uint32_t *)APP_FLASH_BASE;
    uint32_t pc = *(uint32_t *)(APP_FLASH_BASE + 4U);

    if (IsStackPointerValid(sp) == 0U)
    {
        return 0U;
    }
    if ((pc < APP_FLASH_BASE) || (pc >= APP_FLASH_END))
    {
        return 0U;
    }

    return 1U;
}

void BootFlash_JumpToApp(void)
{
    uint32_t app_sp;
    uint32_t app_reset;
    AppEntry app_entry;

    if (BootFlash_IsAppValid() == 0U)
    {
        return;
    }

    app_sp = *(uint32_t *)APP_FLASH_BASE;
    app_reset = *(uint32_t *)(APP_FLASH_BASE + 4U);
    app_entry = (AppEntry)app_reset;

    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

    for (uint32_t i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }

    HAL_MPU_Disable();
    SCB->SHCSR = 0U;
    SCB->CFSR = 0xFFFFFFFFUL;
    SCB->HFSR = 0xFFFFFFFFUL;
    SCB->DFSR = 0xFFFFFFFFUL;
    SCB->VTOR = APP_FLASH_BASE;
    __set_CONTROL(0U);
    __set_PSP(0U);
    __set_BASEPRI(0U);
    __set_FAULTMASK(0U);
    __set_MSP(app_sp);
    __DSB();
    __ISB();
    app_entry();
}

static BootFlashStatus EraseAppArea(uint32_t min_addr, uint32_t max_addr)
{
    uint32_t sector_error = 0U;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t first_sector = SectorFromAddress(min_addr);
    uint32_t last_sector = SectorFromAddress(max_addr - 1U);

    if ((min_addr < APP_FLASH_BASE) || (max_addr > APP_FLASH_END) || (min_addr >= max_addr))
    {
        return BOOT_FLASH_ERR_RANGE;
    }

    HAL_FLASH_Unlock();

    if (BankFromAddress(min_addr) == FLASH_BANK_1)
    {
        erase.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase.Banks = FLASH_BANK_1;
        erase.Sector = first_sector;
        erase.NbSectors = ((last_sector >= first_sector) ? (last_sector - first_sector + 1U) : (FLASH_SECTOR_TOTAL - first_sector));
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
        {
            return BOOT_FLASH_ERR_ERASE;
        }
    }

    if (max_addr > FLASH_BANK2_BASE)
    {
        uint32_t bank2_first = (min_addr < FLASH_BANK2_BASE) ? 0U : SectorFromAddress(min_addr);
        uint32_t bank2_last = SectorFromAddress(max_addr - 1U);

        erase.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase.Banks = FLASH_BANK_2;
        erase.Sector = bank2_first;
        erase.NbSectors = bank2_last - bank2_first + 1U;
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
        {
            return BOOT_FLASH_ERR_ERASE;
        }
    }

    return BOOT_FLASH_OK;
}

static BootFlashStatus FlashWriter_Begin(void)
{
    HAL_FLASH_Unlock();
    fw_base_addr = 0U;
    fw_dirty = 0U;
    memset(fw_buf, 0xFF, sizeof(fw_buf));
    return BOOT_FLASH_OK;
}

static BootFlashStatus FlashWriter_Write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    if ((data == NULL) || (IsAddressInApp(addr, len) == 0U))
    {
        return BOOT_FLASH_ERR_RANGE;
    }

    for (i = 0U; i < len; i++)
    {
        uint32_t aligned = (addr + i) & ~(FLASHWORD_SIZE - 1U);
        uint32_t offset = (addr + i) - aligned;

        if ((fw_dirty != 0U) && (fw_base_addr != aligned))
        {
            BootFlashStatus status = FlashWriter_Flush();
            if (status != BOOT_FLASH_OK)
            {
                return status;
            }
        }

        if (fw_dirty == 0U)
        {
            fw_base_addr = aligned;
            memset(fw_buf, 0xFF, sizeof(fw_buf));
            fw_dirty = 1U;
        }

        fw_buf[offset] = data[i];
    }

    return BOOT_FLASH_OK;
}

static BootFlashStatus FlashWriter_Flush(void)
{
    if (fw_dirty == 0U)
    {
        return BOOT_FLASH_OK;
    }

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, fw_base_addr, (uint32_t)fw_buf) != HAL_OK)
    {
        return BOOT_FLASH_ERR_PROGRAM;
    }

    if (memcmp((const void *)fw_base_addr, fw_buf, sizeof(fw_buf)) != 0)
    {
        return BOOT_FLASH_ERR_VERIFY;
    }

    fw_dirty = 0U;
    memset(fw_buf, 0xFF, sizeof(fw_buf));
    return BOOT_FLASH_OK;
}

static uint8_t IsAddressInApp(uint32_t addr, uint32_t len)
{
    if (len == 0U)
    {
        return 1U;
    }

    if (addr < APP_FLASH_BASE)
    {
        return 0U;
    }

    if ((addr + len) < addr)
    {
        return 0U;
    }

    if ((addr + len) > APP_FLASH_END)
    {
        return 0U;
    }

    return 1U;
}

static uint32_t SectorFromAddress(uint32_t addr)
{
    if (addr >= FLASH_BANK2_BASE)
    {
        return (addr - FLASH_BANK2_BASE) / STM32H743_FLASH_SECTOR_SZ;
    }

    return (addr - FLASH_BANK1_BASE) / STM32H743_FLASH_SECTOR_SZ;
}

static uint32_t BankFromAddress(uint32_t addr)
{
    return (addr >= FLASH_BANK2_BASE) ? FLASH_BANK_2 : FLASH_BANK_1;
}

static uint8_t IsStackPointerValid(uint32_t sp)
{
    if ((sp >= RAM_START) && (sp < RAM_END))
    {
        return 1U;
    }
    if ((sp >= AXI_RAM_START) && (sp < AXI_RAM_END))
    {
        return 1U;
    }
    return 0U;
}

static uint8_t HexNibble(uint8_t ch, uint8_t *value)
{
    if ((ch >= '0') && (ch <= '9'))
    {
        *value = (uint8_t)(ch - '0');
        return 1U;
    }
    if ((ch >= 'A') && (ch <= 'F'))
    {
        *value = (uint8_t)(ch - 'A' + 10U);
        return 1U;
    }
    if ((ch >= 'a') && (ch <= 'f'))
    {
        *value = (uint8_t)(ch - 'a' + 10U);
        return 1U;
    }
    return 0U;
}

static uint8_t HexByte(const uint8_t *text, uint8_t *value)
{
    uint8_t hi;
    uint8_t lo;

    if ((HexNibble(text[0], &hi) == 0U) || (HexNibble(text[1], &lo) == 0U))
    {
        return 0U;
    }

    *value = (uint8_t)((hi << 4) | lo);
    return 1U;
}
