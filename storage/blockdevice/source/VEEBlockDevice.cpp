/* Simple access class for Virtual EEPROM Emulation
 * Copyright (c) 2015 Robin Hourahane
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
#include "VEEBlockDevice.h"
#include "platform/mbed_assert.h"
#include "mbed_assert.h"
#include <stdlib.h>

#include "mbed_trace.h"
#define TRACE_GROUP "VEEF"

using namespace mbed;

#define VEE_TIMEOUT 10000

/* EEPROM physical address offset in each bank */
static uint32_t bank_offset[MX_EEPROMS] =
#ifdef COMPONENT_OSPIF
{
    0x00200000,
    0x01200000,
    0x02200000,
    0x03200000
};
#elif COMPONENT_SPINAND
    {
        0x00080000,
        0x04080000,
        0x08080000,
        0x0c080000
    };
#endif

VEEBlockDevice::VEEBlockDevice(
    BlockDevice *bd)
    : _bd(bd), _is_initialized(false)
{
    MBED_ASSERT(bd);
}

VEEBlockDevice::~VEEBlockDevice()
{
    deinit();
}

int VEEBlockDevice::init()
{
    int err;
    uint32_t bank, ofs;
    err = _bd->init();
    if (err) {
        goto fail;
    }
    if (_is_initialized) {
        return BD_ERROR_OK;
    }

    for (bank = 0; bank < MX_EEPROMS; bank++) {
        /* Check address validity */
#ifdef MX_FLASH_SUPPORT_RWW
        ofs = bank_offset[bank];
        if ((ofs / MX_FLASH_BANK_SIZE != bank) ||
                ((ofs + MX_EEPROM_BANK_SIZE - 1) / MX_FLASH_BANK_SIZE != bank)) {
            tr_error("init : invalid bank%lu offset\r\n", bank);
            err = VEEF_BD_ERROR_EINVAL;
            goto fail;
        }
#else
        for (ofs = bank + 1; ofs < MX_EEPROMS; ofs++) {
            if (abs((int)(bank_offset[ofs] - bank_offset[bank])) < MX_EEPROM_BANK_SIZE) {
                tr_error("init : offset conflict: bank%lu, bank %lu\r\n", bank, ofs);
                err = VEEF_BD_ERROR_EINVAL;
                goto fail;
            }
        }
#endif

        /* Init bank info */
        _eeprom.bi[bank].bank = bank;
        _eeprom.bi[bank].bank_offset = bank_offset[bank];

        /* Empty current block info */
        _eeprom.bi[bank].block = DATA_NONE32;
        _eeprom.bi[bank].block_offset = DATA_NONE32;

        /* Empty current page cache */
        _eeprom.bi[bank].cache_dirty = false;
        memset(&_eeprom.bi[bank].cache, DATA_NONE8, MX_EEPROM_ENTRY_SIZE);

        /* No obsoleted sector */
        _eeprom.bi[bank].dirty_block = DATA_NONE32;
        _eeprom.bi[bank].dirty_sector = DATA_NONE32;

        /* Reset R/W counter */
        _eeprom.rwCnt = 0;
    }

    /* Check RWWEE format */
    err = _ee_check_sys();
    if (err) {
        err = format();
        if (err) {
            tr_error("format : format failed\r\n");
            goto fail;
        }
        /* Check RWWEE format again after formatting*/
        err = _ee_check_sys();
        if (err) {
            tr_error("init : not found valid RWWEE format\r\n");
            goto fail;
        }
    }

#if COMPONENT_SPINAND
#ifdef MX_EEPROM_PC_PROTECTION
    /* Check if power fail during gc */
    err = _ee_check_gc_power_fail();
    if (err) {
        tr_error("init : check power fail failed\r\n");
        goto fail;
    }
#endif
#endif

    /* Init done */
    _is_initialized = true;
    return BD_ERROR_OK;

fail:
    _is_initialized = false;
    return err;
}

int VEEBlockDevice::deinit()
{
    if (!_is_initialized) {
        return BD_ERROR_OK;
    }

    _is_initialized = false;

    return _bd->deinit();
}

int VEEBlockDevice::read(void *buffer, bd_addr_t addr, bd_size_t size)
{
    if (!_is_initialized) {
        return BD_ERROR_DEVICE_ERROR;
    }

    return _ee_rw(addr, size, (uint8_t *)buffer, false);
    //return _bd->read(buffer, addr, size);
}

int VEEBlockDevice::program(const void *buffer, bd_addr_t addr, bd_size_t size)
{
    if (!_is_initialized) {
        return BD_ERROR_DEVICE_ERROR;
    }
    if (!is_valid_program(addr, size)) {
        return BD_ERROR_DEVICE_ERROR;
    }

    return _ee_rw(addr, size, (uint8_t *)buffer, true);
    //return _bd->program(buffer, addr, size);
}

int VEEBlockDevice::erase(bd_addr_t addr, bd_size_t size)
{
    // No erase needed
    return 0;
}

bd_size_t VEEBlockDevice::get_read_size() const
{
    return 1;
}

bd_size_t VEEBlockDevice::get_program_size() const
{
    return 1;
}

bd_size_t VEEBlockDevice::get_erase_size() const
{
    return 1;
}

bd_size_t VEEBlockDevice::size() const
{
    return MX_EEPROM_TOTAL_SIZE;
}

const char *VEEBlockDevice::get_type() const
{
    return "VEE";
}

int VEEBlockDevice::_ee_device_read(bd_addr_t addr, bd_size_t size, void *buffer)
{
    int status;

    tr_debug("_ee_device_read: read  addr 0x%08lx, size %lu\r\n", addr, size);

    /* Do the real read here */
    status = _bd->read(buffer, addr, size);

    MBED_ASSERT(!status);

    return (!status ? VEEF_BD_ERROR_OK : VEEF_BD_ERROR_EIO);
}

int VEEBlockDevice::_ee_device_write(bd_addr_t addr, bd_size_t size, void *buffer)
{
    int status;

    tr_debug("_ee_device_write: write addr 0x%08lx, size %lu\r\n", addr, size);

    /* Do the real write here */
    status = _bd->program(buffer, addr, size);

    MBED_ASSERT(!status);

    return (!status ? VEEF_BD_ERROR_OK : VEEF_BD_ERROR_EIO);
}

int VEEBlockDevice::_ee_device_erase(bd_addr_t addr, bd_size_t size)
{
    int status;

    tr_debug("_ee_device_erase: erase addr 0x%08lx, size %lu\r\n", addr, size);

    /* Do the real erase here */
    status = _bd->erase(addr, size);

    MBED_ASSERT(!status);

    return (!status ? VEEF_BD_ERROR_OK : VEEF_BD_ERROR_EIO);
}

#if COMPONENT_OSPIF
int VEEBlockDevice::_ee_read_sys(struct bank_info *bi, uint32_t entry,
                                 struct system_entry *sys)
{
    int status;
    bd_addr_t addr;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) ||
            (bi->block >= MX_EEPROM_BLOCKS) ||
            (entry >= MX_EEPROM_SYSTEM_ENTRIES)) {
        return VEEF_BD_ERROR_EINVAL;
    }

    addr = bi->block_offset + MX_EEPROM_SYSTEM_SECTOR_OFFSET +
           entry * MX_EEPROM_SYSTEM_ENTRY_SIZE;

    /* Read system entry */
    status = _ee_device_read(addr, sizeof(*sys), sys);
    if (status) {
        tr_error("_ee_read_sys: fail to read, bank %lu, block %lu, entry %lu\r\n",
                 bi->bank, bi->block, entry);
        return status;
    }

    /* Check system entry */
    if ((sys->id != MFTL_ID && sys->id != DATA_NONE16) ||
            (sys->cksum != (sys->id ^ sys->ops ^ sys->arg))) {
        tr_error("_ee_read_sys: corrupted entry, bank %lu, block %lu, entry %lu\r\n",
                 bi->bank, bi->block, entry);
        return VEEF_BD_ERROR_EIO;
    }

    return VEEF_BD_ERROR_OK;
}

int VEEBlockDevice::_ee_update_sys(struct bank_info *bi, uint32_t block,
                                   ee_ops ops, uint32_t arg)
{
    int status;
    bd_addr_t addr;
    struct system_entry sys;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) ||
            (block >= MX_EEPROM_BLOCKS) ||
            (bi->sys_entry[block] >= MX_EEPROM_SYSTEM_ENTRIES)) {
        return VEEF_BD_ERROR_EINVAL;
    }

    addr = bi->bank_offset + block * MX_EEPROM_CLUSTER_SIZE +
           MX_EEPROM_SYSTEM_SECTOR_OFFSET;

    if (++bi->sys_entry[block] == MX_EEPROM_SYSTEM_ENTRIES) {

        /* Erase system sector */
        status = _ee_device_erase(addr, MX_FLASH_SECTOR_SIZE);
        if (status) {
            tr_error("_ee_update_sys: fail to erase, bank %lu, block %lu, sector %d\r\n",
                     bi->bank, block, MX_EEPROM_SYSTEM_SECTOR);

            bi->sys_entry[block] = DATA_NONE32;
            return status;
        }

        /* Round-robin method */
        bi->sys_entry[block] = 0;
    }

    /* Fill system entry */
    sys.id = MFTL_ID;
    sys.ops = ops;
    sys.arg = arg;
    sys.cksum = sys.id ^ sys.ops ^ sys.arg;

    addr += bi->sys_entry[block] * MX_EEPROM_SYSTEM_ENTRY_SIZE;

    /* Update system info */
    status = _ee_device_write(addr, sizeof(sys), &sys);
    if (status) {
        tr_error("_ee_update_sys: fail to update, bank %lu, block %lu, entry %lu\r\n",
                 bi->bank, block, bi->sys_entry[block]);
        return status;
    }

    return VEEF_BD_ERROR_OK;
}
#elif COMPONENT_SPINAND
int VEEBlockDevice::_ee_read_sys(struct bank_info *bi, uint32_t sector, uint32_t system_entry_addr,
                                 struct system_entry *sys)
{
    int status;
    uint32_t addr;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) || (bi->block >= MX_EEPROM_BLOCKS) || (sector >= MX_EEPROM_SECTORS_PER_CLUSTER)) {
        return VEEF_BD_ERROR_EINVAL;
    }

    addr = bi->block_offset + sector * MX_EEPROM_SECTOR_OFFSET + system_entry_addr;

    /* Read system entry */
    status = _ee_device_read(addr, sizeof(*sys), sys);
    if (status) {
        tr_error("ee_rdsys: fail to read, bank %lu, block %lu, sector %lu\r\n", bi->bank, bi->block, sector);
        return status;
    }

    /* Check system entry */
    if ((sys->id != MFTL_ID && sys->id != DATA_NONE16) || (sys->cksum != (sys->id ^ sys->ops ^ sys->arg))) {
        tr_error("ee_rdsys: corrupted entry, bank %lu, block %lu, sector %lu\r\n", bi->bank, bi->block, sector);
        return VEEF_BD_ERROR_EIO;
    }

    return VEEF_BD_ERROR_OK;
}

int VEEBlockDevice::_ee_update_sys(struct bank_info *bi, uint32_t sector, uint32_t system_entry_addr,
                                   ee_ops ops, uint32_t arg)
{
    int status;
    uint32_t addr;
    struct system_entry sys;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) || (bi->block >= MX_EEPROM_BLOCKS) || (sector >= MX_EEPROM_SECTORS_PER_CLUSTER)) {
        return VEEF_BD_ERROR_EINVAL;
    }

    addr = bi->block_offset + sector * MX_EEPROM_SECTOR_OFFSET + system_entry_addr;

    /* Fill system entry */
    sys.id = MFTL_ID;
    sys.ops = ops;
    sys.arg = arg;
    sys.cksum = sys.id ^ sys.ops ^ sys.arg;

    /* Update system info */
    status = _ee_device_write(addr, sizeof(sys), &sys);
    if (status) {
        tr_error("ee_wrsys: fail to update, bank %lu, sector %lu, entry %lu\r\n",
                 bi->bank, sector, bi->sys_entry[sector]);
        return status;
    }

    return VEEF_BD_ERROR_OK;
}
#endif

int VEEBlockDevice::_ee_read(struct bank_info *bi, uint32_t entry, void *buffer,
                             bool header)
{
    int status;
    bd_addr_t addr, size, cksum;
    struct eeprom_entry *cache = (eeprom_entry *)buffer;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) ||
            (bi->block >= MX_EEPROM_BLOCKS) ||
            (entry >= MX_EEPROM_ENTRIES_PER_CLUSTER)) {
        return VEEF_BD_ERROR_EINVAL;
    }

#if COMPONENT_OSPIF
    addr = entry * MX_EEPROM_ENTRY_SIZE + bi->block_offset;
#elif COMPONENT_SPINAND
    addr = bi->block_offset + MX_EEPROM_SECTOR_OFFSET * (entry / MX_EEPROM_ENTRIES_PER_SECTOR) +
           ((entry % MX_EEPROM_ENTRIES_PER_SECTOR + 2) % 4) * MX_EEPROM_ENTRY_SIZE +   // +2 : the first and second entry is for system entry
           ((entry % MX_EEPROM_ENTRIES_PER_SECTOR + 2) / 4) * MX_FLASH_PAGE_OFFSET;
#endif
    size = header ? MX_EEPROM_HEADER_SIZE : MX_EEPROM_ENTRY_SIZE;

    /* Do the real read */
    status = _ee_device_read(addr, size, buffer);
    if (status) {
        tr_error("ee_rddat: fail to read %s, bank %lu, block %lu, entry %lu\r\n",
                 header ? "header" : "entry", bi->bank, bi->block, entry);
        return status;
    }

    /* Check entry address */
    cksum = cache->header.LPA + cache->header.LPA_inv;
    if ((cksum != DATA_NONE8) && (cksum != DATA_NONE8 + DATA_NONE8)) {
        tr_error("ee_rddat: corrupted entry LPA 0x%02x, inv 0x%02x, "
                 "bank %lu, block %lu, entry %lu\r\n",
                 cache->header.LPA, cache->header.LPA_inv,
                 bi->bank, bi->block, entry);
        return VEEF_BD_ERROR_EIO;
    }

    _eeprom.rwCnt++;
    return VEEF_BD_ERROR_OK;
}

int VEEBlockDevice::_ee_write(struct bank_info *bi, uint32_t entry, void *buffer)
{
    int status;
    bd_addr_t addr;
    struct eeprom_entry *cache = (eeprom_entry *)buffer;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) ||
            (bi->block >= MX_EEPROM_BLOCKS) ||
            (entry >= MX_EEPROM_ENTRIES_PER_CLUSTER)) {
        return VEEF_BD_ERROR_EINVAL;
    }

#if COMPONENT_OSPIF
    addr = entry * MX_EEPROM_ENTRY_SIZE + bi->block_offset;
#elif COMPONENT_SPINAND
    addr = bi->block_offset + MX_EEPROM_SECTOR_OFFSET * (entry / MX_EEPROM_ENTRIES_PER_SECTOR) +
           ((entry % MX_EEPROM_ENTRIES_PER_SECTOR + 2) % 4) * MX_EEPROM_ENTRY_SIZE +
           ((entry % MX_EEPROM_ENTRIES_PER_SECTOR + 2) / 4) * MX_FLASH_PAGE_OFFSET;
#endif

    /* Calculate redundant LPA */
    cache->header.LPA_inv = ~cache->header.LPA;

    _eeprom.rwCnt++;

    /* Do the real write */
    status = _ee_device_write(addr, MX_EEPROM_ENTRY_SIZE, cache);
    if (status) {
        tr_error("ee_wrdat: fail to write, bank %lu, block %lu, entry %lu\r\n",
                 bi->bank, bi->block, entry);
    }

    return status;
}

int VEEBlockDevice::_ee_erase(struct bank_info *bi)
{
    int status;
    bd_addr_t addr;

    /* Check address validity */
    if (bi->bank >= MX_EEPROMS) {
        return VEEF_BD_ERROR_EINVAL;
    }
    if ((bi->dirty_block >= MX_EEPROM_BLOCKS) ||
            (bi->dirty_sector >= MX_EEPROM_DATA_SECTORS)) {
        return VEEF_BD_ERROR_OK;
    }

#if COMPONENT_OSPIF
    addr = bi->dirty_sector * MX_FLASH_SECTOR_SIZE +
           bi->dirty_block * MX_EEPROM_CLUSTER_SIZE +
           bi->bank_offset;
#elif COMPONENT_SPINAND
    addr = bi->dirty_sector * MX_EEPROM_SECTOR_OFFSET +
           bi->dirty_block * MX_EEPROM_CLUSTER_OFFSET +
           bi->bank_offset;
#endif

#ifdef MX_DEBUG
    /* Erase count statistics */
    bi->eraseCnt[bi->dirty_block][bi->dirty_sector]++;
#endif

#ifdef MX_EEPROM_PC_PROTECTION
    /* Erase begin */
#if COMPONENT_OSPIF
    _ee_update_sys(bi, bi->dirty_block, OPS_ERASE_BEGIN, bi->dirty_sector);
#elif COMPONENT_SPINAND
    _ee_update_sys(bi, bi->dirty_sector, SYS_ENTRY_ADDR_E_B, OPS_ERASE_BEGIN, DATA_NONE16);
#endif
#endif

    /* Erase obsoleted sector */
    status = _ee_device_erase(addr, MX_FLASH_SECTOR_SIZE);
    if (status) {
        tr_error("_ee_erase: fail to erase, bank %lu, block %lu, sector %lu\r\n",
                 bi->bank, bi->dirty_block, bi->dirty_sector);
    }

#ifdef MX_EEPROM_PC_PROTECTION
    /* Erase end, XXX: will block RWE */
#if COMPONENT_OSPIF
//    _ee_update_sys(bi, bi->dirty_block, OPS_ERASE_END, bi->dirty_sector);
#elif COMPONENT_SPINAND
    _ee_update_sys(bi, bi->dirty_sector, SYS_ENTRY_ADDR_E_E, OPS_ERASE_END, DATA_NONE16);
#endif
#endif

    if (bi->dirty_block == bi->block) {
        /* Mark as free or bad sector */
        if (!status) {
            bi->p2l[bi->dirty_sector] = DATA_NONE8;
#if COMPONENT_SPINAND
            bi->latest_used_entry_per_sector[bi->dirty_sector] = DATA_NONE8;
#endif
        } else {
            bi->p2l[bi->dirty_sector] = MX_EEPROM_LPAS_PER_CLUSTER;
        }
    }

    bi->dirty_block = DATA_NONE32;
    bi->dirty_sector = DATA_NONE32;

    return status;
}

uint32_t VEEBlockDevice::_ee_find_latest(struct bank_info *bi, uint32_t LPA, bool free)
{
    int status;
    struct eeprom_header header;
    uint32_t ofs, entry, cnt = 0, latest = DATA_NONE32;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) ||
            (bi->block >= MX_EEPROM_BLOCKS) ||
            (LPA >= MX_EEPROM_LPAS_PER_CLUSTER)) {
        return DATA_NONE32;
    }

    ofs = bi->l2ps[LPA];

    /* No mapping */
    if (ofs >= MX_EEPROM_DATA_SECTORS) {
        return DATA_NONE32;
    }

    ofs *= MX_EEPROM_ENTRIES_PER_SECTOR;
    entry = bi->l2pe[LPA];

    /* Check if entry mapping exits */
    if (entry) {
        MBED_ASSERT(entry < MX_EEPROM_ENTRIES_PER_SECTOR);

        if (!free) {
            return entry + ofs;
        } else if (entry == MX_EEPROM_ENTRIES_PER_SECTOR - 1) {
            return DATA_NONE32;
        } else {
            cnt = entry + 1;
        }
    }

    /* Read each entry to find the latest, TODO: binary search method */
    for (entry = ofs + cnt; cnt < MX_EEPROM_ENTRIES_PER_SECTOR; cnt++, entry++) {
        /* Read entry header, XXX: Potential risk to check header only? */
        status = _ee_read(bi, entry, &header, true);
        if (status) {
            tr_error("ee_latst: fail to read entry %lu\r\n", entry);
            continue;
        }

        if (header.LPA == LPA) {
            /* Found newer version */
            latest = entry;
        } else if (header.LPA == DATA_NONE8) {
            /* Empty entry */
            break;
        } else {
            tr_error("ee_latst: corrupted entry, bank %lu, block %lu, entry %lu\r\n",
                     bi->bank, bi->block, entry);
        }
    }

    /* Update L2PE mapping */
    if (latest != DATA_NONE32) {
        bi->l2pe[LPA] = latest - ofs;
    }

    /* Return latest entry */
    if (!free) {
        MBED_ASSERT(latest != DATA_NONE32);
        return latest;
    }

    /* Return next free entry */
    if (cnt != MX_EEPROM_ENTRIES_PER_SECTOR) {
        return entry;
    }

    return DATA_NONE32;
}

int VEEBlockDevice::_ee_build_mapping(struct bank_info *bi, uint32_t block)
{
    struct eeprom_header header;
    uint32_t sector, entry, victim;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) || (block >= MX_EEPROM_BLOCKS)) {
        goto fail;
    }

    /* Reset L2P and P2L mappings */
    bi->block = block;
    bi->block_offset = block * MX_EEPROM_CLUSTER_SIZE + bi->bank_offset;
    memset(bi->l2ps, DATA_NONE8, sizeof(bi->l2ps));
    memset(bi->l2pe, 0, sizeof(bi->l2pe));
    memset(bi->p2l, DATA_NONE8, sizeof(bi->p2l));

    for (sector = 0; sector < MX_EEPROM_DATA_SECTORS; sector++) {
        entry = sector * MX_EEPROM_ENTRIES_PER_SECTOR;

        /* Read the first entry header of each sector */
        if (_ee_read(bi, entry, &header, true)) {
            tr_error("ee_build: fail to read entry %lu\r\n", entry);

            /* XXX: Potential risk to erase? */
            victim = sector;
            goto erase;
        }

        /* Free sector */
        if (header.LPA == DATA_NONE8) {
            continue;
        }

        /* Update P2L mapping */
        bi->p2l[sector] = header.LPA;

        if (header.LPA < MX_EEPROM_LPAS_PER_CLUSTER) {
            /* Update L2P mapping */
            if (bi->l2ps[header.LPA] == DATA_NONE8) {
                bi->l2ps[header.LPA] = sector;
                continue;
            }

            /* Handle mapping conflict */
            victim = bi->l2ps[header.LPA];
            entry = _ee_find_latest(bi, header.LPA, true);
            if (entry / MX_EEPROM_ENTRIES_PER_SECTOR == victim) {
                victim = sector;
            }
        } else {
            victim = sector;
        }

erase:
        /* Erase obsoleted sector */
        bi->dirty_block = block;
        bi->dirty_sector = victim;
        if (_ee_erase(bi)) {
            tr_error("ee_build: fail to erase sector %lu\r\n", victim);
        }

        /* Repair L2P mapping */
        if (victim != sector) {
            bi->l2ps[header.LPA] = sector;
            bi->l2pe[header.LPA] = 0;
        }
    }

    return VEEF_BD_ERROR_OK;
fail:
    bi->block = DATA_NONE32;
    bi->block_offset = DATA_NONE32;
    return VEEF_BD_ERROR_EINVAL;
}

uint32_t VEEBlockDevice::_ee_search_free(struct bank_info *bi, uint32_t LPA)
{
    uint32_t entry, sector, cnt;

    /* Check if corresponding sector used up */
    entry = _ee_find_latest(bi, LPA, true);
    if (entry < MX_EEPROM_ENTRIES_PER_CLUSTER) {
        return entry;
    }

    /* Pick a random sector to start the search */
    sector = rand() % MX_EEPROM_DATA_SECTORS;

    /* Search from the random sector to the end sector */
    for (cnt = sector; cnt < MX_EEPROM_DATA_SECTORS; cnt++) {
        if (bi->p2l[cnt] == DATA_NONE8) {
            entry = cnt * MX_EEPROM_ENTRIES_PER_SECTOR;
            return entry;
        }
    }

    /* Search from the start sector to the random sector */
    for (cnt = 0; cnt < sector; cnt++) {
        if (bi->p2l[cnt] == DATA_NONE8) {
            entry = cnt * MX_EEPROM_ENTRIES_PER_SECTOR;
            return entry;
        }
    }

    return DATA_NONE32;
}

int VEEBlockDevice::_ee_read_page(struct bank_info *bi, uint32_t LPA)
{
    uint32_t entry;
    int status, retries = 0;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) ||
            (bi->block >= MX_EEPROM_BLOCKS) ||
            (LPA >= MX_EEPROM_LPAS_PER_CLUSTER)) {
        status = VEEF_BD_ERROR_EINVAL;
        goto fail;
    }

    /* Find the latest version */
    entry = _ee_find_latest(bi, LPA, false);

    /* Fresh read */
    if (entry >= MX_EEPROM_ENTRIES_PER_CLUSTER) {
        memset(bi->cache.data, DATA_NONE8, MX_EEPROM_PAGE_SIZE);
        bi->cache.header.LPA = LPA;
        return VEEF_BD_ERROR_OK;
    }

retry:
    /* Do the real read */
    if (_ee_read(bi, entry, &bi->cache, false) || bi->cache.header.LPA != LPA) {
        tr_error("ee_rpage: fail to read entry %lu\r\n", entry);

        if (retries++ < MX_EEPROM_READ_RETRIES) {
#ifdef MX_EEPROM_READ_ROLLBACK
            /* Roll back to the prior version */
            if (entry % MX_EEPROM_ENTRIES_PER_SECTOR) {
                entry--;
            }
#endif
            goto retry;
        }

        status = VEEF_BD_ERROR_EIO;
        goto fail;
    }

    return VEEF_BD_ERROR_OK;
fail:
    bi->cache_dirty = false;
    bi->cache.header.LPA = DATA_NONE8;
    return status;
}

int VEEBlockDevice::_ee_write_page(struct bank_info *bi, uint32_t LPA)
{
    uint32_t entry, ofs, sector;
    int status, retries = 0;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) ||
            (bi->block >= MX_EEPROM_BLOCKS) ||
            (LPA >= MX_EEPROM_LPAS_PER_CLUSTER)) {
        return VEEF_BD_ERROR_EINVAL;
    }

retry:
    /* Find next free entry */
    entry = _ee_search_free(bi, LPA);
    if (entry >= MX_EEPROM_ENTRIES_PER_CLUSTER) {
        tr_error("_ee_wpage: no free entry left, bank %lu, block %lu\r\n",
                 bi->bank, bi->block);
        return VEEF_BD_ERROR_ENOSPC;
    }

    /* Do the real write */
    status = _ee_write(bi, entry, &bi->cache);
    if (status) {
        tr_error("_ee_wpage: fail to write entry %lu\r\n", entry);

        if (retries++ < MX_EEPROM_WRITE_RETRIES) {
            goto retry;
        }

        return status;
    }

    ofs = entry % MX_EEPROM_ENTRIES_PER_SECTOR;

#if COMPONENT_OSPIF
    if (ofs) {
        /* Update L2E mapping */
        bi->l2pe[LPA] = ofs;
    } else {
        ofs = bi->l2ps[LPA];
        if (ofs < MX_EEPROM_DATA_SECTORS) {
            /* Obsolete sector */
            bi->dirty_block = bi->block;
            bi->dirty_sector = ofs;
        }

        /* Update L2P and P2L mapping */
        ofs = entry / MX_EEPROM_ENTRIES_PER_SECTOR;
        bi->l2ps[LPA] = ofs;
        bi->l2pe[LPA] = 0;
        bi->p2l[ofs] = LPA;
    }
#elif COMPONENT_SPINAND
    sector = entry / MX_EEPROM_ENTRIES_PER_SECTOR;
    /* Update L2E mapping */
    bi->l2pe[LPA] = ofs;
    bi->l2ps[LPA] = sector;
    bi->p2l[sector] = LPA;
    if (bi->l2ps_group[LPA / MX_EEPROM_LPAS_PER_SECTOR] == DATA_NONE8) {
        bi->l2ps_group[LPA / MX_EEPROM_LPAS_PER_SECTOR] = sector;
    }
    bi->latest_used_entry_per_sector[sector] = ofs;
#endif
    /* Clean page cache */
    bi->cache_dirty = false;

    return VEEF_BD_ERROR_OK;
}

int VEEBlockDevice::_ee_rw_buffer(struct bank_info *bi, bd_addr_t addr, bd_size_t size,
                                  uint8_t *buffer, bool rw)
{
    int status;
    uint32_t block, page, ofs;

    /* Calculate current block, page, offset */
    block = addr / MX_EEPROM_BLOCK_SIZE;
    ofs = addr % MX_EEPROM_BLOCK_SIZE;
    page = ofs / MX_EEPROM_PAGE_SIZE;
    ofs = ofs % MX_EEPROM_PAGE_SIZE;

    if ((bi->block != block) || (bi->cache.header.LPA != page)) {
        /* Flush dirty page cache */
        if (bi->cache_dirty) {
            status = _ee_write_page(bi, bi->cache.header.LPA);
            if (status) {
                tr_error("_ee_rwbuf: fail to flush page cache\r\n");
                return status;
            }
        }

        /* Build mapping */
        if (bi->block != block) {
            status = _ee_build_mapping(bi, block);
            if (status) {
                tr_error("_ee_rwbuf: fail to build mapping table, bank %lu, block %lu\r\n",
                         bi->bank, bi->block);
                return status;
            }
        }

        /* Fill page cache */
        if (!rw || size < MX_EEPROM_PAGE_SIZE) {
            status = _ee_read_page(bi, page);
            if (status) {
                tr_error("_ee_rwbuf: fail to fill page cache\r\n");
                bi->cache.header.LPA = DATA_NONE8;
                return status;
            }
        } else {
            bi->cache.header.LPA = page;
        }
    }

    /* Update page cache/buffer */
    if (rw) {
        memcpy(&bi->cache.data[ofs], buffer, size);
        bi->cache_dirty = true;
    } else {
        memcpy(buffer, &bi->cache.data[ofs], size);
    }

    /* Handle obsoleted sector */
    if (_ee_erase(bi)) {
        tr_error("_ee_rwbuf: fail to erase\r\n");
    }

    return VEEF_BD_ERROR_OK;
}


#if (MX_EEPROM_HASH_AlGORITHM == MX_EEPROM_HASH_CROSSBANK)

int VEEBlockDevice::_ee_rw(bd_addr_t addr, bd_size_t size, uint8_t *buffer, bool rw)
{
    int status;
    struct bank_info *bi;
    uint32_t page, ofs, bank, rwpos, rwlen;

    if (addr + size > MX_EEPROM_TOTAL_SIZE) {
        return VEEF_BD_ERROR_EINVAL;
    }

    /* Determine the rwpos and rwlen */
    page = addr / MX_EEPROM_PAGE_SIZE;
    ofs = addr % MX_EEPROM_PAGE_SIZE;
    bank = page % MX_EEPROMS;
    page = page / MX_EEPROMS;
    rwpos = page * MX_EEPROM_PAGE_SIZE + ofs;
    rwlen = min_t(uint32_t, MX_EEPROM_PAGE_SIZE - ofs, size);

    /* Loop to R/W each logical page */
    while (size) {
        bi = &_eeprom.bi[bank];

        /* Only allow one request per bank per time */
        _bank_mutex[bank].lock();

        status = _ee_rw_buffer(bi, rwpos, rwlen, buffer, rw);

        _bank_mutex[bank].unlock();

        if (status) {
            tr_error("_ee_toprw: fail to %s laddr 0x%08lx, size %lu\r\n",
                     rw ? "write" : "read", addr, rwlen);
            return status;
        }

        /* Calculate the next rwpos and rwlen */
        buffer += rwlen;
        addr += rwlen;
        size -= rwlen;

        if (ofs) {
            rwpos -= ofs;
            ofs = 0;
        }

        if (++bank == MX_EEPROMS) {
            bank = 0;
            rwpos += MX_EEPROM_PAGE_SIZE;
        }

        rwlen = min_t(uint32_t, MX_EEPROM_PAGE_SIZE, size);
    }

    return VEEF_BD_ERROR_OK;
}

#elif (MX_EEPROM_HASH_AlGORITHM == MX_EEPROM_HASH_HYBRID)

#define MX_EEPROM_SUPERBLOCK_SIZE    (MX_EEPROM_BLOCK_SIZE * MX_EEPROMS)
int VEEBlockDevice::_ee_rw(bd_addr_t addr, bd_size_t size, uint8_t *buffer, bool rw)
{
    int status;
    struct bank_info *bi;
    uint32_t ofs, bank, base, size, rwpos, rwlen;

    /* Determine the rwpos and rwlen */
    ofs = addr / MX_EEPROM_BLOCK_SIZE;
    bank = ofs % MX_EEPROMS;
    base = (ofs / MX_EEPROMS) * MX_EEPROM_BLOCK_SIZE;
    size = addr % MX_EEPROM_BLOCK_SIZE;
    rwpos = base + size;
    ofs = size % MX_EEPROM_PAGE_SIZE;
    rwlen = min_t(uint32_t, MX_EEPROM_PAGE_SIZE - ofs, size);
    size = MX_EEPROM_BLOCK_SIZE - size;

    /* Loop to R/W each logical page */
    while (size) {
        bi = &_eeprom.bi[bank];

        /* Only allow one request per bank per time */
        _bank_mutex[bank].lock();

        while (size && size) {
            status = _ee_rw_buffer(bi, rwpos, rwlen, buffer, rw);
            if (status) {
                tr_error("_ee_toprw: fail to %s laddr 0x%08lx, size %lu\r\n",
                         rw ? "write" : "read", addr, rwlen);
                return status;
            }

            buffer += rwlen;
            rwpos += rwlen;
            addr += rwlen;
            size -= rwlen;
            size -= rwlen;
            rwlen = min_t(uint32_t, MX_EEPROM_PAGE_SIZE, size);
        }

        _bank_mutex[bank].unlock();

        if (++bank == MX_EEPROMS) {
            bank = 0;
            base += MX_EEPROM_BLOCK_SIZE;
        }

        rwpos = base;
        size = MX_EEPROM_BLOCK_SIZE;
    }

    return VEEF_BD_ERROR_OK;
}

#elif (MX_EEPROM_HASH_AlGORITHM == MX_EEPROM_HASH_SEQUENTIAL)

int VEEBlockDevice::_ee_rw(bd_addr_t addr, bd_size_t size, uint8_t *buffer, bool rw)
{
    int status;
    struct bank_info *bi;
    uint32_t ofs, bank, rwpos, rwlen;

    if (addr + size > MX_EEPROM_TOTAL_SIZE) {
        return VEEF_BD_ERROR_EINVAL;
    }

    /* Determine the rwpos and rwlen */
    bank = addr / MX_EEPROM_SIZE;
    ofs = addr % MX_EEPROM_SIZE;
    rwpos = ofs;
    ofs = ofs % MX_EEPROM_PAGE_SIZE;
    rwlen = min_t(uint32_t, MX_EEPROM_PAGE_SIZE - ofs, size);

    /* Loop to R/W each logical page */
    while (size) {
        bi = &_eeprom.bi[bank];

        /* Only allow one request per bank per time */
        _bank_mutex[bank].lock();

        do {
            status = _ee_rw_buffer(bi, rwpos, rwlen, buffer, rw);
            if (status) {
                tr_error("_ee_toprw: fail to %s laddr 0x%08lx, size %lu\r\n",
                         rw ? "write" : "read", addr, rwlen);
                return status;
            }

            /* Calculate the next rwpos and rwlen */
            buffer += rwlen;
            addr += rwlen;
            rwpos += rwlen;
            size -= rwlen;
            rwlen = min_t(uint32_t, MX_EEPROM_PAGE_SIZE, size);

        } while (size && (rwpos != MX_EEPROM_SIZE));

        _bank_mutex[bank].unlock();

        bank++;
        rwpos = 0;
    }

    return VEEF_BD_ERROR_OK;
}

#endif

int VEEBlockDevice::_eeprom_wb(struct bank_info *bi)
{
    /* Write page cache back */
    if (_ee_write_page(bi, bi->cache.header.LPA)) {
        tr_error("ee_wback: fail to flush page cache\r\n");
        return VEEF_BD_ERROR_EIO;
    }

    /* Handle obsoleted sector */
    if (_ee_erase(bi)) {
        tr_error("ee_wback: fail to erase\r\n");
        return VEEF_BD_ERROR_EIO;
    }

    return VEEF_BD_ERROR_OK;
}

int VEEBlockDevice::program_back(void)
{
    uint32_t bank;
    int status = VEEF_BD_ERROR_OK;
    struct bank_info *bi;

    if (!_is_initialized) {
        return VEEF_BD_ERROR_ENODEV;
    }

    /* Loop to check each bank */
    for (bank = 0; bank < MX_EEPROMS; bank++) {
        bi = &_eeprom.bi[bank];

        /* Check dirty cache */
        if (!bi->cache_dirty) {
            continue;
        }

        _bank_mutex[bank].lock();

        /* Double check */
        if (bi->cache_dirty) {
            /* Write back */
            if (_eeprom_wb(bi)) {
                tr_error("_ee_wback: fail to write back\r\n");
                status = VEEF_BD_ERROR_EIO;
            }
        }

        _bank_mutex[bank].unlock();
    }

    return status;
}

int VEEBlockDevice::sync_program(const void *buffer, bd_addr_t addr, bd_size_t size)
{
    return (program(buffer, addr, size) || program_back());
}

int VEEBlockDevice::flush(void)
{
    int status = VEEF_BD_ERROR_OK;
#ifdef MX_EEPROM_PC_PROTECTION
    struct bank_info *bi;
    uint32_t bank, block;
#endif

    status = program_back();
    if (status) {
        return status;
    }

#ifdef MX_EEPROM_PC_PROTECTION
    /* Update system entry */
    for (bank = 0; bank < MX_EEPROMS; bank++) {
        bi = &_eeprom.bi[bank];

        for (block = 0; block < MX_EEPROM_BLOCKS; block++) {
            _bank_mutex[bank].lock();

#if COMPONENT_OSPIF
            if (_ee_update_sys(bi, block, OPS_NONE, DATA_NONE32)) {
                tr_error("flush: fail to update system entry\r\n");
                status = VEEF_BD_ERROR_EIO;
            }

#elif COMPONENT_SPINAND

#endif
            _bank_mutex[bank].unlock();
        }
    }
#endif

    return status;
}

int VEEBlockDevice::_eeprom_wear_leveling(void)
{
    int status = VEEF_BD_ERROR_OK;
    struct bank_info *bi;
    static uint32_t rwCnt = 0;
    uint32_t bank, page, sector;

    /* Do wear leveling at fixed frequency */
    if (_eeprom.rwCnt - rwCnt < MX_EEPROM_WL_INTERVAL) {
        rwCnt = _eeprom.rwCnt;
        return VEEF_BD_ERROR_OK;
    }

    /* Choose a random logical page */
    bank = rand() % MX_EEPROMS;
    page = rand() % MX_EEPROM_LPAS_PER_CLUSTER;

    bi = &_eeprom.bi[bank];

    /* Get current bank lock */
    _bank_mutex[bank].lock();

    /* Skip unmapped page */
    sector = bi->l2ps[page];
    if (sector >= MX_EEPROM_DATA_SECTORS) {
        goto out;
    }

    if (bi->cache_dirty) {
        /* Skip the current dirty page */
        if (bi->cache.header.LPA == page) {
            goto out;
        }

        /* Flush dirty cache first */
        status = _eeprom_wb(bi);
        if (status) {
            tr_error("ee_wearl: fail to clean cache\r\n");
            goto out;
        }
    }

    /* Read the page out */
    status = _ee_read_page(bi, page);
    if (status) {
        tr_error("ee_wearl: fail to read page %lu\r\n", page);
        bi->cache.header.LPA = DATA_NONE8;
        goto out;
    }

    /* Set cache dirty */
    bi->cache_dirty = true;

    /* Cheat the free entry selector */
    bi->l2pe[page] = MX_EEPROM_ENTRIES_PER_SECTOR - 1;

    /* Flush the dirty cache */
    status = _eeprom_wb(bi);
    if (status) {
        tr_error("ee_wearl: fail to write back\r\n");
    }

out:
    /* Release current bank lock */
    _bank_mutex[bank].lock();

    return status;
}

void VEEBlockDevice::background(void)
{
    /* Flush dirty page cache */
    if (VEEBlockDevice::program_back()) {
        tr_error("background: fail to flush cache\r\n");
    }

    /* Check Wear leveling */
    if (VEEBlockDevice::_eeprom_wear_leveling()) {
        tr_error("background: fail to WL\r\n");
    }
}

#ifdef MX_EEPROM_PC_PROTECTION

int VEEBlockDevice::_ee_check_erase(struct bank_info *bi, uint32_t sector)
{
    uint32_t entry;
    struct eeprom_header header;

    /* Check address validity */
    if ((bi->bank >= MX_EEPROMS) ||
            (bi->block >= MX_EEPROM_BLOCKS) ||
            (sector >= MX_EEPROM_DATA_SECTORS)) {
        return VEEF_BD_ERROR_EINVAL;
    }

    /* XXX: Only check the first and last entry? */

    /* Check the first entry header */
    entry = sector * MX_EEPROM_ENTRIES_PER_SECTOR;
    if (_ee_read(bi, entry, &header, true) ||
            (header.LPA != DATA_NONE8 && header.LPA >= MX_EEPROM_LPAS_PER_CLUSTER)) {
        tr_error("_ee_ckers: fail to read entry 0\r\n");
        goto erase;
    }

    /* Check the last entry header */
    entry += MX_EEPROM_ENTRIES_PER_SECTOR - 1;
    if (_ee_read(bi, MX_EEPROM_ENTRIES_PER_SECTOR - 1, &header, true) || (header.LPA != DATA_NONE8)) {
        tr_error("_ee_ckers: fail to read entry %d\r\n", MX_EEPROM_ENTRIES_PER_SECTOR - 1);
        goto erase;
    }

    return VEEF_BD_ERROR_OK;

erase:
    tr_info("_ee_ckers: detected insufficient erase, block %lu, sector %lu\r\n",
            bi->block, sector);
    bi->dirty_block = bi->block;
    bi->dirty_sector = sector;
    return _ee_erase(bi);
}
#endif

#if COMPONENT_OSPIF
int VEEBlockDevice::_ee_check_sys(void)
{
    uint32_t bank, block;
    bool formatted = false;
    struct system_entry sys;
#ifdef MX_EEPROM_PC_PROTECTION
    struct bank_info *bi;
    uint32_t entry, lowerBound, upperBound;

    /* Loop to check each bank */
    for (bank = 0; bank < MX_EEPROMS; bank++) {
        bi = &_eeprom.bi[bank];
        bi->block = 0;
        bi->block_offset = bi->bank_offset;

        /* Loop to check each block */
        for (block = 0; block < MX_EEPROM_BLOCKS; block++) {
            lowerBound = 0;
            upperBound = MX_EEPROM_SYSTEM_ENTRIES - 1;

            while (lowerBound < upperBound) {
                /* Select the middle entry */
                entry = (lowerBound + upperBound) / 2;
                if (entry == lowerBound) {
                    entry++;
                }

retry:
                /* Read system entry */
                if (_ee_read_sys(bi, entry, &sys)) {
                    tr_error("ee_cksys: fail to read system entry %lu\r\n", entry);
                    goto fail;
                }

                /* Check entry format */
                if (sys.id == MFTL_ID) {
                    /* Valid entry */
                    formatted = true;
                    lowerBound = entry;
                    continue;
                } else if (sys.ops == DATA_NONE16 && sys.arg == DATA_NONE16) {
                    /* Empty entry */
                    upperBound = entry - 1;
                    continue;
                }

fail:
                tr_error("ee_cksys: corrupted system entry %lu\r\n", entry);

                /* XXX: Potential risk? Try sequential search? */
                if (entry < upperBound) {
                    entry++;
                    goto retry;
                }

                lowerBound = upperBound;
            }

            /* Read the latest entry */
            bi->sys_entry[block] = lowerBound;
            if (!_ee_read_sys(bi, lowerBound, &sys)) {
                if (sys.id == MFTL_ID) {
                    formatted = true;
                }

                /* Check insufficient erase */
                if (sys.ops == OPS_ERASE_BEGIN) {
                    _ee_check_erase(bi, sys.arg);
                }
            }

            /* Next block */
            bi->block++;
            bi->block_offset += MX_EEPROM_CLUSTER_SIZE;
        }

        /* Clean up */
        bi->block = DATA_NONE32;
        bi->block_offset = DATA_NONE32;
        bi->dirty_block = DATA_NONE32;
        bi->dirty_sector = DATA_NONE32;
    }
#else
    bd_addr_t addr;

    /* Loop to check each bank */
    for (bank = 0; bank < MX_EEPROMS; bank++) {
        addr = bank_offset[bank] + MX_EEPROM_SYSTEM_SECTOR_OFFSET;

        /* Loop to check each block */
        for (block = 0; block < MX_EEPROM_BLOCKS; block++) {
            if (_bd->read((uint8_t *)&sys, addr, sizeof(sys))) {
                tr_error("ee_formt: fail to read addr 0x%08lx\r\n", addr);
                continue;
            }

            /* Check entry format */
            if ((sys.id == MFTL_ID) && (sys.cksum == (sys.id ^ sys.ops ^ sys.arg))) {
                formatted = true;
                break;
            }

            addr += MX_EEPROM_CLUSTER_SIZE;
        }

        if (formatted) {
            break;
        }
    }
#endif
    return formatted ? VEEF_BD_ERROR_OK : VEEF_BD_ERROR_ENOFS;
}
#elif COMPONENT_SPINAND
int VEEBlockDevice::_ee_check_sys(void)
{
    uint32_t bank, block, sector;
    bool formatted = false;
    struct system_entry sys;
    uint32_t addr;
    struct bank_info *bi;

#ifdef MX_EEPROM_PC_PROTECTION

    /* Loop to check each bank */
    for (bank = 0; bank < MX_EEPROMS; bank++) {
        bi = &_eeprom.bi[bank];
        bi->block = 0;
        bi->block_offset = bi->bank_offset;

        /* Loop to check each block */
        for (block = 0; block < MX_EEPROM_BLOCKS; block++) {
            for (sector = 0; sector < MX_EEPROM_SECTORS_PER_CLUSTER; sector++) {
                if (!_ee_read_sys(bi, sector, SYS_ENTRY_ADDR_E_E, &sys)) {
                    if (sys.ops == OPS_ERASE_END) {
                        if (!_ee_read_sys(bi, sector, SYS_ENTRY_ADDR_E_B, &sys)) {
                            if (sys.ops == DATA_NONE16) {
                                formatted = true;
                            } else {    /* have OPS_ERASE_BEGIN */

                                goto erase;
                            }
                        }
                    } else {

                        if (formatted != true) { //this is the first sector
                            if (!_ee_read_sys(bi, sector + 1, SYS_ENTRY_ADDR_E_E, &sys)) {// read the next sector system entry
                                if (sys.ops == OPS_ERASE_END) { // the first sector is power off when erasing
                                    formatted = true;
                                    goto erase;
                                } else { // two sector system entry are 0xFF means this block is not formatted
                                    break;
                                }
                            }
                        }
erase:
                        bi->dirty_block = bi->block;
                        bi->dirty_sector = sector;
                        _ee_erase(bi);
                        break;
                    }
                }
            }
            /* Next block */
            bi->block++;
            bi->block_offset += MX_EEPROM_CLUSTER_OFFSET;
        }

        /* Clean up */
        bi->block = DATA_NONE32;
        bi->block_offset = DATA_NONE32;
        bi->dirty_block = DATA_NONE32;
        bi->dirty_sector = DATA_NONE32;
    }
#else

    /* Loop to check each bank */
    for (bank = 0; bank < MX_EEPROMS; bank++) {
        addr = bank_offset[bank];
        bi = &_eeprom.bi[bank];
        bi->block = 0;
        bi->block_offset = bi->bank_offset;

        /* Loop to check each block */
        for (block = 0; block < MX_EEPROM_BLOCKS; block++) {
            for (sector = 0; sector < MX_EEPROM_SECTORS_PER_CLUSTER; sector++) {
                if (_ee_read_sys(bi, sector, SYS_ENTRY_ADDR_E_E, &sys)) {
                    tr_error("mxee_formt: fail to read addr 0x%08lx\r\n", addr);
                    continue;
                }

                /* Check entry format */
                if ((sys.id == MFTL_ID) && (sys.cksum == (sys.id ^ sys.ops ^ sys.arg))) {
                    formatted = true;
                    break;
                }
                addr += MX_EEPROM_SECTOR_OFFSET;
            }
            addr += MX_EEPROM_CLUSTER_OFFSET;
        }

        if (formatted) {
            break;
        }
    }
    bi->block = DATA_NONE32;
#endif

    return formatted ? VEEF_BD_ERROR_OK : VEEF_BD_ERROR_ENOFS;
}

uint32_t VEEBlockDevice::_ee_gc_cp(struct bank_info *bi, uint8_t src_sector, uint8_t des_sector)
{
    int status;
    uint32_t LPA, LPA_start, src_entry, des_entry;
    struct eeprom_header header;
    struct eeprom_entry cache_tmp;

    src_entry = src_sector * MX_EEPROM_ENTRIES_PER_SECTOR;
    des_entry = des_sector * MX_EEPROM_ENTRIES_PER_SECTOR;

    status = _ee_read(bi, src_entry, &header, true);
    if (status) {
        tr_error("_ee_gc_cp: fail to read src_entry %lu\r\n", src_entry);
    }
    LPA = header.LPA;
    if (bi->l2ps[LPA] != src_sector) { /* for power fail recovery*/
        bi->l2ps[LPA] = src_sector;
    }

    LPA_start = LPA - (LPA % MX_EEPROM_LPAS_PER_SECTOR);
    for (int i = 0; i < MX_EEPROM_LPAS_PER_SECTOR; i++) {
        src_entry = _ee_find_latest(bi, LPA_start + i, false);
        if (src_entry < MX_EEPROM_ENTRIES_PER_CLUSTER) {
            status = _ee_read(bi, src_entry, &cache_tmp, false);
            if (status) {
                tr_error("_ee_gc_cp: fail to read src_entry %lu\r\n", src_entry);
                return status;
            }
            status = _ee_write(bi, des_entry, &cache_tmp);
            if (status) {
                tr_error("_ee_gc_cp: fail to write des_entry %lu\r\n", des_entry);
                return status;
            }

            /* Update L2E mapping */
            bi->l2pe[LPA_start + i] = des_entry % MX_EEPROM_ENTRIES_PER_SECTOR;
            bi->l2ps[LPA_start + i] = des_sector;
            bi->l2ps_group[(LPA_start + i) / MX_EEPROM_LPAS_PER_SECTOR] = des_sector;
            bi->latest_used_entry_per_sector[des_sector] = des_entry % MX_EEPROM_ENTRIES_PER_SECTOR;
            bi->p2l[des_sector] = LPA;
            des_entry ++;
        }
    }
    return des_entry;
}

uint32_t VEEBlockDevice::_ee_gc(struct bank_info *bi, uint8_t src_sector, uint8_t des_sector)
{
    uint32_t des_entry;

    /* Obsolete sector */
    bi->dirty_block = bi->block;
    bi->dirty_sector = src_sector;

    //mark OPS_GC_CP_BEGIN and des_addr(sector number) in the oob of source sector before cp data
    _ee_update_sys(bi, src_sector, SYS_ENTRY_ADDR_G_B_S, OPS_GC_CP_BEGIN, des_sector);

    //mark OPS_GC_CP_BEGIN and src_addr(sector number) in the oob of destination sector before cp data
    _ee_update_sys(bi, des_sector, SYS_ENTRY_ADDR_G_B_D, OPS_GC_CP_BEGIN, src_sector);

    //copy data from source sector to destination sector
    des_entry = _ee_gc_cp(bi, src_sector, des_sector);

    //mark OPS_GC_CP_END in the oob of source sector after cp data
    _ee_update_sys(bi, src_sector, SYS_ENTRY_ADDR_G_E, OPS_GC_CP_END, DATA_NONE16);

    if (_ee_erase(bi)) {
        tr_error("ee_rwbuf: fail to erase\r\n");
    }

    return des_entry;
}

#ifdef MX_EEPROM_PC_PROTECTION
int VEEBlockDevice::_ee_check_gc_power_fail(void)
{
    uint8_t bank, block, sector, des_sector;
    struct system_entry sys;
    struct bank_info *bi;

    /* Loop to check each bank */
    for (bank = 0; bank < MX_EEPROMS; bank++) {
        bi = &_eeprom.bi[bank];
        bi->block = 0;
        bi->block_offset = bi->bank_offset;

        /* Loop to check each block */
        for (block = 0; block < MX_EEPROM_BLOCKS; block++) {
            for (sector = 0; sector < MX_EEPROM_SECTORS_PER_CLUSTER; sector++) {
                if (!_ee_read_sys(bi, sector, SYS_ENTRY_ADDR_G_B_S, &sys)) {
                    if (sys.ops == OPS_GC_CP_BEGIN) {
                        des_sector = sys.arg;
                        if (!_ee_read_sys(bi, sector, SYS_ENTRY_ADDR_G_E, &sys)) {
                            if (sys.ops == OPS_GC_CP_END) { /* power fail before marking OPS_ERASE_BEGIN */
                                bi->dirty_block = bi->block;
                                bi->dirty_sector = sector;
                                _ee_erase(bi);
                            } else {    /* power fail before marking OPS_ERASE_BEGIN */
                                bi->dirty_block = bi->block;
                                bi->dirty_sector = des_sector;
                                _ee_erase(bi); /* erase destination sector */
                                _ee_gc_cp(bi, sector, des_sector); /* copy data */
                                bi->dirty_block = bi->block;
                                bi->dirty_sector = sector;
                                _ee_erase(bi);    /* erase source sector */
                            }
                        }
                        break;
                    }
                }
            }

            /* Next block */
            bi->block++;
            bi->block_offset += MX_EEPROM_CLUSTER_OFFSET;
        }

        /* Clean up */
        bi->block = DATA_NONE32;
        bi->block_offset = DATA_NONE32;
        bi->dirty_block = DATA_NONE32;
        bi->dirty_sector = DATA_NONE32;
    }
    return VEEF_BD_ERROR_OK;
}
#endif

#endif

int VEEBlockDevice::format(void)
{
    struct system_entry sys;
    uint32_t i, j, k, sector, addr;
    /* Should not format EEPROM after init */
    if (_is_initialized) {
        return VEEF_BD_ERROR_EPERM;
    }

    /* Loop to erase each sector */
    for (i = 0; i < MX_EEPROMS; i++) {
        addr = bank_offset[i];

        for (j = 0; j < MX_EEPROM_BLOCKS; j++) {
            for (k = 0; k < MX_EEPROM_SECTORS_PER_CLUSTER; k++, sector++) {
                if (_bd->erase(addr, MX_FLASH_SECTOR_SIZE)) {
                    tr_error("ee_formt: fail to erase sector %lu\r\n", sector);
                    return VEEF_BD_ERROR_EIO;
                }
#if COMPONENT_OSPIF
                addr += MX_FLASH_SECTOR_SIZE;
#elif COMPONENT_SPINAND
                addr += MX_FLASH_SECTOR_OFFSET;
#endif
            }
#if COMPONENT_OSPIF
            addr += MX_EEPROM_CLUSTER_SIZE;
#elif COMPONENT_SPINAND
            addr += MX_EEPROM_CLUSTER_OFFSET;
#endif
        }
    }

    /* Fill system entry */
    sys.id = MFTL_ID;
#if COMPONENT_OSPIF
    sys.ops = OPS_NONE;
#elif COMPONENT_SPINAND
    sys.ops = OPS_ERASE_END;
#endif
    sys.arg = DATA_NONE16;
    sys.cksum = sys.id ^ sys.ops ^ sys.arg;

    /* Write RWWEE ID (Actually no need to write every block) */
    for (i = 0; i < MX_EEPROMS; i++) {

#if COMPONENT_OSPIF
        addr = bank_offset[i] + MX_EEPROM_SYSTEM_SECTOR_OFFSET;
        for (j = 0; j < MX_EEPROM_BLOCKS; j++) {
            if (_bd->program((uint8_t *)&sys, addr, sizeof(sys))) {
                tr_error("ee_formt: fail to write addr 0x%08lx\r\n", addr);
                return VEEF_BD_ERROR_EIO;
            }
            addr += MX_EEPROM_CLUSTER_SIZE;
        }
#elif COMPONENT_SPINAND
        addr = bank_offset[i];
        for (j = 0; j < MX_EEPROM_BLOCKS; j++) {
            for (sector = 0; sector < MX_EEPROM_SECTORS_PER_CLUSTER; sector++) {
                if (_bd->program((uint8_t *)&sys, addr, sizeof(sys))) {
                    tr_error("ee_formt: fail to write addr 0x%08lx\r\n", addr);
                    return VEEF_BD_ERROR_EIO;
                }
                addr += MX_EEPROM_SECTOR_OFFSET;
            }
            addr += MX_EEPROM_CLUSTER_SIZE;
        }
#endif
    }
    return VEEF_BD_ERROR_OK;
}

