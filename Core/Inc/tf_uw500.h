#ifndef TF_UW500_H
#define TF_UW500_H

#include "uart_port.h"
#include <stdbool.h>
#include <stdint.h>

#define TF_UW500_INVALID_DISTANCE  65535U

typedef enum
{
    TF_UW500_DISTANCE_UNIT_CM = 0,
    TF_UW500_DISTANCE_UNIT_MM
} TfUw500DistanceUnit_t;

typedef struct
{
    uint16_t raw_distance;
    uint16_t distance_cm;
    uint16_t distance_mm;
    uint16_t peak;
    int16_t temperature_centi_c;
    uint8_t confidence;
    TfUw500DistanceUnit_t output_unit;
    uint8_t valid;
    uint32_t update_count;
    uint32_t format_ack_count;
    HAL_StatusTypeDef last_status;
} TfUw500Data_t;

void TfUw500_Init(void);
HAL_StatusTypeDef TfUw500_RequestOutputFormat(AppUartPort port, TfUw500DistanceUnit_t unit);
void TfUw500_SetDistanceUnit(TfUw500DistanceUnit_t unit);
void TfUw500_InputBytes(const uint8_t *data, uint16_t len);
HAL_StatusTypeDef TfUw500_Read(TfUw500Data_t *out_data);
const TfUw500Data_t *TfUw500_GetData(void);
uint16_t TfUw500_GetDistanceMm(void);
bool TfUw500_IsDistanceValid(uint16_t distance_mm);

#endif
