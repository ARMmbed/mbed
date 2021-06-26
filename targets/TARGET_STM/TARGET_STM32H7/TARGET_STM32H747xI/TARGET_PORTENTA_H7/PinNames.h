/* mbed Microcontroller Library
 *******************************************************************************
 * Copyright (c) 2018, STMicroelectronics
 * Copyright (c) 2020, Arduino SA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of STMicroelectronics nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */

/* MBED TARGET LIST: PORTENTA_H7_M4, PORTENTA_H7_M7 */

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
    ALT3  = 0x400,
    ALT4  = 0x500,
} ALTx;

typedef enum {
    PA_0       = 0x00,
    PA_0C      = PA_0  | DUAL_PAD, // dual pad
    PA_0C_ALT0 = PA_0C | ALT0, // same pin used for alternate HW
    PA_0C_ALT1 = PA_0C | ALT1, // same pin used for alternate HW
    PA_0C_ALT2 = PA_0C | ALT2, // same pin used for alternate HW
    PA_1       = 0x01,
    PA_1C      = PA_1  | DUAL_PAD, // dual pad
    PA_1C_ALT0 = PA_1C | ALT0, // same pin used for alternate HW
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
    PA_5_ALT1  = PA_5  | ALT1, // same pin used for alternate HW
    PA_5_ALT2  = PA_5  | ALT2, // same pin used for alternate HW
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
    PC_0_ALT1  = PC_0  | ALT1, // same pin used for alternate HW
    PC_1       = 0x21,
    PC_1_ALT0  = PC_1  | ALT0, // same pin used for alternate HW
    PC_1_ALT1  = PC_1  | ALT1, // same pin used for alternate HW
    PC_1_ALT2  = PC_1  | ALT2, // same pin used for alternate HW
    PC_1_ALT3  = PC_1  | ALT3, // same pin used for alternate HW
    PC_1_ALT4  = PC_1  | ALT4, // same pin used for alternate HW
    PC_2       = 0x22,
    PC_2C      = PC_2  | DUAL_PAD, // dual pad
    PC_2C_ALT0 = PC_2C | ALT0, // same pin used for alternate HW
    PC_2_ALT0  = PC_2  | ALT0, // same pin used for alternate HW
    PC_2_ALT1  = PC_2  | ALT1, // same pin used for alternate HW
    PC_2_ALT2  = PC_2  | ALT2, // same pin used for alternate HW
    PC_2_ALT3  = PC_2  | ALT3, // same pin used for alternate HW
    PC_2_ALT4  = PC_2  | ALT4, // same pin used for alternate HW
    PC_3       = 0x23,
    PC_3C      = PC_3  | DUAL_PAD, // dual pad
    PC_3_ALT0  = PC_3  | ALT0, // same pin used for alternate HW
    PC_3_ALT1  = PC_3  | ALT1, // same pin used for alternate HW
    PC_3_ALT2  = PC_3  | ALT2, // same pin used for alternate HW
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
    PF_0       = 0x50,
    PF_1       = 0x51,
    PF_2       = 0x52,
    PF_3       = 0x53,
    PF_4       = 0x54,
    PF_4_ALT0  = PF_4  | ALT0, // same pin used for alternate HW
    PF_5       = 0x55,
    PF_6       = 0x56,
    PF_6_ALT0  = PF_6  | ALT0, // same pin used for alternate HW
    PF_7       = 0x57,
    PF_8       = 0x58,
    PF_8_ALT0  = PF_8  | ALT0, // same pin used for alternate HW
    PF_9       = 0x59,
    PF_9_ALT0  = PF_9  | ALT0, // same pin used for alternate HW
    PF_10      = 0x5A,
    PF_10_ALT0 = PF_10 | ALT0, // same pin used for alternate HW
    PF_11      = 0x5B,
    PF_12      = 0x5C,
    PF_12_ALT0 = PF_12 | ALT0, // same pin used for alternate HW
    PF_13      = 0x5D,
    PF_14      = 0x5E,
    PF_14_ALT0 = PF_14 | ALT0, // same pin used for alternate HW
    PF_15      = 0x5F,
    PG_0       = 0x60,
    PG_1       = 0x61,
    PG_2       = 0x62,
    PG_3       = 0x63,
    PG_4       = 0x64,
    PG_5       = 0x65,
    PG_6       = 0x66,
    PG_7       = 0x67,
    PG_8       = 0x68,
    PG_9       = 0x69,
    PG_10      = 0x6A,
    PG_11      = 0x6B,
    PG_12      = 0x6C,
    PG_13      = 0x6D,
    PG_14      = 0x6E,
    PG_15      = 0x6F,
    PH_0       = 0x70,
    PH_1       = 0x71,
    PH_2       = 0x72,
    PH_3       = 0x73,
    PH_3_ALT0  = PH_3  | ALT0, // same pin used for alternate HW
    PH_4       = 0x74,
    PH_4_ALT0  = PH_4  | ALT0, // same pin used for alternate HW
    PH_5       = 0x75,
    PH_5_ALT0  = PH_5  | ALT0, // same pin used for alternate HW
    PH_6       = 0x76,
    PH_7       = 0x77,
    PH_8       = 0x78,
    PH_9       = 0x79,
    PH_10      = 0x7A,
    PH_11      = 0x7B,
    PH_12      = 0x7C,
    PH_13      = 0x7D,
    PH_14      = 0x7E,
    PH_15      = 0x7F,
    PI_0       = 0x80,
    PI_1       = 0x81,
    PI_2       = 0x82,
    PI_3       = 0x83,
    PI_4       = 0x84,
    PI_5       = 0x85,
    PI_6       = 0x86,
    PI_7       = 0x87,
    PI_8       = 0x88,
    PI_9       = 0x89,
    PI_10      = 0x8A,
    PI_11      = 0x8B,
    PI_12      = 0x8C,
    PI_13      = 0x8D,
    PI_14      = 0x8E,
    PI_15      = 0x8F,
    PJ_0       = 0x90,
    PJ_1       = 0x91,
    PJ_2       = 0x92,
    PJ_3       = 0x93,
    PJ_4       = 0x94,
    PJ_5       = 0x95,
    PJ_6       = 0x96,
    PJ_7       = 0x97,
    PJ_8       = 0x98,
    PJ_8_ALT0  = PJ_8  | ALT0, // same pin used for alternate HW
    PJ_9       = 0x99,
    PJ_9_ALT0  = PJ_9  | ALT0, // same pin used for alternate HW
    PJ_10      = 0x9A,
    PJ_10_ALT0 = PJ_10 | ALT0, // same pin used for alternate HW
    PJ_11      = 0x9B,
    PJ_11_ALT0 = PJ_11 | ALT0, // same pin used for alternate HW
    PJ_12      = 0x9C,
    PJ_13      = 0x9D,
    PJ_14      = 0x9E,
    PJ_15      = 0x9F,
    PK_0       = 0xA0,
    PK_0_ALT0  = PK_0  | ALT0, // same pin used for alternate HW
    PK_1       = 0xA1,
    PK_1_ALT0  = PK_1  | ALT0, // same pin used for alternate HW
    PK_2       = 0xA2,
    PK_3       = 0xA3,
    PK_4       = 0xA4,
    PK_5       = 0xA5,
    PK_6       = 0xA6,
    PK_7       = 0xA7,

    WL_REG_ON = PJ_1,
    WL_HOST_WAKE = PJ_5,
    WL_SDIO_0 = PC_8,
    WL_SDIO_1 = PC_9,
    WL_SDIO_2 = PC_10,
    WL_SDIO_3 = PC_11,
    WL_SDIO_CMD = PD_2,
    WL_SDIO_CLK = PC_12,

    /**** ADC internal channels ****/

    ADC_TEMP = 0xF0, // Internal pin virtual value
    ADC_VREF = 0xF1, // Internal pin virtual value
    ADC_VBAT = 0xF2, // Internal pin virtual value

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

    //Led mappings
    LED_RED = PK_5,  //Red
    LED_GREEN = PK_6,  //Green
    LED_BLUE = PK_7,  //Blue

    CYBSP_BT_UART_RX = PF_6,
    CYBSP_BT_UART_TX = PA_15,
    CYBSP_BT_UART_RTS = PF_8,
    CYBSP_BT_UART_CTS = PF_9,

    CYBSP_BT_POWER = PJ_12,
    CYBSP_BT_HOST_WAKE = PJ_13,
    CYBSP_BT_DEVICE_WAKE = PJ_14,

    /**** QSPI FLASH pins ****/
    QSPI_FLASH1_IO0 = PD_11,
    QSPI_FLASH1_IO1 = PD_12,
    QSPI_FLASH1_IO2 = PF_7,
    QSPI_FLASH1_IO3 = PD_13,
    QSPI_FLASH1_SCK = PF_10,
    QSPI_FLASH1_CSN = PG_6,

    /**** USB FS pins ****/
    USB_OTG_FS_DM = PA_11,
    USB_OTG_FS_DP = PA_12,
    USB_OTG_FS_ID = PA_10,
    USB_OTG_FS_SOF = PA_8,
    USB_OTG_FS_VBUS = PA_9,

    /**** USB HS pins ****/
    USB_OTG_HS_DM = PB_14,
    USB_OTG_HS_DP = PB_15,
    USB_OTG_HS_ID = PB_12,
    USB_OTG_HS_SOF = PA_4,
    USB_OTG_HS_ULPI_CK = PA_5,
    USB_OTG_HS_ULPI_D0 = PA_3,
    USB_OTG_HS_ULPI_D1 = PB_0,
    USB_OTG_HS_ULPI_D2 = PB_1,
    USB_OTG_HS_ULPI_D3 = PB_10,
    USB_OTG_HS_ULPI_D4 = PB_11,
    USB_OTG_HS_ULPI_D5 = PB_12,
    USB_OTG_HS_ULPI_D6 = PB_13,
    USB_OTG_HS_ULPI_D7 = PB_5,
    USB_OTG_HS_ULPI_DIR = PC_2,
    USB_OTG_HS_ULPI_NXT = PC_3,
    USB_OTG_HS_ULPI_STP = PC_0,
    USB_OTG_HS_VBUS = PB_13,

    /**** ETHERNET pins ****/
    ETH_MDC = PC_1,
    ETH_MDIO = PA_2,
    ETH_CRS_DV = PA_7,
    ETH_REF_CLK = PA_1,
    ETH_RXD0 = PC_4,
    ETH_RXD1 = PC_5,
    ETH_RX_CLK = PA_1,
    ETH_TXD0 = PG_13,
    ETH_TXD1 = PG_12,
    ETH_TX_EN = PG_11,

    /**** OSCILLATOR pins ****/
    RCC_OSC32_IN = PC_14,
    RCC_OSC32_OUT = PC_15,
    RCC_OSC_IN = PH_0,
    RCC_OSC_OUT = PH_1,

    /**** DEBUG pins ****/
    SYS_JTCK_SWCLK = PA_14,
    SYS_JTDI = PA_15,
    SYS_JTDO_SWO = PB_3,
    SYS_JTMS_SWDIO = PA_13,
    SYS_JTRST = PB_4,
    SYS_PVD_IN = PB_7,
    SYS_TRACECLK = PE_2,
    SYS_TRACED0 = PE_3,
    SYS_TRACED0_ALT0 = PC_1,
    SYS_TRACED0_ALT1 = PG_13,
    SYS_TRACED1 = PE_4,
    SYS_TRACED1_ALT0 = PC_8,
    SYS_TRACED1_ALT1 = PG_14,
    SYS_TRACED2 = PE_5,
    SYS_TRACED2_ALT0 = PD_2,
    SYS_TRACED3 = PE_6,
    SYS_TRACED3_ALT0 = PC_12,
    SYS_TRGIO = PC_7,
    SYS_WKUP0 = PA_0,
    SYS_WKUP1 = PA_2,
    SYS_WKUP2 = PC_13,
    SYS_WKUP5 = PC_1,

    // Not connected
    NC = (int)0xFFFFFFFF
} PinName;

// Standardized LED and button names
#define LED1     LED_RED
#define LED2     LED_GREEN
#define LED3     LED_BLUE
#define BUTTON1  PC_13

#ifdef __cplusplus
}
#endif

#endif
