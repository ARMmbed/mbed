# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

set(TEST_NAME atcmdparser-unittest)

add_executable(${TEST_NAME})

target_sources(${TEST_NAME}
    PRIVATE
        ${mbed-os_SOURCE_DIR}/platform/source/ATCmdParser.cpp
        test_ATCmdParser.cpp
)

target_link_libraries(${TEST_NAME}
    PRIVATE
        mbed-headers
        mbed-stubs-headers
        mbed-stubs-platform
        gmock_main
)

add_test(NAME "${TEST_NAME}" COMMAND ${TEST_NAME})

set_tests_properties(${TEST_NAME} PROPERTIES LABELS "platform")
