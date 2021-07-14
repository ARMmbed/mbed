/**
 * @file      system_gpio.c
 *
 * @brief     MCU GPIO-related functions
 *
 * Revised BSD License
 * Copyright Semtech Corporation 2020. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "system_gpio.h"

#include "stm32wb55xx.h"
#include "stm32wbxx_ll_bus.h"
#include "stm32wbxx_ll_exti.h"
#include "stm32wbxx_ll_system.h"
#include "stm32wbxx_ll_gpio.h"


void system_gpio_set_pin_state( gpio_t gpio, const system_gpio_pin_state_t state )
{
    switch( state )
    {
    case SYSTEM_GPIO_PIN_STATE_LOW:
        LL_GPIO_ResetOutputPin( gpio.port, gpio.pin );
        break;
    case SYSTEM_GPIO_PIN_STATE_HIGH:
        LL_GPIO_SetOutputPin( gpio.port, gpio.pin );
        break;
    default:
        break;
    }
}

system_gpio_pin_state_t system_gpio_get_pin_state( gpio_t gpio )
{
    if( LL_GPIO_IsInputPinSet( gpio.port, gpio.pin ) )
    {
        return SYSTEM_GPIO_PIN_STATE_HIGH;
    }
    else
    {
        return SYSTEM_GPIO_PIN_STATE_LOW;
    }
}

void system_gpio_wait_for_state( gpio_t gpio, system_gpio_pin_state_t state )
{
    if( state == SYSTEM_GPIO_PIN_STATE_LOW )
    {
        while( LL_GPIO_IsInputPinSet( gpio.port, gpio.pin ) )
        {
        };
    }
    else
    {
        while( !LL_GPIO_IsInputPinSet( gpio.port, gpio.pin ) )
        {
        };
    }
}