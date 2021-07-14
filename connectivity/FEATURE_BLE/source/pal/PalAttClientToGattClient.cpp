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

#include "source/pal/PalGattClient.h"
#include "source/pal/PalAttClient.h"
#include "source/pal/PalAttClientToGattClient.h"

namespace ble {

PalAttClientToGattClient::PalAttClientToGattClient(PalAttClient& client) : _event_handler(nullptr), _client(client)
{
    _client.when_server_message_received(
        mbed::callback(this, &PalAttClientToGattClient::on_server_event)
    );
    _client.when_transaction_timeout(
        mbed::callback(
            this, &PalAttClientToGattClient::on_transaction_timeout
        )
    );
}


ble_error_t PalAttClientToGattClient::exchange_mtu(connection_handle_t connection)
{
    return _client.exchange_mtu_request(connection);
}


ble_error_t PalAttClientToGattClient::get_mtu_size(
    connection_handle_t connection_handle,
    uint16_t& mtu_size
)
{
    return _client.get_mtu_size(connection_handle, mtu_size);
}


ble_error_t PalAttClientToGattClient::discover_primary_service(
    connection_handle_t connection,
    attribute_handle_t discovery_range_begining
)
{
    return _client.read_by_group_type_request(
        connection,
        attribute_handle_range(discovery_range_begining, END_ATTRIBUTE_HANDLE),
        SERVICE_TYPE_UUID
    );
}


ble_error_t PalAttClientToGattClient::discover_primary_service_by_service_uuid(
    connection_handle_t connection_handle,
    attribute_handle_t discovery_range_begining,
    const UUID& uuid
)
{
    return _client.find_by_type_value_request(
        connection_handle,
        attribute_handle_range(discovery_range_begining, END_ATTRIBUTE_HANDLE),
        SERVICE_TYPE_UUID,
        Span<const uint8_t>(
            uuid.getBaseUUID(),
            (uuid.shortOrLong() == UUID::UUID_TYPE_SHORT) ? 2 : UUID::LENGTH_OF_LONG_UUID
        )
    );
}


ble_error_t PalAttClientToGattClient::find_included_service(
    connection_handle_t connection_handle,
    attribute_handle_range_t service_range
)
{
    return _client.read_by_type_request(
        connection_handle,
        service_range,
        INCLUDE_TYPE_UUID
    );
}


ble_error_t PalAttClientToGattClient::discover_characteristics_of_a_service(
    connection_handle_t connection_handle,
    attribute_handle_range_t discovery_range
)
{
    return _client.read_by_type_request(
        connection_handle,
        discovery_range,
        CHARACTERISTIC_TYPE_UUID
    );
}


ble_error_t PalAttClientToGattClient::discover_characteristics_descriptors(
    connection_handle_t connection_handle,
    attribute_handle_range_t descriptors_discovery_range
)
{
    return _client.find_information_request(
        connection_handle,
        descriptors_discovery_range
    );
}


ble_error_t PalAttClientToGattClient::read_attribute_value(
    connection_handle_t connection_handle,
    attribute_handle_t attribute_handle
)
{
    return _client.read_request(
        connection_handle,
        attribute_handle
    );
}


ble_error_t PalAttClientToGattClient::read_using_characteristic_uuid(
    connection_handle_t connection_handle,
    attribute_handle_range_t read_range,
    const UUID& uuid
)
{
    return _client.read_by_type_request(
        connection_handle,
        read_range,
        uuid
    );
}


ble_error_t PalAttClientToGattClient::read_attribute_blob(
    connection_handle_t connection_handle,
    attribute_handle_t attribute_handle,
    uint16_t offset
)
{
    return _client.read_blob_request(
        connection_handle,
        attribute_handle,
        offset
    );
}


ble_error_t PalAttClientToGattClient::read_multiple_characteristic_values(
    connection_handle_t connection_handle,
    const Span<const attribute_handle_t>& characteristic_value_handles
)
{
    return _client.read_multiple_request(
        connection_handle,
        characteristic_value_handles
    );
}


ble_error_t PalAttClientToGattClient::write_without_response(
    connection_handle_t connection_handle,
    attribute_handle_t characteristic_value_handle,
    const Span<const uint8_t>& value
)
{
    return _client.write_command(
        connection_handle,
        characteristic_value_handle,
        value
    );
}

#if BLE_FEATURE_SIGNING
ble_error_t PalAttClientToGattClient::signed_write_without_response(
    connection_handle_t connection_handle,
    attribute_handle_t characteristic_value_handle,
    const Span<const uint8_t>& value
)
{
    return _client.signed_write_command(
        connection_handle,
        characteristic_value_handle,
        value
    );
}
#endif // BLE_FEATURE_SIGNING

ble_error_t PalAttClientToGattClient::write_attribute(
    connection_handle_t connection_handle,
    attribute_handle_t attribute_handle,
    const Span<const uint8_t>& value
)
{
    return _client.write_request(
        connection_handle,
        attribute_handle,
        value
    );
}


ble_error_t PalAttClientToGattClient::queue_prepare_write(
    connection_handle_t connection_handle,
    attribute_handle_t characteristic_value_handle,
    const Span<const uint8_t>& value,
    uint16_t offset
)
{
    return _client.prepare_write_request(
        connection_handle,
        characteristic_value_handle,
        offset,
        value
    );
}


ble_error_t PalAttClientToGattClient::execute_write_queue(
    connection_handle_t connection_handle,
    bool execute
)
{
    return _client.execute_write_request(connection_handle, execute);
}


ble_error_t PalAttClientToGattClient::initialize()
{
    return _client.initialize();
}


ble_error_t PalAttClientToGattClient::terminate()
{
    return _client.initialize();
}

} // ble
