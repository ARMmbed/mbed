# Copyright (c) 2021 Arm Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-headers-cmsis INTERFACE)

target_include_directories(mbed-headers-cmsis
    INTERFACE
        .
)
