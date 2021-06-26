# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-nucleo-wb15cc INTERFACE)

target_sources(mbed-nucleo-wb15cc
    INTERFACE
        PeripheralPins.c
)

target_include_directories(mbed-nucleo-wb15cc
    INTERFACE
        .
)

target_link_libraries(mbed-nucleo-wb15cc INTERFACE mbed-stm32wb15xc)
