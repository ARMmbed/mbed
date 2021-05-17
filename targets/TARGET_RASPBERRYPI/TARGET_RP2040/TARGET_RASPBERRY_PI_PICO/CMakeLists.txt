# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

set(LINKER_FILE ../TOOLCHAIN_GCC_ARM/memmap_default.ld)

add_library(mbed-raspberry-pi-pico INTERFACE)

target_sources(mbed-raspberry-pi-pico
    INTERFACE
        board.c
        bs2_default_padded_checksummed.S
)

target_include_directories(mbed-raspberry-pi-pico
    INTERFACE
        .
)

mbed_set_linker_script(mbed-raspberry-pi-pico ${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_FILE})

target_link_libraries(mbed-raspberry-pi-pico INTERFACE mbed-rp2040)
