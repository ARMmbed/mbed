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

#ifndef BLE_GAPMOCK_H
#define BLE_GAPMOCK_H

#include "gmock/gmock.h"
#include "GapImpl.h"

class GapMock : public ble::impl::Gap {
public:
    GapMock() {};
    GapMock(const GapMock&) = delete;
    GapMock& operator=(const GapMock&) = delete;
    virtual ~GapMock() {};
};



#endif //BLE_GAPMOCK_H
