#ifndef APP_CONTROL_H
#define APP_CONTROL_H

#include "main.h"
#include <stdint.h>

extern uint16_t g_sonic_uart2;
extern uint16_t g_sonic_uart4;
extern uint16_t g_sonar1_mm;
extern uint16_t g_sonar2_mm;
extern uint16_t g_sonic_uart5;
extern uint16_t g_sonic_uart6;
extern uint16_t g_sonar_front_mm;
extern uint16_t g_sonar_right_mm;
extern uint16_t g_sonar_left_mm;
extern uint16_t g_sonar_rear_mm;
extern uint16_t g_tf_uw500_distance_mm;
extern uint16_t g_vls_front_mm;
extern uint16_t g_vls_right_mm;
extern uint16_t g_vls_left_mm;
extern uint16_t g_vls_rear_mm;

void App_Tasks_Init(void);
void Log_Printf(const char *format, ...);

#endif
