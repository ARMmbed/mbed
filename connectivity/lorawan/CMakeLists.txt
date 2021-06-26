# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests/UNITTESTS)
endif()

add_subdirectory(lorastack)
add_subdirectory(system)

target_include_directories(mbed-lorawan
    INTERFACE
        ..
        .
        ./include
        ./include/lorawan
)

target_sources(mbed-lorawan
    INTERFACE
        source/LoRaWANInterface.cpp
        source/LoRaWANStack.cpp
)

target_compile_definitions(mbed-lorawan
    INTERFACE
        MBED_CONF_LORAWAN_PRESENT=1
)

target_link_libraries(mbed-lorawan
    INTERFACE
        mbed-events
        mbed-mbedtls
)
