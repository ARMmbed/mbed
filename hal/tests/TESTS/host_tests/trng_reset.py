"""
Copyright (c) 2018-2020, Arm Limited and affiliates
SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

"""
This script is the host script for trng test sequence, it send the 
step signaling sequence and receive and transmit data to the device after 
reset if necesarry (default loading and storing mechanism while reseting the device
is KVStore, in case KVStore isn't enabled we'll use current infrastructure,
for more details see main.cpp file)
"""

import time
from htrun import BaseHostTest
from htrun.host_tests_runner.host_test_default import DefaultTestSelector
from time import sleep

DEFAULT_CYCLE_PERIOD      = 1.0
MSG_VALUE_DUMMY           = '0'
MSG_TRNG_READY            = 'ready'
MSG_TRNG_BUFFER           = 'buffer'
MSG_TRNG_TEST_STEP1       = 'check_step1'
MSG_TRNG_TEST_STEP2       = 'check_step2'
MSG_KEY_SYNC              = '__sync'
MSG_KEY_RESET_COMPLETE    = 'reset_complete'
MSG_KEY_TEST_SUITE_ENDED  = 'Test_suite_ended'
MSG_KEY_EXIT              = 'exit'

class TRNGResetTest(BaseHostTest):
    """Test for the TRNG API.
    """

    def __init__(self):
        super(TRNGResetTest, self).__init__()
        self.did_reset = False
        self.ready = False
        self.suite_ended = False
        self.buffer = 0
        self.test_steps_sequence = self.test_steps()
        # Advance the coroutine to it's first yield statement.
        self.test_steps_sequence.send(None)

    #define callback functions for msg handling
    def setup(self):
        self.register_callback(MSG_TRNG_READY, self.cb_device_ready)
        self.register_callback(MSG_TRNG_BUFFER, self.cb_trng_buffer)
        self.register_callback(MSG_KEY_TEST_SUITE_ENDED, self.cb_device_test_suit_ended)
        self.register_callback(MSG_KEY_RESET_COMPLETE, self.cb_reset_complete)

    #receive sent data from device before reset
    def cb_trng_buffer(self, key, value, timestamp):
        """Acknowledge device rebooted correctly and feed the test execution
        """
        self.buffer = value

        try:
            if self.test_steps_sequence.send(value):
                self.notify_complete(True)
        except (StopIteration, RuntimeError) as exc:
            self.notify_complete(False)

    def cb_device_ready(self, key, value, timestamp):
        """Acknowledge device rebooted correctly and feed the test execution
        """
        self.ready = True

        try:
            if self.test_steps_sequence.send(value):
                self.notify_complete(True)
        except (StopIteration, RuntimeError) as exc:
            self.notify_complete(False)

    def cb_reset_complete(self, key, value, timestamp):
        """Acknowledge reset complete
        """
        self.did_reset = True

        try:
            if self.test_steps_sequence.send(value):
                self.notify_complete(True)
        except (StopIteration, RuntimeError) as exc:
            self.notify_complete(False)

    def cb_device_test_suit_ended(self, key, value, timestamp):
        """Acknowledge device finished a test step correctly and feed the test execution
        """
        self.suite_ended = True

        try:
            if self.test_steps_sequence.send(value):
                self.notify_complete(True)
        except (StopIteration, RuntimeError) as exc:
            self.notify_complete(False)

    #define test steps and actions
    def test_steps(self):
        """Test step 1
        """
        wait_for_communication = yield

        self.ready = False
        self.did_reset = False
        self.suite_ended = False
        self.send_kv(MSG_TRNG_TEST_STEP1, MSG_VALUE_DUMMY)
        wait_for_communication = yield
        if self.buffer == 0:
            raise RuntimeError('Phase 1: No buffer received.')

        self.reset()

        """Test step 2 (After reset)
        """
        wait_for_communication = yield
        if self.did_reset == False:
            raise RuntimeError('Phase 1: Platform did not reset as expected.')

        self.send_kv(MSG_KEY_SYNC, MSG_VALUE_DUMMY)

        wait_for_communication = yield

        if self.ready == False:
            raise RuntimeError('Phase 1: Platform not ready as expected.')

        self.send_kv(MSG_TRNG_TEST_STEP2, self.buffer)

        wait_for_communication = yield

        if self.suite_ended == False:
            raise RuntimeError('Test failed.')

        self.send_kv(MSG_KEY_EXIT, MSG_VALUE_DUMMY)

        # The sequence is correct -- test passed.
        yield
