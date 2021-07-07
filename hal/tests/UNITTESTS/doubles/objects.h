/*
 * Copyright (c) 2017, Arm Limited and affiliates.
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

#ifndef MBED_OBJECTS_H
#define MBED_OBJECTS_H

#include <stdint.h>
#include "PeripheralNames.h"
#include "PinNames.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gpio_irq_s {
    uint32_t ch;
};

struct serial_s {
    int x;
};

struct dac_s {
    int unused;
};

struct i2c_s {
    int unused;
};

struct qspi_s {
    int unused;
};

struct spi_s {
    int unused;
};

struct analogin_s {
    int unused;
};

struct port_s {
    int unused;
};

struct pwmout_s {
    int unused;
};

struct flash_s {
    int unused;
};

struct can_s {
    int unused;
};

#include "gpio_object.h"

#ifdef __cplusplus
}
#endif

#endif
