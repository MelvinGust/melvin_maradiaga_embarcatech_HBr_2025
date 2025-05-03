#include "adc_test.h"

float adc_to_celsius(uint16_t adc_val){
    float ADC_RESOLUTION = 3.3f / (1 << 12);
    return (27.0f - (ADC_RESOLUTION * (float)adc_val - 0.706f)/0.001721f);
}