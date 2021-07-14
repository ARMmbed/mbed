# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

#file which is invoked by the cmake build system to check if all necessary python packages are installed.

import sys

try:
    __import__(sys.argv[1])
except ImportError:
    exit(1)

exit(0)