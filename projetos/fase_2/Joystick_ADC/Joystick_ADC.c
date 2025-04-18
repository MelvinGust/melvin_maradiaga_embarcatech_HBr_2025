
/* Diretivos do Pre-processador */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h" // Biblioteca disponibilizada no repositório BitDogLabC
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

#define VRY_GPIO26 26
#define VRX_GPIO27 27
#define ADC_VREF 3.3f
#define OLED_SDA 14
#define OLED_SCL 15
#define ADC_CHANNEL_0 0
#define ADC_CHANNEL_1 1

#define LINEAR_FIT // Comentar esta linha para ver os valores normais do Joystick.

/* Variáveis globais */
uint dma_chan;
uint16_t joystick_samples[2] __attribute__((aligned(4))); // O atributo aligned(2) é usado para alinhar os dados de joystick_samples num bloco de 2 bytes exato na memória.
const uint32_t transfer_count = 2;
const float conv_factor  = ADC_VREF / (1<<12); // O valor de cada "step" do ADC, assumindo sua resolução padrão de 12 bits. 1<<12 = 2^12 = 4096.

// SRI do DMA, usada para reinicializar o DMA uma vez ele acaba sua operação.
void dma_handler1(){    
    (dma_hw->ints0) = 1u << dma_chan; // Reinicia a interrupção
    dma_start_channel_mask(1u << dma_chan); // Reinicia o canal
}

int main()
{   
    /* Configuro a comunicação com o OLED */
    i2c_init(i2c1, ssd1306_i2c_clock * 1000); // Obs. Posso mudar o clk do ssd1306.
    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);
    ssd1306_init();

    // Caso eu desejar customizar a área de renderização no display.
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

    /* Configuro o ADC */
    adc_init();
    // Coloco meus GPIOs com alta impedância na entrada, funcionando como entrada digital, e desabilito o pull_up ou pull_down.
    adc_gpio_init(VRY_GPIO26);
    adc_gpio_init(VRX_GPIO27);
    adc_set_round_robin((1<<ADC_CHANNEL_0) | (1<<ADC_CHANNEL_1)); // Defino que vou fazer um round-robin com o canal 0 e 1 do ADC. Assim, o ADC lê um canal distinto (canal 0 -> canal 1 -> canal 0 -> canal 1 -> ...) a cada amostra.

    adc_fifo_setup(
        true, // Escreve na FIFO do ADC.
        true, // Ativa o DREQ da minha DMA para conversão ADC.
        1, // Com uma amostra seta o DREQ e entra numa interrupção
        false,   // Ativar bit de erro, não interesa a gente.
        false    // Quero transmitir os 12 bits esperados do DMA.
    );
    adc_set_clkdiv(0); // OBS. Preciso configurar isso aqui antes de começar o ADC.

    /* Configuro o DMA */
    dma_chan = dma_claim_unused_channel(true); // Um DMA vai obter os dados do meu ADC, em round robin.
    dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16); // Vou transferir em blocos de 16 bits, um uint16_t.
    channel_config_set_read_increment(&dma_cfg, false); // Não estarei lendo nenhum novo endereço de memória.
    channel_config_set_write_increment(&dma_cfg, true); // joystick_samples[0] -> joystick_samples[1] -> Desativa DMA.
    dma_channel_set_irq0_enabled(dma_chan, true); // Seta uma flag de interrupção uma vez acaba a transferência do DMA.
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler1); // Uma vez setada a flag, entra em dma_handler1.
    irq_set_enabled(DMA_IRQ_0, true); // Ativo as interrupções.

    channel_config_set_dreq(&dma_cfg, DREQ_ADC); // Ativo a DREQ do ADC.
    channel_config_set_ring(&dma_cfg, true, 2); // Crio um buffer de 4 bytes de memoria dentro do meu RP2040

    dma_channel_configure(dma_chan, &dma_cfg,
        joystick_samples,    // Endereço onde vou escrever os dados
        &adc_hw->fifo,  // Endereço onde vou ler os dados
        2,  // # de transferência que irei realizar antes de parar.
        true // Se true, ativo o canal.
    );

    adc_run(true); // Entro no modo free-running do ADC.

    // Neste loop realizo a escrita dos dados amostrados no OLED.
    while(true) { 
        char joystick_y_c[4];
        char joystick_x_c[4];

        // Ativando a flag LINEAR_FIT ativa-se um trecho de código que mediante duas funções lineares faz o ajuste dos valores obtidos do joystick. Dessa forma criando o mapeamento de:
        // Joystick_Y: 19-1952-4085 -> 0-2048-4096
        // Joystick_X: 19-2041-4085 -> 0-2048-4096

        uint16_t temp_vry = joystick_samples[0];
        uint16_t temp_vrx = joystick_samples[1];

        #ifdef LINEAR_FIT
            if(temp_vry <= 1933){
                temp_vry = (uint16_t)((2048.0/1933.0)*(float)(temp_vry-19));
            }else{
                temp_vry = (uint16_t)((2048.0/(4066.0 - 1933.0))*(float)(temp_vry-19) + 192.03);
            }

            if(temp_vrx <= 2022){
                temp_vrx = (uint16_t)((2048.0/2022.0)*(float)(temp_vrx-19));
            }else{
                temp_vrx = (uint16_t)((2048.0/(4066.0 - 2022.0))*(float)(temp_vrx-19) + 22.043);
            }
        #endif

        // Faço a conversão de binário para ASCII dos digitos 0-9 dentro do meu número com valor máximo de 4096. 

        for(uint i = 0; i < count_of(joystick_x_c); i++){
            joystick_y_c[i] = '0' + temp_vry % 10;
            joystick_x_c[i] = '0' + temp_vrx % 10;
            temp_vry /= 10;
            temp_vrx /= 10;
        }

        // Preparo a mensagem que vou apresentar no OLED.
        char mensagem1_display[] = {'J','o','y','s','t','i','c','k',' ','Y',':',' ' , joystick_y_c[3], joystick_y_c[2], joystick_y_c[1], joystick_y_c[0]};
        char mensagem2_display[] = {'J','o','y','s','t','i','c','k',' ','X',':',' ' ,
        joystick_x_c[3], joystick_x_c[2], joystick_x_c[1], joystick_x_c[0]};

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
        render_on_display(ssd, &frame_area);
    }
}
