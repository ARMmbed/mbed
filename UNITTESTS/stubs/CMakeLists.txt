# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-stubs-headers INTERFACE)
add_library(mbed-headers INTERFACE)
add_library(mbed-headers-connectivity INTERFACE)

target_link_libraries(mbed-headers
    INTERFACE
        mbed-headers-platform
        mbed-headers-connectivity
        mbed-headers-drivers
        mbed-headers-hal
        mbed-headers-events
        mbed-headers-rtos
)

target_include_directories(mbed-headers-connectivity
    INTERFACE
        ${mbed-os_SOURCE_DIR}/connectivity
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/include
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/include/ble
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
