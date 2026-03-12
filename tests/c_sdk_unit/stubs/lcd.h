#ifndef TEST_LCD_H
#define TEST_LCD_H

#include <stdbool.h>
#include <stdint.h>

#define LCD_SWAP_COLOR_BYTES 1
#define SWAP_16(x) (((x >> 8) & 0xFFu) | ((x << 8) & 0xFF00u))

static const uint16_t gray2rgb565[64] = {0};

typedef enum {
    LCD_TYPE_ST7789 = 0,
    LCD_TYPE_ILI9486 = 1,
    LCD_TYPE_ILI9481 = 2,
    LCD_TYPE_5P0_7P0 = 3,
    LCD_TYPE_5P0_IPS = 4,
    LCD_TYPE_480_272_4P3 = 5,
    LCD_TYPE_ST7735S = 6,
} lcd_type_t;

typedef enum _lcd_dir {
    DIR_XY_RLUD = 0x00,
    DIR_YX_RLUD = 0x20,
    DIR_XY_LRUD = 0x40,
    DIR_YX_LRUD = 0x60,
    DIR_XY_RLDU = 0x80,
    DIR_YX_RLDU = 0xA0,
    DIR_XY_LRDU = 0xC0,
    DIR_YX_LRDU = 0xE0,
    DIR_XY_MASK = 0x20,
    DIR_MASK = 0xE0,
    DIR_RGB2BRG = 0x08,
} lcd_dir_t;

typedef struct {
    uint8_t rst_pin;
    uint8_t dcx_pin;
    uint8_t cs_pin;
    uint8_t bl_pin;
    uint8_t clk_pin;
    uint32_t freq;
    uint16_t height;
    uint16_t width;
    uint16_t offset_h0;
    uint16_t offset_w0;
    uint16_t offset_h1;
    uint16_t offset_w1;
    lcd_type_t lcd_type;
    bool oct;
    lcd_dir_t dir;
    bool invert;
    void *extra_para;
} lcd_para_t;

typedef struct {
    lcd_para_t *lcd_para;
    int (*init)(lcd_para_t *lcd_para);
    void (*deinit)(void);
    void (*set_direction)(lcd_dir_t dir);
    void (*bgr_to_rgb)(bool enable);
    void (*set_offset)(uint16_t offset_w, uint16_t offset_h);
    void (*set_freq)(uint32_t freq);
    uint32_t (*get_freq)(void);
    uint16_t (*get_width)(void);
    uint16_t (*get_height)(void);
    void (*clear)(uint16_t rgb565_color);
    void (*draw_point)(uint16_t x, uint16_t y, uint16_t color);
    void (*draw_picture)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *img);
    void (*draw_pic_roi)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rx, uint16_t ry, uint16_t rw, uint16_t rh, uint8_t *img);
    void (*draw_pic_gray)(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h, uint8_t *img);
    void (*draw_pic_grayroi)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rx, uint16_t ry, uint16_t rw, uint16_t rh, uint8_t *img);
    void (*fill_rectangle)(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
} lcd_t;

extern lcd_t lcd_mcu;

#endif
