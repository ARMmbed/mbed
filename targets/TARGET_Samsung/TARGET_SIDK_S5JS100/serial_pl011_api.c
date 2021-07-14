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
#include "mbed_assert.h"
#include "gpio_api.h"

#define UART_PTR(ptr)   ((S5JS100_UART_TypeDef *)(ptr))
static uart_irq_handler irq_handler[PL011_UART_MAX];

struct serial_context_data_s {
    uint32_t serial_irq_id;
    gpio_t sw_rts, sw_cts;
    uint8_t count, rx_irq_set_flow, rx_irq_set_api;
};

static struct serial_context_data_s uart_data[PL011_UART_MAX];



/******************************************************************************
 * INTERRUPTS HANDLING
 ******************************************************************************/
static inline void uart_irq(t_pl011_ports_enum index,
                            UARTName uart)
{
    S5JS100_UART_TypeDef *p_PL011_UART = UART_PTR(uart);
    int irq_type = 0xFFFF; /* type none */

    if (!(p_PL011_UART->FR & 1u << 5)) {
        if (p_PL011_UART->IMSC & 1u << 5) {
            irq_type = TxIrq;
        }
    }

    if (p_PL011_UART->MIS & (1u << 4) || p_PL011_UART->MIS & (1u << 6)) {
        /*
                Rx Interrupt & Rx Timeout Interrupt
                The receive timeout interrupt is asserted when the receive FIFO is not empty,
            and no further data is received over a 32-bit period.
        */
        irq_type = RxIrq;
    }

    if (irq_type  == RxIrq) {
        if (uart_data[index].rx_irq_set_api) {
            (irq_handler[index])(uart_data[index].serial_irq_id, irq_type);
        }
    }

    if (irq_type == TxIrq) {
        /* Clear the TX interrupt Flag */
        /* UART TX */
    } else {
        /* Clear the Rx interupt Flag */
        /* UART RX */
    }
}


static void uart2_irq()
{
    uart_irq(PL011_UART0_ID, UART_2);
}

static void uart3_irq()
{
    uart_irq(PL011_UART1_ID, UART_3);
}


static void pl011_serial_irq_set_internal(void *obj, SerialIrq irq, uint32_t enable)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_UART_TypeDef *p_PL011_UART = UART_PTR(priv->uart);



    if (enable) {
        p_PL011_UART->IMSC = 0x50; //interrupt by Rx Timeout & Rx (for fifo mode)

    } else if ((irq == TxIrq) || (uart_data[priv->index].rx_irq_set_api
                                  + uart_data[priv->index].rx_irq_set_flow == 0)) {
        p_PL011_UART->IMSC = 0; //interrupt by Rx Timeout & Rx (for fifo mode)

    }
}



// serial_baud
// set the baud rate, taking in to account the current SystemFrequency
static void pl011_serial_baud(void *obj, int baudrate)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_UART_TypeDef *p_PL011_UART = UART_PTR(priv->uart);

    uint32_t sclk = 0;
    float div, frac;
    switch (priv->index) {
        case PL011_UART0_ID:
            sclk = cal_clk_getrate(d1_uart0);
            break;

        case PL011_UART1_ID:
            sclk = cal_clk_getrate(d1_uart1);
            break;

        default:
            MBED_ASSERT(false);
    }

    div = ((float)sclk / (float)(baudrate * 16));
    frac = (uint32_t)(((div - (int32_t)div) * 64));

    p_PL011_UART->IBRD = (uint32_t)div;
    p_PL011_UART->FBRD = (uint32_t)frac;
}

static void pl011_serial_format(void *obj, int data_bits,
                                SerialParity parity, int stop_bits)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_UART_TypeDef *p_PL011_UART = UART_PTR(priv->uart);
    uint32_t reg;

    switch (data_bits) {
        case 5:
            reg = 0;
            break;
        case 6:
            reg = 1 << 5;
            break;
        case 7:
            reg = 2 << 5;
            break;
        default:
            reg =  3 << 5;
    }

    switch (parity) {
        case ParityNone:
            reg |= 0;
            break;
        case ParityOdd:
            reg |= 4;
            break;
        case ParityEven:
            reg |= 6;
            break;
        case ParityForced1:
            reg |= 5;
            break;
        case ParityForced0:
            reg |= 7;
            break;
    }

    if (stop_bits == 2) {
        reg |= 1 << 3;
    }


    /* Enable FIFO */
    reg |= 1 << 4;


    p_PL011_UART->LCRH = reg;
}

static void pl011_serial_irq_handler(void *obj, uart_irq_handler handler, uint32_t id)
{
    struct serial_s *priv = (struct serial_s *)obj;

    irq_handler[priv->index] = handler;
    uart_data[priv->index].serial_irq_id = id;
}




static void pl011_serial_irq_set(void *obj, SerialIrq irq, uint32_t enable)
{
    struct serial_s *priv = (struct serial_s *)obj;

    if (RxIrq == irq) {
        uart_data[priv->index].rx_irq_set_api = enable;
    }

    pl011_serial_irq_set_internal(obj, irq, enable);
}


static int pl011_serial_readable(void *obj)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_UART_TypeDef *p_PL011_UART = UART_PTR(priv->uart);
    return !(p_PL011_UART->FR & (1u << 4));
}

static int pl011_serial_writable(void *obj)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_UART_TypeDef *p_PL011_UART = UART_PTR(priv->uart);
    return !(p_PL011_UART->FR & (1u << 5));
}

static int pl011_serial_serial_tx_empty(void *obj)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_UART_TypeDef *p_PL011_UART = UART_PTR(priv->uart);
    return ((p_PL011_UART->FR & (1u << 7)) && (!(p_PL011_UART->FR & (1u << 3))));
}


static int pl011_serial_getc(void *obj)

{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_UART_TypeDef *p_PL011_UART = UART_PTR(priv->uart);
    int data;
    while (!serial_readable(priv));
    data = p_PL011_UART->DR & 0xFF;

    return data;
}

static void pl011_serial_putc(void *obj, int c)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_UART_TypeDef *p_PL011_UART = UART_PTR(priv->uart);

    while (!serial_writable(priv));
    p_PL011_UART->DR = c;
}

#if DEVICE_SERIAL_FC
static void pl011_serial_set_flow_control(struct serial_s *obj, FlowControl type,
                                          PinName rxflow, PinName txflow)
{
    error("pl011 flow control is not implenemted");
}
#endif

void pl011_serial_init(void *obj, PinName tx, PinName rx)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_UART_TypeDef *p_PL011_UART = UART_PTR(priv->uart);

    /* Declare a variable of type IRQn, initialise to 0 */
    IRQn_Type irq_n = (IRQn_Type)0;
    uint32_t vector = 0;

    switch ((int)priv->uart) {
        case UART_2:
            irq_n = S5JS100_IRQ_UART0;
            vector = (uint32_t)&uart2_irq;
            priv->index = PL011_UART0_ID;
            break;
        case UART_3 :
            irq_n = S5JS100_IRQ_UART1;
            vector = (uint32_t)&uart3_irq;
            priv->index = PL011_UART1_ID;
            break;
    }


    /* Enable UART and RX/TX path*/
    p_PL011_UART->CR = 0x301;
    /* RX/TX fifo half full interrupt*/
    p_PL011_UART->IFLS = (2 << 3) | 2;
    /* clear all interrupts mask */
    p_PL011_UART->IMSC = 0x0;
    /* Clear all interripts */
    p_PL011_UART->ICR = 0x7FF;
    /* MODESEL as default UART*/
    p_PL011_UART->MODESEL = 0x0;

    priv->ops.serial_baud = pl011_serial_baud;
    priv->ops.serial_format = pl011_serial_format;
    priv->ops.serial_irq_handler = pl011_serial_irq_handler;
    priv->ops.serial_irq_set = pl011_serial_irq_set;
    priv->ops.serial_putc =  pl011_serial_putc;
    priv->ops.serial_writable =  pl011_serial_writable;
    priv->ops.serial_getc = pl011_serial_getc;
    priv->ops.serial_readable = pl011_serial_readable;
    priv->ops.serial_tx_empty = pl011_serial_tx_empty;
#if DEVICE_SERIAL_FC
    priv->ops.serial_set_flow_control = pl011_serial_set_flow_control;
#endif
    uart_data[priv->index].sw_rts.pin = NC;
    uart_data[priv->index].sw_cts.pin = NC;

    /* Assign IRQ in advance and enable, all are masked anyways */
    NVIC_SetVector(irq_n, vector);
#if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    SCB_InvalidateICache();
#endif
    NVIC_EnableIRQ(irq_n);

    /*  dissable IRQ by this if needed:
        NVIC_DisableIRQ(irq_n);
    */

}


#endif // DEVICE_SERIAL
