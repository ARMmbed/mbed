/* mbed Microcontroller Library
 * Copyright (c) 2017-2018 ARM Limited
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

#ifndef NRF_CORDIO_HCI_DRIVER_H_
#define NRF_CORDIO_HCI_DRIVER_H_

#include "ble/driver/CordioHCIDriver.h"
#include "hal/ticker_api.h"
namespace ble {

/**
 * Nordic port of CordioHCIDriver
 */
class NRFCordioHCIDriver : public CordioHCIDriver {
public:
    /**
     * Construct a new instance of an HCI driver.
     * @param transport_driver The driver used to communicate with the chip.
     */
    NRFCordioHCIDriver(CordioHCITransportDriver& transport_driver);

    /**
     * Driver destructor
     */
    virtual ~NRFCordioHCIDriver();

    /**
     * Return the set of memory pool which will be used by the Cordio stack
     */
    virtual buf_pool_desc_t get_buffer_pool_description();

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
    //virtual void handle_reset_sequence(uint8_t *msg);

    /**
     * @copydoc CordioHCIDriver::get_random_static_address
     */
    virtual bool get_random_static_address(ble::address_t& address);

    /**
     * @copydoc copydoc CordioHciDriver::on_deep_sleep
     */
    virtual void on_deep_sleep(timestamp_t wakeTimeMs);
private:
    /**
     * Initialize the chip.
     * The transport is up at that time.
     */
    virtual void do_initialize();

    /**
     * Terminate the driver
     */
    virtual void do_terminate();

    // Cordio parameters
    static const BbRtCfg_t _bb_cfg; /// << Baseband runtime configuration
    static const LlRtCfg_t _ll_cfg; /// << Link layer runtime configuration;

    bool _is_init;
    uint8_t* _stack_buffer;
};

} // namespace ble

#endif /* NRF_CORDIO_HCI_DRIVER_H_ */
