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
#ifndef MBED_SERIALBASE_H
#define MBED_SERIALBASE_H

#include "platform/platform.h"

#if DEVICE_SERIAL || defined(DOXYGEN_ONLY)

#include "platform/Callback.h"
#include "hal/serial_api.h"
#include "platform/mbed_toolchain.h"
#include "platform/NonCopyable.h"

#if DEVICE_SERIAL_ASYNCH
#include "platform/CThunk.h"
#include "hal/dma_api.h"
#endif

namespace mbed {
/**
 * \defgroup drivers_SerialBase SerialBase class
 * \ingroup drivers-public-api-uart
 * @{
 */

/** A base class for serial port implementations
 * Can't be instantiated directly (use UnbufferedSerial or BufferedSerial)
 *
 * @note Synchronization level: Set by subclass
 */
class SerialBase : private NonCopyable<SerialBase> {

public:
    /** Set the baud rate of the serial port
     *
     *  @param baudrate The baudrate of the serial port (default = 9600).
     */
    void baud(int baudrate);

    enum Parity {
        None = 0,
        Odd,
        Even,
        Forced1,
        Forced0
    };

    enum IrqType {
        RxIrq = 0,
        TxIrq,

        IrqCnt
    };

    enum Flow {
        Disabled = 0,
        RTS,
        CTS,
        RTSCTS
    };

    /** Set the transmission format used by the serial port
     *
     *  @param bits The number of bits in a word (5-8; default = 8)
     *  @param parity The parity used (SerialBase::None, SerialBase::Odd, SerialBase::Even, SerialBase::Forced1, SerialBase::Forced0; default = SerialBase::None)
     *  @param stop_bits The number of stop bits (1 or 2; default = 1)
     */
    void format(int bits = 8, Parity parity = SerialBase::None, int stop_bits = 1);

    /** Determine if there is a character available to read
     *
     *  @returns
     *    1 if there is a character available to read,
     *    0 otherwise
     */
    int readable();

    /** Determine if there is space available to write a character
     *
     *  @returns
     *    1 if there is space to write a character,
     *    0 otherwise
     */
    int writeable();

    /** Attach a function to call whenever a serial interrupt is generated
     *
     *  @param func A pointer to a void function, or 0 to set as none
     *  @param type Which serial interrupt to attach the member function to (Serial::RxIrq for receive, TxIrq for transmit buffer empty)
     */
    void attach(Callback<void()> func, IrqType type = RxIrq);

    /** Generate a break condition on the serial line
     *  NOTE: Clear break needs to run at least one frame after set_break is called
     */
    void set_break();

    /** Clear a break condition on the serial line
     *  NOTE: Should be run at least one frame after set_break is called
     */
    void clear_break();

    /** Generate a break condition on the serial line
     */
    void send_break();

    /** Ensures all data stored in peripheral buffers is transmitted to the bus
     *
     *  The function shall block until all the content of peripheral buffers is transmitted.
     *
     *  @return         0 on success, negative error code on failure
     */
    int sync();

    /** Enable serial input
     *
     * If both serial input and serial output are disabled, the
     * peripheral is freed. If either serial input or serial
     * output is re-enabled, the peripheral is reinitialized.
     *
     * On re-initialization rx interrupts will be enabled if a
     * rx handler is attached. The rx handler is called once
     * during re-initialization.
     */
    void enable_input(bool enable = true);

    /** Enable serial output
     *
     * If both serial input and serial output are disabled, the
     * peripheral is freed. If either serial input or serial
     * output is re-enabled, the peripheral is reinitialized.
     *
     * On re-initialization tx interrupts will be enabled if a
     * tx handler is attached. The tx handler is called once
     * during re-initialization.
     */
    void enable_output(bool enable = true);

#if !defined(DOXYGEN_ONLY)
protected:

    /** Acquire exclusive access to this serial port
     */
    virtual void lock(void);

    /** Release exclusive access to this serial port
     */
    virtual void unlock(void);
#endif
public:

#if DEVICE_SERIAL_FC
    /** Set the flow control type on the serial port
     *
     *  @param type the flow control type (Disabled, RTS, CTS, RTSCTS)
     *  @param flow1 the first flow control pin (RTS for RTS or RTSCTS, CTS for CTS)
     *  @param flow2 the second flow control pin (CTS for RTSCTS)
     */
    void set_flow_control(Flow type, PinName flow1 = NC, PinName flow2 = NC);

    /** Set the flow control type on the serial port
     *
     *  @param type the flow control type (Disabled, RTS, CTS, RTSCTS)
     *  @param pinmap reference to structure which holds static pinmap
     */
    void set_flow_control(Flow type, const serial_fc_pinmap_t &static_pinmap);
#endif

    static void _irq_handler(uint32_t id, SerialIrq irq_type);

#if DEVICE_SERIAL_ASYNCH

    /** Begin asynchronous write using 8bit buffer.
     *
     *  The write operation ends with any of the enabled events and invokes
     *  registered callback function (which can be empty to not receive callback at all).
     *  Events that are not enabled by event argument are simply ignored.
     *  Operation has to be ended explicitly by calling abort_write() when
     *  no events are enabled.
     *  This function locks the deep sleep until any event has occurred.
     *
     *  @param buffer   The buffer where received data will be stored
     *  @param length   The buffer length in bytes
     *  @param callback The event callback function
     *  @param event    The logical OR of TX events that should end operation
     *  @return Zero if new transaction was started, -1 if transaction is already on-going
     */
    int write(const uint8_t *buffer, int length, const event_callback_t &callback, int event = SERIAL_EVENT_TX_COMPLETE);

    /** Begin asynchronous write using 16bit buffer.
     *
     *  The write operation ends with any of the enabled events and invokes
     *  registered callback function (which can be empty to not receive callback at all).
     *  Events that are not enabled by event argument are simply ignored.
     *  Operation has to be ended explicitly by calling abort_write() when
     *  no events are enabled.
     *  This function locks the deep sleep until any event has occurred.
     *
     *  @param buffer   The buffer where received data will be stored
     *  @param length   The buffer length in bytes
     *  @param callback The event callback function
     *  @param event    The logical OR of TX events that should end operation
     *  @return Zero if new transaction was started, -1 if transaction is already on-going
     */
    int write(const uint16_t *buffer, int length, const event_callback_t &callback, int event = SERIAL_EVENT_TX_COMPLETE);

    /** Abort the on-going write transfer
     *
     *  It is safe to call abort_write() when there is no on-going transaction.
     */
    void abort_write();

    /** Begin asynchronous reading using 8bit buffer.
     *
     *  The read operation ends with any of the enabled events and invokes registered
     *  callback function (which can be empty to not receive callback at all).
     *  Events that are not enabled by event argument are simply ignored.
     *  Operation has to be ended explicitly by calling abort_read() when
     *  no events are enabled.
     *  This function locks the deep sleep until any event has occurred.
     *
     *  @param buffer     The buffer where received data will be stored
     *  @param length     The buffer length in bytes
     *  @param callback   The event callback function
     *  @param event      The logical OR of RX events that should end operation
     *  @param char_match The matching character
     *  @return Zero if new transaction was started, -1 if transaction is already on-going
     */
    int read(uint8_t *buffer, int length, const event_callback_t &callback, int event = SERIAL_EVENT_RX_COMPLETE, unsigned char char_match = SERIAL_RESERVED_CHAR_MATCH);

    /** Begin asynchronous reading using 16bit buffer.
     *
     *  The read operation ends with any of the enabled events and invokes registered
     *  callback function (which can be empty to not receive callback at all).
     *  Events that are not enabled by event argument are simply ignored.
     *  Operation has to be ended explicitly by calling abort_read() when
     *  no events are enabled.
     *  This function locks the deep sleep until any event has occurred.
     *
     *  @param buffer     The buffer where received data will be stored
     *  @param length     The buffer length in bytes
     *  @param callback   The event callback function
     *  @param event      The logical OR of RX events that should end operation
     *  @param char_match The matching character
     *  @return Zero if new transaction was started, -1 if transaction is already on-going
     */
    int read(uint16_t *buffer, int length, const event_callback_t &callback, int event = SERIAL_EVENT_RX_COMPLETE, unsigned char char_match = SERIAL_RESERVED_CHAR_MATCH);

    /** Abort the on-going read transfer
     *
     *  It is safe to call abort_read() when there is no on-going transaction.
     */
    void abort_read();

    /** Configure DMA usage suggestion for non-blocking TX transfers
     *
     *  @param usage The usage DMA hint for peripheral
     *  @return Zero if the usage was set, -1 if a transaction is on-going
     */
    int set_dma_usage_tx(DMAUsage usage);

    /** Configure DMA usage suggestion for non-blocking RX transfers
     *
     *  @param usage The usage DMA hint for peripheral
     *  @return Zero if the usage was set, -1 if a transaction is on-going
     */
    int set_dma_usage_rx(DMAUsage usage);

#if !defined(DOXYGEN_ONLY)
protected:
    void start_read(void *buffer, int buffer_size, char buffer_width, const event_callback_t &callback, int event, unsigned char char_match);
    void start_write(const void *buffer, int buffer_size, char buffer_width, const event_callback_t &callback, int event);
    void interrupt_handler_asynch(void);
#endif
#endif

#if !defined(DOXYGEN_ONLY)
protected:
    SerialBase(PinName tx, PinName rx, int baud);
    SerialBase(const serial_pinmap_t &static_pinmap, int baud);
    virtual ~SerialBase();

    int _base_getc();

    int _base_putc(int c);

    /** Initialize serial port
     */
    void _init();
    void _init_direct();
    /* Pointer to serial init function */
    void (SerialBase::*_init_func)();

    /** Deinitialize serial port
     */
    void _deinit();

#if DEVICE_SERIAL_ASYNCH
    CThunk<SerialBase> _thunk_irq;
    DMAUsage _tx_usage = DMA_USAGE_NEVER;
    DMAUsage _rx_usage = DMA_USAGE_NEVER;
    event_callback_t _tx_callback;
    event_callback_t _rx_callback;
    bool _tx_asynch_set = false;
    bool _rx_asynch_set = false;
#endif

    serial_t              _serial {};
    Callback<void()>      _irq[IrqCnt];
    int                   _baud;
    bool                  _rx_enabled = true;
    bool                  _tx_enabled = true;
    const PinName         _tx_pin;
    const PinName         _rx_pin;
    const serial_pinmap_t *_static_pinmap = NULL;
    void (SerialBase::*_set_flow_control_dp_func)(Flow, PinName, PinName) = NULL;

#if DEVICE_SERIAL_FC
    Flow                           _flow_type = Disabled;
    PinName                        _flow1 = NC;
    PinName                        _flow2 = NC;
    const serial_fc_pinmap_t       *_static_pinmap_fc = NULL;
    void (SerialBase::*_set_flow_control_sp_func)(Flow, const serial_fc_pinmap_t &) = NULL;
#endif

#endif
};

/** @}*/

} // namespace mbed

#endif

#endif
