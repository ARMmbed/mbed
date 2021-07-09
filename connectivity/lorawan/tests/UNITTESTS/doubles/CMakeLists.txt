# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-headers-lorawan INTERFACE)

target_include_directories(mbed-headers-lorawan
    INTERFACE
        ${mbed-os_SOURCE_DIR}/connectivity
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/include/lorawan
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/lorastack
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/lorastack/mac
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/lorastack/phy
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/system
)

add_library(mbed-stubs-lorawan)

target_compile_definitions(mbed-stubs-lorawan
    PRIVATE
        MBED_CONF_LORA_TX_MAX_SIZE=255
)

target_include_directories(mbed-stubs-lorawan
    PUBLIC
        .
)

target_sources(mbed-stubs-lorawan
    PRIVATE
        LoRaMacChannelPlan_stub.cpp
        LoRaMacCommand_stub.cpp
        LoRaMacCrypto_stub.cpp
        LoRaMac_stub.cpp
        LoRaPHYEU868_stub.cpp
        LoRaPHY_stub.cpp
        LoRaWANStack_stub.cpp
        LoRaWANTimer_stub.cpp
)

target_link_libraries(mbed-stubs-lorawan
    PRIVATE
        mbed-headers-base
        mbed-headers-events
        mbed-headers-hal
        mbed-stubs-headers
        mbed-headers-platform
        mbed-headers-lorawan
        mbed-headers-mbedtls
        gtest
)
