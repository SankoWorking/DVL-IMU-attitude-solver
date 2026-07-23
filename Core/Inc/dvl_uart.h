#ifndef DVL_UART_H
#define DVL_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"

#define DVL_IMU_MOUNTING_ANGLE_OFFSET 0

typedef struct{
	float vx;
	float vy;
	float vz;
	float ve;
	uint8_t status;
	uint32_t checksum_error_count;
	uint32_t frame_count;
	uint32_t last_update_tick;
}DVL_Data_t;

void DvlUart_InputBytes(const uint8_t *data, uint16_t len);
void DvlUart_GetData(DVL_Data_t *out_data);

#endif
