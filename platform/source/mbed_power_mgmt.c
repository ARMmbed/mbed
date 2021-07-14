/* mbed Microcontroller Library
 * Copyright (c) 2017-2019 ARM Limited
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

#include "platform/mbed_power_mgmt.h"
#include "platform/mbed_interface.h"
#include "platform/mbed_atomic.h"
#include "platform/mbed_critical.h"
#include "platform/mbed_error.h"
#include "platform/mbed_stats.h"

#include "hal/sleep_api.h"
#include "hal/us_ticker_api.h"
#include "hal/lp_ticker_api.h"
#include "platform/mbed_wait_api.h"

#include <stdio.h>

#if DEVICE_SLEEP

// deep sleep locking counter. A target is allowed to deep sleep if counter == 0
static uint16_t deep_sleep_lock = 0U;
#if defined(MBED_CPU_STATS_ENABLED) && DEVICE_LPTICKER
static us_timestamp_t sleep_time = 0;
static us_timestamp_t deep_sleep_time = 0;

static const ticker_data_t *sleep_ticker = NULL;
#endif

static inline us_timestamp_t read_us(void)
{
#if defined(MBED_CPU_STATS_ENABLED) && DEVICE_LPTICKER
    if (NULL == sleep_ticker) {
        sleep_ticker = get_lp_ticker_data();
    }
    return ticker_read_us(sleep_ticker);
#else
    return 0;
#endif
}

us_timestamp_t mbed_time_idle(void)
{
#if defined(MBED_CPU_STATS_ENABLED) && DEVICE_LPTICKER
    return (sleep_time + deep_sleep_time);
#else
    return 0;
#endif
}

us_timestamp_t mbed_uptime(void)
{
    return read_us();
}

us_timestamp_t mbed_time_sleep(void)
{
#if defined(MBED_CPU_STATS_ENABLED) && DEVICE_LPTICKER
    return sleep_time;
#else
    return 0;
#endif
}

us_timestamp_t mbed_time_deepsleep(void)
{
#if defined(MBED_CPU_STATS_ENABLED) && DEVICE_LPTICKER
    return deep_sleep_time;
#else
    return 0;
#endif
}

#if defined(MBED_SLEEP_TRACING_ENABLED) || defined(MBED_SLEEP_STAT_ENABLED)

// Length of the identifier extracted from the driver name to store for logging.
#define IDENTIFIER_WIDTH 15

// Number of drivers that can be stored in the structure
#define STATISTIC_COUNT  10

typedef struct sleep_statistic {
    char identifier[IDENTIFIER_WIDTH];
    uint8_t count;
} sleep_statistic_t;

static sleep_statistic_t sleep_stats[STATISTIC_COUNT];

static sleep_statistic_t *sleep_tracker_find(const char *const filename)
{
    char temp[IDENTIFIER_WIDTH];
    strncpy(temp, filename, IDENTIFIER_WIDTH);
    temp[IDENTIFIER_WIDTH - 1] = '\0';

    // Search for the a driver matching the current name and return it's index
    for (int i = 0; i < STATISTIC_COUNT; ++i) {
        if (strcmp(sleep_stats[i].identifier, temp) == 0) {
            return &sleep_stats[i];
        }
    }

    return NULL;
}

static sleep_statistic_t *sleep_tracker_add(const char *const filename)
{
    char temp[IDENTIFIER_WIDTH];
    strncpy(temp, filename, IDENTIFIER_WIDTH);
    temp[IDENTIFIER_WIDTH - 1] = '\0';

    for (int i = 0; i < STATISTIC_COUNT; ++i) {
        if (sleep_stats[i].identifier[0] == '\0') {
            core_util_critical_section_enter();
            strncpy(sleep_stats[i].identifier, temp, sizeof(temp));
            core_util_critical_section_exit();

            return &sleep_stats[i];
        }
    }

    mbed_error_printf("No free indexes left to use in mbed sleep tracker.\r\n");

    return NULL;
}

static void sleep_tracker_print_stats(void)
{
    mbed_error_printf("Sleep locks held:\r\n");
    for (int i = 0; i < STATISTIC_COUNT; ++i) {
        if (sleep_stats[i].count == 0) {
            continue;
        }

        if (sleep_stats[i].identifier[0] == '\0') {
            return;
        }

        mbed_error_printf("[id: %s, count: %u]\r\n", sleep_stats[i].identifier,
                          sleep_stats[i].count);
    }
}

void sleep_tracker_lock(const char *const filename, int line)
{
    sleep_statistic_t *stat = sleep_tracker_find(filename);

    // Entry for this driver does not exist, create one.
    if (stat == NULL) {
        stat = sleep_tracker_add(filename);
    }

    core_util_atomic_incr_u8(&stat->count, 1);

#if defined(MBED_SLEEP_TRACING_ENABLED)
    mbed_error_printf("LOCK: %s, ln: %i, lock count: %u\r\n", filename, line, deep_sleep_lock);
#endif
}

void sleep_tracker_unlock(const char *const filename, int line)
{
    sleep_statistic_t *stat = sleep_tracker_find(filename);

    // Entry for this driver does not exist, something went wrong.
    if (stat == NULL) {
        mbed_error_printf("Unlocking sleep for driver that was not previously locked: %s, ln: %i\r\n", filename, line);
        return;
    }

    core_util_atomic_decr_u8(&stat->count, 1);

#if defined(MBED_SLEEP_TRACING_ENABLED)
    mbed_error_printf("UNLOCK: %s, ln: %i, lock count: %u\r\n", filename, line, deep_sleep_lock);
#endif
}

#endif // MBED_SLEEP_TRACING_ENABLED

void sleep_manager_lock_deep_sleep_internal(void)
{
    if (core_util_atomic_incr_u16(&deep_sleep_lock, 1) == 0) {
        MBED_ERROR1(MBED_MAKE_ERROR(MBED_MODULE_HAL, MBED_ERROR_CODE_OVERFLOW), "DeepSleepLock overflow (> 0xFFFF)", deep_sleep_lock);
    }
}

void sleep_manager_unlock_deep_sleep_internal(void)
{
    if (core_util_atomic_decr_u16(&deep_sleep_lock, 1) == 0xFFFF) {
        MBED_ERROR1(MBED_MAKE_ERROR(MBED_MODULE_HAL, MBED_ERROR_CODE_UNDERFLOW), "DeepSleepLock underflow (< 0)", deep_sleep_lock);
    }
}

bool sleep_manager_can_deep_sleep(void)
{
    return core_util_atomic_load_u16(&deep_sleep_lock) == 0;
}

bool sleep_manager_can_deep_sleep_test_check()
{
    uint32_t check_time_ns = 2000000;

    while (check_time_ns) {
        if (sleep_manager_can_deep_sleep()) {
            return true;
        }

        wait_ns(100000); // 100 us
        check_time_ns -= 100000;
    }
    return false;
}

void sleep_manager_sleep_auto(void)
{
#if defined(MBED_SLEEP_TRACING_ENABLED)
    sleep_tracker_print_stats();
#endif
    core_util_critical_section_enter();
#if defined(MBED_CPU_STATS_ENABLED) && DEVICE_LPTICKER
    us_timestamp_t start = read_us();
    bool deep = false;
#endif

// debug profile should keep debuggers attached, no deep sleep allowed
#ifdef MBED_DEBUG
    hal_sleep();
#else
    if (sleep_manager_can_deep_sleep()) {
#if defined(MBED_CPU_STATS_ENABLED) && DEVICE_LPTICKER
        deep = true;
#endif
        hal_deepsleep();
    } else {
#if defined(MBED_SLEEP_STAT_ENABLED)
        sleep_tracker_print_stats();
#endif
        hal_sleep();
    }
#endif

#if defined(MBED_CPU_STATS_ENABLED) && DEVICE_LPTICKER
    us_timestamp_t end = read_us();
    if (true == deep) {
        deep_sleep_time += end - start;
    } else {
        sleep_time += end - start;
    }
#endif
    core_util_critical_section_exit();
}

#else

// locking is valid only if DEVICE_SLEEP is defined
// we provide empty implementation

void sleep_manager_lock_deep_sleep_internal(void)
{

}

void sleep_manager_unlock_deep_sleep_internal(void)
{

}

bool sleep_manager_can_deep_sleep(void)
{
    // no sleep implemented
    return false;
}

#endif
