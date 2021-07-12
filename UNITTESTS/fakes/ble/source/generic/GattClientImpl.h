/* mbed Microcontroller Library
 * Copyright (c) 2020 ARM Limited
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

#ifndef BLE_GATTCLIENTSTUB_H
#define BLE_GATTCLIENTSTUB_H

#include "events/EventQueue.h"

namespace ble {
namespace impl {

class GattClient {
public:
    using EventHandler = ble::GattClient::EventHandler;
    using WriteOp_t = ble::GattClient::WriteOp_t;
    using HVXCallback_t  = ble::GattClient::HVXCallback_t ;
    using GattClientShutdownCallback_t  = ble::GattClient::GattClientShutdownCallback_t ;
    using GattClientShutdownCallbackChain_t  = ble::GattClient::GattClientShutdownCallbackChain_t ;
    using HVXCallbackChain_t  = ble::GattClient::HVXCallbackChain_t ;
    using ReadCallbackChain_t  = ble::GattClient::ReadCallbackChain_t ;
    using WriteCallbackChain_t  = ble::GattClient::WriteCallbackChain_t ;

    GattClient() {};
    GattClient(const GattClient&) = delete;
    GattClient& operator=(const GattClient&) = delete;
    virtual ~GattClient() {};

    virtual void setEventHandler(EventHandler *handler) { };

    virtual ble_error_t launchServiceDiscovery(
            ble::connection_handle_t connectionHandle,
            ServiceDiscovery::ServiceCallback_t sc = nullptr,
            ServiceDiscovery::CharacteristicCallback_t cc = nullptr,
            const UUID &matchingServiceUUID = UUID::ShortUUIDBytes_t(BLE_UUID_UNKNOWN),
            const UUID &matchingCharacteristicUUIDIn = UUID::ShortUUIDBytes_t(BLE_UUID_UNKNOWN)
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t discoverServices(
            ble::connection_handle_t connectionHandle,
            ServiceDiscovery::ServiceCallback_t callback,
            const UUID &matchingServiceUUID = UUID::ShortUUIDBytes_t(BLE_UUID_UNKNOWN)
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t discoverServices(
            ble::connection_handle_t connectionHandle,
            ServiceDiscovery::ServiceCallback_t callback,
            GattAttribute::Handle_t startHandle,
            GattAttribute::Handle_t endHandle
    ) { return BLE_ERROR_NONE; };

    virtual bool isServiceDiscoveryActive(void) const { return true; };

    virtual void terminateServiceDiscovery(void) { };

    virtual ble_error_t read(
            ble::connection_handle_t connHandle,
            GattAttribute::Handle_t attributeHandle,
            uint16_t offset
    ) const { return BLE_ERROR_NONE; };

    virtual ble_error_t write(
            GattClient::WriteOp_t cmd,
            ble::connection_handle_t connHandle,
            GattAttribute::Handle_t attributeHandle,
            size_t length,
            const uint8_t *value
    ) const { return BLE_ERROR_NONE; };

    /* Event callback handlers. */

    virtual void onDataRead(ReadCallback_t callback) { };

    virtual ReadCallbackChain_t &onDataRead() { static ReadCallbackChain_t chain; return chain; };

    virtual void onDataWritten(WriteCallback_t callback) { };

    virtual WriteCallbackChain_t &onDataWritten() { static WriteCallbackChain_t chain; return chain; };

    virtual void onServiceDiscoveryTermination(
            ServiceDiscovery::TerminationCallback_t callback
    ) { };

    virtual ble_error_t discoverCharacteristicDescriptors(
            const DiscoveredCharacteristic &characteristic,
            const CharacteristicDescriptorDiscovery::DiscoveryCallback_t &discoveryCallback,
            const CharacteristicDescriptorDiscovery::TerminationCallback_t &terminationCallback
    ) { return BLE_ERROR_NONE; };

    virtual bool isCharacteristicDescriptorDiscoveryActive(
            const DiscoveredCharacteristic &characteristic
    ) const { return true; };

    virtual void terminateCharacteristicDescriptorDiscovery(
            const DiscoveredCharacteristic &characteristic
    ) { };

    virtual ble_error_t negotiateAttMtu(ble::connection_handle_t connection) { return BLE_ERROR_NONE; };

    virtual void onHVX(HVXCallback_t callback) { };

    virtual void onShutdown(const GattClientShutdownCallback_t &callback) { };

    virtual GattClientShutdownCallbackChain_t &onShutdown() { static GattClientShutdownCallbackChain_t chain; return chain; };

    virtual HVXCallbackChain_t &onHVX() { static HVXCallbackChain_t chain; return chain; };

    virtual ble_error_t reset(void) { return BLE_ERROR_NONE; };

    virtual void processReadResponse(const GattReadCallbackParams *params) { };

    virtual void processWriteResponse(const GattWriteCallbackParams *params) { };

    virtual void processHVXEvent(const GattHVXCallbackParams *params) { };
};

}
}

#endif //BLE_GATTCLIENTSTUB_H
