# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# ----------------------------------------------
# CMake finder for STMicro's STM32 upload and debugging tools
#
# This module accepts the following components (blame ST for the capitalization, not me):
# STM32CubeProg
# STLINK_gdbserver
#
# This module defines:
# STLINKTools_FOUND - Whether the requested components were found.
# STM32CubeProg_PATH - full path to the STM32 command line programmer (STM32_Programmer_CLI).
# STLINK_gdbserver_PATH - full path to the ST-Link GDB server (ST-LINK_gdbserver).
# STM32CubeProg_COMMAND - Command to run the STM32 command line programmer.
# STLINK_gdbserver_COMMAND - Command to run the ST-Link GDB server.

# first try to locate STM32Cube IDE in its default location
set(STM32CUBE_IDE_LINUX_HINTS "")
if(EXISTS "/opt/st")
    # Linux directory has version number
    file(GLOB STM32CUBE_IDE_LINUX_HINTS LIST_DIRECTORIES TRUE "/opt/st/*")
endif()
find_path(STM32CUBE_IDE_PATH
    NAMES stm32cubeide.ini
    DOC "Path to STM32Cube IDE.  Used to find the ST-Link Tools"
    PATHS
        C:/ST/STM32CubeIDE # default Windows path
        ${STM32CUBE_IDE_LINUX_HINTS} # Linux
        /Applications/STM32CubeIDE.app/Contents/Eclipse # OS X
        )

set(STLINKTools_HINTS "")
if(EXISTS "${STM32CUBE_IDE_PATH}")
    message(STATUS "Located STM32CubeIDE: ${STM32CUBE_IDE_PATH}")

    # find install dirs inside IDE, which also have version numbers
    file(GLOB GDB_SERVER_INSTALL_DIR LIST_DIRECTORIES TRUE "${STM32CUBE_IDE_PATH}/plugins/com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server*/tools/bin")
    if(EXISTS "${GDB_SERVER_INSTALL_DIR}")
        list(APPEND STLINKTools_HINTS ${GDB_SERVER_INSTALL_DIR})
    endif()

    file(GLOB CUBEPROG_INSTALL_DIR LIST_DIRECTORIES TRUE "${STM32CUBE_IDE_PATH}/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer*/tools/bin")
    if(EXISTS "${GDB_SERVER_INSTALL_DIR}")
        list(APPEND STLINKTools_HINTS ${CUBEPROG_INSTALL_DIR})
    endif()
elseif()
    set(FAIL_MESSAGE_ARG FAIL_MESSAGE "Warning: Failed to find STM32CubeIDE, will still look for ST-LINK tools on your PATH.  Recommend setting STM32CUBE_IDE_PATH to the location of STM32CubeIDE.")
endif()
set(STLINKTools_REQUIRED_VARS "")

if("${STLINKTools_FIND_COMPONENTS}" MATCHES "STM32CubeProg")
    find_program(STM32CubeProg_PATH
        NAMES STM32_Programmer_CLI
        HINTS ${STLINKTools_HINTS}
        DOC "Full path to the STM32 command line programmer (STM32_Programmer_CLI)")

    if(STLINKTools_FIND_REQUIRED_STM32CubeProg)
        list(APPEND STLINKTools_REQUIRED_VARS STM32CubeProg_PATH)
    endif()
endif()

if("${STLINKTools_FIND_COMPONENTS}" MATCHES "STLINK_gdbserver")
    find_program(STLINK_gdbserver_PATH
        NAMES ST-LINK_gdbserver
        HINTS ${STLINKTools_HINTS}
        DOC "Full path to the ST-Link GDB server (ST-LINK_gdbserver)")

    if(STLINKTools_FIND_REQUIRED_STLINK_gdbserver)
        list(APPEND STLINKTools_REQUIRED_VARS STLINK_gdbserver_PATH)
    endif()
endif()

# Create COMMAND variables
if(EXISTS "${STM32CubeProg_PATH}")
    set(STM32CubeProg_COMMAND ${STM32CubeProg_PATH})
endif()


# on Linux and Mac the GDB server needs help to find libSTLinkUSBDriver dll which is in a subfolder
if(EXISTS "${STLINK_gdbserver_PATH}")

    # find the "bin/native/xxx" directory, where xxx is related to the current OS name
    get_filename_component(STLINK_gdbserver_DIR ${STLINK_gdbserver_PATH} DIRECTORY)
    file(GLOB STLINK_NATIVE_DIR LIST_DIRECTORIES TRUE "${STLINK_gdbserver_DIR}/native/*")

    if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
        # Linux: set LD_LIBRARY_PATH
        set(STLINK_gdbserver_COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${STLINK_NATIVE_DIR}" ${STLINK_gdbserver_PATH})
    elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
        # OS X: set DYLD_FALLBACK_LIBRARY_PATH
        set(STLINK_gdbserver_COMMAND ${CMAKE_COMMAND} -E env "DYLD_FALLBACK_LIBRARY_PATH=${STLINK_NATIVE_DIR}" ${STLINK_gdbserver_PATH})
    else()
        # Windows -- doesn't need help
        set(STLINK_gdbserver_COMMAND ${STLINK_gdbserver_PATH})
    endif()
endif()

find_package_handle_standard_args(STLINKTools REQUIRED_VARS ${STLINKTools_REQUIRED_VARS})


