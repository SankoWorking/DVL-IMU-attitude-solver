#ifndef BOOT_CDC_IF_H
#define BOOT_CDC_IF_H

#include "usbd_cdc.h"
#include <stdint.h>

extern USBD_CDC_ItfTypeDef USBD_Boot_CDC_fops;

uint8_t Boot_CDC_Transmit(uint8_t *buf, uint16_t len);
void Boot_CDC_Log(const char *text);

#endif
