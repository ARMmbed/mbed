/* mbed Microcontroller Library
 * Copyright (c) 2018-2020 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS //Required for PRIu64
#endif

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include <inttypes.h>
#include <stdlib.h>
#include "BufferedBlockDevice.h"
#include "BlockDevice.h"
#include <algorithm>

#if COMPONENT_SPIF
#include "SPIFBlockDevice.h"
#endif

#if COMPONENT_QSPIF
#include "QSPIFBlockDevice.h"
#endif

#if COMPONENT_OSPIF
#include "OSPIFBlockDevice.h"
#endif

#if COMPONENT_DATAFLASH
#include "DataFlashBlockDevice.h"
#endif

#if COMPONENT_SD
#include "SDBlockDevice.h"
#endif

#if COMPONENT_FLASHIAP
#include "FlashIAPBlockDevice.h"
#endif

#if COMPONENT_SPINAND
#include "SPINANDBlockDevice.h"
#endif

// Debug available
#ifndef MODE_DEBUG
#define MODE_DEBUG      0
#endif

#if MODE_DEBUG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

using namespace utest::v1;

#define TEST_BLOCK_COUNT 10
#define TEST_ERROR_MASK 16
#define TEST_NUM_OF_THREADS 5
#define TEST_THREAD_STACK_SIZE 1152

uint8_t num_of_sectors = TEST_NUM_OF_THREADS * TEST_BLOCK_COUNT;
uint32_t sectors_addr[TEST_NUM_OF_THREADS * TEST_BLOCK_COUNT] = {0};
bd_size_t max_sector_size = 0;

const struct {
    const char *name;
    bd_size_t (BlockDevice::*method)() const;
} ATTRS[] = {
    {"read size",    &BlockDevice::get_read_size},
    {"program size", &BlockDevice::get_program_size},
    {"erase size",   &BlockDevice::get_erase_size},
    {"total size",   &BlockDevice::size},
};

enum bd_type {
    spif = 0,
    qspif,
    dataflash,
    sd,
    flashiap,
    ospif,
    spinand,
    default_bd
};

uint8_t bd_arr[6] = {0};

static uint8_t test_iteration = 0;

static SingletonPtr<PlatformMutex> _mutex;

BlockDevice *block_device = NULL;

#if COMPONENT_FLASHIAP
static inline uint32_t align_up(uint32_t val, uint32_t size)
{
    return (((val - 1) / size) + 1) * size;
}
#endif

static BlockDevice *get_bd_instance(uint8_t bd_type)
{
    switch (bd_arr[bd_type]) {
        case spif: {
#if COMPONENT_SPIF
            static SPIFBlockDevice default_bd(
                MBED_CONF_SPIF_DRIVER_SPI_MOSI,
                MBED_CONF_SPIF_DRIVER_SPI_MISO,
                MBED_CONF_SPIF_DRIVER_SPI_CLK,
                MBED_CONF_SPIF_DRIVER_SPI_CS,
                MBED_CONF_SPIF_DRIVER_SPI_FREQ
            );
            return &default_bd;
#endif
            break;
        }
        case qspif: {
#if COMPONENT_QSPIF
            static QSPIFBlockDevice default_bd(
                MBED_CONF_QSPIF_QSPI_IO0,
                MBED_CONF_QSPIF_QSPI_IO1,
                MBED_CONF_QSPIF_QSPI_IO2,
                MBED_CONF_QSPIF_QSPI_IO3,
                MBED_CONF_QSPIF_QSPI_SCK,
                MBED_CONF_QSPIF_QSPI_CSN,
                MBED_CONF_QSPIF_QSPI_POLARITY_MODE,
                MBED_CONF_QSPIF_QSPI_FREQ
            );
            return &default_bd;
#endif
            break;
        }
        case ospif: {
#if COMPONENT_OSPIF
            static OSPIFBlockDevice default_bd(
                MBED_CONF_OSPIF_OSPI_IO0,
                MBED_CONF_OSPIF_OSPI_IO1,
                MBED_CONF_OSPIF_OSPI_IO2,
                MBED_CONF_OSPIF_OSPI_IO3,
                MBED_CONF_OSPIF_OSPI_IO4,
                MBED_CONF_OSPIF_OSPI_IO5,
                MBED_CONF_OSPIF_OSPI_IO6,
                MBED_CONF_OSPIF_OSPI_IO7,
                MBED_CONF_OSPIF_OSPI_SCK,
                MBED_CONF_OSPIF_OSPI_CSN,
                MBED_CONF_OSPIF_OSPI_DQS,
                MBED_CONF_OSPIF_OSPI_POLARITY_MODE,
                MBED_CONF_OSPIF_OSPI_FREQ
            );
            return &default_bd;
#endif
            break;
        }
        case spinand: {
#if COMPONENT_SPINAND
            static SPINANDBlockDevice default_bd(
                MBED_CONF_SPINAND_SPINAND_IO0,
                MBED_CONF_SPINAND_SPINAND_IO1,
                MBED_CONF_SPINAND_SPINAND_IO2,
                MBED_CONF_SPINAND_SPINAND_IO3,
                MBED_CONF_SPINAND_SPINAND_SCK,
                MBED_CONF_SPINAND_SPINAND_CSN,
                MBED_CONF_SPINAND_SPINAND_POLARITY_MODE,
                MBED_CONF_SPINAND_SPINAND_FREQ
            );
            return &default_bd;
#endif
            break;
        }

        case dataflash: {
#if COMPONENT_DATAFLASH
            static DataFlashBlockDevice default_bd(
                MBED_CONF_DATAFLASH_SPI_MOSI,
                MBED_CONF_DATAFLASH_SPI_MISO,
                MBED_CONF_DATAFLASH_SPI_CLK,
                MBED_CONF_DATAFLASH_SPI_CS
            );
            return &default_bd;
#endif
            break;
        }
        case sd: {
#if COMPONENT_SD
            static SDBlockDevice default_bd(
                MBED_CONF_SD_SPI_MOSI,
                MBED_CONF_SD_SPI_MISO,
                MBED_CONF_SD_SPI_CLK,
                MBED_CONF_SD_SPI_CS
            );
            return &default_bd;
#endif
            break;
        }
        case flashiap: {
#if COMPONENT_FLASHIAP
#if (MBED_CONF_FLASHIAP_BLOCK_DEVICE_SIZE == 0) && (MBED_CONF_FLASHIAP_BLOCK_DEVICE_BASE_ADDRESS == 0xFFFFFFFF)

            size_t flash_size;
            uint32_t start_address;
            uint32_t bottom_address;
            mbed::FlashIAP flash;

            int ret = flash.init();
            if (ret != 0) {
                return NULL;
            }

            //Find the start of first sector after text area
            bottom_address = align_up(FLASHIAP_APP_ROM_END_ADDR, flash.get_sector_size(FLASHIAP_APP_ROM_END_ADDR));
            start_address = flash.get_flash_start();
            flash_size = flash.get_flash_size();

            ret = flash.deinit();

            static FlashIAPBlockDevice default_bd(bottom_address, start_address + flash_size - bottom_address);

#else

            static FlashIAPBlockDevice default_bd;

#endif
            return &default_bd;
#endif
            break;
        }
    }
    return NULL;
}

// Mutex is protecting rand() per srand for buffer writing and verification.
// Mutex is also protecting printouts for clear logs.
// Mutex is NOT protecting Block Device actions: erase/program/read - which is the purpose of the multithreaded test!
void basic_erase_program_read_test(BlockDevice *block_device, bd_size_t block_size, uint8_t *write_block,
                                   uint8_t *read_block, unsigned addrwidth, int thread_num)
{
    int err = 0;
    _mutex->lock();

    // Make sure block address per each test is unique
    static unsigned block_seed = 1;
    srand(block_seed++);

    // Find a random block
    bd_addr_t block = sectors_addr[thread_num];
    bd_size_t curr_block_size = block_device->get_erase_size(block);
    block_size = std::min(block_size, curr_block_size);

    // Use next random number as temporary seed to keep
    // the address progressing in the pseudorandom sequence
    unsigned seed = rand();

    // Fill with random sequence
    srand(seed);
    for (bd_size_t i_ind = 0; i_ind < block_size; i_ind++) {
        write_block[i_ind] = 0xff & rand();
    }
    // Write, sync, and read the block
    DEBUG_PRINTF("test  %0*llx:%llu...\n", addrwidth, block, curr_block_size);

    err = block_device->erase(block, curr_block_size);
    TEST_ASSERT_EQUAL(0, err);

    err = block_device->program(write_block, block, block_size);
    TEST_ASSERT_EQUAL(0, err);

    err = block_device->read(read_block, block, block_size);
    TEST_ASSERT_EQUAL(0, err);

    // Check that the data was unmodified
    srand(seed);
    int val_rand;
    for (bd_size_t i_ind = 0; i_ind < block_size; i_ind++) {
        val_rand = rand();
        if ((0xff & val_rand) != read_block[i_ind]) {
            utest_printf("\n Assert Failed Buf Read - block:size: %llx:%llu \n", block, block_size);
            utest_printf("\n pos: %llu, exp: %02x, act: %02x, wrt: %02x \n", i_ind, (0xff & val_rand),
                         read_block[i_ind],
                         write_block[i_ind]);
        }
        TEST_ASSERT_EQUAL(0xff & val_rand, read_block[i_ind]);
    }
    _mutex->unlock();
}

void test_init_bd()
{
    utest_printf("\nTest Init block device.\n");

    block_device = get_bd_instance(test_iteration);
    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");

    int err = block_device->init();
    TEST_ASSERT_EQUAL(0, err);

    bd_addr_t start_address = 0;
    bd_size_t curr_sector_size = 0;
    uint8_t i = 0;
    for (; i < num_of_sectors && start_address < block_device->size(); i++) {
        sectors_addr[i] = start_address;
        curr_sector_size = block_device->get_erase_size(start_address);
        DEBUG_PRINTF("start_address = 0x%llx, sector_size = %d\n", start_address, curr_sector_size);
        if (curr_sector_size > max_sector_size) {
            max_sector_size = curr_sector_size;
        }
#if COMPONENT_SPINAND
        start_address += 0x40000;
#else
        start_address += curr_sector_size;
#endif
    }
    num_of_sectors = i;
}

void test_random_program_read_erase()
{
    utest_printf("\nTest Random Program Read Erase Starts..\n");

    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");

    for (unsigned atr = 0; atr < sizeof(ATTRS) / sizeof(ATTRS[0]); atr++) {
        static const char *prefixes[] = {"", "k", "M", "G"};
        for (int i_ind = 3; i_ind >= 0; i_ind--) {
            bd_size_t size = (block_device->*ATTRS[atr].method)();
            if (size >= (1ULL << 10 * i_ind)) {
                utest_printf("%s: %llu%sbytes (%llubytes)\n",
                             ATTRS[atr].name, size >> 10 * i_ind, prefixes[i_ind], size);
                break;
            }
        }
    }

    unsigned addrwidth = ceil(log(float(block_device->size() - 1)) / log(float(16))) + 1;

    uint8_t *write_buffer = new (std::nothrow) uint8_t[max_sector_size];
    uint8_t *read_buffer = new (std::nothrow) uint8_t[max_sector_size];

    if (!write_buffer || !read_buffer) {
        utest_printf("Not enough memory for test\n");
        goto end;
    }

    for (int b = 0; b < std::min((uint8_t)TEST_BLOCK_COUNT, num_of_sectors); b++) {
        // basic_erase_program_read_test() can handle non-uniform sector sizes
        // and use only part of the buffers if the sector is smaller
        basic_erase_program_read_test(block_device, max_sector_size, write_buffer, read_buffer, addrwidth, b);
    }

end:
    delete[] read_buffer;
    delete[] write_buffer;
}

#if defined(MBED_CONF_RTOS_PRESENT)
static void test_thread_job()
{
    static int thread_num = 0;
    _mutex->lock();
    int block_num = thread_num++ % TEST_NUM_OF_THREADS;
    _mutex->unlock();

    uint8_t sector_per_thread = (num_of_sectors / TEST_NUM_OF_THREADS);

    unsigned addrwidth = ceil(log(float(block_device->size() - 1)) / log(float(16))) + 1;

    uint8_t *write_buffer = new (std::nothrow) uint8_t[max_sector_size];
    uint8_t *read_buffer = new (std::nothrow) uint8_t[max_sector_size];

    if (!write_buffer || !read_buffer) {
        // Some targets have sectors up to 256KB each and a relatively small RAM.
        // This test may not be able to run in this case.
        utest_printf("Not enough memory for test, is the sector size (%llu) too big?\n", max_sector_size);
        goto end;
    }

    for (int b = 0; b < sector_per_thread; b++) {
        // basic_erase_program_read_test() can handle non-uniform sector sizes
        // and use only part of the buffers if the sector is smaller
        basic_erase_program_read_test(block_device, max_sector_size, write_buffer, read_buffer, addrwidth, block_num * sector_per_thread + b);
    }

end:
    delete[] read_buffer;
    delete[] write_buffer;
}

void test_multi_threads()
{
    utest_printf("\nTest Multi Threaded Erase/Program/Read Starts..\n");

    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");

    for (unsigned atr = 0; atr < sizeof(ATTRS) / sizeof(ATTRS[0]); atr++) {
        static const char *prefixes[] = {"", "k", "M", "G"};
        for (int i_ind = 3; i_ind >= 0; i_ind--) {
            bd_size_t size = (block_device->*ATTRS[atr].method)();
            if (size >= (1ULL << 10 * i_ind)) {
                utest_printf("%s: %llu%sbytes (%llubytes)\n",
                             ATTRS[atr].name, size >> 10 * i_ind, prefixes[i_ind], size);
                break;
            }
        }
    }

    osStatus threadStatus;
    int i_ind, j_ind;
    char *dummy;

    rtos::Thread **bd_thread = new (std::nothrow) rtos::Thread*[TEST_NUM_OF_THREADS];
    TEST_SKIP_UNLESS_MESSAGE((*bd_thread) != NULL, "not enough heap to run test.");
    memset(bd_thread, 0, TEST_NUM_OF_THREADS * sizeof(rtos::Thread *));

    for (i_ind = 0; i_ind < TEST_NUM_OF_THREADS; i_ind++) {

        bd_thread[i_ind] = new (std::nothrow) rtos::Thread((osPriority_t)((int)osPriorityNormal), TEST_THREAD_STACK_SIZE);
        dummy = new (std::nothrow) char[TEST_THREAD_STACK_SIZE];

        if (!bd_thread[i_ind] || !dummy) {
            utest_printf("Not enough heap to run Thread  %d !\n", i_ind + 1);
            break;
        }
        delete[] dummy;

        threadStatus = bd_thread[i_ind]->start(callback(test_thread_job));
        if (threadStatus != 0) {
            utest_printf("Thread %d Start Failed!\n", i_ind + 1);
            break;
        }
    }

    for (j_ind = 0; j_ind < i_ind; j_ind++) {
        bd_thread[j_ind]->join();
    }

    if (bd_thread) {
        for (j_ind = 0; j_ind < i_ind; j_ind++) {
            delete bd_thread[j_ind];
        }

        delete[] bd_thread;
    }
}
#endif

#if defined(MX_FLASH_SUPPORT_RWW)
void test_rww_rwe()
{
    utest_printf("\nTest rww and rwe Starts..\n");

    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");

    utest_printf("\ntest read bank1 data when write bank0\n");

    // Determine start_address in bank 0
    bd_addr_t start_address_B0 = sectors_addr[rand() % num_of_sectors];
    utest_printf("start_address_B0=0x%016" PRIx64 "\n", start_address_B0);

    // Determine start_address in bank 1
    bd_addr_t start_address_B1 = start_address_B0 | 0x1000000;
    utest_printf("start_address_B1=0x%016" PRIx64 "\n", start_address_B1);

    // Determine data_buf_size
    bd_size_t erase_size = block_device->get_erase_size(start_address_B0);
    TEST_ASSERT(erase_size > 0);
    bd_size_t data_buf_size = block_device->get_program_size();;

    // Allocate buffer for write test data
    uint8_t *data_buf = new (std::nothrow) uint8_t[data_buf_size];
    TEST_SKIP_UNLESS_MESSAGE(data_buf != NULL, "Not enough memory for test");

    // Allocate buffer for read test data
    uint8_t *out_data_buf = new (std::nothrow) uint8_t[data_buf_size];
    TEST_SKIP_UNLESS_MESSAGE(out_data_buf != NULL, "Not enough memory for test");

    // First must Erase given memory region
    utest_printf("erasing given memory region in bank0\n");
    int err = block_device->erase(start_address_B0, erase_size);
    TEST_ASSERT_EQUAL(0, err);
    utest_printf("erasing given memory region in bank1\n");
    err = block_device->erase(start_address_B1, erase_size);
    TEST_ASSERT_EQUAL(0, err);

    // Write random data to selected region to make sure data is not accidentally set to "erased" value.
    // With this pre-write, the test case will fail even if block_device->erase() is broken.
    for (bd_size_t i = 0; i < data_buf_size; i++) {
        data_buf[i] = (uint8_t) rand();
    }

    utest_printf("writing given memory region in bank1\n");
    err = block_device->program((const void *)data_buf, start_address_B1, data_buf_size);
    TEST_ASSERT_EQUAL(0, err);

    memset(out_data_buf, 0, data_buf_size);

    utest_printf("writing given memory region in bank0\n");
    err = block_device->program((const void *)data_buf, start_address_B0, data_buf_size);
    TEST_ASSERT_EQUAL(0, err);

    // Read written memory region in bank1 to verify it contains information
//    utest_printf("reading written memory region in bank1 when write bank0\n");
    err = block_device->read((void *)out_data_buf, start_address_B1, data_buf_size);
    TEST_ASSERT_EQUAL(0, err);

    // Verify erased memory region
    utest_printf("verifying written memory region\n");
    for (bd_size_t i = 0; i < data_buf_size; i++) {
        TEST_ASSERT_EQUAL(out_data_buf[i], data_buf[i]);
    }

    utest_printf("\ntest read bank1 data when erase bank0\n");

    utest_printf("erasing given memory region in bank0\n");
    memset(out_data_buf, 0, data_buf_size);
    err = block_device->erase(start_address_B0, erase_size);
    TEST_ASSERT_EQUAL(0, err);

    // Read written memory region in bank1 to verify it contains information

    utest_printf("reading written memory region in bank1 when erase bank0\n");
    err = block_device->read((void *)out_data_buf, start_address_B1, data_buf_size);
    TEST_ASSERT_EQUAL(0, err);

    delete[] out_data_buf;
    delete[] data_buf;
}
#endif

void test_erase_functionality()
{
    utest_printf("\nTest BlockDevice::get_erase_value()..\n");

    // Test flow:
    //  1. Write data to selected region
    //    - Known starting point
    //  2. Erase selected region
    //  3. Read erased region and compare with get_erase_value()

    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");

    // Check erase value
    int erase_value_int = block_device->get_erase_value();
    TEST_SKIP_UNLESS_MESSAGE(erase_value_int >= 0, "Erase not supported in this block device. Test skipped.");

    // Assuming that get_erase_value() returns byte value as documentation mentions
    // "If get_erase_value() returns a non-negative byte value" for unknown case.
    TEST_ASSERT(erase_value_int <= 255);
    uint8_t erase_value = (uint8_t)erase_value_int;

    // Determine start_address
    bd_addr_t start_address = sectors_addr[rand() % num_of_sectors];
    utest_printf("start_address=0x%016" PRIx64 "\n", start_address);

    // Determine data_buf_size
    bd_size_t erase_size = block_device->get_erase_size(start_address);
    TEST_ASSERT(erase_size > 0);
    bd_size_t data_buf_size = erase_size;

    // Allocate buffer for write test data
    uint8_t *data_buf = new (std::nothrow) uint8_t[data_buf_size];
    TEST_SKIP_UNLESS_MESSAGE(data_buf != NULL, "Not enough memory for test");

    // Allocate buffer for read test data
    uint8_t *out_data_buf = new (std::nothrow) uint8_t[data_buf_size];
    TEST_SKIP_UNLESS_MESSAGE(out_data_buf != NULL, "Not enough memory for test");

    // First must Erase given memory region
    utest_printf("erasing given memory region\n");
    int err = block_device->erase(start_address, data_buf_size);
    TEST_ASSERT_EQUAL(0, err);

    // Write random data to selected region to make sure data is not accidentally set to "erased" value.
    // With this pre-write, the test case will fail even if block_device->erase() is broken.
    for (bd_size_t i = 0; i < data_buf_size; i++) {
        data_buf[i] = (uint8_t) rand();
    }

    utest_printf("writing given memory region\n");
    err = block_device->program((const void *)data_buf, start_address, data_buf_size);
    TEST_ASSERT_EQUAL(0, err);

    // Read written memory region to verify it contains information
    memset(out_data_buf, 0, data_buf_size);
    utest_printf("reading written memory region\n");
    err = block_device->read((void *)out_data_buf, start_address, data_buf_size);
    TEST_ASSERT_EQUAL(0, err);

    // Verify erased memory region
    utest_printf("verifying written memory region\n");
    for (bd_size_t i = 0; i < data_buf_size; i++) {
        TEST_ASSERT_EQUAL(out_data_buf[i], data_buf[i]);
    }

    // Erase given memory region
    utest_printf("erasing written memory region\n");
    err = block_device->erase(start_address, data_buf_size);
    TEST_ASSERT_EQUAL(0, err);

    // Read erased memory region
    utest_printf("reading erased memory region\n");
    memset(out_data_buf, 0, data_buf_size);
    err = block_device->read((void *)out_data_buf, start_address, data_buf_size);
    TEST_ASSERT_EQUAL(0, err);

    // Verify erased memory region
    utest_printf("verifying erased memory region\n");
    for (bd_size_t i = 0; i < data_buf_size; i++) {
        TEST_ASSERT_EQUAL(erase_value, out_data_buf[i]);
    }

    delete[] out_data_buf;
    delete[] data_buf;
}

void test_contiguous_erase_write_read()
{
    utest_printf("\nTest Contiguous Erase/Program/Read Starts..\n");

    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");

    // Test flow:
    //  1. Erase whole test area
    //    - Tests contiguous erase
    //  2. Write smaller memory area
    //    - Tests contiguous sector writes
    //  3. Return step 2 for whole erase region

    // Test parameters
    bd_size_t program_size = block_device->get_program_size();
    TEST_ASSERT(program_size > 0);
    utest_printf("program_size=%" PRId64 "\n", program_size);
    utest_printf("block_device->size()=%" PRId64 "\n", block_device->size());

    // Determine start_address & stop_address
    uint8_t sector_num = rand() % (num_of_sectors - 2);
    bd_addr_t start_address = sectors_addr[sector_num];
    bd_addr_t stop_address = sectors_addr[sector_num + 2];
    utest_printf("start_address=0x%016" PRIx64 "\n", start_address);
    utest_printf("stop_address=0x%016" PRIx64 "\n", stop_address);

    bd_size_t contiguous_erase_size = stop_address - start_address;
    TEST_ASSERT(contiguous_erase_size > 0);
    utest_printf("contiguous_erase_size=0x%" PRIx64 "\n", contiguous_erase_size);

    bd_size_t write_read_buf_size = program_size;

    // Reading/writing in larger chunks reduces the number of operations,
    // helping to avoid test timeouts. Try 256-byte chunks if contiguous_erase_size
    // (which should be a power of 2) is greater than that. If it's less than
    // that, the test finishes quickly anyway...
#if COMPONENT_SPINAND
    if ((program_size < 2048) && (2048 % program_size == 0)
            && (contiguous_erase_size >= 2048) && (contiguous_erase_size % 2048 == 0)) {
        utest_printf("using 2048-byte write/read buffer\n");
        write_read_buf_size = 2048;
    }
#else
    if ((program_size < 256) && (256 % program_size == 0)
            && (contiguous_erase_size >= 256) && (contiguous_erase_size % 256 == 0)) {
        utest_printf("using 256-byte write/read buffer\n");
        write_read_buf_size = 256;
    }
#endif

    // Allocate write/read buffer
    uint8_t *write_read_buf = new (std::nothrow) uint8_t[write_read_buf_size];
    TEST_SKIP_UNLESS_MESSAGE(contiguous_erase_size, "Not enough memory for test.\n");

    // Must Erase the whole region first
    utest_printf("erasing memory, from 0x%" PRIx64 " of size 0x%" PRIx64 "\n", start_address, contiguous_erase_size);
    int err = block_device->erase(start_address, contiguous_erase_size);
    TEST_ASSERT_EQUAL(0, err);

    // Pre-fill the to-be-erased region. By pre-filling the region,
    // we can be sure the test will not pass if the erase doesn't work.
#if COMPONENT_SPINAND
    for (bd_size_t offset = 0; start_address + offset < stop_address; offset += 0x40000) {
#else
    for (bd_size_t offset = 0; start_address + offset < stop_address; offset += write_read_buf_size) {
#endif
        for (size_t i = 0; i < write_read_buf_size; i++) {
            write_read_buf[i] = (uint8_t)rand();
        }
        DEBUG_PRINTF("pre-filling memory, from 0x%" PRIx64 " of size 0x%" PRIx64 "", start_address + offset,
                     write_read_buf_size);
        err = block_device->program((const void *)write_read_buf, start_address + offset, write_read_buf_size);
        TEST_ASSERT_EQUAL(0, err);
    }

    // Erase the whole region again
    utest_printf("erasing memory, from 0x%" PRIx64 " of size 0x%" PRIx64 "\n", start_address, contiguous_erase_size);
    err = block_device->erase(start_address, contiguous_erase_size);
    TEST_ASSERT_EQUAL(0, err);

    // Loop through all write/read regions
    int region = 0;
#if COMPONENT_SPINAND
    for (; start_address < stop_address; start_address += 0x40000) {
#else
    for (; start_address < stop_address; start_address += write_read_buf_size) {
#endif
        // Generate test data
        unsigned int seed = rand();
        srand(seed);
        for (size_t i = 0; i < write_read_buf_size; i++) {
            write_read_buf[i] = (uint8_t)rand();
        }

        // Write test data
        err = block_device->program((const void *)write_read_buf, start_address, write_read_buf_size);
        TEST_ASSERT_EQUAL(0, err);

        // Read test data
        memset(write_read_buf, 0, (size_t)write_read_buf_size);
        err = block_device->read(write_read_buf, start_address, write_read_buf_size);
        TEST_ASSERT_EQUAL(0, err);

        // Verify read data
        srand(seed);
        for (size_t i = 0; i < write_read_buf_size; i++) {
            uint8_t expected_value = (uint8_t)rand();
            if (write_read_buf[i] != expected_value) {
                utest_printf("data verify failed, write_read_buf[%d]=%" PRIu8 " and not %" PRIu8 "\n",
                             i, write_read_buf[i], expected_value);
            }
            TEST_ASSERT_EQUAL(write_read_buf[i], expected_value);
        }
    }

    free(write_read_buf);
}

void test_program_read_small_data_sizes()
{
    utest_printf("\nTest program-read small data sizes, from 1 to 7 bytes..\n");

    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");

    bd_size_t program_size = block_device->get_program_size();
    bd_size_t read_size = block_device->get_read_size();
    TEST_ASSERT(program_size > 0);

    // See that we have enough memory for buffered block device
    char *dummy = new (std::nothrow) char[program_size + read_size];
    TEST_SKIP_UNLESS_MESSAGE(dummy, "Not enough memory for test.\n");
    delete[] dummy;

    // use BufferedBlockDevice for better handling of block devices program and read
    BufferedBlockDevice *buff_block_device = new BufferedBlockDevice(block_device);

    // BlockDevice initialization
    int err = buff_block_device->init();
    TEST_ASSERT_EQUAL(0, err);

    const char write_buffer[] = "1234567";
    char read_buffer[7] = {};

    // Determine starting address
    bd_addr_t start_address = 0;
    bd_size_t erase_size = block_device->get_erase_size(start_address);

    for (int i = 1; i <= 7; i++) {
        err = buff_block_device->erase(start_address, erase_size);
        TEST_ASSERT_EQUAL(0, err);

        err = buff_block_device->program((const void *)write_buffer, start_address, i);
        TEST_ASSERT_EQUAL(0, err);

        err = buff_block_device->sync();
        TEST_ASSERT_EQUAL(0, err);

        err = buff_block_device->read(read_buffer, start_address, i);
        TEST_ASSERT_EQUAL(0, err);

        err = memcmp(write_buffer, read_buffer, i);
        TEST_ASSERT_EQUAL(0, err);
    }

    // BlockDevice deinitialization
    err = buff_block_device->deinit();
    TEST_ASSERT_EQUAL(0, err);

    delete buff_block_device;
}


void test_unaligned_erase_blocks()
{

    utest_printf("\nTest Unaligned Erase Starts..\n");

    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");

    TEST_SKIP_UNLESS_MESSAGE(block_device->get_erase_value() != -1, "block device has no erase functionality.");

    bd_addr_t addr = 0;
    bd_size_t sector_erase_size = block_device->get_erase_size(addr);
    unsigned addrwidth = ceil(log(float(block_device->size() - 1)) / log(float(16))) + 1;

    utest_printf("\ntest  %0*llx:%llu...", addrwidth, addr, sector_erase_size);

    //unaligned start address
    addr += 1;
    int err = block_device->erase(addr, sector_erase_size - 1);
    TEST_ASSERT_NOT_EQUAL(0, err);

    err = block_device->erase(addr, sector_erase_size);
    TEST_ASSERT_NOT_EQUAL(0, err);

    err = block_device->erase(addr, 1);
    TEST_ASSERT_NOT_EQUAL(0, err);

    //unaligned end address
    addr = 0;

    err = block_device->erase(addr, 1);
    TEST_ASSERT_NOT_EQUAL(0, err);

    err = block_device->erase(addr, sector_erase_size + 1);
    TEST_ASSERT_NOT_EQUAL(0, err);

    //erase size exceeds flash device size
    err = block_device->erase(addr, block_device->size() + 1);
    TEST_ASSERT_NOT_EQUAL(0, err);

    // Valid erase
    err = block_device->erase(addr, sector_erase_size);
    TEST_ASSERT_EQUAL(0, err);
}

void test_deinit_bd()
{
    utest_printf("\nTest deinit block device.\n");

    test_iteration++;

    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");

    int err = block_device->deinit();
    TEST_ASSERT_EQUAL(0, err);

    block_device = NULL;
}

void test_write_deinit_init()
{
    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");
    // Determine start_address & stop_address
    bd_addr_t addr = sectors_addr[rand() % num_of_sectors];
    bd_size_t erase_size = block_device->get_erase_size(addr);
    bd_size_t prog_size = block_device->get_program_size();
    uint8_t *prog = (uint8_t *) malloc(prog_size);
    TEST_ASSERT_NOT_EQUAL(prog, 0);
    uint8_t *buf = (uint8_t *) malloc(prog_size);
    TEST_ASSERT_NOT_EQUAL(buf, 0);

    for (int i = 0; i < 3; i++) {
        // Generate test pattern
        for (int j = 0; j < prog_size; j++) {
            prog[j] = (uint8_t)'0' + i + j;
        }

        int err;
        err = block_device->erase(addr, erase_size);
        TEST_ASSERT_EQUAL(err, 0);
        err = block_device->program(prog, addr, prog_size);
        TEST_ASSERT_EQUAL(err, 0);
        err = block_device->deinit();
        TEST_ASSERT_EQUAL(0, err);
        err = block_device->init();
        TEST_ASSERT_EQUAL(0, err);
        err = block_device->read(buf, addr, prog_size);
        TEST_ASSERT_EQUAL(0, memcmp(prog, buf, prog_size));
    }
    free(prog);
    free(buf);
}

void test_get_type_functionality()
{
    utest_printf("\nTest get blockdevice type..\n");

    block_device = BlockDevice::get_default_instance();
    TEST_SKIP_UNLESS_MESSAGE(block_device != NULL, "no block device found.");

    const char *bd_type = block_device->get_type();
    TEST_ASSERT_NOT_EQUAL(0, bd_type);

#if COMPONENT_QSPIF
    TEST_ASSERT_EQUAL(0, strcmp(bd_type, "QSPIF"));
#elif COMPONENT_OSPIF
    TEST_ASSERT_EQUAL(0, strcmp(bd_type, "OSPIF"));
#elif COMPONENT_SPIF
    TEST_ASSERT_EQUAL(0, strcmp(bd_type, "SPIF"));
#elif COMPONENT_DATAFLASH
    TEST_ASSERT_EQUAL(0, strcmp(bd_type, "DATAFLASH"));
#elif COMPONENT_SD
    TEST_ASSERT_EQUAL(0, strcmp(bd_type, "SD"));
#elif COMPONENT_FLASHIAP
    TEST_ASSERT_EQUAL(0, strcmp(bd_type, "FLASHIAP"));
#elif COMPONENT_SPINAND
    TEST_ASSERT_EQUAL(0, strcmp(bd_type, "SPINAND"));
#endif
}

utest::v1::status_t greentea_failure_handler(const Case *const source, const failure_t reason)
{
    greentea_case_failure_abort_handler(source, reason);
    return STATUS_CONTINUE;
}

typedef struct {
    const char *description;
    const case_handler_t case_handler;
    const case_failure_handler_t failure_handler;
} template_case_t;

template_case_t template_cases[] = {
    {"Testing Init block device", test_init_bd, greentea_failure_handler},
    {"Testing write -> deinit -> init -> read", test_write_deinit_init, greentea_failure_handler},
    {"Testing read write random blocks", test_random_program_read_erase, greentea_failure_handler},
#if defined(MBED_CONF_RTOS_PRESENT)
    {"Testing multi threads erase program read", test_multi_threads, greentea_failure_handler},
#endif
    {"Testing contiguous erase, write and read", test_contiguous_erase_write_read, greentea_failure_handler},
    {"Testing BlockDevice erase functionality", test_erase_functionality, greentea_failure_handler},
    {"Testing program read small data sizes", test_program_read_small_data_sizes, greentea_failure_handler},
    {"Testing unaligned erase blocks", test_unaligned_erase_blocks, greentea_failure_handler},
#if defined(MX_FLASH_SUPPORT_RWW)
    {"Testing read while write and read while erase", test_rww_rwe, greentea_failure_handler},
#endif
    {"Testing Deinit block device", test_deinit_bd, greentea_failure_handler},
};

template_case_t def_template_case = {"Testing get type functionality", test_get_type_functionality, greentea_failure_handler};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases)
{
    return greentea_test_setup_handler(number_of_cases);
}

int get_bd_count()
{
    int count = 0;

#if COMPONENT_SPIF
    bd_arr[count++] = spif;           //0
#endif
#if COMPONENT_QSPIF
    bd_arr[count++] = qspif;          //1
#endif
#if COMPONENT_DATAFLASH
    bd_arr[count++] = dataflash;      //2
#endif
#if COMPONENT_SD
    bd_arr[count++] = sd;             //3
#endif
#if COMPONENT_FLASHIAP
    bd_arr[count++] = flashiap;       //4
#endif
#if COMPONENT_OSPIF
    bd_arr[count++] = ospif;          //5
#endif
#if COMPONENT_SPINAND
    bd_arr[count++] = spinand;        //6
#endif

    return count;
}

static const char *prefix[] = {"SPIF ", "QSPIF ", "DATAFLASH ", "SD ", "FLASHIAP ", "OSPIF ", "SPINAND ", "DEFAULT "};

int main()
{
    GREENTEA_SETUP(300, "default_auto");

    // We want to replicate our test cases to different types
    size_t num_cases = sizeof(template_cases) / sizeof(template_case_t);
    size_t total_num_cases = 0;

    int bd_count = get_bd_count();

    void *raw_mem = new (std::nothrow) uint8_t[(bd_count * num_cases + 1) * sizeof(Case)];
    Case *cases = static_cast<Case *>(raw_mem);

    for (int j = 0; j < bd_count; j++) {
        for (size_t i = 0; i < num_cases; i++) {
            char desc[128], *desc_ptr;
            sprintf(desc, "%s%s", prefix[bd_arr[j]], template_cases[i].description);
            desc_ptr = new char[strlen(desc) + 1];
            strcpy(desc_ptr, desc);
            new (&cases[total_num_cases]) Case((const char *) desc_ptr, template_cases[i].case_handler,
                                               template_cases[i].failure_handler);
            total_num_cases++;
        }

        //Add test_get_type_functionality once, runs on default blockdevice
        if (j == bd_count - 1) {
            char desc[128], *desc_ptr;
            sprintf(desc, "%s%s", prefix[default_bd], def_template_case.description);
            desc_ptr = new char[strlen(desc) + 1];
            strcpy(desc_ptr, desc);
            new (&cases[total_num_cases]) Case((const char *) desc_ptr, def_template_case.case_handler,
                                               def_template_case.failure_handler);
            total_num_cases++;
        }
    }



    Specification specification(greentea_test_setup, cases, total_num_cases,
                                greentea_test_teardown_handler, default_handler);

    return !Harness::run(specification);
}
