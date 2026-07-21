#ifndef SONAR_FILTER_H
#define SONAR_FILTER_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define FILTER_WINDOW_SIZE        5U
#define SONAR_MIN_VALID_DISTANCE  40U
#define SONAR_MAX_VALID_DISTANCE  5000U
#define SONAR_INVALID_DISTANCE    65533U
#define SONAR_STALE_TIMEOUT_MS    500U
#define SONAR_TURN_DISTANSE       1000U
#define LOW_PASS_ALPHA            0.2f

typedef struct
{
    volatile uint16_t raw_distance;
    volatile uint16_t raw_distance_ch2;
    volatile uint16_t last_input_distance;
    volatile uint16_t last_input_distance_ch2;
    uint16_t filter_buffer[FILTER_WINDOW_SIZE];
    uint16_t filter_distanse;
    volatile bool new_data_ready;
    uint8_t filter_index;
    uint8_t sample_count;
    float last_lowpass_val;
    uint32_t last_update_tick;
    uint32_t frame_count;
    uint32_t checksum_error_count;
} Sonar_t;

extern Sonar_t Sonar_Front;
extern Sonar_t Sonar_Right;
extern Sonar_t Sonar_Left;
extern Sonar_t Sonar_Rear;

void Sonar_Filter_Init(void);
bool Sonar_Is_Data_Valid(uint16_t distance);
void Sonar_Data_Filter(Sonar_t *sonar);
uint16_t Sonar_Get_Filter_Distanse(Sonar_t *sonar);
void Sonar_Update_distanse(Sonar_t *sonar, uint16_t data);
void Sonar_Update_DistancePair(Sonar_t *sonar, uint16_t ch1_distance, uint16_t ch2_distance);
bool Sonar_Check_Newdata(Sonar_t *sonar);

#endif
