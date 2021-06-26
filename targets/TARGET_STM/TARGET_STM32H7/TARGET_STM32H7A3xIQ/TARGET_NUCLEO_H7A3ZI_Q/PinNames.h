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
 * Automatically generated from STM32CubeMX/db/mcu/STM32H7A3Z(G-I)TxQ.xml
 */

/* MBED TARGET LIST: NUCLEO_H7A3ZI_Q */

#ifndef MBED_PINNAMES_H
#define MBED_PINNAMES_H

#include "cmsis.h"
#include "PinNamesTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DUAL_PAD 0xF00

typedef enum {
    ALT0  = 0x100,
    ALT1  = 0x200,
    ALT2  = 0x300,
} ALTx;

typedef enum {
    PA_0       = 0x00,
    PA_1       = 0x01,
    PA_1_ALT0  = PA_1  | ALT0, // same pin used for alternate HW
    PA_2       = 0x02,
    PA_2_ALT0  = PA_2  | ALT0, // same pin used for alternate HW
    PA_3       = 0x03,
    PA_3_ALT0  = PA_3  | ALT0, // same pin used for alternate HW
    PA_4       = 0x04,
    PA_4_ALT0  = PA_4  | ALT0, // same pin used for alternate HW
    PA_4_ALT1  = PA_4  | ALT1, // same pin used for alternate HW
    PA_5       = 0x05,
    PA_5_ALT0  = PA_5  | ALT0, // same pin used for alternate HW
    PA_6       = 0x06,
    PA_6_ALT0  = PA_6  | ALT0, // same pin used for alternate HW
    PA_7       = 0x07,
    PA_7_ALT0  = PA_7  | ALT0, // same pin used for alternate HW
    PA_7_ALT1  = PA_7  | ALT1, // same pin used for alternate HW
    PA_7_ALT2  = PA_7  | ALT2, // same pin used for alternate HW
    PA_8       = 0x08,
    PA_9       = 0x09,
    PA_9_ALT0  = PA_9  | ALT0, // same pin used for alternate HW
    PA_10      = 0x0A,
    PA_10_ALT0 = PA_10 | ALT0, // same pin used for alternate HW
    PA_11      = 0x0B,
    PA_11_ALT0 = PA_11 | ALT0, // same pin used for alternate HW
    PA_12      = 0x0C,
    PA_12_ALT0 = PA_12 | ALT0, // same pin used for alternate HW
    PA_13      = 0x0D,
    PA_14      = 0x0E,
    PA_15      = 0x0F,
    PA_15_ALT0 = PA_15 | ALT0, // same pin used for alternate HW
    PA_15_ALT1 = PA_15 | ALT1, // same pin used for alternate HW
    PB_0       = 0x10,
    PB_0_ALT0  = PB_0  | ALT0, // same pin used for alternate HW
    PB_0_ALT1  = PB_0  | ALT1, // same pin used for alternate HW
    PB_0_ALT2  = PB_0  | ALT2, // same pin used for alternate HW
    PB_1       = 0x11,
    PB_1_ALT0  = PB_1  | ALT0, // same pin used for alternate HW
    PB_1_ALT1  = PB_1  | ALT1, // same pin used for alternate HW
    PB_2       = 0x12,
    PB_3       = 0x13,
    PB_3_ALT0  = PB_3  | ALT0, // same pin used for alternate HW
    PB_3_ALT1  = PB_3  | ALT1, // same pin used for alternate HW
    PB_4       = 0x14,
    PB_4_ALT0  = PB_4  | ALT0, // same pin used for alternate HW
    PB_4_ALT1  = PB_4  | ALT1, // same pin used for alternate HW
    PB_5       = 0x15,
    PB_5_ALT0  = PB_5  | ALT0, // same pin used for alternate HW
    PB_5_ALT1  = PB_5  | ALT1, // same pin used for alternate HW
    PB_6       = 0x16,
    PB_6_ALT0  = PB_6  | ALT0, // same pin used for alternate HW
    PB_6_ALT1  = PB_6  | ALT1, // same pin used for alternate HW
    PB_7       = 0x17,
    PB_7_ALT0  = PB_7  | ALT0, // same pin used for alternate HW
    PB_8       = 0x18,
    PB_8_ALT0  = PB_8  | ALT0, // same pin used for alternate HW
    PB_9       = 0x19,
    PB_9_ALT0  = PB_9  | ALT0, // same pin used for alternate HW
    PB_10      = 0x1A,
    PB_11      = 0x1B,
    PB_12      = 0x1C,
    PB_13      = 0x1D,
    PB_14      = 0x1E,
    PB_14_ALT0 = PB_14 | ALT0, // same pin used for alternate HW
    PB_14_ALT1 = PB_14 | ALT1, // same pin used for alternate HW
    PB_15      = 0x1F,
    PB_15_ALT0 = PB_15 | ALT0, // same pin used for alternate HW
    PB_15_ALT1 = PB_15 | ALT1, // same pin used for alternate HW
    PC_0       = 0x20,
    PC_0_ALT0  = PC_0  | ALT0, // same pin used for alternate HW
    PC_1       = 0x21,
    PC_1_ALT0  = PC_1  | ALT0, // same pin used for alternate HW
    PC_1_ALT1  = PC_1  | ALT1, // same pin used for alternate HW
    PC_1_ALT2  = PC_1  | ALT2, // same pin used for alternate HW
    PC_2       = 0x22,
    PC_2C      = PC_2  | DUAL_PAD, // dual pad
    PC_2C_ALT0 = PC_2C | ALT0, // same pin used for alternate HW
    PC_3       = 0x23,
    PC_3C      = PC_3  | DUAL_PAD, // dual pad
    PC_4       = 0x24,
    PC_4_ALT0  = PC_4  | ALT0, // same pin used for alternate HW
    PC_5       = 0x25,
    PC_5_ALT0  = PC_5  | ALT0, // same pin used for alternate HW
    PC_5_ALT1  = PC_5  | ALT1, // same pin used for alternate HW
    PC_5_ALT2  = PC_5  | ALT2, // same pin used for alternate HW
    PC_6       = 0x26,
    PC_6_ALT0  = PC_6  | ALT0, // same pin used for alternate HW
    PC_7       = 0x27,
    PC_7_ALT0  = PC_7  | ALT0, // same pin used for alternate HW
    PC_8       = 0x28,
    PC_8_ALT0  = PC_8  | ALT0, // same pin used for alternate HW
    PC_9       = 0x29,
    PC_9_ALT0  = PC_9  | ALT0, // same pin used for alternate HW
    PC_10      = 0x2A,
    PC_10_ALT0 = PC_10 | ALT0, // same pin used for alternate HW
    PC_11      = 0x2B,
    PC_11_ALT0 = PC_11 | ALT0, // same pin used for alternate HW
    PC_12      = 0x2C,
    PC_13      = 0x2D,
    PC_14      = 0x2E,
    PC_15      = 0x2F,
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
    PE_5       = 0x45,
    PE_6       = 0x46,
    PE_7       = 0x47,
    PE_8       = 0x48,
    PE_9       = 0x49,
    PE_10      = 0x4A,
    PE_11      = 0x4B,
    PE_12      = 0x4C,
    PE_13      = 0x4D,
    PE_14      = 0x4E,
    PE_15      = 0x4F,
    PF_6       = 0x56,
    PF_7       = 0x57,
    PF_8       = 0x58,
    PF_8_ALT0  = PF_8  | ALT0, // same pin used for alternate HW
    PF_9       = 0x59,
    PF_9_ALT0  = PF_9  | ALT0, // same pin used for alternate HW
    PF_10      = 0x5A,
    PF_11      = 0x5B,
    PF_14      = 0x5E,
    PF_14_ALT0 = PF_14 | ALT0, // same pin used for alternate HW
    PF_15      = 0x5F,
    PG_6       = 0x66,
    PG_7       = 0x67,
    PG_8       = 0x68,
    PG_9       = 0x69,
    PG_10      = 0x6A,
    PG_11      = 0x6B,
    PG_12      = 0x6C,
    PG_13      = 0x6D,
    PG_13_ALT0 = PG_13 | ALT0, // same pin used for alternate HW
    PG_14      = 0x6E,
    PH_0       = 0x70,
    PH_1       = 0x71,

    /**** ADC internal channels ****/

    ADC_TEMP = 0xF0, // Internal pin virtual value
    ADC_VREF = 0xF1, // Internal pin virtual value
    ADC_VBAT = 0xF2, // Internal pin virtual value

#ifdef TARGET_FF_ARDUINO_UNO
    // Arduino Uno (Rev3) pins
    ARDUINO_UNO_A0  = PA_3,
    ARDUINO_UNO_A1  = PC_0,
    ARDUINO_UNO_A2  = PC_3C,
    ARDUINO_UNO_A3  = PB_1,
    ARDUINO_UNO_A4  = PC_2C,
    ARDUINO_UNO_A5  = PF_11,

    ARDUINO_UNO_D0  = PB_7,
    ARDUINO_UNO_D1  = PB_6,
    ARDUINO_UNO_D2  = PG_14,
    ARDUINO_UNO_D3  = PE_13,
    ARDUINO_UNO_D4  = PE_14,
    ARDUINO_UNO_D5  = PE_11,
    ARDUINO_UNO_D6  = PA_8,
    ARDUINO_UNO_D7  = PG_12,
    ARDUINO_UNO_D8  = PG_9,
    ARDUINO_UNO_D9  = PD_15,
    ARDUINO_UNO_D10 = PD_14,
    ARDUINO_UNO_D11 = PA_7,
    ARDUINO_UNO_D12 = PA_6,
    ARDUINO_UNO_D13 = PA_5,
    ARDUINO_UNO_D14 = PB_9,
    ARDUINO_UNO_D15 = PB_8,
#endif

    // ST ZIO connector extending Arduino Uno
    A6  = PC_1,
    A7  = PC_5,
    A8  = PA_2,
    D16 = PC_6,
    D17 = PB_15,
    D18 = PB_13,
    D19 = PB_12,
    D20 = PA_15,
    D21 = PC_7,
    D22 = PB_5,
    D23 = PB_3,
    D24 = PA_4,
    D25 = PB_4,
    D26 = PG_6,
    D27 = PB_2,
    D28 = PD_13,
    D29 = PD_12,
    D30 = PD_11,
    D31 = PE_2,
    D32 = PA_0,
    D33 = PB_0,
    D34 = PE_0,
    D35 = PB_11,
    D36 = PB_10,
    D37 = PE_15,
    D38 = PE_6,
    D39 = PE_12,
    D40 = PE_10,
    D41 = PE_7,
    D42 = PE_8,
    D43 = PC_8,
    D44 = PC_9,
    D45 = PC_10,
    D46 = PC_11,
    D47 = PC_12,
    D48 = PD_2,
    D49 = PG_10,
    D50 = PG_8,
    D51 = PD_7,
    D52 = PD_6,
    D53 = PD_5,
    D54 = PD_4,
    D55 = PD_3,
    D56 = PE_2,
    D57 = PE_4,
    D58 = PE_5,
    D59 = PE_6,
    D60 = PE_3,
    D61 = PF_8,
    D62 = PF_7,
    D63 = PF_9,
    D64 = PD_10,
    D65 = PG_14,
    D66 = PD_1,
    D67 = PD_0,
    D68 = PF_15,
    D69 = PF_14,
    D70 = PB_5,
    D71 = PE_9,
    D72 = PB_2,

    // STDIO for console print
#ifdef MBED_CONF_TARGET_STDIO_UART_TX
    CONSOLE_TX = MBED_CONF_TARGET_STDIO_UART_TX,
#else
    CONSOLE_TX = PD_8,
#endif
#ifdef MBED_CONF_TARGET_STDIO_UART_RX
    CONSOLE_RX = MBED_CONF_TARGET_STDIO_UART_RX,
#else
    CONSOLE_RX = PD_9,
#endif

    /**** USB HS pins ****/
    USB_OTG_HS_DM = PA_11,
    USB_OTG_HS_DP = PA_12,
    USB_OTG_HS_ID = PA_10,
    USB_OTG_HS_SOF = PA_8,
    USB_OTG_HS_ULPI_CK = PA_5,
    USB_OTG_HS_ULPI_D0 = PA_3,
    USB_OTG_HS_ULPI_D1 = PB_0,
    USB_OTG_HS_ULPI_D2 = PB_1,
    USB_OTG_HS_ULPI_D3 = PB_10,
    USB_OTG_HS_ULPI_D4 = PB_11,
    USB_OTG_HS_ULPI_D5 = PB_12,
    USB_OTG_HS_ULPI_D6 = PB_13,
    USB_OTG_HS_ULPI_D7 = PB_5,
    USB_OTG_HS_ULPI_DIR = PC_2C,
    USB_OTG_HS_ULPI_NXT = PC_3C,
    USB_OTG_HS_ULPI_STP = PC_0,
    USB_OTG_HS_VBUS = PA_9,

    /**** OSCILLATOR pins ****/
    RCC_OSC32_IN = PC_14,
    RCC_OSC32_OUT = PC_15,
    RCC_OSC_IN = PH_0,
    RCC_OSC_OUT = PH_1,

    /**** DEBUG pins ****/
    DEBUG_JTCK_SWCLK = PA_14,
    DEBUG_JTDI = PA_15,
    DEBUG_JTDO_SWO = PB_3,
    DEBUG_JTMS_SWDIO = PA_13,
    DEBUG_JTRST = PB_4,
    DEBUG_TRACECLK = PE_2,
    DEBUG_TRACED0 = PE_3,
    DEBUG_TRACED0_ALT0 = PC_1,
    DEBUG_TRACED0_ALT1 = PG_13,
    DEBUG_TRACED1 = PE_4,
    DEBUG_TRACED1_ALT0 = PC_8,
    DEBUG_TRACED1_ALT1 = PG_14,
    DEBUG_TRACED2 = PE_5,
    DEBUG_TRACED2_ALT0 = PD_2,
    DEBUG_TRACED3 = PE_6,
    DEBUG_TRACED3_ALT0 = PC_12,
    DEBUG_TRGIO = PC_7,
    PWR_PVD_IN = PB_7,
    PWR_WKUP1 = PA_0,
    PWR_WKUP2 = PA_2,
    PWR_WKUP4 = PC_13,
    PWR_WKUP6 = PC_1,

    // Not connected
    NC = (int)0xFFFFFFFF
} PinName;

// Standardized LED and button names
#define LED1     PB_0   // LD1 (Green Led)
#define LED2     PE_1   // LD2 (Yellow Led)
#define LED3     PB_14  // LD3 (Red Led)
#define BUTTON1  PC_13  // B1 (Blue PushButton)

#ifdef __cplusplus
}
#endif

#endif
