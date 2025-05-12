#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "hardware/dma.h"
#include "hardware/irq.h"

#define VRY_GPIO26 26
#define VRX_GPIO27 27
#define BOTAO_A 5
#define BOTAO_B 6
#define SWITCH 22
#define OLED_SDA 14
#define OLED_SCL 15
#define ADC_VREF 3.3f
#define ADC_CHANNEL_0 0
#define ADC_CHANNEL_1 1

extern void joystick_init(uint16_t *joystick_samples, irq_handler_t dma_handler, uint dma_chan, dma_channel_config dma_cfg);

#endif