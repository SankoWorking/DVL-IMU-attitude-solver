#include "jy901s_uart.h"

#include <string.h>
#include "cmsis_os2.h"

#define JY901S_UART_HEADER           0x55U
#define JY901S_UART_TYPE_ACCEL       0x51U
#define JY901S_UART_TYPE_GYRO        0x52U
#define JY901S_UART_TYPE_ANGLE       0x53U
#define JY901S_UART_TYPE_MAG         0x54U

static Jy901sUartData_t g_jy901s_uart_data;
static uint8_t g_jy901s_uart_frame[JY901S_UART_FRAME_SIZE];
static uint8_t g_jy901s_uart_index;

static int16_t Jy901sUart_ReadI16(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[1] << 8) | data[0]);
}

static uint8_t Jy901sUart_ChecksumOk(const uint8_t *frame)
{
    uint8_t checksum = 0U;
    uint8_t i;

    for (i = 0U; i < (JY901S_UART_FRAME_SIZE - 1U); i++)
    {
        checksum = (uint8_t)(checksum + frame[i]);
    }

    return (checksum == frame[JY901S_UART_FRAME_SIZE - 1U]) ? 1U : 0U;
}

static void Jy901sUart_ParseFrame(const uint8_t *frame)
{
    int16_t value0;
    int16_t value1;
    int16_t value2;
    int16_t value3;

    if (Jy901sUart_ChecksumOk(frame) == 0U)
    {
        g_jy901s_uart_data.checksum_error_count++;
        return;
    }

    value0 = Jy901sUart_ReadI16(&frame[2]);
    value1 = Jy901sUart_ReadI16(&frame[4]);
    value2 = Jy901sUart_ReadI16(&frame[6]);
    value3 = Jy901sUart_ReadI16(&frame[8]);

    switch (frame[1])
    {
    case JY901S_UART_TYPE_ACCEL:
        g_jy901s_uart_data.accel_raw[0] = value0;
        g_jy901s_uart_data.accel_raw[1] = value1;
        g_jy901s_uart_data.accel_raw[2] = value2;
        g_jy901s_uart_data.temperature_raw = value3;
        g_jy901s_uart_data.accel_g[0] = ((float)value0 * 16.0f) / 32768.0f;
        g_jy901s_uart_data.accel_g[1] = ((float)value1 * 16.0f) / 32768.0f;
        g_jy901s_uart_data.accel_g[2] = ((float)value2 * 16.0f) / 32768.0f;
        g_jy901s_uart_data.temperature_c = (float)value3 / 100.0f;
        g_jy901s_uart_data.valid_mask |= JY901S_UART_VALID_ACCEL;
        break;

    case JY901S_UART_TYPE_GYRO:
        g_jy901s_uart_data.gyro_raw[0] = value0;
        g_jy901s_uart_data.gyro_raw[1] = value1;
        g_jy901s_uart_data.gyro_raw[2] = value2;
        g_jy901s_uart_data.temperature_raw = value3;
        g_jy901s_uart_data.gyro_dps[0] = ((float)value0 * 2000.0f) / 32768.0f;
        g_jy901s_uart_data.gyro_dps[1] = ((float)value1 * 2000.0f) / 32768.0f;
        g_jy901s_uart_data.gyro_dps[2] = ((float)value2 * 2000.0f) / 32768.0f;
        g_jy901s_uart_data.temperature_c = (float)value3 / 100.0f;
        g_jy901s_uart_data.valid_mask |= JY901S_UART_VALID_GYRO;
        break;

    case JY901S_UART_TYPE_ANGLE:
        g_jy901s_uart_data.angle_raw[0] = value0;
        g_jy901s_uart_data.angle_raw[1] = value1;
        g_jy901s_uart_data.angle_raw[2] = value2;
        g_jy901s_uart_data.roll_deg = ((float)value0 * 180.0f) / 32768.0f;
        g_jy901s_uart_data.pitch_deg = ((float)value1 * 180.0f) / 32768.0f;
        g_jy901s_uart_data.yaw_deg = ((float)value2 * 180.0f) / 32768.0f;
        g_jy901s_uart_data.valid_mask |= JY901S_UART_VALID_ANGLE;
        break;

    case JY901S_UART_TYPE_MAG:
        g_jy901s_uart_data.mag_raw[0] = value0;
        g_jy901s_uart_data.mag_raw[1] = value1;
        g_jy901s_uart_data.mag_raw[2] = value2;
        g_jy901s_uart_data.valid_mask |= JY901S_UART_VALID_MAG;
        break;

    default:
        return;
    }

    g_jy901s_uart_data.frame_count++;
    g_jy901s_uart_data.last_update_tick = HAL_GetTick();
}

void Jy901sUart_Init(void)
{
    memset(&g_jy901s_uart_data, 0, sizeof(g_jy901s_uart_data));
    memset(g_jy901s_uart_frame, 0, sizeof(g_jy901s_uart_frame));
    g_jy901s_uart_index = 0U;
}

void Jy901sUart_InputBytes(const uint8_t *data, uint16_t len)
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
        if (g_jy901s_uart_index == 0U)
        {
            if (byte == JY901S_UART_HEADER)
            {
                g_jy901s_uart_frame[g_jy901s_uart_index++] = byte;
            }
            continue;
        }

        g_jy901s_uart_frame[g_jy901s_uart_index++] = byte;
        if (g_jy901s_uart_index >= JY901S_UART_FRAME_SIZE)
        {
            Jy901sUart_ParseFrame(g_jy901s_uart_frame);
            g_jy901s_uart_index = 0U;
        }
    }
}

const Jy901sUartData_t *Jy901sUart_GetData(void)
{
    return &g_jy901s_uart_data;
}

void Jy901sUart_GetDataSafe(Jy901sUartData_t *out_data)
{
		osKernelLock();
    if (out_data == NULL) return;
    *out_data = g_jy901s_uart_data;
		osKernelUnlock();
}

uint8_t Jy901sUart_IsFresh(uint32_t timeout_ms)
{
    if (g_jy901s_uart_data.last_update_tick == 0U)
    {
        return 0U;
    }

    return ((HAL_GetTick() - g_jy901s_uart_data.last_update_tick) <= timeout_ms) ? 1U : 0U;
}
