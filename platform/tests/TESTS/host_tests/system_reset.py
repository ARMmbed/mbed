"""
Copyright (c) 2018 ARM Limited
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
import time
from htrun import BaseHostTest
from htrun.host_tests_runner.host_test_default import DefaultTestSelector

DEFAULT_CYCLE_PERIOD = 1.0

MSG_VALUE_DUMMY = '0'

MSG_KEY_DEVICE_READY = 'ready'
MSG_KEY_DEVICE_RESET = 'reset'
MSG_KEY_SYNC = '__sync'

class SystemResetTest(BaseHostTest):
    """Test for the system_reset API.

    Given a device running code
    When the device is restarted using @a system_reset()
    Then the device is restarted
    """

    def __init__(self):
        super(SystemResetTest, self).__init__()
        self.reset = False
        self.test_steps_sequence = self.test_steps()
        # Advance the coroutine to it's first yield statement.
        self.test_steps_sequence.send(None)

    def setup(self):
        self.register_callback(MSG_KEY_DEVICE_READY, self.cb_device_ready)

    def cb_device_ready(self, key, value, timestamp):
        """Acknowledge device rebooted correctly and feed the test execution
        """
        self.reset = True

        try:
            if self.test_steps_sequence.send(value):
                self.notify_complete(True)
        except (StopIteration, RuntimeError) as exc:
            self.notify_complete(False)

    def test_steps(self):
        """Reset the device and check the status
        """
        system_reset = yield
        self.reset = False

        wait_after_reset = self.get_config_item('forced_reset_timeout')
        wait_after_reset = wait_after_reset if wait_after_reset is not None else DEFAULT_CYCLE_PERIOD

        self.send_kv(MSG_KEY_DEVICE_RESET, MSG_VALUE_DUMMY)
        time.sleep(wait_after_reset)
        self.send_kv(MSG_KEY_SYNC, MSG_VALUE_DUMMY)

        system_reset = yield
        if self.reset == False:
            raise RuntimeError('Platform did not reset as expected.')

        # The sequence is correct -- test passed.
        yield True
