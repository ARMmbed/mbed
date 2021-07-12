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

#ifndef BLE_GAPSTUB_H
#define BLE_GAPSTUB_H

namespace ble {
namespace impl {

class Gap {
public:
    Gap() {};
    Gap(const Gap&) = delete;
    Gap& operator=(const Gap&) = delete;
    virtual ~Gap() {};

    using EventHandler = ::ble::Gap::EventHandler;
    using GapShutdownCallback_t = ::ble::Gap::GapShutdownCallback_t;
    using GapShutdownCallbackChain_t  = ::ble::Gap::GapShutdownCallbackChain_t ;
    using PreferredConnectionParams_t = ::ble::Gap::PreferredConnectionParams_t ;

#if BLE_FEATURE_PRIVACY
#if BLE_ROLE_BROADCASTER
    static const peripheral_privacy_configuration_t default_peripheral_privacy_configuration;
#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_OBSERVER
    /**
     * Default peripheral privacy configuration.
     */
    static const central_privacy_configuration_t default_central_privacy_configuration;
#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PRIVACY

public:
    virtual void setEventHandler(EventHandler *handler) { };

    virtual bool isFeatureSupported(controller_supported_features_t feature) { return true; };

    /*                                     advertising                                           */
#if BLE_ROLE_BROADCASTER

    virtual uint8_t getMaxAdvertisingSetNumber() { return 1; };

    virtual uint16_t getMaxAdvertisingDataLength() { return 23; };

    virtual uint16_t getMaxConnectableAdvertisingDataLength() {  return 21; };

    virtual uint16_t getMaxActiveSetAdvertisingDataLength() {  return 21; };

#if BLE_FEATURE_EXTENDED_ADVERTISING

    virtual ble_error_t createAdvertisingSet(
        advertising_handle_t *handle,
        const AdvertisingParameters &parameters
    ) {  return BLE_ERROR_NONE; };

    virtual ble_error_t destroyAdvertisingSet(advertising_handle_t handle) { return BLE_ERROR_NONE; };

#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    virtual ble_error_t setAdvertisingParameters(
        advertising_handle_t handle,
        const AdvertisingParameters &params
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t setAdvertisingPayload(
        advertising_handle_t handle,
        mbed::Span<const uint8_t> payload
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t setAdvertisingScanResponse(
        advertising_handle_t handle,
        mbed::Span<const uint8_t> response
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t startAdvertising(
        advertising_handle_t handle,
        adv_duration_t maxDuration = adv_duration_t::forever(),
        uint8_t maxEvents = 0
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t stopAdvertising(advertising_handle_t handle) { return BLE_ERROR_NONE; };

    virtual bool isAdvertisingActive(advertising_handle_t handle) { return true; };

#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_PERIODIC_ADVERTISING

    virtual ble_error_t setPeriodicAdvertisingParameters(
        advertising_handle_t handle,
        periodic_interval_t periodicAdvertisingIntervalMin,
        periodic_interval_t periodicAdvertisingIntervalMax,
        bool advertiseTxPower = true
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t setPeriodicAdvertisingPayload(
        advertising_handle_t handle,
        mbed::Span<const uint8_t> payload
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t startPeriodicAdvertising(advertising_handle_t handle) { return BLE_ERROR_NONE; };

    virtual ble_error_t stopPeriodicAdvertising(advertising_handle_t handle) { return BLE_ERROR_NONE; };

    virtual bool isPeriodicAdvertisingActive(advertising_handle_t handle) { return true; };

#endif // BLE_ROLE_BROADCASTER
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

    /*                                     scanning                                              */
#if BLE_ROLE_OBSERVER

    virtual ble_error_t setScanParameters(const ScanParameters &params) { return BLE_ERROR_NONE; };

    virtual ble_error_t startScan(
        scan_duration_t duration = scan_duration_t::forever(),
        duplicates_filter_t filtering = duplicates_filter_t::DISABLE,
        scan_period_t period = scan_period_t(0)
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t initiate_scan() { return BLE_ERROR_NONE; };

    virtual ble_error_t stopScan() { return BLE_ERROR_NONE; };

#endif // BLE_ROLE_OBSERVER

#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING

    virtual ble_error_t createSync(
        peer_address_type_t peerAddressType,
        const address_t &peerAddress,
        uint8_t sid,
        slave_latency_t maxPacketSkip,
        sync_timeout_t timeout
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t createSync(
        slave_latency_t maxPacketSkip,
        sync_timeout_t timeout
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t cancelCreateSync() { return BLE_ERROR_NONE; };

    virtual ble_error_t terminateSync(periodic_sync_handle_t handle) { return BLE_ERROR_NONE; };

    virtual ble_error_t addDeviceToPeriodicAdvertiserList(
        peer_address_type_t peerAddressType,
        const address_t &peerAddress,
        advertising_sid_t sid
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t removeDeviceFromPeriodicAdvertiserList(
            peer_address_type_t peerAddressType,
            const address_t &peerAddress,
            advertising_sid_t sid
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t clearPeriodicAdvertiserList() { return BLE_ERROR_NONE; };

    uint8_t getMaxPeriodicAdvertiserListSize() { return 1; };

#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

#if BLE_ROLE_CENTRAL

    virtual ble_error_t connect(
            peer_address_type_t peerAddressType,
            const address_t &peerAddress,
            const ConnectionParameters &connectionParams
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t cancelConnect() { return BLE_ERROR_NONE; };

#endif // BLE_ROLE_CENTRAL

#if BLE_FEATURE_CONNECTABLE

    virtual ble_error_t updateConnectionParameters(
        connection_handle_t connectionHandle,
        conn_interval_t minConnectionInterval,
        conn_interval_t maxConnectionInterval,
        slave_latency_t slaveLatency,
        supervision_timeout_t supervision_timeout,
        conn_event_length_t minConnectionEventLength = conn_event_length_t(0),
        conn_event_length_t maxConnectionEventLength = conn_event_length_t(0)
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t manageConnectionParametersUpdateRequest(
        bool userManageConnectionUpdateRequest
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t acceptConnectionParametersUpdate(
        connection_handle_t connectionHandle,
        conn_interval_t minConnectionInterval,
        conn_interval_t maxConnectionInterval,
        slave_latency_t slaveLatency,
        supervision_timeout_t supervision_timeout,
        conn_event_length_t minConnectionEventLength = conn_event_length_t(0),
        conn_event_length_t maxConnectionEventLength = conn_event_length_t(0)
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t rejectConnectionParametersUpdate(
        connection_handle_t connectionHandle
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t disconnect(
        connection_handle_t connectionHandle,
        local_disconnection_reason_t reason
    ) { return BLE_ERROR_NONE; };

#endif // BLE_FEATURE_CONNECTABLE
#if BLE_FEATURE_PHY_MANAGEMENT

    virtual ble_error_t readPhy(connection_handle_t connection) { return BLE_ERROR_NONE; };

    virtual ble_error_t setPreferredPhys(
        const phy_set_t *txPhys,
        const phy_set_t *rxPhys
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t setPhy(
        connection_handle_t connection,
        const phy_set_t *txPhys,
        const phy_set_t *rxPhys,
        coded_symbol_per_bit_t codedSymbol
    ) { return BLE_ERROR_NONE; };

#endif // BLE_FEATURE_PHY_MANAGEMENT

#if BLE_FEATURE_PRIVACY

    virtual ble_error_t enablePrivacy(bool enable) { return BLE_ERROR_NONE; };

#if BLE_ROLE_BROADCASTER

    virtual ble_error_t setPeripheralPrivacyConfiguration(
        const peripheral_privacy_configuration_t *configuration
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t getPeripheralPrivacyConfiguration(
        peripheral_privacy_configuration_t *configuration
    ) { return BLE_ERROR_NONE; };

#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_OBSERVER

    virtual ble_error_t setCentralPrivacyConfiguration(
        const central_privacy_configuration_t *configuration
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t getCentralPrivacyConfiguration(
        central_privacy_configuration_t *configuration
    ) { return BLE_ERROR_NONE; };

#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PRIVACY

#if BLE_FEATURE_WHITELIST

    virtual uint8_t getMaxWhitelistSize() const { return 1; };

    virtual ble_error_t getWhitelist(whitelist_t &whitelist) const { return BLE_ERROR_NONE; };

    virtual ble_error_t setWhitelist(const whitelist_t &whitelist) { return BLE_ERROR_NONE; };

#endif // BLE_FEATURE_WHITELIST

    virtual ble_error_t getAddress(
        own_address_type_t &typeP,
        address_t &address
    ) { return BLE_ERROR_NONE; };

    static ble_error_t getRandomAddressType(
        ble::address_t address,
        ble::random_address_type_t *addressType
    ) { return BLE_ERROR_NONE;};

    virtual ble_error_t reset() { return BLE_ERROR_NONE; };

    virtual void onShutdown(const GapShutdownCallback_t &callback) { };

    virtual GapShutdownCallbackChain_t &onShutdown() { static GapShutdownCallbackChain_t chain; return chain; };

    /*
     * API reserved for the controller driver to set the random static address.
     * Setting a new random static address while the controller is operating is
     * forbidden by the Bluetooth specification.
     */
    virtual ble_error_t setRandomStaticAddress(const ble::address_t &address) { return BLE_ERROR_NONE; };

    virtual ble::address_t getRandomStaticAddress() { return ble::address_t(); };
};

}
}

#endif //BLE_GAPSTUB_H
