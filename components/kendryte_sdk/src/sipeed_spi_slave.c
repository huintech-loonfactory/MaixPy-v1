#include <string.h>
#include <spi.h>
#include <fpioa.h>
#include <gpiohs.h>
#include <sysctl.h>
#include <plic.h>
#include "sipeed_spi_slave.h"

/* Shared buffers: 64-byte aligned for DMA, fixed in DRAM */
sipeed_uint8_t g_spi_slave_tx_buf[SPI_SLAVE_TX_BUF_SIZE]
    __attribute__((section(".dram0.bss"), aligned(64)));

sipeed_uint8_t g_spi_slave_rx_buf[SPI_SLAVE_RX_BUF_SIZE]
    __attribute__((section(".dram0.bss"), aligned(64)));

static sipeed_uint8_t s_config_buf[8]
    __attribute__((section(".dram0.bss"), aligned(8)));

static volatile sipeed_uint8_t s_rx_ready = 0;

static int spi_slave_rx_callback(void *ctx)
{
    (void)ctx;
    s_rx_ready = 1;
    return 0;
}

/* CRC-16/CCITT: poly 0x1021, init 0xFFFF, no reflection */
sipeed_uint32_t spi_slave_crc16(const sipeed_uint8_t *data, sipeed_uint32_t len)
{
    sipeed_uint32_t crc = 0xFFFFu;
    sipeed_uint32_t i;
    int b;
    for (i = 0; i < len; i++) {
        crc ^= (sipeed_uint32_t)data[i] << 8;
        for (b = 0; b < 8; b++) {
            if (crc & 0x8000u)
                crc = (crc << 1) ^ 0x1021u;
            else
                crc <<= 1;
        }
    }
    return crc & 0xFFFFu;
}

void sipeed_spi_slave_init(sipeed_uint8_t pin_d0,
                           sipeed_uint8_t pin_ss,
                           sipeed_uint8_t pin_sclk,
                           sipeed_uint8_t int_pin,
                           sipeed_uint8_t ready_pin,
                           sipeed_dmac_channel_number_t dmac_ch)
{
    s_rx_ready = 0;

    memset(g_spi_slave_tx_buf, 0, sizeof(g_spi_slave_tx_buf));
    memset(g_spi_slave_rx_buf, 0, sizeof(g_spi_slave_rx_buf));
    memset(s_config_buf,       0, sizeof(s_config_buf));

    fpioa_set_function(pin_d0,   FUNC_SPI_SLAVE_D0);
    fpioa_set_function(pin_ss,   FUNC_SPI_SLAVE_SS);
    fpioa_set_function(pin_sclk, FUNC_SPI_SLAVE_SCLK);
    /* int_pin and ready_pin use GPIOHS channel == physical pin number */
    fpioa_set_function(int_pin,   FUNC_GPIOHS0 + int_pin);
    fpioa_set_function(ready_pin, FUNC_GPIOHS0 + ready_pin);

    spi_slave_config(int_pin, ready_pin, dmac_ch,
                     8,
                     s_config_buf,
                     sizeof(s_config_buf),
                     spi_slave_rx_callback);
}

void sipeed_spi_slave_set_tx(const sipeed_uint8_t *src, sipeed_uint32_t len)
{
    if (len > SPI_SLAVE_TX_BUF_SIZE)
        len = SPI_SLAVE_TX_BUF_SIZE;
    plic_irq_disable(IRQN_SPI_SLAVE_INTERRUPT);
    memcpy(g_spi_slave_tx_buf, src, len);
    plic_irq_enable(IRQN_SPI_SLAVE_INTERRUPT);
}

void sipeed_spi_slave_get_rx(sipeed_uint8_t *dst, sipeed_uint32_t len)
{
    if (len > SPI_SLAVE_RX_BUF_SIZE)
        len = SPI_SLAVE_RX_BUF_SIZE;
    memcpy(dst, g_spi_slave_rx_buf, len);
}

sipeed_uint8_t sipeed_spi_slave_rx_ready(void)
{
    return s_rx_ready;
}

void sipeed_spi_slave_rx_clear(void)
{
    s_rx_ready = 0;
}

sipeed_uint32_t sipeed_spi_slave_tx_addr(void)
{
    return (sipeed_uint32_t)(uintptr_t)g_spi_slave_tx_buf;
}

sipeed_uint32_t sipeed_spi_slave_rx_addr(void)
{
    return (sipeed_uint32_t)(uintptr_t)g_spi_slave_rx_buf;
}

void sipeed_spi_slave_deinit(void)
{
    plic_irq_disable(IRQN_SPI_SLAVE_INTERRUPT);
    sysctl_clock_disable(SYSCTL_CLOCK_SPI2);
    s_rx_ready = 0;
}
