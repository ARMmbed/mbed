"""
Copyright (c) 2011-2020, Arm Limited and affiliates
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
import calendar
import datetime

class RTC_time_calc_test(BaseHostTest):
    """
    This is the host part of the test to verify if:
    - _rtc_mktime function converts a calendar time into time since UNIX epoch as a time_t,
    - _rtc_localtime function converts a given time in seconds since epoch into calendar time.
    
    The same algoritm to generate next calendar time to be tested is used by both parts of the test.
    We will check if correct time since UNIX epoch is calculated for the first and the last day
    of each month and across valid years.
    
    Mbed part of the test sends calculated time since UNIX epoch.
    This part validates given value and responds to indicate pass or fail.
    Additionally it sends also encoded day of week and day of year which
    will be needed to verify _rtc_localtime.
    
    Support for both types of RTC devices is provided:
    - RTCs which handles all leap years in the mentioned year range correctly. Leap year is determined by checking if
      the year counter value is divisible by 400, 100, and 4. No problem here.
    - RTCs which handles leap years correctly up to 2100. The RTC does a simple bit comparison to see if the two
      lowest order bits of the year counter are zero. In this case 2100 year will be considered
      incorrectly as a leap year, so the last valid point in time will be 28.02.2100 23:59:59 and next day will be
      29.02.2100 (invalid). So after 28.02.2100 the day counter will be off by a day.
      
    """

    edge_date = datetime.datetime(2100, 2, 28, 0, 0, 0)
    
    # Test the following years:
    # - first - 1970
    # - example not leap year (not divisible by 4)
    # - example leap year (divisible by 4 and by 100 and by 400) 
    # - example leap year (divisible by 4 and not by 100) 
    # - example not leap year (divisible by 4 and by 100) 
    # - last fully supported  - 2105
    years = [1970, 1971, 2000, 2096, 2100, 2105]
    year_id = 0
             
             

    full_leap_year_support = False

    RTC_FULL_LEAP_YEAR_SUPPORT = 0
    RTC_PARTIAL_LEAP_YEAR_SUPPORT = 1

    def _set_leap_year_support(self, key, value, timestamp):
        if (int(value) == self.RTC_FULL_LEAP_YEAR_SUPPORT):
            self.full_leap_year_support = True
        else:
            self.full_leap_year_support = False

        self.first = True
        self.date = datetime.datetime(1970, 1, 1, 23, 0, 0)
        self.year_id = 0

    def _verify_timestamp(self, key, value, timestamp):
        # week day in python is counted from sunday(0) and on mbed side week day is counted from monday(0).
        # year day in python is counted from 1 and on mbed side year day is counted from 0.
        week_day = ((self.date.timetuple().tm_wday + 1) % 7)
        year_day = self.date.timetuple().tm_yday - 1

        # Fix for RTC which not have full leap year support.
        if (not self.full_leap_year_support):
            if self.date >= self.edge_date:
                # After 28.02.2100 we should be one day off - add this day and store original
                date_org = self.date
                self.date += datetime.timedelta(days = 1)
                
                # Adjust week day.
                week_day = ((self.date.timetuple().tm_wday + 1) % 7)

                # Adjust year day.
                if (self.date.year == 2100):
                    year_day = self.date.timetuple().tm_yday - 1
                else:
                    year_day = date_org.timetuple().tm_yday - 1

                # Last day in year
                if (self.date.month == 1 and self.date.day == 1):
                    if (self.date.year == 2101):
                        # Exception for year 2100 - ivalid handled by RTC without full leap year support
                        year_day = 365
                    else:
                        year_day = date_org.timetuple().tm_yday - 1

        t = (self.date.year , self.date.month, self.date.day, self.date.hour, self.date.minute, self.date.second, 0, 0, 0)
        
        expected_timestamp = calendar.timegm(t)
        actual_timestamp = int(value) & 0xffffffff # convert to unsigned int

        # encode week day and year day in the response
        response = (week_day << 16) | year_day
        
        if (actual_timestamp == expected_timestamp):
            # response contains encoded week day and year day
            self.send_kv("passed", str(response))
        else:
            self.send_kv("failed", 0)
            print("expected = %d, result = %d" %  (expected_timestamp , actual_timestamp))

        # calculate next date
        if (self.first):
            days_range = calendar.monthrange(self.date.year, self.date.month)
            self.date = self.date.replace(day = days_range[1], minute = 59, second = 59)
            self.first = not self.first
        else:
            self.date += datetime.timedelta(days = 1)
            if (self.date.month == 1):
                self.year_id += 1
                if (len(self.years) == self.year_id):
                    # All years were processed, no need to calc next date
                    return
                self.date = self.date.replace(year = self.years[self.year_id])
            self.date = self.date.replace(day = 1, minute = 0, second = 0)
            self.first = not self.first

    def setup(self):
        self.register_callback('timestamp', self._verify_timestamp)
        self.register_callback('leap_year_setup', self._set_leap_year_support)
