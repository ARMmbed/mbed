/*
 * Copyright (c) 2020 Arm Limited
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
#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"

#include "VEEBlockDevice.h"

#if COMPONENT_SPIF
#include "SPIFBlockDevice.h"
#endif

#if COMPONENT_QSPIF
#include "QSPIFBlockDevice.h"
#endif

#if COMPONENT_OSPIF
#include "OSPIFBlockDevice.h"
#endif

#if COMPONENT_OSPIF
#include "OSPIFBlockDevice.h"
#endif

#include <stdlib.h>

using namespace utest::v1;

#define TEST_BLOCK_COUNT 5
#define TEST_BLOCK_COUNT_FOR_BACKGROUND 100

#define TEST_ERROR_MASK 16

const struct {
    const char *name;
    bd_size_t (BlockDevice::*method)() const;
} ATTRS[] = {
    {"read size",    &BlockDevice::get_read_size},
    {"program size", &BlockDevice::get_program_size},
    {"erase size",   &BlockDevice::get_erase_size},
    {"total size",   &BlockDevice::size},
};

Thread background_thread(osPriorityBelowNormal);

void background_call(VEEBlockDevice *vee_bd)
{
    uint32_t cnt;

    for (cnt = 0; ; cnt++) {
        utest_printf("background_thiread: wake-up times: %lu\r\n", cnt);

        vee_bd->background();

        /* Wake up periodically */
        ThisThread::sleep_for(1000);
    }
}

void test_read_write()
{
#if COMPONENT_OSPIF
    OSPIFBlockDevice default_bd(
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
#elif COMPONENT_SPINAND
    SPINANDBlockDevice default_bd(
        MBED_CONF_SPINAND_SPINAND_IO0,
        MBED_CONF_SPINAND_SPINAND_IO1,
        MBED_CONF_SPINAND_SPINAND_IO2,
        MBED_CONF_SPINAND_SPINAND_IO3,
        MBED_CONF_SPINAND_SPINAND_SCK,
        MBED_CONF_SPINAND_SPINAND_CSN,
        MBED_CONF_SPINAND_SPINAND_POLARITY_MODE,
        MBED_CONF_SPINAND_SPINAND_FREQ
    );
#endif
    VEEBlockDevice bd(&default_bd);

    int err = bd.init();

    TEST_ASSERT_EQUAL(0, err);

    for (unsigned a = 0; a < sizeof(ATTRS) / sizeof(ATTRS[0]); a++) {
        static const char *prefixes[] = {"", "k", "M", "G"};

        for (int i = 3; i >= 0; i--) {
            bd_size_t size = (bd.*ATTRS[a].method)();

            if (size >= (1ULL << 10 * i)) {
                utest_printf("%s: %llu%sbytes (%llubytes)\n",
                             ATTRS[a].name, size >> 10 * i, prefixes[i], size);
                break;
            }
        }
    }

    uint8_t *write_block = new uint8_t[30];
    uint8_t *read_block = new uint8_t[30];


    for (int b = 0; b < TEST_BLOCK_COUNT; b++) {

        utest_printf("test number: %u\n",  b);

        bd_size_t block_size = rand() % 30 + 1;

        // Find a random block
        bd_addr_t block = rand() % bd.size();

        // Use next random number as temporary seed to keep
        // the address progressing in the pseudorandom sequence
        unsigned seed = rand();

        // Fill with random sequence
        srand(seed);

        for (bd_size_t i = 0; i < block_size; i++) {
            write_block[i] = 0xff & rand();
        }

        // Write, sync, and read the block
        utest_printf("test  addr 0x%llx:  size %llu...\n",  block, block_size);

        err = bd.program(write_block, block, block_size);
        TEST_ASSERT_EQUAL(0, err);

        utest_printf("write addr 0x%llx:  size %llu... ", block, block_size);

        for (uint32_t i = 0; i < block_size && i < 16; i++) {
            utest_printf("%02x ", write_block[i]);
        }

        if (block_size > 16) {
            utest_printf("...\n");
        }

        utest_printf("\n");

        err = bd.read(read_block, block, block_size);
        TEST_ASSERT_EQUAL(0, err);

        utest_printf("read  addr 0x%0*llx:  size %llu... ", block, block_size);

        for (uint32_t i = 0; i < block_size && i < 16; i++) {
            utest_printf("%02x ", read_block[i]);
        }

        if (block_size > 16) {
            utest_printf("...");
        }

        utest_printf("\n");

        // Check that the data was unmodified
        srand(seed);

        int val_rand;
        for (bd_size_t i_ind = 0; i_ind < block_size; i_ind++) {
            val_rand = rand();
            if ((0xff & val_rand) != read_block[i_ind]) {
                utest_printf("\n Assert Failed Buf Read - block:size: %llx:%llu \n", block, block_size);
                utest_printf("\n pos: %llu, exp: %02x, act: %02x, wrt: %02x \n", i_ind, (0xff & val_rand),
                             read_block[i_ind], write_block[i_ind]);
            }
            TEST_ASSERT_EQUAL(0xff & val_rand, read_block[i_ind]);
        }
    }

    err = bd.deinit();
    TEST_ASSERT_EQUAL(0, err);
}

void test_background_thread()
{
#if COMPONENT_OSPIF
    OSPIFBlockDevice default_bd(
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
#elif COMPONENT_SPINAND
    SPINANDBlockDevice default_bd(
        MBED_CONF_SPINAND_SPINAND_IO0,
        MBED_CONF_SPINAND_SPINAND_IO1,
        MBED_CONF_SPINAND_SPINAND_IO2,
        MBED_CONF_SPINAND_SPINAND_IO3,
        MBED_CONF_SPINAND_SPINAND_SCK,
        MBED_CONF_SPINAND_SPINAND_CSN,
        MBED_CONF_SPINAND_SPINAND_POLARITY_MODE,
        MBED_CONF_SPINAND_SPINAND_FREQ
    );
#endif
    VEEBlockDevice bd(&default_bd);

    int err = bd.init();

    TEST_ASSERT_EQUAL(0, err);

    background_thread.start(callback(background_call, &bd));

    uint8_t *write_block = new uint8_t[30];
    uint8_t *read_block = new uint8_t[30];


    for (int b = 0; b < TEST_BLOCK_COUNT_FOR_BACKGROUND; b++) {

        utest_printf("test number: %u\n",  b);

        bd_size_t block_size = rand() % 30 + 1;

        // Find a random block
        bd_addr_t block = rand() % bd.size();

        // Use next random number as temporary seed to keep
        // the address progressing in the pseudorandom sequence
        unsigned seed = rand();

        // Fill with random sequence
        srand(seed);

        for (bd_size_t i = 0; i < block_size; i++) {
            write_block[i] = 0xff & rand();
        }

        // Write, sync, and read the block
        utest_printf("test  addr 0x%llx:  size %llu...\n",  block, block_size);

        err = bd.program(write_block, block, block_size);
        TEST_ASSERT_EQUAL(0, err);

        utest_printf("write addr 0x%llx:  size %llu... ", block, block_size);

        for (uint32_t i = 0; i < block_size && i < 16; i++) {
            utest_printf("%02x ", write_block[i]);
        }

        if (block_size > 16) {
            utest_printf("...\n");
        }

        utest_printf("\n");

        err = bd.read(read_block, block, block_size);
        TEST_ASSERT_EQUAL(0, err);

        utest_printf("read  addr 0x%0*llx:  size %llu... ", block, block_size);

        for (uint32_t i = 0; i < block_size && i < 16; i++) {
            utest_printf("%02x ", read_block[i]);
        }

        if (block_size > 16) {
            utest_printf("...");
        }

        utest_printf("\n");

        // Check that the data was unmodified
        srand(seed);

        int val_rand;
        for (bd_size_t i_ind = 0; i_ind < block_size; i_ind++) {
            val_rand = rand();
            if ((0xff & val_rand) != read_block[i_ind]) {
                utest_printf("\n Assert Failed Buf Read - block:size: %llx:%llu \n", block, block_size);
                utest_printf("\n pos: %llu, exp: %02x, act: %02x, wrt: %02x \n", i_ind, (0xff & val_rand),
                             read_block[i_ind], write_block[i_ind]);
            }
            TEST_ASSERT_EQUAL(0xff & val_rand, read_block[i_ind]);
        }
        ThisThread::sleep_for(6000);
    }

    err = bd.deinit();
    TEST_ASSERT_EQUAL(0, err);
}

// Test setup
utest::v1::status_t test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(3000, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("Testing read write random blocks", test_read_write),
    Case("Testing background thread", test_background_thread),
};

Specification specification(test_setup, cases);

int main()
{
    return !Harness::run(specification);
}
