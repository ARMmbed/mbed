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
#include "mbed_error.h"
#include "PinNames.h"
#include "PeripheralPins.h"
#include <string.h>

#if DEVICE_SERIAL

#ifdef __cplusplus
extern "C" {
#endif

int stdio_uart_inited = 0;
serial_t stdio_uart;

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    if (tx == STDIO_UART_TX && stdio_uart_inited != 0) {
        memmove(obj, &stdio_uart, sizeof(serial_t));
        return;
    }
    memset(obj, 0, sizeof(serial_t));
    struct serial_s *ser = obj;
    ser->pin_tx = tx;
    ser->pin_rx = rx;
    ser->pin_rts = NC;
    ser->pin_cts = NC;

    UARTName dev_tx = (UARTName)pinmap_peripheral(tx, PinMap_UART_TX);
    UARTName dev_rx = (UARTName)pinmap_peripheral(rx, PinMap_UART_RX);

    if (dev_tx != dev_rx) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_UNSUPPORTED), "Invalid serial pins");
    }
    if (dev_tx == UART_0) {
        obj->dev = uart0;
    } else {
        obj->dev = uart1;
    }

    uart_init(obj->dev, MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE);
    gpio_set_function(tx, GPIO_FUNC_UART);
    gpio_set_function(rx, GPIO_FUNC_UART);

    //uart_set_translate_crlf(obj->dev, false);
    uart_set_fifo_enabled(obj->dev, false);

    if (tx == STDIO_UART_TX) {
        memmove(&stdio_uart, obj, sizeof(serial_t));
        stdio_uart_inited = 1;
    }
}

void serial_free(serial_t *obj)
{
    uart_deinit(obj->dev);
}

void serial_baud(serial_t *obj, int baudrate)
{
    obj->baud = (uint32_t)baudrate;
    uart_init(obj->dev, obj->baud);
    uart_set_fifo_enabled(obj->dev, false);
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    uart_parity_t hal_parity;
    switch (parity) {
        case ParityNone:
            hal_parity = UART_PARITY_NONE;
            break;
        case ParityOdd:
            hal_parity = UART_PARITY_ODD;
            break;
        case ParityEven:
            hal_parity = UART_PARITY_EVEN;
            break;
        default:
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_UNSUPPORTED), "Unsupported parity");
    }
    uart_set_format(obj->dev, data_bits, stop_bits, hal_parity);
}

static volatile uart_irq_handler irq_handler;
static volatile uint32_t serial_irq_ids[2] = {0};

static inline void uart0_irq(void)
{
    irq_handler(serial_irq_ids[0], RxIrq);
}

static inline void uart1_irq(void)
{
    irq_handler(serial_irq_ids[1], RxIrq);
}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    int UART_IRQ = obj->dev == uart0 ? UART0_IRQ : UART1_IRQ;
    if (obj->dev == uart0) {
        serial_irq_ids[0] = id;
    } else {
        serial_irq_ids[1] = id;
    }
    irq_handler = handler;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    int UART_IRQ = obj->dev == uart0 ? UART0_IRQ : UART1_IRQ;

    irq_set_exclusive_handler(UART_IRQ, obj->dev == uart0 ? uart0_irq : uart1_irq);
    irq_set_enabled(UART_IRQ, enable);

    uart_set_irq_enables(obj->dev, irq == RxIrq, irq == TxIrq);
}

int serial_getc(serial_t *obj)
{
    return uart_getc(obj->dev);
}

void serial_putc(serial_t *obj, int c)
{
    uart_putc_raw(obj->dev, c);
}

int serial_readable(serial_t *obj)
{
    return uart_is_readable(obj->dev);
}

int serial_writable(serial_t *obj)
{
    return uart_is_writable(obj->dev);
}

void serial_clear(serial_t *obj)
{
    MBED_WARNING(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_UNSUPPORTED), "serial_clear");
}

void serial_break_set(serial_t *obj)
{
    uart_set_break(obj->dev, true);
}

void serial_break_clear(serial_t *obj)
{
    uart_set_break(obj->dev, false);
}

void serial_pinout_tx(PinName tx)
{
    MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER_SERIAL, MBED_ERROR_CODE_UNSUPPORTED), "serial_pinout_tx");
}

void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow)
{
    obj->pin_rts = rxflow;
    obj->pin_cts = txflow;

    if (obj->pin_rts != NC) {
        gpio_set_function(obj->pin_rts, GPIO_FUNC_UART);
    }
    if (obj->pin_cts != NC) {
        gpio_set_function(obj->pin_cts, GPIO_FUNC_UART);
    }

    if (type == FlowControlRTSCTS) {
        uart_set_hw_flow(obj->dev, true, true);
    } else {
        uart_set_hw_flow(obj->dev, type == FlowControlCTS, type == FlowControlRTS);
    }
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

#endif