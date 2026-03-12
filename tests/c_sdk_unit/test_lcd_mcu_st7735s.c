#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "st7789.h"

typedef struct {
    char kind;
    uint8_t cmd;
    uint8_t data[16];
    uint32_t len;
} lcd_op_t;

#define MAX_OPS 512

static lcd_op_t g_ops[MAX_OPS];
static size_t g_op_count = 0;
static uint8_t g_last_cmd = 0;

volatile int (*dual_func)(int) = 0;
volatile bool maixpy_sdcard_loading = false;

static void mock_reset(void)
{
    memset(g_ops, 0, sizeof(g_ops));
    g_op_count = 0;
    g_last_cmd = 0;
}

static void push_cmd(uint8_t cmd)
{
    assert(g_op_count < MAX_OPS);
    g_ops[g_op_count].kind = 'C';
    g_ops[g_op_count].cmd = cmd;
    g_ops[g_op_count].len = 0;
    g_op_count++;
}

static void push_data(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    assert(g_op_count < MAX_OPS);
    g_ops[g_op_count].kind = 'D';
    g_ops[g_op_count].cmd = cmd;
    g_ops[g_op_count].len = len;
    if (data != NULL && len > 0)
    {
        uint32_t copy_len = (len > sizeof(g_ops[g_op_count].data)) ? sizeof(g_ops[g_op_count].data) : len;
        memcpy(g_ops[g_op_count].data, data, copy_len);
    }
    g_op_count++;
}

static int find_cmd_index(uint8_t cmd)
{
    size_t i;
    for (i = 0; i < g_op_count; ++i)
    {
        if (g_ops[i].kind == 'C' && g_ops[i].cmd == cmd)
            return (int)i;
    }
    return -1;
}

static const lcd_op_t *find_first_data_for_cmd(uint8_t cmd)
{
    size_t i;
    for (i = 0; i < g_op_count; ++i)
    {
        if (g_ops[i].kind == 'D' && g_ops[i].cmd == cmd)
            return &g_ops[i];
    }
    return NULL;
}

static bool has_cmd(uint8_t cmd)
{
    return find_cmd_index(cmd) >= 0;
}

void tft_hard_init(uint32_t freq, bool oct)
{
    (void)freq;
    (void)oct;
}

void tft_set_clk_freq(uint32_t freq)
{
    (void)freq;
}

void tft_write_command(uint8_t cmd)
{
    g_last_cmd = cmd;
    push_cmd(cmd);
}

void tft_write_byte(uint8_t *data_buf, uint32_t length)
{
    push_data(g_last_cmd, data_buf, length);
}

void tft_write_half(uint16_t *data_buf, uint32_t length)
{
    push_data(g_last_cmd, (const uint8_t *)data_buf, length * 2);
}

void tft_write_word(uint32_t *data_buf, uint32_t length)
{
    push_data(g_last_cmd, (const uint8_t *)data_buf, length * 4);
}

void tft_fill_data(uint32_t *data_buf, uint32_t length)
{
    push_data(g_last_cmd, (const uint8_t *)data_buf, length * 4);
}

void msleep(uint32_t ms)
{
    (void)ms;
}

int printk(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    (void)fmt;
    va_end(ap);
    return 0;
}

static lcd_para_t make_default_para(lcd_type_t type)
{
    lcd_para_t p;
    memset(&p, 0, sizeof(p));
    p.freq = 15000000;
    p.height = 160;
    p.width = 128;
    p.lcd_type = type;
    p.oct = false;
    p.dir = DIR_YX_RLDU;
    p.invert = false;
    return p;
}

static void test_st7735s_uses_pixel_format_0x05(void)
{
    lcd_para_t p = make_default_para(LCD_TYPE_ST7735S);
    const lcd_op_t *pixel_fmt;

    mock_reset();
    assert(lcd_mcu.init(&p) == 0);

    pixel_fmt = find_first_data_for_cmd(PIXEL_FORMAT_SET);
    assert(pixel_fmt != NULL);
    assert(pixel_fmt->len >= 1);
    assert(pixel_fmt->data[0] == 0x05);

    lcd_mcu.deinit();
}

static void test_st7789_uses_pixel_format_0x55(void)
{
    lcd_para_t p = make_default_para(LCD_TYPE_ST7789);
    const lcd_op_t *pixel_fmt;

    mock_reset();
    assert(lcd_mcu.init(&p) == 0);

    pixel_fmt = find_first_data_for_cmd(PIXEL_FORMAT_SET);
    assert(pixel_fmt != NULL);
    assert(pixel_fmt->len >= 1);
    assert(pixel_fmt->data[0] == 0x55);

    lcd_mcu.deinit();
}

static void test_st7735s_preinit_runs_before_sleep_off(void)
{
    lcd_para_t p = make_default_para(LCD_TYPE_ST7735S);
    int idx_reset;
    int idx_st7735_b1;
    int idx_sleep_off;

    mock_reset();
    assert(lcd_mcu.init(&p) == 0);

    idx_reset = find_cmd_index(SOFTWARE_RESET);
    idx_st7735_b1 = find_cmd_index(0xB1);
    idx_sleep_off = find_cmd_index(SLEEP_OFF);

    assert(idx_reset >= 0);
    assert(idx_st7735_b1 > idx_reset);
    assert(idx_sleep_off > idx_st7735_b1);
    assert(has_cmd(0xE1));

    lcd_mcu.deinit();
}

static void test_preinit_handler_does_not_leak_to_st7789(void)
{
    lcd_para_t st7735 = make_default_para(LCD_TYPE_ST7735S);
    lcd_para_t st7789 = make_default_para(LCD_TYPE_ST7789);

    mock_reset();
    assert(lcd_mcu.init(&st7735) == 0);
    lcd_mcu.deinit();

    mock_reset();
    assert(lcd_mcu.init(&st7789) == 0);

    assert(!has_cmd(0xB1));
    assert(!has_cmd(0xB2));
    assert(!has_cmd(0xB3));

    lcd_mcu.deinit();
}

int main(void)
{
    test_st7735s_uses_pixel_format_0x05();
    test_st7789_uses_pixel_format_0x55();
    test_st7735s_preinit_runs_before_sleep_off();
    test_preinit_handler_does_not_leak_to_st7789();

    printf("lcd_mcu unit tests passed (4/4)\n");
    return 0;
}
