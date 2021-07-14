# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

set(TEST_NAME lorawan-loramac-unittest)

add_executable(${TEST_NAME})

target_compile_definitions(${TEST_NAME}
    PRIVATE
        MBED_CONF_LORA_ADR_ON=true
        MBED_CONF_LORA_PUBLIC_NETWORK=true
        MBED_CONF_LORA_NB_TRIALS=2
        MBED_CONF_LORA_DOWNLINK_PREAMBLE_LENGTH=5
        MBED_CONF_LORA_DUTY_CYCLE_ON=true
        MBED_CONF_LORA_MAX_SYS_RX_ERROR=10
        MBED_CONF_LORA_TX_MAX_SIZE=255
        MBED_CONF_LORA_DEVICE_ADDRESS=0x00000000
)

target_compile_options(${TEST_NAME}
    PRIVATE
        "-DMBED_CONF_LORA_NWKSKEY={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}"
        "-DMBED_CONF_LORA_NWKSKEY={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}"
        "-DMBED_CONF_LORA_APPSKEY={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}"
        "-DMBED_CONF_LORA_APPSKEY={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}"
)

target_sources(${TEST_NAME}
    PRIVATE
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/lorastack/mac/LoRaMac.cpp
        Test_LoRaMac.cpp
)

target_link_libraries(${TEST_NAME}
    PRIVATE
        mbed-headers-base
        mbed-headers-events
        mbed-headers-hal
        mbed-headers-platform
        mbed-headers-connectivity
        mbed-headers-mbedtls
        mbed-headers-lorawan
        mbed-stubs
        mbed-stubs-headers
        mbed-stubs-lorawan
        gmock_main
)

add_test(NAME "${TEST_NAME}" COMMAND ${TEST_NAME})

set_tests_properties(${TEST_NAME} PROPERTIES LABELS "lorawan")
