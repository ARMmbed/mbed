# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

set(TEST_NAME filesystemstore-unittest)

add_executable(${TEST_NAME})

target_compile_definitions(${TEST_NAME} 
    PRIVATE
        MBED_LFS_READ_SIZE=64
        MBED_LFS_PROG_SIZE=64
        MBED_LFS_BLOCK_SIZE=512
        MBED_LFS_LOOKAHEAD=512
)

target_sources(${TEST_NAME}
    PRIVATE
        ${mbed-os_SOURCE_DIR}/storage/blockdevice/source/HeapBlockDevice.cpp
        ${mbed-os_SOURCE_DIR}/storage/kvstore/filesystemstore/source/FileSystemStore.cpp
        ${mbed-os_SOURCE_DIR}/storage/filesystem/littlefs/source/LittleFileSystem.cpp
        ${mbed-os_SOURCE_DIR}/storage/filesystem/source/Dir.cpp
        ${mbed-os_SOURCE_DIR}/storage/filesystem/source/File.cpp
        ${mbed-os_SOURCE_DIR}/storage/filesystem/source/FileSystem.cpp
        ${mbed-os_SOURCE_DIR}/platform/mbed-trace/source/mbed_trace.c
        ${mbed-os_SOURCE_DIR}/storage/filesystem/littlefs/littlefs/lfs.c
        ${mbed-os_SOURCE_DIR}/platform/source/FileBase.cpp
        ${mbed-os_SOURCE_DIR}/platform/source/FileSystemHandle.cpp
        ${mbed-os_SOURCE_DIR}/platform/source/FileHandle.cpp
        moduletest.cpp
)

target_link_libraries(${TEST_NAME}
    PRIVATE
        mbed-headers-blockdevice
        mbed-headers-drivers
        mbed-headers-filesystem
        mbed-headers-hal
        mbed-headers-kvstore
        mbed-headers-platform
        mbed-stubs-platform
        mbed-stubs-filesystemstore
        gmock_main
)

add_test(NAME "${TEST_NAME}" COMMAND ${TEST_NAME})

set_tests_properties(${TEST_NAME} PROPERTIES LABELS "storage")
