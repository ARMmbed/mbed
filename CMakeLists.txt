# Copyright (c) 2020-2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# This is the boilerplate for Mbed OS

cmake_minimum_required(VERSION 3.19.0 FATAL_ERROR)

if(${CMAKE_CROSSCOMPILING})
    include(${MBED_CONFIG_PATH}/mbed_config.cmake)
    include(mbed_set_linker_script)
endif()

project(mbed-os)

# Add all paths to the list files within Mbed OS
list(APPEND CMAKE_MODULE_PATH
    "${mbed-os_SOURCE_DIR}/platform/FEATURE_EXPERIMENTAL_API/FEATURE_PSA/TARGET_TFM/TARGET_TFM_LATEST/scripts;${mbed-os_SOURCE_DIR}/targets/TARGET_Cypress/scripts;${mbed-os_SOURCE_DIR}/targets/TARGET_NXP/scripts"
)

option(BUILD_TESTING "Run unit tests only." OFF)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    include(CTest)
    add_subdirectory(UNITTESTS)
endif()

add_library(mbed-core INTERFACE)

add_library(mbed-os INTERFACE)

target_link_libraries(mbed-os
    INTERFACE
        mbed-rtos
        mbed-core
)

add_library(mbed-baremetal INTERFACE)

target_link_libraries(mbed-baremetal
    INTERFACE
        mbed-core
)
# Validate selected C library type
# The C library type selected has to match the library that the target can support
if(${CMAKE_CROSSCOMPILING})
    if(${MBED_C_LIB} STREQUAL "small")
        if(NOT "small" IN_LIST MBED_TARGET_SUPPORTED_C_LIBS)
            if("std" IN_LIST MBED_TARGET_SUPPORTED_C_LIBS)
                message(WARNING
                    "We noticed that target.c_lib is set to `${MBED_C_LIB}`."
                    " As the ${MBED_TARGET} target does not support a small C library for the ${MBED_TOOLCHAIN} toolchain,"
                    " we are using the standard C library instead."
                )
                set(MBED_C_LIB "std" CACHE STRING "")
            endif()
        endif()
    elseif(NOT ${MBED_C_LIB} IN_LIST MBED_TARGET_SUPPORTED_C_LIBS)
        message(FATAL_ERROR
            "Invalid `target.c_lib` ('${MBED_C_LIB}') for '${MBED_TARGET}' target."
            "\nPossible value(s): ${MBED_TARGET_SUPPORTED_C_LIBS}"
        )
    endif()

    # Validate selected printf library
    set(MBED_PRINTF_LIB_TYPES std minimal-printf)
    if(NOT ${MBED_PRINTF_LIB} IN_LIST MBED_PRINTF_LIB_TYPES)
        message(FATAL_ERROR
            "Invalid printf library type '${MBED_PRINTF_LIB}'. Possible values:\n ${MBED_PRINTF_LIB_TYPES}"
        )
    endif()

    mbed_set_cpu_core_definitions(mbed-core)
    if(${MBED_TOOLCHAIN_FILE_USED})
        mbed_set_profile_options(mbed-core ${MBED_TOOLCHAIN})
        mbed_set_c_lib(mbed-core ${MBED_C_LIB})
        mbed_set_printf_lib(mbed-core ${MBED_PRINTF_LIB})

        target_compile_features(mbed-core
            INTERFACE
                c_std_11
                cxx_std_14
        )

    endif()

    target_compile_definitions(mbed-core
        INTERFACE
            ${MBED_TARGET_DEFINITIONS}
            ${MBED_CONFIG_DEFINITIONS}
    )

    # Add MBED_TEST_MODE for backward compatibility with Greentea tests written for use with Mbed CLI 1
    if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
        target_compile_definitions(${PROJECT_NAME}
            PUBLIC
                MBED_TEST_MODE
        )
    endif()

    # We need to generate a "response file" to pass to the C preprocessor when we preprocess the linker
    # script, because of path length limitations on Windows. We set the response file and bind the path
    # to a global property here. The MBED_TARGET being built queries this global property when it sets
    # the linker script.
    #
    # We must set this global property before the targets subdirectory is added to the project. This is
    # required because the MBED_TARGET depends on the response file. If the path to the response file
    # is not defined when the target requests it the config definitions will not be passed to CPP.
    #
    # TODO: Remove this and find a more idiomatic way of passing compile definitions to CPP without
    # using response files or global properties.
    mbed_generate_options_for_linker(mbed-core RESPONSE_FILE_PATH)
    set_property(GLOBAL PROPERTY COMPILE_DEFS_RESPONSE_FILE ${RESPONSE_FILE_PATH})

    # Add compile definitions for backward compatibility with the toolchain
    # supported. New source files should instead check for __GNUC__ and __clang__
    # for the GCC_ARM and ARM toolchains respectively.
    if(${MBED_TOOLCHAIN} STREQUAL "GCC_ARM")
        target_compile_definitions(mbed-core
            INTERFACE
                TOOLCHAIN_GCC_ARM
                TOOLCHAIN_GCC
        )
    elseif(${MBED_TOOLCHAIN} STREQUAL "ARM")
        target_compile_definitions(mbed-core
            INTERFACE
                TOOLCHAIN_ARM
        )
    endif()
endif()

# Include mbed.h and config from generate folder
target_include_directories(mbed-core
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# These targets are made visible here so their source files which
# are spread in different directories can be referenced and can be linked against
# by libraries that depend on them.
# TODO CMake: Should the source files be moved?
add_library(mbed-device_key INTERFACE)
add_library(mbed-rtos INTERFACE)

add_subdirectory(cmsis)
add_subdirectory(drivers)
add_subdirectory(hal)
add_subdirectory(platform)
add_subdirectory(rtos)
add_subdirectory(targets)
add_subdirectory(storage)
add_subdirectory(events)
add_subdirectory(connectivity)

# The directories below contain optional target libraries
add_subdirectory(drivers/device_key EXCLUDE_FROM_ALL)
add_subdirectory(drivers/usb EXCLUDE_FROM_ALL)
add_subdirectory(features EXCLUDE_FROM_ALL)
add_subdirectory(cmsis/CMSIS_5/CMSIS/RTOS2 EXCLUDE_FROM_ALL)
add_subdirectory(cmsis/device/rtos EXCLUDE_FROM_ALL)


if(${CMAKE_CROSSCOMPILING})
    # Ensure the words that make up the Mbed target name are separated with a hyphen, lowercase, and with the `mbed-` prefix.
    string(TOLOWER ${MBED_TARGET} MBED_TARGET_CONVERTED)
    string(REPLACE "_" "-" MBED_TARGET_CONVERTED ${MBED_TARGET_CONVERTED})
    string(PREPEND MBED_TARGET_CONVERTED "mbed-")

    target_link_libraries(mbed-core INTERFACE ${MBED_TARGET_CONVERTED})
endif()

#
# Converts output file of `target` to binary file and to Intel HEX file.
#
function(mbed_generate_bin_hex target)
    get_property(elf_to_bin GLOBAL PROPERTY ELF2BIN)
    if (MBED_TOOLCHAIN STREQUAL "GCC_ARM")
        # The first condition is quoted in case MBED_OUTPUT_EXT is unset
        if ("${MBED_OUTPUT_EXT}" STREQUAL "" OR MBED_OUTPUT_EXT STREQUAL "bin")
            list(APPEND CMAKE_POST_BUILD_COMMAND
                COMMAND ${elf_to_bin} -O binary $<TARGET_FILE:${target}> ${CMAKE_CURRENT_BINARY_DIR}/${target}.bin
                COMMAND ${CMAKE_COMMAND} -E echo "-- built: ${CMAKE_CURRENT_BINARY_DIR}/${target}.bin"
            )
        endif()
        if ("${MBED_OUTPUT_EXT}" STREQUAL "" OR MBED_OUTPUT_EXT STREQUAL "hex")
            list(APPEND CMAKE_POST_BUILD_COMMAND
                COMMAND ${elf_to_bin} -O ihex $<TARGET_FILE:${target}> ${CMAKE_CURRENT_BINARY_DIR}/${target}.hex
                COMMAND ${CMAKE_COMMAND} -E echo "-- built: ${CMAKE_CURRENT_BINARY_DIR}/${target}.hex"
            )
        endif()
    elseif(MBED_TOOLCHAIN STREQUAL "ARM")
        get_property(mbed_studio_arm_compiler GLOBAL PROPERTY MBED_STUDIO_ARM_COMPILER)
        if ("${MBED_OUTPUT_EXT}" STREQUAL "" OR MBED_OUTPUT_EXT STREQUAL "bin")
            list(APPEND CMAKE_POST_BUILD_COMMAND
                COMMAND ${elf_to_bin} ${mbed_studio_arm_compiler} --bin  -o ${CMAKE_CURRENT_BINARY_DIR}/${target}.bin $<TARGET_FILE:${target}>
                COMMAND ${CMAKE_COMMAND} -E echo "-- built: ${CMAKE_CURRENT_BINARY_DIR}/${target}.bin"
            )
        endif()
        if ("${MBED_OUTPUT_EXT}" STREQUAL "" OR MBED_OUTPUT_EXT STREQUAL "hex")
            list(APPEND CMAKE_POST_BUILD_COMMAND
            COMMAND ${elf_to_bin} ${mbed_studio_arm_compiler} --i32combined  -o ${CMAKE_CURRENT_BINARY_DIR}/${target}.hex $<TARGET_FILE:${target}>
            COMMAND ${CMAKE_COMMAND} -E echo "-- built: ${CMAKE_CURRENT_BINARY_DIR}/${target}.hex"
            )
        endif()
    endif()
    add_custom_command(
        TARGET
            ${target}
        POST_BUILD
            ${CMAKE_POST_BUILD_COMMAND}
        COMMENT
            "executable:"
        VERBATIM
    )

    if(TARGET mbed-post-build-bin-${MBED_TARGET})
        # Remove the .elf file to force regenerate the application binaries
        # (including .bin and .hex). This ensures that the post-build script runs
        # on a raw application instead of a previous build that already went
        # through the post-build process once.
        file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/${target}.elf)

        # Pass the application's name to the Mbed target's post build operation
        set_target_properties(mbed-post-build-bin-${MBED_TARGET}
            PROPERTIES
                application ${target}
        )

        # The artefacts must be created before they can be further manipulated
        add_dependencies(mbed-post-build-bin-${MBED_TARGET} ${target})

        # Add a post-build hook to the top-level CMake target in the form of a
        # CMake custom target. The hook depends on Mbed target specific
        # post-build CMake target which has a custom command attached to it.
        add_custom_target(mbed-post-build ALL DEPENDS mbed-post-build-bin-${MBED_TARGET})
    endif()
endfunction()

#
# Parse toolchain generated map file of `target` and display a readable table format.
#
function(mbed_generate_map_file target)
     add_custom_command(
         TARGET
             ${target}
         POST_BUILD
         COMMAND ${Python3_EXECUTABLE} ${mbed-os_SOURCE_DIR}/tools/memap.py -t ${MBED_TOOLCHAIN} ${CMAKE_CURRENT_BINARY_DIR}/${target}${CMAKE_EXECUTABLE_SUFFIX}.map
         WORKING_DIRECTORY
             ${CMAKE_CURRENT_BINARY_DIR}
         COMMENT
             "Displaying memory map for ${target}"
)
endfunction()

#
# Validate selected application profile.
#
function(mbed_validate_application_profile target)
    get_target_property(app_link_libraries ${target} LINK_LIBRARIES)
    string(FIND "${app_link_libraries}" "mbed-baremetal" string_found_position)
    if(${string_found_position} GREATER_EQUAL 0)
        if(NOT "bare-metal" IN_LIST MBED_TARGET_SUPPORTED_APPLICATION_PROFILES)
            message(FATAL_ERROR
                "Use full profile as baremetal profile is not supported for this Mbed target")
        endif()
    elseif(NOT "full" IN_LIST MBED_TARGET_SUPPORTED_APPLICATION_PROFILES)
        message(FATAL_ERROR
            "The full profile is not supported for this Mbed target")
    endif()
endfunction()

#
# Set post build operations
#
function(mbed_set_post_build target)
    # The mapfile name includes the top-level target name and the
    # executable suffix for all toolchains as CMake hardcodes the name of the
    # diagnostic output file for some toolchains.
    mbed_configure_memory_map(${target} "${CMAKE_CURRENT_BINARY_DIR}/${target}${CMAKE_EXECUTABLE_SUFFIX}.map")
    mbed_validate_application_profile(${target})
    mbed_generate_bin_hex(${target})

    if(HAVE_MEMAP_DEPS)
        mbed_generate_map_file(${target})
    endif()
endfunction()

# Ninja requires to be forced for response files
if ("${CMAKE_GENERATOR}" MATCHES "Ninja")
    # known issue ARMClang and Ninja with response files for windows
    # https://gitlab.kitware.com/cmake/cmake/-/issues/21093
    if((CMAKE_HOST_SYSTEM_NAME MATCHES "Windows" AND NOT CMAKE_CXX_COMPILER_ID MATCHES "ARMClang"))
        set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")
    endif()
endif()

# TODO: Remove once all example applications have removed it
function(mbed_configure_app_target target)
endfunction()
