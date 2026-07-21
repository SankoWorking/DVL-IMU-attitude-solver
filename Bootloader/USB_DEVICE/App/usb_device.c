#include "usb_device.h"

#include "boot_cdc_if.h"
#include "boot_msc_storage.h"
#include "main.h"
#include "usbd_cdc.h"
#include "usbd_composite_builder.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_msc.h"

USBD_HandleTypeDef hUsbDeviceFS;

#define BOOT_CDC_CLASS_ID    0U
#define BOOT_MSC_CLASS_ID    1U

static uint8_t cdc_ep_add[] = {0x81U, 0x01U, 0x82U};
static uint8_t msc_ep_add[] = {0x83U, 0x03U};

void MX_USB_DEVICE_Init(void)
{
    if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
    {
        Error_Handler();
    }

    if (USBD_RegisterClassComposite(&hUsbDeviceFS, &USBD_CDC, CLASS_TYPE_CDC, cdc_ep_add) != USBD_OK)
    {
        Error_Handler();
    }

    hUsbDeviceFS.classId = BOOT_CDC_CLASS_ID;
    if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Boot_CDC_fops) != USBD_OK)
    {
        Error_Handler();
    }

    hUsbDeviceFS.classId = hUsbDeviceFS.NumClasses;
    if (USBD_RegisterClassComposite(&hUsbDeviceFS, &USBD_MSC, CLASS_TYPE_MSC, msc_ep_add) != USBD_OK)
    {
        Error_Handler();
    }

    hUsbDeviceFS.classId = BOOT_MSC_CLASS_ID;
    if (USBD_MSC_RegisterStorage(&hUsbDeviceFS, &USBD_Boot_MSC_fops) != USBD_OK)
    {
        Error_Handler();
    }

    hUsbDeviceFS.classId = BOOT_CDC_CLASS_ID;
    if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
    {
        Error_Handler();
    }
}

void MX_USB_DEVICE_DeInit(void)
{
    (void)USBD_Stop(&hUsbDeviceFS);
    (void)USBD_DeInit(&hUsbDeviceFS);
}
