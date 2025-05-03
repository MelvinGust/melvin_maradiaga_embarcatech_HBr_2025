#include "../include/hal_led.h"

static bool led_status = false;

bool hal_led_init(){
    return picow_led_init();
}

void hal_led_toggle(){
    led_status = !(led_status);
    picow_led_put(led_status);
}