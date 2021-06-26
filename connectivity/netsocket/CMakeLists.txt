# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests/UNITTESTS)
endif()

# TODO CMake: Perhaps move this/these file(s) into connectivity/drivers/cellular
target_sources(mbed-cellular
    INTERFACE
        source/CellularNonIPSocket.cpp
)

target_include_directories(mbed-netsocket
    INTERFACE
        .
        ./include
        ./include/netsocket
)

target_sources(mbed-netsocket
    INTERFACE
        source/DTLSSocket.cpp
        source/DTLSSocketWrapper.cpp
        source/EMACInterface.cpp
        source/EthernetInterface.cpp
        source/ICMPSocket.cpp
        source/InternetDatagramSocket.cpp
        source/InternetSocket.cpp
        source/L3IPInterface.cpp
        source/NetStackMemoryManager.cpp
        source/NetworkInterface.cpp
        source/NetworkInterfaceDefaults.cpp
        source/NetworkStack.cpp
        source/PPPInterface.cpp
        source/SocketAddress.cpp
        source/SocketStats.cpp
        source/TCPSocket.cpp
        source/TLSSocket.cpp
        source/TLSSocketWrapper.cpp
        source/UDPSocket.cpp
        source/WiFiAccessPoint.cpp
        source/nsapi_dns.cpp
        source/nsapi_ppp.cpp
)

target_compile_definitions(mbed-netsocket
    INTERFACE
        MBED_CONF_NSAPI_PRESENT=1
)

target_link_libraries(mbed-netsocket
    INTERFACE
        mbed-mbedtls
        mbed-lwipstack
        mbed-events
)

if("DEVICE_EMAC=1" IN_LIST MBED_TARGET_DEFINITIONS)
    target_link_libraries(mbed-netsocket
        INTERFACE
            mbed-emac
    )
endif()

target_link_libraries(mbed-netsocket
    INTERFACE
        mbed-cellular
        mbed-nanostack-libservice
)
