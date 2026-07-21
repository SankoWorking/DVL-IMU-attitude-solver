# H743 实验调试启动手册

本文按当前工程代码和 `H743.ioc` 的实际配置整理，适用于上电、串口、声纳、雷达、电机、蓝牙和自动巡航的台架实验。主调试口使用 CH340 接 `USART1 PA9/PA10`，HC-05 蓝牙命令口使用 `UART8 PE1/PE0`。如果要按代码学习整套工程，先看 `Docs/codebase_code_explanation.md`。

## 1. 上电前检查

1. 电机不要直接接 STM32 引脚，必须经过电机驱动板；STM32 的 `PF6/PF7/PF8` 只输出 PWM，`PA7/PC4/PA4` 只输出方向信号。
2. STM32、传感器、电机驱动控制端必须共地；电机动力电源建议和 MCU 电源分开供电，只共地。
3. 所有串口信号按 3.3V TTL 接入 STM32；如果外设 TX 是 5V TTL，先确认是否兼容 STM32 3.3V 输入。
4. 当前调试日志和有线命令口使用 `USART1`：`PA9/TX`、`PA10/RX`，运行参数是 `115200 8N1`，用 CH340 接入并与 STM32 共地。
5. CH340 接线需要交叉：`CH340 RXD -> PA9/USART1_TX`，`CH340 TXD -> PA10/USART1_RX`，`CH340 GND -> GND`，电平必须是 3.3V TTL。
6. 正常运行程序时 `BOOT0` 保持低电平；只有进入 ROM Bootloader 串口下载时才把 `BOOT0` 拉高并复位，下载串口参数用 `115200 8E1`。
7. HC-05 蓝牙模块使用 `UART8`：`PE1/TX`、`PE0/RX`，模块 UART 必须配置为 `115200 8N1`，并与 STM32 共地。
8. DYP-A23 声纳按规格书供电，串口参数是 `115200 8N1`，触发方式是 STM32 TX 发送 `0xFF`。
9. TF-UW500 使用 `UART7`，代码启动后会请求毫米输出；如果模块 RX 没接好，可能仍按厘米输出，距离会差 10 倍。
10. VLS-H5 使用 `USART3 230400 8N1`，供电为 `5.4 V +/- 5%`；代码启动后发送 `A5 5A 01 00 00` 开始扫描命令，并记录 `4F 4B` 回复。
11. 第一次测试电机时把车架架空，先只给小功率命令。

## 2. 当前引脚表

| 功能 | STM32 引脚 | 代码外设 | 参数/用途 |
| --- | --- | --- | --- |
| 调试日志/命令 TX | PA9 | USART1_TX | 115200 8N1，输出周期调试和传感器日志，接 CH340 RXD |
| 调试命令 RX | PA10 | USART1_RX | 115200 8N1，接收串口工具命令，接 CH340 TXD |
| HC-05 蓝牙命令 TX | PE1 | UART8_TX | 115200 8N1，命令回复发送到 HC-05 RXD |
| HC-05 蓝牙命令 RX | PE0 | UART8_RX | 115200 8N1，接收 HC-05 TXD 转发的手机命令 |
| 前声纳 | PA2/PA3 | USART2_TX/RX | 115200，`SONAR_F`，连接 `TX2/RX2` |
| 右声纳 | PC12/PD2 | UART5_TX/RX | 115200，`SONAR_R`，连接 `TX5/RX5` |
| 左声纳 | PD1/PD0 | UART4_TX/RX | 115200，`SONAR_L`，连接 `TX4/RX4` |
| 后声纳 | PC6/PC7 | USART6_TX/RX | 115200，`SONAR_B`，连接 `TX6/RX6` |
| TF-UW500 | PE8/PE7 | UART7_TX/RX | 115200，`TF_UW500` |
| VLS-H5 | PB10/PB11 | USART3_TX/RX | 230400，`VLS_H5` |
| 右电机 PWM/方向 | PF7/PA7 | TIM17_CH1/GPIO | `Right` |
| 左电机 PWM/方向 | PF6/PC4 | TIM16_CH1/GPIO | `Left` |
| 顶部电机 PWM/方向 | PF8/PA4 | TIM13_CH1/GPIO | `Top` |
| MPU6500 | PB8/PB9 | I2C1_SCL/SDA | 姿态/航向 |
| 水检测 ADC/DO | PC0/PF10 | ADC1_INP10/GPIO | 水位/浸水检测 |
| 24V 输入电压 | PC2_C | ADC1_INP12 | 100k/10k 分压，输出输入电压 |

接线时注意 UART 交叉：STM32_TX 接模块 RX，STM32_RX 接模块 TX，再接 GND。
四路声纳占用的 `PA2/PA3`、`PD1/PD0`、`PC12/PD2`、`PC6/PC7` 在当前固件引脚表中均为独占，不与调试日志、雷达、电机、IMU、水检测或电压采样接口重复。

### 2.1 新增电机接口规划（未实现）

以下两路只作为下一版硬件/固件规划，当前 `H743.ioc`、PWM 初始化、电机驱动代码和控制命令均未启用。

| 新电机 | PWM 引脚 | PWM 外设 | DIR 引脚 | CubeMX 标签建议 |
| --- | --- | --- | --- | --- |
| 电机 4 | PD12 | TIM4_CH1 / AF2 | PD14 | `MOTOR4_DIR` |
| 电机 5 | PD13 | TIM4_CH2 / AF2 | PD15 | `MOTOR5_DIR` |

冲突检查结论：`PD12/PD13/PD14/PD15` 当前未出现在已启用引脚表中，`TIM4` 当前也未配置；按现有固件配置规划新增电机不会占用调试口、声纳、雷达、IMU、水检测、电压采样或现有三路电机接口。进入实现阶段前，仍需在完整原理图工程中确认这些网络未连接到其他电路。

## 3. 编译和下载

### 3.1 VSCode 编译

当前工程已经配置了 VSCode 任务，VSCode 实际调用的是 Keil 命令行工具：

```text
D:\Keil5\UV4\UV4.exe
```

使用方法：

1. 在 VSCode 里打开整个工程根目录：`C:\Users\Lenovo\Desktop\guangjiaoao\H743`，不要只打开 `MDK-ARM` 子目录。
2. 按 `Ctrl + Shift + B`。
3. 默认执行任务 `Keil: Build H743`。
4. 终端看到 `"H743\H743.axf" - 0 Error(s), 0 Warning(s).` 和 `[Keil] Build passed.` 表示编译成功。
5. 生成文件在 `MDK-ARM/H743/H743.hex`。
6. 编译日志在 `MDK-ARM/H743/H743.build_log.htm`。

也可以按 `Ctrl + Shift + P`，输入 `Tasks: Run Task`，手动选择：

| VSCode 任务 | 作用 |
| --- | --- |
| `Keil: Build H743` | 普通增量编译，默认快捷键任务 |
| `Keil: Rebuild H743` | 全量重新编译 |
| `Keil: Clean H743` | 清理 Keil 输出 |
| `Keil: Open UV Project` | 直接打开 Keil 工程 |

如果想在 VSCode 终端手动执行，同等命令是：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\keil_build.ps1 -Action build
```

全量重新编译：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\keil_build.ps1 -Action rebuild
```

清理工程：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\keil_build.ps1 -Action clean
```

如果 Keil 安装路径不是 `D:\Keil5\UV4\UV4.exe`，需要修改 `Tools/keil_build.ps1` 里的 `$KeilExe` 默认路径，或者修改 `.vscode/tasks.json` 里调用的脚本参数。

### 3.2 Keil 图形界面编译

1. 用 Keil 打开 `MDK-ARM/H743.uvprojx`。
2. 选择 `H743` 目标。
3. 点 Build 或 Rebuild。
4. 成功后同样生成 `MDK-ARM/H743/H743.hex`。

### 3.3 下载和运行

1. 用 ST-Link 下载时，直接下载 `MDK-ARM/H743/H743.hex` 或 Keil 工程生成的 axf/hex。
2. 打开串口工具看 USART1 调试日志：`PA9/TX`、`PA10/RX`，`115200 8N1`；CH340 接线为 `RXD->PA9`、`TXD->PA10`、`GND->GND`。
3. ROM Bootloader 串口下载也走 `PA9/PA10`，但需要 `BOOT0=1` 后复位，串口参数用 `115200 8E1`。下载完成后恢复 `BOOT0=0` 再复位运行程序。
4. 如果 CubeProgrammer 能显示 `Activating device: OK`、`Chip ID: 0x450` 和 Bootloader 版本，但随后提示 `RDP is set to level 1`，说明串口链路已通，问题是读保护；勾选 `Read Unprotect (MCU)` 再连接会解除读保护并擦除 Flash。

## 4. 启动顺序

1. 只接 MCU、CH340 调试线、HC-05 和必要传感器上电，先不要接电机动力电。
2. `BOOT0=0` 复位后观察 USART1 PA9/PA10 日志，正常会周期输出：
   - `$MPU6500,...`
   - `$SONAR_F/...`
   - `$TF_UW500,...`
   - `$VLS_H5,...`
   - `$FUSED,...`
   - `[SENSOR] Water ADC:raw MV:mV DO:level WET:state`
   - `[SENSOR] Power ADC:raw MV:adc_pin_mV VIN_MV:input_mV`
   - 周期日志约 `200ms` 输出一次；IMU 内部更新仍按 `50ms` 跑，日志发送任务是低优先级，避免影响电机和串口接收。
3. 如果 PA9/PA10 没日志，先查 CH340 `RXD/TXD` 是否接反、GND、3.3V TTL 电平、波特率 `115200 8N1`，以及 `BOOT0` 是否已经恢复低电平。
4. 串口工具发送 `?`，应收到命令帮助文本；手机蓝牙串口 App 连接 HC-05 后发送 `?`，也应收到同一套命令帮助。
5. 传感器日志稳定后再接电机驱动动力电。
6. 先发送停止命令 `0` 或 `M,0,0`，确认电机不动。
7. 左右主电机台架小功率测试：
   - 前进小功率：`M,-15,-15`
   - 后退小功率：`M,15,15`
   - 停止：`0`
8. 如果方向和实际车体相反，先记录现象，不要同时改线和改代码。
9. 单项传感器都确认后，才测试自动巡航：
   - `Z`：重置航向角
   - `X`：启动自动巡航
   - `Y`：停止自动巡航
   - 如果 MPU6500 或前向融合距离无效，自动巡航会保持停车并输出 `[AUTO] wait ...`，不会盲跑。
   - 自动巡航直行使用和 `M,-15,-15` 相同的前进方向；如果车体实际后退，先不要测自动巡航，先回到手动方向校准。

## 5. 命令说明

| 命令 | 作用 |
| --- | --- |
| `M,left,right` | 直接设置左右电机功率，范围 `-100..100`；负数按代码定义为前进 |
| `?` 或 `H` | 返回命令帮助 |
| `A` | 按全局速度前进 |
| `B` | 按全局速度后退 |
| `C` | 右转 |
| `D` | 左转 |
| `0` | 左右主电机停止 |
| `3` | 速度加 10 |
| `6` | 速度减 10 |
| `!` | 顶部电机打开 |
| `Q` 或 `%` | 顶部电机停止 |
| `X` | 自动巡航使能 |
| `Y` | 自动巡航关闭并停车 |
| `Z` | MPU6500 yaw 清零 |

## 6. 分项调试

### 6.1 USART1 调试串口 / CH340

1. 当前固件把周期日志和有线命令都放到 `USART1 PA9/PA10`，串口参数是 `115200 8N1`。
2. CH340 接线：`RXD -> PA9/USART1_TX`，`TXD -> PA10/USART1_RX`，`GND -> GND`。不要把 5V TTL 直接接到 H743 串口引脚。
3. 正常运行程序时 `BOOT0=0`。此时 PA9/PA10 是应用程序调试串口，可以看 `$MPU6500`、`$SONAR_*`、`$FUSED` 等日志，也可以发送 `?`、`0`、`M,left,right` 等命令。
4. 进入 ROM Bootloader 下载时 `BOOT0=1` 后复位，同样用 PA9/PA10，但串口参数切换为 `115200 8E1`。下载完成后把 `BOOT0` 拉回低电平再复位，否则不会进入用户程序。
5. HC-05 蓝牙不占用 USART1；蓝牙命令走 `UART8 PE1/PE0`，调试日志继续走 `USART1 PA9/PA10`。

### 6.2 HC-05 蓝牙

1. 当前固件把 HC-05 蓝牙命令口放到 `UART8 PE1/PE0`，串口参数是 `115200 8N1`。
2. 正常运行接线：HC-05 `TXD -> PE0/UART8_RX`，HC-05 `RXD -> PE1/UART8_TX`，再接 `GND` 和模块供电。
3. HC-05 正常透传时 `KEY/EN` 不接或保持低电平；只有进入 AT 模式改参数时才把 `KEY/EN` 拉到 3.3V。
4. 手机蓝牙串口 App 连接 HC-05 后发送 `?`，应返回命令帮助；发送有效控制命令后，蓝牙口会返回 `[BT] OK`。
5. 周期传感器日志不走蓝牙，仍从 `USART1 PA9/PA10` 输出，避免手机端被高频日志刷屏。

详细接线和 AT 模式步骤见 `Docs/hc05_bluetooth_usage.md`。

### 6.3 声纳

当前代码兼容两种帧：

1. 旧测试代码的 4 字节帧：`FF Dist_H Dist_L SUM`。
2. DYP-A23 规格书的 6 字节双通道帧：`FF D1_H D1_L D2_H D2_L SUM`。

实验方法：

1. 在 0.3m 到 1m 前放平整目标。
2. 观察 `$SONAR_F/$SONAR_R/$SONAR_L/$SONAR_B`。
3. 无效值会显示 `65533`，融合距离里的无效值会显示 `65535`。
4. 四个探头现在按顺序触发，间隔 `35ms`，同一个探头周期约 `225ms`，用于避免串声。
5. 声纳有效距离范围按代码限制为 `40..5000mm`；超过范围、校验错误或 `500ms` 内无新帧都会显示 `65533`。

### 6.4 TF-UW500

1. 接 `UART7`: STM32 `PE8` 到模块 RX，`PE7` 到模块 TX。
2. 看 `$TF_UW500,time,distance,peak,temp,valid`。
3. `valid=0` 或距离不变，先查供电、GND、TX/RX。
4. 如果距离像厘米值而不是毫米值，检查模块 RX 是否接到 `PE8`。

### 6.5 VLS-H5

代码链路和 C 语法解释见 `Docs/vls_h5_lidar_code_explanation.md`。

1. 接 `USART3`: STM32 `PB10` 到雷达 RX，`PB11` 到雷达 TX，波特率 `230400`。
2. 看 `$VLS_H5,time,front,right,left,rear,frame_count,crc_ok,crc_error_count,running,ack_count`。
3. `ack_count` 增加表示雷达回复了启动命令；`running=1` 且 `frame_count` 持续增加表示点云接收正常。
4. 手动 `M/A/B/C/D/0` 电机命令会退出自动巡航，不受 VLS-H5 前后避障清零影响；只有自动巡航 `X` 开启后，前后融合距离小于 `400mm` 才会触发 `[OBSTACLE] front stop/rear stop` 并停车。
5. 当前融合只使用手册标称量程内 `0 < distance <= 5000 mm` 的点；盲区内近距离点仍保守作为障碍处理。
6. 手册定义 `0` 度位于传感器左前方，车体前后左右扇区必须结合实际安装方向校准角度偏移。
7. 如果 `frame_count=0`，先查 `5.4 V` 供电、GND、USART3 TX/RX 和 `230400 8N1`。

### 6.6 MPU6500

1. 上电初始化时保持静止，代码会做 128 次陀螺仪校准；如果上电时未接 MPU6500，固件会每 `1000ms` 重试，接通后再做一次短校准。
2. 如果看到 `[MPU6500_ERR]`，先查 `PB8/PB9`、上拉、电源和地址。
3. 自动巡航前建议发送 `Z`，让当前车头方向作为 0 度参考。
4. 当前没有磁力计、编码器或外部定位，yaw 只能靠陀螺积分，长期运行一定会漂移；自动巡航只适合短时间相对转向和短直线保持。
5. 每次自动转向完成后，代码会把下一段直行目标重置到当前滤波 yaw，避免把上一段的累计误差继续带到下一段。

### 6.7 电机

1. 右电机：`PF7` PWM，`PA7` 方向。
2. 左电机：`PF6` PWM，`PC4` 方向。
3. 顶部电机：`PF8` PWM，`PA4` 方向。
4. PWM 当前约 31.25kHz，计数范围 `0..99`。
5. 电机 4/5 的 `TIM4` 与 `PD12..PD15` 配置见第 2.1 节，目前仅为规划，不能通过现有命令驱动。

### 6.8 水浸检测

1. 模拟量 `AC/AO` 经过 `R40 1k` 接到 `PC0/ADC1_INP10`，代码按 12 位 ADC 和 3.3V 参考换算为毫伏：`MV = raw * 3300 / 4095`。
2. 数字量 `DO/OUT` 接 `PF10/WATER_DO`，日志里的 `DO` 是引脚电平，`WET` 是代码判定结果。
3. 当前电路的 LM393 输出由 `R35` 上拉，实测浸水时 `DO=0`，因此代码按低电平判定浸水：`DO=0/WET=1`。
4. 传感板未接时，`AC/AO` 被拉到接近 `3.3V`，模拟日志可能接近 `ADC=4095/MV=3300`；此时数字输出应判定为未浸水。

### 6.9 输入电压采集

1. `VIN(6..26V)` 经过 `R25 100k` 和 `R26 10k` 分压，再经过 `R27 1k` 接到 `PC2_C/ADC1_INP12`。
2. 日志里的 `MV` 是 ADC 引脚电压，换算公式为 `MV = raw * 3300 / 4095`。
3. 日志里的 `VIN_MV` 是输入端电压，按当前电阻值换算为 `VIN_MV = MV * 11`。
4. 24V 输入时，ADC 引脚约 `2180mV`，raw 约 `2707`，`VIN_MV` 应接近 `24000`。

## 7. 常见故障定位

| 现象 | 优先检查 |
| --- | --- |
| USART1 PA9/PA10 完全无日志 | CH340 RXD/TXD 是否接反、GND 是否共地、是否 3.3V TTL、程序是否下载到 H743 |
| CubeProgrammer UART 报 `Timeout waiting for acknowledgement` | 用示波器/逻辑分析仪确认 `0x7F` 是否到达 `PA10`、`PA9` 是否回 `0x79`；确认 `BOOT0=1` 后已复位，CubeProgrammer 参数为 `115200 8E1` |
| CubeProgrammer 已显示 `Activating device: OK` 但报 `RDP is set to level 1` | 串口 Bootloader 已连上；勾选 `Read Unprotect (MCU)` 后重新连接，确认会整片擦除 Flash，然后再重新下载 hex |
| FlyMcu 能读到 Bootloader 版本但提示“芯片已设置读保护” | 串口链路已通；优先用 STM32CubeProgrammer 的 `Read Unprotect (MCU)` 或 ST-Link Option Bytes 把 RDP 退回 Level 0，FlyMcu 不作为 H743 解除读保护的首选工具 |
| ROM Bootloader 能下载但运行后没日志 | `BOOT0` 是否仍为高电平；下载后必须恢复 `BOOT0=0` 再复位运行用户程序 |
| 串口乱码 | 运行日志用 `115200 8N1`，ROM Bootloader 下载用 `115200 8E1`，两种参数不要混用 |
| 能输出但命令无效 | 串口工具是否发送换行/额外字符；先用单字符 `0`、`A`、`B` 测 |
| 电机不转 | 电机驱动供电、驱动 EN、共地、PWM 脚、方向脚，不要直接接 MCU |
| 声纳全是无效值 | TX 是否发到声纳 RX，声纳 TX 是否回到 STM32 RX，波特率 115200 |
| TF 距离 10 倍误差 | UART7 TX 到模块 RX 没接好，毫米输出配置未生效 |
| VLS 有旧障碍距离不恢复 | 查安装角度和串口丢帧；当前代码已有 200ms 窗口刷新 |
| 进入系统后卡死 | 先断开外设只保留 CH340 接 PA9/PA10；若卡在 `osKernelStart()` 附近，再查 FreeRTOS 端口和 HardFault |

## 8. 实验记录建议

每次实验记录以下内容：

1. 下载的 hex 时间。
2. 上电接了哪些外设。
3. USART1 PA9/PA10 日志里 `$FUSED`、`$VLS_H5`、`$TF_UW500` 是否变化。
4. 串口工具发送过的命令和电机实际方向。
5. 电源电压、是否共地、是否接电机动力电。
