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

MSG_VALUE_WATCHDOG_PRESENT = 1
MSG_VALUE_DUMMY = '0'
MSG_VALUE_RESET_REASON_GET = 'get'
MSG_VALUE_RESET_REASON_CLEAR = 'clear'
MSG_VALUE_DEVICE_RESET_NVIC = 'nvic'
MSG_VALUE_DEVICE_RESET_WATCHDOG = 'watchdog'

MSG_KEY_DEVICE_READY = 'ready'
MSG_KEY_RESET_REASON_RAW = 'reason_raw'
MSG_KEY_RESET_REASON = 'reason'
MSG_KEY_DEVICE_RESET = 'reset'
MSG_KEY_SYNC = '__sync'
MSG_KEY_RESET_COMPLETE = 'reset_complete'

RESET_REASONS = {
    'POWER_ON': '0',
    'PIN_RESET': '1',
    'BROWN_OUT': '2',
    'SOFTWARE': '3',
    'WATCHDOG': '4',
    'LOCKUP': '5',
    'WAKE_LOW_POWER': '6',
    'ACCESS_ERROR': '7',
    'BOOT_ERROR': '8',
    'MULTIPLE': '9',
    'PLATFORM': '10',
    'UNKNOWN': '11'
}


def raise_if_different(expected, actual, text=''):
    """Raise a RuntimeError if actual is different than expected."""
    if expected != actual:
        raise RuntimeError('{}Got {!r}, expected {!r}'
                           .format(text, actual, expected))


class ResetReasonTest(BaseHostTest):
    """Test for the Reset Reason HAL API.

    Given a device supporting a Reset Reason API.
    When the device is restarted using various methods.
    Then the device returns a correct reset reason for every restart.
    """

    def __init__(self):
        super(ResetReasonTest, self).__init__()
        self.device_reasons = None
        self.device_has_watchdog = None
        self.raw_reset_reasons = set()
        self.sync_delay = DEFAULT_SYNC_DELAY
        self.test_steps_sequence = self.test_steps()
        # Advance the coroutine to it's first yield statement.
        self.test_steps_sequence.send(None)

    def setup(self):
        sync_delay = self.get_config_item('forced_reset_timeout')
        self.sync_delay = sync_delay if sync_delay is not None else DEFAULT_SYNC_DELAY
        self.register_callback(MSG_KEY_DEVICE_READY, self.cb_device_ready)
        self.register_callback(MSG_KEY_RESET_REASON_RAW, self.cb_reset_reason_raw)
        self.register_callback(MSG_KEY_RESET_REASON, self.cb_reset_reason)
        self.register_callback(MSG_KEY_DEVICE_RESET, self.cb_reset_reason)
        self.register_callback(MSG_KEY_RESET_COMPLETE, self.cb_reset_reason)

    def cb_device_ready(self, key, value, timestamp):
        """Request a raw value of the reset_reason register.

        Additionally, save the device's reset_reason capabilities
        and the watchdog status on the first call.
        """
        if self.device_reasons is None:
            reasons, wdg_status = (int(i, base=16) for i in value.split(','))
            self.device_has_watchdog = (wdg_status == MSG_VALUE_WATCHDOG_PRESENT)
            self.device_reasons = [k for k, v in RESET_REASONS.items() if (reasons & 1 << int(v))]
        self.send_kv(MSG_KEY_RESET_REASON_RAW, MSG_VALUE_RESET_REASON_GET)

    def cb_reset_reason_raw(self, key, value, timestamp):
        """Verify that the raw reset_reason register value is unique.

        Fail the test suite if the raw reset_reason value is not unique.
        Request a platform independent reset_reason otherwise.
        """
        if value in self.raw_reset_reasons:
            self.log('TEST FAILED: The raw reset reason is not unique. '
                     '{!r} is already present in {!r}.'
                     .format(value, self.raw_reset_reasons))
            self.notify_complete(False)
        else:
            self.raw_reset_reasons.add(value)
        self.send_kv(MSG_KEY_RESET_REASON, MSG_VALUE_RESET_REASON_GET)

    def cb_reset_reason(self, key, value, timestamp):
        """Feed the test_steps coroutine with reset_reason value.

        Pass the test suite if the coroutine yields True.
        Fail the test suite if the iterator stops or raises a RuntimeError.
        """
        try:
            if self.test_steps_sequence.send(value):
                self.notify_complete(True)
        except (StopIteration, RuntimeError) as exc:
            self.log('TEST FAILED: {}'.format(exc))
            self.notify_complete(False)

    def test_steps(self):
        """Generate a sequence of test steps.

        This coroutine calls yield to wait for the input from the device
        (the reset_reason). If the device gives the wrong response, the
        generator raises a RuntimeError exception and fails the test.
        """
        # Ignore the first reason.
        __ignored_reset_reason = yield
        self.raw_reset_reasons.clear()
        self.send_kv(MSG_KEY_RESET_REASON, MSG_VALUE_RESET_REASON_CLEAR)
        __ignored_clear_ack = yield

        # Request a NVIC_SystemReset() call.
        expected_reason = 'SOFTWARE'
        if expected_reason not in self.device_reasons:
            self.log('Skipping the {} reset reason -- not supported.'.format(expected_reason))
        else:
            # Request a NVIC_SystemReset() call.
            self.send_kv(MSG_KEY_DEVICE_RESET, MSG_VALUE_DEVICE_RESET_NVIC)
            __ignored_reset_ack = yield
            time.sleep(self.sync_delay)
            self.send_kv(MSG_KEY_SYNC, MSG_VALUE_DUMMY)
            reset_reason = yield
            raise_if_different(RESET_REASONS[expected_reason], reset_reason, 'Wrong reset reason. ')
            self.send_kv(MSG_KEY_RESET_REASON, MSG_VALUE_RESET_REASON_CLEAR)
            __ignored_clear_ack = yield

        # Reset the device using DAP.
        expected_reason = 'PIN_RESET'
        if expected_reason not in self.device_reasons:
            self.log('Skipping the {} reset reason -- not supported.'.format(expected_reason))
        else:
            self.reset()
            __ignored_reset_ack = yield  # 'reset_complete'
            time.sleep(self.sync_delay)
            self.send_kv(MSG_KEY_SYNC, MSG_VALUE_DUMMY)
            reset_reason = yield
            raise_if_different(RESET_REASONS[expected_reason], reset_reason, 'Wrong reset reason. ')
            self.send_kv(MSG_KEY_RESET_REASON, MSG_VALUE_RESET_REASON_CLEAR)
            __ignored_clear_ack = yield

        # Start a watchdog timer and wait for it to reset the device.
        expected_reason = 'WATCHDOG'
        if expected_reason not in self.device_reasons or not self.device_has_watchdog:
            self.log('Skipping the {} reset reason -- not supported.'.format(expected_reason))
        else:
            self.send_kv(MSG_KEY_DEVICE_RESET, MSG_VALUE_DEVICE_RESET_WATCHDOG)
            __ignored_reset_ack = yield
            time.sleep(self.sync_delay)
            self.send_kv(MSG_KEY_SYNC, MSG_VALUE_DUMMY)
            reset_reason = yield
            raise_if_different(RESET_REASONS[expected_reason], reset_reason, 'Wrong reset reason. ')

        # The sequence is correct -- test passed.
        yield True
