/* mbed Microcontroller Library
 * Copyright (c) 2016 ARM Limited
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

#if !DEVICE_FLASH
    #error [NOT_SUPPORTED] Flash API not supported for this target
#endif

#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"

#include "mbed.h"
#include "flash_api.h"
#include "flash_data.h"

using namespace utest::v1;

/*
    return values to be checked are documented at:
        http://arm-software.github.io/CMSIS_5/Pack/html/algorithmFunc.html#Verify
*/

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(x, a) ((x)& ~((a) - 1))
#endif

void flash_init_test()
{
    flash_t test_flash;
    int32_t ret = flash_init(&test_flash);
    TEST_ASSERT_EQUAL_INT32(0, ret);
    ret = flash_free(&test_flash);
    TEST_ASSERT_EQUAL_INT32(0, ret);
}

void flash_mapping_alignment_test()
{
    flash_t test_flash;
    int32_t ret = flash_init(&test_flash);
    TEST_ASSERT_EQUAL_INT32(0, ret);

    const uint32_t page_size = flash_get_page_size(&test_flash);
    const uint32_t flash_start = flash_get_start_address(&test_flash);
    const uint32_t flash_size = flash_get_size(&test_flash);
    TEST_ASSERT_TRUE(page_size != 0UL);

    uint32_t sector_size = flash_get_sector_size(&test_flash, flash_start);
    for (uint32_t offset = 0; offset < flash_size;  offset += sector_size) {
        const uint32_t sector_start = flash_start + offset;
        sector_size = flash_get_sector_size(&test_flash, sector_start);
        const uint32_t sector_end = sector_start + sector_size - 1;
        const uint32_t end_sector_size = flash_get_sector_size(&test_flash, sector_end);

        // Sector size must be a valid value
        TEST_ASSERT_NOT_EQUAL(MBED_FLASH_INVALID_SIZE, sector_size);
        // Sector size must be greater than zero
        TEST_ASSERT_NOT_EQUAL(0, sector_size);
        // All flash sectors must be a multiple of page size
        TEST_ASSERT_EQUAL(0, sector_size % page_size);
        // Sector address must be a multiple of sector size
        TEST_ASSERT_EQUAL(0, sector_start % sector_size);
        // All address in a sector must return the same sector size
        TEST_ASSERT_EQUAL(sector_size, end_sector_size);

    }

    // Make sure unmapped flash is reported correctly
    TEST_ASSERT_EQUAL(MBED_FLASH_INVALID_SIZE, flash_get_sector_size(&test_flash, flash_start - 1));
    TEST_ASSERT_EQUAL(MBED_FLASH_INVALID_SIZE, flash_get_sector_size(&test_flash, flash_start + flash_size));

    ret = flash_free(&test_flash);
    TEST_ASSERT_EQUAL_INT32(0, ret);
}

// Erase sector, write one page, erase sector and write new data
void flash_program_page_test()
{
    flash_t test_flash;
    int32_t ret = flash_init(&test_flash);
    TEST_ASSERT_EQUAL_INT32(0, ret);

    uint32_t test_size = flash_get_page_size(&test_flash);
    uint8_t *data = new uint8_t[test_size];
    for (uint32_t i = 0; i < test_size; i++) {
        data[i] = 0xCE;
    }

    // the one before the last page in the system
    uint32_t address = flash_get_start_address(&test_flash) + flash_get_size(&test_flash) - (2*test_size);

    // sector size might not be same as page size
    uint32_t erase_sector_boundary = ALIGN_DOWN(address, flash_get_sector_size(&test_flash, address));
    ret = flash_erase_sector(&test_flash, erase_sector_boundary);
    TEST_ASSERT_EQUAL_INT32(0, ret);

    ret = flash_program_page(&test_flash, address, data, test_size);
    TEST_ASSERT_EQUAL_INT32(0, ret);
    uint8_t *data_flashed = (uint8_t *)address;
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, data_flashed, test_size);

    // sector size might not be same as page size
    erase_sector_boundary = ALIGN_DOWN(address, flash_get_sector_size(&test_flash, address));
    ret = flash_erase_sector(&test_flash, erase_sector_boundary);
    TEST_ASSERT_EQUAL_INT32(0, ret);

    // write another data to be certain we are re-flashing
    for (uint32_t i = 0; i < test_size; i++) {
        data[i] = 0xAC;
    }
    ret = flash_program_page(&test_flash, address, data, test_size);
    TEST_ASSERT_EQUAL_INT32(0, ret);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, data_flashed, test_size);

    ret = flash_free(&test_flash);
    TEST_ASSERT_EQUAL_INT32(0, ret);
    delete[] data;
}

void flash_erase_sector_test()
{
    flash_t test_flash;
    int32_t ret = flash_init(&test_flash);
    TEST_ASSERT_EQUAL_INT32(0, ret);

    uint32_t addr_after_last = flash_get_start_address(&test_flash) + flash_get_size(&test_flash);
    uint32_t last_sector_size = flash_get_sector_size(&test_flash, addr_after_last - 1);
    uint32_t last_sector = addr_after_last - last_sector_size;
    TEST_ASSERT_EQUAL_INT32(0, last_sector % last_sector_size);
    ret = flash_erase_sector(&test_flash, last_sector);
    TEST_ASSERT_EQUAL_INT32(0, ret);

    ret = flash_free(&test_flash);
    TEST_ASSERT_EQUAL_INT32(0, ret);
}

utest::v1::status_t greentea_failure_handler(const Case *const source, const failure_t reason) {
    greentea_case_failure_abort_handler(source, reason);
    return STATUS_CONTINUE;
}

Case cases[] = {
    Case("Flash - init", flash_init_test, greentea_failure_handler),
    Case("Flash - mapping alignment", flash_mapping_alignment_test, greentea_failure_handler),
    Case("Flash - erase sector", flash_erase_sector_test, greentea_failure_handler),
    Case("Flash - program page", flash_program_page_test, greentea_failure_handler),

};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(20, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main() {
    Harness::run(specification);
}
