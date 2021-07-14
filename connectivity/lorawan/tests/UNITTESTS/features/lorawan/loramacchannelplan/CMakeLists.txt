# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

set(TEST_NAME lorawan-loramac-channel-plan-unittest)

add_executable(${TEST_NAME})

target_compile_definitions(${TEST_NAME}
    PRIVATE
        MBED_CONF_LORA_TX_MAX_SIZE=255
)

target_sources(${TEST_NAME}
    PRIVATE
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/lorastack/mac/LoRaMacChannelPlan.cpp
        Test_LoRaMacChannelPlan.cpp
)

target_link_libraries(${TEST_NAME}
    PRIVATE
        mbed-headers-base
        mbed-headers-events
        mbed-headers-hal
        mbed-headers-platform
        mbed-headers-lorawan
        mbed-stubs
        mbed-stubs-headers
        mbed-stubs-lorawan
        gmock_main
)

add_test(NAME "${TEST_NAME}" COMMAND ${TEST_NAME})

set_tests_properties(${TEST_NAME} PROPERTIES LABELS "lorawan")
