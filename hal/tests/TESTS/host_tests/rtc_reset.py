"""
mbed SDK
Copyright (c) 2017-2017 ARM Limited
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
from __future__ import print_function

from htrun import BaseHostTest
from time import sleep


class RtcResetTest(BaseHostTest):
    """This test checks that a device's RTC keeps count through a reset
    
    It does this by setting the RTC's time, triggering a reset,
    delaying and then reading the RTC's time again to ensure
    that the RTC is still counting.
    """

    """Start of the RTC"""
    START_TIME = 1537789823 # GMT: Monday, 24 September 2018 11:50:23
    START_TIME_TOLERANCE = 10
    """Time to delay after sending reset"""
    DELAY_TIME = 5.0
    DELAY_TOLERANCE = 1.0
    VALUE_PLACEHOLDER = "0"

    def setup(self):
        """Register callbacks required for the test"""
        self._error = False
        generator = self.rtc_reset_test()
        next(generator)

        def run_gen(key, value, time):
            """Run the generator, and fail testing if the iterator stops"""
            if self._error:
                return
            try:
                generator.send((key, value, time))
            except StopIteration:
                self._error = True

        for resp in ("start", "read", "ack"):
            self.register_callback(resp, run_gen)

    def teardown(self):
        """No work to do here"""
        pass

    def rtc_reset_test(self):
        """Generator for running the reset test
        
        This function calls yield to wait for the next event from
        the device. If the device gives the wrong response, then the
        generator terminates by returing which raises a StopIteration
        exception and fails the test.
        """

        # Wait for start token
        key, value, time = yield
        if key != "start":
            return

        # Initialize, and set the time
        self.send_kv("init", self.VALUE_PLACEHOLDER)
        
        # Wait for ack from the device
        key, value, time = yield
        if key != "ack":
            return
        
        self.send_kv("write", str(self.START_TIME))
        
        # Wait for ack from the device
        key, value, time = yield
        if key != "ack":
            return
        
        self.send_kv("read", self.VALUE_PLACEHOLDER)
        key, value, time = yield
        if key != "read":
            return
        dev_time_start = int(value)

        # Unitialize, and reset
        self.send_kv("free", self.VALUE_PLACEHOLDER)
        
        # Wait for ack from the device
        key, value, time = yield
        if key != "ack":
            return
        
        self.send_kv("reset", self.VALUE_PLACEHOLDER)
        
        # No ack after reset
        sleep(self.DELAY_TIME)

        # Restart the test, and send the sync token
        self.send_kv("__sync", "00000000-0000-000000000-000000000000")
        key, value, time = yield
        if key != "start":
            return

        # Initialize, and read the time
        self.send_kv("init", self.VALUE_PLACEHOLDER)
        
        # Wait for ack from the device
        key, value, time = yield
        if key != "ack":
            return
        
        self.send_kv("read", self.VALUE_PLACEHOLDER)
        key, value, time = yield
        if key != "read":
            return
        dev_time_end = int(value)

        # Check result
        elapsed = dev_time_end - dev_time_start
        start_time_valid = (self.START_TIME <= dev_time_start <
                            self.START_TIME + self.START_TIME_TOLERANCE)
        elapsed_time_valid = elapsed >= self.DELAY_TIME - self.DELAY_TOLERANCE
        passed = start_time_valid and elapsed_time_valid
        if not start_time_valid:
            self.log("FAIL: Expected start time of %i got %i" %
                     (self.START_TIME, dev_time_start))
        elif not passed:
            self.log("FAIL: Delayed for %fs but device "
                     "reported elapsed time of %fs" %
                     (self.DELAY_TIME, elapsed))
        self.send_kv("exit", "pass" if passed else "fail")
        yield    # No more events expected

