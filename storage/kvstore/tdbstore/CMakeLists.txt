# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests/UNITTESTS)
endif()

target_include_directories(mbed-storage-tdbstore
    INTERFACE
        .
        include
        include/tdbstore
)

target_sources(mbed-storage-tdbstore
    INTERFACE
        source/TDBStore.cpp
)

target_link_libraries(mbed-storage-tdbstore
    INTERFACE
        mbed-storage-kvstore
        mbed-storage-blockdevice
)
