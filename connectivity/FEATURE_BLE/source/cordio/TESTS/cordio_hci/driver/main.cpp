/* mbed Microcontroller Library
 * Copyright (c) 2018 ARM Limited
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
#if !MBED_CONF_BLE_PRESENT
#error [NOT_SUPPORTED] BLE cordio test cases require the BLE library.
#else

#include <stdio.h>

#include "events/mbed_events.h"
#include "platform/Callback.h"

#include "ble/BLE.h"

#include "greentea-client/test_env.h"
#include "utest/utest.h"
#include "unity/unity.h"

#include "hci_api.h"
#include "hci_cmd.h"
#include "hci_core.h"
#include "dm_api.h"
#include "bstream.h"

using namespace utest::v1;
using mbed::callback;
using namespace std::chrono_literals;

#define INITIALIZATION_TIMEOUT 10000ms

static EventQueue event_queue(/* event count */ 10 * EVENTS_EVENT_SIZE);

enum initialization_state_t {
    WAITING_FOR_INITIALIZATION,
    INITIALIZATION_FAILURE,
    INITIALIZATION_SUCCESS
};

static initialization_state_t initialization_state = WAITING_FOR_INITIALIZATION;

static void process_ble_events(BLE::OnEventsToProcessCallbackContext* context)
{
    BLE &ble = BLE::Instance();
    event_queue.call(callback(&ble, &BLE::processEvents));
}

static void on_initialization_complete(BLE::InitializationCompleteCallbackContext *params)
{
    if (params->error == BLE_ERROR_NONE) {
        initialization_state = INITIALIZATION_SUCCESS;
    } else {
        initialization_state = INITIALIZATION_FAILURE;
    }

    event_queue.break_dispatch();
}

static void test_stack_initialization()
{
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(process_ble_events);
    ble.init(on_initialization_complete);
    event_queue.dispatch_for(INITIALIZATION_TIMEOUT);

    // At this point ble is suppose to be initialized; inspect the various state
    // of the stack.
    TEST_ASSERT_EQUAL(INITIALIZATION_SUCCESS, initialization_state);

    // ensure that the size of ACL buffer of the controller has been filled in
    // during the initialisation
    TEST_ASSERT_NOT_EQUAL(0, hciCoreCb.bufSize);

    // Ensure that the total number of buffer available in the controller has
    // been filled in during the initialisation
    TEST_ASSERT_NOT_EQUAL(0, hciCoreCb.numBufs);

    // Ensure that at least one HCI buffer is available
    TEST_ASSERT_NOT_EQUAL(0, hciCoreCb.availBufs);

    // Ensure that allowed LE state has been filled in during initialisation
    // Note: see BT command LE Read Supported States Command in BT specification
    uint8_t invalid_le_states[HCI_LE_STATES_LEN] = { 0 };
    TEST_ASSERT_NOT_EQUAL(0, memcmp(invalid_le_states, hciCoreCb.leStates, HCI_LE_STATES_LEN));

    // Ensure that the size of the whitelist of the controller has been filled
    // in during the initialisation
    TEST_ASSERT_NOT_EQUAL(0, hciCoreCb.whiteListSize);

    // Note: cannot test supported features are the list may be null
    // Note: cannot test resolving list size as this may be null
}

Case cases[] = {
    Case("Test cordio stack reset sequence", test_stack_initialization),
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(15, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main()
{
    return !Harness::run(specification);
}

#endif // !MBED_CONF_BLE_PRESENT
