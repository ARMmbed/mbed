/* mbed Microcontroller Library
 * Copyright (c) 2013-2017 ARM Limited
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
#include "mbed.h"
#include "greentea-client/test_env.h"
#include "utest/utest.h"
#include "unity/unity.h"


#if !DEVICE_LPTICKER
#error [NOT_SUPPORTED] Low power ticker not supported for this target
#else


using utest::v1::Case;
using std::chrono::milliseconds;

#define TEST_ASSERT_EQUAL_DURATION(expected, actual) \
    do { \
        using ct = std::common_type_t<decltype(expected), decltype(actual)>; \
        TEST_ASSERT_EQUAL(ct(expected).count(), ct(actual).count()); \
    } while (0)

#define TEST_ASSERT_DURATION_WITHIN(delta, expected, actual) \
    do { \
        using ct = std::common_type_t<decltype(delta), decltype(expected), decltype(actual)>; \
        TEST_ASSERT_INT_WITHIN(ct(delta).count(), ct(expected).count(), ct(actual).count()); \
    } while (0)

#define TICKER_COUNT 16
#define MULTI_TICKER_TIME 100ms

/* Due to poor accuracy of LowPowerTicker on many platforms
   there is no sense to tune tolerance value as it was in Ticker tests.

   Tolerance value is set to 600us for measurement inaccuracy + 5% tolerance
   for LowPowerTicker. */
#define TOLERANCE_US(DELAY) (600us + DELAY / 20)

static const int test_timeout = 10;
volatile uint32_t ticker_callback_flag;
volatile uint32_t multi_counter;
Timer gtimer;



void sem_release(Semaphore *sem)
{
    sem->release();
}


void stop_gtimer_set_flag(void)
{
    gtimer.stop();
    core_util_atomic_incr_u32((uint32_t *)&ticker_callback_flag, 1);
}

void increment_multi_counter(void)
{
    core_util_atomic_incr_u32((uint32_t *)&multi_counter, 1);;
}

/** Test many tickers run one after the other

    Given many Tickers
    When schedule them one after the other with the same time intervals
    Then tickers properly execute callbacks
    When schedule them one after the other with the different time intervals
    Then tickers properly execute callbacks
 */
void test_multi_ticker(void)
{
    LowPowerTicker ticker[TICKER_COUNT];
    milliseconds extra_wait = 10ms; // extra 10ms wait time

    multi_counter = 0;
    for (int i = 0; i < TICKER_COUNT; i++) {
        ticker[i].attach(callback(increment_multi_counter), MULTI_TICKER_TIME);
    }

    ThisThread::sleep_for(MULTI_TICKER_TIME + extra_wait);
    TEST_ASSERT_EQUAL(TICKER_COUNT, multi_counter);

    for (int i = 0; i < TICKER_COUNT; i++) {
        ticker[i].detach();
    }
    // Because detach calls schedule_interrupt in some circumstances
    // (e.g. when head event is removed), it's good to check if
    // no more callbacks were triggered during detaching.
    TEST_ASSERT_EQUAL(TICKER_COUNT, multi_counter);

    multi_counter = 0;
    for (int i = 0; i < TICKER_COUNT; i++) {
        ticker[i].attach(callback(increment_multi_counter), MULTI_TICKER_TIME + milliseconds{i});
    }

    ThisThread::sleep_for(MULTI_TICKER_TIME + milliseconds{TICKER_COUNT} + extra_wait);
    TEST_ASSERT_EQUAL(TICKER_COUNT, multi_counter);

    for (int i = 0; i < TICKER_COUNT; i++) {
        ticker[i].detach();
    }
    // Because detach calls schedule_interrupt in some circumstances
    // (e.g. when head event is removed), it's good to check if
    // no more callbacks were triggered during detaching.
    TEST_ASSERT_EQUAL(TICKER_COUNT, multi_counter);
}

/** Test multi callback time

    Given a Ticker
    When the callback is attached multiple times
    Then ticker properly execute callback multiple times
 */
void test_multi_call_time(void)
{
    LowPowerTicker ticker;
    const int attach_count = 10;

    for (int i = 0; i < attach_count; i++) {
        ticker_callback_flag = 0;
        gtimer.reset();

        gtimer.start();
        ticker.attach(callback(stop_gtimer_set_flag), MULTI_TICKER_TIME);
        while (!ticker_callback_flag);
        auto time_diff = gtimer.elapsed_time();

        TEST_ASSERT_DURATION_WITHIN(TOLERANCE_US(MULTI_TICKER_TIME), MULTI_TICKER_TIME, time_diff);
    }
}

/** Test if detach cancel scheduled callback event

    Given a Ticker with callback attached
    When the callback is detached
    Then the callback is not being called
 */
void test_detach(void)
{
    LowPowerTicker ticker;
    bool ret;
    const milliseconds ticker_time_s = 100ms;
    const milliseconds wait_time_ms = 500ms;

    Semaphore sem(0, 1);

    ticker.attach(callback(sem_release, &sem), ticker_time_s);

    sem.acquire();

    sem.acquire();
    ticker.detach(); /* cancel */

    ret = sem.try_acquire_for(wait_time_ms);
    TEST_ASSERT_FALSE(ret);
}

/** Test single callback time via attach

    Given a Ticker
    When callback attached with time interval specified
    Then ticker properly executes callback within a specified time interval
 */
template<us_timestamp_t DELAY_US>
void test_attach_time(void)
{
    LowPowerTicker ticker;
    ticker_callback_flag = 0;
    milliseconds DELAY = milliseconds{DELAY_US};

    gtimer.reset();
    gtimer.start();
    ticker.attach(callback(stop_gtimer_set_flag), DELAY);
    while (!ticker_callback_flag);
    ticker.detach();
    auto time_diff = gtimer.elapsed_time();

    TEST_ASSERT_DURATION_WITHIN(TOLERANCE_US(DELAY), DELAY, time_diff);
}

/** Test single callback time via attach_us

    Given a Ticker
    When callback attached with time interval specified
    Then ticker properly executes callback within a specified time interval
 */
template<us_timestamp_t DELAY_US>
void test_attach_us_time(void)
{
    LowPowerTicker ticker;
    ticker_callback_flag = 0;
    milliseconds DELAY_MS = milliseconds{DELAY_US};

    gtimer.reset();
    gtimer.start();
    ticker.attach_us(callback(stop_gtimer_set_flag), DELAY_US);
    while (!ticker_callback_flag);
    ticker.detach();
    auto time_diff = gtimer.elapsed_time();

    TEST_ASSERT_DURATION_WITHIN(TOLERANCE_US(milliseconds{DELAY_MS}), DELAY_MS, time_diff);
}

// Test cases
Case cases[] = {
    Case("Test attach for 0.001s and time measure", test_attach_time<1000>),
    Case("Test attach_us for 1ms and time measure", test_attach_us_time<1000>),
    Case("Test attach for 0.01s and time measure", test_attach_time<10000>),
    Case("Test attach_us for 10ms and time measure", test_attach_us_time<10000>),
    Case("Test attach for 0.1s and time measure", test_attach_time<100000>),
    Case("Test attach_us for 100ms and time measure", test_attach_us_time<100000>),
    Case("Test attach for 0.5s and time measure", test_attach_time<500000>),
    Case("Test attach_us for 500ms and time measure", test_attach_us_time<500000>),
    Case("Test detach", test_detach),
    Case("Test multi call and time measure", test_multi_call_time),
    Case("Test multi ticker", test_multi_ticker),
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(test_timeout, "default_auto");
    return utest::v1::greentea_test_setup_handler(number_of_cases);
}

utest::v1::Specification specification(greentea_test_setup, cases, utest::v1::greentea_test_teardown_handler);

int main()
{
    utest::v1::Harness::run(specification);
}

#endif // !DEVICE_LPTICKER
