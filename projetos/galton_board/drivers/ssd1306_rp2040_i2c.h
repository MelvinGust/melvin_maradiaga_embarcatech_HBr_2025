/*
--------------------
| SSD1306_OLED_I2C |
--------------------

Esta biblioteca é uma versão modificada da biblioteca fornecida do repositorio do BitDogLab C.
Os ajustes foram feitos para renderizar como desejado o tabuleiro de galton
*/

#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306_program_sprites.h"

#ifndef SSD1306_INC_H
#define SSD1306_INC_H

#define ssd1306_height 64 // Define a altura máxima do display (64 pixels)
#define ssd1306_width 128 // Define a largura máxima do display (128 pixels)

#define ssd1306_i2c_address _u(0x3C) // Define o endereço do i2c do display

#define ssd1306_i2c_clock 1000 // Define o tempo do clock (pode ser aumentado)

// Comandos de configuração (endereços)
#define ssd1306_set_memory_mode _u(0x20)
#define ssd1306_set_column_address _u(0x21)
#define ssd1306_set_page_address _u(0x22)
#define ssd1306_set_horizontal_scroll _u(0x26)
#define ssd1306_set_scroll _u(0x2E)

#define ssd1306_set_display_start_line _u(0x40)

#define ssd1306_set_contrast _u(0x81)
#define ssd1306_set_charge_pump _u(0x8D)

#define ssd1306_set_segment_remap _u(0xA0)
#define ssd1306_set_entire_on _u(0xA4)
#define ssd1306_set_all_on _u(0xA5)
#define ssd1306_set_normal_display _u(0xA6)
#define ssd1306_set_inverse_display _u(0xA7)
#define ssd1306_set_mux_ratio _u(0xA8)
#define ssd1306_set_display _u(0xAE)
#define ssd1306_set_common_output_direction _u(0xC0)
#define ssd1306_set_common_output_direction_flip _u(0xC0)

#define ssd1306_set_display_offset _u(0xD3)
#define ssd1306_set_display_clock_divide_ratio _u(0xD5)
#define ssd1306_set_precharge _u(0xD9)
#define ssd1306_set_common_pin_configuration _u(0xDA)
#define ssd1306_set_vcomh_deselect_level _u(0xDB)

#define ssd1306_set_fade_out_and_blinking _u(0x23)
#define ssd1306_set_zoom_in _u(0xD6)

#define ssd1306_page_height _u(8)
#define ssd1306_n_pages (ssd1306_height / ssd1306_page_height)
#define ssd1306_buffer_length (ssd1306_n_pages * ssd1306_width)

#define ssd1306_write_mode _u(0xFE)
#define ssd1306_read_mode _u(0xFF)

// Encaminha um comando pontual para configurar o OLED.
extern void ssd1306_init();
extern void ssd1306_blink(bool on);

extern void ssd1306_send_command_list(uint8_t *ssd, int number);
extern void ssd1306_send_command(uint8_t command);

extern void render_on_display(render_region_t *region);
extern void remove_on_display(render_region_t *region);
extern void ssd1306_send_buffer(uint8_t ssd[], int buffer_length);

extern void ssd1306_set_pixel(uint8_t *ssd, int x, int y, bool set, bool horizontal_addressing);
extern void ssd1306_draw_sprite(render_region_t *region, sprite_t *sprite, bool horizontal_addressing);
extern void ssd1306_remove_sprite(render_region_t *region, sprite_t *sprite, bool horizontal_addressing);

#endif