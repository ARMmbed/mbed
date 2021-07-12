# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(fakes)

add_library(mbed-headers-feature-ble INTERFACE)

target_include_directories(mbed-headers-feature-ble
    INTERFACE
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/include
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/include/ble
)
