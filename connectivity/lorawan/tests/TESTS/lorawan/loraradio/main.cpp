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

#if COMPONENT_SX1272
#include "SX1272_LoRaRadio.h"
#elif COMPONENT_SX1276
#include "SX1276_LoRaRadio.h"
#elif COMPONENT_SX126X
#include "SX126X_LoRaRadio.h"
#elif (TARGET_STM32WL)
#include "STM32WL_LoRaRadio.h"
#else
#error [NOT_SUPPORTED] Lora radio is not configured
#endif

#include "utest.h"
#include "unity.h"
#include "greentea-client/test_env.h"

#include "Semaphore.h"

#include "mbed_trace.h"
#define TRACE_GROUP "RTST"

#include "LoRaRadio.h"

using namespace utest::v1;
using namespace mbed;

static LoRaRadio *radio = NULL;
rtos::Semaphore event_sem(0);

enum event_t {
    EV_NONE,
    EV_TX_DONE,
    EV_TX_TIMEOUT,
    EV_RX_DONE,
    EV_RX_TIMEOUT,
    EV_RX_ERROR,
};
static volatile event_t received_event;


static void tx_done()
{
    TEST_ASSERT_EQUAL(EV_NONE, received_event);
    received_event = EV_TX_DONE;
    TEST_ASSERT_EQUAL(osOK, event_sem.release());
}

static void tx_timeout()
{
    TEST_ASSERT_EQUAL(EV_NONE, received_event);
    received_event = EV_TX_TIMEOUT;
    TEST_ASSERT_EQUAL(osOK, event_sem.release());
}

static void rx_done(const uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    TEST_ASSERT_EQUAL(EV_NONE, received_event);
    received_event = EV_RX_DONE;
    TEST_ASSERT_EQUAL(osOK, event_sem.release());
}

static void rx_timeout()
{
    TEST_ASSERT_EQUAL(EV_NONE, received_event);
    received_event = EV_RX_TIMEOUT;
    TEST_ASSERT_EQUAL(osOK, event_sem.release());
}

static void rx_error()
{
    TEST_ASSERT_EQUAL(EV_NONE, received_event);
    received_event = EV_RX_ERROR;
    TEST_ASSERT_EQUAL(osOK, event_sem.release());
}

static radio_events radio_callbacks = {
    .tx_done    = tx_done,
    .tx_timeout = tx_timeout,
    .rx_done    = rx_done,
    .rx_timeout = rx_timeout,
    .rx_error   = rx_error
};


void test_random()
{
    const uint32_t rand1 = radio->random();
    const uint32_t rand2 = radio->random();
    TEST_ASSERT_NOT_EQUAL(rand1, rand2);
}

void test_set_tx_config()
{
    uint8_t buffer[] = {0};

    TEST_ASSERT_EQUAL(RF_IDLE, radio->get_status());

    radio->set_tx_config(MODEM_LORA, 13, 0,  // modem, power, fdev,
                         0, 7,               // bandwidth, datarate,
                         1, 8,               // coderate, preamble_len,
                         false, true, false, // fix_len, crc_on, freq_hop_on,
                         0, false, 1000);    // hop_period, iq_inverted, timeout
    radio->send(buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL(RF_TX_RUNNING, radio->get_status());

    TEST_ASSERT_TRUE(event_sem.try_acquire_for(1000));
    TEST_ASSERT_EQUAL(EV_TX_DONE, received_event);
    received_event = EV_NONE;
}

void test_set_rx_config()
{
    TEST_ASSERT_EQUAL(RF_IDLE, radio->get_status());

    radio->set_rx_config(MODEM_LORA, 0,     // modem, bandwidth,
                         7, 1,              // datarate, coderate,
                         0, 8,              // bandwidth_afc, preamble_len,
                         24, false,         // symb_timeout, fix_len,
                         0,                 // payload_len,
                         false, false, 0,   // crc_on, freq_hop_on, hop_period,
                         true, false);      // iq_inverted, rx_continuous
    radio->receive();

    TEST_ASSERT_EQUAL(RF_RX_RUNNING, radio->get_status());

    TEST_ASSERT_TRUE(event_sem.try_acquire_for(1000));

    // Nobody was sending to us so timeout is expected.
    TEST_ASSERT_EQUAL(EV_RX_TIMEOUT, received_event);
    received_event = EV_NONE;
}

void test_time_on_air()
{
    radio->set_rx_config(MODEM_LORA, 0,
                         7, 1,
                         0, 8,
                         24, false,
                         0,
                         false, false, 0,
                         true, false);
    TEST_ASSERT_EQUAL(52, radio->time_on_air(MODEM_LORA, 20));

    radio->set_tx_config(MODEM_LORA, 13, 0,
                         0, 7,
                         1, 8,
                         false, true, false,
                         0, false, 100);
    TEST_ASSERT_EQUAL(72, radio->time_on_air(MODEM_LORA, 32));

    // TODO: Add FSK tests
}

void test_perform_carrier_sense()
{
    TEST_ASSERT_TRUE(radio->perform_carrier_sense(MODEM_FSK, 865000000, -20, 1));
    TEST_ASSERT_TRUE(radio->perform_carrier_sense(MODEM_LORA, 865000000, -20, 1));
}

void test_check_rf_frequency()
{
    // Test EU868 frequency
    TEST_ASSERT_TRUE(radio->check_rf_frequency(865000000));
}

// Test setup
utest::v1::status_t test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(20, "default_auto");

    mbed_trace_init();

    return verbose_test_setup_handler(number_of_cases);
}

utest::v1::status_t case_setup_handler(const Case *const source, const size_t index_of_case)
{
#if COMPONENT_SX1272
    radio = new SX1272_LoRaRadio();
#elif COMPONENT_SX1276
    radio = new SX1276_LoRaRadio();
#elif COMPONENT_SX126X
    radio = new SX126X_LoRaRadio();
#elif (TARGET_STM32WL)
    radio = new STM32WL_LoRaRadio();
#endif

    TEST_ASSERT(radio);
    radio->init_radio(&radio_callbacks);
    radio->sleep();

    return greentea_case_setup_handler(source, index_of_case);
}

utest::v1::status_t case_teardown_handler(const Case *const source, const size_t passed, const size_t failed, const failure_t reason)
{
    radio->sleep();

#if COMPONENT_SX1272
    delete static_cast<SX1272_LoRaRadio *>(radio);

#elif COMPONENT_SX1276
    delete static_cast<SX1276_LoRaRadio *>(radio);

#elif COMPONENT_SX126X
    delete static_cast<SX126X_LoRaRadio *>(radio);

#elif TARGET_STM32WL
    delete static_cast<STM32WL_LoRaRadio *>(radio);

#endif
    radio = NULL;

    return greentea_case_teardown_handler(source, passed, failed, reason);
}

const Case cases[] = {
    Case("Test random",        case_setup_handler, test_random,        case_teardown_handler),
    Case("Test set_tx_config", case_setup_handler, test_set_tx_config, case_teardown_handler),
    Case("Test set_rx_config", case_setup_handler, test_set_rx_config, case_teardown_handler),
    Case("Test time_on_air",   case_setup_handler, test_time_on_air,   case_teardown_handler),
    Case("Test perform_carrier_sense", case_setup_handler, test_perform_carrier_sense, case_teardown_handler),
    Case("Test check_rf_frequency",    case_setup_handler, test_check_rf_frequency,    case_teardown_handler),
};

Specification specification(test_setup, cases);

int main()
{
    return !Harness::run(specification);
}
