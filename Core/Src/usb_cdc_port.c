#include "usb_cdc_port.h"

#include "usbd_cdc_if.h"
#include "usbd_core.h"

#include <string.h>

#define USB_CDC_RX_RING_SIZE      512U
#define USB_CDC_TX_TIMEOUT_MS     3U
#define USB_CDC_TX_STALL_MS       250U
#define STM32H7_SYSTEM_MEMORY     0x1FF09800UL
#define NVIC_REGISTER_COUNT       8U

static uint8_t usb_cdc_rx_ring[USB_CDC_RX_RING_SIZE];
static uint8_t usb_cdc_tx_buffer[APP_TX_DATA_SIZE];
static volatile uint16_t usb_cdc_rx_head = 0U;
static volatile uint16_t usb_cdc_rx_tail = 0U;
static volatile uint16_t usb_cdc_rx_overflow = 0U;
static uint32_t usb_cdc_tx_start_tick = 0U;
static uint8_t usb_cdc_tx_in_flight = 0U;
static volatile uint32_t usb_cdc_tx_recovery_count = 0U;

extern USBD_HandleTypeDef hUsbDeviceFS;

static uint16_t UsbCdcPort_NextIndex(uint16_t index)
{
    index++;
    if (index >= USB_CDC_RX_RING_SIZE)
    {
        index = 0U;
    }

    return index;
}

void UsbCdcPort_InputBytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint16_t next_head;

    if ((data == NULL) || (len == 0U))
    {
        return;
    }

    for (i = 0U; i < len; i++)
    {
        next_head = UsbCdcPort_NextIndex(usb_cdc_rx_head);
        if (next_head == usb_cdc_rx_tail)
        {
            usb_cdc_rx_overflow++;
            break;
        }

        usb_cdc_rx_ring[usb_cdc_rx_head] = data[i];
        usb_cdc_rx_head = next_head;
    }
}

uint16_t UsbCdcPort_Read(uint8_t *data, uint16_t max_len)
{
    uint32_t primask;
    uint16_t len;

    if ((data == NULL) || (max_len == 0U))
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    len = 0U;
    while ((usb_cdc_rx_tail != usb_cdc_rx_head) && (len < max_len))
    {
        data[len] = usb_cdc_rx_ring[usb_cdc_rx_tail];
        usb_cdc_rx_tail = UsbCdcPort_NextIndex(usb_cdc_rx_tail);
        len++;
    }

    if (primask == 0U)
    {
        __enable_irq();
    }

    return len;
}

HAL_StatusTypeDef UsbCdcPort_Send(const uint8_t *data, uint16_t len)
{
    USBD_CDC_HandleTypeDef *hcdc;
    uint32_t tick_start;
    uint32_t now;
    uint8_t status;

    if ((data == NULL) || (len == 0U) || (len > APP_TX_DATA_SIZE) ||
        (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED))
    {
        return HAL_ERROR;
    }

    hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (hcdc == NULL)
    {
        return HAL_ERROR;
    }

    now = HAL_GetTick();
    tick_start = now;
    while (hcdc->TxState != 0U)
    {
        if (usb_cdc_tx_in_flight == 0U)
        {
            usb_cdc_tx_start_tick = now;
            usb_cdc_tx_in_flight = 1U;
        }

        if ((now - usb_cdc_tx_start_tick) >= USB_CDC_TX_STALL_MS)
        {
            (void)USBD_LL_FlushEP(&hUsbDeviceFS, CDC_IN_EP);
            hcdc->TxState = 0U;
            usb_cdc_tx_in_flight = 0U;
            usb_cdc_tx_recovery_count++;
            return HAL_BUSY;
        }

        if ((HAL_GetTick() - tick_start) > USB_CDC_TX_TIMEOUT_MS)
        {
            return HAL_BUSY;
        }
        now = HAL_GetTick();
    }

    usb_cdc_tx_in_flight = 0U;
    memcpy(usb_cdc_tx_buffer, data, len);
    status = CDC_Transmit_FS(usb_cdc_tx_buffer, len);
    if (status == USBD_OK)
    {
        usb_cdc_tx_start_tick = HAL_GetTick();
        usb_cdc_tx_in_flight = 1U;
        return HAL_OK;
    }

    return HAL_BUSY;
}

uint32_t UsbCdcPort_GetTxRecoveryCount(void)
{
    return usb_cdc_tx_recovery_count;
}

void UsbCdcPort_JumpToDfuBootloader(void)
{
    typedef void (*BootloaderEntry_t)(void);
    uint32_t boot_stack;
    uint32_t boot_entry;
    uint32_t i;

    __disable_irq();
    (void)USBD_Stop(&hUsbDeviceFS);
    (void)HAL_DeInit();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    SCB_DisableICache();
    SCB_DisableDCache();

    for (i = 0U; i < NVIC_REGISTER_COUNT; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }

    SCB->VTOR = STM32H7_SYSTEM_MEMORY;

    boot_stack = *(__IO uint32_t *)STM32H7_SYSTEM_MEMORY;
    boot_entry = *(__IO uint32_t *)(STM32H7_SYSTEM_MEMORY + 4U);
    __set_CONTROL(0U);
    __set_MSP(boot_stack);
    __DSB();
    __ISB();
    __enable_irq();
    ((BootloaderEntry_t)boot_entry)();

    while (1)
    {
    }
}
