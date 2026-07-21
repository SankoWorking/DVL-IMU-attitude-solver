#ifndef BOOT_MSC_STORAGE_H
#define BOOT_MSC_STORAGE_H

#include "usbd_msc.h"
#include <stdint.h>

extern USBD_StorageTypeDef USBD_Boot_MSC_fops;

uint8_t BootMsc_IsInstallRequested(void);
void BootMsc_ClearInstallRequest(void);
const uint8_t *BootMsc_GetDiskData(void);
uint32_t BootMsc_GetDiskSize(void);

#endif
