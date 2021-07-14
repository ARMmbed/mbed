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
#ifndef MBED_I2C_H
#define MBED_I2C_H

#include "platform/platform.h"
#include "hal/gpio_api.h"

#if DEVICE_I2C || defined(DOXYGEN_ONLY)

#include "hal/i2c_api.h"
#include "platform/SingletonPtr.h"
#include "platform/PlatformMutex.h"
#include "platform/NonCopyable.h"

#if DEVICE_I2C_ASYNCH
#include "platform/CThunk.h"
#include "hal/dma_api.h"
#include "platform/Callback.h"
#endif

namespace mbed {
/** \defgroup drivers-public-api-i2c I2C
 * \ingroup drivers-public-api
 */

/**
 * \defgroup drivers_I2C I2C class
 * \ingroup drivers-public-api-i2c
 * @{
 */

/** An I2C Master, used for communicating with I2C slave devices
 *
 * @note Synchronization level: Thread safe
 *
 * Example:
 * @code
 * Read temperature from LM75BD
 * #include "mbed.h"
 * I2C i2c(I2C_SDA , I2C_SCL);
 * const int addr7bit = 0x48;      // 7-bit I2C address
 * const int addr8bit = 0x48 << 1; // 8-bit I2C address, 0x90
 *
 * int main() {
 *     char cmd[2];
 *     while (1) {
 *         cmd[0] = 0x01;
 *         cmd[1] = 0x00;
 *
 *         // read and write takes the 8-bit version of the address.
 *         // set up configuration register (at 0x01)
 *         i2c.write(addr8bit, cmd, 2);
 *
 *         ThisThread::sleep_for(500);
 *
 *         // read temperature register
 *         cmd[0] = 0x00;
 *         i2c.write(addr8bit, cmd, 1);
 *         i2c.read( addr8bit, cmd, 2);
 *
 *         float tmp = (float((cmd[0]<<8)|cmd[1]) / 256.0);
 *         printf("Temp = %.2f\n", tmp);
 *   }
 * }
 * @endcode
 */
class I2C : private NonCopyable<I2C> {

public:
    enum RxStatus {
        NoData,
        MasterGeneralCall,
        MasterWrite,
        MasterRead
    };

    enum Acknowledge {
        NoACK = 0,
        ACK   = 1
    };

    /** Create an I2C Master interface, connected to the specified pins
     *
     *  @param sda I2C data line pin
     *  @param scl I2C clock line pin
     */
    I2C(PinName sda, PinName scl);

    /** Create an I2C Master interface, connected to the specified pins
     *
     *  @param static_pinmap reference to structure which holds static pinmap.
     */
    I2C(const i2c_pinmap_t &static_pinmap);
    I2C(const i2c_pinmap_t &&) = delete; // prevent passing of temporary objects

    /** Set the frequency of the I2C interface
     *
     *  @param hz The bus frequency in hertz
     */
    void frequency(int hz);

    /** Read from an I2C slave
     *
     * Performs a complete read transaction. The bottom bit of
     * the address is forced to 1 to indicate a read.
     *
     *  @param address 8-bit I2C slave address [ addr | 1 ]
     *  @param data Pointer to the byte-array to read data in to
     *  @param length Number of bytes to read
     *  @param repeated Repeated start, true - don't send stop at end
     *         default value is false.
     *
     *  @returns
     *       0 on success (ack),
     *       nonzero on failure (nack)
     */
    int read(int address, char *data, int length, bool repeated = false);

    /** Read a single byte from the I2C bus
     *
     *  @param ack indicates if the byte is to be acknowledged (1 = acknowledge)
     *
     *  @returns
     *    the byte read
     */
    int read(int ack);

    /** Write to an I2C slave
     *
     * Performs a complete write transaction. The bottom bit of
     * the address is forced to 0 to indicate a write.
     *
     *  @param address 8-bit I2C slave address [ addr | 0 ]
     *  @param data Pointer to the byte-array data to send
     *  @param length Number of bytes to send
     *  @param repeated Repeated start, true - do not send stop at end
     *         default value is false.
     *
     *  @returns
     *       0 on success (ack),
     *       nonzero on failure (nack)
     */
    int write(int address, const char *data, int length, bool repeated = false);

    /** Write single byte out on the I2C bus
     *
     *  @param data data to write out on bus
     *
     *  @returns
     *    '0' - NAK was received
     *    '1' - ACK was received,
     *    '2' - timeout
     */
    int write(int data);

    /** Creates a start condition on the I2C bus
     */
    void start(void);

    /** Creates a stop condition on the I2C bus
     */
    void stop(void);

    /** Acquire exclusive access to this I2C bus
     */
    virtual void lock(void);

    /** Release exclusive access to this I2C bus
     */
    virtual void unlock(void);

    virtual ~I2C()
    {
        // Do nothing
    }

#if DEVICE_I2C_ASYNCH

    /** Start nonblocking I2C transfer.
     *
     * This function locks the deep sleep until any event has occurred
     *
     * @param address   8/10 bit I2C slave address
     * @param tx_buffer The TX buffer with data to be transferred
     * @param tx_length The length of TX buffer in bytes
     * @param rx_buffer The RX buffer, which is used for received data
     * @param rx_length The length of RX buffer in bytes
     * @param event     The logical OR of events to modify
     * @param callback  The event callback function
     * @param repeated Repeated start, true - do not send stop at end
     *        default value is false.
     *
     * @returns Zero if the transfer has started, or -1 if I2C peripheral is busy
     */
    int transfer(int address, const char *tx_buffer, int tx_length, char *rx_buffer, int rx_length, const event_callback_t &callback, int event = I2C_EVENT_TRANSFER_COMPLETE, bool repeated = false);

    /** Abort the ongoing I2C transfer
     */
    void abort_transfer();

#if !defined(DOXYGEN_ONLY)
protected:
    /** Lock deep sleep only if it is not yet locked */
    void lock_deep_sleep();

    /** Unlock deep sleep only if it has been locked */
    void unlock_deep_sleep();

    void irq_handler_asynch(void);
    event_callback_t _callback;
    CThunk<I2C> _irq;
    DMAUsage _usage;
    bool _deep_sleep_locked;
#endif
#endif

#if !defined(DOXYGEN_ONLY)
protected:

    i2c_t _i2c;
    int    _hz;
    SingletonPtr<PlatformMutex> _mutex;
    PinName _sda;
    PinName _scl;

private:
    /** Recover I2C bus, when stuck with SDA low
     *  @note : Initialization of I2C bus is required after this API.
     *
     *  @param sda I2C data line pin
     *  @param scl I2C clock line pin
     *
     * @returns
     *    '0' - Successfully recovered
     *    'I2C_ERROR_BUS_BUSY' - In case of failure
     *
     */
    int recover(PinName sda, PinName scl);
#endif
};

/** @}*/

} // namespace mbed

#endif

#endif
