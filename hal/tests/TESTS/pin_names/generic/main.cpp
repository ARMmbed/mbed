/* mbed Microcontroller Library
 * Copyright (c) 2020 ARM Limited
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

#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"
#include "mbed.h"

/*
Requirements specified in docs/design-documents/hal/0004-pin-names-general-guidelines.md
*/

#ifndef LED1
#error [NOT_SUPPORTED] Target is not following mbed-os pin names standard // Test is set as Skipped
#else

using namespace utest::v1;

template <int LedId, PinName LedPin>
void LED_test()
{
    utest_printf("LED %u Pin 0x%x\n", LedId, LedPin);
    DigitalOut TEST(LedPin);
    TEST = 1;
    ThisThread::sleep_for(1s);
    TEST = 0;
}


template <int ButtonId, PinName ButtonPin>
void BUTTON_test()
{
    utest_printf("BUTTON %u Pin 0x%x\n", ButtonId, ButtonPin);
    DigitalIn TEST(ButtonPin);
}


Case cases[] = {
#ifdef LED1
    Case("LED1", LED_test<1, LED1>),
#endif
#ifdef LED2
    Case("LED2", LED_test<2, LED2>),
#endif
#ifdef LED3
    Case("LED3", LED_test<3, LED3>),
#endif
#ifdef LED4
    Case("LED4", LED_test<4, LED4>),
#endif
#ifdef LED5
    Case("LED5", LED_test<5, LED5>),
#endif
#ifdef LED6
    Case("LED6", LED_test<6, LED6>),
#endif
#ifdef LED7
    Case("LED7", LED_test<7, LED7>),
#endif
#ifdef LED8
    Case("LED8", LED_test<8, LED8>),
#endif

#ifdef BUTTON1
    Case("BUTTON1", BUTTON_test<1, BUTTON1>),
#endif
#ifdef BUTTON2
    Case("BUTTON2", BUTTON_test<2, BUTTON2>),
#endif
#ifdef BUTTON3
    Case("BUTTON3", BUTTON_test<3, BUTTON3>),
#endif
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(20, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main()
{
    Harness::run(specification);
}

#endif
