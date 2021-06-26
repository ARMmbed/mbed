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

#include <algorithm>
#include <cstdint>

#include "ble/Gap.h"
#include "ble/SecurityManager.h"

#include "source/BLEInstanceBase.h"
#include "source/generic/GapImpl.h"

#include "source/pal/PalGap.h"
#include "source/pal/GapEvents.h"
#include "source/pal/GapTypes.h"
#include "source/pal/PalGenericAccessService.h"
#include "source/pal/PalEventQueue.h"
#include "source/pal/PalSecurityManager.h"

#include "mbed-trace/mbed_trace.h"
#include "common/ble_trace_helpers.h"

#define TRACE_GROUP "BLGP"

// Cordio defines the random address used by connection to be the global one
#define CORDIO_GLOBAL_RANDOM_ADDRESS_FOR_CONNECTION 1

using namespace std::chrono;

MBED_STATIC_ASSERT(BLE_GAP_MAX_ADVERTISING_SETS < 0xFF, "BLE_GAP_MAX_ADVERTISING_SETS must be less than 255");
MBED_STATIC_ASSERT(BLE_GAP_MAX_ADVERTISING_SETS > 0, "BLE_GAP_MAX_ADVERTISING_SETS must be a positive number");

namespace ble {
namespace impl {

namespace {

// Constants
const uint16_t scan_interval_min = 0x0004;
const uint16_t scan_interval_max = 0x4000;
const uint16_t connection_interval_min = 0x0006;
const uint16_t connection_interval_max = 0x0C80;
const uint16_t slave_latency_min = 0x0000;
const uint16_t slave_latency_max = 0x01F3;
const uint16_t advertising_interval_min = 0x0020;
const uint16_t advertising_interval_max = 0x4000;
const uint16_t supervision_timeout_min = 0x000A;
const uint16_t supervision_timeout_max = 0x0C80;

const mbed_error_status_t mixed_scan_api_error =
    MBED_MAKE_ERROR(MBED_MODULE_BLE, MBED_ERROR_CODE_BLE_USE_INCOMPATIBLE_API);

const mbed_error_status_t illegal_state_error =
    MBED_MAKE_ERROR(MBED_MODULE_BLE, MBED_ERROR_CODE_BLE_ILLEGAL_STATE);

/*
 * Return true if value is included in the range [lower_bound : higher_bound]
 */
template<typename T>
static bool is_in_range(T value, T lower_bound, T higher_bound)
{
    if (value < lower_bound || value > higher_bound) {
        return false;
    }
    return true;
}

#if BLE_FEATURE_CONNECTABLE
/*
 * Return true of the connection parameters are acceptable as preferred connection
 * parameters.
 *
 * Preferred connection parameters unlike actual connection parameters allow the
 * max connection interval, min connection interval and connection supervision
 * timeout to be equal to 0xFFFF. When it is the case that value can be
 * interpreted as "non specific".
 */
bool is_preferred_connection_params_valid(const Gap::PreferredConnectionParams_t *params)
{
    if (params == nullptr) {
        return false;
    }

    if (!is_in_range(params->slaveLatency, slave_latency_min, slave_latency_max)) {
        return false;
    }

    if (!is_in_range(params->maxConnectionInterval, connection_interval_min, connection_interval_max) &&
        (params->maxConnectionInterval != 0xFFFF)) {
        return false;
    }

    if (!is_in_range(params->minConnectionInterval, connection_interval_min, params->maxConnectionInterval) &&
        (params->minConnectionInterval != 0xFFFF)) {
        return false;
    }

    if (params->connectionSupervisionTimeout == 0xFFFF) {
        return true;
    }

    if (!is_in_range(params->connectionSupervisionTimeout, supervision_timeout_min, supervision_timeout_max)) {
        return false;
    }

    if (params->maxConnectionInterval == 0xFFFF) {
        return true;
    }

    uint16_t max_connection_interval_ms =
        ((uint32_t) params->maxConnectionInterval * 125) / 100;
    uint16_t min_connection_supervision_timeout =
        ((1 + params->slaveLatency) * max_connection_interval_ms * 2) / 10;

    if (params->connectionSupervisionTimeout < min_connection_supervision_timeout) {
        return false;
    }

    return true;
}
#endif // BLE_FEATURE_CONNECTABLE

/**
 * Check if random bytes of an address are valid.
 */
bool is_prand_valid(const uint8_t *bytes, size_t len)
{
    // at least one bit of the random part of the static address shall be
    // equal to 0 and at least one bit of the random part of the static
    // address shall be equal to 1
    for (size_t i = 0; i < (len - 1); ++i) {
        if ((bytes[i] != 0x00) && (bytes[i] != 0xFF)) {
            return true;
        }

        if ((i > 0) && (bytes[i] != bytes[i - 1])) {
            return true;
        }
    }

    if (((bytes[len - 1] & 0x3F) == 0x3F) && (bytes[len - 2] == 0xFF)) {
        return false;
    }

    if (((bytes[len - 1] & 0x3F) == 0x00) && (bytes[len - 2] == 0x00)) {
        return false;
    }

    return true;
}

/*
 * Check if the random part of a random address with 48 random bytes are valid
 * or not.
 * Return true if it is the case and false otherwise.
 */
bool is_prand_48_bits_valid(const address_t &address)
{
    return is_prand_valid(address.data(), 6);
}

/*
 * Check if the random part of a random address with 24 random bytes are valid
 * or not.
 * Return true if it is the case and false otherwise.
 */
bool is_prand_24_bits_valid(const address_t &address)
{
    return is_prand_valid(address.data() + 3, 3);
}

/*
 * Return true if address is a random static address.
 */
bool is_random_static_address(const address_t &address)
{
    // top two msb bits shall be equal to 0b11.
    if ((address[5] & 0xC0) != 0xC0) {
        return false;
    }

    return is_prand_48_bits_valid(address);
}

/*
 * Return true if address is a random private non resolvable address.
 */
bool is_random_private_non_resolvable_address(
    const address_t &address
)
{
    // top two msb bits shall be equal to 0b00.
    if ((address[5] & 0xC0) != 0x00) {
        return false;
    }

    return is_prand_48_bits_valid(address);
}

/*
 * Return true if address is a random private resolvable address.
 */
bool is_random_private_resolvable_address(
    const address_t &address
)
{
    // top two msb bits shall be equal to 0b01.
    if ((address[5] & 0xC0) != 0x40) {
        return false;
    }

    return is_prand_24_bits_valid(address);
}

/*
 * Return true if the address is a random address.
 */
bool is_random_address(const address_t &address)
{
    return is_random_private_resolvable_address(address) ||
        is_random_private_non_resolvable_address(address) ||
        is_random_static_address(address);
}

#if BLE_FEATURE_WHITELIST
/*
 * Return true if the whitelist in input is valid or false otherwise.
 */
bool is_whitelist_valid(const ::ble::whitelist_t &whitelist)
{
    if (whitelist.size > whitelist.capacity) {
        return false;
    }

    for (size_t i = 0; i < whitelist.size; ++i) {
        const whitelist_t::entry_t &entry = whitelist.addresses[i];
        switch (entry.type.value()) {
            case peer_address_type_t::PUBLIC:
            case peer_address_type_t::PUBLIC_IDENTITY:
                if (is_random_address(entry.address)) {
                    return false; // needs to be public
                }
                break;
            case peer_address_type_t::RANDOM:
            case peer_address_type_t::RANDOM_STATIC_IDENTITY:
                if (!is_random_static_address(entry.address)) {
                    return false; // needs to be random static
                }
                break;
            default: // ANONYMOUS
                return false;
        }

    }
    return true;
}

/*
 * Return true if device is present in the whitelist.
 */
bool is_in_whitelist(
    const whitelist_t::entry_t &device, const whitelist_t &whitelist
)
{
    for (size_t i = 0; i < whitelist.size; ++i) {
        const whitelist_t::entry_t &potential_device = whitelist.addresses[i];

        if (potential_device.address == device.address) {
            return true;
        }
    }
    return false;
}

/*
 * Convert a peer_address_type_t into a whitelist_address_type_t.
 */
whitelist_address_type_t to_whitelist_address_type(
    peer_address_type_t address_type
)
{
    return (
        address_type == peer_address_type_t::PUBLIC
            || address_type == peer_address_type_t::PUBLIC_IDENTITY
    ) ?
        whitelist_address_type_t::PUBLIC_DEVICE_ADDRESS :
        whitelist_address_type_t::RANDOM_DEVICE_ADDRESS;
}
#endif // BLE_FEATURE_WHITELIST

#if BLE_FEATURE_CONNECTABLE
microsecond_t minSupervisionTimeout(
    const conn_interval_t &maxConnectionInterval,
    const slave_latency_t &slaveLatency
)
{
    // see BLUETOOTH SPECIFICATION Version 5.0 | Vol 2, Part E
    // Section 7.8.12 LE Create Connection
    return (1 + slaveLatency.value()) * maxConnectionInterval * 2;
}
#endif // BLE_FEATURE_CONNECTABLE

bool is_connectable_advertising(const AdvertisingParameters &params)
{
    advertising_event_properties_t event_properties(params.getType());
    return event_properties.connectable;
}

} // end of anonymous namespace

#if BLE_FEATURE_PRIVACY
#if BLE_ROLE_BROADCASTER
const peripheral_privacy_configuration_t Gap::default_peripheral_privacy_configuration = {
    /* use_non_resolvable_random_address */ false,
    /* resolution_strategy */ peripheral_privacy_configuration_t::PERFORM_PAIRING_PROCEDURE
};
#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_OBSERVER
const central_privacy_configuration_t Gap::default_central_privacy_configuration = {
    /* use_non_resolvable_random_address */ false,
    /* resolution_strategy */ central_privacy_configuration_t::RESOLVE_AND_FORWARD
};
#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PRIVACY

Gap::Gap(
    ble::PalEventQueue &event_queue,
    ble::PalGap &pal_gap,
    ble::PalGenericAccessService &generic_access_service
#if BLE_FEATURE_PRIVACY
    , ble::PrivateAddressController &private_address_controller
#endif //BLE_FEATURE_PRIVACY
) : _event_queue(event_queue),
    _pal_gap(pal_gap),
    _gap_service(generic_access_service),
#if BLE_FEATURE_PRIVACY
    _private_address_controller(private_address_controller),
#endif // BLE_FEATURE_PRIVACY
    _address_type(own_address_type_t::PUBLIC),
    _initiator_policy_mode(initiator_policy_t::NO_FILTER),
    _scanning_filter_policy(scanning_filter_policy_t::NO_FILTER),
    _advertising_filter_policy(advertising_filter_policy_t::NO_FILTER),
    _whitelist(),
    _privacy_enabled(false),
#if BLE_FEATURE_PRIVACY
#if BLE_ROLE_BROADCASTER
    _peripheral_privacy_configuration(default_peripheral_privacy_configuration),
#endif // BLE_ROLE_BROADCASTER
#if BLE_ROLE_OBSERVER
    _central_privacy_configuration(default_central_privacy_configuration),
#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PRIVACY
    _advertising_timeout(),
    _scan_timeout(),
    _user_manage_connection_parameter_requests(false)
#if BLE_ROLE_OBSERVER
    , _scan_parameters_set(false)
#endif // BLE_ROLE_OBSERVER
{
#if BLE_FEATURE_EXTENDED_ADVERTISING
    _advertising_enable_command_params.number_of_handles = 0;
#endif //BLE_FEATURE_EXTENDED_ADVERTISING
    _pal_gap.initialize();

    _pal_gap.when_gap_event_received(
        mbed::callback(this, &Gap::on_gap_event_received)
    );

    // Recover static random identity
    _random_static_identity_address = _pal_gap.get_random_address();

    _pal_gap.set_event_handler(this);
#if BLE_FEATURE_PRIVACY
    _private_address_controller.set_event_handler(this);
#endif // BLE_FEATURE_PRIVACY
}


Gap::~Gap() = default;


bool Gap::isFeatureSupported(controller_supported_features_t feature)
{
#if !BLE_FEATURE_PHY_MANAGEMENT
    if (feature == ble::controller_supported_features_t::LE_CODED_PHY ||
        feature == ble::controller_supported_features_t::LE_2M_PHY
        ) {
        return false;
    }
#endif

#if !BLE_FEATURE_EXTENDED_ADVERTISING
    if (feature == ble::controller_supported_features_t::LE_EXTENDED_ADVERTISING) {
        return false;
    }
#endif

#if !BLE_FEATURE_PERIODIC_ADVERTISING
    if (feature == ble::controller_supported_features_t::LE_PERIODIC_ADVERTISING) {
        return false;
    }
#endif

#if !BLE_FEATURE_PRIVACY
    if (feature == ble::controller_supported_features_t::LL_PRIVACY) {
        return false;
    }
#endif

    return _pal_gap.is_feature_supported(feature);
}


ble_error_t Gap::setRandomStaticAddress(
    const ble::address_t &address
)
{
    if (is_random_static_address(address.data()) == false) {
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!_privacy_enabled) {
        ble_error_t err = _pal_gap.set_random_address(address);
        if (err) {
            return err;
        }
    }

    _address_type = own_address_type_t::RANDOM;
    _random_static_identity_address = address;

    return BLE_ERROR_NONE;
}

ble::address_t Gap::getRandomStaticAddress()
{
    return _random_static_identity_address;
}

ble_error_t Gap::getAddress(
    own_address_type_t &type,
    address_t &address
)
{
    tr_info("Get address - "
            "type=%s, "
            "address=%s",
            to_string(type),
            to_string(address));

    type = _address_type;

    if (_address_type == own_address_type_t::PUBLIC) {
        address = _pal_gap.get_device_address();
    } else {
        address = _pal_gap.get_random_address();
    }

    return BLE_ERROR_NONE;
}


ble_error_t Gap::getRandomAddressType(
    const address_t address,
    ble::random_address_type_t *type
)
{
    // see section Device address in Bluetooth Link Layer specification
    // (Vol 6 - Part B)
    switch (address[5] >> 6) {
        case 0x03:
            *type = random_address_type_t::STATIC;
            return BLE_ERROR_NONE;
        case 0x00:
            *type = random_address_type_t::NON_RESOLVABLE_PRIVATE;
            return BLE_ERROR_NONE;
        case 0x01:
            *type = random_address_type_t::RESOLVABLE_PRIVATE;
            return BLE_ERROR_NONE;
        default:
            return BLE_ERROR_INVALID_PARAM;
    }
}

#if BLE_ROLE_OBSERVER
ble_error_t Gap::stopScan()
{
    tr_info("Stop scan");
    ble_error_t err;

    if (_initiating) {
        tr_error("stack busy");
        return BLE_STACK_BUSY;
    }

    _scan_requested = false;

    if (_scan_state == ScanState::scan) {
#if BLE_FEATURE_EXTENDED_ADVERTISING
        if (is_extended_advertising_available()) {
            err = _pal_gap.extended_scan_enable(false, duplicates_filter_t::DISABLE, 0, 0);
        } else
#endif
        {
            err = _pal_gap.scan_enable(false, false);
        }

        if (err) {
            return err;
        }
        set_scan_state(ScanState::pending_stop_scan);
    }

    _scan_timeout.detach();

    return BLE_ERROR_NONE;
}
#endif

#if BLE_ROLE_CENTRAL
ble_error_t Gap::connect(
    peer_address_type_t peerAddressType,
    const ble::address_t &peerAddress,
    const ConnectionParameters &connectionParams
)
{
    tr_info("Connect - "
            "peerAddressType=%s, "
            "peerAddress=%s, "
            "_filterPolicy=%s, "
            "_ownAddressType=%s, ",
            to_string(peerAddressType),
            to_string(peerAddress),
            to_string(connectionParams.getFilter()),
            to_string(connectionParams.getOwnAddressType()));

    // Trace connection params for each enabled PHY
    for (size_t i = 0; i < connectionParams.getPhySet().count(); ++i) {
        tr_info("PHY %d - "
                "_scanInterval=%" PRIu32 "ms, "
                "_scanWindow=%" PRIu32 "ms, "
                "_minConnectionInterval=%" PRIu32 "ms, "
                "_maxConnectionInterval=%" PRIu32 "ms, "
                "_slaveLatency=%u, "
                "_connectionSupervisionTimeout=%" PRIu32 "ms, "
                "_minEventLength=%" PRIu32 "ms, "
                "_maxEventLength=%" PRIu32 "ms",
                i,
                scan_interval_t(connectionParams.getScanIntervalArray()[i]).valueInMs(),
                scan_window_t(connectionParams.getScanWindowArray()[i]).valueInMs(),
                conn_interval_t(connectionParams.getMinConnectionIntervalArray()[i]).valueInMs(),
                conn_interval_t(connectionParams.getMaxConnectionIntervalArray()[i]).valueInMs(),
                slave_latency_t(connectionParams.getSlaveLatencyArray()[i]).value(),
                supervision_timeout_t(connectionParams.getConnectionSupervisionTimeoutArray()[i]).valueInMs(),
                conn_event_length_t(connectionParams.getMinEventLengthArray()[i]).valueInMs(),
                conn_event_length_t(connectionParams.getMaxEventLengthArray()[i]).valueInMs());
    }

#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    if (_connect_to_host_resolved_address_state == ConnectionToHostResolvedAddressState::scan) {
        return BLE_ERROR_OPERATION_NOT_PERMITTED;
    }
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

    if (!connectionParams.getNumberOfEnabledPhys()) {
        tr_error("no enabled PHY");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (peerAddressType == peer_address_type_t::ANONYMOUS) {
        tr_error("peer address type cannot be anonymous");
        return BLE_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < connectionParams.getPhySet().count(); ++i) {
        if (supervision_timeout_t(connectionParams.getConnectionSupervisionTimeoutArray()[i]) <=
            minSupervisionTimeout(
                conn_interval_t(connectionParams.getMaxConnectionIntervalArray()[i]),
                slave_latency_t(connectionParams.getSlaveLatencyArray()[i])
            )
            ) {
            tr_error("supervision timout cannot be less than/equal to %" PRIu32 "ms", minSupervisionTimeout(
                             conn_interval_t(connectionParams.getMaxConnectionIntervalArray()[i]),
                             slave_latency_t(connectionParams.getSlaveLatencyArray()[i])).valueInMs());
            return BLE_ERROR_INVALID_PARAM;
        }
    }

    // get the random address to set, if not valid, report the error
    const address_t *address = get_random_address(controller_operation_t::initiating);
    if (!address) {
        tr_error("could not get random address");
        return BLE_ERROR_INVALID_STATE;
    }

    ble_error_t ret = BLE_ERROR_INTERNAL_STACK_FAILURE;

#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    /* if host resolution is used we need to connect in two passes, first we scan for addresses to find
     * a resolving match and then we call connect again with the correct address */
    if (_connect_to_host_resolved_address_state == ConnectionToHostResolvedAddressState::idle) {
        if (peerAddressType == peer_address_type_t::RANDOM_STATIC_IDENTITY ||
            peerAddressType == peer_address_type_t::PUBLIC_IDENTITY) {

            _connect_to_host_resolved_address_parameters = new ConnectionParameters(connectionParams);
            if (!_connect_to_host_resolved_address_parameters) {
                return BLE_ERROR_NO_MEM;
            }

            _connect_to_host_resolved_address_type = peerAddressType;
            _connect_to_host_resolved_address = peerAddress;
            _connect_to_host_resolved_address_state = ConnectionToHostResolvedAddressState::scan;
        }
    } else if (_connect_to_host_resolved_address_state == ConnectionToHostResolvedAddressState::connect) {
        /* the first pass of connect has completed and this is the second connect that doesn't require
         * address resolution */
        _connect_to_host_resolved_address_state = ConnectionToHostResolvedAddressState::idle;
        _initiating = false;
    }

#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

    if (is_extended_advertising_available() == false) {
        phy_set_t set(connectionParams.getPhySet());
        if (set.count() != 1 || set.get_1m() == false) {
            tr_error("use 1 PHY w/legacy advertising");
            return BLE_ERROR_INVALID_PARAM;
        }

        if (_scan_state == ScanState::idle) {
            if (!_active_sets.get(LEGACY_ADVERTISING_HANDLE) &&
                !_pending_sets.get(LEGACY_ADVERTISING_HANDLE)
                ) {
                _pal_gap.set_random_address(*address);
            }
        } else {
            stopScan();
        }
#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
        if (_connect_to_host_resolved_address_state == ConnectionToHostResolvedAddressState::scan) {
            ret = startScan(scan_duration_t::forever(), duplicates_filter_t::ENABLE, scan_period_t(0));
            if (ret != BLE_ERROR_NONE) {
                _connect_to_host_resolved_address_state = ConnectionToHostResolvedAddressState::idle;
            }
        } else
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
        {
            ret = _pal_gap.create_connection(
                connectionParams.getScanIntervalArray()[0],
                connectionParams.getScanWindowArray()[0],
                connectionParams.getFilter(),
                (connection_peer_address_type_t::type) peerAddressType.value(),
                peerAddress,
                connectionParams.getOwnAddressType(),
                connectionParams.getMinConnectionIntervalArray()[0],
                connectionParams.getMaxConnectionIntervalArray()[0],
                connectionParams.getSlaveLatencyArray()[0],
                connectionParams.getConnectionSupervisionTimeoutArray()[0],
                connectionParams.getMinEventLengthArray()[0],
                connectionParams.getMaxConnectionIntervalArray()[0]
            );
        }
    }
#if BLE_FEATURE_EXTENDED_ADVERTISING
    else {
        // set the correct mac address before starting scanning.
        if (_scan_state == ScanState::idle) {
            _pal_gap.set_random_address(*address);
        } else {
            // ensure scan is stopped.
            stopScan();
        }

        // reduce the address type to public or random
        peer_address_type_t adjusted_address_type(peer_address_type_t::PUBLIC);

        if (peerAddressType == peer_address_type_t::RANDOM ||
            peerAddressType == peer_address_type_t::RANDOM_STATIC_IDENTITY) {
            adjusted_address_type = peer_address_type_t::RANDOM;
        }

#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
        if (_connect_to_host_resolved_address_state == ConnectionToHostResolvedAddressState::scan) {
            ret = startScan(scan_duration_t::forever(), duplicates_filter_t::ENABLE, scan_period_t(0));
            if (ret != BLE_ERROR_NONE) {
                _connect_to_host_resolved_address_state = ConnectionToHostResolvedAddressState::idle;
            }
        } else
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
        {
            ret = _pal_gap.extended_create_connection(
                connectionParams.getFilter(),
                connectionParams.getOwnAddressType(),
                adjusted_address_type,
                peerAddress,
                connectionParams.getPhySet(),
                connectionParams.getScanIntervalArray(),
                connectionParams.getScanWindowArray(),
                connectionParams.getMinConnectionIntervalArray(),
                connectionParams.getMaxConnectionIntervalArray(),
                connectionParams.getSlaveLatencyArray(),
                connectionParams.getConnectionSupervisionTimeoutArray(),
                connectionParams.getMinEventLengthArray(),
                connectionParams.getMaxEventLengthArray()
            );
        }
    }
#endif

    if (ret == BLE_ERROR_NONE) {
        _initiating = true;
    } else {
        tr_error("PAL could not create connection");
    }
    return ret;
}
#endif


#if BLE_FEATURE_CONNECTABLE
ble_error_t Gap::manageConnectionParametersUpdateRequest(bool flag)
{
    _user_manage_connection_parameter_requests = flag;
    return BLE_ERROR_NONE;
}
#endif


#if BLE_FEATURE_CONNECTABLE
ble_error_t Gap::updateConnectionParameters(
    connection_handle_t connectionHandle,
    conn_interval_t minConnectionInterval,
    conn_interval_t maxConnectionInterval,
    slave_latency_t slaveLatency,
    supervision_timeout_t supervisionTimeout,
    conn_event_length_t minConnectionEventLength,
    conn_event_length_t maxConnectionEventLength
)
{
    tr_info("Connection %d: update connection paramters - "
            "minConnectionInterval=%" PRIu32 "ms, "
            "maxConnectionInterval=%" PRIu32 "ms, "
            "slaveLatency=%u,"
            "supervisionTimeout=%" PRIu32 "ms, "
            "minConnectionEventLength=%" PRIu32 "ms, "
            "maxConnectionEventLength=%" PRIu32 "ms",
            connectionHandle,
            minConnectionInterval.valueInMs(),
            maxConnectionInterval.valueInMs(),
            slaveLatency.value(),
            supervisionTimeout.valueInMs(),
            minConnectionInterval.valueInMs(),
            minConnectionInterval.valueInMs());

    if (supervisionTimeout <= minSupervisionTimeout(maxConnectionInterval, slaveLatency)) {
        tr_error("supervision timeout cannot be less than/equal to %" PRIu32 "ms",
                 minSupervisionTimeout(maxConnectionInterval, slaveLatency).valueInMs());
        return BLE_ERROR_INVALID_PARAM;
    }

    return _pal_gap.connection_parameters_update(
        connectionHandle,
        minConnectionInterval.value(),
        maxConnectionInterval.value(),
        slaveLatency.value(),
        supervisionTimeout.value(),
        minConnectionEventLength.value(),
        maxConnectionEventLength.value()
    );
}

ble_error_t Gap::acceptConnectionParametersUpdate(
    connection_handle_t connectionHandle,
    conn_interval_t minConnectionInterval,
    conn_interval_t maxConnectionInterval,
    slave_latency_t slaveLatency,
    supervision_timeout_t supervisionTimeout,
    conn_event_length_t minConnectionEventLength,
    conn_event_length_t maxConnectionEventLength
)
{
    tr_info("Connection %d: accept connection paramater update - "
            "minConnectionInterval=%" PRIu32 "ms, "
            "maxConnectionInterval=%" PRIu32 "ms, "
            "slaveLatency=%u,"
            "supervisionTimeout=%" PRIu32 "ms, "
            "minConnectionEventLength=%" PRIu32 "ms, "
            "minConnectionEventLength=%" PRIu32 "ms",
            connectionHandle,
            minConnectionInterval.valueInMs(),
            maxConnectionInterval.valueInMs(),
            slaveLatency.value(),
            supervisionTimeout.valueInMs(),
            minConnectionEventLength.valueInMs(),
            maxConnectionEventLength.valueInMs());

    if (supervisionTimeout <= minSupervisionTimeout(maxConnectionInterval, slaveLatency)) {
        tr_error("supervision timeout cannot be less than/equal to %" PRIu32 "ms",
                 minSupervisionTimeout(maxConnectionInterval, slaveLatency).valueInMs());
        return BLE_ERROR_INVALID_PARAM;
    }

    return _pal_gap.accept_connection_parameter_request(
        connectionHandle,
        minConnectionInterval.value(),
        maxConnectionInterval.value(),
        slaveLatency.value(),
        supervisionTimeout.value(),
        minConnectionEventLength.value(),
        maxConnectionEventLength.value()
    );
}

ble_error_t Gap::rejectConnectionParametersUpdate(
    connection_handle_t connectionHandle
)
{
    return _pal_gap.reject_connection_parameter_request(
        connectionHandle,
        hci_error_code_t::UNACCEPTABLE_CONNECTION_PARAMETERS
    );
}
#endif // BLE_FEATURE_CONNECTABLE

#if BLE_ROLE_CENTRAL
ble_error_t Gap::cancelConnect()
{
    tr_info("Cancel connect");
#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    if (_connect_to_host_resolved_address_state == ConnectionToHostResolvedAddressState::scan) {
        connecting_to_host_resolved_address_failed(false);
        stopScan();
        return BLE_ERROR_NONE;
    }
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

    if (!_initiating) {
        tr_error("Failed to cancel connect: stack busy");
        return BLE_ERROR_NONE;
    }

    auto ret = _pal_gap.cancel_connection_creation();
    if (ret) {
        _initiating = false;
    }
    return ret;
}
#endif // BLE_ROLE_CENTRAL

#if BLE_FEATURE_PHY_MANAGEMENT
ble_error_t Gap::readPhy(ble::connection_handle_t connection)
{
    return _pal_gap.read_phy(connection);
}

ble_error_t Gap::setPreferredPhys(
    const phy_set_t *txPhys,
    const phy_set_t *rxPhys
)
{
    tr_info("Set preferred PHYs - "
            "txPhys=%s, "
            "rxPhys=%s, ",
            to_string(*txPhys),
            to_string(*rxPhys));

    phy_set_t tx_phys(txPhys ? txPhys->value() : 0);
    phy_set_t rx_phys(rxPhys ? rxPhys->value() : 0);
    return _pal_gap.set_preferred_phys(tx_phys, rx_phys);
}

ble_error_t Gap::setPhy(
    ble::connection_handle_t connection,
    const phy_set_t *txPhys,
    const phy_set_t *rxPhys,
    ble::coded_symbol_per_bit_t codedSymbol
)
{
    tr_info("Set PHY for connection %d - "
            "txPhys=%s, "
            "rxPhys=%s, "
            "codedSymbol=%s",
            connection,
            to_string(*txPhys),
            to_string(*rxPhys),
            to_string(codedSymbol));

    phy_set_t tx_phys(txPhys ? txPhys->value() : 0);
    phy_set_t rx_phys(rxPhys ? rxPhys->value() : 0);
    return _pal_gap.set_phy(connection, tx_phys, rx_phys, codedSymbol);
}

void Gap::on_read_phy(
    hci_error_code_t hci_status,
    ble::connection_handle_t connection_handle,
    phy_t tx_phy,
    phy_t rx_phy
)
{
    ble_error_t status = BLE_ERROR_NONE;
    if (hci_status != hci_error_code_t::SUCCESS) {
        status = BLE_ERROR_UNSPECIFIED;
    }

    if (_event_handler) {
        _event_handler->onPhyUpdateComplete(status, connection_handle, tx_phy, rx_phy);
    }
}
#endif // BLE_FEATURE_PHY_MANAGEMENT

#if BLE_FEATURE_CONNECTABLE
void Gap::on_data_length_change(
    connection_handle_t connection_handle,
    uint16_t tx_size,
    uint16_t rx_size
)
{
    if (_event_handler) {
        _event_handler->onDataLengthChange(connection_handle, tx_size, rx_size);
    }
}
#endif // BLE_FEATURE_CONNECTABLE

#if BLE_FEATURE_PHY_MANAGEMENT
void Gap::on_phy_update_complete(
    hci_error_code_t hci_status,
    ble::connection_handle_t connection_handle,
    phy_t tx_phy,
    phy_t rx_phy
)
{
    ble_error_t status = BLE_ERROR_NONE;
    if (hci_status != hci_error_code_t::SUCCESS) {
        status = BLE_ERROR_UNSPECIFIED;
    }

    if (_event_handler) {
        _event_handler->onPhyUpdateComplete(status, connection_handle, tx_phy, rx_phy);
    }
}
#endif // BLE_FEATURE_PHY_MANAGEMENT

#if BLE_FEATURE_CONNECTABLE
ble_error_t Gap::disconnect(
    connection_handle_t connectionHandle,
    local_disconnection_reason_t reason
)
{
    tr_info("Connection %d: disconnect - "
            "reason=%s",
            connectionHandle,
            to_string(reason));

    return _pal_gap.disconnect(connectionHandle, reason);
}
#endif // BLE_FEATURE_CONNECTABLE

#if BLE_FEATURE_WHITELIST
uint8_t Gap::getMaxWhitelistSize(void) const
{
    return _pal_gap.read_white_list_capacity();
}

ble_error_t Gap::getWhitelist(whitelist_t &whitelist) const
{
    if (initialize_whitelist() == false) {
        tr_error("Cannot get whitelist: failed to initialize");
        return BLE_ERROR_INVALID_STATE;
    }

    if (whitelist.capacity < _whitelist.capacity) {
        tr_error("Cannot get whitelist: capacity less than %d", _whitelist.capacity);
        return BLE_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < _whitelist.size; ++i) {
        whitelist.addresses[i] = _whitelist.addresses[i];
    }

    whitelist.capacity = _whitelist.capacity;
    return BLE_ERROR_NONE;
}

ble_error_t Gap::setWhitelist(const whitelist_t &whitelist)
{
    tr_info("Set whitelist");
    if (is_whitelist_valid(whitelist) == false) {
        tr_error("not a valid whitelist");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (initialize_whitelist() == false) {
        tr_error("could not initialize whitelist");
        return BLE_ERROR_INVALID_STATE;
    }

    if (whitelist.capacity > _whitelist.capacity) {
        tr_error("capacity of whitelist greater than %d", _whitelist.capacity);
        return BLE_ERROR_INVALID_PARAM;
    }

    // first evict devices not in the existing whitelist
    for (size_t i = 0; i < _whitelist.size; ++i) {
        const whitelist_t::entry_t &device = _whitelist.addresses[i];

        if (is_in_whitelist(device, whitelist) == false) {
            ble_error_t err = _pal_gap.remove_device_from_whitelist(
                to_whitelist_address_type(device.type),
                device.address
            );

            // try to restore the whitelist to its initial state
            if (err) {
                for (size_t j = 0; j < i; ++j) {
                    const whitelist_t::entry_t &device = _whitelist.addresses[j];

                    if (is_in_whitelist(device, whitelist) == false) {
                        _pal_gap.add_device_to_whitelist(
                            to_whitelist_address_type(device.type),
                            device.address
                        );
                    }
                }
                return err;
            }
        }
    }

    // second add devices which were not in the initial whitelist
    for (size_t i = 0; i < whitelist.size; ++i) {
        const whitelist_t::entry_t &device = whitelist.addresses[i];

        if (is_in_whitelist(device, _whitelist) == false) {
            ble_error_t err = _pal_gap.add_device_to_whitelist(
                to_whitelist_address_type(device.type),
                device.address
            );

            // try to restore the whitelist to its initial state
            if (err) {
                // first remove the devices added
                for (size_t j = 0; j < i; ++j) {
                    const whitelist_t::entry_t &device = whitelist.addresses[j];

                    if (is_in_whitelist(device, _whitelist) == false) {
                        _pal_gap.remove_device_from_whitelist(
                            to_whitelist_address_type(device.type),
                            device.address
                        );
                    }
                }

                // second add the devices of the initial list evicted
                for (size_t i = 0; i < _whitelist.size; ++i) {
                    const whitelist_t::entry_t &device = _whitelist.addresses[i];

                    if (is_in_whitelist(device, whitelist) == false) {
                        _pal_gap.add_device_to_whitelist(
                            to_whitelist_address_type(device.type),
                            device.address
                        );
                    }
                }

                return err;
            }
        }
    }

    // commit the devices into the whitelist
    for (size_t i = 0; i < whitelist.size; ++i) {
        _whitelist.addresses[i] = whitelist.addresses[i];
    }

    _whitelist.size = whitelist.size;

    return BLE_ERROR_NONE;
}
#endif // BLE_FEATURE_WHITELIST

#if BLE_FEATURE_PRIVACY
ble_error_t Gap::enablePrivacy(bool enable)
{
    tr_info("Enable privacy -"
            "enable=%s",
            to_string(enable));

    if (enable == _privacy_enabled) {
        tr_error("privacy already enabled");
        // No change
        return BLE_ERROR_NONE;
    }

    if (is_radio_active()) {
        tr_error("radio active");
        return BLE_ERROR_INVALID_STATE;
    }

    _privacy_enabled = enable;

    if (_privacy_enabled) {
        _private_address_controller.start_private_address_generation();
        if (_private_address_controller.get_non_resolvable_private_address() != address_t {} &&
            _private_address_controller.get_resolvable_private_address() != address_t{}
        ) {
            _event_queue.post([this] {
               if (_event_handler) {
                   _event_handler->onPrivacyEnabled();
               }
            });
        } else {
            _privacy_initialization_pending = true;
        }
    } else {
        _private_address_controller.stop_private_address_generation();
        _privacy_initialization_pending = false;
    }

#if !BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    if (_private_address_controller.is_controller_privacy_supported()) {
        update_ll_address_resolution_setting();
    }
#endif // !BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

    return BLE_ERROR_NONE;
}
#endif // BLE_FEATURE_PRIVACY


#if BLE_FEATURE_PRIVACY
#if BLE_ROLE_BROADCASTER
ble_error_t Gap::setPeripheralPrivacyConfiguration(
    const peripheral_privacy_configuration_t *configuration
)
{
    tr_info("Set peripheral privacy configuration - "
            "use_non_resolvable_random_address=%s, "
            "resolution_strategy=%s",
            to_string(configuration->use_non_resolvable_random_address),
            to_string(configuration->resolution_strategy));

    _peripheral_privacy_configuration = *configuration;

#if !BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    if (_private_address_controller.is_controller_privacy_supported()) {
        update_ll_address_resolution_setting();
    }
#endif // !BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

    return BLE_ERROR_NONE;
}

ble_error_t Gap::getPeripheralPrivacyConfiguration(
    peripheral_privacy_configuration_t *configuration
)
{
    *configuration = _peripheral_privacy_configuration;

    return BLE_ERROR_NONE;
}
#endif // BLE_ROLE_BROADCASTER
#endif // BLE_FEATURE_PRIVACY

#if BLE_FEATURE_PRIVACY
#if BLE_ROLE_OBSERVER
ble_error_t Gap::setCentralPrivacyConfiguration(
    const central_privacy_configuration_t *configuration
)
{
    _central_privacy_configuration = *configuration;

#if !BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    if (_private_address_controller.is_controller_privacy_supported()) {
        update_ll_address_resolution_setting();
    } else {
        tr_error("Failed to set central privacy configuration: controller privacy not supported");
    }
#endif // !BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

    return BLE_ERROR_NONE;
}

ble_error_t Gap::getCentralPrivacyConfiguration(
    central_privacy_configuration_t *configuration
)
{
    *configuration = _central_privacy_configuration;

    return BLE_ERROR_NONE;
}
#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PRIVACY

ble_error_t Gap::reset()
{
    /* Notify that the instance is about to shut down */
//    shutdownCallChain.call(this);
    shutdownCallChain.clear();

    _event_handler = nullptr;
    _initiating = false;

#if BLE_FEATURE_PRIVACY
    _privacy_initialization_pending = false;
#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    _connect_to_host_resolved_address_state = ConnectionToHostResolvedAddressState::idle;
    delete _connect_to_host_resolved_address_parameters;
    _connect_to_host_resolved_address_parameters = nullptr;
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
#endif // BLE_FEATURE_PRIVACY

#if BLE_ROLE_OBSERVER
    set_scan_state(ScanState::idle);
    _scan_requested = false;
    _scan_parameters_set = false;
    _scan_timeout.detach();
#endif

#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_EXTENDED_ADVERTISING
    /* clear advertising set data on the controller */
    _pal_gap.clear_advertising_sets();
    /* reset pending advertising sets */
    _advertising_enable_command_params.number_of_handles = 0;
    _process_enable_queue_pending = false;
    _process_disable_queue_pending = false;
    _existing_sets.clear();
    _pending_stop_sets.clear();
#if BLE_FEATURE_PERIODIC_ADVERTISING
    _active_periodic_sets.clear();
#endif // BLE_FEATURE_PERIODIC_ADVERTISING
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
    _active_sets.clear();
    _pending_sets.clear();
    _address_refresh_sets.clear();
    _interruptible_sets.clear();
    _connectable_payload_size_exceeded.clear();
    _set_is_connectable.clear();
    _advertising_timeout.detach();
#endif // #BLE_ROLE_BROADCASTER

    return BLE_ERROR_NONE;
}

void Gap::onShutdown(const GapShutdownCallback_t &callback)
{
    tr_info("GAP instance shutting down...");
    shutdownCallChain.add(callback);
}

Gap::GapShutdownCallbackChain_t &Gap::onShutdown()
{
    tr_info("GAP instance shutting down...");
    return shutdownCallChain;
}

#if BLE_ROLE_OBSERVER
void Gap::on_scan_started(bool success)
{
    tr_info("Scan %s", success ? "successfully started" : "failed to start");

    MBED_ASSERT(_scan_state == ScanState::pending_scan);

    if (success) {
        set_scan_state(ScanState::scan);
        /* if no longer want the scan */
        if (!_scan_requested) {
            stopScan();
        }
    } else {
        set_scan_state(ScanState::idle);
    }
}

void Gap::set_scan_state(ScanState state)
{
    _scan_state = state;

    if (state == ScanState::idle) {
        tr_info("Scan state: idle");
    } else if (state == ScanState::pending_scan) {
        tr_info("Scan state: pending_scan");
    } else if (state == ScanState::pending_stop_scan) {
        tr_info("Scan state: pending_stop_scan");
    } else if (state == ScanState::scan) {
        tr_info("Scan state: scan");
    }
}

void Gap::on_scan_stopped(bool success)
{
    tr_info("Scan %s", success ? "successfully stopped" : "failed to stop");

    if (!success) {
        return;
    }

    set_scan_state(ScanState::idle);

#if BLE_ROLE_BROADCASTER
    /* with legacy advertising we might need to wait for scanning and advertising to both stop */
    if (!is_extended_advertising_available()) {
        bool wait_for_advertising_stop =
            _address_refresh_sets.get(LEGACY_ADVERTISING_HANDLE) &&
            _active_sets.get(LEGACY_ADVERTISING_HANDLE) &&
            _pending_sets.get(LEGACY_ADVERTISING_HANDLE);

        if (wait_for_advertising_stop) {
            /* return early if we're waiting for advertising to stop and do not restart scan even if requested */
            tr_error("advertising must be stopped before address can be refreshed");
            return;
        }

        bool restart_advertising =
            !_active_sets.get(LEGACY_ADVERTISING_HANDLE) &&
            !_pending_sets.get(LEGACY_ADVERTISING_HANDLE) &&
            _address_refresh_sets.get(LEGACY_ADVERTISING_HANDLE);

        if (restart_advertising) {
            /* this will refresh the address and we can continue scan if we want as well */
            _address_refresh_sets.clear(LEGACY_ADVERTISING_HANDLE);
            startAdvertising(LEGACY_ADVERTISING_HANDLE);
            _adv_started_from_refresh.set(LEGACY_ADVERTISING_HANDLE);
        }
    }
#endif // BLE_ROLE_BROADCASTER

    if (_scan_requested) {
        initiate_scan();
    }
}
#endif // BLE_ROLE_OBSERVER

#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
void Gap::connecting_to_host_resolved_address_failed(bool inform_user)
{
    tr_info("Connecting to host resolved address failed%s", inform_user ? ", informing user" : "");

    if (inform_user && _event_handler) {
        _event_handler->onConnectionComplete(
            ConnectionCompleteEvent(
                BLE_ERROR_NOT_FOUND,
                INVALID_ADVERTISING_HANDLE,
                connection_role_t::CENTRAL,
                peer_address_type_t::ANONYMOUS,
                ble::address_t(),
                ble::address_t(),
                ble::address_t(),
                ble::conn_interval_t::max(),
                /* dummy slave latency */ 0,
                ble::supervision_timeout_t::max(),
                /* master clock accuracy */ 0
            )
        );
    }
    _connect_to_host_resolved_address_state = ConnectionToHostResolvedAddressState::idle;
    delete _connect_to_host_resolved_address_parameters;
    _connect_to_host_resolved_address_parameters = nullptr;
    _initiating = false;
}
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

#if BLE_ROLE_OBSERVER
void Gap::on_scan_timeout()
{
    tr_info("Scan timed out");

    if (_scan_state == ScanState::idle) {
        tr_warning("event not passed to handler, scan state idle");
        return;
    }

    set_scan_state(ScanState::idle);
    _scan_requested = false;

    if (_event_handler) {
        _event_handler->onScanTimeout(ScanTimeoutEvent());
    }
}
#endif // BLE_ROLE_OBSERVER

#if BLE_ROLE_OBSERVER
void Gap::process_legacy_scan_timeout()
{
    tr_info("Process legacy scan timed out");

    if (_scan_state == ScanState::idle) {
        tr_warning("event not passed to handler, scan state idle");
        return;
    }

    stopScan();

    if (_event_handler) {
        _event_handler->onScanTimeout(ScanTimeoutEvent());
    }
}
#endif // BLE_ROLE_OBSERVER

#if BLE_ROLE_BROADCASTER
void Gap::on_advertising_timeout()
{
    tr_info("Advertising timed out");

    _event_queue.post(mbed::callback(this, &Gap::process_advertising_timeout));
}

void Gap::process_advertising_timeout()
{
    tr_info("Process advertising timed out");

    if (!_active_sets.get(LEGACY_ADVERTISING_HANDLE)) {
        return;
    }

    stopAdvertising(LEGACY_ADVERTISING_HANDLE);
}
#endif // BLE_ROLE_BROADCASTER

void Gap::on_gap_event_received(const GapEvent &e)
{
    tr_debug("GAP event of type %s received", to_string(e.type));

    switch (e.type.value()) {
#if BLE_ROLE_OBSERVER
        case GapEventType::ADVERTISING_REPORT:
            on_advertising_report(static_cast<const GapAdvertisingReportEvent &>(e));
            break;
#endif // BLE_ROLE_OBSERVER
#if BLE_FEATURE_CONNECTABLE
        case GapEventType::CONNECTION_COMPLETE:
            on_connection_complete(static_cast<const GapConnectionCompleteEvent &>(e));
            break;

        case GapEventType::CONNECTION_UPDATE:
            on_connection_update(static_cast<const GapConnectionUpdateEvent &>(e));
            break;

        case GapEventType::DISCONNECTION_COMPLETE:
            on_disconnection_complete(static_cast<const GapDisconnectionCompleteEvent &>(e));
            break;

        case GapEventType::REMOTE_CONNECTION_PARAMETER_REQUEST:
            on_connection_parameter_request(static_cast<const GapRemoteConnectionParameterRequestEvent &>(e));
            break;
#endif // BLE_FEATURE_CONNECTABLE
        case GapEventType::UNEXPECTED_ERROR:
            on_unexpected_error(static_cast<const GapUnexpectedErrorEvent &>(e));
            break;

        default:
            break;
    }
}

#if BLE_ROLE_OBSERVER
void Gap::on_advertising_report(const GapAdvertisingReportEvent &e)
{
    tr_info("GAP advertising report received");

    for (size_t i = 0; i < e.size(); ++i) {
        GapAdvertisingReportEvent::advertising_t advertising = e[i];

        tr_debug("Advertising %d - "
                "type=%s, "
                "address_type=%s, "
                "address=%s, "
                "data=%s, "
                "rssi=%d",
                i, /* Advertising number */
                to_string(advertising.type),
                to_string(advertising.address_type),
                to_string(advertising.address),
                mbed_trace_array(advertising.data.data(), advertising.data.size()),
                advertising.rssi);

        // note 1-to-1 conversion between connection_peer_address_type_t and
        // peer_address_type_t
        peer_address_type_t peer_address_type =
            static_cast<peer_address_type_t::type>(advertising.address_type.value());

        // report in new event handler
        if (!_event_handler) {
            continue;
        }

        uint8_t event_type = 0;

        // Conversion table available at BLUETOOTH SPECIFICATION Version 5.0 | Vol 2, Part E
        // 7.7.65.13
        switch (advertising.type.value()) {
            case received_advertising_type_t::ADV_IND:
                event_type = 0x13;
                break;
            case received_advertising_type_t::ADV_DIRECT_IND:
                event_type = 0x15;
                break;
            case received_advertising_type_t::ADV_SCAN_IND:
                event_type = 0x12;
                break;
            case received_advertising_type_t::ADV_NONCONN_IND:
                event_type = 0x10;
                break;
            case received_advertising_type_t::SCAN_RESPONSE:
                event_type = 0x1B;
                break;
        }

        AdvertisingReportEvent event(
            advertising_event_t(event_type),
            peer_address_type,
            advertising.address,
            /* primary */ phy_t::LE_1M,
            /* secondary */ phy_t::NONE,
            /* SID - NO ADI FIELD IN THE PDU */ 0xFF,
            /* tx power information not available */ 127,
            advertising.rssi,
            /* NO PERIODIC ADVERTISING */ 0,
            peer_address_type_t::ANONYMOUS,
            ble::address_t(),
            Span<const uint8_t>(advertising.data.data(), advertising.data.size())
        );

        signal_advertising_report(
            event
        );
    }
}
#endif // BLE_ROLE_OBSERVER

#if BLE_FEATURE_CONNECTABLE
void Gap::on_connection_complete(const GapConnectionCompleteEvent &e)
{
    tr_info("Connection %d %s - "
            "role=%s, "
            "peer_address_type=%s "
            "peer_address=%s, "
            "connection_interval=%d, "
            "connection_latency=%d, "
            "supervision_timeout=%d, "
            "local_resolvable_private_address=%s, "
            "peer_resolvable_private_address=%s",
            e.connection_handle,
            e.status == 0 ? "successfully completed" : "failed to complete",
            e.status == 0 ? to_string(e.role) : "n/a",
            to_string(e.peer_address_type),
            to_string(e.peer_address),
            e.connection_interval,
            e.connection_latency,
            e.supervision_timeout,
            to_string(e.local_resolvable_private_address),
            to_string(e.peer_resolvable_private_address));

    if (e.role == connection_role_t::CENTRAL) {
        _initiating = false;
    }

    if (e.status != hci_error_code_t::SUCCESS) {
        if (_event_handler) {
            _event_handler->onConnectionComplete(
                ConnectionCompleteEvent(
                    BLE_ERROR_NOT_FOUND,
                    INVALID_ADVERTISING_HANDLE,
                    connection_role_t::CENTRAL,
                    peer_address_type_t::ANONYMOUS,
                    ble::address_t(),
                    ble::address_t(),
                    ble::address_t(),
                    ble::conn_interval_t::max(),
                    /* dummy slave latency */ 0,
                    ble::supervision_timeout_t::max(),
                    /* master clock accuracy */ 0
                )
            );
        }

        return;
    }

    if (!is_extended_advertising_available() && e.role == connection_role_t::PERIPHERAL) {
        _active_sets.clear(LEGACY_ADVERTISING_HANDLE);
    }

#if BLE_ROLE_PERIPHERAL
    if (e.role.value() == e.role.PERIPHERAL) {
        _advertising_timeout.detach();
        _pal_gap.advertising_enable(false);
    }
#endif // BLE_ROLE_PERIPHERAL

    ConnectionCompleteEvent event(
        BLE_ERROR_NONE,
        e.connection_handle,
        e.role,
        e.peer_address_type,
        e.peer_address,
        e.local_resolvable_private_address,
        e.peer_resolvable_private_address,
        conn_interval_t(e.connection_interval),
        e.connection_latency,
        supervision_timeout_t(e.supervision_timeout),
        /* default master clock accuracy */ ble::clock_accuracy_t::PPM_500
    );

    if (_event_handler) {
        signal_connection_complete(event);
    } else {
        report_internal_connection_complete(event);
    }
}

void Gap::on_disconnection_complete(const GapDisconnectionCompleteEvent &e)
{
    if (e.status == 0) {
        tr_info("Disconnection %d successfully completed - "
                "reason=%d",
                e.connection_handle,
                e.reason);
    } else {
        tr_info("Disconnection %d failed", e.connection_handle);
    }

    if (e.status == hci_error_code_t::SUCCESS) {
        // signal internal stack
        if (_connection_event_handler) {
            _connection_event_handler->on_disconnected(
                (connection_handle_t) e.connection_handle,
                disconnection_reason_t(e.reason)
            );
        }

        // signal application
        if (_event_handler) {
            _event_handler->onDisconnectionComplete(
                DisconnectionCompleteEvent(
                    (connection_handle_t) e.connection_handle,
                    (disconnection_reason_t::type) e.reason
                )
            );
        }
    } else {
        // TODO: define what to do in case of failure
    }
}

void Gap::on_connection_parameter_request(const GapRemoteConnectionParameterRequestEvent &e)
{
    tr_info("Connection %d parameter request - "
            "min_connection_interval=%d, "
            "max_connection_interval=%d, "
            "connection_latency=%d, "
            "supervision_timeout=%d",
            e.connection_handle,
            e.min_connection_interval,
            e.max_connection_interval,
            e.connection_latency,
            e.supervision_timeout);

    if (_user_manage_connection_parameter_requests) {
        if (_event_handler) {
            _event_handler->onUpdateConnectionParametersRequest(
                UpdateConnectionParametersRequestEvent(
                    e.connection_handle,
                    conn_interval_t(e.min_connection_interval),
                    conn_interval_t(e.max_connection_interval),
                    e.connection_latency,
                    supervision_timeout_t(e.supervision_timeout)
                )
            );
        } else {
            MBED_ERROR(illegal_state_error, "Event handler required if connection params are user handled");
        }
    } else {
        _pal_gap.accept_connection_parameter_request(
            e.connection_handle,
            e.min_connection_interval,
            e.max_connection_interval,
            e.connection_latency,
            e.supervision_timeout,
            /* minimum_connection_event_length */ 0,
            /* maximum_connection_event_length */ 0
        );
    }
}

void Gap::on_connection_update(const GapConnectionUpdateEvent &e)
{
    tr_info("Connection %d update %s - "
            "connection_interval=%d, "
            "connection_latency=%d,"
            "supervision_timeout=%d, ",
            e.connection_handle,
            e.status == 0 ? "successfully completed" : "failed",
            e.connection_interval,
            e.connection_latency,
            e.supervision_timeout);

    if (!_event_handler) {
        return;
    }

    _event_handler->onConnectionParametersUpdateComplete(
        ConnectionParametersUpdateCompleteEvent(
            e.status == hci_error_code_t::SUCCESS ? BLE_ERROR_NONE : BLE_ERROR_UNSPECIFIED,
            e.connection_handle,
            conn_interval_t(e.connection_interval),
            e.connection_latency,
            supervision_timeout_t(e.supervision_timeout)
        )
    );
}
#endif //BLE_FEATURE_CONNECTABLE

void Gap::on_unexpected_error(const GapUnexpectedErrorEvent &e)
{
    // TODO: add feature in interface to notify the user that the connection
    // has been updated.
}

own_address_type_t Gap::get_own_address_type(AddressUseType_t address_use_type)
{
#if BLE_FEATURE_PRIVACY
    if (_privacy_enabled) {
        bool use_non_resolvable_address = false;
#if BLE_ROLE_OBSERVER
        if (address_use_type == CENTRAL_SCAN) {
            use_non_resolvable_address = _central_privacy_configuration.use_non_resolvable_random_address;
        } else
#endif
#if BLE_ROLE_BROADCASTER
        if (address_use_type == PERIPHERAL_NON_CONNECTABLE) {
            use_non_resolvable_address = _peripheral_privacy_configuration.use_non_resolvable_random_address;
        }
#endif // BLE_ROLE_BROADCASTER

        // An non resolvable private address should be generated
        if (use_non_resolvable_address) {
            return own_address_type_t::RANDOM;
        }

        switch (_address_type.value()) {
            case own_address_type_t::PUBLIC:
                return own_address_type_t::RESOLVABLE_PRIVATE_ADDRESS_PUBLIC_FALLBACK;
            default:
                return own_address_type_t::RESOLVABLE_PRIVATE_ADDRESS_RANDOM_FALLBACK;
        }
    }
#endif // BLE_FEATURE_PRIVACY

    switch (_address_type.value()) {
        case own_address_type_t::PUBLIC:
            return own_address_type_t::PUBLIC;
        default:
            return own_address_type_t::RANDOM;
    }
}

#if BLE_FEATURE_WHITELIST
bool Gap::initialize_whitelist() const
{
    tr_info("Initialize whitelist");

    if (_whitelist.addresses != nullptr) {
        tr_error("whitelist already initialized");
        return true;
    }

    uint8_t whitelist_capacity = _pal_gap.read_white_list_capacity();

    if (whitelist_capacity == 0) {
        tr_error("capacity of whitelist is zero");
        return false;
    }

    _whitelist.addresses = new(std::nothrow) whitelist_t::entry_t[whitelist_capacity];
    if (_whitelist.addresses == nullptr) {
        tr_error("failed to dynamically allocate memory for a whitelist of %d addresses", whitelist_capacity);
        return false;
    }

    _whitelist.size = 0;
    _whitelist.capacity = whitelist_capacity;

    return true;
}
#endif // BLE_FEATURE_WHITELIST

#if BLE_FEATURE_PRIVACY && !BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
ble_error_t Gap::update_ll_address_resolution_setting()
{
    // enable if privacy is enabled and resolution is requested in either central or peripheral mode
    bool enable = false;

    if (_privacy_enabled) {
#if BLE_ROLE_BROADCASTER
        if (_peripheral_privacy_configuration.resolution_strategy !=
            peripheral_privacy_configuration_t::DO_NOT_RESOLVE) {
            enable = true;
        }
#endif // BLE_ROLE_BROADCASTER
#if BLE_ROLE_OBSERVER
        if (_central_privacy_configuration.resolution_strategy != central_privacy_configuration_t::DO_NOT_RESOLVE) {
            enable = true;
        }
#endif // BLE_ROLE_OBSERVER
    }

    return _private_address_controller.enable_controller_address_resolution(enable);
}
#endif // BLE_FEATURE_PRIVACY && !BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

#if BLE_ROLE_BROADCASTER
uint8_t Gap::getMaxAdvertisingSetNumber()
{
#if BLE_FEATURE_EXTENDED_ADVERTISING
    if (is_extended_advertising_available()) {
        uint8_t set_number = _pal_gap.get_max_number_of_advertising_sets();
        return std::min((uint8_t) BLE_GAP_MAX_ADVERTISING_SETS, set_number);
    } else
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
    {
        return 1;
    }
}
#endif


#if BLE_ROLE_BROADCASTER
uint16_t Gap::getMaxAdvertisingDataLength()
{
    return _pal_gap.get_maximum_advertising_data_length();
}
#endif


#if BLE_ROLE_BROADCASTER
uint16_t Gap::getMaxConnectableAdvertisingDataLength()
{
    return _pal_gap.get_maximum_connectable_advertising_data_length();
}
#endif


#if BLE_ROLE_BROADCASTER
uint16_t Gap::getMaxActiveSetAdvertisingDataLength()
{
    return _pal_gap.get_maximum_hci_advertising_data_length();
}
#endif


#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_EXTENDED_ADVERTISING
ble_error_t Gap::createAdvertisingSet(
    advertising_handle_t *handle,
    const AdvertisingParameters &parameters
)
{
    tr_info("Create advertising set");
    if (is_extended_advertising_available() == false) {
        tr_error("extended advertising not available");
        return BLE_ERROR_OPERATION_NOT_PERMITTED;
    }

    uint8_t new_handle = LEGACY_ADVERTISING_HANDLE + 1;
    uint8_t end = getMaxAdvertisingSetNumber();

    *handle = INVALID_ADVERTISING_HANDLE;

    for (; new_handle < end; ++new_handle) {
        if (!_existing_sets.get(new_handle)) {
            // Note: we use setAdvertisingParameters instead of
            // setExtendedAdvertisingParameters as it checks all the inputs.
            // We need to indicate the set existing before making the request
            // in case of failure we mark it back as not existing.
            _existing_sets.set(new_handle);
            ble_error_t err = setAdvertisingParameters(
                new_handle,
                parameters
            );

            if (err) {
                tr_error("could not set advertising parameters");
                _existing_sets.clear(new_handle);
            } else {
                *handle = new_handle;
            }
            return err;
        }
    }

    tr_info("Success: created advertising set %d", *handle);
    return BLE_ERROR_NO_MEM;
}
#endif
#endif


#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_EXTENDED_ADVERTISING
ble_error_t Gap::destroyAdvertisingSet(advertising_handle_t handle)
{
    tr_info("Destroy advertising set %d", handle);
    if (is_extended_advertising_available() == false) {
        tr_error("extended advertising not available");
        return BLE_ERROR_OPERATION_NOT_PERMITTED;
    }

    if (handle == LEGACY_ADVERTISING_HANDLE) {
        tr_error("handle cannot be equal to %d", LEGACY_ADVERTISING_HANDLE);
        return BLE_ERROR_INVALID_PARAM;
    }

    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("handle cannot be greater than/equal to %d", getMaxAdvertisingSetNumber());
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!_existing_sets.get(handle)) {
        tr_error("non-existent advertising set");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (_pending_sets.get(handle) || _active_sets.get(handle)) {
        tr_error("active advertising set");
        return BLE_ERROR_OPERATION_NOT_PERMITTED;
    }
#if BLE_FEATURE_PERIODIC_ADVERTISING
    if (_active_periodic_sets.get(handle)) {
        tr_error("active periodic advertising set");
        return BLE_ERROR_OPERATION_NOT_PERMITTED;
    }
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

    ble_error_t err = _pal_gap.remove_advertising_set(handle);
    if (err) {
        tr_error("PAL could not remove advertising set");
        return err;
    }

    _connectable_payload_size_exceeded.clear(handle);
    _set_is_connectable.clear(handle);
    _existing_sets.clear(handle);
    return BLE_ERROR_NONE;
}
#endif
#endif


#if BLE_ROLE_BROADCASTER
ble_error_t Gap::setAdvertisingParameters(
    advertising_handle_t handle,
    const AdvertisingParameters &params
)
{
    tr_info("Advertising set %d: set advertising parameters - "
            "_advType=%s, "
            "_minInterval=%" PRIu32 "ms, "
            "_maxInterval=%" PRIu32 "ms, "
            "_peerAddressType=%s, "
            "_ownAddressType=%s, "
            "_policy=%s, "
            "_primaryPhy=%s, "
            "_secondaryPhy=%s, "
            "_peerAddress=%s, "
            "_txPower=%d, "
            "_maxSkip=%u, "
            "_channel37=%s, "
            "_channel38=%s, "
            "_channel39=%s, "
            "_anonymous=%s, "
            "_notifyOnScan=%s, "
            "_legacyPDU=%s, "
            "_includeHeaderTxPower=%s",
            handle,
            to_string(params.getType()),
            params.getMinPrimaryInterval().valueInMs(),
            params.getMaxPrimaryInterval().valueInMs(),
            to_string(params.getPeerAddressType()),
            to_string(params.getOwnAddressType()),
            to_string(params.getFilter()),
            to_string(params.getPrimaryPhy()),
            to_string(params.getSecondaryPhy()),
            to_string(params.getPeerAddress()),
            params.getTxPower(),
            params.getSecondaryMaxSkip(),
            to_string(params.getChannel37()),
            to_string(params.getChannel38()),
            to_string(params.getChannel39()),
            to_string(params.getAnonymousAdvertising()),
            to_string(params.getScanRequestNotification()),
            to_string(params.getUseLegacyPDU()),
            to_string(params.getTxPowerInHeader()));

    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("handle cannot be greater than/equal to %d", getMaxAdvertisingSetNumber());
        return BLE_ERROR_INVALID_PARAM;
    }

    // It is not permitted to reconfigure an advertising set while advertising
    if (_pending_sets.get(handle) || _active_sets.get(handle)) {
        tr_error("active advertising set");
        return BLE_ERROR_OPERATION_NOT_PERMITTED;
    }

#if BLE_FEATURE_PRIVACY
    // If privacy is enabled, alter the own address type used during advertising
    if (_privacy_enabled && params.getOwnAddressType() != own_address_type_t::RANDOM) {
        tr_error("privacy enabled, cannot use random address");
        return BLE_ERROR_INVALID_PARAM;
    }
#endif

#if BLE_FEATURE_EXTENDED_ADVERTISING
    if (!_existing_sets.get(handle)) {
        if (handle == LEGACY_ADVERTISING_HANDLE) {
            return prepare_legacy_advertising_set(params);
        } else {
            tr_error("legacy advertising, handle must be equal to %d", LEGACY_ADVERTISING_HANDLE);
            return BLE_ERROR_INVALID_PARAM;
        }
    } else if (is_extended_advertising_available()) {
        return setExtendedAdvertisingParameters(handle, params);
    } else
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
    {
        if (handle != LEGACY_ADVERTISING_HANDLE) {
            tr_error("legacy advertising, handle must be equal to %d", LEGACY_ADVERTISING_HANDLE);
            return BLE_ERROR_INVALID_PARAM;
        }

        advertising_channel_map_t channel_map(
            params.getChannel37(),
            params.getChannel38(),
            params.getChannel39()
        );

        auto err = _pal_gap.set_advertising_parameters(
            params.getMinPrimaryInterval().value(),
            params.getMaxPrimaryInterval().value(),
            params.getType(),
            params.getOwnAddressType(),
            params.getPeerAddressType(),
            params.getPeerAddress(),
            channel_map,
            params.getFilter()
        );

        if (!err) {
            update_advertising_set_connectable_attribute(handle, params);
        } else {
            tr_error("PAL could not set advertising parameters");
        }
        return err;
    }
}
#endif

#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_EXTENDED_ADVERTISING
ble_error_t Gap::setExtendedAdvertisingParameters(
    advertising_handle_t handle,
    const AdvertisingParameters &params
)
{
    tr_info("Advertising set %d: set extended advertising parameters - "
            "_advType=%s, "
            "_minInterval=%" PRIu32 "ms, "
            "_maxInterval=%" PRIu32 "ms, "
            "_peerAddressType=%s, "
            "_ownAddressType=%s, "
            "_policy=%s, "
            "_primaryPhy=%s, "
            "_secondaryPhy=%s, "
            "_peerAddress=%s, "
            "_txPower=%d, "
            "_maxSkip=%u, "
            "_channel37=%s, "
            "_channel38=%s, "
            "_channel39=%s, "
            "_anonymous=%s, "
            "_notifyOnScan=%s, "
            "_legacyPDU=%s, "
            "_includeHeaderTxPower=%s",
            handle,
            to_string(params.getType()),
            params.getMinPrimaryInterval().valueInMs(),
            params.getMaxPrimaryInterval().valueInMs(),
            to_string(params.getPeerAddressType()),
            to_string(params.getOwnAddressType()),
            to_string(params.getFilter()),
            to_string(params.getPrimaryPhy()),
            to_string(params.getSecondaryPhy()),
            to_string(params.getPeerAddress()),
            params.getTxPower(),
            params.getSecondaryMaxSkip(),
            to_string(params.getChannel37()),
            to_string(params.getChannel38()),
            to_string(params.getChannel39()),
            to_string(params.getAnonymousAdvertising()),
            to_string(params.getScanRequestNotification()),
            to_string(params.getUseLegacyPDU()),
            to_string(params.getTxPowerInHeader()));

    /* check for illegal parameter combination */
    if ((params.getType() == advertising_type_t::CONNECTABLE_UNDIRECTED ||
         params.getType() == advertising_type_t::CONNECTABLE_DIRECTED) &&
        params.getUseLegacyPDU() == false
    ) {
        /* these types can only be used with legacy PDUs */
        tr_error("directed and undirected connections can only be used with legacy PDUs");
        return BLE_ERROR_INVALID_PARAM;
    }

    advertising_event_properties_t event_properties(params.getType());
    event_properties.include_tx_power = params.getTxPowerInHeader();
    event_properties.omit_advertiser_address = params.getAnonymousAdvertising();
    event_properties.use_legacy_pdu = params.getUseLegacyPDU();

    advertising_channel_map_t channel_map(
        params.getChannel37(),
        params.getChannel38(),
        params.getChannel39()
    );

    auto err = _pal_gap.set_extended_advertising_parameters(
        handle,
        event_properties,
        params.getMinPrimaryInterval().value(),
        params.getMaxPrimaryInterval().value(),
        channel_map,
        params.getOwnAddressType(),
        params.getPeerAddressType(),
        params.getPeerAddress(),
        params.getFilter(),
        params.getTxPower(),
        params.getPrimaryPhy(),
        params.getSecondaryMaxSkip(),
        params.getSecondaryPhy(),
        /* SID */ (handle % 0x10),
        params.getScanRequestNotification()
    );

    if (!err) {
        update_advertising_set_connectable_attribute(handle, params);
    } else {
        tr_error("PAL could not set extended advertising parameters");
    }

    return err;
}
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_BROADCASTER
ble_error_t Gap::setAdvertisingPayload(
    advertising_handle_t handle,
    Span<const uint8_t> payload
)
{
    return setAdvertisingData(
        handle,
        payload,
        /* minimise fragmentation */ true,
        /* scan response */ false
    );
}
#endif


#if BLE_ROLE_BROADCASTER
ble_error_t Gap::setAdvertisingScanResponse(
    advertising_handle_t handle,
    Span<const uint8_t> response
)
{
    return setAdvertisingData(
        handle,
        response,
        /* minimise fragmentation */ true,
        /* scan response */ true
    );
}
#endif

#if BLE_ROLE_BROADCASTER
ble_error_t Gap::setAdvertisingData(
    advertising_handle_t handle,
    Span<const uint8_t> payload,
    bool minimiseFragmentation,
    bool scan_response
)
{
    tr_info("Advertising set %d: set advertising data - "
            "payload=%s, "
            "minimiseFragmentation=%s, "
            "scan_response=%s",
            handle,
            mbed_trace_array(payload.data(), payload.size()),
            to_string(minimiseFragmentation),
            to_string(scan_response));

    // type declarations
    typedef advertising_fragment_description_t op_t;
    typedef ble_error_t (PalGap::*legacy_set_data_fn_t)(
        uint8_t,
        const advertising_data_t &
    );
    typedef ble_error_t (PalGap::*set_data_fn_t)(
        advertising_handle_t advertising_handle,
        op_t operation,
        bool minimize_fragmentation,
        uint8_t scan_response_data_size,
        const uint8_t *scan_response_data
    );

#if BLE_FEATURE_EXTENDED_ADVERTISING
    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("non-existent advertising set");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!_existing_sets.get(handle)) {
        if (handle == LEGACY_ADVERTISING_HANDLE) {
            prepare_legacy_advertising_set(AdvertisingParameters{});
        } else {
            tr_error("non-existent advertising set");
            return BLE_ERROR_INVALID_PARAM;
        }
    }

    // handle special case of legacy advertising
    if (is_extended_advertising_available() == false) {
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
        if (handle != LEGACY_ADVERTISING_HANDLE) {
            tr_error("handle must be equal to %d for legacy advertising", LEGACY_ADVERTISING_HANDLE);
            return BLE_ERROR_INVALID_PARAM;
        }

        if (payload.size() > LEGACY_ADVERTISING_MAX_SIZE) {
            tr_error("legacy advertising payload size cannot be greater than %d", LEGACY_ADVERTISING_MAX_SIZE);
            return BLE_ERROR_INVALID_PARAM;
        }

        // select the pal function
        legacy_set_data_fn_t set_data = scan_response ?
            &PalGap::set_scan_response_data :
            &PalGap::set_advertising_data;

        // set the payload
        return (_pal_gap.*set_data)(
            payload.size(),
            advertising_data_t(payload.data(), payload.size())
        );
#if BLE_FEATURE_EXTENDED_ADVERTISING
    }

    if (payload.size() > getMaxAdvertisingDataLength()) {
        tr_error("payload size cannot be greater than %u", getMaxAdvertisingDataLength());
        return BLE_ERROR_INVALID_PARAM;
    }

    if ((_pending_sets.get(handle) || _active_sets.get(handle)) && payload.size() > this->getMaxActiveSetAdvertisingDataLength()) {
        tr_error ("payload size for active sets cannot be greater than %u", getMaxActiveSetAdvertisingDataLength());
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!scan_response) {
        if (payload.size() > this->getMaxConnectableAdvertisingDataLength()) {
            if ((_pending_sets.get(handle) || _active_sets.get(handle)) && _set_is_connectable.get(handle)) {
                tr_error("payload size for connectable advertising cannot be greater %u", getMaxAdvertisingDataLength());
                return BLE_ERROR_INVALID_PARAM;
            } else {
                _connectable_payload_size_exceeded.set(handle);
            }
        } else {
            _connectable_payload_size_exceeded.clear(handle);
        }
    }

    // select the pal function
    set_data_fn_t set_data = scan_response ?
        &PalGap::set_extended_scan_response_data :
        &PalGap::set_extended_advertising_data;

    const size_t hci_length = _pal_gap.get_maximum_hci_advertising_data_length();

    for (size_t i = 0, end = payload.size(); (i < end) || (i == 0 && end == 0); i += hci_length) {
        // select the operation based on the index
        op_t op(op_t::INTERMEDIATE_FRAGMENT);
        if (end < hci_length) {
            op = op_t::COMPLETE_FRAGMENT;
        } else if (i == 0) {
            op = op_t::FIRST_FRAGMENT;
        } else if ((end - i) <= hci_length) {
            op = op_t::LAST_FRAGMENT;
        }

        // extract the payload
        Span<const uint8_t> sub_payload = payload.subspan(
            i,
            std::min(hci_length, (end - i))
        );

        // set the payload
        ble_error_t err = (_pal_gap.*set_data)(
            handle,
            op,
            minimiseFragmentation,
            sub_payload.size(),
            sub_payload.data()
        );

        if (err) {
            return err;
        }
    }
    return BLE_ERROR_NONE;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
}
#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_BROADCASTER
ble_error_t Gap::startAdvertising(
    advertising_handle_t handle,
    adv_duration_t maxDuration,
    uint8_t maxEvents
)
{
    tr_info("Advertising set %d: start advertising - "
            "maxDuration=%" PRIu32 "ms, "
            "maxEvents=%u",
            handle,
            maxDuration.valueInMs(),
            maxEvents);

    ble_error_t error = BLE_ERROR_NONE;

    // the stack is busy because it is starting, stopping or refreshing internally
    // the address.
    if (_pending_sets.get(handle) || _address_refresh_sets.get(handle)) {
        tr_error("stack busy");
        return BLE_STACK_BUSY;
    }

#if BLE_FEATURE_EXTENDED_ADVERTISING
    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("handle cannot be greater than/equal to %d", getMaxAdvertisingSetNumber());
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!_existing_sets.get(handle)) {
        tr_error("non-existent advertising set");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (_connectable_payload_size_exceeded.get(handle) && _set_is_connectable.get(handle)) {
        tr_error("payload size exceeds size allowed for connectable advertising");
        return BLE_ERROR_INVALID_STATE;
    }
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    const address_t* random_address = get_random_address(controller_operation_t::advertising, handle);
    if (!random_address) {
        tr_error("could not get random address");
        return BLE_ERROR_INVALID_STATE;
    }

#if BLE_FEATURE_EXTENDED_ADVERTISING
    if (is_extended_advertising_available()) {
        // Addresses can be updated if the set is not advertising
        if (!_active_sets.get(handle)) {
#if CORDIO_GLOBAL_RANDOM_ADDRESS_FOR_CONNECTION
            _pal_gap.set_random_address(*random_address);
#endif
            _pal_gap.set_advertising_set_random_address(handle, *random_address);
        }

        /* check we haven't run out of space to remember parameters */
        if (_advertising_enable_command_params.number_of_handles == BLE_GAP_HOST_MAX_OUTSTANDING_ADVERTISING_START_COMMANDS) {
            /* try to process early */
            tr_debug("Processing enable queue early as run out of queue space");
            process_enable_queue();
            /* if the processing didn't clear the handles we cannot continue */
            if (_advertising_enable_command_params.number_of_handles) {
                tr_warn("Cannot enable set as no memory to record the parameters");
                return BLE_ERROR_NO_MEM;
            }
        }

        /* remember the parameters that will be enabled in the delayed processing */
        const uint8_t i = _advertising_enable_command_params.number_of_handles;
        _advertising_enable_command_params.handles[i] = handle;
        _advertising_enable_command_params.max_durations[i] = maxDuration;
        _advertising_enable_command_params.max_events[i] = maxEvents;
        _advertising_enable_command_params.number_of_handles++;

        /* we delay the processing to gather as many calls as we can in one go */
        if (!_process_enable_queue_pending) {
            _process_enable_queue_pending = _event_queue.post([this] {
                process_enable_queue();
            });
        }

    } else
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
    {
        if (handle != LEGACY_ADVERTISING_HANDLE) {
            tr_error("legacy advertising, handle must be equal to %d", LEGACY_ADVERTISING_HANDLE);
            return BLE_ERROR_INVALID_PARAM;
        }

#if BLE_ROLE_OBSERVER
        // Address can be updated if the device is not scanning or advertising
        if ((_scan_state == ScanState::idle) && !_active_sets.get(LEGACY_ADVERTISING_HANDLE)) {
            _pal_gap.set_random_address(*random_address);
        } else {
            tr_error("could not update address, device scanning/advertising");
        }
#endif // BLE_ROLE_OBSERVER

        error = _pal_gap.advertising_enable(true);
        if (error) {
            tr_error("PAL could not enable advertising");
            return error;
        }

        _advertising_timeout.detach();
        if (maxDuration.value()) {
            _advertising_timeout.attach(
                mbed::callback(this, &Gap::on_advertising_timeout),
                maxDuration.valueChrono()
            );
        }

        _interruptible_sets.set(LEGACY_ADVERTISING_HANDLE);

        _pending_sets.set(handle);
    }

    return error;
}
#endif

#if BLE_FEATURE_EXTENDED_ADVERTISING
void Gap::process_enable_queue()
{
    _process_enable_queue_pending = false;

    tr_info("Evaluating pending advertising sets to be started");
    if (!_advertising_enable_command_params.number_of_handles) {
        /* no set pending to be enabled */
        tr_debug("No sets to be enabled");
        return;
    }

    for (size_t i = 0; i < BLE_GAP_MAX_ADVERTISING_SETS; ++i) {
        if (_pending_sets.get(i)) {
            /* we have to wait until nothing is pending */
            tr_info("Cannot enable sets as pending sets present, will retry later");
            return;
        }
    }

    ble_error_t error = _pal_gap.extended_advertising_enable(
        /* enable */ true,
        _advertising_enable_command_params.number_of_handles,
        _advertising_enable_command_params.handles,
        (uint16_t*)&_advertising_enable_command_params.max_durations,
        _advertising_enable_command_params.max_events
    );

    if (error) {
        tr_error("Failed to start advertising set with error: %s", to_string(error));
        if (_event_handler) {
            for (size_t i = 0; i < _advertising_enable_command_params.number_of_handles; ++i) {
                _pending_sets.clear(_advertising_enable_command_params.handles[i]);
                _event_handler->onAdvertisingStart(
                    AdvertisingStartEvent(_advertising_enable_command_params.handles[i], error)
                );
            }
        }
    } else {
        for (size_t i = 0; i < _advertising_enable_command_params.number_of_handles; ++i) {
            if (_advertising_enable_command_params.max_durations[i].value() || _advertising_enable_command_params.max_events[i]) {
                _interruptible_sets.clear(_advertising_enable_command_params.handles[i]);
            } else {
                _interruptible_sets.set(_advertising_enable_command_params.handles[i]);
            }
            _pending_sets.set(_advertising_enable_command_params.handles[i]);
        }
    }

    _advertising_enable_command_params.number_of_handles = 0;
}
#endif //BLE_FEATURE_EXTENDED_ADVERTISING

#if BLE_ROLE_BROADCASTER
ble_error_t Gap::stopAdvertising(advertising_handle_t handle)
{
    tr_info("Advertising set %d: stop advertising", handle);
    ble_error_t status;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("handle cannot be greater than/equal to %d", getMaxAdvertisingSetNumber());
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!_existing_sets.get(handle)) {
        tr_error("non-existent advertising set");
        return BLE_ERROR_INVALID_PARAM;
    }
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    if (_pending_sets.get(handle)) {
        tr_error("set pending");
        return BLE_STACK_BUSY;
    }

    if (!_active_sets.get(handle)) {
        return BLE_ERROR_NONE;
    }

#if BLE_FEATURE_EXTENDED_ADVERTISING
    if (is_extended_advertising_available()) {
        _pending_stop_sets.set(handle);
        /* delay execution of command to accumulate multiple sets */
        if (!_process_disable_queue_pending) {
            _process_disable_queue_pending = _event_queue.post([this] {
                process_disable_queue();
            });
        }

        status = BLE_ERROR_NONE;

    } else
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
    {
        if (handle != LEGACY_ADVERTISING_HANDLE) {
            tr_error("legacy advertising, handle must be equal to %d", LEGACY_ADVERTISING_HANDLE);
            return BLE_ERROR_INVALID_PARAM;
        }

        status = _pal_gap.advertising_enable(false);

        if (status) {
            return status;
        }

        _advertising_timeout.detach();

        _pending_sets.set(handle);
    }

    return status;
}

#if BLE_FEATURE_EXTENDED_ADVERTISING
void Gap::process_disable_queue()
{
    advertising_handle_t sets[BLE_GAP_MAX_ADVERTISING_SETS];
    uint8_t number_of_handles = 0;
    // refresh for address for all connectable advertising sets
    for (size_t i = 0; i < BLE_GAP_MAX_ADVERTISING_SETS; ++i) {
        if (_pending_stop_sets.get(i)) {
            sets[number_of_handles] = i;
            number_of_handles++;
            _pending_stop_sets.clear(i);
        }
    }

    if (number_of_handles) {
        ble_error_t error = _pal_gap.extended_advertising_enable(
            /* enable */ false,
             number_of_handles,
             (advertising_handle_t*)&sets,
             nullptr,
             nullptr
        );

        if (error) {
            for (size_t i = 0; i < number_of_handles; ++i) {
                _event_handler->onAdvertisingEnd(
                    AdvertisingEndEvent(
                        (advertising_handle_t)sets[i],
                        0/*connection*/,
                        0/*completed_events*/,
                        false/*connected*/,
                        error
                    )
                );
                tr_error("Could not stop advertising set %u, error: %s", i, to_string(error));
            }
        } else {
            for (size_t i = 0; i < number_of_handles; ++i) {
                _pending_sets.set(sets[i]);
            }
        }
    }

    _process_disable_queue_pending = false;
}
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

#endif


#if BLE_ROLE_BROADCASTER
bool Gap::isAdvertisingActive(advertising_handle_t handle)
{
    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("Cannot determine if periodic advertising is active: handle greater than/equal to %d", getMaxAdvertisingSetNumber());
        return false;
    }

    return _active_sets.get(handle);
}
#endif


#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::setPeriodicAdvertisingParameters(
    advertising_handle_t handle,
    periodic_interval_t periodicAdvertisingIntervalMin,
    periodic_interval_t periodicAdvertisingIntervalMax,
    bool advertiseTxPower
)
{
    tr_info("Advertising et %d: set periodic advertising parameters- "
            "periodicAdvertisingIntervalMin=%" PRIu32 "ms, "
            "periodicAdvertisingIntervalMax=%" PRIu32 "ms, "
            "advertiseTxPower=%s",
            handle,
            periodicAdvertisingIntervalMin.valueInMs(),
            periodicAdvertisingIntervalMax.valueInMs(),
            to_string(advertiseTxPower));

    if (periodicAdvertisingIntervalMin.value() > periodicAdvertisingIntervalMax.value()) {
        tr_error("min. adv. interval cannot be greater than max. adv. interval");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (handle == LEGACY_ADVERTISING_HANDLE) {
        tr_error("handle cannot be equal to %d", LEGACY_ADVERTISING_HANDLE);
        return BLE_ERROR_INVALID_PARAM;
    }

    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("handle cannot be greater than/equal to %d", getMaxAdvertisingSetNumber());
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!_existing_sets.get(handle)) {
        tr_error("non-existent advertising set");
        return BLE_ERROR_INVALID_PARAM;
    }

    return _pal_gap.set_periodic_advertising_parameters(
        handle,
        periodicAdvertisingIntervalMin.value(),
        periodicAdvertisingIntervalMax.value(),
        advertiseTxPower
    );
}
#endif
#endif


#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::setPeriodicAdvertisingPayload(
    advertising_handle_t handle,
    Span<const uint8_t> payload
)
{
    tr_info("Advertising set %d: set periodic advertising payload", handle);
#if TRACE_WRITE_VALUES
    tr_debug("payload=%s", mbed_trace_array(payload, payload.size()));
#endif
    if (handle == LEGACY_ADVERTISING_HANDLE) {
        tr_error("handle cannot be equal to %d", LEGACY_ADVERTISING_HANDLE);
        return BLE_ERROR_INVALID_PARAM;
    }

    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("handle cannot be greater than/equal %d", getMaxAdvertisingSetNumber());
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!_existing_sets.get(handle)) {
        tr_error("non-existent advertising set");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (payload.size() > getMaxAdvertisingDataLength()) {
        tr_error("payload size cannot be greater than %d", getMaxAdvertisingSetNumber());
        return BLE_ERROR_INVALID_PARAM;
    }

    typedef advertising_fragment_description_t op_t;

    const size_t hci_length = _pal_gap.get_maximum_hci_advertising_data_length();
    size_t i = 0;
    size_t end = payload.size();

    /* always do at least one iteration for empty payload */
    do {
        // select the operation based on the index
        op_t op(op_t::INTERMEDIATE_FRAGMENT);
        if (end < hci_length) {
            op = op_t::COMPLETE_FRAGMENT;
        } else if (i == 0) {
            op = op_t::FIRST_FRAGMENT;
        } else if ((end - i) <= hci_length) {
            op = op_t::LAST_FRAGMENT;
        }

        // extract the payload
        Span<const uint8_t> sub_payload = payload.subspan(
            i,
            std::min(hci_length, (end - i))
        );

        ble_error_t err = _pal_gap.set_periodic_advertising_data(
            handle,
            op,
            sub_payload.size(),
            sub_payload.data()
        );

        if (err) {
            return err;
        }

        i += hci_length;
    }
    while (i < end);

    return BLE_ERROR_NONE;
}
#endif
#endif


#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::startPeriodicAdvertising(advertising_handle_t handle)
{
    tr_info("Advertising set %d: start periodic advertising", handle);
    if (handle == LEGACY_ADVERTISING_HANDLE) {
        tr_error("handle cannot be equal to %d", LEGACY_ADVERTISING_HANDLE);
        return BLE_ERROR_INVALID_PARAM;
    }

    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("handle cannot be greater than/equal to %d", getMaxAdvertisingSetNumber());
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!_existing_sets.get(handle)) {
        tr_error("non-existent advertising set");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (_active_sets.get(handle) == false) {
        tr_error("inactive advertising set");
        return BLE_ERROR_INVALID_STATE;
    }

    if (_active_periodic_sets.get(handle) == true) {
        tr_error("active periodic advertising set");
        return BLE_ERROR_INVALID_STATE;
    }

    ble_error_t err = _pal_gap.periodic_advertising_enable(true, handle);
    if (err) {
        return err;
    }

    _active_periodic_sets.set(handle);
    return BLE_ERROR_NONE;
}
#endif
#endif


#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::stopPeriodicAdvertising(advertising_handle_t handle)
{
    tr_info("Advertising set %d: stop periodic advertising", handle);
    if (handle == LEGACY_ADVERTISING_HANDLE) {
        tr_error("handle cannot be equal to %d", handle);
        return BLE_ERROR_INVALID_PARAM;
    }

    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("handle cannot be greater than/equal to %d", getMaxAdvertisingSetNumber());
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!_existing_sets.get(handle)) {
        tr_error("non-existent advertising set");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (_active_periodic_sets.get(handle) == false) {
        tr_error("inactive advertising set");
        return BLE_ERROR_INVALID_STATE;
    }

    ble_error_t err = _pal_gap.periodic_advertising_enable(false, handle);
    if (err) {
        return err;
    }

    _active_periodic_sets.clear(handle);
    return BLE_ERROR_NONE;
}
#endif
#endif


#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_PERIODIC_ADVERTISING
bool Gap::isPeriodicAdvertisingActive(advertising_handle_t handle)
{
    if (handle >= getMaxAdvertisingSetNumber()) {
        tr_error("Can't determine if periodic advertising is active: handle greater than/equal to %d", getMaxAdvertisingSetNumber());
        return false;
    }

    return _active_periodic_sets.get(handle);
}
#endif // BLE_FEATURE_PERIODIC_ADVERTISING
#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_EXTENDED_ADVERTISING
void Gap::on_extended_advertising_report(
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
)
{
    tr_debug("Extended advertising report - "
            "event_type=["
            "connectable=%s, "
            "scannable_advertising=%s, "
            "direct_advertising=%s, "
            "scan_response=%s, "
            "legacy_advertising=%s, "
            "complete=%s, "
            "more_data_to_come=%s, "
            "truncated=%s], "
            "address_type=%s, "
            "address=%s, "
            "primary_phy=%s, "
            "secondary_phy=%s, "
            "advertising_sid=%d, "
            "tx_power=%d, "
            "rssi=%d, "
            "periodic_advertising_interval=%d, "
            "direct_address_type=%s, "
            "direct_address=%s,"
            "data_length=%d, "
            "data=%s",
            to_string(event_type.connectable()),
            to_string(event_type.scannable_advertising()),
            to_string(event_type.directed_advertising()),
            to_string(event_type.scan_response()),
            to_string(event_type.legacy_advertising()),
            to_string(event_type.complete()),
            to_string(event_type.more_data_to_come()),
            to_string(event_type.truncated()),
            to_string(*address_type),
            to_string(address),
            to_string(primary_phy),
            to_string(*secondary_phy),
            advertising_sid,
            tx_power,
            rssi,
            periodic_advertising_interval,
            to_string(direct_address_type),
            to_string(direct_address),
            data_length,
            mbed_trace_array(data, data_length));

    if (!_event_handler) {
        return;
    }

    peer_address_type_t peer_address_type = address_type ? (peer_address_type_t::type) address_type->value()
                                                         : peer_address_type_t::ANONYMOUS;

    AdvertisingReportEvent event(
        event_type,
        peer_address_type,
        address,
        primary_phy,
        secondary_phy ? *secondary_phy : phy_t::NONE,
        advertising_sid,
        tx_power,
        rssi,
        periodic_advertising_interval,
        (ble::peer_address_type_t::type) direct_address_type.value(),
        (address_t &) direct_address,
        make_Span(data, data_length)
    );

    signal_advertising_report(
        event
    );
}
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
#endif //BLE_ROLE_OBSERVER

#if BLE_FEATURE_CONNECTABLE
void Gap::report_internal_connection_complete(const ConnectionCompleteEvent& event)
{
    if (!_connection_event_handler || event.getStatus() != BLE_ERROR_NONE) {
        return;
    }

    ble::address_t address;
    if (_address_type == own_address_type_t::PUBLIC) {
        address = _pal_gap.get_device_address();
    } else {
        address = _pal_gap.get_random_address();
    }

    _connection_event_handler->on_connected(
        event.getConnectionHandle(),
        event.getOwnRole(),
        event.getPeerAddressType(),
        event.getPeerAddress(),
        _address_type,
        address
    );
}


void Gap::signal_connection_complete(
    ConnectionCompleteEvent& event
)
{
#if BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    bool address_resolved = false;

    /* if address resolution is not needed or already handled then the address is already resolved */
    if (!_privacy_enabled ||
        event.getPeerAddressType() == peer_address_type_t::PUBLIC ||
        !is_random_private_resolvable_address(event.getPeerAddress())) {
        address_resolved = true;
    } else {
#if BLE_ROLE_CENTRAL
        if (event.getOwnRole() == connection_role_t::CENTRAL) {
            if (_central_privacy_configuration.resolution_strategy == central_privacy_configuration_t::DO_NOT_RESOLVE) {
                address_resolved = true;
            }
        }
#endif // BLE_ROLE_CENTRAL

#if BLE_ROLE_PERIPHERAL
        if (event.getOwnRole() == connection_role_t::PERIPHERAL) {
            if (_peripheral_privacy_configuration.resolution_strategy ==
                peripheral_privacy_configuration_t::DO_NOT_RESOLVE) {
                address_resolved = true;
            }
        }
#endif // BLE_ROLE_PERIPHERAL
    }

    const address_t *peer_address;
    target_peer_address_type_t peer_address_type(target_peer_address_type_t::RANDOM);

    /* first try to resolve synchronously in cache */
    if (!address_resolved) {
        address_resolved = _private_address_controller.resolve_address_in_host_cache(
            event.getPeerAddress(),
            &peer_address_type,
            &peer_address
        );

        if (address_resolved) {
            conclude_signal_connection_complete_after_address_resolution(
                event,
                peer_address_type,
                peer_address
            );
            return;
        }
    }

    /* if successful then proceed to call the handler immediately same as for when privacy is disabled */
    if (address_resolved) {
        if (!apply_peripheral_privacy_connection_policy(event)) {
            return;
        }
        report_internal_connection_complete(event);
        _event_handler->onConnectionComplete(event);
    } else {
        bool resolution_pending = false;
        ble_error_t ret = _private_address_controller.queue_resolve_address_on_host(event.getPeerAddress());

        if (ret == BLE_ERROR_NONE) {
            ConnectionCompleteEvent* event_copy = new(std::nothrow) ConnectionCompleteEvent(event);
            if (!event_copy ||  !_connections_pending_address_resolution.push(event_copy)) {
                delete event_copy;
            } else {
                resolution_pending = true;
            }
        }

        if (!resolution_pending) {
            event.setStatus(BLE_ERROR_NO_MEM);
            _event_handler->onConnectionComplete(
                event
            );
            tr_info("Connection %d: disconnecting - reason=%s",
                    event.getConnectionHandle(),
                    to_string(local_disconnection_reason_t::LOW_RESOURCES));
            _pal_gap.disconnect(
                event.getConnectionHandle(),
                local_disconnection_reason_t::LOW_RESOURCES
            );
        }
    }
#else
#if BLE_FEATURE_PRIVACY
    if (!apply_peripheral_privacy_connection_policy(event)) {
        return;
    }
#endif // BLE_FEATURE_PRIVACY
    report_internal_connection_complete(event);
    _event_handler->onConnectionComplete(event);
#endif // BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
}

#if BLE_FEATURE_PRIVACY
bool Gap::apply_peripheral_privacy_connection_policy(
    const ConnectionCompleteEvent &event
)
{
#if BLE_ROLE_PERIPHERAL
    if (event.getOwnRole() != connection_role_t::PERIPHERAL) {
        return true;
    }

    if (event.getPeerAddressType() != peer_address_type_t::RANDOM) {
        return true;
    }

    if (!is_random_private_resolvable_address(event.getPeerAddress())) {
        return true;
    }

    auto connection_handle = event.getConnectionHandle();

    switch (_peripheral_privacy_configuration.resolution_strategy) {
        case peripheral_privacy_configuration_t::REJECT_NON_RESOLVED_ADDRESS:
            /* if there is no bond then allow unresolved addresses */
            if (_private_address_controller.read_resolving_list_size() == 0) {
                return true;
            }
            tr_info("Connection %d: disconnecting - reason=%s",
                    event.getConnectionHandle(),
                    to_string(local_disconnection_reason_t{local_disconnection_reason_t::AUTHENTICATION_FAILURE}));
            _pal_gap.disconnect(
                connection_handle,
                local_disconnection_reason_t::AUTHENTICATION_FAILURE
            );
            return false;

        case peripheral_privacy_configuration_t::PERFORM_PAIRING_PROCEDURE:
            _event_queue.post([connection_handle] {
                BLE::Instance().securityManager().requestAuthentication(connection_handle);
            });
            return true;

        case peripheral_privacy_configuration_t::PERFORM_AUTHENTICATION_PROCEDURE:
            _event_queue.post([connection_handle] {
                BLE::Instance().securityManager().setLinkSecurity(
                    connection_handle,
                    ble::SecurityManager::SecurityMode_t::SECURITY_MODE_ENCRYPTION_WITH_MITM
                );
            });
            return true;

        default:
            return true;
    }
#else
    return true;
#endif
}
#endif // BLE_FEATURE_PRIVACY

#if BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
void Gap::conclude_signal_connection_complete_after_address_resolution(
    ConnectionCompleteEvent &event,
    target_peer_address_type_t identity_address_type,
    const address_t *identity_address
)
{
    /* fix the event addresses */
    if (identity_address) {
        /* move old address to resolvable address */
        event.setPeerResolvablePrivateAddress(event.getPeerAddress());
        event.setLocalResolvablePrivateAddress(_private_address_controller.get_resolvable_private_address());

        event.setPeerAddress(*identity_address);
        event.setPeerAddressType(identity_address_type == target_peer_address_type_t::RANDOM ?
                                 peer_address_type_t::RANDOM_STATIC_IDENTITY
                                 : peer_address_type_t::PUBLIC_IDENTITY);
    }

    if (!apply_peripheral_privacy_connection_policy(event)) {
        return;
    }

    report_internal_connection_complete(event);
    _event_handler->onConnectionComplete(event);
}
#endif // BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
#endif // BLE_FEATURE_CONNECTABLE

#if BLE_ROLE_OBSERVER
void Gap::signal_advertising_report(
    AdvertisingReportEvent& event
)
{
#if BLE_FEATURE_PRIVACY
#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
    bool address_resolved = false;

    /* if address resolution is not needed or already handled then the address is already resolved */
    if (!_privacy_enabled ||
        _central_privacy_configuration.resolution_strategy == central_privacy_configuration_t::DO_NOT_RESOLVE ||
        event.getPeerAddressType() == peer_address_type_t::PUBLIC ||
        !is_random_private_resolvable_address(event.getPeerAddress())) {
        address_resolved = true;
    }

    /* first try to resolve synchronously in cache */
    if (!address_resolved) {
        const address_t *peer_address = nullptr;
        target_peer_address_type_t peer_address_type(target_peer_address_type_t::RANDOM);

        address_resolved = _private_address_controller.resolve_address_in_host_cache(
            event.getPeerAddress(),
            &peer_address_type,
            &peer_address
        );

        if (address_resolved) {
            conclude_signal_advertising_report_after_address_resolution(
                event,
                peer_address_type,
                peer_address
            );
            return;
        }
    }

    /* if successful then proceed to call the handler immediately same as for when privacy is disabled */
    if (address_resolved) {
        if (_connect_to_host_resolved_address_state == ConnectionToHostResolvedAddressState::scan) {
            if (_connect_to_host_resolved_address_type == event.getDirectAddressType() &&
                _connect_to_host_resolved_address == event.getDirectAddress()) {
                _connect_to_host_resolved_address_state = ConnectionToHostResolvedAddressState::connect;
                connect(
                    _connect_to_host_resolved_address_type,
                    _connect_to_host_resolved_address,
                    *_connect_to_host_resolved_address_parameters
                );
                delete _connect_to_host_resolved_address_parameters;
                _connect_to_host_resolved_address_parameters = nullptr;
            }

            /* do not report it as this was a scan run for connection purposes */
            return;
        }
    } else {
        /* check if there already is a RPA like that in the list of other pending reports */
        PendingAdvertisingReportEvent *duplicate_pending_event = _reports_pending_address_resolution.find(
            [&event](PendingAdvertisingReportEvent& pending_event){
                return (pending_event.get_pending_event().getPeerAddress() == event.getPeerAddress());
            }
        );

        ble_error_t ret = BLE_ERROR_NONE;

        /* if there is already an item with the same address pending don't kick off resolution*/
        if (!duplicate_pending_event) {
            ret = _private_address_controller.queue_resolve_address_on_host(event.getPeerAddress());
        }

        if (ret == BLE_ERROR_NONE) {
            PendingAdvertisingReportEvent *pending_event = new(std::nothrow) PendingAdvertisingReportEvent(event);

            if (pending_event) {
                if (!pending_event->is_valid() || !_reports_pending_address_resolution.push(pending_event)) {
                    delete pending_event;
                }
            }
        }

        /* we need to wait until the address is resolved before reporting it */
        return;
    }
#else
    /* filter out unresolved address if at least one bond exists */
    if (_private_address_controller.read_resolving_list_size() > 0 &&
        _central_privacy_configuration.resolution_strategy == central_privacy_configuration_t::RESOLVE_AND_FILTER &&
        event.getPeerAddressType() != peer_address_type_t::PUBLIC &&
        is_random_private_resolvable_address(event.getPeerAddress())) {
        return;
    }
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
#endif // BLE_FEATURE_PRIVACY
    _event_handler->onAdvertisingReport(
        event
    );
}
#endif //BLE_ROLE_OBSERVER

#if BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
#if BLE_ROLE_OBSERVER
void Gap::conclude_signal_advertising_report_after_address_resolution(
    AdvertisingReportEvent &event,
    target_peer_address_type_t identity_address_type,
    const address_t *identity_address
)
{
    /* fix the report with the new address if there's an identity found */
    if (identity_address) {
        const peer_address_type_t peer_address_type = (identity_address_type == target_peer_address_type_t::RANDOM) ?
                                                      peer_address_type_t::RANDOM_STATIC_IDENTITY
                                                      : peer_address_type_t::PUBLIC_IDENTITY;

        if (_connect_to_host_resolved_address_state == ConnectionToHostResolvedAddressState::scan) {
            if (_connect_to_host_resolved_address_type == peer_address_type && _connect_to_host_resolved_address == *identity_address) {
                _connect_to_host_resolved_address_state = ConnectionToHostResolvedAddressState::connect;
                connect(
                    event.getPeerAddressType(),
                    event.getPeerAddress(),
                    *_connect_to_host_resolved_address_parameters
                );
                delete _connect_to_host_resolved_address_parameters;
                _connect_to_host_resolved_address_parameters = nullptr;
                return;
            }
        }

        event.setPeerAddress(*identity_address);
        event.setPeerAddressType(peer_address_type);
    } else if (_central_privacy_configuration.resolution_strategy ==
        central_privacy_configuration_t::RESOLVE_AND_FILTER &&
        _private_address_controller.read_resolving_list_size() > 0) {
        /* filter out unresolved address if at least one bond exists */
        return;
    }

    _event_handler->onAdvertisingReport(event);
}
#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PRIVACY && BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION

#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING
void Gap::on_periodic_advertising_sync_established(
    hci_error_code_t error,
    sync_handle_t sync_handle,
    advertising_sid_t advertising_sid,
    connection_peer_address_type_t advertiser_address_type,
    const ble::address_t &advertiser_address,
    phy_t advertiser_phy,
    uint16_t periodic_advertising_interval,
    clock_accuracy_t clock_accuracy
)
{
    tr_info("Sync %d: periodic advertising sync established - "
            "error=%s, "
            "advertising_sid=%d, "
            "advertiser_address_typ=%s, "
            "advertiser_address=%s, "
            "advertiser_phy=%s, "
            "periodic_advertising_interval=%d, "
            "clock_accuracy=%d ppm",
            sync_handle,
            to_string(error),
            advertising_sid,
            to_string(advertiser_address_type),
            to_string(advertiser_address),
            to_string(advertiser_phy),
            periodic_advertising_interval,
            clock_accuracy.get_ppm());

    if (!_event_handler) {
        return;
    }

    _event_handler->onPeriodicAdvertisingSyncEstablished(
        PeriodicAdvertisingSyncEstablishedEvent(
            (error == hci_error_code_t::SUCCESS) ? BLE_ERROR_NONE : BLE_ERROR_INTERNAL_STACK_FAILURE,
            sync_handle,
            advertising_sid,
            static_cast<ble::peer_address_type_t::type>(advertiser_address_type.value()),
            advertiser_address,
            advertiser_phy,
            periodic_advertising_interval,
            clock_accuracy
        )
    );
}


void Gap::on_periodic_advertising_report(
    sync_handle_t sync_handle,
    advertising_power_t tx_power,
    rssi_t rssi,
    advertising_data_status_t data_status,
    uint8_t data_length,
    const uint8_t *data
)
{
    tr_debug("Sync %d: periodic advertising report - "
            "tx_power=%d, "
            "rssi=%d,"
            "data_status=%s, "
            "data_length=%d,"
            "data=%s",
            sync_handle,
            tx_power,
            rssi,
            to_string(data_status),
            data_length,
            mbed_trace_array(data, data_length));

    if (!_event_handler) {
        return;
    }

    _event_handler->onPeriodicAdvertisingReport(
        PeriodicAdvertisingReportEvent(
            sync_handle,
            tx_power,
            rssi,
            data_status,
            make_const_Span(data, data_length)
        )
    );
}


void Gap::on_periodic_advertising_sync_loss(sync_handle_t sync_handle)
{
    tr_info("Sync %d: periodic advertising sync loss", sync_handle);

    if (!_event_handler) {
        return;
    }

    _event_handler->onPeriodicAdvertisingSyncLoss(
        PeriodicAdvertisingSyncLoss(sync_handle)
    );
}
#endif // BLE_FEATURE_PERIODIC_ADVERTISING
#endif // BLE_ROLE_OBSERVER

#if BLE_ROLE_BROADCASTER
void Gap::on_legacy_advertising_started()
{
    tr_info("Legacy advertising started");

    _active_sets.set(LEGACY_ADVERTISING_HANDLE);
    _pending_sets.clear(LEGACY_ADVERTISING_HANDLE);

    if (_adv_started_from_refresh.get(LEGACY_ADVERTISING_HANDLE)) {
        _adv_started_from_refresh.clear(LEGACY_ADVERTISING_HANDLE);
    } else if(_event_handler) {
        _event_handler->onAdvertisingStart(
            AdvertisingStartEvent(LEGACY_ADVERTISING_HANDLE)
        );
    }
}

void Gap::on_legacy_advertising_stopped()
{
    tr_info("Legacy advertising stopped");

    _active_sets.clear(LEGACY_ADVERTISING_HANDLE);
    _pending_sets.clear(LEGACY_ADVERTISING_HANDLE);

    // restart advertising if it was stopped to refresh the address
    if (_address_refresh_sets.get(LEGACY_ADVERTISING_HANDLE)
#if BLE_ROLE_OBSERVER
        && (_scan_state == ScanState::idle)
#endif //BLE_ROLE_OBSERVER
    ) {
        _address_refresh_sets.clear(LEGACY_ADVERTISING_HANDLE);
        startAdvertising(LEGACY_ADVERTISING_HANDLE);
        _adv_started_from_refresh.set(LEGACY_ADVERTISING_HANDLE);
#if BLE_ROLE_OBSERVER
        /* scan is idle but we might still want to be scan since this could've been caused be address refresh */
        if (_scan_requested) {
            initiate_scan();
        }
#endif // BLE_ROLE_OBSERVER
    } else if (_event_handler) {
        _event_handler->onAdvertisingEnd(AdvertisingEndEvent());
    }
}

#if BLE_FEATURE_EXTENDED_ADVERTISING
void Gap::on_advertising_set_started(const mbed::Span<const uint8_t>& handles)
{
    tr_info("Advertising set started - handles=%s", mbed_trace_array(handles.data(), handles.size()));

    for (const auto &handle : handles) {
        _active_sets.set(handle);
        _pending_sets.clear(handle);
        if (_adv_started_from_refresh.get(handle)) {
            _adv_started_from_refresh.clear(handle);
        } else if (_event_handler) {
            _event_handler->onAdvertisingStart(
                AdvertisingStartEvent(handle)
            );
        }
    }

    /* delay processing to minimise churn (if multiple events are pending that would trigger it) */
    if (!_process_enable_queue_pending) {
        _process_enable_queue_pending = _event_queue.post([this] {
            process_enable_queue();
        });
    }
}

void Gap::on_advertising_set_terminated(
    hci_error_code_t status,
    advertising_handle_t advertising_handle,
    connection_handle_t connection_handle,
    uint8_t number_of_completed_extended_advertising_events
)
{
    tr_info("Advertising set %d on connection %d terminated - "
            "status=%s, "
            "number_of_completed_extended_advertising_events=%d ",
            advertising_handle,
            connection_handle,
            to_string(status),
            number_of_completed_extended_advertising_events);

    ble_error_t error_code = BLE_ERROR_UNSPECIFIED;
    bool connected = false;

    /* translate HCI error into BLE API error code */
    if (status == hci_error_code_t::SUCCESS) {
        error_code = BLE_ERROR_NONE;
        /* self cancelled set will have the handle set to invalid value */
        if (connection_handle != DM_CONN_ID_NONE) {
            connected = true;
        }
    } else if (status == hci_error_code_t::ADVERTISING_TIMEOUT) {
        error_code = BLE_ERROR_TIMEOUT;
    } else if (status == hci_error_code_t::LIMIT_REACHED) {
        error_code = BLE_ERROR_LIMIT_REACHED;
    }

    if (error_code != BLE_ERROR_UNSPECIFIED) {
        _active_sets.clear(advertising_handle);
        _pending_sets.clear(advertising_handle);

        // If this is part of the address refresh start advertising again.
        if (_address_refresh_sets.get(advertising_handle) && !connection_handle) {
            _address_refresh_sets.clear(advertising_handle);
            tr_info("Part of the address refresh, restarting advertising");
            startAdvertising(advertising_handle);
            _adv_started_from_refresh.set(advertising_handle);
            return;
        }
    }

    /* postpone as other events may still be pending */
    if (!_process_disable_queue_pending) {
        _process_disable_queue_pending = _event_queue.post([this] {
            process_disable_queue();
        });
    }

    if (!_event_handler) {
        return;
    }

    _event_handler->onAdvertisingEnd(
        AdvertisingEndEvent(
            advertising_handle,
            connection_handle,
            number_of_completed_extended_advertising_events,
            connected,
            error_code
        )
    );
}

void Gap::on_scan_request_received(
    advertising_handle_t advertising_handle,
    connection_peer_address_type_t scanner_address_type,
    const ble::address_t &address
)
{
    tr_info("Advertising set %d: scan request received - "
            "scanner_address_type=%s, "
            "address=%s",
            advertising_handle,
            to_string(scanner_address_type),
            to_string(address));

    if (!_event_handler) {
        return;
    }

    _event_handler->onScanRequestReceived(
        ScanRequestEvent(
            advertising_handle,
            (peer_address_type_t::type) scanner_address_type.value(),
            address
        )
    );
}
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
#endif // BLE_ROLE_BROADCASTER

#if BLE_FEATURE_CONNECTABLE
void Gap::on_connection_update_complete(
    hci_error_code_t status,
    connection_handle_t connection_handle,
    uint16_t connection_interval,
    uint16_t connection_latency,
    uint16_t supervision_timeout
)
{
    tr_info("Update complete for connection %d - "
            "status=%s, "
            "connection_interval=%d, "
            "connection_latency=%d, "
            "supervision_timeout=%d",
            connection_handle,
            to_string(status),
            connection_interval,
            connection_latency,
            supervision_timeout);

    if (!_event_handler) {
        return;
    }

    _event_handler->onConnectionParametersUpdateComplete(
        ConnectionParametersUpdateCompleteEvent(
            status == hci_error_code_t::SUCCESS ? BLE_ERROR_NONE : BLE_ERROR_UNSPECIFIED,
            connection_handle,
            conn_interval_t(connection_interval),
            slave_latency_t(connection_latency),
            supervision_timeout_t(supervision_timeout)
        )
    );
}

void Gap::on_remote_connection_parameter(
    connection_handle_t connection_handle,
    uint16_t connection_interval_min,
    uint16_t connection_interval_max,
    uint16_t connection_latency,
    uint16_t supervision_timeout
)
{
    tr_info("Connection %d: remote connection parameter request - "
            "connection_interval_min=%d, "
            "connection_interval_max=%d, "
            "connection_latency=%d, "
            "supervision_timeout=%d",
            connection_handle,
            connection_interval_min,
            connection_interval_max,
            connection_latency,
            supervision_timeout);

    if (_user_manage_connection_parameter_requests) {
        if (_event_handler) {
            _event_handler->onUpdateConnectionParametersRequest(
                UpdateConnectionParametersRequestEvent(
                    connection_handle,
                    conn_interval_t(connection_interval_min),
                    conn_interval_t(connection_interval_max),
                    connection_latency,
                    supervision_timeout_t(supervision_timeout))
            );
        } else {
            MBED_ERROR(illegal_state_error, "Event handler required if connection params are user handled");
        }
    } else {
        _pal_gap.accept_connection_parameter_request(
            connection_handle,
            connection_interval_min, connection_interval_max,
            connection_latency, supervision_timeout,
            /* minimum_connection_event_length */0,
            /* maximum_connection_event_length */0
        );
    }
}
#endif // BLE_FEATURE_CONNECTABLE

#if BLE_ROLE_OBSERVER
ble_error_t Gap::setScanParameters(const ScanParameters &params)
{
    if (params.get1mPhyConfiguration().isActiveScanningSet()) {
        tr_info("Set scan parameters - "
                "own_address_type=%s, "
                "scanning_filter_policy=%s, "
                "phys=%d, "
                "phy_1m_configuration:[interval=%" PRIu32 "ms,window=%" PRIu32 "ms,active_scanning=%s]",
                to_string(params.getOwnAddressType()),
                to_string(params.getFilter()),
                params.getPhys().value(),
                params.get1mPhyConfiguration().getInterval().valueInMs(),
                params.get1mPhyConfiguration().getWindow().valueInMs(),
                to_string(params.get1mPhyConfiguration().isActiveScanningSet()));
    } else {
        tr_info("Set scan parameters - "
                "own_address_type=%s, "
                "scanning_filter_policy=%s, "
                "phys=%d, "
                "phy_coded_configuration:[interval=%" PRIu32 "ms,window=%" PRIu32 "ms,active_scanning=%s]",
                to_string(params.getOwnAddressType()),
                to_string(params.getFilter()),
                params.getPhys().value(),
                params.getCodedPhyConfiguration().getInterval().valueInMs(),
                params.getCodedPhyConfiguration().getWindow().valueInMs(),
                to_string(params.getCodedPhyConfiguration().isActiveScanningSet()));
    }

    if (_privacy_enabled && params.getOwnAddressType() != own_address_type_t::RANDOM) {
        tr_error("privacy enabled, cannot use random address");
        return BLE_ERROR_INVALID_PARAM;
    }

#if BLE_FEATURE_EXTENDED_ADVERTISING
    if (is_extended_advertising_available()) {
        bool active_scanning[] = {
            params.get1mPhyConfiguration().isActiveScanningSet(),
            params.getCodedPhyConfiguration().isActiveScanningSet()
        };

        uint16_t scan_interval[] = {
            params.get1mPhyConfiguration().getInterval().value(),
            params.getCodedPhyConfiguration().getInterval().value()
        };

        uint16_t scan_window[] = {
            params.get1mPhyConfiguration().getWindow().value(),
            params.getCodedPhyConfiguration().getWindow().value()
        };

        _scan_parameters_set = true;

        return _pal_gap.set_extended_scan_parameters(
            params.getOwnAddressType(),
            params.getFilter(),
            params.getPhys(),
            active_scanning,
            scan_interval,
            scan_window
        );
    } else
#endif
    {
        if (params.getPhys().get_coded()) {
            tr_error("cannot use coded PHY with legacy configuration");
            return BLE_ERROR_INVALID_PARAM;
        }

        ScanParameters::phy_configuration_t legacy_configuration =
            params.get1mPhyConfiguration();

        _scan_parameters_set = true;

        return _pal_gap.set_scan_parameters(
            legacy_configuration.isActiveScanningSet(),
            legacy_configuration.getInterval().value(),
            legacy_configuration.getWindow().value(),
            params.getOwnAddressType(),
            params.getFilter()
        );
    }
}
#endif


#if BLE_ROLE_OBSERVER
ble_error_t Gap::startScan(
    scan_duration_t duration,
    duplicates_filter_t filtering,
    scan_period_t period
)
{
    tr_info("Start scan - "
            "duration=%" PRIu32 "ms, "
            "filtering=%s, "
            "period=%" PRIu32 "ms",
            duration.valueInMs(),
            to_string(filtering),
            period.valueInMs());

    if (_initiating) {
        tr_error("busy trying to connect");
        return BLE_STACK_BUSY;
    }

    if (!_scan_parameters_set) {
        tr_error("Scan parameters not set.");
        return BLE_ERROR_OPERATION_NOT_PERMITTED;
    }

    _scan_requested_duration = duration;
    _scan_requested_filtering = filtering;
    _scan_requested_period = period;

    /* only initiate scan if we're not already busy */
    if (_scan_state == ScanState::idle) {
        ble_error_t ret = initiate_scan();
        if (ret != BLE_ERROR_NONE) {
            return ret;
        }
    } else {
        tr_warning("Cannot start scan immediately as not idle");
    }

    _scan_requested = true;

    return BLE_ERROR_NONE;
}

ble_error_t Gap::initiate_scan()
{
    tr_info("Initiate scan");
    const address_t *address = get_random_address(controller_operation_t::scanning);
    if (!address) {
        tr_error("could not get random address");
        return BLE_ERROR_INVALID_STATE;
    }

#if BLE_FEATURE_EXTENDED_ADVERTISING
    if (is_extended_advertising_available()) {
        // set the correct mac address before starting scanning.
        if (_scan_state == ScanState::idle) {
            _pal_gap.set_random_address(*address);
        }

        ble_error_t err = _pal_gap.extended_scan_enable(
            /* enable */true,
            _scan_requested_filtering,
            _scan_requested_duration.value(),
            _scan_requested_period.value()
        );

        if (err) {
            return err;
        }
    } else
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
    {
        if (_scan_requested_period.value() != 0) {
            tr_error("scan request period cannot be equal to 0");
            return BLE_ERROR_INVALID_PARAM;
        }

        // update the address if no scan or advertising is running
        auto adv_handle = LEGACY_ADVERTISING_HANDLE;
        if ((_scan_state == ScanState::idle) && !_active_sets.get(adv_handle) && !_pending_sets.get(adv_handle)) {
            _pal_gap.set_random_address(*address);
        }

        ble_error_t err = _pal_gap.scan_enable(
            true,
            _scan_requested_filtering == duplicates_filter_t::DISABLE ? false : true
        );

        if (err) {
            return err;
        }

        _scan_timeout.detach();
        if (_scan_requested_duration.value()) {
            _scan_timeout.attach(
                [this]() { _event_queue.post([this] { process_legacy_scan_timeout(); }); },
                _scan_requested_duration.valueChrono()
            );
        }
    }

    set_scan_state(ScanState::pending_scan);

    return BLE_ERROR_NONE;
}
#endif

#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::createSync(
    peer_address_type_t peerAddressType,
    const ble::address_t &peerAddress,
    advertising_sid_t sid,
    slave_latency_t maxPacketSkip,
    sync_timeout_t timeout
)
{
    tr_info("Create sync - "
            "peerAddressType=%s, "
            "peerAddress=%s, "
            "sid=%u, "
            "maxPacketSkip=%hu, "
            "timeout=%" PRIu32 "ms",
            to_string(peerAddressType),
            to_string(peerAddress),
            sid,
            maxPacketSkip.value(),
            timeout.valueInMs());

    if (is_extended_advertising_available() == false) {
        tr_error("extended advertising not available");
        return BLE_ERROR_NOT_IMPLEMENTED;
    }

    if (peerAddressType != peer_address_type_t::PUBLIC &&
        peerAddressType != peer_address_type_t::RANDOM
        ) {
        tr_error("peer address type must be public/random");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (sid > 0x0F) {
        tr_error("sid cannot be greater than 0x0F");
        return BLE_ERROR_INVALID_PARAM;
    }

    return _pal_gap.periodic_advertising_create_sync(
        /* use advertiser list */ false,
        sid,
        (peer_address_type_t::type) peerAddressType.value(),
        peerAddress,
        maxPacketSkip.value(),
        timeout.value()
    );
}
#endif
#endif


#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::createSync(
    slave_latency_t maxPacketSkip,
    sync_timeout_t timeout
)
{
    tr_info("Create sync - "
            "maxPacketSkip=%hu, "
            "timeout=%" PRIu32 "ms",
            maxPacketSkip.value(),
            timeout.valueInMs());

    if (is_extended_advertising_available() == false) {
        tr_error("extended advertising not available");
        return BLE_ERROR_NOT_IMPLEMENTED;
    }

    return _pal_gap.periodic_advertising_create_sync(
        /* use advertiser list */ true,
        /* sid - not used */ 0x00,
        /* N/A */ peer_address_type_t::PUBLIC,
        /* N/A */ ble::address_t(),
        maxPacketSkip.value(),
        timeout.value()
    );
}
#endif
#endif


#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::cancelCreateSync()
{
    if (is_extended_advertising_available() == false) {
        tr_error("Failed to cancel create sync: extended advertising not available");
        return BLE_ERROR_NOT_IMPLEMENTED;
    }

    return _pal_gap.cancel_periodic_advertising_create_sync();
}
#endif
#endif


#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::terminateSync(periodic_sync_handle_t handle)
{
    tr_info("Teminate sync %d", handle);
    
    if (is_extended_advertising_available() == false) {
        tr_error("extended advertising not available");
        return BLE_ERROR_NOT_IMPLEMENTED;
    }

    return _pal_gap.periodic_advertising_terminate_sync(handle);
}
#endif
#endif


#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::addDeviceToPeriodicAdvertiserList(
    peer_address_type_t peerAddressType,
    const ble::address_t &peerAddress,
    advertising_sid_t sid
)
{
    tr_info("Add device to periodic advertising list - "
            "peerAddressType=%s, "
            "peerAddress=%s, "
            "sid=%u",
            to_string(peerAddressType),
            to_string(peerAddress),
            sid);

    if (is_extended_advertising_available() == false) {
        tr_error("extended advertising not available");
        return BLE_ERROR_NOT_IMPLEMENTED;
    }

    if ((peerAddressType != peer_address_type_t::PUBLIC) &&
        (peerAddressType != peer_address_type_t::RANDOM)) {
        tr_error("peer address type must be public/random");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (sid > 0x0F) {
        tr_error("sid cannot be greater than 0x0F");
        return BLE_ERROR_INVALID_PARAM;
    }

    return _pal_gap.add_device_to_periodic_advertiser_list(
        (advertising_peer_address_type_t::type) peerAddressType.value(),
        peerAddress,
        sid
    );
}
#endif
#endif


#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::removeDeviceFromPeriodicAdvertiserList(
    peer_address_type_t peerAddressType,
    const ble::address_t &peerAddress,
    advertising_sid_t sid
)
{
    tr_info("Remove device from periodic advertising list - "
            "peerAddressType=%s, "
            "peerAddress=%s, "
            "sid=%u",
            to_string(peerAddressType),
            to_string(peerAddress),
            sid);

    if (is_extended_advertising_available() == false) {
        tr_error("extended advertising not available");
        return BLE_ERROR_NOT_IMPLEMENTED;
    }

    if ((peerAddressType != peer_address_type_t::PUBLIC) &&
        (peerAddressType != peer_address_type_t::RANDOM)) {
        tr_error("peer address must be public/random");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (sid > 0x0F) {
        tr_error("set ID cannot be greater than 0x0F");
        return BLE_ERROR_INVALID_PARAM;
    }

    return _pal_gap.remove_device_from_periodic_advertiser_list(
        (advertising_peer_address_type_t::type) peerAddressType.value(),
        peerAddress,
        sid
    );
}
#endif
#endif


#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING
ble_error_t Gap::clearPeriodicAdvertiserList()
{
    if (is_extended_advertising_available() == false) {
        tr_error("Failed to clear periodic advertiser list: extended advertising not available");
        return BLE_ERROR_NOT_IMPLEMENTED;
    }

    return _pal_gap.clear_periodic_advertiser_list();
}
#endif
#endif


#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING
uint8_t Gap::getMaxPeriodicAdvertiserListSize()
{
    if (is_extended_advertising_available() == false) {
        tr_error("Failed to get max. periodic advertiser list size: extended advertising not available");
        return BLE_ERROR_NOT_IMPLEMENTED;
    }

    return _pal_gap.read_periodic_advertiser_list_size();
}
#endif
#endif


bool Gap::is_extended_advertising_available()
{
#if BLE_FEATURE_EXTENDED_ADVERTISING
    return isFeatureSupported(
        controller_supported_features_t::LE_EXTENDED_ADVERTISING
    );
#else
    return false;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
}

#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_EXTENDED_ADVERTISING
ble_error_t Gap::prepare_legacy_advertising_set(const AdvertisingParameters& parameters)
{
    if (_existing_sets.get(LEGACY_ADVERTISING_HANDLE)) {
        return BLE_ERROR_NONE;
    }

    auto err = setExtendedAdvertisingParameters(
        LEGACY_ADVERTISING_HANDLE,
        parameters
    );

    if (!err) {
        _existing_sets.set(LEGACY_ADVERTISING_HANDLE);
    }

    return err;
}
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
#endif // BLE_ROLE_BROADCASTER

void Gap::setEventHandler(Gap::EventHandler *handler)
{
    if (handler == nullptr) {
        tr_warning("Setting GAP event handler to a null pointer");
    }
    _event_handler = handler;
}

#if BLE_FEATURE_PRIVACY
void Gap::on_resolvable_private_addresses_generated(const address_t &address)
{
    on_private_address_generated(true);
}

void Gap::on_non_resolvable_private_addresses_generated(const address_t &address)
{
    on_private_address_generated(false);
}

void Gap::on_private_address_generated(bool connectable)
{
    tr_info("Private address generated - connectable=%s", to_string(connectable));

    if (!_privacy_enabled) {
        tr_error("privacy enabled, not passing to event handler");
        return;
    }

    if (_privacy_initialization_pending &&
        _private_address_controller.get_resolvable_private_address() != address_t{} &&
        _private_address_controller.get_non_resolvable_private_address() != address_t{}
    ) {
        _privacy_initialization_pending = false;
        if (_event_handler) {
            _event_handler->onPrivacyEnabled();
        }
    }

#if BLE_ROLE_BROADCASTER
    // refresh for address for all connectable advertising sets
    for (size_t i = 0; i < BLE_GAP_MAX_ADVERTISING_SETS; ++i) {
        if (!_pending_sets.get(i) && _active_sets.get(i) &&
            _set_is_connectable.get(i) == connectable && _interruptible_sets.get(i)
        ) {
            auto err = stopAdvertising(i);
            if (err) {
                continue;
            }
            _address_refresh_sets.set(i);
        }
    }
#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_OBSERVER
    /* if we are connectable and we want to use a resolvable address we want to refresh it */
    if (!_central_privacy_configuration.use_non_resolvable_random_address == connectable) {
        /* if we're scanning we need to stop */
        if (_scan_state == ScanState::scan) {
            /* but we can only stop if we're scanning forever */
            if (_scan_requested_duration == scan_duration_t::forever() && _scan_requested_period == scan_period_t(0)) {
                /* when the scan stops it will refresh the address and restart scan */
                ble_error_t err = stopScan();
                /* but after we stop we actually want to continue */
                _scan_requested = true;
            }
        }
    }
#endif // BLE_ROLE_OBSERVER
}

#if BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
void Gap::on_address_resolution_completed(
    const address_t &peer_resolvable_address,
    bool resolved,
    target_peer_address_type_t identity_address_type,
    const address_t &identity_address
)
{
    tr_info("Address resolution completed - "
            "peer_resolvable_address=%s, "
            "resolved=%s, "
            "identity_address_type=%s, "
            "identity_address=%s",
            to_string(peer_resolvable_address),
            to_string(resolved),
            to_string(identity_address_type),
            to_string(identity_address));

    if (!_event_handler || !_privacy_enabled) {
        return;
    }

#if BLE_FEATURE_CONNECTABLE
    ConnectionCompleteEvent* pending_connection_event = _connections_pending_address_resolution.pop(
        [peer_resolvable_address](ConnectionCompleteEvent &event){
            return (event.getPeerAddress() == peer_resolvable_address);
        }
    );

    if (pending_connection_event) {
        /* move old address to resolvable address */
        conclude_signal_connection_complete_after_address_resolution(
            *pending_connection_event,
            identity_address_type,
            resolved ? &identity_address : nullptr
        );

        delete pending_connection_event;

        /* we can return early, if there was a duplicate advertising report it will have its own resolution callback */
        return;
    }
#endif //BLE_FEATURE_CONNECTABLE

#if BLE_ROLE_OBSERVER
    uint8_t items_searched = 0;
    while(true) {
        PendingAdvertisingReportEvent *event = _reports_pending_address_resolution.continue_pop(
            [peer_resolvable_address](PendingAdvertisingReportEvent &event){
                return (event.get_pending_event().getPeerAddress() == peer_resolvable_address);
            },
            &items_searched
        );

        if (!event) {
            break;
        }

        conclude_signal_advertising_report_after_address_resolution(
            event->get_pending_event(),
            identity_address_type,
            resolved ? &identity_address : nullptr
        );

        delete event;
    }
#endif // BLE_ROLE_OBSERVER
}
#endif // BLE_GAP_HOST_BASED_PRIVATE_ADDRESS_RESOLUTION
#endif // BLE_FEATURE_PRIVACY

bool Gap::is_advertising() const
{
    for (size_t i = 0; i < BLE_GAP_MAX_ADVERTISING_SETS; ++i) {
        if (_pending_sets.get(i)) {
            return true;
        }
        if (_active_sets.get(i)) {
            return true;
        }
        if (_active_periodic_sets.get(i)) {
            return true;
        }
    }
    return false;
}

bool Gap::is_radio_active() const {
    return _initiating ||
#if BLE_ROLE_OBSERVER
        (_scan_state != ScanState::idle) ||
#endif // BLE_ROLE_OBSERVER
        is_advertising();
}

void Gap::update_advertising_set_connectable_attribute(
    advertising_handle_t handle,
    const AdvertisingParameters& parameters
)
{
    if (is_connectable_advertising(parameters)) {
        _set_is_connectable.set(handle);
    } else {
        _set_is_connectable.clear(handle);
    }
}

const address_t *Gap::get_random_address(controller_operation_t operation, size_t set_id)
{
    // If privacy is not enabled, then the random address is always the random static address.
#if BLE_FEATURE_PRIVACY
    if (_privacy_enabled == false)
#endif // BLE_FEATURE_PRIVACY
    {
        return &_random_static_identity_address;
    }

#if BLE_FEATURE_PRIVACY
    const auto &resolvable_address = _private_address_controller.get_resolvable_private_address();
    const auto &non_resolvable_address = _private_address_controller.get_non_resolvable_private_address();

#if BLE_ROLE_OBSERVER
    bool central_non_resolvable = _central_privacy_configuration.use_non_resolvable_random_address;
#else
    bool central_non_resolvable = false;
#endif // BLE_ROLE_OBSERVER
#if BLE_ROLE_BROADCASTER
    bool peripheral_non_resolvable = _peripheral_privacy_configuration.use_non_resolvable_random_address;
#else
    bool peripheral_non_resolvable = false;
#endif // BLE_ROLE_BROADCASTER
    const address_t *address_in_use = nullptr;
    const address_t *desired_address = nullptr;

    bool advertising_use_main_address = true;

    // Extended advertising is a special case as each set has its own address
#if !CORDIO_GLOBAL_RANDOM_ADDRESS_FOR_CONNECTION
#if BLE_FEATURE_EXTENDED_ADVERTISING
    if (is_extended_advertising_available()) {
        if (operation == controller_operation_t::advertising) {
            if (!_set_is_connectable.get(set_id) && peripheral_non_resolvable) {
                return &non_resolvable_address;
            } else {
                return &resolvable_address;
            }
        } else {
            advertising_use_main_address = false;
        }
    }
#endif
#endif

    // For other cases we first compute the address being used and then compares
    // it to the address to use to determine if the address is correct or not.
    if (_initiating) {
        address_in_use = &resolvable_address;
#if BLE_ROLE_OBSERVER
    } else if (_scan_state != ScanState::idle) {
        if (central_non_resolvable) {
            address_in_use = &non_resolvable_address;
        } else {
            address_in_use = &resolvable_address;
        }
#endif //BLE_ROLE_OBSERVER
    } else if (advertising_use_main_address && (_active_sets.get(set_id) || _pending_sets.get(set_id))) {
        if (!_set_is_connectable.get(set_id) && peripheral_non_resolvable) {
            address_in_use = &non_resolvable_address;
        } else {
            address_in_use = &resolvable_address;
        }
    } else {
        address_in_use = nullptr;
    }

    // Compute the desired address
    switch (operation) {
        case controller_operation_t::initiating:
            desired_address = &resolvable_address;
            break;
        case controller_operation_t::scanning:
            if (central_non_resolvable) {
                desired_address = &non_resolvable_address;
            } else {
                desired_address = &resolvable_address;
            }
            break;
        case controller_operation_t::advertising:
            if (!_set_is_connectable.get(set_id) && peripheral_non_resolvable) {
                desired_address = &non_resolvable_address;
            } else {
                desired_address = &resolvable_address;
            }
            break;
    }

    if (*desired_address == address_t{}) {
        return nullptr;
    }

    if (!address_in_use) {
        return desired_address;
    }

    // Request impossible to fulfill
    if (address_in_use != desired_address) {
        return nullptr;
    }

    return desired_address;
#endif // BLE_FEATURE_PRIVACY
}


} // impl
} // ble
