/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
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
#include "mbed_assert.h"
#include "serial_api.h"

// math.h required for floating point operations for baud rate calculation
#include <math.h>

#include <string.h>

#include "cmsis.h"
#include "pinmap.h"
#include "clk_freqs.h"
#include "PeripheralPins.h"

#define UART_NUM        2

/******************************************************************************
 * INITIALIZATION
 ******************************************************************************/

static uint32_t serial_irq_ids[UART_NUM] = {0};
static uart_irq_handler irq_handler;

int stdio_uart_inited = 0;
serial_t stdio_uart;

void serial_init(serial_t *obj, PinName tx, PinName rx) {
    // determine the UART to use
    UARTName uart_tx = (UARTName)pinmap_peripheral(tx, PinMap_UART_TX);
    UARTName uart_rx = (UARTName)pinmap_peripheral(rx, PinMap_UART_RX);
    UARTName uart = (UARTName)pinmap_merge(uart_tx, uart_rx);
    MBED_ASSERT((int)uart != NC);

    obj->uart = (LPUART_Type *)uart;

    // enable clk
    switch (uart) {
        case UART_0:
          SIM->SOPT2 |= SIM_SOPT2_LPUART0SRC(1);
          SIM->SCGC5 |= SIM_SCGC5_LPUART0_MASK;
          break;
        case UART_1:
          SIM->SOPT2 |= SIM_SOPT2_LPUART1SRC(1);
          SIM->SCGC5 |= SIM_SCGC5_LPUART1_MASK;
          break;
        case UART_2: /* TODO: add UART2 support */ break;
    }

    obj->uart->CTRL &= ~(LPUART_CTRL_RE_MASK | LPUART_CTRL_TE_MASK);

    switch (uart) {
        case UART_0: obj->index = 0; break;
        case UART_1: obj->index = 1; break;
        case UART_2: /* TODO: add UART2 support */ break;
    }

    // set default baud rate and format
    serial_baud  (obj, 9600);
    serial_format(obj, 8, ParityNone, 1);

    // pinout the chosen uart
    pinmap_pinout(tx, PinMap_UART_TX);
    pinmap_pinout(rx, PinMap_UART_RX);

    // set rx/tx pins in PullUp mode
    if (tx != NC) pin_mode(tx, PullUp);
    if (rx != NC) pin_mode(rx, PullUp);

    obj->uart->CTRL |= (LPUART_CTRL_RE_MASK | LPUART_CTRL_TE_MASK);

    if (uart == STDIO_UART) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }
}

void serial_free(serial_t *obj) {
    serial_irq_ids[obj->index] = 0;
}

// serial_baud
//
// set the baud rate, taking in to account the current SystemFrequency
void serial_baud(serial_t *obj, int baudrate) {

	uint32_t PCLK;
	PCLK = bus_frequency() / (baudrate * 16);

    uint32_t temp;
    /* save C2 state */
    temp = obj->uart->CTRL & (LPUART_CTRL_RE_MASK | LPUART_CTRL_TE_MASK);

    /* disable UART before changing registers */
    obj->uart->CTRL &= ~(LPUART_CTRL_RE_MASK | LPUART_CTRL_TE_MASK);

    /* reset old baudrate settings */
    obj->uart->BAUD &= ~(LPUART_BAUD_SBR_MASK | LPUART_BAUD_OSR_MASK | LPUART_BAUD_BOTHEDGE_MASK);

    /* write the new osr and sbr values to the BAUD register */
    obj->uart->BAUD |= LPUART_BAUD_SBR(PCLK) | LPUART_BAUD_OSR(PCLK);

    /* restore C2 state */
    obj->uart->CTRL |= temp;
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits) {
    MBED_ASSERT((stop_bits == 1) || (stop_bits == 2));
    MBED_ASSERT((parity == ParityNone) || (parity == ParityOdd) || (parity == ParityEven));
    MBED_ASSERT(data_bits == 8); // TODO: Support other number of data bits (also in the write method!)

    // save C2 state
    uint32_t c2_state = obj->uart->CTRL & (LPUART_CTRL_RE_MASK | LPUART_CTRL_TE_MASK);

    // disable UART before changing registers
    obj->uart->CTRL &= ~(LPUART_CTRL_RE_MASK | LPUART_CTRL_TE_MASK);


    uint8_t parity_enable = 0, parity_select = 0;
    switch (parity) {
        case ParityNone: parity_enable = 0; parity_select = 0; break;
        case ParityOdd : parity_enable = 1; parity_select = 1; data_bits++; break;
        case ParityEven: parity_enable = 1; parity_select = 0; data_bits++; break;
        default:
            break;
    }

    stop_bits -= 1;

    // data bits, parity and parity mode
    obj->uart->CTRL = ((parity_enable << 1) |  (parity_select << 0));

    // stop bits
    obj->uart->BAUD &= ~LPUART_BAUD_SBNS_MASK;
    obj->uart->BAUD |= (stop_bits << LPUART_BAUD_SBNS_SHIFT);

    // restore C2 state
    obj->uart->CTRL |= c2_state;
}

/******************************************************************************
 * INTERRUPTS HANDLING
 ******************************************************************************/
static inline void uart_irq(uint32_t status, uint32_t index) {
    if (serial_irq_ids[index] != 0) {
        if (status & LPUART_STAT_TDRE_MASK)
            irq_handler(serial_irq_ids[index], TxIrq);

        if (status & LPUART_STAT_RDRF_MASK)
            irq_handler(serial_irq_ids[index], RxIrq);
    }
}

void uart0_irq() {uart_irq(LPUART0->STAT, 0);}
void uart1_irq() {uart_irq(LPUART1->STAT, 1);}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id) {
    irq_handler = handler;
    serial_irq_ids[obj->index] = id;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable) {
    IRQn_Type irq_n = (IRQn_Type)0;
    uint32_t vector = 0;
    switch ((int)obj->uart) {
        case UART_0: irq_n=LPUART0_IRQn; vector = (uint32_t)&uart0_irq; break;
        case UART_1: irq_n=LPUART1_IRQn; vector = (uint32_t)&uart1_irq; break;
    }

    if (enable) {
        switch (irq) {
            case RxIrq: obj->uart->CTRL |= LPUART_CTRL_RIE_MASK; break;
            case TxIrq: obj->uart->CTRL |= LPUART_CTRL_TIE_MASK; break;
        }
        NVIC_SetVector(irq_n, vector);
        NVIC_EnableIRQ(irq_n);

    } else { // disable
        int all_disabled = 0;
        SerialIrq other_irq = (irq == RxIrq) ? (TxIrq) : (RxIrq);
        switch (irq) {
            case RxIrq: obj->uart->CTRL &= ~(LPUART_CTRL_RIE_MASK); break;
            case TxIrq: obj->uart->CTRL &= ~(LPUART_CTRL_TIE_MASK); break;
        }
        switch (other_irq) {
            case RxIrq: all_disabled = (obj->uart->CTRL & LPUART_CTRL_RIE_MASK) == 0; break;
            case TxIrq: all_disabled = (obj->uart->CTRL & LPUART_CTRL_TIE_MASK) == 0; break;
        }
        if (all_disabled)
            NVIC_DisableIRQ(irq_n);
    }
}

/******************************************************************************
 * READ/WRITE
 ******************************************************************************/
int serial_getc(serial_t *obj) {
    while (!serial_readable(obj));
    return (obj->uart->DATA & 0xFFu);
}

void serial_putc(serial_t *obj, int c) {
    while (!serial_writable(obj));
    obj->uart->DATA = c;
}

int serial_readable(serial_t *obj) {
    // check overrun
    if (obj->uart->STAT &  LPUART_STAT_OR_MASK) {
        obj->uart->STAT |= LPUART_STAT_OR_MASK;
    }
    return (obj->uart->STAT & LPUART_STAT_RDRF_MASK);
}

int serial_writable(serial_t *obj) {
    // check overrun
    if (obj->uart->STAT &  LPUART_STAT_OR_MASK) {
        obj->uart->STAT |= LPUART_STAT_OR_MASK;
    }
    return (obj->uart->STAT & LPUART_STAT_TDRE_MASK);
}

void serial_clear(serial_t *obj) {
}

void serial_pinout_tx(PinName tx) {
    pinmap_pinout(tx, PinMap_UART_TX);
}

void serial_break_set(serial_t *obj) {
    obj->uart->CTRL |= LPUART_CTRL_SBK_MASK;
}

void serial_break_clear(serial_t *obj) {
    obj->uart->CTRL &= ~LPUART_CTRL_SBK_MASK;
}

