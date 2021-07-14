# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Add our CMake list files to CMake default module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

find_program(CCACHE "ccache")
if(CCACHE)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
endif()

include(${MBED_CONFIG_PATH}/mbed_config.cmake)
include(mbed_set_post_build)

# Load toolchain file
if(NOT CMAKE_TOOLCHAIN_FILE OR MBED_TOOLCHAIN_FILE_USED)
    set(MBED_TOOLCHAIN_FILE_USED TRUE CACHE INTERNAL "")
    include(mbed_toolchain)
endif()

# Specify available build profiles and add options for the selected build profile
include(mbed_profile)

enable_language(C CXX ASM)

# set executable suffix (has to be done after enabling languages)
# Note: This is nice to have, but is also required because STM32Cube will only work on files with a .elf extension
set(CMAKE_EXECUTABLE_SUFFIX .elf)

# Find Python
find_package(Python3 COMPONENTS Interpreter)
include(CheckPythonPackage)

# Check python packages from requirements.txt
file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/requirements.txt PYTHON_REQUIREMENTS)
foreach(REQUIREMENT ${PYTHON_REQUIREMENTS})
    # Look for a string from the start of each line that does not contain "<", ">", "=", or " ".
    if(REQUIREMENT MATCHES "^([^<>= ]+)")
        set(PACKAGE_NAME ${CMAKE_MATCH_1})
        string(TOUPPER ${PACKAGE_NAME} PACKAGE_NAME_UCASE) # Ucase name needed for CMake variable
        string(TOLOWER ${PACKAGE_NAME} PACKAGE_NAME_LCASE) # Lcase name needed for import statement

        check_python_package(${PACKAGE_NAME_LCASE} HAVE_PYTHON_${PACKAGE_NAME_UCASE})
        if(NOT HAVE_PYTHON_${PACKAGE_NAME_UCASE})
            message(WARNING "Missing Python dependency ${PACKAGE_NAME}")
        endif()
    else()
        message(FATAL_ERROR "Cannot parse line \"${REQUIREMENT}\" in requirements.txt")
    endif()

endforeach()

# Check deps for memap
if(Python3_FOUND AND HAVE_PYTHON_INTELHEX AND HAVE_PYTHON_PRETTYTABLE)
    set(HAVE_MEMAP_DEPS TRUE)
else()
    set(HAVE_MEMAP_DEPS FALSE)
    message(STATUS "Missing Python dependencies (python3, intelhex, prettytable) so the memory map cannot be printed")
endif()

