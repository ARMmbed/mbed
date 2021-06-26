/*
 * Copyright (c) 2017, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !MBED_CONF_NFCEEPROM
#error [NOT_SUPPORTED] NFC EEPROM not supported for this target
#else

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"

#include <events/mbed_events.h>
#include "NFCEEPROMDriver.h"

using namespace utest::v1;
using namespace mbed::nfc;

/* individual steps that map to specific operations that include parameters */
typedef enum {
    START_SESSION        = 0x0000,

    END_SESSION          = 0x0100,

    READ_BYTES           = 0x0200,
    READ_2_BYTES         = 0x0201,
    READ_2_BYTES_OFFSET_FAIL = 0x0202,
    READ_4_BYTES         = 0x0203,
    READ_4_BYTES_MIXED   = 0x0204,
    READ_4_BYTES_OFFSET  = 0x0205,
    READ_4_BYTES_ERASED  = 0x0206,
    READ_4_BYTES_FAIL    = 0x0207,

    WRITE_BYTES          = 0x0300,
    WRITE_2_BYTES        = 0x0301,
    WRITE_2_BYTES_OFFSET = 0x0302,
    WRITE_2_BYTES_OFFSET_FAIL = 0x0303,
    WRITE_4_BYTES        = 0x0304,
    WRITE_4_BYTES_FAIL   = 0x0305,

    ERASE_BYTES          = 0x0400,
    ERASE_4_BYTES        = 0x0401,

    READ_SIZE            = 0x0500,
    READ_SIZE_2          = 0x0501,
    READ_SIZE_4          = 0x0502,

    WRITE_SIZE           = 0x0600,
    WRITE_SIZE_2         = 0x0601,
    WRITE_SIZE_4         = 0x0602,

    TERMINATE            = 0xFF00
} TestCommand_t;

/* We group the command based on their first byte to simplify step checking.
 * Individual conditions of a step are checked in the event so this doesn't
 * sacrifice any correctness checking */
const size_t TEST_COMMAND_GROUP_MASK = 0xFF00;

/* test case sequences that index into the array of commands which comprise them */
typedef enum {
    SESSION_TEST,
    WRITE_READ_TEST,
    ERASE_TEST,
    WRITE_READ_SIZE_TEST,
    TRUNCATE_TEST,
    WRITE_TRUNCATE_TEST,
    WRITE_WITH_OFFSET_TEST,
    WRITE_BEYOND_SIZE_TEST,
    SEQUENCE_MAX
} TestSequence_t;

static EventQueue event_queue(/* event count */ 10 * EVENTS_EVENT_SIZE);

static const size_t MAX_STEP = 10;
static const size_t BUFFER_MAX = 10;

/* defines the "script" the test follows, each step is checked and all
 * have to execute to completion for a successful run */
static const TestCommand_t SEQUENCES[SEQUENCE_MAX][MAX_STEP] = {
    /* SESSION_TEST */
    { START_SESSION, END_SESSION, TERMINATE },

    /* WRITE_READ_TEST */
    {
        START_SESSION, WRITE_SIZE_4, READ_SIZE_4, WRITE_4_BYTES, READ_4_BYTES,
        WRITE_2_BYTES, READ_4_BYTES_MIXED, END_SESSION, TERMINATE
    },

    /* ERASE_TEST */
    {
        START_SESSION, WRITE_SIZE_4, READ_SIZE_4, WRITE_4_BYTES, READ_4_BYTES,
        ERASE_4_BYTES, READ_4_BYTES_ERASED, END_SESSION, TERMINATE
    },

    /* WRITE_READ_SIZE_TEST */
    {
        START_SESSION, WRITE_SIZE_2, READ_SIZE_2, WRITE_SIZE_4, READ_SIZE_4,
        END_SESSION, TERMINATE
    },

    /* TRUNCATE_TEST */
    {
        START_SESSION, WRITE_SIZE_4, READ_SIZE_4, WRITE_4_BYTES, READ_4_BYTES,
        WRITE_SIZE_2, READ_SIZE_2, READ_4_BYTES_FAIL, END_SESSION, TERMINATE
    },

    /* WRITE_TRUNCATE_TEST */
    {
        START_SESSION, WRITE_SIZE_2, READ_SIZE_2, WRITE_4_BYTES_FAIL, READ_4_BYTES_FAIL,
        END_SESSION, TERMINATE
    },

    /* WRITE_WITH_OFFSET_TEST */
    {
        START_SESSION, WRITE_SIZE_4, READ_SIZE_4, WRITE_4_BYTES, READ_4_BYTES,
        WRITE_2_BYTES_OFFSET, READ_4_BYTES_OFFSET, END_SESSION, TERMINATE
    },

    /* WRITE_BEYOND_SIZE_TEST */
    {
        START_SESSION, WRITE_SIZE_2, READ_SIZE_2, WRITE_2_BYTES_OFFSET_FAIL, READ_2_BYTES_OFFSET_FAIL,
        WRITE_2_BYTES, READ_2_BYTES, END_SESSION, TERMINATE
    }
};

static const uint8_t DATA_4_BYTES[] = { 0x01, 0x02, 0x03, 0x04 };
static const uint8_t DATA_2_BYTES[] = { 0x05, 0x06 };
static const uint8_t DATA_4_BYTES_MIXED[] = { 0x05, 0x06, 0x03, 0x04 };
static const uint8_t DATA_4_BYTES_OFFSET[] = { 0x01, 0x02, 0x05, 0x06 };
static const uint8_t DATA_4_BYTES_ERASED[] = { 0x00, 0x00, 0x00, 0x00 };

class DriverTest : public NFCEEPROMDriver::Delegate {
public:
    DriverTest(NFCEEPROMDriver *_driver)
        : _operation_data(NULL),
          _driver(_driver),
          _sequence(SEQUENCE_MAX),
          _step(0),
          _result_size(0),
          _address(0),
          _success(true) { };

    virtual ~DriverTest() { };

    /* Delegate events */

    virtual void on_session_started(bool success)
    {
        if (success != _success) {
            TEST_FAIL_MESSAGE("failed to start session");
        }

        evaluate_step(START_SESSION);
    };

    virtual void on_session_ended(bool success)
    {
        if (success != _success) {
            TEST_FAIL_MESSAGE("failed to end session");
        }

        evaluate_step(END_SESSION);
    };

    virtual void on_bytes_read(size_t count)
    {
        if (count != _result_size) {
            TEST_FAIL_MESSAGE("failed to read bytes");
        }
        if (memcmp(_buffer, _operation_data, count) != 0) {
            TEST_FAIL_MESSAGE("read bytes are different than expected");
        }

        evaluate_step(READ_BYTES);
    };

    virtual void on_bytes_written(size_t count)
    {
        if (count != _result_size) {
            TEST_FAIL_MESSAGE("failed to write bytes");
        }

        evaluate_step(WRITE_BYTES);
    };

    virtual void on_size_written(bool success)
    {
        if (success != _success) {
            TEST_FAIL_MESSAGE("failed to write size");
        }

        evaluate_step(WRITE_SIZE);
    };

    virtual void on_size_read(bool success, size_t size)
    {
        if (success != _success || size != _result_size) {
            TEST_FAIL_MESSAGE("failed to read size");
        }

        evaluate_step(READ_SIZE);
    };

    virtual void on_bytes_erased(size_t count)
    {
        if (count != _result_size) {
            TEST_FAIL_MESSAGE("failed to erase bytes");
        }

        evaluate_step(ERASE_BYTES);
    }

    /* Sequence running code */

    void run_sequence(TestSequence_t sequence)
    {
        _sequence = sequence;

        if (_sequence >= SEQUENCE_MAX) {
            TEST_FAIL_MESSAGE("internal test failure - invalid command");
        }

        execute_next_step();

        event_queue.dispatch_forever();

        TEST_ASSERT_EQUAL(TERMINATE, SEQUENCES[_sequence][_step]);
    }

    void execute_next_step()
    {
        TestCommand_t command = SEQUENCES[_sequence][_step];

        /* setup data buffer */
        switch (command) {
            case READ_2_BYTES:
            case READ_2_BYTES_OFFSET_FAIL:
            case WRITE_2_BYTES:
            case WRITE_2_BYTES_OFFSET:
            case WRITE_2_BYTES_OFFSET_FAIL:
                _operation_data = DATA_2_BYTES;
                break;
            case READ_4_BYTES:
            case READ_4_BYTES_FAIL:
            case WRITE_4_BYTES:
            case WRITE_4_BYTES_FAIL:
                _operation_data = DATA_4_BYTES;
                break;
            case READ_4_BYTES_ERASED:
                _operation_data = DATA_4_BYTES_ERASED;
                break;
            case READ_4_BYTES_MIXED:
                _operation_data = DATA_4_BYTES_MIXED;
                break;
            case READ_4_BYTES_OFFSET:
                _operation_data = DATA_4_BYTES_OFFSET;
                break;
            default:
                _operation_data = NULL;
        }

        /* setup result size */
        switch (command) {
            case READ_2_BYTES:
            case READ_4_BYTES_FAIL:
            case WRITE_2_BYTES:
            case WRITE_4_BYTES_FAIL:
            case WRITE_2_BYTES_OFFSET:
            case READ_SIZE_2:
                _result_size = 2;
                break;
            case READ_4_BYTES:
            case READ_4_BYTES_ERASED:
            case READ_4_BYTES_MIXED:
            case READ_4_BYTES_OFFSET:
            case WRITE_4_BYTES:
            case ERASE_4_BYTES:
            case READ_SIZE_4:
                _result_size = 4;
                break;
            default:
                _result_size = 0;
        }

        /* setup operation size */
        switch (command) {
            case READ_2_BYTES:
            case READ_2_BYTES_OFFSET_FAIL:
            case WRITE_2_BYTES:
            case WRITE_2_BYTES_OFFSET:
            case WRITE_2_BYTES_OFFSET_FAIL:
            case WRITE_SIZE_2:
                _operation_size = 2;
                break;
            case READ_4_BYTES:
            case READ_4_BYTES_ERASED:
            case READ_4_BYTES_MIXED:
            case READ_4_BYTES_OFFSET:
            case READ_4_BYTES_FAIL:
            case WRITE_4_BYTES:
            case WRITE_4_BYTES_FAIL:
            case ERASE_4_BYTES:
            case READ_SIZE_4:
            case WRITE_SIZE_4:
                _operation_size = 4;
                break;
            default:
                _operation_size = 0;
        }

        /* setup offset */
        switch (command) {
            case READ_2_BYTES_OFFSET_FAIL:
            case WRITE_2_BYTES_OFFSET:
            case WRITE_2_BYTES_OFFSET_FAIL:
                _address = 2;
                break;
            default:
                _address = 0;
        }

        /* setup command success */
        switch (command) {
            case READ_2_BYTES_OFFSET_FAIL:
            case WRITE_2_BYTES_OFFSET_FAIL:
                _success = false;
                break;
            default:
                _success = true;
        }

        /* call next command */
        switch (command & TEST_COMMAND_GROUP_MASK) {
            case START_SESSION:
                _driver->start_session(true);
                break;
            case END_SESSION:
                _driver->end_session();
                break;
            case READ_BYTES:
                _driver->read_bytes(_address, _buffer, _operation_size);
                break;
            case WRITE_BYTES:
                _driver->write_bytes(_address, _operation_data, _operation_size);
                break;
            case ERASE_BYTES:
                _driver->erase_bytes(_address, _operation_size);
                break;
            case READ_SIZE:
                _driver->read_size();
                break;
            case WRITE_SIZE:
                _driver->write_size(_operation_size);
                break;
            case TERMINATE:
                event_queue.break_dispatch();
                break;
            default:
                TEST_FAIL_MESSAGE("internal test failure - invalid command");
        }
    }

    void evaluate_step(TestCommand_t command_completed)
    {
        /* check last command succeeded */
        TEST_ASSERT_EQUAL(command_completed, SEQUENCES[_sequence][_step]&TEST_COMMAND_GROUP_MASK);

        _step++;

        if (_step == MAX_STEP) {
            TEST_FAIL_MESSAGE("internal test failure - too many steps");
        }

        execute_next_step();
    }

private:
    uint8_t _buffer[BUFFER_MAX];
    const uint8_t *_operation_data;

    NFCEEPROMDriver *_driver;

    TestSequence_t _sequence;
    size_t _step;
    size_t _result_size;
    size_t _operation_size;
    size_t _address;
    bool _success;
};

/* test case running code */

static DriverTest *driver_test;
extern NFCEEPROMDriver *greentea_nfc_EEPROM_driver_get_instance();

utest::v1::status_t test_setup(const Case *const source, const size_t index_of_case)
{
    NFCEEPROMDriver *driver = greentea_nfc_EEPROM_driver_get_instance();
    driver_test = new DriverTest(driver);

    driver->set_event_queue(&event_queue);
    driver->set_delegate(driver_test);
    driver->reset();

    TEST_ASSERT_NOT_EQUAL(0, driver->read_max_size());

    return greentea_case_setup_handler(source, index_of_case);
}

void session()
{
    driver_test->run_sequence(SESSION_TEST);
}

void write_read()
{
    driver_test->run_sequence(WRITE_READ_TEST);
}

void erase_bytes()
{
    driver_test->run_sequence(ERASE_TEST);
}

void write_read_size()
{
    driver_test->run_sequence(WRITE_READ_SIZE_TEST);
}

void truncate_size()
{
    driver_test->run_sequence(TRUNCATE_TEST);
}

void write_bytes_truncated()
{
    driver_test->run_sequence(WRITE_TRUNCATE_TEST);
}

void write_with_offset()
{
    driver_test->run_sequence(WRITE_WITH_OFFSET_TEST);
}

void write_beyond_size()
{
    driver_test->run_sequence(WRITE_BEYOND_SIZE_TEST);
}

utest::v1::status_t test_tear_down(const Case *const source, const size_t passed,
                                   const size_t failed, const failure_t reason)
{
    delete driver_test;

    return greentea_case_teardown_handler(source, passed, failed, reason);
}

/* test setup */

utest::v1::status_t test_init(const size_t number_of_cases)
{
    GREENTEA_SETUP(10, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("NFCEEPROM-SESSION", test_setup, session, test_tear_down),
    Case("NFCEEPROM-WRITE-READ", test_setup, write_read, test_tear_down),
    Case("NFCEEPROM-ERASE-BYTES", test_setup, erase_bytes, test_tear_down),
    Case("NFCEEPROM-WRITE-READ-SIZE", test_setup, write_read_size, test_tear_down),
    Case("NFCEEPROM-TRUNCATE-SIZE", test_setup, truncate_size, test_tear_down),
    Case("NFCEEPROM-WRITE-BYTES-TRUNCATED", test_setup, write_bytes_truncated, test_tear_down),
    Case("NFCEEPROM-WRITE-WITH-OFFSET", test_setup, write_with_offset, test_tear_down),
    Case("NFCEEPROM-WRITE-BEYOND-SIZE", test_setup, write_beyond_size, test_tear_down)
};

Specification specification(test_init, cases);

// Entry point into the tests
int main()
{
    return !Harness::run(specification);
}

#endif // !MBED_CONF_NFCEEPROM
