# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

set(TEST_NAME netsocket-dtls-socket-wrapper-unittest)

add_executable(${TEST_NAME})

target_compile_definitions(${TEST_NAME}
    PRIVATE
        MBED_CONF_NSAPI_DNS_ADDRESSES_LIMIT=10
)

target_sources(${TEST_NAME}
    PRIVATE
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/InternetSocket.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/InternetDatagramSocket.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/UDPSocket.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/DTLSSocketWrapper.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/TLSSocketWrapper.cpp
        test_DTLSSocketWrapper.cpp
)

target_link_libraries(${TEST_NAME}
    PRIVATE
        mbed-headers-events
        mbed-headers-drivers
        mbed-headers-rtos
        mbed-headers-nanostack-libservice
        mbed-headers-mbedtls
        mbed-headers-netsocket
        mbed-stubs
        mbed-stubs-headers
        mbed-stubs-mbedtls
        mbed-stubs-netsocket
        gmock_main
)


set(MBEDTLS_USER_CONFIG_FILE_PATH "\"${mbed-os_SOURCE_DIR}/connectivity/netsocket/tests/UNITTESTS/netsocket/DTLSSocketWrapper/dtls_test_config.h\"")
set_source_files_properties(${mbed-os_SOURCE_DIR}/connectivity/netsocket/tests/UNITTESTS/netsocket/DTLSSocketWrapper/test_DTLSSocketWrapper.cpp PROPERTIES COMPILE_DEFINITIONS MBEDTLS_USER_CONFIG_FILE=${MBEDTLS_USER_CONFIG_FILE_PATH})
set_source_files_properties(${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/DTLSSocketWrapper.cpp PROPERTIES COMPILE_DEFINITIONS MBEDTLS_USER_CONFIG_FILE=${MBEDTLS_USER_CONFIG_FILE_PATH})
set_source_files_properties(${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/TLSSocketWrapper.cpp PROPERTIES COMPILE_DEFINITIONS MBEDTLS_USER_CONFIG_FILE=${MBEDTLS_USER_CONFIG_FILE_PATH})

add_test(NAME "${TEST_NAME}" COMMAND ${TEST_NAME})

set_tests_properties(${TEST_NAME} PROPERTIES LABELS "netsocket")
