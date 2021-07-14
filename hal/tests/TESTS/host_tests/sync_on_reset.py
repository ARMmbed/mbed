"""
Copyright (c) 2018-2019 Arm Limited and affiliates.

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

DEFAULT_SYNC_DELAY = 4.0

MSG_VALUE_DUMMY = '0'
MSG_KEY_DEVICE_READY = 'ready'
MSG_KEY_START_CASE = 'start_case'
MSG_KEY_DEVICE_RESET = 'reset_on_case_teardown'
MSG_KEY_SYNC = '__sync'


class SyncOnReset(BaseHostTest):
    """Host side test that handles device reset during case teardown.

    Given a device that performs a reset during a test case teardown.
    When the device notifies the host about the reset.
    Then the host:
    * keeps track of the test case index of the current test suite,
    * performs a dev-host handshake,
    * advances the test suite to next test case.

    Note:
    Developed for a watchdog test, so that it can be run on devices that
    do not support watchdog timeout updates after the initial setup.
    As a solution, after testing watchdog with one set of settings, the
    device performs a reset and notifies the host with the test case number,
    so that the test suite may be advanced once the device boots again.
    """

    def __init__(self):
        super(SyncOnReset, self).__init__()
        self.test_case_num = 0
        self.sync_delay = DEFAULT_SYNC_DELAY

    def setup(self):
        sync_delay = self.get_config_item('forced_reset_timeout')
        self.sync_delay = sync_delay if sync_delay is not None else DEFAULT_SYNC_DELAY
        self.register_callback(MSG_KEY_DEVICE_READY, self.cb_device_ready)
        self.register_callback(MSG_KEY_DEVICE_RESET, self.cb_device_reset)

    def cb_device_ready(self, key, value, timestamp):
        """Advance the device test suite to the next test case."""
        self.send_kv(MSG_KEY_START_CASE, self.test_case_num)

    def cb_device_reset(self, key, value, timestamp):
        """Wait for the device to boot and perform a handshake.

        Additionally, keep track of the last test case number.
        """
        try:
            self.test_case_num = int(value)
        except ValueError:
            pass
        self.test_case_num += 1
        time.sleep(self.sync_delay)
        self.send_kv(MSG_KEY_SYNC, MSG_VALUE_DUMMY)
