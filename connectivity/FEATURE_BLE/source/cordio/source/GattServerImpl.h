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

#ifndef MBED_CORDIO_GATT_SERVER_H__
#define MBED_CORDIO_GATT_SERVER_H__


#include <cstddef>

#include "wsf_types.h"
#include "att_api.h"

#include "ble/GattServer.h"
#include "ble/Gap.h"
#include "ble/SecurityManager.h"

#include "ble/gatt/GattService.h"
#include "ble/gatt/GattAttribute.h"
#include "ble/gatt/GattCallbackParamTypes.h"
#include "ble/gatt/GattCallbackParamTypes.h"

#include "ble/common/CallChainOfFunctionPointersWithContext.h"
#include "ble/common/blecommon.h"

#include "source/BLEInstanceBase.h"
#include "source/generic/GattServerEvents.h"
#if BLE_FEATURE_SIGNING
#include "source/pal/PalSigningMonitor.h"
#endif //BLE_FEATURE_SIGNING

namespace ble {

// fwd declaration of PalAttClient, PalGenericAccessService and BLE
class PalAttClient;
class PalGenericAccessService;
class BLE;

namespace impl {
class GattServer
#if BLE_FEATURE_SIGNING
    : public PalSigningMonitor
#endif //BLE_FEATURE_SIGNING
{
    friend ble::BLE;
    friend ble::PalAttClient;
#if BLE_FEATURE_SIGNING
    friend PalSigningMonitor;
#endif //BLE_FEATURE_SIGNING
    friend PalGenericAccessService;

    using EventHandler = ble::GattServer::EventHandler;
    using DataSentCallback_t = ble::GattServer::DataSentCallback_t ;
    using DataSentCallbackChain_t = ble::GattServer::DataSentCallbackChain_t ;
    using DataWrittenCallback_t = ble::GattServer::DataWrittenCallback_t ;
    using DataWrittenCallbackChain_t = ble::GattServer::DataWrittenCallbackChain_t ;
    using DataReadCallback_t = ble::GattServer::DataReadCallback_t;
    using DataReadCallbackChain_t = ble::GattServer::DataReadCallbackChain_t;
    using GattServerShutdownCallback_t = ble::GattServer::GattServerShutdownCallback_t;
    using GattServerShutdownCallbackChain_t = ble::GattServer::GattServerShutdownCallbackChain_t;
    using EventCallback_t = ble::GattServer::EventCallback_t;

    // inherited typedefs have the wrong types so we have to redefine them
public:
    void setEventHandler(EventHandler *handler);

    ble_error_t reset(ble::GattServer* server);

    ble_error_t addService(GattService &service);

    ble_error_t read(
        GattAttribute::Handle_t attributeHandle,
        uint8_t buffer[],
        uint16_t *lengthP
    );

    ble_error_t read(
        ble::connection_handle_t connectionHandle,
        GattAttribute::Handle_t attributeHandle,
        uint8_t *buffer,
        uint16_t *lengthP
    );

    ble_error_t write(
        GattAttribute::Handle_t attributeHandle,
        const uint8_t *value,
        uint16_t size,
        bool localOnly = false
    );

    ble_error_t write(
        ble::connection_handle_t connectionHandle,
        GattAttribute::Handle_t attributeHandle,
        const uint8_t *value,
        uint16_t size,
        bool localOnly = false
    );

    ble_error_t areUpdatesEnabled(
        const GattCharacteristic &characteristic,
        bool *enabledP
    );

    ble_error_t areUpdatesEnabled(
        ble::connection_handle_t connectionHandle,
        const GattCharacteristic &characteristic,
        bool *enabledP
    );

    ble::Gap::PreferredConnectionParams_t getPreferredConnectionParams();

    void setPreferredConnectionParams(const ble::Gap::PreferredConnectionParams_t &params);

    bool isOnDataReadAvailable() const;

    void onDataSent(const DataSentCallback_t &callback);

    DataSentCallbackChain_t &onDataSent();

    void onDataWritten(const DataWrittenCallback_t &callback)
    {
        dataWrittenCallChain.add(callback);
    }

    DataWrittenCallbackChain_t &onDataWritten();

    ble_error_t onDataRead(const DataReadCallback_t &callback);

    DataReadCallbackChain_t &onDataRead();

    void onShutdown(const GattServerShutdownCallback_t &callback);

    GattServerShutdownCallbackChain_t &onShutdown();

    void onUpdatesEnabled(EventCallback_t callback);

    void onUpdatesDisabled(EventCallback_t callback);

    void onConfirmationReceived(EventCallback_t callback);

    /* Entry points for the underlying stack to report events back to the user. */
    protected:

    void handleDataWrittenEvent(const GattWriteCallbackParams *params);

    void handleDataReadEvent(const GattReadCallbackParams *params);

    void handleEvent(
        GattServerEvents::gattEvent_e type,
        ble::connection_handle_t connHandle,
        GattAttribute::Handle_t attributeHandle
    );

    void handleDataSentEvent(unsigned count);

    /* ===================================================================== */
    /*                    private implementation follows                     */

    #if 0 // Disabled until reworked and reintroduced to GattServer API
    public:
        /**
         * @see ble::GattServer::setDeviceName
         */
        ble_error_t setDeviceName(const uint8_t *deviceName);

        /**
         * @see ble::GattServer::getDeviceName
         */
        void getDeviceName(const uint8_t*& name, uint16_t& length);

        /**
         * @see ble::GattServer::setAppearance
         */
        void setAppearance(GapAdvertisingData::Appearance appearance);

        /**
         * @see ble::GattServer::getAppearance
         */
        GapAdvertisingData::Appearance getAppearance();

    #endif // Disabled until reworked and reintroduced to GattServer API
private:
    struct char_auth_callback {
        /** The registered callback handler for read authorization reply. */
        FunctionPointerWithContext<GattReadAuthCallbackParams *> read_cb;
        /** The registered callback handler for write authorization reply. */
        FunctionPointerWithContext<GattWriteAuthCallbackParams *> write_cb;
        /** built in list */
        char_auth_callback *_next = nullptr;
        /** Characteristic handle the callbacks belong to. */
        ble::attribute_handle_t handle = 0;
        /** security requirement of update operations */
        ble::att_security_requirement_t update_security = ble::att_security_requirement_t::NONE;
    };

public:
    /**
     * Return the singleton of ble::GattServer.
     */
    static GattServer &getInstance();

    static void att_cb(const attEvt_t *pEvt);

    /**
     * Initialize the GattServer and add mandatory services (generic access and
     * generic attribute service).
     */
    void initialize();

    static uint8_t atts_auth_cb(dmConnId_t connId, uint8_t permit, uint16_t handle);
#if BLE_FEATURE_SIGNING
    void set_signing_event_handler(
        PalSigningMonitorEventHandler *signing_event_handler
    ) override;
#endif //BLE_FEATURE_SIGNING
private:

    GattServer();

    GattServer(const GattServer &);

    const GattServer &operator=(const GattServer &);

    EventHandler *getEventHandler();

    void add_default_services();

    static uint16_t compute_attributes_count(GattService &service);

    void insert_service_attribute(
        GattService &service,
        attsAttr_t *&attribute_it
    );

    ble_error_t insert_characteristic(
        GattCharacteristic *characteristic,
        attsAttr_t *&attribute_it
    );

    bool is_characteristic_valid(GattCharacteristic *characteristic);

    void insert_characteristic_declaration_attribute(
        GattCharacteristic *characteristic,
        attsAttr_t *&attribute_it
    );

    ble_error_t insert_characteristic_value_attribute(
        GattCharacteristic *characteristic,
        attsAttr_t *&attribute_it
    );

    ble_error_t insert_descriptor(
        GattCharacteristic *characteristic,
        GattAttribute *descriptor,
        attsAttr_t *&attribute_it,
        bool &cccd_created
    );

    ble_error_t insert_cccd(
        GattCharacteristic *characteristic,
        attsAttr_t *&attribute_it
    );

    static void cccd_cb(attsCccEvt_t *pEvt);

    static uint8_t atts_read_cb(dmConnId_t connId, uint16_t handle, uint8_t operation, uint16_t offset, attsAttr_t *pAttr);

    static uint8_t atts_write_cb(
        dmConnId_t connId,
        uint16_t handle,
        uint8_t operation,
        uint16_t offset,
        uint16_t len,
        uint8_t *pValue,
        attsAttr_t *pAttr
    );

    void add_generic_access_service();

    void add_generic_attribute_service();

    void *alloc_block(size_t block_size);

    char_auth_callback *get_auth_callback(uint16_t value_handle);

    bool get_cccd_index_by_cccd_handle(GattAttribute::Handle_t cccd_handle, uint8_t &idx) const;

    bool get_cccd_index_by_value_handle(GattAttribute::Handle_t char_handle, uint8_t &idx) const;

    bool get_value_handle_by_cccd_handle(GattAttribute::Handle_t cccd_handle, GattAttribute::Handle_t &char_handle) const;

    bool is_update_authorized(connection_handle_t connection, GattAttribute::Handle_t value_handle);

    struct alloc_block_t {
        alloc_block_t *next;
        uint8_t data[1];
    };

    struct internal_service_t {
        attsGroup_t attGroup;
        internal_service_t *next;
    };

private:
    /**
     * Event handler provided by the application.
     */
    EventHandler *eventHandler;

    /**
     * The total number of services added to the ATT table.
     */
    uint8_t serviceCount;

    /**
     * The total number of characteristics added to the ATT table.
     */
    uint8_t characteristicCount;

    /**
     * Callchain containing all registered callback handlers for data sent
     * events.
     */
    DataSentCallbackChain_t dataSentCallChain;

    /**
     * Callchain containing all registered callback handlers for data written
     * events.
     */
    DataWrittenCallbackChain_t dataWrittenCallChain;

    /**
     * Callchain containing all registered callback handlers for data read
     * events.
     */
    DataReadCallbackChain_t dataReadCallChain;

    /**
     * Callchain containing all registered callback handlers for shutdown
     * events.
     */
    GattServerShutdownCallbackChain_t shutdownCallChain;

    /**
     * The registered callback handler for updates enabled events.
     */
    EventCallback_t updatesEnabledCallback;

    /**
     * The registered callback handler for updates disabled events.
     */
    EventCallback_t updatesDisabledCallback;

    /**
     * The registered callback handler for confirmation received events.
     */
    EventCallback_t confirmationReceivedCallback;
#if BLE_FEATURE_SIGNING
    PalSigningMonitorEventHandler *_signing_event_handler;
#endif //BLE_FEATURE_SIGNING

    attsCccSet_t cccds[MBED_CONF_BLE_API_IMPLEMENTATION_MAX_CCCD_COUNT];
    uint16_t cccd_values[MBED_CONF_BLE_API_IMPLEMENTATION_MAX_CCCD_COUNT];
    uint16_t cccd_handles[MBED_CONF_BLE_API_IMPLEMENTATION_MAX_CCCD_COUNT];
    uint8_t cccd_cnt;

    char_auth_callback *_auth_callbacks;
    uint8_t _auth_callbacks_count;

    struct {
        attsGroup_t service;
        attsAttr_t attributes[7];
        uint8_t device_name_declaration_value[5];
        uint16_t device_name_length;
        uint8_t appearance_declaration_value[5];
        uint16_t appearance;
        uint8_t ppcp_declaration_value[5];
        uint8_t ppcp[8];

        uint8_t *&device_name_value()
        {
            return attributes[2].pValue;
        }
    } generic_access_service;

    struct {
        attsGroup_t service;
        attsAttr_t attributes[4];
        uint8_t service_changed_declaration[5];
    } generic_attribute_service;

    internal_service_t *registered_service;
    alloc_block_t *allocated_blocks;

    uint16_t currentHandle;

    bool default_services_added;
};

} // namespace impl
} // ble

#endif /* ifndef MBED_CORDIO_GATT_SERVER_H__ */
