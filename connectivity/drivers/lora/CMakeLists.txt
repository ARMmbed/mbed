# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if("SX126X" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_SX126X)
endif()

if("SX1272" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_SX1272)
endif()

if("SX1276" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_SX1276)
endif()

if("STM32WL" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(TARGET_STM32WL)
endif()

if("lr1110" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_lr1110)
endif()
