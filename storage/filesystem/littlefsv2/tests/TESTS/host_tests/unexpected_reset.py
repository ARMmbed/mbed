"""
mbed SDK
Copyright (c) 2017-2017 ARM Limited

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


class UnexpectedResetTest(BaseHostTest):
    """This test checks that a device's RTC keeps count through a reset

    It does this by setting the RTC's time, triggering a reset,
    delaying and then reading the RTC's time again to ensure
    that the RTC is still counting.
    """

    """Number of times to reset the device in this test"""
    RESET_COUNT = 20
    RESET_DELAY_BASE = 1.0
    RESET_DELAY_INC = 0.02
    VALUE_PLACEHOLDER = "0"

    def setup(self):
        """Register callbacks required for the test"""
        self._error = False
        generator = self.unexpected_reset_test()
        generator.next()

        def run_gen(key, value, time):
            """Run the generator, and fail testing if the iterator stops"""
            if self._error:
                return
            try:
                generator.send((key, value, time))
            except StopIteration:
                self._error = True

        for resp in ("start", "read", "format_done", "reset_complete"):
            self.register_callback(resp, run_gen)

    def teardown(self):
        """No work to do here"""
        pass

    def unexpected_reset_test(self):
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

        # Format the device before starting the test
        self.send_kv("format", self.VALUE_PLACEHOLDER)
        key, value, time = yield
        if key != "format_done":
            return

        for i in range(self.RESET_COUNT):

            self.send_kv("run", self.VALUE_PLACEHOLDER)
            sleep(self.RESET_DELAY_BASE + self.RESET_DELAY_INC * i)

            self.reset()

            # Wait for start token
            key, value, time = yield
            self.log("Key from yield: %s" % key)
            if key != "reset_complete":
                return


            self.send_kv("__sync", "00000000-0000-000000000-000000000000")

            # Wait for start token
            key, value, time = yield
            if key != "start":
                return

        self.send_kv("exit", "pass")

        yield    # No more events expected
