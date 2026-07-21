#ifndef BOOT_FLASH_H
#define BOOT_FLASH_H

#include <stdint.h>

typedef enum
{
    BOOT_FLASH_OK = 0,
    BOOT_FLASH_ERR_FORMAT,
    BOOT_FLASH_ERR_RANGE,
    BOOT_FLASH_ERR_ERASE,
    BOOT_FLASH_ERR_PROGRAM,
    BOOT_FLASH_ERR_VERIFY,
    BOOT_FLASH_ERR_NO_APP,
} BootFlashStatus;

BootFlashStatus BootFlash_InstallHexImage(const uint8_t *data, uint32_t len);
uint8_t BootFlash_IsAppValid(void);
void BootFlash_JumpToApp(void);

#endif
