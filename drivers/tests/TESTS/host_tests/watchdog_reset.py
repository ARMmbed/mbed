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
import collections
import threading
from htrun import BaseHostTest

TestCaseData = collections.namedtuple('TestCaseData', ['index', 'data_to_send'])

DEFAULT_SYNC_DELAY = 4.0
MAX_HB_PERIOD = 2.5  # [s] Max expected heartbeat period.

MSG_VALUE_DUMMY = '0'
CASE_DATA_INVALID = 0xffffffff
CASE_DATA_PHASE2_OK = 0xfffffffe
CASE_DATA_INSUFF_HB = 0x0

MSG_KEY_SYNC = '__sync'
MSG_KEY_DEVICE_READY = 'ready'
MSG_KEY_START_CASE = 'start_case'
MSG_KEY_DEVICE_RESET = 'dev_reset'
MSG_KEY_HEARTBEAT = 'hb'


class WatchdogReset(BaseHostTest):
    """Host side test that handles device reset.

    Given a device with a watchdog timer started.
    When the device notifies the host about an incoming reset.
    Then the host:
    * keeps track of the test case index of the current test suite,
    * performs a dev-host handshake.
    """

    def __init__(self):
        super(WatchdogReset, self).__init__()
        self.current_case = TestCaseData(0, CASE_DATA_INVALID)
        self.__handshake_timer = None
        self.sync_delay = DEFAULT_SYNC_DELAY
        self.drop_heartbeat_messages = True
        self.hb_timestamps_us = []

    def handshake_timer_start(self, seconds=1.0, pre_sync_fun=None):
        """Start a new handshake timer."""

        def timer_handler():
            """Perform a dev-host handshake by sending a sync message."""
            if pre_sync_fun is not None:
                pre_sync_fun()
            self.send_kv(MSG_KEY_SYNC, MSG_VALUE_DUMMY)

        self.__handshake_timer = threading.Timer(seconds, timer_handler)
        self.__handshake_timer.start()

    def handshake_timer_cancel(self):
        """Cancel the current handshake timer."""
        try:
            self.__handshake_timer.cancel()
        except AttributeError:
            pass
        finally:
            self.__handshake_timer = None

    def heartbeat_timeout_handler(self):
        """Handler for the heartbeat timeout.

        Compute the time span of the last heartbeat sequence.
        Set self.current_case.data_to_send to CASE_DATA_INVALID if no heartbeat was received.
        Set self.current_case.data_to_send to CASE_DATA_INSUFF_HB if only one heartbeat was
        received.
        """
        self.drop_heartbeat_messages = True
        dev_data = CASE_DATA_INVALID
        if len(self.hb_timestamps_us) == 1:
            dev_data = CASE_DATA_INSUFF_HB
            self.log('Not enough heartbeats received.')
        elif len(self.hb_timestamps_us) >= 2:
            dev_data = int(round(0.001 * (self.hb_timestamps_us[-1] - self.hb_timestamps_us[0])))
            self.log('Heartbeat time span was {} ms.'.format(dev_data))
        self.current_case = TestCaseData(self.current_case.index, dev_data)

    def setup(self):
        sync_delay = self.get_config_item('forced_reset_timeout')
        self.sync_delay = sync_delay if sync_delay is not None else DEFAULT_SYNC_DELAY
        self.register_callback(MSG_KEY_DEVICE_READY, self.cb_device_ready)
        self.register_callback(MSG_KEY_DEVICE_RESET, self.cb_device_reset)
        self.register_callback(MSG_KEY_HEARTBEAT, self.cb_heartbeat)

    def teardown(self):
        self.handshake_timer_cancel()

    def cb_device_ready(self, key, value, timestamp):
        """Advance the device test suite to a proper test case.

        Additionally, send test case data to the device.
        """
        self.handshake_timer_cancel()
        msg_value = '{0.index:02x},{0.data_to_send:08x}'.format(self.current_case)
        self.send_kv(MSG_KEY_START_CASE, msg_value)
        self.drop_heartbeat_messages = False
        self.hb_timestamps_us = []

    def cb_device_reset(self, key, value, timestamp):
        """Keep track of the test case number.

        Also set a new handshake timeout, so when the device gets
        restarted by the watchdog, the communication will be restored
        by the __handshake_timer.
        """
        self.handshake_timer_cancel()
        case_num, dev_reset_delay_ms = (int(i, base=16) for i in value.split(','))
        self.current_case = TestCaseData(case_num, CASE_DATA_PHASE2_OK)
        self.handshake_timer_start(self.sync_delay + dev_reset_delay_ms / 1000.0)

    def cb_heartbeat(self, key, value, timestamp):
        """Save the timestamp of a heartbeat message.

        Additionally, keep track of the test case number.

        Also each heartbeat sets a new timeout, so when the device gets
        restarted by the watchdog, the communication will be restored
        by the __handshake_timer.
        """
        if self.drop_heartbeat_messages:
            return
        self.handshake_timer_cancel()
        case_num, timestamp_us = (int(i, base=16) for i in value.split(','))
        self.current_case = TestCaseData(case_num, CASE_DATA_INVALID)
        self.hb_timestamps_us.append(timestamp_us)
        self.handshake_timer_start(
            seconds=(MAX_HB_PERIOD + self.sync_delay),
            pre_sync_fun=self.heartbeat_timeout_handler)
