# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

target_include_directories(mbed-storage-spinand
    INTERFACE
        include
        include/SPINAND
)

target_sources(mbed-storage-spinand
    INTERFACE
        source/SPINANDBlockDevice.cpp
)
