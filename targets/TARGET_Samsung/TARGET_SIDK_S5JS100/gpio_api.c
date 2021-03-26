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
#if DEVICE_GPIO
#include "gpio_api.h"

#include "gpio_object.h"
#include "pinmap.h"
#include "PinNames.h"
#include "PeripheralPins.h"
#include "s5js100_error.h"
#include "s5js100_pinconfig.h"

/** Set the given pin as GPIO
 *
 * @param pin The pin to be set as GPIO
 * @return The GPIO port mask for this pin
 **/
uint32_t gpio_set(PinName pin)
{
    int bit;

    MBED_ASSERT(pin != (PinName)NC);
    GPIOName g_name = (GPIOName) pinmap_peripheral(pin, PinMap_GPIO);
    if ((PinName) g_name == NC) {
        return 0;
    }
    bit = (int) g_name;
    return (1UL << bit);
}

/** Checks if gpio object is connected (pin was not initialized with NC)
 * @param obj The GPIO object
 * @return 0 if object was initialized with NC
 * @return non-zero if object was initialized with a valid PinName
 **/
int gpio_is_connected(const gpio_t *obj)
{
    if (obj->pin != (PinName) NC) {
        return 1;
    } else {
        return 0;
    }
}

/** Initialize the GPIO pin
 *
 * @param obj The GPIO object to initialize
 * @param pin The GPIO pin to initialize (may be NC)
 */
void gpio_init(gpio_t *obj, PinName pin)
{
    GPIOName g_name = (GPIOName) pinmap_peripheral(pin, PinMap_GPIO);
    int pincfg = pinmap_function(pin, PinMap_GPIO);
    if ((PinName) g_name == NC || !pincfg) {
        obj->pin = NC;
        obj->pincfg = NC;
        return;
    }

    obj->pin = pin;
    obj->pincfg = pincfg;
    s5js100_configgpio(pincfg);
}

/** Releases the GPIO pin
 *
 * @param obj The GPIO object to release
 */
void gpio_free(gpio_t *obj)
{
    if (obj->pin == NC) {
        return;
    }
    s5js100_unconfiggpio(obj->pincfg);
    memset(obj, NC, sizeof(gpio_t));
}

/** Set the input pin mode
 *
 * @param obj  The GPIO object (must be connected)
 * @param mode The pin mode to be set
 */
void gpio_mode(gpio_t *obj, PinMode mode)
{
    obj->pincfg &= ~GPIO_PUPD_MASK;

    if (mode == PullNone) {
        obj->pincfg |= GPIO_FLOAT;
    } else if (mode == PullUp) {
        obj->pincfg |= GPIO_PULLUP;
    } else if (mode == PullDown) {
        obj->pincfg |= GPIO_PULLDOWN;
    } else {
        //return -EINVAL;
    }
    s5js100_pullup(obj->pincfg);
}

/** Set the pin direction
 *
 * @param obj       The GPIO object (must be connected)
 * @param direction The pin direction to be set
 */
void gpio_dir(gpio_t *obj, PinDirection direction)
{
    obj->pincfg &= ~GPIO_INPUT_MASK;
    obj->pincfg &= ~GPIO_OUTPUT_MASK;

    if (direction == PIN_OUTPUT) {
        obj->pincfg |= GPIO_OUTPUT;
        s5js100_configoutput(obj->pincfg);
    } else if (direction == PIN_INPUT) {
        obj->pincfg |= GPIO_INPUT;
        s5js100_configinput(obj->pincfg);
    }
}

/** Get the pins that support all GPIO tests
 *
 * Return a PinMap array of pins that support GPIO. The
 * array is terminated with {NC, NC, 0}.
 *
 * Targets should override the weak implementation of this
 * function to provide the actual pinmap for GPIO testing.
 *
 * @return PinMap array
 */
const PinMap *gpio_pinmap(void)
{
    return PinMap_GPIO;
}
#endif
