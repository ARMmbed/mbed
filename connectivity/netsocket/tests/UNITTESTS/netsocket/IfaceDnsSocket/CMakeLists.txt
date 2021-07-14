# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

set(TEST_NAME netsocket-iface-dns-socket-unittest)

add_executable(${TEST_NAME})

target_compile_definitions(${TEST_NAME}
    PRIVATE
        MBED_CONF_NSAPI_DNS_ADDRESSES_LIMIT=10
        DEVICE_EMAC
        MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE=ETHERNET
        MBED_CONF_NSAPI_DNS_RESPONSE_WAIT_TIME=10000
        MBED_CONF_NSAPI_DNS_RETRIES=1
        MBED_CONF_NSAPI_DNS_TOTAL_ATTEMPTS=10
        MBED_CONF_NSAPI_DNS_CACHE_SIZE=5
)

target_sources(${TEST_NAME}
    PRIVATE
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/SocketAddress.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/NetworkInterface.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/NetworkStack.cpp #nsapi_create_stack
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/InternetSocket.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/InternetDatagramSocket.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/UDPSocket.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/EthernetInterface.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/EMACInterface.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/source/nsapi_dns.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/libraries/nanostack-libservice/source/libip4string/ip4tos.c
        ${mbed-os_SOURCE_DIR}/connectivity/libraries/nanostack-libservice/source/libip6string/ip6tos.c
        ${mbed-os_SOURCE_DIR}/connectivity/libraries/nanostack-libservice/source/libip4string/stoip4.c
        ${mbed-os_SOURCE_DIR}/connectivity/libraries/nanostack-libservice/source/libip6string/stoip6.c
        ${mbed-os_SOURCE_DIR}/connectivity/libraries/nanostack-libservice/source/libBits/common_functions.c
        ${mbed-os_SOURCE_DIR}/connectivity/libraries/nanostack-libservice/source/libList/ns_list.c
        moduletest.cpp
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
        mbed-stubs-netsocket
        gmock_main
)

add_test(NAME "${TEST_NAME}" COMMAND ${TEST_NAME})

set_tests_properties(${TEST_NAME} PROPERTIES LABELS "netsocket")
