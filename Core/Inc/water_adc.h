#ifndef WATER_ADC_H
#define WATER_ADC_H

#include "main.h"
#include <stdint.h>

#ifndef WATER_DO_WET_LEVEL
#define WATER_DO_WET_LEVEL GPIO_PIN_RESET
#endif

void Myadc_Init(void);
uint16_t Get_ADC_Value(void);
uint16_t Water_Get_AnalogRaw(void);
uint32_t Water_RawToVoltageMv(uint16_t raw);
uint32_t Water_Get_AnalogVoltageMv(void);
uint16_t Power_Get_AdcRaw(void);
uint32_t Power_RawToAdcVoltageMv(uint16_t raw);
uint32_t Power_RawToInputVoltageMv(uint16_t raw);
uint32_t Power_Get_InputVoltageMv(void);
uint8_t Water_Get_Digital(void);
uint8_t Water_Is_Wet(void);

#endif
