from dvl import DVLSerialReader
from dvl_imu_fusion import DvlImuFuser
import time

fuser = DvlImuFuser(0)

dvl = DVLSerialReader(fuser.fuser, port="/dev/ttyACM0")

try:
    while True:
        #dvl_data = dvl.get_latest_data()
        #print(f"DVL:  vx={dvl_data.get('vx')}  vy={dvl_data.get('vy')} s: {dvl_data.get('status')}")
        # imu_data = dvl.get_latest_imu()
        # print(f"IMU: yaw={imu_data.get('yaw')}")
        enu_data = fuser.get_enu_vel()
        print(f"ENU: Vn={enu_data.get('vn'):.2f}  Ve={enu_data.get('ve'):.2f}")

        xy_data = fuser.get_current_position()
        print(f"XY: x={xy_data.get('x'):.3f} y={xy_data.get('y'):.3f}")

        time.sleep(0.1)
except KeyboardInterrupt:
    dvl.stop()
