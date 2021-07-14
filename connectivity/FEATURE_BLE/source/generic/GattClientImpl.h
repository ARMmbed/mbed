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

#ifndef IMPL_GATT_CLIENT_H__
#define IMPL_GATT_CLIENT_H__

#include "ble/BLE.h"

#include "ble/common/CallChainOfFunctionPointersWithContext.h"
#include <algorithm>

#include "ble/common/blecommon.h"
#include "ble/gatt/GattAttribute.h"
#include "ble/gatt/ServiceDiscovery.h"
#include "ble/gatt/CharacteristicDescriptorDiscovery.h"
#include "ble/gatt/GattCallbackParamTypes.h"
#include "source/pal/PalGattClient.h"
#if BLE_FEATURE_SIGNING
#include "source/pal/PalSigningMonitor.h"
#endif //BLE_FEATURE_SIGNING
#include "ble/GattClient.h"

namespace ble {

class BLEInstanceBase;

namespace impl {

class GattClient :
#if BLE_FEATURE_SIGNING
    public PalSigningMonitor,
#endif //BLE_FEATURE_SIGNING
    public PalGattClientEventHandler {
#if BLE_FEATURE_SIGNING
    friend PalSigningMonitor;
#endif //BLE_FEATURE_SIGNING
    friend BLEInstanceBase;
public:
    using EventHandler = ble::GattClient::EventHandler;
    using WriteOp_t = ble::GattClient::WriteOp_t;
    using HVXCallback_t  = ble::GattClient::HVXCallback_t ;
    using GattClientShutdownCallback_t  = ble::GattClient::GattClientShutdownCallback_t ;
    using GattClientShutdownCallbackChain_t  = ble::GattClient::GattClientShutdownCallbackChain_t ;
    using HVXCallbackChain_t  = ble::GattClient::HVXCallbackChain_t ;
    using ReadCallbackChain_t  = ble::GattClient::ReadCallbackChain_t ;
    using WriteCallbackChain_t  = ble::GattClient::WriteCallbackChain_t ;



    void setEventHandler(EventHandler *handler);

    ble_error_t launchServiceDiscovery(
        ble::connection_handle_t connectionHandle,
        ServiceDiscovery::ServiceCallback_t sc = nullptr,
        ServiceDiscovery::CharacteristicCallback_t cc = nullptr,
        const UUID &matchingServiceUUID = UUID::ShortUUIDBytes_t(BLE_UUID_UNKNOWN),
        const UUID &matchingCharacteristicUUIDIn = UUID::ShortUUIDBytes_t(BLE_UUID_UNKNOWN)
    );

    ble_error_t discoverServices(
        ble::connection_handle_t connectionHandle,
        ServiceDiscovery::ServiceCallback_t callback,
        const UUID &matchingServiceUUID = UUID::ShortUUIDBytes_t(BLE_UUID_UNKNOWN)
    );

    ble_error_t discoverServices(
        ble::connection_handle_t connectionHandle,
        ServiceDiscovery::ServiceCallback_t callback,
        GattAttribute::Handle_t startHandle,
        GattAttribute::Handle_t endHandle
    );

    bool isServiceDiscoveryActive(void) const;

    void terminateServiceDiscovery(void);

    ble_error_t read(
        ble::connection_handle_t connHandle,
        GattAttribute::Handle_t attributeHandle,
        uint16_t offset
    ) const;

    ble_error_t write(
        GattClient::WriteOp_t cmd,
        ble::connection_handle_t connHandle,
        GattAttribute::Handle_t attributeHandle,
        size_t length,
        const uint8_t *value
    ) const;

    /* Event callback handlers. */

    void onDataRead(ReadCallback_t callback);

    ReadCallbackChain_t &onDataRead();

    void onDataWritten(WriteCallback_t callback);

    WriteCallbackChain_t &onDataWritten();

    void onServiceDiscoveryTermination(
        ServiceDiscovery::TerminationCallback_t callback
    );

    ble_error_t discoverCharacteristicDescriptors(
        const DiscoveredCharacteristic &characteristic,
        const CharacteristicDescriptorDiscovery::DiscoveryCallback_t &discoveryCallback,
        const CharacteristicDescriptorDiscovery::TerminationCallback_t &terminationCallback
    );

    bool isCharacteristicDescriptorDiscoveryActive(
        const DiscoveredCharacteristic &characteristic
    ) const;

    void terminateCharacteristicDescriptorDiscovery(
        const DiscoveredCharacteristic &characteristic
    );

    ble_error_t negotiateAttMtu(ble::connection_handle_t connection);

    /**
     * Register an handler for Handle Value Notification/Indication events.
     *
     * @param callback Event handler to register.
     *
     * @note It is possible to unregister a callback by using
     * onHVX().detach(callbackToRemove).
     */
    void onHVX(HVXCallback_t callback);

    void onShutdown(const GattClientShutdownCallback_t &callback);

    GattClientShutdownCallbackChain_t &onShutdown();

    HVXCallbackChain_t &onHVX();

    ble_error_t reset(void);

    /* Entry points for the underlying stack to report events back to the user. */

    void processReadResponse(const GattReadCallbackParams *params);

    void processWriteResponse(const GattWriteCallbackParams *params);

    void processHVXEvent(const GattHVXCallbackParams *params);

private:
    /* Disallow copy and assignment. */
    GattClient(const GattClient &);

    GattClient &operator=(const GattClient &);

    /* ===================================================================== */
    /*                    private implementation follows                     */

private:
#if BLE_FEATURE_SIGNING
    /**
     * @see ble::PalSigningMonitor::set_signing_event_handler
     */
    void set_signing_event_handler(PalSigningMonitorEventHandler *signing_event_handler) override;
#endif //BLE_FEATURE_SIGNING
    /**
     * @see PalGattClient::EventHandler::on_att_mtu_change
     */
    void on_att_mtu_change(
        ble::connection_handle_t connection_handle,
        uint16_t att_mtu_size
    ) override;

    /**
     * @see PalGattClient::EventHandler::on_write_command_sent
     */
    void on_write_command_sent(
        ble::connection_handle_t connection_handle,
        ble::attribute_handle_t attribute_handle,
        uint8_t status
    ) override;

private:
    struct ProcedureControlBlock;
    struct DiscoveryControlBlock;
    struct ReadControlBlock;
    struct WriteControlBlock;
    struct DescriptorDiscoveryControlBlock;

    ProcedureControlBlock *get_control_block(connection_handle_t connection);

    const ProcedureControlBlock *get_control_block(connection_handle_t connection) const;

    void insert_control_block(ProcedureControlBlock *cb) const;

    void remove_control_block(ProcedureControlBlock *cb) const;

    void on_termination(connection_handle_t connection_handle);

    void on_server_message_received(connection_handle_t, const AttServerMessage &);

    void on_server_response(connection_handle_t, const AttServerMessage &);

    void on_server_event(connection_handle_t, const AttServerMessage &);

    void on_transaction_timeout(connection_handle_t);

    uint16_t get_mtu(connection_handle_t connection) const;

private:
    /**
     * Event handler provided by the application.
     */
    EventHandler *eventHandler;

    /**
     * Callchain containing all registered event handlers for data read
     * events.
     */
    ReadCallbackChain_t onDataReadCallbackChain;

    /**
     * Callchain containing all registered event handlers for data write
     * events.
     */
    WriteCallbackChain_t onDataWriteCallbackChain;

    /**
     * Callchain containing all registered event handlers for update
     * events.
     */
    HVXCallbackChain_t onHVXCallbackChain;

    /**
     * Callchain containing all registered event handlers for shutdown
     * events.
     */
    GattClientShutdownCallbackChain_t shutdownCallChain;

    PalGattClient &_pal_client;
    ServiceDiscovery::TerminationCallback_t _termination_callback;
#if BLE_FEATURE_SIGNING
    PalSigningMonitorEventHandler *_signing_event_handler;
#endif // BLE_FEATURE_SIGNING
    mutable ProcedureControlBlock *control_blocks;
    bool _is_reseting;

private:
    /**
     * Create a PalGattClient from a PalGattClient
     */
    GattClient(PalGattClient &pal_client);

    ~GattClient() = default;
};

} // namespace impl
} // namespace ble

#endif /* ifndef IMPL_GATT_CLIENT_H__ */
