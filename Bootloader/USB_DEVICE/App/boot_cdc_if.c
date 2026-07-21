#include "boot_cdc_if.h"

#include "usbd_cdc.h"
#include "usbd_core.h"
#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceFS;

#define BOOT_CDC_CLASS_ID    0U

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

static uint8_t UserRxBufferFS[CDC_DATA_FS_OUT_PACKET_SIZE];
static uint8_t UserTxBufferFS[CDC_DATA_FS_IN_PACKET_SIZE];

USBD_CDC_ItfTypeDef USBD_Boot_CDC_fops =
{
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    CDC_TransmitCplt_FS
};

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0U, BOOT_CDC_CLASS_ID);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
    return (USBD_OK);
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)cmd;
    (void)pbuf;
    (void)length;
    return (USBD_OK);
}

static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    (void)Buf;
    (void)Len;
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    (void)USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (USBD_OK);
}

uint8_t Boot_CDC_Transmit(uint8_t *buf, uint16_t len)
{
    USBD_CDC_HandleTypeDef *hcdc;

    hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassDataCmsit[BOOT_CDC_CLASS_ID];
    if (hcdc == NULL)
    {
        return USBD_FAIL;
    }

    if (hcdc->TxState != 0U)
    {
        return USBD_BUSY;
    }

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, buf, len, BOOT_CDC_CLASS_ID);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS, BOOT_CDC_CLASS_ID);
}

void Boot_CDC_Log(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    (void)Boot_CDC_Transmit((uint8_t *)text, (uint16_t)strlen(text));
}

static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    (void)Buf;
    (void)Len;
    (void)epnum;
    return (USBD_OK);
}

void UsbCdcPort_JumpToDfuBootloader(void)
{
}
