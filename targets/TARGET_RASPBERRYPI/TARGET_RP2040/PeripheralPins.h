/*
 * mbed Microcontroller Library
 * Copyright (c) 2017-2018 Future Electronics
 * Copyright (c) 2018-2019 Cypress Semiconductor Corporation
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
// SPDX-License-Identifier: Apache-2.0
#ifndef MBED_PERIPHERALPINS_H
#define MBED_PERIPHERALPINS_H

#include "PeripheralNames.h"

//*** I2C ***
#if DEVICE_I2C
extern const PinMap PinMap_I2C_SDA[];
extern const PinMap PinMap_I2C_SCL[];
#endif

//*** PWM ***
#if DEVICE_PWMOUT
extern const PinMap PinMap_PWM_OUT[];
#endif

//*** SERIAL ***
#if DEVICE_SERIAL
extern const PinMap PinMap_UART_TX[];
extern const PinMap PinMap_UART_RX[];
extern const PinMap PinMap_UART_RTS[];
extern const PinMap PinMap_UART_CTS[];
#endif

//*** SPI ***
#if DEVICE_SPI
extern const PinMap PinMap_SPI_MISO[];
extern const PinMap PinMap_SPI_SSEL[];
extern const PinMap PinMap_SPI_SCLK[];
extern const PinMap PinMap_SPI_MOSI[];
#endif

//*** ADC ***
#if DEVICE_ANALOGIN
extern const PinMap PinMap_ADC[];
#endif

//*** DAC ***
#if DEVICE_ANALOGOUT
extern const PinMap PinMap_DAC[];
#endif

//*** QSPI ***
#if DEVICE_QSPI
extern const PinMap PinMap_QSPI_SCLK[];
extern const PinMap PinMap_QSPI_SSEL[];
extern const PinMap PinMap_QSPI_DATA0[];
extern const PinMap PinMap_QSPI_DATA1[];
extern const PinMap PinMap_QSPI_DATA2[];
extern const PinMap PinMap_QSPI_DATA3[];
#endif

#endif
