# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(TARGET_NUCLEO_WB15CC EXCLUDE_FROM_ALL)

if(${MBED_TOOLCHAIN} STREQUAL "GCC_ARM")
    set(STARTUP_FILE TOOLCHAIN_GCC_ARM/startup_stm32wb15xx.S)
    set(LINKER_FILE TOOLCHAIN_GCC_ARM/stm32wb15xc.ld)
elseif(${MBED_TOOLCHAIN} STREQUAL "ARM")
    set(STARTUP_FILE TOOLCHAIN_ARM/startup_stm32wb15xx.S)
    set(LINKER_FILE TOOLCHAIN_ARM/stm32wb15xc.sct)
endif()

add_library(mbed-stm32wb15xc INTERFACE)

target_include_directories(mbed-stm32wb15xc
    INTERFACE
        .
)

target_sources(mbed-stm32wb15xc
    INTERFACE
        ${STARTUP_FILE}
        system_clock.c
)

mbed_set_linker_script(mbed-stm32wb15xc ${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_FILE})

target_link_libraries(mbed-stm32wb15xc INTERFACE mbed-stm32wb)
