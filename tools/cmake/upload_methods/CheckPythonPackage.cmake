# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# CMake functions for checking for Python packages
# Requires PYTHON_EXECUTABLE to be defined.  Call FindPythonInterp first!

# NOTE: if moving this file, be sure to also move python_packagecheck.py

# must evaluate this now since CMAKE_CURRENT_LIST_DIR doesn't work in function scope
set(PYTHON_PACKAGECHECK_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/python_packagecheck.py)

# set OUTPUT_VAR to whether PACKAGENAME was found
function(check_python_package PACKAGENAME OUTPUT_VAR)

    # can't have Python packages without Python!
    if(NOT Python3_FOUND)
        set(${OUTPUT_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    set(NEED_TO_RUN_CHECK TRUE)

    if(DEFINED ${OUTPUT_VAR})
        if(${OUTPUT_VAR})

            # if the python interpreter changed, we need to recheck
            if("${PY_INTERP_FOR_${OUTPUT_VAR}}" STREQUAL "${Python3_EXECUTABLE}")
                set(NEED_TO_RUN_CHECK FALSE)
            endif()

        endif()
    endif()

    if(NEED_TO_RUN_CHECK)
        set(PY_INTERP_FOR_${OUTPUT_VAR} ${Python3_EXECUTABLE} CACHE INTERNAL "The python interpreter used to run the ${OUTPUT_VAR} check" FORCE)

        execute_process(COMMAND ${Python3_EXECUTABLE} ${PYTHON_PACKAGECHECK_SCRIPT} ${PACKAGENAME}
                RESULT_VARIABLE PACKAGECHECK_RESULT)

        if(${PACKAGECHECK_RESULT} EQUAL 0)
            set(HAVE_PACKAGE TRUE)
        else()
            set(HAVE_PACKAGE FALSE)
        endif()

            if(HAVE_PACKAGE)
            message(STATUS "Checking for Python package ${PACKAGENAME} -- found")
        else()
            message(STATUS "Checking for Python package ${PACKAGENAME} -- not found")
        endif()

        set(${OUTPUT_VAR} ${HAVE_PACKAGE} CACHE BOOL "Whether the Python package ${PACKAGENAME} was found" FORCE)
        mark_as_advanced(${OUTPUT_VAR})

    endif()
endfunction(check_python_package)

# check that PACKAGENAME can be imported, and print an error if not
function(verify_python_package PACKAGENAME)

    # we can just generate our own variable name
    string(TOUPPER "HAVE_${PACKAGENAME}" HAVE_VAR_NAME)

    check_python_package(${PACKAGENAME} ${HAVE_VAR_NAME})

    if(NOT ${HAVE_VAR_NAME})
        message(FATAL_ERROR "The required Python package ${PACKAGENAME} was not found in ${Python3_EXECUTABLE}.  Please install it.")
    endif()
endfunction(verify_python_package)