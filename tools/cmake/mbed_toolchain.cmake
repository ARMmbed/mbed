# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Generate a file containing compile definitions
function(mbed_generate_options_for_linker target output_response_file_path)
    set(_compile_definitions
        "$<TARGET_PROPERTY:${target},INTERFACE_COMPILE_DEFINITIONS>"
    )

    # Remove macro definitions that contain spaces as the lack of escape sequences and quotation marks
    # in the macro when retrieved using generator expressions causes linker errors.
    # This includes string macros, array macros, and macros with operations.
    # TODO CMake: Add escape sequences and quotation marks where necessary instead of removing these macros.
    set(_compile_definitions
       "$<FILTER:${_compile_definitions},EXCLUDE, +>"
    )

    # Append -D to all macros as we pass these as response file to cxx compiler
    set(_compile_definitions
        "$<$<BOOL:${_compile_definitions}>:-D$<JOIN:${_compile_definitions}, -D>>"
    )
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compile_time_defs.txt" CONTENT "${_compile_definitions}\n")
    set(${output_response_file_path} @${CMAKE_CURRENT_BINARY_DIR}/compile_time_defs.txt PARENT_SCOPE)
endfunction()

# Backward compatibility with older CMake which uses CMAKE_SYSTEM_PROCESSOR to
# automatically add compile and link options for the Arm Compiler.
# Note: From version 3.21, CMake by default (policy CMP0123 set to NEW) does not
# use this macro anymore, and projects have full control over compile and link
# options. This is because the old algorithm based on CMAKE_SYSTEM_PROCESSOR
# is too restrictive and does not support things like Cortex-M33.no_dsp.no_fp.
if(MBED_TOOLCHAIN STREQUAL "ARM")
    if(NOT POLICY CMP0123)
        # Old versions of CMake do not have CMP0123.
        # In the future, support for old versions of CMake will be
        # dropped from Mbed OS.
        set(deprecated_system_processor ON)
    else()
        cmake_policy(GET CMP0123 policy_CMP0123)
        if("${policy_CMP0123}" STREQUAL "")
            # CMP0123 is unset if an old `cmake_minimum_required()` is used with a
            # new CMake. Enable new CMP0123 to take advantage of the improvement
            # and dismiss deprecation warnings from CMake.
            cmake_policy(SET CMP0123 NEW)
        elseif("${policy_CMP0123}" STREQUAL "OLD")
            # Respect old CMP0123 forced by user application
            set(deprecated_system_processor ON)
        endif()
    endif()
endif()

if(deprecated_system_processor)
    if (MBED_CPU_CORE STREQUAL Cortex-A9)
        set(CMAKE_SYSTEM_PROCESSOR cortex-a9)
    elseif (MBED_CPU_CORE STREQUAL Cortex-A5)
        set(CMAKE_SYSTEM_PROCESSOR cortex-a5)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M0+)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m0plus)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M0)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m0)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M1)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m1)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M23-NS)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m23)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M23)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m23)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M3)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m3)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M33-NS)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m33)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M33)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m33)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M33F-NS)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m33)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M33F)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m33)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M33FE-NS)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m33)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M33FE)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m33)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M4)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m4)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M4F)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m4)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M55)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m55)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M7)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m7)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M7F)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m7)
    elseif (MBED_CPU_CORE STREQUAL Cortex-M7FD)
        set(CMAKE_SYSTEM_PROCESSOR cortex-m7)
    endif()
endif()

# Compiler setup
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_CROSSCOMPILING TRUE)

# Clear toolchains options for all languages as Mbed OS uses
# different initialisation options (such as for optimization and debug symbols)
set(CMAKE_C_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)

set(CMAKE_ASM_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)

# Use response files always
set(CMAKE_ASM_USE_RESPONSE_FILE_FOR_INCLUDES 1)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES 1)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES 1)

set(CMAKE_ASM_USE_RESPONSE_FILE_FOR_OBJECTS 1)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS 1)

set(CMAKE_ASM_USE_RESPONSE_FILE_FOR_LIBRARIES 1)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES 1)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_LIBRARIES 1)

# Set basic compiler and linker flags.
# Also load some utility functions for setting the toolchain options.
set(link_options "")
set(common_options "")
set(c_cxx_compile_options "") # compile options only for C/CXX
set(asm_compile_options "") # compile options only for ASM

include(toolchains/${MBED_TOOLCHAIN})

set(MBED_CPU_CORE_TAG ${MBED_CPU_CORE})
if (MBED_CPU_CORE MATCHES "Cortex-A[0-9]+$")
    set(MBED_CPU_CORE_TAG "Cortex-A")
    string(REPLACE "Cortex-" "" MBED_CPU_CORE_CODE "${MBED_CPU_CORE}")
endif()

include(cores/${MBED_CPU_CORE_TAG})

#converts a list into a string with each of its elements seperated by a space
macro(list_to_space_separated OUTPUT_VAR)# 2nd arg: LIST...
    string(REPLACE ";" " " ${OUTPUT_VAR} "${ARGN}")
endmacro(list_to_space_separated)

# set toolchain flags with CMake (INIT variables will be picked up on first run)
list_to_space_separated(CMAKE_C_FLAGS_INIT ${common_options} ${c_cxx_compile_options})
set(CMAKE_CXX_FLAGS_INIT ${CMAKE_C_FLAGS_INIT})
list_to_space_separated(CMAKE_ASM_FLAGS_INIT ${common_options} ${asm_compile_options})
list_to_space_separated(CMAKE_EXE_LINKER_FLAGS_INIT ${link_options})

# due to a bug in ARMClang CMake compiler file, all _INIT flags must end with a space.
# (this line: https://github.com/Kitware/CMake/blob/7d4a6ce714a9213ffd34b13a3debcb31a2430e04/Modules/Compiler/ARMClang.cmake#L97)
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} ")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} ")
set(CMAKE_ASM_FLAGS_INIT "${CMAKE_ASM_FLAGS_INIT} ")
