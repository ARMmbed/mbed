/* mbed Microcontroller Library
 * Copyright (c) 2021 ARM Limited
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

#include "GattServerImpl_mock.h"

namespace ble {

GattServerMock::GattServerMock()
{
    ON_CALL(*this, addService).WillByDefault([this](GattService &service) {
        // Fake registration, it populates the handles of the input and store its
        // representation in the services field.
        fake_register_services(service);
        return BLE_ERROR_NONE;
    });
}

GattServerMock::~GattServerMock() {};

void GattServerMock::fake_register_services(GattService& gattService)
{
    gattService.setHandle(current_handle++);
    service_t result {
        gattService.getUUID(),
        gattService.getHandle()
    };

    for (size_t i = 0; i < gattService.getCharacteristicCount(); ++i) {
        current_handle++; // Increment for the characteristic declaration handle
        auto& ref = *gattService.getCharacteristic(i);
        ref.getValueAttribute().setHandle(current_handle++);

        characteristic_t c;
        c.uuid = ref.getValueAttribute().getUUID();
        c.value_handle = ref.getValueHandle();
        c.properties = ref.getProperties();
        c.read_security = ref.getReadSecurityRequirement();
        c.write_security = ref.getWriteSecurityRequirement();
        c.update_security = ref.getUpdateSecurityRequirement();
        c.read_cb = ref.getReadAuthorizationCallback();
        c.write_cb = ref.getWriteAuthorizationCallback();
        c.value.reserve(ref.getValueAttribute().getMaxLength());
        c.value.resize(ref.getValueAttribute().getLength());
        {
            auto value_ptr = ref.getValueAttribute().getValuePtr();
            if (value_ptr) {
                std::copy(value_ptr, value_ptr + c.value.size(), c.value.begin());
            }
        }
        c.has_variable_len = ref.getValueAttribute().hasVariableLength();

        for (size_t j = 0; j < ref.getDescriptorCount(); ++j) {
            auto& ref_desc = *ref.getDescriptor(j);
            ref_desc.setHandle(current_handle++);

            descriptor_t d;
            d.uuid = ref_desc.getUUID();
            d.handle = ref_desc.getHandle();
            d.read_security = ref_desc.getReadSecurityRequirement();
            d.write_security = ref_desc.getWriteSecurityRequirement();
            d.is_readable = ref_desc.isReadAllowed();
            d.is_writable = ref_desc.isWriteAllowed();
            d.value.reserve(ref_desc.getMaxLength());
            d.value.resize(ref_desc.getLength());
            {
                auto value_ptr = ref_desc.getValuePtr();
                if (value_ptr) {
                    std::copy(value_ptr, value_ptr + d.value.size(), d.value.begin());
                }
            }

            c.descriptors.push_back(d);
        }

        result.characteristics.push_back(c);
    }

    services.push_back(result);
}


}
