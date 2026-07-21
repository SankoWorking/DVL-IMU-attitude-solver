#ifndef USB_CDC_PORT_H
#define USB_CDC_PORT_H

#include "main.h"
#include <stdint.h>

void UsbCdcPort_InputBytes(const uint8_t *data, uint16_t len);
uint16_t UsbCdcPort_Read(uint8_t *data, uint16_t max_len);
HAL_StatusTypeDef UsbCdcPort_Send(const uint8_t *data, uint16_t len);
uint32_t UsbCdcPort_GetTxRecoveryCount(void);
void UsbCdcPort_JumpToDfuBootloader(void);

#endif
