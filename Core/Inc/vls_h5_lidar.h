#ifndef VLS_H5_LIDAR_H
#define VLS_H5_LIDAR_H

#include "uart_port.h"
#include <stdbool.h>
#include <stdint.h>

#define VLS_H5_POINT_COUNT          12U
#define VLS_H5_FRAME_SIZE           47U
#define VLS_H5_MAX_DISTANCE_MM      5000U
#define VLS_H5_INVALID_DISTANCE_MM  65535U

typedef enum
{
    VLS_H5_SECTOR_FRONT = 0,
    VLS_H5_SECTOR_RIGHT,
    VLS_H5_SECTOR_LEFT,
    VLS_H5_SECTOR_REAR,
    VLS_H5_SECTOR_COUNT
} VlsH5Sector_t;

typedef struct
{
    uint16_t angle_cdeg;
    uint16_t distance_mm;
    uint8_t intensity;
} VlsH5Point_t;

typedef struct
{
    uint16_t speed_dps;
    uint16_t start_angle_cdeg;
    uint16_t end_angle_cdeg;
    uint16_t timestamp_ms;
    uint8_t crc_ok;
    uint32_t frame_count;
    uint32_t crc_error_count;
    uint32_t command_ack_count;
    uint32_t rx_byte_count;
    uint32_t header_count;
    uint32_t ver_len_mismatch_count;
    uint8_t running;
    uint8_t last_bytes[8];
    VlsH5Point_t points[VLS_H5_POINT_COUNT];
    uint16_t sector_distance_mm[VLS_H5_SECTOR_COUNT];
} VlsH5Data_t;

void VlsH5_Init(AppUartPort port);
void VlsH5_SetAngleOffsetCdeg(int16_t offset_cdeg);
void VlsH5_Start(void);
void VlsH5_Stop(void);
void VlsH5_InputBytes(const uint8_t *data, uint16_t len);
const VlsH5Data_t *VlsH5_GetData(void);
uint16_t VlsH5_GetSectorDistanceMm(VlsH5Sector_t sector);
bool VlsH5_IsDistanceValid(uint16_t distance_mm);

#endif
