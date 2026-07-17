from dvl import DVLSerialReader
from dvl_imu_fusion import DvlImuFuser
import matplotlib.pyplot as plt

# 1. 实例化融合器和 DVL 读取器 (保持不变)
fuser = DvlImuFuser(missalign_angle=0.0)
dvl = DVLSerialReader(fuser.fuser, port="/dev/ttyACM0")

# 2. 初始化 Matplotlib 实时绘图窗口
plt.ion()  # 开启交互模式 (Interactive On)
fig, ax = plt.subplots(figsize=(8, 8)) # 创建一个正方形画布，长宽比例一致更符合真实物理世界

x_history = []
y_history = []

# 画一条蓝色的线段，加上数据点标记
line, = ax.plot(x_history, y_history, 'b-', marker='.', markersize=2, label='Trajectory')

ax.set_xlabel('East (X) [m]')
ax.set_ylabel('North (Y) [m]')
ax.set_title('Real-time Dead Reckoning (DVL + IMU)')
ax.grid(True, linestyle='--', alpha=0.6)
ax.legend()

print("开始实时绘制轨迹... (按 Ctrl+C 停止)")

try:
    while True:
        # 获取最新计算出的坐标点
        # 注意：这里调用的是你刚刚写好的 get_current_position 方法
        pos = fuser.get_current_position() 
        current_x = pos['x']
        current_y = pos['y']
        
        # 将新坐标追加到历史列表中
        x_history.append(current_x)
        y_history.append(current_y)
        
        # 更新图表中的数据
        line.set_xdata(x_history)
        line.set_ydata(y_history)
        
        # 动态调整坐标轴的显示范围，让轨迹始终在画面视野内
        ax.relim()
        ax.autoscale_view()
        
        # 强制绘制图像并暂停 0.1 秒 (相当于 10Hz 刷新率，同时替代了 time.sleep)
        plt.draw()
        plt.pause(0.1) 
        
        # 你依然可以在终端打印数据，双管齐下
        print(f"当前坐标: X(东)={current_x:.3f}m, Y(北)={current_y:.3f}m")

except KeyboardInterrupt:
    print("\n收到退出指令，正在安全关闭串口...")
    dvl.stop()
    
    # 退出交互模式，展示最终的静态完整轨迹图，防止窗口闪退
    plt.ioff() 
    plt.show() 
    print("程序已完全退出。")