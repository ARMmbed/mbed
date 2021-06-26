# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-fakes-event-queue)

target_sources(mbed-fakes-event-queue
    PRIVATE
        events/EventQueue.cpp
)

target_include_directories(mbed-fakes-event-queue
    PUBLIC
        .        
)

target_link_libraries(mbed-fakes-event-queue
    PRIVATE
        mbed-headers
        gcov
)
