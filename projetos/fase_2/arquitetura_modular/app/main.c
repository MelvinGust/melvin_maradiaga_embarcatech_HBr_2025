#include "pico/stdlib.h"
#include "../include/hal_led.h"

int main() {
    stdio_init_all();
 
    if(!hal_led_init()){
        return -1;
    }

    while (true) {
        hal_led_toggle();
        sleep_ms(500);
    }
}
