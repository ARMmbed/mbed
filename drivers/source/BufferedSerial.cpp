/* mbed Microcontroller Library
 * Copyright (c) 2006-2019 ARM Limited
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
#include "drivers/BufferedSerial.h"

#if (DEVICE_SERIAL && DEVICE_INTERRUPTIN)

#include "platform/mbed_poll.h"
#include "platform/mbed_thread.h"

namespace mbed {

BufferedSerial::BufferedSerial(PinName tx, PinName rx, int baud):
    SerialBase(tx, rx, baud)
{
    update_rx_irq();
}

BufferedSerial::BufferedSerial(const serial_pinmap_t &static_pinmap, int baud):
    SerialBase(static_pinmap, baud)
{
    update_rx_irq();
}

BufferedSerial::~BufferedSerial()
{
    delete _dcd_irq;
}

void BufferedSerial::dcd_irq()
{
    wake();
}

void BufferedSerial::set_baud(int baud)
{
    SerialBase::baud(baud);
}

void BufferedSerial::set_data_carrier_detect(PinName dcd_pin, bool active_high)
{
    delete _dcd_irq;
    _dcd_irq = NULL;

    if (dcd_pin != NC) {
        _dcd_irq = new InterruptIn(dcd_pin);
        if (active_high) {
            _dcd_irq->fall(callback(this, &BufferedSerial::dcd_irq));
        } else {
            _dcd_irq->rise(callback(this, &BufferedSerial::dcd_irq));
        }
    }
}

void BufferedSerial::set_format(int bits, Parity parity, int stop_bits)
{
    api_lock();
    SerialBase::format(bits, parity, stop_bits);
    api_unlock();
}

#if DEVICE_SERIAL_FC
void BufferedSerial::set_flow_control(Flow type, PinName flow1, PinName flow2)
{
    api_lock();
    SerialBase::set_flow_control(type, flow1, flow2);
    api_unlock();
}
#endif

int BufferedSerial::close()
{
    // Does not let us pass a file descriptor. So how to close ?
    // Also, does it make sense to close a device type file descriptor?
    return 0;
}

int BufferedSerial::isatty()
{
    return 1;
}

off_t BufferedSerial::seek(off_t offset, int whence)
{
    // lseek can be done theoratically, but is it sane to mark positions on
    // a dynamically growing/shrinking buffer system (from an interrupt
    // context)
    return -ESPIPE;
}

int BufferedSerial::sync()
{
    api_lock();

    while (!_txbuf.empty()) {
        api_unlock();
        // Doing better than wait would require TxIRQ to also do wake() when
        // becoming empty. Worth it?
        thread_sleep_for(1);
        api_lock();
    }

    api_unlock();

    return SerialBase::sync();
}

void BufferedSerial::sigio(Callback<void()> func)
{
    core_util_critical_section_enter();
    _sigio_cb = func;
    if (_sigio_cb) {
        short current_events = poll(0x7FFF);
        if (current_events) {
            _sigio_cb();
        }
    }
    core_util_critical_section_exit();
}

/* Special synchronous write designed to work from critical section, such
 * as in mbed_error_vprintf.
 */
ssize_t BufferedSerial::write_unbuffered(const char *buf_ptr, size_t length)
{
    while (!_txbuf.empty()) {
        tx_irq();
    }

    for (size_t data_written = 0; data_written < length; data_written++) {
        SerialBase::_base_putc(*buf_ptr++);
    }

    return length;
}

ssize_t BufferedSerial::write(const void *buffer, size_t length)
{
    size_t data_written = 0;
    const char *buf_ptr = static_cast<const char *>(buffer);

    if (length == 0) {
        return 0;
    }

    if (core_util_in_critical_section()) {
        return write_unbuffered(buf_ptr, length);
    }

    api_lock();

    // Unlike read, we should write the whole thing if blocking. POSIX only
    // allows partial as a side-effect of signal handling; it normally tries to
    // write everything if blocking. Without signals we can always write all.
    while (data_written < length) {

        if (_txbuf.full()) {
            if (!_blocking) {
                break;
            }
            do {
                api_unlock();
                // Should we have a proper wait?
                thread_sleep_for(1);
                api_lock();
            } while (_txbuf.full());
        }

        while (data_written < length && !_txbuf.full()) {
            _txbuf.push(*buf_ptr++);
            data_written++;
        }

        update_tx_irq();
    }

    api_unlock();

    return data_written != 0 ? (ssize_t) data_written : (ssize_t) - EAGAIN;
}

ssize_t BufferedSerial::read(void *buffer, size_t length)
{
    size_t data_read = 0;

    char *ptr = static_cast<char *>(buffer);

    if (length == 0) {
        return 0;
    }

    api_lock();

    while (_rxbuf.empty()) {
        if (!_blocking) {
            api_unlock();
            return -EAGAIN;
        }
        api_unlock();
        // Do we need a proper wait?
        thread_sleep_for(1);
        api_lock();
    }

    while (data_read < length && !_rxbuf.empty()) {
        _rxbuf.pop(*ptr++);
        data_read++;
    }

    update_rx_irq();

    api_unlock();

    return data_read;
}

bool BufferedSerial::hup() const
{
    return _dcd_irq && _dcd_irq->read() != 0;
}

void BufferedSerial::wake()
{
    if (_sigio_cb) {
        _sigio_cb();
    }
}

short BufferedSerial::poll(short events) const
{

    short revents = 0;
    // Check the Circular Buffer if space available for writing out


    if (!_rxbuf.empty()) {
        revents |= POLLIN;
    }

    // POLLHUP and POLLOUT are mutually exclusive
    if (hup()) {
        revents |= POLLHUP;
    } else if (!_txbuf.full()) {
        revents |= POLLOUT;
    }

    return revents;
}

void BufferedSerial::api_lock(void)
{
    _mutex.lock();
}

void BufferedSerial::api_unlock(void)
{
    _mutex.unlock();
}

void BufferedSerial::rx_irq(void)
{
    bool was_empty = _rxbuf.empty();

    // Fill in the receive buffer if the peripheral is readable
    // and receive buffer is not full.
    while (!_rxbuf.full() && SerialBase::readable()) {
        char data = SerialBase::_base_getc();
        _rxbuf.push(data);
    }

    if (_rx_irq_enabled && _rxbuf.full()) {
        disable_rx_irq();
    }

    // Report the File handler that data is ready to be read from the buffer.
    if (was_empty && !_rxbuf.empty()) {
        wake();
    }
}

// Also called from write to start transfer
void BufferedSerial::tx_irq(void)
{
    bool was_full = _txbuf.full();
    char data;

    // Write to the peripheral if there is something to write
    // and if the peripheral is available to write.
    while (SerialBase::writeable() && _txbuf.pop(data)) {
        SerialBase::_base_putc(data);
    }

    if (_tx_irq_enabled && _txbuf.empty()) {
        disable_tx_irq();
    }

    // Report the File handler that data can be written to peripheral.
    if (was_full && !_txbuf.full() && !hup()) {
        wake();
    }
}

/* Attach Rx-IRQ routine to the serial device eventually.
 */
void BufferedSerial::update_rx_irq()
{
    core_util_critical_section_enter();
    if (_rx_enabled && !_rx_irq_enabled) {
        BufferedSerial::rx_irq();
        if (!_rxbuf.full()) {
            SerialBase::attach(callback(this, &BufferedSerial::rx_irq), RxIrq);
            _rx_irq_enabled = true;
        }
    }
    core_util_critical_section_exit();
}

/* This is called called from critical section or interrupt context */
void BufferedSerial::disable_rx_irq()
{
    SerialBase::attach(NULL, RxIrq);
    _rx_irq_enabled = false;
}

/* Attach Tx-IRQ routine to the serial device eventually.
 */
void BufferedSerial::update_tx_irq()
{
    core_util_critical_section_enter();
    if (_tx_enabled && !_tx_irq_enabled) {
        BufferedSerial::tx_irq();
        if (!_txbuf.empty()) {
            SerialBase::attach(callback(this, &BufferedSerial::tx_irq), TxIrq);
            _tx_irq_enabled = true;
        }
    }
    core_util_critical_section_exit();
}

/* This is called called from critical section or interrupt context */
void BufferedSerial::disable_tx_irq()
{
    SerialBase::attach(NULL, TxIrq);
    _tx_irq_enabled = false;
}

int BufferedSerial::enable_input(bool enabled)
{
    api_lock();
    SerialBase::enable_input(enabled);
    update_rx_irq(); // Eventually enable rx-interrupt to handle incoming data
    api_unlock();

    return 0;
}

int BufferedSerial::enable_output(bool enabled)
{
    api_lock();
    SerialBase::enable_output(enabled);
    update_tx_irq(); // Eventually enable tx-interrupt to flush buffered data
    api_unlock();

    return 0;
}

} // namespace mbed

#endif //(DEVICE_SERIAL && DEVICE_INTERRUPTIN)
