#include "usbd_cdc_if.h"

#include "usb_cdc_port.h"

#include <stdarg.h>
#include <stdio.h>

uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

static USBD_CDC_LineCodingTypeDef line_coding =
{
    115200U,
    0U,
    0U,
    8U
};

extern USBD_HandleTypeDef hUsbDeviceFS;

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    CDC_TransmitCplt_FS
};

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0U);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
    return (USBD_OK);
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)length;

    switch (cmd)
    {
    case CDC_SEND_ENCAPSULATED_COMMAND:
    case CDC_GET_ENCAPSULATED_RESPONSE:
    case CDC_SET_COMM_FEATURE:
    case CDC_GET_COMM_FEATURE:
    case CDC_CLEAR_COMM_FEATURE:
        break;

    case CDC_SET_LINE_CODING:
        if (pbuf != NULL)
        {
            line_coding.bitrate = ((uint32_t)pbuf[0]) |
                                  ((uint32_t)pbuf[1] << 8) |
                                  ((uint32_t)pbuf[2] << 16) |
                                  ((uint32_t)pbuf[3] << 24);
            line_coding.format = pbuf[4];
            line_coding.paritytype = pbuf[5];
            line_coding.datatype = pbuf[6];
        }
        break;

    case CDC_GET_LINE_CODING:
        if (pbuf != NULL)
        {
            pbuf[0] = (uint8_t)(line_coding.bitrate);
            pbuf[1] = (uint8_t)(line_coding.bitrate >> 8);
            pbuf[2] = (uint8_t)(line_coding.bitrate >> 16);
            pbuf[3] = (uint8_t)(line_coding.bitrate >> 24);
            pbuf[4] = line_coding.format;
            pbuf[5] = line_coding.paritytype;
            pbuf[6] = line_coding.datatype;
        }
        break;

    case CDC_SET_CONTROL_LINE_STATE:
    case CDC_SEND_BREAK:
    default:
        break;
    }

    return (USBD_OK);
}

static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    if ((Buf != NULL) && (Len != NULL) && (*Len <= 0xFFFFUL))
    {
        UsbCdcPort_InputBytes(Buf, (uint16_t)(*Len));
    }

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    (void)USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (USBD_OK);
}

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
    USBD_CDC_HandleTypeDef *hcdc;

    if ((Buf == NULL) || (Len == 0U) || (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED))
    {
        return USBD_FAIL;
    }

    hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if ((hcdc == NULL) || (hcdc->TxState != 0U))
    {
        return USBD_BUSY;
    }

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}

void USB_printf(const char *format, ...)
{
    va_list args;
    int len;

    if (format == NULL)
    {
        return;
    }

    va_start(args, format);
    len = vsnprintf((char *)UserTxBufferFS, APP_TX_DATA_SIZE, format, args);
    va_end(args);

    if (len <= 0)
    {
        return;
    }

    if (len > APP_TX_DATA_SIZE)
    {
        len = APP_TX_DATA_SIZE;
    }

    (void)CDC_Transmit_FS(UserTxBufferFS, (uint16_t)len);
}

static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    (void)Buf;
    (void)Len;
    (void)epnum;
    return (USBD_OK);
}
