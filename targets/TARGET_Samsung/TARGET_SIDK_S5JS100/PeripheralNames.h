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
/* @file  : PeriPheralNames.h
 * @brief : supported peripheral list header
 * @date  : June 2019
 *
 * @note  : List up chip dependent peripherals
 *
 */

#ifndef MBED_PERIPHERALNAMES_H
#define MBED_PERIPHERALNAMES_H

#include "cmsis.h"
#include "spi_def.h"
#include "pwmout_def.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GPIO_0 = 0,
    GPIO_1,
    GPIO_2,
    GPIO_3,
    GPIO_4,
    GPIO_5,
    GPIO_6,
    GPIO_7,
    GPIO_8,
    GPIO_9,
    GPIO_10,
    GPIO_11,
    GPIO_12,
    GPIO_13,
    GPIO_14,
    GPIO_15,
    GPIO_16,
    GPIO_17,
    GPIO_18,
    GPIO_19,
    GPIO_20,
    GPIO_21,
    GPIO_22,
    GPIO_23,
    GPIO_24,
    GPIO_25,
    GPIO_26,
    GPIO_27,
    GPIO_28,
    GPIO_29,
    GPIO_30,
    GPIO_31,
    GPIO_32,
    GPIO_33,
    GPIO_34,
    GPIO_35,
    GPIO_36,
    GPIO_37,
    GPIO_38,
    GPIO_39,
    GPIO_40,
    GPIO_41,
    GPIO_42,
    GPIO_43,
    GPIO_44,
    GPIO_45,
    GPIO_46,
    GPIO_47,
    GPIO_48,
    GPIO_49,
    GPIO_50,
    GPIO_51,
    GPIO_52,
    GPIO_53,
    GPIO_54,
    GPIO_55,
    GPIO_56,
    GPIO_57,
    GPIO_58,
    GPIO_59,
    GPIO_60,
    GPIO_61,
    GPIO_62,
    GPIO_63,
    GPIO_64,
    GPIO_65,
    GPIO_66,
    GPIO_67,
    GPIO_68,
    GPIO_69,
    GPIO_70,
    GPIO_71,
    GPIO_72,
} GPIOName;

typedef enum {
    UART_0 = (int)S5JS100_USI0_BASE,
    UART_1 = (int)S5JS100_USI1_BASE,
    UART_2 = (int)S5JS100_UART0_BASE,
    UART_3 = (int)S5JS100_UART1_BASE,
    UART_4 = 4,
    UART_5 = 5,
} UARTName;

typedef enum {
    I2C_0 = (int)S5JS100_I2C,
} I2CName;

typedef enum {
    ADC0_0 = 0,
    ADC0_1,
    ADC0_2,
    ADC0_3
} ADCName;

typedef enum {
    SPI_0 = (int)SPI0_BASE,
    SPI_1 = (int)SPI1_BASE,
    SPI_2,
} SPIName;

typedef enum {
    PWM_0 = (int)(S5JS100_PWM + S5JS100_PWM_TIMER0_OFFSET),
    PWM_1 = (int)(S5JS100_PWM + S5JS100_PWM_TIMER1_OFFSET),
    PWM_2 = (int)(S5JS100_PWM + S5JS100_PWM_TIMER2_OFFSET),
    PWM_3 = (int)(S5JS100_PWM + S5JS100_PWM_TIMER3_OFFSET),
} PWMName;

typedef enum {
    QSPI_1 = (int)S5JS100_QSPI_SFR
} QSPIName;


#define _UART_TX_(port)             UART ## port ## _TXD
#define _UART_RX_(port)             UART ## port ## _RXD
#define _UART_NAME_(port)           UART_ ## port
#define _UART_TX(port)              _UART_TX_(port)
#define _UART_RX(port)              _UART_RX_(port)
#define _UART_NAME(port)            _UART_NAME_(port)

#ifndef UART_STDIO_PORT
#define STDIO_UART_TX       UART0_TXD
#define STDIO_UART_RX       UART0_RXD
#define STDIO_UART          UART_0
#else
#define STDIO_UART_TX       _UART_TX(UART_STDIO_PORT)
#define STDIO_UART_RX       _UART_RX(UART_STDIO_PORT)
#define STDIO_UART          _UART_NAME(UART_STDIO_PORT)
#endif

#define USBTX   UART0_TXD
#define USBRX   UART0_RXD

#ifdef __cplusplus
}
#endif

#endif
