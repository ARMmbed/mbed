/*
 * Copyright (c) 2019 Arm Limited and affiliates.
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
#if !DEVICE_SERIAL
#error [NOT_SUPPORTED] serial communication not supported for this target
#else

#include "mbed.h"
#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"
#include "platform/FileHandle.h"
#include "drivers/BufferedSerial.h"


using namespace utest::v1;


/**
 * Macros for setting console flow control.
 */
#define CONSOLE_FLOWCONTROL_RTS                                               1
#define CONSOLE_FLOWCONTROL_CTS                                               2
#define CONSOLE_FLOWCONTROL_RTSCTS                                            3
#define mbed_console_concat_(x) CONSOLE_FLOWCONTROL_##x
#define mbed_console_concat(x) mbed_console_concat_(x)
#define CONSOLE_FLOWCONTROL mbed_console_concat(MBED_CONF_TARGET_CONSOLE_UART_FLOW_CONTROL)


#define MSG_KEY_SYNC_MESSAGE                                     "sync_message"
#define MSG_VALUE_HELLO_WORLD                                   "Hello, world!"


#define EXPECTED_SYNCED_STRING "{{" MSG_KEY_SYNC_MESSAGE ";" MSG_VALUE_HELLO_WORLD "}}"
// The target is expected to transmit Greentea messages with \n (or \r\n) or they are not detected by the host
#define SYNC_STRING_TO_SEND EXPECTED_SYNCED_STRING "\n"


static BufferedSerial buffered_serial_obj(
    CONSOLE_TX, CONSOLE_RX, MBED_CONF_PLATFORM_STDIO_BAUD_RATE
);


FileHandle *mbed::mbed_override_console(int fd)
{
    return &buffered_serial_obj;
}


static void test_serial_sync()
{
    char tx_msg[] = SYNC_STRING_TO_SEND;

    TEST_ASSERT_EQUAL_UINT(
        strlen(tx_msg) + 1,
        buffered_serial_obj.write(tx_msg, strlen(tx_msg) + 1)
    );

    // Wait the message is completely sent to the bus
    buffered_serial_obj.sync();
    system_reset();
    TEST_ASSERT_MESSAGE(0, "The device did not reset as expected.");
}


int main()
{
#if   CONSOLE_FLOWCONTROL == CONSOLE_FLOWCONTROL_RTS
    buffered_serial_obj.set_flow_control(
        SerialBase::RTS, STDIO_UART_RTS, NC
    );
#elif CONSOLE_FLOWCONTROL == CONSOLE_FLOWCONTROL_CTS
    buffered_serial_obj.set_flow_control(
        SerialBase::CTS, NC, STDIO_UART_CTS
    );
#elif CONSOLE_FLOWCONTROL == CONSOLE_FLOWCONTROL_RTSCTS
    buffered_serial_obj.set_flow_control(
        SerialBase::RTSCTS, STDIO_UART_RTS, STDIO_UART_CTS
    );
#endif
    GREENTEA_SETUP(12, "serial_comms");
    test_serial_sync(); // The result of this test suite is reported by the host side.
    GREENTEA_TESTSUITE_RESULT(0); // Fail on any error.
}

#endif // !DEVICE_SERIAL
