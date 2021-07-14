/*
 * Copyright (c) 2020 SparkFun Electronics
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if DEVICE_SERIAL

#include "serial_api.h"

#include "mbed_assert.h"
#include "PeripheralPins.h"

// globals
int stdio_uart_inited = 0;
serial_t stdio_uart;
bool value = false;

// interrupt variables
static uart_irq_handler irq_handler;
static ap3_uart_control_t ap3_uart_control[AM_REG_UART_NUM_MODULES];

// forward declarations
extern void am_uart_isr(void);
extern void am_uart1_isr(void);
void uart_configure_pin_function(PinName pin, UARTName uart, const PinMap *map);

/**
 * \defgroup hal_GeneralSerial Serial Configuration Functions
 *
 * # Defined behavior
 * * ::serial_init initializes the ::serial_t
 * * ::serial_init sets the default parameters for serial peripheral (9600 bps, 8N1 format)
 * * ::serial_init configures the specified pins
 * * ::serial_free releases the serial peripheral
 * * ::serial_baud configures the baud rate
 * * at least 9600 bps the baud rate  must be supported
 * * ::serial_format configures the transmission format (number of bits, parity and the number of stop bits)
 * * at least 8N1 format must be supported
 * * ::serial_irq_handler registers the interrupt handler which will be invoked when the interrupt fires.
 * * ::serial_irq_set enables or disables the serial RX or TX IRQ.
 * * If `RxIrq` is enabled by ::serial_irq_set, ::serial_irq_handler will be invoked whenever
 * Receive Data Register Full IRQ is generated.
 * * If `TxIrq` is enabled by ::serial_irq_set, ::serial_irq_handler will be invoked whenever
 * Transmit Data Register Empty IRQ is generated.
 * * If the interrupt condition holds true, when the interrupt is enabled with ::serial_irq_set,
 * the ::serial_irq_handler is called instantly.
 * * ::serial_getc returns the character from serial buffer.
 * * ::serial_getc is a blocking call (waits for the character).
 * * ::serial_putc sends a character.
 * * ::serial_putc is a blocking call (waits for a peripheral to be available).
 * * ::serial_readable returns non-zero value if a character can be read, 0 otherwise.
 * * ::serial_writable returns non-zero value if a character can be written, 0 otherwise.
 * * ::serial_tx_empty returns non-zero value if tx is empty, 0 otherwise.
 * * ::serial_clear clears the ::serial_t RX/TX buffers
 * * ::serial_break_set sets the break signal.
 * * ::serial_break_clear clears the break signal.
 * * ::serial_pinout_tx configures the TX pin as an output (to be used in half-duplex mode).
 * * ::serial_set_flow_control configures serial flow control.
 * * ::serial_set_flow_control sets flow control in the hardware if a serial peripheral supports it,
 * otherwise software emulation is used.
 * * ::serial_tx_asynch starts the serial asynchronous transfer.
 * * ::serial_tx_asynch writes `tx_length` bytes from the `tx` to the bus.
 * * ::serial_tx_asynch must support 8 data bits
 * * The callback given to ::serial_tx_asynch is invoked when the transfer completes.
 * * ::serial_tx_asynch specifies the logical OR of events to be registered.
 * * The ::serial_tx_asynch function may use the `DMAUsage` hint to select the appropriate async algorithm.
 * * ::serial_rx_asynch starts the serial asynchronous transfer.
 * * ::serial_rx_asynch reads `rx_length` bytes to the `rx` buffer.
 * * ::serial_rx_asynch must support 8 data bits
 * * The callback given to ::serial_rx_asynch is invoked when the transfer completes.
 * * ::serial_rx_asynch specifies the logical OR of events to be registered.
 * * The ::serial_rx_asynch function may use the `DMAUsage` hint to select the appropriate async algorithm.
 * * ::serial_rx_asynch specifies a character in range 0-254 to be matched, 255 is a reserved value.
 * * If SERIAL_EVENT_RX_CHARACTER_MATCH event is not registered, the `char_match` is ignored.
 * * The SERIAL_EVENT_RX_CHARACTER_MATCH event is set in the callback when SERIAL_EVENT_RX_CHARACTER_MATCH event is
 * registered AND `char_match` is present in the received data.
 * * ::serial_tx_active returns non-zero if the TX transaction is ongoing, 0 otherwise.
 * * ::serial_rx_active returns non-zero if the RX transaction is ongoing, 0 otherwise.
 * * ::serial_irq_handler_asynch returns event flags if a transfer termination condition was met, otherwise returns 0.
 * * ::serial_irq_handler_asynch takes no longer than one packet transfer time (packet_bits / baudrate) to execute.
 * * ::serial_tx_abort_asynch aborts the ongoing TX transaction.
 * * ::serial_tx_abort_asynch disables the enabled interupt for TX.
 * * ::serial_tx_abort_asynch flushes the TX hardware buffer if TX FIFO is used.
 * * ::serial_rx_abort_asynch aborts the ongoing RX transaction.
 * * ::serial_rx_abort_asynch disables the enabled interupt for RX.
 * * ::serial_rx_abort_asynch flushes the TX hardware buffer if RX FIFO is used.
 * * Correct operation guaranteed when interrupt latency is shorter than one packet transfer time (packet_bits / baudrate)
 * if the flow control is not used.
 * * Correct operation guaranteed regardless of interrupt latency if the flow control is used.
 *
 * # Undefined behavior
 * * Calling ::serial_init multiple times on the same `serial_t` without ::serial_free.
 * * Passing invalid pin to ::serial_init, ::serial_pinout_tx.
 * * Calling any function other than ::serial_init on am uninitialized or freed `serial_t`.
 * * Passing an invalid pointer as `obj` to any function.
 * * Passing an invalid pointer as `handler` to ::serial_irq_handler, ::serial_tx_asynch, ::serial_rx_asynch.
 * * Calling ::serial_tx_abort while no async TX transfer is being processed.
 * * Calling ::serial_rx_abort while no async RX transfer is being processed.
 * * Devices behavior is undefined when the interrupt latency is longer than one packet transfer time
 * (packet_bits / baudrate) if the flow control is not used.
 * @{
 */

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    // determine the UART to use
    UARTName uart_tx = (UARTName)pinmap_peripheral(tx, serial_tx_pinmap());
    UARTName uart_rx = (UARTName)pinmap_peripheral(rx, serial_rx_pinmap());
    UARTName uart = (UARTName)pinmap_merge(uart_tx, uart_rx);
    MBED_ASSERT((int)uart != NC);
    obj->serial.uart_control = &ap3_uart_control[uart];
    obj->serial.uart_control->inst = uart;

    // config uart pins
    pinmap_config(tx, serial_tx_pinmap());
    pinmap_config(rx, serial_rx_pinmap());

    if (!obj->serial.uart_control->handle) {
        // if handle uninitialized this is first time set up
        // ensure that HAL queueing is disabled (we want to use the FIFOs directly)
        obj->serial.uart_control->cfg.pui8RxBuffer = NULL;
        obj->serial.uart_control->cfg.pui8TxBuffer = NULL;
        obj->serial.uart_control->cfg.ui32RxBufferSize = 0;
        obj->serial.uart_control->cfg.ui32TxBufferSize = 0;

        obj->serial.uart_control->cfg.ui32FifoLevels = AM_HAL_UART_RX_FIFO_7_8;

        // start UART instance
        MBED_ASSERT(am_hal_uart_initialize(uart, &(obj->serial.uart_control->handle)) == AM_HAL_STATUS_SUCCESS);
        MBED_ASSERT(am_hal_uart_power_control(obj->serial.uart_control->handle, AM_HAL_SYSCTRL_WAKE, false) == AM_HAL_STATUS_SUCCESS);
        MBED_ASSERT(am_hal_uart_configure_fifo(obj->serial.uart_control->handle, &(obj->serial.uart_control->cfg), false) == AM_HAL_STATUS_SUCCESS);

        // set default format
        serial_format(obj, 8, ParityNone, 1);
    }
}

void serial_free(serial_t *obj)
{
    // nothing to do unless resources are allocated for members of the serial_s serial member of obj
    // assuming mbed handles obj and its members
}

void serial_baud(serial_t *obj, int baudrate)
{
    obj->serial.uart_control->cfg.ui32BaudRate = (uint32_t)baudrate;
    MBED_ASSERT(am_hal_uart_configure_fifo(obj->serial.uart_control->handle, &(obj->serial.uart_control->cfg), false) == AM_HAL_STATUS_SUCCESS);
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    uint32_t am_hal_data_bits = 0;
    switch (data_bits) {
        case 5:
            am_hal_data_bits = AM_HAL_UART_DATA_BITS_5;
            break;
        case 6:
            am_hal_data_bits = AM_HAL_UART_DATA_BITS_6;
            break;
        case 7:
            am_hal_data_bits = AM_HAL_UART_DATA_BITS_7;
            break;
        case 8:
            am_hal_data_bits = AM_HAL_UART_DATA_BITS_8;
            break;
        default:
            MBED_ASSERT(0);
            break;
    }

    uint32_t am_hal_parity = AM_HAL_UART_PARITY_NONE;
    switch (parity) {
        case ParityNone:
            am_hal_parity = AM_HAL_UART_PARITY_NONE;
            break;
        case ParityOdd:
            am_hal_parity = AM_HAL_UART_PARITY_ODD;
            break;
        case ParityEven:
            am_hal_parity = AM_HAL_UART_PARITY_EVEN;
            break;
        default: // fall-through intentional after default
        case ParityForced1:
        case ParityForced0:
            MBED_ASSERT(0);
            break;
    }

    uint32_t am_hal_stop_bits = 0;
    switch (stop_bits) {
        case 1:
            am_hal_stop_bits = AM_HAL_UART_ONE_STOP_BIT;
            break;
        case 2:
            am_hal_stop_bits = AM_HAL_UART_TWO_STOP_BITS;
            break;
        default:
            MBED_ASSERT(0);
    }

    obj->serial.uart_control->cfg.ui32DataBits = (uint32_t)am_hal_data_bits;
    obj->serial.uart_control->cfg.ui32Parity = (uint32_t)am_hal_parity;
    obj->serial.uart_control->cfg.ui32StopBits = (uint32_t)am_hal_stop_bits;
    MBED_ASSERT(am_hal_uart_configure_fifo(obj->serial.uart_control->handle, &(obj->serial.uart_control->cfg), false) == AM_HAL_STATUS_SUCCESS);
}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    irq_handler = handler;
    obj->serial.uart_control->serial_irq_id = id;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    MBED_ASSERT(obj->serial.uart_control->handle != NULL);
    if (enable) {
        switch (irq) {
            case RxIrq:
                MBED_ASSERT(am_hal_uart_interrupt_enable(obj->serial.uart_control->handle, AM_HAL_UART_INT_RX) == AM_HAL_STATUS_SUCCESS);
                break;
            case TxIrq:
                MBED_ASSERT(am_hal_uart_interrupt_enable(obj->serial.uart_control->handle, AM_HAL_UART_INT_TXCMP) == AM_HAL_STATUS_SUCCESS);
                break;
            default:
                break;
        }
        // NVIC_SetVector(uart_irqs[obj->serial.index], vector);
        NVIC_EnableIRQ((IRQn_Type)(UART0_IRQn + obj->serial.uart_control->inst));
    } else { // disable
        switch (irq) {
            case RxIrq:
                MBED_ASSERT(am_hal_uart_interrupt_disable(obj->serial.uart_control->handle, AM_HAL_UART_INT_RX) == AM_HAL_STATUS_SUCCESS);
                break;
            case TxIrq:
                MBED_ASSERT(am_hal_uart_interrupt_disable(obj->serial.uart_control->handle, AM_HAL_UART_INT_TXCMP) == AM_HAL_STATUS_SUCCESS);
                break;
            default:
                break;
        }
    }
}

int serial_getc(serial_t *obj)
{
    MBED_ASSERT(obj->serial.uart_control != NULL);

    uint8_t rx_c = 0x00;
    volatile uint32_t bytes_read = 0x00;
    am_hal_uart_transfer_t am_hal_uart_xfer_read_single = {
        .ui32Direction = AM_HAL_UART_READ,
        .pui8Data = (uint8_t *) &rx_c,
        .ui32NumBytes = 1,
        .ui32TimeoutMs = 0,
        .pui32BytesTransferred = (uint32_t *) &bytes_read,
    };

    do {
        am_hal_uart_transfer(obj->serial.uart_control->handle, &am_hal_uart_xfer_read_single);
    } while (bytes_read == 0);

    return (int)rx_c;
}

void serial_putc(serial_t *obj, int c)
{
    MBED_ASSERT(obj->serial.uart_control != NULL);

    volatile uint32_t bytes_sent = 0;
    am_hal_uart_transfer_t am_hal_uart_xfer_write_single = {
        .ui32Direction = AM_HAL_UART_WRITE,
        .pui8Data = (uint8_t *)(&c),
        .ui32NumBytes = 1,
        .ui32TimeoutMs = 0,
        .pui32BytesTransferred = (uint32_t *) &bytes_sent,
    };

    do {
        am_hal_uart_transfer(obj->serial.uart_control->handle, &am_hal_uart_xfer_write_single);
    } while (bytes_sent == 0);
}

int serial_readable(serial_t *obj)
{
    MBED_ASSERT(obj->serial.uart_control != NULL);
    return !(UARTn(obj->serial.uart_control->inst)->FR_b.RXFE);
}

int serial_writable(serial_t *obj)
{
    MBED_ASSERT(obj->serial.uart_control != NULL);
    return !(UARTn(obj->serial.uart_control->inst)->FR_b.TXFF);
}

int serial_tx_empty(serial_t *obj)
{
    MBED_ASSERT(obj->serial.uart_control != NULL);
    return ((UARTn(obj->serial.uart_control->inst)->FR_b.TXFE) && (!(UARTn(obj->serial.uart_control->inst)->FR_b.TXBUSY)));
}

void serial_clear(serial_t *obj)
{
    // todo:
    MBED_ASSERT(0);
}

void serial_break_set(serial_t *obj)
{
    MBED_ASSERT(obj->serial.uart_control != NULL);
    UARTn(obj->serial.uart_control->inst)->LCRH |= UART0_LCRH_BRK_Msk;
}

void serial_break_clear(serial_t *obj)
{
    MBED_ASSERT(obj->serial.uart_control != NULL);
    UARTn(obj->serial.uart_control->inst)->LCRH &= ~UART0_LCRH_BRK_Msk;
}

void serial_pinout_tx(PinName tx)
{
    // todo: vestigial?
    MBED_ASSERT(0);
}

#if DEVICE_SERIAL_FC

void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow)
{
    // todo:
    MBED_ASSERT(0);
}

void serial_set_flow_control_direct(serial_t *obj, FlowControl type, const serial_fc_pinmap_t *pinmap)
{
    // todo:
    MBED_ASSERT(0);
}
#endif

const PinMap *serial_tx_pinmap(void)
{
    return PinMap_UART_TX;
}

const PinMap *serial_rx_pinmap(void)
{
    return PinMap_UART_RX;
}

#if DEVICE_SERIAL_FC

const PinMap *serial_cts_pinmap(void)
{
    return PinMap_UART_CTS;
}

const PinMap *serial_rts_pinmap(void)
{
    return PinMap_UART_RTS;
}
#endif

static inline void uart_irq(uint32_t instance)
{
    void *handle = ap3_uart_control[instance].handle;
    MBED_ASSERT(handle != NULL);

    // check flags
    uint32_t status = 0x00;
    MBED_ASSERT(am_hal_uart_interrupt_status_get(handle, &status, true) == AM_HAL_STATUS_SUCCESS);
    MBED_ASSERT(am_hal_uart_interrupt_clear(handle, status) == AM_HAL_STATUS_SUCCESS);

    if (ap3_uart_control[instance].serial_irq_id != 0) {
        if (status & AM_HAL_UART_INT_TXCMP) { // for transmit complete
            if (irq_handler) {
                irq_handler(ap3_uart_control[instance].serial_irq_id, TxIrq);
            }
        }
        if (status & AM_HAL_UART_INT_RX) { // for receive complete
            if (irq_handler) {
                irq_handler(ap3_uart_control[instance].serial_irq_id, RxIrq);
            }
        }
    }
}

extern void am_uart_isr(void)
{
    uart_irq(UART_0);
}

extern void am_uart1_isr(void)
{
    uart_irq(UART_1);
}

#ifdef __cplusplus
}
#endif

#endif

/** @}*/
