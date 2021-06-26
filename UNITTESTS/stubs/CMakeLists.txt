# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-stubs-headers INTERFACE)
add_library(mbed-headers INTERFACE)
add_library(mbed-headers-base INTERFACE)
add_library(mbed-headers-platform INTERFACE)
add_library(mbed-headers-connectivity INTERFACE)
add_library(mbed-headers-storage INTERFACE)
add_library(mbed-headers-hal INTERFACE)
add_library(mbed-headers-events INTERFACE)
add_library(mbed-headers-rtos INTERFACE)

target_link_libraries(mbed-headers
    INTERFACE
        mbed-headers-base
        mbed-headers-platform
        mbed-headers-connectivity
        mbed-headers-storage
        mbed-headers-drivers
        mbed-headers-hal
        mbed-headers-events
        mbed-headers-rtos
)

target_include_directories(mbed-headers-platform
    INTERFACE
        ${mbed-os_SOURCE_DIR}/platform/include
        ${mbed-os_SOURCE_DIR}/platform/include/platform
        ${mbed-os_SOURCE_DIR}/platform/randlib/include/mbed-client-randlib/
        ${mbed-os_SOURCE_DIR}/platform/randlib/include/
        ${mbed-os_SOURCE_DIR}/platform/mbed-trace/include
)

target_include_directories(mbed-headers-base
    INTERFACE
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/platform
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/platform/cxxsupport
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/drivers
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/rtos/include
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/rtos
        ${mbed-os_SOURCE_DIR}/UNITTESTS/target_h/sys
)

target_include_directories(mbed-headers-storage
    INTERFACE
        ${mbed-os_SOURCE_DIR}/storage/filesystem/fat/include
        ${mbed-os_SOURCE_DIR}/storage/filesystem/fat/ChaN
        ${mbed-os_SOURCE_DIR}/storage/filesystem/littlefs
        ${mbed-os_SOURCE_DIR}/storage/filesystem/littlefs/include
        ${mbed-os_SOURCE_DIR}/storage/filesystem/littlefsv2/littlefs
        ${mbed-os_SOURCE_DIR}/storage/filesystem/littlefsv2/littlefs/bd
        ${mbed-os_SOURCE_DIR}/storage/filesystem/littlefs/littlefs
        ${mbed-os_SOURCE_DIR}/storage/blockdevice/include
        ${mbed-os_SOURCE_DIR}/storage/filesystem/include
        ${mbed-os_SOURCE_DIR}/storage/kvstore/include
        ${mbed-os_SOURCE_DIR}/storage/kvstore/kv_config
        ${mbed-os_SOURCE_DIR}/storage/kvstore/kv_config/include
        ${mbed-os_SOURCE_DIR}/storage/kvstore/tdbstore/include
        ${mbed-os_SOURCE_DIR}/storage/kvstore/filesystemstore/include
        ${mbed-os_SOURCE_DIR}/storage/kvstore/kvstore_global_api/include
        ${mbed-os_SOURCE_DIR}/storage/blockdevice/include/blockdevice
)

target_include_directories(mbed-headers-connectivity
    INTERFACE
        ${mbed-os_SOURCE_DIR}/connectivity/libraries/nanostack-libservice
        ${mbed-os_SOURCE_DIR}/connectivity/libraries/nanostack-libservice/mbed-client-libservice
        ${mbed-os_SOURCE_DIR}/connectivity/netsocket/include
        ${mbed-os_SOURCE_DIR}/connectivity/cellular/include/cellular/framework/API
        ${mbed-os_SOURCE_DIR}/connectivity/cellular/include/cellular/framework/AT
        ${mbed-os_SOURCE_DIR}/connectivity/cellular/include/cellular/framework/device
        ${mbed-os_SOURCE_DIR}/connectivity/cellular/include/cellular/framework
        ${mbed-os_SOURCE_DIR}/connectivity/cellular/include/cellular/framework/common
        ${mbed-os_SOURCE_DIR}/connectivity
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/include/lorawan
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/lorastack
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/lorastack/mac
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/lorastack/phy
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan
        ${mbed-os_SOURCE_DIR}/connectivity/lorawan/system
        ${mbed-os_SOURCE_DIR}/connectivity/mbedtls
        ${mbed-os_SOURCE_DIR}/connectivity/mbedtls/include
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/include
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/include/ble
)

target_include_directories(mbed-headers-events
    INTERFACE
        ${mbed-os_SOURCE_DIR}/events/tests/UNITTESTS/target_h
        ${mbed-os_SOURCE_DIR}/events/tests/UNITTESTS/target_h/equeue
        ${mbed-os_SOURCE_DIR}/events/include
        ${mbed-os_SOURCE_DIR}/events/include/events
        ${mbed-os_SOURCE_DIR}/events/include/events/internal
)

target_include_directories(mbed-headers-hal
    INTERFACE
        ${mbed-os_SOURCE_DIR}/hal
        ${mbed-os_SOURCE_DIR}/hal/include
)

target_include_directories(mbed-headers-rtos
    INTERFACE
        ${mbed-os_SOURCE_DIR}/rtos/include
        ${mbed-os_SOURCE_DIR}/rtos/include/rtos
)

target_include_directories(mbed-headers
    INTERFACE
        ${mbed-os_SOURCE_DIR}/features
        ${mbed-os_SOURCE_DIR}/features/frameworks
)

target_include_directories(mbed-stubs-headers
    INTERFACE
        .
        ${mbed-os_SOURCE_DIR}/connectivity/nanostack/coap-service/test/coap-service/unittest/stub
)

add_subdirectory(connectivity)
add_subdirectory(events)
add_subdirectory(hal)
add_subdirectory(platform)
add_subdirectory(rtos)
add_subdirectory(storage)

add_library(mbed-stubs INTERFACE)

target_link_libraries(mbed-stubs
    INTERFACE
        mbed-stubs-connectivity
        mbed-stubs-drivers
        mbed-stubs-events
        mbed-stubs-hal
        mbed-stubs-platform
        mbed-stubs-rtos
        mbed-stubs-storage
)
