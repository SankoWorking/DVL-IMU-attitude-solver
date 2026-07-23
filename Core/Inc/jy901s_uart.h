#ifndef JY901S_UART_H
#define JY901S_UART_H

#include "main.h"
#include <stdint.h>

#define JY901S_UART_FRAME_SIZE       11U
#define JY901S_UART_VALID_ACCEL      0x01U
#define JY901S_UART_VALID_GYRO       0x02U
#define JY901S_UART_VALID_ANGLE      0x04U
#define JY901S_UART_VALID_MAG        0x08U

typedef struct
{
    int16_t accel_raw[3];
    int16_t gyro_raw[3];
    int16_t angle_raw[3];
    int16_t mag_raw[3];
    int16_t temperature_raw;
    float accel_g[3];
    float gyro_dps[3];
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    float temperature_c;
    uint8_t valid_mask;
    uint32_t frame_count;
    uint32_t checksum_error_count;
    uint32_t last_update_tick;
} Jy901sUartData_t;

void Jy901sUart_Init(void);
void Jy901sUart_InputBytes(const uint8_t *data, uint16_t len);
const Jy901sUartData_t *Jy901sUart_GetData(void);
void Jy901sUart_GetDataSafe(Jy901sUartData_t * out_data);
uint8_t Jy901sUart_IsFresh(uint32_t timeout_ms);

#endif
