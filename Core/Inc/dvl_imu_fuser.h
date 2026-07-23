#ifndef DVL_IMU_FUSER_H
#define DVL_IMU_FUSER_H

#include <math.h>
#include <stdint.h>

#include "jy901s_uart.h"
#include "main.h"
#include "dvl_uart.h"

#define DVL_IMU_MISSALIGN_ANGLE 0;
#ifndef M_PI
#define M_PI 3.1415926f
#endif
#define DEG_TO_RAD(x) ((x)*(M_PI / 180.0f))

typedef struct{
	float pos_x;
	float pos_y;
	float vn;
	float ve;
	uint32_t timestamp;
}NavigationState_t;

void DVL_IMU_Fuser_Init(void);
void DVL_IMU_Fuser(uint32_t last_fuse_time, uint32_t now, DVL_Data_t *dvl, Jy901sUartData_t *imu);
void Get_NavigationState(NavigationState_t *out_data);

#endif
