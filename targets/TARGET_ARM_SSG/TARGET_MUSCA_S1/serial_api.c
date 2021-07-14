/* mbed Microcontroller Library
 * Copyright (c) 2017-2019 Arm Limited
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

#include <string.h>

#include "mbed_error.h"
#include "objects.h"
#include "pinmap.h"
#include "serial_api.h"
#include "mbed_serial_platform.h"

#define STDIO_UART_NOT_INITED 0
#define STDIO_UART_INITED     1

extern const PinMap PinMap_UART_TX[];
extern const PinMap PinMap_UART_RX[];

/* Global variables needed for mbed */
int stdio_uart_inited = STDIO_UART_NOT_INITED;
serial_t stdio_uart;


void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    /* Determine the UART to use */
    UARTName uart_tx = (UARTName)pinmap_peripheral(tx, PinMap_UART_TX);
    UARTName uart_rx = (UARTName)pinmap_peripheral(rx, PinMap_UART_RX);
    UARTName uart = (UARTName)pinmap_merge(uart_tx, uart_rx);
    if (uart == (UARTName)NC) {
        error("Serial pinout mapping failed");
    }
    mbed_uart_platform_init(obj, uart);
    obj->uart_index = uart;

    /* Pinout the chosen uart */
    pinmap_pinout(tx, PinMap_UART_TX);
    pinmap_pinout(rx, PinMap_UART_RX);

    uart_pl011_init(obj->uart_dev, SystemCoreClock);

    uart_pl011_set_baudrate(obj->uart_dev,
        MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE);

    uart_pl011_enable(obj->uart_dev);

    if (uart == STDIO_UART) {
        stdio_uart_inited = STDIO_UART_INITED;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }
}

void serial_free(serial_t *obj)
{
    uart_pl011_uninit(obj->uart_dev);
}

void serial_baud(serial_t *obj, int baudrate)
{
    if(uart_pl011_set_baudrate(obj->uart_dev,baudrate) != UART_PL011_ERR_NONE) {
        error("serial_baud: invalid baudrate");
    }
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity,
                   int stop_bits)
{
    enum uart_pl011_wlen_t uart_word_len = UART_PL011_WLEN_8;
    enum uart_pl011_parity_t uart_parity = UART_PL011_PARITY_DISABLED;
    enum uart_pl011_stopbit_t uart_stop_bits = UART_PL011_STOPBIT_1;
    switch (data_bits) {
    case 5:
        uart_word_len = UART_PL011_WLEN_5;
        break;
    case 6:
        uart_word_len = UART_PL011_WLEN_6;
        break;
    case 7:
        uart_word_len = UART_PL011_WLEN_7;
        break;
    case 8:
        uart_word_len = UART_PL011_WLEN_8;
        break;
    default:
        error("serial_format: unexpected data bits");
    }
    switch (parity) {
    case ParityNone:
        uart_parity = UART_PL011_PARITY_DISABLED;
        break;
    case ParityOdd:
        uart_parity = UART_PL011_PARITY_ODD;
        break;
    case ParityEven:
        uart_parity = UART_PL011_PARITY_EVEN;
        break;
    case ParityForced1:
        uart_parity = UART_PL011_PARITY_STICKY_ONE;
        break;
    case ParityForced0:
        uart_parity = UART_PL011_PARITY_STICKY_ZERO;
        break;
    default:
        error("serial_format: unexpected parity");
    }
    switch (stop_bits) {
    case 1:
        uart_stop_bits = UART_PL011_STOPBIT_1;
        break;
    case 2:
        uart_stop_bits = UART_PL011_STOPBIT_2;
        break;
    default:
        error("serial_format: unexpected stop bit");
    }
    (void)uart_pl011_set_format(obj->uart_dev,
                            uart_word_len, uart_parity, uart_stop_bits);
}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    uart_irq[obj->uart_index].handler = handler;
    uart_irq[obj->uart_index].id = id;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    switch(irq) {
    case RxIrq:
        if (enable) {
            NVIC_EnableIRQ(obj->rx_irq);
            uart_pl011_enable_intr(obj->uart_dev, UART_PL011_RX_INTR_MASK);
            NVIC_EnableIRQ(obj->rx_timeout_irq);
            uart_pl011_enable_intr(obj->uart_dev, UART_PL011_RT_INTR_MASK);
        } else {
            uart_pl011_disable_intr(obj->uart_dev, UART_PL011_RX_INTR_MASK);
            NVIC_DisableIRQ(obj->rx_irq);
            uart_pl011_disable_intr(obj->uart_dev, UART_PL011_RT_INTR_MASK);
            NVIC_DisableIRQ(obj->rx_timeout_irq);
        }
        break;
    case TxIrq:
        if (enable) {
            NVIC_EnableIRQ(obj->tx_irq);
            uart_pl011_enable_intr(obj->uart_dev, UART_PL011_TX_INTR_MASK);
        } else {
            uart_pl011_disable_intr(obj->uart_dev, UART_PL011_TX_INTR_MASK);
            NVIC_DisableIRQ(obj->tx_irq);
        }
        break;
    default : return;
    }
}

int serial_readable(serial_t *obj)
{
    return (int)uart_pl011_is_readable(obj->uart_dev);
}

int serial_writable(serial_t *obj)
{
    return (int)uart_pl011_is_writable(obj->uart_dev);
}

int serial_tx_empty(serial_t *obj)
{
    return (int)uart_pl011_is_tx_empty(obj->uart_dev);
}

int serial_getc(serial_t *obj)
{
    uint8_t byte = 0;
    while(!uart_pl011_is_readable(obj->uart_dev));
    (void)uart_pl011_read(obj->uart_dev, &byte);
    return byte;
}

void serial_putc(serial_t *obj, int c)
{
    while(!uart_pl011_is_writable(obj->uart_dev));
    uart_pl011_write(obj->uart_dev, (uint8_t)c);
}

void serial_clear(serial_t *obj)
{
    uart_pl011_write(obj->uart_dev, (uint8_t)0);
}

void serial_pinout_tx(PinName tx)
{
    pinmap_pinout(tx, PinMap_UART_TX);
}

void serial_break_set(serial_t *obj)
{
    uart_pl011_enable_break(obj->uart_dev);
}

void serial_break_clear(serial_t *obj)
{
    uart_pl011_disable_break(obj->uart_dev);
}

void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow,
                             PinName txflow)
{
    error("serial_set_flow_control: Flow control is not supported in MUSCA");
}
