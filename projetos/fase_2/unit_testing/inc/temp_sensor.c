#include "temp_sensor.h"

void temp_sensor_init(){
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_set_clkdiv(0);
}

float temp_sensor_read(){
    adc_select_input(ADC_CHANNEL_4);
    uint16_t adc_val = adc_read();
    return adc_to_celsius(adc_val);
}

float adc_to_celsius(uint16_t adc_val){
    float ADC_RESOLUTION = 3.3f / (1 << 12);
    return (27.0f - (ADC_RESOLUTION * (float)adc_val - 0.706f)/0.001721f);
}
