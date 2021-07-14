"""
Copyright (c) 2011-2020 ARM Limited
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
from htrun.host_tests_logger import HtrunLogger
import platform
import time, re, os
import subprocess


class SoftAPBasicHostTests(BaseHostTest):
    """ Basic SoftAP test with host as STA

        [Following configuration is required before test]
        1. Identify and update the host wifi device name self.wifi_dev
        2. Add user as sudoer without password to allow script to run sudo command. Restrict the sudo command pattern to enhance security
            ###  sudoer config begin ###
            # Allow members of group sudo to execute any command
            %sudo	ALL=(ALL:ALL) ALL
            <user>  ALL=(ALL) NOPASSWD: /usr/sbin/rfkill unblock wifi, /sbin/wpa_supplicant -B -i * -c *, /usr/bin/killall wpa_supplicant, /sbin/wpa_cli -i * status, /sbin/dhclient -r *, /sbin/dhclient -nw -1 *
            ###  sudoer config end ###

        [Test behavior]
        1. Host wait for DUT SoftAP to start
        2. Upon receive ('softap', 'xxx') message from DUT SoftAP to perform corresponding test
    """
    # Class constants
    HOST_WIFI_CONNECTION_WAIT_SEC = 8
    HOST_WIFI_DHCP_GET_IP_WAIT_SEC = 3
    HOST_WIFI_INTER_CMDS_WAIT_SEC = 1
    HOST_WIFI_SEND_PING_ECHO_CNT = 1
    HOST_WPA_STATUS_REGEX = re.compile("^wpa_state=(\w+)$", re.MULTILINE)
    HOST_WPA_SSID_REGEX = re.compile("^ssid=(\w+)$", re.MULTILINE)
    HOST_PING_SOFTAP_REGEX = re.compile("^(\d+) packets transmitted, (\d+) received", re.MULTILINE)

    def __init__(self):
        super(SoftAPBasicHostTests, self).__init__()
        self.logger = HtrunLogger('TEST')
        self.log = self.logger.prn_inf

        self.__result = False
        self.wifi_dev = 'wlan0'

        # The SoftAP config must be consisten with client side
        self.softap_ssid = 'MBEDSoftAPTest'
        self.softap_password = 'abcd1234'
        self.softap_ip_address = '192.168.10.1'

        self.softap_event_action_table = {
            'up' : self.host_wifi_softap_up_test
        }

    def _callback_os_type(self, key, value, timestamp):
        ''' Get host OS type
        '''
        system_name = platform.system()
        self.log('os_type = {}'.format(system_name))
        self.send_kv("os_type", system_name)
        self.__result = True

    def _callback_softap_events(self, key, value, timestamp):
        ''' DUT SoftAP event handler
            Root handler of all 'softap' events, dispatch secondary handler based on "value"
        '''
        self.log('Get ({}, {}) from DUT'.format(key, value))

        if not value in self.softap_event_action_table:
            self.log('Unknown softap event {}'.format(value))
            self.report_error('Unknow softap event')
            return None

        if not self.softap_event_action_table[value]():
            self.report_error('Response to ({}, {}) FAILED!'.format(key, value))
        else:
            self.report_success()
            self.__result = True
        return None

    def setup(self):
        # all functions that can be called from the client
        self.register_callback('softap', self._callback_softap_events)
        self.register_callback('get_os_type', self._callback_os_type)
        try:
            cmd = ['sudo', 'rfkill', 'unblock', 'wifi']
            output = subprocess.check_output(cmd, universal_newlines=True)
            self.log('SETUP: {} => {}'.format(cmd, output))
            time.sleep(SoftAPBasicHostTests.HOST_WIFI_INTER_CMDS_WAIT_SEC)
        except subprocess.CalledProcessError:
            self.log ('WARNING: Setup commands may not all scceeed.')


    def result(self):
        return self.__result

    def teardown(self):
        # release existing dhclient
        try:
            cmd = ['sudo', 'dhclient', '-r', self.wifi_dev]
            output = subprocess.check_output(cmd, universal_newlines=True)
            self.log('TEARDOWN: {} => {}'.format(cmd, output))
            time.sleep(SoftAPBasicHostTests.HOST_WIFI_INTER_CMDS_WAIT_SEC)

            cmd = ['sudo', 'killall', 'wpa_supplicant']
            output = subprocess.check_output(cmd, universal_newlines=True)
            self.log('TEARDOWN: {} => {}'.format(cmd, output))
        except subprocess.CalledProcessError:
            self.log ('WARNING: TearDown commands may not all scceeed.')

    def host_wifi_softap_up_test(self):
        ''' Run host side test for ('softap', 'up') event
        '''
        if not self.host_wifi_connect_softap_linux():
            return False
        if not self.host_wifi_restart_dhcp_linux():
            return False
        if not self.host_wifi_ping_softap():
            return False
        return True

    def host_wifi_connect_softap_linux(self):
        ''' Host Linux as STA to connect to DUT SoftAP
            Test will run after receiving ('softap', 'up') message from DUT SoftAP
        '''
        # killall is expected to fail when the host does NOT run wpa_supplicant by default
        try:
            cmd = ['sudo', 'killall', 'wpa_supplicant']
            output = subprocess.check_output(cmd, universal_newlines=True)
            self.log('kill existing wpa_supplicant: {}'.format(cmd, universal_newlines=True))
        except subprocess.CalledProcessError:
            self.log('INFO: no prior wpa_supplicant found')

        try:
            dir_path = os.path.dirname(os.path.realpath(__file__))
            time.sleep(SoftAPBasicHostTests.HOST_WIFI_INTER_CMDS_WAIT_SEC)
            cmd = ['sudo', 'wpa_supplicant', '-B', '-i', self.wifi_dev, '-c', \
                dir_path+'/softap_wpa_supplicant.conf']
            output = subprocess.check_output(cmd, universal_newlines=True)
            self.log('start wpa_supplicant: {} => {}'.format(cmd, output))

            self.log('Wait {} sec to connecct to {} ...'.format(\
                SoftAPBasicHostTests.HOST_WIFI_CONNECTION_WAIT_SEC, self.softap_ssid))
            time.sleep(SoftAPBasicHostTests.HOST_WIFI_CONNECTION_WAIT_SEC)
            cmd = ['sudo', 'wpa_cli', '-i', self.wifi_dev, 'status']
            output = subprocess.check_output(cmd, universal_newlines=True)
            self.log('{}: {}'.format(cmd, output))
        except subprocess.CalledProcessError as error:
            self.log("ERROR: {} => {}".format(error.cmd, error.output))
            return False

        match = SoftAPBasicHostTests.HOST_WPA_STATUS_REGEX.search(output)
        if match is None or (match.group(1) != 'COMPLETED' and match.group(1) != 'ASSOCIATED'):
            self.log('ERROR: status => {}'.format(output))
            return False

        match = SoftAPBasicHostTests.HOST_WPA_SSID_REGEX.search(output)
        if match is None or match.group(1) != self.softap_ssid:
            self.log('ERROR: status => {}'.format(output))
            return False
        self.log('SoftAP join PASS')
        return True

    def host_wifi_restart_dhcp_linux(self):
        ''' Restart host dhcp client to obtain IP
        '''
        try:
            # release existing dhclient
            cmd = ['sudo', 'dhclient', '-r', self.wifi_dev]
            output = subprocess.check_output(cmd, universal_newlines=True)
            self.log('{} => {}'.format(cmd, output))
            time.sleep(SoftAPBasicHostTests.HOST_WIFI_INTER_CMDS_WAIT_SEC)

            cmd = ['sudo', 'dhclient', '-nw', '-1', self.wifi_dev]
            output = subprocess.check_output(cmd, universal_newlines=True)
            self.log('{} => {}'.format(cmd, output))
            self.log('Wait {} sec for dhcp to get IP from SoftAP'.format( \
                SoftAPBasicHostTests.HOST_WIFI_DHCP_GET_IP_WAIT_SEC))
            time.sleep(SoftAPBasicHostTests.HOST_WIFI_DHCP_GET_IP_WAIT_SEC)
        except subprocess.CalledProcessError as error:
            self.log("ERROR: {} => {}".format(error.cmd, error.output))
            return False
        return True

    def host_wifi_ping_softap(self):
        ''' Ping SoftAP with self.softap_ip_address
        '''
        try:
            cmd = ['ping', '-c', str(SoftAPBasicHostTests.HOST_WIFI_SEND_PING_ECHO_CNT), \
                self.softap_ip_address]
            output = subprocess.check_output(cmd, universal_newlines=True)
            self.log('{} => {}'.format(cmd, output))
        except subprocess.CalledProcessError as error:
            self.log("ERROR: {} => {}".format(error.cmd, error.output))
            return False

        match = SoftAPBasicHostTests.HOST_PING_SOFTAP_REGEX.search(output)
        if match is None or match.group(2) == '0':
            self.log('Ping ERROR: {} => {}'.format(cmd, output))
            return False
        self.log('Ping PASS: {}/{} echo received'.format(match.group(2), match.group(1)))
        return True

    def get_wpa_supplicant_conf_path(self):
        ''' Report host side test success to DUT
        '''
        self.send_kv("passed", "0")

    def report_success(self):
        ''' Report host side test success to DUT
        '''
        self.send_kv("passed", "0")

    def report_error(self, msg):
        ''' Report host side test error to DUT
        '''
        self.log('{} failed !!!'.format(msg))
        self.send_kv("failed", "0")
