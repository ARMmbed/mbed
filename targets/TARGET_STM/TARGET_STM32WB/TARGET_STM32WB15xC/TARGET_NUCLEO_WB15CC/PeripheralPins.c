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

#include "PeripheralPins.h"
#include "mbed_toolchain.h"

//==============================================================================
// Notes
//
// - The pins mentioned Px_y_ALTz are alternative possibilities which use other
//   HW peripheral instances. You can use them the same way as any other "normal"
//   pin (i.e. PwmOut pwm(PA_7_ALT0);). These pins are not displayed on the board
//   pinout image on mbed.org.
//
// - The pins which are connected to other components present on the board have
//   the comment "Connected to xxx". The pin function may not work properly in this
//   case. These pins may not be displayed on the board pinout image on mbed.org.
//   Please read the board reference manual and schematic for more information.
//
// - Warning: pins connected to the default STDIO_UART_TX and STDIO_UART_RX pins are commented
//   See https://os.mbed.com/teams/ST/wiki/STDIO for more information.
//
//==============================================================================




//*** ADC ***

MBED_WEAK const PinMap PinMap_ADC[] = {
    {PA_0,       ADC_1, STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 5, 0)}, // ADC1_IN5 // Connected to B1 [Push Button]
    {PA_1,       ADC_1, STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 6, 0)}, // ADC1_IN6
    {PA_2,       ADC_1, STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 7, 0)}, // ADC1_IN7
    {PA_3,       ADC_1, STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 8, 0)}, // ADC1_IN8
    {PA_4,       ADC_1, STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 9, 0)}, // ADC1_IN9
    {PA_5,       ADC_1, STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 10, 0)}, // ADC1_IN10
    {PA_6,       ADC_1, STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 11, 0)}, // ADC1_IN11 // Connected to B3 [Push Button]
    {PA_7,       ADC_1, STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 2, 0)}, // ADC1_IN2
    {PA_8,       ADC_1, STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 3, 0)}, // ADC1_IN3
//  {PA_9,       ADC_1, STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 4, 0)}, // ADC1_IN4 // Connected to STDIO_UART_TX
    {NC, NC, 0}
};

MBED_WEAK const PinMap PinMap_ADC_Internal[] = {
    {ADC_TEMP,   ADC_1,    STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 17, 0)},
    {ADC_VREF,   ADC_1,    STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0,  0, 0)},
    {ADC_VBAT,   ADC_1,    STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 18, 0)},
    {NC, NC, 0}
};

//*** I2C ***

MBED_WEAK const PinMap PinMap_I2C_SDA[] = {
//  {PA_10,      I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF4_I2C1)}, // Connected to STDIO_UART_RX
    {PB_7,       I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF4_I2C1)},
    {PB_9,       I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF4_I2C1)},
    {NC, NC, 0}
};

MBED_WEAK const PinMap PinMap_I2C_SCL[] = {
//  {PA_9,       I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF4_I2C1)}, // Connected to STDIO_UART_TX
    {PB_6,       I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF4_I2C1)},
    {PB_8,       I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF4_I2C1)},
    {NC, NC, 0}
};

//*** PWM ***

// TIM2 cannot be used because already used by the us_ticker
// (update us_ticker_data.h file if another timer is chosen)
MBED_WEAK const PinMap PinMap_PWM[] = {
//  {PA_0,       PWM_2,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM2, 1, 0)}, // TIM2_CH1 // Connected to B1 [Push Button]
//  {PA_1,       PWM_2,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM2, 2, 0)}, // TIM2_CH2
//  {PA_2,       PWM_2,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM2, 3, 0)}, // TIM2_CH3
//  {PA_3,       PWM_2,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM2, 4, 0)}, // TIM2_CH4
//  {PA_5,       PWM_2,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM2, 1, 0)}, // TIM2_CH1
    {PA_7,       PWM_1,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM1, 1, 1)}, // TIM1_CH1N
    {PA_8,       PWM_1,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM1, 1, 0)}, // TIM1_CH1
//  {PA_9,       PWM_1,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM1, 2, 0)}, // TIM1_CH2 // Connected to STDIO_UART_TX
//  {PA_10,      PWM_1,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM1, 3, 0)}, // TIM1_CH3 // Connected to STDIO_UART_RX
    {PA_11,      PWM_1,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM1, 4, 0)}, // TIM1_CH4
//  {PA_15,      PWM_2,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM2, 1, 0)}, // TIM2_CH1
//  {PB_3,       PWM_2,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM2, 2, 0)}, // TIM2_CH2 // Connected to JTDO
    {PB_7,       PWM_1,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF12_TIM1, 3, 0)}, // TIM1_CH3
    {PB_8,       PWM_1,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM1, 2, 1)}, // TIM1_CH2N
    {PB_9,       PWM_1,  STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF1_TIM1, 3, 1)}, // TIM1_CH3N
    {NC, NC, 0}
};

//*** SERIAL ***

MBED_WEAK const PinMap PinMap_UART_TX[] = {
    {PA_2,       LPUART_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF8_LPUART1)},
    {PA_9,       UART_1,   STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF7_USART1)}, // Connected to STDIO_UART_TX
    {PB_5,       LPUART_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF8_LPUART1)}, // Connected to LD1 [Blue Led]
    {PB_6,       UART_1,   STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF7_USART1)},
    {NC, NC, 0}
};

MBED_WEAK const PinMap PinMap_UART_RX[] = {
    {PA_3,       LPUART_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF8_LPUART1)},
    {PA_10,      UART_1,   STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF7_USART1)}, // Connected to STDIO_UART_RX
    {PA_12,      LPUART_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF8_LPUART1)},
    {PB_7,       UART_1,   STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF7_USART1)},
    {NC, NC, 0}
};

MBED_WEAK const PinMap PinMap_UART_RTS[] = {
    {PA_12,      UART_1,   STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF7_USART1)},
    {PB_1,       LPUART_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF8_LPUART1)}, // Connected to LD3 [Red Led]
    {PB_3,       UART_1,   STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF7_USART1)}, // Connected to JTDO
    {NC, NC, 0}
};

MBED_WEAK const PinMap PinMap_UART_CTS[] = {
    {PA_6,       LPUART_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF8_LPUART1)}, // Connected to B3 [Push Button]
    {PA_11,      UART_1,   STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF7_USART1)},
    {PB_4,       UART_1,   STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF7_USART1)},
    {NC, NC, 0}
};

//*** SPI ***

MBED_WEAK const PinMap PinMap_SPI_MOSI[] = {
    {PA_5,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF4_SPI1)},
    {PA_7,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)},
    {PA_12,      SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)},
    {PA_13,      SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)}, // Connected to JTMS
    {PB_5,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)}, // Connected to LD1 [Blue Led]
    {NC, NC, 0}
};

MBED_WEAK const PinMap PinMap_SPI_MISO[] = {
    {PA_6,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)}, // Connected to B3 [Push Button]
    {PA_11,      SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)},
    {PB_4,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)},
    {NC, NC, 0}
};

MBED_WEAK const PinMap PinMap_SPI_SCLK[] = {
    {PA_1,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)},
    {PA_5,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)},
    {PB_3,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)}, // Connected to JTDO
    {NC, NC, 0}
};

MBED_WEAK const PinMap PinMap_SPI_SSEL[] = {
    {PA_4,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)},
    {PA_14,      SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)}, // Connected to JTCK
    {PA_15,      SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)},
    {PB_2,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)},
    {PB_6,       SPI_1, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1)},
    {NC, NC, 0}
};
