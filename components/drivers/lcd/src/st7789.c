#include "st7789.h"
#include "sipeed_spi.h"
#include "sleep.h"
#include "lcd.h"

static bool standard_spi = false;
static spi_work_mode_t standard_work_mode = SPI_WORK_MODE_0;
static bool spi_cfg_valid = false;
static bool spi_nonstd_cfg_valid = false;
static spi_work_mode_t last_work_mode;
static uint8_t last_frame_format;
static uint8_t last_data_bits;
static uint8_t last_instr_len;
static uint8_t last_addr_len;
static uint8_t last_wait_cycles;

static void tft_config_transfer(uint8_t frame_format, uint8_t data_bits, uint8_t instr_len, uint8_t addr_len, uint8_t wait_cycles)
{
    spi_work_mode_t mode = standard_spi ? standard_work_mode : SPI_WORK_MODE_0;

    if (!spi_cfg_valid || last_work_mode != mode || last_frame_format != frame_format || last_data_bits != data_bits)
    {
        spi_init(SPI_CHANNEL, mode, frame_format, data_bits, 0);
        last_work_mode = mode;
        last_frame_format = frame_format;
        last_data_bits = data_bits;
        spi_cfg_valid = true;
        spi_nonstd_cfg_valid = false;
    }

    if (!standard_spi)
    {
        if (!spi_nonstd_cfg_valid || last_instr_len != instr_len || last_addr_len != addr_len || last_wait_cycles != wait_cycles)
        {
            spi_init_non_standard(SPI_CHANNEL, instr_len, addr_len, wait_cycles,
                                  SPI_AITM_AS_FRAME_FORMAT);
            last_instr_len = instr_len;
            last_addr_len = addr_len;
            last_wait_cycles = wait_cycles;
            spi_nonstd_cfg_valid = true;
        }
    }
}

static void init_dcx(void)
{
    gpiohs_set_drive_mode(DCX_GPIONUM, GPIO_DM_OUTPUT);
    gpiohs_set_pin(DCX_GPIONUM, GPIO_PV_HIGH);
}

static void set_dcx_control(void)
{
    gpiohs_set_pin(DCX_GPIONUM, GPIO_PV_LOW);
}

static void set_dcx_data(void)
{
    gpiohs_set_pin(DCX_GPIONUM, GPIO_PV_HIGH);
}

static void init_rst(void)
{
    gpiohs_set_drive_mode(RST_GPIONUM, GPIO_DM_OUTPUT);
    gpiohs_set_pin(RST_GPIONUM, GPIO_PV_HIGH);
}

static void set_rst(uint8_t val)
{
    gpiohs_set_pin(RST_GPIONUM, val);
}

void tft_set_clk_freq(uint32_t freq)
{
    spi_set_clk_rate(SPI_CHANNEL, freq);
}

void tft_hard_init(uint32_t freq, bool oct)
{
    standard_spi = !oct;
    spi_cfg_valid = false;
    spi_nonstd_cfg_valid = false;
    init_dcx();
    init_rst();
    set_rst(0);
    if(standard_spi)
    {
        /* Init SPI IO map and function settings */
        // sysctl_set_spi0_dvp_data(1);
        tft_config_transfer(SPI_FF_STANDARD, 8, 0, 0, 0);
    }
    else
    {
        /* Init SPI IO map and function settings */
        // sysctl_set_spi0_dvp_data(0);
        tft_config_transfer(SPI_FF_OCTAL, 8, 8, 0, 0);
    }
    tft_set_clk_freq(freq);
    msleep(50);
    set_rst(1);
    msleep(50);
}

void tft_write_command(uint8_t cmd)
{
    set_dcx_control();
    if(!standard_spi)
    {
        tft_config_transfer(SPI_FF_OCTAL, 8, 8, 0, 0);
        spi_send_data_normal_dma(SPI_DMA_CH, SPI_CHANNEL, LCD_SPI_SLAVE_SELECT, (uint8_t *)(&cmd), 1, SPI_TRANS_CHAR);
    }
    else
    {
        tft_config_transfer(SPI_FF_STANDARD, 8, 0, 0, 0);
        spi_send_data_normal_dma(SPI_DMA_CH, SPI_CHANNEL, LCD_SPI_SLAVE_SELECT, (uint8_t *)(&cmd), 1, SPI_TRANS_CHAR);
    }
}

void tft_write_byte(uint8_t *data_buf, uint32_t length)
{
    set_dcx_data();
    if(!standard_spi)
    {
        tft_config_transfer(SPI_FF_OCTAL, 8, 0, 8, 0);
        spi_send_data_normal_dma(SPI_DMA_CH, SPI_CHANNEL, LCD_SPI_SLAVE_SELECT, data_buf, length, SPI_TRANS_CHAR);
    }
    else
    {
        tft_config_transfer(SPI_FF_STANDARD, 8, 0, 0, 0);
        spi_send_data_normal_dma(SPI_DMA_CH, SPI_CHANNEL, LCD_SPI_SLAVE_SELECT, data_buf, length, SPI_TRANS_CHAR);
    }
}

void tft_write_half(uint16_t *data_buf, uint32_t length)
{
    set_dcx_data();
    if(!standard_spi)
    {
        tft_config_transfer(SPI_FF_OCTAL, 16, 0, 16, 0);
        spi_send_data_normal_dma(SPI_DMA_CH, SPI_CHANNEL, LCD_SPI_SLAVE_SELECT, data_buf, length, SPI_TRANS_SHORT);
    }
    else
    {
        tft_config_transfer(SPI_FF_STANDARD, 16, 0, 0, 0);
        spi_send_data_normal_dma(SPI_DMA_CH, SPI_CHANNEL, LCD_SPI_SLAVE_SELECT, data_buf, length, SPI_TRANS_SHORT);
    }
}

void tft_write_word(uint32_t *data_buf, uint32_t length)
{
    set_dcx_data();
    if(!standard_spi)
    {
        tft_config_transfer(SPI_FF_OCTAL, 32, 0, 32, 0);
        spi_send_data_normal_dma(SPI_DMA_CH, SPI_CHANNEL, LCD_SPI_SLAVE_SELECT, data_buf, length, SPI_TRANS_INT);
    }
    else
    {
        tft_config_transfer(SPI_FF_STANDARD, 32, 0, 0, 0);
        spi_send_data_normal_dma(SPI_DMA_CH, SPI_CHANNEL, LCD_SPI_SLAVE_SELECT, data_buf, length, SPI_TRANS_INT);
    }
}

void tft_fill_data(uint32_t *data_buf, uint32_t length)
{
    set_dcx_data();
    if(!standard_spi)
    {
        tft_config_transfer(SPI_FF_OCTAL, 32, 0, 32, 0);
        spi_fill_data_dma(SPI_DMA_CH, SPI_CHANNEL, LCD_SPI_SLAVE_SELECT, data_buf, length);
    }
    else
    {
        tft_config_transfer(SPI_FF_STANDARD, 32, 0, 0, 0);
        spi_fill_data_dma(SPI_DMA_CH, SPI_CHANNEL, LCD_SPI_SLAVE_SELECT, data_buf, length);
    }
}

void tft_set_datawidth(uint8_t width)
{

}


