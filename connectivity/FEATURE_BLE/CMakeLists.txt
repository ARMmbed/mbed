# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests/UNITTESTS)
endif()

add_subdirectory(libraries)
add_subdirectory(source)

target_include_directories(mbed-ble
    INTERFACE
        .
        include
        include/ble
        include/ble/common
        include/ble/common/ble
        include/ble/common/ble/gap
        include/ble/compatibility
        include/ble/compatibility/ble
        include/ble/driver
        include/ble/gap
        include/ble/gatt
        include/ble/services
        source
)

target_compile_definitions(mbed-ble
    INTERFACE
        MBED_CONF_BLE_PRESENT=1
)
