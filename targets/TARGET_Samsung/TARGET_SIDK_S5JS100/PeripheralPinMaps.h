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
#ifndef MBED_PERIPHERALPINMAPS_H
#define MBED_PERIPHERALPINMAPS_H

#include <mstd_cstddef>
#include "mbed_toolchain.h"
#include "s5js100_pinconfig.h"
#include "PinNames.h"
#include "PeripheralNames.h"

MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_GPIO[] = {
    {P00, GPIO_0, GPIO_GPIO0},
    {P01, GPIO_1, GPIO_GPIO1},
    {P02, GPIO_2, GPIO_GPIO2},
    {P03, GPIO_3, GPIO_GPIO3},
    {P04, GPIO_4, GPIO_GPIO4},
    {P05, GPIO_5, GPIO_GPIO5},
    {P06, GPIO_6, GPIO_GPIO6},
    {P07, GPIO_7, GPIO_GPIO7},
    {P08, GPIO_8, GPIO_GPIO8},
    {P09, GPIO_9, GPIO_GPIO9},
    {P10, GPIO_10, GPIO_GPIO10},
    {P11, GPIO_11, GPIO_GPIO11},
    {P12, GPIO_12, GPIO_GPIO12},
    {P13, GPIO_13, GPIO_GPIO13},
    {P14, GPIO_14, GPIO_GPIO14},
    {P15, GPIO_15, GPIO_GPIO15},
    {P16, GPIO_16, GPIO_GPIO16},
    {P17, GPIO_17, GPIO_GPIO17},
    {P18, GPIO_18, GPIO_GPIO18},
    {P19, GPIO_19, GPIO_GPIO19},
    {P20, GPIO_20, GPIO_GPIO20},
    {P21, GPIO_21, GPIO_GPIO21},
    {P22, GPIO_22, GPIO_GPIO22},
    {P23, GPIO_23, GPIO_GPIO23},
    {P24, GPIO_24, GPIO_GPIO24},
    {P25, GPIO_25, GPIO_GPIO25},
    {P26, GPIO_26, GPIO_GPIO26},
    {P27, GPIO_27, GPIO_GPIO27},
    {P28, GPIO_28, GPIO_GPIO28},
    {P29, GPIO_29, GPIO_GPIO29},
    {P30, GPIO_30, GPIO_GPIO30},
    {P31, GPIO_31, GPIO_GPIO31},
    {P32, GPIO_32, GPIO_GPIO32},
    {P33, GPIO_33, GPIO_GPIO33},
    {P34, GPIO_34, GPIO_GPIO34},
    {P35, GPIO_35, GPIO_GPIO35},
    {P36, GPIO_36, GPIO_GPIO36},
    {P37, GPIO_37, GPIO_GPIO37},
    {P38, GPIO_38, GPIO_GPIO38},
    {P39, GPIO_39, GPIO_GPIO39},
    {P40, GPIO_40, GPIO_GPIO40},
    {P41, GPIO_41, GPIO_GPIO41},
    {P42, GPIO_42, GPIO_GPIO42},
    {P43, GPIO_43, GPIO_GPIO43},
    {P44, GPIO_44, GPIO_GPIO44},
    {P45, GPIO_45, GPIO_GPIO45},
    {P46, GPIO_46, GPIO_GPIO46},
    {P47, GPIO_47, GPIO_GPIO47},
    {P48, GPIO_48, GPIO_GPIO48},
    {P49, GPIO_49, GPIO_GPIO49},
    {P50, GPIO_50, GPIO_GPIO50},
    {P51, GPIO_51, GPIO_GPIO51},
    {P52, GPIO_52, GPIO_GPIO52},
    {P53, GPIO_53, GPIO_GPIO53},
    {P54, GPIO_54, GPIO_GPIO54},
    {P55, GPIO_55, GPIO_GPIO55},
    {P56, GPIO_56, GPIO_GPIO56},
    {P57, GPIO_57, GPIO_GPIO57},
    {P58, GPIO_58, GPIO_GPIO58},
    {P59, GPIO_59, GPIO_GPIO59},
    {P60, GPIO_60, GPIO_GPIO60},
    {P61, GPIO_61, GPIO_GPIO61},
    {P62, GPIO_62, GPIO_GPIO62},
    {P63, GPIO_63, GPIO_GPIO63},
    {P64, GPIO_64, GPIO_GPIO64},
    {P65, GPIO_65, GPIO_GPIO65},
    {P66, GPIO_66, GPIO_GPIO66},
    {P67, GPIO_67, GPIO_GPIO67},
    {P68, GPIO_68, GPIO_GPIO68},
    {P69, GPIO_69, GPIO_GPIO69},
    {P70, GPIO_70, GPIO_GPIO70},
    {P71, GPIO_71, GPIO_GPIO71},
    {P72, GPIO_72, GPIO_GPIO72},
    {NC, NC, 0}
};

/************ADC***************/
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_ADC[] = {
    {ADC_CH0, ADC0_0, 0},
    {ADC_CH1, ADC0_1, 0},
    {ADC_CH2, ADC0_2, 0},
    {ADC_CH3, ADC0_3, 0},
    {NC, NC, 0}
};

/************I2C***************/
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_I2C_SDA[] = {
    {I2C0_SDA, I2C_0, GPIO_I2C0_SDA},
    {NC, NC, 0}
};

MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_I2C_SCL[] = {
    {I2C0_SCL, I2C_0, GPIO_I2C0_SCL},
    {NC, NC, 0}
};

/************PWM***************/
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_PWM[] = {
    {PWM_TOUT0, PWM_0, GPIO_PWM_TOUT0},
    {PWM_TOUT1, PWM_1, GPIO_PWM_TOUT1},
    {PWM_TOUT2, PWM_2, GPIO_PWM_TOUT2},
    {PWM_TOUT3, PWM_3, GPIO_PWM_TOUT3},
    {NC, NC, 0}
};

/************SPI***************/
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_SPI_SCLK[] = {
    {SPI0_CLK, SPI_0, GPIO_SPI0_CLK},
    {NC, NC, 0}
};

MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_SPI_MOSI[] = {
    {SPI0_TXD, SPI_0, GPIO_SPI0_TXD},
    {NC, NC, 0}
};

MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_SPI_MISO[] = {
    {SPI0_RXD, SPI_0, GPIO_SPI0_RXD},
    {NC, NC, 0}
};

MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_SPI_SSEL[] = {
    {SPI0_CSN, SPI_0, GPIO_SPI0_CS},
    {P12, SPI_0, GPIO_GPIO12},
    {NC, NC, 0}
};

/************UART**************/
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_UART_TX[] = {
    {UART0_TXD, UART_0, GPIO_USI0_TXD_MOSI_SDA},
    {UART1_TXD, UART_1, GPIO_USI1_TXD_MOSI_SDA},
    {UART2_TXD, UART_2, GPIO_UART0_TXD},
    {UART3_TXD, UART_3, GPIO_UART1_TXD},
    {UART4_TXD, UART_4, UART4_TXD},
    {UART5_TXD, UART_5, UART5_TXD},
    {NC, NC, NC}
};

MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_UART_RX[] = {
    {UART0_RXD, UART_0, GPIO_USI0_RXD_CLK_SCL},
    {UART1_RXD, UART_1, GPIO_USI1_RXD_CLK_SCL},
    {UART2_RXD, UART_2, GPIO_UART0_RXD},
    {UART3_RXD, UART_3, GPIO_UART1_RXD},
    {UART4_RXD, UART_4, UART4_RXD},
    {UART5_RXD, UART_5, UART5_RXD},
    {NC, NC, NC}
};

MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_UART_CTS[] = {
    {UART0_CTS, UART_0, GPIO_USI0_CTSN_CSN_NA},
    {UART1_CTS, UART_1, GPIO_USI1_CTSN_CSN_NA},
    {UART2_CTS, UART_2, GPIO_UART0_CTSB},
    {UART3_CTS, UART_3, GPIO_UART1_CTSB},
    {NC, NC, NC}
};

MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_UART_RTS[] = {
    {UART0_RTS, UART_0, GPIO_USI0_RTSN_MISO_NA},
    {UART1_RTS, UART_1, GPIO_USI1_RTSN_MISO_NA},
    {UART2_RTS, UART_2, GPIO_UART0_RTSB},
    {UART3_RTS, UART_3, GPIO_UART1_RTSB},
    {NC, NC, NC}
};

/************QSPI**************/
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_QSPI_DQ0[] = {
    {NC, NC, NC}
};
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_QSPI_DQ1[] = {
    {NC, NC, NC}
};
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_QSPI_DQ2[] = {
    {NC, NC, NC}
};
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_QSPI_DQ3[] = {
    {NC, NC, NC}
};
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_QSPI_SCLK[] = {
    {NC, NC, NC}
};
MSTD_CONSTEXPR_OBJ_11 PinMap PinMap_QSPI_CS0[] = {
    {NC, NC, NC}
};

#define PINMAP_ANALOGIN PinMap_ADC
#define PINMAP_I2C_SDA PinMap_I2C_SDA
#define PINMAP_I2C_SCL PinMap_I2C_SCL
#define PINMAP_UART_TX PinMap_UART_TX
#define PINMAP_UART_RX PinMap_UART_RX
#define PINMAP_UART_CTS PinMap_UART_CTS
#define PINMAP_UART_RTS PinMap_UART_RTS
#define PINMAP_SPI_SCLK PinMap_SPI_SCLK
#define PINMAP_SPI_MOSI PinMap_SPI_MOSI
#define PINMAP_SPI_MISO PinMap_SPI_MISO
#define PINMAP_SPI_SSEL PinMap_SPI_SSEL
#define PINMAP_PWM PinMap_PWM


#endif
