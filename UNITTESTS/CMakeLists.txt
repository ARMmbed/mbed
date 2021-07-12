# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Setup c++ standard
set(CMAKE_CXX_STANDARD 14 CACHE STRING "")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "")

if (MINGW)
    # enable PRIx formatting globally
    add_definitions(-D__STDC_FORMAT_MACROS)
endif (MINGW)

####################
# GTEST
####################
include(FetchContent)
# Download and unpack googletest
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG master
)
FetchContent_MakeAvailable(googletest)

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)


####################
# TESTING
####################

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_BINARY_DIR}/Testing"
)

####################
# CODE COVERAGE SETUP
####################

if (COVERAGE)

    if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "Non-debug build may result misleading code coverage results.")
    endif()

    # Append coverage compiler flags
    set(COVERAGE_COMPILER_FLAGS "-g -O0 --coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILER_FLAGS}" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILER_FLAGS}" CACHE STRING "" FORCE)

endif(COVERAGE)

if (VALGRIND)
    find_program(MEMORYCHECK_COMMAND valgrind)
endif(VALGRIND)

add_subdirectory(stubs)
