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

#ifndef BLE_PAL_GAP_H_
#define BLE_PAL_GAP_H_

#include "platform/Callback.h"

#include "ble/common/BLERoles.h"
#include "source/pal/GapTypes.h"
#include "source/pal/GapEvents.h"
#include "ble/common/blecommon.h"

namespace ble {

struct PalGapEventHandler {
public:

#if BLE_FEATURE_PHY_MANAGEMENT
    /**
     * @copydoc PalGap::EventHandler::onReadPhy
     */
    virtual void on_read_phy(
        hci_error_code_t status,
        connection_handle_t connectionHandle,
        ble::phy_t tx_phy,
        ble::phy_t rx_phy
    ) = 0;
#endif // BLE_FEATURE_PHY_MANAGEMENT

#if BLE_FEATURE_CONNECTABLE
    /**
     * @copydoc PalGap::EventHandler::onDataLengthChange
     */
    virtual void on_data_length_change(
        connection_handle_t connection_handle,
        uint16_t tx_size,
        uint16_t rx_size
    ) = 0;
#endif

#if BLE_FEATURE_PHY_MANAGEMENT
    /**
     * @copydoc PalGap::EventHandler::onPhyUpdateComplete
     */
    virtual void on_phy_update_complete(
        hci_error_code_t status,
        connection_handle_t connection_handle,
        ble::phy_t tx_phy,
        ble::phy_t rx_phy
     ) = 0;
#endif // BLE_FEATURE_PHY_MANAGEMENT

#if BLE_FEATURE_EXTENDED_ADVERTISING
#if BLE_ROLE_OBSERVER
    /** Called on advertising report event.
     *
     * @param event_type Type of advertising used.
     * @param address_type Peer address type of advertiser.
     * @param address Peer address of advertiser.
     * @param primary_phy PHY used on the primary channels.
     * @param secondary_phy PHY used on secondary channels.
     * @param advertising_sid Set identification number.
     * @param tx_power Transmission power reported by the packet.
     * @param rssi Measured signal strength.
     * @param periodic_advertising_interval Interval of periodic advertising.
     * @param direct_address_type Directed advertising target address type.
     * @param direct_address Directed advertising target address.
     * @param data_length Advertising payload length.
     * @param data Advertising payload.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.7.65.13 LE extended advertising
     * report event.
     */
    virtual void on_extended_advertising_report(
        advertising_event_t event_type,
        const connection_peer_address_type_t *address_type,
        const address_t &address,
        phy_t primary_phy,
        const phy_t *secondary_phy,
        advertising_sid_t advertising_sid,
        advertising_power_t tx_power,
        rssi_t rssi,
        uint16_t periodic_advertising_interval,
        direct_address_type_t direct_address_type,
        const address_t &direct_address,
        uint8_t data_length,
        const uint8_t *data_size
     ) = 0;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
#endif // BLE_ROLE_OBSERVER

#if BLE_FEATURE_PERIODIC_ADVERTISING
#if BLE_ROLE_OBSERVER
    /** Called on advertising sync event.
     *
     * @param error SUCCESS if synchronisation was achieved.
     * @param sync_handle Advertising sync handle.
     * @param advertising_sid Advertising set identifier.
     * @param advertiser_address_type Peer address type.
     * @param advertiser_address Peer address.
     * @param advertiser_phy PHY used for advertisements.
     * @param periodic_advertising_interval Periodic advertising interval.
     * @param clock_accuracy Peer clock accuracy.
     */
    virtual void on_periodic_advertising_sync_established(
        hci_error_code_t error,
        sync_handle_t sync_handle,
        advertising_sid_t advertising_sid,
        connection_peer_address_type_t advertiser_address_type,
        const ble::address_t &advertiser_address,
        phy_t advertiser_phy,
        uint16_t periodic_advertising_interval,
        clock_accuracy_t clock_accuracy
     ) = 0;

    /** Called after a periodic advertising report event.
     *
     * @param sync_handle Periodic advertising sync handle
     * @param tx_power TX power.
     * @param rssi Received signal strength.
     * @param data_status Status to indicate the completeness of the payload.
     * @param data_length Periodic advertisement payload length.
     * @param data Periodic advertisement payload.
     */
    virtual void on_periodic_advertising_report(
        sync_handle_t sync_handle,
        advertising_power_t tx_power,
        rssi_t rssi,
        advertising_data_status_t data_status,
        uint8_t data_length,
        const uint8_t *data
     ) = 0;

    /** Called on periodic advertising sync loss event.
     *
     * @param sync_handle Advertising sync handle'
     */
    virtual void on_periodic_advertising_sync_loss(
        sync_handle_t sync_handle
     ) = 0;
#endif // BLE_FEATURE_PERIODIC_ADVERTISING
#endif // BLE_ROLE_OBSERVER

#if BLE_ROLE_OBSERVER
    /**
     * Called when scanning start
     */
    virtual void on_scan_started(bool success) = 0;

    /**
     * Called when scanning stop
     */
    virtual void on_scan_stopped(bool success) = 0;


    /** Called when scanning times out.
     */
    virtual void on_scan_timeout( ) = 0;
#endif // BLE_ROLE_OBSERVER

#if BLE_ROLE_BROADCASTER
    /**
     * Called when legacy advertising has been effectively started.
     */
    virtual void on_legacy_advertising_started() = 0;

    /**
     * Called when legacy advertising has been stopped.
     */
    virtual void on_legacy_advertising_stopped() = 0;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    /**
     * Called when extended advertising has been started.
     */
    virtual void on_advertising_set_started(const mbed::Span<const uint8_t>& handles) = 0;

    /** Called when advertising set stops advertising.
     *
     * @param status SUCCESS if connection has been established or if stopped by user.
     * @param advertising_handle Advertising set handle.
     * @param advertising_handle Connection handle. Set to invalid handle if no connection made.
     * @param number_of_completed_extended_advertising_events Number of events created during before advertising end.
     */
    virtual void on_advertising_set_terminated(
        hci_error_code_t status,
        advertising_handle_t advertising_handle,
        connection_handle_t connection_handle,
        uint8_t number_of_completed_extended_advertising_events
     ) = 0;

    /** Called when a device receives a scan request from an active scanning device.
     *
     * @param advertising_handle Advertising set handle.
     * @param scanner_address_type Peer address type.
     * @param address Peer address.
     */
    virtual void on_scan_request_received(
        advertising_handle_t advertising_handle,
        connection_peer_address_type_t scanner_address_type,
        const address_t &address
     ) = 0;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
#endif // BLE_ROLE_BROADCASTER

#if BLE_FEATURE_CONNECTABLE
    virtual void on_connection_update_complete(
        hci_error_code_t status,
        connection_handle_t connection_handle,
        uint16_t connection_interval,
        uint16_t connection_latency,
        uint16_t supervision_timeout
     ) = 0;

    virtual void on_remote_connection_parameter(
        connection_handle_t connection_handle,
        uint16_t connection_interval_min,
        uint16_t connection_interval_max,
        uint16_t connection_latency,
        uint16_t supervision_timeout
     ) = 0;
#endif // BLE_FEATURE_CONNECTABLE
};

/**
 * Adaptation interface for the GAP layer.
 *
 * Define the primitive necessary to realize GAP operations. the API and event
 * follow closely the definition of the HCI commands and events used
 * by that layer.
 */
class PalGap {
public:
    /**
     * Initialisation of the instance. An implementation can use this function
     * to initialise the subsystems needed to realize the operations of this
     * interface.
     *
     * This function has to be called before any other operations.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     */
    virtual ble_error_t initialize() = 0;

    /**
     * Termination of the instance. An implementation can use this function
     * to release the subsystems initialised to realise the operations of
     * this interface.
     *
     * After a call to this function, initialise should be called again to
     * allow use of the interface.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     */
    virtual ble_error_t terminate() = 0;

    /**
     * Return the public device address.
     *
     * @note The public device address is usually acquired at initialization and
     * stored in the instance.
     *
     * @return the public device address.
     */
    virtual address_t get_device_address() = 0;

    /**
     * Return the current random address.
     *
     * @note The random address is usually acquired at initialization and stored
     * in the instance.
     *
     * @return the random device address.
     */
    virtual address_t get_random_address() = 0;

    /**
     * Set the random address which will used be during scan, connection or
     * advertising process if the own address type selected is random.
     *
     * Changing the address during scan, connection or advertising process is
     * forbiden.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.4 LE set random address command.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     */
    virtual ble_error_t set_random_address(const address_t &address) = 0;

#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_EXTENDED_ADVERTISING
    /**
     * Set the random device address used by an advertising set.
     *
     * @pre No connectable advertising set should be enabled.
     *
     * @post The new random address should be used when @p advertising_set_handle
     * is enabled.
     *
     * @param advertising_handle Handle to the advertising set that will be
     * advertised with the address @p address.
     *
     * @param address The random address to use when @p advertising_set_handle
     * is in use.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note See Bluetooth 5 Vol 2, Part E: 7.8.52 LE Set Advertising Set Random
     * Address Command
     */
    virtual ble_error_t set_advertising_set_random_address(
        advertising_handle_t advertising_handle,
        const address_t &address
    ) = 0;
#endif

    /**
     * Set the advertising parameters which will be used during the advertising
     * process.
     *
     * @param advertising_interval_min: Minimum advertising interval which can
     * be used during undirected and low duty cycle directed advertising. This
     * parameter shall be less than or equal to advertising_interval_max. This
     * parameter shall be in the range [0x20 : 0x4000] where each unit is equal
     * to 0.625ms.
     * This parameter is not used by directed high duty cycle advertising.
     *
     * @param advertising_interval_max: Maximum advertising interval which can
     * be used during undirected and low duty cycle directed advertising. This
     * parameter shall be more than or equal to advertising_interval_min. This
     * parameter shall be in the range [0x20 : 0x4000] where each unit is equal
     * to 0.625ms.
     * This parameter is not used by directed high duty cycle advertising.
     *
     * @param advertising_type Packet type that is used during the
     * advertising process. Direct advertising require valid peer addresses
     * parameters and ignore the filter policy set.
     * If the High duty cycle advertising is used then the advertising parameter
     * intervals are ignored.
     *
     * @param own_address_type Own address type used during advertising.
     * If own address type is equal to RESOLVABLE_PRIVATE_ADDRESS_PUBLIC_FALLBACK
     * or RESOLVABLE_PRIVATE_ADDRESS_RANDOM_FALLBACK then the peer address
     * parameters (type and address) will be used to find the local IRK.
     *
     * @param peer_address_type Address type of the peer.
     * This parameter shall be valid if directed advertising is used (
     * ADV_DIRECT_IND or ADV_DIRECT_IND_LOW_DUTY_CYCLE). This parameter should
     * be valid if the own address type is equal to 0x02 or 0x03.
     * In other cases, this parameter is ignored.
     *
     * @param peer_address Public device address, Random device addres, Public
     * identity address or Random static identity address of the device targeted
     * by the advertising.
     * This parameter shall be valid if directed advertising is used (
     * ADV_DIRECT_IND or ADV_DIRECT_IND_LOW_DUTY_CYCLE). This parameter should
     * be valid if the own address type is equal to 0x02 or 0x03.
     * In other cases, this parameter is ignored.
     *
     * @param advertising_channel_map Map of the channel used to send
     * advertising data.
     *
     * @param advertising_filter_policy Filter policy applied during the
     * advertising process. The subsystem should use the whitelist to apply the
     * policy. This parameter is ignored if the advertising type is directed (
     * ADV_DIRECT_IND or ADV_DIRECT_IND_LOW_DUTY_CYCLE).
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.5 LE set advertising parameters
     * command.
     */
    virtual ble_error_t set_advertising_parameters(
        uint16_t advertising_interval_min,
        uint16_t advertising_interval_max,
        advertising_type_t advertising_type,
        own_address_type_t own_address_type,
        advertising_peer_address_type_t peer_address_type,
        const address_t &peer_address,
        advertising_channel_map_t advertising_channel_map,
        advertising_filter_policy_t advertising_filter_policy
    ) = 0;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    /**
     * Define the advertising parameters of an advertising set.
     *
     * @param advertising_handle Identify the advertising set being configured.
     *
     * @param event_properties Describe the type of advertising event and its
     * properties.
     *
     * @param primary_advertising_interval_min, primary_advertising_interval_max
     * Advertising interval range for undirected and low duty cycle advertising.
     * This parameter is not considered if @p event_properties.high_duty_cycle is
     * set.
     *
     * @param primary_advertising_channel_map Map of the advertising channel to
     * use while advertising.
     *
     * @param own_address_type The type of address being used by the local
     * controller. If this parameter is a resolvable address then the peer
     * address should be an identity address.
     *
     * @param peer_address_type, peer_address Address of the peer. This parameter
     * is not evaluated if @p event_properties.directed is not set.
     *
     * @param advertising_filter_policy Filter applied to scan and connection
     * requests. It is ignored for directed advertising (@p
     * event_properties.directed set).
     *
     * @param advertising_power Maximum power level used during advertising.
     * The controller select a power level lower than or equal to this value.
     * The special value 127 should be used if the host as no preference towards
     * the transmit power used.
     *
     * @param primary_advertising_phy PHY used to transmit data on the primary
     * advertising channel. Accepted values are: phy_t::LE_1M and phy_t::LE_CODED.
     *
     * @param secondary_advertising_max_skip Maximum number of advertising events
     * that can be skipped before the AUX_ADV_IND can be sent.
     *
     * @param secondary_phy PHY used to transmit data on the secondary advertising
     * channel.
     *
     * @param advertising_sid 4bit value identifying the advertising set over the
     * air. It is not used by legacy advertising.
     *
     * @param scan_request_notification Indicate if the host wants to receive
     * scan request notification events.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.53 LE Set Extended Advertising
     * Parameters Command.
     *
     * @note If the advertising set does not exist on the controller, it is
     * created.
     *
     * @pre @p advertising_handle should not be enabled when this command is
     * issued.
     */
    virtual ble_error_t set_extended_advertising_parameters(
        advertising_handle_t advertising_handle,
        advertising_event_properties_t event_properties,
        advertising_interval_t primary_advertising_interval_min,
        advertising_interval_t primary_advertising_interval_max,
        advertising_channel_map_t primary_advertising_channel_map,
        own_address_type_t own_address_type,
        advertising_peer_address_type_t peer_address_type,
        const address_t &peer_address,
        advertising_filter_policy_t advertising_filter_policy,
        advertising_power_t advertising_power,
        phy_t primary_advertising_phy,
        uint8_t secondary_advertising_max_skip,
        phy_t secondary_phy,
        uint8_t advertising_sid,
        bool scan_request_notification
    ) = 0;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

#if BLE_FEATURE_PERIODIC_ADVERTISING
    /**
     * Configure periodic advertising parameters of an advertising set.
     *
     * @pre The advertising handle must exist on the controller @see
     * set_extended_advertising_parameters .
     *
     * @pre Periodic advertising of @p advertising_handle should not be enabled
     * when this function is called.
     *
     * @pre The advertising set identified by @p advertising_handle should not
     * be configured as anonymous advertising.
     *
     * @param advertising_handle The advertising set being configured.
     *
     * @param periodic_advertising_min, periodic_advertising_max Advertising
     * interval range for periodic advertising of this set.
     *
     * @param advertise_power If true, the transmission power should be included
     * in the advertising PDU.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.61 LE Set Periodic Advertising
     * Parameters Command
     */
    virtual ble_error_t set_periodic_advertising_parameters(
        advertising_handle_t advertising_handle,
        periodic_advertising_interval_t periodic_advertising_min,
        periodic_advertising_interval_t periodic_advertising_max,
        bool advertise_power
    ) = 0;
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

    /**
     * Set the data sends in advertising packet. If the advertising is
     * currently enabled, the data shall be used when a new advertising packet
     * is issued.
     *
     * @param advertising_data_length Number of significant bytes in the
     * advertising data.
     *
     * @param advertising_data The data sends in advertising packets. Non
     * significant bytes shall be equal to 0.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.5 LE set advertising data
     * command.
     */
    virtual ble_error_t set_advertising_data(
        uint8_t advertising_data_length,
        const advertising_data_t &advertising_data
    ) = 0;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    /**
     * Set data in advertising PDUs.
     *
     * @note this command can be called at anytime, whether the advertising set
     * is enabled or not.
     *
     * @note the extended scan response can be set in one or more command.
     *
     * @pre @p advertising_handle should exist. To create it call
     * set_extended_scan_parameters.
     *
     * @post The data inserted is used for subsequent advertising events.
     *
     * @param advertising_handle The handle identifying the advertising set
     * associated with the data.
     *
     * @param operation Description of the operation to apply to @p
     * advertising_handle data.
     *
     * @param minimize_fragmentation Hint for the controller to minimize
     * fragmentation of data sent over the air.
     *
     * @param advertising_data_size, advertising_data The data to associate to @p
     * advertising_handle.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.54 LE set extended advertising
     * data command.
     */
    virtual ble_error_t set_extended_advertising_data(
        advertising_handle_t advertising_handle,
        advertising_fragment_description_t operation,
        bool minimize_fragmentation,
        uint8_t advertising_data_size,
        const uint8_t *advertising_data
    ) = 0;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

#if BLE_FEATURE_PERIODIC_ADVERTISING
    /**
     * Set the data used in periodic advertising PDUs.
     *
     * @note the extended scan response can be set in one or more command.
     *
     * @pre @p advertising_handle must exist. See set_extended_advertising_parameters.
     *
     * @pre @p advertising_handle must be configured for periodic advertising.
     * See set_periodic_advertising_parameters .
     *
     * @param advertising_handle The handle identifying the advertising set
     * associated with the data.
     *
     * @param fragment_description Description of the operation to apply to @p
     * advertising_handle data.
     *
     * @param advertising_data_size, advertising_data The data to associate to @p
     * advertising_handle.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.62 LE set periodic advertising
     * data command.
     */
    virtual ble_error_t set_periodic_advertising_data(
        advertising_handle_t advertising_handle,
        advertising_fragment_description_t fragment_description,
        uint8_t advertising_data_size,
        const uint8_t *advertising_data
    ) = 0;
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

    /**
     * Set the data sends in scan response packets. If the advertising is
     * currently enabled, the data shall be used when a new scan response is
     * issued.
     *
     * @param scan_response_data_length Number of significant bytes in the
     * scan response data.
     *
     * @param scan_response_data The data sends in scan response packets. Non
     * significant bytes shall be equal to 0.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.8 LE set scan response data
     * command.
     */
    virtual ble_error_t set_scan_response_data(
        uint8_t scan_response_data_length,
        const advertising_data_t &scan_response_data
    ) = 0;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    /**
     * Set the data sends in extended scan response packets.  If the advertising
     * is currently enabled, the data shall be used when a new extended scan
     * response is issued.
     *
     * @note the extended scan response can be set in one or more command.
     *
     * @param advertising_handle The advertising handle that owns the scan
     * response data.
     *
     * @param operation Description of the operation to apply to @p
     * advertising_handle data.
     *
     * @param minimize_fragmentation Hint for the controller to minimize
     * fragmentation of data sent over the air.
     *
     * @param scan_response_data_size, scan_response_data The data to associate
     * to @p advertising_handle.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.55 LE set extended scan response
     * data command.
     */
    virtual ble_error_t set_extended_scan_response_data(
        advertising_handle_t advertising_handle,
        advertising_fragment_description_t operation,
        bool minimize_fragmentation,
        uint8_t scan_response_data_size,
        const uint8_t *scan_response_data
    ) = 0;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    /**
     * Start or stop advertising.
     *
     * The process use advertising and scan response data set with
     * set_advertising_data and set_scan_response_data while the parameters used
     * are the one set by set_advertising_parameters.
     *
     * The advertising shall continue until:
     *     - The advertising is manually disabled (advertising_enable(false)).
     *     - A connection is created.
     *     - Time out in case of high duty cycle directed advertising.
     *
     * If the random address has not been set and the advertising parameter
     * own_address_type is set to 0x01 then the procedure shall fail.
     *
     * If the random address has not been set and the advertising parameter
     * own_address_type is set to RESOLVABLE_PRIVATE_ADDRESS_RANDOM_FALLBACK and
     * the peer address is not in the resolving list then the procedure shall
     * fail.
     *
     * @param enable If true start the advertising process, if the process was
     * already runing and own_address_type is equal to 0x02 or 0x03, the
     * subsystem can change the random address.
     * If false and the advertising is running then the process should be
     * stoped.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.9 LE set advertising enable
     * command.
     *
     * @note If the advertising type is ADV_DIRECT_IND and the connection is not
     * created during the time allowed to the procedure then a
     * ConnectionComplete event shall be emmited with its error code set to
     * ADVERTISING_TIMEOUT.
     *
     * @note Successfull connection shall emit a ConnectionComplete event. It
     * also means advertising is disabled.
     */
    virtual ble_error_t advertising_enable(bool enable) = 0;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    /**
     * Start of stop advertising of extended advertising sets.
     *
     * @pre advertising handles present in @p handles must exist.
     *
     * @pre advertising or scan response data must be complete for advertising
     * handles in @p handles.
     *
     * @note Disabling the advertising set identified by @p handle[i] does not
     * disable any periodic advertising associated with that set.
     *
     * @param enable If true, start advertising of the advertising sets pointed
     * by @p handles. If false stop advertising for the advertising sets pointed
     * by @p handles.
     *
     * @param number_of_sets Number of advertising sets concerned by the operation.
     * @p handles, @p durations and @p max_extended_advertising_events should
     * contain @p number_of_sets elements.
     *
     * @param handles Handle of advertising sets to start or stop.
     *
     * @param durations Advertising duration for @p handle. The unit is 10ms and
     * the special value 0x000 disable advertising timeout.
     *
     * @param max_extended_advertising_events Maximum number of extended
     * advertising events the controller must attempt to send prior to
     * terminating the extended advertising. The special value 0 indicate that
     * there is no maximum.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.56 LE extended advertising enable
     * command.
     */
    virtual ble_error_t extended_advertising_enable(
        bool enable,
        uint8_t number_of_sets,
        const advertising_handle_t *handles,
        const uint16_t *durations,
        const uint8_t *max_extended_advertising_events
    ) = 0;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

#if BLE_FEATURE_PERIODIC_ADVERTISING
    /**
     * Enable or disable periodic advertising of an advertising set.
     *
     * @param enable If true enable periodic advertising of @p advertising_handle
     * otherwise disable it.
     *
     * @param advertising_handle Handle to the advertising set to enable or
     * disable.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.63 LE periodic advertising enable
     * command.
     */
    virtual ble_error_t periodic_advertising_enable(
        bool enable,
        advertising_handle_t advertising_handle
    ) = 0;
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

    /**
     * Query the maximum data length the controller supports in an advertising
     * set.
     *
     * @return The length in byte the controller can support in an advertising
     * set.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.57 LE read maximum advertising
     * data length command.
     */
    virtual uint16_t get_maximum_advertising_data_length() = 0;

    /**
     * Query the maximum data length the controller supports in an advertising set
     * using connectable advertising.
     *
     * @return The length in byte the controller can support in an advertising set
     * for connectable advertising.
     */
    virtual uint16_t get_maximum_connectable_advertising_data_length() = 0;

    /**
     * Query the maximum payload length for a single HCI packet carrying partial
     * (or complete if it fits) data for advertising set.
     *
     * @return Max size of the HCI packet transporting the data.
     */
    virtual uint8_t get_maximum_hci_advertising_data_length() = 0;

    /**
     * Query the maximum number of concurrent advertising sets that is supported
     * by the controller.
     *
     * @return The number of concurrent advertising sets supported by the
     * controller.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.58 LE number of supported
     * advertising sets command.
     */
    virtual uint8_t get_max_number_of_advertising_sets() = 0;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    /**
     * Remove an advertising set from the controller.
     *
     * After this operation, configuration and data associated with @p
     * advertising_handle are lost.
     *
     * @pre @p advertising_handle must exist.
     *
     * @pre Advertising of @p advertising_handle must be disabled.
     *
     * @param advertising_handle The advertising handle to destroy.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.59 LE remove advertising set
     * command.
     */
    virtual ble_error_t remove_advertising_set(
        advertising_handle_t advertising_handle
    ) = 0;

    /**
     * Remove all advertising sets maintained by the controller.
     *
     * @pre Advertising of all advertising sets must be disabled.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.60 LE clear advertising sets
     * command.
     */
    virtual ble_error_t clear_advertising_sets() = 0;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING
#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_OBSERVER
    /**
     * Set the parameter of the scan process.
     *
     * This command shall not be effective when the scanning process is running.
     *
     * @param active_scanning If true the subsystem does active scanning and
     * the bluetooth subsystem shall send scanning PDUs. It shall also listen
     * to scan responses. If false no scanning PDUs are sent during the scan
     * process.
     *
     * @param scan_interval The time interval between two subsequent LE scans in
     * unit of 0.625ms. This parameter shall be in the range [0x0004 : 0x4000].
     *
     * @param scan_window Duration of the LE scan. It shall be less than or
     * equal to scan_interval value. This parameter shall be in the range
     * [0x0004 : 0x4000] and is in unit of 0.625ms.
     *
     * @param own_address_type Own address type used in scan request packets.
     *
     * @param filter_policy Filter policy applied when scanning.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.10 LE set scan parameters
     * command.
     */
    virtual ble_error_t set_scan_parameters(
        bool active_scanning,
        uint16_t scan_interval,
        uint16_t scan_window,
        own_address_type_t own_address_type,
        scanning_filter_policy_t filter_policy
    ) = 0;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    /**
     * Set extended scan parameters to be used on advertising channels.
     *
     * @param own_address_type Type of the address used in scan requests.
     *
     * @param filter_policy Scanning filter policy.
     *
     * @param scanning_phys The phy affected by the parameter change. Just
     * LE_1M and LE_CODED can be set. If two phys are set then @p active_scanning,
     * @p scan_interval and @p scan_window must contain two elements. The first
     * being applied to LE_1M and the second being applied to LE_CODED.
     *
     * @param active_scanning If true, active scanning is used for the
     * correcponding PHY.
     *
     * @param scan_interval Interval between two subsequent scan. This parameter
     * shall be in the range [0x0004 : 0x4000] and is in unit of 0.625ms.
     *
     * @param scan_window Duration of the LE scan. It shall be less than or
     * equal to scan_interval value. This parameter shall be in the range
     * [0x0004 : 0x4000] and is in unit of 0.625ms.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.64 LE set extended scan parameters
     * command.
     */
    virtual ble_error_t set_extended_scan_parameters(
        own_address_type_t own_address_type,
        scanning_filter_policy_t filter_policy,
        phy_set_t scanning_phys,
        const bool *active_scanning,
        const uint16_t *scan_interval,
        const uint16_t *scan_window
    ) = 0;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    /**
     * Start/stop scanning process.
     *
     * Parameters of the scanning process shall be set before the scan launch
     * by using the function set_scan_parameters.
     *
     * @parameter enable Start the scanning process if true and stop it if
     * false. If the scan process is already started, enabling it again will
     * only update the duplicate filtering = 0; based on the new parameter.
     *
     * @parameter filter_duplicates Enable duplicate filtering if true,
     * otherwise disable it.
     *
     * @attention advertising data or scan response data is not considered
     * significant when determining duplicate advertising reports.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.11 LE set scan enable command.
     */
    virtual ble_error_t scan_enable(
        bool enable,
        bool filter_duplicates
    ) = 0;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    /**
     * Enable or disable extended scanning.
     *
     * @param enable If true enable scanning and disable it otherwise.
     *
     * @param filter_duplicates If true filter out duplicate scan reports.
     *
     * @param duration Duration of the scan operation. The unit is 10ms and
     * the special value 0 indicate continuous scan until explicitly disabled.
     *
     * @param period Interval between two scan operations. The unit is 1.28s and
     * the special value 0 can be used to disable periodic scanning. If @p
     * filter_duplicates is equal to duplicates_filter_t::PERIODIC_RESET then
     * the duplicate cache is reset whenever a scan operation starts.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.65 LE set extended scan enable
     * command.
     */
    virtual ble_error_t extended_scan_enable(
        bool enable,
        duplicates_filter_t filter_duplicates,
        uint16_t duration,
        uint16_t period
    ) = 0;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

#if BLE_FEATURE_PERIODIC_ADVERTISING
    /**
     * Synchronize an observer with a periodic advertising broadcaster.
     *
     * One the synchronization is establish the function
     * EventHandler::on_periodic_advertising_sync_established is called and for
     * every periodic advertising received is reported via the function
     * EventHandler::on_periodic_advertising_report .
     *
     * @param use_periodic_advertiser_list Determine if the periodic advertiser
     * list is used or ignored.
     *
     * @param advertising_sid SID to sync with.
     *
     * @param peer_address_type, peer_address Address of the advertiser. If @p
     * use_periodic_advertiser_list is true then these parameters are ignored.
     *
     * @param allowed_skip Number of periodic advertisement that can be skipped
     * after a successful receive by the local device.
     *
     * @param sync_timeout Specifies the maximum permitted time between two
     * successful receives. If this time is exceeded then the synchronization is
     * considered lost and EventHandler::on_periodic_advertising_loss is called.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.67 LE periodic advertising create
     * sync command.
     */
    virtual ble_error_t periodic_advertising_create_sync(
        bool use_periodic_advertiser_list,
        uint8_t advertising_sid,
        peer_address_type_t peer_address_type,
        const address_t &peer_address,
        uint16_t allowed_skip,
        uint16_t sync_timeout
    ) = 0;

    /**
     * Cancel the establishment of synchronization with a periodic advertising
     * broadcaster.
     *
     * @pre There should be an ongoing sync establishment operation.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.68 LE periodic advertising create
     * sync cancel command.
     */
    virtual ble_error_t cancel_periodic_advertising_create_sync() = 0;

    /**
     * Stop reception of the periodic advertising identified by @p sync_handle.
     *
     * @param sync_handle The handle identifying the periodic advertiser. It is
     * obtained when EventHandler::on_periodic_advertising_sync_established is
     * called.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.69 LE periodic advertising
     * terminate sync command.
     */
    virtual ble_error_t periodic_advertising_terminate_sync(
        sync_handle_t sync_handle
    ) = 0;

    /**
     * Add a device to the periodic advertiser list stored in the controller.
     * This list may be used to create a periodic advertiser sync.
     *
     * @pre The device should not be in the periodic advertiser list.
     *
     * @pre Create sync operation should not be pending
     *
     * @pre The periodic advertising list of the controller should not be full.
     *
     * @param advertiser_address_type The address type of the device to add.
     *
     * @param advertiser_address The address value of the device to add.
     *
     * @param advertising_sid The SID field used to identify periodic advertising
     * from the device.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.70 LE add device to periodic
     * advertising list command.
     */
    virtual ble_error_t add_device_to_periodic_advertiser_list(
        advertising_peer_address_type_t advertiser_address_type,
        const address_t &advertiser_address,
        uint8_t advertising_sid
    ) = 0;

    /**
     * Remove a device from the periodic advertiser list.
     *
     * @pre Create sync operation should not be pending
     *
     * @param advertiser_address_type The address type of the device to remove.
     *
     * @param advertiser_address The address value of the device to remove.
     *
     * @param advertising_sid The SID field used to identify periodic advertising
     * from the device.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.71 LE remove device from periodic
     * advertising list command.
     */
    virtual ble_error_t remove_device_from_periodic_advertiser_list(
        advertising_peer_address_type_t advertiser_address_type,
        const address_t &advertiser_address,
        uint8_t advertising_sid
    ) = 0;

    /**
     * Clear all devices from the list of periodic advertisers.
     *
     * @pre Create sync operation should not be pending
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.72 LE clear periodic advertising
     * list command.
     */
    virtual ble_error_t clear_periodic_advertiser_list() = 0;

    /**
     * Return the total number of entries that can be stored by the periodic
     * advertiser list.
     *
     * @return The capacity of the periodic advertiser list.
     *
     * @note We (wrongfully) assume that value doesn't change over time.
     */
    virtual uint8_t read_periodic_advertiser_list_size() = 0;
#endif // BLE_FEATURE_PERIODIC_ADVERTISING
#endif // BLE_ROLE_OBSERVER

#if BLE_ROLE_CENTRAL
    /**
     * Create a new le connection to a connectable advertiser.
     *
     * @param scan_interval Hint to the le subsystem indicating how
     * frequently it should scan for the peer address. It represent the time
     * interval between two subsequent scan for the peer. It shall be in the
     * range [0x0004 : 0x4000] and the time is in unit of 0.625ms.
     *
     * @param scan_window Hint to the le subsystem indicating for how long it
     * should scan during a scan interval. The value shall be smaller or equal
     * to scan_interval. If it is equal to scan_interval then scanning should
     * run continuously. It shall be in the range [0x0004 : 0x4000] and the time
     * is in unit of 0.625ms.
     *
     * @param initiator_policy Used to determine if the whitelist is used to
     * determine the address to connect to. If the whitelist is not used, the
     * connection shall be made against an advertiser matching the peer_address
     * and the peer_address_type parameters. Otherwise those parameters are
     * ignored.
     *
     * @param peer_address_type Type of address used by the advertiser. Not used
     * if initiator_policy use the whitelist.
     *
     * @param Address used by the advertiser in its advertising packets. Not
     * used if initiator_policy use the whitelist.
     *
     * @param own_address_type Type of address used in the connection request
     * packet.
     *
     * @param connection_interval_min Minimum interval between two connection
     * events allowed for the connection. It shall be less than or equal to
     * connection_interval_max. This value shall be in range [0x0006 : 0x0C80]
     * and is in unit of 1.25ms.
     *
     * @param connection_interval_max Maximum interval between two connection
     * events allowed for the connection. It shall be greater than or equal to
     * connection_interval_min. This value shall be in range [0x0006 : 0x0C80]
     * and is in unit of 1.25ms.
     *
     * @param connection_latency Number of connection events the slave can drop
     * if it has nothing to communicate to the master. This value shall be in
     * the range [0x0000 : 0x01F3].
     *
     * @param supervision_timeout Link supervision timeout for the connection.
     * It shall be larger than:
     *        (1 + connection_latency) * connection_interval_max * 2
     * Note: connection_interval_max is in ms in this formulae.
     * Everytime the master or the slave receive a valid packet from the peer,
     * the supervision timer is reset. If the supervision timer reaches
     * supervision_timeout then the connection is considered lost and a
     * disconnect event shall be emmited.
     * This value shall be in the range [0x000A : 0x0C80] and is in unit of 10
     * ms.
     *
     * @param minimum_connection_event_length Informative parameter of the
     * minimum length a connection event. It shall be less than or equal to
     * maximum_connection_event_length. It shall be in the range
     * [0x0000 : 0xFFFF]. It should also be less than the expected connection
     * interval. The unit is 0.625ms.
     *
     * @param maximum_connection_event_length Informative parameter of the
     * maximum length a connection event. It shall be more than or equal to
     * minimum_connection_event_length. It shall be in the range
     * [0x0000 : 0xFFFF]. It should also be less than the expected connection
     * interval. The unit is 0.625ms.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.12 LE create connection command.
     */
    virtual ble_error_t create_connection(
        uint16_t scan_interval,
        uint16_t scan_window,
        initiator_policy_t initiator_policy,
        connection_peer_address_type_t peer_address_type,
        const address_t &peer_address,
        own_address_type_t own_address_type,
        uint16_t connection_interval_min,
        uint16_t connection_interval_max,
        uint16_t connection_latency,
        uint16_t supervision_timeout,
        uint16_t minimum_connection_event_length,
        uint16_t maximum_connection_event_length
    ) = 0;

#if BLE_FEATURE_EXTENDED_ADVERTISING
    /**
     * Create a new le connection to a connectable advertiser.
     *
     * @param initiator_policy Used to determine if the whitelist is used to
     * determine the address to connect to. If the whitelist is not used, the
     * connection shall be made against an advertiser matching the peer_address
     * and the peer_address_type parameters. Otherwise those parameters are
     * ignored.
     *
     * @param own_address_type Type of address used in the connection request
     * packet.
     *
     * @param peer_address_type Type of address used by the advertiser. Not used
     * if initiator_policy use the whitelist.
     *
     * @param peer_address Address used by the advertiser in its advertising
     * packets. Not used if initiator_policy use the whitelist.
     *
     * @param initiating_phys Indicates the advertising PHYs and the controller
     * may use find the target and establish the connection. Subsequent
     * parameters are arrays of value associated with phys present in @p
     * initiating_phys. The order of phys is 1M, 2M, Coded.
     *
     * @param scan_intervals Hint to the le subsystem indicating how
     * frequently it should scan for the peer address. It represent the time
     * interval between two subsequent scan for the peer. It shall be in the
     * range [0x0004 : 0x4000] and the time is in unit of 0.625ms.
     *
     * @param scan_windows Hint to the le subsystem indicating for how long it
     * should scan during a scan interval. The value shall be smaller or equal
     * to scan_interval. If it is equal to scan_interval then scanning should
     * run continuously. It shall be in the range [0x0004 : 0x4000] and the time
     * is in unit of 0.625ms.
     *
     * @param connection_intervals_min Minimum interval between two connection
     * events allowed for the connection. It shall be less than or equal to
     * connection_interval_max. This value shall be in range [0x0006 : 0x0C80]
     * and is in unit of 1.25ms.
     *
     * @param connection_intervals_max Maximum interval between two connection
     * events allowed for the connection. It shall be greater than or equal to
     * connection_interval_min. This value shall be in range [0x0006 : 0x0C80]
     * and is in unit of 1.25ms.
     *
     * @param connection_latencies Number of connection events the slave can drop
     * if it has nothing to communicate to the master. This value shall be in
     * the range [0x0000 : 0x01F3].
     *
     * @param supervision_timeouts Link supervision timeout for the connection.
     * It shall be larger than:
     *        (1 + connection_latency) * connection_interval_max * 2
     * Note: connection_interval_max is in ms in this formulae.
     * Everytime the master or the slave receive a valid packet from the peer,
     * the supervision timer is reset. If the supervision timer reaches
     * supervision_timeout then the connection is considered lost and a
     * disconnect event shall be emmited.
     * This value shall be in the range [0x000A : 0x0C80] and is in unit of 10
     * ms.
     *
     * @param minimum_connection_event_lengths Informative parameter of the
     * minimum length a connection event. It shall be less than or equal to
     * maximum_connection_event_length. It shall be in the range
     * [0x0000 : 0xFFFF]. It should also be less than the expected connection
     * interval. The unit is 0.625ms.
     *
     * @param maximum_connection_event_lengths Informative parameter of the
     * maximum length a connection event. It shall be more than or equal to
     * minimum_connection_event_length. It shall be in the range
     * [0x0000 : 0xFFFF]. It should also be less than the expected connection
     * interval. The unit is 0.625ms.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.66 LE extended create connection
     * command.
     */
    virtual ble_error_t extended_create_connection(
        initiator_policy_t initiator_policy,
        own_address_type_t own_address_type,
        peer_address_type_t peer_address_type,
        const address_t &peer_address,
        phy_set_t initiating_phys,
        const uint16_t *scan_intervals,
        const uint16_t *scan_windows,
        const uint16_t *connection_intervals_min,
        const uint16_t *connection_intervals_max,
        const uint16_t *connection_latencies,
        const uint16_t *supervision_timeouts,
        const uint16_t *minimum_connection_event_lengths,
        const uint16_t *maximum_connection_event_lengths
    ) = 0;
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    /**
     * Cancel the ongoing connection creation process.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.13 LE create connection cancel
     * command.
     */
    virtual ble_error_t cancel_connection_creation() = 0;
#endif // BLE_ROLE_CENTRAL

#if BLE_FEATURE_WHITELIST
    /**
     * Return the number of total whitelist entries that can be stored in the
     * le subsystem.
     *
     * @note The value returned can change over time.
     *
     * @return The number of entries that can be stored in the LE subsystem. It
     * range from 0x01 to 0xFF.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.14 LE read white list size
     * command.
     */
    virtual uint8_t read_white_list_capacity() = 0;

    /**
     * Clear the whitelist stored in the LE subsystem.
     *
     * @attention This command shall not be issued if the whitelist is being
     * used by the advertising, scanning or connection creation procedure.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.15 LE clear white list command.
     */
    virtual ble_error_t clear_whitelist() = 0;

    /**
     * Add a device to the LE subsystem Whitelist.
     *
     * @param address_type address_type Type of the address to add in the
     * whitelist.
     *
     * @param address Address of the device.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @attention This command shall not be issued if the whitelist is being
     * used by the advertising, scanning or connection creation procedure.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.16 LE add device to white list
     * command.
     */
    virtual ble_error_t add_device_to_whitelist(
        whitelist_address_type_t address_type,
        address_t address
    ) = 0;

    /**
     * Remove a device from the LE subsystem Whitelist.
     *
     * @param address_type address_type Type of the address of the device to
     * remove from the whitelist.
     *
     * @param address Address of the device to remove from the whitelist
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @attention This command shall not be issued if the whitelist is being
     * used by the advertising, scanning or connection creation procedure.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.17 LE remove device from white
     * list command.
     */
    virtual ble_error_t remove_device_from_whitelist(
        whitelist_address_type_t address_type,
        address_t address
    ) = 0;
#endif // BLE_FEATURE_WHITELIST

#if BLE_FEATURE_CONNECTABLE
    /**
     * Start a connection update procedure.
     *
     * This procedure change the parameter used for a connection it can be
     * master or slave initiated.
     *
     * The peer will received a connection parameters request and will either
     * accept or reject the new parameters for the connection.
     *
     * Once the peer response has been received, the procedure ends and a
     * Connection update complete event is emmited.
     *
     * @param connection Handle of the connection.
     *
     * @param connection_interval_min Minimum interval between two connection
     * events allowed for the connection. It shall be less than or equal to
     * connection_interval_max. This value shall be in range [0x0006 : 0x0C80]
     * and is in unit of 1.25ms.
     *
     * @param connection_interval_max Maximum interval between two connection
     * events allowed for the connection. It shall be greater than or equal to
     * connection_interval_min. This value shall be in range [0x0006 : 0x0C80]
     * and is in unit of 1.25ms.
     *
     * @param connection_latency Number of connection events the slave can drop
     * if it has nothing to communicate to the master. This value shall be in
     * the range [0x0000 : 0x01F3].
     *
     * @param supervision_timeout Link supervision timeout for the connection.
     * It shall be larger than:
     *        (1 + connection_latency) * connection_interval_max * 2
     * Note: connection_interval_max is in ms in this formulae.
     * Everytime the master or the slave receive a valid packet from the peer,
     * the supervision timer is reset. If the supervision timer reaches
     * supervision_timeout then the connection is considered lost and a
     * disconnect event shall be emmited.
     * This value shall be in the range [0x000A : 0x0C80] and is in unit of 10
     * ms.
     *
     * @param minimum_connection_event_length Informative parameter of the
     * minimum length a connection event. It shall be less than or equal to
     * maximum_connection_event_length. It shall be in the range
     * [0x0000 : 0xFFFF]. It should also be less than the expected connection
     * interval. The unit is 0.625ms.
     *
     * @param maximum_connection_event_length Informative parameter of the
     * maximum length a connection event. It shall be more than or equal to
     * minimum_connection_event_length. It shall be in the range
     * [0x0000 : 0xFFFF]. It should also be less than the expected connection
     * interval. The unit is 0.625ms.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.18 LE Connection update command.
     *
     */
    virtual ble_error_t connection_parameters_update(
        connection_handle_t connection,
        uint16_t connection_interval_min,
        uint16_t connection_interval_max,
        uint16_t connection_latency,
        uint16_t supervision_timeout,
        uint16_t minimum_connection_event_length,
        uint16_t maximum_connection_event_length
    ) = 0;

    /**
     * Accept connection parameter request.
     *
     * This command sends a positive response to a connection parameter request
     * from a peer.
     *
     * @param connection Handle of the connection.
     *
     * @param connection_interval_min Minimum interval between two connection
     * events allowed for the connection. It shall be less than or equal to
     * connection_interval_max. This value shall be in range [0x0006 : 0x0C80]
     * and is in unit of 1.25ms.
     *
     * @param connection_interval_max Maximum interval between two connection
     * events allowed for the connection. It shall be greater than or equal to
     * connection_interval_min. This value shall be in range [0x0006 : 0x0C80]
     * and is in unit of 1.25ms.
     *
     * @param connection_latency Number of connection events the slave can drop
     * if it has nothing to communicate to the master. This value shall be in
     * the range [0x0000 : 0x01F3].
     *
     * @param supervision_timeout Link supervision timeout for the connection.
     * It shall be larger than:
     *        (1 + connection_latency) * connection_interval_max * 2
     * Note: connection_interval_max is in ms in this formulae.
     * Everytime the master or the slave receive a valid packet from the peer,
     * the supervision timer is reset. If the supervision timer reaches
     * supervision_timeout then the connection is considered lost and a
     * disconnect event shall be emmited.
     * This value shall be in the range [0x000A : 0x0C80] and is in unit of 10
     * ms.
     *
     * @param minimum_connection_event_length Informative parameter of the
     * minimum length a connection event. It shall be less than or equal to
     * maximum_connection_event_length. It shall be in the range
     * [0x0000 : 0xFFFF]. It should also be less than the expected connection
     * interval. The unit is 0.625ms.
     *
     * @param maximum_connection_event_length Informative parameter of the
     * maximum length a connection event. It shall be more than or equal to
     * minimum_connection_event_length. It shall be in the range
     * [0x0000 : 0xFFFF]. It should also be less than the expected connection
     * interval. The unit is 0.625ms.
     *
     * @note Usually parameters of this function match the connection parameters
     * received in the connection parameter request event.
     *
     * @attention: Once the new connection parameters are in used a Connection
     * Update Complete event shall be emmited.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.31 LE remote connection parameter
     * request reply command.
     */
    virtual ble_error_t accept_connection_parameter_request(
        connection_handle_t connection_handle,
        uint16_t interval_min,
        uint16_t interval_max,
        uint16_t latency,
        uint16_t supervision_timeout,
        uint16_t minimum_connection_event_length,
        uint16_t maximum_connection_event_length
    ) = 0;

    /**
     * Reject a connection parameter update request.
     *
     * @param connection_handle handle to the peer which has issued the
     * connection parameter request.
     *
     * @param rejection_reason Indicate to the peer why the proposed connection
     * parameters were rejected.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.8.32 LE Remote Connection Parameter
     * Request Negative Reply Command.
     */
    virtual ble_error_t reject_connection_parameter_request(
        connection_handle_t connection_handle,
        hci_error_code_t rejection_reason
    ) = 0;

    /**
     * Start a disconnection procedure.
     *
     * Once the procedure is complete it should emit a disconnection complete
     * event.
     *
     * @param connection Handle of the connection to terminate.
     *
     * @param disconnection_reason Indicates the reason for ending the
     * connection.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     *
     * @note: See Bluetooth 5 Vol 2 PartE: 7.1.6 disconenct command.
     */
    virtual ble_error_t disconnect(
        connection_handle_t connection,
        local_disconnection_reason_t disconnection_reason
    ) = 0;
#endif

    /**
     * Checked support for a feature in the link controller.
     *
     * @param feature feature to be checked.
     * @return TRUE if feature is supported.
     */
    virtual bool is_feature_supported(
        controller_supported_features_t feature
    ) = 0;

#if BLE_FEATURE_PHY_MANAGEMENT
    /**
    * @see PalGap::readPhy
    */
    virtual ble_error_t read_phy(connection_handle_t connection) = 0;

    /**
    * @see PalGap::setPreferredPhys
    */
    virtual ble_error_t set_preferred_phys(
        const phy_set_t &tx_phys,
        const phy_set_t &rx_phys
    ) = 0;

    /**
    * @see PalGap::setPhy
    */
    virtual ble_error_t set_phy(
        connection_handle_t connection,
        const phy_set_t &tx_phys,
        const phy_set_t &rx_phys,
        coded_symbol_per_bit_t coded_symbol
    ) = 0;
#endif // BLE_FEATURE_PHY_MANAGEMENT

    /**
     * Register a callback which will handle PalGap events.
     *
     * @param cb The callback object which will handle PalGap events from the
     * LE subsystem.
     * It accept a single parameter in input: The event received.
     */
    virtual void when_gap_event_received(mbed::Callback<void(const GapEvent &)> cb) = 0;

    /**
    * Sets the event handler that us called by the PAL porters to notify the stack of events
    * which will in turn be passed onto the user application when appropriate.
    *
    * @param[in] event_handler the new event handler interface implementation. Memory
    * owned by caller who is responsible for updating this pointer if interface changes.
    */
    virtual void set_event_handler(PalGapEventHandler *event_handler) = 0;

    /**
     * Return the registered event handler.
     * @return Pointer to event handler
     */
    virtual PalGapEventHandler *get_event_handler() = 0;
};

} // namespace ble

#endif /* BLE_PAL_GAP_H_ */
