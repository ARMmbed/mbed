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

/* MBED TARGET LIST: S5JS100 */

#ifndef MBED_PINNAMES_H
#define MBED_PINNAMES_H

#include "cmsis.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIN_INPUT,
    PIN_OUTPUT
} PinDirection;

#define PORT_SHIFT  5
#define STATIC_PINMAP_READY 1

typedef enum {
    P00 = 0,
    P01,
    P02,
    P03,
    P04,
    P05,
    P06,
    P07,
    P08,
    P09,
    P10,
    P11,
    P12,
    P13,
    P14,
    P15,
    P16,
    P17,
    P18,
    P19,
    P20,
    P21,
    P22,
    P23,
    P24,
    P25,
    P26,
    P27,
    P28,
    P29,
    P30,
    P31,
    P32,
    P33,
    P34,
    P35,
    P36,
    P37,
    P38,
    P39,
    P40,
    P41,
    P42,
    P43,
    P44,
    P45,
    P46,
    P47,
    P48,
    P49,
    P50,
    P51,
    P52,
    P53,
    P54,
    P55,
    P56,
    P57,
    P58,
    P59,
    P60,
    P61,
    P62,
    P63,
    P64,
    P65,
    P66,
    P67,
    P68,
    P69,
    P70,
    P71,
    P72,
    P73,
    P74,
    P75,
    P76,

    /* Actually Dummy */
    UART4_RXD,
    UART4_TXD,
    UART5_RXD,
    UART5_TXD,

    LED1 = P00,
    LED2 = P01,
    LED3 = P02,
    LED4 = P03,

    USI0_RXD_CLK_SCL = P08,
    USI0_TXD_MOSI_SDA = P09,
    USI0_RTSN_MISO_NA = P10,
    USI0_CTSN_CSN_NA = P11,
    USI1_RXD_CLK_SCL = P12,
    USI1_TXD_MOSI_SDA = P13,
    USI1_RTSN_MISO_NA = P14,
    USI1_CTSN_CSN_NA = P15,

    UART0_RXD = USI0_RXD_CLK_SCL,
    UART0_TXD = USI0_TXD_MOSI_SDA,
    UART0_CTS = USI0_CTSN_CSN_NA,
    UART0_RTS = USI0_RTSN_MISO_NA,
    UART1_RXD = USI1_RXD_CLK_SCL,
    UART1_TXD = USI1_TXD_MOSI_SDA,
    UART1_CTS = USI1_CTSN_CSN_NA,
    UART1_RTS = USI1_RTSN_MISO_NA,

    UART2_RXD = P27,
    UART2_TXD = P28,
    UART2_CTS = P16,
    UART2_RTS = P17,
    UART3_RXD = P29,
    UART3_TXD = P30,
    UART3_CTS = P18,
    UART3_RTS = P19,

    SPI0_RXD = P31,
    SPI0_TXD = P32,
    SPI0_CLK = P33,
    SPI0_CSN = P34,

    I2C0_SCL = P35,
    I2C0_SDA = P36,

    PWM_TOUT0 = P14,
    PWM_TOUT1 = P15,
    PWM_TOUT2 = P22,
    PWM_TOUT3 = P23,

    ADC_CH0 = P73,
    ADC_CH1 = P74,
    ADC_CH2 = P75,
    ADC_CH3 = P76,

    // Arduino connector namings
    A0          = P73,  //adc0
    A1          = P74,  //adc1
    A2          = P75,  //adc2
    A3          = P76,  //adc3
    A4          = P73,
    A5          = P73,
// USI
    D1          = P12,  //usi1 rx
    D2          = P13,  //usi1 tx
    D3          = P14,  //usi1 rts
    D4          = P15,  //usi1 cts
// UART
    D5          = P16,  //uart0 cts
    D6          = P17,  //uart0 rts
    D7          = P27,  //uart0 rx
    D8          = P28,  //uart0 tx
// PWM
    D9          = P22,  //pwm2
    D0          = P23,  //pwm3
// SPI
    D10         = P33,  //spi0 clk
    D11         = P34,  //spi0 cs
    D12         = P31,  //spi0 rx
    D13         = P32,  //spi0 tx
// I2C
    D14         = P36,  //i2c0 sda
    D15         = P35,  //i2c0 scl

    // Not connected
    NC = (int)0xFFFFFFFF,

} PinName;

typedef enum {
    PullUp = 2,
    PullDown = 1,
    PullNone = 0,
    Repeater = 3,
    OpenDrain = 4,
    PullDefault = PullDown
} PinMode;

#ifdef __cplusplus
}
#endif

#endif
