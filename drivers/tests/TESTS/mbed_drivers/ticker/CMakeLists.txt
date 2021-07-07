# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

include(mbed_greentea)

mbed_greentea_add_test(
    TEST_NAME
        mbed-drivers-ticker
    TEST_SOURCES
        main.cpp
    HOST_TESTS_DIR
        "${CMAKE_CURRENT_LIST_DIR}/../../host_tests"
)
