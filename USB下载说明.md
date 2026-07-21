# H7_LU_motor USB 下载说明

本文说明 `H7_LU_motor` 当前工程的启动结构、USB 工作模式、固件地址、编译产物，以及使用 ST-Link、`H7BOOT` 虚拟 U 盘和 STM32 ROM DFU 下载固件的方法。

本文以当前代码和脚本为准。正常更新过程中不需要反复断电，Bootloader、Motor 和 ROM DFU 之间都通过软件复位切换。

## 首先看这里：只有 USB 数据线时怎么烧录

只有一根连接 H7 USB 设备口的数据线、没有 ST-Link 时，先根据电脑当前识别到的设备选择脚本。

### 情况一：电脑已经显示 Motor 虚拟串口

设备管理器中可看到类似：

```text
USB 串行设备 (COM26)
VID_0483&PID_5740
```

日常更新 Motor 推荐调用：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File "C:\Users\Administrator\WANG_XIANGDA\H7_LU_motor\Tools\msc_download.ps1" `
  -NoPause
```

powershell -NoProfile -ExecutionPolicy Bypass `
  -File "C:\Users\91316\Desktop\H7_LU_motor\H7_LU_motor\Tools\dfu_download.ps1" `
  -ProgrammerCli "D:/stm32tools/STM32Programmer/bin/STM32_Programmer_CLI.exe" `
  -NoPause


该脚本只安装一次 Motor，自动执行：

```text
通过 Motor COM 口发送 BOOTMSC
  -> 等待 H7BOOT 虚拟 U 盘
  -> 复制 MDK-ARM/H743/H743.hex
  -> 等待 Bootloader 写入完成并自动重启 Motor
  -> 向重新出现的 Motor COM 口发送 ?
  -> 收到 [MOTOR] 后明确输出 SUCCESS
```

这是只有 USB 数据线时最常用、最推荐的 Motor 更新方式。

执行前必须关闭占用 Motor COM 口的串口助手。COM 号不一定是 `COM26`，脚本会自动查找；如果电脑同时连接了多块同类板卡，可显式指定：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\Tools\msc_download.ps1 `
  -ComPort COM26 `
  -NoPause
```

### 情况二：电脑已经显示 H7BOOT 虚拟 U 盘

仍然调用同一个脚本：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File "C:\Users\Administrator\WANG_XIANGDA\H7_LU_motor\Tools\msc_download.ps1" `
  -NoPause
```

脚本检测到 `H7BOOT` 后会直接复制 `H743.hex`，不再发送串口命令。也可以手动把 `MDK-ARM/H743/H743.hex` 拖入 `H7BOOT`，但复制后不要立即弹出 U 盘或断电，必须等待 `H7BOOT` 自动消失。

### 情况三：希望通过 ROM DFU 更新 Motor

板子当前为 Motor 虚拟串口，或者已经进入 `DFU in FS Mode` 时，调用：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File "C:\Users\Administrator\WANG_XIANGDA\H7_LU_motor\Tools\dfu_download.ps1" `
  -NoPause
```

板子在 Motor 模式时，该脚本自动执行：

```text
通过 Motor COM 口发送 BOOTDFU
  -> Motor 写入 DFU 请求并复位
  -> 自定义 Bootloader 跳转到 STM32 ROM DFU
  -> CubeProgrammer 下载并校验 H743.hex
  -> 从 0x08000000 启动 Bootloader，再进入 Motor
```

这也是只使用 H7 USB 数据线，不需要 ST-Link。

### 情况四：需要同时验证 H7BOOT 和 ROM DFU

调用完整验证脚本：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File "C:\Users\Administrator\WANG_XIANGDA\H7_LU_motor\Tools\verify_motor_usb_update.ps1"
```

该脚本用于验收两条 USB 下载通道，会先通过 `H7BOOT` 安装一次 Motor，再进入 ROM DFU 下载同一份 Motor HEX。因此它会下载两次，不是日常更新必须执行的脚本。

### 情况五：USB 没有 COM、没有 H7BOOT，但能用 BOOT0

如果用户 Flash 已损坏或被擦除，但硬件可以把 `BOOT0` 拉高：

1. 设置 `BOOT0=1`。
2. 复位或重新上电，电脑应识别 `STM32 BOOTLOADER` 或 `DFU in FS Mode`。
3. 先通过 ROM DFU 恢复自定义 Bootloader：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File "C:\Users\Administrator\WANG_XIANGDA\H7_LU_motor\Tools\dfu_download.ps1" `
  -HexPath "Bootloader\MDK-ARM\Bootloader\Bootloader.hex" `
  -GoAddress 0x08000000 `
  -SkipSerialDetach `
  -NoPause
```

4. 下载成功后设置 `BOOT0=0`，再复位或重新上电。
5. 等待 `H7BOOT` 出现，再运行 `msc_download.ps1` 安装 Motor。

如果 USB 没有任何设备、不能通过 `BOOT0=1` 进入 ROM DFU，并且自定义 Bootloader 已损坏，那么只靠 USB 数据线无法恢复，必须使用 ST-Link。

### USB 下载脚本选择表

| 当前状态或目的 | 调用脚本 | 是否下载两次 |
| --- | --- | --- |
| Motor COM 正常，日常更新 Motor | `Tools/msc_download.ps1` | 否 |
| 已显示 H7BOOT，安装 Motor | `Tools/msc_download.ps1` | 否 |
| 使用 ROM DFU 更新 Motor | `Tools/dfu_download.ps1` | 否 |
| 同时验收 H7BOOT 和 ROM DFU | `Tools/verify_motor_usb_update.ps1` | 是 |
| ROM DFU 中恢复 Bootloader | `Tools/dfu_download.ps1 -HexPath Bootloader/.../Bootloader.hex` | 否 |
| USB 完全失效且无法进入 ROM DFU | USB 脚本不可用，改用 ST-Link | - |

## 1. 当前工程组成

工程包含两个独立固件：

| 固件 | Keil 工程 | 生成的 HEX | Flash 地址 |
| --- | --- | --- | --- |
| 自定义 Bootloader | `Bootloader/MDK-ARM/Bootloader.uvprojx` | `Bootloader/MDK-ARM/Bootloader/Bootloader.hex` | `0x08000000-0x0803FFFF` |
| Motor 应用 | `MDK-ARM/H743.uvprojx` | `MDK-ARM/H743/H743.hex` | `0x08040000-0x081FFFFF` |

Flash 布局如下：

```text
0x08000000  +-------------------------------+
            | 自定义 Bootloader，256 KB     |
0x08040000  +-------------------------------+
            | Motor 应用                    |
            | H743.hex 必须从这里开始       |
0x08200000  +-------------------------------+
```

重要限制：

1. `Bootloader.hex` 必须从 `0x08000000` 开始。
2. `H743.hex` 必须从 `0x08040000` 开始，不能链接到 `0x08000000`。
3. 全片擦除会同时删除 Bootloader 和 Motor，擦除后必须先恢复 Bootloader。
4. 只更新 Motor 时不要全片擦除，否则会把 USB 虚拟 U 盘功能一起删除。

当前 Motor 应用提供 USB CDC 命令、USART1 数据转发、三路电机控制、两路 DYP-A23 声呐和 JY901S 串口 IMU：

| 电机 | PWM | 方向 |
| --- | --- | --- |
| Top ESC | `PD12/TIM4_CH1`，50 Hz；上电5%保持3秒后自动切到7.5%半油门 | PWM-only |
| Front roller | `PF8/TIM13_CH1` | `PA4` |
| Right | `PF7/TIM17_CH1` | `PA7` |
| Left | `PF6/TIM16_CH1` | `PC4` |

| 传感器 | 发送/接收 | 串口参数 |
| --- | --- | --- |
| DYP-A23 声呐 1 | `PA2/PA3`，USART2 TX/RX | `115200 8N1` |
| DYP-A23 声呐 2 | `PD1/PD0`，UART4 TX/RX | `115200 8N1` |
| JY901S IMU | `PE8/PE7`，UART7 TX/RX | 自动扫描，当前实机锁定 `9600 8N1` |
| 北醒 VLS-H5 雷达 | `PC6/PC7`，USART6 TX/RX | 自动扫描常用波特率 |

USB CDC 发送 `SENSORS` 或 `STATUS` 可立即输出两路声呐距离、VLS-H5 四方向距离和 JY901S 姿态状态。

## 2. 三种 USB 工作模式

### 2.1 Motor 应用模式

Motor 正常运行时，电脑识别为 USB 虚拟串口：

```text
VID: 0x0483
PID: 0x5740
设备示例: USB 串行设备 (COM26)
串口参数: 115200 8N1
```

COM 号由 Windows 分配，不保证每台电脑都是 `COM26`。发送 `?` 后应看到类似内容：

```text
[USB] Send BOOTMSC or UPDATE to enter USB disk bootloader
[USB] Send DFU or BOOTDFU to enter STM32 ROM USB DFU bootloader
[MOTOR] Usage: MOTOR <T|R|L> <CW|CCW> <0-100>
```

与下载相关的命令：

| 命令 | 作用 |
| --- | --- |
| `BOOTMSC` 或 `UPDATE` | 软件复位并进入自定义 Bootloader，显示 `H7BOOT` 虚拟 U 盘 |
| `BOOTDFU` 或 `DFU` | 写入 DFU 请求并软件复位，由自定义 Bootloader 进入 STM32 ROM DFU |

### 2.2 H7BOOT 虚拟 U 盘模式

自定义 Bootloader 工作时，电脑识别为 CDC+MSC 复合设备：

```text
VID: 0x0483
PID: 0x5741
卷标: H7BOOT
```

Bootloader 的行为：

1. 如果 Motor 应用有效且没有收到启动请求，Bootloader 自动跳转到 `0x08040000`。
2. 如果应用无效，或者 Motor 发送了 `BOOTMSC/UPDATE`，Bootloader 保持在 `H7BOOT` 模式。
3. 收到 Intel HEX 文件后，Bootloader 只允许写入 Motor 应用区域。
4. 安装成功后，Bootloader 自动断开 USB、复位并启动 Motor。

`H7BOOT` 虚拟磁盘总大小为 256 KB，可用空间略小于 256 KB。当前 `H743.hex` 可以放入；如果以后 HEX 文本文件超过虚拟磁盘容量，应改用 ROM DFU 或 ST-Link。

### 2.3 STM32 ROM DFU 模式

这是 STM32H743 芯片内部固化的系统 Bootloader，不在用户 Flash 中。CubeProgrammer 通常显示：

```text
Product ID: DFU in FS Mode
VID/PID: 0483:DF11
Device ID: 0x0450
```

当前 Motor 的 `BOOTDFU` 不在 FreeRTOS 任务中直接切换栈并跳转，而是执行以下流程：

```text
Motor 写入 DFU 请求标志
  -> 系统复位
  -> 自定义 Bootloader 读取请求
  -> 跳转到 STM32 ROM DFU
```

这种方式不需要手动改变 `BOOT0`，也不需要中途断电。

## 3. 编译固件

在工程根目录执行 Motor 增量编译：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\keil_build.ps1 -Action build
```

Motor 全量重编译：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\keil_build.ps1 -Action rebuild
```

编译 Bootloader：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\keil_build.ps1 `
  -Action rebuild `
  -Project "Bootloader\MDK-ARM\Bootloader.uvprojx" `
  -Log "Bootloader\MDK-ARM\Bootloader\Bootloader.build_log.htm"
```

成功标志：

```text
0 Error(s), 0 Warning(s)
[Keil] Build passed.
```

每次生成 Motor HEX 后都应确认它从 `0x08040000` 开始。完整验证脚本会自动检查 HEX 地址范围，地址不正确时拒绝下载。

## 4. 首次烧录或全片擦除后的恢复

首次使用空芯片，或者执行过全片擦除时，Flash 中没有自定义 Bootloader，不能直接使用 `H7BOOT`。推荐先用 ST-Link 恢复 Bootloader。

准备条件：

1. 安装 STM32CubeProgrammer。
2. 连接 ST-Link 的 SWD、SWCLK、NRST、GND 和目标板电源参考。
3. 正常启动时保持 `BOOT0=0`。
4. 使用确认能传输数据的 USB 线连接 H7 USB 设备口。

在 PowerShell 中执行全片擦除：

```powershell
$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
& $cli -c port=SWD mode=UR reset=HWrst -e all
```

然后烧录并校验 Bootloader：

```powershell
& $cli `
  -c port=SWD mode=UR reset=HWrst `
  -d "C:\Users\Administrator\WANG_XIANGDA\H7_LU_motor\Bootloader\MDK-ARM\Bootloader\Bootloader.hex" `
  -v `
  -g 0x08000000
```

预期输出：

```text
Mass erase successfully achieved
Download verified successfully
Start operation achieved successfully
```

空片只有 Bootloader、没有有效 Motor 时，启动后应出现 `H7BOOT`。然后按第 5 节安装 Motor。

## 5. 使用 H7BOOT 安装 Motor

### 5.1 推荐：执行 MSC 下载脚本

无论板子当前处于 Motor CDC 模式还是已经显示 `H7BOOT`，都可以执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File "C:\Users\Administrator\WANG_XIANGDA\H7_LU_motor\Tools\msc_download.ps1" `
  -NoPause
```

脚本会自动：

1. 查找 Motor USB CDC。
2. 必要时向串口发送 `BOOTMSC/UPDATE`。
3. 等待 `H7BOOT` 出现。
4. 把 `MDK-ARM/H743/H743.hex` 复制到虚拟磁盘。
5. 等待 Bootloader 完成安装并自行断开 `H7BOOT`。

成功输出：

```text
[MSC] Copying to E:\H743.hex ...
[MSC] Waiting ... for the bootloader to install and disconnect ...
[MSC] H7BOOT disconnected; waiting ... for Motor startup ...
[MSC] SUCCESS: Motor download verified on COM26.
```

只有看到最后一行 `SUCCESS`，脚本才会以退出码 `0` 返回。`H7BOOT` 断开但 Motor 没有重新响应时，脚本会输出 `ERROR` 并返回非零退出码。

### 5.2 手动拖拽 HEX

1. Motor 正常运行时，打开 USB CDC 串口，参数为 `115200 8N1`。
2. 发送一行 `BOOTMSC`，行尾使用 CRLF 或 LF。
3. 等待 Windows 出现卷标为 `H7BOOT` 的虚拟 U 盘。
4. 把 `MDK-ARM/H743/H743.hex` 复制到 `H7BOOT` 根目录。
5. 复制完成后不要立即点击“弹出 U 盘”，也不要立即拔线或断电。
6. 等待 Bootloader 自行安装；成功时 `H7BOOT` 会自动消失，随后 Motor CDC 会重新出现。
7. 打开新出现的 COM 口并发送 `?`，确认返回 `[MOTOR]` 帮助信息。

如果复制后 `H7BOOT` 长时间不消失，表示安装没有触发或 HEX 无效。不要反复复制，先检查 HEX 是否从 `0x08040000` 开始、文件是否超过虚拟磁盘容量以及 USB 线是否稳定。

### 5.3 Keil Download 按钮

当前 `MDK-ARM/H743.uvprojx` 的 Download 用户命令配置为：

```text
..\Tools\msc_download.bat -NoPause
```

因此 Motor 工程中的 Keil Download 按钮执行的是 `H7BOOT` MSC 安装流程，不是 ST-Link Flash Algorithm 下载。执行前应关闭占用 Motor COM 口的串口工具。

## 6. 使用 STM32 ROM DFU 更新 Motor

### 6.1 推荐：执行 DFU 下载脚本

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File "C:\Users\Administrator\WANG_XIANGDA\H7_LU_motor\Tools\dfu_download.ps1" `
  -NoPause
```

如果 Motor 正在运行，脚本会自动发送 `BOOTDFU`；如果板子已经处于 ROM DFU，脚本会直接下载。下载文件为 `MDK-ARM/H743/H743.hex`，流程包含擦除对应应用扇区、下载、校验和从 `0x08000000` 启动。

成功输出应包含：

```text
STM32 ROM DFU detected
File download complete
Download verified successfully
[DFU] Done.
```

编译后立即通过 DFU 下载，也可以执行：

```cmd
Tools\build_and_dfu_download.bat
```

### 6.2 手动使用 STM32CubeProgrammer GUI

1. 关闭占用 Motor COM 口的串口软件。
2. 向 Motor CDC 发送 `BOOTDFU`。
3. 等待 Windows 和 CubeProgrammer 识别 `DFU in FS Mode`。
4. 在 CubeProgrammer 右上角选择 `USB` 并连接。
5. 选择 `MDK-ARM/H743/H743.hex`，执行 Download，并启用 Verify。
6. 下载完成后从 `0x08000000` 启动，使自定义 Bootloader 检查并跳转到 Motor。

## 7. 一键完整验证

完整验证脚本位置：

```text
Tools/verify_motor_usb_update.ps1
```

执行命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File "C:\Users\Administrator\WANG_XIANGDA\H7_LU_motor\Tools\verify_motor_usb_update.ps1"
```

脚本依次完成：

```text
H7BOOT 安装 Motor
  -> 等待 Motor CDC 并发送 ? 验证
  -> 发送 BOOTDFU
  -> 等待 STM32 ROM DFU
  -> DFU 再次下载并校验 H743.hex
  -> 等待 Motor CDC 重启并再次验证
```

最终成功标志：

```text
[VERIFY] PASS: H7BOOT install, Motor BOOTDFU, ROM DFU download, and final Motor startup all succeeded.
```

只做环境和地址检查、不改变板子状态：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\Tools\verify_motor_usb_update.ps1 `
  -PreflightOnly
```

该脚本不负责 ST-Link 全片擦除和 Bootloader 首次烧录。自定义 Bootloader 必须已经存在。

## 8. Bootloader 自身的更新

Bootloader 位于 `0x08000000`，不能通过 `H7BOOT` 虚拟 U 盘更新；`H7BOOT` 只允许安装 `0x08040000` 以后的 Motor 应用。

更新 Bootloader 有两种方式：

1. 推荐使用 ST-Link，命令见第 4 节。只执行 `-d Bootloader.hex -v` 时只擦写 Bootloader 对应扇区，不会主动全片擦除。
2. 使用 STM32 ROM DFU 下载 `Bootloader.hex`。当前 Bootloader Keil 工程的 Download 用户命令调用 `Tools/dfu_download.bat`，目标文件是 Bootloader HEX。

恢复故障板时优先使用 ST-Link，因为它不依赖现有 Motor CDC、自定义 Bootloader或 ROM DFU 的 USB 枚举状态。

## 9. BOOT0 和重新上下电规则

正常状态：

```text
BOOT0=0
```

以下流程正常都不需要手动断电：

```text
Motor -> H7BOOT
H7BOOT 安装 -> Motor
Motor -> ROM DFU
ROM DFU 下载 -> Bootloader -> Motor
```

只有以下情况才考虑重新上下电：

1. 软件复位后 USB 超过脚本等待时间仍未重新枚举。
2. Windows 一直显示“未知 USB 设备”，并且确认固件正在运行。
3. 修改了 `BOOT0` 电平，需要重新采样启动模式。

重新上下电前应先检查：

1. USB 线必须支持数据传输。仅充电线会导致无设备或描述符请求失败。
2. 使用 H7 的 USB 设备口，不要接到仅供电口。
3. 优先直连电脑 USB 口，排除扩展坞和不稳定 Hub。
4. 关闭占用 COM 口的串口终端。
5. 确认板卡、ST-Link 和电脑共地，目标电压约为 3.3 V。

## 10. 故障恢复顺序

### 10.1 Motor 能显示 COM 口

优先执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\msc_download.ps1 -NoPause
```

或执行 `dfu_download.ps1`。

### 10.2 只有 H7BOOT

直接运行 `msc_download.ps1`，或手动复制 `H743.hex` 后等待自动重启。

### 10.3 只有 STM32 ROM DFU

如果自定义 Bootloader 确认仍存在，可用 `dfu_download.ps1` 下载 Motor。如果 Bootloader 已被擦除，先通过 ROM DFU 或 ST-Link写入 `Bootloader.hex`，再保持 `BOOT0=0` 复位，最后通过 `H7BOOT` 安装 Motor。

### 10.4 USB 完全无设备，但 ST-Link 可连接

按第 4 节执行：

```text
全片擦除
  -> ST-Link 烧 Bootloader.hex
  -> H7BOOT 安装 H743.hex
```

### 10.5 Windows 报“设备描述符请求失败”

1. 更换一根确认可以传数据的 USB 线。
2. 更换电脑直连 USB 口。
3. 用 ST-Link Hot Plug 确认 CPU 是否仍在 Bootloader 或 Motor 中运行。
4. 如果更换数据线后 COM 或 `H7BOOT` 恢复，说明固件和下载地址没有问题。

## 11. 日志文件

| 流程 | 日志 |
| --- | --- |
| Keil Motor 编译 | `MDK-ARM/H743/H743.build_log.htm` |
| H7BOOT MSC 下载 | `MDK-ARM/H743/msc_download.log` |
| ROM DFU 下载 | `MDK-ARM/H743/dfu_download.log` |
| 完整 USB 验证 | `MDK-ARM/H743/motor_update_verify.log` |

出现失败时，先查看对应日志的最后一条 `[MSC]`、`[DFU]` 或 `[VERIFY]` 信息，再判断故障发生在 USB 模式切换、文件复制、Flash 下载还是应用重启阶段。

## 12. 当前已验证结果

当前版本已经实际完成以下闭环测试：

```text
全片擦除成功
  -> ST-Link 烧录并校验 Bootloader
  -> H7BOOT 安装 Motor
  -> Motor 在 USB CDC 上响应
  -> BOOTDFU 进入 STM32 ROM DFU
  -> ROM DFU 下载并校验同一份 H743.hex
  -> Motor 重启并再次响应
```

完整验证脚本最终返回 `PASS`。测试中确认 USB 数据线质量会直接影响 CDC、MSC 和 ROM DFU 三种模式；出现描述符读取失败时应首先更换数据线。
