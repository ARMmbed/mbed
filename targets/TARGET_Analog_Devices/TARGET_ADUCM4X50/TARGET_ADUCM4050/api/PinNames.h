/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
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
#ifndef MBED_PINNAMES_H
#define MBED_PINNAMES_H

/*
	The ADuCM4050 is made in two package variants.

	64 lead LFCSP & 72 ball WLCSP

	There are some differences for Port 2 between the two variants
	WLCSP also has Port 3.

	The #define ADUCM4050_LFCSP is used to determine which variant the code
	is built for.

	For LFCSP leave the #define in, to build for ADUCM4050_WLCSP remove.
*/
#define ADUCM4050_LFCSP

#include "cmsis.h"

#include "adi_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIN_INPUT,
    PIN_OUTPUT
} PinDirection;

//update

#define GPIO_PORT_SHIFT 12


typedef enum {
    P0_00  = (0 << GPIO_PORT_SHIFT | 0 ),
    P0_01  = (0 << GPIO_PORT_SHIFT | 1 ),
    P0_02  = (0 << GPIO_PORT_SHIFT | 2 ),
    P0_03  = (0 << GPIO_PORT_SHIFT | 3 ),
    P0_04  = (0 << GPIO_PORT_SHIFT | 4 ),
    P0_05  = (0 << GPIO_PORT_SHIFT | 5 ),
    P0_06  = (0 << GPIO_PORT_SHIFT | 6 ),
    P0_07  = (0 << GPIO_PORT_SHIFT | 7 ),
    P0_08  = (0 << GPIO_PORT_SHIFT | 8 ),
    P0_09  = (0 << GPIO_PORT_SHIFT | 9 ),
    P0_10 = (0 << GPIO_PORT_SHIFT | 10),
    P0_11 = (0 << GPIO_PORT_SHIFT | 11),
    P0_12 = (0 << GPIO_PORT_SHIFT | 12),
    P0_13 = (0 << GPIO_PORT_SHIFT | 13),
    P0_14 = (0 << GPIO_PORT_SHIFT | 14),
    P0_15 = (0 << GPIO_PORT_SHIFT | 15),
    P1_00  = (1 << GPIO_PORT_SHIFT | 0 ),
    P1_01  = (1 << GPIO_PORT_SHIFT | 1 ),
    P1_02  = (1 << GPIO_PORT_SHIFT | 2 ),
    P1_03  = (1 << GPIO_PORT_SHIFT | 3 ),
    P1_04  = (1 << GPIO_PORT_SHIFT | 4 ),
    P1_05  = (1 << GPIO_PORT_SHIFT | 5 ),
    P1_06  = (1 << GPIO_PORT_SHIFT | 6 ),
    P1_07  = (1 << GPIO_PORT_SHIFT | 7 ),
    P1_08  = (1 << GPIO_PORT_SHIFT | 8 ),
    P1_09  = (1 << GPIO_PORT_SHIFT | 9 ),
    P1_10 = (1 << GPIO_PORT_SHIFT | 10),
    P1_11 = (1 << GPIO_PORT_SHIFT | 11),
    P1_12 = (1 << GPIO_PORT_SHIFT | 12),
    P1_13 = (1 << GPIO_PORT_SHIFT | 13),
    P1_14 = (1 << GPIO_PORT_SHIFT | 14),
    P1_15 = (1 << GPIO_PORT_SHIFT | 15),
    P2_00  = (2 << GPIO_PORT_SHIFT | 0 ),
    P2_01  = (2 << GPIO_PORT_SHIFT | 1 ),
    P2_02  = (2 << GPIO_PORT_SHIFT | 2 ),
    P2_03  = (2 << GPIO_PORT_SHIFT | 3 ),
    P2_04  = (2 << GPIO_PORT_SHIFT | 4 ),
    P2_05  = (2 << GPIO_PORT_SHIFT | 5 ),
    P2_06  = (2 << GPIO_PORT_SHIFT | 6 ),
    P2_07  = (2 << GPIO_PORT_SHIFT | 7 ),
    P2_08  = (2 << GPIO_PORT_SHIFT | 8 ),
    P2_09  = (2 << GPIO_PORT_SHIFT | 9 ),
#if defined(ADUCM4050_LFCSP)
    P2_10 = (2 << GPIO_PORT_SHIFT | 10),
    P2_11 = (2 << GPIO_PORT_SHIFT | 11),
#else
    P2_11 = (2 << GPIO_PORT_SHIFT | 11),
    P2_12 = (2 << GPIO_PORT_SHIFT | 12),
    P2_13 = (2 << GPIO_PORT_SHIFT | 13),
    P2_14 = (2 << GPIO_PORT_SHIFT | 14),
    P2_15 = (2 << GPIO_PORT_SHIFT | 15),
    P3_00  = (3 << GPIO_PORT_SHIFT | 0 ),
    P3_01  = (3 << GPIO_PORT_SHIFT | 1 ),
    P3_02  = (3 << GPIO_PORT_SHIFT | 2 ),
    P3_03  = (3 << GPIO_PORT_SHIFT | 3 ),
#endif

    // mbed original LED naming
    LED1 = P0_13,
    LED2 = P1_12,
    LED3 = P1_13,
    LED4 = P0_13,   //duplicate of led1
    LED5 = P1_12,   //duplicate of led2
    LED6 = P1_13,   //duplicate of led3

    //Push buttons
    PB0 = P1_14,
    PB1 = P2_06,
    BOOT = P1_01,
    WAKE_J12 = P0_15,  //4 options depending on jumper JP10 position
    WAKE_J34 = P1_00,  //4 options depending on jumper JP10 position
    WAKE_J56 = P0_13,  //4 options depending on jumper JP10 position
    WAKE_J78 = P2_01,  //4 options depending on jumper JP10 position

    // USB Pins
    USBTX0 = P0_10,
    USBRX0 = P0_11,
    USBTX = P1_15,
    USBRX = P2_00,
    // Arduino Headers
    D0 = P0_11,
    D1 = P0_10,
    D2 = P0_15,
    D3 = P2_11,
    D4 = P2_01,
    D5 = P2_02,
    D6 = P2_00,
    D7 = P0_12,
    D8 = P1_02,
    D9 = P1_15,
    D10 = P0_03,
    D11 = P0_01,
    D12 = P0_02,
    D13 = P0_00,
    D14 = P0_05,
    D15 = P0_04,

    I2C_SCL = P0_04,
    I2C_SDA = P0_05,

    // SPI Pins
    SPI0_SCLK = P0_00,
    SPI0_MOSI = P0_01,
    SPI0_MISO = P0_02,
    SPI0_CS0 = P0_03,
    SPI0_CS1 = P1_10,
    SPI0_CS2 = P2_08,
    SPI0_CS3 = P2_09,

    SPI1_SCLK = P1_06,
    SPI1_MOSI = P1_07,
    SPI1_MISO = P1_08,
    SPI1_CS0 = P1_09,
    SPI1_CS1 = P2_11,
    SPI1_CS2 = P2_02,
    SPI1_CS3 = P1_10,

    SPI2_SCLK = P1_02,
    SPI2_MOSI = P1_03,
    SPI2_MISO = P1_04,
    SPI2_CS0 = P1_05,
    SPI2_CS1 = P0_09,
#if defined(ADUCM4050_LFCSP)
    SPI2_CS2 = P2_10,
#else
    SPI2_CS2 = P2_15,
#endif
    SPI2_CS3 = P2_07,

// ADC Analog Input Pins
    A0 = P2_03,
    A1 = P2_04,
    A2 = P2_05,
    A3 = P2_06,
    A4 = P2_07,
    A5 = P2_08,

    // Not connected
    NC = (int)0xFFFFFFFF
} PinName;


typedef enum {
    PullNone = 0,
    PullDown = 1,
    PullUp   = 2,
    PullDefault = PullNone
} PinMode;

#ifdef __cplusplus
}
#endif

#endif
