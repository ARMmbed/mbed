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


void (*dummy_uart_write_redirect)(int c);


// serial_baud
// set the baud rate, taking in to account the current SystemFrequency
static void dummy_serial_baud(void *obj, int baudrate)
{
}

static void dummy_serial_format(void *obj, int data_bits,
                                SerialParity parity, int stop_bits)
{
}

static void dummy_serial_irq_handler(void *obj, uart_irq_handler handler, uint32_t id)
{
}

static void dummy_serial_irq_set(void *obj, SerialIrq irq, uint32_t enable)
{
}


static int dummy_serial_readable(void *obj)
{
    return 0;
}

static int dummy_serial_writable(void *obj)
{
    return 1;
}

static int dummy_serial_tx_empty(void *obj)
{
    return 1;
}


static int dummy_serial_getc(void *obj)

{

    return 0;
}

static void dummy_serial_putc(void *obj, int c)
{
    if (dummy_uart_write_redirect) {
        dummy_uart_write_redirect(c);
    }
}

#if DEVICE_SERIAL_FC
static void dummy_serial_set_flow_control(struct serial_s *obj, FlowControl type,
                                          PinName rxflow, PinName txflow)
{
    error("dummy flow control is not implenemted");
}
#endif

void dummy_serial_init(void *obj, PinName tx, PinName rx)
{
    struct serial_s *priv = (struct serial_s *)obj;

    switch ((int)priv->uart) {
        case UART_4:
            priv->index = DUMMY_UART0_ID;
            break;
        case UART_5 :
            priv->index = DUMMY_UART1_ID;
            break;
    }

    priv->ops.serial_baud = dummy_serial_baud;
    priv->ops.serial_format = dummy_serial_format;
    priv->ops.serial_irq_handler = dummy_serial_irq_handler;
    priv->ops.serial_irq_set = dummy_serial_irq_set;
    priv->ops.serial_putc =  dummy_serial_putc;
    priv->ops.serial_writable =  dummy_serial_writable;
    priv->ops.serial_getc = dummy_serial_getc;
    priv->ops.serial_readable = dummy_serial_readable;
    priv->ops.serial_tx_empty = dummy_serial_tx_empty;
#if DEVICE_SERIAL_FC
    priv->ops.serial_set_flow_control = dummy_serial_set_flow_control;
#endif
    dummy_uart_write_redirect = NULL;
}


#endif // DEVICE_SERIAL
