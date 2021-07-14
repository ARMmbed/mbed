"""
mbed SDK
Copyright (c) 2019 ARM Limited
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
from __future__ import print_function
import functools
import time
import threading
import uuid
import sys
import htrun
import usb.core
from usb.util import (
    CTRL_IN,
    CTRL_OUT,
    CTRL_TYPE_STANDARD,
    CTRL_TYPE_CLASS,
    CTRL_RECIPIENT_DEVICE,
    CTRL_RECIPIENT_INTERFACE,
    DESC_TYPE_CONFIG,
    build_request_type)

if sys.platform.startswith('win'):
    # Use libusb0 on Windows. libusb1 implementation for Windows
    # does not support all features necessary for testing.
    import usb.backend.libusb0
    USB_BACKEND = usb.backend.libusb0.get_backend()
else:
    # Use a default backend on other platforms.
    USB_BACKEND = None

try:
    import hid
except ImportError:
    CYTHON_HIDAPI_PRESENT = False
else:
    CYTHON_HIDAPI_PRESENT = True

# USB device -- device classes
USB_CLASS_HID = 0x03

# USB device -- standard requests
USB_REQUEST_GET_DESCRIPTOR = 0x06

# USB device -- HID class requests
HID_REQUEST_GET_REPORT = 0x01
HID_REQUEST_SET_REPORT = 0x09
HID_REQUEST_GET_IDLE = 0x02
HID_REQUEST_SET_IDLE = 0x0A
HID_REQUEST_GET_PROTOCOL = 0x03
HID_REQUEST_SET_PROTOCOL = 0x0B

# USB device -- HID class descriptors
DESC_TYPE_HID_HID = 0x21
DESC_TYPE_HID_REPORT = 0x22
DESC_TYPE_HID_PHYSICAL = 0x23

# USB device -- HID class descriptor lengths
DESC_LEN_HID_HID = 0x09

# USB device -- descriptor fields offsets
DESC_OFFSET_BLENGTH = 0
DESC_OFFSET_BDESCRIPTORTYPE = 1

# USB device -- HID subclasses
HID_SUBCLASS_NONE = 0
HID_SUBCLASS_BOOT = 1

# USB device -- HID protocols
HID_PROTOCOL_NONE = 0
HID_PROTOCOL_KEYBOARD = 1
HID_PROTOCOL_MOUSE = 2

# Greentea message keys used for callbacks
MSG_KEY_DEVICE_READY = 'dev_ready'
MSG_KEY_HOST_READY = 'host_ready'
MSG_KEY_SERIAL_NUMBER = 'usb_dev_sn'
MSG_KEY_TEST_GET_DESCRIPTOR_HID = 'test_get_desc_hid'
MSG_KEY_TEST_GET_DESCRIPTOR_CFG = 'test_get_desc_cfg'
MSG_KEY_TEST_REQUESTS = 'test_requests'
MSG_KEY_TEST_RAW_IO = 'test_raw_io'

# Greentea message keys used to notify DUT of test status
MSG_KEY_TEST_CASE_FAILED = 'fail'
MSG_KEY_TEST_CASE_PASSED = 'pass'
MSG_VALUE_DUMMY = '0'
MSG_VALUE_NOT_SUPPORTED = 'not_supported'

# Constants for the tests.
KEYBOARD_IDLE_RATE_TO_SET = 0x00  # Duration = 0 (indefinite)
HID_PROTOCOL_TO_SET = 0x01  # Protocol = 1 (Report Protocol)
RAW_IO_REPS = 16  # Number of loopback test reps.


def build_get_desc_value(desc_type, desc_index):
    """Build and return a wValue field for control requests."""
    return (desc_type << 8) | desc_index


def usb_hid_path(serial_number):
    """Get a USB HID device system path based on the serial number."""
    if not CYTHON_HIDAPI_PRESENT:
        return None
    for device_info in hid.enumerate():  # pylint: disable=no-member
        if device_info.get('serial_number') == serial_number:  # pylint: disable=not-callable
            return device_info['path']
    return None


def get_descriptor_types(desc):
    """Return a list of all bDescriptorType values found in desc.

    desc is expected to be a sequence of bytes, i.e. array.array('B')
    returned from usb.core.

    From the USB 2.0 spec, paragraph 9.5:
    Each descriptor begins with a byte-wide field that contains the total
    number of bytes in the descriptor followed by a byte-wide field that
    identifies the descriptor type.
    """
    tmp_desc = desc[DESC_OFFSET_BLENGTH:]
    desc_types = []
    while True:
        try:
            bLength = tmp_desc[DESC_OFFSET_BLENGTH]  # pylint: disable=invalid-name
            bDescriptorType = tmp_desc[DESC_OFFSET_BDESCRIPTORTYPE]  # pylint: disable=invalid-name
            desc_types.append(int(bDescriptorType))
            tmp_desc = tmp_desc[int(bLength):]
        except IndexError:
            break
    return desc_types


def get_hid_descriptor_parts(hid_descriptor):
    """Return bNumDescriptors, bDescriptorType, wDescriptorLength from hid_descriptor."""
    err_msg = 'Invalid HID class descriptor'
    try:
        if hid_descriptor[1] != DESC_TYPE_HID_HID:
            raise TypeError(err_msg)
        bNumDescriptors = int(hid_descriptor[5])  # pylint: disable=invalid-name
        bDescriptorType = int(hid_descriptor[6])  # pylint: disable=invalid-name
        wDescriptorLength = int((hid_descriptor[8] << 8) | hid_descriptor[7])  # pylint: disable=invalid-name
    except (IndexError, ValueError):
        raise TypeError(err_msg)
    return bNumDescriptors, bDescriptorType, wDescriptorLength


def get_usbhid_dev_type(intf):
    """Return a name of the HID device class type for intf."""
    if not isinstance(intf, usb.core.Interface):
        return None
    if intf.bInterfaceClass != USB_CLASS_HID:
        # USB Device Class Definition for HID, v1.11, paragraphs 4.1, 4.2 & 4.3:
        #   the class is specified in the Interface descriptor
        #   and not the Device descriptor.
        return None
    if (intf.bInterfaceSubClass == HID_SUBCLASS_BOOT
            and intf.bInterfaceProtocol == HID_PROTOCOL_KEYBOARD):
        return 'boot_keyboard'
    if (intf.bInterfaceSubClass == HID_SUBCLASS_BOOT
            and intf.bInterfaceProtocol == HID_PROTOCOL_MOUSE):
        return 'boot_mouse'
    # Determining any other HID dev type, like a non-boot_keyboard or
    # a non-boot_mouse requires getting and parsing a HID Report descriptor
    # for intf.
    # Only the boot_keyboard, boot_mouse and other_device are used for this
    # greentea test suite.
    return 'other_device'


class RetryError(Exception):
    """Exception raised by retry_fun_call()."""


def retry_fun_call(fun, num_retries=3, retry_delay=0.0):
    """Call fun and retry if any exception was raised.

    fun is called at most num_retries with a retry_dalay in between calls.
    Raises RetryError if the retry limit is exhausted.
    """
    verbose = False
    final_err = None
    for retry in range(1, num_retries + 1):
        try:
            return fun()  # pylint: disable=not-callable
        except Exception as exc:  # pylint: disable=broad-except
            final_err = exc
            if verbose:
                print('Retry {}/{} failed ({})'
                      .format(retry, num_retries, str(fun)))
            time.sleep(retry_delay)
    err_msg = 'Failed with "{}". Tried {} times.'
    raise RetryError(err_msg.format(final_err, num_retries))


def raise_if_different(expected, actual, text=''):
    """Raise a RuntimeError if actual is different than expected."""
    if expected != actual:
        raise RuntimeError('{}Got {!r}, expected {!r}.'.format(text, actual, expected))


def raise_if_false(expression, text):
    """Raise a RuntimeError if expression is False."""
    if not expression:
        raise RuntimeError(text)


class USBHIDTest(htrun.BaseHostTest):
    """Host side test for USB device HID class."""

    @staticmethod
    def get_usb_hid_path(usb_id_str):
        """Get a USB HID device path as registered in the system.

        Search is based on the unique USB SN generated by the host
        during test suite setup.
        Raises RuntimeError if the device is not found.
        """
        hid_path = usb_hid_path(usb_id_str)
        if hid_path is None:
            err_msg = 'USB HID device (SN={}) not found.'
            raise RuntimeError(err_msg.format(usb_id_str))
        return hid_path

    @staticmethod
    def get_usb_dev(usb_id_str):
        """Get a usb.core.Device instance.

        Search is based on the unique USB SN generated by the host
        during test suite setup.
        Raises RuntimeError if the device is not found.
        """
        usb_dev = usb.core.find(custom_match=lambda d: d.serial_number == usb_id_str, backend=USB_BACKEND)
        if usb_dev is None:
            err_msg = 'USB device (SN={}) not found.'
            raise RuntimeError(err_msg.format(usb_id_str))
        return usb_dev

    def __init__(self):
        super(USBHIDTest, self).__init__()
        self.__bg_task = None
        self.dut_usb_dev_sn = uuid.uuid4().hex  # 32 hex digit string

    def notify_error(self, msg):
        """Terminate the test with an error msg."""
        self.log('TEST ERROR: {}'.format(msg))
        self.notify_complete(None)

    def notify_failure(self, msg):
        """Report a host side test failure to the DUT."""
        self.log('TEST FAILED: {}'.format(msg))
        self.send_kv(MSG_KEY_TEST_CASE_FAILED, MSG_VALUE_DUMMY)

    def notify_success(self, value=None, msg=''):
        """Report a host side test success to the DUT."""
        if msg:
            self.log('TEST PASSED: {}'.format(msg))
        if value is None:
            value = MSG_VALUE_DUMMY
        self.send_kv(MSG_KEY_TEST_CASE_PASSED, value)

    def cb_test_get_hid_desc(self, key, value, timestamp):
        """Verify the device handles Get_Descriptor request correctly.

        Two requests are tested for every HID interface:
        1. Get_Descriptor(HID),
        2. Get_Descriptor(Report).
        Details in USB Device Class Definition for HID, v1.11, paragraph 7.1.
        """
        kwargs_hid_desc_req = {
            'bmRequestType': build_request_type(
                CTRL_IN, CTRL_TYPE_STANDARD, CTRL_RECIPIENT_INTERFACE),
            'bRequest': USB_REQUEST_GET_DESCRIPTOR,
            # Descriptor Index (part of wValue) is reset to zero for
            # HID class descriptors other than Physical ones.
            'wValue': build_get_desc_value(DESC_TYPE_HID_HID, 0x00),
            # wIndex is replaced with the Interface Number in the loop.
            'wIndex': None,
            'data_or_wLength': DESC_LEN_HID_HID}
        kwargs_report_desc_req = {
            'bmRequestType': build_request_type(
                CTRL_IN, CTRL_TYPE_STANDARD, CTRL_RECIPIENT_INTERFACE),
            'bRequest': USB_REQUEST_GET_DESCRIPTOR,
            # Descriptor Index (part of wValue) is reset to zero for
            # HID class descriptors other than Physical ones.
            'wValue': build_get_desc_value(DESC_TYPE_HID_REPORT, 0x00),
            # wIndex is replaced with the Interface Number in the loop.
            'wIndex': None,
            # wLength is replaced with the Report Descriptor Length in the loop.
            'data_or_wLength': None}
        mbed_hid_dev = None
        report_desc_lengths = []
        try:
            mbed_hid_dev = retry_fun_call(
                fun=functools.partial(self.get_usb_dev, self.dut_usb_dev_sn),  # pylint: disable=not-callable
                num_retries=20,
                retry_delay=0.05)
        except RetryError as exc:
            self.notify_error(exc)
            return
        try:
            for intf in mbed_hid_dev.get_active_configuration():  # pylint: disable=not-callable
                if intf.bInterfaceClass != USB_CLASS_HID:
                    continue
                try:
                    if mbed_hid_dev.is_kernel_driver_active(intf.bInterfaceNumber):
                        mbed_hid_dev.detach_kernel_driver(intf.bInterfaceNumber)  # pylint: disable=not-callable
                except (NotImplementedError, AttributeError):
                    pass

                # Request the HID descriptor.
                kwargs_hid_desc_req['wIndex'] = intf.bInterfaceNumber
                hid_desc = mbed_hid_dev.ctrl_transfer(**kwargs_hid_desc_req)  # pylint: disable=not-callable
                try:
                    bNumDescriptors, bDescriptorType, wDescriptorLength = get_hid_descriptor_parts(hid_desc)  # pylint: disable=invalid-name
                except TypeError as exc:
                    self.notify_error(exc)
                    return
                raise_if_different(1, bNumDescriptors, 'Exactly one HID Report descriptor expected. ')
                raise_if_different(DESC_TYPE_HID_REPORT, bDescriptorType, 'Invalid HID class descriptor type. ')
                raise_if_false(wDescriptorLength > 0, 'Invalid HID Report descriptor length. ')

                # Request the Report descriptor.
                kwargs_report_desc_req['wIndex'] = intf.bInterfaceNumber
                kwargs_report_desc_req['data_or_wLength'] = wDescriptorLength
                report_desc = mbed_hid_dev.ctrl_transfer(**kwargs_report_desc_req)  # pylint: disable=not-callable
                raise_if_different(wDescriptorLength, len(report_desc),
                                   'The size of data received does not match the HID Report descriptor length. ')
                report_desc_lengths.append(len(report_desc))
        except usb.core.USBError as exc:
            self.notify_failure('Get_Descriptor request failed. {}'.format(exc))
        except RuntimeError as exc:
            self.notify_failure(exc)
        else:
            # Send the report desc len to the device.
            # USBHID::report_desc_length() returns uint16_t
            msg_value = '{0:04x}'.format(max(report_desc_lengths))
            self.notify_success(msg_value)

    def cb_test_get_cfg_desc(self, key, value, timestamp):
        """Verify the device provides required HID descriptors.

        USB Device Class Definition for HID, v1.11, paragraph 7.1:
        When a Get_Descriptor(Configuration) request is issued, it
        returns (...), and the HID descriptor for each interface.
        """
        kwargs_cfg_desc_req = {
            'bmRequestType': build_request_type(
                CTRL_IN, CTRL_TYPE_STANDARD, CTRL_RECIPIENT_DEVICE),
            'bRequest': USB_REQUEST_GET_DESCRIPTOR,
            # Descriptor Index (part of wValue) is reset to zero.
            'wValue': build_get_desc_value(DESC_TYPE_CONFIG, 0x00),
            # wIndex is reset to zero.
            'wIndex': 0x00,
            # wLength unknown, set to 1024.
            'data_or_wLength': 1024}
        mbed_hid_dev = None
        try:
            mbed_hid_dev = retry_fun_call(
                fun=functools.partial(self.get_usb_dev, self.dut_usb_dev_sn),  # pylint: disable=not-callable
                num_retries=20,
                retry_delay=0.05)
        except RetryError as exc:
            self.notify_error(exc)
            return
        try:
            # Request the Configuration descriptor.
            cfg_desc = mbed_hid_dev.ctrl_transfer(**kwargs_cfg_desc_req)  # pylint: disable=not-callable
            raise_if_false(DESC_TYPE_HID_HID in get_descriptor_types(cfg_desc),
                           'No HID class descriptor in the Configuration descriptor.')
        except usb.core.USBError as exc:
            self.notify_failure('Get_Descriptor request failed. {}'.format(exc))
        except RuntimeError as exc:
            self.notify_failure(exc)
        else:
            self.notify_success()

    def cb_test_class_requests(self, key, value, timestamp):
        """Verify all required HID requests are supported.

        USB Device Class Definition for HID, v1.11, Appendix G:
        1. Get_Report -- required for all types,
        2. Set_Report -- not required if dev doesn't declare an Output Report,
        3. Get_Idle -- required for keyboards,
        4. Set_Idle -- required for keyboards,
        5. Get_Protocol -- required for boot_keyboard and boot_mouse,
        6. Set_Protocol -- required for boot_keyboard and boot_mouse.

        Details in USB Device Class Definition for HID, v1.11, paragraph 7.2.
        """
        kwargs_get_report_request = {
            'bmRequestType': build_request_type(
                CTRL_IN, CTRL_TYPE_CLASS, CTRL_RECIPIENT_INTERFACE),
            'bRequest': HID_REQUEST_GET_REPORT,
            # wValue: ReportType = Input, ReportID = 0 (not used)
            'wValue': (0x01 << 8) | 0x00,
            # wIndex: InterfaceNumber (defined later)
            'wIndex': None,
            # wLength: unknown, set to 1024
            'data_or_wLength': 1024}
        kwargs_get_idle_request = {
            'bmRequestType': build_request_type(
                CTRL_IN, CTRL_TYPE_CLASS, CTRL_RECIPIENT_INTERFACE),
            'bRequest': HID_REQUEST_GET_IDLE,
            # wValue: 0, ReportID = 0 (not used)
            'wValue': (0x00 << 8) | 0x00,
            # wIndex: InterfaceNumber (defined later)
            'wIndex': None,
            'data_or_wLength': 1}
        kwargs_set_idle_request = {
            'bmRequestType': build_request_type(
                CTRL_OUT, CTRL_TYPE_CLASS, CTRL_RECIPIENT_INTERFACE),
            'bRequest': HID_REQUEST_SET_IDLE,
            # wValue: Duration, ReportID = 0 (all input reports)
            'wValue': (KEYBOARD_IDLE_RATE_TO_SET << 8) | 0x00,
            # wIndex: InterfaceNumber (defined later)
            'wIndex': None,
            'data_or_wLength': 0}
        kwargs_get_protocol_request = {
            'bmRequestType': build_request_type(
                CTRL_IN, CTRL_TYPE_CLASS, CTRL_RECIPIENT_INTERFACE),
            'bRequest': HID_REQUEST_GET_PROTOCOL,
            'wValue': 0x00,
            # wIndex: InterfaceNumber (defined later)
            'wIndex': None,
            'data_or_wLength': 1}
        kwargs_set_protocol_request = {
            'bmRequestType': build_request_type(
                CTRL_OUT, CTRL_TYPE_CLASS, CTRL_RECIPIENT_INTERFACE),
            'bRequest': HID_REQUEST_SET_PROTOCOL,
            'wValue': HID_PROTOCOL_TO_SET,
            # wIndex: InterfaceNumber (defined later)
            'wIndex': None,
            'data_or_wLength': 0}
        mbed_hid_dev = None
        try:
            mbed_hid_dev = retry_fun_call(
                fun=functools.partial(self.get_usb_dev, self.dut_usb_dev_sn),  # pylint: disable=not-callable
                num_retries=20,
                retry_delay=0.05)
        except RetryError as exc:
            self.notify_error(exc)
            return
        hid_dev_type = None
        tested_request_name = None
        try:
            for intf in mbed_hid_dev.get_active_configuration():  # pylint: disable=not-callable
                hid_dev_type = get_usbhid_dev_type(intf)
                if hid_dev_type is None:
                    continue
                try:
                    if mbed_hid_dev.is_kernel_driver_active(intf.bInterfaceNumber):
                        mbed_hid_dev.detach_kernel_driver(intf.bInterfaceNumber)  # pylint: disable=not-callable
                except (NotImplementedError, AttributeError):
                    pass
                if hid_dev_type == 'boot_keyboard':
                    # 4. Set_Idle
                    tested_request_name = 'Set_Idle'
                    kwargs_set_idle_request['wIndex'] = intf.bInterfaceNumber
                    mbed_hid_dev.ctrl_transfer(**kwargs_set_idle_request)  # pylint: disable=not-callable
                    # 3. Get_Idle
                    tested_request_name = 'Get_Idle'
                    kwargs_get_idle_request['wIndex'] = intf.bInterfaceNumber
                    idle_rate = mbed_hid_dev.ctrl_transfer(**kwargs_get_idle_request)  # pylint: disable=not-callable
                    raise_if_different(KEYBOARD_IDLE_RATE_TO_SET, idle_rate, 'Invalid idle rate received. ')
                if hid_dev_type in ('boot_keyboard', 'boot_mouse'):
                    # 6. Set_Protocol
                    tested_request_name = 'Set_Protocol'
                    kwargs_set_protocol_request['wIndex'] = intf.bInterfaceNumber
                    mbed_hid_dev.ctrl_transfer(**kwargs_set_protocol_request)  # pylint: disable=not-callable
                    # 5. Get_Protocol
                    tested_request_name = 'Get_Protocol'
                    kwargs_get_protocol_request['wIndex'] = intf.bInterfaceNumber
                    protocol = mbed_hid_dev.ctrl_transfer(**kwargs_get_protocol_request)  # pylint: disable=not-callable
                    raise_if_different(HID_PROTOCOL_TO_SET, protocol, 'Invalid protocol received. ')
                # 1. Get_Report
                tested_request_name = 'Get_Report'
                kwargs_get_report_request['wIndex'] = intf.bInterfaceNumber
                mbed_hid_dev.ctrl_transfer(**kwargs_get_report_request)  # pylint: disable=not-callable
        except usb.core.USBError as exc:
            self.notify_failure('The {!r} does not support the {!r} HID class request ({}).'
                                .format(hid_dev_type, tested_request_name, exc))
        except RuntimeError as exc:
            self.notify_failure('Set/Get data mismatch for {!r} for the {!r} HID class request ({}).'
                                .format(hid_dev_type, tested_request_name, exc))
        else:
            self.notify_success()

    def raw_loopback(self, report_size):
        """Send every input report back to the device."""
        mbed_hid_path = None
        mbed_hid = hid.device()
        try:
            mbed_hid_path = retry_fun_call(
                fun=functools.partial(self.get_usb_hid_path, self.dut_usb_dev_sn),  # pylint: disable=not-callable
                num_retries=20,
                retry_delay=0.05)
            retry_fun_call(
                fun=functools.partial(mbed_hid.open_path, mbed_hid_path),  # pylint: disable=not-callable
                num_retries=10,
                retry_delay=0.05)
        except RetryError as exc:
            self.notify_error(exc)
            return
        # Notify the device it can send reports now.
        self.send_kv(MSG_KEY_HOST_READY, MSG_VALUE_DUMMY)
        try:
            for _ in range(RAW_IO_REPS):
                # There are no Report ID tags in the Report descriptor.
                # Receiving only the Report Data, Report ID is omitted.
                report_in = mbed_hid.read(report_size)
                report_out = report_in[:]
                # Set the Report ID to 0x00 (not used).
                report_out.insert(0, 0x00)
                mbed_hid.write(report_out)
        except (ValueError, IOError) as exc:
            self.notify_failure('HID Report transfer failed. {}'.format(exc))
        finally:
            mbed_hid.close()

    def setup(self):
        self.register_callback(MSG_KEY_DEVICE_READY, self.cb_device_ready)
        self.register_callback(MSG_KEY_TEST_GET_DESCRIPTOR_HID, self.cb_test_get_hid_desc)
        self.register_callback(MSG_KEY_TEST_GET_DESCRIPTOR_CFG, self.cb_test_get_cfg_desc)
        self.register_callback(MSG_KEY_TEST_REQUESTS, self.cb_test_class_requests)
        self.register_callback(MSG_KEY_TEST_RAW_IO, self.cb_test_raw_io)

    def cb_device_ready(self, key, value, timestamp):
        """Send a unique USB SN to the device.

        DUT uses this SN every time it connects to host as a USB device.
        """
        self.send_kv(MSG_KEY_SERIAL_NUMBER, self.dut_usb_dev_sn)

    def start_bg_task(self, **thread_kwargs):
        """Start a new daemon thread.

        Some callbacks delegate HID dev handling to a background task to
        prevent any delays in the device side assert handling. Only one
        background task is kept running to prevent multiple access
        to the HID device.
        """
        try:
            self.__bg_task.join()
        except (AttributeError, RuntimeError):
            pass
        self.__bg_task = threading.Thread(**thread_kwargs)
        self.__bg_task.daemon = True
        self.__bg_task.start()

    def cb_test_raw_io(self, key, value, timestamp):
        """Receive HID reports and send them back to the device."""
        if not CYTHON_HIDAPI_PRESENT:
            self.send_kv(MSG_KEY_HOST_READY, MSG_VALUE_NOT_SUPPORTED)
            return
        try:
            # The size of input and output reports used in test.
            report_size = int(value)
        except ValueError as exc:
            self.notify_error(exc)
            return
        self.start_bg_task(
            target=self.raw_loopback,
            args=(report_size, ))
