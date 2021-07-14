/* mbed Microcontroller Library
 * SPDX-License-Identifier: BSD-3-Clause
 ******************************************************************************
 *
 * Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* application example

#include "mbed.h"

int main()
{
    while (true) {
        rgb_led_off();
        ThisThread::sleep_for(2s);

        rgb_led_red();
        ThisThread::sleep_for(2s);

        rgb_led_green();
        ThisThread::sleep_for(2s);

        rgb_led_blue();
        ThisThread::sleep_for(2s);

        rgb_led_on(0x41, 0x41, 0);    // yellow
        ThisThread::sleep_for(2s);

        rgb_led_on(0x41, 0, 0x41);    // magenta
        ThisThread::sleep_for(2s);

        rgb_led_on(0, 0x41, 0x41);    // cyan
        ThisThread::sleep_for(2s);

        rgb_led_on(0x41, 0x41, 0x41); // white
        ThisThread::sleep_for(2s);
    }
}
*/

#include "drivers/DigitalOut.h"
#include "platform/mbed_wait_api.h"


#define DELAY          1
#define T_CYCLE_0      4
#define T_CYCLE_1      1
#define WRITE_COMMAND  0x3A

#define rgb_led_Wait(NbCycles) wait_us(NbCycles * 10)


void rgb_led_SendBit(uint8_t bit)
{
    mbed::DigitalOut RGB_LED_SDI(RGB_LED);

    /* Start next cycle */
    RGB_LED_SDI = 1;
    rgb_led_Wait(DELAY);
    RGB_LED_SDI = 0;
    rgb_led_Wait(DELAY);

    if (bit) {
        RGB_LED_SDI = 1;
        rgb_led_Wait(DELAY);
        RGB_LED_SDI = 0;
        rgb_led_Wait(T_CYCLE_1);
    } else {
        rgb_led_Wait(T_CYCLE_0);
    }
}


void rgb_led_SenByte(uint8_t byte)
{
    rgb_led_SendBit(byte & (1 << 7));
    rgb_led_SendBit(byte & (1 << 6));
    rgb_led_SendBit(byte & (1 << 5));
    rgb_led_SendBit(byte & (1 << 4));
    rgb_led_SendBit(byte & (1 << 3));
    rgb_led_SendBit(byte & (1 << 2));
    rgb_led_SendBit(byte & (1 << 1));
    rgb_led_SendBit(byte & (1 << 0));
}


void rgb_led_on(uint8_t RedValue, uint8_t GreenValue, uint8_t BlueValue)
{
    mbed::DigitalOut RGB_SELECT(GPIO_SELECT2);
    mbed::DigitalOut RGB_LED_SDI(RGB_LED);

    RGB_SELECT = 1;

    /* TCycle measurement sequence */
    RGB_LED_SDI = 1;
    rgb_led_Wait(DELAY);
    RGB_LED_SDI = 0;
    rgb_led_Wait(T_CYCLE_0);

    /* Write command */
    rgb_led_SenByte(WRITE_COMMAND);

    /* Write the GS data */
    rgb_led_SenByte(RedValue);
    rgb_led_SenByte(GreenValue);
    rgb_led_SenByte(BlueValue);

    rgb_led_Wait(T_CYCLE_0 * 2);

    RGB_SELECT = 0;
}

void rgb_led_off(void)
{
    rgb_led_on(0, 0, 0);
}

void rgb_led_red(void)
{
    rgb_led_on(0x41, 0, 0);
}

void rgb_led_green(void)
{
    rgb_led_on(0, 0x41, 0);
}

void rgb_led_blue(void)
{
    rgb_led_on(0, 0, 0x41);
}

