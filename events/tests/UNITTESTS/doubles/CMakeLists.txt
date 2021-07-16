# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(fakes)

add_library(mbed-headers-events INTERFACE)

target_include_directories(mbed-headers-events
    INTERFACE
        ${mbed-os_SOURCE_DIR}/events/tests/UNITTESTS/target_h
        ${mbed-os_SOURCE_DIR}/events/tests/UNITTESTS/target_h/equeue
        ${mbed-os_SOURCE_DIR}/events/include
        ${mbed-os_SOURCE_DIR}/events/include/events
        ${mbed-os_SOURCE_DIR}/events/include/events/internal
)

add_library(mbed-stubs-events)

target_sources(mbed-stubs-events
    PRIVATE
        equeue_stub.c
        EqueuePosix_stub.c
        EventFlags_stub.cpp
        EventQueue_stub.cpp
        mbed_shared_queues_stub.cpp
)

target_include_directories(mbed-stubs-events
    PUBLIC
        .
)

target_link_libraries(mbed-stubs-events
    PRIVATE
        mbed-headers-drivers
        mbed-headers-hal
        mbed-headers-events
        mbed-headers-rtos
        mbed-headers-platform
)
