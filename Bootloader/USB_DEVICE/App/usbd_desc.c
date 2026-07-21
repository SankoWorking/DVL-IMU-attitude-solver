#include "usbd_desc.h"
#include "usbd_conf.h"
#include "usbd_ctlreq.h"

#define USBD_VID                       1155U
#define USBD_PID_FS                    22337U
#define USBD_LANGID_STRING             1033U
#define USBD_MANUFACTURER_STRING       "H7 Luxiaoban"
#define USBD_PRODUCT_STRING_FS         "H7 Luxiaoban Bootloader"
#define USBD_CONFIGURATION_STRING_FS   "CDC MSC Config"
#define USBD_INTERFACE_STRING_FS       "CDC MSC Interface"

static void Get_SerialNum(void);
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len);

uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef FS_Desc =
{
    USBD_FS_DeviceDescriptor,
    USBD_FS_LangIDStrDescriptor,
    USBD_FS_ManufacturerStrDescriptor,
    USBD_FS_ProductStrDescriptor,
    USBD_FS_SerialStrDescriptor,
    USBD_FS_ConfigStrDescriptor,
    USBD_FS_InterfaceStrDescriptor
};

__ALIGN_BEGIN uint8_t USBD_FS_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END =
{
    0x12,
    USB_DESC_TYPE_DEVICE,
    0x00, 0x02,
    0xEF,
    0x02,
    0x01,
    USB_MAX_EP0_SIZE,
    LOBYTE(USBD_VID),
    HIBYTE(USBD_VID),
    LOBYTE(USBD_PID_FS),
    HIBYTE(USBD_PID_FS),
    0x00, 0x02,
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION
};

__ALIGN_BEGIN uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING)
};

__ALIGN_BEGIN uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;
__ALIGN_BEGIN uint8_t USBD_StringSerial[USB_SIZ_STRING_SERIAL] __ALIGN_END =
{
    USB_SIZ_STRING_SERIAL,
    USB_DESC_TYPE_STRING,
};

uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_FS_DeviceDesc);
    return USBD_FS_DeviceDesc;
}

uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_PRODUCT_STRING_FS, USBD_StrDesc, length);
    return USBD_StrDesc;
}

uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = USB_SIZ_STRING_SERIAL;
    Get_SerialNum();
    return USBD_StringSerial;
}

uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_CONFIGURATION_STRING_FS, USBD_StrDesc, length);
    return USBD_StrDesc;
}

uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_INTERFACE_STRING_FS, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static void Get_SerialNum(void)
{
    uint32_t deviceserial0;
    uint32_t deviceserial1;
    uint32_t deviceserial2;

    deviceserial0 = *(uint32_t *)DEVICE_ID1;
    deviceserial1 = *(uint32_t *)DEVICE_ID2;
    deviceserial2 = *(uint32_t *)DEVICE_ID3;
    deviceserial0 += deviceserial2;
    if (deviceserial0 != 0U)
    {
        IntToUnicode(deviceserial0, &USBD_StringSerial[2], 8U);
        IntToUnicode(deviceserial1, &USBD_StringSerial[18], 4U);
    }
}

static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
    uint8_t idx;

    for (idx = 0U; idx < len; idx++)
    {
        if (((value >> 28)) < 0xAU)
        {
            pbuf[2U * idx] = (uint8_t)((value >> 28) + '0');
        }
        else
        {
            pbuf[2U * idx] = (uint8_t)((value >> 28) + 'A' - 10U);
        }
        value <<= 4;
        pbuf[(2U * idx) + 1U] = 0U;
    }
}
