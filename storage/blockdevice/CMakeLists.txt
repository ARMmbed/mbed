# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests/UNITTESTS)
endif()

if("DATAFLASH" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_DATAFLASH)
endif()

if("FLASHIAP" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_FLASHIAP)
endif()

if("I2CEE" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_I2CEE)
endif()

if("QSPIF" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_QSPIF)
endif()

if("OSPIF" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_OSPIF)
endif()

if("SD" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_SD)
endif()

if("SPIF" IN_LIST MBED_TARGET_LABELS)
    add_subdirectory(COMPONENT_SPIF)
endif()


target_include_directories(mbed-storage-blockdevice
    INTERFACE
        .
        ./include
        ./include/blockdevice
        ./include/blockdevice/internal
)

target_sources(mbed-storage-blockdevice
    INTERFACE
        source/BufferedBlockDevice.cpp
        source/ChainingBlockDevice.cpp
        source/ExhaustibleBlockDevice.cpp
        source/FlashSimBlockDevice.cpp
        source/HeapBlockDevice.cpp
        source/MBRBlockDevice.cpp
        source/ObservingBlockDevice.cpp
        source/ProfilingBlockDevice.cpp
        source/ReadOnlyBlockDevice.cpp
        source/SFDP.cpp
        source/SlicingBlockDevice.cpp
)
