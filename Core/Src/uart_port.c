#include "uart_port.h"
#include <string.h>

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart5;
extern UART_HandleTypeDef huart6;
extern UART_HandleTypeDef huart7;
extern UART_HandleTypeDef huart8;

uint8_t UART1_FrameBuff[APP_UART_FRAMEBUF_SIZE];
uint8_t UART2_FrameBuff[APP_UART_FRAMEBUF_SIZE];
uint8_t UART3_FrameBuff[APP_UART_FRAMEBUF_SIZE];
uint8_t UART4_FrameBuff[APP_UART_FRAMEBUF_SIZE];
uint8_t UART5_FrameBuff[APP_UART_FRAMEBUF_SIZE];
uint8_t UART6_FrameBuff[APP_UART_FRAMEBUF_SIZE];
uint8_t UART7_FrameBuff[APP_UART_FRAMEBUF_SIZE];
uint8_t UART8_FrameBuff[APP_UART_FRAMEBUF_SIZE];

volatile uint16_t UART1_RcvCnt = 0;
volatile uint16_t UART2_RcvCnt = 0;
volatile uint16_t UART3_RcvCnt = 0;
volatile uint16_t UART4_RcvCnt = 0;
volatile uint16_t UART5_RcvCnt = 0;
volatile uint16_t UART6_RcvCnt = 0;
volatile uint16_t UART7_RcvCnt = 0;
volatile uint16_t UART8_RcvCnt = 0;

volatile uint32_t timerRcvCnt1 = 0;
volatile uint32_t timerRcvCnt2 = 0;
volatile uint32_t timerRcvCnt3 = 0;
volatile uint32_t timerRcvCnt4 = 0;
volatile uint32_t timerRcvCnt5 = 0;
volatile uint32_t timerRcvCnt6 = 0;
volatile uint32_t timerRcvCnt7 = 0;
volatile uint32_t timerRcvCnt8 = 0;

typedef struct
{
    UART_HandleTypeDef *huart;
    AppUartPort port;
    AppUartMode_t mode;
    uint8_t *frame;
    volatile uint16_t *count;
    volatile uint32_t *idle_count;
    volatile uint8_t frame_ready;
    volatile uint16_t ready_size;
    volatile uint16_t stream_head;
    volatile uint16_t stream_tail;
    volatile uint16_t stream_overflow;
    uint8_t rx_byte;
} AppUartContext;

static AppUartContext app_uarts[APP_UART_COUNT] =
{
    {&huart1, APP_UART_1, APP_UART_MODE_IDLE_FRAME, UART1_FrameBuff, &UART1_RcvCnt, &timerRcvCnt1, 0U, 0U, 0U, 0U, 0U, 0U},
    {&huart2, APP_UART_2, APP_UART_MODE_IDLE_FRAME, UART2_FrameBuff, &UART2_RcvCnt, &timerRcvCnt2, 0U, 0U, 0U, 0U, 0U, 0U},
    {&huart3, APP_UART_3, APP_UART_MODE_IDLE_FRAME, UART3_FrameBuff, &UART3_RcvCnt, &timerRcvCnt3, 0U, 0U, 0U, 0U, 0U, 0U},
    {&huart4, APP_UART_4, APP_UART_MODE_IDLE_FRAME, UART4_FrameBuff, &UART4_RcvCnt, &timerRcvCnt4, 0U, 0U, 0U, 0U, 0U, 0U},
    {&huart5, APP_UART_5, APP_UART_MODE_IDLE_FRAME, UART5_FrameBuff, &UART5_RcvCnt, &timerRcvCnt5, 0U, 0U, 0U, 0U, 0U, 0U},
    {&huart6, APP_UART_6, APP_UART_MODE_IDLE_FRAME, UART6_FrameBuff, &UART6_RcvCnt, &timerRcvCnt6, 0U, 0U, 0U, 0U, 0U, 0U},
    {&huart7, APP_UART_7, APP_UART_MODE_IDLE_FRAME, UART7_FrameBuff, &UART7_RcvCnt, &timerRcvCnt7, 0U, 0U, 0U, 0U, 0U, 0U},
    {&huart8, APP_UART_8, APP_UART_MODE_IDLE_FRAME, UART8_FrameBuff, &UART8_RcvCnt, &timerRcvCnt8, 0U, 0U, 0U, 0U, 0U, 0U},
};

static volatile uint32_t app_uart_rx_byte_count[APP_UART_COUNT];
static volatile uint32_t app_uart_error_count[APP_UART_COUNT];

static AppUartContext *App_UART_FindContext(UART_HandleTypeDef *huart)
{
    uint32_t i;

    for (i = 0; i < (uint32_t)APP_UART_COUNT; i++)
    {
        if (app_uarts[i].huart == huart || app_uarts[i].huart->Instance == huart->Instance)
        {
            return &app_uarts[i];
        }
    }

    return NULL;
}

static void App_UART_ResetFrame(AppUartContext *ctx)
{
    *ctx->count = 0U;
    *ctx->idle_count = 0U;
    ctx->frame_ready = 0U;
    ctx->ready_size = 0U;
    ctx->stream_head = 0U;
    ctx->stream_tail = 0U;
    ctx->stream_overflow = 0U;
}

static void App_UART_ClearFrame(AppUartContext *ctx)
{
    memset(ctx->frame, 0, APP_UART_FRAMEBUF_SIZE);
    App_UART_ResetFrame(ctx);
}

static void App_UART_MarkReady(AppUartContext *ctx)
{
    if ((ctx->frame_ready == 0U) && (*ctx->count != 0U))
    {
        ctx->ready_size = *ctx->count;
        ctx->frame_ready = 1U;
    }
}

static uint16_t App_UART_NextStreamIndex(uint16_t index)
{
    index++;
    if (index >= APP_UART_FRAMEBUF_SIZE)
    {
        index = 0U;
    }

    return index;
}

static void App_UART_PushStreamByte(AppUartContext *ctx, uint8_t byte)
{
    uint16_t next_head;

    next_head = App_UART_NextStreamIndex(ctx->stream_head);
    if (next_head == ctx->stream_tail)
    {
        ctx->stream_overflow++;
        return;
    }

    ctx->frame[ctx->stream_head] = byte;
    ctx->stream_head = next_head;
}

static uint16_t App_UART_ReadStream(AppUartContext *ctx, uint8_t *out, uint16_t max_len)
{
    uint16_t len;

    len = 0U;
    while ((ctx->stream_tail != ctx->stream_head) && (len < max_len))
    {
        out[len] = ctx->frame[ctx->stream_tail];
        ctx->stream_tail = App_UART_NextStreamIndex(ctx->stream_tail);
        len++;
    }

    return len;
}

static void App_UART_DispatchFrame(uint32_t index, uint8_t *frame, uint16_t size)
{
    if ((frame == NULL) || (size == 0U))
    {
        return;
    }

    switch ((AppUartPort)index)
    {
    case APP_UART_1:
        UART1_rxCallback(frame, size);
        break;

    case APP_UART_2:
        UART2_rxCallback(frame, size);
        break;

    case APP_UART_3:
        UART3_rxCallback(frame, size);
        break;

    case APP_UART_4:
        UART4_rxCallback(frame, size);
        break;

    case APP_UART_5:
        UART5_rxCallback(frame, size);
        break;

    case APP_UART_6:
        UART6_rxCallback(frame, size);
        break;

    case APP_UART_7:
        UART7_rxCallback(frame, size);
        break;

    case APP_UART_8:
        UART8_rxCallback(frame, size);
        break;

    default:
        break;
    }
}

static void App_UART_Rearm(AppUartContext *ctx)
{
    (void)HAL_UART_Receive_IT(ctx->huart, &ctx->rx_byte, 1U);
}

void App_UART_Init(void)
{
    app_uarts[APP_UART_1].mode = APP_UART_MODE_IDLE_FRAME;
    App_UART_ClearFrame(&app_uarts[APP_UART_1]);
    App_UART_Rearm(&app_uarts[APP_UART_1]);

    app_uarts[APP_UART_2].mode = APP_UART_MODE_IDLE_FRAME;
    App_UART_ClearFrame(&app_uarts[APP_UART_2]);
    App_UART_Rearm(&app_uarts[APP_UART_2]);

    app_uarts[APP_UART_4].mode = APP_UART_MODE_IDLE_FRAME;
    App_UART_ClearFrame(&app_uarts[APP_UART_4]);
    App_UART_Rearm(&app_uarts[APP_UART_4]);

    app_uarts[APP_UART_6].mode = APP_UART_MODE_STREAM;
    App_UART_ClearFrame(&app_uarts[APP_UART_6]);
    App_UART_Rearm(&app_uarts[APP_UART_6]);

    app_uarts[APP_UART_7].mode = APP_UART_MODE_STREAM;
    App_UART_ClearFrame(&app_uarts[APP_UART_7]);
    App_UART_Rearm(&app_uarts[APP_UART_7]);
}

void App_UART_SetMode(AppUartPort port, AppUartMode_t mode)
{
    uint32_t primask;

    if (port >= APP_UART_COUNT)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    app_uarts[port].mode = mode;
    App_UART_ClearFrame(&app_uarts[port]);

    if (primask == 0U)
    {
        __enable_irq();
    }
}

void App_UART_Tick1ms(void)
{
    uint32_t i;

    for (i = 0; i < (uint32_t)APP_UART_COUNT; i++)
    {
        if (app_uarts[i].mode == APP_UART_MODE_STREAM)
        {
            continue;
        }

        if ((app_uarts[i].frame_ready == 0U) && (*app_uarts[i].count != 0U))
        {
            if (*app_uarts[i].idle_count >= APP_UART_IDLELINE_TIME)
            {
                App_UART_MarkReady(&app_uarts[i]);
            }
            else
            {
                (*app_uarts[i].idle_count)++;
            }
        }
    }
}

uint8_t App_UART_Process(void)
{
		//存放从串口搬运出的数据
    uint8_t frame[APP_UART_FRAMEBUF_SIZE];
		//备份当前的中断屏蔽状态
    uint32_t primask;
		//循环索引
    uint32_t i;
		//当前串口读取到的数据长度
    uint16_t size;
		//统计本轮一共处理了多少个串口数据
    uint8_t processed;

    processed = 0U;
    for (i = 0; i < (uint32_t)APP_UART_COUNT; i++)
    {
        size = 0U;
        primask = __get_PRIMASK();
				//关闭中断，确保在搬运数据的过程中，没有任何其他中断可以打断。
        __disable_irq();

        if (app_uarts[i].mode == APP_UART_MODE_STREAM)
        {
            size = App_UART_ReadStream(&app_uarts[i], frame, (uint16_t)sizeof(frame));
        }
        else if (app_uarts[i].frame_ready != 0U)
        {
            size = app_uarts[i].ready_size;
            if (size > APP_UART_FRAMEBUF_SIZE)
            {
                size = APP_UART_FRAMEBUF_SIZE;
            }

            memcpy(frame, app_uarts[i].frame, size);
            App_UART_ResetFrame(&app_uarts[i]);
        }

        if (primask == 0U)
        {
            __enable_irq();
        }

        if (size != 0U)
        {
						//解析分离出来的帧
            App_UART_DispatchFrame(i, frame, size);
            processed++;
        }
    }

    return processed;
}

HAL_StatusTypeDef App_UART_Send(AppUartPort port, const uint8_t *data, uint16_t len)
{
    if (port >= APP_UART_COUNT || data == NULL || len == 0U)
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(app_uarts[port].huart, (uint8_t *)data, len, 100U);
}

void App_UART_SendByte(AppUartPort port, uint8_t data)
{
    (void)App_UART_Send(port, &data, 1U);
}

uint32_t App_UART_GetRxByteCount(AppUartPort port)
{
    return (port < APP_UART_COUNT) ? app_uart_rx_byte_count[port] : 0U;
}

uint32_t App_UART_GetErrorCount(AppUartPort port)
{
    return (port < APP_UART_COUNT) ? app_uart_error_count[port] : 0U;
}

uint16_t App_UART_GetStreamOverflowCount(AppUartPort port)
{
    return (port < APP_UART_COUNT) ? app_uarts[port].stream_overflow : 0U;
}

uint32_t App_UART_GetBaudRate(AppUartPort port)
{
    return (port < APP_UART_COUNT) ? app_uarts[port].huart->Init.BaudRate : 0U;
}

HAL_StatusTypeDef App_UART_SetBaudRate(AppUartPort port, uint32_t baud_rate)
{
    AppUartContext *ctx;
    HAL_StatusTypeDef status;

    if ((port >= APP_UART_COUNT) || (baud_rate == 0U))
    {
        return HAL_ERROR;
    }

    ctx = &app_uarts[port];
    (void)HAL_UART_AbortReceive(ctx->huart);
    ctx->huart->Init.BaudRate = baud_rate;
    status = HAL_UART_Init(ctx->huart);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_UARTEx_DisableFifoMode(ctx->huart);
    if (status != HAL_OK)
    {
        return status;
    }

    App_UART_ClearFrame(ctx);
    App_UART_Rearm(ctx);
    return HAL_OK;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    AppUartContext *ctx;
    uint16_t count;

    ctx = App_UART_FindContext(huart);
    if (ctx == NULL)
    {
        return;
    }

    app_uart_rx_byte_count[ctx->port]++;

    if (ctx->mode == APP_UART_MODE_STREAM)
    {
        uint8_t byte = ctx->rx_byte;
        App_UART_PushStreamByte(ctx, byte);
        App_UART_Rearm(ctx);
        return;
    }

    if (ctx->frame_ready == 0U)
    {
        count = *ctx->count;
        if (count < APP_UART_FRAMEBUF_SIZE)
        {
            ctx->frame[count] = ctx->rx_byte;
            count++;
            *ctx->count = count;
            *ctx->idle_count = 0U;
        }

        if (count >= APP_UART_FRAMEBUF_SIZE)
        {
            App_UART_MarkReady(ctx);
        }
    }

    App_UART_Rearm(ctx);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    AppUartContext *ctx;

    ctx = App_UART_FindContext(huart);
    if (ctx != NULL)
    {
        app_uart_error_count[ctx->port]++;
        App_UART_Rearm(ctx);
    }
}

void UART1_SendByte(uint8_t data) { App_UART_SendByte(APP_UART_1, data); }
void UART2_SendByte(uint8_t data) { App_UART_SendByte(APP_UART_2, data); }
void UART3_SendByte(uint8_t data) { App_UART_SendByte(APP_UART_3, data); }
void UART4_SendByte(uint8_t data) { App_UART_SendByte(APP_UART_4, data); }
void UART5_SendByte(uint8_t data) { App_UART_SendByte(APP_UART_5, data); }
void USART6_SendByte(uint8_t data) { App_UART_SendByte(APP_UART_6, data); }
void UART7_SendByte(uint8_t data) { App_UART_SendByte(APP_UART_7, data); }
void UART8_SendByte(uint8_t data) { App_UART_SendByte(APP_UART_8, data); }

void UART1_printf(uint8_t *ptr, uint16_t len) { (void)App_UART_Send(APP_UART_1, ptr, len); }
void UART2_printf(uint8_t *ptr, uint16_t len) { (void)App_UART_Send(APP_UART_2, ptr, len); }
void UART3_printf(uint8_t *ptr, uint16_t len) { (void)App_UART_Send(APP_UART_3, ptr, len); }
void UART4_printf(uint8_t *ptr, uint16_t len) { (void)App_UART_Send(APP_UART_4, ptr, len); }
void UART5_printf(uint8_t *ptr, uint16_t len) { (void)App_UART_Send(APP_UART_5, ptr, len); }
void UART6_printf(uint8_t *ptr, uint16_t len) { (void)App_UART_Send(APP_UART_6, ptr, len); }
void UART7_printf(uint8_t *ptr, uint16_t len) { (void)App_UART_Send(APP_UART_7, ptr, len); }
void UART8_printf(uint8_t *ptr, uint16_t len) { (void)App_UART_Send(APP_UART_8, ptr, len); }

__weak void UART1_rxCallback(uint8_t *packet, uint16_t size) { (void)packet; (void)size; }
__weak void UART2_rxCallback(uint8_t *packet, uint16_t size) { (void)packet; (void)size; }
__weak void UART3_rxCallback(uint8_t *packet, uint16_t size) { (void)packet; (void)size; }
__weak void UART4_rxCallback(uint8_t *packet, uint16_t size) { (void)packet; (void)size; }
__weak void UART5_rxCallback(uint8_t *packet, uint16_t size) { (void)packet; (void)size; }
__weak void UART6_rxCallback(uint8_t *packet, uint16_t size) { (void)packet; (void)size; }
__weak void UART7_rxCallback(uint8_t *packet, uint16_t size) { (void)packet; (void)size; }
__weak void UART8_rxCallback(uint8_t *packet, uint16_t size) { (void)packet; (void)size; }
