#ifndef BOOTLOADER_USBD_DESC_H
#define BOOTLOADER_USBD_DESC_H

#include "usbd_def.h"

#define USB_SIZ_STRING_SERIAL       0x1AU
#define DEVICE_ID1                  UID_BASE
#define DEVICE_ID2                  (UID_BASE + 0x4U)
#define DEVICE_ID3                  (UID_BASE + 0x8U)

extern USBD_DescriptorsTypeDef FS_Desc;

#endif
