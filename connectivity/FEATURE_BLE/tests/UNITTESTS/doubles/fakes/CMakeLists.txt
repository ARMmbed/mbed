# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-fakes-ble)

target_include_directories(mbed-fakes-ble
    PUBLIC
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/tests/UNITTESTS/doubles/fakes
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/include
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/include/ble
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/source
    PRIVATE
        ${gtest_SOURCE_DIR}/include
        ${gmock_SOURCE_DIR}/include
)

target_sources(mbed-fakes-ble
    PRIVATE
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/source/gap/AdvertisingDataBuilder.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/source/gap/AdvertisingParameters.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/source/gap/ConnectionParameters.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/source/gatt/DiscoveredCharacteristic.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/source/Gap.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/source/GattClient.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/source/GattServer.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/source/SecurityManager.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/tests/UNITTESTS/doubles/fakes/BLE.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/tests/UNITTESTS/doubles/fakes/source/GattServerImpl_mock.cpp
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/tests/UNITTESTS/doubles/fakes/ble_mocks.h
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/tests/UNITTESTS/doubles/fakes/GapImpl_mock.h
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/tests/UNITTESTS/doubles/fakes/GattClientImpl_mock.h
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/tests/UNITTESTS/doubles/fakes/GattServerImpl_mock.h
        ${mbed-os_SOURCE_DIR}/connectivity/FEATURE_BLE/tests/UNITTESTS/doubles/fakes/SecurityManagerImpl_mock.h
)

target_link_libraries(mbed-fakes-ble
    PRIVATE
        mbed-headers-base
        mbed-headers-platform
        mbed-headers-events
        gcov
)
