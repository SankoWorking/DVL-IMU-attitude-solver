import serial
import threading

import time


class DVLSerialReader:
    def __init__(self, port, baudrate=115200):
        self.ser = serial.Serial(port, baudrate, timeout=1)
        print("DVL serial opened")
        self.running = True
        
        self.lastest_data = {
            "vx": 0.0,
            "vy": 0.0,
            "vz": 0.0,
            "status": 'v',
            "timestamp": 0
        }
        self.lock = threading.Lock()

        self.thread = threading.Thread(target=self._read_loop, daemon=True)
        self.thread.start()

    def _parse(self, line):
        arrival_time = time.perf_counter()

        parts = [p.strip() for p in line.split(',')]
        # print(parts)
        try:
            vx = float(parts[1])
            vy = float(parts[2])
            vz = float(parts[3])
            status = parts[5]

            with self.lock:
                self.lastest_data.update({
                    "vx": vx,
                    "vy": vy,
                    "vz": vz,
                    "status": status,
                    "timestamp": arrival_time
                })
        except (IndexError,ValueError) as e:
            print("parse error")
            return None
    
    def _read_loop(self):
        while self.running:
            if self.ser.in_waiting:
                line = self.ser.readline().decode('utf-8').strip()
                if line.startswith(':BI'):
                    self._parse(line)

    def get_latest_data(self):
        with self.lock:
            return self.lastest_data.copy()
    
    def stop(self):
        self.running = False
        self.ser.close()