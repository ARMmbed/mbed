/* mbed Microcontroller Library
 * Copyright (c) 2017-2017 ARM Limited
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

#ifndef IMPL_HCI_DRIVER_H_
#define IMPL_HCI_DRIVER_H_

#include <cstddef>
#include <cstdint>
#include "platform/Callback.h"
#include "ble/common/BLETypes.h"
#include "ble/driver/CordioHCITransportDriver.h"
#include "ble/common/blecommon.h"
#include "hal/ticker_api.h"

// FIXME: make this invisible!
#include "wsf_buf.h"

namespace ble {

/**
 * Contain description of the memory pool used by the Cordio stack.
 */
struct buf_pool_desc_t {
    /**
     * Create a new memory pool description
     * @param buffer the Buffer used by the memory pool.
     * @param pool_desc How the memory is split
     */
    template<size_t BufferSize, size_t PoolCount>
    buf_pool_desc_t(
        uint8_t (&buffer)[BufferSize],
        const wsfBufPoolDesc_t (&pool_desc)[PoolCount]
    ) : buffer_memory(buffer), buffer_size(BufferSize),
        pool_description(pool_desc), pool_count(PoolCount)
    {
    }

    uint8_t* buffer_memory;         /// Pointer to the buffer memory
    size_t buffer_size;             /// Size of the buffer
    const wsfBufPoolDesc_t* pool_description;   /// Pointer to the first element describing the pool
    size_t pool_count;      /// Number of pools
};

/**
 * Base class of the HCI driver use by the BLE port of the Cordio stack.
 * This class provide to the stack:
 *   - The buffer necessary to run BLE API
 *   - The reset sequence of the BLE module
 *   - Access to the write function of the underlying HCITransport driver.
 */
class CordioHCIDriver {

    // hook for internal tests and passthrough driver
    friend class CordioHCIHook;

public:
    /**
     * Construct a new instance of an HCI driver.
     * @param transport_driver The driver used to communicate with the chip.
     */
    CordioHCIDriver(CordioHCITransportDriver& transport_driver);

    /**
     * Driver destructor
     */
    virtual ~CordioHCIDriver() = default;

    /**
     * Return the set of memory pool which will be used by the Cordio stack
     */
    virtual buf_pool_desc_t get_buffer_pool_description() = 0;

    /**
     * Initialize the HCI driver.
     * This function start by initializing the transport driver then it delegates
     * what's remain of the initialization to the function do_initialize.
     */
    void initialize();

    /**
     * Termination of the driver.
     * It call in sequence:
     *   - do_terminate
     *   - terminate the transport driver.
     */
    void terminate();

    /**
     * Start the reset sequence of the BLE module.
     */
    virtual void start_reset_sequence();

    /**
     * Handle HCI messages received during the reset sequence.
     *
     * @param msg The HCI message received.
     * @note The driver should signal to the stack that the initialization
     * sequence is done by calling the function: signal_reset_sequence_done.
     */
    virtual void handle_reset_sequence(uint8_t *msg);

    /**
     * Get the random static address of the controller
     *
     * @return false if the address has not been set and true otherwise.
     */
    virtual bool get_random_static_address(ble::address_t& address);

    /**
     * Signal to the stack that the reset sequence has been done.
     */
    void signal_reset_sequence_done();

    /**
     * Write data in the transport channel.
     *
     * @param type The type of packet to transmit. It might be an HCI command
     * packet, ACL packet or EVT packet. Depending on the type of transport
     * it can prefix the packet itself.
     * @param len Number of bytes to transmit.
     * @param pData pointer to the data to transmit.
     *
     * @return The number of bytes which have been transmited.
     */
    uint16_t write(uint8_t type, uint16_t len, uint8_t *pData);

    /**
     * React to host stack inactivity.
     *
     * The host stack invoke this function when it is inactive. It allows a
     * driver to put its controller to sleep if all the conditions are met.
     *
     * Any call to write signals to the driver that the host stack is active.
     */
    virtual void on_host_stack_inactivity();

    /** 
     * React to host stack preparing to enter deep sleep.
     * 
     * \param[in] wakeTimeMs The amount of time in which to wake.
     */
    virtual void on_deep_sleep(timestamp_t wakeTimeMs) {}

     /* BLE Tester commands */

     /**
      * This will be called by host part of the stack to indicate the end of the test.
      *
      * @param success True if the TEST END command was a success.
      * @param packets Number of packets received during the test.
      * @return BLE_ERROR_NONE on success.
      */
     void handle_test_end(bool success, uint16_t packets);

     /** Callback to inform the caller of the result of the test, the parameters are success and the
      *  number of packets received.
      */
     typedef mbed::Callback<void(bool, uint16_t)> test_end_handler_t;

     /**
      * Start BLE receiver test. Call rf_test_end when you want to stop.
      * @param test_end_handler Handler that will be called with the number of packets received.
      * @param channel Channel to use.
      * @return BLE_ERROR_NONE on success.
      */
     ble_error_t rf_test_start_le_receiver_test(test_end_handler_t test_end_handler, uint8_t channel);

     /**
      * Start BLE transmitter test. Call rf_test_end when you want to stop.
      * @param test_end_handler Handler that will be called with status and the number of packets set to 0.
      * @param channel Channel to use.
      * @param length Size of payload.
      * @param type Type of pattern to transmit @see BLE spec Volume 6 Part F, Section 4.1.5.
      * @return BLE_ERROR_NONE on success.
      */
     ble_error_t rf_test_start_le_transmitter_test(test_end_handler_t test_end_handler, uint8_t channel,
                                                           uint8_t length, uint8_t type);

     /**
      * Complete the test. This will trigger the end test event which will call handle_test_end
      * @return BLE_ERROR_NONE on success.
      */
     ble_error_t rf_test_end();

     /**
      * Set desired transmit power. Value equal or bigger will be used from available levels.
      * Consult chip documentation for available values. Actual TX power is not guaranteed
      * and is down to the implementation.
      *
      * @param level_db Signal level in dBm.
      * @return BLE_ERROR_NONE on success.
      */
     virtual ble_error_t set_tx_power(int8_t level_db);

protected:
    /**
     * Return a default set of memory pool that the Cordio stack can use.
     * This function can be used to implement get_buffer_pool_description().
     */
    buf_pool_desc_t get_default_buffer_pool_description();

    /**
     * Allows the driver to set a random static address. Unlike the HCI command
     * this function reports the random static address to the whole BLE system.
     * @param random_static_address The random static address to set.
     */
    void set_random_static_address(const ble::address_t& random_static_address);

private:
    /**
     * Initialize the chip.
     * The transport is up at that time.
     */
    virtual void do_initialize() = 0;

    /**
     * Terminate the driver
     */
    virtual void do_terminate() = 0;

protected:
    test_end_handler_t _test_end_handler;
private:
    CordioHCITransportDriver& _transport_driver;
};

} // namespace ble

#endif /* IMPL_HCI_DRIVER_H_ */
