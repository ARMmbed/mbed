/* mbed Microcontroller Library
 * Copyright (c) 2021 ARM Limited
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
#ifndef MBED_NRF5X_ADAPTATION_H
#define MBED_NRF5X_ADAPTATION_H

#include "nrf.h"

/* RTC1 and TIMER3 used by Cordio baseband scheduler. */
#define PAL_BB_RTC                          NRF_RTC1
#define PAL_BB_TIMER                        NRF_TIMER3
#define PAL_BB_IRQn                         TIMER3_IRQn
#define PAL_BB_IRQ_HANDLER                  TIMER3_IRQHandler
#define PAL_BB_IRQ_EXPIRY_CHANNEL           5
#define PAL_BB_TIMER_EXPIRY_INTENSET_MASK   TIMER_INTENSET_COMPARE5_Msk
#define PAL_BB_TIMER_EXPIRY_INTENCLR_MASK   TIMER_INTENCLR_COMPARE5_Msk

#define PAL_ALLOW_DEEP_SLEEP                1
#define PAL_WAKE_LATENCY_MS                 5

#endif // ! MBED_NRF5X_ADAPTATION_H
