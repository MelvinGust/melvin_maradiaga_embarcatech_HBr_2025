#include "ssd1306_program_sprites.h"

render_region_t galton_render_region = {
    SSD1306_COORDS_X_ORIGIN,
    SSD1306_COORDS_X_ORIGIN + count_of(galton_bitmap)/SSD1306_PAGE_COUNT - 1,
    SSD1306_COORDS_Y_ORIGIN,
    SSD1306_PAGE_COUNT - 1,
    count_of(galton_bitmap)/SSD1306_PAGE_COUNT,
    64,
    galton_bitmap,
    sizeof(galton_bitmap) / sizeof(galton_bitmap[0]),
};

render_region_t histogram_render_region = {
    SSD1306_COORDS_X_ORIGIN + count_of(galton_bitmap)/SSD1306_PAGE_COUNT,
    SSD1306_COORDS_X_ORIGIN + count_of(galton_bitmap)/SSD1306_PAGE_COUNT + count_of(histogram_bitmap)/SSD1306_PAGE_COUNT - 1,
    SSD1306_COORDS_Y_ORIGIN,
    SSD1306_PAGE_COUNT - 1,
    count_of(histogram_bitmap)/SSD1306_PAGE_COUNT,
    64,
    histogram_bitmap,
    sizeof(histogram_bitmap) / sizeof(histogram_bitmap[0]),
};

render_region_t menu_render_region = {
    SSD1306_COORDS_X_ORIGIN + count_of(galton_bitmap)/SSD1306_PAGE_COUNT + count_of(histogram_bitmap)/SSD1306_PAGE_COUNT,
    SSD1306_COORDS_X_ORIGIN + count_of(galton_bitmap)/SSD1306_PAGE_COUNT + count_of(histogram_bitmap)/SSD1306_PAGE_COUNT + count_of(menu_bitmap)/SSD1306_PAGE_COUNT - 1,
    SSD1306_COORDS_Y_ORIGIN,    
    SSD1306_PAGE_COUNT - 1,
    count_of(menu_bitmap)/SSD1306_PAGE_COUNT,
    64,
    menu_bitmap,
    sizeof(menu_bitmap) / sizeof(menu_bitmap[0]),
};

sprite_t galton_board[NUM_PINS] = {
    {9, 32, pin_width, pin_height, pin_bitmap},
    {19, 27, pin_width, pin_height, pin_bitmap},
    {19, 37, pin_width, pin_height, pin_bitmap},
    {29, 22, pin_width, pin_height, pin_bitmap},
    {29, 32, pin_width, pin_height, pin_bitmap},
    {29, 42, pin_width, pin_height, pin_bitmap},
    {39, 17, pin_width, pin_height, pin_bitmap},
    {39, 27, pin_width, pin_height, pin_bitmap},
    {39, 37, pin_width, pin_height, pin_bitmap},
    {39, 47, pin_width, pin_height, pin_bitmap},
    {49, 12, pin_width, pin_height, pin_bitmap},
    {49, 22, pin_width, pin_height, pin_bitmap},
    {49, 32, pin_width, pin_height, pin_bitmap},
    {49, 42, pin_width, pin_height, pin_bitmap},
    {49, 52, pin_width, pin_height, pin_bitmap},
};

sprite_t nes_font[13] = {
    {0, 0, font_width, font_height, FONT_NES_0},
    {0, 0, font_width, font_height, FONT_NES_1},
    {0, 0, font_width, font_height, FONT_NES_2},
    {0, 0, font_width, font_height, FONT_NES_3},
    {0, 0, font_width, font_height, FONT_NES_4},
    {0, 0, font_width, font_height, FONT_NES_5},
    {0, 0, font_width, font_height, FONT_NES_6},
    {0, 0, font_width, font_height, FONT_NES_7},
    {0, 0, font_width, font_height, FONT_NES_8},
    {0, 0, font_width, font_height, FONT_NES_9},
    {0, 0, font_width, font_height, FONT_NES_O},
    {0, 0, font_width, font_height, FONT_NES_N},
    {0, 0, font_width, font_height, FONT_NES_F},
};

sprite_t game_ball = {
    0,
    0,
    ball_width,
    ball_height,
    ball_bitmap
};

sprite_t bar_histogram = {
    0,
    0,
    bar_width,
    bar_height,
    bar_bitmap
};
