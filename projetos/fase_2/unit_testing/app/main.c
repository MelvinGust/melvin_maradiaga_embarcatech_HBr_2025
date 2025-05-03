/* Diretivos do Pre-processador */
#include <stdio.h>
#include "pico/stdlib.h"
#include "../inc/temp_sensor.h"

int main()
{
    stdio_init_all();
    temp_sensor_init();

    while (true) {
        float temperature = temp_sensor_read();
        printf("Temperatura: %f \n", temperature);
        sleep_ms(200);
    }
}