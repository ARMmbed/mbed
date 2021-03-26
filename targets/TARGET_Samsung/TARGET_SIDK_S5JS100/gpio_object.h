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

#ifndef MBED_GPIO_OBJECT_H
#define MBED_GPIO_OBJECT_H

#include "cmsis.h"
#include "mbed_assert.h"
#include "PortNames.h"
#include "PeripheralNames.h"
#include "PinNames.h"
#include "s5js100_gpio.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    PinName pin;
    uint32_t pincfg;
} gpio_t;

static inline void gpio_write(gpio_t *obj, int value)
{
    MBED_ASSERT(obj->pin != (PinName) NC);
    s5js100_gpiowrite(obj->pincfg, (unsigned int) value);
}

static inline int gpio_read(gpio_t *obj)
{
    MBED_ASSERT(obj->pin != (PinName) NC);
    return s5js100_gpioread(obj->pincfg);
}

#ifdef __cplusplus
}
#endif

#endif
