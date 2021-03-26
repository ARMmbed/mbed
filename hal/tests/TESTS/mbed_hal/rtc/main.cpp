/* mbed Microcontroller Library
 * Copyright (c) 2017 ARM Limited
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

#if !DEVICE_RTC
#error [NOT_SUPPORTED] RTC API not supported for this target
#else

#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"
#include "rtc_test.h"

#include "mbed.h"
#include "drivers/RealTimeClock.h"
#include "rtc_api.h"
#include <type_traits>
#include <mstd_atomic>

using namespace utest::v1;
using namespace std::chrono;

static constexpr auto WAIT_TIME = 4s;
static constexpr auto WAIT_TOLERANCE = 1s;

#define TEST_ASSERT_DURATION_WITHIN(delta, expected, actual) \
    do { \
        using ct = std::common_type_t<decltype(delta), decltype(expected), decltype(actual)>; \
        TEST_ASSERT_INT_WITHIN(ct(delta).count(), ct(expected).count(), ct(actual).count()); \
    } while (0)

#if DEVICE_LPTICKER
mstd::atomic_bool expired;

void set_flag_true(void)
{
    expired = true;
}

/* Auxiliary function to test if RTC continue counting in
 * sleep and deep-sleep modes. */
void rtc_sleep_test_support(bool deepsleep_mode)
{
    LowPowerTimeout timeout;
    const auto start = RealTimeClock::time_point(100s);
    expired = false;

    /*
     * Since deepsleep() may shut down the UART peripheral, we wait for 10ms
     * to allow for hardware serial buffers to completely flush.
     * This should be replaced with a better function that checks if the
     * hardware buffers are empty. However, such an API does not exist now,
     * so we'll use the ThisThread::sleep_for() function for now.
     */
    ThisThread::sleep_for(10ms);

    RealTimeClock::init();

    if (deepsleep_mode == false) {
        sleep_manager_lock_deep_sleep();
    }

    RealTimeClock::write(start);

    timeout.attach(set_flag_true, 4s);

    TEST_ASSERT(sleep_manager_can_deep_sleep_test_check() == deepsleep_mode);

    while (!expired) {
        sleep();
    }

    const auto stop = RealTimeClock::now();

    TEST_ASSERT_DURATION_WITHIN(1s, 4s, stop - start);

    timeout.detach();

    if (deepsleep_mode == false) {
        sleep_manager_unlock_deep_sleep();
    }

    RealTimeClock::free();
}
#endif

/* Test that ::rtc_init can be called multiple times. */
void rtc_init_test()
{
    for (int i = 0; i < 10; i++) {
        RealTimeClock::init();
    }

    RealTimeClock::free();
}

#if DEVICE_LPTICKER
/** Test that the RTC keeps counting in the various sleep modes. */

void rtc_sleep_test()
{
    /* Test sleep mode. */
    rtc_sleep_test_support(false);

    /* Test deep-sleep mode. */
    rtc_sleep_test_support(true);
}
#endif

/* Test that the RTC keeps counting even after ::rtc_free has been called. */
void rtc_persist_test()
{
    const auto start = RealTimeClock::time_point(100s);
    RealTimeClock::init();
    RealTimeClock::write(start);
    RealTimeClock::free();

    ThisThread::sleep_for(WAIT_TIME);

    RealTimeClock::init();
    const auto stop = RealTimeClock::now();
    const bool enabled = RealTimeClock::isenabled();
    RealTimeClock::free();

    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_DURATION_WITHIN(WAIT_TOLERANCE, WAIT_TIME, stop - start);
}

/* Test time does not glitch backwards due to an incorrectly implemented ripple counter driver. */
void rtc_glitch_test()
{
    const auto start = RealTimeClock::time_point(0xffffes);
    RealTimeClock::init();

    RealTimeClock::write(start);
    auto last = start;
    while (last < start + 4s) {
        const auto cur = RealTimeClock::now();
        TEST_ASSERT(cur >= last);
        last = cur;
    }

    RealTimeClock::free();
}

/* Test that the RTC correctly handles different time values. */
void rtc_range_test()
{
    static const RealTimeClock::time_point starts[] = {
        RealTimeClock::time_point{0x00000000s},
        RealTimeClock::time_point{0x7ffffffas},   //0xEFFFFFFF had already overflow, the max is 0x7FFFFFFF, time_t is 32bit and signed
        RealTimeClock::time_point{0x00001000s},
        RealTimeClock::time_point{0x00010000s},
    };

    RealTimeClock::init();
    for (const auto &start : starts) {
        RealTimeClock::write(start);
        ThisThread::sleep_for(WAIT_TIME);
        const auto stop = RealTimeClock::now();
        TEST_ASSERT_DURATION_WITHIN(WAIT_TOLERANCE, WAIT_TIME, stop - start);
    }
    RealTimeClock::free();
}

/* Test that the RTC accuracy is at least 10%. */
void rtc_accuracy_test()
{
    Timer timer1;

    const auto start = RealTimeClock::time_point(100s);
    RealTimeClock::init();
    RealTimeClock::write(start);

    timer1.start();
    while (RealTimeClock::now() < (start + 10s)) {
        /* Just wait. */
    }
    timer1.stop();

    /* RTC accuracy is at least 10%. */
    TEST_ASSERT_DURATION_WITHIN(1s, 10s, timer1.elapsed_time());
}

/* Test that ::rtc_write/::rtc_read functions provides availability to set/get RTC time. */
void rtc_write_read_test()
{
    RealTimeClock::init();

    /* NB: IAR compilation issue with "auto init_val = RealTimeClock::time_point(100s)" */
    for (auto init_val = RealTimeClock::time_point(seconds(100)); init_val < RealTimeClock::time_point(400s); init_val += 100s) {
        core_util_critical_section_enter();

        RealTimeClock::write(init_val);
        const auto read_val = RealTimeClock::now();

        core_util_critical_section_exit();

        /* No tolerance is provided since we should have 1 second to
         * execute this case after the RTC time is set.
         */
        TEST_ASSERT(init_val == read_val);
    }

    RealTimeClock::free();
}

/* Test that ::is_enabled function returns 1 if the RTC is counting and the time has been set. */
void rtc_enabled_test()
{
    /* Since some platforms use RTC for low power timer RTC may be already enabled.
     * Because of that we will only verify if rtc_isenabled() returns 1 in case when init is done
     * and RTC time is set.
     */

    RealTimeClock::init();
    RealTimeClock::write(RealTimeClock::time_point(0s));
    TEST_ASSERT_TRUE(RealTimeClock::isenabled());
    RealTimeClock::free();
}

Case cases[] = {
    Case("RTC - init", rtc_init_test),
#if DEVICE_LPTICKER
    Case("RTC - sleep", rtc_sleep_test),
#endif
    Case("RTC - persist", rtc_persist_test),
    Case("RTC - glitch", rtc_glitch_test),
    Case("RTC - range", rtc_range_test),
    Case("RTC - accuracy", rtc_accuracy_test),
    Case("RTC - write/read", rtc_write_read_test),
    Case("RTC - enabled", rtc_enabled_test),
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(60, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main()
{
    Harness::run(specification);
}

#endif // !DEVICE_RTC
