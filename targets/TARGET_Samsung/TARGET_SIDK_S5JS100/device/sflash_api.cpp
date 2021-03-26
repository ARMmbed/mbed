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

#include <string.h>
#include "rtos/rtos.h"
#include "stdlib.h"
#include "cmsis.h"
#include "mbed_wait_api.h"
#include "flash_api.h"


/****************************************************************************
 * Private Data
 ****************************************************************************/
int flash_no_erase;
int flash_idx = 0;
#define S5JS100_FLASH_DELAYTIME 1
#define S5JS100_FLASH_WRITEUNIT 32
#define CACHE_LINE_MASK 0xffffffe0
#define CACHE_LINE_SIZE 32
Semaphore *g_sem = new Semaphore(1);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static void hw_delay_us(unsigned int Value)
{
    volatile unsigned i, j;

    for (i = 0; i < (Value * 2); i++)
        for (j = 0; j < 100; j++);
}

unsigned int up_progmem_pagesize(void)
{
    return S5JS100_FLASH_PAGE_SIZE;
}


unsigned int up_progmem_blocksize(void)
{
    return S5JS100_FLASH_BLOCK_SIZE;
}
/*
static void sflash_set_page_size(eQSPI_PAGE_SIZE size)
{
    //Bytes per Page
    modifyreg32(S5JS100_SFLASH_SFCON, 0xF << 8, size << 8);
}
*/
static unsigned int up_progmem_npages(void)
{
    return S5JS100_FLASH_SIZE / S5JS100_FLASH_PAGE_SIZE;
}

static int up_progmem_getaddress(int page)
{
    return S5JS100_FLASH_PADDR + S5JS100_FLASH_PAGE_SIZE * page;
}

int up_progmem_getpage(int addr)
{
    return (addr - S5JS100_FLASH_PADDR) / S5JS100_FLASH_PAGE_SIZE;
}


static void s5js100_sflash_disable_wp(void)
{
    /* someone has been disabled wp, we should wait until it's released */
    while (getreg32(S5JS100_SFLASH_SFCON) & SFLASH_SFCON_WP_DISABLE) ;
    modifyreg32(S5JS100_SFLASH_SFCON, SFLASH_SFCON_WP_MASK, SFLASH_SFCON_WP_DISABLE);
}

static void s5js100_sflash_enable_wp(void)
{
    modifyreg32(S5JS100_SFLASH_SFCON, SFLASH_SFCON_WP_MASK, SFLASH_SFCON_WP_ENABLE);
    // command 3. RDSR1 read Status Register 1
    modifyreg32(0x85020024, 0xFFFF, 0x0505);  
}

static uint8_t s5js100_sflash_read_status(void)
{
    return getreg8(S5JS100_SFLASH_RDSR);
}

/* return FLASH capacity in BYTE */
uint32_t s5js100_sflash_read_capacity(void)
{
    uint32_t capacity_type;
    capacity_type = (getreg32(S5JS100_SFLASH_RDID) & (0xFF << 16)) >> 16;
    capacity_type = (0x1 << ((capacity_type & 0xF) - 1)) * 1024 * 1024;
    return capacity_type / 8;
}

int s5js100_sflash_write_protect(eQSPI_PROTECTION_AREA area)
{
    int status_register;

    s5js100_sflash_disable_wp();

    status_register = getreg8(S5JS100_SFLASH_RDSR);
    //lldbg("status_register : 0x%x, area : 0x%x\n", status_register, area);

    /* send Write Enable */
    putreg8(1, S5JS100_SFLASH_WREN);
    while (!(getreg8(S5JS100_SFLASH_RDSR) & 0x2));

    /* write status register */
    status_register = (status_register & 0x83) | (area << 2);
    //lldbg("status_register write : 0x%x ==> ", status_register);
    putreg32(status_register | 0x0200, S5JS100_SFLASH_WRSR);
    while (getreg8(S5JS100_SFLASH_RDSR) & 0x1);

    status_register = getreg8(S5JS100_SFLASH_RDSR);
    //lldbg("0x%x\n", status_register);

    /* send Write Disable */
    putreg8(1, S5JS100_SFLASH_WRDI);
    while (getreg8(S5JS100_SFLASH_RDSR) & 0x2);

    s5js100_sflash_enable_wp();

    return 0;
}



/* SFlash_DriverInitialize: SFlash Driver Initialize function */
void SFlash_DriverInitialize()
{
    putreg32(0x8660060A, S5JS100_SFLASH_SFCON); /*disable write protect for FLASH stage changing*/
#if !DEVICE_QSPI 
    modifyreg32(0x85020074, 0x1, 1);    /*Set FAST READ */

    /* Enable Quad Read */
    putreg32(0x4, S5JS100_SFLASH_IO_MODE);
    putreg32(0x8, S5JS100_SFLASH_PERF_MODE);

    // command 3. RDSR2 read Status Register 2
    modifyreg32(0x85020024, 0xFF, 0x35);    //Set QE to Status2

    while (!(getreg8(S5JS100_SFLASH_BASE + 0xDC) & (0x1 << 1))) {
    };  /* Check FLASH has Quad Enabled */

    cal_clk_setrate(d1_qspi, 100000000);
#endif
    putreg32(0x0660061A, S5JS100_SFLASH_SFCON); //enable write protect + byte program

    /* change drive strength */
    modifyreg32(0x82021070, 0x3 << 8, 0x0 << 8);    //Drive strength CS to  (0x0)2mA
    modifyreg32(0x82021074, 0x3 << 8, 0x0 << 8);    //Drive strength SCK to  (x0)2mA
    modifyreg32(0x82021078, 0x3 << 8, 0x0 << 8);    //Drive strength SI to  (0x0)2mA
    modifyreg32(0x8202107C, 0x3 << 8, 0x0 << 8);    //Drive strength SO to  (0x0)2mA
    modifyreg32(0x82021080, 0x3 << 8, 0x0 << 8);    //Drive strength WP to  (0x0)2mA
    modifyreg32(0x82021084, 0x3 << 8, 0x0 << 8);    //Drive strength HLD to  (0x0)2mA
#if !DEVICE_QSPI 
    s5js100_sflash_write_protect(SFLASH_PROTECTION_NONE);
#endif
}

extern "C" {
    void s5js100_sflash_reinit(void)
    {
        cal_clk_setrate(d1_qspi, 100000000);
    }
}

static void local_memcpy(void *dst, void *src, size_t n)
{
    unsigned char *pout = (unsigned char *)dst;
    unsigned char *pin = (unsigned char *)src;
    while (n-- > 0) {
        *pout++ = *pin++;
    }
}

static int up_progmem_write_disabledcache(unsigned int addr, const void *buf, int count)
{
    int remain = count;
    char *pos = (char *)buf;

    g_sem->try_acquire();
    while (remain) {
        int tmp = remain;

        if (tmp > S5JS100_FLASH_WRITEUNIT) {
            tmp = S5JS100_FLASH_WRITEUNIT;
        }

        s5js100_sflash_disable_wp();

        /* Temporary code for testing */
        //memcpy((void *)(addr + S5JS100_SFLASH_WOFFSET), (void *)(pos), tmp);
        local_memcpy((void *)(addr + S5JS100_SFLASH_WOFFSET), (void *)(pos), tmp);

        /* Flash Mirror Address is device attribute, need to POC with Flash Read Address */
        /* invalidate cache for read address, not read and write in POC state */
        /* CM7 cache line 32bytes, align 32bytes */
        invalidate_dcache_by_addr((uint32_t *)(addr & CACHE_LINE_MASK), tmp + CACHE_LINE_SIZE);

        s5js100_sflash_enable_wp();

        pos += tmp;
        addr += tmp;
        remain -= tmp;

        hw_delay_us(1000 * S5JS100_FLASH_DELAYTIME);
    }
    g_sem->release();

    return count;
}


int sflash_write(unsigned int addr, unsigned char *buf, int count)
{
    unsigned int uf_start = (unsigned int)S5JS100_FLASH_FS_PADDR;
    unsigned int uf_end = S5JS100_FLASH_PADDR + S5JS100_FLASH_SIZE;
    /*
     * Check the request address is in a specific userflash area
     * We assumed that the userflash area's address is started from the end of
     * OS partition to the end of flash.
     */
    if (addr >= uf_start && addr < uf_end) {
        up_progmem_write_disabledcache(addr, buf, count);
        return 0;
    }

    int pagesize;
    int remain = count;

    pagesize = S5JS100_FLASH_PAGE_SIZE;

    g_sem->try_acquire();
    while (remain) {
        int tmp = remain;

        if (tmp > pagesize) {
            tmp = pagesize;
        }

        s5js100_sflash_disable_wp();

        /* Load and write data */
        local_memcpy((void *)addr, buf, tmp);

        /* Flush cache */
        clean_invalidate_dcache_by_addr((uint32_t *)(addr & CACHE_LINE_MASK), tmp + CACHE_LINE_SIZE);

        s5js100_sflash_enable_wp();

        buf += tmp;
        addr += tmp;
        remain -= tmp;
    }
    g_sem->release();

    return 0;
}

static int _is_erased(unsigned int address, int size)
{
    unsigned int *p = (unsigned int *)address;

    while (size > 0) {
        if (*p != 0xFFFFFFFF) {
            return 0;
        } else {
            p++;
            size -= 4;
        }
    }

    return 1;
}

int up_progmem_early_erasepage(unsigned int page)
{
    int addr;

    if (page >= up_progmem_npages()) {
        return -1;
    }

    addr = up_progmem_getaddress(page);
    /* skip erased block */
    if (_is_erased(addr, up_progmem_blocksize())) {
        return 0;
    }

    s5js100_sflash_disable_wp();

    /* Set sector address and then send erase command */
    putreg32(addr - S5JS100_FLASH_PADDR, S5JS100_SFLASH_ERASE_ADDRESS);
    putreg8(0xff, S5JS100_SFLASH_SE);

    /* Wait for the completion */
    while (s5js100_sflash_read_status() & 0x1) {
    };

    /* Invalidate cache */
    invalidate_dcache_by_addr((uint32_t *)(addr & CACHE_LINE_MASK)/* + S5JS100_FLASH_PADDR*/, up_progmem_blocksize());
    s5js100_sflash_enable_wp();


    return 0;
}

int up_progmem_erasepage(unsigned int page)
{
    int addr;

    if (page >= up_progmem_npages()) {
        return -1;
    }

    addr = up_progmem_getaddress(page);
    /* skip erased block */
    if (_is_erased(addr, up_progmem_blocksize())) {
        return 0;
    }

    g_sem->try_acquire();
    //s5js100_flash_take_sem();

    s5js100_sflash_disable_wp();

    /* Set sector address and then send erase command */
    putreg32(addr - S5JS100_FLASH_PADDR, S5JS100_SFLASH_ERASE_ADDRESS);
    putreg8(0xff, S5JS100_SFLASH_SE);

    /* Wait for the completion */
    while (s5js100_sflash_read_status() & 0x1) {
    };

    /* Invalidate cache */
    invalidate_dcache_by_addr((uint32_t *)(addr & CACHE_LINE_MASK)/* + S5JS100_FLASH_PADDR*/, up_progmem_blocksize());
    s5js100_sflash_enable_wp();


    g_sem->release();

    return 0;
}


unsigned int up_progmem_write(unsigned int addr, const void *buf, unsigned int count)
{
    return sflash_write(addr, (unsigned char *)buf, count);
}


int sflash_erase(unsigned int address)
{
    int page;
    page = up_progmem_getpage(address);
    return up_progmem_erasepage(page);
}

#define ENV_MAX 20
struct _env_list {
    char env_name[11];
    char env_val[41];
} env_list[ENV_MAX];

#define BOOTARG_VAL_OFFSET  0x40
#define BOOTARG_VAL_MAX_SIZE    0x40
void sflash_os_env_parser(void)
{
    char *env_addr, *ptr;
    int argc = 0;
    char buf[4096];
    uint32_t env_offset;

    if (s5js100_sflash_read_capacity() == 8 * 1024 * 1024) {
        env_offset = S5JS100_OS_ENV_OFFSET_8MB;
    }

    if (s5js100_sflash_read_capacity() == 16 * 1024 * 1024) {
        env_offset = S5JS100_OS_ENV_OFFSET_16MB;
    }

    env_addr = (char *)(S5JS100_FLASH_PADDR + env_offset + BOOTARG_VAL_OFFSET);
    memcpy(buf, env_addr, 4 * 1024);

    ptr = buf;

    while (argc < ENV_MAX) {
        char *arg = ptr;
        int i = 0, j = 1; //j from '='

        if (*ptr == 0xFF || *ptr == 0) {
            ptr += BOOTARG_VAL_OFFSET;//strtok(NULL, "\n\r\t ,");
            argc++;
            continue;
        }
        while (*(arg + i) != '=') {
            if (*(arg + i) == 0xFF || i >= 10) {
                return;
            }
            i++;
        }
        *(arg + i) = '\0';

        strcpy(env_list[argc].env_name, arg);

        while (j < BOOTARG_VAL_MAX_SIZE - i) {
            if (*(arg + i + j) == 0xFF || *(arg + i + j) == 0) {
                break;
            }
            j++;
        }
        *(arg + i + j) = '\0';

        strcpy(env_list[argc++].env_val, arg + i + 1);

        ptr += BOOTARG_VAL_OFFSET;//strtok(NULL, "\n\r\t ,");
    }

}

char *get_env(const char *name)
{
    int i;
    for (i = 0; i < ENV_MAX; i++) {
        if (!strcmp(name, env_list[i].env_name)) {
            return env_list[i].env_val;
        }
    }
    return 0;
}


#ifdef DEVICE_FLASH
/*************** flash_hal API ********************/
/* hal/flash_api.h                                */
int32_t flash_init(flash_t *obj)
{
    SFlash_DriverInitialize();
    return 0;
}

int32_t flash_free(flash_t *obj)
{
    return 0;
}

uint32_t flash_get_page_size(const flash_t *info)
{
    return 4; /*S5JS100_FLASH_PAGE_SIZE*/
}

uint32_t flash_get_sector_size(const flash_t *info, uint32_t addr)
{
    return up_progmem_blocksize();
}

uint32_t flash_get_start_address(const flash_t *info)
{
    return S5JS100_FLASH_FS_PADDR;
}

uint32_t flash_get_size(const flash_t *info)
{
    return S5JS100_FLASH_FS_SIZE;
}

int32_t flash_program_page(flash_t *obj, uint32_t addr, const uint8_t *data, uint32_t size)
{
    if (addr > S5JS100_FLASH_PADDR) {
        return sflash_write(addr, (unsigned char *)data, size);
    } else {
        local_memcpy((void *)addr, (void *)data, size);
        return 0;
    }
}
uint8_t flash_get_erase_value(const flash_t *obj)
{
    (void)obj;
    return 0xFF;
}

int32_t flash_erase_sector(flash_t *obj, uint32_t addr)
{
    if (addr > S5JS100_FLASH_PADDR) {
        return sflash_erase(addr);
    } else {
        memset((void *)addr, 0xFFFFFFFF, 4096);
        return 0;
    }
}
#endif

