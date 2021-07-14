"""
Copyright (c) 2019, Arm Limited and affiliates.
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
import psutil
import tempfile
import uuid
import os
import platform
import subprocess
import sys
system_name = platform.system()
if system_name == "Windows":
    import wmi


class PyusbMSDTest(BaseHostTest):
    """Host side test for USB MSD class."""

    __result = None
    MOUNT_WAIT_TIME = 25  # in [s]
    initial_disk_list = None
    msd_disk = None
    serial_number = None

    def _callback_device_ready(self, key, value, timestamp):
        """Send a unique USB SN to the device.
        DUT uses this SN every time it connects to host as a USB device.
        """
        self.serial_number = uuid.uuid4().hex # 32 hex digit string
        self.send_kv("serial_number", self.serial_number)

    def _callback_check_file_exist(self, key, value, timestamp):
        """Check if file exist.

        """
        folder_name, file_name, file_content = value.split(' ')
        msd_disk = MSDUtils.disk_path(self.serial_number)
        file_path = os.path.join(msd_disk, folder_name, file_name)
        try:
            file = open(file_path, 'r')
            line = file.readline()
            file.close()
            time.sleep(2)  # wait for msd communication done
            if line == file_content:
                self.send_kv("exist", "0")
                return
            self.report_error("file content invalid")
        except IOError as err:
            self.log('{} !!!'.format(err))
        self.send_kv("non-exist", "0")

    def _callback_delete_files(self, key, value, timestamp):
        """Delete test file.

        """
        dir_name, file_name = value.split(' ')
        msd_disk = MSDUtils.disk_path(self.serial_number)
        try:
            os.remove(os.path.join(msd_disk, dir_name, file_name))
        except:
            self.report_error("delete files")
            return
        time.sleep(2)  # wait for msd communication done
        self.report_success()

    def _callback_check_if_mounted(self, key, value, timestamp):
        """Check if disk was mounted.

        """
        wait_time = self.MOUNT_WAIT_TIME
        while wait_time != 0:
            msd_disk = MSDUtils.disk_path(self.serial_number)
            if msd_disk is not None:
                # MSD disk found
                time.sleep(2)  # wait for msd communication done
                self.report_success()
                return
            wait_time -= 1
            time.sleep(1)  # wait 1s and try again
        self.report_error("mount check")

    def _callback_check_if_not_mounted(self, key, value, timestamp):
        """Check if disk was unmouted.

        """
        wait_time = self.MOUNT_WAIT_TIME
        while wait_time != 0:
            msd_disk = MSDUtils.disk_path(self.serial_number)
            if msd_disk is None:
                #self.msd_disk = None
                time.sleep(2)  # wait for msd communication done
                self.report_success()
                return
            wait_time -= 1
            time.sleep(1)  # wait 1s and try again
        self.report_error("unmount check")

    def _callback_get_mounted_fs_size(self, key, value, timestamp):
        """Record visible filesystem size.

        """
        stats = psutil.disk_usage(MSDUtils.disk_path(self.serial_number))
        self.send_kv("{}".format(stats.total), "0")

    def _callback_unmount(self, key, value, timestamp):
        """Disk unmount.

        """
        if MSDUtils.unmount(serial=self.serial_number):
            self.report_success()
        else:
            self.report_error("unmount")

    def _callback_os_type(self, key, value, timestamp):
        system_name = platform.system()
        if system_name == "Windows":
            self.send_kv("os_type", 1)
        elif system_name == "Linux":
            self.send_kv("os_type", 2)
        elif system_name == "Darwin":
            self.send_kv("os_type", 3)

    def setup(self):
        self.register_callback("get_os_type", self._callback_os_type)
        self.register_callback("get_serial_number", self._callback_device_ready)
        self.register_callback('check_if_mounted', self._callback_check_if_mounted)
        self.register_callback('check_if_not_mounted', self._callback_check_if_not_mounted)
        self.register_callback('get_mounted_fs_size', self._callback_get_mounted_fs_size)
        self.register_callback('check_file_exist', self._callback_check_file_exist)
        self.register_callback('delete_files', self._callback_delete_files)
        self.register_callback('unmount', self._callback_unmount)

    def report_success(self):
        self.send_kv("passed", "0")

    def report_error(self, msg):
        self.log('{} failed !!!'.format(msg))
        self.send_kv("failed", "0")

    def result(self):
        return self.__result

    def teardown(self):
        pass


class MSDUtils(object):

    @staticmethod
    def disk_path(serial):
        system_name = platform.system()
        if system_name == "Windows":
            return MSDUtils._disk_path_windows(serial)
        elif system_name == "Linux":
            return MSDUtils._disk_path_linux(serial)
        elif system_name == "Darwin":
            return MSDUtils._disk_path_mac(serial)
        return None

    @staticmethod
    def unmount(serial):
        system_name = platform.system()
        if system_name == "Windows":
            return MSDUtils._unmount_windows(serial)
        elif system_name == "Linux":
            return MSDUtils._unmount_linux(serial)
        elif system_name == "Darwin":
            return MSDUtils._unmount_mac(serial)
        return False

    @staticmethod
    def _disk_path_windows(serial):
        c = wmi.WMI()
        for physical_disk in c.Win32_DiskDrive():
            if serial == physical_disk.SerialNumber:
                for partition in physical_disk.associators("Win32_DiskDriveToDiskPartition"):
                    for logical_disk in partition.associators("Win32_LogicalDiskToPartition"):
                        return logical_disk.Caption
        return None

    @staticmethod
    def _disk_path_linux(serial):
        output = subprocess.check_output(['lsblk', '-dnoserial,mountpoint']).split(b'\n')
        for line in output:
            serial_and_mount_point = line.split()
            if len(serial_and_mount_point) == 2:
                if serial_and_mount_point[0] == str(serial):
                    return serial_and_mount_point[1]
        return None

    @staticmethod
    def _disk_path_mac(serial):
        # TODO:
        #     add implementation
        return None

    @staticmethod
    def _unmount_windows(serial):
        disk_path = MSDUtils._disk_path_windows(serial)
        cmd_string = r'(New-Object -comObject Shell.Application).Namespace(17).ParseName("{}").InvokeVerb("Eject")'.format(disk_path)

        try_count = 10
        while try_count:
            p = subprocess.Popen(["powershell.exe", cmd_string], stdout=sys.stdout)
            p.communicate()
            try_count -= 1
            if MSDUtils._disk_path_windows(serial) is None:
                return True
            time.sleep(1)

        return False

    @staticmethod
    def _unmount_linux(serial):
        disk_path = MSDUtils._disk_path_linux(serial)
        os.system("umount " + disk_path)
        return MSDUtils._disk_path_linux(serial) is None

    @staticmethod
    def _unmount_mac(serial):
        disk_path = MSDUtils._disk_path_mac(serial)
        os.system("diskutil unmount " + disk_path)
        disks = set(MSDUtils._disks_mac())
        return MSDUtils._disk_path_mac(serial) is None
