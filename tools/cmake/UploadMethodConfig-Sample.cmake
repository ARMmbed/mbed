# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Edit this file to configure the upload methods for your processor.

# See here for documentation about these options: https://github.com/USCRPL/mbed-cmake/wiki/Upload-Methods

# General config options
# -------------------------------------------------------------
set(UPLOAD_METHOD_DEFAULT MBED)

# use a higher numbered port to allow use without root on Linux/Mac
set(GDB_PORT 23331)

# Config options for MBED
# -------------------------------------------------------------

set(MBED_UPLOAD_ENABLED TRUE)
set(MBED_RESET_BAUDRATE 9600)

# Config options for JLINK
# -------------------------------------------------------------

set(JLINK_UPLOAD_ENABLED FALSE)

if(JLINK_UPLOAD_ENABLED)
    set(JLINK_CPU_NAME STM32F429ZI)
    set(JLINK_JTAG_SPEED 4000)
endif()

# Config options for PYOCD
# -------------------------------------------------------------

set(PYOCD_UPLOAD_ENABLED TRUE)

if(PYOCD_UPLOAD_ENABLED)
    set(PYOCD_TARGET_NAME stm32f429xi)
    set(PYOCD_CLOCK_SPEED 4000k)
endif()

# Config options for OPENOCD
# -------------------------------------------------------------

set(OPENOCD_UPLOAD_ENABLED FALSE)

if(OPENOCD_UPLOAD_ENABLED)
    set(OPENOCD_CHIP_CONFIG_COMMANDS
        -f ${CMAKE_CURRENT_LIST_DIR}/stm32f429-disco.cfg
        -c "gdb_memory_map disable" # prevents OpenOCD crash on GDB connect
        )
endif()

# Config options for STM32Cube
# -------------------------------------------------------------

set(STM32CUBE_UPLOAD_ENABLED TRUE)

if(STM32CUBE_UPLOAD_ENABLED)

    set(STM32CUBE_CONNECT_COMMAND -c port=SWD reset=HWrst)
    set(STM32CUBE_GDBSERVER_ARGS --swd)

endif()
