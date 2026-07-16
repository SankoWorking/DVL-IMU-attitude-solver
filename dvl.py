import serial
import threading

import time


class DVLSerialReader:
    def __init__(self, fuser, port, baudrate=115200):
        self.fuse_callback = fuser
        self.ser = serial.Serial(port, baudrate, timeout=1)
        print("DVL serial opened")
        self.running = True
        
        self.latest_data = {
            "vx": 0.0,
            "vy": 0.0,
            "vz": 0.0,
            "status": 'v',
            "timestamp": 0
        }

        self.latest_imu = {
            "yaw": 0.0,
            "timestamp": 0 
        }

        self.lock = threading.Lock()

        self.thread = threading.Thread(target=self._read_loop, daemon=True)
        self.thread.start()

    def _parse(self, line):
        arrival_time = time.perf_counter()

        parts = [p.strip() for p in line.split(',')]
        try:
            vx = float(parts[1])
            vy = float(parts[2])
            vz = float(parts[3])
            status = parts[5]

            with self.lock:
                self.latest_data.update({
                    "vx": vx,
                    "vy": vy,
                    "vz": vz,
                    "status": status,
                    "timestamp": arrival_time
                })
            if status == 'A':
                self.fuse_callback(vx, vy, self.get_latest_imu().get("yaw"))
        except (IndexError,ValueError) as e:
            print("Parse error")
            return None
    
    def _parse_imu(self, line):
        arrival_time = time.perf_counter()

        parts = [p.strip() for p in line.split(',')]
        try:
            yaw = float(parts[2])

            with self.lock:
                self.latest_imu.update({
                    "yaw": yaw,
                    "timestamp": arrival_time
                })
        except (IndexError,ValueError) as e:
            print("Attitude parse error")
            return None
    
    def _read_loop(self):
        while self.running:
            if self.ser.in_waiting:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if line.startswith(':BI'):
                    self._parse(line)
                elif line.startswith(':SA'):
                    self._parse_imu(line)

    def get_latest_data(self):
        with self.lock:
            return self.latest_data.copy()
    
    def get_latest_imu(self):
        with self.lock:
            return self.latest_imu.copy()
    
    def stop(self):
        self.running = False
        self.ser.close()
        print("\nDVL serial closed safely")