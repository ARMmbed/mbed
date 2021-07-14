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
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mbed_assert.h"
#include "serial_api.h"
#include "cmsis.h"
#include "pinmap.h"
#include "PeripheralPins.h"


/******************************************************************************
 * INITIALIZATION
 ******************************************************************************/

#define UART_NUM                3
#define UART_HWCONTROL_NONE     0
#define UART_HWCONTROL_RTS      1
#define UART_HWCONTROL_CTS      2
#define UART_HWCONTROL_RTS_CTS  3
static uint32_t serial_irq_ids[UART_NUM] = {0};
static uart_irq_handler irq_handler;

int stdio_uart_inited = 0;
serial_t stdio_uart;

//#define _USE_UART_FIFO
#if DEVICE_SERIAL_ASYNCH
#define serial_s(obj) (&((obj)->serial))
#else
#define serial_s(obj) (obj)
#endif

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    struct serial_s *objs = serial_s(obj);

    // determine the UART to use
    UARTName uart_tx = (UARTName)pinmap_peripheral(tx, PinMap_UART_TX);
    UARTName uart_rx = (UARTName)pinmap_peripheral(rx, PinMap_UART_RX);
    UARTName uart = (UARTName)pinmap_merge(uart_tx, uart_rx);
    MBED_ASSERT((int)uart != NC);

    /* Configure the TX and RX pins */
    pinmap_pinout(tx, PinMap_UART_TX);
    pinmap_pinout(rx, PinMap_UART_RX);
// Configure UART
    objs->baudrate = 115200; // baudrate default value
    if (uart == STDIO_UART) {
#if MBED_CONF_PLATFORM_STDIO_BAUD_RATE
        objs->baudrate = MBED_CONF_PLATFORM_STDIO_BAUD_RATE; // baudrate takes value from platform/mbed_lib.json
#endif /* MBED_CONF_PLATFORM_STDIO_BAUD_RATE */
    } else {
#if MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE
        objs->baudrate = MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE; // baudrate takes value from platform/mbed_lib.json
#endif /* MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE */
    }

    objs->databits = UART_WL_8BIT;
    objs->stopbits = UART_1_STOP_BIT;
    objs->parity   = UART_NO_PARITY;
#if DEVICE_SERIAL_FC
    objs->hw_flow_ctl = UART_HWCONTROL_NONE;
#endif
    objs->pin_tx = uart_tx;
    objs->pin_rx = uart_rx;
    objs->index = 0;
    objs->uart = uart;
    switch (uart) {
        case UART_0:
            obj->index = 0;
            break;
        case UART_1:
            obj->index = 1;
            break;
        case UART_2:
            obj->index = 2;
            break;
    }
    bp6a_cmu_enable_clock(obj->index +  CMU_UART0_CLK, true);

    modifyreg32(uart + UART_ULCON_OFFSET,
                UART_ULCON_WORD_LEN_MASK |
                UART_ULCON_N_STOP_BIT_MASK |
                UART_ULCON_PARITY_MODE_MASK,
                UART_ULCON_WORD_LEN(UART_WL_8BIT) |
                UART_ULCON_N_STOP_BIT(UART_1_STOP_BIT) |
                UART_ULCON_PARITY_MODE(UART_NO_PARITY));

    modifyreg32(uart + UART_UCON_OFFSET,
                UART_UCON_RCV_MODE_MASK |
                UART_UCON_TX_MODE_MASK |
                UART_UCON_RX_TIMOUT_INT_INTER_MASK,
                UART_UCON_RCV_MODE(0x01) |
                UART_UCON_TX_MODE(0x01));

    modifyreg32(uart + UART_UFCON_OFFSET,
                UART_UFCON_RX_FIFO_RESET_MASK |
                UART_UFCON_TX_FIFO_RESET_MASK,
                UART_UFCON_RX_FIFO_RESET(0x01) |
                UART_UFCON_TX_FIFO_RESET(0x01));

    serial_baud(obj,  objs->baudrate);

    if (uart == STDIO_UART) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }
}

void serial_free(serial_t *obj)
{
    bp6a_cmu_enable_clock(obj->index +  CMU_UART0_CLK, false);
}

void serial_baud(serial_t *obj, int baudrate)
{
    struct serial_s *objs = serial_s(obj);
    float fFrac = 0;
    float fDiv  = 0;
    uint32_t Peri_Clock = bp6a_cmu_get_clock_freq(CMU_UART0_CLK + obj->index);

    fDiv = ((float)Peri_Clock / ((float)baudrate * 16)) - (float)1.0;
    fFrac = (uint32_t)((fDiv - (int32_t)fDiv) * 16.0f);
    putreg32(objs->uart + UART_UBRDIV_OFFSET, (uint32_t)fDiv);
    putreg32(objs->uart + UART_UFRAC_OFFSET, (uint32_t)fFrac);
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    struct serial_s *objs = serial_s(obj);
    uint8_t prity_bits[] = {UART_NO_PARITY, UART_ODD_PARITY, UART_EVEN_PARITY, \
                            UART_FORCE_1_PARITY, UART_FORCE_0_PARITY
                           };

    MBED_ASSERT((stop_bits == 1) || (stop_bits == 2)); // 0: 1 stop bits, 1: 2 stop bits
    MBED_ASSERT((data_bits > 4) && (data_bits < 9)); // 0: 5 data bits ... 3: 8 data bits

    modifyreg32(objs->uart + UART_ULCON_OFFSET,
                UART_ULCON_WORD_LEN_MASK |
                UART_ULCON_N_STOP_BIT_MASK |
                UART_ULCON_PARITY_MODE_MASK,
                UART_ULCON_WORD_LEN(data_bits - 5) |
                UART_ULCON_N_STOP_BIT(stop_bits - 1) |
                UART_ULCON_PARITY_MODE(prity_bits[parity]));

}

/******************************************************************************
 * INTERRUPTS HANDLING
 ******************************************************************************/
static inline void _uart_irq_handler(SerialIrq irq_type, uint32_t index)
{
    if (serial_irq_ids[index] != 0) {
        irq_handler(serial_irq_ids[index], irq_type);
    }
}

void uart0_irq(void)
{
    uint32_t uints = getreg32(BP_UART0_BASE + UART_UINTP_OFFSET);

    if (uints & UART_UINTS_RXD_MASK) {
        _uart_irq_handler(RxIrq, 0);
        modifyreg32(BP_UART0_BASE + UART_UINTP_OFFSET,
                    UART_UINTP_RXD_MASK,
                    UART_UINTP_RXD(1));
    } else if (uints & UART_UINTS_TXD_MASK) {
        _uart_irq_handler(TxIrq, 0);
        modifyreg32(BP_UART0_BASE + UART_UINTP_OFFSET,
                    UART_UINTP_TXD_MASK,
                    UART_UINTP_TXD(1));
    }
    NVIC_ClearPendingIRQ((IRQn_Type)UARTR0_IRQn);
}

void uart1_irq(void)
{
    uint32_t uints = getreg32(BP_UART1_BASE + UART_UINTP_OFFSET);

    if (uints & UART_UINTS_RXD_MASK) {
        _uart_irq_handler(RxIrq, 1);
        modifyreg32(BP_UART1_BASE + UART_UINTP_OFFSET,
                    UART_UINTP_RXD_MASK,
                    UART_UINTP_RXD(1));
    } else if (uints & UART_UINTS_TXD_MASK) {
        _uart_irq_handler(TxIrq, 1);
        modifyreg32(BP_UART1_BASE + UART_UINTP_OFFSET,
                    UART_UINTP_TXD_MASK,
                    UART_UINTP_TXD(1));
    }
    NVIC_ClearPendingIRQ((IRQn_Type)UARTR1_IRQn);
}

void uart2_irq(void)
{
    uint32_t uints = getreg32(BP_UART2_BASE + UART_UINTP_OFFSET);

    if (uints & UART_UINTS_RXD_MASK) {
        _uart_irq_handler(RxIrq, 2);
        modifyreg32(BP_UART2_BASE + UART_UINTP_OFFSET,
                    UART_UINTP_TXD_MASK,
                    UART_UINTP_TXD(1));
    } else if (uints & UART_UINTS_TXD_MASK) {
        _uart_irq_handler(TxIrq, 2);
        modifyreg32(BP_UART2_BASE + UART_UINTP_OFFSET,
                    UART_UINTP_TXD_MASK,
                    UART_UINTP_TXD(1));
    }
    NVIC_ClearPendingIRQ((IRQn_Type)UARTR2_IRQn);
}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    irq_handler = handler;
    serial_irq_ids[obj->index] = id;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    struct serial_s *objs = serial_s(obj);
    uint8_t en = (enable != 0) ? 1 : 0;
    uint32_t vector = 0;

    switch ((int)objs->uart) {
        case UART_0:
            vector = (uint32_t)&uart0_irq;
            break;
        case UART_1:
            vector = (uint32_t)&uart1_irq;
            break;
        case UART_2:
            vector = (uint32_t)&uart2_irq;
            break;
    }
    if (irq == RxIrq) {
        modifyreg32(obj->uart + UART_UCON_OFFSET,
                    UART_UCON_RCV_MODE_MASK |
                    UART_UCON_RX_INT_TYPE_MASK,
                    UART_UCON_RCV_MODE(1) |
                    UART_UCON_RX_INT_TYPE(1));

        modifyreg32(obj->uart + UART_UINTM_OFFSET,
                    UART_UINTM_RXD_MASK |
                    UART_UINTM_TXD_MASK,
                    UART_UINTM_RXD(!en) |
                    UART_UINTM_TXD(en));
    } else {
        modifyreg32(obj->uart + UART_UCON_OFFSET,
                    UART_UCON_TX_MODE_MASK |
                    UART_UCON_TX_INT_TYPE_MASK,
                    UART_UCON_TX_MODE(1) |
                    UART_UCON_TX_INT_TYPE(1));

        modifyreg32(obj->uart + UART_UINTM_OFFSET,
                    UART_UINTM_TXD_MASK |
                    UART_UINTM_TXD_MASK,
                    UART_UINTM_RXD(en) |
                    UART_UINTM_TXD(!en));
    }
#ifdef _USE_UART_FIFO
    modifyreg32(obj->uart + UART_UFCON_OFFSET,
                UART_UFCON_FIFO_EN_MASK,
                UART_UFCON_FIFO_EN(1));
#endif
    if (enable) {
        NVIC_ClearPendingIRQ((IRQn_Type)(UARTR0_IRQn + objs->index));
        NVIC_SetVector((IRQn_Type)(UARTR0_IRQn + objs->index), vector);
        NVIC_EnableIRQ((IRQn_Type)(UARTR0_IRQn + objs->index));
    } else {
        uint32_t intm = getreg32(obj->uart + UART_UINTM_OFFSET);
        if (intm == 0) {
            NVIC_DisableIRQ((IRQn_Type)(UARTR0_IRQn + objs->index));
        }
    }
}

/******************************************************************************
 * READ/WRITE
 ******************************************************************************/
int serial_getc(serial_t *obj)
{
    struct serial_s *objs = serial_s(obj);

    while (!serial_readable(obj));

    return getreg8(objs->uart + UART_URXH_OFFSET);
}

void serial_putc(serial_t *obj, int c)
{
    struct serial_s *objs = serial_s(obj);

    while (!serial_writable(obj));

    putreg8(objs->uart + UART_UTXH_OFFSET, (uint8_t)c);
}

int serial_readable(serial_t *obj)
{
    struct serial_s *objs = serial_s(obj);

    return (getreg32(objs->uart + UART_UTRSTAT_OFFSET) & UART_UTRSTAT_RCV_BUF_DATA_READY_MASK);
}

int serial_writable(serial_t *obj)
{
    struct serial_s *objs = serial_s(obj);

#ifdef _USE_UART_FIFO
    return !((getreg32(objs->uart + UART_UFSTAT_OFFSET) & UART_UFSTAT_TX_FIFO_FULL_MASK) >> UART_UFSTAT_TX_FIFO_FULL_SHIFT);

#else
    return ((getreg32(objs->uart + UART_UTRSTAT_OFFSET) & UART_UTRSTAT_TX_BUF_EMPTY_MASK));
#endif
}

int serial_tx_empty(serial_t *obj)
{
    struct serial_s *objs = serial_s(obj);

    return ((getreg32(objs->uart + UART_UTRSTAT_OFFSET) & UART_UTRSTAT_TX_EMPTY_MASK));
}

void serial_clear(serial_t *obj)
{
    struct serial_s *objs = serial_s(obj);

    modifyreg32(objs->uart + UART_UFCON_OFFSET,  UART_UFCON_RX_FIFO_RESET_MASK |
                UART_UFCON_TX_FIFO_RESET_MASK,
                UART_UFCON_RX_FIFO_RESET(1) |
                UART_UFCON_TX_FIFO_RESET(1));

}

#if DEVICE_SERIAL_FC
void serial_set_flow_control(serial_t *obj, FlowControl type,
                             PinName rxflow, PinName txflow)
{
    struct serial_s *objs = serial_s(obj);

    UARTName uart_rts = (UARTName)pinmap_peripheral(rxflow, PinMap_UART_RTS);
    UARTName uart_cts = (UARTName)pinmap_peripheral(txflow, PinMap_UART_CTS);

    if (((UARTName)pinmap_merge(uart_rts, objs->uart) == (UARTName)NC) || ((UARTName)pinmap_merge(uart_cts, objs->uart) == (UARTName)NC)) {
        MBED_ASSERT(0);
        return;
    }
    if (type == FlowControlNone) {
        // Disable hardware flow control
        objs->hw_flow_ctl = UART_HWCONTROL_NONE;
    }
    if (type == FlowControlRTS) {
        // Enable RTS
        MBED_ASSERT(uart_rts != (UARTName)NC);
        // Enable the pin for RTS function
        pinmap_pinout(rxflow, PinMap_UART_RTS);
        objs->pin_rts = rxflow;
    }
    if (type == FlowControlCTS) {
        // Enable CTS
        MBED_ASSERT(uart_cts != (UARTName)NC);
        // Enable the pin for CTS function
        pinmap_pinout(txflow, PinMap_UART_CTS);

        objs->hw_flow_ctl = UART_HWCONTROL_CTS;
        objs->pin_cts = txflow;
    }
    if (type == FlowControlRTSCTS) {
        // Enable CTS & RTS
        MBED_ASSERT(uart_rts != (UARTName)NC);
        MBED_ASSERT(uart_cts != (UARTName)NC);
        // Enable the pin for CTS function
        pinmap_pinout(txflow, PinMap_UART_CTS);
        // Enable the pin for RTS function
        pinmap_pinout(rxflow, PinMap_UART_RTS);
        objs->hw_flow_ctl = UART_HWCONTROL_RTS_CTS;

        objs->pin_rts = rxflow;
        objs->pin_cts = txflow;
    }

    modifyreg32(objs->uart + UART_UMCON_OFFSET,  UART_UMCON_AFC_MASK |
                UART_UMCON_AFC_MASK,
                UART_UMCON_RTS(1) |
                UART_UMCON_AFC(1));
}
#endif

void serial_pinout_tx(PinName tx)
{
    pinmap_pinout(tx, PinMap_UART_TX);
}

void serial_break_set(serial_t *obj)
{
    struct serial_s *objs = serial_s(obj);

    modifyreg32(objs->uart + UART_UCON_OFFSET, UART_UCON_SND_BRK_SIG_MASK, UART_UCON_SND_BRK_SIG(1));
}

void serial_break_clear(serial_t *obj)
{
    struct serial_s *objs = serial_s(obj);

    modifyreg32(objs->uart + UART_UCON_OFFSET, UART_UCON_SND_BRK_SIG_MASK, UART_UCON_SND_BRK_SIG(0));
}

const PinMap *serial_tx_pinmap()
{
    return PinMap_UART_TX;
}

const PinMap *serial_rx_pinmap()
{
    return PinMap_UART_RX;
}

const PinMap *serial_cts_pinmap()
{
#if !DEVICE_SERIAL_FC
    static const PinMap PinMap_UART_CTS[] = {
        {NC, NC, 0}
    };
#endif

    return PinMap_UART_CTS;
}

const PinMap *serial_rts_pinmap()
{
#if !DEVICE_SERIAL_FC
    static const PinMap PinMap_UART_RTS[] = {
        {NC, NC, 0}
    };
#endif

    return PinMap_UART_RTS;
}

#endif
