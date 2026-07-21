#ifndef UART_PORT_H
#define UART_PORT_H

#include "main.h"
#include <stdint.h>

#define APP_UART_FRAMEBUF_SIZE 255U
#define APP_UART_IDLELINE_TIME 10U

typedef enum
{
    APP_UART_1 = 0,
    APP_UART_2,
    APP_UART_3,
    APP_UART_4,
    APP_UART_5,
    APP_UART_6,
    APP_UART_7,
    APP_UART_8,
    APP_UART_COUNT
} AppUartPort;

typedef enum
{
    APP_UART_MODE_IDLE_FRAME = 0,
    APP_UART_MODE_STREAM
} AppUartMode_t;

extern uint8_t UART1_FrameBuff[APP_UART_FRAMEBUF_SIZE];
extern uint8_t UART2_FrameBuff[APP_UART_FRAMEBUF_SIZE];
extern uint8_t UART3_FrameBuff[APP_UART_FRAMEBUF_SIZE];
extern uint8_t UART4_FrameBuff[APP_UART_FRAMEBUF_SIZE];
extern uint8_t UART5_FrameBuff[APP_UART_FRAMEBUF_SIZE];
extern uint8_t UART6_FrameBuff[APP_UART_FRAMEBUF_SIZE];
extern uint8_t UART7_FrameBuff[APP_UART_FRAMEBUF_SIZE];
extern uint8_t UART8_FrameBuff[APP_UART_FRAMEBUF_SIZE];

extern volatile uint16_t UART1_RcvCnt;
extern volatile uint16_t UART2_RcvCnt;
extern volatile uint16_t UART3_RcvCnt;
extern volatile uint16_t UART4_RcvCnt;
extern volatile uint16_t UART5_RcvCnt;
extern volatile uint16_t UART6_RcvCnt;
extern volatile uint16_t UART7_RcvCnt;
extern volatile uint16_t UART8_RcvCnt;

extern volatile uint32_t timerRcvCnt1;
extern volatile uint32_t timerRcvCnt2;
extern volatile uint32_t timerRcvCnt3;
extern volatile uint32_t timerRcvCnt4;
extern volatile uint32_t timerRcvCnt5;
extern volatile uint32_t timerRcvCnt6;
extern volatile uint32_t timerRcvCnt7;
extern volatile uint32_t timerRcvCnt8;

void App_UART_Init(void);
void App_UART_SetMode(AppUartPort port, AppUartMode_t mode);
void App_UART_Tick1ms(void);
uint8_t App_UART_Process(void);
HAL_StatusTypeDef App_UART_Send(AppUartPort port, const uint8_t *data, uint16_t len);
void App_UART_SendByte(AppUartPort port, uint8_t data);
uint32_t App_UART_GetRxByteCount(AppUartPort port);
uint32_t App_UART_GetErrorCount(AppUartPort port);
uint16_t App_UART_GetStreamOverflowCount(AppUartPort port);
uint32_t App_UART_GetBaudRate(AppUartPort port);
HAL_StatusTypeDef App_UART_SetBaudRate(AppUartPort port, uint32_t baud_rate);

void UART1_SendByte(uint8_t data);
void UART2_SendByte(uint8_t data);
void UART3_SendByte(uint8_t data);
void UART4_SendByte(uint8_t data);
void UART5_SendByte(uint8_t data);
void USART6_SendByte(uint8_t data);
void UART7_SendByte(uint8_t data);
void UART8_SendByte(uint8_t data);

void UART1_printf(uint8_t *ptr, uint16_t len);
void UART2_printf(uint8_t *ptr, uint16_t len);
void UART3_printf(uint8_t *ptr, uint16_t len);
void UART4_printf(uint8_t *ptr, uint16_t len);
void UART5_printf(uint8_t *ptr, uint16_t len);
void UART6_printf(uint8_t *ptr, uint16_t len);
void UART7_printf(uint8_t *ptr, uint16_t len);
void UART8_printf(uint8_t *ptr, uint16_t len);

void UART1_rxCallback(uint8_t *packet, uint16_t size);
void UART2_rxCallback(uint8_t *packet, uint16_t size);
void UART3_rxCallback(uint8_t *packet, uint16_t size);
void UART4_rxCallback(uint8_t *packet, uint16_t size);
void UART5_rxCallback(uint8_t *packet, uint16_t size);
void UART6_rxCallback(uint8_t *packet, uint16_t size);
void UART7_rxCallback(uint8_t *packet, uint16_t size);
void UART8_rxCallback(uint8_t *packet, uint16_t size);

#endif
