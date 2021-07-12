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

#ifndef BLE_GATTCLIENTMOCK_H
#define BLE_GATTCLIENTMOCK_H

#include "gmock/gmock.h"
#include "source/generic/GattClientImpl.h"

namespace ble {

class GattClientMock : public ble::impl::GattClient {
public:
    GattClientMock() {};
    GattClientMock(const GattClientMock&) = delete;
    GattClientMock& operator=(const GattClientMock&) = delete;
    virtual ~GattClientMock() {};

    MOCK_METHOD(ble_error_t, reset, (), (override));
    MOCK_METHOD(void, setEventHandler, (EventHandler *handler), (override));
    MOCK_METHOD(ble_error_t, launchServiceDiscovery, (ble::connection_handle_t connectionHandle, ServiceDiscovery::ServiceCallback_t sc, ServiceDiscovery::CharacteristicCallback_t cc, const UUID &matchingServiceUUID, const UUID &matchingCharacteristicUUIDIn), (override));
    MOCK_METHOD(ble_error_t, discoverServices, (ble::connection_handle_t connectionHandle, ServiceDiscovery::ServiceCallback_t callback, const UUID &matchingServiceUUID), (override));
    MOCK_METHOD(ble_error_t, discoverServices, (ble::connection_handle_t connectionHandle, ServiceDiscovery::ServiceCallback_t callback, GattAttribute::Handle_t startHandle, GattAttribute::Handle_t endHandle), (override));
    MOCK_METHOD(bool, isServiceDiscoveryActive, (), (const, override));
    MOCK_METHOD(void, terminateServiceDiscovery, (), (override));
    MOCK_METHOD(ble_error_t, read, (ble::connection_handle_t connHandle, GattAttribute::Handle_t attributeHandle, uint16_t offset), (const, override));
    MOCK_METHOD(ble_error_t, write, (GattClient::WriteOp_t cmd, ble::connection_handle_t connHandle, GattAttribute::Handle_t attributeHandle, size_t length, const uint8_t *value), (const, override));
    MOCK_METHOD(void, onDataRead, (ReadCallback_t callback), (override));
    MOCK_METHOD(ReadCallbackChain_t&, onDataRead, (), (override));
    MOCK_METHOD(void, onDataWritten, (WriteCallback_t callback), (override));
    MOCK_METHOD(WriteCallbackChain_t&, onDataWritten, (), (override));
    MOCK_METHOD(void, onServiceDiscoveryTermination, (ServiceDiscovery::TerminationCallback_t callback), (override));
    MOCK_METHOD(ble_error_t, discoverCharacteristicDescriptors, (const DiscoveredCharacteristic &characteristic, const CharacteristicDescriptorDiscovery::DiscoveryCallback_t &discoveryCallback, const CharacteristicDescriptorDiscovery::TerminationCallback_t &terminationCallback), (override));
    MOCK_METHOD(bool, isCharacteristicDescriptorDiscoveryActive, (const DiscoveredCharacteristic &characteristic), (const, override));
    MOCK_METHOD(void, terminateCharacteristicDescriptorDiscovery, (const DiscoveredCharacteristic &characteristic), (override));
    MOCK_METHOD(ble_error_t, negotiateAttMtu, (ble::connection_handle_t connection), (override));
    MOCK_METHOD(void, onHVX, (HVXCallback_t callback), (override));
    MOCK_METHOD(void, onShutdown, (const GattClientShutdownCallback_t &callback), (override));
    MOCK_METHOD(GattClientShutdownCallbackChain_t&, onShutdown, (), (override));
    MOCK_METHOD(HVXCallbackChain_t&, onHVX, (), (override));
    MOCK_METHOD(void, processReadResponse, (const GattReadCallbackParams *params), (override));
    MOCK_METHOD(void, processWriteResponse, (const GattWriteCallbackParams *params), (override));
    MOCK_METHOD(void, processHVXEvent, (const GattHVXCallbackParams *params), (override));
};

}

#endif //BLE_GATTCLIENTMOCK_H
