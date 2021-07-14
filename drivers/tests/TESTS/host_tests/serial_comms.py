"""
Copyright (c) 2019 Arm Limited and affiliates.

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

from htrun import BaseHostTest


MSG_KEY_ECHO_MESSAGE = "echo_message"


class SerialComms(BaseHostTest):
    """Host side test that handles messages sent using serial classes."""

    def __init__(self):
        """Initialize an object."""
        super(SerialComms, self).__init__()

    def setup(self):
        """Register call backs to handle message from the target."""
        self.register_callback(MSG_KEY_ECHO_MESSAGE, self.cb_echo_message)

    def cb_echo_message(self, key, value, timestamp):
        """Send back the key and value received."""
        self.send_kv(key, value)
