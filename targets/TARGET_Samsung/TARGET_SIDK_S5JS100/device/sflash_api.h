/****************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#ifndef _SFLASH_DRV_H
#define _SFLASH_DRV_H

//#include "fcache_api.h"

#ifdef __cplusplus

extern "C" {
#else
#include <stdio.h>
#endif
#define S5JS100_FLASH_PAGE_SIZE         256
#define S5JS100_FLASH_BLOCK_SIZE        4096
#define S5JS100_FLASH_PADDR             (0x40000000)

#define S5JS100_FLASH_FS_PADDR          0x40EF5000
#define S5JS100_FLASH_FS_SIZE          256*1024
#define S5JS100_FLASH_QSPI_TEST_PADDR   0x406AE000 //used for hal_qspi

#define S5JS100_FLASH_SIZE              (16 * 1024 * 1024)
#define S5JS100_OS_ENV_OFFSET_16MB      (0x2E000)
#define S5JS100_OS_ENV_OFFSET_8MB       (0x3B000)
#define S5JS100_RTC_RET_OFFSET          (0x7FF000) //will be 4K for RTC retention

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define S5JS100_SFLASH_BASE             (0x85020000)
#define S5JS100_SFLASH_SFCON            ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x0004))
#define S5JS100_SFLASH_ERASE_ADDRESS    ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x0010))
#define S5JS100_SFLASH_USER_COMMAND     ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x0018))
#define S5JS100_SFLASH_COMMAND1         ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x001C))
#define S5JS100_SFLASH_COMMAND2         ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x0020))
#define S5JS100_SFLASH_COMMAND3         ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x0024))
#define S5JS100_SFLASH_COMMAND4         ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x0028))
#define S5JS100_SFLASH_COMMAND5         ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x002C))
#define S5JS100_SFLASH_USER_INSTRUCTION ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x0059))
#define S5JS100_SFLASH_SE               ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x005E))
#define S5JS100_SFLASH_IO_MODE          ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x0074))
#define S5JS100_SFLASH_PERF_MODE        ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x0078))
#define S5JS100_SFLASH_RDID             ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x00AC))
#define S5JS100_SFLASH_BE               ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x00BE))
#define S5JS100_SFLASH_CE               ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x00CE))
#define S5JS100_SFLASH_RDSR             ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x00DC))
#define S5JS100_SFLASH_WRDI             ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x00DD))
#define S5JS100_SFLASH_WRSR             ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x00E0))
#define S5JS100_SFLASH_WREN             ((volatile unsigned int *)(S5JS100_SFLASH_BASE + 0x00EE))
/* Register Bitfield Definitions ********************************************/
/* Control Register */
#define SFLASH_SFCON_WP_SHIFT           31
#define SFLASH_SFCON_WP_MASK            (0x1 << SFLASH_SFCON_WP_SHIFT)
#define SFLASH_SFCON_WP_ENABLE          (0x0 << SFLASH_SFCON_WP_SHIFT)
#define SFLASH_SFCON_WP_DISABLE         (0x1 << SFLASH_SFCON_WP_SHIFT)

#define SFLASH_SFCON_MEMORY_VENDOR_SHIFT    16
#define SFLASH_SFCON_MEMORY_VENDOR_MASK     (0x3f << SFLASH_SFCON_MEMORY_VENDOR_SHIFT)
#define SFLASH_SFCON_MEMORY_VENDOR_WINBOND  (0x20 << SFLASH_SFCON_MEMORY_VENDOR_SHIFT)
#define SFLASH_SFCON_MEMORY_VENDOR_MACRONIX (0x10 << SFLASH_SFCON_MEMORY_VENDOR_SHIFT)
#define SFLASH_SFCON_MEMORY_VENDOR_ATMEL    (0x08 << SFLASH_SFCON_MEMORY_VENDOR_SHIFT)
#define SFLASH_SFCON_MEMORY_VENDOR_AMD      (0x04 << SFLASH_SFCON_MEMORY_VENDOR_SHIFT)
#define SFLASH_SFCON_MEMORY_VENDOR_STMICRO  (0x02 << SFLASH_SFCON_MEMORY_VENDOR_SHIFT)
#define SFLASH_SFCON_MEMORY_VENDOR_SST      (0x01 << SFLASH_SFCON_MEMORY_VENDOR_SHIFT)

#define SFLASH_SFCON_PAGE_EN_SHIFT      15
#define SFLASH_SFCON_PAGE_EN_MASK       (0x1 << SFLASH_SFCON_PAGE_EN_SHIFT)
#define SFLASH_SFCON_PAGE_EN_BYTEPROG   (0x0 << SFLASH_SFCON_PAGE_EN_SHIFT)
#define SFLASH_SFCON_PAGE_EN_PAGEPROG   (0x1 << SFLASH_SFCON_PAGE_EN_SHIFT)

#define SFLASH_SFCON_PAGE_SHIFT         8
#define SFLASH_SFCON_PAGE_MASK          (0xf << SFLASH_SFCON_PAGE_SHIFT)
#define SFLASH_SFCON_PAGE_4BYTES        (0x0 << SFLASH_SFCON_PAGE_SHIFT)
#define SFLASH_SFCON_PAGE_8BYTES        (0x1 << SFLASH_SFCON_PAGE_SHIFT)
#define SFLASH_SFCON_PAGE_16BYTES       (0x2 << SFLASH_SFCON_PAGE_SHIFT)
#define SFLASH_SFCON_PAGE_32BYTES       (0x3 << SFLASH_SFCON_PAGE_SHIFT)
#define SFLASH_SFCON_PAGE_64BYTES       (0x4 << SFLASH_SFCON_PAGE_SHIFT)
#define SFLASH_SFCON_PAGE_128BYTES      (0x5 << SFLASH_SFCON_PAGE_SHIFT)
#define SFLASH_SFCON_PAGE_256BYTES      (0x6 << SFLASH_SFCON_PAGE_SHIFT)
#define SFLASH_SFCON_PAGE_RESERVED      (0x7 << SFLASH_SFCON_PAGE_SHIFT)

#define SFLASH_SFCON_HALF_DELAY_SHIFT   7
#define SFLASH_SFCON_HALF_DELAY_MASK    (0x1 << SFLASH_SFCON_HALF_DELAY_SHIFT)
#define SFLASH_SFCON_HALF_DELAY_OFF     (0x0 << SFLASH_SFCON_HALF_DELAY_SHIFT)
#define SFLASH_SFCON_HALF_DELAY_ON      (0x1 << SFLASH_SFCON_HALF_DELAY_SHIFT)

#define SFLASH_SFCON_ERASE_WAIT_SHIFT   4
#define SFLASH_SFCON_ERASE_WAIT_MASK    (0x1 << SFLASH_SFCON_ERASE_WAIT_SHIFT)
#define SFLASH_SFCON_ERASE_WAIT_OFF     (0x0 << SFLASH_SFCON_ERASE_WAIT_SHIFT)
#define SFLASH_SFCON_ERASE_WAIT_ON      (0x1 << SFLASH_SFCON_ERASE_WAIT_SHIFT)

#define SFLASH_SFCON_PRE_CHARGE_SHIFT   0
#define SFLASH_SFCON_PRE_CHARGE_MASK    (0xf << SFLASH_SFCON_PRE_CHARGE_SHIFT)

/* Flash I/O Mode */
#define SFLASH_IO_MODE_MASK             0xf
#define SFLASH_IO_MODE_READ             0x0
#define SFLASH_IO_MODE_FAST_READ        0x1
#define SFLASH_IO_MODE_DUAL_FAST_READ   0x2
#define SFLASH_IO_MODE_QUAD_FAST_READ   0x4

/* Flash Performance Mode */
#define SFLASH_PERF_MODE_MASK           0x8
#define SFLASH_PERF_MODE_NORMAL         0x0
#define SFLASH_PERF_MODE_DUAL_QUAD      0x8

#define SFLASH_DUMMY_DATA          0x1
#define SFLASH_SECTOR_OFFSET       (12)
#define SFLASH_BLOCK32K_OFFSET     (15)
#define SFLASH_BLOCK64K_OFFSET     (16)

#define SFLASH_SIZE_64KB   (1<<SFLASH_BLOCK64K_OFFSET)
#define SFLASH_SIZE_32KB   (1<<SFLASH_BLOCK32K_OFFSET)
#define SFLASH_SIZE_4KB    (1<<SFLASH_SECTOR_OFFSET)

#define COMMAND_ERASE_32KB  (0x52)
#define COMMAND_ERASE_64KB  (0xD8)

#define S5JS100_SFLASH_WOFFSET 0x50000000 /* FLASH Mirror Offset */

typedef enum {
    TYPE_ERR = 0,
    TYPE_4KB = 1,
    TYPE_32KB = 2,
    TYPE_64KB = 3,
} eERASE_UNIT;


typedef enum {
    SFLASH_SINGLE_IO,
    SFLASH_DUAL_FAST,
    SFLASH_DUAL_IO,
    SFLASH_QUAD_FAST,
    SFLASH_QUAD_IO,
} eQSPI_MODE;

typedef enum {
    SFLASH_VENDOR_SST      = 0x1,
    SFLASH_VENDOR_STMICRO  = 0x2,
    SFLASH_VENDOR_AMD      = 0x4,
    SFLASH_VENDOR_ATMEL    = 0x8,
    SFLASH_VENDOR_MACRONIX = 0x10,
    SFLASH_VENDOR_WINBOND  = 0x20,
} eQSPI_VENDOR;

typedef enum {
    SFLASH_PAGE_4BYTES     = 0x0,
    SFLASH_PAGE_8BYTES     = 0x1,
    SFLASH_PAGE_16BYTES    = 0x2,
    SFLASH_PAGE_32BYTES    = 0x3,
    SFLASH_PAGE_64BYTES    = 0x4,
    SFLASH_PAGE_128BYTES   = 0x5,
    SFLASH_PAGE_256BYTES   = 0x6,
} eQSPI_PAGE_SIZE;

typedef struct _status_register_t {
    unsigned char rdsr;
    struct {
        unsigned WIP: 1;
        unsigned WEL: 1;
        unsigned BP0: 1;
        unsigned BP1: 1;
        unsigned BP2: 1;
        unsigned BP3: 1;
        unsigned QE: 1;  //Quad enable
        unsigned SRWD: 1; //Status Register Write Disable
    } b;
} sRead_Status_Register;

typedef enum {
    SFLASH_PROTECTION_NONE             = 0x0,
    SFLASH_PROTECTION_BOTTOM_256KB     = 0x1,  /* 0xFC0000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_BOTTOM_512KB     = 0x2,  /* 0xF80000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_BOTTOM_1MB       = 0x3,  /* 0xF00000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_BOTTOM_2MB       = 0x4,  /* 0xE00000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_BOTTOM_4MB       = 0x5,  /* 0xC00000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_BOTTOM_8MB       = 0x6,  /* 0x800000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_ALL              = 0x7,  /* 0x000000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_TOP_256KB        = 0x9,  /* 0x000000 ~ 0x03FFFF */
    SFLASH_PROTECTION_TOP_512KB        = 0xA,  /* 0x000000 ~ 0x07FFFF */
    SFLASH_PROTECTION_TOP_1MB          = 0xB,  /* 0x000000 ~ 0x0FFFFF */
    SFLASH_PROTECTION_TOP_2MB          = 0xC,  /* 0x000000 ~ 0x1FFFFF */
    SFLASH_PROTECTION_TOP_4MB          = 0xD,  /* 0x000000 ~ 0x3FFFFF */
    SFLASH_PROTECTION_TOP_8MB          = 0xE,  /* 0x000000 ~ 0x7FFFFF */
    SFLASH_PROTECTION_BOTTOM_4KB       = 0x11, /* 0xFFF000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_BOTTOM_8KB       = 0x12, /* 0xFFE000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_BOTTOM_16KB      = 0x13, /* 0xFFC000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_BOTTOM_32KB      = 0x14, /* 0xFF8000 ~ 0xFFFFFF */
    SFLASH_PROTECTION_TOP_4KB          = 0x19, /* 0x000000 ~ 0x000FFF */
    SFLASH_PROTECTION_TOP_8KB          = 0x1A, /* 0x000000 ~ 0x001FFF */
    SFLASH_PROTECTION_TOP_16KB         = 0x1B, /* 0x000000 ~ 0x003FFF */
    SFLASH_PROTECTION_TOP_32KB         = 0x1C, /* 0x000000 ~ 0x007FFF */
} eQSPI_PROTECTION_AREA;

extern void SFlash_DriverInitialize(void);
extern int sflash_write(unsigned int addr, unsigned char *buf, int const);
extern int sflash_erase(unsigned int addr);
extern void sflash_os_env_parser(void);
extern char *get_env(const char *name);

extern int up_progmem_erasepage(unsigned int page);
extern int up_progmem_early_erasepage(unsigned int page);
extern unsigned int up_progmem_blocksize(void);
extern unsigned int up_progmem_write(unsigned int addr, const void *buf, unsigned int count);


#ifdef __cplusplus
}
#endif
#endif /* _FCACHE_DRV_H */
