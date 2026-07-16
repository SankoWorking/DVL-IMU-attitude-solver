from dvl import DVLSerialReader
from dvl_imu_fusion import DvlImuFuser
import time

fuser = DvlImuFuser(0)

dvl = DVLSerialReader(fuser.fuser, port="/dev/ttyACM0")

try:
    while True:
        dvl_data = dvl.get_latest_data()
        print(f"DVL:  vx={dvl_data.get('vx')}  vy={dvl_data.get('vy')} s: {dvl_data.get('status')}")
        # data = dvl.get_latest_imu()
        # print(f"IMU: yaw={data.get('yaw')}")
        data = fuser.get_enu_vel()
        print(f"ENU: Vn={data.get('vn')}  Ve={data.get('ve')}")
        time.sleep(0.1)
except KeyboardInterrupt:
    dvl.stop()
