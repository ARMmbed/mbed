/* mbed Microcontroller Library
 * Copyright (c) 2020 ARM Limited
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
#ifndef MBED_OSPI_FLASH_MX31LF4GE4BC_H
#define MBED_OSPI_FLASH_MX31LF4GE4BC_H

//#define OSPI_FLASH_CHIP_STRING "macronix MX25LW51245G"

#define MX_FLASH_BLOCK_SIZE           2048*64    /* 64 blocks of 2048 Bytes */
#define MX_FLASH_SECTOR_SIZE          2048*64    /* 64 blocks of 2048 Bytes */
#define MX_FLASH_PAGE_SIZE            512        /* 512 bytes for sub-page */
#define MX_FLASH_CHUNK_SIZE           0x10       /* 16 bytes */
#define MX_FLASH_BANK_SIZE            0x01000000 /* 16 MBytes */
#define MX_FLASH_BANK_SIZE_MASK       ~(MX_FLASH_BANK_SIZE - 1) /* 0xFF000000 */
#define MX_FLASH_BLOCK_OFFSET         0x40000
#define MX_FLASH_SECTOR_OFFSET        0x40000
#define MX_FLASH_PAGE_OFFSET          0x1000

#endif // MBED_OSPI_FLASH_MX31LF4GE4BC_H
