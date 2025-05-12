
#include "pico/stdlib.h"
#include "bitdoglab.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

// This mode of operation for the joystick was implemented as part of the EMBARCATECH course-load.
// Check the joystick_adc activity in my repository for details.

void joystick_init(uint16_t *joystick_samples, irq_handler_t dma_handler, uint dma_chan, dma_channel_config dma_cfg){
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
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16); // Vou transferir em blocos de 16 bits, um uint16_t.
    channel_config_set_read_increment(&dma_cfg, false); // Não estarei lendo nenhum novo endereço de memória.
    channel_config_set_write_increment(&dma_cfg, true); // joystick_samples[0] -> joystick_samples[1] -> Desativa DMA.
    dma_channel_set_irq0_enabled(dma_chan, true); // Seta uma flag de interrupção uma vez acaba a transferência do DMA.
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler); // Uma vez setada a flag, entra em dma_handler1.
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
}

