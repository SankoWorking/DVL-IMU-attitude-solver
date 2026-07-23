#include "dvl_imu_fuser.h"

static NavigationState_t nav = {0};
static osMutexId_t nav_mutex = NULL;

void DVL_IMU_Fuser_Init(void)
{
    if (nav_mutex == NULL) {
        const osMutexAttr_t mutex_attr = {
            .name = "navMutex",
            .attr_bits = osMutexRecursive | osMutexPrioInherit, // ??? + ?????(???????)
            .cb_mem = NULL,
            .cb_size = 0U
        };
        nav_mutex = osMutexNew(&mutex_attr);
    }
}

void DVL_IMU_Fuser(uint32_t last_fuse_time, uint32_t now, DVL_Data_t *dvl, Jy901sUartData_t *imu)
{
		float theta_deg = imu->yaw_deg;
		float theta_rad = DEG_TO_RAD(theta_deg);
	
		float cos_t = cosf(theta_rad);
		float sin_t = sinf(theta_rad);
	
		float vn = dvl->vx * cos_t -dvl->vy * sin_t;
		float ve = dvl->vx * sin_t -dvl->vy * cos_t;
			
		double dt = now - last_fuse_time;
		float delta_x = (ve / 1000.0f) * dt;
		float delta_y = (vn / 1000.0f) * dt;
	
		if (nav_mutex != NULL){
			if (osMutexAcquire(nav_mutex, osWaitForever) == osOK) {
				nav.pos_x += delta_x;
				nav.pos_y += delta_y;
				nav.timestamp = now;
				nav.ve = ve;
				nav.vn = vn;
				osMutexRelease(nav_mutex);
			}
		}
}

void Get_NavigationState(NavigationState_t *out_data)
{
    if (out_data == NULL) return;

    if (nav_mutex != NULL) {
        if (osMutexAcquire(nav_mutex, osWaitForever) == osOK) {
            *out_data = nav; 
            
            osMutexRelease(nav_mutex);
        }
    }
}
