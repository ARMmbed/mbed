
/** \addtogroup hal */
/** @{*/
/* mbed Microcontroller Library
 * Copyright (c) 2020 ARM Limited
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
#ifndef MBED_PIN_NAME_ALIASES_H
#define MBED_PIN_NAME_ALIASES_H

/* Aliases for legacy reasons. To be removed in the next Mbed OS version */
#define USBTX CONSOLE_TX
#define USBRX CONSOLE_RX

#if defined (TARGET_FF_ARDUINO) || (TARGET_FF_ARDUINO_UNO)

#ifdef TARGET_FF_ARDUINO_UNO

#ifndef A0
#define A0  ARDUINO_UNO_A0
#endif

#ifndef A1
#define A1  ARDUINO_UNO_A1
#endif

#ifndef A2
#define A2  ARDUINO_UNO_A2
#endif

#ifndef A3
#define A3  ARDUINO_UNO_A3
#endif

#ifndef A4
#define A4  ARDUINO_UNO_A4
#endif

#ifndef A5
#define A5  ARDUINO_UNO_A5
#endif

#ifndef D0
#define D0  ARDUINO_UNO_D0
#endif

#ifndef D1
#define D1  ARDUINO_UNO_D1
#endif

#ifndef D2
#define D2  ARDUINO_UNO_D2
#endif

#ifndef D3
#define D3  ARDUINO_UNO_D3
#endif

#ifndef D4
#define D4  ARDUINO_UNO_D4
#endif

#ifndef D5
#define D5  ARDUINO_UNO_D5
#endif

#ifndef D6
#define D6  ARDUINO_UNO_D6
#endif

#ifndef D7
#define D7  ARDUINO_UNO_D7
#endif

#ifndef D8
#define D8  ARDUINO_UNO_D8
#endif

#ifndef D9
#define D9  ARDUINO_UNO_D9
#endif

#ifndef D10
#define D10 ARDUINO_UNO_D10
#endif

#ifndef D11
#define D11 ARDUINO_UNO_D11
#endif

#ifndef D12
#define D12 ARDUINO_UNO_D12
#endif

#ifndef D13
#define D13 ARDUINO_UNO_D13
#endif

#ifndef D14
#define D14 ARDUINO_UNO_D14
#endif

#ifndef D15
#define D15 ARDUINO_UNO_D15
#endif

#endif // TARGET_FF_ARDUINO_UNO

#ifdef TARGET_FF_ARDUINO

#warning ARDUINO form factor should not be used any more => use ARDUINO_UNO

#define ARDUINO_UNO_A0  A0
#define ARDUINO_UNO_A1  A1
#define ARDUINO_UNO_A2  A2
#define ARDUINO_UNO_A3  A3
#define ARDUINO_UNO_A4  A4
#define ARDUINO_UNO_A5  A5
#define ARDUINO_UNO_D0  D0
#define ARDUINO_UNO_D1  D1
#define ARDUINO_UNO_D2  D2
#define ARDUINO_UNO_D3  D3
#define ARDUINO_UNO_D4  D4
#define ARDUINO_UNO_D5  D5
#define ARDUINO_UNO_D6  D6
#define ARDUINO_UNO_D7  D7
#define ARDUINO_UNO_D8  D8
#define ARDUINO_UNO_D9  D9
#define ARDUINO_UNO_D10 D10
#define ARDUINO_UNO_D11 D11
#define ARDUINO_UNO_D12 D12
#define ARDUINO_UNO_D13 D13
#define ARDUINO_UNO_D14 D14
#define ARDUINO_UNO_D15 D15

#endif // TARGET_FF_ARDUINO

#ifdef TARGET_FF_ARDUINO_UNO

// Arduino Uno I2C signals aliases
#define ARDUINO_UNO_I2C_SDA ARDUINO_UNO_D14
#define ARDUINO_UNO_I2C_SCL ARDUINO_UNO_D15

// Legacy I2C aliases
#ifndef I2C_SDA
#define I2C_SDA ARDUINO_UNO_I2C_SDA
#endif
#ifndef I2C_SCL
#define I2C_SCL ARDUINO_UNO_I2C_SCL
#endif

// Arduino Uno SPI signals aliases
#define ARDUINO_UNO_SPI_CS   ARDUINO_UNO_D10
#define ARDUINO_UNO_SPI_MOSI ARDUINO_UNO_D11
#define ARDUINO_UNO_SPI_MISO ARDUINO_UNO_D12
#define ARDUINO_UNO_SPI_SCK  ARDUINO_UNO_D13

// Legacy SPI aliases
#ifndef SPI_CS
#define SPI_CS ARDUINO_UNO_SPI_CS
#endif
#ifndef SPI_MOSI
#define SPI_MOSI ARDUINO_UNO_SPI_MOSI
#endif
#ifndef SPI_MISO
#define SPI_MISO ARDUINO_UNO_SPI_MISO
#endif
#ifndef SPI_SCK
#define SPI_SCK ARDUINO_UNO_SPI_SCK
#endif

// Arduino Uno UART signals aliases
#define ARDUINO_UNO_UART_TX ARDUINO_UNO_D1
#define ARDUINO_UNO_UART_RX ARDUINO_UNO_D0

#endif // TARGET_FF_ARDUINO_UNO

#endif // (TARGET_FF_ARDUINO) || (TARGET_FF_ARDUINO_UNO)

#endif // MBED_PIN_NAME_ALIASES_H

/** @}*/
