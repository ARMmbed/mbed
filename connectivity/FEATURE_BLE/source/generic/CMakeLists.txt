# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

target_include_directories(mbed-ble
    INTERFACE
        .
)

target_sources(mbed-ble
    INTERFACE
        FileSecurityDb.cpp
        GapImpl.cpp
        GattClientImpl.cpp
        KVStoreSecurityDb.cpp
        MemorySecurityDb.cpp
        PrivateAddressController.cpp
        SecurityDb.cpp
        SecurityManagerImpl.cpp
)

target_link_libraries(mbed-ble
    INTERFACE
        mbed-storage-kvstore
        mbed-storage-tdbstore
        mbed-storage-kv-global-api
)
