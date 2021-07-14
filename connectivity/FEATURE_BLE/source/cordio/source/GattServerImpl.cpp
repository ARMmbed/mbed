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

#include "ble/common/BLERoles.h"
#include <algorithm>
#include "source/GattServerImpl.h"
#include "source/BLEInstanceBaseImpl.h"
#include "wsf_types.h"
#include "att_api.h"

#include <new>

#include "mbed-trace/mbed_trace.h"
#include "common/ble_trace_helpers.h"

#define TRACE_GROUP "BLGS"

namespace ble {
namespace impl {

namespace {

static UUID CCCD_UUID(BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG);
static const uint16_t CCCD_SIZE = sizeof(uint16_t);

static const unsigned int READ_PROPERTY =
    GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ;
static const unsigned int WRITE_PROPERTY =
    GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE;
static const unsigned int WRITE_WITHOUT_RESPONSE_PROPERTY =
    GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE;
static const unsigned int SIGNED_WRITE_PROPERTY =
    GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_AUTHENTICATED_SIGNED_WRITES;
static const unsigned int NOTIFY_PROPERTY =
    GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY;
static const unsigned int INDICATE_PROPERTY =
    GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE;

static const uint8_t WRITABLE_PROPERTIES =
    WRITE_PROPERTY |
        WRITE_WITHOUT_RESPONSE_PROPERTY |
        SIGNED_WRITE_PROPERTY;

static const uint8_t UPDATE_PROPERTIES =
    NOTIFY_PROPERTY |
        INDICATE_PROPERTY;

static const uint16_t CONNECTION_ID_LIMIT = 0x100;

} // end of anonymous namespace

GattServer &GattServer::getInstance()
{
    static GattServer m_instance;
    return m_instance;
}

void GattServer::initialize()
{
    tr_info("Initialize GattServer");
#if BLE_FEATURE_SECURITY
    AttsAuthorRegister(atts_auth_cb);
#endif
    add_default_services();
}

void GattServer::add_default_services()
{
    if (!default_services_added) {
        default_services_added = true;
        add_generic_access_service();
        add_generic_attribute_service();
    }
}

ble_error_t GattServer::addService(GattService &service)
{
    tr_info("Add service with UUID=%s and %d characteristic(s) to the Gatt Server",
            to_string(service.getUUID()),
            service.getCharacteristicCount());

    add_default_services();
    // create and fill the service structure
    internal_service_t *att_service = new internal_service_t;
    att_service->attGroup.pNext = nullptr;
    att_service->attGroup.readCback = atts_read_cb;
    att_service->attGroup.writeCback = atts_write_cb;

    // Determine the attribute list length
    uint16_t attributes_count = compute_attributes_count(service);

    // Create cordio attribute list
    att_service->attGroup.pAttr =
        (attsAttr_t *) alloc_block(attributes_count * sizeof(attsAttr_t));
    if (att_service->attGroup.pAttr == nullptr) {
        tr_error("Failed to create Cordio attribute list");
        delete att_service;
        return BLE_ERROR_BUFFER_OVERFLOW;
    }

    // insert every element in the iterator
    attsAttr_t *attribute_it = att_service->attGroup.pAttr;

    /* Service */
    insert_service_attribute(service, attribute_it);
    att_service->attGroup.startHandle = currentHandle;
    service.setHandle(currentHandle);

    /* Add characteristics to the service */
    for (int i = 0; i < service.getCharacteristicCount(); i++) {
        ble_error_t err = insert_characteristic(
            service.getCharacteristic(i),
            attribute_it
        );
        if (err) {
            // FIXME: proper cleanup of data structure:
            //   - att_service->attGroup.pAttr
            //   - blocks allocated for characteristics value
            // NOTE: those are rightfully released when reset() is called.
            tr_error("Failed to insert characteristic");
            delete att_service;
            return err;
        }
    }
    att_service->attGroup.endHandle = currentHandle;

    // add the service to the list of registered services
    if (registered_service) {
        att_service->next = registered_service;
    } else {
        att_service->next = nullptr;
    }

    registered_service = att_service;

    // register services and update cccds
    AttsAddGroup(&att_service->attGroup);
    AttsCccRegister(cccd_cnt, (attsCccSet_t *) cccds, cccd_cb);
    return BLE_ERROR_NONE;
}

uint16_t GattServer::compute_attributes_count(GattService &service)
{
    // start at 1, one attribute is required for the service itself
    uint16_t attributes_count = 1;
    for (int i = 0; i < service.getCharacteristicCount(); i++) {
        attributes_count += 2;
        GattCharacteristic *p_char = service.getCharacteristic(i);
        attributes_count += p_char->getDescriptorCount();
        if (p_char->getProperties() & UPDATE_PROPERTIES) {
            // add a CCCD
            ++attributes_count;

            // verify that it hasn't been counted twice
            for (size_t j = 0; j < p_char->getDescriptorCount(); ++j) {
                if (p_char->getDescriptor(j)->getUUID() ==
                    UUID(BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG)
                    ) {
                    --attributes_count;
                    break;
                }
            }
        }
    }

    return attributes_count;
}

void GattServer::insert_service_attribute(
    GattService &service,
    attsAttr_t *&attribute_it
)
{
    ++currentHandle;
    const UUID &service_uuid = service.getUUID();

    attribute_it->pUuid = attPrimSvcUuid;
    if (service_uuid.shortOrLong() == UUID::UUID_TYPE_LONG) {
        attribute_it->maxLen = UUID::LENGTH_OF_LONG_UUID;
    } else {
        attribute_it->maxLen = sizeof(UUID::ShortUUIDBytes_t);
    }
    attribute_it->pValue = (uint8_t *) alloc_block(attribute_it->maxLen);
    memcpy(attribute_it->pValue, service_uuid.getBaseUUID(), attribute_it->maxLen);
    attribute_it->pLen = &attribute_it->maxLen;
    attribute_it->settings = 0;
    attribute_it->permissions = ATTS_PERMIT_READ;

    ++attribute_it;
}

ble_error_t GattServer::insert_characteristic(
    GattCharacteristic *characteristic,
    attsAttr_t *&attribute_it
)
{
    tr_info("Insert characteristic %s - "
            "valuePtr=%p, "
            "lenMax=%d, "
            "len=%d, "
            "hasVariableLength=%s, "
            "read_security=%s, "
            "write_security=%s, "
            "properties=[ %s%s%s%s%s%s%s%s], "
            "descriptor_count=%d",
            to_string(characteristic->getValueAttribute().getUUID()),
            characteristic->getValueAttribute().getValuePtr(),
            characteristic->getValueAttribute().getMaxLength(),
            characteristic->getValueAttribute().getLength(),
            to_string(characteristic->getValueAttribute().hasVariableLength()),
            to_string(characteristic->getValueAttribute().getReadSecurityRequirement()),
            to_string(characteristic->getValueAttribute().getWriteSecurityRequirement()),
            characteristic->getProperties() & GattCharacteristic::Properties_t::BLE_GATT_CHAR_PROPERTIES_BROADCAST
            ? "BROADCAST " : "",
            characteristic->getProperties() & GattCharacteristic::Properties_t::BLE_GATT_CHAR_PROPERTIES_READ
            ? "READ " : "",
            characteristic->getProperties() & GattCharacteristic::Properties_t::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE
            ? "WRITE_WITHOUT_RESPONSE " : "",
            characteristic->getProperties() & GattCharacteristic::Properties_t::BLE_GATT_CHAR_PROPERTIES_WRITE
            ? "WRITE " : "",
            characteristic->getProperties() & GattCharacteristic::Properties_t::BLE_GATT_CHAR_PROPERTIES_NOTIFY
            ? "NOTIFY " : "",
            characteristic->getProperties() & GattCharacteristic::Properties_t::BLE_GATT_CHAR_PROPERTIES_INDICATE
            ? "INDICATE " : "",
            characteristic->getProperties() & GattCharacteristic::Properties_t::BLE_GATT_CHAR_PROPERTIES_AUTHENTICATED_SIGNED_WRITES
            ? "AUTHENTICATED_SIGNED_WRITES " : "",
            characteristic->getProperties() & GattCharacteristic::Properties_t::BLE_GATT_CHAR_PROPERTIES_EXTENDED_PROPERTIES
            ? "EXTENDED_PROPERTIES " : "",
            characteristic->getDescriptorCount());

    bool valid = is_characteristic_valid(characteristic);
    if (!valid) {
        tr_error("Characteristic is not valid");
        return BLE_ERROR_INVALID_PARAM;
    }

    uint8_t properties = characteristic->getProperties();

    // Create Characteristic Declaration Attribute
    insert_characteristic_declaration_attribute(characteristic, attribute_it);
    ble_error_t err = insert_characteristic_value_attribute(characteristic, attribute_it);
    if (err) {
        tr_error("Failed to insert value attribute");
        return err;
    }


    // insert descriptors
    bool cccd_created = false;
    for (size_t i = 0; i < characteristic->getDescriptorCount(); i++) {
        err = insert_descriptor(
            characteristic,
            characteristic->getDescriptor(i),
            attribute_it,
            cccd_created
        );
        if (err) {
            return err;
            tr_error("Failed to insert descriptor");
        }
    }

    // insert implicit CCCD
    if ((properties & UPDATE_PROPERTIES) && (cccd_created == false)) {
        err = insert_cccd(characteristic, attribute_it);
        if (err) {
            tr_error("Failed to insert implicit CCCD");
            return err;
        }
    }

    tr_info("Successfully inserted characteristic %s: handle=%d",
            to_string(characteristic->getValueAttribute().getUUID()),
            characteristic->getValueHandle());
    return BLE_ERROR_NONE;
}

bool GattServer::is_characteristic_valid(GattCharacteristic *characteristic)
{
    uint8_t properties = characteristic->getProperties();

    // nothing to read while the characteristic is flagged as readable
    if ((characteristic->getValueAttribute().getValuePtr() == nullptr) &&
        (characteristic->getValueAttribute().getMaxLength() == 0) &&
        (properties == READ_PROPERTY) &&
        (characteristic->isReadAuthorizationEnabled() == false)
        ) {
        return false;
    }

    // nothing to write while the characteristic is flagged as writable
    if ((characteristic->getValueAttribute().getValuePtr() == nullptr) &&
        (characteristic->getValueAttribute().getMaxLength() == 0) &&
        (properties & WRITABLE_PROPERTIES) &&
        (characteristic->isWriteAuthorizationEnabled() == false)
        ) {
        return false;
    }

#if BLE_FEATURE_SIGNING
    // check for invalid permissions
    if ((properties == SIGNED_WRITE_PROPERTY) &&
        (
            characteristic->getWriteSecurityRequirement() == att_security_requirement_t::NONE
#if BLE_FEATURE_SECURE_CONNECTIONS
                || characteristic->getWriteSecurityRequirement() == att_security_requirement_t::SC_AUTHENTICATED

#endif // BLE_FEATURE_SECURE_CONNECTIONS
    )
        ) {
        return false;
    }
#endif // BLE_FEATURE_SIGNING

    return true;
}

void GattServer::insert_characteristic_declaration_attribute(
    GattCharacteristic *characteristic,
    attsAttr_t *&attribute_it
)
{
    tr_info("Insert declaration attribute in characteristic %s",
            to_string(characteristic->getValueAttribute().getUUID()));

    const UUID &value_uuid = characteristic->getValueAttribute().getUUID();

    // move the current handle to point to the value handle
    currentHandle += 2;
    characteristic->getValueAttribute().setHandle(currentHandle);

    // fill the cordio attribute
    attribute_it->pUuid = attChUuid;
    attribute_it->maxLen = 1 + sizeof(currentHandle) + value_uuid.getLen();
    attribute_it->pLen = &attribute_it->maxLen;
    attribute_it->pValue = (uint8_t *) alloc_block(attribute_it->maxLen);
    attribute_it->settings = 0;
    attribute_it->permissions = ATTS_PERMIT_READ;

    // set the attribute value
    uint8_t *value_it = attribute_it->pValue;
    *value_it++ = characteristic->getProperties();
    memcpy(value_it, &currentHandle, sizeof(currentHandle));
    value_it += sizeof(currentHandle);
    memcpy(value_it, value_uuid.getBaseUUID(), value_uuid.getLen());

    ++attribute_it;
}

ble_error_t GattServer::insert_characteristic_value_attribute(
    GattCharacteristic *characteristic,
    attsAttr_t *&attribute_it
)
{
    tr_info("Insert value attribute in characteristic %s",
            to_string(characteristic->getValueAttribute().getUUID()));

    GattAttribute &value_attribute = characteristic->getValueAttribute();
    uint8_t properties = characteristic->getProperties();

    // note: currentHandle has been already moved to the correct value

    // Create Value Attribute
    attribute_it->pUuid = value_attribute.getUUID().getBaseUUID();
    attribute_it->maxLen = characteristic->getValueAttribute().getMaxLength();
    attribute_it->pLen = (uint16_t *) alloc_block(attribute_it->maxLen + sizeof(uint16_t));
    *attribute_it->pLen = value_attribute.getLength();
    attribute_it->pValue = (uint8_t *) ((uint16_t *) attribute_it->pLen + 1);
    memcpy(attribute_it->pValue, value_attribute.getValuePtr(), *attribute_it->pLen);
    memset(attribute_it->pValue + *attribute_it->pLen, 0, attribute_it->maxLen - *attribute_it->pLen);

    // Set value attribute settings
    attribute_it->settings = 0;

    if (properties & READ_PROPERTY) {
        attribute_it->settings |= ATTS_SET_READ_CBACK;
    }
    if (properties & WRITABLE_PROPERTIES) {
        attribute_it->settings |= ATTS_SET_WRITE_CBACK;
        attribute_it->settings |= ATTS_SET_ALLOW_OFFSET;
    }
    if (value_attribute.getUUID().shortOrLong() == UUID::UUID_TYPE_LONG) {
        attribute_it->settings |= ATTS_SET_UUID_128;
    }
    if (value_attribute.hasVariableLength()) {
        attribute_it->settings |= ATTS_SET_VARIABLE_LEN;
    }
    if (properties & SIGNED_WRITE_PROPERTY) {
        attribute_it->settings |= ATTS_SET_ALLOW_SIGNED;
    }

    // setup permissions
    attribute_it->permissions = 0;

    // configure read permission
    if (properties & READ_PROPERTY) {
        attribute_it->permissions |= ATTS_PERMIT_READ;
        switch (characteristic->getReadSecurityRequirement().value()) {
            case att_security_requirement_t::NONE:
                break;
#if BLE_FEATURE_SECURITY
            case att_security_requirement_t::UNAUTHENTICATED:
                attribute_it->permissions |= ATTS_PERMIT_READ_ENC;
                break;
            case att_security_requirement_t::AUTHENTICATED:
                attribute_it->permissions |=
                    ATTS_PERMIT_READ_ENC |
                        ATTS_PERMIT_READ_AUTH;
                break;
#if BLE_FEATURE_SECURE_CONNECTIONS
            case att_security_requirement_t::SC_AUTHENTICATED:
                // Note: check done in the cordio stack doesn't cover LESC
                // so this one is done in attsAuthorCback
                attribute_it->permissions |=
                    ATTS_PERMIT_READ_ENC |
                        ATTS_PERMIT_READ_AUTH |
                        ATTS_PERMIT_READ_AUTHORIZ;
                break;
#endif // BLE_FEATURE_SECURE_CONNECTIONS
#endif // BLE_FEATURE_SECURITY
        }
    }

    // configure write permission
    if (properties & WRITABLE_PROPERTIES) {
        attribute_it->permissions |= ATTS_PERMIT_WRITE;
        switch (characteristic->getWriteSecurityRequirement().value()) {
            case att_security_requirement_t::NONE:
                break;
#if BLE_FEATURE_SECURITY
            case att_security_requirement_t::UNAUTHENTICATED:
                attribute_it->permissions |= ATTS_PERMIT_WRITE_ENC;
                break;
            case att_security_requirement_t::AUTHENTICATED:
                attribute_it->permissions |=
                    ATTS_PERMIT_WRITE_ENC |
                        ATTS_PERMIT_WRITE_AUTH;
                break;
#if BLE_FEATURE_SECURE_CONNECTIONS
            case att_security_requirement_t::SC_AUTHENTICATED:
                // Note: check done in the cordio stack doesn't cover LESC
                // so this one is done in attsAuthorCback
                attribute_it->permissions |=
                    ATTS_PERMIT_WRITE_ENC |
                        ATTS_PERMIT_WRITE_AUTH |
                        ATTS_PERMIT_WRITE_AUTHORIZ;
                break;
#endif // BLE_FEATURE_SECURE_CONNECTIONS
#endif // BLE_FEATURE_SECURITY
        }
    }

    // Register characteristic in authorisation list
    // Note: ATTS_PERMIT_*_AUTHORIZ is uniquely used to check if security
    // requirements are met according to the security requirements set in the
    // characteristic.
    // User defined security authorisation does not impact this flag
    if ((attribute_it->permissions & (ATTS_PERMIT_READ_AUTHORIZ | ATTS_PERMIT_WRITE_AUTHORIZ)) ||
        (properties & UPDATE_PROPERTIES) ||
        characteristic->isReadAuthorizationEnabled() ||
        characteristic->isWriteAuthorizationEnabled()
        ) {
        if (_auth_callbacks_count >= MBED_CONF_BLE_API_IMPLEMENTATION_MAX_CHARACTERISTIC_AUTHORISATION_COUNT) {
            tr_error("Authorisation characteristic count cannot be greater than %d",
                     MBED_CONF_BLE_API_IMPLEMENTATION_MAX_CHARACTERISTIC_AUTHORISATION_COUNT);
            return BLE_ERROR_NO_MEM;
        }

        char_auth_callback *new_cb = (char_auth_callback *) alloc_block(sizeof(char_auth_callback));

        if (!new_cb) {
            tr_error("Failed to allocate space on the heap for new R/W authorisation callbacks");
            return BLE_ERROR_NO_MEM;
        }

        new_cb->read_cb = characteristic->readAuthorizationCallback;
        new_cb->write_cb = characteristic->writeAuthorizationCallback;
        new_cb->handle = characteristic->getValueHandle();
        new_cb->update_security = characteristic->getUpdateSecurityRequirement();
        new_cb->_next = nullptr;

        /* add it to the list */
        if (_auth_callbacks) {
            char_auth_callback *last_cb = _auth_callbacks;
            while (last_cb->_next) {
                last_cb = last_cb->_next;
            }
            last_cb->_next = new_cb;
        } else {
            _auth_callbacks = new_cb;
        };

        ++_auth_callbacks_count;
    }

    ++attribute_it;

    return BLE_ERROR_NONE;
}

ble_error_t GattServer::insert_descriptor(
    GattCharacteristic *characteristic,
    GattAttribute *descriptor,
    attsAttr_t *&attribute_it,
    bool &cccd_created
)
{
    tr_info("Insert descriptor in characteristic %s",
            to_string(characteristic->getValueAttribute().getUUID()));

    uint8_t properties = characteristic->getProperties();

    currentHandle++;

    descriptor->setHandle(currentHandle);

    attribute_it->pUuid = descriptor->getUUID().getBaseUUID();
    attribute_it->maxLen = descriptor->getMaxLength();
    attribute_it->pLen = (uint16_t *) alloc_block(attribute_it->maxLen + sizeof(uint16_t));
    *attribute_it->pLen = descriptor->getLength();
    attribute_it->pValue = (uint8_t *) ((uint16_t *) attribute_it->pLen + 1);
    memcpy(attribute_it->pValue, descriptor->getValuePtr(), *attribute_it->pLen);
    memset(attribute_it->pValue + *attribute_it->pLen, 0, attribute_it->maxLen - *attribute_it->pLen);

    attribute_it->settings = 0;

    if (descriptor->getUUID().shortOrLong() == UUID::UUID_TYPE_LONG) {
        attribute_it->settings |= ATTS_SET_UUID_128;
    }

    // handle the special case of a CCCD
    if (descriptor->getUUID() == UUID(BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG)) {
        if (cccd_cnt >= MBED_CONF_BLE_API_IMPLEMENTATION_MAX_CCCD_COUNT) {
            tr_error("CCCD count cannot be greater than/equal to %d",
                     MBED_CONF_BLE_API_IMPLEMENTATION_MAX_CCCD_COUNT);

            return BLE_ERROR_NO_MEM;
        }

        if (descriptor->isReadAllowed() == false ||
            descriptor->getReadSecurityRequirement() != att_security_requirement_t::NONE
            ) {
            tr_error("The attribute must be readable and secure");
            return BLE_ERROR_INVALID_PARAM;
        }

        cccd_created = true;
        tr_info("cccd_created=%s", to_string(cccd_created));
        attribute_it->settings |= ATTS_SET_CCC;

        cccds[cccd_cnt].handle = currentHandle;
        cccds[cccd_cnt].valueRange = 0;
        if (properties & NOTIFY_PROPERTY) {
            cccds[cccd_cnt].valueRange |= ATT_CLIENT_CFG_NOTIFY;
        }
        if (properties & INDICATE_PROPERTY) {
            cccds[cccd_cnt].valueRange |= ATT_CLIENT_CFG_INDICATE;
        }
        cccd_handles[cccd_cnt] = characteristic->getValueAttribute().getHandle();
        cccd_cnt++;
    }

    if (descriptor->hasVariableLength()) {
        attribute_it->settings |= ATTS_SET_VARIABLE_LEN;
    }

    // setup permissions
    attribute_it->permissions = 0;

    // configure read permission
    if (descriptor->isReadAllowed()) {
        attribute_it->permissions |= ATTS_PERMIT_READ;
        switch (descriptor->getReadSecurityRequirement().value()) {
            case att_security_requirement_t::NONE:
                break;
#if BLE_FEATURE_SECURITY
            case att_security_requirement_t::UNAUTHENTICATED:
                attribute_it->permissions |= ATTS_PERMIT_READ_ENC;
                break;
            case att_security_requirement_t::AUTHENTICATED:
                attribute_it->permissions |=
                    ATTS_PERMIT_READ_ENC |
                        ATTS_PERMIT_READ_AUTH;
                break;
#if BLE_FEATURE_SECURE_CONNECTIONS
            case att_security_requirement_t::SC_AUTHENTICATED:
                // Note: check done in the cordio stack doesn't cover LESC
                // so this one is done in attsAuthorCback
                attribute_it->permissions |=
                    ATTS_PERMIT_READ_ENC |
                        ATTS_PERMIT_READ_AUTH |
                        ATTS_PERMIT_READ_AUTHORIZ;
                break;
#endif // BLE_FEATURE_SECURE_CONNECTIONS
#endif // BLE_FEATURE_SECURITY
        }

        if (!(attribute_it->settings & ATTS_SET_CCC)) {
            attribute_it->settings |= ATTS_SET_READ_CBACK;
        }
    }

    // configure write permission
    if (descriptor->isWriteAllowed()) {
        attribute_it->permissions |= ATTS_PERMIT_WRITE;
        switch (descriptor->getWriteSecurityRequirement().value()) {
            case att_security_requirement_t::NONE:
                break;
#if BLE_FEATURE_SECURITY
            case att_security_requirement_t::UNAUTHENTICATED:
                attribute_it->permissions |= ATTS_PERMIT_WRITE_ENC;
                break;
            case att_security_requirement_t::AUTHENTICATED:
                attribute_it->permissions |=
                    ATTS_PERMIT_WRITE_ENC |
                        ATTS_PERMIT_WRITE_AUTH;
                break;
#if BLE_FEATURE_SECURE_CONNECTIONS
            case att_security_requirement_t::SC_AUTHENTICATED:
                // Note: check done in the cordio stack doesn't cover LESC
                // so this one is done in attsAuthorCback
                attribute_it->permissions |=
                    ATTS_PERMIT_WRITE_ENC |
                        ATTS_PERMIT_WRITE_AUTH |
                        ATTS_PERMIT_WRITE_AUTHORIZ;
                break;
#endif // BLE_FEATURE_SECURE_CONNECTIONS
#endif // BLE_FEATURE_SECURITY
        }

        if (!(attribute_it->settings & ATTS_SET_CCC)) {
            attribute_it->settings |= ATTS_SET_WRITE_CBACK;
        }
    }

    attribute_it++;
    return BLE_ERROR_NONE;
}

ble_error_t GattServer::insert_cccd(
    GattCharacteristic *characteristic,
    attsAttr_t *&attribute_it
)
{
    tr_info("Insert CCCD in characteristic %s",
            to_string(characteristic->getValueAttribute().getUUID()));

    if (cccd_cnt >= MBED_CONF_BLE_API_IMPLEMENTATION_MAX_CCCD_COUNT) {
        tr_error("failed insert cccd: table full");
        return BLE_ERROR_NO_MEM;
    }

    uint8_t properties = characteristic->getProperties();

    currentHandle++;

    attribute_it->pUuid = CCCD_UUID.getBaseUUID();
    attribute_it->pValue = (uint8_t *) &cccd_values[cccd_cnt];
    attribute_it->maxLen = CCCD_SIZE;
    attribute_it->pLen = &attribute_it->maxLen;
    attribute_it->settings = ATTS_SET_CCC;
    attribute_it->permissions = (ATTS_PERMIT_READ | ATTS_PERMIT_WRITE);

    cccds[cccd_cnt].handle = currentHandle;
    cccds[cccd_cnt].valueRange = 0;
    if (properties & NOTIFY_PROPERTY) {
        cccds[cccd_cnt].valueRange |= ATT_CLIENT_CFG_NOTIFY;
    }
    if (properties & INDICATE_PROPERTY) {
        cccds[cccd_cnt].valueRange |= ATT_CLIENT_CFG_INDICATE;
    }
    cccds[cccd_cnt].secLevel = characteristic->getUpdateSecurityRequirement().value();
    cccd_handles[cccd_cnt] = characteristic->getValueAttribute().getHandle();

    /**
     * Set the characteristic's implicitly-created CCCD
     *
     * Ownership is passed to the GattCharacteristic
     */
    GattAttribute* implicit_cccd = new (std::nothrow) GattAttribute(
                                       CCCD_UUID,
                                       attribute_it->pValue,
                                       *attribute_it->pLen,
                                       attribute_it->maxLen,
                                       false);

    if(implicit_cccd == nullptr) {
        tr_error("failed to create implicit CCCD");
        currentHandle--;
        return BLE_ERROR_NO_MEM;
    }

    implicit_cccd->setHandle(cccds[cccd_cnt].handle);
    implicit_cccd->allowRead(true);
    implicit_cccd->allowWrite(true);
    characteristic->setImplicitCCCD(implicit_cccd);

    cccd_cnt++;
    attribute_it++;

    return BLE_ERROR_NONE;
}

ble_error_t GattServer::read(
    GattAttribute::Handle_t att_handle,
    uint8_t buffer[],
    uint16_t *buffer_length
)
{
    uint16_t att_length = 0;
    uint8_t *att_value = nullptr;

    if (AttsGetAttr(att_handle, &att_length, &att_value) != ATT_SUCCESS) {
        tr_error("Failed to read attribute %d: attribute not found", att_handle);
        return BLE_ERROR_PARAM_OUT_OF_RANGE;
    }

    if (buffer) {
        memcpy(buffer, att_value, std::min(*buffer_length, att_length));
    }

    *buffer_length = att_length;

    tr_debug("Read attribute %d - value=%s",
            att_handle,
            mbed_trace_array(buffer, *buffer_length));
    return BLE_ERROR_NONE;
}

ble_error_t GattServer::read(
    connection_handle_t connection,
    GattAttribute::Handle_t att_handle,
    uint8_t buffer[],
    uint16_t *buffer_length
)
{
    // Check to see if this is a CCCD
    uint8_t cccd_index;
    if (get_cccd_index_by_cccd_handle(att_handle, cccd_index)) {
        if (connection == DM_CONN_ID_NONE) {
            tr_error("Failed to read attribute %d on connection %d: unknown connection ID",
                     att_handle,
                     connection);
            return BLE_ERROR_PARAM_OUT_OF_RANGE;
        }
        uint16_t cccd_value = AttsCccGet(connection, cccd_index);
        uint16_t cccd_length = sizeof(cccd_value);

        if (buffer) {
            memcpy(buffer, &cccd_value, std::min(*buffer_length, cccd_length));
        }

        *buffer_length = cccd_length;

        tr_debug("Connection %d: read attribute %d - value=%s",
                connection,
                att_handle,
                mbed_trace_array(buffer, *buffer_length));
        return BLE_ERROR_NONE;
    }

    // This is not a CCCD. Use the non-connection specific update method.
    return read(att_handle, buffer, buffer_length);
}

ble_error_t GattServer::write(
    GattAttribute::Handle_t att_handle,
    const uint8_t buffer[],
    uint16_t len,
    bool local_only
)
{
    tr_info("Write attribute %d - "
            "value=%s, "
            "local_only=%s",
            att_handle,
            mbed_trace_array(buffer, len),
            to_string(local_only));

    // Check to see if this is a CCCD, if it is the case update the value for all
    // connections
    uint8_t cccd_index;
    if (get_cccd_index_by_cccd_handle(att_handle, cccd_index)) {
        if (len != sizeof(uint16_t)) {
            tr_error("CCCDs must be 16 bits");
            return BLE_ERROR_INVALID_PARAM;
        }

        uint16_t cccd_value;
        memcpy(&cccd_value, buffer, sizeof(cccd_value));

        for (dmConnId_t conn_id = DM_CONN_MAX; conn_id > DM_CONN_ID_NONE; --conn_id) {
            if (DmConnInUse(conn_id) == true) {
                AttsCccSet(conn_id, cccd_index, cccd_value);
            }
        }

        return BLE_ERROR_NONE;
    }

    // write the value to the attribute handle
    if (AttsSetAttr(att_handle, len, (uint8_t *) buffer) != ATT_SUCCESS) {
        tr_error("Invalid attribute length");
        return BLE_ERROR_PARAM_OUT_OF_RANGE;
    }

    // return if the update does not have to be propagated to peers
    if (local_only || !get_cccd_index_by_value_handle(att_handle, cccd_index)) {
        return BLE_ERROR_NONE;
    }

    // This characteristic has a CCCD attribute. Handle notifications and
    // indications for all active connections if the authentication is
    // successful
    size_t updates_sent = 0;

#if BLE_FEATURE_SECURITY
    for (dmConnId_t conn_id = DM_CONN_MAX; conn_id > DM_CONN_ID_NONE; --conn_id) {
        if (DmConnInUse(conn_id) == true) {
            if (is_update_authorized(conn_id, att_handle)) {
                uint16_t cccd_config = AttsCccEnabled(conn_id, cccd_index);
                if (cccd_config & ATT_CLIENT_CFG_NOTIFY) {
                    AttsHandleValueNtf(conn_id, att_handle, len, (uint8_t *) buffer);
                    updates_sent++;
                }
                if (cccd_config & ATT_CLIENT_CFG_INDICATE) {
                    AttsHandleValueInd(conn_id, att_handle, len, (uint8_t *) buffer);
                    updates_sent++;
                }
            }
        }
    }
#endif // BLE_FEATURE_SECURITY

    return BLE_ERROR_NONE;
}

ble_error_t GattServer::write(
    connection_handle_t connection,
    GattAttribute::Handle_t att_handle,
    const uint8_t buffer[],
    uint16_t len,
    bool local_only
)
{
    tr_info("Connection %d: write attribute %d - "
            "value=%s, "
            "local_only=%s",
            connection,
            att_handle,
            mbed_trace_array(buffer, len),
            to_string(local_only));

    // Check to see if this is a CCCD
    uint8_t cccd_index;
    if (get_cccd_index_by_cccd_handle(att_handle, cccd_index)) {
        if ((connection == DM_CONN_ID_NONE) || (len != 2)) { // CCCDs are always 16 bits
            tr_error("Unknown connection ID/length not 16 bits");
            return BLE_ERROR_PARAM_OUT_OF_RANGE;
        }

        uint16_t cccd_value;
        memcpy(&cccd_value, buffer, sizeof(cccd_value));
        AttsCccSet(connection, cccd_index, cccd_value);
        return BLE_ERROR_NONE;
    }

    // write the value to the attribute handle
    if (AttsSetAttr(att_handle, len, (uint8_t *) buffer) != ATT_SUCCESS) {
        tr_error("Invalid attribute length");
        return BLE_ERROR_PARAM_OUT_OF_RANGE;
    }

    // return if the update does not have to be propagated to peers
    if (local_only || !get_cccd_index_by_value_handle(att_handle, cccd_index)) {
        return BLE_ERROR_NONE;
    }

    // This characteristic has a CCCD attribute. Handle notifications and indications.
    size_t updates_sent = 0;

#if BLE_FEATURE_SECURITY
    if (is_update_authorized(connection, att_handle)) {
        uint16_t cccEnabled = AttsCccEnabled(connection, cccd_index);
        if (cccEnabled & ATT_CLIENT_CFG_NOTIFY) {
            AttsHandleValueNtf(connection, att_handle, len, (uint8_t *) buffer);
            updates_sent++;
        }
        if (cccEnabled & ATT_CLIENT_CFG_INDICATE) {
            AttsHandleValueInd(connection, att_handle, len, (uint8_t *) buffer);
            updates_sent++;
        }
    }
#endif // BLE_FEATURE_SECURITY

    return BLE_ERROR_NONE;
}

ble_error_t GattServer::areUpdatesEnabled(
    const GattCharacteristic &characteristic,
    bool *enabled
)
{
    for (size_t idx = 0; idx < cccd_cnt; idx++) {
        if (characteristic.getValueHandle() == cccd_handles[idx]) {
            for (dmConnId_t conn_id = DM_CONN_MAX; conn_id > DM_CONN_ID_NONE; --conn_id) {
                if (DmConnInUse(conn_id) == true) {
                    uint16_t cccd_value = AttsCccGet(conn_id, idx);
                    if (cccd_value & (ATT_CLIENT_CFG_NOTIFY | ATT_CLIENT_CFG_INDICATE)) {
                        *enabled = true;
                        tr_debug("Updates are enabled in characteristic %s",
                                to_string(characteristic.getValueAttribute().getUUID()));
                        return BLE_ERROR_NONE;
                    }
                }
            }
            *enabled = false;
            tr_debug("Updates are not enabled in characteristic %s",
                    to_string(characteristic.getValueAttribute().getUUID()));
            return BLE_ERROR_NONE;
        }
    }

    tr_error("Failed to determine if updates are enabled: characteristic %s not in CCCD table",
             to_string(characteristic.getValueAttribute().getUUID()));
    return BLE_ERROR_PARAM_OUT_OF_RANGE;
}

ble_error_t GattServer::areUpdatesEnabled(
    connection_handle_t connectionHandle,
    const GattCharacteristic &characteristic,
    bool *enabled
)
{
    if (connectionHandle == DM_CONN_ID_NONE) {
        tr_error("Connection %d: failed to determine if updates are enabled - unknown connection ID", connectionHandle);
        return BLE_ERROR_INVALID_PARAM;
    }

    for (uint8_t idx = 0; idx < cccd_cnt; idx++) {
        if (characteristic.getValueHandle() == cccd_handles[idx]) {
            uint16_t cccd_value = AttsCccGet(connectionHandle, idx);
            if (cccd_value & (ATT_CLIENT_CFG_NOTIFY | ATT_CLIENT_CFG_INDICATE)) {
                *enabled = true;
            } else {
                *enabled = false;
            }
            tr_debug("Connection %d: updates are%s enabled in characteristic %s",
                    connectionHandle,
                    *enabled ? "" : " not",
                    to_string(characteristic.getValueAttribute().getUUID()));
            return BLE_ERROR_NONE;
        }
    }

    tr_error("Connection %d: failed to determine if updates are enabled - characteristic %s not in CCCD table",
             connectionHandle,
             to_string(characteristic.getValueAttribute().getUUID()));
    return BLE_ERROR_PARAM_OUT_OF_RANGE;
}

bool GattServer::isOnDataReadAvailable() const
{
    return true;
}

ble::Gap::PreferredConnectionParams_t GattServer::getPreferredConnectionParams()
{
    ble::Gap::PreferredConnectionParams_t params = {0};
    memcpy(&params.minConnectionInterval, generic_access_service.ppcp, 2);
    memcpy(&params.maxConnectionInterval, generic_access_service.ppcp + 2, 2);
    memcpy(&params.slaveLatency, generic_access_service.ppcp + 4, 2);
    memcpy(&params.connectionSupervisionTimeout, generic_access_service.ppcp + 6, 2);
    return params;
}

void GattServer::setPreferredConnectionParams(const ble::Gap::PreferredConnectionParams_t &params)
{
    memcpy(generic_access_service.ppcp, &params.minConnectionInterval, 2);
    memcpy(generic_access_service.ppcp + 2, &params.maxConnectionInterval, 2);
    memcpy(generic_access_service.ppcp + 4, &params.slaveLatency, 2);
    memcpy(generic_access_service.ppcp + 6, &params.connectionSupervisionTimeout, 2);
}

#if 0 // Disabled until reworked and reintroduced to GattServer API

ble_error_t GattServer::setDeviceName(const uint8_t *deviceName)
{
    size_t length = 0;

    if (deviceName != nullptr) {
        length = strlen((const char*)deviceName);
    }

    if (length == 0) {
        free(generic_access_service.device_name_value());
    } else {
        uint8_t* res = (uint8_t*) realloc(generic_access_service.device_name_value(), length);
        if (res == nullptr) {
            return BLE_ERROR_NO_MEM;
        }

        generic_access_service.device_name_value() = res;
        memcpy(res, deviceName, length);
    }

    generic_access_service.device_name_length = length;

    return BLE_ERROR_NONE;
}

void GattServer::getDeviceName(const uint8_t*& name, uint16_t& length)
{
    length = generic_access_service.device_name_length;
    name = generic_access_service.device_name_value();
}

void GattServer::setAppearance(GapAdvertisingData::Appearance appearance)
{
    generic_access_service.appearance = appearance;
}

GapAdvertisingData::Appearance GattServer::getAppearance()
{
    return (GapAdvertisingData::Appearance) generic_access_service.appearance;
}

#endif // Disabled until reworked and reintroduced to GattServer API

ble_error_t GattServer::reset(ble::GattServer* server)
{
    tr_info("Reset GattServer");
    /* Notify that the instance is about to shutdown */
    if (eventHandler) {
        eventHandler->onShutdown(*server);
    }

    // Execute callbacks added with deprecated API
    shutdownCallChain.call(server);
    shutdownCallChain.clear();

    serviceCount = 0;
    characteristicCount = 0;
    default_services_added = false;

    dataSentCallChain.clear();
    dataWrittenCallChain.clear();
    dataReadCallChain.clear();
    updatesEnabledCallback = nullptr;
    updatesDisabledCallback = nullptr;
    confirmationReceivedCallback = nullptr;

    while (registered_service) {
        internal_service_t *s = registered_service;
        registered_service = s->next;
        AttsRemoveGroup(s->attGroup.startHandle);
        delete s;
    }

    while (allocated_blocks) {
        alloc_block_t *b = allocated_blocks;
        allocated_blocks = b->next;
        free(b);
    }

    AttsRemoveGroup(generic_access_service.service.startHandle);
    AttsRemoveGroup(generic_attribute_service.service.startHandle);
    free(generic_access_service.device_name_value());

    currentHandle = 0;
    cccd_cnt = 0;

    _auth_callbacks_count = 0;
    _auth_callbacks = nullptr;

    AttsCccRegister(cccd_cnt, (attsCccSet_t *) cccds, cccd_cb);

    return BLE_ERROR_NONE;
}

void GattServer::cccd_cb(attsCccEvt_t *evt)
{
    GattServerEvents::gattEvent_t evt_type =
        evt->value & (ATT_CLIENT_CFG_NOTIFY | ATT_CLIENT_CFG_INDICATE) ?
            GattServerEvents::GATT_EVENT_UPDATES_ENABLED :
            GattServerEvents::GATT_EVENT_UPDATES_DISABLED;

    getInstance().handleEvent(evt_type, evt->hdr.param, evt->handle);
}

void GattServer::att_cb(const attEvt_t *evt)
{
    if (evt->hdr.status == ATT_SUCCESS && evt->hdr.event == ATT_MTU_UPDATE_IND) {
        ble::GattServer::EventHandler *handler = getInstance().getEventHandler();
        tr_info("Connection %d: Att MTU changed, new MTU size is %d bytes", evt->hdr.param, evt->mtu);
        if (handler) {
            handler->onAttMtuChange(evt->hdr.param, evt->mtu);
        }
    } else if (evt->hdr.status == ATT_SUCCESS && evt->hdr.event == ATTS_HANDLE_VALUE_CNF) {
        getInstance().handleEvent(GattServerEvents::GATT_EVENT_DATA_SENT, evt->hdr.param, evt->handle);
    }
}

uint8_t GattServer::atts_read_cb(
    dmConnId_t connId,
    uint16_t handle,
    uint8_t operation,
    uint16_t offset,
    attsAttr_t *pAttr
)
{
    uint8_t err = ATT_SUCCESS;

    char_auth_callback *auth_cb = getInstance().get_auth_callback(handle);
    if (auth_cb && auth_cb->read_cb) {
        GattReadAuthCallbackParams read_auth_params = {
            connId,
            handle,
            offset,
            /* len */ pAttr->maxLen,
            /* data */ nullptr,
            AUTH_CALLBACK_REPLY_SUCCESS
        };

        auth_cb->read_cb.call(&read_auth_params);

        if (read_auth_params.authorizationReply != AUTH_CALLBACK_REPLY_SUCCESS) {
            tr_warning("Request to read attribute %d on connection %d declined with authorization reply %s",
                     handle,
                     connId,
                     to_string(read_auth_params.authorizationReply & 0xFF));

            return read_auth_params.authorizationReply & 0xFF;
        }

        /* if new data provided copy into the attribute value buffer */
        if (read_auth_params.data) {
            if (read_auth_params.len > pAttr->maxLen) {
                tr_error("Read authorisation callback set length larger than maximum attribute length, "
                         "cannot copy data");
                err = ATT_ERR_UNLIKELY;
            } else {
                memcpy(pAttr->pValue, read_auth_params.data, read_auth_params.len);
                *pAttr->pLen = read_auth_params.len;

                if (read_auth_params.len < offset) {
                    tr_warning("Read authorisation callback shortened data beyond current offset, "
                               "current read will fail");
                    err = ATT_ERR_OFFSET;
                }
            }
        }
    }

    tr_debug("Read attribute %d on connection %d - value=%s",
            handle,
            connId,
            mbed_trace_array(pAttr->pValue, *pAttr->pLen));

    GattReadCallbackParams read_params = {
        connId,
        handle,
        offset,
        *pAttr->pLen,
        pAttr->pValue,
        /* status */ (err == ATT_SUCCESS) ? BLE_ERROR_NONE : BLE_ERROR_PARAM_OUT_OF_RANGE
    };
    getInstance().handleDataReadEvent(&read_params);

    return err;
}

uint8_t GattServer::atts_write_cb(
    dmConnId_t connId,
    uint16_t handle,
    uint8_t operation,
    uint16_t offset,
    uint16_t len,
    uint8_t *pValue,
    attsAttr_t *pAttr
)
{
    tr_debug("Connection %d: Write attribute %d, operation=%d, offset=%d, value=%s",
            connId, handle, operation, offset, mbed_trace_array(pValue, len));

    char_auth_callback* auth_cb = getInstance().get_auth_callback(handle);
    if (auth_cb && auth_cb->write_cb) {
        GattWriteAuthCallbackParams write_auth_params = {
            connId,
            handle,
            offset,
            len,
            pValue,
            AUTH_CALLBACK_REPLY_SUCCESS
        };

        auth_cb->write_cb.call(&write_auth_params);

        if (write_auth_params.authorizationReply != AUTH_CALLBACK_REPLY_SUCCESS) {
            tr_warning("Request to write attribute %d on connection %d declined with authorization reply %s",
                     handle,
                     connId,
                     to_string(write_auth_params.authorizationReply & 0xFF));

            return write_auth_params.authorizationReply & 0xFF;
        }
    }

    /* we don't write anything during the prepare phase */
    bool write_happened = (operation != ATT_PDU_PREP_WRITE_REQ);

    MBED_ASSERT(len + offset <= pAttr->maxLen);

    if (write_happened) {
        WsfTaskLock();

        memcpy((pAttr->pValue + offset), pValue, len);

        /* write the length if variable length attribute */
        if ((pAttr->settings & ATTS_SET_VARIABLE_LEN) != 0) {
            *(pAttr->pLen) = offset + len;
        }

        WsfTaskUnlock();
    }

    GattWriteCallbackParams::WriteOp_t writeOp;
    switch (operation) {
        case ATT_PDU_WRITE_REQ:
            writeOp = GattWriteCallbackParams::OP_WRITE_REQ;
            break;
        case ATT_PDU_WRITE_CMD:
            writeOp = GattWriteCallbackParams::OP_WRITE_CMD;
            break;
#if BLE_FEATURE_SIGNING
        case ATT_PDU_SIGNED_WRITE_CMD:
            if (getInstance()._signing_event_handler) {
                getInstance()._signing_event_handler->on_signed_write_received(
                    connId,
                    AttsGetSignCounter(connId)
                );
            }
            writeOp = GattWriteCallbackParams::OP_SIGN_WRITE_CMD;
            break;
#endif // BLE_FEATURE_SIGNING
        case ATT_PDU_PREP_WRITE_REQ:
            writeOp = GattWriteCallbackParams::OP_PREP_WRITE_REQ;
            break;
        case ATT_PDU_EXEC_WRITE_REQ:
            writeOp = GattWriteCallbackParams::OP_EXEC_WRITE_REQ_NOW;
            break;
        default:
            writeOp = GattWriteCallbackParams::OP_INVALID;
            break;
    }

    if (write_happened) {
        GattWriteCallbackParams write_params = {
            connId,
            handle,
            writeOp,
            offset,
            len,
            pValue
        };

        getInstance().handleDataWrittenEvent(&write_params);
    }

    return ATT_SUCCESS;
}


uint8_t GattServer::atts_auth_cb(dmConnId_t connId, uint8_t permit, uint16_t handle)
{
    tr_debug("Authenticate R/W request for attribute %d on connection %d", handle, connId);

#if BLE_FEATURE_SECURITY
    // this CB is triggered when read or write of an attribute (either a value
    // handle or a descriptor) requires secure connection security.
    ble::SecurityManager &security_manager = impl::BLEInstanceBase::deviceInstance().getSecurityManager();

    link_encryption_t encryption(link_encryption_t::NOT_ENCRYPTED);
    ble_error_t err = security_manager.getLinkEncryption(connId, &encryption);
    if (err) {
        tr_error("Insufficient authentication");
        return ATT_ERR_AUTH;
    }

    if (encryption != link_encryption_t::ENCRYPTED_WITH_SC_AND_MITM) {
        tr_error("The link must be secure and authenticated with a secure connection key");
        return ATT_ERR_AUTH;
    }

    return ATT_SUCCESS;
#else
    return ATT_ERR_AUTH;
#endif
}


void GattServer::add_generic_access_service()
{
    ++currentHandle;
    generic_access_service.service.pNext = nullptr;
    generic_access_service.service.startHandle = currentHandle;
    generic_access_service.service.readCback = atts_read_cb;
    generic_access_service.service.writeCback = atts_write_cb;

    // bind attributes to the service
    generic_access_service.service.pAttr = generic_access_service.attributes;

    attsAttr_t *current_attribute = generic_access_service.attributes;

    // service attribute
    current_attribute->pUuid = attPrimSvcUuid;
    current_attribute->pValue = (uint8_t *) attGapSvcUuid;
    current_attribute->maxLen = sizeof(attGapSvcUuid);
    current_attribute->pLen = &current_attribute->maxLen;
    current_attribute->settings = 0;
    current_attribute->permissions = ATTS_PERMIT_READ;

    tr_info("Add Generic Access Service to the Gatt Server: handle=%d", currentHandle);

    // device name declaration
    currentHandle += 2; // note: incremented by two to get a pointer to the value handle
    ++current_attribute;

    // set properties
    generic_access_service.device_name_declaration_value[0] = ATT_PROP_READ;
    // set value handle
    memcpy(generic_access_service.device_name_declaration_value + 1, &currentHandle, sizeof(currentHandle));
    // set the characteristic UUID
    memcpy(generic_access_service.device_name_declaration_value + 3, attDnChUuid, sizeof(attDnChUuid));

    current_attribute->pUuid = attChUuid;
    current_attribute->pValue = generic_access_service.device_name_declaration_value;
    current_attribute->maxLen = sizeof(generic_access_service.device_name_declaration_value);
    current_attribute->pLen = &current_attribute->maxLen;
    current_attribute->settings = 0;
    current_attribute->permissions = ATTS_PERMIT_READ;

    // device name value
    ++current_attribute;
    generic_access_service.device_name_length = 0;
    current_attribute->pUuid = attDnChUuid;
    current_attribute->maxLen = 248;
    current_attribute->pLen = &generic_access_service.device_name_length;
    current_attribute->pValue = nullptr;
    current_attribute->settings = ATTS_SET_VARIABLE_LEN;
    current_attribute->permissions = ATTS_PERMIT_READ;

    tr_info("Add Device Name characteristic: handle=%d", currentHandle);

    // appearance declaration
    currentHandle += 2; // note: incremented by two to get a pointer to the value handle
    ++current_attribute;

    // set properties
    generic_access_service.appearance_declaration_value[0] = ATT_PROP_READ;
    // set value handle
    memcpy(generic_access_service.appearance_declaration_value + 1, &currentHandle, sizeof(currentHandle));
    // set the characteristic UUID
    memcpy(generic_access_service.appearance_declaration_value + 3, attApChUuid, sizeof(attApChUuid));

    current_attribute->pUuid = attChUuid;
    current_attribute->pValue = generic_access_service.appearance_declaration_value;
    current_attribute->maxLen = sizeof(generic_access_service.appearance_declaration_value);
    current_attribute->pLen = &current_attribute->maxLen;
    current_attribute->settings = 0;
    current_attribute->permissions = ATTS_PERMIT_READ;

    // appearance value
    ++current_attribute;
    generic_access_service.appearance = 0; // unknown appearance
    current_attribute->pUuid = attApChUuid;
    current_attribute->maxLen = sizeof(generic_access_service.appearance);
    current_attribute->pLen = &current_attribute->maxLen;
    current_attribute->pValue = (uint8_t *) &generic_access_service.appearance;
    current_attribute->settings = 0;
    current_attribute->permissions = ATTS_PERMIT_READ;

    tr_info("Add Appearance characteristic: handle=%d", currentHandle);

    // peripheral preferred connection parameters declaration
    currentHandle += 2; // note: incremented by two to get a pointer to the value handle
    ++current_attribute;

    // set properties
    generic_access_service.ppcp_declaration_value[0] = ATT_PROP_READ;
    // set value handle
    memcpy(generic_access_service.ppcp_declaration_value + 1, &currentHandle, sizeof(currentHandle));
    // set the characteristic UUID
    memcpy(generic_access_service.ppcp_declaration_value + 3, attPpcpChUuid, sizeof(attPpcpChUuid));

    current_attribute->pUuid = attChUuid;
    current_attribute->pValue = generic_access_service.ppcp_declaration_value;
    current_attribute->maxLen = sizeof(generic_access_service.ppcp_declaration_value);
    current_attribute->pLen = &current_attribute->maxLen;
    current_attribute->settings = 0;
    current_attribute->permissions = ATTS_PERMIT_READ;

    // peripheral preferred connection parameters value
    ++current_attribute;
    const uint8_t default_ppcp_value[] = {
        0xFF, 0xFF, // no specific min connection interval
        0xFF, 0xFF, // no specific max connection interval
        0x00, 0x00, // no slave latency
        0xFF, 0xFF // no specific connection supervision timeout
    };
    memcpy(&generic_access_service.ppcp, default_ppcp_value, sizeof(default_ppcp_value));
    current_attribute->pUuid = attPpcpChUuid;
    current_attribute->maxLen = sizeof(generic_access_service.ppcp);
    current_attribute->pLen = &current_attribute->maxLen;
    current_attribute->pValue = generic_access_service.ppcp;
    current_attribute->settings = 0;
    current_attribute->permissions = ATTS_PERMIT_READ;

    tr_info("Add Peripheral Preferred Connection characteristic: handle=%d", currentHandle);

    generic_access_service.service.endHandle = currentHandle;
    AttsAddGroup(&generic_access_service.service);
}

void GattServer::add_generic_attribute_service()
{
    ++currentHandle;
    generic_attribute_service.service.pNext = nullptr;
    generic_attribute_service.service.startHandle = currentHandle;
    generic_attribute_service.service.readCback = atts_read_cb;
    generic_attribute_service.service.writeCback = atts_write_cb;

    // bind attributes to the service
    generic_attribute_service.service.pAttr = generic_attribute_service.attributes;

    attsAttr_t *current_attribute = generic_attribute_service.attributes;

    // service attribute
    current_attribute->pUuid = attPrimSvcUuid;
    current_attribute->pValue = (uint8_t *) attGattSvcUuid;
    current_attribute->maxLen = sizeof(attGattSvcUuid);
    current_attribute->pLen = &current_attribute->maxLen;
    current_attribute->settings = 0;
    current_attribute->permissions = ATTS_PERMIT_READ;

    tr_info("Add Generic Attribute Service to the Gatt Server: handle=%d", currentHandle);

    // service changed declaration
    currentHandle += 2; // note: incremented by two to get a pointer to the value handle
    ++current_attribute;

    // set properties
    generic_attribute_service.service_changed_declaration[0] = ATT_PROP_INDICATE;
    // set value handle
    memcpy(generic_attribute_service.service_changed_declaration + 1, &currentHandle, sizeof(currentHandle));
    // set the characteristic UUID
    memcpy(generic_attribute_service.service_changed_declaration + 3, attScChUuid, sizeof(attScChUuid));

    current_attribute->pUuid = attChUuid;
    current_attribute->pValue = generic_attribute_service.service_changed_declaration;
    current_attribute->maxLen = sizeof(generic_attribute_service.service_changed_declaration);
    current_attribute->pLen = &current_attribute->maxLen;
    current_attribute->settings = 0;
    current_attribute->permissions = ATTS_PERMIT_READ;

    // service changed value
    ++current_attribute;
    current_attribute->pUuid = attScChUuid;
    current_attribute->maxLen = 0;
    current_attribute->pLen = &current_attribute->maxLen;
    current_attribute->pValue = nullptr;
    current_attribute->settings = 0;
    current_attribute->permissions = 0;

    tr_info("Add Service Changed characteristic: handle=%d", currentHandle);

    // CCCD
    ++current_attribute;
    current_attribute->pUuid = attCliChCfgUuid;
    current_attribute->pValue = (uint8_t *) &cccd_values[cccd_cnt];
    current_attribute->maxLen = 2;
    current_attribute->pLen = &current_attribute->maxLen;
    current_attribute->settings = ATTS_SET_CCC;
    current_attribute->permissions = (ATTS_PERMIT_READ | ATTS_PERMIT_WRITE);

    cccds[cccd_cnt].handle = currentHandle;
    cccds[cccd_cnt].valueRange = ATT_CLIENT_CFG_INDICATE;
    cccds[cccd_cnt].secLevel = DM_SEC_LEVEL_NONE;
    cccd_handles[cccd_cnt] = currentHandle - 1;
    cccd_cnt++;

    generic_attribute_service.service.endHandle = currentHandle;
    AttsAddGroup(&generic_attribute_service.service);
    AttsCccRegister(cccd_cnt, (attsCccSet_t *) cccds, cccd_cb);
}

void *GattServer::alloc_block(size_t block_size)
{
    auto *block = (alloc_block_t *) malloc(sizeof(alloc_block_t) + block_size);
    if (block == nullptr) {
        return nullptr;
    }

    if (allocated_blocks) {
        block->next = allocated_blocks;
    } else {
        block->next = nullptr;
    }

    allocated_blocks = block;

    return block->data;
}

GattServer::char_auth_callback *GattServer::get_auth_callback(uint16_t value_handle)
{
    GattServer::char_auth_callback* current = _auth_callbacks;
    while (current) {
        if (current->handle == value_handle) {
            break;
        }
        current = current->_next;
    }
    return current;
}

bool GattServer::get_cccd_index_by_cccd_handle(GattAttribute::Handle_t cccd_handle, uint8_t &idx) const
{
    for (idx = 0; idx < cccd_cnt; idx++) {
        if (cccd_handle == cccds[idx].handle) {
            return true;
        }
    }
    return false;
}

bool GattServer::get_cccd_index_by_value_handle(GattAttribute::Handle_t char_handle, uint8_t &idx) const
{
    for (idx = 0; idx < cccd_cnt; ++idx) {
        if (char_handle == cccd_handles[idx]) {
            return true;
        }
    }
    return false;
}

bool GattServer::get_value_handle_by_cccd_handle(GattAttribute::Handle_t cccd_handle, GattAttribute::Handle_t &char_handle) const {
    uint8_t idx;
    if (!get_cccd_index_by_cccd_handle(cccd_handle, idx)) {
        return false;
    }
    char_handle = cccd_handles[idx];
    return true;
}

bool GattServer::is_update_authorized(
    connection_handle_t connection,
    GattAttribute::Handle_t value_handle
)
{
    char_auth_callback *auth_cb = get_auth_callback(value_handle);
    if (!auth_cb) {
        return true;
    }

    const att_security_requirement_t sec_req = auth_cb->update_security;

    if (sec_req == att_security_requirement_t::NONE) {
        return true;
    }

#if BLE_FEATURE_SECURITY
    ble::SecurityManager &security_manager = impl::BLEInstanceBase::deviceInstance().getSecurityManager();
    link_encryption_t encryption(link_encryption_t::NOT_ENCRYPTED);
    ble_error_t err = security_manager.getLinkEncryption(connection, &encryption);
    if (err) {
        return false;
    }
#endif // BLE_FEATURE_SECURITY

    switch (sec_req.value()) {
#if BLE_FEATURE_SECURITY
        case att_security_requirement_t::UNAUTHENTICATED:
            if (encryption < link_encryption_t::ENCRYPTED) {
                return false;
            }
            return true;

        case att_security_requirement_t::AUTHENTICATED:
            if (encryption < link_encryption_t::ENCRYPTED_WITH_MITM) {
                return false;
            }
            return true;
#if BLE_FEATURE_SECURE_CONNECTIONS
        case att_security_requirement_t::SC_AUTHENTICATED:
            if (encryption != link_encryption_t::ENCRYPTED_WITH_SC_AND_MITM) {
                return false;
            }
            return true;
#endif // BLE_FEATURE_SECURE_CONNECTIONS
#endif // BLE_FEATURE_SECURITY
        default:
            return false;
    }
}

GattServer::GattServer() :
    eventHandler(nullptr),
    serviceCount(0),
    characteristicCount(0),
    dataSentCallChain(),
    dataWrittenCallChain(),
    dataReadCallChain(),
    updatesEnabledCallback(nullptr),
    updatesDisabledCallback(nullptr),
    confirmationReceivedCallback(nullptr),
#if BLE_FEATURE_SIGNING
    _signing_event_handler(nullptr),
#endif //BLE_FEATURE_SIGNING
    cccds(),
    cccd_values(),
    cccd_handles(),
    cccd_cnt(0),
    _auth_callbacks(nullptr),
    _auth_callbacks_count(0),
    generic_access_service(),
    generic_attribute_service(),
    registered_service(nullptr),
    allocated_blocks(nullptr),
    currentHandle(0),
    default_services_added(false)
{
}

#if BLE_FEATURE_SIGNING
void GattServer::set_signing_event_handler(
    PalSigningMonitorEventHandler *signing_event_handler
)
{
    _signing_event_handler = signing_event_handler;
}
#endif //BLE_FEATURE_SIGNING

void GattServer::onDataSent(const DataSentCallback_t &callback)
{
    dataSentCallChain.add(callback);
}

ble::GattServer::DataSentCallbackChain_t &GattServer::onDataSent()
{
    return dataSentCallChain;
}

ble::GattServer::DataWrittenCallbackChain_t &GattServer::onDataWritten()
{
    return dataWrittenCallChain;
}

ble_error_t GattServer::onDataRead(const DataReadCallback_t &callback)
{
    if (!isOnDataReadAvailable()) {
        return BLE_ERROR_NOT_IMPLEMENTED;
    }

    dataReadCallChain.add(callback);
    return BLE_ERROR_NONE;
}

ble::GattServer::DataReadCallbackChain_t &GattServer::onDataRead()
{
    return dataReadCallChain;
}

void GattServer::onShutdown(const GattServerShutdownCallback_t &callback)
{
    shutdownCallChain.add(callback);
}

GattServer::GattServerShutdownCallbackChain_t &GattServer::onShutdown()
{
    return shutdownCallChain;
}

void GattServer::onUpdatesEnabled(EventCallback_t callback)
{
    updatesEnabledCallback = callback;
}

void GattServer::onUpdatesDisabled(EventCallback_t callback)
{
    updatesDisabledCallback = callback;
}

void GattServer::onConfirmationReceived(EventCallback_t callback)
{
    confirmationReceivedCallback = callback;
}

void GattServer::setEventHandler(EventHandler *handler)
{
    if (handler == nullptr) {
        tr_warning("Setting Gatt Server event handler to a null pointer");
    }
    eventHandler = handler;
}

GattServer::EventHandler *GattServer::getEventHandler()
{
    return eventHandler;
}

void GattServer::handleDataWrittenEvent(const GattWriteCallbackParams *params)
{
    if(eventHandler) {
        eventHandler->onDataWritten(*params);
    }

    // Execute callbacks added with deprecated API
    dataWrittenCallChain.call(params);
}

void GattServer::handleDataReadEvent(const GattReadCallbackParams *params)
{
    if(eventHandler) {
        eventHandler->onDataRead(*params);
    }

    // Execute callbacks added with deprecated API
    dataReadCallChain.call(params);
}

void GattServer::handleEvent(
    GattServerEvents::gattEvent_e type,
    ble::connection_handle_t connHandle,
    GattAttribute::Handle_t attributeHandle
)
{
    // To be used in cases where the characteristic value handle differs from the attribute handle
    GattAttribute::Handle_t charHandle;

    switch (type) {
        case GattServerEvents::GATT_EVENT_UPDATES_ENABLED:
            tr_info("Updates enabled for attribute %d on connection %d", attributeHandle, connHandle);
            MBED_ASSERT(get_value_handle_by_cccd_handle(attributeHandle, charHandle));
            if(eventHandler) {
                GattUpdatesEnabledCallbackParams params({
                    .connHandle = connHandle,
                    .attHandle = attributeHandle,
                    .charHandle = charHandle
                });
                eventHandler->onUpdatesEnabled(params);
            }

            // Execute deprecated callback
            if (updatesEnabledCallback) {
                updatesEnabledCallback(attributeHandle);
            }
            break;
        case GattServerEvents::GATT_EVENT_UPDATES_DISABLED:
            tr_info("Updates disabled for attribute %d on connection %d", attributeHandle, connHandle);
            MBED_ASSERT(get_value_handle_by_cccd_handle(attributeHandle, charHandle));
            if(eventHandler) {
                GattUpdatesDisabledCallbackParams params({
                    .connHandle = connHandle,
                    .attHandle = attributeHandle,
                    .charHandle = charHandle
                });
                eventHandler->onUpdatesDisabled(params);
            }

            // Execute deprecated callback
            if (updatesDisabledCallback) {
                updatesDisabledCallback(attributeHandle);
            }
            break;

        case GattServerEvents::GATT_EVENT_DATA_SENT:
            tr_debug("Data sent for attribute %d on connection %d", attributeHandle, connHandle);
            if(eventHandler) {
                GattDataSentCallbackParams params({
                    .connHandle = connHandle,
                    .attHandle = attributeHandle
                });
                eventHandler->onDataSent(params);
            }

            // Execute deprecated callback
            // Called every time a notification or indication has been sent
            handleDataSentEvent(1);
            break;

        default:
            break;
    }
}

void GattServer::handleDataSentEvent(unsigned count)
{
    dataSentCallChain.call(count);
}

} // namespace impl
} // namespace ble
