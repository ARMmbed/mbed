# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-stubs-headers INTERFACE)
add_library(mbed-headers INTERFACE)
add_library(mbed-headers-base INTERFACE)

target_link_libraries(mbed-headers
    INTERFACE
        mbed-headers-base
        mbed-headers-platform
        mbed-headers-drivers
        mbed-headers-hal
        mbed-headers-events
        mbed-headers-rtos
)

target_include_directories(mbed-headers-base
    INTERFACE
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/platform
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/platform/cxxsupport
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/drivers
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/rtos/include
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/rtos
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/sys
)


target_include_directories(mbed-headers
    INTERFACE
        ${mbed-os_SOURCE_DIR}/features
        ${mbed-os_SOURCE_DIR}/features/frameworks
)

target_include_directories(mbed-stubs-headers
    INTERFACE
        .
)

add_library(mbed-stubs INTERFACE)

target_link_libraries(mbed-stubs
    INTERFACE
        mbed-stubs-drivers
        mbed-stubs-events
        mbed-stubs-hal
        mbed-stubs-platform
        mbed-stubs-rtos
)
