#ifndef TEST_ST7789_H
#define TEST_ST7789_H

#include <stdbool.h>
#include <stdint.h>

#define SOFTWARE_RESET         0x01
#define SLEEP_OFF              0x11
#define NORMAL_DISPALY_ON      0x13
#define INVERSION_DISPALY_ON   0x21
#define DISPALY_ON             0x29
#define HORIZONTAL_ADDRESS_SET 0x2A
#define VERTICAL_ADDRESS_SET   0x2B
#define MEMORY_WRITE           0x2C
#define MEMORY_ACCESS_CTL      0x36
#define PIXEL_FORMAT_SET       0x3A

void tft_hard_init(uint32_t freq, bool oct);
void tft_set_clk_freq(uint32_t freq);
void tft_write_command(uint8_t cmd);
void tft_write_byte(uint8_t *data_buf, uint32_t length);
void tft_write_half(uint16_t *data_buf, uint32_t length);
void tft_write_word(uint32_t *data_buf, uint32_t length);
void tft_fill_data(uint32_t *data_buf, uint32_t length);

#endif
