/* mbed Microcontroller Library
 * Copyright (c) 2018 GigaDevice Semiconductor Inc.
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
#include "mbed_assert.h"
#include "serial_api.h"

#if DEVICE_SERIAL

#include "cmsis.h"
#include "pinmap.h"
#include "mbed_error.h"
#include <string.h>
#include "PeripheralPins.h"

#define USART_NUM (5)

static uint32_t serial_irq_ids[USART_NUM]   = {0};
static rcu_periph_enum usart_clk[USART_NUM] = {RCU_USART0, RCU_USART1, RCU_USART2, RCU_UART3, RCU_UART4};
static IRQn_Type usart_irq_n[USART_NUM]     = {USART0_IRQn, USART1_IRQn, USART2_IRQn, UART3_IRQn, UART4_IRQn};

static uart_irq_handler irq_handler;

int stdio_uart_inited = 0;
serial_t stdio_uart;

#if DEVICE_SERIAL_ASYNCH
#define GET_SERIAL_S(obj) (&((obj)->serial))
#else
#define GET_SERIAL_S(obj) (obj)
#endif /* DEVICE_SERIAL_ASYNCH */

/** Initialize the USART peripheral.
 *
 * @param obj_s The serial object
 */
static void usart_init(struct serial_s *obj_s)
{
    if (obj_s->index >= USART_NUM) {
        return;
    }

    /* USART configuration */
    usart_deinit(obj_s->uart);
    usart_word_length_set(obj_s->uart, obj_s->databits);
    usart_baudrate_set(obj_s->uart, obj_s->baudrate);
    usart_stop_bit_set(obj_s->uart, obj_s->stopbits);
    usart_parity_config(obj_s->uart, obj_s->parity);
#if DEVICE_SERIAL_FC
    if (obj_s->hw_flow_ctl == USART_HWCONTROL_NONE) {
        usart_hardware_flow_cts_config(obj_s->uart, USART_CTS_DISABLE);
        usart_hardware_flow_rts_config(obj_s->uart, USART_RTS_DISABLE);
    } else if (obj_s->hw_flow_ctl == USART_HWCONTROL_RTS) {
        usart_hardware_flow_cts_config(obj_s->uart, USART_CTS_DISABLE);
        usart_hardware_flow_rts_config(obj_s->uart, USART_RTS_ENABLE);
    } else if (obj_s->hw_flow_ctl == USART_HWCONTROL_CTS) {
        usart_hardware_flow_cts_config(obj_s->uart, USART_CTS_ENABLE);
        usart_hardware_flow_rts_config(obj_s->uart, USART_RTS_DISABLE);
    } else if (obj_s->hw_flow_ctl == USART_HWCONTROL_RTS_CTS) {
        usart_hardware_flow_cts_config(obj_s->uart, USART_CTS_ENABLE);
        usart_hardware_flow_rts_config(obj_s->uart, USART_RTS_ENABLE);
    }
#endif /* DEVICE_SERIAL_FC */
    usart_receive_config(obj_s->uart, USART_RECEIVE_ENABLE);
    usart_transmit_config(obj_s->uart, USART_TRANSMIT_ENABLE);
    usart_enable(obj_s->uart);
}

/** Initialize the serial peripheral. It sets the default parameters for serial
 *  peripheral, and configures its specifieds pins.
 *
 * @param obj The serial object
 * @param tx  The TX pin name
 * @param rx  The RX pin name
 */
void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    UARTName uart_tx = (UARTName)pinmap_peripheral(tx, PinMap_UART_TX);
    UARTName uart_rx = (UARTName)pinmap_peripheral(rx, PinMap_UART_RX);

    p_obj->uart = (UARTName)pinmap_merge(uart_tx, uart_rx);
    MBED_ASSERT(p_obj->uart != (UARTName)NC);

    /* enable UART peripheral clock */
    if (p_obj->uart == UART_0) {
        p_obj->index = 0;
        rcu_periph_clock_enable(usart_clk[p_obj->index]);
    } else if (p_obj->uart == UART_1) {
        p_obj->index = 1;
        rcu_periph_clock_enable(usart_clk[p_obj->index]);
    } else if (p_obj->uart == UART_2) {
        p_obj->index = 2;
        rcu_periph_clock_enable(usart_clk[p_obj->index]);
    } else if (p_obj->uart == UART_3) {
        p_obj->index = 3;
        rcu_periph_clock_enable(usart_clk[p_obj->index]);
    } else if (p_obj->uart == UART_4) {
        p_obj->index = 4;
        rcu_periph_clock_enable(usart_clk[p_obj->index]);
    }

    /* configurte the pins */
    pinmap_pinout(tx, PinMap_UART_TX);
    pinmap_pinout(rx, PinMap_UART_RX);

    /* default UART parameters */
    p_obj->baudrate    = 9600U;
    p_obj->databits    = USART_WL_8BIT;
    p_obj->stopbits    = USART_STB_1BIT;
    p_obj->parity      = USART_PM_NONE;

#if DEVICE_SERIAL_FC
    p_obj->hw_flow_ctl = USART_HWCONTROL_NONE;
#endif /* DEVICE_SERIAL_FC */

    p_obj->pin_tx = tx;
    p_obj->pin_rx = rx;

    p_obj->tx_state = OP_STATE_BUSY;
    p_obj->rx_state = OP_STATE_BUSY;

    usart_init(p_obj);

    p_obj->tx_state = OP_STATE_READY;
    p_obj->rx_state = OP_STATE_READY;

    if (p_obj->uart == STDIO_UART) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }
}

/** Release the serial peripheral, not currently invoked. It requires further
 *  resource management.
 *
 * @param obj The serial object
 */
void serial_free(serial_t *obj)
{
    struct serial_s *p_obj     = GET_SERIAL_S(obj);
    rcu_periph_enum rcu_periph = usart_clk[p_obj->index];

    /* reset USART and disable clock */
    usart_deinit(p_obj->uart);
    rcu_periph_clock_disable(rcu_periph);

    serial_irq_ids[p_obj->index] = 0;

    /* reset the GPIO state */
    pin_function(p_obj->pin_tx, MODE_IN_FLOATING);
    pin_function(p_obj->pin_rx, MODE_IN_FLOATING);
}

/** Configure the baud rate
 *
 * @param obj      The serial object
 * @param baudrate The baud rate to be configured
 */
void serial_baud(serial_t *obj, int baudrate)
{
    uint16_t uen_flag = 0U;
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    /* store the UEN flag */
    uen_flag = USART_CTL0(p_obj->uart) & USART_CTL0_UEN;

    /* disable the USART clock first */
    usart_disable(p_obj->uart);

    usart_baudrate_set(p_obj->uart, baudrate);

    p_obj->baudrate = baudrate;

    /* restore the UEN flag */
    if (RESET != uen_flag) {
        usart_enable(p_obj->uart);
    }
}

/** Configure the format. Set the number of bits, parity and the number of stop bits
 *
 * @param obj       The serial object
 * @param data_bits The number of data bits
 * @param parity    The parity
 * @param stop_bits The number of stop bits
 */
void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    uint16_t uen_flag = 0U;
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    /* store the UEN flag */
    uen_flag = USART_CTL0(p_obj->uart) & USART_CTL0_UEN;

    /* disable the UART clock first */
    usart_disable(p_obj->uart);

    /* configurate the UART parity */
    switch (parity) {
        case ParityOdd:
            p_obj->parity = USART_PM_ODD;
            usart_parity_config(p_obj->uart, USART_PM_ODD);
            break;

        case ParityEven:
            p_obj->parity = USART_PM_EVEN;
            usart_parity_config(p_obj->uart, USART_PM_EVEN);
            break;

        case ParityForced0:
        case ParityForced1:
        default:
            p_obj->parity = USART_PM_NONE;
            usart_parity_config(p_obj->uart, USART_PM_NONE);
            break;
    }

    if (p_obj->parity == USART_PM_NONE) {
        if (data_bits == 9) {
            usart_word_length_set(p_obj->uart, USART_WL_9BIT);
        } else if (data_bits == 8) {
            usart_word_length_set(p_obj->uart, USART_WL_8BIT);
        } else if (data_bits == 7) {
            return;
        }
    } else {
        if (data_bits == 9) {
            return;
        } else if (data_bits == 8) {
            usart_word_length_set(p_obj->uart, USART_WL_9BIT);
        } else if (data_bits == 7) {
            usart_word_length_set(p_obj->uart, USART_WL_8BIT);
        }
    }

    if (stop_bits == 2) {
        usart_stop_bit_set(p_obj->uart, USART_STB_2BIT);
    } else {
        usart_stop_bit_set(p_obj->uart, USART_STB_1BIT);
    }

    /* restore the UEN flag */
    if (RESET != uen_flag) {
        usart_enable(p_obj->uart);
    }
}

/** The serial interrupt handler registration
 *
 * @param obj     The serial object
 * @param handler The interrupt handler which will be invoked when the interrupt fires
 * @param id      The SerialBase object
 */
void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    irq_handler = handler;
    serial_irq_ids[p_obj->index] = id;
}

/** This function handles USART interrupt handler
 *
 * @param usart_index  The index of UART
 * @param usart_periph The UART peripheral
 */
static void usart_irq(int usart_index, uint32_t usart_periph)
{
    if (serial_irq_ids[usart_index] != 0) {
        if (usart_interrupt_flag_get(usart_periph, USART_INT_FLAG_TC) != RESET) {
            usart_interrupt_flag_clear(usart_periph, USART_FLAG_TC);
            irq_handler(serial_irq_ids[usart_index], TxIrq);
        }

        if (usart_interrupt_flag_get(usart_periph, USART_INT_FLAG_RBNE) != RESET) {
            usart_interrupt_flag_clear(usart_periph, USART_FLAG_RBNE);
            irq_handler(serial_irq_ids[usart_index], RxIrq);
        }

        if (usart_interrupt_flag_get(usart_periph, USART_INT_FLAG_ERR_ORERR) != RESET) {
            usart_interrupt_flag_clear(usart_periph, USART_FLAG_ORERR);
        }
    }
}

/** This function handles USART0 interrupt handler
 *
 */
static void usart0_irq(void)
{
    usart_irq(0, USART0);
}

/** This function handles USART1 interrupt handler
 *
 */
static void usart1_irq(void)
{
    usart_irq(1, USART1);
}

/** This function handles USART2 interrupt handler
 *
 */
static void usart2_irq(void)
{
    usart_irq(2, USART2);
}

/** This function handles USART3 interrupt handler
 *
 */
static void uart3_irq(void)
{
    usart_irq(3, UART3);
}

/** This function handles USART4 interrupt handler
 *
 */
static void uart4_irq(void)
{
    usart_irq(4, UART4);
}

/** Configure serial interrupt. This function is used for word-approach
 *
 * @param obj    The serial object
 * @param irq    The serial IRQ type (RX or TX)
 * @param enable Set to non-zero to enable events, or zero to disable them
 */
void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);
    IRQn_Type irq_n = (IRQn_Type)0;
    uint32_t vector = 0;

    if (p_obj->uart == USART0) {
        irq_n = USART0_IRQn;
        vector = (uint32_t)&usart0_irq;
    } else if (p_obj->uart == USART1) {
        irq_n = USART1_IRQn;
        vector = (uint32_t)&usart1_irq;
    } else if (p_obj->uart == USART2) {
        irq_n = USART2_IRQn;
        vector = (uint32_t)&usart2_irq;
    } else if (p_obj->uart == UART3) {
        irq_n = UART3_IRQn;
        vector = (uint32_t)&uart3_irq;
    } else if (p_obj->uart == UART4) {
        irq_n = UART4_IRQn;
        vector = (uint32_t)&uart4_irq;
    }

    if (enable) {
        if (irq == RxIrq) {
            /* Rx IRQ */
            usart_interrupt_enable(p_obj->uart, USART_INT_RBNE);
        } else {
            /* Tx IRQ */
            usart_interrupt_enable(p_obj->uart, USART_INT_TBE);
        }

        NVIC_SetVector(irq_n, vector);
        NVIC_EnableIRQ(irq_n);
    } else {
        if (irq == RxIrq) {
            /* Rx IRQ */
            usart_interrupt_disable(p_obj->uart, USART_INT_RBNE);
        } else {
            /* Tx IRQ */
            usart_interrupt_disable(p_obj->uart, USART_INT_TBE);
        }
    }
}

/** Get character. This is a blocking call, waiting for a character
 *
 * @param obj The serial object
 */
int serial_getc(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    while (!serial_readable(obj));
    return (int)(usart_data_receive(p_obj->uart) & BITS(0, 7 + (p_obj->databits >> 12)));
}

/** Send a character. This is a blocking call, waiting for a peripheral to be available
 *  for writing
 *
 * @param obj The serial object
 * @param c   The character to be sent
 */
void serial_putc(serial_t *obj, int c)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    while (!serial_writable(obj));
    usart_data_transmit(p_obj->uart, (int)((c) & BITS(0, 7 + (p_obj->databits >> 12))));
}

/** Check if the serial peripheral is readable
 *
 * @param obj The serial object
 * @return Non-zero value if a character can be read, 0 if nothing to read
 */
int serial_readable(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    return (usart_flag_get(p_obj->uart, USART_FLAG_RBNE) != RESET) ? 1 : 0;
}

/** Check if the serial peripheral is writable
 *
 * @param obj The serial object
 * @return Non-zero value if a character can be written, 0 otherwise.
 */
int serial_writable(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    return (usart_flag_get(p_obj->uart, USART_FLAG_TBE) != RESET) ? 1 : 0;
}

/** Check if the serial peripheral tx buffer is empty
 *
 * @param obj The serial object
 * @return Non-zero value if the tx buffer is empty, 0 otherwise.
 */
int serial_tx_empty(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    return ((usart_flag_get(p_obj->uart, USART_FLAG_TBE) != RESET) && (usart_flag_get(p_obj->uart, USART_FLAG_TC) != RESET)) ? 1 : 0;
}

/** Clear the serial peripheral
 *
 * @param obj The serial object
 */
void serial_clear(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    p_obj->tx_count = 0U;
    p_obj->rx_count = 0U;
}

/** Set the break
 *
 * @param obj The serial object
 */
void serial_break_set(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    usart_send_break(p_obj->uart);
}

/** Clear the break
 *
 * @param obj The serial object
 */
void serial_break_clear(serial_t *obj)
{
    /* do nothing */
}

/** Configure the TX pin for UART function.
 *
 * @param tx The pin name used for TX
 */
void serial_pinout_tx(PinName tx)
{
    pinmap_pinout(tx, PinMap_UART_TX);
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

#if DEVICE_SERIAL_ASYNCH
/**
 * Enable the serial events
 *
 * @param obj   The serial object
 * @param event The events to be configured
 */
static void serial_event_enable(serial_t *obj, int event)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    p_obj->events |= event;

}

/**
 * Disable the serial events
 *
 * @param obj   The serial object
 * @param event The events to be configured
 */
static void serial_event_disable(serial_t *obj, int event)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    p_obj->events &= ~event;
}

/**
 * Preprocess the USART tx interrupt
 *
 * @param obj_s The serial object
 * @param pData Pointer to tx buffer
 * @param Size  Size of tx buffer
 * @return Returns the status
 */
gd_status_enum usart_tx_interrupt_preprocess(struct serial_s *obj_s, uint8_t *pData, uint16_t Size)
{
    if (obj_s->tx_state == OP_STATE_READY) {
        if ((pData == NULL) || (Size == 0U)) {
            return GD_ERROR;
        }

        obj_s->tx_buffer_ptr = pData;
        obj_s->tx_count      = Size;
        obj_s->error_code    = USART_ERROR_CODE_NONE;
        obj_s->tx_state      = OP_STATE_BUSY_TX;

        usart_interrupt_enable(obj_s->uart, USART_INT_TBE);

        return GD_OK;
    } else {
        return GD_BUSY;
    }
}

/**
 * Preprocess the USART rx interrupt
 *
 * @param obj_s The serial object
 * @param pData Pointer to rx buffer
 * @param Size  Size of rx buffer
 * @return Returns the status
 */
gd_status_enum usart_rx_interrupt_preprocess(struct serial_s *obj_s, uint8_t *pData, uint16_t Size)
{
    if (obj_s->rx_state == OP_STATE_READY) {
        if ((pData == NULL) || (Size == 0U)) {
            return GD_ERROR;
        }

        obj_s->rx_buffer_ptr = pData;
        obj_s->rx_size       = Size;
        obj_s->rx_count      = Size;
        obj_s->error_code    = USART_ERROR_CODE_NONE;
        obj_s->rx_state      = OP_STATE_BUSY_RX;

        usart_interrupt_enable(obj_s->uart, USART_INT_PERR);
        usart_interrupt_enable(obj_s->uart, USART_INT_ERR);
        usart_interrupt_enable(obj_s->uart, USART_INT_RBNE);

        return GD_OK;
    } else {
        return GD_BUSY;
    }
}

/** Begin asynchronous TX transfer. The used buffer is specified in the serial object,
 *  tx_buff
 *
 * @param obj       The serial object
 * @param tx        The transmit buffer
 * @param tx_length The number of bytes to transmit
 * @param tx_width  Deprecated argument
 * @param handler   The serial handler
 * @param event     The logical OR of events to be registered
 * @param hint      A suggestion for how to use DMA with this transfer
 * @return Returns number of data transfered, otherwise returns 0
 */
int serial_tx_asynch(serial_t *obj, const void *tx, size_t tx_length, uint8_t tx_width, uint32_t handler, uint32_t event, DMAUsage hint)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);
    IRQn_Type irq = usart_irq_n[p_obj->index];

    if ((tx_length == 0) || (tx_width != 8)) {
        return 0;
    }

    if (serial_tx_active(obj)) {
        /* some transmit is in progress */
        return 0;
    }

    obj->tx_buff.buffer = (void *)tx;
    obj->tx_buff.length = tx_length;
    obj->tx_buff.pos = 0;

    /* disable all events first */
    serial_event_disable(obj, SERIAL_EVENT_TX_ALL);
    /* enable the specific event */
    serial_event_enable(obj, event);

    /* enable interrupt */
    /* clear pending IRQ */
    NVIC_ClearPendingIRQ(irq);
    /* disable the IRQ first */
    NVIC_DisableIRQ(irq);
    /* set the priority and vector */
    NVIC_SetPriority(irq, 1);
    NVIC_SetVector(irq, (uint32_t)handler);
    /* enable IRQ */
    NVIC_EnableIRQ(irq);

    if (usart_tx_interrupt_preprocess(p_obj, (uint8_t *)tx, tx_length) != GD_OK) {
        return 0;
    }

    return tx_length;
}

/** Begin asynchronous RX transfer (enable interrupt for data collecting)
 *  The used buffer is specified in the serial object - rx_buff
 *
 * @param obj        The serial object
 * @param rx         The receive buffer
 * @param rx_length  The number of bytes to receive
 * @param rx_width   Deprecated argument
 * @param handler    The serial handler
 * @param event      The logical OR of events to be registered
 * @param handler    The serial handler
 * @param char_match A character in range 0-254 to be matched
 * @param hint       A suggestion for how to use DMA with this transfer
 */
void serial_rx_asynch(serial_t *obj, void *rx, size_t rx_length, uint8_t rx_width, uint32_t handler, uint32_t event, uint8_t char_match, DMAUsage hint)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);
    IRQn_Type irq = usart_irq_n[p_obj->index];

    if ((rx_length == 0) || (rx_width != 8)) {
        return;
    }

    /* disable all events first */
    serial_event_disable(obj, SERIAL_EVENT_RX_ALL);
    /* enable the specific event */
    serial_event_enable(obj, event);

    obj->char_match = char_match;

    if (serial_rx_active(obj)) {
        /* some reception is in progress */
        return;
    }

    obj->rx_buff.buffer = rx;
    obj->rx_buff.length = rx_length;
    obj->rx_buff.pos = 0;

    /* enable interrupt */
    /* clear pending IRQ */
    NVIC_ClearPendingIRQ(irq);
    /* disable the IRQ first */
    NVIC_DisableIRQ(irq);
    /* set the priority(higher than Tx) and vector */
    NVIC_SetPriority(irq, 0);
    NVIC_SetVector(irq, (uint32_t)handler);
    /* enable IRQ */
    NVIC_EnableIRQ(irq);

    usart_rx_interrupt_preprocess(p_obj, (uint8_t *)rx, rx_length);
}

/** Attempts to determine if the serial peripheral is already in use for TX
 *
 * @param obj The serial object
 * @return Non-zero if the RX transaction is ongoing, 0 otherwise
 */
uint8_t serial_tx_active(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    return ((p_obj->tx_state == OP_STATE_BUSY_TX) ? 1 : 0);
}

/** Attempts to determine if the serial peripheral is already in use for RX
 *
 * @param obj The serial object
 * @return Non-zero if the RX transaction is ongoing, 0 otherwise
 */
uint8_t serial_rx_active(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    return ((p_obj->rx_state == OP_STATE_BUSY_RX) ? 1 : 0);
}

/** Handle the serial rx interrupt
 *
 * @param obj_s The serial object
 * @return Returns the status
 */
static gd_status_enum usart_rx_interrupt(struct serial_s *obj_s)
{
    uint16_t *temp;

    if (obj_s->rx_state == OP_STATE_BUSY_RX) {
        if (obj_s->databits == USART_WL_9BIT) {
            temp = (uint16_t *) obj_s->rx_buffer_ptr;
            if (obj_s->parity == USART_PM_NONE) {
                /* 9-bit data, none parity bit */
                *temp = (uint16_t)(USART_DATA(obj_s->uart) & (uint16_t)0x01FF);
                obj_s->rx_buffer_ptr += 2U;
            } else {
                /* 9-bit data, with parity bit */
                *temp = (uint16_t)(USART_DATA(obj_s->uart) & (uint16_t)0x00FF);
                obj_s->rx_buffer_ptr += 1U;
            }
        } else {
            if (obj_s->parity == USART_PM_NONE) {
                /* 8-bit data, none parity bit */
                *obj_s->rx_buffer_ptr++ = (uint8_t)(USART_DATA(obj_s->uart) & (uint8_t)0x00FF);
            } else {
                /* 8-bit data, with parity bit */
                *obj_s->rx_buffer_ptr++ = (uint8_t)(USART_DATA(obj_s->uart) & (uint8_t)0x007F);
            }
        }

        if (--obj_s->rx_count == 0U) {
            usart_interrupt_disable(obj_s->uart, USART_INT_RBNE);
            usart_interrupt_disable(obj_s->uart, USART_INT_PERR);
            usart_interrupt_disable(obj_s->uart, USART_INT_ERR);

            obj_s->rx_state = OP_STATE_READY;
        }

        return GD_OK;
    } else {
        return GD_BUSY;
    }
}

/** Handle the serial tx interrupt
 *
 * @param obj_s The serial object
 * @return Returns the status
 */
static gd_status_enum usart_tx_interrupt(struct serial_s *obj_s)
{
    uint16_t *temp;

    if (obj_s->tx_state == OP_STATE_BUSY_TX) {
        if (obj_s->databits == USART_WL_9BIT) {
            temp = (uint16_t *) obj_s->tx_buffer_ptr;
            USART_DATA(obj_s->uart) = (uint16_t)(*temp & (uint16_t)0x01FF);
            if (obj_s->parity == USART_PM_NONE) {
                obj_s->tx_buffer_ptr += 2U;
            } else {
                obj_s->tx_buffer_ptr += 1U;
            }
        } else {
            USART_DATA(obj_s->uart) = (uint8_t)(*obj_s->tx_buffer_ptr++ & (uint8_t)0x00FF);
        }

        if (--obj_s->tx_count == 0U) {
            /* disable USART_INT_TBE interrupt */
            usart_interrupt_disable(obj_s->uart, USART_INT_TBE);

            /* enable USART_INT_TC interrupt */
            usart_interrupt_enable(obj_s->uart, USART_INT_TC);
        }

        return GD_OK;
    } else {
        return GD_BUSY;
    }
}

/** Handle the serial tx complete interrupt
 *
 * @param obj_s The serial object
 */
void usart_tx_complete_interrupt(struct serial_s *obj_s)
{
    usart_interrupt_disable(obj_s->uart, USART_INT_TC);

    obj_s->tx_state = OP_STATE_READY;
}

/** Handle all the serial interrupt request
 *
 * @param obj_s The serial object
 */
void usart_irq_handler(struct serial_s *obj_s)
{
    uint32_t err_flags = 0U;

    /* no error occurs */
    err_flags = (USART_STAT0(obj_s->uart) & (uint32_t)(USART_FLAG_PERR | USART_FLAG_FERR | USART_FLAG_ORERR | USART_FLAG_NERR));
    if (err_flags == RESET) {
        /* check whether USART is in receiver mode or not */
        if (usart_interrupt_flag_get(obj_s->uart, USART_INT_FLAG_RBNE) != RESET) {
            usart_rx_interrupt(obj_s);

            return;
        }
    }

    /* some errors occur */
    if ((err_flags != RESET) &&
            (((USART_CTL2(obj_s->uart) & USART_INT_ERR) != RESET) ||
             ((USART_CTL0(obj_s->uart) & (USART_INT_RBNE | USART_INT_PERR)) != RESET))) {

        if (usart_interrupt_flag_get(obj_s->uart, USART_INT_FLAG_PERR) != RESET) {
            obj_s->error_code |= USART_ERROR_CODE_PERR;
        }

        if (usart_interrupt_flag_get(obj_s->uart, USART_INT_FLAG_ERR_NERR) != RESET) {
            obj_s->error_code |= USART_ERROR_CODE_NERR;
        }

        if (usart_interrupt_flag_get(obj_s->uart, USART_INT_FLAG_ERR_FERR) != RESET) {
            obj_s->error_code |= USART_ERROR_CODE_FERR;
        }

        if (usart_interrupt_flag_get(obj_s->uart, USART_INT_FLAG_ERR_ORERR) != RESET) {
            obj_s->error_code |= USART_ERROR_CODE_ORERR;
        }

        if (obj_s->error_code != USART_ERROR_CODE_NONE) {
            if (usart_interrupt_flag_get(obj_s->uart, USART_INT_FLAG_RBNE) != RESET) {
                usart_rx_interrupt(obj_s);
            }
        }

        return;
    }

    if (usart_interrupt_flag_get(obj_s->uart, USART_INT_FLAG_TBE) != RESET) {
        usart_tx_interrupt(obj_s);
        return;
    }

    if (usart_interrupt_flag_get(obj_s->uart, USART_INT_FLAG_TC) != RESET) {
        usart_tx_complete_interrupt(obj_s);
        return;
    }
}

/** The asynchronous TX and RX handler.
 *
 * @param obj The serial object
 * @return Returns event flags if an RX transfer termination condition was met; otherwise returns 0
 */
int serial_irq_handler_asynch(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);
    volatile uint8_t i = 0;
    volatile int return_val = 0;
    uint8_t *p_buf = (uint8_t *)(obj->rx_buff.buffer);

    if (usart_interrupt_flag_get(p_obj->uart, USART_INT_FLAG_PERR) != RESET) {
        usart_interrupt_flag_clear(p_obj->uart, USART_INT_FLAG_PERR);
        return_val |= (SERIAL_EVENT_RX_PARITY_ERROR & p_obj->events);
        p_obj->error_code |= USART_ERROR_CODE_PERR;
    }

    if (usart_interrupt_flag_get(p_obj->uart, USART_INT_FLAG_ERR_FERR) != RESET) {
        usart_interrupt_flag_clear(p_obj->uart, USART_INT_FLAG_ERR_FERR);
        return_val |= (SERIAL_EVENT_RX_FRAMING_ERROR & p_obj->events);
        p_obj->error_code |= USART_ERROR_CODE_FERR;
    }

    if (usart_interrupt_flag_get(p_obj->uart, USART_INT_FLAG_ERR_ORERR) != RESET) {
        usart_interrupt_flag_clear(p_obj->uart, USART_INT_FLAG_ERR_ORERR);
        return_val |= (SERIAL_EVENT_RX_OVERRUN_ERROR & p_obj->events);
        p_obj->error_code |= USART_ERROR_CODE_ORERR;
    }

    if (return_val & (SERIAL_EVENT_RX_PARITY_ERROR | SERIAL_EVENT_RX_FRAMING_ERROR |
                      SERIAL_EVENT_RX_OVERRUN_ERROR)) {
        return return_val;
    }

    if (usart_interrupt_flag_get(p_obj->uart, USART_INT_FLAG_TC) != RESET) {
        if ((p_obj->events & SERIAL_EVENT_TX_COMPLETE) != 0) {
            return_val |= (SERIAL_EVENT_TX_COMPLETE & p_obj->events);
        }
    }

    usart_irq_handler(p_obj);

    if (p_obj->rx_size != 0) {
        obj->rx_buff.pos = p_obj->rx_size - p_obj->rx_count;
    }

    if ((p_obj->rx_count == 0) && (obj->rx_buff.pos >= (obj->rx_buff.length - 1))) {
        return_val |= (SERIAL_EVENT_RX_COMPLETE & p_obj->events);
    }

    if (p_obj->events & SERIAL_EVENT_RX_CHARACTER_MATCH) {
        if (p_buf != NULL) {
            for (i = 0; i < obj->rx_buff.pos; i++) {
                if (p_buf[i] == obj->char_match) {
                    obj->rx_buff.pos = i;
                    return_val |= (SERIAL_EVENT_RX_CHARACTER_MATCH & p_obj->events);
                    serial_rx_abort_asynch(obj);
                    break;
                }
            }
        }
    }

    return return_val;
}

/** Abort the ongoing TX transaction. It disables the enabled interupt for TX and
 *  flushes the TX hardware buffer if TX FIFO is used
 *
 * @param obj The serial object
 */
void serial_tx_abort_asynch(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    usart_interrupt_disable(p_obj->uart, USART_INT_TC);
    usart_interrupt_disable(p_obj->uart, USART_INT_TBE);

    usart_flag_clear(p_obj->uart, USART_FLAG_TC);

    p_obj->tx_count = 0;
    p_obj->tx_state = OP_STATE_READY;
}

/** Abort the ongoing RX transaction. It disables the enabled interrupt for RX and
 *  flushes the RX hardware buffer if RX FIFO is used
 *
 * @param obj The serial object
 */
void serial_rx_abort_asynch(serial_t *obj)
{
    struct serial_s *p_obj = GET_SERIAL_S(obj);

    /* disable interrupts */
    usart_interrupt_disable(p_obj->uart, USART_INT_RBNE);
    usart_interrupt_disable(p_obj->uart, USART_INT_PERR);
    usart_interrupt_disable(p_obj->uart, USART_INT_ERR);

    /* clear USART_FLAG_RBNE flag */
    usart_flag_clear(p_obj->uart, USART_FLAG_RBNE);

    /* clear errors flag */
    usart_flag_clear(p_obj->uart, USART_FLAG_PERR);
    usart_flag_clear(p_obj->uart, USART_FLAG_FERR);
    usart_flag_clear(p_obj->uart, USART_FLAG_ORERR);
    /* clear RBNE flag */
    USART_DATA(p_obj->uart);

    /* reset rx transfer count */
    p_obj->rx_count = 0;

    /* reset rx state */
    p_obj->rx_state = OP_STATE_READY;
}

#endif /* DEVICE_SERIAL_ASYNCH */

#if DEVICE_SERIAL_FC
/** Configure the serial for the flow control. It sets flow control in the hardware
 *  if a serial peripheral supports it, otherwise software emulation is used.
 *
 * @param obj    The serial object
 * @param type   The type of the flow control. Look at the available FlowControl types.
 * @param rxflow The TX pin name
 * @param txflow The RX pin name
 */
void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow)
{
    uint16_t uen_flag = 0U;
    struct serial_s *p_obj = GET_SERIAL_S(obj);
    /* store the UEN flag */
    uen_flag = USART_CTL0(p_obj->uart) & USART_CTL0_UEN;

    UARTName uart_rts = (UARTName)pinmap_peripheral(rxflow, PinMap_UART_RTS);
    UARTName uart_cts = (UARTName)pinmap_peripheral(txflow, PinMap_UART_CTS);

    p_obj->uart = (UARTName)pinmap_merge(uart_cts, uart_rts);
    MBED_ASSERT(p_obj->uart != (UARTName)NC);

    /* disable USART to modify CTS/RTS configuration */
    usart_disable(p_obj->uart);

    if (type == FlowControlNone) {
        p_obj->hw_flow_ctl = USART_HWCONTROL_NONE;
        usart_hardware_flow_cts_config(p_obj->uart, USART_CTS_DISABLE);
        usart_hardware_flow_rts_config(p_obj->uart, USART_RTS_DISABLE);
    }

    if (type == FlowControlRTS) {
        MBED_ASSERT(uart_rts != (UARTName)NC);
        p_obj->hw_flow_ctl = USART_HWCONTROL_RTS;
        p_obj->pin_rts = rxflow;
        pinmap_pinout(rxflow, PinMap_UART_RTS);
        usart_hardware_flow_cts_config(p_obj->uart, USART_CTS_DISABLE);
        usart_hardware_flow_rts_config(p_obj->uart, USART_RTS_ENABLE);
    }

    if (type == FlowControlCTS) {
        MBED_ASSERT(uart_cts != (UARTName)NC);
        p_obj->hw_flow_ctl = USART_HWCONTROL_CTS;
        p_obj->pin_cts = txflow;
        pinmap_pinout(txflow, PinMap_UART_CTS);
        usart_hardware_flow_rts_config(p_obj->uart, USART_RTS_DISABLE);
        usart_hardware_flow_cts_config(p_obj->uart, USART_CTS_ENABLE);
    }

    if (type == FlowControlRTSCTS) {
        MBED_ASSERT(uart_rts != (UARTName)NC);
        MBED_ASSERT(uart_cts != (UARTName)NC);
        p_obj->hw_flow_ctl = USART_HWCONTROL_RTS_CTS;
        p_obj->pin_rts = rxflow;
        p_obj->pin_cts = txflow;
        pinmap_pinout(txflow, PinMap_UART_CTS);
        pinmap_pinout(rxflow, PinMap_UART_RTS);
        usart_hardware_flow_cts_config(p_obj->uart, USART_CTS_ENABLE);
        usart_hardware_flow_rts_config(p_obj->uart, USART_RTS_ENABLE);
    }

    /* restore the UEN flag */
    if (RESET != uen_flag) {
        usart_enable(p_obj->uart);
    }
}

#endif /* DEVICE_SERIAL_FC */

#if DEVICE_SLEEP
/** Check whether the serial is in busy state
 *
 * @return 0: all the serial is free to use, 1: some serial is in busy in transfer
 */
int serial_busy_state_check(void)
{
#if defined(USART0)
    if ((USART_CTL0(USART0) & USART_CTL0_UEN) && !(USART_STAT0(USART0) & USART_STAT0_TC)) {
        return 1;
    }
#endif

#if defined(USART1)
    if ((USART_CTL0(USART1) & USART_CTL0_UEN) && !(USART_STAT0(USART1) & USART_STAT0_TC)) {
        return 1;
    }
#endif

#if defined(USART2)
    if ((USART_CTL0(USART2) & USART_CTL0_UEN) && !(USART_STAT0(USART2) & USART_STAT0_TC)) {
        return 1;
    }
#endif

#if defined(UART3)
    if ((USART_CTL0(UART3) & USART_CTL0_UEN) && !(USART_STAT0(UART3) & USART_STAT0_TC)) {
        return 1;
    }
#endif

#if defined(UART4)
    if ((USART_CTL0(UART4) & USART_CTL0_UEN) && !(USART_STAT0(UART4) & USART_STAT0_TC)) {
        return 1;
    }
#endif

    /* no serial is in busy state */
    return 0;
}
#endif /* DEVICE_SLEEP */

#endif /* DEVICE_SERIAL */
