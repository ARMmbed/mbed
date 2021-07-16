# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-headers-drivers INTERFACE)

target_include_directories(mbed-headers-drivers
    INTERFACE
        .
        ${mbed-os_SOURCE_DIR}/drivers
        ${mbed-os_SOURCE_DIR}/drivers/include
        ${mbed-os_SOURCE_DIR}/drivers/include/drivers
)

add_library(mbed-stubs-drivers)

target_sources(mbed-stubs-drivers
    PRIVATE
        BufferedSerial_stub.cpp
        SerialBase_stub.cpp
)

target_compile_definitions(mbed-stubs-drivers
    PRIVATE
        DEVICE_SERIAL=1
        DEVICE_INTERRUPTIN=1
        MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE=115200
)

target_link_libraries(mbed-stubs-drivers
    PRIVATE
        mbed-headers-hal
        mbed-headers-drivers
        mbed-headers-platform
)
