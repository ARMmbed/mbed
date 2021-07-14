# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# ----------------------------------------------
# CMake finder for SEGGER's J-Link upload tools
#
#
# This module defines:
# JLINK - full path to jlink flash executable
# JLINK_GDBSERVER - full path to JLink command line GDB server
# JLINK_FOUND - whether or not the JLink executable was found

set(JLINK_EXE_NAME JLinkExe)
set(JLINK_PATH "")
set(GDBSERVER_EXE_NAME JLinkGDBServer)

# try to figure out where JLink may be installed.
if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")

    # on Windows, the default install location is in Program Files, under a folder called SEGGER then another folder with JLink then the version number.
    file(GLOB JLINK_PATH "C:/Program Files*/SEGGER/Jlink*")

    # if we found multiple paths, check the one with the highest version number first
    list(SORT JLINK_PATH)
    list(REVERSE JLINK_PATH)

    # on Windows, it's jlink.exe
    set(JLINK_EXE_NAME JLink)

    # on Windows, they add a CL suffix to differentiate against the GUI version
    set(GDBSERVER_EXE_NAME JLinkGDBServerCL)
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")

    # symlinks are supposed to get created here
    set(JLINK_PATH /usr/local/bin)
endif()

find_program(JLINK NAMES ${JLINK_EXE_NAME} PATHS ${JLINK_PATH} DOC "Path to the JLink flash executable")
find_program(JLINK_GDBSERVER NAMES ${GDBSERVER_EXE_NAME} PATHS ${JLINK_PATH} DOC "Path to the JLink GDB server")

find_package_handle_standard_args(JLINK FOUND_VAR JLINK_FOUND REQUIRED_VARS JLINK JLINK_GDBSERVER)


