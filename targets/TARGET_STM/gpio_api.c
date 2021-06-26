/* mbed Microcontroller Library
 *******************************************************************************
 * Copyright (c) 2015, STMicroelectronics
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of STMicroelectronics nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */
#include "mbed_assert.h"
#include "gpio_api.h"
#include "pinmap.h"
#include "mbed_error.h"
#include "pin_device.h"
#include "PeripheralPins.h"

extern const uint32_t ll_pin_defines[16];

// Enable GPIO clock and return GPIO base address
GPIO_TypeDef *Set_GPIO_Clock(uint32_t port_idx)
{
    uint32_t gpio_add = 0;
    switch (port_idx) {
        case PortA:
            gpio_add = GPIOA_BASE;
            __HAL_RCC_GPIOA_CLK_ENABLE();
            break;
        case PortB:
            gpio_add = GPIOB_BASE;
            __HAL_RCC_GPIOB_CLK_ENABLE();
            break;
#if defined(GPIOC_BASE)
        case PortC:
            gpio_add = GPIOC_BASE;
            __HAL_RCC_GPIOC_CLK_ENABLE();
            break;
#endif
#if defined GPIOD_BASE
        case PortD:
            gpio_add = GPIOD_BASE;
            __HAL_RCC_GPIOD_CLK_ENABLE();
            break;
#endif
#if defined GPIOE_BASE
        case PortE:
            gpio_add = GPIOE_BASE;
            __HAL_RCC_GPIOE_CLK_ENABLE();
            break;
#endif
#if defined GPIOF_BASE
        case PortF:
            gpio_add = GPIOF_BASE;
            __HAL_RCC_GPIOF_CLK_ENABLE();
            break;
#endif
#if defined GPIOG_BASE
        case PortG:
#if defined PWR_CR2_IOSV /* TARGET_STM32L4 / TARGET_STM32L5 */
            __HAL_RCC_PWR_CLK_ENABLE();
            HAL_PWREx_EnableVddIO2();
#endif
            gpio_add = GPIOG_BASE;
            __HAL_RCC_GPIOG_CLK_ENABLE();
            break;
#endif
#if defined GPIOH_BASE
        case PortH:
            gpio_add = GPIOH_BASE;
            __HAL_RCC_GPIOH_CLK_ENABLE();
            break;
#endif
#if defined GPIOI_BASE
        case PortI:
            gpio_add = GPIOI_BASE;
            __HAL_RCC_GPIOI_CLK_ENABLE();
            break;
#endif
#if defined GPIOJ_BASE
        case PortJ:
            gpio_add = GPIOJ_BASE;
            __HAL_RCC_GPIOJ_CLK_ENABLE();
            break;
#endif
#if defined GPIOK_BASE
        case PortK:
            gpio_add = GPIOK_BASE;
            __HAL_RCC_GPIOK_CLK_ENABLE();
            break;
#endif
        default:
            error("Pinmap error: wrong port number.");
            break;
    }
    return (GPIO_TypeDef *) gpio_add;
}

uint32_t gpio_set(PinName pin)
{
    MBED_ASSERT(pin != (PinName)NC);

    pin_function(pin, STM_PIN_DATA(STM_MODE_INPUT, GPIO_NOPULL, 0));

    return (uint32_t)(1 << ((uint32_t)pin & 0xF)); // Return the pin mask
}


void gpio_init(gpio_t *obj, PinName pin)
{
    obj->pin = pin;
    if (pin == (PinName)NC) {
        return;
    }

    uint32_t port_index = STM_PORT(pin);

    // Enable GPIO clock
    GPIO_TypeDef *gpio = Set_GPIO_Clock(port_index);

    // Fill GPIO object structure for future use
    obj->mask    = gpio_set(pin);
    obj->gpio    = gpio;
    obj->ll_pin  = ll_pin_defines[STM_PIN(obj->pin)];
    obj->reg_in  = &gpio->IDR;
    obj->reg_set = &gpio->BSRR;
#ifdef GPIO_IP_WITHOUT_BRR
    obj->reg_clr = &gpio->BSRR;
#else
    obj->reg_clr = &gpio->BRR;
#endif
}

void gpio_mode(gpio_t *obj, PinMode mode)
{
    pin_mode(obj->pin, mode);
}

inline void gpio_dir(gpio_t *obj, PinDirection direction)
{
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_GPIO_SEMID)) {
    }
#endif /* DUAL_CORE */

    if (direction == PIN_INPUT) {
        LL_GPIO_SetPinMode(obj->gpio, obj->ll_pin, LL_GPIO_MODE_INPUT);
    } else {
        LL_GPIO_SetPinMode(obj->gpio, obj->ll_pin, LL_GPIO_MODE_OUTPUT);
    }

#if defined(DUAL_CORE) && (TARGET_STM32H7)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_GPIO_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
}

#if GPIO_PINMAP_READY
/* If this macro is defined, then PinMap_GPIO is present in PeripheralPins.c */
const PinMap *gpio_pinmap()
{
    return PinMap_GPIO;
}


void gpio_get_capabilities(gpio_t *obj, gpio_capabilities_t *cap)
{
    switch (pinmap_find_function(obj->pin, PinMap_GPIO)) {
        case GPIO_NOPULL:
            cap->pull_none = 1;
            cap->pull_down = 1;
            cap->pull_up = 1;
            break;
        case GPIO_PULLUP:
            cap->pull_none = 1;
            cap->pull_down = 0;
            cap->pull_up = 1;
            break;
        case GPIO_PULLDOWN:
            cap->pull_none = 1;
            cap->pull_down = 1;
            cap->pull_up = 0;
            break;
        default:
            break;
    }
}

#endif
