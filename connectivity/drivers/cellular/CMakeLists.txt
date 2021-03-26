# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if("STMOD_CELLULAR" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_STMOD_CELLULAR)
endif()

add_subdirectory(Altair)
add_subdirectory(GEMALTO)
add_subdirectory(GENERIC)
add_subdirectory(MultiTech)
add_subdirectory(QUECTEL)
add_subdirectory(RiotMicro)
add_subdirectory(TELIT)
add_subdirectory(UBLOX)

if("Samsung" IN_LIST MBED_TARGET_LABELS)
add_subdirectory(Samsung)
endif()

target_include_directories(mbed-cellular
    INTERFACE
        .
)
