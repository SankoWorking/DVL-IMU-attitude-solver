#include "vls_h5_lidar.h"

#include <string.h>

#define VLS_H5_HEADER              0x54U
#define VLS_H5_VER_LEN            0x2CU
#define VLS_H5_ANGLE_MAX_CDEG     36000U
#define VLS_H5_FRONT_MIN_CDEG     34500U
#define VLS_H5_FRONT_MAX_CDEG     1500U
#define VLS_H5_RIGHT_MIN_CDEG     7500U
#define VLS_H5_RIGHT_MAX_CDEG     10500U
#define VLS_H5_REAR_MIN_CDEG      16500U
#define VLS_H5_REAR_MAX_CDEG      19500U
#define VLS_H5_LEFT_MIN_CDEG      25500U
#define VLS_H5_LEFT_MAX_CDEG      28500U
#define VLS_H5_SECTOR_STALE_MS    500U
#define VLS_H5_SECTOR_WINDOW_MS   200U
#define VLS_H5_ACK_FIRST_BYTE     0x4FU
#define VLS_H5_ACK_SECOND_BYTE    0x4BU

#ifndef VLS_H5_DEFAULT_ANGLE_OFFSET_CDEG
#define VLS_H5_DEFAULT_ANGLE_OFFSET_CDEG 0
#endif

static VlsH5Data_t g_vls_h5_data;
static AppUartPort g_vls_h5_port = APP_UART_3;
static uint8_t g_vls_h5_frame[VLS_H5_FRAME_SIZE];
static uint8_t g_vls_h5_frame_index = 0U;
static uint8_t g_vls_h5_ack_index = 0U;
static uint32_t g_vls_h5_sector_tick[VLS_H5_SECTOR_COUNT];
static uint32_t g_vls_h5_sector_window_tick[VLS_H5_SECTOR_COUNT];
static int32_t g_vls_h5_angle_offset_cdeg = VLS_H5_DEFAULT_ANGLE_OFFSET_CDEG;

typedef enum
{
    VLS_H5_COMMAND_NONE = 0,
    VLS_H5_COMMAND_START,
    VLS_H5_COMMAND_STOP
} VlsH5Command_t;

static VlsH5Command_t g_vls_h5_pending_command = VLS_H5_COMMAND_NONE;

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[1] << 8) | data[0]);
}

static uint8_t vls_h5_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc;
    uint8_t i;
    uint8_t bit;

    crc = 0U;
    for (i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x80U) != 0U)
            {
                crc = (uint8_t)((crc << 1) ^ 0x4DU);
            }
            else
            {
                crc = (uint8_t)(crc << 1);
            }
        }
    }

    return crc;
}

static uint16_t normalize_cdeg(int32_t angle_cdeg)
{
    while (angle_cdeg < 0)
    {
        angle_cdeg += (int32_t)VLS_H5_ANGLE_MAX_CDEG;
    }
    while (angle_cdeg >= (int32_t)VLS_H5_ANGLE_MAX_CDEG)
    {
        angle_cdeg -= (int32_t)VLS_H5_ANGLE_MAX_CDEG;
    }

    return (uint16_t)angle_cdeg;
}

static bool angle_in_range(uint16_t angle_cdeg, uint16_t min_cdeg, uint16_t max_cdeg)
{
    if (min_cdeg <= max_cdeg)
    {
        return ((angle_cdeg >= min_cdeg) && (angle_cdeg <= max_cdeg));
    }

    return ((angle_cdeg >= min_cdeg) || (angle_cdeg <= max_cdeg));
}

static bool sector_from_angle(uint16_t angle_cdeg, VlsH5Sector_t *sector)
{
    if (sector == NULL)
    {
        return false;
    }

    if (angle_in_range(angle_cdeg, VLS_H5_FRONT_MIN_CDEG, VLS_H5_FRONT_MAX_CDEG))
    {
        *sector = VLS_H5_SECTOR_FRONT;
        return true;
    }

    if (angle_in_range(angle_cdeg, VLS_H5_RIGHT_MIN_CDEG, VLS_H5_RIGHT_MAX_CDEG))
    {
        *sector = VLS_H5_SECTOR_RIGHT;
        return true;
    }

    if (angle_in_range(angle_cdeg, VLS_H5_LEFT_MIN_CDEG, VLS_H5_LEFT_MAX_CDEG))
    {
        *sector = VLS_H5_SECTOR_LEFT;
        return true;
    }

    if (angle_in_range(angle_cdeg, VLS_H5_REAR_MIN_CDEG, VLS_H5_REAR_MAX_CDEG))
    {
        *sector = VLS_H5_SECTOR_REAR;
        return true;
    }

    return false;
}

static void expire_sector_if_stale(VlsH5Sector_t sector, uint32_t now)
{
    if (sector >= VLS_H5_SECTOR_COUNT)
    {
        return;
    }

    if ((g_vls_h5_sector_tick[sector] != 0U) &&
        ((now - g_vls_h5_sector_tick[sector]) > VLS_H5_SECTOR_STALE_MS))
    {
        g_vls_h5_data.sector_distance_mm[sector] = VLS_H5_INVALID_DISTANCE_MM;
        g_vls_h5_sector_tick[sector] = 0U;
        g_vls_h5_sector_window_tick[sector] = 0U;
    }
}

static void reset_sector_window_if_due(VlsH5Sector_t sector, uint32_t now)
{
    if (sector >= VLS_H5_SECTOR_COUNT)
    {
        return;
    }

    if ((g_vls_h5_sector_window_tick[sector] == 0U) ||
        ((now - g_vls_h5_sector_window_tick[sector]) >= VLS_H5_SECTOR_WINDOW_MS))
    {
        g_vls_h5_data.sector_distance_mm[sector] = VLS_H5_INVALID_DISTANCE_MM;
        g_vls_h5_sector_window_tick[sector] = now;
    }
}

static void update_sector_distance(uint16_t angle_cdeg, uint16_t distance_mm)
{
    VlsH5Sector_t sector;
    uint32_t now;

    if (!VlsH5_IsDistanceValid(distance_mm))
    {
        return;
    }

    if (!sector_from_angle(angle_cdeg, &sector))
    {
        return;
    }

    now = HAL_GetTick();
    expire_sector_if_stale(sector, now);
    reset_sector_window_if_due(sector, now);

    if ((g_vls_h5_data.sector_distance_mm[sector] == VLS_H5_INVALID_DISTANCE_MM) ||
        (distance_mm < g_vls_h5_data.sector_distance_mm[sector]))
    {
        g_vls_h5_data.sector_distance_mm[sector] = distance_mm;
    }

    g_vls_h5_sector_tick[sector] = now;
}

static void parse_command_ack_byte(uint8_t byte)
{
    if (g_vls_h5_pending_command == VLS_H5_COMMAND_NONE)
    {
        return;
    }

    if (g_vls_h5_ack_index == 0U)
    {
        g_vls_h5_ack_index = (byte == VLS_H5_ACK_FIRST_BYTE) ? 1U : 0U;
        return;
    }

    if (byte == VLS_H5_ACK_SECOND_BYTE)
    {
        g_vls_h5_data.command_ack_count++;
        g_vls_h5_data.running = (g_vls_h5_pending_command == VLS_H5_COMMAND_START) ? 1U : 0U;
        g_vls_h5_pending_command = VLS_H5_COMMAND_NONE;
    }

    g_vls_h5_ack_index = (byte == VLS_H5_ACK_FIRST_BYTE) ? 1U : 0U;
}

static void parse_frame(const uint8_t *frame)
{
    uint16_t start_angle;
    uint16_t end_angle;
    uint16_t angle_span;
    uint16_t angle;
    uint16_t distance;
    uint8_t crc;
    uint8_t i;
    uint8_t point_offset;
    uint32_t point_angle;

    if ((frame[0] != VLS_H5_HEADER) || (frame[1] != VLS_H5_VER_LEN))
    {
        return;
    }

    crc = vls_h5_crc8(frame, VLS_H5_FRAME_SIZE - 1U);
    g_vls_h5_data.crc_ok = (crc == frame[VLS_H5_FRAME_SIZE - 1U]) ? 1U : 0U;
    if (g_vls_h5_data.crc_ok == 0U)
    {
        g_vls_h5_data.crc_error_count++;
        return;
    }

    g_vls_h5_data.speed_dps = read_u16_le(&frame[2]);
    start_angle = read_u16_le(&frame[4]);
    end_angle = read_u16_le(&frame[42]);
    g_vls_h5_data.start_angle_cdeg = start_angle;
    g_vls_h5_data.end_angle_cdeg = end_angle;
    g_vls_h5_data.timestamp_ms = read_u16_le(&frame[44]);

    if (end_angle >= start_angle)
    {
        angle_span = (uint16_t)(end_angle - start_angle);
    }
    else
    {
        angle_span = (uint16_t)(VLS_H5_ANGLE_MAX_CDEG - start_angle + end_angle);
    }

    for (i = 0U; i < VLS_H5_POINT_COUNT; i++)
    {
        point_offset = (uint8_t)(6U + (3U * i));
        distance = read_u16_le(&frame[point_offset]);
        point_angle = (uint32_t)start_angle +
                      (((uint32_t)angle_span * (uint32_t)i) / (VLS_H5_POINT_COUNT - 1U));
        angle = normalize_cdeg((int32_t)point_angle + (int32_t)g_vls_h5_angle_offset_cdeg);

        g_vls_h5_data.points[i].angle_cdeg = angle;
        g_vls_h5_data.points[i].distance_mm = distance;
        g_vls_h5_data.points[i].intensity = frame[point_offset + 2U];
        update_sector_distance(angle, distance);
    }

    g_vls_h5_data.frame_count++;
    g_vls_h5_data.running = 1U;
}

void VlsH5_Init(AppUartPort port)
{
    uint8_t i;

    memset(&g_vls_h5_data, 0, sizeof(g_vls_h5_data));
    for (i = 0U; i < (uint8_t)VLS_H5_SECTOR_COUNT; i++)
    {
        g_vls_h5_data.sector_distance_mm[i] = VLS_H5_INVALID_DISTANCE_MM;
        g_vls_h5_sector_tick[i] = 0U;
        g_vls_h5_sector_window_tick[i] = 0U;
    }
    g_vls_h5_frame_index = 0U;
    g_vls_h5_ack_index = 0U;
    g_vls_h5_pending_command = VLS_H5_COMMAND_NONE;
    g_vls_h5_port = port;
    g_vls_h5_angle_offset_cdeg = VLS_H5_DEFAULT_ANGLE_OFFSET_CDEG;
}

void VlsH5_SetAngleOffsetCdeg(int16_t offset_cdeg)
{
    g_vls_h5_angle_offset_cdeg = (int32_t)normalize_cdeg(offset_cdeg);
}

void VlsH5_Start(void)
{
    static const uint8_t start_cmd[5] = {0xA5U, 0x5AU, 0x01U, 0x00U, 0x00U};

    g_vls_h5_ack_index = 0U;
    g_vls_h5_pending_command = VLS_H5_COMMAND_START;
    (void)App_UART_Send(g_vls_h5_port, start_cmd, sizeof(start_cmd));
}

void VlsH5_Stop(void)
{
    static const uint8_t stop_cmd[5] = {0xA5U, 0x5AU, 0x03U, 0x00U, 0x02U};

    g_vls_h5_ack_index = 0U;
    g_vls_h5_pending_command = VLS_H5_COMMAND_STOP;
    (void)App_UART_Send(g_vls_h5_port, stop_cmd, sizeof(stop_cmd));
}

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
        memmove(&g_vls_h5_data.last_bytes[0],
                &g_vls_h5_data.last_bytes[1],
                sizeof(g_vls_h5_data.last_bytes) - 1U);
        g_vls_h5_data.last_bytes[sizeof(g_vls_h5_data.last_bytes) - 1U] = byte;
        g_vls_h5_data.rx_byte_count++;
        parse_command_ack_byte(byte);

        if (g_vls_h5_frame_index == 0U)
        {
            if (byte != VLS_H5_HEADER)
            {
                continue;
            }
            g_vls_h5_data.header_count++;
            g_vls_h5_frame[g_vls_h5_frame_index++] = byte;
        }
        else if (g_vls_h5_frame_index == 1U)
        {
            if (byte != VLS_H5_VER_LEN)
            {
                g_vls_h5_data.ver_len_mismatch_count++;
                g_vls_h5_frame_index = 0U;
                if (byte == VLS_H5_HEADER)
                {
                    g_vls_h5_frame[g_vls_h5_frame_index++] = byte;
                }
                continue;
            }
            g_vls_h5_frame[g_vls_h5_frame_index++] = byte;
        }
        else
        {
            g_vls_h5_frame[g_vls_h5_frame_index++] = byte;
            if (g_vls_h5_frame_index >= VLS_H5_FRAME_SIZE)
            {
                parse_frame(g_vls_h5_frame);
                g_vls_h5_frame_index = 0U;
            }
        }
    }
}

const VlsH5Data_t *VlsH5_GetData(void)
{
    return &g_vls_h5_data;
}

uint16_t VlsH5_GetSectorDistanceMm(VlsH5Sector_t sector)
{
    uint32_t now;

    if (sector >= VLS_H5_SECTOR_COUNT)
    {
        return VLS_H5_INVALID_DISTANCE_MM;
    }

    now = HAL_GetTick();
    expire_sector_if_stale(sector, now);

    return g_vls_h5_data.sector_distance_mm[sector];
}

bool VlsH5_IsDistanceValid(uint16_t distance_mm)
{
    /*
     * Keep sub-100 mm returns conservative for obstacle handling; only reject
     * zero and data beyond the documented 5 m measurement range.
     */
    return ((distance_mm != 0U) &&
            (distance_mm <= VLS_H5_MAX_DISTANCE_MM) &&
            (distance_mm != VLS_H5_INVALID_DISTANCE_MM));
}
