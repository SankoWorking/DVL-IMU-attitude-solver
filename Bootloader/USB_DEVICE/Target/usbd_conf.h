#ifndef BOOTLOADER_USBD_CONF_H
#define BOOTLOADER_USBD_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_USBD_COMPOSITE
#define USBD_CMPSIT_ACTIVATE_CDC       1U
#define USBD_CMPSIT_ACTIVATE_MSC       1U
#define USBD_COMPOSITE_USE_IAD         1U
#define USBD_CMPST_MAX_CONFDESC_SZ     256U

#define USBD_MAX_NUM_INTERFACES        3U
#define USBD_MAX_NUM_CONFIGURATION     1U
#define USBD_MAX_STR_DESC_SIZ          512U
#define USBD_DEBUG_LEVEL               0U
#define USBD_LPM_ENABLED               0U
#define USBD_SELF_POWERED              1U
#define USBD_MAX_POWER                 0x32U

#define MSC_MEDIA_PACKET               512U
#define MSC_BOT_MAX_LUN                1U

#define DEVICE_FS                      0
#define DEVICE_HS                      1

#define USBD_malloc                    (void *)USBD_static_malloc
#define USBD_free                      USBD_static_free
#define USBD_memset                    memset
#define USBD_memcpy                    memcpy
#define USBD_Delay                     HAL_Delay

#define USBD_UsrLog(...)
#define USBD_ErrLog(...)
#define USBD_DbgLog(...)

void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#ifdef __cplusplus
}
#endif

#endif
