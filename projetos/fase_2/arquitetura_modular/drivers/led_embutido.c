#include "pico/cyw43_arch.h"

bool picow_led_init(void){
    if(cyw43_arch_init()){
        return false;
    }
    return true;
}

void picow_led_put(bool status){
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, status);
}
