/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-value"

#ifdef __cplusplus
#include <stdint.h>
#include <cstddef>
extern "C" {
#endif
#include "pico.h"
#include "pico/platform.h"
#include "pico/types.h"
#include "pico/assert.h"
#include "pico/time.h"
#include "pico/types.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/resets.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/regs/addressmap.h"
#ifdef __cplusplus
}
#endif

#pragma GCC diagnostic pop

#include "cmsis.h"

#include "PortNames.h"
#include "PeripheralNames.h"
#include "PinNames.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gpio_s {
    PinName  pin;
    PinDirection  direction;
};

struct gpio_irq_s {
    IRQn_Type irq_n;
    uint32_t irq_index;
    uint32_t event;
    PinName pin;
};

struct port_s {
    PortName port;
    uint32_t mask;
    PinDirection direction;
    __IO uint32_t *reg_in;
    __IO uint32_t *reg_out;
};

/* common objects */
struct analogin_s {
    uint8_t channel;
};

struct serial_s {
    uart_inst_t * dev;
    PinName pin_rx;
    PinName pin_tx;
    PinName pin_rts;
    PinName pin_cts;
    uint32_t baud;
};

struct i2c_s {
    i2c_inst_t * dev;
    unsigned int baudrate;

#if DEVICE_I2CSLAVE
    bool was_slave;
    bool is_slave;
    uint8_t slave_addr;
#endif
};

struct spi_s {
    spi_inst_t * dev;
};

struct pwmout_s {
    PinName pin;
    uint8_t slice;
    uint8_t channel;
    uint16_t period;
    float percent;
    pwm_config cfg;
};

struct flash_s {
	/*  nothing to be stored for now */
	uint32_t dummy;
};

typedef struct gpio_s gpio_t;
typedef struct serial_s serial_t;

#ifdef __cplusplus
}
#endif

#endif
