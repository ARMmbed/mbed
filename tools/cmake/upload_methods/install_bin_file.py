#!/usr/bin/env python3

# Bin file installer that uses mbed tools to find the board, install the file, and reset.
# Code extracted from mbed-cli.

# usage:

import os
import ctypes
import sys

from mbed_os_tools.test.host_tests_toolbox import flash_dev, reset_dev
import mbed_os_tools.detect


def get_detected_targets():
    targets = []
    oldError = None
    if os.name == 'nt':
        oldError = ctypes.windll.kernel32.SetErrorMode(1) # Disable Windows error box temporarily. note that SEM_FAILCRITICALERRORS = 1
    mbeds = mbed_os_tools.detect.create()
    detect_muts_list = mbeds.list_mbeds()
    if os.name == 'nt':
        ctypes.windll.kernel32.SetErrorMode(oldError)

    for mut in detect_muts_list:
        targets.append({
            'id': mut['target_id'], 'name': mut['platform_name'],
            'mount': mut['mount_point'], 'serial': mut['serial_port'],
            'uid': mut['target_id_usb_id']
        })

    return targets

def error(lines, code=-1):
    sys.stderr.write("[install-bin-file] ERROR: %s\n" % (lines.pop(0),))
    for line in lines:
        sys.stderr.write("       %s\n" % (line,))
    sys.stderr.write("---\n")
    sys.exit(code)

if len(sys.argv) < 3:
    print("Error: Usage: python " + sys.argv[0] + " <path to bin file> <Mbed OS target name> <serial baudrate for sending break> [Target UID for distinguishing multiple targets]")
    exit(1)

bin_file = sys.argv[1]
target_name = sys.argv[2]
serial_baud = int(sys.argv[3])
if len(sys.argv) > 4:
    target_uid = sys.argv[4]
else:
    target_uid = None

all_connected = []

# Convert Dual Core target name to mbedls target name
if target_name.upper().endswith('_CM4') or target_name.upper().endswith('_CM7'):
    target_name = target_name[:-4]
    print('Target to detect: %s' % (target_name,))
targets = get_detected_targets()
if targets:
    for _target in targets:
        if _target['name'] is None:
            continue
        elif _target['name'].upper() == target_name.upper():
            if target_uid is None or _target['uid'] == target_uid:
                all_connected.append(_target)

if len(all_connected) == 0:
    error_lines = ["The target board you compiled for is not connected to your system.",
                   "Please reconnect it and retry the last command."]
    if target_uid is None:
        error_lines.append("(Searched for any %s board.)" % (target_name,))
    else:
        error_lines.append("(Searched for a %s board with UID %s.)" % (target_name, target_uid))
    error(error_lines, 2)

elif len(all_connected) > 1:
    error_lines = ["There are multiple of the targeted board connected to the system.  Which do you wish to flash?"]
    for target in all_connected:
        error_lines.append("Board: %s, Mount Point: %s, UID: %s" % (target['name'], target['mount'], target['uid']))
    error_lines.append("Please set MBED_TARGET_UID to the UID of the board you wish to flash.")
    error(error_lines, 5)

connected = all_connected[0]
# apply new firmware
if not os.path.exists(bin_file):
    error("Build program file (firmware) not found \"%s\"" % bin_file, 1)
if not flash_dev(connected['mount'], bin_file, program_cycle_s=4):
    error("Unable to flash the target board connected to your system.", 1)

# reset board
reset_dev(port=connected["serial"], disk=connected['mount'], baudrate=serial_baud)
