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

#ifndef BLE_MOCKS_H
#define BLE_MOCKS_H

#include "GattServerImpl_mock.h"
#include "GattClientImpl_mock.h"
#include "GapImpl_mock.h"
#include "SecurityManagerImpl_mock.h"

/***
 * You must use delete_mocks() at the end of the test. BLE::Instance(), ble::gap() etc. inits the mocks. Do not cache
 * pointers between tests. Call BLE::Instance() at the start of the tests, otherwise call init_mocks() yourself.
 * To access mocks use:
 * gap_mock(), gatt_server_mock(), gatt_client_mock(), security_manager_mock().
 * All functions are in namespace ble.
 */
namespace ble {

void init_mocks();
void delete_mocks();

GapMock& gap_mock();
GattServerMock& gatt_server_mock();
GattClientMock& gatt_client_mock();
SecurityManagerMock& security_manager_mock();

}

#endif // BLE_MOCKS_H
