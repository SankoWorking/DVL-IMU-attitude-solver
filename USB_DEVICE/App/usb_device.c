#include "usb_device.h"

#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "usbd_core.h"
#include "usbd_desc.h"

USBD_HandleTypeDef hUsbDeviceFS;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

void MX_USB_DEVICE_Init(void)
{
    if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
    {
        Error_Handler();
    }
    (void)HAL_PCD_DevDisconnect(&hpcd_USB_OTG_FS);
    HAL_Delay(120U);
    if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK)
    {
        Error_Handler();
    }
    if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK)
    {
        Error_Handler();
    }
    if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
    {
        Error_Handler();
    }
}
