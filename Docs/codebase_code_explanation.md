# H743 工程完整代码说明书

本文按当前 `c:\Users\Lenovo\Desktop\guangjiaoao\H743` 工程代码整理，目标是让刚入门 C 语言和 STM32 的人能按文件、按运行顺序看懂这套程序。

这份说明不是通用 STM32 教程，而是对应当前工程里的真实文件、真实引脚和真实运行链路。

## 1. 先记住整套程序在做什么

当前程序运行在 `STM32H743ZIT6` 上，主要功能是：

1. 用 `USART1 PA9/PA10` 输出调试日志，也能接收有线串口命令。
2. 用 `UART8 PE1/PE0` 接 HC-05 蓝牙命令。
3. 读取 4 个 DYP-A23 声纳、TF-UW500 测距、VLS-H5 激光雷达、MPU6500 姿态和水位/电源 ADC。
4. 控制左右电机和顶部电机。
5. 支持手动命令，也支持 `X` 命令开启自动巡航。

最核心的运行链路是：

```text
上电
  -> main.c 初始化时钟、GPIO、ADC、I2C、定时器、串口
  -> 启动 TIM13/TIM16/TIM17 PWM
  -> Motor_Init()
  -> App_UART_Init()
  -> TIM6 每 1ms 给串口空闲帧计时
  -> osKernelStart()
  -> App_Tasks_Init() 创建多个 FreeRTOS 任务
  -> 串口命令和传感器数据进入 app_control.c
  -> PWM_Ramp_Task() 平滑控制左右电机
```

## 2. 文件怎么分工

| 文件 | 主要作用 |
| --- | --- |
| `Core/Src/main.c` | CubeMX 生成的主入口，初始化芯片外设，启动 PWM、串口接收和 RTOS |
| `Core/Src/stm32h7xx_hal_msp.c` | 外设底层 GPIO/时钟/中断初始化，比如 PA9/PA10 设置成 USART1 |
| `Core/Src/stm32h7xx_it.c` | 中断入口，串口中断、TIM6 中断等都会进这里 |
| `Core/Src/app_control.c` | 应用主逻辑，任务创建、命令解析、日志、电机目标速度、传感器融合、自动巡航 |
| `Core/Src/uart_port.c` | 8 路 UART 的统一接收/发送框架 |
| `Core/Src/motor.c` | 左右电机和顶部电机的方向脚、PWM 占空比输出 |
| `Core/Src/sonar.c` | DYP-A23 声纳触发和原始帧解析 |
| `Core/Src/sonar_filter.c` | 声纳滤波，把跳变值、无效值处理得更稳定 |
| `Core/Src/tf_uw500.c` | TF-UW500 测距帧和配置命令解析 |
| `Core/Src/vls_h5_lidar.c` | VLS-H5 激光雷达启动命令、ACK、点云帧、四方向距离解析 |
| `Core/Src/mpu6500.c` | MPU6500 初始化、陀螺仪读数、零偏校准、Yaw 积分 |
| `Core/Src/water_adc.c` | 水位模拟量、电源电压 ADC、水位数字输入 |
| `Core/Src/i2c_bus.c` | I2C1 初始化和复位辅助函数 |
| `Core/Inc/*.h` | 对应 `.c` 文件的声明，给其他文件调用 |

`.c` 文件通常放函数实现，`.h` 文件通常放宏定义、结构体、函数声明。比如 `motor.c` 里实现 `Set_Motor()`，`motor.h` 里声明 `Set_Motor()`，其他文件 `#include "motor.h"` 后才能调用它。

## 3. 当前关键引脚

| 功能 | 引脚 | 外设 | 说明 |
| --- | --- | --- | --- |
| 调试 TX | PA9 | USART1_TX | STM32 发日志给 CH340 RXD |
| 调试 RX | PA10 | USART1_RX | CH340 TXD 发命令给 STM32 |
| 蓝牙 TX | PE1 | UART8_TX | STM32 回 `[BT] OK` 给 HC-05 RXD |
| 蓝牙 RX | PE0 | UART8_RX | HC-05 TXD 发手机命令给 STM32 |
| VLS-H5 TX | PB10 | USART3_TX | STM32 发 `A5 5A 01 00 00` 启动雷达 |
| VLS-H5 RX | PB11 | USART3_RX | 雷达点云数据进 STM32 |
| TF-UW500 TX/RX | PE8/PE7 | UART7 | 启动后请求毫米输出 |
| 前声纳 | PA2/PA3 | USART2 | 前方 DYP-A23 |
| 右声纳 | PC12/PD2 | UART5 | 右侧 DYP-A23 |
| 左声纳 | PD1/PD0 | UART4 | 左侧 DYP-A23 |
| 后声纳 | PC6/PC7 | USART6 | 后方 DYP-A23 |
| 右电机 | PF7/PA7 | TIM17_CH1/GPIO | PF7 是 PWM，PA7 是方向 |
| 左电机 | PF6/PC4 | TIM16_CH1/GPIO | PF6 是 PWM，PC4 是方向 |
| 顶部电机 | PF8/PA4 | TIM13_CH1/GPIO | PF8 是 PWM，PA4 是方向 |

调试口和 ROM Bootloader 下载口现在都是 PA9/PA10，但参数不同：

```text
应用程序调试: BOOT0=0, 115200 8N1
ROM Bootloader: BOOT0=1 后复位, 115200 8E1
```

## 4. main.c 上电后做了什么

`main()` 是 C 程序入口。当前 `Core/Src/main.c` 的主要顺序是：

```c
HAL_Init();
SystemClock_Config();
PeriphCommonClock_Config();

MX_GPIO_Init();
MX_ADC1_Init();
MX_I2C1_Init();
MX_TIM6_Init();
MX_TIM7_Init();
MX_TIM13_Init();
MX_TIM16_Init();
MX_TIM17_Init();
MX_UART4_Init();
MX_UART5_Init();
MX_UART7_Init();
MX_UART8_Init();
MX_USART1_UART_Init();
MX_USART2_UART_Init();
MX_USART3_UART_Init();
MX_USART6_UART_Init();
```

这些 `MX_..._Init()` 是 CubeMX 生成的初始化函数。名字里带 `MX` 只是 CubeMX 的习惯写法，不是 C 语言关键字。

随后用户代码启动 PWM：

```c
HAL_TIM_PWM_Start(&htim13, TIM_CHANNEL_1);
HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);
HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);

__HAL_TIM_SET_COMPARE(&htim13, TIM_CHANNEL_1, 0);
__HAL_TIM_SET_COMPARE(&htim16, TIM_CHANNEL_1, 0);
__HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 0);
```

这表示：

1. TIM13 输出顶部电机 PWM。
2. TIM16 输出左电机 PWM。
3. TIM17 输出右电机 PWM。
4. 刚上电先把比较值设为 0，避免电机误转。

然后：

```c
Motor_Init();
App_UART_Init();
HAL_TIM_Base_Start_IT(&htim6);
```

这里的意思是：

1. `Motor_Init()` 最终调用 `Set_Motor_Stop()`，再次确保电机停止。
2. `App_UART_Init()` 给 8 路 UART 都打开 1 字节中断接收。
3. `HAL_TIM_Base_Start_IT(&htim6)` 启动 TIM6 定时中断。TIM6 每 1ms 调用一次 `App_UART_Tick1ms()`，用于普通串口的空闲帧判断。

最后创建 RTOS 并启动调度：

```c
osKernelInitialize();
App_Tasks_Init();
osKernelStart();
```

`osKernelStart()` 之后，程序基本由多个任务并行运行。

## 5. FreeRTOS 任务怎么分工

任务都在 `app_control.c` 里创建。`App_Tasks_Init()` 里有这些任务：

| 任务 | 周期/延时 | 作用 |
| --- | --- | --- |
| `Uart_Parse_Task` | 1ms | 调用 `App_UART_Process()`，把收到的串口数据交给对应解析函数 |
| `Uart_Send_Task` | 有数据就发，无数据延时 1ms | 从日志环形缓冲取数据，通过 USART1 发出去 |
| `Water_Task` | 1000ms | 输出水位 ADC、水位数字量、电源电压日志 |
| `Move_Task` | 1ms | 处理 `A/B/C/D/0/3/6/!/Q/%` 这些手动动作命令 |
| `Sensor_Tx_Task` | 50ms | 更新 MPU6500，约 200ms 输出一次传感器日志 |
| `PWM_Ramp_Task` | 3ms | 让左右电机目标速度平滑变化，并真正调用 `Set_Motor()` |
| `Sonic_Collect_Task` | 循环触发 | 依次触发四个声纳并滤波 |
| `Range_Task` | 50ms | 读取 TF-UW500 和 VLS-H5 距离，必要时重发 VLS 启动命令 |
| `AutoCruise_Task` | 3ms 或 100ms | 自动巡航状态机，只在 `X` 开启后工作 |

刚入门时，优先看这 3 条线：

```text
串口命令线: uart_port.c -> UART1_rxCallback/UART8_rxCallback -> App_Process_Command()
电机执行线: App_Set_Target_Power() -> PWM_Ramp_Task() -> apply_motor_power() -> Set_Motor()
雷达测距线: UART3_rxCallback() -> VlsH5_InputBytes() -> Range_Task() -> $VLS_H5/$FUSED 日志
```

## 6. 串口框架 uart_port.c

当前工程有 8 路 UART，为了避免每一路都写重复代码，`uart_port.c` 做了统一封装。

### 6.1 AppUartPort 枚举

`Core/Inc/uart_port.h` 里有：

```c
typedef enum
{
    APP_UART_1 = 0,
    APP_UART_2,
    APP_UART_3,
    APP_UART_4,
    APP_UART_5,
    APP_UART_6,
    APP_UART_7,
    APP_UART_8,
    APP_UART_COUNT
} AppUartPort;
```

`enum` 是枚举。它适合表示有限几个固定选项。

这里实际等价于：

```text
APP_UART_1 = 0
APP_UART_2 = 1
APP_UART_3 = 2
...
APP_UART_8 = 7
APP_UART_COUNT = 8
```

代码里用 `APP_UART_3` 比直接写数字 `2` 更容易看懂。

### 6.2 两种接收模式

当前有两种模式：

```c
typedef enum
{
    APP_UART_MODE_IDLE_FRAME = 0,
    APP_UART_MODE_STREAM
} AppUartMode_t;
```

`APP_UART_MODE_IDLE_FRAME` 用于命令和声纳。它的特点是：收到一串字节后，超过约 10ms 没新字节，就认为一帧结束。

`APP_UART_MODE_STREAM` 用于 TF-UW500 和 VLS-H5。它们是持续二进制流，不能靠空闲时间切帧，所以现在先把每个字节放进环形缓冲，再由任务解析。

当前代码在 `App_Tasks_Init()` 里设置：

```c
App_UART_SetMode(APP_UART_7, APP_UART_MODE_STREAM);
App_UART_SetMode(APP_UART_3, APP_UART_MODE_STREAM);
```

意思是：

1. UART7 给 TF-UW500，用流模式。
2. USART3 给 VLS-H5，用流模式。

### 6.3 为什么把 VLS/TF 解析移出中断

之前高波特率传感器数据可能在 `HAL_UART_RxCpltCallback()` 中断里直接进入协议解析。这样不合理，因为中断里做太多事会影响其他中断和任务调度。

现在已经修正为：

```text
串口中断里只做两件事:
  1. 把 rx_byte 放进环形缓冲
  2. 重新打开下一字节接收

Uart_Parse_Task 里再做:
  1. 从环形缓冲取出字节
  2. 调 UART3_rxCallback/UART7_rxCallback
  3. 进入 VlsH5_InputBytes/TfUw500_InputBytes
```

这样 VLS-H5 的 `230400` 波特率更稳，也降低了“一接雷达就影响电机任务”的风险。

## 7. 调试日志怎么发出来

应用里不要直接频繁 `HAL_UART_Transmit()`，而是调用：

```c
Log_Printf("text %u\r\n", value);
```

`Log_Printf()` 会先把字符串写到 `log_ring_buf` 环形缓冲。`Uart_Send_Task()` 再慢慢从缓冲里取出，发到 `APP_DEBUG_UART`。

当前定义是：

```c
#define APP_DEBUG_UART APP_UART_1
```

所以调试日志从 `USART1 PA9` 发出，CH340 接法是：

```text
CH340 RXD -> PA9/USART1_TX
CH340 TXD -> PA10/USART1_RX
CH340 GND -> GND
```

## 8. 命令怎么进来

有线串口和蓝牙都能发命令：

```c
void UART1_rxCallback(uint8_t *packet, uint16_t size)
{
    App_Process_Command(APP_DEBUG_UART, packet, size);
}

void UART8_rxCallback(uint8_t *packet, uint16_t size)
{
    App_Process_Command(APP_BLUETOOTH_UART, packet, size);
}
```

这表示：

1. USART1 收到命令，走 `UART1_rxCallback()`。
2. UART8 收到蓝牙命令，走 `UART8_rxCallback()`。
3. 两者最后都调用 `App_Process_Command()`。

当前主要命令：

| 命令 | 作用 |
| --- | --- |
| `?` | 输出帮助 |
| `M,left,right` | 直接设置左右目标功率，范围可用 `-100..100` |
| `A` | 前进 |
| `B` | 后退 |
| `C` | 右转 |
| `D` | 左转 |
| `0` | 停止左右电机 |
| `3` | 左右速度加 10 |
| `6` | 左右速度减 10 |
| `!` | 顶部电机开启 |
| `Q` 或 `%` | 顶部电机关闭 |
| `X` | 开启自动巡航 |
| `Y` | 关闭自动巡航并停车 |
| `Z` | 重置 MPU6500 yaw |

注意当前逻辑：

```text
M/A/B/C/D/0 都会退出自动巡航
只有 X 开启自动巡航后，VLS-H5/TF/声纳融合避障才会主动清零左右电机目标速度
```

这是为了解决之前“VLS-H5 一启动，手动给左右电机命令但电机不转”的问题。

## 9. M,left,right 命令怎么解析

`M,-30,-30` 会进入 `App_Parse_M_Command()`。

简化逻辑是：

```c
if (App_Parse_M_Command(packet, size) != 0U)
{
    App_Command_Ack(reply_port);
    return;
}
```

`!= 0U` 的意思是“不等于 0”。在这个工程里，很多函数用 `1U` 表示成功，用 `0U` 表示失败。

`M` 命令支持两种输入：

1. `M,-100..100,-100..100`，直接作为电机功率百分比。
2. `M,1100..1900,1100..1900`，当作老式 PWM 中值输入，再换算成 `-100..100`。

当前工程约定：

```text
负数功率 = 车体前进方向
正数功率 = 车体后退方向
```

所以：

```text
M,-30,-30  前进，小功率
M,30,30    后退，小功率
M,-30,30   原地向一侧转
M,30,-30   原地向另一侧转
```

## 10. 电机控制 motor.c

电机最终由 `Set_Motor()` 输出：

```c
void Set_Motor(MotrotID id, MotorDirection dir, uint8_t speed)
```

参数意思：

| 参数 | 意思 |
| --- | --- |
| `id` | 哪个电机，`Right`、`Left`、`Top` |
| `dir` | 方向，`FORWARD` 或 `REVERSE` |
| `speed` | 速度，代码限制到最大 99 |

右电机：

```c
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, ...);
__HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, pwm);
```

左电机：

```c
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, ...);
__HAL_TIM_SET_COMPARE(&htim16, TIM_CHANNEL_1, pwm);
```

顶部电机：

```c
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, ...);
__HAL_TIM_SET_COMPARE(&htim13, TIM_CHANNEL_1, pwm);
```

如果你用示波器测：

```text
右电机 PWM: PF7
左电机 PWM: PF6
顶部电机 PWM: PF8
```

如果 `PF6/PF7` 有 PWM，但电机不转，优先查驱动板：

1. 驱动板 PWM 输入是否真收到波形。
2. 方向脚 `PC4/PA7` 是否到驱动板。
3. 驱动板 `EN/STBY/SLEEP/FAULT` 是否正常。
4. 电机电源是否掉压。
5. VLS-H5 上电后是否拉低了 5V 或 GND。

## 11. PWM_Ramp_Task 为什么存在

如果串口命令一下子从 0 跳到 100，电机电流冲击很大。`PWM_Ramp_Task()` 每 3ms 把当前功率慢慢靠近目标功率：

```c
if (g_current_left_power < g_target_left_power)
{
    g_current_left_power += POWER_RAMP_STEP;
}
```

`g_target_left_power` 是目标值，命令或自动巡航会改它。

`g_current_left_power` 是当前真正输出值，`PWM_Ramp_Task()` 慢慢改变它。

最后调用：

```c
apply_motor_power(g_current_left_power, g_current_right_power);
```

再进入 `Set_Motor()` 输出 PWM 和方向。

## 12. VLS-H5 雷达代码

VLS-H5 使用：

```text
USART3 PB10/PB11
230400 8N1
```

初始化在 `App_Tasks_Init()`：

```c
VlsH5_Init(APP_UART_3);
App_UART_SetMode(APP_UART_3, APP_UART_MODE_STREAM);
```

启动在 `Range_Task()`：

```c
osDelay(500U);
VlsH5_Start();
```

`VlsH5_Start()` 会发：

```text
A5 5A 01 00 00
```

雷达正常会回：

```text
4F 4B
```

也就是 ASCII 的 `OK`。

当前已经修正了一个不合理点：如果上电后没有收到 ACK，也没有收到数据帧，`Range_Task()` 每 1 秒重发一次 `VlsH5_Start()`。

```c
if ((vls_data->frame_count == 0UL) &&
    (vls_data->command_ack_count == 0UL) &&
    ((now_tick - last_vls_start_tick) >= VLS_H5_START_RETRY_MS))
{
    VlsH5_Start();
    last_vls_start_tick = now_tick;
}
```

这样可以避免雷达刚上电不稳定时，只发一次启动命令然后永远不再启动。

### 12.1 VLS-H5 数据怎么变成前后左右

`vls_h5_lidar.c` 里把一帧数据解析成 12 个点。每个点有：

```c
typedef struct
{
    uint16_t angle_cdeg;
    uint16_t distance_mm;
    uint8_t intensity;
} VlsH5Point_t;
```

`angle_cdeg` 是角度，单位是 0.01 度。比如：

```text
9000 = 90.00 度
18000 = 180.00 度
35999 = 359.99 度
```

当前方向划分大致是：

| 扇区 | 角度范围 |
| --- | --- |
| 前 | 345 到 360 度，或 0 到 15 度 |
| 右 | 75 到 105 度 |
| 后 | 165 到 195 度 |
| 左 | 255 到 285 度 |

如果雷达实际安装方向不一样，后续要改 `VlsH5_SetAngleOffsetCdeg()` 的角度偏移。

## 13. TF-UW500 代码

TF-UW500 使用 `UART7 PE8/PE7`。

初始化时：

```c
TfUw500_Init();
App_UART_SetMode(APP_UART_7, APP_UART_MODE_STREAM);
(void)TfUw500_RequestOutputFormat(APP_UART_7, TF_UW500_DISTANCE_UNIT_MM);
```

这表示：

1. 清空 TF-UW500 内部状态。
2. UART7 使用流模式。
3. 向 TF-UW500 发送命令，请求用毫米输出。

日志格式是：

```text
$TF_UW500,time,distance,peak,temp,valid
```

你给过的例子：

```text
$TF_UW500,123.064,65535,3,3300,0
```

含义是：

| 字段 | 值 | 含义 |
| --- | --- | --- |
| `$TF_UW500` | 固定头 | 这行是 TF-UW500 日志 |
| `123.064` | 秒 | 系统运行到约 123.064 秒 |
| `65535` | 距离 | 无效距离，当前工程用 `65535` 表示无效 |
| `3` | peak | 回波强度很低 |
| `3300` | 温度 | 33.00 摄氏度 |
| `0` | valid | 数据无效 |

## 14. 声纳代码

四个 DYP-A23 声纳由 `Sonic_Collect_Task()` 依次触发：

```text
前 -> 右 -> 左 -> 后
```

触发函数是：

```c
Sonic_Trigger(2U);
```

`2U` 表示 USART2，也就是前声纳。

声纳返回数据后，走对应 UART 回调：

```c
UART2_rxCallback -> Sonar_Front
UART5_rxCallback -> Sonar_Right
UART4_rxCallback -> Sonar_Left
UART6_rxCallback -> Sonar_Rear
```

然后 `Sonar_Data_Filter()` 做滤波，最后写到：

```c
g_sonar_front_mm
g_sonar_right_mm
g_sonar_left_mm
g_sonar_rear_mm
```

日志格式：

```text
$SONAR_F,time,distance
$SONAR_R,time,distance
$SONAR_L,time,distance
$SONAR_B,time,distance
```

## 15. MPU6500 姿态代码

MPU6500 走 I2C1。

启动时：

```c
I2C1_Init();
Mpu6500_Init();
Mpu6500_CalibrateGyro(128U);
```

`Sensor_Tx_Task()` 会周期调用：

```c
Mpu6500_Update(dt_s);
```

`dt_s` 是两次更新之间的秒数。MPU6500 代码用陀螺仪 Z 轴角速度积分得到 yaw。

日志：

```text
$MPU6500,time,roll,pitch,yaw,gx,gy,gz,temp,stationary,bias_z
$YAW_RAW,time,yaw
$YAW_FILTER,time,filtered_yaw
```

`Z` 命令会调用：

```c
App_Reset_Imu_Yaw();
```

它把 yaw 和滤波状态清零，适合上车前先校准方向。

## 16. 水位和电源 ADC

`water_adc.c` 里读两个模拟量：

| 函数 | 作用 |
| --- | --- |
| `Water_Get_AnalogRaw()` | 水位模拟量原始 ADC |
| `Water_RawToVoltageMv()` | 转成毫伏 |
| `Power_Get_AdcRaw()` | 电源分压 ADC |
| `Power_RawToInputVoltageMv()` | 估算输入电压 |
| `Water_Get_Digital()` | 水位数字输入 |
| `Water_Is_Wet()` | 判断是否有水 |

`Water_Task()` 每 1 秒输出：

```text
[SENSOR] Water ADC:... MV:... DO:... WET:...
[SENSOR] Power ADC:... MV:... VIN_MV:...
```

## 17. 自动巡航怎么工作

自动巡航只在收到 `X` 后开启：

```c
g_auto_cruise_enable = 1U;
```

收到 `Y`、`M`、`A`、`B`、`C`、`D`、`0` 会关闭：

```c
g_auto_cruise_enable = 0U;
```

自动巡航准备条件：

```c
static uint8_t App_AutoCruise_Ready(void)
{
    if (App_Is_Imu_Ready() == 0U)
    {
        return 0U;
    }

    return App_Is_Range_Valid(App_Get_Front_Range_Mm());
}
```

也就是：

1. MPU6500 必须正常。
2. 前方距离必须有效。

自动巡航时，前方距离来自融合：

```c
front = min_valid(sonar_front, tf_uw500, vls_front)
```

右、左、后也会把声纳和 VLS-H5 进行有效值比较。

`App_Apply_Obstacle_Guard()` 现在只在自动巡航开启时工作：

```c
if (g_auto_cruise_enable == 0U)
{
    return;
}
```

所以手动调车时，VLS-H5 不会再把左右电机目标速度清零。

## 18. 常见日志怎么看

| 日志 | 说明 |
| --- | --- |
| `[UART] Command/log port...` | 程序已进入应用，调试口是 USART1 PA9/PA10 |
| `[BT] HC-05 command port...` | 蓝牙口是 UART8 PE1/PE0 |
| `$MPU6500,...` | 姿态数据 |
| `$YAW_RAW,...` | 原始 yaw |
| `$YAW_FILTER,...` | 滤波后的 yaw |
| `$SONAR_F,...` | 前声纳 |
| `$SONAR_R,...` | 右声纳 |
| `$SONAR_L,...` | 左声纳 |
| `$SONAR_B,...` | 后声纳 |
| `$TF_UW500,...` | TF-UW500 |
| `$VLS_H5,...` | VLS-H5 前右左后距离和状态 |
| `$FUSED,...` | 融合后的前右左后距离 |
| `[OBSTACLE] front stop` | 自动巡航时前方小于 400mm，主动停车 |
| `[AUTO] wait MPU6500` | 自动巡航等待 MPU6500 正常 |
| `[AUTO] wait front range` | 自动巡航等待前方距离有效 |

`$VLS_H5` 格式：

```text
$VLS_H5,time,front,right,left,rear,frame_count,crc_ok,crc_error_count,running,ack_count
```

重点看：

1. `frame_count` 是否持续增加。
2. `crc_ok` 是否大多数时候为 `1`。
3. `running` 是否为 `1`。
4. `ack_count` 是否大于 0。
5. `front/right/left/rear` 是否经常是 `65535`。`65535` 表示无效距离。

## 19. C 语言语法细节

这一节专门解释你看当前工程时最常遇到的 C 语法。

### 19.1 #include

```c
#include "motor.h"
#include <stdint.h>
```

`#include` 是预处理指令。编译前，编译器会把头文件内容引入当前文件。

双引号 `"motor.h"` 通常表示工程自己的头文件。

尖括号 `<stdint.h>` 通常表示标准库或编译器提供的头文件。

### 19.2 #define

```c
#define VLS_H5_START_RETRY_MS 1000U
```

`#define` 是宏定义。编译前，代码里的 `VLS_H5_START_RETRY_MS` 会被替换成 `1000U`。

`1000U` 的 `U` 表示 unsigned，无符号整数。

### 19.3 uint8_t、uint16_t、uint32_t

这些来自 `<stdint.h>`：

| 类型 | 大小 | 范围 |
| --- | --- | --- |
| `uint8_t` | 8 位 | 0 到 255 |
| `uint16_t` | 16 位 | 0 到 65535 |
| `uint32_t` | 32 位 | 0 到 4294967295 |
| `int8_t` | 8 位有符号 | -128 到 127 |
| `int16_t` | 16 位有符号 | -32768 到 32767 |
| `int32_t` | 32 位有符号 | 约 -21 亿到 21 亿 |

STM32 工程里建议用这些固定宽度类型，因为你能明确知道变量占几个字节。

### 19.4 static

```c
static int8_t g_current_left_power = 0;
```

`static` 放在全局变量前面，表示这个变量只在当前 `.c` 文件里可见。其他文件不能直接访问它。

```c
static void Range_Task(void *argument)
```

`static` 放在函数前面，表示这个函数只给当前 `.c` 文件内部使用。

这样做的好处是减少文件之间互相乱调用。

### 19.5 volatile

```c
volatile int8_t g_target_left_power = 0;
```

`volatile` 告诉编译器：这个变量可能被中断或其他任务随时改变，不要擅自优化读取。

在 STM32 里，中断、RTOS 任务共享的变量经常会加 `volatile`。

### 19.6 extern

```c
extern volatile int8_t g_target_left_power;
```

`extern` 表示：这个变量在别的 `.c` 文件里真正定义，这里只是声明它存在。

比如变量定义在 `app_control.c`：

```c
volatile int8_t g_target_left_power = 0;
```

头文件 `app_control.h` 里写：

```c
extern volatile int8_t g_target_left_power;
```

其他文件包含这个头文件后才能使用它。

### 19.7 typedef enum

```c
typedef enum
{
    FORWARD = 1,
    REVERSE
} MotorDirection;
```

这定义了一个新类型 `MotorDirection`，它只能取几个枚举值。使用时：

```c
Set_Motor(Left, FORWARD, 50);
```

比写 `Set_Motor(2, 1, 50)` 清楚很多。

### 19.8 typedef struct

```c
typedef struct
{
    uint16_t angle_cdeg;
    uint16_t distance_mm;
    uint8_t intensity;
} VlsH5Point_t;
```

`struct` 是结构体，用来把多个相关变量打包成一个整体。

使用时：

```c
VlsH5Point_t point;
point.distance_mm = 300;
```

`.` 用来访问结构体成员。

如果是结构体指针：

```c
const VlsH5Data_t *vls_data;
vls_data = VlsH5_GetData();
vls_data->frame_count;
```

`->` 等价于“先通过指针找到结构体，再访问成员”。

### 19.9 指针

```c
uint8_t *packet
```

`*` 表示这是一个指针。指针保存的是地址。

串口收到的数据是一段字节数组，函数只需要知道“数据从哪里开始”和“长度是多少”，所以常见写法是：

```c
void UART1_rxCallback(uint8_t *packet, uint16_t size)
```

`packet` 是首地址，`size` 是长度。

### 19.10 const

```c
const VlsH5Data_t *vls_data;
```

`const` 表示通过这个指针不应该修改数据，只读。

这可以防止误写传感器内部状态。

### 19.11 if/else

```c
if (g_auto_cruise_enable == 0U)
{
    return;
}
```

`if` 判断条件是否成立。

注意：

```c
==
```

是判断相等。

```c
=
```

是赋值。初学 C 时最容易把这两个写错。

### 19.12 switch/case

```c
switch (cmd)
{
case 'A':
    BLE_Moter_Flag = Forward_Flag;
    break;
case '0':
    BLE_Moter_Flag = Stop_Motor_Flag;
    break;
default:
    return;
}
```

`switch` 适合处理多个固定命令。每个 `case` 对应一个命令。`break` 表示这个分支执行完就跳出。

如果没有 `break`，程序会继续执行下一个 `case`，这通常是 bug。

### 19.13 for (;;)

```c
for (;;)
{
    App_UART_Process();
    osDelay(1U);
}
```

`for (;;)` 是死循环，等价于 `while (1)`。

RTOS 任务通常都写成死循环，因为任务要一直运行。

### 19.14 return

```c
return 1U;
```

`return` 表示函数结束，并返回一个值。

比如 `App_Parse_M_Command()` 返回 `1U` 表示解析成功，返回 `0U` 表示失败。

如果函数是 `void`，可以只写：

```c
return;
```

表示提前退出，不返回具体值。

### 19.15 强制类型转换

```c
(uint16_t)sizeof(frame)
```

括号里的类型表示强制转换。`sizeof(frame)` 默认类型可能比较大，这里明确转成 `uint16_t`。

又比如：

```c
(int32_t)left_power
```

表示把 `left_power` 转成 32 位有符号整数再参与计算，避免小类型计算溢出。

### 19.16 sizeof

```c
sizeof(frame)
```

`sizeof` 返回变量或类型占多少字节。数组 `frame` 如果定义为：

```c
uint8_t frame[255];
```

那 `sizeof(frame)` 就是 255。

### 19.17 NULL

```c
if (packet == NULL)
{
    return;
}
```

`NULL` 表示空指针，也就是没有有效地址。

函数拿到指针参数时，先判断是否为 `NULL`，可以避免程序跑飞。

### 19.18 && 和 ||

```c
if ((distance_mm != 0U) &&
    (distance_mm != 65535U))
```

`&&` 表示“并且”，两个条件都成立才成立。

```c
if ((cmd == '?') || (cmd == 'H') || (cmd == 'h'))
```

`||` 表示“或者”，任意一个成立就成立。

### 19.19 数组

```c
uint8_t UART1_FrameBuff[APP_UART_FRAMEBUF_SIZE];
```

这是数组。数组下标从 0 开始。如果 `APP_UART_FRAMEBUF_SIZE` 是 255，下标范围是：

```text
0 到 254
```

代码写数组时一定要注意不能越界。

### 19.20 函数声明和函数定义

头文件里常见的是声明：

```c
void App_Tasks_Init(void);
```

`.c` 文件里是定义：

```c
void App_Tasks_Init(void)
{
    ...
}
```

声明告诉编译器“有这个函数”，定义才是真正的函数内容。

## 20. 本次发现并修正的不合理点

这次检查代码时，我已经修正了 2 个与雷达和串口稳定性相关的问题。

### 20.1 VLS-H5/TF-UW500 流数据不再在串口中断里直接解析

修改位置：

```text
Core/Src/uart_port.c
```

修正前的风险：

```text
VLS-H5 230400 波特率，数据很多。如果在 UART 中断里直接解析协议，中断占用时间会变长，可能影响电机控制任务和其他外设。
```

现在的做法：

```text
UART 中断: 收 1 字节 -> 放进环形缓冲 -> 立刻重新打开接收
Uart_Parse_Task: 每 1ms 取缓冲数据 -> 交给 VLS/TF 协议解析
```

### 20.2 VLS-H5 启动命令增加重发

修改位置：

```text
Core/Src/app_control.c
```

修正前的风险：

```text
Range_Task 只发一次 VlsH5_Start()。如果雷达刚上电没准备好，启动命令丢了，后面就不会再启动扫描。
```

现在的做法：

```text
如果 frame_count 仍为 0，ack_count 仍为 0，每 1 秒重发一次启动命令。
```

### 20.3 已保留的逻辑

之前已经修正过的电机保护逻辑继续保留：

```text
手动 M/A/B/C/D/0 命令会退出自动巡航。
VLS-H5 前后 400mm 避障只在 X 自动巡航开启后参与停车。
```

所以现在手动调试电机时，雷达只提供距离日志，不应该直接拦截你的左右电机命令。

## 21. 现在调试时推荐的阅读顺序

如果你是刚开始看代码，建议按这个顺序：

1. 先看 `Docs/debug_startup_manual.md`，确认引脚、接线、启动步骤。
2. 再看本文件第 1 到第 8 节，理解程序怎么从上电跑到命令解析。
3. 看 `Core/Src/motor.c`，理解 PF6/PF7/PA7/PC4 怎么控制电机。
4. 看 `Core/Src/app_control.c` 的 `App_Process_Command()`、`PWM_Ramp_Task()`、`Range_Task()`。
5. 看 `Core/Src/vls_h5_lidar.c`，理解雷达数据怎样变成四方向距离。
6. 看 `Core/Src/uart_port.c`，理解 8 路串口怎么统一收发。

## 22. 后续仍建议注意的点

当前我发现并修正了明确不合理的串口流解析和 VLS 启动重发问题。后续硬件调试仍建议重点看：

1. 如果 PWM 正常但电机不转，优先查电机驱动电源、EN/STBY/SLEEP/FAULT，而不是先怀疑 MCU。
2. 如果 VLS-H5 日志 `frame_count` 不增长，查 PB10/PB11 接线、230400 波特率、雷达供电和启动 ACK。
3. 如果自动巡航不启动，看 `[AUTO] wait MPU6500` 还是 `[AUTO] wait front range`。
4. 如果 TF-UW500 一直 `65535`，看模块 RX 是否接好，因为请求毫米输出命令需要 STM32 TX 能到模块 RX。
5. 如果 USART1 PA9/PA10 没日志，先确认 BOOT0 已恢复低电平，串口工具是 `115200 8N1`。

## 23. VSCode 编译方法

当前 VSCode 编译不是直接用 VSCode 自己编译，而是通过 `.vscode/tasks.json` 调用 Keil 命令行：

```text
D:\Keil5\UV4\UV4.exe
```

最常用方法：

1. 用 VSCode 打开工程根目录 `C:\Users\Lenovo\Desktop\guangjiaoao\H743`。
2. 按 `Ctrl + Shift + B`。
3. 默认任务是 `Keil: Build H743`。
4. 看到 `0 Error(s), 0 Warning(s)` 和 `[Keil] Build passed.` 表示成功。
5. 生成的下载文件是 `MDK-ARM/H743/H743.hex`。

VSCode 里也可以按 `Ctrl + Shift + P`，输入 `Tasks: Run Task`，选择：

```text
Keil: Build H743
Keil: Rebuild H743
Keil: Clean H743
Keil: Open UV Project
```

终端手动编译命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\keil_build.ps1 -Action build
```

全量重新编译：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\keil_build.ps1 -Action rebuild
```

编译日志：

```text
MDK-ARM/H743/H743.build_log.htm
```

注意：编译只生成 hex，不会自动烧录。烧录仍然用 Keil/ST-Link、STM32CubeProgrammer，或者按 ROM Bootloader 流程用 PA9/PA10 下载。
