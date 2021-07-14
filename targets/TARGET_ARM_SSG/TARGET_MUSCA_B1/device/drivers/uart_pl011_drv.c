/*
 * Copyright (c) 2016-2019 Arm Limited
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "uart_pl011_drv.h"

#include <stddef.h>
#include "cmsis_compiler.h"

#define FREQ_IRLPBAUD16_MIN             (1420000u)     /* 1.42 MHz */
#define FREQ_IRLPBAUD16_MAX             (2120000u)     /* 2.12 MHz */
#define SAMPLING_FACTOR                 (16u)
#define UART_PL011_FBRD_WIDTH           (6u)

/**
 * \brief UART PL011 register map structure
 */
struct _uart_pl011_reg_map_t {
    volatile uint32_t uartdr;          /*!< Offset: 0x000 (R/W) Data register */
    union {
        volatile uint32_t uartrsr;
                /*!< Offset: 0x004 (R/ ) Receive status register */
        volatile uint32_t uartecr;
                /*!< Offset: 0x004 ( /W) Error clear register */
    };
    volatile uint32_t reserved_0[4];   /*!< Offset: 0x008-0x014 Reserved */
    volatile uint32_t uartfr;          /*!< Offset: 0x018 (R/ ) Flag register */
    volatile uint32_t reserved_1;      /*!< Offset: 0x01C       Reserved */
    volatile uint32_t uartilpr;
                /*!< Offset: 0x020 (R/W) IrDA low-power counter register */
    volatile uint32_t uartibrd;
                /*!< Offset: 0x024 (R/W) Integer baud rate register */
    volatile uint32_t uartfbrd;
                /*!< Offset: 0x028 (R/W) Fractional baud rate register */
    volatile uint32_t uartlcr_h;
                /*!< Offset: 0x02C (R/W) Line control register */
    volatile uint32_t uartcr;
                /*!< Offset: 0x030 (R/W) Control register */
    volatile uint32_t uartifls;
                /*!< Offset: 0x034 (R/W) Interrupt FIFO level select register */
    volatile uint32_t uartimsc;
                /*!< Offset: 0x038 (R/W) Interrupt mask set/clear register */
    volatile uint32_t uartris;
                /*!< Offset: 0x03C (R/ ) Raw interrupt status register */
    volatile uint32_t uartmis;
                /*!< Offset: 0x040 (R/ ) Masked interrupt status register */
    volatile uint32_t uarticr;
                /*!< Offset: 0x044 ( /W) Interrupt clear register */
    volatile uint32_t uartdmacr;
                /*!< Offset: 0x048 (R/W) DMA control register */
    volatile uint32_t reserved_2[13];  /*!< Offset: 0x04C-0x07C Reserved */
    volatile uint32_t reserved_3[4];
                /*!< Offset: 0x080-0x08C Reserved for test purposes */
    volatile uint32_t reserved_4[976]; /*!< Offset: 0x090-0xFCC Reserved */
    volatile uint32_t reserved_5[4];
                /*!< Offset: 0xFD0-0xFDC Reserved for future ID expansion */
    volatile uint32_t uartperiphid0;
                /*!< Offset: 0xFE0 (R/ ) UARTPeriphID0 register */
    volatile uint32_t uartperiphid1;
                /*!< Offset: 0xFE4 (R/ ) UARTPeriphID1 register */
    volatile uint32_t uartperiphid2;
                /*!< Offset: 0xFE8 (R/ ) UARTPeriphID2 register */
    volatile uint32_t uartperiphid3;
                /*!< Offset: 0xFEC (R/ ) UARTPeriphID3 register */
    volatile uint32_t uartpcellid0;
                /*!< Offset: 0xFF0 (R/ ) UARTPCellID0 register */
    volatile uint32_t uartpcellid1;
                /*!< Offset: 0xFF4 (R/ ) UARTPCellID1 register */
    volatile uint32_t uartpcellid2;
                /*!< Offset: 0xFF8 (R/ ) UARTPCellID2 register */
    volatile uint32_t uartpcellid3;
                /*!< Offset: 0xFFC (R/ ) UARTPCellID3 register */
};

#define UART_PL011_UARTFR_CTS_MASK (                    \
            0x1u<<UART_PL011_UARTFR_CTS_OFF)
#define UART_PL011_UARTFR_DSR_MASK (                    \
            0x1u<<UART_PL011_UARTFR_DSR_OFF)
#define UART_PL011_UARTFR_DCD_MASK (                    \
            0x1u<<UART_PL011_UARTFR_DCD_OFF)
#define UART_PL011_UARTFR_BUSYBIT (                     \
            0x1u<<UART_PL011_UARTFR_BUSYBIT_OFF)
#define UART_PL011_UARTFR_RX_FIFO_EMPTY (               \
            0x1u<<UART_PL011_UARTFR_RX_FIFO_EMPTY_OFF)
#define UART_PL011_UARTFR_TX_FIFO_FULL (                \
            0x1u<<UART_PL011_UARTFR_TX_FIFO_FULL_OFF)
#define UART_PL011_UARTFR_TX_FIFO_EMPTY (                \
            0x1u<<UART_PL011_UARTFR_TX_FIFO_EMPTY_OFF)
#define UART_PL011_UARTFR_RI_MASK (                     \
            0x1u<<UART_PL011_UARTFR_RI_OFF)

#define UART_PL011_UARTLCR_H_BRK_MASK (                 \
            0x1u<<UART_PL011_UARTLCR_H_BRK_OFF)
#define UART_PL011_UARTLCR_H_PARITY_MASK (              \
            0x1u<<UART_PL011_UARTLCR_H_PEN_OFF          \
          | 0x1u<<UART_PL011_UARTLCR_H_EPS_OFF          \
          | 0x1u<<UART_PL011_UARTLCR_H_SPS_OFF)
#define UART_PL011_UARTLCR_H_STOPBIT_MASK (             \
            0x1u<<UART_PL011_UARTLCR_H_STP2_OFF)
#define UART_PL011_UARTLCR_H_FEN_MASK (                 \
            0x1u<<UART_PL011_UARTLCR_H_FEN_OFF)
#define UART_PL011_UARTLCR_H_WLEN_MASK (                \
            0x3u<<UART_PL011_UARTLCR_H_WLEN_OFF)
#define UART_PL011_FORMAT_MASK (                        \
            UART_PL011_UARTLCR_H_PARITY_MASK            \
          | UART_PL011_UARTLCR_H_STOPBIT_MASK           \
          | UART_PL011_UARTLCR_H_WLEN_MASK)

#define UART_PL011_UARTCR_EN_MASK (                     \
            0x1u<<UART_PL011_UARTCR_UARTEN_OFF)
#define UART_PL011_UARTCR_SIREN_MASK (                  \
            0x1u<<UART_PL011_UARTCR_SIREN_OFF)
#define UART_PL011_UARTCR_SIRLP_MASK (                  \
            0x1u<<UART_PL011_UARTCR_SIRLP_OFF)
#define UART_PL011_UARTCR_LBE_MASK (                    \
            0x1u<<UART_PL011_UARTCR_LBE_OFF)
#define UART_PL011_UARTCR_TX_EN_MASK (                  \
            0x1u<<UART_PL011_UARTCR_TXE_OFF)
#define UART_PL011_UARTCR_RX_EN_MASK (                  \
            0x1u<<UART_PL011_UARTCR_RXE_OFF)
#define UART_PL011_UARTCR_DTR_MASK (                    \
            0x1u<<UART_PL011_UARTCR_DTR_OFF)
#define UART_PL011_UARTCR_RTS_MASK (                    \
            0x1u<<UART_PL011_UARTCR_RTS_OFF)
#define UART_PL011_UARTCR_OUT1_MASK (                   \
            0x1u<<UART_PL011_UARTCR_OUT1_OFF)
#define UART_PL011_UARTCR_OUT2_MASK (                   \
            0x1u<<UART_PL011_UARTCR_OUT2_OFF)
#define UART_PL011_UARTCR_RTSE_MASK (                   \
            0x1u<<UART_PL011_UARTCR_RTSE_OFF)
#define UART_PL011_UARTCR_CTSE_MASK (                   \
            0x1u<<UART_PL011_UARTCR_CTSE_OFF)

#define UART_PL011_UARTIFLS_TX_FIFO_LVL_MASK (          \
            0x7u<<UART_PL011_UARTIFLS_TX_OFF)
#define UART_PL011_UARTIFLS_RX_FIFO_LVL_MASK (          \
            0x7u<<UART_PL011_UARTIFLS_RX_OFF)

#define UART_PL011_UARTDMACR_RX_MASK (                  \
            0x1u<<UART_PL011_UARTDMACR_RXEN_OFF         \
          | 0x1u<<UART_PL011_UARTDMACR_ON_ERR_OFF)
#define UART_PL011_UARTDMACR_TX_MASK (                  \
            0x1u<<UART_PL011_UARTDMACR_TXEN_OFF)

/* Default register values of UART PL011 */
#define UART_PL011_DATA_REG_RESET_VALUE     (0x0u)
#define UART_PL011_ECR_REG_CLEAR_VALUE      (0xFFu)
#define UART_PL011_ILPR_REG_RESET_VALUE     (0x0u)
#define UART_PL011_IBRD_REG_RESET_VALUE     (0x0u)
#define UART_PL011_FBRD_REG_RESET_VALUE     (0x0u)
#define UART_PL011_LCR_H_REG_RESET_VALUE    (0x0u)
#define UART_PL011_CR_REG_RESET_VALUE       (0x0300u)
#define UART_PL011_IFLS_REG_RESET_VALUE     (0x12u)
#define UART_PL011_IMSC_REG_RESET_VALUE     (0x0u)
#define UART_PL011_ICR_REG_CLEAR_VALUE      (0x7FFu)
#define UART_PL011_DMACR_REG_RESET_VALUE    (0x0u)

static void _uart_pl011_enable(struct _uart_pl011_reg_map_t* p_uart)
{
    p_uart->uartcr |=  UART_PL011_UARTCR_EN_MASK;
}

static void _uart_pl011_disable(struct _uart_pl011_reg_map_t* p_uart)
{
    p_uart->uartcr &= ~UART_PL011_UARTCR_EN_MASK;
}

static bool _uart_pl011_is_enabled(struct _uart_pl011_reg_map_t* p_uart)
{
    return (bool)(p_uart->uartcr & UART_PL011_UARTCR_EN_MASK);
}

static void _uart_pl011_enable_fifo(struct _uart_pl011_reg_map_t* p_uart)
{
    p_uart->uartlcr_h |= UART_PL011_UARTLCR_H_FEN_MASK;
}

static void _uart_pl011_disable_fifo(struct _uart_pl011_reg_map_t* p_uart)
{
    p_uart->uartlcr_h &= ~UART_PL011_UARTLCR_H_FEN_MASK;
}

static bool _uart_pl011_is_fifo_enabled(struct _uart_pl011_reg_map_t* p_uart)
{
    return (bool)(p_uart->uartlcr_h & UART_PL011_UARTLCR_H_FEN_MASK);
}

static bool _uart_pl011_is_busy(struct _uart_pl011_reg_map_t* p_uart)
{
    return (bool)(p_uart->uartfr & UART_PL011_UARTFR_BUSYBIT);
}

static enum uart_pl011_error_t _uart_pl011_set_baudrate(
                    struct _uart_pl011_reg_map_t* p_uart,
                    uint32_t clk, uint32_t baudrate)
{
    /* Avoiding float calculations, bauddiv is left shifted by 6 */
    uint64_t bauddiv = (((uint64_t)clk)<<UART_PL011_FBRD_WIDTH)
                       /(SAMPLING_FACTOR*baudrate);

    /* Valid bauddiv value
     * uart_clk (min) >= 16 x baud_rate (max)
     * uart_clk (max) <= 16 x 65535 x baud_rate (min)
     */
    if((bauddiv < (1u<<UART_PL011_FBRD_WIDTH))
       || (bauddiv > (65535u<<UART_PL011_FBRD_WIDTH))) {
        return UART_PL011_ERR_INVALID_BAUD;
    }

    p_uart->uartibrd = (uint32_t)(bauddiv >> UART_PL011_FBRD_WIDTH);
    p_uart->uartfbrd = (uint32_t)(bauddiv &
                                 ((1u << UART_PL011_FBRD_WIDTH) - 1u));

    __DMB();

    /* In order to internally update the contents of uartibrd or uartfbrd, a
     * uartlcr_h write must always be performed at the end
     * ARM DDI 0183F, Pg 3-13
     */
    p_uart->uartlcr_h = p_uart->uartlcr_h;

    return UART_PL011_ERR_NONE;
}

static void _uart_pl011_set_format(struct _uart_pl011_reg_map_t* p_uart,
                    enum uart_pl011_wlen_t word_len,
                    enum uart_pl011_parity_t parity,
                    enum uart_pl011_stopbit_t stop_bits)
{
    uint32_t ctrl_reg = p_uart->uartlcr_h & ~(UART_PL011_FORMAT_MASK);

    /* Making sure other bit are not changed */
    word_len  &= UART_PL011_UARTLCR_H_WLEN_MASK;
    parity    &= UART_PL011_UARTLCR_H_PARITY_MASK;
    stop_bits &= UART_PL011_UARTLCR_H_STOPBIT_MASK;

    p_uart->uartlcr_h = ctrl_reg | word_len | parity | stop_bits;

}

static void _uart_pl011_set_cr_bit(struct _uart_pl011_reg_map_t* p_uart,
                    uint32_t mask)
{
    bool uart_enabled = _uart_pl011_is_enabled(p_uart);
    bool fifo_enabled = _uart_pl011_is_fifo_enabled(p_uart);

    /* UART must be disabled before any Control Register or
     * Line Control Register are reprogrammed */
    _uart_pl011_disable(p_uart);

    /* Flush the transmit FIFO by disabling bit 4 (FEN) in
     * the line control register (UARTCLR_H) */
    _uart_pl011_disable_fifo(p_uart);

    p_uart->uartcr |= (mask);

    /* Enabling the FIFOs if previously enabled */
    if(fifo_enabled) {
        _uart_pl011_enable_fifo(p_uart);
    }

    /* Enabling the UART if previously enabled */
    if(uart_enabled) {
        _uart_pl011_enable(p_uart);
    }
}

static void _uart_pl011_clear_cr_bit(struct _uart_pl011_reg_map_t* p_uart,
                    uint32_t mask)
{
    bool uart_enabled = _uart_pl011_is_enabled(p_uart);
    bool fifo_enabled = _uart_pl011_is_fifo_enabled(p_uart);

    /* UART must be disabled before any Control Register or
     * Line Control Register are reprogrammed */
    _uart_pl011_disable(p_uart);

    /* Flush the transmit FIFO by disabling bit 4 (FEN) in
     * the line control register (UARTCLR_H) */
    _uart_pl011_disable_fifo(p_uart);

    p_uart->uartcr &= ~(mask);

    /* Enabling the FIFOs if previously enabled */
    if(fifo_enabled) {
        _uart_pl011_enable_fifo(p_uart);
    }

    /* Enabling the UART if previously enabled */
    if(uart_enabled) {
        _uart_pl011_enable(p_uart);
    }
}

static void _uart_pl011_set_lcr_h_bit(struct _uart_pl011_reg_map_t* p_uart,
                    uint32_t mask)
{
    bool uart_enabled = _uart_pl011_is_enabled(p_uart);

    /* UART must be disabled before any Control Register or
     * Line Control Register are reprogrammed */
    _uart_pl011_disable(p_uart);

    p_uart->uartlcr_h |= (mask);

    /* Enabling the UART if previously enabled */
    if(uart_enabled) {
        _uart_pl011_enable(p_uart);
    }
}

static void _uart_pl011_clear_lcr_h_bit(struct _uart_pl011_reg_map_t* p_uart,
                    uint32_t mask)
{
    bool uart_enabled = _uart_pl011_is_enabled(p_uart);

    /* UART must be disabled before any Control Register or
     * Line Control Register are reprogrammed */
    _uart_pl011_disable(p_uart);

    p_uart->uartlcr_h &= ~(mask);

    /* Enabling the UART if previously enabled */
    if(uart_enabled) {
        _uart_pl011_enable(p_uart);
    }
}

static void _uart_pl011_reset_regs(struct _uart_pl011_reg_map_t* p_uart)
{
    /* Restore the default value of UART registers, the registers which
     * are not listed below are Read-Only */

    /* Will disable the UART */
    p_uart->uartcr      = UART_PL011_CR_REG_RESET_VALUE;
    p_uart->uartdr      = UART_PL011_DATA_REG_RESET_VALUE;
    /* Clear all the errors */
    p_uart->uartecr     = UART_PL011_ECR_REG_CLEAR_VALUE;
    p_uart->uartilpr    = UART_PL011_ILPR_REG_RESET_VALUE;
    p_uart->uartibrd    = UART_PL011_IBRD_REG_RESET_VALUE;
    p_uart->uartfbrd    = UART_PL011_FBRD_REG_RESET_VALUE;
    p_uart->uartlcr_h   = UART_PL011_LCR_H_REG_RESET_VALUE;
    p_uart->uartifls    = UART_PL011_IFLS_REG_RESET_VALUE;
    p_uart->uartimsc    = UART_PL011_IMSC_REG_RESET_VALUE;
    /* Clear all the interrupts */
    p_uart->uarticr     = UART_PL011_ICR_REG_CLEAR_VALUE;
    p_uart->uartdmacr   = UART_PL011_DMACR_REG_RESET_VALUE;
}

enum uart_pl011_error_t uart_pl011_init(struct uart_pl011_dev_t* dev,
                    uint32_t uart_clk)
{
    enum uart_pl011_error_t err;

    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    uint32_t def_baud = dev->cfg->def_baudrate;

    if(uart_clk == 0) {
        return UART_PL011_ERR_INVALID_ARG;
    }

    if(def_baud == 0) {
        return UART_PL011_ERR_INVALID_BAUD;
    }

    /* Updating the system clock */
    dev->data->uart_clk = uart_clk;

    /* Setting the default baudrate */
    err = _uart_pl011_set_baudrate(p_uart, uart_clk, def_baud);

    if(err != UART_PL011_ERR_NONE) {
        return err;
    }

    /* Setting the default character format */
    _uart_pl011_set_format(p_uart, dev->cfg->def_wlen,
                                   dev->cfg->def_parity,
                                   dev->cfg->def_stopbit);

    /* Enabling the FIFOs */
    _uart_pl011_enable_fifo(p_uart);

    dev->data->state = UART_PL011_INITIALIZED;

    return UART_PL011_ERR_NONE;
}

void uart_pl011_uninit(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    while(_uart_pl011_is_busy(p_uart));

    /* Disable and restore the default configuration of the peripheral */
    _uart_pl011_reset_regs(p_uart);

    dev->data->state = UART_PL011_UNINITIALIZED;

    return;
}

enum uart_pl011_state_t uart_pl011_get_state(struct uart_pl011_dev_t* dev)
{
    return dev->data->state;
}

enum uart_pl011_error_t uart_pl011_set_baudrate(
                    struct uart_pl011_dev_t* dev, uint32_t baudrate)
{
    enum uart_pl011_error_t err = UART_PL011_ERR_NONE;

    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    bool uart_enabled = _uart_pl011_is_enabled(p_uart);

    if(uart_pl011_get_state(dev) != UART_PL011_INITIALIZED) {
        return UART_PL011_ERR_NOT_INIT;
    }

    if(baudrate == 0) {
        return UART_PL011_ERR_INVALID_BAUD;
    }

    /* UART must be disabled before any Control Register or
    *  Line Control Register are reprogrammed */
    _uart_pl011_disable(p_uart);

    /* If baudrate is not valid ie UART_PL011_ERR_NONE is not returned then
     * the UART will continue to function at the old baudrate */
    err = _uart_pl011_set_baudrate(p_uart, dev->data->uart_clk, baudrate);

    if(err == UART_PL011_ERR_NONE) {
        dev->data->baudrate = baudrate;
    }

    if(uart_enabled) {
        _uart_pl011_enable(p_uart);
    }

    return err;
}

uint32_t uart_pl011_get_baudrate(struct uart_pl011_dev_t* dev)
{
    return dev->data->baudrate;
}

void uart_pl011_enable_intr(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_intr_t mask)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    p_uart->uartimsc |= (uint32_t)(mask);

    return;
}

void uart_pl011_disable_intr(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_intr_t mask)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    p_uart->uartimsc &= (uint32_t)(~mask);

    return;
}

void uart_pl011_clear_intr(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_intr_t mask)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    p_uart->uarticr = (uint32_t)mask;

    return;
}


enum uart_pl011_intr_t uart_pl011_get_masked_intr_status(
                    struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    return (enum uart_pl011_intr_t)(p_uart->uartmis);

}

enum uart_pl011_intr_t uart_pl011_get_raw_intr_status(
                    struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    return (enum uart_pl011_intr_t)(p_uart->uartris);
}

void uart_pl011_set_rx_fifo_lvl(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_rx_fifo_lvl_t rx_lvl)
{
    uint32_t fifo_lvl;

    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    /* Check if rx_lvl have valid values */
    rx_lvl &= UART_PL011_UARTIFLS_RX_FIFO_LVL_MASK;

    fifo_lvl = p_uart->uartifls
                        & ~(UART_PL011_UARTIFLS_RX_FIFO_LVL_MASK);
    p_uart->uartifls = fifo_lvl | rx_lvl;

    return;
}

void uart_pl011_set_tx_fifo_lvl(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_tx_fifo_lvl_t tx_lvl)
{
    uint32_t fifo_lvl;

    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;
    /* Check if tx_lvl have valid values */
    tx_lvl &= UART_PL011_UARTIFLS_TX_FIFO_LVL_MASK;

    fifo_lvl = p_uart->uartifls
                        & ~(UART_PL011_UARTIFLS_TX_FIFO_LVL_MASK);
    p_uart->uartifls = fifo_lvl | tx_lvl;

    return;
}

void uart_pl011_set_tx_dma(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_tx_dma_t enable)
{
    uint32_t dma_cr;

    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    enable &= UART_PL011_UARTDMACR_TX_MASK;

    dma_cr = p_uart->uartdmacr
                      & ~(UART_PL011_UARTDMACR_TX_MASK);

    p_uart->uartdmacr = dma_cr | enable;

    return;
}

void uart_pl011_set_rx_dma(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_rx_dma_t enable)
{
    uint32_t dma_cr;

    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    enable &= UART_PL011_UARTDMACR_RX_MASK;

    dma_cr = p_uart->uartdmacr
                      & ~(UART_PL011_UARTDMACR_RX_MASK);

    p_uart->uartdmacr = dma_cr | enable;

    return;
}

bool uart_pl011_is_readable(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    if( (uart_pl011_get_state(dev) == UART_PL011_INITIALIZED) &&
                /* UART is initialized */
        (p_uart->uartcr & UART_PL011_UARTCR_EN_MASK) &&
                /* UART is enabled */
        (p_uart->uartcr & UART_PL011_UARTCR_RX_EN_MASK) &&
                /* Receive is enabled */
        ((p_uart->uartfr & UART_PL011_UARTFR_RX_FIFO_EMPTY) == 0)) {
                /* Receive Fifo is not empty */
        return true;
    }

    return false;

}

enum uart_pl011_error_t uart_pl011_read(
                    struct uart_pl011_dev_t* dev, uint8_t* byte)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    *byte = p_uart->uartdr;

    return (enum uart_pl011_error_t)(p_uart->uartrsr
                                         & UART_PL011_RX_ERR_MASK);
}

bool uart_pl011_is_writable(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    if( (uart_pl011_get_state(dev) == UART_PL011_INITIALIZED) &&
                /* UART is initialized */
        (p_uart->uartcr & UART_PL011_UARTCR_EN_MASK) &&
                /* UART is enabled */
        (p_uart->uartcr & UART_PL011_UARTCR_TX_EN_MASK) &&
                /* Transmit is enabled */
        ((p_uart->uartfr & UART_PL011_UARTFR_TX_FIFO_FULL) == 0)) {
                /* Transmit Fifo is not full */
        return true;
    }
    return false;

}

bool uart_pl011_is_tx_empty(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    if( ((p_uart->uartfr & UART_PL011_UARTFR_TX_FIFO_EMPTY) == 1) &&
                /* Transmit Fifo is empty */
        ((p_uart->uartfr & UART_PL011_UARTFR_BUSYBIT) == 0)) {
                /* Tx is not BUSY */
        return true;
    }
    return false;

}

void uart_pl011_write(struct uart_pl011_dev_t* dev, uint8_t byte)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    p_uart->uartdr = byte;

    return;
}

enum uart_pl011_error_t uart_pl011_set_format(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_wlen_t word_len,
                    enum uart_pl011_parity_t parity,
                    enum uart_pl011_stopbit_t stop_bits)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    bool uart_enabled = _uart_pl011_is_enabled(p_uart);

    if(uart_pl011_get_state(dev) != UART_PL011_INITIALIZED) {
        return UART_PL011_ERR_NOT_INIT;
    }

    /* UART must be disabled before any Control Register or
     * Line Control Register are reprogrammed */
    _uart_pl011_disable(p_uart);

    _uart_pl011_set_format(p_uart, word_len, parity, stop_bits);

    /* Enabling the UART if previously enabled */
    if(uart_enabled) {
        _uart_pl011_enable(p_uart);
    }

    return UART_PL011_ERR_NONE;
}

void uart_pl011_enable_fifo(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_lcr_h_bit(p_uart, UART_PL011_UARTLCR_H_FEN_MASK);

    return;
}

void uart_pl011_disable_fifo(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_lcr_h_bit(p_uart, UART_PL011_UARTLCR_H_FEN_MASK);

    return;
}

void uart_pl011_enable_break(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_lcr_h_bit(p_uart, UART_PL011_UARTLCR_H_BRK_MASK);

    return;
}

void uart_pl011_disable_break(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_lcr_h_bit(p_uart, UART_PL011_UARTLCR_H_BRK_MASK);

    return;
}

void uart_pl011_enable_cts_flowcontrol(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart, UART_PL011_UARTCR_CTSE_MASK);

    return;
}

void uart_pl011_disable_cts_flowcontrol(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart, UART_PL011_UARTCR_CTSE_MASK);

    return;
}

void uart_pl011_enable_rts_flowcontrol(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart, UART_PL011_UARTCR_RTSE_MASK);

    return;
}

void uart_pl011_disable_rts_flowcontrol(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart, UART_PL011_UARTCR_RTSE_MASK);

    return;
}

void uart_pl011_enable_ri(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart, UART_PL011_UARTCR_OUT2_MASK);

    return;
}

void uart_pl011_disable_ri(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart, UART_PL011_UARTCR_OUT2_MASK);

    return;
}

void uart_pl011_enable_dcd(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart, UART_PL011_UARTCR_OUT1_MASK);

    return;
}

void uart_pl011_disable_dcd(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart, UART_PL011_UARTCR_OUT1_MASK);

    return;
}

void uart_pl011_set_rts(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart, UART_PL011_UARTCR_RTS_MASK);

    return;
}

void uart_pl011_clear_rts(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart, UART_PL011_UARTCR_RTS_MASK);

    return;
}

void uart_pl011_set_dtr(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart, UART_PL011_UARTCR_DTR_MASK);

    return;
}

void uart_pl011_clear_dtr(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart, UART_PL011_UARTCR_DTR_MASK);

    return;
}

void uart_pl011_enable_receive(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart, UART_PL011_UARTCR_RX_EN_MASK);

    return;
}

void uart_pl011_disable_receive(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart, UART_PL011_UARTCR_RX_EN_MASK);

    return;
}

void uart_pl011_enable_transmit(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart, UART_PL011_UARTCR_TX_EN_MASK);

    return;
}

void uart_pl011_disable_transmit(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart, UART_PL011_UARTCR_TX_EN_MASK);

    return;
}

void uart_pl011_set_loopback(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart, UART_PL011_UARTCR_LBE_MASK);

    return;
}

void uart_pl011_clear_loopback(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart, UART_PL011_UARTCR_LBE_MASK);

    return;
}

void uart_pl011_enable_sirlp(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart,
     UART_PL011_UARTCR_SIREN_MASK | UART_PL011_UARTCR_SIRLP_MASK);

    return;
}

void uart_pl011_disable_sirlp(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart,
     UART_PL011_UARTCR_SIREN_MASK | UART_PL011_UARTCR_SIRLP_MASK);

    return;
}

void uart_pl011_enable_sir(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_set_cr_bit(p_uart, UART_PL011_UARTCR_SIREN_MASK);

    return;
}

void uart_pl011_disable_sir(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_clear_cr_bit(p_uart, UART_PL011_UARTCR_SIREN_MASK);

    return;
}

void uart_pl011_enable(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_enable(p_uart);

    return;
}

void uart_pl011_disable(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    _uart_pl011_disable(p_uart);

    return;
}

bool uart_pl011_get_cts_status(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;
    return (bool)(p_uart->uartfr & UART_PL011_UARTFR_CTS_MASK);

}

bool uart_pl011_get_dsr_status(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;
    return (bool)(p_uart->uartfr & UART_PL011_UARTFR_DSR_MASK);

}

bool uart_pl011_get_dcd_status(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;
    return (bool)(p_uart->uartfr & UART_PL011_UARTFR_DCD_MASK);

}

bool uart_pl011_get_ri_status(struct uart_pl011_dev_t* dev)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;
    return (bool)(p_uart->uartfr & UART_PL011_UARTFR_RI_MASK);

}

enum uart_pl011_error_t uart_pl011_set_sirlp_divisor(
            struct uart_pl011_dev_t* dev, uint32_t value)
{
    struct _uart_pl011_reg_map_t* p_uart =
        (struct _uart_pl011_reg_map_t*)dev->cfg->base;

    uint32_t irlp_baud16_clk;

    if(uart_pl011_get_state(dev) != UART_PL011_INITIALIZED) {
        return UART_PL011_ERR_NOT_INIT;
    }

    if(value == 0) {
        return UART_PL011_ERR_INVALID_ARG;
    }

    irlp_baud16_clk = dev->data->uart_clk/value;

    /* Chose the divisor so that 1.42MHz < FIrLPBaud16 < 2.12MHz, that
     * results in a low-power pulse duration of 1.41–2.11μs (three times
     * the period of IrLPBaud16). ARM DDI0183F Pg 3-9 */
    if(irlp_baud16_clk < FREQ_IRLPBAUD16_MIN ||
       irlp_baud16_clk > FREQ_IRLPBAUD16_MAX) {
        return UART_PL011_ERR_INVALID_ARG;
    }

    p_uart->uartilpr = value;

    return UART_PL011_ERR_NONE;
}
