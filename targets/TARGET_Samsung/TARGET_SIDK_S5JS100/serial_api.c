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

#include "mbed_assert.h"



void usi_serial_init(void *obj, PinName tx, PinName rx);
void pl011_serial_init(void *obj, PinName tx, PinName rx);
void dummy_serial_init(void *obj, PinName tx, PinName rx);

/******************************************************************************
 * INITIALIZATION
 ******************************************************************************/

static const PinMap PinMap_UART_TX[] = {
    {UART_TX0, UART_0, UART_TX0},
    {UART_TX1, UART_1, UART_TX1},
    {UART_TX2, UART_2, UART_TX2},
    {UART_TX3, UART_3, UART_TX3},
    {UART_TX4, UART_4, UART_TX4},
    {UART_TX5, UART_5, UART_TX5},
    {NC, NC, NC}
};
static const PinMap PinMap_UART_RX[] = {
    {UART_RX0, UART_0, UART_RX0},
    {UART_RX1, UART_1, UART_RX1},
    {UART_RX2, UART_2, UART_RX2},
    {UART_RX3, UART_3, UART_RX3},
    {UART_RX4, UART_4, UART_RX4},
    {UART_RX5, UART_5, UART_RX5},
    {NC, NC, NC}
};

/*
static const PinMap PinMap_UART_CTS[] = {
    {UART0_CTS, UART_0, UART0_CTS},
    {UART1_CTS, UART_1, UART1_CTS},
    {UART2_CTS, UART_2, UART2_CTS},
    {UART3_CTS, UART_3, UART3_CTS},
    {NC, NC, NC}
};

static const PinMap PinMap_UART_RTS[] = {
    {UART0_RTS, UART_0, UART0_RTS},
    {UART1_RTS, UART_1, UART1_RTS},
    {UART2_RTS, UART_2, UART2_RTS},
    {UART3_RTS, UART_3, UART3_RTS},
    {NC, NC, NC}
};
*/


/* Used in platform/mbed_retarget.cpp  */
/* What shall I do with it??? What is it for? */
int stdio_uart_inited = 0;
serial_t stdio_uart;


void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    struct serial_s *priv = (struct serial_s *)obj;

    // determine the UART to use ???
    // Shall we check if it is already allocated ???
    UARTName uart_tx = (UARTName)pinmap_peripheral(tx, PinMap_UART_TX);
    UARTName uart_rx = (UARTName)pinmap_peripheral(rx, PinMap_UART_RX);
    UARTName uart = (UARTName)pinmap_merge(uart_tx, uart_rx);
    if ((PinName)uart == NC) {
        error("Serial pinout mapping failed");
    }
    if ((PinName)uart_tx != NC) {
        s5js100_configgpio(pinmap_find_function(tx, PinMap_UART_TX));
    }

    if ((PinName)uart_rx != NC) {
        s5js100_configgpio(pinmap_find_function(rx, PinMap_UART_RX));
    }

    /* BAD Pointer assignment!!! Better to redesign*/
    priv->uart = (void *)uart;
    switch (uart) {
        case UART_0:
        case UART_1:
            usi_serial_init(obj, tx, rx);
            break;

        case UART_2:
        case UART_3:
            pl011_serial_init(obj, tx, rx);
            break;

        case UART_4:
        case UART_5:
            dummy_serial_init(obj, tx, rx);
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

    UARTName uart_rts = (UARTName)pinmap_peripheral(rxflow, PinMap_UART_RTS);
    UARTName uart_cts = (UARTName)pinmap_peripheral(txflow, PinMap_UART_CTS);
    UARTName uart = (UARTName)pinmap_merge(uart_rts, uart_cts);
    if ((PinName)uart == NC) {
        error("Serial pinout mapping failed");
    }

    if ((int)uart != (int)priv->uart) {
        error("Serial pinout mapping failed");
    }


    if (type == FlowControlRTS) {
        // Enable RTS
        MBED_ASSERT(uart_rts != (UARTName)NC);
        // Enable the pin for RTS function
        s5js100_configgpio(pinmap_find_function(rxflow, PinMap_UART_RTS));
    }
    if (type == FlowControlCTS) {
        // Enable CTS
        MBED_ASSERT(uart_cts != (UARTName)NC);
        // Enable the pin for CTS function
        s5js100_configgpio(pinmap_find_function(txflow, PinMap_UART_CTS));
    }
    if (type == FlowControlRTSCTS) {
        // Enable CTS & RTS
        MBED_ASSERT(uart_rts != (UARTName)NC);
        MBED_ASSERT(uart_cts != (UARTName)NC);
        // Enable the pin for CTS function
        s5js100_configgpio(pinmap_find_function(txflow, PinMap_UART_CTS));
        // Enable the pin for RTS function
        s5js100_configgpio(pinmap_find_function(rxflow, PinMap_UART_RTS));
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

int serial_tx_empty(serial_t *obj)
{
    return obj->ops.serial_tx_empty(obj);
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

#endif // DEVICE_SERIAL
