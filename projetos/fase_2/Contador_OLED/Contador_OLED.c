/* Diretivos do Preprocessador */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"

#define BOTAO_A 5    
#define BOTAO_B 6
#define OLED_SDA 14
#define OLED_SCL 15
#define WAIT_DEBOUNCE 20000U
#define ATIVO_DEBOUNCE // Se esta flag é comentada, não há lógica de debounce no programa.

volatile uint8_t num_clicks = 0; // volatile por ser modificada dentro de uma SRI.
uint8_t time_counter = 0;

typedef enum {
    ATIVA_CONTADOR,
    CONTANDO,
    DEBOUNCE_RISE,
    DEBOUNCE_FALL,
    STANDBY
} state_flags; // Defino um enum para representar os meus estados da máquina de estados.
volatile state_flags estado_programa = STANDBY;

#ifdef ATIVO_DEBOUNCE
volatile absolute_time_t debounce_time;
#endif
absolute_time_t countdown_time;

/*
gpio_irq_callback:

Como no RP2040 existe um único IRQ para todos os GPIOs, é necessário conferir dentro da SRI o botão que realizou a solicitação. Neste caso, é necessário conferir se o Botao A ou o B setaram a flag de interrupção.
O argumento de entrada events é utilizado para confirmar o motivo pelo qual o Botão entrou na SRI. Nesta aplicação, ser por uma borda de descida (GPIO_IRQ_EDGE_FALL) ou por uma borda de subida (GPIO_IRQ_EDGE_RISE).

*/
void gpio_irq_callback(uint gpio, uint32_t events){
    if(gpio == BOTAO_A){
        estado_programa = ATIVA_CONTADOR; // Troco de estado.
    }
    if((gpio == BOTAO_B)&&(estado_programa == CONTANDO)){

        #ifdef ATIVO_DEBOUNCE
        if(events & GPIO_IRQ_EDGE_FALL){
            gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, false);
            debounce_time = get_absolute_time(); // Obtenho o tempo em que entrei na SRI
            estado_programa = DEBOUNCE_FALL; // Realizo o debounce da borda de descida
            num_clicks++; // Aumento o número de vezes que o botão foi apertado.
        } else if(events & GPIO_IRQ_EDGE_RISE){
            gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_RISE, false);
            debounce_time = get_absolute_time();
            estado_programa = DEBOUNCE_RISE; // Realizo o debounce da borda de subida
        }
        #elif !ATIVO_DEBOUNCE
            num_clicks++;
        #endif
    }
}

int main()
{  
    /* Configuro interrupção e botões */
    gpio_init(BOTAO_A);
    gpio_init(BOTAO_B);
    gpio_pull_up(BOTAO_A);
    gpio_pull_up(BOTAO_B);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_set_dir(BOTAO_B, GPIO_IN);

    // Como o RP2040 conta com um único IRQ para todos os GPIOs, eu realizo a configuração geral com o primeiro comando (o irq_enabled_with_callback) e depois habilito interrupções no outro pino.
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_callback);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, false); // Só é ativado quando começo a contagem.

    /* Configuro o I2C */
    i2c_init(i2c1, ssd1306_i2c_clock * 1000); // Obs. Posso mudar o clk do ssd1306.
    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);
    ssd1306_init();

    // Caso eu desejar customizar a área de renderização no display, posso mudar estes parâmetros.
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);
    // Zera o display inteiro antes de permitir o seu uso.
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    while (true) {
        if(ATIVA_CONTADOR == estado_programa){ 
            estado_programa = CONTANDO; 
            time_counter = 9; // Inicializo o contador de tempo em 9s.
            num_clicks = 0; // Inicializo o # de clicks em 0.
            countdown_time = get_absolute_time(); // Inicializo o contador de tempo.
            gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true); // Ativo a interrupção
        }

        #ifdef ATIVO_DEBOUNCE
        if(absolute_time_diff_us(debounce_time, get_absolute_time()) > WAIT_DEBOUNCE){
            if(DEBOUNCE_FALL == estado_programa){ // Depois de fazer o debounce da descida
                gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_RISE, true); // Faço o debounce da subida.
                estado_programa = CONTANDO;
            } else if (DEBOUNCE_RISE == estado_programa){ // Depois de fazer o debounce da subida
                gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true); // Volto a fazer o debounce da descida.
                estado_programa = CONTANDO;
            }
        }
        #endif

        absolute_time_t current_time = get_absolute_time();
        if((CONTANDO == estado_programa)&&(absolute_time_diff_us(countdown_time, current_time) > 1000000u)){ // Se passou 1s desde que configurei time_counter, diminuo o valor da variável.
            countdown_time = current_time; // Reinicio o contador de tempo.
            time_counter--;
            if (time_counter == 0){
                estado_programa = STANDBY; // Se finalizei a contagem, volto ao estado inicial.
            }
        }

        /*
        Atualizo os valores no OLED a cada loop no main. Note-se que ele sempre vai ficar mostrado o mesmo texto: 
        contador: #
        num_clicks: #
        */

        char time_counter_c;
        time_counter_c = '0' + time_counter;

        char num_clicks_c[3];
        uint8_t temp_clicks = num_clicks; // Realizo a conversão de inteiro à caractere, e escrevo os dados numa matriz.
        for(uint i = 0; i < count_of(num_clicks_c); i++){
            num_clicks_c[i] = '0' + temp_clicks % 10;
            temp_clicks = temp_clicks / 10;
        }

        char mensagem1_display[] = {'c','o','n','t','a','d','o','r',':',' ',time_counter_c};
        char mensagem2_display[] = {'n','u','m',' ','c','l','i','c','k','s',':',' ', num_clicks_c[2], num_clicks_c[1], num_clicks_c[0]};

        // Manipulando os valores de x e y posso definir onde minha mensagem vai aparecer. Parece que o y funciona em pulos de 8 pixels. Por conta de facilidade, decidi usar a função draw_char, em vez de draw_string.

        int y = 8;
        int x = 0;
        for( uint i = 0; i < count_of(mensagem1_display); i++){
            ssd1306_draw_char(ssd, x, y, mensagem1_display[i]);
            x += 8;
        }

        y += 16;
        x = 0;
        for( uint i = 0; i < count_of(mensagem2_display); i++){
            ssd1306_draw_char(ssd, x, y, mensagem2_display[i]);
            x += 8;
        }
        render_on_display(ssd, &frame_area); // Escreve no OLED.
    }
}
