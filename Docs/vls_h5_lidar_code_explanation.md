# VLS-H5 激光雷达代码说明书

本文按当前工程代码整理，适合刚开始看 C 语言和 STM32 工程时对照阅读。重点解释 VLS-H5 激光雷达的数据怎么进来、怎么变成前后左右距离、怎么影响电机，以及相关 C 语法是什么意思。

## 1. 先记住结论

VLS-H5 当前使用 `USART3`：

| 信号 | STM32 引脚 | 方向 |
| --- | --- | --- |
| VLS-H5 RX | PB10 / USART3_TX | STM32 发启动命令给雷达 |
| VLS-H5 TX | PB11 / USART3_RX | 雷达发数据给 STM32 |
| GND | GND | 必须共地 |

串口参数是 `230400 8N1`。

当前代码里，VLS-H5 对电机的影响已经改成：

1. 手动命令 `M/A/B/C/D/0` 会退出自动巡航。
2. 手动模式下，VLS-H5 不会因为前后距离小于 `400mm` 把电机速度清零。
3. 只有自动巡航 `X` 开启后，VLS-H5 和其他测距融合结果才会参与避障停车。

所以如果你手动发 `M,-30,-30` 或 `A`，雷达数据正常时不应该再把左右电机挡住。

## 2. 相关文件

| 文件 | 作用 |
| --- | --- |
| `Core/Src/main.c` | 初始化 USART3、定时器、PWM、UART 接收 |
| `Core/Src/stm32h7xx_hal_msp.c` | 配置 USART3 的 PB10/PB11 引脚 |
| `Core/Src/uart_port.c` | 通用串口接收框架，负责把 USART3 收到的字节送给回调 |
| `Core/Src/vls_h5_lidar.c` | VLS-H5 协议解析，处理启动命令、ACK、点云帧和四方向距离 |
| `Core/Inc/vls_h5_lidar.h` | VLS-H5 对外接口和数据结构 |
| `Core/Src/app_control.c` | 把 VLS-H5 距离接入日志、融合距离、自动巡航和避障 |

## 3. 整体数据流

VLS-H5 的运行链路可以这样看：

```text
main.c 初始化 USART3
        |
        v
App_UART_Init() 开启每个 UART 的中断接收
        |
        v
App_Tasks_Init()
  - VlsH5_Init(APP_UART_3)
  - App_UART_SetMode(APP_UART_3, APP_UART_MODE_STREAM)
        |
        v
Range_Task 启动后延时 500ms
        |
        v
VlsH5_Start() 通过 PB10 发送 A5 5A 01 00 00
        |
        v
VLS-H5 通过 PB11 持续发送数据
        |
        v
USART3 中断 -> UART3_rxCallback()
        |
        v
VlsH5_InputBytes() 逐字节解析雷达数据帧
        |
        v
g_vls_front_mm / right / left / rear 更新
        |
        v
$VLS_H5 和 $FUSED 日志输出
        |
        v
自动巡航开启时参与前后避障
```

## 4. 雷达启动命令

启动命令在 `Core/Src/vls_h5_lidar.c`：

```c
void VlsH5_Start(void)
{
    static const uint8_t start_cmd[5] = {0xA5U, 0x5AU, 0x01U, 0x00U, 0x00U};

    g_vls_h5_ack_index = 0U;
    g_vls_h5_pending_command = VLS_H5_COMMAND_START;
    (void)App_UART_Send(g_vls_h5_port, start_cmd, sizeof(start_cmd));
}
```

实际发出去的 5 个字节是：

```text
A5 5A 01 00 00
```

它在 `Range_Task()` 里调用：

```c
osDelay(500U);
VlsH5_Start();
```

含义是系统任务启动后先等 `500ms`，再发一次启动命令。

如果雷达收到命令并回复 `4F 4B`，代码会把 `command_ack_count` 加 1：

```c
if (byte == VLS_H5_ACK_SECOND_BYTE)
{
    g_vls_h5_data.command_ack_count++;
    g_vls_h5_data.running = (g_vls_h5_pending_command == VLS_H5_COMMAND_START) ? 1U : 0U;
    g_vls_h5_pending_command = VLS_H5_COMMAND_NONE;
}
```

在日志里对应 `$VLS_H5` 最后一项 `ack_count`。

## 5. 串口接收是怎么进来的

`App_Tasks_Init()` 里有两句：

```c
VlsH5_Init(APP_UART_3);
App_UART_SetMode(APP_UART_3, APP_UART_MODE_STREAM);
```

`APP_UART_3` 对应 `USART3`。`APP_UART_MODE_STREAM` 表示流模式：串口中断收到 1 个字节后，先把它放进环形缓冲，后面由 `Uart_Parse_Task()` 取出来再交给 VLS-H5 解析。

在 `uart_port.c` 里：

```c
if (ctx->mode == APP_UART_MODE_STREAM)
{
    uint8_t byte = ctx->rx_byte;
    App_UART_PushStreamByte(ctx, byte);
    App_UART_Rearm(ctx);
    return;
}
```

这段的意思是：

1. 从串口中断里拿到刚收到的 1 个字节。
2. 把这个字节放进环形缓冲。
3. 立刻重新打开下一次接收。

随后 `Uart_Parse_Task()` 会调用：

```c
size = App_UART_ReadStream(&app_uarts[i], frame, (uint16_t)sizeof(frame));
App_UART_DispatchFrame(i, frame, size);
```

这段才会把缓冲里的数据分发给对应 UART 的回调函数。

USART3 最终会进入 `app_control.c`：

```c
void UART3_rxCallback(uint8_t *packet, uint16_t size)
{
    VlsH5_InputBytes(packet, size);
}
```

因为流模式会从环形缓冲批量取数据，所以这里的 `size` 可能是 `1`，也可能一次包含多个字节。

## 6. VLS-H5 数据帧怎么解析

VLS-H5 的数据帧由 `VlsH5_InputBytes()` 逐字节解析：

```c
void VlsH5_InputBytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t byte;

    if (data == NULL)
    {
        return;
    }

    for (i = 0U; i < len; i++)
    {
        byte = data[i];
        parse_command_ack_byte(byte);
        ...
    }
}
```

这段 C 语法解释：

- `const uint8_t *data`：`data` 是一个指针，指向收到的数据。`const` 表示函数不会修改这些数据。
- `uint16_t len`：收到的数据长度。
- `if (data == NULL)`：如果指针为空，就直接返回，避免程序崩溃。
- `for (i = 0U; i < len; i++)`：循环处理每一个字节。
- `byte = data[i];`：取出第 `i` 个字节。

代码只接受以 `0x54 0x2C` 开头的 47 字节数据帧：

```c
#define VLS_H5_HEADER              0x54U
#define VLS_H5_VER_LEN            0x2CU
#define VLS_H5_FRAME_SIZE           47U
```

解析过程大概是：

1. 等待第 1 个字节是 `0x54`。
2. 等待第 2 个字节是 `0x2C`。
3. 继续收满 47 字节。
4. 做 CRC 校验。
5. 校验通过后解析角度、距离、强度。
6. 更新前后左右扇区的最近距离。

CRC 校验失败时：

```c
if (g_vls_h5_data.crc_ok == 0U)
{
    g_vls_h5_data.crc_error_count++;
    return;
}
```

也就是错误计数加 1，然后丢弃这一帧。

## 7. 前后左右距离怎么来的

雷达一帧里有多个点，每个点有角度和距离。代码把角度划成 4 个扇区：

```c
#define VLS_H5_FRONT_MIN_CDEG     34500U
#define VLS_H5_FRONT_MAX_CDEG     1500U
#define VLS_H5_RIGHT_MIN_CDEG     7500U
#define VLS_H5_RIGHT_MAX_CDEG     10500U
#define VLS_H5_REAR_MIN_CDEG      16500U
#define VLS_H5_REAR_MAX_CDEG      19500U
#define VLS_H5_LEFT_MIN_CDEG      25500U
#define VLS_H5_LEFT_MAX_CDEG      28500U
```

这里的单位是 `cdeg`，也就是百分之一度：

```text
1500  = 15.00 度
9000  = 90.00 度
18000 = 180.00 度
27000 = 270.00 度
34500 = 345.00 度
```

前方扇区是 `34500..36000` 和 `0..1500`，也就是车头正前方附近。

每个扇区只保留最近的有效距离：

```c
if ((g_vls_h5_data.sector_distance_mm[sector] == VLS_H5_INVALID_DISTANCE_MM) ||
    (distance_mm < g_vls_h5_data.sector_distance_mm[sector]))
{
    g_vls_h5_data.sector_distance_mm[sector] = distance_mm;
}
```

含义是：

1. 如果这个方向还没有有效距离，就保存当前点。
2. 如果当前点比原来的距离更近，就更新为当前点。
3. 所以日志里的前后左右距离是该扇区里最近的障碍距离。

## 8. 无效距离和过期处理

无效距离定义在头文件：

```c
#define VLS_H5_INVALID_DISTANCE_MM  65535U
```

看到日志里某个方向是 `65535`，表示这个方向当前没有有效数据。

有效距离判断：

```c
return ((distance_mm != 0U) &&
        (distance_mm <= VLS_H5_MAX_DISTANCE_MM) &&
        (distance_mm != VLS_H5_INVALID_DISTANCE_MM));
```

含义是：

- `0` 无效
- 大于 `5000mm` 无效
- `65535` 无效

旧数据过期时间：

```c
#define VLS_H5_SECTOR_STALE_MS    500U
```

如果某个方向超过 `500ms` 没有新数据，会变回 `65535`。

## 9. $VLS_H5 日志怎么读

日志格式在 `sonar_format_and_send_on_uart1()`：

```text
$VLS_H5,time,front,right,left,rear,frame_count,crc_ok,crc_error_count,running,ack_count
```

例如：

```text
$VLS_H5,12.300,350,65535,800,65535,1024,1,0,1,1
```

逐项解释：

| 字段 | 含义 |
| --- | --- |
| `12.300` | 系统运行时间，单位秒 |
| `350` | 前方最近距离，单位 mm |
| `65535` | 右方无有效距离 |
| `800` | 左方最近距离 800mm |
| `65535` | 后方无有效距离 |
| `1024` | 已成功解析的数据帧数量 |
| `1` | 最近一帧 CRC 正确 |
| `0` | CRC 错误帧累计数量 |
| `1` | 雷达处于运行/已收到有效帧 |
| `1` | 收到启动 ACK 的次数 |

如果 `frame_count` 一直增加，说明接收和解析基本正常。

如果 `crc_error_count` 增长很快，优先查：

1. 波特率是否 `230400`
2. GND 是否共地
3. 雷达 TX 到 STM32 RX 的线是否太长或干扰大
4. 电源是否有明显纹波或掉压

## 10. $FUSED 日志和 VLS-H5 的关系

`$FUSED` 是融合后的前后左右距离。前方融合代码：

```c
range_mm = App_Min_Valid_Range(g_sonar_front_mm, g_tf_uw500_distance_mm);
range_mm = App_Min_Valid_Range(range_mm, g_vls_front_mm);
return range_mm;
```

含义是：

1. 先在前声纳和 TF-UW500 之间选一个有效且更近的距离。
2. 再和 VLS-H5 前方距离比较。
3. 最后返回更近的有效距离。

所以 `$FUSED` 不是单独的 VLS-H5 数据，而是多个传感器融合后的结果。

## 11. 雷达以前为什么会影响手动电机

之前 `PWM_Ramp_Task()` 每 3ms 都调用：

```c
App_Apply_Obstacle_Guard();
```

这个函数原本不区分手动/自动，只要目标速度是前进，并且前方距离小于 `400mm`，就清零电机目标速度：

```c
if ((g_target_left_power < 0) && (g_target_right_power < 0) &&
    (App_Is_Too_Close(front_mm, FRONT_BLOCK_DISTANCE_MM) != 0U))
{
    App_Set_Target_Power(0, 0);
}
```

本工程里负数是前进方向，所以 `M,-30,-30` 会被看成前进。

如果 VLS-H5 检测到前方小于 `400mm`，目标速度会立刻被清零。你看到的现象就是：雷达一启动，电机命令发了但不转。

## 12. 现在代码怎么避免这个影响

现在 `App_Apply_Obstacle_Guard()` 开头加了判断：

```c
if (g_auto_cruise_enable == 0U)
{
    return;
}
```

含义是：

- 如果自动巡航没有开启，直接退出，不做前后避障清零。
- 只有 `g_auto_cruise_enable == 1` 时，才继续检查前后距离。

手动命令也会主动关闭自动巡航。

`M,left,right` 命令里：

```c
g_auto_cruise_enable = 0U;
return 1U;
```

`A/B/C/D/0` 命令里也会设置：

```c
g_auto_cruise_enable = 0U;
```

所以现在手动调车时，VLS-H5 只负责输出距离日志，不再挡住左右电机。

## 13. 自动巡航时雷达怎么参与控制

自动巡航由命令 `X` 开启：

```c
if (cmd == 'X')
{
    g_auto_cruise_enable = 1U;
    App_Command_Ack(reply_port);
    return;
}
```

关闭命令是 `Y`：

```c
if (cmd == 'Y')
{
    g_auto_cruise_enable = 0U;
    App_Set_Target_Power(0, 0);
    App_Command_Ack(reply_port);
    return;
}
```

自动巡航开启后：

1. `App_Apply_Obstacle_Guard()` 会检查前后距离。
2. 自动巡航任务本身也会用 `App_Get_Front_Range_Mm()` 判断前方是否太近。
3. 如果前方小于 `400mm`，会停车并进入避障转向流程。

这就是“自动模式保留雷达保护，手动模式不挡电机”的设计。

## 14. 常见 C 语法解释

### 14.1 `#define`

例子：

```c
#define VLS_H5_FRAME_SIZE 47U
```

这是宏定义。编译前，代码里的 `VLS_H5_FRAME_SIZE` 会被替换成 `47U`。

`U` 表示 unsigned，也就是无符号整数。

### 14.2 `uint8_t`、`uint16_t`、`uint32_t`

这些是固定长度整数：

| 类型 | 大小 | 范围 |
| --- | --- | --- |
| `uint8_t` | 8 位 | 0..255 |
| `uint16_t` | 16 位 | 0..65535 |
| `uint32_t` | 32 位 | 0..4294967295 |

串口字节通常用 `uint8_t`。

距离 `mm` 用 `uint16_t`，因为最大 `65535` 足够表示。

### 14.3 `static`

例子：

```c
static VlsH5Data_t g_vls_h5_data;
```

在 `.c` 文件里的全局变量前加 `static`，表示这个变量只允许当前文件使用，其他文件不能直接访问。

这样做可以减少误用。

### 14.4 `volatile`

例子：

```c
volatile uint8_t g_auto_cruise_enable = 0U;
```

`volatile` 告诉编译器：这个变量可能会被中断、任务或其他地方随时改变，不要过度优化读取。

在嵌入式代码里，跨任务、跨中断使用的状态变量经常会用 `volatile`。

### 14.5 `typedef enum`

例子：

```c
typedef enum
{
    VLS_H5_SECTOR_FRONT = 0,
    VLS_H5_SECTOR_RIGHT,
    VLS_H5_SECTOR_LEFT,
    VLS_H5_SECTOR_REAR,
    VLS_H5_SECTOR_COUNT
} VlsH5Sector_t;
```

这是枚举，用名字代表数字。

实际值大概是：

```text
VLS_H5_SECTOR_FRONT = 0
VLS_H5_SECTOR_RIGHT = 1
VLS_H5_SECTOR_LEFT  = 2
VLS_H5_SECTOR_REAR  = 3
VLS_H5_SECTOR_COUNT = 4
```

好处是代码更容易读，不用写神秘数字。

### 14.6 `typedef struct`

例子：

```c
typedef struct
{
    uint16_t angle_cdeg;
    uint16_t distance_mm;
    uint8_t intensity;
} VlsH5Point_t;
```

这是结构体。可以把多个相关变量打包成一个类型。

一个 `VlsH5Point_t` 代表雷达的一个点：

- `angle_cdeg`：角度
- `distance_mm`：距离
- `intensity`：强度

### 14.7 数组

例子：

```c
static uint8_t g_vls_h5_frame[VLS_H5_FRAME_SIZE];
```

这是一个数组，用来存一整帧雷达数据。

因为 `VLS_H5_FRAME_SIZE` 是 `47`，所以这个数组有 47 个元素：

```text
g_vls_h5_frame[0]  到  g_vls_h5_frame[46]
```

C 语言数组下标从 0 开始。

### 14.8 指针

例子：

```c
const uint8_t *data
```

`*data` 表示 `data` 是一个指针。可以理解为“数据的地址”。

串口回调传进来的不是一堆复制好的变量，而是一段内存的地址。代码通过 `data[i]` 读取第 `i` 个字节。

### 14.9 `if` 判断

例子：

```c
if (distance_mm <= 400U)
{
    App_Set_Target_Power(0, 0);
}
```

意思是：如果距离小于等于 400mm，就停车。

注意：

- `=` 是赋值
- `==` 是判断是否相等
- `!=` 是不等于
- `&&` 是并且
- `||` 是或者

### 14.10 `return`

例子：

```c
if (data == NULL)
{
    return;
}
```

`return` 表示当前函数结束。

这里的意思是：如果数据指针为空，后面不能继续读，直接退出函数。

## 15. 调试时先看哪些日志

VLS-H5 正常时：

```text
$VLS_H5,time,front,right,left,rear,frame_count,crc_ok,crc_error_count,running,ack_count
```

重点看：

1. `frame_count` 是否持续增加。
2. `crc_ok` 是否大多数时候是 `1`。
3. `crc_error_count` 是否快速增加。
4. `running` 是否为 `1`。
5. `front/rear` 是否经常小于 `400`。

手动电机测试时：

```text
M,-30,-30
M,30,30
M,-30,30
M,30,-30
0
```

如果手动命令下仍不转，同时 `PF6/PF7` PWM 波形正常，优先查电机驱动板：

1. 驱动板 PWM 输入端是否收到波形。
2. 方向脚 `PC4/PA7` 是否到达驱动板。
3. 电机电源是否掉压。
4. 驱动板 `EN/STBY/SLEEP/FAULT` 是否异常。
5. VLS-H5 上电后是否把 5V 或 GND 拉乱。

## 16. 现在已经修正和以后可能要优化的点

当前代码已经做了两个稳定性修正：

1. VLS-H5 启动命令不再只发一次。`Range_Task()` 在没有收到 ACK、也没有收到数据帧时，会每 1 秒重发一次 `VlsH5_Start()`。
2. VLS-H5 和 TF-UW500 的高频串口数据不再在 UART 中断里直接解析。中断里只把字节放进环形缓冲，`Uart_Parse_Task()` 再调用 `VlsH5_InputBytes()` 和 `TfUw500_InputBytes()`。

后续还可以继续优化：

1. `g_vls_h5_data` 现在由解析任务写、其他任务读。后续可以加快照或临界区，避免读到一半更新的数据。
2. 如果雷达实际安装方向和代码假设不同，需要调用 `VlsH5_SetAngleOffsetCdeg()` 设置角度偏移。

## 17. 你现在最该记住的运行规则

1. VLS-H5 数据从 `USART3 PB11` 进来。
2. STM32 从 `PB10` 发 `A5 5A 01 00 00` 启动雷达。
3. `vls_h5_lidar.c` 负责把雷达帧解析成前后左右距离。
4. `app_control.c` 把这些距离输出到 `$VLS_H5` 和 `$FUSED`。
5. 手动命令现在不会被 VLS-H5 避障打断。
6. 自动巡航开启后，VLS-H5 会参与前后 `400mm` 避障停车。
