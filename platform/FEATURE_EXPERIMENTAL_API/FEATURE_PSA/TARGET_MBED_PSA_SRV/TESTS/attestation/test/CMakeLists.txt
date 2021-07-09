# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.19.0 FATAL_ERROR)

set(MBED_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../ CACHE INTERNAL "")
set(TEST_TARGET mbed-platform-psa-attestation)

include(${MBED_PATH}/tools/cmake/mbed_greentea.cmake)

project(${TEST_TARGET})

if(MBED_TEST_BAREMETAL)
    set(TEST_SKIPPED "RTOS required")
elseif(NOT ("PSA" IN_LIST MBED_TARGET_LABELS AND "EXPERIMENTAL_API" IN_LIST MBED_TARGET_LABELS))
    set(TEST_SKIPPED "Experiemental API and PSA required")
endif()

mbed_greentea_add_test(
    TEST_NAME 
        ${TEST_TARGET}
    TEST_SOURCES
        main.cpp
    TEST_SKIPPED
        ${TEST_SKIPPED}
)
