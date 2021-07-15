# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if("STM32F2" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(TARGET_STM32F2)
elseif("STM32F4" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(TARGET_STM32F4)
elseif("STM32F7" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(TARGET_STM32F7)
elseif("STM32H7" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(TARGET_STM32H7)
endif()

target_include_directories(mbed-emac
    INTERFACE
        .
)

target_sources(mbed-emac
    INTERFACE
        stm32xx_emac.cpp
        stm32xx_eth_irq_callback.cpp
)
