# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests/UNITTESTS)
else()

add_library(mbed-events INTERFACE)

target_include_directories(mbed-events
    INTERFACE
        .
        ./include
        ./include/events
        ./include/events/internal
)

target_sources(mbed-events
    INTERFACE
        source/EventQueue.cpp
        source/equeue.c
        source/equeue_mbed.cpp
        source/equeue_posix.c
        source/mbed_shared_queues.cpp
)

target_compile_definitions(mbed-events
    INTERFACE
        MBED_CONF_EVENTS_PRESENT=1
)
endif()
