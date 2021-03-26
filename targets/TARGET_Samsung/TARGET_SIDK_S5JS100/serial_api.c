/****************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#if DEVICE_SERIAL

// math.h required for floating point operations for baud rate calculation
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "device.h"
#include "serial_api.h"
#include "cmsis.h"
#include "pinmap.h"
#include "PinNames.h"
#include "mbed_error.h"
#include "gpio_api.h"
#include "PeripheralPins.h"

#include "mbed_assert.h"
#include "s5js100.h"


void usi_serial_init(void *obj, PinName tx, PinName rx);
void pl011_serial_init(void *obj, PinName tx, PinName rx);
void dummy_serial_init(void *obj, PinName tx, PinName rx);

/* Used in platform/mbed_retarget.cpp  */
/* What shall I do with it??? What is it for? */
int stdio_uart_inited = 0;
serial_t stdio_uart;


void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    struct serial_s *priv = (struct serial_s *)obj;

    if (!obj) {
        return;
    }
    // determine the UART to use ???
    // Shall we check if it is already allocated ???
    UARTName uart_tx = (UARTName)pinmap_peripheral(tx, PinMap_UART_TX);
    UARTName uart_rx = (UARTName)pinmap_peripheral(rx, PinMap_UART_RX);
    UARTName uart = (UARTName)pinmap_merge(uart_tx, uart_rx);
    if ((PinName)uart == NC) {
        error("Serial pinout mapping failed");
    }

    int peripheral = uart;
    int tx_function = pinmap_function(tx, PinMap_UART_TX);
    int rx_function = pinmap_function(rx, PinMap_UART_RX);

    const serial_pinmap_t pinmap = {peripheral,
                                    tx,
                                    tx_function,
                                    rx,
                                    rx_function,
                                    0
                                   };

    serial_init_direct(obj, &pinmap);
}

void serial_init_direct(serial_t *obj, const serial_pinmap_t *pinmap)
{
    struct serial_s *priv = (struct serial_s *)obj;
    PinName tx_pin = pinmap->tx_pin;
    PinName rx_pin = pinmap->rx_pin;
    UARTName uart = (UARTName)pinmap->peripheral;

    if (!obj || !pinmap) {
        return;
    }

    if (pinmap->tx_function != NC) {
        s5js100_configgpio(pinmap->tx_function);
    } else {
        error("Serial tx pin function error");
    }

    if (pinmap->rx_function != NC) {
        s5js100_configgpio(pinmap->rx_function);
    } else {
        error("Serial rx pin function error");
    }

    /* BAD Pointer assignment!!! Better to redesign*/
    priv->uart = (void *)uart;
    if (uart == NC) {
        error("Serial peripheral error");
    }

    switch (uart) {
        case UART_0:
        case UART_1:
            usi_serial_init(obj, tx_pin, rx_pin);
            break;

        case UART_2:
        case UART_3:
            pl011_serial_init(obj, tx_pin, rx_pin);
            break;

        case UART_4:
        case UART_5:
            dummy_serial_init(obj, tx_pin, rx_pin);
            break;
    }

    // set default baud rate and format
    obj->ops.serial_baud(obj, 115200);
    obj->ops.serial_format(obj, 8, ParityNone, 1);

    if (uart == STDIO_UART) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }
}

void serial_free(serial_t *obj)
{
    //needs release serial
    struct serial_s *priv = (struct serial_s *)obj;
    IRQn_Type irq_n = (IRQn_Type)0;

    if (!obj) {
        return;
    }

    switch ((int)priv->uart) {
        case UART_0:
            irq_n = S5JS100_IRQ_USI0;
            break;
        case UART_1:
            irq_n = S5JS100_IRQ_USI1;
            break;
        case UART_2:
            irq_n = S5JS100_IRQ_UART0;
            break;
        case UART_3:
            irq_n = S5JS100_IRQ_UART1;
            break;
        default:
            break;
    }

    NVIC_DisableIRQ(irq_n);

    priv->ops.serial_baud = NULL;
    priv->ops.serial_format = NULL;
    priv->ops.serial_irq_handler = NULL;
    priv->ops.serial_irq_set = NULL;
    priv->ops.serial_putc =  NULL;
    priv->ops.serial_writable =  NULL;
    priv->ops.serial_getc = NULL;
    priv->ops.serial_readable = NULL;
#if DEVICE_SERIAL_FC
    priv->ops.serial_set_flow_control = NULL;
#endif

    priv->uart = NULL;
}

// serial_baud
// set the baud rate, taking in to account the current SystemFrequency
void serial_baud(serial_t *obj, int baudrate)
{
    obj->ops.serial_baud(obj, baudrate);

}

void serial_format(serial_t *obj, int data_bits,
                   SerialParity parity, int stop_bits)
{
    obj->ops.serial_format(obj, data_bits, parity, stop_bits);
}


void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    obj->ops.serial_irq_handler(obj, handler, id);
}


void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    obj->ops.serial_irq_set(obj, irq, enable);
}

#if DEVICE_SERIAL_FC
void serial_set_flow_control(serial_t *obj, FlowControl type,
                             PinName rxflow, PinName txflow)
{
    struct serial_s *priv = (struct serial_s *)obj;

    if (!obj) {
        return;
    }

    UARTName uart_rts = (UARTName)pinmap_peripheral(rxflow, PinMap_UART_RTS);
    UARTName uart_cts = (UARTName)pinmap_peripheral(txflow, PinMap_UART_CTS);
    UARTName uart = (UARTName)pinmap_merge(uart_rts, uart_cts);
    if ((PinName)uart == NC) {
        error("Serial pinout mapping failed");
    }

    if ((int)uart != (int)priv->uart) {
        error("Serial pinout mapping failed");
    }

    int peripheral = uart;
    int tx_function = pinmap_function(txflow, PinMap_UART_TX);
    int rx_function = pinmap_function(rxflow, PinMap_UART_RX);

    const serial_pinmap_t pinmap = {peripheral,
                                    txflow,
                                    tx_function,
                                    rxflow,
                                    rx_function,
                                    0
                                   };

    serial_set_flow_control_direct(obj, type, &pinmap);
}

void serial_set_flow_control_direct(serial_t *obj, FlowControl type,
                                    const serial_fc_pinmap_t *pinmap)
{
    struct serial_s *priv = (struct serial_s *)obj;
    PinName rxflow = pinmap->rx_pin;
    PinName txflow = pinmap->tx_pin;

    if (!obj || !pinmap) {
        return;
    }

    rxflow = pinmap->rx_pin;
    txflow = pinmap->tx_pin;

    if (type == FlowControlRTS) {
        // Enable RTS
        MBED_ASSERT(((UARTName))pinmap->tx_function != (UARTName)NC);
        // Enable the pin for RTS function
        s5js100_configgpio(pinmap->tx_function);
    }
    if (type == FlowControlCTS) {
        // Enable CTS
        MBED_ASSERT(((UARTName))pinmap->rx_function != (UARTName)NC);
        // Enable the pin for CTS function
        s5js100_configgpio(pinmap->rx_function);
    }
    if (type == FlowControlRTSCTS) {
        // Enable CTS & RTS
        MBED_ASSERT(((UARTName))pinmap->tx_function != (UARTName)NC);
        MBED_ASSERT(((UARTName))pinmap->rx_function != (UARTName)NC);
        // Enable the pin for CTS function
        s5js100_configgpio(pinmap->tx_function);
        // Enable the pin for RTS function
        s5js100_configgpio(pinmap->rx_function);
    }

    obj->ops.serial_set_flow_control(obj, type, rxflow, txflow);
}

#endif


/******************************************************************************
 * READ/WRITE
 ******************************************************************************/
int serial_getc(serial_t *obj)
{
    return obj->ops.serial_getc(obj);
}

void serial_putc(serial_t *obj, int c)
{
    obj->ops.serial_putc(obj, c);
}

int serial_readable(serial_t *obj)
{
    return obj->ops.serial_readable(obj);
}

int serial_writable(serial_t *obj)
{
    return obj->ops.serial_writable(obj);
}


/* Shall it be ever used ???
void serial_clear(serial_t *obj)
{
    if (obj->index < 2)
    {
        USI_PTR(obj->uart)->URXH = 0x00;
        while ((getreg32(0x83015018) & UART_UFSTAT_TX_FIFO_FULL));
        while ((getreg32(0x83015018) & UART_UFSTAT_TX_FIFO_COUNT_MASK));
        putreg8('C', 0x83015020);
        while ((getreg32(0x83015018) & UART_UFSTAT_TX_FIFO_FULL));
        while ((getreg32(0x83015018) & UART_UFSTAT_TX_FIFO_COUNT_MASK));
        putreg8('C', 0x83015020);
        putreg8('C', 0x83015020);
        putreg8('C', 0x83015020);
        putreg8('C', 0x83015020);
        putreg8('C', 0x83015020);
        putreg8('C', 0x83015020);
    }
    else
    {
        UART_PTR(obj->uart)->DR = 0x00;
        while (UART_PTR(obj->uart)->FR & (1u << 5));
        while (UART_PTR(obj->uart)->FR & (1u << 7));
        UART_PTR(obj->uart)->DR = 'C';
        while (UART_PTR(obj->uart)->FR & (1u << 5));
        while (UART_PTR(obj->uart)->FR & (1u << 7));
        UART_PTR(obj->uart)->DR = 'C';
        UART_PTR(obj->uart)->DR = 'C';
        UART_PTR(obj->uart)->DR = 'C';
        UART_PTR(obj->uart)->DR = 'C';
        UART_PTR(obj->uart)->DR = 'C';
        UART_PTR(obj->uart)->DR = 'C';
    }
}
*/

/* Used in
 * features/unsupported/tests/libs/SerialHalfDuplex/SerialHalfDuplex.cpp
 * What is it for?
 */
void serial_pinout_tx(PinName tx)
{
    pinmap_pinout(tx, PinMap_UART_TX);
}


/* Serial break may never be used.
 * In general set TX to "0".
 * However - shall we?
 */

void serial_break_set(serial_t *obj)
{
}

void serial_break_clear(serial_t *obj)
{
}


/* pinmap test entry */

const PinMap *serial_tx_pinmap()
{
    return PinMap_UART_TX;
}

const PinMap *serial_rx_pinmap()
{
    return PinMap_UART_RX;
}

const PinMap *serial_cts_pinmap(void)
{
    return PinMap_UART_CTS;
}

const PinMap *serial_rts_pinmap(void)
{
    return PinMap_UART_RTS;
}

#endif // DEVICE_SERIAL
