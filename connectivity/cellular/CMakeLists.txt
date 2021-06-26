# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests/UNITTESTS)
endif()

add_subdirectory(source/framework)

target_include_directories(mbed-cellular
    INTERFACE
        .
        ./include
        ./include/cellular
        ./include/cellular/framework
        ./include/cellular/framework/API
        ./include/cellular/framework/AT
        ./include/cellular/framework/common
        ./include/cellular/framework/device
)

target_compile_definitions(mbed-cellular
    INTERFACE
        MBED_CONF_CELLULAR_PRESENT=1
)

target_link_libraries(mbed-cellular
    INTERFACE
        mbed-netsocket
        mbed-core
)
