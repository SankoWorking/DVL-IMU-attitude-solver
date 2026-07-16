import math
import threading

class DvlImuFuser:
    def __init__(self, missalign_angle):
        self.dvl_imu_x_missalign_angle = missalign_angle

        self.latest_enu_vel = {
            "vn": 0.0,
            "ve": 0.0
        }

        self.lock = threading.Lock()
    
    def fuser(self, dvl_vx, dvl_vy, imu_yaw):
        theta = math.radians(imu_yaw + self.dvl_imu_x_missalign_angle)

        cos_t = math.cos(theta)
        sin_t = math.sin(theta)

        vn = dvl_vx * cos_t - dvl_vy * sin_t
        ve = dvl_vx * sin_t + dvl_vy * cos_t
        with self.lock:
            self.latest_enu_vel.update({
                "vn": vn,
                "ve": ve
            })
    
    def get_enu_vel(self):
        with self.lock:
            return self.latest_enu_vel.copy()