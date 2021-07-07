# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-headers-platform INTERFACE)
target_include_directories(mbed-headers-platform
    INTERFACE
        ${mbed-os_SOURCE_DIR}/platform/include
        ${mbed-os_SOURCE_DIR}/platform/include/platform
        ${mbed-os_SOURCE_DIR}/platform/randlib/include/mbed-client-randlib/
        ${mbed-os_SOURCE_DIR}/platform/randlib/include/
        ${mbed-os_SOURCE_DIR}/platform/mbed-trace/include
)
target_link_libraries(mbed-headers-platform
    INTERFACE
        mbed-headers-cmsis
)

add_library(mbed-stubs-platform)
target_sources(mbed-stubs-platform
    PRIVATE
        mbed_critical_stub.c
        mbed_atomic_stub.c
        mbed_error.c
        mbed_poll_stub.cpp
        mbed_assert_stub.cpp
        mbed_wait_api_stub.cpp
        mbed_retarget_stub.cpp
        FileHandle_stub.cpp
        nvic_wrapper_stub.c
        randLIB_stub.c
        randLIB_stub.cpp
)
target_include_directories(mbed-stubs-platform
    PUBLIC
        .
)
target_link_libraries(mbed-stubs-platform
    PUBLIC
        mbed-headers-base
        mbed-headers-hal
        mbed-headers-platform
)
