#ifndef _SIPEEED_SPI_SLAVE_H
#define _SIPEEED_SPI_SLAVE_H

#include <spi.h>
#include <dmac.h>
#include "sipeed_type.h"

#define sipeed_dmac_channel_number_t dmac_channel_number_t

/* Buffer sizes */
#define SPI_SLAVE_TX_BUF_SIZE  512   /* K210 -> master */
#define SPI_SLAVE_RX_BUF_SIZE   64   /* master -> K210 */

/* Shared DRAM buffers; advertise addresses to the master via UART */
extern sipeed_uint8_t g_spi_slave_tx_buf[SPI_SLAVE_TX_BUF_SIZE];
extern sipeed_uint8_t g_spi_slave_rx_buf[SPI_SLAVE_RX_BUF_SIZE];

void sipeed_spi_slave_init(sipeed_uint8_t pin_d0,
                           sipeed_uint8_t pin_ss,
                           sipeed_uint8_t pin_sclk,
                           sipeed_uint8_t int_pin,
                           sipeed_uint8_t ready_pin,
                           sipeed_dmac_channel_number_t dmac_ch);

void sipeed_spi_slave_set_tx(const sipeed_uint8_t *src, sipeed_uint32_t len);
void sipeed_spi_slave_get_rx(sipeed_uint8_t *dst, sipeed_uint32_t len);
sipeed_uint8_t sipeed_spi_slave_rx_ready(void);
void sipeed_spi_slave_rx_clear(void);
sipeed_uint32_t sipeed_spi_slave_tx_addr(void);
sipeed_uint32_t sipeed_spi_slave_rx_addr(void);
void sipeed_spi_slave_deinit(void);

sipeed_uint32_t spi_slave_crc16(const sipeed_uint8_t *data, sipeed_uint32_t len);

#endif /* _SIPEEED_SPI_SLAVE_H */
