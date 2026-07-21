#include "tf_uw500.h"

#include <string.h>

#define TF_UW500_FRAME_HEADER        0x59U
#define TF_UW500_FRAME_SIZE          9U
#define TF_UW500_CMD_HEADER          0x5AU
#define TF_UW500_CMD_MAX_SIZE        16U
#define TF_UW500_ID_OUTPUT_FORMAT    0x05U
#define TF_UW500_FORMAT_CM           0x01U
#define TF_UW500_FORMAT_MM           0x06U
#define TF_UW500_STALE_TIMEOUT_MS    300U

static TfUw500Data_t g_tf_uw500_data = {0};
static uint8_t g_tf_uw500_frame[TF_UW500_FRAME_SIZE];
static uint8_t g_tf_uw500_frame_index = 0U;
static uint8_t g_tf_uw500_cmd_frame[TF_UW500_CMD_MAX_SIZE];
static uint8_t g_tf_uw500_cmd_index = 0U;
static uint8_t g_tf_uw500_cmd_len = 0U;
static TfUw500DistanceUnit_t g_tf_uw500_distance_unit = TF_UW500_DISTANCE_UNIT_CM;
static uint32_t g_tf_uw500_last_tick = 0U;

static uint8_t tf_uw500_checksum_ok(const uint8_t *frame)
{
    uint16_t sum;
    uint8_t i;

    sum = 0U;
    for (i = 0U; i < (TF_UW500_FRAME_SIZE - 1U); i++)
    {
        sum = (uint16_t)(sum + frame[i]);
    }

    return (((uint8_t)sum) == frame[TF_UW500_FRAME_SIZE - 1U]) ? 1U : 0U;
}

static void tf_uw500_mark_invalid(HAL_StatusTypeDef status)
{
    g_tf_uw500_data.distance_cm = TF_UW500_INVALID_DISTANCE;
    g_tf_uw500_data.distance_mm = TF_UW500_INVALID_DISTANCE;
    g_tf_uw500_data.output_unit = g_tf_uw500_distance_unit;
    g_tf_uw500_data.valid = 0U;
    g_tf_uw500_data.last_status = status;
}

static void tf_uw500_parse_frame(const uint8_t *frame)
{
    uint16_t raw_distance;
    uint32_t scaled_distance;

    if ((frame[0] != TF_UW500_FRAME_HEADER) ||
        (frame[1] != TF_UW500_FRAME_HEADER) ||
        (tf_uw500_checksum_ok(frame) == 0U))
    {
        g_tf_uw500_data.last_status = HAL_ERROR;
        return;
    }

    raw_distance = (uint16_t)(((uint16_t)frame[3] << 8) | frame[2]);

    g_tf_uw500_data.raw_distance = raw_distance;
    g_tf_uw500_data.output_unit = g_tf_uw500_distance_unit;

    if (raw_distance == TF_UW500_INVALID_DISTANCE)
    {
        g_tf_uw500_data.distance_cm = TF_UW500_INVALID_DISTANCE;
        g_tf_uw500_data.distance_mm = TF_UW500_INVALID_DISTANCE;
    }
    else if (g_tf_uw500_distance_unit == TF_UW500_DISTANCE_UNIT_MM)
    {
        g_tf_uw500_data.distance_mm = raw_distance;
        g_tf_uw500_data.distance_cm = (uint16_t)(raw_distance / 10U);
    }
    else
    {
        scaled_distance = (uint32_t)raw_distance * 10U;
        g_tf_uw500_data.distance_cm = raw_distance;
        g_tf_uw500_data.distance_mm = (scaled_distance > TF_UW500_INVALID_DISTANCE) ?
                                      TF_UW500_INVALID_DISTANCE : (uint16_t)scaled_distance;
    }

    g_tf_uw500_data.peak = (uint16_t)(((uint16_t)frame[5] << 8) | frame[4]);
    g_tf_uw500_data.temperature_centi_c = (int16_t)((int8_t)frame[6]) * 100;
    g_tf_uw500_data.confidence = frame[7];
    g_tf_uw500_data.valid = TfUw500_IsDistanceValid(g_tf_uw500_data.distance_mm) ? 1U : 0U;
    g_tf_uw500_data.update_count++;
    g_tf_uw500_data.last_status = HAL_OK;
    g_tf_uw500_last_tick = HAL_GetTick();
}

static uint8_t tf_uw500_cmd_checksum_ok(const uint8_t *frame, uint8_t len)
{
    uint16_t sum;
    uint8_t i;

    sum = 0U;
    for (i = 0U; i < (uint8_t)(len - 1U); i++)
    {
        sum = (uint16_t)(sum + frame[i]);
    }

    return (((uint8_t)sum) == frame[len - 1U]) ? 1U : 0U;
}

static void tf_uw500_parse_cmd_frame(const uint8_t *frame, uint8_t len)
{
    if ((len < 4U) || (frame[0] != TF_UW500_CMD_HEADER) ||
        (tf_uw500_cmd_checksum_ok(frame, len) == 0U))
    {
        return;
    }

    if ((frame[2] == TF_UW500_ID_OUTPUT_FORMAT) && (len >= 5U))
    {
        if (frame[3] == TF_UW500_FORMAT_MM)
        {
            TfUw500_SetDistanceUnit(TF_UW500_DISTANCE_UNIT_MM);
            g_tf_uw500_data.format_ack_count++;
        }
        else if (frame[3] == TF_UW500_FORMAT_CM)
        {
            TfUw500_SetDistanceUnit(TF_UW500_DISTANCE_UNIT_CM);
            g_tf_uw500_data.format_ack_count++;
        }
    }
}

static void tf_uw500_input_cmd_byte(uint8_t byte)
{
    if (g_tf_uw500_cmd_index == 0U)
    {
        if (byte == TF_UW500_CMD_HEADER)
        {
            g_tf_uw500_cmd_frame[0] = byte;
            g_tf_uw500_cmd_index = 1U;
            g_tf_uw500_cmd_len = 0U;
        }
        return;
    }

    if (g_tf_uw500_cmd_index == 1U)
    {
        if ((byte < 4U) || (byte > TF_UW500_CMD_MAX_SIZE))
        {
            g_tf_uw500_cmd_index = 0U;
            g_tf_uw500_cmd_len = 0U;
            return;
        }

        g_tf_uw500_cmd_frame[1] = byte;
        g_tf_uw500_cmd_len = byte;
        g_tf_uw500_cmd_index = 2U;
        return;
    }

    g_tf_uw500_cmd_frame[g_tf_uw500_cmd_index] = byte;
    g_tf_uw500_cmd_index++;
    if (g_tf_uw500_cmd_index >= g_tf_uw500_cmd_len)
    {
        tf_uw500_parse_cmd_frame(g_tf_uw500_cmd_frame, g_tf_uw500_cmd_len);
        g_tf_uw500_cmd_index = 0U;
        g_tf_uw500_cmd_len = 0U;
    }
}

void TfUw500_Init(void)
{
    memset(&g_tf_uw500_data, 0, sizeof(g_tf_uw500_data));
    memset(g_tf_uw500_frame, 0, sizeof(g_tf_uw500_frame));
    memset(g_tf_uw500_cmd_frame, 0, sizeof(g_tf_uw500_cmd_frame));
    g_tf_uw500_frame_index = 0U;
    g_tf_uw500_cmd_index = 0U;
    g_tf_uw500_cmd_len = 0U;
    g_tf_uw500_distance_unit = TF_UW500_DISTANCE_UNIT_CM;
    g_tf_uw500_last_tick = 0U;
    tf_uw500_mark_invalid(HAL_TIMEOUT);
}

HAL_StatusTypeDef TfUw500_RequestOutputFormat(AppUartPort port, TfUw500DistanceUnit_t unit)
{
    uint8_t cmd[5];

    cmd[0] = TF_UW500_CMD_HEADER;
    cmd[1] = 0x05U;
    cmd[2] = TF_UW500_ID_OUTPUT_FORMAT;
    cmd[3] = (unit == TF_UW500_DISTANCE_UNIT_MM) ? TF_UW500_FORMAT_MM : TF_UW500_FORMAT_CM;
    cmd[4] = (uint8_t)(cmd[0] + cmd[1] + cmd[2] + cmd[3]);

    return App_UART_Send(port, cmd, sizeof(cmd));
}

void TfUw500_SetDistanceUnit(TfUw500DistanceUnit_t unit)
{
    g_tf_uw500_distance_unit = unit;
    g_tf_uw500_data.output_unit = unit;
}

void TfUw500_InputBytes(const uint8_t *data, uint16_t len)
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
        tf_uw500_input_cmd_byte(byte);

        if (g_tf_uw500_frame_index == 0U)
        {
            if (byte == TF_UW500_FRAME_HEADER)
            {
                g_tf_uw500_frame[0] = byte;
                g_tf_uw500_frame_index = 1U;
            }
            continue;
        }

        if (g_tf_uw500_frame_index == 1U)
        {
            if (byte == TF_UW500_FRAME_HEADER)
            {
                g_tf_uw500_frame[1] = byte;
                g_tf_uw500_frame_index = 2U;
            }
            else
            {
                g_tf_uw500_frame_index = 0U;
            }
            continue;
        }

        g_tf_uw500_frame[g_tf_uw500_frame_index] = byte;
        g_tf_uw500_frame_index++;
        if (g_tf_uw500_frame_index >= TF_UW500_FRAME_SIZE)
        {
            tf_uw500_parse_frame(g_tf_uw500_frame);
            g_tf_uw500_frame_index = 0U;
        }
    }
}

HAL_StatusTypeDef TfUw500_Read(TfUw500Data_t *out_data)
{
    if ((g_tf_uw500_last_tick == 0U) ||
        ((HAL_GetTick() - g_tf_uw500_last_tick) > TF_UW500_STALE_TIMEOUT_MS))
    {
        tf_uw500_mark_invalid(HAL_TIMEOUT);
    }

    if (out_data != NULL)
    {
        *out_data = g_tf_uw500_data;
    }

    return g_tf_uw500_data.last_status;
}

const TfUw500Data_t *TfUw500_GetData(void)
{
    return &g_tf_uw500_data;
}

uint16_t TfUw500_GetDistanceMm(void)
{
    return g_tf_uw500_data.distance_mm;
}

bool TfUw500_IsDistanceValid(uint16_t distance_mm)
{
    return ((distance_mm != 0U) && (distance_mm != TF_UW500_INVALID_DISTANCE));
}
