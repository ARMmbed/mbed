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
#ifndef MBED_PERIPHERALNAMES_H
#define MBED_PERIPHERALNAMES_H

#include "cmsis.h"
#include "PinNames.h"

typedef enum {
    UART_0 = 0,
    UART_1,
} UARTName;

typedef enum {
    ADC0 = 0,
} ADCName;

typedef enum {
    SPI_0 = 0,
    SPI_1
} SPIName;

typedef enum {
    I2C_0 = 0,
    I2C_1
} I2CName;

typedef enum {
    PWM_0 = 0,
    PWM_1,
    PWM_2,
    PWM_3,
    PWM_4,
    PWM_5,
    PWM_6,
    PWM_7
} PWMName;

/*  Defines to be used by application */
typedef enum {
    PIN_INPUT = 0,
    PIN_OUTPUT
} PinDirection;

typedef enum {
    PullNone          = 0,
    PullUp            = 1,
    PullDown          = 2,
    OpenDrainPullUp   = 3,
    OpenDrainNoPull   = 4,
    OpenDrainPullDown = 5,
    PushPullNoPull    = PullNone,
    PushPullPullUp    = PullUp,
    PushPullPullDown  = PullDown,
    OpenDrain         = OpenDrainPullUp,
    PullDefault       = PullNone
} PinMode;

#define STDIO_UART_TX     CONSOLE_TX
#define STDIO_UART_RX     CONSOLE_RX
#define STDIO_UART        uart0

// Default peripherals
#define MBED_SPI0         p5, p6, p7, p8
#define MBED_SPI1         p11, p12, p13, p14

#define MBED_UART0        p9, p10
#define MBED_UART1        p13, p14
#define MBED_UART2        p28, p27
#define MBED_UARTUSB      CONSOLE_TX, CONSOLE_RX

#define MBED_I2C0         p28, p27
#define MBED_I2C1         p9, p10

#define MBED_ANALOGIN0    p15
#define MBED_ANALOGIN1    p16
#define MBED_ANALOGIN2    p17
#define MBED_ANALOGIN3    p18

#define MBED_PWMOUT0      p26
#define MBED_PWMOUT1      p25
#define MBED_PWMOUT2      p24
#define MBED_PWMOUT3      p23
#define MBED_PWMOUT4      p22
#define MBED_PWMOUT5      p21

#endif
