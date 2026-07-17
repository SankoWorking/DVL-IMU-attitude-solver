import math
import threading

class DvlImuFuser:
    def __init__(self, missalign_angle):
        self.dvl_imu_x_missalign_angle = missalign_angle

        self.latest_enu_vel = {
            "vn": 0.0,
            "ve": 0.0
        }

        self.last_time = None

        self.pos_x = 0.0
        self.pos_y = 0.0

        self.lock = threading.Lock()
    
    def fuser(self, dvl_vx, dvl_vy, imu_yaw, current_timestamp):
        theta = math.radians(imu_yaw + self.dvl_imu_x_missalign_angle)

        cos_t = math.cos(theta)
        sin_t = math.sin(theta)

        vn = dvl_vx * cos_t - dvl_vy * sin_t
        ve = dvl_vx * sin_t + dvl_vy * cos_t

        with self.lock:
            if self.last_time is not None:
                dt = current_timestamp - self.last_time
                self.pos_x += (ve / 1000.0) * dt
                self.pos_y += (vn / 1000.0) * dt

            self.last_time = current_timestamp
            self.latest_enu_vel.update({
                "vn": vn,
                "ve": ve
            })
    
    def get_current_position(self):
        with self.lock:
            return {
                "x": self.pos_x,
                "y": self.pos_y
            }

    def get_enu_vel(self):
        with self.lock:
            return self.latest_enu_vel.copy()