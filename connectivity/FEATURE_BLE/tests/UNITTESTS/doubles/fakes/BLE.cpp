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

#include "ble/BLE.h"
#include "GattServerImpl_mock.h"
#include "GattClientImpl_mock.h"
#include "GapImpl_mock.h"
#include "SecurityManagerImpl_mock.h"
#include "ble/GattClient.h"
#include "ble/GattServer.h"
#include "ble/SecurityManager.h"
#include "ble/Gap.h"
#include "ble_mocks.h"

namespace ble {

static GapMock *gap_impl = nullptr;
static GattServerMock *gatt_server_impl = nullptr;
static GattClientMock *gatt_client_impl = nullptr;
static SecurityManagerMock *security_manager_impl = nullptr;

static Gap *gap = nullptr;
static GattServer *gatt_server = nullptr;
static GattClient *gatt_client = nullptr;
static SecurityManager *security_manager = nullptr;

GapMock& gap_mock() {
    return *ble::gap_impl;
}

GattServerMock& gatt_server_mock() {
    return *ble::gatt_server_impl;
}

GattClientMock& gatt_client_mock() {
    return *ble::gatt_client_impl;
}

SecurityManagerMock& security_manager_mock() {
    return *ble::security_manager_impl;
}

void init_mocks() {
    if (gap_impl) {
        /* we are already initialised */
        return;
    }

    /* mocks */
    gap_impl = new GapMock();
    gatt_server_impl = new GattServerMock();
    gatt_client_impl = new GattClientMock();
    security_manager_impl = new SecurityManagerMock();
    /* user APIS */
    gap = new Gap(gap_impl);
    gatt_server = new GattServer(gatt_server_impl);
    gatt_client = new GattClient(gatt_client_impl);
    security_manager = new SecurityManager(security_manager_impl);
}

void delete_mocks() {
    delete gap;
    delete gap_impl;
    delete gatt_server;
    delete gatt_server_impl;
    delete gatt_client;
    delete gatt_client_impl;
    delete security_manager;
    delete security_manager_impl;

    gap = nullptr;
    gap_impl = nullptr;
    gatt_server = nullptr;
    gatt_server_impl = nullptr;
    gatt_client = nullptr;
    gatt_client_impl = nullptr;
    security_manager = nullptr;
    security_manager_impl = nullptr;
}

class BLEInstanceBase {
};

BLE::BLE(ble::BLEInstanceBase &transport) : transport(transport)
{
}

BLE& BLE::Instance()
{
    static ble::BLEInstanceBase transport;
    static BLE instance(transport);
    init_mocks();
    return instance;
}

ble::Gap &BLE::gap()
{
    init_mocks();
    return *ble::gap;
}

ble::GattServer &BLE::gattServer()
{
    init_mocks();
    return *ble::gatt_server;
}

ble::GattClient &BLE::gattClient()
{
    init_mocks();
    return *ble::gatt_client;
}

ble::SecurityManager &BLE::securityManager()
{
    init_mocks();
    return *ble::security_manager;
}

const ble::Gap &BLE::gap() const
{
    auto &self = const_cast<BLE &>(*this);
    return const_cast<const ble::Gap &>(self.gap());
}

const ble::GattServer &BLE::gattServer() const
{
    auto &self = const_cast<BLE &>(*this);
    return const_cast<const ble::GattServer &>(self.gattServer());
}

const ble::GattClient &BLE::gattClient() const
{
    auto &self = const_cast<BLE &>(*this);
    return const_cast<const ble::GattClient &>(self.gattClient());
}

const ble::SecurityManager &BLE::securityManager() const
{
    auto &self = const_cast<BLE &>(*this);
    return const_cast<const ble::SecurityManager &>(self.securityManager());
}

void BLE::processEvents()
{

}

}
