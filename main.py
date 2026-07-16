from dvl import DVLSerialReader

dvl = DVLSerialReader(port="/dev/ttyACM0")

try:
    while True:
        data = dvl.get_latest_data()
        print(f"DVL:  vx={data.get('vx')}  vy={data.get('vy')} s: {data.get('status')}\n ")
except KeyboardInterrupt:
    dvl.stop()
