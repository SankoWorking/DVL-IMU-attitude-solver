# -*- coding: utf-8 -*-
import serial

def dvl_pd6_parser(raw_line):
    line = raw_line.decode('utf-8').strip()
    if not line.startswith(':'):
        return None
    
    parts = [p.strip() for p in line.split(',')]
    header = parts[0]

    data = {"header": header}

    try:
        if header == ':BS':
            data.update({
                "type":"Ship Velocity",
                "vx": float(parts[1]),
                "vy": float(parts[2]),
                "vz": float(parts[3]),
                #"status": float(parts[4])
            })
        elif header == ':BI':
            data.update({
                "type":"Equipment Velocity",
                "vx": float(parts[1]),
                "vy": float(parts[2]),
                "vz": float(parts[3]),
                #"status": float(parts[4])
            })
        elif header == ':BE':
            data.update({
                "type":"Earth Velocity",
                "v_east": float(parts[1]),
                "v_north": float(parts[2]),
                "v_up": float(parts[3]),
                #"status": float(parts[4])
            })
        elif header == ':SA':
            data.update({
                "type":"Attitude",
                "pitch": float(parts[1]),
                "roll": float(parts[2]),
                "yaw": float(parts[3])
            })
        elif header == ':BD':
            data.update({
                "type":"Distance",
                "dist_east": float(parts[1]),
                "dist_north": float(parts[2]),
                "dist_up": float(parts[3]),
                "altitude": float(parts[4]),
                "time_s": float(parts[5])
            })
        elif header == ':TS':
            data.update({
                "type":"Enviroment",
                "timestamp": parts[1],
                "salinity": float(parts[2]),
                "temperature": float(parts[3]),
                "depth": float(parts[4]),
                "sound_speed": float(parts[5])
            })
        return data
    except (IndexError,ValueError) as e:
        print("Raw data error, can't parse")
        return None

def print_data(data):
    if data is None:
        return
    header = data.get("header")
    if header == ':BS':
        print(f"船体坐标系移动速度:\n vx={data.get('vx')}\n vy={data.get('vy')}\n vz={data.get('vz')}\n ")
    elif header == ':BI':
        print(f"设备坐标系移动速度:\n vx={data.get('vx')}\n vy={data.get('vy')}\n vz={data.get('vz')}\n ")
    elif header == ':BE':
        print(f"大地坐标系速度:\n v_e={data.get('v_east')}\n v_n={data.get('v_north')}\n v_u={data.get('v_up')}\n")
    elif header == ':SA':
        print(f"姿态角(度):\n pitch: {data.get('pitch')}\n roll: {data.get('roll')}\n yaw: {data.get('yaw')}\n")
    elif header == ':BD':
        print(f"大地坐标系距离:\n dist_e: {data.get('dist_east')}\n dist_n: {data.get('dist_north')}\n dist_u: {data.get('dist_up')}\n")
    elif header == ':TS':
        print(f"timestamp: {data.get('timestamp')}\n")

PORT_NAME = '/dev/ttyACM0'
BAUD_RATE = 115200

try:
    ser = serial.Serial(PORT_NAME, BAUD_RATE, timeout=1)
    print("Serial Opened")
    while True:
        if ser.in_waiting > 0:
            raw_data = ser.readline()
            data = dvl_pd6_parser(raw_data)
            print_data(data)


except serial.SerialException as e:
    print("Failed to open Serial")

except KeyboardInterrupt:
    print("User Interrupted")

finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
        print("Serial Closed safely")
