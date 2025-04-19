
/* Diretivos do Pre-processador */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h" // Biblioteca disponibilizada no repositório BitDogLabC, ligeiramente modificada nesta aplicação.
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#define OLED_SDA 14
#define OLED_SCL 15
#define ADC_CHANNEL_4 4U

const float ADC_RESOLUTION = 3.3f / (1 << 12); // Esta expressão permite transformar os 12 bits obtidos do ADC em um float correspondente à tensão DC do sinal, assumindo VREF = 3,3 V.

int main()
{
    /* Configuro a comunicação com o OLED */
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);
    ssd1306_init();
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    /* Configuro o ADC para ler a temperatura interna do RP2040 */
    adc_init();
    adc_select_input(ADC_CHANNEL_4); // Leio seus dados internos
    adc_set_temp_sensor_enabled(true); // Ativo o sensor de temperatura do RP2040
    adc_set_clkdiv(0);

    while (true) {

        float ADC_voltage = ADC_RESOLUTION * (float)adc_read();
        float temperature = 27.0f - (ADC_voltage - 0.706f)/0.001721f;

        // Preparo a mensagem que vou apresentar no OLED.
        char mensagem_display[14];
        snprintf(mensagem_display, 14, "Temp: %.2f;C", temperature); // Coloco o ; para representar um simbolo de grau ° no display OLED. Este simbolo o coloquei manualmente na biblioteca.

        int y = 8;
        int x = 0;
        for( uint i = 0; i < count_of(mensagem_display); i++){
            ssd1306_draw_char(ssd, x, y, mensagem_display[i]);
            x += 8;
        }

        render_on_display(ssd, &frame_area); // Escrevo a string no OLED.
        sleep_ms(200); // A cada 200ms.
    }
}
