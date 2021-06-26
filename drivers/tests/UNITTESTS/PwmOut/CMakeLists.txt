# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

set(TEST_NAME pwmout-unittest)

add_executable(${TEST_NAME})

target_compile_definitions(${TEST_NAME} 
    PRIVATE
        DEVICE_PWMOUT
)

target_sources(${TEST_NAME}
    PRIVATE
        ${mbed-os_SOURCE_DIR}/drivers/source/PwmOut.cpp
        test_pwmout.cpp
)

target_link_libraries(${TEST_NAME}
    PRIVATE
        mbed-headers-base
        mbed-headers-platform
        mbed-headers-hal
        mbed-headers-drivers
        mbed-stubs-hal
        mbed-stubs-platform
        gmock_main
)

add_test(NAME "${TEST_NAME}" COMMAND ${TEST_NAME})

set_tests_properties(${TEST_NAME} PROPERTIES LABELS "drivers")
