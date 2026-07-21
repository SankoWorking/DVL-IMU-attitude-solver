#include "sonar_filter.h"
#include <string.h>

Sonar_t Sonar_Front = {0};
Sonar_t Sonar_Right = {0};
Sonar_t Sonar_Left = {0};
Sonar_t Sonar_Rear = {0};

static uint16_t Median_Filter(Sonar_t *sonar)
{
    uint16_t temp_buf[FILTER_WINDOW_SIZE];
    uint8_t i;
    uint8_t j;
    uint8_t count;

    sonar->filter_buffer[sonar->filter_index] = sonar->raw_distance;
    sonar->filter_index = (uint8_t)((sonar->filter_index + 1U) % FILTER_WINDOW_SIZE);
    if (sonar->sample_count < FILTER_WINDOW_SIZE)
    {
        sonar->sample_count++;
    }

    count = sonar->sample_count;
    memcpy(temp_buf, sonar->filter_buffer, sizeof(temp_buf));

    for (i = 0U; i < count - 1U; i++)
    {
        for (j = 0U; j < count - 1U - i; j++)
        {
            if (temp_buf[j] > temp_buf[j + 1U])
            {
                uint16_t temp;

                temp = temp_buf[j];
                temp_buf[j] = temp_buf[j + 1U];
                temp_buf[j + 1U] = temp;
            }
        }
    }

    return temp_buf[count / 2U];
}

static void Sonar_Mark_Invalid(Sonar_t *sonar)
{
    uint8_t i;

    if (sonar == NULL)
    {
        return;
    }

    sonar->raw_distance = SONAR_INVALID_DISTANCE;
    sonar->raw_distance_ch2 = SONAR_INVALID_DISTANCE;
    sonar->filter_distanse = SONAR_INVALID_DISTANCE;
    sonar->last_lowpass_val = 0.0f;
    sonar->sample_count = 0U;
    sonar->filter_index = 0U;
    sonar->new_data_ready = false;
    for (i = 0U; i < FILTER_WINDOW_SIZE; i++)
    {
        sonar->filter_buffer[i] = SONAR_INVALID_DISTANCE;
    }
}

void Sonar_Filter_Init(void)
{
    Sonar_t *sonars[4];
    uint8_t i;
    uint8_t t;

    sonars[0] = &Sonar_Front;
    sonars[1] = &Sonar_Right;
    sonars[2] = &Sonar_Left;
    sonars[3] = &Sonar_Rear;

    for (i = 0U; i < 4U; i++)
    {
        memset(sonars[i], 0, sizeof(Sonar_t));
        for (t = 0U; t < FILTER_WINDOW_SIZE; t++)
        {
            sonars[i]->filter_buffer[t] = SONAR_INVALID_DISTANCE;
        }
        sonars[i]->last_lowpass_val = 0.0f;
        sonars[i]->filter_distanse = SONAR_INVALID_DISTANCE;
        sonars[i]->raw_distance = SONAR_INVALID_DISTANCE;
        sonars[i]->raw_distance_ch2 = SONAR_INVALID_DISTANCE;
        sonars[i]->last_input_distance = SONAR_INVALID_DISTANCE;
        sonars[i]->last_input_distance_ch2 = SONAR_INVALID_DISTANCE;
        sonars[i]->new_data_ready = false;
        sonars[i]->filter_index = 0U;
        sonars[i]->sample_count = 0U;
        sonars[i]->last_update_tick = 0U;
    }
}

bool Sonar_Is_Data_Valid(uint16_t distance)
{
    return ((distance >= SONAR_MIN_VALID_DISTANCE) &&
            (distance <= SONAR_MAX_VALID_DISTANCE) &&
            (distance != SONAR_INVALID_DISTANCE));
}

void Sonar_Data_Filter(Sonar_t *sonar)
{
    uint16_t median_val;
    float filtered_val;
    uint32_t now;

    if (sonar == NULL)
    {
        return;
    }

    now = HAL_GetTick();
    if ((sonar->last_update_tick == 0U) ||
        ((now - sonar->last_update_tick) > SONAR_STALE_TIMEOUT_MS))
    {
        Sonar_Mark_Invalid(sonar);
        return;
    }

    if (sonar->new_data_ready)
    {
        sonar->new_data_ready = false;
        if (Sonar_Is_Data_Valid(sonar->raw_distance))
        {
            median_val = Median_Filter(sonar);
            if ((sonar->sample_count <= 1U) ||
                (sonar->filter_distanse == SONAR_INVALID_DISTANCE))
            {
                filtered_val = (float)median_val;
            }
            else
            {
                filtered_val = LOW_PASS_ALPHA * median_val +
                               (1.0f - LOW_PASS_ALPHA) * sonar->last_lowpass_val;
            }
            sonar->last_lowpass_val = filtered_val;
            sonar->filter_distanse = (uint16_t)filtered_val;
        }
        else
        {
            Sonar_Mark_Invalid(sonar);
        }
    }
}

uint16_t Sonar_Get_Filter_Distanse(Sonar_t *sonar)
{
    if (sonar == NULL)
    {
        return SONAR_INVALID_DISTANCE;
    }

    return sonar->filter_distanse;
}

void Sonar_Update_distanse(Sonar_t *sonar, uint16_t data)
{
    Sonar_Update_DistancePair(sonar, data, SONAR_INVALID_DISTANCE);
}

void Sonar_Update_DistancePair(Sonar_t *sonar, uint16_t ch1_distance, uint16_t ch2_distance)
{
    if (sonar == NULL)
    {
        return;
    }

    sonar->raw_distance = ch1_distance;
    sonar->raw_distance_ch2 = ch2_distance;
    sonar->last_input_distance = ch1_distance;
    sonar->last_input_distance_ch2 = ch2_distance;
    sonar->new_data_ready = true;
    sonar->last_update_tick = HAL_GetTick();
    sonar->frame_count++;
}

bool Sonar_Check_Newdata(Sonar_t *sonar)
{
    if (sonar == NULL)
    {
        return false;
    }

    return sonar->new_data_ready;
}
