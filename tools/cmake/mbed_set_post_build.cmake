# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

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


#
# Sets the post build operation for Mbed targets.
#
macro(mbed_set_post_build_operation)

    add_custom_target(mbed-post-build-bin-${mbed_target_name})

    add_custom_command(
        TARGET
            mbed-post-build-bin-${mbed_target_name}
        POST_BUILD
        COMMAND
            ${post_build_command}
        VERBATIM
    )
endmacro()
