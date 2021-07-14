/* mbed Microcontroller Library
 * Copyright (c) 2019, Arm Limited and affiliates.
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

#include "hal/serial_api.h"
#include "cyhal_uart.h"
#include "mbed_error.h"
#include "PeripheralPins.h"
#include "PinNames.h"

#if DEVICE_SERIAL

#ifdef __cplusplus
extern "C" {
#endif

int stdio_uart_inited = 0;
serial_t stdio_uart;

static inline struct serial_s *cy_serial_get_struct(serial_t *obj)
{
#if DEVICE_SERIAL_ASYNCH
    return &(obj->serial);
#else
    return obj;
#endif
}

static void serial_handler_internal(void *handler_arg, cyhal_uart_event_t event)
{
    serial_t *obj = (serial_t *)handler_arg;
    struct serial_s *ser = cy_serial_get_struct(obj);
#if DEVICE_SERIAL_ASYNCH
    ser->event_flags = event;
    void (*async_rx_handler)(void) = (void (*)(void))ser->async_rx_handler;
    if (NULL != async_rx_handler && (event & ser->rx_event_mask)) {
        (*async_rx_handler)();
    }
    void (*async_tx_handler)(void) = (void (*)(void))ser->async_tx_handler;
    if (NULL != async_tx_handler && (event & ser->tx_event_mask)) {
        (*async_tx_handler)();
    }
#endif
    void (*handler)(uint32_t, int) = (void (*)(uint32_t, int))ser->handler;
    if (NULL != handler) {
        if (CYHAL_UART_IRQ_RX_NOT_EMPTY == (event & ser->rx_event_mask)) {
            (*handler)(ser->handler_arg, (int)RxIrq);
        }
        if (CYHAL_UART_IRQ_TX_EMPTY == (event & ser->tx_event_mask)) {
            (*handler)(ser->handler_arg, (int)TxIrq);
        }
    }
}

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    if (tx == STDIO_UART_TX && stdio_uart_inited != 0) {
        memmove(obj, &stdio_uart, sizeof(serial_t));
        return;
    }
    memset(obj, 0, sizeof(serial_t));
    struct serial_s *ser = cy_serial_get_struct(obj);
    ser->pin_tx = tx;
    ser->pin_rx = rx;
    ser->pin_rts = NC;
    ser->pin_cts = NC;
    if (CY_RSLT_SUCCESS != cyhal_uart_init(&(ser->hal_obj), tx, rx, NULL, NULL)) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_FAILED_OPERATION), "cyhal_uart_init");
    }
    cyhal_uart_register_callback(&(ser->hal_obj), &serial_handler_internal, obj);

#if DEVICE_SERIAL_ASYNCH
    static const cyhal_uart_event_t ENABLE_EVENTS = (cyhal_uart_event_t)
                                                    (CYHAL_UART_IRQ_TX_DONE | CYHAL_UART_IRQ_TX_ERROR |
                                                     CYHAL_UART_IRQ_RX_DONE | CYHAL_UART_IRQ_RX_ERROR);
    cyhal_uart_enable_event(&(ser->hal_obj), ENABLE_EVENTS, CYHAL_ISR_PRIORITY_DEFAULT, true);
#endif
    if (tx == STDIO_UART_TX) {
        memmove(&stdio_uart, obj, sizeof(serial_t));
        stdio_uart_inited = 1;
    }
}

void serial_free(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    cyhal_uart_free(&(ser->hal_obj));
    stdio_uart_inited = 0;
}

void serial_baud(serial_t *obj, int baudrate)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    if (CY_RSLT_SUCCESS != cyhal_uart_set_baud(&(ser->hal_obj), baudrate, NULL)) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_FAILED_OPERATION), "serial_baud");
    }
    ser->baud = (uint32_t)baudrate;
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    cyhal_uart_parity_t hal_parity;
    switch (parity) {
        case ParityNone:
            hal_parity = CYHAL_UART_PARITY_NONE;
            break;
        case ParityOdd:
            hal_parity = CYHAL_UART_PARITY_ODD;
            break;
        case ParityEven:
            hal_parity = CYHAL_UART_PARITY_EVEN;
            break;
        default:
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_UNSUPPORTED), "Unsupported parity");
    }
    cyhal_uart_cfg_t cfg = {
        .data_bits = data_bits,
        .stop_bits = stop_bits,
        .parity = hal_parity,
    };
    if (CY_RSLT_SUCCESS != cyhal_uart_configure(&(ser->hal_obj), &cfg)) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_FAILED_OPERATION), "cyhal_uart_configure");
    }
}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    ser->handler_arg = id;
    // The struct uses a different type because objects.h cannot include serial_api.h without creating a cycle
    ser->handler = (void *)handler;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    if (irq == RxIrq) {
        static const cyhal_uart_event_t interrupt_mask = (cyhal_uart_event_t)
                                                         (CYHAL_UART_IRQ_RX_DONE | CYHAL_UART_IRQ_RX_ERROR | CYHAL_UART_IRQ_RX_NOT_EMPTY);
        ser->rx_event_mask = enable
                             ? (cyhal_uart_event_t)(ser->rx_event_mask | interrupt_mask)
                             : (cyhal_uart_event_t)(ser->rx_event_mask & ~interrupt_mask);
        cyhal_uart_enable_event(&(ser->hal_obj), interrupt_mask, CYHAL_ISR_PRIORITY_DEFAULT, (bool)enable);
    } else if (irq == TxIrq) {
        static const cyhal_uart_event_t interrupt_mask = (cyhal_uart_event_t)(CYHAL_UART_IRQ_TX_DONE | CYHAL_UART_IRQ_TX_ERROR | CYHAL_UART_IRQ_TX_EMPTY);
        ser->tx_event_mask = enable
                             ? (cyhal_uart_event_t)(ser->tx_event_mask | interrupt_mask)
                             : (cyhal_uart_event_t)(ser->tx_event_mask & ~interrupt_mask);
        cyhal_uart_enable_event(&(ser->hal_obj), interrupt_mask, CYHAL_ISR_PRIORITY_DEFAULT, (bool)enable);
    }
}

int  serial_getc(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    uint8_t value;
    if (CY_RSLT_SUCCESS != cyhal_uart_getc(&(ser->hal_obj), &value, 0)) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_READ_FAILED), "serial_getc");
    }
    return value;
}

void serial_putc(serial_t *obj, int c)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    if (CY_RSLT_SUCCESS != cyhal_uart_putc(&(ser->hal_obj), (uint32_t)c)) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_WRITE_FAILED), "serial_putc");
    }
}

int  serial_readable(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    return cyhal_uart_readable(&(ser->hal_obj)) > 0 ? 1 : 0;
}

int  serial_writable(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    return cyhal_uart_writable(&(ser->hal_obj)) > 0 ? 1 : 0;
}

int  serial_tx_empty(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    return cyhal_uart_is_tx_active(&(ser->hal_obj)) ? 0 : 1;
}

void serial_clear(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    if (CY_RSLT_SUCCESS != cyhal_uart_clear(&(ser->hal_obj))) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_WRITE_FAILED), "serial_clear");
    }
}

void serial_break_set(serial_t *obj)
{
    MBED_WARNING(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_UNSUPPORTED), "serial_break_set");
}

void serial_break_clear(serial_t *obj)
{
    MBED_WARNING(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_UNSUPPORTED), "serial_break_clear");
}

void serial_pinout_tx(PinName tx)
{
    MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_UNSUPPORTED), "serial_pinout_tx");
}

void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    if (CY_RSLT_SUCCESS != cyhal_uart_set_flow_control(&(ser->hal_obj), txflow, rxflow)) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_FAILED_OPERATION), "serial_set_flow_control");
    }
    ser->pin_rts = rxflow;
    ser->pin_cts = txflow;
}

const PinMap *serial_tx_pinmap(void)
{
    return PinMap_UART_TX;
}

const PinMap *serial_rx_pinmap(void)
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

#if DEVICE_SERIAL_ASYNCH

int serial_tx_asynch(serial_t *obj, const void *tx, size_t tx_length, MBED_UNUSED uint8_t tx_width, uint32_t handler, uint32_t event, MBED_UNUSED DMAUsage dma)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    // handler calls serial_irq_handler_async
    ser->async_tx_handler = (void *)handler;
    if (CY_RSLT_SUCCESS != cyhal_uart_write_async(&(ser->hal_obj), (void *)tx, tx_length)) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_WRITE_FAILED), "serial_tx_asynch");
    }
    return 0;
}

void serial_rx_asynch(serial_t *obj, void *rx, size_t rx_length, MBED_UNUSED uint8_t rx_width, uint32_t handler, uint32_t event, uint8_t char_match, MBED_UNUSED DMAUsage dma)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    // handler calls serial_irq_handler_async
    ser->async_rx_handler = (void *)handler;
    if (CY_RSLT_SUCCESS != cyhal_uart_read_async(&(ser->hal_obj), rx, rx_length)) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_WRITE_FAILED), "serial_rx_asynch");
    }
}

uint8_t serial_tx_active(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    return cyhal_uart_is_tx_active(&(ser->hal_obj)) ? 1 : 0;
}

uint8_t serial_rx_active(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    return cyhal_uart_is_rx_active(&(ser->hal_obj)) ? 1 : 0;
}

int serial_irq_handler_asynch(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    int flags = 0;
    if (ser->event_flags & CYHAL_UART_IRQ_TX_DONE) {
        ser->async_tx_handler = NULL;
        flags |= SERIAL_EVENT_TX_COMPLETE;
    }
    if (ser->event_flags & CYHAL_UART_IRQ_RX_DONE) {
        ser->async_rx_handler = NULL;
        flags |= SERIAL_EVENT_RX_COMPLETE;
    }
    // HAL event type does not specify which error occurred
    if (ser->event_flags & CYHAL_UART_IRQ_RX_ERROR) {
        flags |= SERIAL_EVENT_RX_FRAMING_ERROR;
    }
    return flags;
}

void serial_tx_abort_asynch(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    if (CY_RSLT_SUCCESS != cyhal_uart_write_abort(&(ser->hal_obj))) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_FAILED_OPERATION), "serial_tx_abort_asynch");
    }
}

void serial_rx_abort_asynch(serial_t *obj)
{
    struct serial_s *ser = cy_serial_get_struct(obj);
    if (CY_RSLT_SUCCESS != cyhal_uart_read_abort(&(ser->hal_obj))) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_FAILED_OPERATION), "serial_rx_abort_asynch");
    }
}

#endif

#ifdef __cplusplus
}
#endif

#endif
