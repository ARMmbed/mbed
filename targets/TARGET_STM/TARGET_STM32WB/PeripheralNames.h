/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
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
#ifndef MBED_PERIPHERALNAMES_H
#define MBED_PERIPHERALNAMES_H

#include "cmsis.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADC_1 = (int)ADC1_BASE
} ADCName;

typedef enum {
    UART_1 = (int)USART1_BASE,
    LPUART_1 = (int)LPUART1_BASE
} UARTName;

#define DEVICE_SPI_COUNT 2
typedef enum {
    SPI_1 = (int)SPI1_BASE,
#if defined SPI2_BASE
    SPI_2 = (int)SPI2_BASE,
#endif
} SPIName;

typedef enum {
    I2C_1 = (int)I2C1_BASE,
#if defined I2C3_BASE
    I2C_3 = (int)I2C3_BASE
#endif
} I2CName;

typedef enum {
    PWM_1  = (int)TIM1_BASE,
#if defined TIM2_BASE
    PWM_2  = (int)TIM2_BASE,
#endif
#if defined TIM16_BASE
    PWM_16 = (int)TIM16_BASE,
#endif
#if defined TIM17_BASE
    PWM_17 = (int)TIM17_BASE
#endif
} PWMName;

#if defined QUADSPI_R_BASE
typedef enum {
    QSPI_1 = (int)QUADSPI_R_BASE
} QSPIName;
#endif

#if defined USB_BASE
typedef enum {
    USB_FS = (int)USB_BASE,
} USBName;
#endif

#ifdef __cplusplus
}
#endif

#endif
