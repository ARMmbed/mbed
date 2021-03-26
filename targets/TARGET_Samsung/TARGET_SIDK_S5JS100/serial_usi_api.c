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

#include "s5js100.h"
#include "mbed_wait_api.h"
#include "mbed_thread.h"

#define USI1_RX_OFFSET    0x0130
#define USI1_TX_OFFSET    0x0134
#define USI_PTR(ptr) ((S5JS100_USI_UART_TypeDef *)(ptr))
static uart_irq_handler irq_handler[USI_MAX_PORTS];
static uint32_t serial_irq_id[USI_MAX_PORTS] = {0};




/****************** Internal code ************************/
static inline void uart_irq(uint32_t intstatus, uint32_t index,
                            UARTName uart)
{

    if (intstatus & (1 << 0)) {
        (irq_handler[index])(serial_irq_id[index], RxIrq);
    } else if (intstatus & (1 << 1)) {
        // RTS Int
    } else if (intstatus & (1 << 2)) {
        (irq_handler[index])(serial_irq_id[index], TxIrq);
    } else if (intstatus & (1 << 3)) {
        // CTS Int
    } else {
    }

    USI_PTR(uart)->UINTP = intstatus;
}




static void uart0_irq()
{
    uart_irq(S5JS100_USI0_UART->UINTP & 0xF, 0, UART_0);
}

static void uart1_irq()
{
    uart_irq(S5JS100_USI1_UART->UINTP & 0xF, 1, UART_1);
}


/*********************** API ******************************/

// serial_baud
// set the baud rate, taking in to account the current SystemFrequency
static void usi_serial_baud(void *obj, int baudrate)
{
    struct serial_s *priv = (struct serial_s *)obj;
    int32_t sclk = 0;
    float div, frac;
    S5JS100_USI_UART_TypeDef *p_USI_UART = USI_PTR(priv->uart);
    switch ((int)priv->uart) {
        case UART_0:
            /* UBRDIV and UFRACVAL */
            sclk = cal_clk_getrate(d1_usi0);
            break;

        case UART_1:
            /* UBRDIV and UFRACVAL */
            sclk = cal_clk_getrate(d1_usi1);
            break;

        default:
            MBED_ASSERT(false);
    }


    div = ((float)sclk / (float)(baudrate * 16)) - 1.0;
    frac = (uint32_t)(((div - (int32_t)div) * 16));
    p_USI_UART->UBRDIV = (uint32_t)div;
    p_USI_UART->UFRACVAL = (uint32_t)frac;
}


static void usi_serial_format(void *obj, int data_bits,
                              SerialParity parity, int stop_bits)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_USI_UART_TypeDef *p_USI_UART = USI_PTR(priv->uart);
    uint32_t reg;

    switch (data_bits) {
        case 5:
            reg = UART_ULCON_DATABITS_5BITS;
            break;
        case 6:
            reg = UART_ULCON_DATABITS_6BITS;
            break;
        case 7:
            reg = UART_ULCON_DATABITS_7BITS;
            break;
        default:
            reg = UART_ULCON_DATABITS_8BITS;
    }

    switch (parity) {
        case ParityNone:
            reg |= UART_ULCON_PARITY_NONE;
            break;
        case ParityOdd:
            reg |= UART_ULCON_PARITY_ODD;
            break;
        case ParityEven:
            reg |= UART_ULCON_PARITY_EVEN;
            break;
        case ParityForced1:
            reg |= UART_ULCON_PARITY_FORCE1;
            break;
        case ParityForced0:
            reg |= UART_ULCON_PARITY_FORCE0;
            break;
    }

    if (stop_bits == 2) {
        reg |= UART_ULCON_STOPBITS_2BITS;
    }

    p_USI_UART->ULCON = reg;
}


static void usi_serial_irq_handler(void *obj, uart_irq_handler handler, uint32_t id)
{
    struct serial_s *priv = (struct serial_s *)obj;

    irq_handler[priv->index] = handler;
    serial_irq_id[priv->index] = id;
}



static void usi_serial_irq_set(void *obj, SerialIrq irq, uint32_t enable)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_USI_UART_TypeDef *p_USI_UART = USI_PTR(priv->uart);

    switch (irq) {
        case RxIrq:
            if (enable) {
                p_USI_UART->UINTM &= ~(UART_UINTM_RXD_MASK);
            } else {
                p_USI_UART->UINTM |= UART_UINTM_RXD_MASK;
                p_USI_UART->UINTP = UART_UINTP_RXD;
            }
            break;
        case TxIrq:
            if (enable) {
                p_USI_UART->UINTM &= ~(UART_UINTM_TXD_MASK);
            } else {
                p_USI_UART->UINTM |= UART_UINTM_TXD_MASK;
                p_USI_UART->UINTP = UART_UINTP_TXD;
            }
            break;
    }
}

static int usi_serial_writable(void *obj)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_USI_UART_TypeDef *p_USI_UART = USI_PTR(priv->uart);

    return !(p_USI_UART->UFSTAT & UART_UFSTAT_TX_FIFO_FULL);
}


static void usi_serial_putc(void *obj, int c)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_USI_UART_TypeDef *p_USI_UART = USI_PTR(priv->uart);
    while (!serial_writable(obj));
    p_USI_UART->UTXH = c;
}



static int usi_serial_readable(void *obj)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_USI_UART_TypeDef *p_USI_UART = USI_PTR(priv->uart);
    return (p_USI_UART->UFSTAT & (UART_UFSTAT_RX_FIFO_COUNT_MASK | UART_UFSTAT_RX_FIFO_FULL));
}

static int usi_serial_getc(void *obj)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_USI_UART_TypeDef *p_USI_UART = USI_PTR(priv->uart);
    int data;
    while (!serial_readable(obj));
    data = p_USI_UART->URXH & 0xFF;

    return data;
}

#if DEVICE_SERIAL_FC
static void usi_serial_set_flow_control(struct serial_s *obj, FlowControl type,
                                        PinName rxflow, PinName txflow)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_USI_UART_TypeDef *p_USI_UART = USI_PTR(priv->uart);
    // Checked used UART name (UART_1, UART_2, ...)
    if (type == FlowControlRTSCTS) {
        p_USI_UART->UMCON = UART_UMCON_RTS_TRIG_14BYTES | UART_UMCON_AFC_ENABLE;
    } else {
        p_USI_UART->UMCON = UART_UMCON_AFC_DISABLE;
    }
}
#endif

void usi_serial_init(void *obj, PinName tx, PinName rx)
{
    struct serial_s *priv = (struct serial_s *)obj;
    S5JS100_USI_UART_TypeDef *p_USI_UART = USI_PTR(priv->uart);

    IRQn_Type irq_n = (IRQn_Type)0;
    uint32_t vector = 0;


    p_USI_UART->UINTM = 0xF;
    p_USI_UART->UINTP = 0xF;

    switch ((int)priv->uart) {
        case UART_0:
            if (! USI_VERSION_UART(S5JS100_USI0_REG->VERSION)) {
                /* UART NOT SUPPORTED */
                MBED_ASSERT(false);
            }

            S5JS100_USI0_REG->CON = USI_CON_RST;
            S5JS100_SYSCFG_USI0_CONF = USI_CONFIG_UART;
            S5JS100_USI0_REG->CON &= ~USI_CON_RST;

            priv->index = USI0_PORT_ID;
            priv->rx_fifo_depth = USI_RXFIFO(S5JS100_USI0_REG->FIFO_DEPTH);
            priv->tx_fifo_depth = USI_TXFIFO(S5JS100_USI0_REG->FIFO_DEPTH);

            irq_n = S5JS100_IRQ_USI0;
            vector = (uint32_t)&uart0_irq;

            break;

        case UART_1:
            if (! USI_VERSION_UART(S5JS100_USI1_REG->VERSION)) {
                /* UART NOT SUPPORTED */
                while (1);
            }
            putreg32(0x6001, TOP_MUX_BASE + USI1_RX_OFFSET);
            putreg32(0x7001, TOP_MUX_BASE + USI1_TX_OFFSET);
            S5JS100_USI1_REG->CON = USI_CON_RST;
            S5JS100_SYSCFG_USI1_CONF = USI_CONFIG_UART;
            S5JS100_USI1_REG->CON &= ~USI_CON_RST;

            priv->index = USI1_PORT_ID;
            priv-> rx_fifo_depth = USI_RXFIFO(S5JS100_USI1_REG->FIFO_DEPTH);
            priv-> tx_fifo_depth = USI_TXFIFO(S5JS100_USI1_REG->FIFO_DEPTH);
            irq_n = S5JS100_IRQ_USI1;
            vector = (uint32_t)&uart1_irq;

            break;
    }



    // Disable all interrupts and clear pendings
    p_USI_UART->UINTM = UART_UINTM_MODEM_MASK | UART_UINTM_TXD_MASK |
                        UART_UINTM_RXD_MASK | UART_UINTM_ERROR_MASK;
    p_USI_UART->UINTP = UART_UINTP_MODEM | UART_UINTP_TXD |
                        UART_UINTP_ERROR | UART_UINTP_RXD;

    // reset FIFO and set interrupt trigger level
    p_USI_UART->UFCON = UART_UFCON_TX_FIFO_TRIG_0BYTE | UART_UFCON_RX_FIFO_TRIG_14BYTES |
                        UART_UFCON_TX_FIFO_RESET | UART_UFCON_RX_FIFO_RESET |
                        UART_UFCON_FIFO_ENABLE ;

    //wait_ms(10);
    thread_sleep_for(10);

    //Enable TX/RX fifo int/poll mode with RX timeout of 32 bits duration
    p_USI_UART->UCON = UART_UCON_RX_TOUT_32FRAMES | UART_UCON_RX_TOUTINT_ENABLE |
                       UART_UCON_TX_INTTYPE_LEVEL | UART_UCON_RX_INTTYPE_LEVEL |
                       UART_UCON_TX_MODE_IRQPOLL | UART_UCON_RX_MODE_IRQPOLL;

    priv->ops.serial_baud = usi_serial_baud;
    priv->ops.serial_format = usi_serial_format;
    priv->ops.serial_irq_handler = usi_serial_irq_handler;
    priv->ops.serial_irq_set = usi_serial_irq_set;
    priv->ops.serial_putc = usi_serial_putc;
    priv->ops.serial_writable = usi_serial_writable;
    priv->ops.serial_getc = usi_serial_getc;
    priv->ops.serial_readable = usi_serial_readable;

#if DEVICE_SERIAL_FC
    priv->ops.serial_set_flow_control = usi_serial_set_flow_control;
#endif
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
