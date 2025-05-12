/*
----------------
| GALTON BOARD |
----------------
ENABLE WORD WRAP.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/rand.h"

#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/adc.h"

#include "../drivers/ssd1306_rp2040_i2c.h"
#include "../drivers/bitdoglab.h"

/* INTERFACE */
// The constants here change vital characteristics of the program. Feel free to change em as needed. 
// As a general suggestion, keep GAME_TICK_MS > OLED_REFRESH_RATE_MS
// Also, due to how the game is implemented, high MAX_BALL_COUNT will start showing unpredictable MAX_BALL_TICK_COUNT.

#define MAX_BALL_COUNT 1000 // How many balls will be simulated.
#define NEW_BALL_TICK_COUNT 4U // How many ticks between each ball falling.
#define DEBOUNCE_TIME_MS 200U // How long to wait for the bouncing to stop in ms.
#define OLED_REFRESH_RATE_MS 2U // How often (in ms) the OLED will be refreshed per second.
#define GAME_TICK_MS 10U // How often (in ms) game logic be processed per second.


#if MAX_BALL_COUNT < 10
    #define NUM_DIGITS 1
#elif MAX_BALL_COUNT < 100
    #define NUM_DIGITS 2
#elif MAX_BALL_COUNT < 1000
    #define NUM_DIGITS 3
#elif MAX_BALL_COUNT < 10000
    #define NUM_DIGITS 4
#else
    #define NUM_DIGITS 5
#endif

#define HISTOGRAM_LEVELS 6U

typedef enum{
    START,
    RUN,
    END,
    RESET,
} gamestate_t; // Possible states of my program


typedef enum {
    DEBOUNCE_RISE,
    DEBOUNCE_FALL,
} debounce_state_t; // Possible states of the debouncing process.
volatile debounce_state_t debounce_state = DEBOUNCE_FALL;


typedef enum{
    NONE,
    LEFT,
    RIGHT
} direction_t; // Possible directions ball can move.

typedef struct{
    sprite_t sprite; // Contains info about sprite position, width, height, and points to associated bitmap.
    uint8_t v_y; // UNUSED
    uint8_t v_x; // UNUSED
    bool collision_flag; // Did I impact a pin? Set when collision checking.
    bool in_game; // Is this ball ingame?
    direction_t mov_direction;
} ball_t; // Struct containing information about the balls

const int32_t oled_refresh_rate_ms = OLED_REFRESH_RATE_MS;
const int32_t game_timer_rate_ms = GAME_TICK_MS;

const uint I2C_SDA = OLED_SDA;
const uint I2C_SCL = OLED_SCL;

// Volatile variables
volatile bool refresh_oled = false; // Controlled by oled_refresh_rate_ms
volatile bool game_tick = false; // Controlled by game_timer_rate_ms
volatile bool render_normalized_histogram = true;
volatile uint32_t game_tick_count; // Global game_tick counter
volatile uint16_t balls_ingame; // How many balls are ingame.
volatile gamestate_t gamestate;

ball_t galton_board_ball[MAX_BALL_COUNT]; // Contains data about every ball that has been rendered.
uint16_t ball_count = 0; 
uint16_t total_histogram_values[HISTOGRAM_LEVELS]; // Total histogram values, to be displayed at the end.
uint16_t histogram_display_values[HISTOGRAM_LEVELS]; // Histogram values to be update real time on the OLED.
char histogram_display_char[HISTOGRAM_LEVELS][NUM_DIGITS]; // Used for transforming the histogram data into characters.
uint16_t normalized_histogram[HISTOGRAM_LEVELS];
float temp_normalized_histogram[HISTOGRAM_LEVELS];

uint32_t rand_32_midvalue;
uint16_t joystick_samples[2] __attribute__((aligned(4))); // Memory aligned in order to comply with free-running requirements.
uint dma_chan;
bool flag_joystick = true;


/* Functions */

// Turns a 12 bit value (for example, ADC INPUT) into a 32 bit value.
uint32_t map_12bit_to_32bit(uint16_t input) {
    assert(input >= 1 && input <= 4096);// Since ADC has a 2^12 = 4096 resolution
    // Prevent overflow during calculation. Suggested by GPT.
    uint64_t scaled = (uint64_t)input * 0xFFFFFFFF;  // 2^32 - 1
    uint32_t result = (uint32_t)(scaled / 4095);      // 2^12 - 1
    return (result == 0) ? 1 : result; // Ensures output is in [1, 2^32]
}

/* IRQs and Callbacks */

// Establish OLED refresh rate.
bool refresh_rate_callback(struct repeating_timer *t){
    refresh_oled = true;
    return true;
}

// Establish game tick.
bool game_tick_callback(struct repeating_timer *t){
    game_tick_count++;
    game_tick = true;
    return true;
}

// Callback to re-enable button B after it has been debounced.
int64_t debounce_callback(alarm_id_t id, __unused void *user_data){
    if(DEBOUNCE_FALL == debounce_state){
        gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_RISE, true);
        gpio_set_irq_enabled(SWITCH, GPIO_IRQ_EDGE_RISE, true);
    }else if(DEBOUNCE_RISE == debounce_state){
        gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
        gpio_set_irq_enabled(SWITCH, GPIO_IRQ_EDGE_FALL, true);
    }
    return 0;
}

// Resets the DMA channel after it finished transmission.
void dma_handler1(){    
    (dma_hw->ints0) = 1u << dma_chan;
    dma_start_channel_mask(1u << dma_chan);
}

// Debounces button B (Used to enable or disable joystick) and permits button A (Reset) to function.
void gpio_irq_callback(uint gpio, uint32_t events){
    if(gpio == BOTAO_A){
        gamestate = RESET; // Reset the game.
    }
    if((gpio == BOTAO_B)){ // Enable or disable joystick
        if(events & GPIO_IRQ_EDGE_FALL){
            gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, false);
            add_alarm_in_ms(DEBOUNCE_TIME_MS, debounce_callback, NULL, false);
            debounce_state = DEBOUNCE_FALL;
            flag_joystick = !flag_joystick;
        } else if(events & GPIO_IRQ_EDGE_RISE){
            gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_RISE, false);
            add_alarm_in_ms(DEBOUNCE_TIME_MS, debounce_callback, NULL, false);
            debounce_state = DEBOUNCE_RISE;
        }
    }
    if((gpio == SWITCH)){ // Enable or disable joystick
        if(events & GPIO_IRQ_EDGE_FALL){
            gpio_set_irq_enabled(SWITCH, GPIO_IRQ_EDGE_FALL, false);
            add_alarm_in_ms(DEBOUNCE_TIME_MS, debounce_callback, NULL, false);
            debounce_state = DEBOUNCE_FALL;
            render_normalized_histogram = !render_normalized_histogram;
        } else if(events & GPIO_IRQ_EDGE_RISE){
            gpio_set_irq_enabled(SWITCH, GPIO_IRQ_EDGE_RISE, false);
            add_alarm_in_ms(DEBOUNCE_TIME_MS, debounce_callback, NULL, false);
            debounce_state = DEBOUNCE_RISE;
        }
    }
}

int main()
{
    /* Initialization */

    stdio_init_all(); // Enable stdio for testing

    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Check ssd1306_rp2040_i2c header for details.
    ssd1306_init();

    // Setup timer for refresing the OLED.
    struct repeating_timer oled_fps;
    add_repeating_timer_ms(oled_refresh_rate_ms, refresh_rate_callback, NULL, &oled_fps);

    // Setup timer for the game's tick.
    struct repeating_timer game_timer;
    add_repeating_timer_ms(game_timer_rate_ms, game_tick_callback, NULL,&game_timer);

    // Setup joystick for biasing the RNG. Check bitdoglab header for details.
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);
    joystick_init(joystick_samples, (irq_handler_t)dma_handler1, dma_chan, dma_cfg);
    
    // Setup buttons for input
    gpio_init(BOTAO_A);
    gpio_init(BOTAO_B);
    gpio_init(SWITCH);
    gpio_pull_up(BOTAO_A);
    gpio_pull_up(BOTAO_B);
    gpio_pull_up(SWITCH);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_set_dir(SWITCH, GPIO_IN);

    // Enable GPIO IRQ.
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_callback);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(SWITCH, GPIO_IRQ_EDGE_FALL, true);

    // START GAME
    gamestate = START;
    while(true) {
        // Send the bitmap info for the three distinct regions of the OLED: GALTON, HISTOGRAM, MENU.
        if(true == refresh_oled){ // Once OLED_REFRESH_RATE_MS time has passed, send the bitmap to the SSD1306.
            render_on_display(&galton_render_region);
            render_on_display(&histogram_render_region);
            render_on_display(&menu_render_region);
            refresh_oled = false;
        }

        if(true == game_tick){ // Every tick, process game logic
            switch(gamestate){ // Can run in four different states: START, RESET, RUN, END.
                case START: // Currently, START does the same process as RESET.
                case RESET:

                    // Clean galton, histogram and menu memory buffer.
                    memset(galton_render_region.bitmap, 0, (galton_render_region.buffer_length-1) - SSD1306_PAGE_COUNT);
                    memset(histogram_render_region.bitmap, 0, histogram_render_region.buffer_length-1);
                    memset(menu_render_region.bitmap, 0, menu_render_region.buffer_length-1);

                    // Redraw Galton Pins.
                    for(uint i = 0; i < count_of(galton_board); i++){
                        ssd1306_draw_sprite(&galton_render_region, &galton_board[i], false);
                    }

                    // Reset storage variables
                    memset(&total_histogram_values, 0, sizeof(total_histogram_values));
                    memset(&histogram_display_values, 0, sizeof(histogram_display_values));
                    ball_count = 0; 

                    // Return to RUN gamestate
                    gamestate = RUN;
                    break;

                case RUN:

                    /* BASIC CHECKS */
                    // Remove old balls from OLED memory, since we're going to update them.
                    for(uint i = 0; i < ball_count; i++){
                        if(true == galton_board_ball[i].in_game){
                            ssd1306_remove_sprite(&galton_render_region, &galton_board_ball[i].sprite, false);
                        }
                    }
                    
                    // Crate a new ball every NEW_BALL_TICK_COUNT ticks. Note, this updates the total ball_count variable.
                    if((game_tick_count % NEW_BALL_TICK_COUNT == 0)&&(ball_count < MAX_BALL_COUNT)){
                        galton_board_ball[ball_count] = (ball_t){
                            game_ball,
                            0,
                            0,
                            false,
                            true,
                            NONE
                        }; // Create new ball in board
                        galton_board_ball[ball_count].sprite.x_pos = 0; // Center its sprite
                        galton_board_ball[ball_count].sprite.y_pos = 33;
                        ball_count++;
                    }

                    // If joystick is ON, check for its x coordinate value.
                    if(flag_joystick){
                        uint16_t temp_vry = joystick_samples[0];
                        uint16_t temp_vrx = joystick_samples[1];
                        rand_32_midvalue = map_12bit_to_32bit(temp_vrx);
                    } else {
                        rand_32_midvalue = (1<<31);
                    }

                    /* BALL MOVEMENT */
                    // Note, this is executed for EVERY BALL that has been rendered. For high ball counts, this leads to slow down.
                        for(uint i = 0; i < ball_count; i++){
                            if(true == galton_board_ball[i].in_game){
                                // Make the ball fall down.
                                galton_board_ball[i].sprite.x_pos++;

                                // Check if the ball's new position hits a pin in the galton_board.     
                                // First, I check if the ball's hitbox has had any contact with the pin's hitbox. (Since we're falling only in a single direction, I only check for my ball's right side.).

                                uint8_t x_pos = galton_board_ball[i].sprite.x_pos;
                                uint8_t y_pos = galton_board_ball[i].sprite.y_pos;
                                uint8_t x_hitbox =  x_pos + galton_board_ball[i].sprite.width;
                                uint8_t y_hitbox = y_pos + galton_board_ball[i].sprite.height;
                                for( uint pin = 0; pin < NUM_PINS; pin++){
                                    if((x_hitbox >= galton_board[pin].x_pos)&&(y_pos >= galton_board[pin].y_pos)
                                    &&(y_pos <= galton_board[pin].y_pos + galton_board[pin].height)
                                    &&(x_pos <= galton_board[pin].x_pos + galton_board[pin].width)){ 
                                        // If it hit the pin, enable the collision flag.
                                        galton_board_ball[i].collision_flag = true;

                                        // The random number is seeded by the internal Ring Oscillator.
                                        // If joystick is ON, do a comparison with the value given by the joystick.
                                        // If joystick is OFF, check only the MSB (most significant bit) of the uint32_t it generated. If 1, RIGHT. If 0, LEFT.
                                        if(flag_joystick){
                                            galton_board_ball[i].mov_direction = (get_rand_32() > rand_32_midvalue)?(RIGHT):(LEFT);
                                        }else{
                                            galton_board_ball[i].mov_direction = (get_rand_32() & rand_32_midvalue)?(RIGHT):(LEFT);
                                        }
                                        break; // If a collision was detected, GET OUT of the for loop for this specific ball.
                                    }
                                }

                                // If ball collided with a pin, move it in the direction determined previously.
                                if(true == galton_board_ball[i].collision_flag){
                                    if(RIGHT == galton_board_ball[i].mov_direction){
                                        galton_board_ball[i].sprite.y_pos -= 5;
                                    } else {
                                        galton_board_ball[i].sprite.y_pos += 5;
                                    }
                                    galton_board_ball[i].collision_flag = false;
                                    galton_board_ball[i].mov_direction = NONE;
                                }

                                // If ball reached the end of the GALTON BOARD, it's no longer in game and the histogram will be updated. 
                                if(galton_board_ball[i].sprite.x_pos + galton_board_ball[i].sprite.width >= galton_render_region.end_column - 1 ){
                                    total_histogram_values[galton_board_ball[i].sprite.y_pos / 10] += 1; // Writes directly to the corresponding address.
                                    histogram_display_values[galton_board_ball[i].sprite.y_pos / 10] += 1;
                                    galton_board_ball[i].in_game = false;
                                }
                            }
                        }

                    /* BALL RENDERING */
                    // Updates ball position in my internal bitmap.

                    for(uint i = 0; i < ball_count; i++){
                        if(true == galton_board_ball[i].in_game){
                            ssd1306_draw_sprite(&galton_render_region, &galton_board_ball[i].sprite, false);
                        }
                    }

                    /* HISTOGRAM RENDERING */
                    // The histogram is rendered by writing my histogram bar sprite on different coordinates of the HISTOGRAM REGION. There's only one sprite, whose x and y position is varied accordingly.
                    uint8_t bar_pos;
                    uint16_t height_hist;
                    for(uint i = 0; i < HISTOGRAM_LEVELS; i++){ 

                        if(histogram_display_values[i] >= histogram_render_region.width-1){
                            // If I hit the ceiling of my HISTOGRAM REGION, flash screen and restart display. Also,
                            for(uint k = 0; k < HISTOGRAM_LEVELS; k++){
                                histogram_display_values[k] = 0;
                                bar_histogram.x_pos = 0;
                                bar_histogram.y_pos = 0;
                                ssd1306_blink(true);
                                sleep_ms(15);
                                ssd1306_blink(false);
                                memset(histogram_render_region.bitmap, 0, histogram_render_region.buffer_length-1); //  Restart display.
                            }
                            break;
                        } else if(histogram_display_values[i] != 0) {
                            bar_pos = histogram_render_region.width - histogram_display_values[i] - 1; 
                            bar_histogram.x_pos = bar_pos;
                            bar_histogram.y_pos = 7 + 10*i; // These magic numbers refer to WHERE i want to place the bar. They're set with their origin in multiples of 10, starting in 7.
                            ssd1306_draw_sprite(&histogram_render_region, &bar_histogram, false);
                        }
                    }

                    /* MENU RENDERING */
                    // In the menu, we write the status of flag_joystick (as either ON or OFF).
                    // We also write ball_count on the MENU.
                    if(flag_joystick){
                        // Draw the O
                        nes_font[10].x_pos = 1;
                        nes_font[10].y_pos = 56;
                        ssd1306_draw_sprite(&menu_render_region, &nes_font[10], false);

                        // Draw the N
                        nes_font[11].x_pos = 1;
                        nes_font[11].y_pos = 48;
                        ssd1306_draw_sprite(&menu_render_region, &nes_font[11], false);

                        // Delete the F
                        nes_font[12].x_pos = 1;
                        nes_font[12].y_pos = 40;
                        ssd1306_remove_sprite(&menu_render_region, &nes_font[12], false);
                    } else {
                        // Draw the O
                        nes_font[10].x_pos = 1;
                        nes_font[10].y_pos = 56;
                        ssd1306_draw_sprite(&menu_render_region, &nes_font[10], false);

                        // Draw the F
                        nes_font[12].x_pos = 1;
                        nes_font[12].y_pos = 48;
                        ssd1306_draw_sprite(&menu_render_region, &nes_font[12], false);

                        // Draw the F
                        nes_font[12].x_pos = 1;
                        nes_font[12].y_pos = 40;
                        ssd1306_draw_sprite(&menu_render_region, &nes_font[12], false);
                    }
                    char ball_count_char[NUM_DIGITS];
                    uint16_t temp_ball_count = ball_count; // Convert ball_count to a char.
                    for(uint i = 0; i < count_of(ball_count_char); i++){
                        ball_count_char[i] = '0' + temp_ball_count % 10;
                        temp_ball_count = temp_ball_count / 10;
                    }

                    // Write the char in the memory bitmap.
                    for(int i = count_of(ball_count_char)-1; i >= 0; i--){
                        uint number_from_char = ball_count_char[i] - '0';
                        nes_font[number_from_char].x_pos = 1;
                        nes_font[number_from_char].y_pos = (40-8*count_of(ball_count_char)) + i*nes_font[number_from_char].height;
                        ssd1306_draw_sprite(&menu_render_region, &nes_font[number_from_char], false);
                    }

                    // Has game ended? To know, check if the number of balls ingame is 0. If so, END game.
                    if(ball_count == MAX_BALL_COUNT){
                        balls_ingame = 0;

                        for(uint i = 0; i < ball_count; i++){
                            balls_ingame += (int)galton_board_ball[i].in_game;
                        }

                        if(balls_ingame == 0){
                            gamestate = END;
                        }
                    }
                    break;

                case END:
                    // Draw numbers on histogram render area.
                    // Obtain numbers for every histogram slot in character form.
                    memset(histogram_render_region.bitmap, 0, histogram_render_region.buffer_length);

                    if(render_normalized_histogram){
                        float total_histogram_val = 0;

                        for(uint i = 0; i < HISTOGRAM_LEVELS; i++){
                            total_histogram_val = total_histogram_val + total_histogram_values[i];
                            temp_normalized_histogram[i] = total_histogram_values[i];
                        }

                        for(uint i = 0; i < HISTOGRAM_LEVELS; i++){
                            normalized_histogram[i] = ((temp_normalized_histogram[i] / total_histogram_val)*(57.0));
                        }

                        bar_pos = 0;
                        for(uint i = 0; i < HISTOGRAM_LEVELS; i++){ 
                            if(normalized_histogram[i] != 0) {
                                while(bar_pos < normalized_histogram[i]){
                                    bar_histogram.x_pos = histogram_render_region.width - 1 - bar_pos;
                                    bar_histogram.y_pos = 7 + 10*i; // These magic numbers refer to WHERE i want to place the bar. They're set with their origin in multiples of 10, starting in 7.
                                    ssd1306_draw_sprite(&histogram_render_region, &bar_histogram, false);
                                    bar_pos++;
                                }
                                bar_pos = 0;
                            }
                        }

                    } else {
                        for(uint i = 0; i < HISTOGRAM_LEVELS; i++){
                            uint16_t temp_histogram_count = total_histogram_values[i]; // Realizo a conversão de inteiro à caractere, e escrevo os dados numa matriz.
                            for(int k = 0; k < count_of(histogram_display_char[0]); k++){
                                histogram_display_char[i][k] = '0' + temp_histogram_count % 10;
                                temp_histogram_count = temp_histogram_count / 10;
                            }
    
                            for(int k = 0; k < count_of(histogram_display_char[0]); k++){
                                uint number_from_char = histogram_display_char[i][k] - '0';
                                nes_font[number_from_char].x_pos = 1 + (count_of(histogram_display_char) - i - 1)*9;
                                nes_font[number_from_char].y_pos = (48-8*count_of(histogram_display_char[0])) + k*nes_font[0].height;
                                ssd1306_draw_sprite(&histogram_render_region, &nes_font[number_from_char], false);
                            }
    
                        }
                    }
                    break;
            }
            game_tick = false; // A game tick has passed. If any other game tick is set during execution, nothing should happen.
        }
    }
    return 0;
}
