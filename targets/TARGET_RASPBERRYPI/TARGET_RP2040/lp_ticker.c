/* mbed Microcontroller Library
 * Copyright (c) 2015 ARM Limited
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
#include "lp_ticker_api.h"
#include "hardware/rtc.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/clocks.h"
#include "time.h"

#include "platform/mbed_critical.h"

#define RTC_COUNTER_BITS        32u
#define RTC_FREQ                1u

#if DEVICE_LPTICKER

/* LP ticker counts with 1Hz resolution */
const ticker_info_t* lp_ticker_get_info()
{
    static const ticker_info_t info = {
        RTC_FREQ,
        RTC_COUNTER_BITS
    };
    return &info;
}

static datetime_t now = {
    .year = 0,
    .month = 1,
    .day = 1,
    .dotw = 0,
    .hour = 0,
    .min = 0,
    .sec = 0
};

static datetime_t* epoch_to_datetime(timestamp_t epoch, datetime_t* date) {
    return NULL;
}

static timestamp_t datetime_to_epoch(datetime_t* date) {
    struct tm t;
    time_t t_of_day;

    t.tm_year = date->year;             // Year
    t.tm_mon = date->month + 1;           // Month, where 0 = jan
    t.tm_mday = date->day;              // Day of the month
    t.tm_hour = date->hour;
    t.tm_min = date->min;
    t.tm_sec = date->sec;
    t_of_day = mktime(&t);

    return (long)t_of_day;
}

void lp_ticker_init(void)
{
    if (rtc_running()) {
        // Populates now struct with existing data
        lp_ticker_read();
        return;
    }
    rtc_init();
    rtc_set_datetime(&now);
}

void lp_ticker_free(void)
{
}

uint32_t lp_ticker_read()
{
    rtc_get_datetime(&now);
    return 0; //datetime_to_epoch(&now);
}

void lp_ticker_set_interrupt(timestamp_t timestamp)
{
    timestamp_t target_ts = lp_ticker_read() + timestamp;
    datetime_t target;
    epoch_to_datetime(target_ts, &target);
    rtc_set_alarm(&target, NULL);
}

void lp_ticker_fire_interrupt(void)
{
    rtc_get_datetime(&now);
    rtc_set_alarm(&now, NULL);
}

void lp_ticker_disable_interrupt(void)
{
    rtc_disable_alarm();
}

void lp_ticker_clear_interrupt(void)
{
}

#endif // DEVICE_LPTICKER
