/**
 * @file      configuration.h
 *
 * @brief     LR1110 EVK configuration.
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

#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include "stm32wbxx_ll_gpio.h"
#include "stm32wbxx_ll_spi.h"

#define LR1110_NSS_PORT GPIOA
#define LR1110_NSS_PIN LL_GPIO_PIN_8
#define LR1110_RESET_PORT GPIOA
#define LR1110_RESET_PIN LL_GPIO_PIN_0
#define LR1110_IRQ_PORT GPIOB
#define LR1110_IRQ_PIN LL_GPIO_PIN_4
#define LR1110_BUSY_PORT GPIOB
#define LR1110_BUSY_PIN LL_GPIO_PIN_3

#define LR1110_LED_SCAN_PORT GPIOB
#define LR1110_LED_SCAN_PIN LL_GPIO_PIN_5
#define LR1110_LED_TX_PORT GPIOC
#define LR1110_LED_TX_PIN LL_GPIO_PIN_1
#define LR1110_LED_RX_PORT GPIOC
#define LR1110_LED_RX_PIN LL_GPIO_PIN_0

#define LR1110_LNA_PORT GPIOB
#define LR1110_LNA_PIN LL_GPIO_PIN_0

#define DISPLAY_NSS_PORT GPIOB
#define DISPLAY_NSS_PIN LL_GPIO_PIN_6
#define DISPLAY_DC_PORT GPIOC
#define DISPLAY_DC_PIN LL_GPIO_PIN_7

#define TOUCH_IRQ_PORT GPIOA
#define TOUCH_IRQ_PIN LL_GPIO_PIN_10

#define FLASH_NSS_PORT GPIOB
#define FLASH_NSS_PIN LL_GPIO_PIN_10
#define ACCELERATOR_IRQ_PORT GPIOA
#define ACCELERATOR_IRQ_PIN LL_GPIO_PIN_9

#define ANTENNA_SWITCH_CTRL_PORT ( GPIOC )
#define ANTENNA_SWITCH_CTRL_PIN ( LL_GPIO_PIN_8 )
#define ANTENNA_SWITCH_N_CTRL_PORT ( GPIOC )
#define ANTENNA_SWITCH_N_CTRL_PIN ( LL_GPIO_PIN_6 )

#define BUTTON_BLUE_PORT ( GPIOC )
#define BUTTON_BLUE_PIN ( LL_GPIO_PIN_13 )

typedef struct configuration
{
    GPIO_TypeDef* port;
    uint32_t      pin;
} gpio_t;

typedef struct
{
    SPI_TypeDef* spi;
    gpio_t       nss;
    gpio_t       reset;
    gpio_t       irq;
    gpio_t       busy;
} radio_t;

#endif