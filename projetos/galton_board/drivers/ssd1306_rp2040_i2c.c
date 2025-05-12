#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "ssd1306_rp2040_i2c.h"

// Boot up the SSD1306
void ssd1306_init(){
    uint8_t init_commands[] = {
        ssd1306_set_display, // Disable display.
        ssd1306_set_memory_mode, 0x01, // Set to vertical addressing.
        ssd1306_set_display_start_line, // Set display start line register as 0.
        ssd1306_set_segment_remap | 0x01, // Column address 127 is mapped to SEG0 of memory.
        ssd1306_set_mux_ratio, ssd1306_height - 1, // Set MUX ratio to 64
        ssd1306_set_common_output_direction | 0x08, // Scan from COM64 -> COM0
        ssd1306_set_display_offset, 0x00, // Disable OFFSET
        ssd1306_set_common_pin_configuration, 0x12, // Enable alternative COM pin config.
        ssd1306_set_display_clock_divide_ratio, 0x80, // Set Freq_osc = 8 and D_CLK = 0
        ssd1306_set_precharge, 0xF1,
        ssd1306_set_vcomh_deselect_level, 0x30,
        ssd1306_set_contrast, 0xFF, // Set max contrast.
        ssd1306_set_entire_on,
        ssd1306_set_normal_display,
        ssd1306_set_charge_pump, 0x14,
        ssd1306_set_scroll | 0x00, // Deactivate scrolling.
        ssd1306_set_display | 0x01, // Turn display on.
    };
    ssd1306_send_command_list(init_commands, count_of(init_commands));
}

/**
 * @brief Used for flashing SSD1306 display.
 * 
 * @param on If on, inverts display. If off, sets normal display.
 */
void ssd1306_blink(bool on){ 
    if(true == on ){
        ssd1306_send_command(ssd1306_set_inverse_display);
    } else {
        ssd1306_send_command(ssd1306_set_normal_display);
    }
}

/**
 * @brief Sends a list of commands.
 * 
 * @param ssd Buffer to be sent.
 * @param number Number of commands (elements in buffer) to be sent.
 */
void ssd1306_send_command_list(uint8_t *ssd, int number) {
    for (int i = 0; i < number; i++) {
        ssd1306_send_command(ssd[i]);
    }
}

/**
 * @brief Send a specific control command.
 * 
 * @param command Command element to be sent.
 */
void ssd1306_send_command(uint8_t command) {
    uint8_t buffer[2] = {0x80, command};
    i2c_write_blocking(i2c1, ssd1306_i2c_address, buffer, 2, false);
}

/**
 * @brief Renders a render_region_t on the SSD1306. Note this is sent considering the vertical memory addressing. If it were horizontally addressed, reformat your bitmap.
 * 
 * @param region The render region to be sent.
 */
void render_on_display(render_region_t *region){
    uint8_t pos_commands[] = {
        ssd1306_set_column_address, region->start_column, region->end_column,
        ssd1306_set_page_address, region->start_page, region->end_page
    };
    ssd1306_send_command_list(pos_commands, count_of(pos_commands));
    ssd1306_send_buffer(region->bitmap, region->buffer_length);
}

/**
 * @brief Removes the render_region_t from the SSD1306.
 * 
 * @param region The render region to be sent.
 */
void remove_on_display(render_region_t *region){
    uint8_t pos_commands[] = {
        ssd1306_set_column_address, region->start_column, region->end_column,
        ssd1306_set_page_address, region->start_page, region->end_page
    };
    ssd1306_send_command_list(pos_commands, count_of(pos_commands));
    uint8_t *temp_data = malloc(region->buffer_length);
    memset(temp_data, 0, region->buffer_length);
    ssd1306_send_buffer(temp_data, region->buffer_length);
    free(temp_data);
}

/**
 * @brief Send a specific memory buffer to be stored on the SSD1306 RAM. Use of malloc may fragment memory.
 * 
 * @param ssd Memory buffer to be sent.
 * @param buffer_length Size  of the buffer.
 */
void ssd1306_send_buffer(uint8_t ssd[], int buffer_length) {
    uint8_t *temp_buffer = malloc(buffer_length + 1);

    temp_buffer[0] = 0x40;
    memcpy(temp_buffer + 1, ssd, buffer_length);

    i2c_write_blocking(i2c1, ssd1306_i2c_address, temp_buffer, buffer_length + 1, false);

    free(temp_buffer);
}

/**
 * @brief Determines which specific pixel should be set on my memory buffer. Depending on how the SSD1306 is addressed, two different processes should be run.
 * 
 * @param ssd Memory buffer to be modified.
 * @param x X address (left-to-right) of the pixel to be set.
 * @param y Y address (up-to-down) of the pixel to be set.
 * @param set If true, set pixel. If false, disable pixel.
 * @param horizontal_addressing If SSD1306 is horizontally addressed, use a different procedure to determine pixel position.
 */
void ssd1306_set_pixel(uint8_t *ssd, int x, int y, bool set, bool horizontal_addressing) {
    assert(x >= 0 && x < ssd1306_width && y >= 0 && y < ssd1306_height);

    if(horizontal_addressing == true){
        const int bytes_per_row = ssd1306_width;

        int byte_idx = (y / 8) * bytes_per_row + x;
        uint8_t byte = ssd[byte_idx];
    
        if (set) {
            byte |= 1 << (y % 8);
        }
        else {
            byte &= ~(1 << (y % 8));
        }
    
        ssd[byte_idx] = byte;
    } else { // Vertical addressing
        const int bytes_per_column = ssd1306_n_pages; // Since I'm sending bytes, I need to count the pages in memory.

        int page = y / 8;  // Which page am I acessing?
        int bit_pos = y % 8;  // Which bit within the byte.
    
        // Byte index for vertical addressing
        int byte_idy = x * bytes_per_column + page;
        uint8_t byte = ssd[byte_idy];
    
        if (set) {
            byte |= (1 << bit_pos);
        } else {
            byte &= ~(1 << bit_pos);
        }
        ssd[byte_idy] = byte;
    }
}

/**
 * @brief Render a sprite on the memory buffer associated with the specified render region.
 * 
 * @param region Render region to be accessed and modified.
 * @param sprite Sprite to be drawn on the render region.
 * @param horizontal_addressing If the SSD1306 is horizontally addressed, use a different procedure.
 */
void ssd1306_draw_sprite(render_region_t *region, sprite_t *sprite, bool horizontal_addressing){
    uint8_t base_x_coord = sprite->x_pos;
    uint8_t base_y_coord = sprite->y_pos;
    uint8_t x, y;
    if (horizontal_addressing == true) {
        // Not used in this proyect, so it wasn't implemented.
    } else {
        for(uint8_t sprite_row = 0; sprite_row < sprite->width; sprite_row++){
            for(uint8_t sprite_column = 0; sprite_column < sprite->height; sprite_column++){
                // Counting from LSB to MSB of the corresponding row.
                x = base_x_coord + sprite_row;
                y = base_y_coord + sprite_column;
                if(sprite->bitmap[sprite_row] & (1<<sprite_column)){
                    // If pixel in column is 1, set it on my render region.
                    ssd1306_set_pixel(region->bitmap, x, y, true, false);
                } else {
                    // If pixel in column is 0, clear it on my render region.
                    ssd1306_set_pixel(region->bitmap, x, y, false, false);
                }
            }
        }
    }
}

/**
 * @brief Remove a sprite from the render region's memory buffer.
 * 
 * @param region Render region to be accessed and modified.
 * @param sprite Sprite to be removed from the render region.
 * @param horizontal_addressing If the SSD1306 is horizontally addressed, use a different procedure.
 */
void ssd1306_remove_sprite(render_region_t *region, sprite_t *sprite, bool horizontal_addressing){
    uint8_t base_x_coord = sprite->x_pos;
    uint8_t base_y_coord = sprite->y_pos;
    uint8_t x, y;
    if (horizontal_addressing == true) {
        // Not used in this proyect, so it wasn't implemented.
    } else {
        for(uint8_t sprite_row = 0; sprite_row < sprite->width; sprite_row++){
            for(uint8_t sprite_column = 0; sprite_column < sprite->height; sprite_column++){
                // Counting from LSB to MSB of the corresponding row.
                x = base_x_coord + sprite_row;
                y = base_y_coord + sprite_column;
                if(sprite->bitmap[sprite_row] & (1<<sprite_column)){
                    // If pixel in column is 1, set it on my render region.
                    ssd1306_set_pixel(region->bitmap, x, y, false, false);
                } else {
                    // If pixel in column is 0, clear it on my render region.
                    ssd1306_set_pixel(region->bitmap, x, y, false, false);
                }
            }
        }
    } 
}
