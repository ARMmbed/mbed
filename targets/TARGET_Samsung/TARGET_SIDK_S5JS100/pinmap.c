/****************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include "mbed_assert.h"
#include "pinmap.h"
#include "mbed_error.h"
#include "PeripheralNames.h"

void pin_function(PinName pin, int function)
{
    MBED_ASSERT(pin != (PinName)NC);

}

void pin_mode(PinName pin, PinMode mode)
{
    MBED_ASSERT(pin != (PinName)NC);
}

static const PinName ff_default_pins[] = {
    D0, D1, D2, D3, D4, D5, D6, D7,
    D8, D9, D10, D11, D12, D13, D14, D15,
    A0, A1, A2, A3, A4, A5
};

static const char *ff_default_names[] = {
    "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
    "D8", "D9", "D10", "D11", "D12", "D13", "D14", "D15",
    "A0", "A1", "A2", "A3", "A4", "A5"
};

static const PinList ff_default_list = {
    sizeof(ff_default_pins) / sizeof(ff_default_pins[0]),
    ff_default_pins
};

const PinList *pinmap_ff_default_pins()
{
    return &ff_default_list;
}

const char *pinmap_ff_default_pin_to_string(PinName pin)
{
    if (pin == NC) {
        return "NC";
    }
    for (size_t i = 0; i < ff_default_list.count; i++) {
        if (ff_default_list.pins[i] == pin) {
            return ff_default_names[i];
        }
    }
    return "Unknown";
}

const PinList *pinmap_restricted_pins()
{
    static const PinName pins[] = {
        A3, A4, A5, D0, D7, D8
    };
    static const PinList pin_list = {
        sizeof(pins) / sizeof(pins[0]),
        pins
    };
    return &pin_list;
}

//*** Default restricted peripherals ***
const PeripheralList *pinmap_restricted_peripherals()
{
    static const PeripheralList peripheral_list = {
        0,
        0
    };
    return &peripheral_list;
}
