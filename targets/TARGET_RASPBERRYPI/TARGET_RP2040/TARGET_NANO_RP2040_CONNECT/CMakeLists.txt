# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

set(LINKER_FILE ../TOOLCHAIN_GCC_ARM/memmap_default.ld)

add_library(mbed-nano-rp2040-connect INTERFACE)

target_sources(mbed-nano-rp2040-connect
    INTERFACE
        board.c
        bs2_default_padded_checksummed.S
)

target_include_directories(mbed-nano-rp2040-connect
    INTERFACE
        .
)

mbed_set_linker_script(mbed-nano-rp2040-connect ${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_FILE})

target_link_libraries(mbed-nano-rp2040-connect INTERFACE mbed-rp2040)
