# Copyright (c) 2020-2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(TARGET_RP2040 EXCLUDE_FROM_ALL)

add_library(mbed-raspberrypi INTERFACE)

target_include_directories(mbed-raspberrypi
    INTERFACE
        .
)

target_sources(mbed-raspberrypi
    INTERFACE
    	.
)

target_link_libraries(mbed-raspberrypi INTERFACE mbed-cmsis-cortex-m)
