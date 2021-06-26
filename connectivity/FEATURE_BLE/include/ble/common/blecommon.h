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

#ifndef MBED_BLE_COMMON_H__
#define MBED_BLE_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ble
 * @{
 * @addtogroup common
 * @{
 */

/**
 * Assigned values for BLE UUIDs.
 */
enum {
    /**
     * Reserved UUID.
     */
    BLE_UUID_UNKNOWN = 0x0000,

    /**
     * Primary Service.
     */
    BLE_UUID_SERVICE_PRIMARY = 0x2800,

    /**
     * Secondary Service.
     */
    BLE_UUID_SERVICE_SECONDARY = 0x2801,

    /**
     * Included service.
     */
    BLE_UUID_SERVICE_INCLUDE = 0x2802,

    /**
     * Characteristic.
     */
    BLE_UUID_CHARACTERISTIC = 0x2803,

    /**
     * Characteristic Extended Properties Descriptor.
     */
    BLE_UUID_DESCRIPTOR_CHAR_EXT_PROP = 0x2900,

    /**
     * Characteristic User Description Descriptor.
     */
    BLE_UUID_DESCRIPTOR_CHAR_USER_DESC = 0x2901,

    /**
     * Client Characteristic Configuration Descriptor.
     */
    BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG = 0x2902,

    /**
     * Server Characteristic Configuration Descriptor.
     */
    BLE_UUID_DESCRIPTOR_SERVER_CHAR_CONFIG = 0x2903,

    /**
     * Characteristic Presentation Format Descriptor.
     */
    BLE_UUID_DESCRIPTOR_CHAR_PRESENTATION_FORMAT = 0x2904,

    /**
     * Characteristic Aggregate Format Descriptor.
     */
    BLE_UUID_DESCRIPTOR_CHAR_AGGREGATE_FORMAT = 0x2905,

/* GATT specific UUIDs */
    /**
     * Generic Attribute Profile.
     */
    BLE_UUID_GATT = 0x1801,

    /**
     * Service Changed Characteristic.
     */
    BLE_UUID_GATT_CHARACTERISTIC_SERVICE_CHANGED = 0x2A05,

/* GAP specific UUIDs */

    /**
     * Generic Access Profile.
     */
    BLE_UUID_GAP = 0x1800,

    /**
     * Device Name Characteristic.
     */
    BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME = 0x2A00,

    /**
     * Appearance Characteristic.
     */
    BLE_UUID_GAP_CHARACTERISTIC_APPEARANCE = 0x2A01,

    /**
     * Peripheral Privacy Flag Characteristic.
     */
    BLE_UUID_GAP_CHARACTERISTIC_PPF = 0x2A02,

    /**
     * Reconnection Address Characteristic.
     */
    BLE_UUID_GAP_CHARACTERISTIC_RECONN_ADDR = 0x2A03,

    /**
     * Peripheral Preferred Connection Parameters Characteristic.
     */
    BLE_UUID_GAP_CHARACTERISTIC_PPCP = 0x2A04,
};

/**
 * Error codes for the BLE API.
 *
 * The value 0 means that no error was reported; therefore, it allows an API
 * user to cleanly test for errors.
 *
 * @code
 * ble_error_t error = some_ble_api_function();
 * if (error) {
 *   // handle the error
 * }
 * @endcode
 */
enum ble_error_t {
    /**
     * No error.
     */
    BLE_ERROR_NONE = 0,

    /**
     * The requested action would cause a buffer overflow and has been aborted.
     */
    BLE_ERROR_BUFFER_OVERFLOW = 1,

    /**
     * Requested a feature that isn't yet implemented or isn't supported by the
     * target HW.
     */
    BLE_ERROR_NOT_IMPLEMENTED = 2,

    /**
     * One of the supplied parameters is outside the valid range.
     */
    BLE_ERROR_PARAM_OUT_OF_RANGE = 3,

    /**
     * One of the supplied parameters is invalid.
     */
    BLE_ERROR_INVALID_PARAM = 4,

    /**
     * The stack is busy.
     */
    BLE_STACK_BUSY = 5,

    /**
     * Invalid state.
     */
    BLE_ERROR_INVALID_STATE = 6,

    /**
     * Out of memory.
     */
    BLE_ERROR_NO_MEM = 7,

    /**
     * The operation requested is not permitted.
     */
    BLE_ERROR_OPERATION_NOT_PERMITTED = 8,

    /**
     * The BLE subsystem has not completed its initialization.
     */
    BLE_ERROR_INITIALIZATION_INCOMPLETE = 9,

    /**
     *  The BLE system has already been initialized.
     */
    BLE_ERROR_ALREADY_INITIALIZED = 10,

    /**
     * Unknown error.
     */
    BLE_ERROR_UNSPECIFIED = 11,

    /**
     * The platform-specific stack failed.
     */
    BLE_ERROR_INTERNAL_STACK_FAILURE = 12,

    /**
     * Data not found or there is nothing to return.
     */
    BLE_ERROR_NOT_FOUND = 13,

    /**
     * Specified timeout expired.
     */
    BLE_ERROR_TIMEOUT = 14,

    /**
     * Specified limit expired.
     */
    BLE_ERROR_LIMIT_REACHED = 15
};

/**
 * Default MTU size.
 */
static const unsigned BLE_GATT_MTU_SIZE_DEFAULT = 23;

/**
 * Handle Value Notification/Indication event.
 *
 * Emmitted when a notification or indication has been received from a GATT
 * server.
 */
enum HVXType_t {
    /**
     * Handle Value Notification.
     */
    BLE_HVX_NOTIFICATION = 0x01,

    /**
     * Handle Value Indication.
     */
    BLE_HVX_INDICATION = 0x02,
};

/**
 * @}
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // ifndef MBED_BLE_COMMON_H__
