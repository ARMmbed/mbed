/*
 * Copyright (c) 2016-2018 Arm Limited
 *
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

/**
 * \file uart_pl011_drv.h
 * \brief Driver for ARM UART PL011.
 */

#ifndef __UART_PL011_DRV_H__
#define __UART_PL011_DRV_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief ARM UART PL011 state types
 */
enum uart_pl011_state_t {
    UART_PL011_UNINITIALIZED = 0x0u,
    UART_PL011_INITIALIZED   = 0x1u,
};

#define UART_PL011_UARTRSR_FE_ERR_OFF        0x0u
                 /*!< Receive Status Register Frame Error bit field offset */
#define UART_PL011_UARTRSR_PE_ERR_OFF        0x1u
                 /*!< Receive Status Register Parity Error bit field offset */
#define UART_PL011_UARTRSR_BE_ERR_OFF        0x2u
                 /*!< Receive Status Register Break Error bit field offset */
#define UART_PL011_UARTRSR_OE_ERR_OFF        0x3u
                 /*!< Receive Status Register Overrun Error bit field offset */

#define UART_PL011_RX_ERR_MASK (                 \
            0x1u<<UART_PL011_UARTRSR_FE_ERR_OFF  \
          | 0x1u<<UART_PL011_UARTRSR_PE_ERR_OFF  \
          | 0x1u<<UART_PL011_UARTRSR_BE_ERR_OFF  \
          | 0x1u<<UART_PL011_UARTRSR_OE_ERR_OFF)
                           /*!< Receive Status Register Error Mask */

#define UART_PL011_UARTFR_CTS_OFF            0x0u
                     /*!< Flag Register Clear to send bit field offset */
#define UART_PL011_UARTFR_DSR_OFF            0x1u
                     /*!< Flag Register Data set ready bit field offset */
#define UART_PL011_UARTFR_DCD_OFF            0x2u
                     /*!< Flag Register Data carrier detect bit field offset */
#define UART_PL011_UARTFR_BUSYBIT_OFF        0x3u
                     /*!< Flag Register Busy bit field offset */
#define UART_PL011_UARTFR_RX_FIFO_EMPTY_OFF  0x4u
                     /*!< Flag Register Receive fifo empty bit field offset */
#define UART_PL011_UARTFR_TX_FIFO_FULL_OFF   0x5u
                     /*!< Flag Register Transmit fifo full bit field offset */
#define UART_PL011_UARTFR_RX_FIFO_FULL_OFF   0x6u
                     /*!< Flag Register Receive fifo full bit field offset */
#define UART_PL011_UARTFR_TX_FIFO_EMPTY_OFF  0x7u
                     /*!< Flag Register Transmit fifo empty bit field offset */
#define UART_PL011_UARTFR_RI_OFF             0x8u
                     /*!< Flag Register Ring indicator bit field offset */

#define UART_PL011_UARTLCR_H_BRK_OFF         0x0u
             /*!< Line Control Register Break bit field offset */
#define UART_PL011_UARTLCR_H_PEN_OFF         0x1u
             /*!< Line Control Register Parity enable bit field offset */
#define UART_PL011_UARTLCR_H_EPS_OFF         0x2u
             /*!< Line Control Register Even parity select bit field offset */
#define UART_PL011_UARTLCR_H_STP2_OFF        0x3u
             /*!< Line Control Register 2 stop bit select bit field offset */
#define UART_PL011_UARTLCR_H_FEN_OFF         0x4u
             /*!< Line Control Register Fifo enable bit field offset */
#define UART_PL011_UARTLCR_H_WLEN_OFF        0x5u
             /*!< Line Control Register Word length bit field offset */
#define UART_PL011_UARTLCR_H_SPS_OFF         0x7u
             /*!< Line Control Register Stick parity select bit field offset */

/**
 * \brief Allowed word length options UART PL011
 */
enum uart_pl011_wlen_t {
  UART_PL011_WLEN_5 = (0x0u<<UART_PL011_UARTLCR_H_WLEN_OFF),
  UART_PL011_WLEN_6 = (0x1u<<UART_PL011_UARTLCR_H_WLEN_OFF),
  UART_PL011_WLEN_7 = (0x2u<<UART_PL011_UARTLCR_H_WLEN_OFF),
  UART_PL011_WLEN_8 = (0x3u<<UART_PL011_UARTLCR_H_WLEN_OFF),
};

/**
 * \brief Allowed parity options UART PL011
 */
enum uart_pl011_parity_t {
  UART_PL011_PARITY_DISABLED  =  (0x0u<<UART_PL011_UARTLCR_H_PEN_OFF),
  UART_PL011_PARITY_ODD       =  (0x1u<<UART_PL011_UARTLCR_H_PEN_OFF
                                    | 0x0u<<UART_PL011_UARTLCR_H_EPS_OFF
                                    | 0x0u<<UART_PL011_UARTLCR_H_SPS_OFF),
  UART_PL011_PARITY_EVEN      =  (0x1u<<UART_PL011_UARTLCR_H_PEN_OFF
                                    | 0x1u<<UART_PL011_UARTLCR_H_EPS_OFF
                                    | 0x0u<<UART_PL011_UARTLCR_H_SPS_OFF),
  UART_PL011_PARITY_STICKY_ONE=  (0x1u<<UART_PL011_UARTLCR_H_PEN_OFF
                                    | 0x0u<<UART_PL011_UARTLCR_H_EPS_OFF
                                    | 0x1u<<UART_PL011_UARTLCR_H_SPS_OFF),
  UART_PL011_PARITY_STICKY_ZERO= (0x1u<<UART_PL011_UARTLCR_H_PEN_OFF
                                    | 0x1u<<UART_PL011_UARTLCR_H_EPS_OFF
                                    | 0x1u<<UART_PL011_UARTLCR_H_SPS_OFF),
};

/**
 * \brief Allowed stop bits options UART PL011
 */
enum uart_pl011_stopbit_t {
  UART_PL011_STOPBIT_1    = (0x0u<<UART_PL011_UARTLCR_H_STP2_OFF),
  UART_PL011_STOPBIT_2    = (0x1u<<UART_PL011_UARTLCR_H_STP2_OFF),
};

#define UART_PL011_UARTCR_UARTEN_OFF         0x0u
     /*!< Control Register Uart enable bit field offset */
#define UART_PL011_UARTCR_SIREN_OFF          0x1u
     /*!< Control Register Sir enable bit field offset */
#define UART_PL011_UARTCR_SIRLP_OFF          0x2u
     /*!< Control Register Sir low power bit field offset */
#define UART_PL011_UARTCR_LBE_OFF            0x7u
     /*!< Control Register Loop back enable bit field offset */
#define UART_PL011_UARTCR_TXE_OFF            0x8u
     /*!< Control Register Transmit enable bit field offset */
#define UART_PL011_UARTCR_RXE_OFF            0x9u
     /*!< Control Register Receive enable bit field offset */
#define UART_PL011_UARTCR_DTR_OFF            0xAu
     /*!< Control Register Data transmit ready bit field offset */
#define UART_PL011_UARTCR_RTS_OFF            0xBu
     /*!< Control Register Request to send bit field offset */
#define UART_PL011_UARTCR_OUT1_OFF           0xCu
     /*!< Control Register Out1 bit field offset */
#define UART_PL011_UARTCR_OUT2_OFF           0xDu
     /*!< Control Register Out2 bit field offset */
#define UART_PL011_UARTCR_RTSE_OFF           0xEu
     /*!< Control Register RTS hardware flow control enable bit field offset */
#define UART_PL011_UARTCR_CTSE_OFF           0xFu
     /*!< Control Register CTS hardware flow control enable bit field offset */

#define UART_PL011_UARTIFLS_TX_OFF           0x0u
         /*!< Interrupt FIFO Level Select Register Transmit bit field offset */
#define UART_PL011_UARTIFLS_RX_OFF           0x3u
         /*!< Interrupt FIFO Level Select Register Receive bit field offset */

/**
 * \brief UART Receive fifo levels
 */
enum uart_pl011_rx_fifo_lvl_t {
  UART_PL011_RX_FIFO_LVL_1_8 = (0x0u<<UART_PL011_UARTIFLS_RX_OFF),
  UART_PL011_RX_FIFO_LVL_1_4 = (0x1u<<UART_PL011_UARTIFLS_RX_OFF),
  UART_PL011_RX_FIFO_LVL_1_2 = (0x2u<<UART_PL011_UARTIFLS_RX_OFF),
  UART_PL011_RX_FIFO_LVL_3_4 = (0x3u<<UART_PL011_UARTIFLS_RX_OFF),
  UART_PL011_RX_FIFO_LVL_7_8 = (0x4u<<UART_PL011_UARTIFLS_RX_OFF),
};

/**
 * \brief UART Transmit fifo levels
 */
enum uart_pl011_tx_fifo_lvl_t {
  UART_PL011_TX_FIFO_LVL_1_8 = (0x0u<<UART_PL011_UARTIFLS_TX_OFF),
  UART_PL011_TX_FIFO_LVL_1_4 = (0x1u<<UART_PL011_UARTIFLS_TX_OFF),
  UART_PL011_TX_FIFO_LVL_1_2 = (0x2u<<UART_PL011_UARTIFLS_TX_OFF),
  UART_PL011_TX_FIFO_LVL_3_4 = (0x3u<<UART_PL011_UARTIFLS_TX_OFF),
  UART_PL011_TX_FIFO_LVL_7_8 = (0x4u<<UART_PL011_UARTIFLS_TX_OFF),
};

#define UART_PL011_UARTDMACR_RXEN_OFF        0x0u
             /*!< DMA Control Register Receive DMA enable bit field offset */
#define UART_PL011_UARTDMACR_TXEN_OFF        0x1u
             /*!< DMA Control Register Transmit DMA enable bit field offset */
#define UART_PL011_UARTDMACR_ON_ERR_OFF      0x2u
             /*!< DMA Control Register DMA on error bit field offset */

/**
 * \brief Transmit DMA Enable
 */
enum uart_pl011_tx_dma_t {
  UART_PL011_TX_DMA_DISABLE = (0x0u<<UART_PL011_UARTDMACR_TXEN_OFF),
  UART_PL011_TX_DMA_ENABLE  = (0x1u<<UART_PL011_UARTDMACR_TXEN_OFF),
};

/**
 * \brief Receive DMA Enable
 */
enum uart_pl011_rx_dma_t {
  UART_PL011_RX_DMA_DISABLE  =  (0x0u<<UART_PL011_UARTDMACR_RXEN_OFF),
  UART_PL011_RX_DMA_ENABLE   =  (0x1u<<UART_PL011_UARTDMACR_RXEN_OFF),
  UART_PL011_RX_DMA_ON_ERR_EN=  (0x1u<<UART_PL011_UARTDMACR_RXEN_OFF
                                | 0x1u<<UART_PL011_UARTDMACR_ON_ERR_OFF),
};

#define UART_PL011_INTR_RI_OFF   0x0u
                         /*!< Ring indicator interrupt bit field offset */
#define UART_PL011_INTR_CTS_OFF  0x1u
                         /*!< Clear to send interrupt bit field offset */
#define UART_PL011_INTR_DCD_OFF  0x2u
                         /*!< Data carrier detect interrupt bit field offset */
#define UART_PL011_INTR_DSR_OFF  0x3u
                         /*!< Data set ready interrupt bit field offset */
#define UART_PL011_INTR_RX_OFF   0x4u
                         /*!< Receive interrupt bit field offset */
#define UART_PL011_INTR_TX_OFF   0x5u
                         /*!< Transmit interrupt bit field offset */
#define UART_PL011_INTR_RT_OFF   0x6u
                         /*!< Receive timeout interrupt bit field offset */
#define UART_PL011_INTR_FE_OFF   0x7u
                         /*!< Frame error interrupt bit field offset */
#define UART_PL011_INTR_PE_OFF   0x8u
                         /*!< Parity error interrupt bit field offset */
#define UART_PL011_INTR_BE_OFF   0x9u
                         /*!< Break error interrupt bit field offset */
#define UART_PL011_INTR_OE_OFF   0xAu
                         /*!< Overrun error interrupt bit field offset */

/**
 * \brief ARM UART PL011 Interrupt data structure
 */
enum uart_pl011_intr_t {
  UART_PL011_RI_INTR_MASK  = (0x1u<<UART_PL011_INTR_RI_OFF),
  UART_PL011_CTS_INTR_MASK = (0x1u<<UART_PL011_INTR_CTS_OFF),
  UART_PL011_DCD_INTR_MASK = (0x1u<<UART_PL011_INTR_DCD_OFF),
  UART_PL011_DSR_INTR_MASK = (0x1u<<UART_PL011_INTR_DSR_OFF),
  UART_PL011_RX_INTR_MASK  = (0x1u<<UART_PL011_INTR_RX_OFF),
  UART_PL011_TX_INTR_MASK  = (0x1u<<UART_PL011_INTR_TX_OFF),
  UART_PL011_RT_INTR_MASK  = (0x1u<<UART_PL011_INTR_RT_OFF),
  UART_PL011_FE_INTR_MASK  = (0x1u<<UART_PL011_INTR_FE_OFF),
  UART_PL011_PE_INTR_MASK  = (0x1u<<UART_PL011_INTR_PE_OFF),
  UART_PL011_BE_INTR_MASK  = (0x1u<<UART_PL011_INTR_BE_OFF),
  UART_PL011_OE_INTR_MASK  = (0x1u<<UART_PL011_INTR_OE_OFF),
};

/**
 * \brief ARM UART PL011 error enumeration types
 */
enum uart_pl011_error_t {
  UART_PL011_ERR_NONE        = (0x0u),
  UART_PL011_ERR_RX_FRAME    = (0x1u<<UART_PL011_UARTRSR_FE_ERR_OFF),
  UART_PL011_ERR_RX_PARITY   = (0x1u<<UART_PL011_UARTRSR_PE_ERR_OFF),
  UART_PL011_ERR_RX_BREAK    = (0x1u<<UART_PL011_UARTRSR_BE_ERR_OFF),
  UART_PL011_ERR_RX_OVERFLOW = (0x1u<<UART_PL011_UARTRSR_OE_ERR_OFF),
  UART_PL011_ERR_INVALID_ARG = (UART_PL011_RX_ERR_MASK + 1),
  UART_PL011_ERR_NOT_READY,
  UART_PL011_ERR_INVALID_BAUD,
  UART_PL011_ERR_NOT_INIT,
};

/**
 * \brief ARM UART PL011 device configuration structure
 */
struct uart_pl011_dev_cfg_t {
    const uint32_t base;                         /*!< UART PL011 base address */
    const uint32_t def_baudrate;                        /*!< Default baudrate */
    const enum uart_pl011_wlen_t def_wlen;       /*!< Default word length */
    const enum uart_pl011_parity_t def_parity;        /*!< Default parity */
    const enum uart_pl011_stopbit_t def_stopbit;   /*!< Default stop bits */
};

/**
 * \brief ARM UART PL011 device data structure
 */
struct uart_pl011_dev_data_t {
    enum uart_pl011_state_t state;    /*!< UART State */
    uint32_t uart_clk;                    /*!< UART clock */
    uint32_t baudrate;                    /*!< Baudrate */
};

/**
 * \brief ARM UART PL011 device structure
 */
struct uart_pl011_dev_t {
    const struct uart_pl011_dev_cfg_t* const cfg;
                                              /*!< UART PL011 configuration */
    struct uart_pl011_dev_data_t* const data;
                                              /*!< UART PL011 data */
};

/**
 * \brief Initializes UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 * \param[in] uart_clk    UART clock used by the device.
 *
 * It uses the default baudrate to configure UART.
 *
 * \return Returns error code as specified in \ref uart_pl011_error_t
 *
 * \note This API should be called before calling any of the below UART APIs.
 * \note This function doesn't check if dev is NULL.
 */
enum uart_pl011_error_t uart_pl011_init(struct uart_pl011_dev_t* dev,
                    uint32_t uart_clk);

/**
 * \brief Uninitializes UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_uninit(struct uart_pl011_dev_t* dev);

/**
 * \brief Returns the UART PL011 operational state.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \return Returns the UART operational state
 *
 * \note This function doesn't check if dev is NULL.
 */
enum uart_pl011_state_t uart_pl011_get_state(struct uart_pl011_dev_t* dev);

/**
 * \brief Sets the UART baudrate.
 *
 * \param[in] dev       UART device struct \ref uart_pl011_dev_t
 * \param[in] baudrate  New baudrate.
 *
 * \return Returns error code as specified in \ref uart_pl011_error_t
 *
 * \note This function doesn't check if dev is NULL.
 */
enum uart_pl011_error_t uart_pl011_set_baudrate(
                    struct uart_pl011_dev_t* dev, uint32_t baudrate);

/**
 * \brief Gets the UART baudrate.
 *
 * \param[in] dev  UART device struct \ref uart_pl011_dev_t
 *
 * \return Returns the UART baudrate.
 *
 * \note The UART should be in valid state before calling this API
 *       \ref uart_pl011_get_state should return UART_PL011_INITIALIZED
 * \note This function doesn't check if dev is NULL.
 */
uint32_t uart_pl011_get_baudrate(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables UART interrupts
 *
 * \param[in] dev    UART device struct \ref uart_pl011_dev_t
 * \param[in] mask   Bit mask for enabling/disabling interrupts
 *                   \ref uart_pl011_intr_t
 *
 * \note User is responsible to configure the interrupt vector and
 *       the interrupt controller.
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_enable_intr(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_intr_t mask);

/**
 * \brief Disables UART interrupts
 *
 * \param[in] dev    UART device struct \ref uart_pl011_dev_t
 * \param[in] mask   Bit mask for enabling/disabling interrupts
 *                   \ref uart_pl011_intr_t
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_disable_intr(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_intr_t mask);

/**
 * \brief Clears UART Interrupt
 *
 * \param[in] dev  UART device struct \ref uart_pl011_dev_t
 * \param[in] mask Bit mask for clearing interrupts \ref uart_pl011_intr_t
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_clear_intr(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_intr_t mask);

/**
 * \brief Returns the UART Masked interrupt status
 *
 * \param[in] dev  UART device struct \ref uart_pl011_dev_t
 *
 * \return Masked interrupt status \ref uart_pl011_intr_t
 *
 * \note This function doesn't check if dev is NULL.
 */
enum uart_pl011_intr_t uart_pl011_get_masked_intr_status(
                    struct uart_pl011_dev_t* dev);

/**
 * \brief Returns the UART Raw interrupt status
 *
 * \param[in] dev  UART device struct \ref uart_pl011_dev_t
 *
 * \return Raw interrupt status \ref uart_pl011_intr_t
 *
 * \note This function doesn't check if dev is NULL.
 */
enum uart_pl011_intr_t uart_pl011_get_raw_intr_status(
                    struct uart_pl011_dev_t* dev);

/**
 * \brief Sets receive fifo levels
 *
 * \param[in] dev     UART device struct \ref uart_pl011_dev_t
 * \param[in] rx_lvl  Receive fifo levels \ref uart_pl011_rx_fifo_lvl_t
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_set_rx_fifo_lvl(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_rx_fifo_lvl_t rx_lvl);

/**
 * \brief Sets transmit fifo levels
 *
 * \param[in] dev     UART device struct \ref uart_pl011_dev_t
 * \param[in] tx_lvl  Transmit fifo levels \ref uart_pl011_tx_fifo_lvl_t
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_set_tx_fifo_lvl(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_tx_fifo_lvl_t tx_lvl);

/**
 * \brief Enables/Disables transmit UART DMA
 *
 * \param[in] dev     UART device struct \ref uart_pl011_dev_t
 * \param[in] enable  To enable/disable the UART transmit DMA
 *                    \ref uart_pl011_tx_dma_t
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_set_tx_dma(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_tx_dma_t enable);

/**
 * \brief Enables/Disables receive UART DMA
 *
 * \param[in] dev     UART device struct \ref uart_pl011_dev_t
 * \param[in] enable  To enable/disable the UART receive DMA
 *                    \ref uart_pl011_rx_dma_t
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_set_rx_dma(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_rx_dma_t enable);

/**
 * \brief Check if the UART dev is readable
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \return Returns bool, true if UART is readable, false otherwise
 *
 * \note This function doesn't check if dev is NULL.
 */
bool uart_pl011_is_readable(struct uart_pl011_dev_t* dev);

/**
 * \brief Reads one byte from UART dev.
 *
 * \param[in] dev    UART device struct \ref uart_pl011_dev_t
 * \param[out] byte  Pointer to byte that is populated with the data to be read
 *
 * \return Error status (if any) as specified in
 * \ref uart_pl011_error_t
 *
 * \note This API should only be called when the device is readable
 * \ref uart_pl011_is_readable
 * \note For better performance, this function doesn't check if dev and byte
 * pointer are NULL, and if the driver is initialized.
 */
enum uart_pl011_error_t uart_pl011_read(struct uart_pl011_dev_t* dev,
                    uint8_t* byte);

/**
 * \brief Check if the UART dev is writable
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \return Returns bool, true if UART is writable, false otherwise
 *
 * \note This function doesn't check if dev is NULL.
 */
bool uart_pl011_is_writable(struct uart_pl011_dev_t* dev);

/**
 * \brief Check if the UART tx buffer is empty
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \return Returns bool, true if UART is tx buffer is empty, false otherwise
 *
 * \note This function doesn't check if dev is NULL.
 */
bool uart_pl011_is_tx_empty(struct uart_pl011_dev_t* dev);

/**
 * \brief Writes a byte to UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 * \param[in] byte  One byte to write.
 *
 * \note This API should only be called when the device is writable
 * \ref uart_pl011_is_writable
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_write(struct uart_pl011_dev_t* dev, uint8_t byte);

/**
 * \brief Sets the UART format.
 *
 * \param[in] dev       UART device struct \ref uart_pl011_dev_t
 * \param[in] word_len  UART word length \ref uart_pl011_wlen_t
 * \param[in] parity    UART parity \ref uart_pl011_parity_t
 * \param[in] stop_bits UART stop bits \ref uart_pl011_stopbit_t
 *
 * \return Returns error code as specified in \ref uart_pl011_error_t
 *
 * \note This function doesn't check if dev is NULL.
 */
enum uart_pl011_error_t uart_pl011_set_format(struct uart_pl011_dev_t* dev,
                    enum uart_pl011_wlen_t word_len,
                    enum uart_pl011_parity_t parity,
                    enum uart_pl011_stopbit_t stop_bits);

/**
 * \brief Enables the UART fifo.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_enable_fifo(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables the UART fifo.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_disable_fifo(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables break transmission from UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_enable_break(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables break transmission from UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_disable_break(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables CTS flow control in UART PL011
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_enable_cts_flowcontrol(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables CTS flow control in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_disable_cts_flowcontrol(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables RTS flow control in UART PL011
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_enable_rts_flowcontrol(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables RTS flow control in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_disable_rts_flowcontrol(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables Data carrier detect in UART PL011
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \note For DTE nUARTOut1 can be used as Data carrier detect (DCD).
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_enable_dcd(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables Data carrier detect in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \note For DTE nUARTOut1 can be used as Data carrier detect (DCD).
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_disable_dcd(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables RTS signal for UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_set_rts(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables RTS signal for UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_clear_rts(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables DTR signal for UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_set_dtr(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables DTR signal for UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_clear_dtr(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables reception in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_enable_receive(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables reception in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_disable_receive(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables transmission in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_enable_transmit(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables transmission in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_disable_transmit(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables loopback in UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_set_loopback(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables loopback in UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_clear_loopback(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables IrDA Sir low power mode in UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_enable_sirlp(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables IrDA Sir in UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_disable_sirlp(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables IrDA Sir in UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_enable_sir(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables IrDA Sir in UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 *
 * \note For better performance, this function doesn't check if dev is NULL and
 * if the driver is initialized to have better performance.
 */
void uart_pl011_disable_sir(struct uart_pl011_dev_t* dev);

/**
 * \brief Enables in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_enable(struct uart_pl011_dev_t* dev);

/**
 * \brief Disables in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 *
 * \note This function doesn't check if dev is NULL.
 */
void uart_pl011_disable(struct uart_pl011_dev_t* dev);

/**
 * \brief Gets the Clear to send status in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \return Returns bool, true when the modem status input is 0, false otherwise
 *
 * \note This bit is the complement of the UART clear to send (nUARTCTS) modem
 *       status input.
 * \note This function doesn't check if dev is NULL.
 */
bool uart_pl011_get_cts_status(struct uart_pl011_dev_t* dev);

/**
 * \brief Gets the Data set ready status in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \return Returns bool, true when the modem status input is 0, false otherwise
 *
 * \note This bit is the complement of the UART UART data set ready (nUARTDSR)
 *       modem status input.
 * \note This function doesn't check if dev is NULL.
 */
bool uart_pl011_get_dsr_status(struct uart_pl011_dev_t* dev);

/**
 * \brief Gets the Data carrier detect status in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \return Returns bool, true when the modem status input is 0, false otherwise
 *
 * \note This bit is the complement of the UART data carrier detect (nUARTDCD)
 *       modem status input.
 * \note This function doesn't check if dev is NULL.
 */
bool uart_pl011_get_dcd_status(struct uart_pl011_dev_t* dev);

/**
 * \brief Gets the Ring indicator status in UART PL011.
 *
 * \param[in] dev         UART PL011 device struct \ref uart_pl011_dev_t
 *
 * \return Returns bool, true when the modem status input is 0, false otherwise
 *
 * \note This bit is the complement of the UART ring indicator (nUARTRI) modem
 *       status input.
 * \note This function doesn't check if dev is NULL.
 */
bool uart_pl011_get_ri_status(struct uart_pl011_dev_t* dev);

/**
 * \brief Sets the Low power Divisor in UART dev.
 *
 * \param[in] dev   UART device struct \ref uart_pl011_dev_t
 * \param[in] value Low power divisor value to be set
 *
 * \return Returns error code as specified in \ref uart_pl011_error_t
 *
 * \note For better performance, this function doesn't check if dev is NULL
 */
enum uart_pl011_error_t uart_pl011_set_sirlp_divisor(
            struct uart_pl011_dev_t* dev, uint32_t value);

#ifdef __cplusplus
}
#endif
#endif /* __UART_PL011_DRV_H__ */
