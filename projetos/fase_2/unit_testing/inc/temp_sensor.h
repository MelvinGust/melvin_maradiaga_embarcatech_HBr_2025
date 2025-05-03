#ifndef TEMP_SENSOR_HEADER
#define TEMP_SENSOR_HEADER

#include "hardware/adc.h"

#define ADC_CHANNEL_4 4U

/**
 * @brief 
 * 
 */
void temp_sensor_init();

/**
 * @brief 
 * 
 * @return float 
 */
float temp_sensor_read();

/**
 * @brief 
 * 
 */
float adc_to_celsius(uint16_t adc_val);

#endif