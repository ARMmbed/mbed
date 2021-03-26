/* mbed Microcontroller Library
 * Copyright (c) 2006-2020 ARM Limited
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
/* @file  : gpio_irq_api.c
 * @brief : GPIO interrupt API source file
 * @date  : June 2019
 *
 * @note  : Add chip dependent feature and support up to 72 Pin
 *
 */
#if DEVICE_GPIO
#include <stddef.h>
#include "cmsis.h"
#include "gpio_irq_api.h"
#include "mbed_error.h"
#include "mbed_assert.h"
#include "PeripheralPins.h"
#include "s5js100_pinconfig.h"

#define PIN_NUM (73)

static int last_irq_t;
struct _gpio_irq_info {
    PinName pin;
    uint32_t pincfg;
    uint32_t id;
    gpio_irq_event event;
    gpio_irq_handler handler;
} gpio_irq_info[PIN_NUM] = { 0 };

static void hw_delay_us(unsigned int Value)
{
    volatile unsigned i, j;

    for (i = 0; i < (Value * 2); i++)
        for (j = 0; j < 100; j++)
            ;
}

static inline void handle_interrupt_in(uint32_t channel)
{
    int pin;
    gpio_irq_event event;
    int cur_irq_t;

    pin = s5js100_get_intpin(channel);
    if (pin < 0) {
        return;
    }

#if GPIO_EINT_DEBOUNCE
    cur_irq_t = (int) sys_now();

    if (cur_irq_t - last_irq_t < 10) {
        last_irq_t = cur_irq_t;
        s5js100_gpio_clear_pending(gpio_irq_info[pin].pincfg);
        return;
    }
#endif
    MBED_ASSERT(pin < PIN_NUM);
    hw_delay_us(5000);

    int ret = s5js100_gpioread(gpio_irq_info[pin].pincfg);
    if (ret) {
        event = IRQ_RISE;
    } else {
        event = IRQ_FALL;
    }
    if (gpio_irq_info[pin].handler != NULL) {
        gpio_irq_info[pin].handler(gpio_irq_info[pin].id, event);
    }

    s5js100_gpio_clear_pending(gpio_irq_info[pin].pincfg);
}

void gpio0_irq(void)
{
    handle_interrupt_in(0);
}
void gpio1_irq(void)
{
    handle_interrupt_in(1);
}
void gpio2_irq(void)
{
    handle_interrupt_in(2);
}

/** Initialize the GPIO IRQ pin
 *
 * @param obj     The GPIO object to initialize
 * @param pin     The GPIO pin name
 * @param handler The handler to be attached to GPIO IRQ
 * @param id      The object ID (id != 0, 0 is reserved)
 * @return -1 if pin is NC, 0 otherwise
 */
int gpio_irq_init(gpio_irq_t *obj, PinName pin, gpio_irq_handler handler,
                  uint32_t id)
{
    GPIOName g_name = (GPIOName) pinmap_peripheral(pin, PinMap_GPIO);
    int pincfg = pinmap_function(pin, PinMap_GPIO);
    if ((PinName) g_name == NC || !pincfg) {
        return -1;
    }

    int irqvector = S5JS100_IRQ_GPIO_INTR0 + pin / 32;
    pincfg |= irqvector << GPIO_IRQ_SHIFT;

    obj->pin = pin;
    obj->pincfg = pincfg;

    gpio_irq_info[(int) pin].pincfg = pincfg;
    gpio_irq_info[(int) pin].id = id;
    gpio_irq_info[(int) pin].handler = handler;

    NVIC_SetVector(S5JS100_IRQ_GPIO_INTR0, (uint32_t)gpio0_irq);
    NVIC_SetVector(S5JS100_IRQ_GPIO_INTR1, (uint32_t)gpio1_irq);
    NVIC_SetVector(S5JS100_IRQ_GPIO_INTR2, (uint32_t)gpio2_irq);
#if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    SCB_InvalidateICache();
#endif
    NVIC_EnableIRQ(S5JS100_IRQ_GPIO_INTR0);
    NVIC_EnableIRQ(S5JS100_IRQ_GPIO_INTR1);
    NVIC_EnableIRQ(S5JS100_IRQ_GPIO_INTR2);

    return 0;
}

/** Release the GPIO IRQ PIN
 *
 * @param obj The gpio object
 */
void gpio_irq_free(gpio_irq_t *obj)
{

    if (obj->pin == NC) {
        return;
    }
    gpio_irq_disable(obj);
    s5js100_unconfiggpio(gpio_irq_info[(int) obj->pin].pincfg);
    gpio_irq_info[(int) obj->pin].handler = NULL;
}

/** Enable/disable pin IRQ event
 *
 * @param obj    The GPIO object
 * @param event  The GPIO IRQ event
 * @param enable The enable flag
 */
void gpio_irq_set(gpio_irq_t *obj, gpio_irq_event event, uint32_t enable)
{
    int irqvector;

    MBED_ASSERT(obj->pin != (PinName) NC);
    irqvector = s5js100_gpio_irqvector(obj->pincfg);
    if (!irqvector) {
        return;
    }
    /* clear eint mask */
    gpio_irq_info[(int) obj->pin].event = IRQ_NONE;

    if (enable && event == IRQ_FALL) {
        gpio_irq_info[(int) obj->pin].event = IRQ_FALL;
#if GPIO_EINT_LEVEL
        obj->pincfg &= ~(GPIO_INT_MASK | GPIO_EINT_MASK | GPIO_PUPD_MASK);
        obj->pincfg |= GPIO_EINT | GPIO_EINT_LOW | GPIO_PULLUP;
#else
        if ((obj->pincfg & (GPIO_EINT_MASK | GPIO_INT_MASK)) == (GPIO_EINT_RISING_EDGE | GPIO_EINT)) {
            obj->pincfg &= ~GPIO_EINT_MASK;
            obj->pincfg |= GPIO_EINT | GPIO_EINT_BOTH_EDGE;
        } else {
            obj->pincfg |= GPIO_EINT | GPIO_EINT_FALLING_EDGE;
        }
#endif
    } else if (enable && event == IRQ_RISE) {
        gpio_irq_info[(int) obj->pin].event = IRQ_RISE;
#if GPIO_EINT_LEVEL
        obj->pincfg &= ~(GPIO_INT_MASK | GPIO_EINT_MASK | GPIO_PUPD_MASK);
        obj->pincfg |= GPIO_EINT | GPIO_EINT_HIGH | GPIO_PULLDOWN;
#else
        if ((obj->pincfg & (GPIO_EINT_MASK | GPIO_INT_MASK)) == (GPIO_EINT_FALLING_EDGE | GPIO_EINT)) {
            obj->pincfg &= ~GPIO_EINT_MASK;
            obj->pincfg |= GPIO_EINT | GPIO_EINT_BOTH_EDGE;
        } else {
            obj->pincfg |= GPIO_EINT | GPIO_EINT_RISING_EDGE;
        }
#endif
    } else if (!enable && event == IRQ_RISE) {
        gpio_irq_info[(int) obj->pin].event = IRQ_FALL;
        obj->pincfg &= ~GPIO_EINT_MASK;
        obj->pincfg |= GPIO_EINT | GPIO_EINT_FALLING_EDGE;
    } else if (!enable && event == IRQ_FALL) {
        obj->pincfg &= ~GPIO_EINT_MASK;
        gpio_irq_info[(int) obj->pin].event = IRQ_RISE;
        obj->pincfg |= GPIO_EINT | GPIO_EINT_RISING_EDGE;
    }

    gpio_irq_info[(int) obj->pin].pincfg = obj->pincfg;
    s5js100_configgpio(obj->pincfg);
}

/** Enable GPIO IRQ
 *
 * This is target dependent, as it might enable the entire port or just a pin
 * Enable a group of gpio irq
 * @param obj The GPIO object
 */
void gpio_irq_enable(gpio_irq_t *obj)
{
    MBED_ASSERT(obj->pin != (PinName) NC);
    int irqvector;
    irqvector = s5js100_gpio_irqvector(obj->pincfg);
    if (!irqvector) {
        //return -EINVAL;
    }

    obj->pincfg |= GPIO_EINT;
    gpio_irq_info[(int) obj->pin].pincfg = obj->pincfg;

    NVIC_EnableIRQ((IRQn_Type) irqvector);
    s5js100_configgpio(obj->pincfg);
}

/** Disable GPIO IRQ
 *
 * This is target dependent, as it might disable the entire port or just a pin
 * Disable a group of gpio irq
 * @param obj The GPIO object
 */
void gpio_irq_disable(gpio_irq_t *obj)
{
    MBED_ASSERT(obj->pin != (PinName) NC);
    int irqvector;
    irqvector = s5js100_gpio_irqvector(obj->pincfg);
    if (!irqvector) {
        //return -EINVAL;
    }

    obj->pincfg &= ~GPIO_EINT;
    gpio_irq_info[(int) obj->pin].pincfg = obj->pincfg;

    NVIC_DisableIRQ((IRQn_Type) irqvector);
    s5js100_configgpio(obj->pincfg);
}
#endif
