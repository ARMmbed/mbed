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
 * Automatically generated from STM32CubeMX/db/mcu/STM32WB5MMGHx.xml
 */

/* MBED TARGET LIST: DISCO_WB5MMG */

#ifndef MBED_PINNAMES_H
#define MBED_PINNAMES_H

#include "cmsis.h"
#include "PinNamesTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void rgb_led_on(uint8_t RedValue, uint8_t GreenValue, uint8_t BlueValue);
void rgb_led_off(void);
void rgb_led_red(void);
void rgb_led_green(void);
void rgb_led_blue(void);


typedef enum {
    ALT0  = 0x100,
} ALTx;

typedef enum {
    PA_0       = 0x00,
    PA_1       = 0x01,
    PA_2       = 0x02,
    PA_3       = 0x03,
    PA_4       = 0x04,
    PA_5       = 0x05,
    PA_6       = 0x06,
    PA_7       = 0x07,
    PA_7_ALT0  = PA_7  | ALT0, // same pin used for alternate HW
    PA_8       = 0x08,
    PA_9       = 0x09,
    PA_10      = 0x0A,
    PA_11      = 0x0B,
    PA_12      = 0x0C,
    PA_13      = 0x0D,
    PA_14      = 0x0E,
    PA_15      = 0x0F,
    PB_2       = 0x12,
    PB_3       = 0x13,
    PB_4       = 0x14,
    PB_5       = 0x15,
    PB_6       = 0x16,
    PB_7       = 0x17,
    PB_8       = 0x18,
    PB_9       = 0x19,
    PB_9_ALT0  = PB_9  | ALT0, // same pin used for alternate HW
    PB_10      = 0x1A,
    PB_11      = 0x1B,
    PB_12      = 0x1C,
    PB_13      = 0x1D,
    PB_14      = 0x1E,
    PB_15      = 0x1F,
    PC_0       = 0x20,
    PC_1       = 0x21,
    PC_2       = 0x22,
    PC_3       = 0x23,
    PC_4       = 0x24,
    PC_5       = 0x25,
    PC_6       = 0x26,
    PC_7       = 0x27,
    PC_8       = 0x28,
    PC_9       = 0x29,
    PC_10      = 0x2A,
    PC_11      = 0x2B,
    PC_12      = 0x2C,
    PC_13      = 0x2D,
    PD_0       = 0x30,
    PD_1       = 0x31,
    PD_2       = 0x32,
    PD_3       = 0x33,
    PD_4       = 0x34,
    PD_5       = 0x35,
    PD_6       = 0x36,
    PD_7       = 0x37,
    PD_8       = 0x38,
    PD_9       = 0x39,
    PD_10      = 0x3A,
    PD_11      = 0x3B,
    PD_12      = 0x3C,
    PD_13      = 0x3D,
    PD_14      = 0x3E,
    PD_15      = 0x3F,
    PE_0       = 0x40,
    PE_1       = 0x41,
    PE_2       = 0x42,
    PE_3       = 0x43,
    PE_4       = 0x44,
    PH_0       = 0x70,
    PH_1       = 0x71,
    PH_3       = 0x73,

    /**** ADC internal channels ****/

    ADC_TEMP = 0xF0, // Internal pin virtual value
    ADC_VREF = 0xF1, // Internal pin virtual value
    ADC_VBAT = 0xF2, // Internal pin virtual value

#ifdef TARGET_FF_ARDUINO_UNO
    // Arduino Uno (Rev3) pins
    ARDUINO_UNO_A0 = PC_3,
    ARDUINO_UNO_A1 = PA_2,
    ARDUINO_UNO_A2 = PA_5,
    ARDUINO_UNO_A3 = PC_1,
    ARDUINO_UNO_A4 = PC_4,
    ARDUINO_UNO_A5 = PC_5,
    ARDUINO_UNO_D0 = PC_0, // connected to LPUART switch
    ARDUINO_UNO_D1 = PB_5, // connected to LPUART switch
    ARDUINO_UNO_D2 = PD_12,
    ARDUINO_UNO_D3 = PD_14,
    ARDUINO_UNO_D4 = PE_4,
    ARDUINO_UNO_D5 = PB_10,
    ARDUINO_UNO_D6 = PE_0,
    ARDUINO_UNO_D7 = PB_2,
    ARDUINO_UNO_D8 = PD_13,
    ARDUINO_UNO_D9 = PD_15,
    ARDUINO_UNO_D10 = PA_4,
    ARDUINO_UNO_D11 = PA_7,
    ARDUINO_UNO_D12 = PB_4,
    ARDUINO_UNO_D13 = PA_1,
    ARDUINO_UNO_D14 = PA_10,
    ARDUINO_UNO_D15 = PB_8,
#endif

    // STDIO for console print
#ifdef MBED_CONF_TARGET_STDIO_UART_TX
    CONSOLE_TX = MBED_CONF_TARGET_STDIO_UART_TX,
#else
    CONSOLE_TX = PB_6,
#endif
#ifdef MBED_CONF_TARGET_STDIO_UART_RX
    CONSOLE_RX = MBED_CONF_TARGET_STDIO_UART_RX,
#else
    CONSOLE_RX = PB_7,
#endif

    /**** USB pins ****/
    USB_DM = PA_11,
    USB_DP = PA_12,
    USB_NOE = PA_13,
    USB_NOE_ALT0 = PC_9,

    /**** DEBUG pins ****/
    SYS_JTCK_SWCLK = PA_14,
    SYS_JTDI = PA_15,
    SYS_JTDO_SWO = PB_3,
    SYS_JTMS_SWDIO = PA_13,
    SYS_JTRST = PB_4,
    SYS_PVD_IN = PB_7,
    SYS_TRACECLK = PE_2,
    SYS_TRACED0 = PD_9,
    SYS_TRACED1 = PC_10,
    SYS_TRACED2 = PD_2,
    SYS_TRACED3 = PC_12,
    SYS_TRIG_INOUT = PD_10,
    SYS_WKUP1 = PA_0,
    SYS_WKUP2 = PC_13,
    SYS_WKUP3 = PC_12,
    SYS_WKUP4 = PA_2,
    SYS_WKUP5 = PC_5,

    /**** QSPI FLASH pins ****/
    QSPI_FLASH1_IO0 = PB_9,
    QSPI_FLASH1_IO1 = PD_5,
    QSPI_FLASH1_IO2 = PD_6,
    QSPI_FLASH1_IO3 = PD_7,
    QSPI_FLASH1_SCK = PA_3,
    QSPI_FLASH1_CSN = PD_3,

    // Not connected
    NC = (int)0xFFFFFFFF
} PinName;

// Standardized LED and button names
#define BUTTON1  PC_12
#define BUTTON2  PC_13

// RBG LED
#define RGB_LED       PA_7  // LD4 is a RGB LED connected to a PWM LED driver
#define GPIO_SELECT2  PH_1  // See User Manual

#ifdef __cplusplus
}
#endif

#endif
