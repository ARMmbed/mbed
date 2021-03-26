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

#ifndef MBED_PERIPHERALPINS_H
#define MBED_PERIPHERALPINS_H

#include "pinmap.h"
#include "PeripheralNames.h"

#if DEVICE_GPIO
/************ADC***************/
extern const PinMap PinMap_GPIO[];
#endif

#if DEVICE_ANALOGIN
/************ADC***************/
extern const PinMap PinMap_ADC[];
#endif

#if DEVICE_I2C
/************I2C***************/
extern const PinMap PinMap_I2C_SCL[];
extern const PinMap PinMap_I2C_SDA[];
#endif

#if DEVICE_PWMOUT
/************PWM***************/
extern const PinMap PinMap_PWM[];
#endif

#if DEVICE_SPI
/************SPI***************/
extern const PinMap PinMap_SPI_MOSI[];
extern const PinMap PinMap_SPI_MISO[];
extern const PinMap PinMap_SPI_SCLK[];
extern const PinMap PinMap_SPI_SSEL[];
#endif

#if DEVICE_SERIAL
/************UART**************/
extern const PinMap PinMap_UART_TX[];
extern const PinMap PinMap_UART_RX[];
extern const PinMap PinMap_UART_CTS[];
extern const PinMap PinMap_UART_RTS[];
#endif

#if DEVICE_QSPI
/************QSPI**************/
extern const PinMap PinMap_QSPI_DQ0[];
extern const PinMap PinMap_QSPI_DQ1[];
extern const PinMap PinMap_QSPI_DQ2[];
extern const PinMap PinMap_QSPI_DQ3[];
extern const PinMap PinMap_QSPI_SCLK[];
extern const PinMap PinMap_QSPI_CS0[];
#endif

#endif
