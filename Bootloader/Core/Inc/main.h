#ifndef BOOTLOADER_MAIN_H
#define BOOTLOADER_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

void Error_Handler(void);
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif
