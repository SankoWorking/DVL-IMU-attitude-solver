#include "sonar.h"
#include "uart_port.h"

static uint8_t dyp_a23_checksum_ok(const uint8_t *frame)
{
    uint16_t sum;
    uint8_t i;

    sum = 0U;
    for (i = 0U; i < (DYP_A23_UART_FRAME_SIZE - 1U); i++)
    {
        sum = (uint16_t)(sum + frame[i]);
    }

    return (((uint8_t)sum) == frame[DYP_A23_UART_FRAME_SIZE - 1U]) ? 1U : 0U;
}

static uint8_t sonar_single_checksum_ok(const uint8_t *frame)
{
    uint8_t sum;

    sum = (uint8_t)(frame[0] + frame[1] + frame[2]);
    return (sum == frame[SONAR_UART_FRAME_SIZE - 1U]) ? 1U : 0U;
}

void Sonic_Trigger(uint8_t uart_ch)
{
    switch (uart_ch)
    {
    case 2:
        UART2_SendByte(DYP_A23_TRIGGER_BYTE);
        break;
    case 3:
        UART3_SendByte(DYP_A23_TRIGGER_BYTE);
        break;
    case 4:
        UART4_SendByte(DYP_A23_TRIGGER_BYTE);
        break;
    case 5:
        UART5_SendByte(DYP_A23_TRIGGER_BYTE);
        break;
    case 6:
        USART6_SendByte(DYP_A23_TRIGGER_BYTE);
        break;
    default:
        break;
    }
}

uint8_t Sonar_InputFrame(Sonar_t *sonar, const uint8_t *packet, uint16_t size)
{
    uint16_t i;
    uint16_t ch1_distance;
    uint16_t ch2_distance;
    uint8_t parsed;

    if ((sonar == NULL) || (packet == NULL) || (size < SONAR_UART_FRAME_SIZE))
    {
        return 0U;
    }

    parsed = 0U;
    for (i = 0U; i <= (uint16_t)(size - SONAR_UART_FRAME_SIZE); i++)
    {
        if (packet[i] != DYP_A23_UART_FRAME_HEADER)
        {
            continue;
        }

        if (((i + DYP_A23_UART_FRAME_SIZE) <= size) &&
            (dyp_a23_checksum_ok(&packet[i]) != 0U))
        {
            ch1_distance = (uint16_t)(((uint16_t)packet[i + 1U] << 8) | packet[i + 2U]);
            ch2_distance = (uint16_t)(((uint16_t)packet[i + 3U] << 8) | packet[i + 4U]);
            Sonar_Update_DistancePair(sonar, ch1_distance, ch2_distance);
            parsed = 1U;
            i = (uint16_t)(i + DYP_A23_UART_FRAME_SIZE - 1U);
            continue;
        }

        if (sonar_single_checksum_ok(&packet[i]) != 0U)
        {
            ch1_distance = (uint16_t)(((uint16_t)packet[i + 1U] << 8) | packet[i + 2U]);
            Sonar_Update_DistancePair(sonar, ch1_distance, SONAR_INVALID_DISTANCE);
            parsed = 1U;
            i = (uint16_t)(i + SONAR_UART_FRAME_SIZE - 1U);
            continue;
        }

        sonar->checksum_error_count++;
    }

    return parsed;
}
