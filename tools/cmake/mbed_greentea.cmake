# Copyright (c) 2020-2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

option(MBED_TEST_BAREMETAL OFF)

set(MBED_CONFIG_PATH ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "")

include(${CMAKE_CURRENT_LIST_DIR}/app.cmake)
set(MBED_ROOT ${CMAKE_CURRENT_LIST_DIR}/../.. CACHE INTERNAL "")

# CMake Macro for generalizing CMake configuration across the greentea test suite with configurable parameters
# Macro args:
# TEST_NAME - Test suite name
# TEST_INCLUDE_DIRS - Test suite include directories for the test
# TEST_SOURCES - Test suite sources
# TEST_REQUIRED_LIBS - Test suite required libraries
# TEST_SKIPPED - Reason if suite is skipped
#
# calling the macro:
# if(MBED_TEST_BAREMETAL)
#     set(skip_reason "RTOS required")
# endif()
# mbed_greentea_add_test(
#     TEST_NAME
#         mbed-platform-system-reset
#     TEST_INCLUDE_DIRS
#         mbed_store
#     TEST_SOURCES
#         foo.cpp
#         bar.cpp
#     TEST_REQUIRED_LIBS
#         mbed-kvstore
#         mbed-xyz
#     TEST_SKIPPED
#         ${skip_reason}
# )

macro(mbed_greentea_add_test)
    set(options)
    set(singleValueArgs
        TEST_NAME
        TEST_SKIPPED
    )
    set(multipleValueArgs
        TEST_INCLUDE_DIRS
        TEST_SOURCES
        TEST_REQUIRED_LIBS
    )
    cmake_parse_arguments(MBED_GREENTEA
        "${options}"
        "${singleValueArgs}"
        "${multipleValueArgs}"
        ${ARGN}
    )
    if(NOT "${MBED_GREENTEA_TEST_SKIPPED}" STREQUAL "")
        set(msg "Skipping ${MBED_GREENTEA_TEST_NAME}: ${MBED_GREENTEA_TEST_SKIPPED}")
        message(DEBUG "${msg}")
        add_custom_target(${MBED_GREENTEA_TEST_NAME}
            ALL
            COMMAND ${CMAKE_COMMAND} -E echo ${msg}
        )
    else()

        add_subdirectory(${MBED_ROOT} build)

        add_executable(${MBED_GREENTEA_TEST_NAME})

        # Explicitly enable BUILD_TESTING until CTest is added to the Greentea client
        set(BUILD_TESTING ON)

        target_include_directories(${MBED_GREENTEA_TEST_NAME}
            PRIVATE
                .
                ${MBED_GREENTEA_TEST_INCLUDE_DIRS}
        )

        target_sources(${MBED_GREENTEA_TEST_NAME}
            PRIVATE
                ${MBED_GREENTEA_TEST_SOURCES}
        )

        if(MBED_TEST_BAREMETAL)
            list(APPEND MBED_GREENTEA_TEST_REQUIRED_LIBS mbed-baremetal)
        else()
            list(APPEND MBED_GREENTEA_TEST_REQUIRED_LIBS mbed-os)
        endif()

        list(APPEND MBED_GREENTEA_TEST_REQUIRED_LIBS greentea::client_userio mbed-greentea-io mbed-unity mbed-utest)

        target_link_libraries(${MBED_GREENTEA_TEST_NAME}
            PRIVATE
                ${MBED_GREENTEA_TEST_REQUIRED_LIBS}
        )

        mbed_set_post_build(${MBED_GREENTEA_TEST_NAME})

        option(VERBOSE_BUILD "Have a verbose build process")
        if(VERBOSE_BUILD)
            set(CMAKE_VERBOSE_MAKEFILE ON)
        endif()

    endif() # NOT MBED_GREENTEA_TEST_SKIPPED

endmacro()
