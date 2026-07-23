#include "app_control.h"

#include "boot_shared.h"
#include "cmsis_os2.h"
#include "jy901s_uart.h"
#include "sonar.h"
#include "sonar_filter.h"
#include "uart_port.h"
#include "usb_cdc_port.h"
#include "vls_h5_lidar.h"
#include "dvl_uart.h"
#include "dvl_imu_fuser.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define LOG_RING_BUF_SIZE            2048U
#define LOG_TX_CHUNK_SIZE            63U
#define USB_COMMAND_BUF_SIZE         64U
#define DVL_LOG_BYTES_MAX            32U
#define SONAR_TRIGGER_GAP_MS          35U
#define SONAR_RESPONSE_WAIT_MS        100U
#define SONAR_COLLECT_PERIOD_MS       100U
#define DVL_COLLECT_PERIOD_MS					100U
#define SENSOR_REPORT_PERIOD_MS       1000U
#define JY901S_FRESH_TIMEOUT_MS       500U
#define VLS_H5_BAUD_PROBE_MS          2000U
#define JY901S_BAUD_PROBE_MS          1500U

#define DISABLED_RANGE_INVALID_MM    65535U

typedef struct
{
    uint32_t chunk_count;
    uint16_t last_chunk_size;
    uint8_t last_bytes[8];
} SonarUartDiag_t;

static uint8_t log_ring_buf[LOG_RING_BUF_SIZE];
static volatile uint32_t log_write_ptr = 0U;
static volatile uint32_t log_read_ptr = 0U;

uint16_t sonic_uart2 = 0U;
uint16_t sonic_uart4 = 0U;
uint16_t sonic_uart5 = 0U;
uint16_t sonic_uart6 = 0U;
uint16_t g_sonic_uart2 = DISABLED_RANGE_INVALID_MM;
uint16_t g_sonic_uart4 = DISABLED_RANGE_INVALID_MM;
uint16_t g_sonar1_mm = DISABLED_RANGE_INVALID_MM;
uint16_t g_sonar2_mm = DISABLED_RANGE_INVALID_MM;
uint16_t g_sonic_uart5 = DISABLED_RANGE_INVALID_MM;
uint16_t g_sonic_uart6 = DISABLED_RANGE_INVALID_MM;
uint16_t g_sonar_front_mm = DISABLED_RANGE_INVALID_MM;
uint16_t g_sonar_right_mm = DISABLED_RANGE_INVALID_MM;
uint16_t g_sonar_left_mm = DISABLED_RANGE_INVALID_MM;
uint16_t g_sonar_rear_mm = DISABLED_RANGE_INVALID_MM;
uint16_t g_tf_uw500_distance_mm = DISABLED_RANGE_INVALID_MM;
uint16_t g_vls_front_mm = DISABLED_RANGE_INVALID_MM;
uint16_t g_vls_right_mm = DISABLED_RANGE_INVALID_MM;
uint16_t g_vls_left_mm = DISABLED_RANGE_INVALID_MM;
uint16_t g_vls_rear_mm = DISABLED_RANGE_INVALID_MM;

static uint8_t g_usb_command_buf[USB_COMMAND_BUF_SIZE];
static uint16_t g_usb_command_len = 0U;
static SonarUartDiag_t g_sonar_uart_diag[2];
//static uint8_t g_dvl_log_bytes[DVL_LOG_BYTES_MAX];
static volatile uint16_t g_dvl_log_len = 0U;
static volatile uint32_t g_dvl_interval_rx = 0U;
static volatile uint32_t g_dvl_total_rx = 0U;

static void Uart_Parse_Task(void *argument);
static void Uart_Send_Task(void *argument);
//static void Sensor_Task(void *argument);
static void App_LogSensorStatus(void);
//static void App_LogDvlStatus(void);
static void App_RequestJy901sOutput(void);
static void App_DebugCommand_ProcessBytes(const uint8_t *packet, uint16_t size);
static void App_EnterUsbDiskBootloader(void);
static uint8_t App_DebugCommand_HandleMotor(const uint8_t *packet, uint16_t size);
static void App_UpdateSonarUartDiag(uint8_t index, const uint8_t *packet, uint16_t size);

static const osThreadAttr_t uart_parse_task_attributes =
{
    .name = "uartParse",
    .stack_size = 512U * 4U,
    .priority = (osPriority_t)osPriorityAboveNormal,
};

static const osThreadAttr_t uart_send_task_attributes =
{
    .name = "uartSend",
    .stack_size = 512U * 4U,
    .priority = (osPriority_t)osPriorityLow,
};

static const osThreadAttr_t sensor_task_attributes =
{
    .name = "sensors",
    .stack_size = 512U * 4U,
    .priority = (osPriority_t)osPriorityNormal,
};

static const uint32_t jy901s_baud_candidates[] =
{
    9600U,
    115200U,
    230400U,
    57600U,
    38400U,
    19200U,
    4800U,
};
/*
static const uint32_t vls_h5_baud_candidates[] =
{
    230400U,
    115200U,
    256000U,
    460800U,
    921600U,
    57600U,
};
*/
static void Log_Ring_WriteBlock(const uint8_t *data, uint16_t len)
{
    uint32_t primask;
    uint16_t copied;

    if ((data == NULL) || (len == 0U))
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    for (copied = 0U; copied < len; copied++)
    {
        uint32_t next_write_ptr;

        next_write_ptr = (log_write_ptr + 1U) % LOG_RING_BUF_SIZE;
        if (next_write_ptr == log_read_ptr)
        {
            log_read_ptr = (log_read_ptr + 1U) % LOG_RING_BUF_SIZE;
        }

        log_ring_buf[log_write_ptr] = data[copied];
        log_write_ptr = next_write_ptr;
    }

    if (primask == 0U)
    {
        __enable_irq();
    }
}

static uint16_t Log_Ring_ReadBlock(uint8_t *data, uint16_t max_len)
{
    uint32_t primask;
    uint16_t copied;

    if ((data == NULL) || (max_len == 0U))
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    copied = 0U;
    while ((copied < max_len) && (log_read_ptr != log_write_ptr))
    {
        data[copied] = log_ring_buf[log_read_ptr];
        copied++;
        log_read_ptr = (log_read_ptr + 1U) % LOG_RING_BUF_SIZE;
    }

    if (primask == 0U)
    {
        __enable_irq();
    }

    return copied;
}

void Log_Printf(const char *format, ...)
{
    char buf[192];
    va_list args;
    int len;

    if (format == NULL)
    {
        return;
    }

    va_start(args, format);
    len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (len < 0)
    {
        return;
    }

    if (len >= (int)sizeof(buf))
    {
        len = (int)sizeof(buf) - 1;
    }

    Log_Ring_WriteBlock((const uint8_t *)buf, (uint16_t)len);
}

static uint8_t App_Command_Equals(const uint8_t *packet, uint16_t size, const char *cmd)
{
    uint16_t cmd_len;
    uint16_t len;

    if ((packet == NULL) || (cmd == NULL) || (size == 0U))
    {
        return 0U;
    }

    len = size;
    while ((len != 0U) &&
           ((packet[len - 1U] == '\r') || (packet[len - 1U] == '\n') || (packet[len - 1U] == ' ')))
    {
        len--;
    }

    cmd_len = (uint16_t)strlen(cmd);
    if (len != cmd_len)
    {
        return 0U;
    }

    return (memcmp(packet, cmd, cmd_len) == 0) ? 1U : 0U;
}

static uint8_t App_IsCommandSpace(char ch)
{
    return ((ch == ' ') || (ch == '\t')) ? 1U : 0U;
}

static char App_ToUpper(char ch)
{
    if ((ch >= 'a') && (ch <= 'z'))
    {
        ch = (char)(ch - ('a' - 'A'));
    }

    return ch;
}

static uint8_t App_CopyTrimmedCommand(const uint8_t *packet,
                                      uint16_t size,
                                      char *buf,
                                      uint16_t buf_size)
{
    uint16_t start;
    uint16_t end;
    uint16_t len;
    uint16_t i;

    if ((packet == NULL) || (buf == NULL) || (buf_size == 0U))
    {
        return 0U;
    }

    start = 0U;
    while ((start < size) && App_IsCommandSpace((char)packet[start]) != 0U)
    {
        start++;
    }

    end = size;
    while ((end > start) &&
           ((packet[end - 1U] == '\r') ||
            (packet[end - 1U] == '\n') ||
            (packet[end - 1U] == ' ') ||
            (packet[end - 1U] == '\t')))
    {
        end--;
    }

    len = (uint16_t)(end - start);
    if (len >= buf_size)
    {
        len = (uint16_t)(buf_size - 1U);
    }

    for (i = 0U; i < len; i++)
    {
        buf[i] = App_ToUpper((char)packet[start + i]);
    }
    buf[len] = '\0';

    return (len != 0U) ? 1U : 0U;
}

static char *App_NextToken(char **cursor)
{
    char *token;
    char *s;

    if ((cursor == NULL) || (*cursor == NULL))
    {
        return NULL;
    }

    s = *cursor;
    while ((*s != '\0') && (App_IsCommandSpace(*s) != 0U))
    {
        s++;
    }

    if (*s == '\0')
    {
        *cursor = s;
        return NULL;
    }

    token = s;
    while ((*s != '\0') && (App_IsCommandSpace(*s) == 0U))
    {
        s++;
    }

    if (*s != '\0')
    {
        *s = '\0';
        s++;
    }
    *cursor = s;

    return token;
}

static uint8_t App_ParseMotorOutput(const char *token, MotorOutput_t *motor)
{
    if ((token == NULL) || (motor == NULL))
    {
        return 0U;
    }

    if ((strcmp(token, "T") == 0) || (strcmp(token, "TOP") == 0) || (strcmp(token, "1") == 0))
    {
        *motor = MOTOR_OUTPUT_TOP;
        return 1U;
    }
    if ((strcmp(token, "R") == 0) || (strcmp(token, "RIGHT") == 0) || (strcmp(token, "2") == 0))
    {
        *motor = MOTOR_OUTPUT_RIGHT;
        return 1U;
    }
    if ((strcmp(token, "L") == 0) || (strcmp(token, "LEFT") == 0) || (strcmp(token, "3") == 0))
    {
        *motor = MOTOR_OUTPUT_LEFT;
        return 1U;
    }
    if ((strcmp(token, "F") == 0) || (strcmp(token, "FRONT") == 0) ||
        (strcmp(token, "ROLLER") == 0) || (strcmp(token, "BRUSH") == 0) ||
        (strcmp(token, "4") == 0))
    {
        *motor = MOTOR_OUTPUT_FRONT_ROLLER;
        return 1U;
    }

    return 0U;
}

static const char *App_MotorOutputName(MotorOutput_t motor)
{
    switch (motor)
    {
    case MOTOR_OUTPUT_TOP:
        return "TOP";
    case MOTOR_OUTPUT_RIGHT:
        return "RIGHT";
    case MOTOR_OUTPUT_LEFT:
        return "LEFT";
    case MOTOR_OUTPUT_FRONT_ROLLER:
        return "FRONT_ROLLER";
    default:
        return "?";
    }
}

static uint8_t App_ParseMotorDir(const char *token, MotorDir_t *dir)
{
    if ((token == NULL) || (dir == NULL))
    {
        return 0U;
    }

    if (strcmp(token, "CW") == 0)
    {
        *dir = MOTOR_DIR_CW;
        return 1U;
    }
    if (strcmp(token, "CCW") == 0)
    {
        *dir = MOTOR_DIR_CCW;
        return 1U;
    }

    return 0U;
}

static uint8_t App_ParsePercent(const char *token, uint8_t *percent)
{
    uint16_t value;

    if ((token == NULL) || (percent == NULL) || (*token == '\0'))
    {
        return 0U;
    }

    value = 0U;
    while (*token != '\0')
    {
        if ((*token < '0') || (*token > '9'))
        {
            return 0U;
        }

        value = (uint16_t)((value * 10U) + (uint16_t)(*token - '0'));
        if (value > 100U)
        {
            return 0U;
        }
        token++;
    }

    *percent = (uint8_t)value;
    return 1U;
}

static void App_LogMotorUsage(void)
{
    Log_Printf("[MOTOR] Right/left/front roller: MOTOR <R|L|F> <CW|CCW> <0-100>\r\n");
    Log_Printf("[MOTOR] Top ESC PD12 50Hz: MOTOR T <0|5-10> (0=stop)\r\n");
    Log_Printf("[MOTOR] Stop all: MSTOP\r\n");
}

static uint8_t App_DebugCommand_HandleMotor(const uint8_t *packet, uint16_t size)
{
    char cmd[USB_COMMAND_BUF_SIZE + 1U];
    char *cursor;
    char *token;
    char *motor_token;
    char *dir_token;
    char *duty_token;
    MotorOutput_t motor;
    MotorDir_t dir;
    uint8_t duty_percent;

    if (App_CopyTrimmedCommand(packet, size, cmd, (uint16_t)sizeof(cmd)) == 0U)
    {
        return 0U;
    }

    cursor = cmd;
    token = App_NextToken(&cursor);
    if (token == NULL)
    {
        return 0U;
    }

    if ((strcmp(token, "MSTOP") == 0) || (strcmp(token, "MOTORSTOP") == 0))
    {
        Motor_StopAll();
        Log_Printf("[MOTOR] stopped\r\n");
        return 1U;
    }

    if ((strcmp(token, "MOTOR") != 0) && (strcmp(token, "M") != 0))
    {
        return 0U;
    }

    motor_token = App_NextToken(&cursor);
    if ((motor_token != NULL) && (strcmp(motor_token, "STOP") == 0))
    {
        Motor_StopAll();
        Log_Printf("[MOTOR] stopped\r\n");
        return 1U;
    }

    if (App_ParseMotorOutput(motor_token, &motor) == 0U)
    {
        App_LogMotorUsage();
        return 1U;
    }

    dir_token = App_NextToken(&cursor);
    duty_token = App_NextToken(&cursor);
    if (motor == MOTOR_OUTPUT_TOP)
    {
        dir = MOTOR_DIR_CW;
        if (duty_token == NULL)
        {
            duty_token = dir_token;
        }
        else if (App_ParseMotorDir(dir_token, &dir) == 0U)
        {
            App_LogMotorUsage();
            return 1U;
        }
    }
    else if (App_ParseMotorDir(dir_token, &dir) == 0U)
    {
        App_LogMotorUsage();
        return 1U;
    }

    if (App_ParsePercent(duty_token, &duty_percent) == 0U)
    {
        App_LogMotorUsage();
        return 1U;
    }

    if ((motor == MOTOR_OUTPUT_TOP) &&
        (duty_percent != 0U) &&
        ((duty_percent < 5U) || (duty_percent > 10U)))
    {
        App_LogMotorUsage();
        return 1U;
    }

    Motor_SetPercent(motor, dir, duty_percent);
    if ((motor == MOTOR_OUTPUT_TOP) && (duty_percent == 0U))
    {
        Log_Printf("[MOTOR] TOP ESC stopped (5%% pulse)\r\n");
        return 1U;
    }
    Log_Printf("[MOTOR] %s %s %u%%\r\n",
               App_MotorOutputName(motor),
               (dir == MOTOR_DIR_CCW) ? "CCW" : "CW",
               (unsigned int)duty_percent);

    return 1U;
}

static void App_DebugCommand_Input(uint8_t *packet, uint16_t size)
{
    if ((packet != NULL) && (size != 0U) && (packet[0] == '?'))
    {
        Log_Printf("[USB] Send BOOTMSC or UPDATE to enter USB disk bootloader\r\n");
        Log_Printf("[USB] Send DFU or BOOTDFU to enter STM32 ROM USB DFU bootloader\r\n");
        App_LogMotorUsage();
        Log_Printf("[SENSOR] SENSORS: show A23, VLS-H5 and JY901S status\r\n");
    }
    else if (App_DebugCommand_HandleMotor(packet, size) != 0U)
    {
        return;
    }
    else if ((App_Command_Equals(packet, size, "DFU") != 0U) ||
             (App_Command_Equals(packet, size, "BOOTDFU") != 0U))
    {
        Log_Printf("[USB] entering STM32 ROM USB DFU bootloader\r\n");
        osDelay(50U);
        __HAL_RCC_RTC_CLK_ENABLE();
        HAL_PWR_EnableBkUpAccess();
        RTC->BKP0R = DFU_REQUEST_MAGIC;
        NVIC_SystemReset();
    }
    else if ((App_Command_Equals(packet, size, "BOOTMSC") != 0U) ||
             (App_Command_Equals(packet, size, "UPDATE") != 0U))
    {
        Log_Printf("[USB] entering USB disk bootloader\r\n");
        osDelay(50U);
        App_EnterUsbDiskBootloader();
    }
    else if ((App_Command_Equals(packet, size, "SENSORS") != 0U) ||
             (App_Command_Equals(packet, size, "STATUS") != 0U))
    {
        App_LogSensorStatus();
    }
}

static void App_EnterUsbDiskBootloader(void)
{
    __HAL_RCC_RTC_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    RTC->BKP0R = BOOT_REQUEST_MAGIC;
    NVIC_SystemReset();
}

static void App_DebugCommand_ProcessBytes(const uint8_t *packet, uint16_t size)
{
    uint16_t i;
    uint8_t ch;

    if ((packet == NULL) || (size == 0U))
    {
        return;
    }

    for (i = 0U; i < size; i++)
    {
        ch = packet[i];
        if ((ch == '\r') || (ch == '\n'))
        {
            if (g_usb_command_len != 0U)
            {
                App_DebugCommand_Input(g_usb_command_buf, g_usb_command_len);
                g_usb_command_len = 0U;
            }
        }
        else if ((ch >= ' ') && (ch <= '~'))
        {
            if (g_usb_command_len < USB_COMMAND_BUF_SIZE)
            {
                g_usb_command_buf[g_usb_command_len] = ch;
                g_usb_command_len++;
            }
            else
            {
                g_usb_command_len = 0U;
            }
        }
    }
}

void UART1_rxCallback(uint8_t *packet, uint16_t size)
{
    uint32_t primask;
    //uint16_t i;

    if ((packet == NULL) || (size == 0U))
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
		/*
    g_dvl_interval_rx += size;
    g_dvl_total_rx += size;
    for (i = 0U; (i < size) && (g_dvl_log_len < DVL_LOG_BYTES_MAX); i++)
    {
        g_dvl_log_bytes[g_dvl_log_len] = packet[i];
        g_dvl_log_len++;
    }
		*/
		DvlUart_InputBytes(packet, size);
		
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void UART2_rxCallback(uint8_t *packet, uint16_t size)
{
    App_UpdateSonarUartDiag(0U, packet, size);
    if (Sonar_InputFrame(&Sonar_Front, packet, size) != 0U)
    {
        sonic_uart2 = Sonar_Front.raw_distance;
    }
}

void UART3_rxCallback(uint8_t *packet, uint16_t size)
{
    (void)packet;
    (void)size;
}

void UART4_rxCallback(uint8_t *packet, uint16_t size)
{
    App_UpdateSonarUartDiag(1U, packet, size);
    if (Sonar_InputFrame(&Sonar_Right, packet, size) != 0U)
    {
        sonic_uart4 = Sonar_Right.raw_distance;
    }
}

static void App_UpdateSonarUartDiag(uint8_t index, const uint8_t *packet, uint16_t size)
{
    uint16_t i;

    if ((index >= 2U) || (packet == NULL) || (size == 0U))
    {
        return;
    }

    g_sonar_uart_diag[index].chunk_count++;
    g_sonar_uart_diag[index].last_chunk_size = size;
    for (i = 0U; i < size; i++)
    {
        memmove(&g_sonar_uart_diag[index].last_bytes[0],
                &g_sonar_uart_diag[index].last_bytes[1],
                sizeof(g_sonar_uart_diag[index].last_bytes) - 1U);
        g_sonar_uart_diag[index].last_bytes[sizeof(g_sonar_uart_diag[index].last_bytes) - 1U] = packet[i];
    }
}

void UART5_rxCallback(uint8_t *packet, uint16_t size)
{
    (void)packet;
    (void)size;
}

void UART6_rxCallback(uint8_t *packet, uint16_t size)
{
		(void)packet;
		(void)size;
    //VlsH5_InputBytes(packet, size);
}

void UART7_rxCallback(uint8_t *packet, uint16_t size)
{
    Jy901sUart_InputBytes(packet, size);
}

void UART8_rxCallback(uint8_t *packet, uint16_t size)
{
    (void)packet;
    (void)size;
}

static void Uart_Parse_Task(void *argument)
{
    uint8_t usb_rx[APP_UART_FRAMEBUF_SIZE];
    uint16_t usb_len;
		
		//告诉编译器不使用此参数
    (void)argument;

    for (;;)
    {
				//遍历所有串口，读取传感器数据
        (void)App_UART_Process();
        usb_len = UsbCdcPort_Read(usb_rx, (uint16_t)sizeof(usb_rx));
        if (usb_len != 0U)
        {
            App_DebugCommand_ProcessBytes(usb_rx, usb_len);
        }
        osDelay(1U);
    }
}

static void Uart_Send_Task(void *argument)
{
    uint8_t tx_buf[LOG_TX_CHUNK_SIZE];
    uint16_t len = 0U;

    (void)argument;

    for (;;)
    {
        if (len == 0U)
        {
            len = Log_Ring_ReadBlock(tx_buf, (uint16_t)sizeof(tx_buf));
        }

        if (len != 0U)
        {
            if (UsbCdcPort_Send(tx_buf, len) == HAL_OK)
            {
                len = 0U;
            }
            else
            {
                osDelay(1U);
            }
        }
        else
        {
            osDelay(1U);
        }
    }
}

static void App_LogSensorStatus(void)
{
    const Jy901sUartData_t *imu;
    //const VlsH5Data_t *vls;
		DVL_Data_t dvl;
		NavigationState_t nav;
    imu = Jy901sUart_GetData();
    //vls = VlsH5_GetData();
		DvlUart_GetData(&dvl);
    //App_LogDvlStatus();
		Get_NavigationState(&nav);
		
		
		Log_Printf("[DVL] vx=%.3f vy=%.3f vz=%.3f ve=%.3f status=%c frames=%lu errors=%lu\r\n",
               dvl.vx,
               dvl.vy,
               dvl.vz,
               dvl.ve,
               dvl.status,
               (unsigned long)dvl.frame_count,
               (unsigned long)dvl.checksum_error_count);
    /*
		Log_Printf("[A23-1] filtered=%u raw=%u/%u frames=%lu checksum_errors=%lu uart_rx=%lu uart_errors=%lu\r\n",
               (unsigned int)g_sonar1_mm,
               (unsigned int)Sonar_Front.last_input_distance,
               (unsigned int)Sonar_Front.last_input_distance_ch2,
               (unsigned long)Sonar_Front.frame_count,
               (unsigned long)Sonar_Front.checksum_error_count,
               (unsigned long)App_UART_GetRxByteCount(APP_UART_2),
               (unsigned long)App_UART_GetErrorCount(APP_UART_2));
    Log_Printf("[A23-2] filtered=%u raw=%u/%u frames=%lu checksum_errors=%lu uart_rx=%lu uart_errors=%lu\r\n",
               (unsigned int)g_sonar2_mm,
               (unsigned int)Sonar_Right.last_input_distance,
               (unsigned int)Sonar_Right.last_input_distance_ch2,
               (unsigned long)Sonar_Right.frame_count,
               (unsigned long)Sonar_Right.checksum_error_count,
               (unsigned long)App_UART_GetRxByteCount(APP_UART_4),
               (unsigned long)App_UART_GetErrorCount(APP_UART_4));
    Log_Printf("[A23-UART] s1 chunks=%lu size=%u last=%02X %02X %02X %02X; s2 chunks=%lu size=%u last=%02X %02X %02X %02X\r\n",
               (unsigned long)g_sonar_uart_diag[0].chunk_count,
               (unsigned int)g_sonar_uart_diag[0].last_chunk_size,
               g_sonar_uart_diag[0].last_bytes[4],
               g_sonar_uart_diag[0].last_bytes[5],
               g_sonar_uart_diag[0].last_bytes[6],
               g_sonar_uart_diag[0].last_bytes[7],
               (unsigned long)g_sonar_uart_diag[1].chunk_count,
               (unsigned int)g_sonar_uart_diag[1].last_chunk_size,
               g_sonar_uart_diag[1].last_bytes[4],
               g_sonar_uart_diag[1].last_bytes[5],
               g_sonar_uart_diag[1].last_bytes[6],
               g_sonar_uart_diag[1].last_bytes[7]);
		*/
    Log_Printf("[JY901S] fresh=%u valid=0x%02X roll=%.2f pitch=%.2f yaw=%.2f frames=%lu errors=%lu\r\n",
               (unsigned int)Jy901sUart_IsFresh(JY901S_FRESH_TIMEOUT_MS),
               (unsigned int)imu->valid_mask,
               imu->roll_deg,
               imu->pitch_deg,
               imu->yaw_deg,
               (unsigned long)imu->frame_count,
               (unsigned long)imu->checksum_error_count);
		Log_Printf("[NAV] pos_x=%.3f pos_y=%.3f vn=%.3f ve=%.3f timestamp=%lu\r\n",
               nav.pos_x,
               nav.pos_y,
               nav.vn,
               nav.ve,
               (unsigned long)nav.timestamp);
		/*
    Log_Printf("[JY901S-UART] rx=%lu errors=%lu overflow=%u baud=%lu\r\n",
               (unsigned long)App_UART_GetRxByteCount(APP_UART_7),
               (unsigned long)App_UART_GetErrorCount(APP_UART_7),
               (unsigned int)App_UART_GetStreamOverflowCount(APP_UART_7),
               (unsigned long)App_UART_GetBaudRate(APP_UART_7));
		*/
		/*
    Log_Printf("[VLS-H5] running=%u front=%u right=%u left=%u rear=%u mm frames=%lu crc_errors=%lu ack=%lu\r\n",
               (unsigned int)vls->running,
               (unsigned int)g_vls_front_mm,
               (unsigned int)g_vls_right_mm,
               (unsigned int)g_vls_left_mm,
               (unsigned int)g_vls_rear_mm,
               (unsigned long)vls->frame_count,
               (unsigned long)vls->crc_error_count,
               (unsigned long)vls->command_ack_count);
    Log_Printf("[VLS-H5-UART] rx=%lu errors=%lu overflow=%u baud=%lu\r\n",
               (unsigned long)App_UART_GetRxByteCount(APP_UART_6),
               (unsigned long)App_UART_GetErrorCount(APP_UART_6),
               (unsigned int)App_UART_GetStreamOverflowCount(APP_UART_6),
               (unsigned long)App_UART_GetBaudRate(APP_UART_6));
    Log_Printf("[VLS-H5-RAW] header54=%lu ver_mismatch=%lu last=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
               (unsigned long)vls->header_count,
               (unsigned long)vls->ver_len_mismatch_count,
               (unsigned int)vls->last_bytes[0],
               (unsigned int)vls->last_bytes[1],
               (unsigned int)vls->last_bytes[2],
               (unsigned int)vls->last_bytes[3],
               (unsigned int)vls->last_bytes[4],
               (unsigned int)vls->last_bytes[5],
               (unsigned int)vls->last_bytes[6],
               (unsigned int)vls->last_bytes[7]);
		*/
    /*
		Log_Printf("[USB-CDC] tx_recoveries=%lu\r\n",
               (unsigned long)UsbCdcPort_GetTxRecoveryCount());
		*/
}

/*
static void App_LogDvlStatus(void)
{
    uint8_t bytes[DVL_LOG_BYTES_MAX];
    char line[192];
    uint32_t primask;
    uint32_t interval_rx;
    uint32_t total_rx;
    uint16_t len;
    uint16_t i;
    int used;
    int written;

    primask = __get_PRIMASK();
    __disable_irq();

    interval_rx = g_dvl_interval_rx;
    total_rx = g_dvl_total_rx;
    len = g_dvl_log_len;
    memcpy(bytes, g_dvl_log_bytes, len);
    g_dvl_interval_rx = 0U;
    g_dvl_log_len = 0U;

    if (primask == 0U)
    {
        __enable_irq();
    }

    used = snprintf(line, sizeof(line), "[DVL] rx=%lu total=%lu",
                    (unsigned long)interval_rx,
                    (unsigned long)total_rx);
    if ((used < 0) || (used >= (int)sizeof(line)))
    {
        return;
    }

    if (len != 0U)
    {
        written = snprintf(&line[used], sizeof(line) - (size_t)used, " data=");
        if ((written < 0) || (written >= ((int)sizeof(line) - used)))
        {
            return;
        }
        used += written;

        for (i = 0U; i < len; i++)
        {
            written = snprintf(&line[used], sizeof(line) - (size_t)used,
                               (i == 0U) ? "%02X" : " %02X",
                               (unsigned int)bytes[i]);
            if ((written < 0) || (written >= ((int)sizeof(line) - used)))
            {
                break;
            }
            used += written;
        }

        if (interval_rx > len)
        {
            written = snprintf(&line[used], sizeof(line) - (size_t)used,
                               " (+%lu)",
                               (unsigned long)(interval_rx - len));
            if ((written > 0) && (written < ((int)sizeof(line) - used)))
            {
                used += written;
            }
        }
    }

    if (used <= ((int)sizeof(line) - 3))
    {
        line[used++] = '\r';
        line[used++] = '\n';
        line[used] = '\0';
    }
    Log_Ring_WriteBlock((const uint8_t *)line, (uint16_t)used);
}
*/
static void App_RequestJy901sOutput(void)
{
    static const uint8_t unlock_cmd[5] = {0xFFU, 0xAAU, 0x69U, 0x88U, 0xB5U};
    static const uint8_t output_cmd[5] = {0xFFU, 0xAAU, 0x02U, 0x0FU, 0x02U};

    (void)App_UART_Send(APP_UART_7, unlock_cmd, (uint16_t)sizeof(unlock_cmd));
    osDelay(20U);
    (void)App_UART_Send(APP_UART_7, output_cmd, (uint16_t)sizeof(output_cmd));
}

/*
static void Sensor_Task(void *argument)
{
    uint32_t last_report_tick;
    uint32_t last_vls_probe_tick;
    uint32_t last_jy901s_probe_tick;
    uint32_t now;
    const VlsH5Data_t *vls;
    const Jy901sUartData_t *imu;
    uint8_t jy901s_baud_index;
    uint8_t vls_h5_baud_index;

    (void)argument;
    last_report_tick = osKernelGetTickCount();
    last_vls_probe_tick = 0U;
    last_jy901s_probe_tick = 0U;
    jy901s_baud_index = 0U;
    vls_h5_baud_index = 0U;

    for (;;)
    {
				
        Sonic_Trigger(2U);
        osDelay(SONAR_TRIGGER_GAP_MS);
        Sonic_Trigger(4U);
        osDelay(SONAR_RESPONSE_WAIT_MS);

        Sonar_Data_Filter(&Sonar_Front);
        Sonar_Data_Filter(&Sonar_Right);
        g_sonic_uart2 = Sonar_Get_Filter_Distanse(&Sonar_Front);
        g_sonic_uart4 = Sonar_Get_Filter_Distanse(&Sonar_Right);
        g_sonar1_mm = g_sonic_uart2;
        g_sonar2_mm = g_sonic_uart4;
        g_sonar_front_mm = g_sonar1_mm;
        g_sonar_right_mm = g_sonar2_mm;

        now = osKernelGetTickCount();
        imu = Jy901sUart_GetData();
        if ((imu->frame_count == 0U) &&
            ((last_jy901s_probe_tick == 0U) ||
             ((now - last_jy901s_probe_tick) >= JY901S_BAUD_PROBE_MS)))
        {
            if (last_jy901s_probe_tick != 0U)
            {
                jy901s_baud_index++;
                if (jy901s_baud_index >=
                    (uint8_t)(sizeof(jy901s_baud_candidates) / sizeof(jy901s_baud_candidates[0])))
                {
                    jy901s_baud_index = 0U;
                }

                (void)App_UART_SetBaudRate(APP_UART_7,
                                           jy901s_baud_candidates[jy901s_baud_index]);
                Jy901sUart_Init();
            }

            App_RequestJy901sOutput();
            last_jy901s_probe_tick = now;
        }
				
        vls = VlsH5_GetData();
        if ((vls->frame_count == 0U) &&
            ((last_vls_probe_tick == 0U) ||
             ((now - last_vls_probe_tick) >= VLS_H5_BAUD_PROBE_MS)))
        {
            if ((last_vls_probe_tick != 0U) &&
                (vls->command_ack_count == 0U))
            {
                vls_h5_baud_index++;
                if (vls_h5_baud_index >=
                    (uint8_t)(sizeof(vls_h5_baud_candidates) / sizeof(vls_h5_baud_candidates[0])))
                {
                    vls_h5_baud_index = 0U;
                }

                (void)App_UART_SetBaudRate(APP_UART_6,
                                           vls_h5_baud_candidates[vls_h5_baud_index]);
                VlsH5_Init(APP_UART_6);
            }

            if (vls->command_ack_count == 0U)
            {
                VlsH5_Start();
            }
            last_vls_probe_tick = now;
        }

        g_vls_front_mm = VlsH5_GetSectorDistanceMm(VLS_H5_SECTOR_FRONT);
        g_vls_right_mm = VlsH5_GetSectorDistanceMm(VLS_H5_SECTOR_RIGHT);
        g_vls_left_mm = VlsH5_GetSectorDistanceMm(VLS_H5_SECTOR_LEFT);
        g_vls_rear_mm = VlsH5_GetSectorDistanceMm(VLS_H5_SECTOR_REAR);
				
        if ((now - last_report_tick) >= SENSOR_REPORT_PERIOD_MS)
        {
            App_LogSensorStatus();
            last_report_tick = now;
        }

        osDelay(DVL_COLLECT_PERIOD_MS);
    }
}
*/


static void DVL_IMU_Fusion_Task(void *argument)
{
	
    uint32_t last_report_tick;
    uint32_t last_jy901s_probe_tick;
    uint32_t now;
		uint32_t last_fuse_time;
		bool first_time = true;
		Jy901sUartData_t imu = {0};
		DVL_Data_t dvl = {0};
    uint8_t jy901s_baud_index;

    (void)argument;
    last_report_tick = osKernelGetTickCount();
    last_jy901s_probe_tick = 0U;
    jy901s_baud_index = 0U;

    for (;;)
    {
        now = osKernelGetTickCount();
				DvlUart_GetData(&dvl);
				Jy901sUart_GetDataSafe(&imu);
        if ((imu.frame_count == 0U) &&
            ((last_jy901s_probe_tick == 0U) ||
             ((now - last_jy901s_probe_tick) >= JY901S_BAUD_PROBE_MS)))
        {
            if (last_jy901s_probe_tick != 0U)
            {
                jy901s_baud_index++;
                if (jy901s_baud_index >=
                    (uint8_t)(sizeof(jy901s_baud_candidates) / sizeof(jy901s_baud_candidates[0])))
                {
                    jy901s_baud_index = 0U;
                }

                (void)App_UART_SetBaudRate(APP_UART_7,
                                           jy901s_baud_candidates[jy901s_baud_index]);
                Jy901sUart_Init();
            }

            App_RequestJy901sOutput();
            last_jy901s_probe_tick = now;
        }
				
				if (first_time){
					last_fuse_time = now;
					first_time = false;
				}else if(dvl.status == 'A'){
					DVL_IMU_Fuser(last_fuse_time, now, &dvl, &imu);
				}
				
        if ((now - last_report_tick) >= SENSOR_REPORT_PERIOD_MS)
        {
            App_LogSensorStatus();
            last_report_tick = now;
        }

        osDelay(DVL_COLLECT_PERIOD_MS);
    }
}

void App_Tasks_Init(void)
{
    App_UART_SetMode(APP_UART_1, APP_UART_MODE_STREAM);
    //App_UART_SetMode(APP_UART_6, APP_UART_MODE_STREAM);
    App_UART_SetMode(APP_UART_7, APP_UART_MODE_STREAM);
    //Sonar_Filter_Init();
    //VlsH5_Init(APP_UART_6);
    Jy901sUart_Init();

    Log_Printf("[DVL] USART1 PA9(TX)/PA10(RX), 115200 8N1, hex log every 1s\r\n");
    //Log_Printf("[A23] sonar1 PA2/PA3; sonar2 PD1(TX)/PD0(RX), 115200 8N1\r\n");
    Log_Printf("[JY901S] UART7 PE8(TX)/PE7(RX), auto baud starting at 9600 8N1\r\n");
    //Log_Printf("[VLS-H5] USART6 PC6(TX)/PC7(RX), auto baud scan\r\n");
    App_LogMotorUsage();
		
    (void)osThreadNew(Uart_Parse_Task, NULL, &uart_parse_task_attributes);
    (void)osThreadNew(Uart_Send_Task, NULL, &uart_send_task_attributes);
    (void)osThreadNew(DVL_IMU_Fusion_Task, NULL, &sensor_task_attributes);
}
