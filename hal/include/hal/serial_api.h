
/** \addtogroup hal */
/** @{*/
/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
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
#ifndef MBED_SERIAL_API_H
#define MBED_SERIAL_API_H

#include "device.h"
#include "pinmap.h"
#include "hal/buffer.h"
#include "hal/dma_api.h"

#if DEVICE_SERIAL

#define SERIAL_EVENT_TX_SHIFT (2)
#define SERIAL_EVENT_RX_SHIFT (8)

#define SERIAL_EVENT_TX_MASK (0x00FC)
#define SERIAL_EVENT_RX_MASK (0x3F00)

#define SERIAL_EVENT_ERROR (1 << 1)

/**
 * @defgroup SerialTXEvents Serial TX Events Macros
 *
 * @{
 */
#define SERIAL_EVENT_TX_COMPLETE (1 << (SERIAL_EVENT_TX_SHIFT + 0))
#define SERIAL_EVENT_TX_ALL      (SERIAL_EVENT_TX_COMPLETE)
/**@}*/

/**
 * @defgroup SerialRXEvents Serial RX Events Macros
 *
 * @{
 */
#define SERIAL_EVENT_RX_COMPLETE        (1 << (SERIAL_EVENT_RX_SHIFT + 0))
#define SERIAL_EVENT_RX_OVERRUN_ERROR   (1 << (SERIAL_EVENT_RX_SHIFT + 1))
#define SERIAL_EVENT_RX_FRAMING_ERROR   (1 << (SERIAL_EVENT_RX_SHIFT + 2))
#define SERIAL_EVENT_RX_PARITY_ERROR    (1 << (SERIAL_EVENT_RX_SHIFT + 3))
#define SERIAL_EVENT_RX_OVERFLOW        (1 << (SERIAL_EVENT_RX_SHIFT + 4))
#define SERIAL_EVENT_RX_CHARACTER_MATCH (1 << (SERIAL_EVENT_RX_SHIFT + 5))
#define SERIAL_EVENT_RX_ALL             (SERIAL_EVENT_RX_OVERFLOW | SERIAL_EVENT_RX_PARITY_ERROR | \
                                         SERIAL_EVENT_RX_FRAMING_ERROR | SERIAL_EVENT_RX_OVERRUN_ERROR | \
                                         SERIAL_EVENT_RX_COMPLETE | SERIAL_EVENT_RX_CHARACTER_MATCH)
/**@}*/

#define SERIAL_RESERVED_CHAR_MATCH (255)

typedef enum {
    ParityNone = 0,
    ParityOdd = 1,
    ParityEven = 2,
    ParityForced1 = 3,
    ParityForced0 = 4
} SerialParity;

/** Serial interrupt sources
 */
typedef enum {
    RxIrq,      /**< Receive Data Register Full */
    TxIrq       /**< Transmit Data Register Empty */
} SerialIrq;

typedef enum {
    FlowControlNone,
    FlowControlRTS,
    FlowControlCTS,
    FlowControlRTSCTS
} FlowControl;

typedef void (*uart_irq_handler)(uint32_t id, SerialIrq event);

#if DEVICE_SERIAL_ASYNCH
/** Asynch serial HAL structure
 */
typedef struct {
    struct serial_s serial;  /**< Target specific serial structure */
    struct buffer_s tx_buff; /**< TX buffer */
    struct buffer_s rx_buff; /**< RX buffer */
    uint8_t char_match;      /**< Character to be matched */
    uint8_t char_found;      /**< State of the matched character */
} serial_t;

#else
/** Non-asynch serial HAL structure
 */
typedef struct serial_s serial_t;

#endif

typedef struct {
    int peripheral;
    PinName tx_pin;
    int tx_function;
    PinName rx_pin;
    int rx_function;
    bool stdio_config;
} serial_pinmap_t;

typedef struct {
    int peripheral;
    PinName tx_flow_pin;
    int tx_flow_function;
    PinName rx_flow_pin;
    int rx_flow_function;
} serial_fc_pinmap_t;

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * \defgroup hal_GeneralSerial_tests Serial hal tests
 * The Serial HAL tests ensure driver conformance to defined behavior.
 *
 * To run the Serial hal tests use the command:
 *
 *     mbed test -t <toolchain> -m <target> -n tests-mbed_hal_fpga_ci_test_shield-uart
 *
 */

/** Initialize the serial peripheral. It sets the default parameters for serial
 *  peripheral, and configures its specifieds pins.
 *
 * @param obj The serial object
 * @param tx  The TX pin name
 * @param rx  The RX pin name
 */
void serial_init(serial_t *obj, PinName tx, PinName rx);

/** Initialize the serial peripheral. It sets the default parameters for serial
 *  peripheral, and configures its specifieds pins.
 *
 * @param obj The serial object
 * @param pinmap pointer to structure which holds static pinmap
 */
void serial_init_direct(serial_t *obj, const serial_pinmap_t *pinmap);


/** Release the serial peripheral, not currently invoked. It requires further
 *  resource management.
 *
 * @param obj The serial object
 */
void serial_free(serial_t *obj);

/** Configure the baud rate
 *
 * @param obj      The serial object
 * @param baudrate The baud rate to be configured
 */
void serial_baud(serial_t *obj, int baudrate);

/** Configure the format. Set the number of bits, parity and the number of stop bits
 *
 * @param obj       The serial object
 * @param data_bits The number of data bits
 * @param parity    The parity
 * @param stop_bits The number of stop bits
 */
void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits);

/** The serial interrupt handler registration
 *
 * @param obj     The serial object
 * @param handler The interrupt handler which will be invoked when the interrupt fires
 * @param id      The SerialBase object
 */
void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id);

/** Configure serial interrupt. This function is used for word-approach
 *
 * @param obj    The serial object
 * @param irq    The serial IRQ type (RX or TX)
 * @param enable Set to non-zero to enable events, or zero to disable them
 */
void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable);

/** Get character. This is a blocking call, waiting for a character
 *
 * @param obj The serial object
 */
int  serial_getc(serial_t *obj);

/** Send a character. This is a blocking call, waiting for a peripheral to be available
 *  for writing
 *
 * @param obj The serial object
 * @param c   The character to be sent
 */
void serial_putc(serial_t *obj, int c);

/** Check if the serial peripheral is readable
 *
 * @param obj The serial object
 * @return Non-zero value if a character can be read, 0 if nothing to read
 */
int  serial_readable(serial_t *obj);

/** Check if the serial peripheral is writable
 *
 * @param obj The serial object
 * @return Non-zero value if a character can be written, 0 otherwise.
 */
int  serial_writable(serial_t *obj);

/** Check if the serial peripheral tx buffer or tx FIFO is empty and the last byte has
 *  been completely transmitted from the shift register to the bus.
 *
 * @param obj The serial object
 * @return Non-zero value if tx is empty, 0 otherwise.
 */
int  serial_tx_empty(serial_t *obj);

/** Clear the serial peripheral
 *
 * @param obj The serial object
 */
void serial_clear(serial_t *obj);

/** Set the break
 *
 * @param obj The serial object
 */
void serial_break_set(serial_t *obj);

/** Clear the break
 *
 * @param obj The serial object
 */
void serial_break_clear(serial_t *obj);

/** Configure the TX pin for UART function.
 *
 * @param tx The pin name used for TX
 */
void serial_pinout_tx(PinName tx);

#if DEVICE_SERIAL_FC
/** Configure the serial for the flow control. It sets flow control in the hardware
 *  if a serial peripheral supports it, otherwise software emulation is used.
 *
 * @param obj    The serial object
 * @param type   The type of the flow control. Look at the available FlowControl types.
 * @param rxflow The TX pin name
 * @param txflow The RX pin name
 */
void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow);

/** Configure the serial for the flow control. It sets flow control in the hardware
 *  if a serial peripheral supports it, otherwise software emulation is used.
 *
 * @param obj    The serial object
 * @param type   The type of the flow control. Look at the available FlowControl types.
 * @param pinmap Pointer to structure which holds static pinmap
 */
void serial_set_flow_control_direct(serial_t *obj, FlowControl type, const serial_fc_pinmap_t *pinmap);
#endif

/** Get the pins that support Serial TX
 *
 * Return a PinMap array of pins that support Serial TX. The
 * array is terminated with {NC, NC, 0}.
 *
 * @return PinMap array
 */
const PinMap *serial_tx_pinmap(void);

/** Get the pins that support Serial RX
 *
 * Return a PinMap array of pins that support Serial RX. The
 * array is terminated with {NC, NC, 0}.
 *
 * @return PinMap array
 */
const PinMap *serial_rx_pinmap(void);

#if DEVICE_SERIAL_FC
/** Get the pins that support Serial CTS
 *
 * Return a PinMap array of pins that support Serial CTS. The
 * array is terminated with {NC, NC, 0}.
 *
 * @return PinMap array
 */
const PinMap *serial_cts_pinmap(void);

/** Get the pins that support Serial RTS
 *
 * Return a PinMap array of pins that support Serial RTS. The
 * array is terminated with {NC, NC, 0}.
 *
 * @return PinMap array
 */
const PinMap *serial_rts_pinmap(void);
#endif

#if DEVICE_SERIAL_ASYNCH

/**@}*/

/**
 * \defgroup hal_AsynchSerial Asynchronous Serial Hardware Abstraction Layer
 * @{
 */

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
int serial_tx_asynch(serial_t *obj, const void *tx, size_t tx_length, uint8_t tx_width, uint32_t handler, uint32_t event, DMAUsage hint);

/** Begin asynchronous RX transfer (enable interrupt for data collecting)
 *  The used buffer is specified in the serial object - rx_buff
 *
 * @param obj        The serial object
 * @param rx         The receive buffer
 * @param rx_length  The number of bytes to receive
 * @param rx_width   Deprecated argument
 * @param handler    The serial handler
 * @param event      The logical OR of events to be registered
 * @param char_match A character in range 0-254 to be matched
 * @param hint       A suggestion for how to use DMA with this transfer
 */
void serial_rx_asynch(serial_t *obj, void *rx, size_t rx_length, uint8_t rx_width, uint32_t handler, uint32_t event, uint8_t char_match, DMAUsage hint);

/** Attempts to determine if the serial peripheral is already in use for TX
 *
 * @param obj The serial object
 * @return Non-zero if the RX transaction is ongoing, 0 otherwise
 */
uint8_t serial_tx_active(serial_t *obj);

/** Attempts to determine if the serial peripheral is already in use for RX
 *
 * @param obj The serial object
 * @return Non-zero if the RX transaction is ongoing, 0 otherwise
 */
uint8_t serial_rx_active(serial_t *obj);

/** The asynchronous TX and RX handler.
 *
 * @param obj The serial object
 * @return Returns event flags if an RX transfer termination condition was met; otherwise returns 0
 */
int serial_irq_handler_asynch(serial_t *obj);

/** Abort the ongoing TX transaction. It disables the enabled interupt for TX and
 *  flushes the TX hardware buffer if TX FIFO is used
 *
 * @param obj The serial object
 */
void serial_tx_abort_asynch(serial_t *obj);

/** Abort the ongoing RX transaction. It disables the enabled interrupt for RX and
 *  flushes the RX hardware buffer if RX FIFO is used
 *
 * @param obj The serial object
 */
void serial_rx_abort_asynch(serial_t *obj);

/**@}*/

#endif

#ifdef __cplusplus
}
#endif

#endif

#endif

/** @}*/
