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

#if !DEVICE_USTICKER
#error [NOT_SUPPORTED] test not supported
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

#define TICKER_TIME  1ms
#define MULTI_TICKER_TIME 100ms

#define TICKER_COUNT 16
volatile uint32_t callback_trigger_count = 0;
static const int test_timeout = 240;
static const int total_ticks = 10;


/* Tolerance is quite arbitrary due to large number of boards with varying level of accuracy */
#define TOLERANCE 1000us

volatile uint32_t ticker_callback_flag;
volatile uint32_t multi_counter;

Timer gtimer;
volatile int ticker_count = 0;

void ticker_callback_1(void)
{
    ++callback_trigger_count;
}

void ticker_callback_2(void)
{
    ++callback_trigger_count;
}


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
    core_util_atomic_incr_u32((uint32_t *)&multi_counter, 1);
}


/* Tests is to measure the accuracy of Ticker over a period of time
 *
 * 1) DUT would start to update callback_trigger_count every milli sec
 * 2) Host would query what is current count base_time, Device responds by the callback_trigger_count.
 * 3) Host after waiting for measurement stretch. It will query for device time again final_time.
 * 4) Host computes the drift considering base_time, final_time, transport delay and measurement stretch
 * 5) Finally host send the results back to device pass/fail based on tolerance.
 * 6) More details on tests can be found in timing_drift_auto.py
 */
void test_case_1x_ticker()
{
    char _key[11] = { };
    char _value[128] = { };
    int expected_key = 1;
    Ticker ticker;

    callback_trigger_count = 0;

    greentea_send_kv("timing_drift_check_start", 0);
    ticker.attach(&ticker_callback_1, TICKER_TIME);

    // wait for 1st signal from host
    do {
        greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
        expected_key = strcmp(_key, "base_time");
    } while (expected_key);
    greentea_send_kv(_key, callback_trigger_count * TICKER_TIME.count());

    // wait for 2nd signal from host
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
    greentea_send_kv(_key, callback_trigger_count * TICKER_TIME.count());

    //get the results from host
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

    TEST_ASSERT_EQUAL_STRING_MESSAGE("pass", _key, "Host side script reported a fail...");
}

/* Tests is to measure the accuracy of Ticker over a period of time
 *
 * 1) DUT would start to update callback_trigger_count every milli sec, we use 2 tickers
 *    to update the count alternatively.
 * 2) Host would query what is current count base_time, Device responds by the callback_trigger_count
 * 3) Host after waiting for measurement stretch. It will query for device time again final_time.
 * 4) Host computes the drift considering base_time, final_time, transport delay and measurement stretch
 * 5) Finally host send the results back to device pass/fail based on tolerance.
 * 6) More details on tests can be found in timing_drift_auto.py
 */
void test_case_2x_ticker()
{
    char _key[11] = { };
    char _value[128] = { };
    int expected_key =  1;
    Ticker ticker1, ticker2;

    callback_trigger_count = 0;

    ticker1.attach(ticker_callback_1, 2 * TICKER_TIME);
    // delay second ticker to have a pair of tickers tick every one millisecond
    wait_us(TICKER_TIME.count());
    greentea_send_kv("timing_drift_check_start", 0);
    ticker2.attach(ticker_callback_2, 2 * TICKER_TIME);

    // wait for 1st signal from host
    do {
        greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
        expected_key = strcmp(_key, "base_time");
    } while (expected_key);
    greentea_send_kv(_key, callback_trigger_count * TICKER_TIME.count());

    // wait for 2nd signal from host
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
    greentea_send_kv(_key, callback_trigger_count * TICKER_TIME.count());

    //get the results from host
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

    TEST_ASSERT_EQUAL_STRING_MESSAGE("pass", _key, "Host side script reported a fail...");
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
    Ticker ticker[TICKER_COUNT];
    milliseconds extra_wait = 5ms; // extra 5ms wait time

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
    Ticker ticker;
    const int attach_count = 10;

    for (int i = 0; i < attach_count; i++) {
        ticker_callback_flag = 0;
        gtimer.reset();

        gtimer.start();
        ticker.attach(callback(stop_gtimer_set_flag), MULTI_TICKER_TIME);
        while (!ticker_callback_flag);
        auto time_diff = gtimer.elapsed_time();

        TEST_ASSERT_DURATION_WITHIN(TOLERANCE, MULTI_TICKER_TIME, time_diff);
    }
}

/** Test if detach cancel scheduled callback event

    Given a Ticker with callback attached
    When the callback is detached
    Then the callback is not being called
 */
void test_detach(void)
{
    Ticker ticker;
    bool ret;
    milliseconds ticker_time = 100ms;
    milliseconds wait_time = 500ms;
    Semaphore sem(0, 1);

    ticker.attach(callback(sem_release, &sem), ticker_time);

    sem.acquire();

    sem.acquire();
    ticker.detach(); /* cancel */

    ret = sem.try_acquire_for(wait_time);
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
    Ticker ticker;
    ticker_callback_flag = 0;
    milliseconds delay = milliseconds{DELAY_US};

    gtimer.reset();
    gtimer.start();
    ticker.attach(callback(stop_gtimer_set_flag), delay);
    while (!ticker_callback_flag);
    ticker.detach();
    auto time_diff = gtimer.elapsed_time();

    TEST_ASSERT_DURATION_WITHIN(TOLERANCE, delay, time_diff);
}

/** Test single callback time via attach_us

    Given a Ticker
    When callback attached with time interval specified
    Then ticker properly executes callback within a specified time interval
 */
template<us_timestamp_t DELAY_US>
void test_attach_us_time(void)
{
    Ticker ticker;
    ticker_callback_flag = 0;

    gtimer.reset();
    gtimer.start();
    ticker.attach_us(callback(stop_gtimer_set_flag), DELAY_US);
    while (!ticker_callback_flag);
    ticker.detach();
    auto time_diff = gtimer.elapsed_time();

    TEST_ASSERT_DURATION_WITHIN(TOLERANCE, milliseconds{DELAY_US}, time_diff);
}


// Test cases
Case cases[] = {
    Case("Test attach for 0.01s and time measure", test_attach_time<10000>),
    Case("Test attach_us for 10ms and time measure", test_attach_us_time<10000>),
    Case("Test attach for 0.1s and time measure", test_attach_time<100000>),
    Case("Test attach_us for 100ms and time measure", test_attach_us_time<100000>),
    Case("Test attach for 0.5s and time measure", test_attach_time<500000>),
    Case("Test attach_us for 500ms and time measure", test_attach_us_time<500000>),
    Case("Test detach", test_detach),
    Case("Test multi call and time measure", test_multi_call_time),
    Case("Test multi ticker", test_multi_ticker),

#if !defined(SKIP_TIME_DRIFT_TESTS)
    Case("Test timers: 1x ticker", test_case_1x_ticker),
    Case("Test timers: 2x ticker", test_case_2x_ticker)
#endif
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(test_timeout, "timing_drift_auto");
    return utest::v1::greentea_test_setup_handler(number_of_cases);
}

utest::v1::Specification specification(greentea_test_setup, cases, utest::v1::greentea_test_teardown_handler);

int main()
{
    utest::v1::Harness::run(specification);
}

#endif // !DEVICE_USTICKER
