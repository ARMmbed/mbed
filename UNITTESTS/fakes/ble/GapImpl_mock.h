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

#ifndef BLE_GAPMOCK_H
#define BLE_GAPMOCK_H

#include "gmock/gmock.h"
#include "source/generic/GapImpl.h"

namespace ble {

class GapMock : public ble::impl::Gap {
public:
    GapMock() {};
    GapMock(const GapMock&) = delete;
    GapMock& operator=(const GapMock&) = delete;
    virtual ~GapMock() {};

    MOCK_METHOD((ble_error_t), reset, (), (override));
    MOCK_METHOD(void, setEventHandler, (EventHandler *handler), (override));
    MOCK_METHOD(bool, isFeatureSupported, (controller_supported_features_t feature), (override));
    MOCK_METHOD(uint8_t, getMaxAdvertisingSetNumber, (), (override));
    MOCK_METHOD(uint16_t, getMaxAdvertisingDataLength, (), (override));
    MOCK_METHOD(uint16_t, getMaxConnectableAdvertisingDataLength, (), (override));
    MOCK_METHOD(uint16_t, getMaxActiveSetAdvertisingDataLength, (), (override));
    MOCK_METHOD(ble_error_t, createAdvertisingSet, (advertising_handle_t *handle, const AdvertisingParameters &parameters), (override));
    MOCK_METHOD(ble_error_t, destroyAdvertisingSet, (advertising_handle_t handle), (override));
    MOCK_METHOD(ble_error_t, setAdvertisingParameters, (advertising_handle_t handle, const AdvertisingParameters &params), (override));
    MOCK_METHOD(ble_error_t, setAdvertisingPayload, (advertising_handle_t handle, mbed::Span<const uint8_t> payload), (override));
    MOCK_METHOD(ble_error_t, setAdvertisingScanResponse, (advertising_handle_t handle, mbed::Span<const uint8_t> response), (override));
    MOCK_METHOD(ble_error_t, startAdvertising, (advertising_handle_t handle, adv_duration_t maxDuration, uint8_t maxEvents), (override));
    MOCK_METHOD(ble_error_t, stopAdvertising, (advertising_handle_t handle), (override));
    MOCK_METHOD(bool, isAdvertisingActive, (advertising_handle_t handle), (override));
    MOCK_METHOD(ble_error_t, setPeriodicAdvertisingParameters, (advertising_handle_t handle, periodic_interval_t periodicAdvertisingIntervalMin, periodic_interval_t periodicAdvertisingIntervalMax, bool advertiseTxPower), (override));
    MOCK_METHOD(ble_error_t, setPeriodicAdvertisingPayload, (advertising_handle_t handle, mbed::Span<const uint8_t> payload), (override));
    MOCK_METHOD(ble_error_t, startPeriodicAdvertising, (advertising_handle_t handle), (override));
    MOCK_METHOD(ble_error_t, stopPeriodicAdvertising, (advertising_handle_t handle), (override));
    MOCK_METHOD(bool, isPeriodicAdvertisingActive, (advertising_handle_t handle), (override));
    MOCK_METHOD(ble_error_t, setScanParameters, (const ScanParameters &params), (override));
    MOCK_METHOD(ble_error_t, startScan, (scan_duration_t duration, duplicates_filter_t filtering, scan_period_t period), (override));
    MOCK_METHOD(ble_error_t, initiate_scan, (), (override));
    MOCK_METHOD(ble_error_t, stopScan, (), (override));
    MOCK_METHOD(ble_error_t, createSync, (peer_address_type_t peerAddressType, const address_t &peerAddress, uint8_t sid, slave_latency_t maxPacketSkip, sync_timeout_t timeout), (override));
    MOCK_METHOD(ble_error_t, createSync, (slave_latency_t maxPacketSkip, sync_timeout_t timeout), (override));
    MOCK_METHOD(ble_error_t, cancelCreateSync, (), (override));
    MOCK_METHOD(ble_error_t, terminateSync, (periodic_sync_handle_t handle), (override));
    MOCK_METHOD(ble_error_t, addDeviceToPeriodicAdvertiserList, (peer_address_type_t peerAddressType, const address_t &peerAddress, advertising_sid_t sid), (override));
    MOCK_METHOD(ble_error_t, removeDeviceFromPeriodicAdvertiserList, (peer_address_type_t peerAddressType, const address_t &peerAddress, advertising_sid_t sid), (override));
    MOCK_METHOD(ble_error_t, clearPeriodicAdvertiserList, (), (override));
    MOCK_METHOD(ble_error_t, connect, (peer_address_type_t peerAddressType, const address_t &peerAddress, const ConnectionParameters &connectionParams), (override));
    MOCK_METHOD(ble_error_t, cancelConnect, (), (override));
    MOCK_METHOD(ble_error_t, updateConnectionParameters, (connection_handle_t connectionHandle, conn_interval_t minConnectionInterval, conn_interval_t maxConnectionInterval, slave_latency_t slaveLatency, supervision_timeout_t supervision_timeout, conn_event_length_t minConnectionEventLength, conn_event_length_t maxConnectionEventLength), (override));
    MOCK_METHOD(ble_error_t, manageConnectionParametersUpdateRequest, (bool userManageConnectionUpdateRequest), (override));
    MOCK_METHOD(ble_error_t, acceptConnectionParametersUpdate, (connection_handle_t connectionHandle, conn_interval_t minConnectionInterval, conn_interval_t maxConnectionInterval, slave_latency_t slaveLatency, supervision_timeout_t supervision_timeout, conn_event_length_t minConnectionEventLength, conn_event_length_t maxConnectionEventLength), (override));
    MOCK_METHOD(ble_error_t, rejectConnectionParametersUpdate, (connection_handle_t connectionHandle), (override));
    MOCK_METHOD(ble_error_t, disconnect, (connection_handle_t connectionHandle, local_disconnection_reason_t reason), (override));
    MOCK_METHOD(ble_error_t, readPhy, (connection_handle_t connection), (override));
    MOCK_METHOD(ble_error_t, setPreferredPhys, (const phy_set_t *txPhys, const phy_set_t *rxPhys), (override));
    MOCK_METHOD(ble_error_t, setPhy, (connection_handle_t connection, const phy_set_t *txPhys, const phy_set_t *rxPhys, coded_symbol_per_bit_t codedSymbol), (override));
    MOCK_METHOD(ble_error_t, enablePrivacy, (bool enable), (override));
    MOCK_METHOD(ble_error_t, setPeripheralPrivacyConfiguration, (const peripheral_privacy_configuration_t *configuration), (override));
    MOCK_METHOD(ble_error_t, getPeripheralPrivacyConfiguration, (peripheral_privacy_configuration_t *configuration), (override));
    MOCK_METHOD(ble_error_t, setCentralPrivacyConfiguration, (const central_privacy_configuration_t *configuration), (override));
    MOCK_METHOD(ble_error_t, getCentralPrivacyConfiguration, (central_privacy_configuration_t *configuration), (override));
    MOCK_METHOD(uint8_t, getMaxWhitelistSize, (), (const, override));
    MOCK_METHOD(ble_error_t, getWhitelist, (whitelist_t &whitelist), (const, override));
    MOCK_METHOD(ble_error_t, setWhitelist, (const whitelist_t &whitelist), (override));
    MOCK_METHOD(ble_error_t, getAddress, (own_address_type_t &typeP, address_t &address), (override));
    MOCK_METHOD(void, onShutdown, (const GapShutdownCallback_t &callback), (override));
    MOCK_METHOD(GapShutdownCallbackChain_t&, onShutdown, (), (override));
    MOCK_METHOD(ble_error_t, setRandomStaticAddress, (const ble::address_t &address), (override));
    MOCK_METHOD(ble::address_t, getRandomStaticAddress, (), (override));
};

}

#endif //BLE_GAPMOCK_H
