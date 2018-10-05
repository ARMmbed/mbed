"""
mbed SDK
Copyright (c) 2011-2016 ARM Limited

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


import uuid
from mbed_host_tests import BaseHostTest

class Device_Echo(BaseHostTest):

    def _callback_repeat(self, key, value, _):
        self.send_kv(key, value)

    def setup(self):
        self.register_callback("echo", self._callback_repeat)
        self.register_callback("echo_count", self._callback_repeat)

    def teardown(self):
        pass
