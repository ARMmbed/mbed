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
#ifndef MBED_PWMOUTDEF_H
#define MBED_PWMOUTDEF_H

#include "s5js100.h"

#define S5JS100_PWM_TCFG0_OFFSET    (0x0000)
#define S5JS100_PWM_TCFG1_OFFSET    (0x0004)
#define S5JS100_PWM_TCON_OFFSET     (0x0008)
#define S5JS100_PWM_TIMER0_OFFSET   (0x000C)
#define S5JS100_PWM_TIMER1_OFFSET   (0x0018)
#define S5JS100_PWM_TIMER2_OFFSET   (0x0024)
#define S5JS100_PWM_TIMER3_OFFSET   (0x0030)
#define S5JS100_PWM_TCNTB_OFFSET    (0x0000)
#define S5JS100_PWM_TCMPB_OFFSET    (0x0004)
#define S5JS100_PWM_TCNTO_OFFSET    (0x0008)

#define S5JS100_PWM_TCFG0           (S5JS100_PWM + S5JS100_PWM_TCFG0_OFFSET)
#define S5JS100_PWM_TCFG1           (S5JS100_PWM + S5JS100_PWM_TCFG1_OFFSET)
#define S5JS100_PWM_TCON            (S5JS100_PWM + S5JS100_PWM_TCON_OFFSET)

typedef enum {
    PWM_TCON_OFFSET_TIMER0 = 0,
    PWM_TCON_OFFSET_TIMER1 = 8,
    PWM_TCON_OFFSET_TIMER2 = 12,
    PWM_TCON_OFFSET_TIMER3 = 16,
} PWMTconBitOffset;

#define S5JS100_PWM_TCON_OFFSET_TIMER0  (0)
#define S5JS100_PWM_TCON_OFFSET_TIMER1  (8)
#define S5JS100_PWM_TCON_OFFSET_TIMER2  (12)
#define S5JS100_PWM_TCON_OFFSET_TIMER3  (16)

#define S5JS100_PWM_TCON_OFFSET_START   (0)
#define S5JS100_PWM_TCON_OFFSET_UPDATE  (1)
#define S5JS100_PWM_TCON_OFFSET_INVERTER    (2)
#define S5JS100_PWM_TCON_OFFSET_AUTOLOAD    (3)

#define S5JS100_OS_SCCLK                    26000000
#define S5JS100_OS_US                       ()

#define S5JS100_PWM_DEADZONE_ENABLE         0
#define S5JS100_PWM_DEADZONE_DISABLE        1

#define S5JS100_PWM_AUTORELOAD_ENABLE       1
#define S5JS100_PWM_AUTORELOAD_DISABLE      0

#define S5JS100_PWM_MANULUPDATE_ENABLE      1
#define S5JS100_PWM_MANULUPDATE_DISABLE     0

#define S5JS100_PWM_OUTINVERTER_ENABLE      1
#define S5JS100_PWM_OUTINVERTER_DISABLE     0

#define S5JS100_PWM_TIMER_START             1
#define S5JS100_PWM_TIMER_STOP              0

#define S5JS100_PWM_DEFAULT_PRESCALER       1
#define S5JS100_PWM_DEFAULT_DUTYCYCLE       0
#define S5JS100_PWM_DEFAULT_PERIOD          10
#define S5JS100_PWM_DEFAULT_PULSEWIDTH      0
#define S5JS100_PWM_DEFAULT_DEADZONE        S5JS100_PWM_DEADZONE_DISABLE
#define S5JS100_PWM_DEFAULT_AUTORELOAD      S5JS100_PWM_AUTORELOAD_ENABLE
#define S5JS100_PWM_DEFAULT_MANULUPDATE     S5JS100_PWM_MANULUPDATE_ENABLE
#define S5JS100_PWM_DEFAULT_OUTINVERTER     S5JS100_PWM_OUTINVERTER_DISABLE

#define S5JS100_SENDS_TO_MSENONDS           1000
#define S5JS100_MSENDS_TO_USENONDS          1000
#define S5JS100_SENDS_TO_USENONDS           1000000

#define S5JS100_USENONDS_TO_TICK(S)         ((S * (S5JS100_OS_SCCLK / S5JS100_SENDS_TO_USENONDS)) / (S5JS100_PWM_DEFAULT_PRESCALER + 1) - 1)

#define S5JS100_TICK_TO_USENONDS(T)         (((S5JS100_PWM_DEFAULT_PRESCALER + 1) * (T + 1) / S5JS100_OS_SCCLK) \
                                            * S5JS100_SENDS_TO_USENONDS)

#define S5JS100_PWM_TCON_OFFSET_TIMERn(X)   ((X == 0) ? (0) : (8 + 4 * (X - 1)))


#endif