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

#ifndef BLE_GATTSERVERSTUB_H
#define BLE_GATTSERVERSTUB_H

#include "ble/GattServer.h"
#include "generic/GattServerEvents.h"
#include "ble/Gap.h"

namespace ble {
namespace impl {

class GattServer {
public:
    GattServer() {};
    GattServer(const GattServer&) = delete;
    GattServer& operator=(const GattServer&) = delete;
    virtual ~GattServer() {};

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

    virtual void setEventHandler(EventHandler *handler) { };

    virtual ble_error_t reset(ble::GattServer* server) { return BLE_ERROR_NONE; };

    virtual ble_error_t addService(GattService &service) { return BLE_ERROR_NONE; };

    virtual ble_error_t read(
        GattAttribute::Handle_t attributeHandle,
        uint8_t buffer[],
        uint16_t *lengthP
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t read(
        ble::connection_handle_t connectionHandle,
        GattAttribute::Handle_t attributeHandle,
        uint8_t *buffer,
        uint16_t *lengthP
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t write(
        GattAttribute::Handle_t attributeHandle,
        const uint8_t *value,
        uint16_t size,
        bool localOnly = false
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t write(
        ble::connection_handle_t connectionHandle,
        GattAttribute::Handle_t attributeHandle,
        const uint8_t *value,
        uint16_t size,
        bool localOnly = false
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t areUpdatesEnabled(
        const GattCharacteristic &characteristic,
        bool *enabledP
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t areUpdatesEnabled(
        ble::connection_handle_t connectionHandle,
        const GattCharacteristic &characteristic,
        bool *enabledP
    ) { return BLE_ERROR_NONE; };

    virtual ble::Gap::PreferredConnectionParams_t getPreferredConnectionParams() {
        ble::Gap::PreferredConnectionParams_t params = {0}; return params;
    };

    virtual void setPreferredConnectionParams(const ble::Gap::PreferredConnectionParams_t &params) { };

    virtual bool isOnDataReadAvailable() const { return true; };

    virtual void onDataSent(const DataSentCallback_t &callback) { };

    virtual DataSentCallbackChain_t &onDataSent() { static DataSentCallbackChain_t chain; return chain; };

    virtual void onDataWritten(const DataWrittenCallback_t &callback) { };

    virtual DataWrittenCallbackChain_t &onDataWritten() { static DataWrittenCallbackChain_t chain; return chain; };

    virtual ble_error_t onDataRead(const DataReadCallback_t &callback) { return BLE_ERROR_NONE; };

    virtual DataReadCallbackChain_t &onDataRead() { static DataReadCallbackChain_t chain; return chain; };

    virtual void onShutdown(const GattServerShutdownCallback_t &callback) { };

    virtual GattServerShutdownCallbackChain_t &onShutdown() { static GattServerShutdownCallbackChain_t chain; return chain; };

    virtual void onUpdatesEnabled(EventCallback_t callback) { };

    virtual void onUpdatesDisabled(EventCallback_t callback) { };

    virtual void onConfirmationReceived(EventCallback_t callback) { };

    virtual void handleDataWrittenEvent(const GattWriteCallbackParams *params) { };

    virtual void handleDataReadEvent(const GattReadCallbackParams *params) { };

    virtual void handleEvent(
        GattServerEvents::gattEvent_e type,
        ble::connection_handle_t connHandle,
        GattAttribute::Handle_t attributeHandle
    ) { };

    virtual void handleDataSentEvent(unsigned count) { };
};

}
}

#endif //BLE_GATTSERVERSTUB_H
