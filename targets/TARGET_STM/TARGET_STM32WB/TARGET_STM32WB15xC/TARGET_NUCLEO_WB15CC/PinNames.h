/* mbed Microcontroller Library
 * SPDX-License-Identifier: BSD-3-Clause
 ******************************************************************************
 *
 * Copyright (c) 2016-2021 STMicroelectronics.
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 *
 * Automatically generated from STM32CubeMX/db/mcu/STM32WB15CCUx.xml
 */

/* MBED TARGET LIST: NUCLEO_WB15CC */

#ifndef MBED_PINNAMES_H
#define MBED_PINNAMES_H

#include "cmsis.h"
#include "PinNamesTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {

    PA_0       = 0x00,
    PA_1       = 0x01,
    PA_2       = 0x02,
    PA_3       = 0x03,
    PA_4       = 0x04,
    PA_5       = 0x05,
    PA_6       = 0x06,
    PA_7       = 0x07,
    PA_8       = 0x08,
    PA_9       = 0x09,
    PA_10      = 0x0A,
    PA_11      = 0x0B,
    PA_12      = 0x0C,
    PA_13      = 0x0D,
    PA_14      = 0x0E,
    PA_15      = 0x0F,
    PB_0       = 0x10,
    PB_1       = 0x11,
    PB_2       = 0x12,
    PB_3       = 0x13,
    PB_4       = 0x14,
    PB_5       = 0x15,
    PB_6       = 0x16,
    PB_7       = 0x17,
    PB_8       = 0x18,
    PB_9       = 0x19,
    PC_14      = 0x2E,
    PC_15      = 0x2F,
    PE_4       = 0x44,
    PH_3       = 0x73,

    /**** ADC internal channels ****/

    ADC_TEMP = 0xF0, // Internal pin virtual value
    ADC_VREF = 0xF1, // Internal pin virtual value
    ADC_VBAT = 0xF2, // Internal pin virtual value

#ifdef TARGET_FF_ARDUINO_UNO
    // Arduino Uno (Rev3) pins
    ARDUINO_UNO_A0  = PA_4,
    ARDUINO_UNO_A1  = PA_6,
    ARDUINO_UNO_A2  = PA_1,
    ARDUINO_UNO_A3  = PA_0,
    ARDUINO_UNO_A4  = PA_2,
    ARDUINO_UNO_A5  = PA_3,

    ARDUINO_UNO_D0  = PB_7,
    ARDUINO_UNO_D1  = PB_6,
    ARDUINO_UNO_D2  = PB_0,
    ARDUINO_UNO_D3  = PA_12,
    ARDUINO_UNO_D4  = PB_1,
    ARDUINO_UNO_D5  = PA_11,
    ARDUINO_UNO_D6  = PA_8,
    ARDUINO_UNO_D7  = PE_4,
    ARDUINO_UNO_D8  = PB_5,
    ARDUINO_UNO_D9  = PA_15,
    ARDUINO_UNO_D10 = PB_2,
    ARDUINO_UNO_D11 = PA_7,
    ARDUINO_UNO_D12 = PB_4,
    ARDUINO_UNO_D13 = PA_5,
    ARDUINO_UNO_D14 = PB_9,
    ARDUINO_UNO_D15 = PB_8,
#endif

    // STDIO for console print
#ifdef MBED_CONF_TARGET_STDIO_UART_TX
    CONSOLE_TX = MBED_CONF_TARGET_STDIO_UART_TX,
#else
    CONSOLE_TX = PA_9,
#endif
#ifdef MBED_CONF_TARGET_STDIO_UART_RX
    CONSOLE_RX = MBED_CONF_TARGET_STDIO_UART_RX,
#else
    CONSOLE_RX = PA_10,
#endif

    /**** OSCILLATOR pins ****/
    RCC_OSC32_IN = PC_14,
    RCC_OSC32_OUT = PC_15,

    /**** DEBUG pins ****/
    SYS_JTCK_SWCLK = PA_14,
    SYS_JTDI = PA_15,
    SYS_JTDO_SWO = PB_3,
    SYS_JTMS_SWDIO = PA_13,
    SYS_JTRST = PB_4,
    SYS_PVD_IN = PB_7,
    SYS_WKUP1 = PA_0,
    SYS_WKUP4 = PA_2,

    // Not connected
    NC = (int)0xFFFFFFFF
} PinName;

// Standardized LED and button names
#define LED1     PB_5   // LD1 [Blue Led]
#define LED2     PB_0   // LD2 [Green Led]
#define LED3     PB_1   // LD3 [Red Led]
#define BUTTON1  PA_0   // B1 [Push Button]
#define BUTTON2  PE_4   // B2 [Push Button]
#define BUTTON3  PA_6   // B3 [Push Button]

#ifdef __cplusplus
}
#endif

#endif
