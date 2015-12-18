/* mbed Microcontroller Library
 * Copyright (c) 2006-2015 ARM Limited
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
#include "mbed_assert.h"
#include "PinNames.h"
#include "gpio_object.h"
#include "gpio_api.h"
#include "compiler.h"

uint32_t gpio_set(PinName pin)
{
    MBED_ASSERT(pin != (PinName)NC);
}

void gpio_init(gpio_t *obj, PinName pin)
{
    MBED_ASSERT(pin != (PinName)NC);
}

void gpio_mode(gpio_t *obj, PinMode mode)
{
    MBED_ASSERT(obj->pin != (PinName)NC);
}

void gpio_dir(gpio_t *obj, PinDirection direction)
{
    MBED_ASSERT(obj->pin != (PinName)NC);
}
