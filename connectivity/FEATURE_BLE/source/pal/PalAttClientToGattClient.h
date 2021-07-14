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

#ifndef BLE_PAL_ATTCLIENTTOGATTCLIENTADAPTER_H_
#define BLE_PAL_ATTCLIENTTOGATTCLIENTADAPTER_H_

#include "source/pal/PalGattClient.h"

namespace ble {

class PalAttClient;

/**
 * Adapt a PalAttClient into a PalGattClient.
 *
 * This class let vendors define their abstraction layer in term of an PalAttClient
 * and adapt any PalAttClient into a PalGattClient.
 */
class PalAttClientToGattClient : public PalGattClient {
public:
    static const uint16_t END_ATTRIBUTE_HANDLE = 0xFFFF;
    static const uint16_t SERVICE_TYPE_UUID = 0x2800;
    static const uint16_t INCLUDE_TYPE_UUID = 0x2802;
    static const uint16_t CHARACTERISTIC_TYPE_UUID = 0x2803;

    /**
     * Construct an instance of PalGattClient from an instance of PalAttClient.
     * @param client The client to adapt.
     */
    PalAttClientToGattClient(PalAttClient& client);

    /**
     * @see ble::PalGattClient::exchange_mtu
     */
    ble_error_t exchange_mtu(connection_handle_t connection) override;

    /**
     * @see ble::PalGattClient::get_mtu_size
     */
    ble_error_t get_mtu_size(
        connection_handle_t connection_handle,
        uint16_t& mtu_size
    ) override;

    /**
     * @see ble::PalGattClient::discover_primary_service
     */
    ble_error_t discover_primary_service(
        connection_handle_t connection,
        attribute_handle_t discovery_range_begining
    ) override;

    /**
     * @see ble::PalGattClient::discover_primary_service_by_service_uuid
     */
    ble_error_t discover_primary_service_by_service_uuid(
        connection_handle_t connection_handle,
        attribute_handle_t discovery_range_begining,
        const UUID& uuid
    ) override;

    /**
     * @see ble::PalGattClient::find_included_service
     */
    ble_error_t find_included_service(
        connection_handle_t connection_handle,
        attribute_handle_range_t service_range
    ) override;

    /**
     * @see ble::PalGattClient::discover_characteristics_of_a_service
     */
    ble_error_t discover_characteristics_of_a_service(
        connection_handle_t connection_handle,
        attribute_handle_range_t discovery_range
    ) override;

    /**
     * @see ble::PalGattClient::discover_characteristics_descriptors
     */
    ble_error_t discover_characteristics_descriptors(
        connection_handle_t connection_handle,
        attribute_handle_range_t descriptors_discovery_range
    ) override;

    /**
     * @see ble::PalGattClient::read_attribute_value
     */
    ble_error_t read_attribute_value(
        connection_handle_t connection_handle,
        attribute_handle_t attribute_handle
    ) override;

    /**
     * @see ble::PalGattClient::read_using_characteristic_uuid
     */
    ble_error_t read_using_characteristic_uuid(
        connection_handle_t connection_handle,
        attribute_handle_range_t read_range,
        const UUID& uuid
    ) override;

    /**
     * @see ble::PalGattClient::read_attribute_blob
     */
    ble_error_t read_attribute_blob(
        connection_handle_t connection_handle,
        attribute_handle_t attribute_handle,
        uint16_t offset
    ) override;

    /**
     * @see ble::PalGattClient::read_multiple_characteristic_values
     */
    ble_error_t read_multiple_characteristic_values(
        connection_handle_t connection_handle,
        const Span<const attribute_handle_t>& characteristic_value_handles
    ) override;

    /**
     * @see ble::PalGattClient::write_without_response
     */
    ble_error_t write_without_response(
        connection_handle_t connection_handle,
        attribute_handle_t characteristic_value_handle,
        const Span<const uint8_t>& value
    ) override;
#if BLE_FEATURE_SIGNING
    /**
     * @see ble::PalGattClient::signed_write_without_response
     */
    ble_error_t signed_write_without_response(
        connection_handle_t connection_handle,
        attribute_handle_t characteristic_value_handle,
        const Span<const uint8_t>& value
    ) override;
#endif //BLE_FEATURE_SIGNING
    /**
     * @see ble::PalGattClient::write_attribute
     */
    ble_error_t write_attribute(
        connection_handle_t connection_handle,
        attribute_handle_t attribute_handle,
        const Span<const uint8_t>& value
    ) override;

    /**
     * @see ble::PalGattClient::queue_prepare_write
     */
    ble_error_t queue_prepare_write(
        connection_handle_t connection_handle,
        attribute_handle_t characteristic_value_handle,
        const Span<const uint8_t>& value,
        uint16_t offset
    ) override;

    /**
     * @see ble::PalGattClient::execute_write_queue
     */
    ble_error_t execute_write_queue(
        connection_handle_t connection_handle,
        bool execute
    ) override;

    /**
     * @see ble::PalGattClient::initialize
     */
    ble_error_t initialize() override;

    /**
     * @see ble::PalGattClient::terminate
     */
    ble_error_t terminate() override;

    /**
     * @see ble::PalGattClient::when_server_message_received
     */
    void when_server_message_received(
        mbed::Callback<void(connection_handle_t, const AttServerMessage&)> cb
    ) override {
        _server_message_cb = cb;
    }

    /**
     * @see ble::PalGattClient::when_transaction_timeout
     */
    void when_transaction_timeout(
        mbed::Callback<void(connection_handle_t)> cb
    ) override {
        _transaction_timeout_cb = cb;
    }

    /**
     * @see ble::PalGattClient::set_event_handler
     */
    void set_event_handler(PalGattClientEventHandler* event_handler) override {
        _event_handler = event_handler;
    }

    /**
     * @see ble::PalGattClient::get_event_handler
     */
    PalGattClientEventHandler* get_event_handler() override {
        return _event_handler;
    }

private:
    /**
     * Upon server message reception an implementation shall call this function.
     *
     * @param connection_handle The handle of the connection which has received
     * the server message.
     * @param server_message The message received from the server.
     */
    void on_server_event(
        connection_handle_t connection_handle,
        const AttServerMessage& server_message
    ) {
        if (_server_message_cb) {
            _server_message_cb(connection_handle, server_message);
        }
    }

    /**
     * Upon transaction timeout an implementation shall call this function.
     *
     * @param connection_handle The handle of the connection of the transaction
     * which has times out.
     *
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part F Section 3.3.3
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.4.14
     */
    void on_transaction_timeout(
        connection_handle_t connection_handle
    ) {
        if (_transaction_timeout_cb) {
            _transaction_timeout_cb(connection_handle);
        }
    }

    PalGattClientEventHandler* _event_handler;

    /**
     * Callback called when the client receive a message from the server.
     */
    mbed::Callback<void(connection_handle_t, const AttServerMessage&)> _server_message_cb;

    /**
     * Callback called when a transaction times out.
     */
    mbed::Callback<void(connection_handle_t)> _transaction_timeout_cb;

    PalAttClient& _client;
};

} // namespace ble

#endif /* BLE_PAL_ATTCLIENTTOGATTCLIENTADAPTER_H_ */
