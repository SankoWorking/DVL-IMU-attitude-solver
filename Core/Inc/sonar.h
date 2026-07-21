#ifndef SONAR_H
#define SONAR_H

#include "main.h"
#include "sonar_filter.h"
#include <stdint.h>

#define DYP_A23_UART_FRAME_HEADER  0xFFU
#define DYP_A23_UART_FRAME_SIZE    6U
#define SONAR_UART_FRAME_SIZE      4U
#define DYP_A23_TRIGGER_BYTE       0xFFU

void Sonic_Trigger(uint8_t uart_ch);
uint8_t Sonar_InputFrame(Sonar_t *sonar, const uint8_t *packet, uint16_t size);

#endif
