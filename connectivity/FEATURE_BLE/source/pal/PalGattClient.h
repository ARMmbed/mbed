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

#ifndef BLE_PAL_GATT_CLIENT_H_
#define BLE_PAL_GATT_CLIENT_H_

#include "platform/Callback.h"

#include "ble/common/UUID.h"
#include "ble/common/BLETypes.h"
#include "ble/common/blecommon.h"
#include "AttServerMessage.h"

namespace ble {

/**
 * Definition of the general handler of PalGattClient related events.
 */
struct PalGattClientEventHandler {
    /**
     * Function invoked when the connections changes the ATT_MTU which controls
     * the maximum size of an attribute that can be read in a single L2CAP packet
     * which might be fragmented across multiple packets.
     *
     * @param connectionHandle The handle of the connection that changed the size.
     * @param attMtuSize
     */
    virtual void on_att_mtu_change(
        ble::connection_handle_t connection_handle,
        uint16_t att_mtu_size
    ) = 0;

    /**
     * Function invoked when a write command has been sent out of the stack
     * (either to the controller or over the air).
     *
     * @param connection_handle Connection targeted by the write command
     * @param attribute_handle Attribute written
     * @param status HCI status of the operation.
     */
    virtual void on_write_command_sent(
        ble::connection_handle_t connection_handle,
        ble::attribute_handle_t attribute_handle,
        uint8_t status
    ) = 0;
};

/**
 * Adaptation layer for a GATT client.
 *
 * Define the primitive necessary to implement a proper GATT client. These
 * primitives are sometime GATT procedure, ATT procedure or a bit of both.
 *
 * In general, discovery procedures follow strictly GATT formulation while
 * attribute manipulation procedures (read/write) follow the ATT formulation
 * to avoid multiple identical implementation for characteristic values and
 * characteristic descriptors value, it also fill a hole leave by the
 * specification which doesn't define any GATT procedure to get the UUID of an
 * included service with a 128 bit UUID.
 *
 * Complementary informations around ATT procedures can be found in the Section
 * 3.4 of the Vol3, Part F of the Bluetooth specification while more informations
 * around the GATT procedures can be found in the  Section 4 of the Vol 3, Part
 * G of the Bluetooth specification.
 *
 * Complete and compliant Gatt Client used by developer is realized at an higher
 * level using the primitives defined in this adaptation layer.
 *
 * If a stack expose the complete ATT layer then it is possible to provide an
 * implementation for PalGattClient by subclassing the PalAttClient class and use
 * the class PalGattClient
 */
class PalGattClient {
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
     * allow usage of the interface.
     *
     * @return BLE_ERROR_NONE if the request has been successfully sent or the
     * appropriate error otherwise.
     */
    virtual ble_error_t terminate() = 0;

    /**
     * Negotiate the mtu to use by this connection.
     * First the client send to the server the maximum rx mtu that it can receive
     * then the client reply with the maximum rx mtu it can receive.
     * The mtu chosen for the connection is the minimum of the client Rx mtu
     * and server Rx mtu values.
     *
     * If an error occurred then the mtu used remains the default value.
     *
     * The server will reply to this request with an AttExchangeMTUResponse in
     * case of success or AttErrorResponse in case of failure.
     *
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.3.1
     *
     * @param connection The handle of the connection to send this request to.
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t exchange_mtu(connection_handle_t connection) = 0;

    /**
     * Acquire the size of the mtu for a given connection.
     *
     * @param connection The handle of the connection for which the the MTU size
     * should be acquired.
     *
     * @param mtu_size Output parameter which will contain the MTU size.
     *
     * @return BLE_ERROR_NONE if the MTU size has been acquired or the
     * appropriate error otherwise.
     */
    virtual ble_error_t get_mtu_size(
        connection_handle_t connection_handle,
        uint16_t& mtu_size
    ) = 0;

    /**
     * Discover primary services in the range [begin - 0xFFFF].
     *
     * If services exists in the range provided, the server will reply with a
     * ReadByGoupType response where for each attribute_data exposed:
     *   - attribute_handle is the service attribute handle
     *   - end_group_handle is the last handle of the service
     *   - attribute_value is the UUID of the service.
     *
     * If the end of the range is not reached, this procedure can be relaunched
     * with the last handle of the last service discovered plus one as the
     * beginning of the discovery range.
     *
     * If there is not services left to discover in the range, the server can
     * either:
     *   * Reply with an ErrorResponse with the Error code set to ATTRIBUTE_NOT_FOUND
     *   * Set the end handle of the last service to 0xFFFF.
     *
     * @note This function realize a partial implementation of the Discover All
     * Primary Services procedure. The complete implementation of the procedure
     * is realized at an higher level.
     * @note This function issue a  Read By Group Type ATT request where the
     * type parameter is equal to Primary Service and the Ending Handle parameter
     * is equal to 0xFFFF.
     * @note BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.4.1
     *
     * @param connection The handle of the connection to send this request to.
     * @param begin The beginning of the discovery range.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t discover_primary_service(
        connection_handle_t connection,
        attribute_handle_t discovery_range_begining
    ) = 0;

    /**
     * Discover primary services by UUID in the range [discovery_range_begining - 0xFFFF].
     *
     * If services exists in the range provided, the server will reply with a
     * FindByTypeValueResponse containing the attribute range of each service
     * discovered.
     *
     * If the end of the range is not reached, this procedure can be relaunched
     * with the last handle of the last service  discovered plus one as the
     * beginning of the discovery range.
     *
     * If there is not services left to discover in the range, the server can
     * either:
     *   * Reply with an ErrorResponse with the Error code set to ATTRIBUTE_NOT_FOUND
     *   * Set the end handle of the last service to 0xFFFF.
     *
     * @note This function realize a partial implementation of the Discover
     * Primary Service by Service UUID procedure. The complete implementation of
     * the procedure is realized at an higher level.
     * @note This function issue a  Find By Type Value ATT request where the
     * type parameter is equal to Primary Service and the Ending Handle
     * parameter is equal to 0xFFFF.
     * @note BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.4.2
     *
     * @param connection The handle of the connection to send this request to.
     * @param begin The beginning of the discovery range.
     * @param uuid The UUID of the service to discover.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t discover_primary_service_by_service_uuid(
        connection_handle_t connection_handle,
        attribute_handle_t discovery_range_beginning,
        const UUID& uuid
    ) = 0;

    /**
     * Find included services within a service.
     *
     * If services are included in the service range then the server will reply
     * with a ReadByTypeResponse where for each attribute record:
     *   - attribute_handle The handle where the service is included within the
     *     service definition.
     *   - attribute_data Contains two handles defining the range of the included.
     *     If the service found have a 16 bit uuid then its UUID is also included
     *     in the attribute data.
     *
     * If the end of the service range is not reached, this procedure can be
     * relaunched with the handle of the last included service discovered plus
     * one as the beginning of the new discovery range.
     *
     * The procedure is complete when either:
     *   - The server reply with an ErrorResponse with the Error code set to
     *     ATTRIBUTE_NOT_FOUND
     *   - An included service handle returned is equal to the end of the range.
     *
     * If the service UUID is a 128 bits one then it is necessary to issue a read
     * attribute request on the first handle of the service discovered to get it.
     *
     * @note This function realize a partial implementation of the Find Included
     * Services procedure. The complete implementation of the procedure is
     * realized at an higher level.
     * @note This function issue a  Read By Type ATT request where the
     * type parameter is equal to Include.
     * @note BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.5.1
     *
     * @param connection The handle of the connection to send this request to.
     * @param service_range The range of the service where service inclusion has
     * to be discovered.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t find_included_service(
        connection_handle_t connection_handle,
        attribute_handle_range_t service_range
    ) = 0;

    /**
     * Find characteristic declarations within a service definition.
     *
     * If characteristic declarations are found within the range then the server
     * should reply with a ReadByTypeResponse where for each attribute record:
     *   - attribute_handle is the handle of the characteristic definition
     *   - attribute_data contains the the following values attached to the
     *     characteristic :
     *      + properties: the properties of the characteristic.
     *      + value handle: the handle of the value of the characteristic.
     *      + uuid: the UUID of the characteristic.
     *
     * The procedure is considered complete when the server send an ErrorResponse
     * with the ErrorCode set to ATTRIBUTE_NOT_FOUND or a ReadByType response
     * has an attribute_handle set to the end of the discovery range.
     *
     * If the procedure is not complete after a server response, it should be
     * relaunched with an updated range starting at the last attribute_handle
     * discovered plus one.
     *
     * @note This function realize a partial implementation of the Discover All
     * Characteristics of a Service procedure. The complete implementation of
     * the procedure is realized at an higher level.
     * @note This function issue a  Read By Type ATT request where the type
     * parameter is equal to Characteristic.
     * @note BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.6.1
     *
     * @note The GATT procedure Discover Characteristics by UUID is implemented
     * at a higher level using this function as a base.
     *
     * @param connection The handle of the connection to send this request to.
     * @param discovery_range The range of attributes where the characteristics
     * are discovered. It should be contained within a service.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t discover_characteristics_of_a_service(
        connection_handle_t connection_handle,
        attribute_handle_range_t discovery_range
    ) = 0;

    /**
     * Discover characteristic descriptors of a characteristic.
     *
     * If descriptors are found within the range provided then the server should
     * reply with a FindInformationResponse containing a list of
     * attribute_handle_t, UUID pairs where the attribute handle is the
     * descriptor handle and UUID is the UUID of the descriptor.
     *
     * The procedure is complete when the server send an ErrorResponse with the
     * error code ATTRIBUTE_NOT_FOUND or the FindInformationResponse has an
     * attribute handle that is equal to the end handle of the discovery range.
     *
     * @note This function realize a partial implementation of the Discover All
     * Characteristics Descriptors procedure. The complete implementation of
     * the procedure is realized at an higher level.
     * @note This function issue a Find Information ATT request..
     * @note BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.7.1
     *
     * @note It should be possible to use this function to issue a regular
     * ATT Find Information Request.
     *
     * @param connection The handle of the connection to send this request to.
     * @param descriptors_discovery_range The range of attributes where the
     * descriptors are discovered. The first handle shall be no less than the
     * characteristic value handle plus one and the last handle shall be no more
     * than the last handle of the characteristic.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t discover_characteristics_descriptors(
        connection_handle_t connection_handle,
        attribute_handle_range_t descriptors_discovery_range
    ) = 0;

    /**
     * Read the value of an attribute.
     *
     * The server will reply with an AttReadResponse. In case of error, the
     * server will reply with an AttErrorResponse.
     *
     * @note This function issue an ATT Read Request.
     *
     * @note: This function is the base function for the read Characteristic
     * Value and Read Characteristic Descriptor GATT procedures. It can also
     * be used to read the 128 bit UUID of a service discovered with
     * find_included_service.
     *
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.8.1
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.12.1
     *
     * @param connection The handle of the connection to send this request to.
     * @param attribute_handle Handle of the attribute to read.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t read_attribute_value(
        connection_handle_t connection_handle,
        attribute_handle_t attribute_handle
    ) = 0;

    /**
     * Read a characteristic value using its UUID (type).
     *
     * The server will respond a ReadByTypeResponse containing a sequence of
     * attribute handle and attribute value pairs.
     * To read remaining attributes, the client should launch a new request
     * with an updated range.
     *
     * The procedure is considered complete when the server respond with an
     * ErrorResponse containing the ErrorCode ATTRIBUTE_NOT_FOUND or when an
     * handle in the ReadByTypeResponse is equal to the end of the discovered
     * range.
     *
     * @note This function realize a partial implementation of the Read Using
     * Characteristics Characteristic procedure. The complete implementation of
     * the procedure is realized at an higher level.
     * @note This function issue a Read By Type ATT request.
     * @note BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.8.2
     *
     * @note It should be possible to use this function to issue a regular
     * ATT Read By Type Request.
     *
     * @param connection The handle of the connection to send this request to.
     * @param attribute_range Range of the handle where an attribute with
     * uuid as type is present.
     * @param uuid UUID of the characteristic(s) to read.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t read_using_characteristic_uuid(
        connection_handle_t connection_handle,
        attribute_handle_range_t read_range,
        const UUID& uuid
    ) = 0;

    /**
     * Read a partial value of an attribute.
     *
     * The server will respond with a ReadBlobResponse containing the data read
     * or an ErrorResponse in case of error.
     *
     * The procedure is not complete as long as the value in response have the
     * same size as the mtu minus one. If the procedure is not complete, it can
     * be launch again with an updated offset to read the remaining part of the
     * attribute value.
     *
     * @note This function issue an ATT Read Blob Request.
     *
     * @note: This function is the base function for the Read Long
     * Characteristic Value and Read Long Characteristic Descriptor GATT
     * procedures.
     *
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.8.3
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.12.2
     *
     * @param connection_handle The handle of the connection to send this request to.
     * @param attribute_handle Handle of the attribute to read.
     * @param offset Beginning offset for the read operation.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t read_attribute_blob(
        connection_handle_t connection_handle,
        attribute_handle_t attribute_handle,
        uint16_t offset
    ) = 0;

    /**
     * Read atomically multiple characteristics values.
     *
     * The server will respond with a ReadMultiple response containing the
     * concatenation of the values of the characteristics.
     *
     * @note values might be truncated
     *
     * In case of error, the server should respond a with an ErrorResponse.
     *
     * @note This function issue an ATT Read Multiple Request.
     *
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.8.4
     *
     * @param connection_handle The handle of the connection to send this request to.
     * @param characteristic_value_handles Handle of the characteristic values
     * to read.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t read_multiple_characteristic_values(
        connection_handle_t connection_handle,
        const Span<const attribute_handle_t>& characteristic_value_handles
    ) = 0;

    /**
     * Send a write command to the server.
     *
     * @note This function issue an ATT Write Command.
     *
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.9.1
     *
     * @param connection_handle The handle of the connection to send this request to.
     * @param attribute_handle Handle of the attribute to write.
     * @param value The value to write.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t write_without_response(
        connection_handle_t connection_handle,
        attribute_handle_t characteristic_value_handle,
        const Span<const uint8_t>& value
    ) = 0;
#if BLE_FEATURE_SIGNING
    /**
     * Send a Signed Write without Response command to the server.
     *
     * @note This function issue an ATT Write Command with the signed flag and
     * the signature.
     *
     * @note signature is calculated by the stack.
     *
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.9.2
     *
     * @param connection_handle The handle of the connection to send this request to.
     * @param attribute_handle Handle of the attribute to write.
     * @param value The value to write.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t signed_write_without_response(
        connection_handle_t connection_handle,
        attribute_handle_t characteristic_value_handle,
        const Span<const uint8_t>& value
    ) = 0;
#endif //BLE_FEATURE_SIGNING
    /**
     * Send a write request to the server.
     *
     * The server should respond with a WriteResponse in case of success or an
     * ErrorResponse in case of error.
     *
     * @note This function issue an ATT Write Request.
     *
     * @note: This function is the base function for the Write Characteristic
     * Value and Write Characteristic Descriptors GATT procedures.
     *
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.9.3
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.12.3
     *
     * @param connection_handle The handle of the connection to send this request to.
     * @param attribute_handle Handle of the attribute to write.
     * @param value The value to write.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t write_attribute(
        connection_handle_t connection_handle,
        attribute_handle_t attribute_handle,
        const Span<const uint8_t>& value
    ) = 0;

    /**
     * Send a prepare write request to the server.
     *
     * The write request will go into a queue until the client execute or cancel
     * the request with an execute write request which will write all the values
     * in the queue atomically.
     *
     * The server should respond with a PrepareWriteResponse containing the
     * content of the request in case of success and an ErrorResponse in case of
     * error.
     *
     * If an ErrorResponse is received it doesn't invalidate what is already in
     * the queue.
     *
     * @note This function issue an ATT Prepare Write Request.
     *
     * @note: This function is one of the base function for the Write Long
     * Characteristic Value and Reliable Writes GATT procedures.
     *
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.9.4
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.9.5
     *
     * @param connection_handle The handle of the connection to send this request to.
     * @param attribute_handle Handle of the attribute to write.
     * @param value The value to write.
     * @param offset offset where the value should be written.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t queue_prepare_write(
        connection_handle_t connection_handle,
        attribute_handle_t characteristic_value_handle,
        const Span<const uint8_t>& value,
        uint16_t offset
    ) = 0;

    /**
     * Send a request to the server to execute the queue of prepared write
     * requests.
     *
     * The server should respond with an ExecuteWriteResponse in case of success
     * and an Error response in case of failure.
     *
     * @note This function issue an ATT Execute Write Request.
     *
     * @note: This function is one of the base function for the Write Long
     * Characteristic Value and Reliable Writes GATT procedures.
     *
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.9.4
     * @note see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G Section 4.9.5
     *
     * @param connection_handle The handle of the connection to send this request to.
     * @param execute If true, execute the write request queue otherwise cancel it.
     *
     * @return BLE_ERROR_NONE or an appropriate error.
     */
    virtual ble_error_t execute_write_queue(
        connection_handle_t connection_handle,
        bool execute
    ) = 0;

    /**
     * Register a callback which will handle messages from the server.
     *
     * @param cb The callback object which will handle messages from the server.
     * It accept two parameters in input: The handle of the connection where the
     * message was received and the message received. Real type of the message
     * can be obtained from its opcode.
     */
    virtual void when_server_message_received(
        mbed::Callback<void(connection_handle_t, const AttServerMessage&)> cb
    ) = 0;

    /**
     * Register a callback handling transaction timeout.
     *
     * @param cb The callback handling timeout of a transaction. It accepts as
     * a parameter the connection handle involved in the timeout.
     *
     * @note No more attribute protocol requests, commands, indication or
     * notification shall be sent over a connection implied in a transaction
     * timeout. To send a new ATT message, the conenction should be
     * reestablished.
     */
     virtual void when_transaction_timeout(
         mbed::Callback<void(connection_handle_t)> cb
     ) = 0;

     /**
      * Sets the event handler that us called by the PAL porters to notify the stack of events
      * which will in turn be passed onto the user application when appropriate.
      *
      * @param event_handler The new event handler interface implementation.
      */
     virtual void set_event_handler(PalGattClientEventHandler* event_handler) = 0;

     /**
      * Get the currently registered event handler.
      *
      * @return Currently registered event handler. NULL if no event handler is present.
      */
     virtual PalGattClientEventHandler* get_event_handler() = 0;
};

} // namespace ble

#endif /* BLE_PAL_GATT_CLIENT_H_ */
