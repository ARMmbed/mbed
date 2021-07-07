# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-headers-hal INTERFACE)

target_include_directories(mbed-headers-hal
    INTERFACE
        .
        ${mbed-os_SOURCE_DIR}/hal
        ${mbed-os_SOURCE_DIR}/hal/include
)

add_library(mbed-stubs-hal)

target_compile_definitions(mbed-stubs-hal
    PRIVATE
        DEVICE_PWMOUT
        DEVICE_WATCHDOG
        MBED_WDOG_ASSERT=1
)

target_sources(mbed-stubs-hal
    PRIVATE
        pwmout_api_stub.c
        us_ticker_stub.cpp
        watchdog_api_stub.c
)

target_link_libraries(mbed-stubs-hal
    PRIVATE
        mbed-headers-base
        mbed-headers-hal
        mbed-headers-platform
        mbed-stubs-headers
)
