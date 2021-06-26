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

#ifndef IMPL_GAP_GAP_H
#define IMPL_GAP_GAP_H

#include "ble/common/CallChainOfFunctionPointersWithContext.h"

#include <algorithm>

#include "platform/mbed_error.h"

#include "ble/common/BLERoles.h"
#include "ble/common/BLETypes.h"
#include "ble/gap/AdvertisingDataBuilder.h"
#include "ble/gap/AdvertisingDataSimpleBuilder.h"
#include "ble/gap/ConnectionParameters.h"
#include "ble/gap/ScanParameters.h"
#include "ble/gap/AdvertisingParameters.h"
#include "ble/gap/Events.h"

#include "source/pal/PalGap.h"
#include "source/pal/PalConnectionMonitor.h"
#include "source/pal/PalEventQueue.h"
#include "source/generic/PrivateAddressController.h"

#include "ble/Gap.h"

#ifdef DEVICE_LPTICKER
#include "drivers/LowPowerTimeout.h"
#include "drivers/LowPowerTicker.h"
#else
#include "drivers/Timeout.h"
#include "drivers/Ticker.h"
#endif

namespace ble {

class PalGenericAccessService;

class PalSecurityManager;

class PalGap;

class PalEventQueue;

namespace impl {

class BLEInstanceBase;

class Gap :
    public ble::PalConnectionMonitor,
    public PalGapEventHandler
#if BLE_FEATURE_PRIVACY
    , public PrivateAddressController::EventHandler
#endif //BLE_FEATURE_PRIVACY
    {
    friend PalConnectionMonitor;
    friend PalGapEventHandler;
    friend PalGap;
    friend impl::BLEInstanceBase;

    using EventHandler = ::ble::Gap::EventHandler;
    using GapShutdownCallback_t = ::ble::Gap::GapShutdownCallback_t;
    using GapShutdownCallbackChain_t  = ::ble::Gap::GapShutdownCallbackChain_t ;
public:
    using PreferredConnectionParams_t = ::ble::Gap::PreferredConnectionParams_t ;

#ifdef DEVICE_LPTICKER
    using Timeout = mbed::LowPowerTimeout;
    using Ticker  = mbed::LowPowerTicker;
#else
    using Timeout = mbed::Timeout;
    using Ticker  = mbed::Ticker;
#endif

#if BLE_FEATURE_PRIVACY
#if BLE_ROLE_BROADCASTER
    /**
     * Default peripheral privacy configuration.
     */
    static const peripheral_privacy_configuration_t
        default_peripheral_privacy_configuration;
#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_OBSERVER
    /**
     * Default peripheral privacy configuration.
     */
    static const central_privacy_configuration_t
        default_central_privacy_configuration;
#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PRIVACY

public:
    void setEventHandler(EventHandler *handler);

    bool isFeatureSupported(controller_supported_features_t feature);

    /*                                     advertising                                           */
#if BLE_ROLE_BROADCASTER

    uint8_t getMaxAdvertisingSetNumber();

    uint16_t getMaxAdvertisingDataLength();

    uint16_t getMaxConnectableAdvertisingDataLength();

    uint16_t getMaxActiveSetAdvertisingDataLength();

#if BLE_FEATURE_EXTENDED_ADVERTISING

    ble_error_t createAdvertisingSet(
        advertising_handle_t *handle,
        const AdvertisingParameters &parameters
    );

    ble_error_t destroyAdvertisingSet(advertising_handle_t handle);

#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    ble_error_t setAdvertisingParameters(
        advertising_handle_t handle,
        const AdvertisingParameters &params
    );

    ble_error_t setAdvertisingPayload(
        advertising_handle_t handle,
        mbed::Span<const uint8_t> payload
    );

    ble_error_t setAdvertisingScanResponse(
        advertising_handle_t handle,
        mbed::Span<const uint8_t> response
    );

    ble_error_t startAdvertising(
        advertising_handle_t handle,
        adv_duration_t maxDuration = adv_duration_t::forever(),
        uint8_t maxEvents = 0
    );

    ble_error_t stopAdvertising(advertising_handle_t handle);

    bool isAdvertisingActive(advertising_handle_t handle);

#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_PERIODIC_ADVERTISING

    ble_error_t setPeriodicAdvertisingParameters(
        advertising_handle_t handle,
        periodic_interval_t periodicAdvertisingIntervalMin,
        periodic_interval_t periodicAdvertisingIntervalMax,
        bool advertiseTxPower = true
    );

    ble_error_t setPeriodicAdvertisingPayload(
        advertising_handle_t handle,
        mbed::Span<const uint8_t> payload
    );

    ble_error_t startPeriodicAdvertising(advertising_handle_t handle);

    ble_error_t stopPeriodicAdvertising(advertising_handle_t handle);

    bool isPeriodicAdvertisingActive(advertising_handle_t handle);

#endif // BLE_ROLE_BROADCASTER
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

    /*                                     scanning                                              */
#if BLE_ROLE_OBSERVER

    ble_error_t setScanParameters(const ScanParameters &params);

    ble_error_t startScan(
        scan_duration_t duration = scan_duration_t::forever(),
        duplicates_filter_t filtering = duplicates_filter_t::DISABLE,
        scan_period_t period = scan_period_t(0)
    );

    ble_error_t initiate_scan();

    ble_error_t stopScan();

#endif // BLE_ROLE_OBSERVER

#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING

    ble_error_t createSync(
        peer_address_type_t peerAddressType,
        const address_t &peerAddress,
        uint8_t sid,
        slave_latency_t maxPacketSkip,
        sync_timeout_t timeout
    );

    ble_error_t createSync(
        slave_latency_t maxPacketSkip,
        sync_timeout_t timeout
    );

    ble_error_t cancelCreateSync();

    ble_error_t terminateSync(periodic_sync_handle_t handle);

    ble_error_t addDeviceToPeriodicAdvertiserList(
        peer_address_type_t peerAddressType,
        const address_t &peerAddress,
        advertising_sid_t sid
    );

    ble_error_t removeDeviceFromPeriodicAdvertiserList(
        peer_address_type_t peerAddressType,
        const address_t &peerAddress,
        advertising_sid_t sid
    );

    ble_error_t clearPeriodicAdvertiserList();

    uint8_t getMaxPeriodicAdvertiserListSize();

#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

#if BLE_ROLE_CENTRAL

    ble_error_t connect(
        peer_address_type_t peerAddressType,
        const address_t &peerAddress,
        const ConnectionParameters &connectionParams
    );

    ble_error_t cancelConnect();

#endif // BLE_ROLE_CENTRAL

#if BLE_FEATURE_CONNECTABLE

    ble_error_t updateConnectionParameters(
        connection_handle_t connectionHandle,
        conn_interval_t minConnectionInterval,
        conn_interval_t maxConnectionInterval,
        slave_latency_t slaveLatency,
        supervision_timeout_t supervision_timeout,
        conn_event_length_t minConnectionEventLength = conn_event_length_t(0),
        conn_event_length_t maxConnectionEventLength = conn_event_length_t(0)
    );

    ble_error_t manageConnectionParametersUpdateRequest(
        bool userManageConnectionUpdateRequest
    );

    ble_error_t acceptConnectionParametersUpdate(
        connection_handle_t connectionHandle,
        conn_interval_t minConnectionInterval,
        conn_interval_t maxConnectionInterval,
        slave_latency_t slaveLatency,
        supervision_timeout_t supervision_timeout,
        conn_event_length_t minConnectionEventLength = conn_event_length_t(0),
        conn_event_length_t maxConnectionEventLength = conn_event_length_t(0)
    );

    ble_error_t rejectConnectionParametersUpdate(
        connection_handle_t connectionHandle
    );

    ble_error_t disconnect(
        connection_handle_t connectionHandle,
        local_disconnection_reason_t reason
    );

#endif // BLE_FEATURE_CONNECTABLE
#if BLE_FEATURE_PHY_MANAGEMENT

    ble_error_t readPhy(connection_handle_t connection);

    ble_error_t setPreferredPhys(
        const phy_set_t *txPhys,
        const phy_set_t *rxPhys
    );

    ble_error_t setPhy(
        connection_handle_t connection,
        const phy_set_t *txPhys,
        const phy_set_t *rxPhys,
        coded_symbol_per_bit_t codedSymbol
    );

#endif // BLE_FEATURE_PHY_MANAGEMENT

#if BLE_FEATURE_PRIVACY

    ble_error_t enablePrivacy(bool enable);

#if BLE_ROLE_BROADCASTER

    ble_error_t setPeripheralPrivacyConfiguration(
        const peripheral_privacy_configuration_t *configuration
    );

    ble_error_t getPeripheralPrivacyConfiguration(
        peripheral_privacy_configuration_t *configuration
    );

#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_OBSERVER

    ble_error_t setCentralPrivacyConfiguration(
        const central_privacy_configuration_t *configuration
    );

    ble_error_t getCentralPrivacyConfiguration(
        central_privacy_configuration_t *configuration
    );

#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PRIVACY

#if BLE_FEATURE_WHITELIST

    uint8_t getMaxWhitelistSize() const;

    ble_error_t getWhitelist(whitelist_t &whitelist) const;

    ble_error_t setWhitelist(const whitelist_t &whitelist);

#endif // BLE_FEATURE_WHITELIST

    ble_error_t getAddress(
        own_address_type_t &typeP,
        address_t &address
    );

    static ble_error_t getRandomAddressType(
        ble::address_t address,
        ble::random_address_type_t *addressType
    );

    ble_error_t reset();

    void onShutdown(const GapShutdownCallback_t &callback);

    GapShutdownCallbackChain_t &onShutdown();

#if !defined(DOXYGEN_ONLY)

    /*
     * API reserved for the controller driver to set the random static address.
     * Setting a new random static address while the controller is operating is
     * forbidden by the Bluetooth specification.
     */
    ble_error_t setRandomStaticAddress(const ble::address_t &address);

    ble::address_t getRandomStaticAddress();

#endif // !defined(DOXYGEN_ONLY)

    /* ===================================================================== */
    /*                    private implementation follows                     */

private:
    /** List in random order */
    template<typename EventType, typename IndexType, IndexType MAX_EVENTS>
    class EventList {
    public:
        EventList()
        {
        };

        ~EventList()
        {
            for (IndexType i = 0; i < _current_size; ++i) {
                delete _pointers[i];
            }
        };

        /** Add event to the list. List takes ownership of memory.
         *
         * @param event List will point to this event.
         * @return False if list full.
         */
        bool push(EventType *event)
        {
            if (_current_size < MAX_EVENTS) {
                _pointers[_current_size] = event;
                _current_size++;
                return true;
            }
            return false;
        };

        /** Take one entry of the list. Transfers ownership to caller.
         *
         * @return The event return. Memory belongs to caller.
         */
        EventType* pop()
        {
            MBED_ASSERT(_current_size);

            if (!_current_size) {
                return nullptr;
            }

            EventType* event_returned = _pointers[_current_index];

            _current_size--;
            if (_current_size != _current_index) {
                _pointers[_current_index] = _pointers[_current_size];
            } else {
                _current_index = 0;
            }

            return event_returned;
        };

        /** Return pointer to the first element that fulfills the passed in condition and remove the entry
         * that was pointing to the item. Transfers ownership to caller.
         *
         * @param compare_func The condition that is checked for all the items.
         * @return First element that fulfills the passed in condition or nullptr if no such item found.
         */
        EventType* pop(mbed::Callback<bool(EventType&)> compare_func)
        {
            for (IndexType i = 0; i < _current_size ; ++i) {
                if (compare_func(*_pointers[_current_index])) {
                    return pop();
                }
                increment_current_index();
            }

            return nullptr;
        }

        /** Return pointer to the first element that fulfills the passed in condition and remove the entry
         * that was pointing to the item. Takes and returns number of failed matches allowing to speed up search.
         * Transfers ownership to caller.
         *
         * @note Calls must be consecutive - any call to pop or find will invalidate the search.
         *
         * @param compare_func The condition that is checked for all the items.
         * @param events_not_matching Pointer to the number of items already searched but not matching.
         * This is updated in the method.
         * @return First element that fulfills the passed in condition or nullptr if no such item found.
         */
        EventType* continue_pop(mbed::Callback<bool(EventType&)> compare_func, IndexType *events_not_matching)
        {
            _current_index = *events_not_matching;
            for (IndexType i = *events_not_matching; i < _current_size ; ++i) {
                if (compare_func(*_pointers[_current_index])) {
                    return pop();
                }
                (*events_not_matching)++;
                increment_current_index();
            }

            return nullptr;
        }

        /** Return pointer to the first element that fulfills the passed in condition. Does not remove item from list.
         *
         * @param compare_func The condition that is checked for all the items.
         * @return First element that fulfills the passed in condition or nullptr if no such item found.
         */
        EventType* find(mbed::Callback<bool(EventType&)> compare_func)
        {
            for (IndexType i = 0; i < _current_size ; ++i) {
                if (compare_func(*_pointers[_current_index])) {
                    return _pointers[_current_index];
                }
                increment_current_index();
            }

            return nullptr;
        }

        /** Return number of events stored.
         *
         * @return Number of events stored.
         */
        IndexType get_size()
        {
            return _current_size;
        }

    private:
        void increment_current_index()
        {
            _current_index++;
            if (_current_index == _current_size) {
                _current_index = 0;
            }
        }

    private:
        EventType* _pointers[MAX_EVENTS];
        IndexType _current_size = 0;
        /* this helps us find the event faster */
        IndexType _current_index = 0;
    };

#if BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    class PendingAdvertisingReportEvent {
    public:
        PendingAdvertisingReportEvent(
            const AdvertisingReportEvent& event_to_copy
        ) : event(event_to_copy)
        {
            /* copy the data to the buffer */
            const mbed::Span<const uint8_t> payload = event_to_copy.getPayload();
            if (payload.size()) {
                advertising_data_buffer = new(std::nothrow) uint8_t[payload.size()];
                if (advertising_data_buffer) {
                    memcpy(advertising_data_buffer, payload.data(), payload.size());
                    /* set the payload to our local copy of the data */
                    event.setAdvertisingData(mbed::make_Span(advertising_data_buffer, payload.size()));
                }
            }
        };

        ~PendingAdvertisingReportEvent()
        {
            delete[] advertising_data_buffer;
        }

        bool is_valid()
        {
            return advertising_data_buffer || (event.getPayload().size() == 0);
        }

        AdvertisingReportEvent& get_pending_event()
        {
            return event;
        }

    private:
        AdvertisingReportEvent event;
        uint8_t *advertising_data_buffer = nullptr;
    };
#endif // BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

private:
    /* Disallow copy and assignment. */
    Gap(const Gap &);

    Gap &operator=(const Gap &);

    Gap(
        ble::PalEventQueue &event_queue,
        ble::PalGap &pal_gap,
        ble::PalGenericAccessService &generic_access_service
#if BLE_FEATURE_PRIVACY
        , ble::PrivateAddressController &private_address_controller
#endif // BLE_FEATURE_PRIVACY
    );

    ~Gap();

#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_EXTENDED_ADVERTISING
    void process_enable_queue();
    void process_disable_queue();
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    ble_error_t setAdvertisingData(
        advertising_handle_t handle,
        Span<const uint8_t> payload,
        bool minimiseFragmentation,
        bool scan_response
    );

    void on_advertising_timeout();

    void process_advertising_timeout();
#endif // BLE_ROLE_BROADCASTER

    void on_gap_event_received(const GapEvent &e);

#if BLE_ROLE_OBSERVER
    void on_advertising_report(const GapAdvertisingReportEvent &e);
#endif // BLE_ROLE_OBSERVER

#if BLE_FEATURE_CONNECTABLE
    void on_connection_complete(const GapConnectionCompleteEvent &e);

    void on_disconnection_complete(const GapDisconnectionCompleteEvent &e);

    void on_connection_parameter_request(
        const GapRemoteConnectionParameterRequestEvent &e
    );

    void on_connection_update(const GapConnectionUpdateEvent &e);
#endif // BLE_FEATURE_CONNECTABLE

    void on_unexpected_error(const GapUnexpectedErrorEvent &e);

    enum AddressUseType_t {
        CENTRAL_CONNECTION,
        CENTRAL_SCAN,
        PERIPHERAL_CONNECTABLE,
        PERIPHERAL_NON_CONNECTABLE
    };

    own_address_type_t get_own_address_type(AddressUseType_t address_use_type);

#if BLE_FEATURE_WHITELIST
    bool initialize_whitelist() const;
#endif // BLE_FEATURE_WHITELIST

#if BLE_FEATURE_PRIVACY && !BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    ble_error_t update_ll_address_resolution_setting();
#endif // BLE_FEATURE_PRIVACY && !BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_EXTENDED_ADVERTISING
    ble_error_t setExtendedAdvertisingParameters(
        advertising_handle_t handle,
        const AdvertisingParameters &parameters
    );
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
#endif // BLE_ROLE_BROADCASTER

    bool is_extended_advertising_available();

#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_EXTENDED_ADVERTISING
    ble_error_t prepare_legacy_advertising_set(const AdvertisingParameters& parameters);
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
#endif // BLE_ROLE_BROADCASTER

#if BLE_FEATURE_CONNECTABLE
    /** Call the internal handlers that report to the security manager and GATT
     * that a connection has been established.
     *
     * @param report Connection event
     */
    void report_internal_connection_complete(const ConnectionCompleteEvent& report);

    /** Pass the connection complete event to the application. This may involve privacy resolution.
     *
     * @param report Event to be passed to the user application.
     */
    void signal_connection_complete(ConnectionCompleteEvent& report);

#if BLE_FEATURE_PRIVACY
    /**
     * Apply the privacy policies when the local peripheral is connected.
     * @param event The connection event
     * @return true if the policy process has been successful and false if the
     * it fails meaning the process connection shouldn't continue.
     */
    bool apply_peripheral_privacy_connection_policy(
        const ConnectionCompleteEvent &event
    );
#endif // BLE_FEATURE_PRIVACY

#if BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    /** Pass the connection complete event to the application after privacy resolution completed.
     *
     * @param event Event to be passed to the user application.
     * @param identity_address_type Address type of the identity address.
     * @param identity_address Address resolved by private address resolution, nullptr if no identity found.
     */
    void conclude_signal_connection_complete_after_address_resolution(
        ConnectionCompleteEvent &event,
        target_peer_address_type_t identity_address_type,
        const address_t *identity_address
    );
#endif // BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
#endif // BLE_FEATURE_CONNECTABLE

#if BLE_ROLE_OBSERVER
    /** Pass the advertising report to the application. This may involve privacy resolution.
     *
     * @param report Report to be passed to the user application.
     */
    void signal_advertising_report(AdvertisingReportEvent& report);

#if BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    /** Pass the advertising report to the application after privacy resolution completed.
     *
     * @param event Event to be passed to the user application.
     * @param identity_address_type Address type of the identity address.
     * @param identity_address Address resolved by private address resolution, nullptr if no identity found.
     */
    void conclude_signal_advertising_report_after_address_resolution(
        AdvertisingReportEvent &event,
        target_peer_address_type_t identity_address_type,
        const address_t *identity_address
    );
#endif // BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
#endif // BLE_ROLE_OBSERVER

    /* implements PalGap::EventHandler */
private:
#if BLE_FEATURE_PHY_MANAGEMENT
    void on_read_phy(
        hci_error_code_t hci_status,
        connection_handle_t connection_handle,
        phy_t tx_phy,
        phy_t rx_phy
    ) override;
#endif // BLE_FEATURE_PHY_MANAGEMENT

#if BLE_FEATURE_CONNECTABLE
    void on_data_length_change(
        connection_handle_t connection_handle,
        uint16_t tx_size,
        uint16_t rx_size
    ) override;
#endif

#if BLE_FEATURE_PHY_MANAGEMENT
    void on_phy_update_complete(
        hci_error_code_t hci_status,
        connection_handle_t connection_handle,
        phy_t tx_phy,
        phy_t rx_phy
    ) override;
#endif // BLE_FEATURE_PHY_MANAGEMENT

#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_EXTENDED_ADVERTISING
    void on_extended_advertising_report(
        advertising_event_t event_type,
        const connection_peer_address_type_t *address_type,
        const ble::address_t &address,
        phy_t primary_phy,
        const phy_t *secondary_phy,
        advertising_sid_t advertising_sid,
        advertising_power_t tx_power,
        rssi_t rssi,
        uint16_t periodic_advertising_interval,
        direct_address_type_t direct_address_type,
        const ble::address_t &direct_address,
        uint8_t data_length,
        const uint8_t *data
    ) override;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

#if BLE_FEATURE_PERIODIC_ADVERTISING
    void on_periodic_advertising_sync_established(
        hci_error_code_t error,
        sync_handle_t sync_handle,
        advertising_sid_t advertising_sid,
        connection_peer_address_type_t advertiser_address_type,
        const ble::address_t &advertiser_address,
        phy_t advertiser_phy,
        uint16_t periodic_advertising_interval,
        clock_accuracy_t clock_accuracy
    ) override;

    void on_periodic_advertising_report(
        sync_handle_t sync_handle,
        advertising_power_t tx_power,
        rssi_t rssi,
        advertising_data_status_t data_status,
        uint8_t data_length,
        const uint8_t *data
    ) override;

    void on_periodic_advertising_sync_loss(sync_handle_t sync_handle) override;
#endif // BLE_FEATURE_PERIODIC_ADVERTISING
#endif // BLE_ROLE_OBSERVER

#if BLE_ROLE_BROADCASTER
    void on_legacy_advertising_started() override;

    void on_legacy_advertising_stopped() override;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    void on_advertising_set_started(const mbed::Span<const uint8_t>& handles) override;

    void on_advertising_set_terminated(
        hci_error_code_t status,
        advertising_handle_t advertising_handle,
        connection_handle_t connection_handle,
        uint8_t number_of_completed_extended_advertising_events
    ) override;

    void on_scan_request_received(
        advertising_handle_t advertising_handle,
        connection_peer_address_type_t scanner_address_type,
        const ble::address_t &address
    ) override;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
#endif // BLE_ROLE_BROADCASTER

#if BLE_FEATURE_CONNECTABLE
    void on_connection_update_complete(
        hci_error_code_t status,
        connection_handle_t connection_handle,
        uint16_t connection_interval,
        uint16_t connection_latency,
        uint16_t supervision_timeout
    ) override;

    void on_remote_connection_parameter(
        connection_handle_t connection_handle,
        uint16_t connection_interval_min,
        uint16_t connection_interval_max,
        uint16_t connection_latency,
        uint16_t supervision_timeout
    ) override;
#endif // BLE_FEATURE_CONNECTABLE

#if BLE_ROLE_OBSERVER
    void on_scan_started(bool success) override;

    void on_scan_stopped(bool success) override;

    void on_scan_timeout() override;

    void process_legacy_scan_timeout();
#endif // BLE_ROLE_OBSERVER

#if BLE_FEATURE_PRIVACY
    /* Implement PrivateAddressController::EventHandler */
private:
    void on_resolvable_private_addresses_generated(const address_t &address) final;

    void on_non_resolvable_private_addresses_generated(const address_t &address) final;

    void on_private_address_generated(bool connectable);

#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    void on_address_resolution_completed(
        const address_t &peer_resolvable_address,
        bool resolved,
        target_peer_address_type_t identity_address_type,
        const address_t &identity_address
    ) final;
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
#endif // BLE_FEATURE_PRIVACY

private:
    bool is_advertising() const;

    bool is_radio_active() const;

    void update_advertising_set_connectable_attribute(
        advertising_handle_t handle,
        const AdvertisingParameters& parameters
    );

    enum class controller_operation_t {
        scanning, advertising, initiating
    };

    const address_t *get_random_address(controller_operation_t operation, size_t advertising_set = 0);

#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    void connecting_to_host_resolved_address_failed(bool inform_user = true);
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

private:
    /**
     * Callchain containing all registered callback handlers for shutdown
     * events.
     */
    GapShutdownCallbackChain_t shutdownCallChain;

    /**
     * Event handler provided by the application.
     */
    ble::Gap::EventHandler *_event_handler;

#if BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
#if BLE_ROLE_OBSERVER
    EventList<PendingAdvertisingReportEvent, uint8_t, BLE_GAP_MAX_ADVERTISING_REPORTS_PENDING_ADDRESS_RESOLUTION> _reports_pending_address_resolution;
#endif // BLE_ROLE_OBSERVER
#if BLE_FEATURE_CONNECTABLE
    EventList<ConnectionCompleteEvent, uint8_t, DM_CONN_MAX> _connections_pending_address_resolution;
#endif // BLE_FEATURE_CONNECTABLE
#endif // BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

    PalEventQueue &_event_queue;
    PalGap &_pal_gap;
    PalGenericAccessService &_gap_service;
#if BLE_FEATURE_PRIVACY
    PrivateAddressController &_private_address_controller;
#endif // BLE_FEATURE_PRIVACY
    ble::own_address_type_t _address_type;
    initiator_policy_t _initiator_policy_mode;
    scanning_filter_policy_t _scanning_filter_policy;
    advertising_filter_policy_t _advertising_filter_policy;
    mutable whitelist_t _whitelist;

    bool _privacy_enabled;
#if BLE_FEATURE_PRIVACY
    bool _privacy_initialization_pending = false;
#if BLE_ROLE_PERIPHERAL
    peripheral_privacy_configuration_t _peripheral_privacy_configuration;
#endif // BLE_ROLE_PERIPHERAL
#if BLE_ROLE_OBSERVER
    central_privacy_configuration_t _central_privacy_configuration;
#endif //BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PRIVACY
    ble::address_t _random_static_identity_address;

#if BLE_ROLE_OBSERVER
    enum class ScanState : uint8_t {
        idle,
        scan,
        pending_scan,
        pending_stop_scan
    };

    ScanState _scan_state = ScanState::idle;

    void set_scan_state(ScanState state);

    scan_duration_t _scan_requested_duration = scan_duration_t::forever();
    duplicates_filter_t _scan_requested_filtering = duplicates_filter_t::DISABLE;
    scan_period_t _scan_requested_period = scan_period_t(0);

    bool _scan_requested = false;
#endif // BLE_ROLE_OBSERVER

#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    enum class ConnectionToHostResolvedAddressState : uint8_t {
        idle,
        scan,
        connect
    };
    ble::address_t _connect_to_host_resolved_address;
    peer_address_type_t _connect_to_host_resolved_address_type = peer_address_type_t::ANONYMOUS;
    ConnectionToHostResolvedAddressState _connect_to_host_resolved_address_state = ConnectionToHostResolvedAddressState::idle;
    ConnectionParameters *_connect_to_host_resolved_address_parameters = nullptr;
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

    Timeout _advertising_timeout;
    Timeout _scan_timeout;
    Ticker _address_rotation_ticker;

    bool _initiating = false;

    template<size_t bit_size>
    struct BitArray {
        BitArray() : data()
        {
        }

        bool get(size_t index) const
        {
            position p(index);
            return (data[p.byte_index] >> p.bit_index) & 0x01;
        }

        void set(size_t index)
        {
            position p(index);
            data[p.byte_index] |= (0x01 << p.bit_index);
        }

        void clear(size_t index)
        {
            position p(index);
            data[p.byte_index] &= ~(0x01 << p.bit_index);
        }

        void clear()
        {
            for (size_t i = 0; i < (bit_size / 8 + 1); ++i) {
                data[i] = 0;
            }
        }

    private:
        struct position {
            position(size_t bit_number) :
                byte_index(bit_number / 8),
                bit_index(bit_number % 8)
            {
            }

            size_t byte_index;
            uint8_t bit_index;
        };

        uint8_t data[bit_size / 8 + 1];
    };

    BitArray<BLE_GAP_MAX_ADVERTISING_SETS> _existing_sets;
#if BLE_FEATURE_EXTENDED_ADVERTISING
    BitArray<BLE_GAP_MAX_ADVERTISING_SETS> _pending_stop_sets;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
    BitArray<BLE_GAP_MAX_ADVERTISING_SETS> _active_sets;
    BitArray<BLE_GAP_MAX_ADVERTISING_SETS> _active_periodic_sets;
    BitArray<BLE_GAP_MAX_ADVERTISING_SETS> _connectable_payload_size_exceeded;
    BitArray<BLE_GAP_MAX_ADVERTISING_SETS> _set_is_connectable;
    BitArray<BLE_GAP_MAX_ADVERTISING_SETS> _pending_sets;
    BitArray<BLE_GAP_MAX_ADVERTISING_SETS> _address_refresh_sets;
    BitArray<BLE_GAP_MAX_ADVERTISING_SETS> _interruptible_sets;
    BitArray<BLE_GAP_MAX_ADVERTISING_SETS> _adv_started_from_refresh;

#if BLE_FEATURE_EXTENDED_ADVERTISING
#if BLE_GAP_HOST_MAX_OUTSTANDING_ADVERTISING_START_COMMANDS < 1 || BLE_GAP_HOST_MAX_OUTSTANDING_ADVERTISING_START_COMMANDS > BLE_GAP_MAX_ADVERTISING_SETS
#error "BLE_GAP_HOST_MAX_OUTSTANDING_ADVERTISING_START_COMMANDS must be at least 1 and not bigger than BLE_GAP_MAX_ADVERTISING_SETS"
#endif
    struct AdvertisingEnableCommandParams_t {
        adv_duration_t max_durations[BLE_GAP_HOST_MAX_OUTSTANDING_ADVERTISING_START_COMMANDS];
        advertising_handle_t handles[BLE_GAP_HOST_MAX_OUTSTANDING_ADVERTISING_START_COMMANDS];
        uint8_t max_events[BLE_GAP_HOST_MAX_OUTSTANDING_ADVERTISING_START_COMMANDS];
        uint8_t number_of_handles;
    };

    AdvertisingEnableCommandParams_t _advertising_enable_command_params;
    bool _process_enable_queue_pending = false;
    bool _process_disable_queue_pending = false;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    bool _user_manage_connection_parameter_requests;
#if BLE_ROLE_OBSERVER
    bool _scan_parameters_set;
#endif // BLE_ROLE_OBSERVER

};

} // namespace impl
} // namespace ble

#endif //IMPL_GAP_GAP_H
