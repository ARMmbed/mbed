/* mbed Microcontroller Library
 * Copyright (c) 2006-2020 ARM Limited
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

#ifndef IMPL_BLE_INSTANCE_BASE_H_
#define IMPL_BLE_INSTANCE_BASE_H_

#include "ble/BLE.h"

#include "ble/GattServer.h"
#include "ble/GattClient.h"
#include "ble/Gap.h"
#include "ble/SecurityManager.h"

#include "ble/common/blecommon.h"
#include "ble/driver/CordioHCIDriver.h"

#include "source/BLEInstanceBase.h"
#include "source/pal/PalAttClient.h"
#include "source/pal/PalGattClient.h"
#include "source/pal/PalGap.h"
#include "source/pal/PalGenericAccessService.h"
#include "source/pal/PalEventQueue.h"
#include "source/pal/PalSecurityManager.h"

#include "source/generic/GapImpl.h"
#include "source/generic/GattClientImpl.h"
#include "source/generic/SecurityManagerImpl.h"
#include "source/GattServerImpl.h"
#include "source/PalEventQueueImpl.h"
#include "source/PalPrivateAddressControllerImpl.h"
#include "source/generic/PrivateAddressController.h"

#include "drivers/Timeout.h"
#ifdef DEVICE_LPTICKER
#include "drivers/LowPowerTimeout.h"
#include "drivers/LowPowerTimer.h"
#else
#include "drivers/Timer.h"
#endif

namespace ble {
#if BLE_FEATURE_SIGNING
class PalSigningMonitor;
#endif //BLE_FEATURE_SIGNING

namespace impl {

/**
 * @see BLEInstanceBase
 */
class BLEInstanceBase final : public ble::BLEInstanceBase {
#ifdef DEVICE_LPTICKER
    using Timeout = mbed::LowPowerTimeout;
    using Timer   = mbed::LowPowerTimer;
#else
    using Timeout = mbed::Timeout;
    using Timer   = mbed::Timer;
#endif
#if BLE_FEATURE_SIGNING
    friend PalSigningMonitor;
#endif //BLE_FEATURE_SIGNING

    /**
     * Construction with an HCI driver.
     */
    BLEInstanceBase(CordioHCIDriver &hci_driver);

    /**
     * Destructor
     */
    virtual ~BLEInstanceBase();

public:

    /**
     * Access to the singleton containing the implementation of BLEInstanceBase
     * for the Cordio stack.
     */
    static BLEInstanceBase &deviceInstance();

    /**
    * @see BLEInstanceBase::init
    */
    ble_error_t init(
        FunctionPointerWithContext<::BLE::InitializationCompleteCallbackContext *> initCallback
    ) final;

    /**
     * @see BLEInstanceBase::hasInitialized
     */
    bool hasInitialized() const final;

    /**
     * @see BLEInstanceBase::shutdown
     */
    ble_error_t shutdown() final;

    /**
     * @see BLEInstanceBase::getVersion
     */
    const char *getVersion() final;

    ble::impl::Gap &getGapImpl();

    /**
     * @see BLEInstanceBase::getGap
     */
    ble::Gap &getGap() final;

    /**
     * @see BLEInstanceBase::getGap
     */
    const ble::Gap &getGap() const final;

#if BLE_FEATURE_GATT_SERVER

    ble::impl::GattServer &getGattServerImpl();

    /**
     * @see BLEInstanceBase::getGattServer
     */
    ble::GattServer &getGattServer() final;

    /**
     * @see BLEInstanceBase::getGattServer
     */
    const ble::GattServer &getGattServer() const final;

#endif // BLE_FEATURE_GATT_SERVER

#if BLE_FEATURE_GATT_CLIENT

    ble::impl::GattClient &getGattClientImpl();

    /**
     * @see BLEInstanceBase::getGattClient
     */
    ble::GattClient &getGattClient() final;

    /**
     * Get the PAL Gatt Client.
     *
     * @return PAL Gatt Client.
     */
    PalGattClient &getPalGattClient();

#endif // BLE_FEATURE_GATT_CLIENT

#if BLE_FEATURE_SECURITY

    ble::impl::SecurityManager &getSecurityManagerImpl();

    /**
     * @see BLEInstanceBase::getSecurityManager
     */
    ble::SecurityManager &getSecurityManager() final;

    /**
     * @see BLEInstanceBase::getSecurityManager
     */
    const ble::SecurityManager &getSecurityManager() const final;

#if BLE_FEATURE_PRIVACY
    ble::PrivateAddressController &getPrivateAddressRegistry();
#endif

#endif // BLE_FEATURE_SECURITY

    /**
     * @see BLEInstanceBase::waitForEvent
     */
    void waitForEvent();

    /**
     * @see BLEInstanceBase::processEvents
     */
    void processEvents() final;

private:
    static void stack_handler(wsfEventMask_t event, wsfMsgHdr_t *msg);

    static void device_manager_cb(dmEvt_t *dm_event);

    static void connection_handler(dmEvt_t *dm_event);

    static void timeoutCallback();

    void stack_setup();

    void start_stack_reset();

    void callDispatcher();

    static CordioHCIDriver *_hci_driver;
    static FunctionPointerWithContext<::BLE::InitializationCompleteCallbackContext *> _init_callback;

    enum {
        NOT_INITIALIZED,
        INITIALIZING,
        INITIALIZED
    } initialization_status;

    mutable ble::impl::PalEventQueue _event_queue;
    Timer _timer;
    uint64_t _last_update_us;
};

} // namespace impl
} // namespace ble


#endif /* IMPL_BLE_INSTANCE_BASE_H_ */
