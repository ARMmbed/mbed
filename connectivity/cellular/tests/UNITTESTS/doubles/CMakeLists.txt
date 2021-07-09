# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-headers-cellular INTERFACE)

target_include_directories(mbed-headers-cellular
    INTERFACE
        ${mbed-os_SOURCE_DIR}/connectivity/cellular/include/cellular/framework/API
        ${mbed-os_SOURCE_DIR}/connectivity/cellular/include/cellular/framework/AT
        ${mbed-os_SOURCE_DIR}/connectivity/cellular/include/cellular/framework/device
        ${mbed-os_SOURCE_DIR}/connectivity/cellular/include/cellular/framework
        ${mbed-os_SOURCE_DIR}/connectivity/cellular/include/cellular/framework/common        
)

add_library(mbed-stubs-cellular)

target_compile_definitions(mbed-stubs-cellular
    PRIVATE
        DEVICE_SERIAL=1
        DEVICE_INTERRUPTIN=1
        MBED_CONF_CELLULAR_USE_SMS=1
        MBED_CONF_NSAPI_DEFAULT_CELLULAR_APN=NULL
        MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE=115200
        MBED_CONF_CELLULAR_AT_HANDLER_BUFFER_SIZE=32
        MDMTXD=NC
        MDMRXD=NC
)

target_include_directories(mbed-stubs-cellular
    PUBLIC
        .
)

target_sources(mbed-stubs-cellular
    PRIVATE
        AT_CellularContext_stub.cpp
        AT_CellularDevice_stub.cpp
        AT_CellularInformation_stub.cpp
        AT_CellularNetwork_stub.cpp
        AT_CellularSMS_stub.cpp
        AT_CellularStack_stub.cpp
        AT_ControlPlane_netif_stub.cpp
        ATHandler_stub.cpp
        CellularContext_stub.cpp
        CellularDevice_stub.cpp
        CellularStateMachine_stub.cpp
        CellularUtil_stub.cpp        
)

target_link_libraries(mbed-stubs-cellular
    PRIVATE
        mbed-headers-base
        mbed-stubs-platform
        mbed-headers-rtos
        mbed-headers-drivers
        mbed-headers-hal
        mbed-headers-events
        mbed-headers-netsocket
        mbed-headers-cellular
        mbed-stubs-netsocket
        mbed-stubs-headers
        gtest
)
