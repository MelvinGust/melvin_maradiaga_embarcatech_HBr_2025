/*
---------------------
| BitDogAnalyzer V1 |
---------------------
EMBARCATECH - Projeto Final
Aluno: Melvin Gustavo Maradiaga Elvir

Obs. Ativar word wrap (Alt + Z no VSCode) para ler melhor o código.

Esta primeira versão do BitDogAnalyzer é uma prova de conceito, testando a viabilidade da implementação de um analisador lógico no RP2040. O projeto utiliza o PIO e DMA do microcontrolador, além de implementar o protocolo SUMP para comunicação com SIGROK configurado com o driver Openbench Logic Sniffer.
*/

/* Diretivos do Preprocessador */
// Inclusão de bibliotecas
#include <stdio.h> // Para o encaminhamento e recepção de dados mediante a porta serial.
#include <stdlib.h> // Para acesso a malloc.
#include "hardware/pio.h" // Para configurar PIO.
#include "hardware/dma.h" // Para configurar DMA.
#include "hardware/structs/bus_ctrl.h" // Para manipular o barramento de dados.
#include "hardware/structs/pwm.h" // Para teste do analisador lógico.
#include "pico/stdlib.h"
#include "BitDogAnalyzer.pio.h"

// Comandos do protocolo SUMP
#define SUMP_RESET 0x00
#define SUMP_RUN 0x01
#define SUMP_ID 0x02
#define SUMP_SELF_TEST 0x03
#define SUMP_METADATA 0x04
#define SUMP_SET_DIVIDER 0x80
#define SUMP_SET_READ_DELAY 0x81
#define SUMP_SET_FLAGS 0x82
#define SUMP_TRIGGER_MSK 0xC0
#define SUMP_TRIGGER_VAL 0xC1
#define SUMP_TRIGGER_CONFIG 0xC2

// Flags usadas para validação do circuito. Caso ela for comentada, não vai se entrar em modo de teste.
#define TESTE

// Variáveis globais
const uint MAX_SAMPLE_SIZE_BITS = 65536; // Em bits. Menor que 65535 propositalmente.
const uint MAX_SAMPLE_SIZE = MAX_SAMPLE_SIZE_BITS / 8; // 8192 bytes.
const uint GPIO_MEMORY_SIZE = MAX_SAMPLE_SIZE / sizeof(uint32_t); // Para armazenar 8192 bytes em blocos de 4 bytes, preciso de 2048 palavras (cada palavra é de 32 bits)
const uint MAX_SAMPLE_RATE = 100000000; // Em Hz. Usado com SUMP_SET_DIVIDER.
const uint FREQ_RP2040 = 125000000;
const uint NUMBER_PROBES = 4;
static const uint GPIO0 = 0; // Fico com medo que existam essas variáveis em outros lugares, logo usei o static.
static const uint GPIO1 = 1;
static const uint GPIO2 = 2;
static const uint GPIO3 = 3;
static const uint GPIO12 = 12;
const int chan0 = 0;
const int chan1 = 1;
const int chan2 = 2;
const int chan3 = 3;
uint8_t sump_command = 0;
uint8_t sump_command_bytes[5];
uint32_t freq_divider = 0;
uint8_t trigger_mask = 0;
uint8_t trigger_value = 0;

#ifdef TESTE
    static const uint GPIO16 = 16;
    static const uint GPIO17 = 17;
    static const uint GPIO18 = 18;
    static const uint GPIO19 = 19;
#endif

// Declaração de funções
void setup_DMA(PIO pio, uint sm, uint dma_chan, uint32_t *buf_capture_data, size_t capture_size_words);

void sump_receive_data(void);

int main()
{
    stdio_init_all();
    
    // Configuração LED
    gpio_init(GPIO12);
    gpio_set_dir(GPIO12, GPIO_OUT);

    /* 
    --------------------
    | Configuração PIO |
    --------------------

    Nesta seção do código se realizam as configurações do PIO detalhadas na seção de Hardware da documentação. Estou usando o PIO_0 e nenhuma máquina de estado nele será inicializada ainda. 

    Para fins de flexibilidade, o programa do PIO vai ser implementado mediante um arquivo .pio, dado que versões futuras poderiam precisar de algum mecânismo mais sofisticado implementado neles. Os comandos simples de configuração serão usados. Lembrando que preciso:
    1. Carregar o programa na memoria de algum PIO.
    2. Configurar os pinos e associar a máquina de estado responsável.
    3. Configurar as máquinas de estado (juntar o FIFO, ativar o wrap, ativar o DREQ).
    Obs. Algumas destas configurações já estão sendo feitas dentro do arquivo .pio deste projeto.
    */

    PIO pio = pio0;
    uint offset0 = pio_add_program(pio, &sample0_program);
    uint offset1 = pio_add_program(pio, &sample1_program);
    uint offset2 = pio_add_program(pio, &sample2_program);
    uint offset3 = pio_add_program(pio, &sample3_program);
    // Essas funções aqui foram escritas no arquivo BitDogAnalyzer.pio, e elas estão expandidas em BitDogAnalyzer.pio.h. Conferir para ter uma melhor ideia delas.
    pio_sm_config sm0_config = sample0_program_init(pio, 0, offset0, GPIO0, 12500.0f);
    pio_sm_config sm1_config = sample1_program_init(pio, 1, offset1, GPIO1, 12500.0f);
    pio_sm_config sm2_config = sample2_program_init(pio, 2, offset2, GPIO2, 12500.0f);
    pio_sm_config sm3_config = sample3_program_init(pio, 3, offset3, GPIO3, 12500.0f);

    /*
    --------------------
    | Configuração DMA |
    --------------------

    Nesta seção do código se realizam as configurações do DMA detalhadas na seção de Hardware da documentação. Vou utilizar quatro canais de DMA para não ter nenhum problema entre eles no momento de estar querendo atender à solicitação do DREQ.

    Nesta seção se começa pela alocação de memória na SRAM para os quatro arrays de armazenamento. Lembrando do processo, após preencher a FIFO_RX com as 8 palavras de 32 bits, o DMA escreve cada palavra dentro da SRAM (a palavra escrita também vai ser de 32 bits) até chegar na quantidade máxima de amostras desejada pelo usuario (neste caso, PulseView que escolhe).
    
    Como irei armazenar um máximo de MAX_SAMPLE_SIZE bits (65528 bits), vou alocar a quantidade de bytes correspondentes na minha memória. Isso seria, em total, 65528/8 = 8191 bytes por cada GPIO, totalizando 32764 bytes (ou 32,764 kB) da SRAM. O hard_assert depois de cada expressão termina a execução do programa se não tivermos memoria suficiente disponível.

    Vou configurar quatro canais separados de DMA para estarem mandando os dados da sua respectiva FIFO à SRAM.
    */

    uint32_t *GPIO0_data = malloc(MAX_SAMPLE_SIZE);
    hard_assert(GPIO0_data);
    uint32_t *GPIO1_data = malloc(MAX_SAMPLE_SIZE);
    hard_assert(GPIO1_data);
    uint32_t *GPIO2_data = malloc(MAX_SAMPLE_SIZE);
    hard_assert(GPIO2_data);
    uint32_t *GPIO3_data = malloc(MAX_SAMPLE_SIZE);
    hard_assert(GPIO3_data);

    // Esta função configura os state machines 0-3 do pio0 a funcionarem com o DREQ devido.
    setup_DMA(pio, 0, chan0, GPIO0_data, GPIO_MEMORY_SIZE);
    setup_DMA(pio, 1, chan1, GPIO1_data, GPIO_MEMORY_SIZE);
    setup_DMA(pio, 2, chan2, GPIO2_data, GPIO_MEMORY_SIZE);
    setup_DMA(pio, 3, chan3, GPIO3_data, GPIO_MEMORY_SIZE);

    // Colocar com prioridade no barramento de dados, para saturar ele completamente.
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    /*
    -----------------------------
    | Configuração PWM de Teste |
    -----------------------------
    Retirado do exemplo de analisador lógico no Pi Pico.

    Configuramos um PWM para gerar dois sinais em GPIO16 e GPIO17, um com um duty cycle de 25% e o outro com um duty cycle de 75%.
    Estas formas de onda foram visualizadas mediante Sigrok ao testarmos o código.
    */

    #ifdef TESTE
        gpio_set_function(GPIO16, GPIO_FUNC_PWM);
        gpio_set_function(GPIO16 + 1, GPIO_FUNC_PWM);

        pwm_hw->slice[0].top = 3;
        pwm_hw->slice[0].div = 4 << PWM_CH0_DIV_INT_LSB;
        pwm_hw->slice[0].cc =
                (1 << PWM_CH0_CC_A_LSB) |
                (3 << PWM_CH0_CC_B_LSB);
        pwm_hw->slice[0].csr = PWM_CH0_CSR_EN_BITS;
    #endif

    busy_wait_ms(10); // Dá um tempo para todo terminar de configurar.

    /*
    ------------------
    | Loop principal |
    ------------------
    
    O Loop principal é a seção do código encarregada da implementação do protocolo SUMP para comunicação com o SIGROK. Neste loop basicamente estou conferindo o canal USB, aguardando um novo comando.
    */

    while (true) {
        sump_command = getchar_timeout_us(10000);
        if (sump_command != PICO_ERROR_TIMEOUT){
            switch(sump_command){
                case SUMP_RESET:
                    // Faz nada. Pode encaminhar este comando ás vezes, melhor não interpretar.
                    break;

                case SUMP_RUN:
                
                    // Vou zerar as 2048 palavras de cada array.
                    for (int i = 0 ; i < GPIO_MEMORY_SIZE; i++) {
                        GPIO0_data[i] = 0;
                        GPIO1_data[i] = 0;
                        GPIO2_data[i] = 0;
                        GPIO3_data[i] = 0;
                    }

                    // Roda o analisador lógico, colocando a condição de trigger e ativando o DMA e o PIO.
                    if(trigger_mask & 0x01){ // Se der true, tem trigger.
                        pio_sm_exec(pio, 0, pio_encode_wait_gpio((0x01 & trigger_value),  GPIO0));
                    }
                    if(trigger_mask & 0x02){
                        pio_sm_exec(pio, 1, pio_encode_wait_gpio((0x02 & trigger_value),  GPIO1));
                    }
                    if(trigger_mask & 0x04){
                        pio_sm_exec(pio, 2, pio_encode_wait_gpio((0x04 & trigger_value),  GPIO2));
                    }
                    if(trigger_mask & 0x08){
                        pio_sm_exec(pio, 3, pio_encode_wait_gpio((0x08 & trigger_value),  GPIO3));
                    }

                    dma_channel_start(chan0);
                    dma_channel_start(chan1);
                    dma_channel_start(chan2);
                    dma_channel_start(chan3);

                    pio_sm_set_enabled(pio, 0, true);
                    pio_sm_set_enabled(pio, 1, true);
                    pio_sm_set_enabled(pio, 2, true);
                    pio_sm_set_enabled(pio, 3, true);

                    // Aguarda até todos os DMAs terminarem sua escrita de dados. Para isso, estou conferindo o flag da interrupção de transmissão finalizada.
                    while((!(dma_hw->intr & 1u << chan0))||(!(dma_hw->intr & 1u << chan1))||(!(dma_hw->intr & 1u << chan2))||(!(dma_hw->intr & 1u << chan3)));

                    dma_hw->ints0 = 1u << chan0;
                    dma_hw->ints0 = 1u << chan1;
                    dma_hw->ints0 = 1u << chan2;
                    dma_hw->ints0 = 1u << chan3;

                    // Desativa os PIOs e limpa o seu ISR e FIFO.
                    pio_sm_set_enabled(pio, 0, false);
                    pio_sm_set_enabled(pio, 1, false);
                    pio_sm_set_enabled(pio, 2, false);
                    pio_sm_set_enabled(pio, 3, false);
                    
                    pio_sm_clear_fifos(pio, 0);
                    pio_sm_restart(pio, 0);

                    pio_sm_clear_fifos(pio, 1);
                    pio_sm_restart(pio, 1);

                    pio_sm_clear_fifos(pio, 2);
                    pio_sm_restart(pio, 2);

                    pio_sm_clear_fifos(pio, 3);
                    pio_sm_restart(pio, 3);
                    
                    // Reseta o endereço inicial do DMA e desativa a interrupção
                    dma_channel_abort(chan0);
                    dma_channel_abort(chan1);
                    dma_channel_abort(chan2);
                    dma_channel_abort(chan3);

                    dma_channel_set_write_addr(chan0, GPIO0_data, false);
                    dma_channel_set_write_addr(chan1, GPIO1_data, false);
                    dma_channel_set_write_addr(chan2, GPIO2_data, false);
                    dma_channel_set_write_addr(chan3, GPIO3_data, false);

                    gpio_put(GPIO12,1); // Estou escrevendo os dados.

                    // Desativa o analisador e encaminha os dados mediante a porta serial.
                    for (int bit_pos = 0; bit_pos < 32; bit_pos++) {
                        for (int word_pos = 0; word_pos < GPIO_MEMORY_SIZE; word_pos++) {
                            uint8_t data_byte = ((GPIO3_data[word_pos] >> bit_pos) & 1) << 3 |((GPIO2_data[word_pos] >> bit_pos) & 1) << 2 | ((GPIO1_data[word_pos] >> bit_pos) & 1) << 1 | ((GPIO0_data[word_pos] >> bit_pos) & 1);
                            // Extrai bit_pos de GPIO3_data e move para posição 3
                            // Extrai bit_pos de GPIO2_data e move para posição 2
                            // Extrai bit_pos de GPIO1_data e move para posição 1
                            // Extrai bit_pos de GPIO0_data e mantém na posição 0
                            // Faz isso aqui para cada palavra.
                            putchar(data_byte);
                        }
                    }

                    busy_wait_ms(10);

                    gpio_put(GPIO12,0);

                    break;
                case SUMP_ID:
                    gpio_put(GPIO12,1);
                    // Encaminha identificação ao PulseView.
                    putchar('1');
                    putchar('A');
                    putchar('L');
                    putchar('S');
                    break;
                case SUMP_SELF_TEST:
                    // Faz nada, o comando não está configurado.
                    break;
                case SUMP_METADATA:
                    // Encaminho a metadata do PICO conforme solicitado pelo SUMP.
                    // Nome: BitDogAnalyzer
                    putchar((uint8_t)0x01);
                    putchar('B');
                    putchar('i');
                    putchar('t');
                    putchar('D');
                    putchar('o');
                    putchar('g');
                    putchar('A');
                    putchar('n');
                    putchar('a');
                    putchar('l');
                    putchar('y');
                    putchar('z');
                    putchar('e');
                    putchar('r');
                    putchar((uint8_t)0x00);

                    // Versão de firmware (V 0,1)
                    putchar((uint8_t)0x02);
                    putchar('0');
                    putchar(',');
                    putchar('1');
                    putchar((uint8_t)0x00);

                    // Memória máxima disponível (65536 bits)
                    putchar((uint8_t)0x21);
                    putchar((uint8_t)0x00);
                    putchar((uint8_t)0x01);
                    putchar((uint8_t)0x00);
                    putchar((uint8_t)0x00);

                    // Frequencia máxima possível (100 MHz)
                    putchar((uint8_t)0x23);
                    putchar((uint8_t)0x05);
                    putchar((uint8_t)0xF5);
                    putchar((uint8_t)0xE1);
                    putchar((uint8_t)0x00);

                    // Número de pinos disponíveis (4)
                    putchar((uint8_t)0x40);
                    putchar((uint8_t)0x04);

                    // Versão do protocolo (V2)
                    putchar((uint8_t)0x41);
                    putchar((uint8_t)0x02);

                    // Avisamos que finalizamos de encaminhar a informação
                    putchar((uint8_t)0x00);
                    gpio_put(GPIO12,0);

                    break;
                case SUMP_SET_DIVIDER:

                    // Este valor é o divisor que será considerando na formula do protocolo SUMP para a frequência desejada.
                    sump_receive_data();
                    freq_divider = sump_command_bytes[2];
                    freq_divider = freq_divider << 8;
                    freq_divider += sump_command_bytes[1];
                    freq_divider = freq_divider << 8;
                    freq_divider += sump_command_bytes[0];

                    /*
                    Preciso descobrir uma expressão que me permita determinar o valor do prescaler que vou colocar nas configurações das máquinas de estado. Isso aqui pode ser feito misturando e reescrevendo as duas formulas relevantes para a configuração do relógio:

                    Formula do SUMP (assume relógio de 100 MHz): freq_SUMP = 100000000/(freq_divider + 1)

                    Formula do PIO: freq_PIO = freq_sys_clock / clkdiv (onde clkdiv é um float).

                    Misturando os dois e resolvendo para clkdiv, obtemos:
                    clkdiv = (freq_sys_clock/100000000)*(freq_divider+1).

                    Note-se que internamente, o registrador (SMx_CLKDIV) no qual estamos escrevendo os dados de configuração tem a seguinte estrutura:
                    Bits 31:16 INT
                    Bits 15:8 FRAC  
                    Bits 7:0 Reservados

                    Onde clkdiv = INT + FRAC/256. 
                    Devido ao tamanho limitado (no registrador) dos dois valores, INT pode ser no máximo 65536 e FRAC pode ser no máximo 256. Isso faz com que seja impossível configurar na frequencia de clock normal um PIO com clk menor a 1908 Hz. Por conta disso, realizamos uma revisão no código para conferir se a frequência desejada está permitida.
                    
                    (Obs.Mmudando a frequência do CPU, poderiamos diminuir ainda mais a frequência de amostragem, mas aí entramos em problemas com a comunicação USB conforme dito na documentação. Também, frequências maiores de 2 MHz começam a ser imprecisas.)

                    O projeto atual possui só um mecanismo de trigger simples.
                    */
                    
                    // Fato curioso, sm_config_set_clkdiv só modifica a estrutura do tipo pio_sm_config. Para modificar diretamente a máquina de estados, usar pio_sm_set_clkdiv.
                    if(freq_divider > 49999){
                        pio_sm_set_clkdiv(pio, 0, 62500.0f);
                        pio_sm_set_clkdiv(pio, 1, 62500.0f);
                        pio_sm_set_clkdiv(pio, 2, 62500.0f);
                        pio_sm_set_clkdiv(pio, 3, 62500.0f);
                    }else{
                        float clkdiv = (((float)FREQ_RP2040)/((float)MAX_SAMPLE_RATE))*((float)(freq_divider+1));
                        pio_sm_set_clkdiv(pio, 0, clkdiv);
                        pio_sm_set_clkdiv(pio, 1, clkdiv);
                        pio_sm_set_clkdiv(pio, 2, clkdiv);
                        pio_sm_set_clkdiv(pio, 3, clkdiv);
                    }
                    busy_wait_ms(10);

                    break;
                    // Usado para configurar triggers mais complexos. A versão atual só funciona com trigger simples.~
                case SUMP_SET_READ_DELAY:
                    sump_receive_data();
                    break;

                case SUMP_SET_FLAGS:
                    // Lê os dados mas ignora.
                    sump_receive_data();
                    break;
                
                case SUMP_TRIGGER_MSK:
                    sump_receive_data();
                    trigger_mask = sump_command_bytes[0];
                    break;

                case SUMP_TRIGGER_VAL:
                    sump_receive_data();
                    trigger_value = sump_command_bytes[0];
                    break;

                case SUMP_TRIGGER_CONFIG:
                    // Lê os dados mas ignora.
                    sump_receive_data();
                    break;
                    
                default:
                    // Caso entrar qualquer outro valor, ignorar.
                    break;
            }
        }
    }
}

/*
-----------
| Funções |
-----------
(1) setup_DMA: Como o nome especifica, configura o DMA como desejamos utilizar ele nesta aplicação.

(2) sump_receive_data: Recebe os dados do USB que foram encaminhados pelo sigrok.
*/

void setup_DMA(PIO pio, uint sm, uint dma_chan, uint32_t *buf_capture_data, size_t capture_size_words){
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, false); // Não aumenta o endereço fonte.
    channel_config_set_write_increment(&c, true); // Aumenta o endereço de destino.
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
    dma_channel_configure(dma_chan, &c,
        buf_capture_data,        // Endereço de destino
        &pio->rxf[sm],      // Endereço fonte
        capture_size_words, // Número de transferências
        false               // Se false, não começa a funcionar immediatamente.
    );
    dma_channel_set_irq0_enabled(dma_chan, true); // Caso terminar as transferências, ativa uma flag de interrupção. Esta flag de interrupção vai ser lida para terminar nossa amostragem.
}

void sump_receive_data(void){
    busy_wait_ms(10);
    sump_command_bytes[0] = getchar();
    sump_command_bytes[1] = getchar();
    sump_command_bytes[2] = getchar();
    sump_command_bytes[3] = getchar();
}

