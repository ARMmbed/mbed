# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Sets cpu core options
if(${MBED_TOOLCHAIN} STREQUAL "GCC_ARM")
    list(APPEND common_options
        "-mthumb"
        "-mcpu=cortex-m33+nodsp+nofp"
    )
elseif(${MBED_TOOLCHAIN} STREQUAL "ARM")
    list(APPEND common_options
        "-mcpu=cortex-m33+nodsp"
        "-mfpu=none"
    )
    if(deprecated_system_processor)
        # Normally `--cpu` is not needed, because `armlink` can infer
        # features from object files. But CMake versions below 3.21
        # automatically add `--cpu=${CMAKE_SYSTEM_PROCESSOR}` which is
        # incorrect, so as a workaround we need to add `no_fp`.
        list(APPEND link_options
            "--cpu=Cortex-M33.no_fp"
        )
    endif()
endif()

function(mbed_set_cpu_core_definitions target)
    target_compile_definitions(${target}
        INTERFACE
            __CORTEX_M33
            ARM_MATH_ARMV8MML
            DOMAIN_NS=1
            __CMSIS_RTOS
    )
endfunction()
