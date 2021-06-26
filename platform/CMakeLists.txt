# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests/UNITTESTS)
endif()

# List of all optional platform libraries available.
add_library(mbed-psa INTERFACE)

if("EXPERIMENTAL_API" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(FEATURE_EXPERIMENTAL_API)
endif()

add_subdirectory(cxxsupport)
add_subdirectory(mbed-trace)
add_subdirectory(randlib)
add_subdirectory(source)

target_include_directories(mbed-core
    INTERFACE
        include
        include/platform
        include/platform/internal
)

