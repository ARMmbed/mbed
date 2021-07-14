"""
mbed SDK
Copyright (c) 2011-2013 ARM Limited
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
import time


class TimingDriftSync(BaseHostTest):
    """
    This works as master-slave fashion
    1) Device says its booted up and ready to run the test, wait for host to respond
    2) Host sends the message to get the device current time i.e base time

    #
    # *
    #   *                   |
    #<---* DUT<- base_time  | - round_trip_base_time ------
    #   *                   |                              |
    # *                    -                               |
    #                      -                               |
    #                       |                              |
    #                       |                              |
    #                       | - measurement_stretch        | - nominal_time
    #                       |                              |
    #                       |                              |
    #                      -                               |
    # *                    -                               |
    #   *                   |                              |
    #<---* DUT <-final_time | - round_trip_final_time------
    #   *                   |
    # *                    -
    #
    #
    # As we increase the measurement_stretch, the error because of transport delay diminishes.
    # The values of measurement_stretch is propotional to round_trip_base_time(transport delays)
    # by factor time_measurement_multiplier.This multiplier is used is 80 to tolerate 2 sec of
    # transport delay and test time ~ 180 secs
    #
    # Failure in timing can occur if we are ticking too fast or we are ticking too slow, hence we have
    # min_range and max_range. if we cross on either side tests would be marked fail. The range is a function of
    # tolerance/acceptable drift currently its 5%.
    #

    """
    __result = None
    mega = 1000000.0
    max_measurement_time = 180

    # this value is obtained for measurements when there is 0 transport delay and we want accurancy of 5%
    time_measurement_multiplier = 80

    def _callback_timing_drift_check_start(self, key, value, timestamp):
        self.round_trip_base_start = timestamp
        self.send_kv("base_time", 0)

    def _callback_base_time(self, key, value, timestamp):
        self.round_trip_base_end = timestamp
        self.device_time_base = float(value)
        self.round_trip_base_time = self.round_trip_base_end - self.round_trip_base_start

        self.log("Device base time {}".format(value))
        measurement_stretch = (self.round_trip_base_time * self.time_measurement_multiplier) + 5

        if measurement_stretch > self.max_measurement_time:
            self.log("Time required {} to determine device timer is too high due to transport delay, skipping".format(measurement_stretch))
        else:
            self.log("sleeping for {} to measure drift accurately".format(measurement_stretch))
            time.sleep(measurement_stretch)
        self.round_trip_final_start = time.time()
        self.send_kv("final_time", 0)

    def _callback_final_time(self, key, value, timestamp):
        self.round_trip_final_end = timestamp
        self.device_time_final = float(value)
        self.round_trip_final_time = self.round_trip_final_end - self.round_trip_final_start
        self.log("Device final time {} ".format(value))

        # compute the test results and send to device
        results = "pass" if self.compute_parameter() else "fail"
        self.send_kv(results, "0")

    def setup(self):
        self.register_callback('timing_drift_check_start', self._callback_timing_drift_check_start)
        self.register_callback('base_time', self._callback_base_time)
        self.register_callback('final_time', self._callback_final_time)

    def compute_parameter(self, failure_criteria=0.05):
        t_max = self.round_trip_final_end - self.round_trip_base_start
        t_min = self.round_trip_final_start - self.round_trip_base_end
        t_max_hi = t_max * (1 + failure_criteria)
        t_max_lo = t_max * (1 - failure_criteria)
        t_min_hi = t_min * (1 + failure_criteria)
        t_min_lo = t_min * (1 - failure_criteria)
        device_time = (self.device_time_final - self.device_time_base) / self.mega

        self.log("Compute host events")
        self.log("Transport delay 0: {}".format(self.round_trip_base_time))
        self.log("Transport delay 1: {}".format(self.round_trip_final_time))
        self.log("DUT base time : {}".format(self.device_time_base))
        self.log("DUT end time  : {}".format(self.device_time_final))

        self.log("min_pass : {} , max_pass : {} for {}%%".format(t_max_lo, t_min_hi, failure_criteria * 100))
        self.log("min_inconclusive : {} , max_inconclusive : {}".format(t_min_lo, t_max_hi))
        self.log("Time reported by device: {}".format(device_time))

        if t_max_lo <= device_time <= t_min_hi:
            self.log("Test passed !!!")
            self.__result = True
        elif t_min_lo <= device_time <= t_max_hi:
            self.log("Test inconclusive due to transport delay, retrying")
            self.__result = False
        else:
            self.log("Time outside of passing range. Timing drift seems to be present !!!")
            self.__result = False
        return self.__result

    def result(self):
        return self.__result

    def teardown(self):
        pass
