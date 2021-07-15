/* mbed Microcontroller Library
 * Copyright (c) 2015-2018 ARM Limited and affiliates.
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

#ifndef COMMON_RTC_H
#define COMMON_RTC_H

#include "nrf_rtc.h"

#define RTC_COUNTER_BITS        24u
#define RTC_FREQ                32768u

// Instances 1 and 2 are reserved for Cordio
// Instance 0 is used as a common one for lp_ticker and (in case
// of NRF51) as an alternative tick source for RTOS.
#define COMMON_RTC_INSTANCE     NRF_RTC0
#define COMMON_RTC_IRQn         RTC0_IRQn
#define COMMON_RTC_IRQ_HANDLER  RTC0_IRQHandler
#define OS_TICK_CC_CHANNEL      0
#define LP_TICKER_CC_CHANNEL    1

#define COMMON_RTC_EVENT_COMPARE(channel) \
    CONCAT_2(NRF_RTC_EVENT_COMPARE_, channel)
#define COMMON_RTC_INT_COMPARE_MASK(channel) \
    CONCAT_3(NRF_RTC_INT_COMPARE, channel, _MASK)

#define OS_TICK_EVENT       COMMON_RTC_EVENT_COMPARE(OS_TICK_CC_CHANNEL)
#define OS_TICK_INT_MASK    COMMON_RTC_INT_COMPARE_MASK(OS_TICK_CC_CHANNEL)
#define LP_TICKER_EVENT     COMMON_RTC_EVENT_COMPARE(LP_TICKER_CC_CHANNEL)
#define LP_TICKER_INT_MASK  COMMON_RTC_INT_COMPARE_MASK(LP_TICKER_CC_CHANNEL)

extern bool              m_common_rtc_enabled;
extern uint32_t volatile m_common_rtc_overflows;
extern bool volatile     lp_ticker_interrupt_fire;

void     common_rtc_free(void);
void     common_rtc_init(void);
uint32_t common_rtc_32bit_ticks_get(void);
uint64_t common_rtc_64bit_us_get(void);
void     common_rtc_set_interrupt(uint32_t us_timestamp, uint32_t cc_channel,
                                  uint32_t int_mask);

#endif // COMMON_RTC_H
