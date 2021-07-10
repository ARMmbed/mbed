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

#include "blockdevice/internal/SFDP.h"
#include "platform/Callback.h"
#include "OSPIFBlockDevice.h"
#include <string.h>
#include "rtos/ThisThread.h"

#ifndef MBED_CONF_MBED_TRACE_ENABLE
#define MBED_CONF_MBED_TRACE_ENABLE        0
#endif

#include "mbed_trace.h"
#define TRACE_GROUP "OSPIF"

using namespace std::chrono;
using namespace mbed;

/* Default OSPIF Parameters */
/****************************/
#define OSPIF_DEFAULT_SE_SIZE    4096
// The SFDP spec only defines two status registers. But some devices,
// have three "status-like" registers (one status, two config)
#define OSPI_MAX_STATUS_REGISTERS 3
#define OSPI_DEFAULT_STATUS_REGISTERS 2
#ifndef UINT64_MAX
#define UINT64_MAX -1
#endif
#define OSPI_NO_ADDRESS_COMMAND UINT64_MAX
#define OSPI_ALT_DEFAULT_VALUE  0
// Status Register Bits
#define OSPIF_STATUS_BIT_WIP        0x1 // Write In Progress
#define OSPIF_STATUS_BIT_WEL        0x2 // Write Enable Latch
#define OSPIF_NO_QUAD_ENABLE        (-1)

/* SFDP Header Parsing */
/***********************/
#define OSPIF_RSFDP_DUMMY_CYCLES 8

/* Basic Parameters Table Parsing */
/**********************************/
//READ Instruction support according to BUS Configuration
#define OSPIF_BASIC_PARAM_TABLE_FAST_READ_SUPPORT_BYTE 2
#define OSPIF_BASIC_PARAM_TABLE_QPI_READ_SUPPORT_BYTE 16
#define OSPIF_BASIC_PARAM_TABLE_OPI_READ_SUPPORT_BYTE 17
#define OSPIF_BASIC_PARAM_TABLE_888_READ_INST_BYTE 19
#define OSPIF_BASIC_PARAM_TABLE_444_READ_INST_BYTE 27
#define OSPIF_BASIC_PARAM_TABLE_144_READ_INST_BYTE 9
#define OSPIF_BASIC_PARAM_TABLE_114_READ_INST_BYTE 11
#define OSPIF_BASIC_PARAM_TABLE_222_READ_INST_BYTE 23
#define OSPIF_BASIC_PARAM_TABLE_122_READ_INST_BYTE 15
#define OSPIF_BASIC_PARAM_TABLE_112_READ_INST_BYTE 13
#define OSPIF_BASIC_PARAM_TABLE_PAGE_SIZE_BYTE 40
// Quad Enable Params
#define OSPIF_BASIC_PARAM_TABLE_QER_BYTE 58
#define OSPIF_BASIC_PARAM_TABLE_444_MODE_EN_SEQ_BYTE 56
// Erase Types Params
#define OSPIF_BASIC_PARAM_TABLE_ERASE_TYPE_1_BYTE 29
#define OSPIF_BASIC_PARAM_TABLE_ERASE_TYPE_2_BYTE 31
#define OSPIF_BASIC_PARAM_TABLE_ERASE_TYPE_3_BYTE 33
#define OSPIF_BASIC_PARAM_TABLE_ERASE_TYPE_4_BYTE 35
#define OSPIF_BASIC_PARAM_TABLE_ERASE_TYPE_1_SIZE_BYTE 28
#define OSPIF_BASIC_PARAM_TABLE_ERASE_TYPE_2_SIZE_BYTE 30
#define OSPIF_BASIC_PARAM_TABLE_ERASE_TYPE_3_SIZE_BYTE 32
#define OSPIF_BASIC_PARAM_TABLE_ERASE_TYPE_4_SIZE_BYTE 34
#define OSPIF_BASIC_PARAM_4K_ERASE_TYPE_BYTE 1

#define OSPIF_BASIC_PARAM_TABLE_SOFT_RESET_BYTE 61
#define OSPIF_BASIC_PARAM_TABLE_4BYTE_ADDR_BYTE 63

#define SOFT_RESET_RESET_INST_BITMASK            0b001000
#define SOFT_RESET_ENABLE_AND_RESET_INST_BITMASK 0b010000

// Erase Types Per Region BitMask
#define ERASE_BITMASK_TYPE4 0x08
#define ERASE_BITMASK_TYPE1 0x01
#define ERASE_BITMASK_NONE  0x00
#define ERASE_BITMASK_ALL   0x0F

// 4-Byte Addressing Support Bitmasks
#define FOURBYTE_ADDR_B7_BITMASK           0b00000001
#define FOURBYTE_ADDR_B7_WREN_BITMASK      0b00000010
#define FOURBYTE_ADDR_EXT_ADDR_REG_BITMASK 0b00000100
#define FOURBYTE_ADDR_BANK_REG_BITMASK     0b00001000
#define FOURBYTE_ADDR_CONF_REG_BITMASK     0b00010000
#define FOURBYTE_ADDR_INSTS_BITMASK        0b00100000
#define FOURBYTE_ADDR_ALWAYS_BITMASK       0b01000000

/* 4-Byte Instruction Table Parsing */
/**********************************/
#define SFDP_DEFAULT_4_BYTE_INST_TABLE_SIZE_BYTES 8 /* 2 DWORDS */

#define OSPIF_4_BYTE_INST_TABLE_FAST_READ_SUPPORT_BYTE 0
#define OSPIF_4_BYTE_INST_TABLE_ERASE_SUPPORT_BYTE     1
#define OSPIF_4_BYTE_INST_TABLE_INST_ERASE_TYPE_1_BYTE 4
#define OSPIF_4_BYTE_INST_TABLE_INST_ERASE_TYPE_2_BYTE 5
#define OSPIF_4_BYTE_INST_TABLE_INST_ERASE_TYPE_3_BYTE 6
#define OSPIF_4_BYTE_INST_TABLE_INST_ERASE_TYPE_4_BYTE 7

// 4-Byte Instruction Support Bitmasks
#define FOURBYTE_INST_1_1_1_READ_BITMASK           0b00000001
#define FOURBYTE_INST_1_1_2_FAST_READ_BITMASK      0b00000010
#define FOURBYTE_INST_1_2_2_FAST_READ_BITMASK      0b00000100
#define FOURBYTE_INST_1_1_4_FAST_READ_BITMASK      0b00001000
#define FOURBYTE_INST_1_4_4_FAST_READ_BITMASK      0b00010000
#define FOURBYTE_INST_ERASE_TYPE_1_BITMASK         0b00000010
#define FOURBYTE_INST_ERASE_TYPE_2_BITMASK         0b00000100
#define FOURBYTE_INST_ERASE_TYPE_3_BITMASK         0b00001000
#define FOURBYTE_INST_ERASE_TYPE_4_BITMASK         0b00010000

#define IS_MEM_READY_MAX_RETRIES 10000


// General OSPI instructions
#define OSPIF_INST_WSR1  0x01 // Write status register 1
#define OSPIF_INST_PROG  0x02 // Page program
#define OSPIF_INST_WRDI  0x04 // Write disable
#define OSPIF_INST_RSR1  0x05 // Read status register 1
#define OSPIF_INST_WREN  0x06 // Write enable
#define OSPIF_INST_RSFDP 0x5A // Read SFDP
#define OSPIF_INST_RDID  0x9F // Read Manufacturer and JDEC Device ID
#define DTROSPIF_INST_READ_OCTA_STR  0xEC // Read data in SOPI mode
#define DTROSPIF_INST_READ_OCTA_DTR  0xEE // Read data in DOPI mode
#define OSPIF_INST_WRCR2  0x72 // Write Configuration Register2
#define OSPIF_INST_RDCR2  0x71 // Read Configuration Register2

//Macronix flash 4-Byte address instructions
#define OSPIF_INST_READ_4B  0x13 // Read data with 4 byte address
#define OSPIF_INST_PROG_4B  0x12 // Page program with 4 byte address

// Device-specific instructions
#define OSPIF_INST_ULBPR 0x98 // Clear all write-protection bits in the Block-Protection register
#define OSPIF_INST_RDCR  0x15 // Read the two control registers

// Default read/legacy erase instructions
#define OSPIF_INST_READ_DEFAULT          0x03
#define OSPIF_INST_LEGACY_ERASE_DEFAULT  OSPI_NO_INST

// Default status register 2 read/write instructions
#define OSPIF_INST_WSR2_DEFAULT    OSPI_NO_INST
#define OSPIF_INST_RSR2_DEFAULT    0x35

// Default 4-byte extended addressing register write instruction
#define OSPIF_INST_4BYTE_REG_WRITE_DEFAULT OSPI_NO_INST


// Length of data returned from RDID instruction
#define OSPI_RDID_DATA_LENGTH 3

#ifdef NEED_DEFINE_SFDP_PARA
static const uint8_t _sfdp_head_table[32] = {0x53, 0x46, 0x44, 0x50, 0x06, 0x01, 0x02, 0xFF, 0x00, 0x06, 0x01,
                                             0x10, 0x30, 0x00, 0x00, 0xFF, 0xC2, 0x00, 0x01, 0x04, 0x10, 0x01,
                                             0x00, 0xFF, 0x84, 0x00, 0x01, 0x02, 0xC0, 0x00, 0x00, 0xFF
                                            };
static const uint8_t _sfdp_basic_param_table[64] = {0x30, 0xFF, 0xFB, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF,
                                                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x14, 0xEC,
                                                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0C, 0x20,
                                                    0x10, 0xDC, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                    0x81, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                    0xFF, 0x50, 0xF9, 0x80
                                                   };
static const uint8_t _sfdp_4_byte_inst_table[8] = {0x7F, 0xEF, 0xFF, 0xFF, 0x21, 0x5C, 0xDC, 0x14};
#endif

/* Init function to initialize Different Devices CS static list */
static PinName *generate_initialized_active_ospif_csel_arr();
// Static Members for different devices csel
// _devices_mutex is used to lock csel list - only one OSPIFBlockDevice instance per csel is allowed
SingletonPtr<PlatformMutex> OSPIFBlockDevice::_devices_mutex;
int OSPIFBlockDevice::_number_of_active_ospif_flash_csel = 0;
PinName *OSPIFBlockDevice::_active_ospif_flash_csel_arr = generate_initialized_active_ospif_csel_arr();

/********* Public API Functions *********/
/****************************************/
OSPIFBlockDevice::OSPIFBlockDevice(PinName io0, PinName io1, PinName io2, PinName io3, PinName io4, PinName io5, PinName io6, PinName io7, PinName sclk, PinName csel, PinName dqs,
                                   int clock_mode, int freq)
    : _ospi(io0, io1, io2, io3, io4, io5, io6, io7, sclk, csel, dqs, clock_mode), _csel(csel), _freq(freq),
      _init_ref_count(0),
      _is_initialized(false)
{
    _unique_device_status = add_new_csel_instance(csel);

    if (_unique_device_status == 0) {
        tr_debug("Adding a new OSPIFBlockDevice csel: %d", (int)csel);
    } else if (_unique_device_status == -1) {
        tr_error("OSPIFBlockDevice with the same csel(%d) already exists", (int)csel);
    } else {
        tr_error("Too many different OSPIFBlockDevice devices - max allowed: %d", OSPIF_MAX_ACTIVE_FLASH_DEVICES);
    }

    // Initialize parameters
    _sfdp_info.bptbl.legacy_erase_instruction = OSPIF_INST_LEGACY_ERASE_DEFAULT;
    _sfdp_info.bptbl.device_size_bytes = 0;
    _sfdp_info.smptbl.regions_min_common_erase_size = 0;
    _sfdp_info.smptbl.region_cnt = 1;
    _sfdp_info.smptbl.region_erase_types_bitfld[0] = SFDP_ERASE_BITMASK_NONE;

    // Until proven otherwise, assume no quad enable
    _quad_enable_register_idx = OSPIF_NO_QUAD_ENABLE;
    _quad_enable_bit = OSPIF_NO_QUAD_ENABLE;
    _needs_fast_mode = false;

    // Default Bus Setup 1_1_1 with 0 dummy and mode cycles
    _inst_width = OSPI_CFG_BUS_SINGLE;
    _inst_size = OSPI_CFG_INST_SIZE_16;
    _address_width = OSPI_CFG_BUS_SINGLE;
    _address_size = OSPI_CFG_ADDR_SIZE_24;
    _alt_size = 0;
    _dummy_cycles = 0;
    _data_width = OSPI_CFG_BUS_SINGLE;

    // Set default read/erase instructions
    _read_instruction = OSPIF_INST_READ_DEFAULT;
    _prog_instruction = OSPIF_INST_PROG;
    _legacy_erase_instruction = OSPIF_INST_LEGACY_ERASE_DEFAULT;

    _num_status_registers = OSPI_DEFAULT_STATUS_REGISTERS;
    // Set default status register 2 write/read instructions
    _write_status_reg_2_inst = OSPIF_INST_WSR2_DEFAULT;
    _read_status_reg_2_inst = OSPIF_INST_RSR2_DEFAULT;

    _clear_protection_method = OSPIF_BP_CLEAR_SR;

    // Set default 4-byte addressing extension register write instruction
    _attempt_4_byte_addressing = true;
    _4byte_msb_reg_write_inst = OSPIF_INST_4BYTE_REG_WRITE_DEFAULT;
    _support_4_byte_inst = false;

#ifdef MX_FLASH_SUPPORT_RWW
    _wait_flag = NOT_STARTED;
    _busy_bank = 0xffffffff;
#endif
}

int OSPIFBlockDevice::init()
{
    int status = OSPIF_BD_ERROR_OK;

    if (_unique_device_status == 0) {
        tr_debug("OSPIFBlockDevice csel: %d", (int)_csel);
    } else if (_unique_device_status == -1) {
        tr_error("OSPIFBlockDevice with the same csel(%d) already exists", (int)_csel);
        return OSPIF_BD_ERROR_DEVICE_NOT_UNIQUE;
    } else {
        tr_error("Too many different OSPIFBlockDevice devices - max allowed: %d", OSPIF_MAX_ACTIVE_FLASH_DEVICES);
        return OSPIF_BD_ERROR_DEVICE_MAX_EXCEED;
    }

    _mutex.lock();

    if (!_is_initialized) {
        _init_ref_count = 0;
    }

    _init_ref_count++;

    if (_init_ref_count != 1) {
        goto exit_point;
    }


    // All commands other than Read and RSFDP use default 1-1-1 bus mode (Program/Erase are constrained by flash memory performance more than bus performance)
    if (OSPI_STATUS_OK != _ospi.configure_format(OSPI_CFG_BUS_SINGLE, OSPI_CFG_INST_SIZE_8, OSPI_CFG_BUS_SINGLE, _address_size, OSPI_CFG_BUS_SINGLE,
                                                 0, OSPI_CFG_BUS_SINGLE, 0)) {
        tr_error("_ospi_configure_format failed");
        status = OSPIF_BD_ERROR_DEVICE_ERROR;
        goto exit_point;
    }

    _alt_size = 0;
    _dummy_cycles = 0;
    if (OSPI_STATUS_OK != _ospi_set_frequency(_freq)) {
        tr_error("OSPI Set Frequency Failed");
        status = OSPIF_BD_ERROR_DEVICE_ERROR;
        goto exit_point;
    }

    // Synchronize Device
    if (false == _is_mem_ready()) {
        tr_error("Init - _is_mem_ready Failed");
        status = OSPIF_BD_ERROR_READY_FAILED;
        goto exit_point;
    }

    if (_handle_vendor_quirks() < 0) {
        tr_error("Init - Could not read vendor id");
        status = OSPIF_BD_ERROR_DEVICE_ERROR;
        goto exit_point;
    }

    /**************************** Parse SFDP data ***********************************/
    {
        _sfdp_info.bptbl.addr = 0x0;
        _sfdp_info.bptbl.size = 0;
        _sfdp_info.smptbl.addr = 0x0;
        _sfdp_info.smptbl.size = 0;

        if (sfdp_parse_headers(callback(this, &OSPIFBlockDevice::_ospi_send_read_sfdp_command), _sfdp_info) < 0) {
            tr_error("Init - Parse SFDP Headers Failed");
            status = OSPIF_BD_ERROR_PARSING_FAILED;
            goto exit_point;
        }

        /************************ Parse 4-Byte Address Instructions Table ************************/
        if (_sfdp_parse_4_byte_inst_table(callback(this, &OSPIFBlockDevice::_ospi_send_read_sfdp_command),
                                          _sfdp_info) < 0) {
            tr_error("Init - Parse 4-Byte Address Instructions Table Failed");
            status = OSPIF_BD_ERROR_PARSING_FAILED;
            goto exit_point;
        }

        if (_sfdp_parse_basic_param_table(callback(this, &OSPIFBlockDevice::_ospi_send_read_sfdp_command),
                                          _sfdp_info) < 0) {
            tr_error("Init - Parse Basic Param Table Failed");
            status = OSPIF_BD_ERROR_PARSING_FAILED;
            goto exit_point;
        }

        if (sfdp_parse_sector_map_table(callback(this, &OSPIFBlockDevice::_ospi_send_read_sfdp_command), _sfdp_info) < 0) {
            tr_error("Init - Parse Sector Map Table Failed");
            status = OSPIF_BD_ERROR_PARSING_FAILED;
            goto exit_point;
        }
    }

    if (0 != _clear_block_protection()) {
        tr_error("Init - clearing block protection failed");
        status = OSPIF_BD_ERROR_PARSING_FAILED;
        goto exit_point;
    }

    _is_initialized = true;

exit_point:
    _mutex.unlock();

    return status;
}

int OSPIFBlockDevice::deinit()
{
    int result = OSPIF_BD_ERROR_OK;

    _mutex.lock();

    if (!_is_initialized) {
        _init_ref_count = 0;
        _mutex.unlock();
        return result;
    }

    _init_ref_count--;

    if (_init_ref_count) {
        _mutex.unlock();
        return result;
    }

    if (false == _is_mem_ready()) {
        tr_error("Device not ready after write, failed");
    }

#ifdef MX_FLASH_SUPPORT_RWW
    _wait_flag = NOT_STARTED;
#endif

    change_mode(SPI);

    // Disable Device for Writing
    ospi_status_t status = _ospi_send_general_command(OSPIF_INST_WRDI, OSPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0);
    if (status != OSPI_STATUS_OK)  {
        tr_error("Write Disable failed");
        result = OSPIF_BD_ERROR_DEVICE_ERROR;
    }
    _address_size = OSPI_CFG_ADDR_SIZE_24;
    _is_initialized = false;

    _mutex.unlock();

    if (_unique_device_status == 0) {
        remove_csel_instance(_csel);
    }

    return result;
}

int OSPIFBlockDevice::read(void *buffer, bd_addr_t addr, bd_size_t size)
{
    int status = OSPIF_BD_ERROR_OK;
    tr_debug("Read Inst: 0x%xh", _read_instruction);

#ifdef MX_FLASH_SUPPORT_RWW
    bool need_wait;
    need_wait = (_wait_flag != NOT_STARTED) && ((addr & MX_FLASH_BANK_SIZE_MASK) == _busy_bank);

    // Wait for ready
    if (need_wait) {

        _busy_mutex.lock();

        if (_is_mem_ready_rww(addr, false) == false) {
            return OSPIF_BD_ERROR_OK;
        }

    } else {
        if (_wait_flag == WRITE_WAIT_STARTED) {
            tr_debug("\r\n RWW1 CNT");
        } else if (_wait_flag == ERASE_WAIT_STARTED) {
            tr_debug("\r\n RWE2 CNT");
        }
    }

#endif

    _mutex.lock();

    // In DOPI mode, the number of read data should be even
    if (size % 2 != 0) {
        size ++;
    }

    if (OSPI_STATUS_OK != _ospi_send_read_command(_read_instruction, buffer, addr, size)) {
        tr_error("Read Command failed");
        status = OSPIF_BD_ERROR_DEVICE_ERROR;
    }

    _mutex.unlock();

#ifdef MX_FLASH_SUPPORT_RWW
    if (need_wait) {
        _busy_mutex.unlock();
    }
#endif

    return status;
}

int OSPIFBlockDevice::program(const void *buffer, bd_addr_t addr, bd_size_t size)
{
    ospi_status_t result = OSPI_STATUS_OK;
    bool program_failed = false;
    int status = OSPIF_BD_ERROR_OK;
    uint32_t offset = 0;
    uint32_t chunk = 0;
    bd_size_t written_bytes = 0;

    tr_debug("Program - Buff: %p, addr: %llu, size: %llu", buffer, addr, size);

    while (size > 0) {
        // Write on _page_size_bytes boundaries (Default 256 bytes a page)
        offset = addr % _page_size_bytes;
        chunk = (offset + size < _page_size_bytes) ? size : (_page_size_bytes - offset);
        written_bytes = chunk;

#ifdef MX_FLASH_SUPPORT_RWW
        _busy_mutex.lock();

        // Wait for ready
        if (_is_mem_ready_rww(addr, true) == false) {
            return OSPIF_BD_ERROR_OK;
        }

#endif

        _mutex.lock();

        //Send WREN
        if (_set_write_enable() != 0) {
            tr_error("Write Enabe failed");
            program_failed = true;
            status = OSPIF_BD_ERROR_WREN_FAILED;
            goto exit_point;
        }

        //_prog_instruction can be OSPIF_INST_PROG or OSPIF_INST_PROG_4B
        result = _ospi_send_program_command(_prog_instruction, buffer, addr, &written_bytes);
        if ((result != OSPI_STATUS_OK) || (chunk != written_bytes)) {
            tr_error("Write failed");
            program_failed = true;
            status = OSPIF_BD_ERROR_DEVICE_ERROR;
            goto exit_point;
        }

#ifdef MX_FLASH_SUPPORT_RWW
        _wait_flag = WRITE_WAIT_STARTED;
        _busy_bank = addr & MX_FLASH_BANK_SIZE_MASK;

        _mutex.unlock();

        _busy_mutex.unlock();
#else
        if (false == _is_mem_ready()) {
            tr_error("Device not ready after write, failed");
            program_failed = true;
            status = OSPIF_BD_ERROR_READY_FAILED;
            goto exit_point;
        }
        _mutex.unlock();
#endif
        buffer = static_cast<const uint8_t *>(buffer) + chunk;
        addr += chunk;
        size -= chunk;
    }

exit_point:
    if (program_failed) {
        _mutex.unlock();
    }

    return status;
}

int OSPIFBlockDevice::erase(bd_addr_t addr, bd_size_t size)
{
    int type = 0;
    ospi_inst_t cur_erase_inst = OSPI_NO_INST;
    bool erase_failed = false;
    int status = OSPIF_BD_ERROR_OK;
    // Find region of erased address
    int region = sfdp_find_addr_region(addr, _sfdp_info);
    // Erase Types of selected region
    uint8_t bitfield = _sfdp_info.smptbl.region_erase_types_bitfld[region];

    tr_debug("Erase - addr: %llu, size: %llu", addr, size);

    if ((addr + size) > _sfdp_info.bptbl.device_size_bytes) {
        tr_error("Erase exceeds flash device size");
        return OSPIF_BD_ERROR_INVALID_ERASE_PARAMS;
    }

    if (((addr % get_erase_size(addr)) != 0) || (((addr + size) % get_erase_size(addr + size - 1)) != 0)) {
        tr_error("Invalid erase - unaligned address and size");
        return OSPIF_BD_ERROR_INVALID_ERASE_PARAMS;
    }

    // For each iteration erase the largest section supported by current region
    while (size > 0) {
        unsigned int eu_size;
        if (_sfdp_info.bptbl.legacy_erase_instruction == OSPI_NO_INST) {
            // Iterate to find next largest erase type that is a) supported by region, and b) smaller than size.
            // Find the matching instruction and erase size chunk for that type.
            type = sfdp_iterate_next_largest_erase_type(bitfield, size, (int)addr,
                                                        region,
                                                        _sfdp_info.smptbl);
            //cur_erase_inst should be 4-Byte erase instruction for Macronix octa flash OPI mode
            cur_erase_inst = (_address_size == OSPI_CFG_ADDR_SIZE_32) ? _sfdp_info.fbatbl.erase_type_4_byte_inst_arr[type] : _sfdp_info.smptbl.erase_type_inst_arr[type];
            eu_size = _sfdp_info.smptbl.erase_type_size_arr[type];
        } else {
            // Must use legacy 4k erase instruction
            cur_erase_inst = _sfdp_info.bptbl.legacy_erase_instruction;
            eu_size = OSPIF_DEFAULT_SE_SIZE;
        }

        if (addr % eu_size != 0 || addr + size < eu_size) {
            // Should not happen if the erase table parsing
            // and alignment checks were performed correctly
            tr_error("internal error: address %llu not aligned to erase size %u (type %d)",
                     addr, eu_size, type);
        }

        tr_debug("Erase - addr: %llu, size:%llu, Inst: 0x%xh, erase size: %u ",
                 addr, size, cur_erase_inst, eu_size);
        tr_debug("Erase - Region: %d, Type:%d ",
                 region, type);

#ifdef MX_FLASH_SUPPORT_RWW
        _busy_mutex.lock();

        // Wait for ready
        if (_is_mem_ready_rww(addr, true) == false) {
            return OSPIF_BD_ERROR_OK;
        }
#endif

        _mutex.lock();

        if (_set_write_enable() != 0) {
            tr_error("OSPI Erase Device not ready - failed");
            erase_failed = true;
            status = OSPIF_BD_ERROR_WREN_FAILED;
            goto exit_point;
        }

        if (OSPI_STATUS_OK != _ospi_send_erase_command(cur_erase_inst, addr, size)) {
            tr_error("OSPI Erase command failed!");
            erase_failed = true;
            status = OSPIF_BD_ERROR_DEVICE_ERROR;
            goto exit_point;
        }

#ifdef MX_FLASH_SUPPORT_RWW
        _wait_flag = ERASE_WAIT_STARTED;
        _busy_bank = addr & MX_FLASH_BANK_SIZE_MASK;

        _mutex.unlock();

        _busy_mutex.unlock();
#else
        if (false == _is_mem_ready()) {
            tr_error("OSPI After Erase Device not ready - failed");
            erase_failed = true;
            status = OSPIF_BD_ERROR_READY_FAILED;
            goto exit_point;
        }

        _mutex.unlock();
#endif

        addr += eu_size;
        size -= eu_size;

        if ((size > 0) && (addr > _sfdp_info.smptbl.region_high_boundary[region])) {
            // erase crossed to next region
            region++;
            bitfield = _sfdp_info.smptbl.region_erase_types_bitfld[region];
        }
    }

exit_point:
    if (erase_failed) {
        _mutex.unlock();
    }

    return status;
}

bd_size_t OSPIFBlockDevice::get_read_size() const
{
    // Return minimum read size in bytes for the device
    return MBED_CONF_OSPIF_OSPI_MIN_READ_SIZE;
}

bd_size_t OSPIFBlockDevice::get_program_size() const
{
    // Return minimum program/write size in bytes for the device
    return MBED_CONF_OSPIF_OSPI_MIN_PROG_SIZE;
}

bd_size_t OSPIFBlockDevice::get_erase_size() const
{
    // return minimal erase size supported by all regions (0 if none exists)
    return _sfdp_info.smptbl.regions_min_common_erase_size;
}

const char *OSPIFBlockDevice::get_type() const
{
    return "OSPIF";
}

// Find minimal erase size supported by the region to which the address belongs to
bd_size_t OSPIFBlockDevice::get_erase_size(bd_addr_t addr)
{
    // If the legacy erase instruction is in use, the erase size is uniformly 4k
    if (_sfdp_info.bptbl.legacy_erase_instruction != OSPI_NO_INST) {
        return OSPIF_DEFAULT_SE_SIZE;
    }

    // Find region of current address
    int region = sfdp_find_addr_region(addr, _sfdp_info);

    int min_region_erase_size = _sfdp_info.smptbl.regions_min_common_erase_size;
    int8_t type_mask = SFDP_ERASE_BITMASK_TYPE1;
    int i_ind = 0;

    if (region != -1) {
        type_mask = 0x01;

        for (i_ind = 0; i_ind < 4; i_ind++) {
            // loop through erase types bitfield supported by region
            if (_sfdp_info.smptbl.region_erase_types_bitfld[region] & type_mask) {

                min_region_erase_size = _sfdp_info.smptbl.erase_type_size_arr[i_ind];
                break;
            }
            type_mask = type_mask << 1;
        }

        if (i_ind == 4) {
            tr_error("No erase type was found for region addr");
        }
    }

    return (bd_size_t)min_region_erase_size;
}

bd_size_t OSPIFBlockDevice::size() const
{
    return _sfdp_info.bptbl.device_size_bytes;
}

int OSPIFBlockDevice::get_erase_value() const
{
    return 0xFF;
}

int OSPIFBlockDevice::change_mode(int mode)
{
    int status = OSPIF_BD_ERROR_OK;
    char config_reg2 = 0;

    if (((mode == SPI) && (_read_instruction == OSPIF_INST_READ_4B)) ||
            ((mode == SOPI) && (_read_instruction == DTROSPIF_INST_READ_OCTA_STR)) ||
            ((mode == DOPI) && (_read_instruction == DTROSPIF_INST_READ_OCTA_DTR))) {
        tr_debug("Flash does not need change mode");
    }

    if (mode == SOPI) {
        if ((_read_instruction != OSPIF_INST_READ_4B) && (_read_instruction != OSPIF_INST_READ_DEFAULT)) { //change mode from DOPI to SPI
            // Write new Status Register Setup
            if (_set_write_enable() != 0) {
                tr_error("Write Enabe failed");
                return -1;
            }

            config_reg2 = 0x00;

            if (OSPI_STATUS_OK == _ospi_send_general_command(OSPIF_INST_WRCR2, OSPIF_CR2_OPI_EN_ADDR, &config_reg2,
                                                             1, NULL, 0)) {
                tr_debug(" Writing Config Register2 Success: value = 0x%x", config_reg2);
            } else {
                tr_error(" Writing Config Register2 Failed");
                return -1;
            }

            // Configure BUS Mode to 1_1_1
            _ospi.configure_format(OSPI_CFG_BUS_SINGLE, OSPI_CFG_INST_SIZE_8, OSPI_CFG_BUS_SINGLE, OSPI_CFG_ADDR_SIZE_32,
                                   OSPI_CFG_BUS_SINGLE, 0, OSPI_CFG_BUS_SINGLE, 0);
        }

        _ospi.configure_format(OSPI_CFG_BUS_SINGLE, OSPI_CFG_INST_SIZE_8, OSPI_CFG_BUS_SINGLE, OSPI_CFG_ADDR_SIZE_32,
                               OSPI_CFG_BUS_SINGLE, 0, OSPI_CFG_BUS_SINGLE, 0);
        // Write new Status Register Setup
        if (_set_write_enable() != 0) {
            tr_error("Write Enabe failed");
            return -1;
        }

        config_reg2 = 0x01;

        if (OSPI_STATUS_OK == _ospi_send_general_command(OSPIF_INST_WRCR2, OSPIF_CR2_OPI_EN_ADDR, &config_reg2,
                                                         1, NULL, 0)) {
            tr_debug("OPI mode enable - Writing Config Register2 Success: value = 0x%x", config_reg2);
        } else {
            tr_error("OPI mode enable - Writing Config Register2 failed");
            return -1;
        }

        _read_instruction = DTROSPIF_INST_READ_OCTA_STR;
        _dummy_cycles = 20;

        _inst_width = OSPI_CFG_BUS_OCTA;
        _inst_size = OSPI_CFG_INST_SIZE_16;
        _address_width = OSPI_CFG_BUS_OCTA;
        _address_size = OSPI_CFG_ADDR_SIZE_32;
        _data_width = OSPI_CFG_BUS_OCTA;

        _ospi.configure_format(_inst_width, _inst_size, _address_width, _address_size, OSPI_CFG_BUS_SINGLE,
                               0, _data_width, 0);
    } else if (mode == DOPI) {
        if ((_read_instruction != OSPIF_INST_READ_4B) && (_read_instruction != OSPIF_INST_READ_DEFAULT)) {//chang mode from SOPI to SPI
            // Write new Status Register Setup
            if (_set_write_enable() != 0) {
                tr_error("Write Enabe failed");
                return -1;
            }

            config_reg2 = 0x00;

            if (OSPI_STATUS_OK == _ospi_send_general_command(OSPIF_INST_WRCR2, OSPIF_CR2_OPI_EN_ADDR, &config_reg2,
                                                             1, NULL, 0)) {
                tr_debug("OPI mode enable - Writing Config Register2 Success: value = 0x%x", config_reg2);
            } else {
                tr_error("OPI mode enable - Writing Config Register2 failed");
                return -1;
            }

            // Configure  BUS Mode to 1_1_1
            _ospi.configure_format(OSPI_CFG_BUS_SINGLE, OSPI_CFG_INST_SIZE_8, OSPI_CFG_BUS_SINGLE, OSPI_CFG_ADDR_SIZE_32,
                                   OSPI_CFG_BUS_SINGLE, 0, OSPI_CFG_BUS_SINGLE, 0);
        }

        _ospi.configure_format(OSPI_CFG_BUS_SINGLE, OSPI_CFG_INST_SIZE_8, OSPI_CFG_BUS_SINGLE, OSPI_CFG_ADDR_SIZE_32,
                               OSPI_CFG_BUS_SINGLE, 0, OSPI_CFG_BUS_SINGLE, 0);
        // Write new Status Register Setup
        if (_set_write_enable() != 0) {
            tr_error("Write Enabe failed");
            return -1;
        }

        config_reg2 = 0x02;

        if (OSPI_STATUS_OK == _ospi_send_general_command(OSPIF_INST_WRCR2, OSPIF_CR2_OPI_EN_ADDR, &config_reg2,
                                                         1, NULL, 0)) {
            tr_debug("OPI mode enable - Writing Config Register2 Success: value = 0x%x", config_reg2);
        } else {
            tr_error("OPI mode enable - Writing Config Register2 failed");
            return -1;
        }

        _read_instruction = DTROSPIF_INST_READ_OCTA_DTR;
        _dummy_cycles = 20;

        _inst_width = OSPI_CFG_BUS_OCTA_DTR;
        _inst_size = OSPI_CFG_INST_SIZE_16;
        _address_width = OSPI_CFG_BUS_OCTA_DTR;
        _address_size = OSPI_CFG_ADDR_SIZE_32;
        _data_width = OSPI_CFG_BUS_OCTA_DTR;

        _ospi.configure_format(_inst_width, _inst_size, _address_width, _address_size, OSPI_CFG_BUS_SINGLE,
                               0, _data_width, 0);
    } else if (mode == SPI) {
        // Write new Status Register Setup
        if (_set_write_enable() != 0) {
            tr_error("Write Enabe failed");
            return -1;
        }

        config_reg2 = 0x00;

        if (OSPI_STATUS_OK == _ospi_send_general_command(OSPIF_INST_WRCR2, OSPIF_CR2_OPI_EN_ADDR, &config_reg2,
                                                         1, NULL, 0)) {
            tr_debug("OPI mode enable - Writing Config Register2 Success: value = 0x%x", config_reg2);
        } else {
            tr_error("OPI mode enable - Writing Config Register2 failed");
            return -1;
        }
        _read_instruction = OSPIF_INST_READ_4B;
        _dummy_cycles = 0;

        _inst_width = OSPI_CFG_BUS_SINGLE;
        _inst_size = OSPI_CFG_INST_SIZE_8;
        _address_width = OSPI_CFG_BUS_SINGLE;
        _address_size = OSPI_CFG_ADDR_SIZE_32;
        _data_width = OSPI_CFG_BUS_SINGLE;

        _ospi.configure_format(_inst_width, _inst_size, _address_width, _address_size, OSPI_CFG_BUS_SINGLE,
                               0, _data_width, _dummy_cycles);
    }
    return status;
}


/********************************/
/*   Different Device Csel Mgmt */
/********************************/
static PinName *generate_initialized_active_ospif_csel_arr()
{
    PinName *init_arr = new PinName[OSPIF_MAX_ACTIVE_FLASH_DEVICES];
    for (int i_ind = 0; i_ind < OSPIF_MAX_ACTIVE_FLASH_DEVICES; i_ind++) {
        init_arr[i_ind] = NC;
    }
    return init_arr;
}

int OSPIFBlockDevice::add_new_csel_instance(PinName csel)
{
    int status = 0;
    _devices_mutex->lock();
    if (_number_of_active_ospif_flash_csel >= OSPIF_MAX_ACTIVE_FLASH_DEVICES) {
        status = -2;
        goto exit_point;
    }

    // verify the device is unique(no identical csel already exists)
    for (int i_ind = 0; i_ind < OSPIF_MAX_ACTIVE_FLASH_DEVICES; i_ind++) {
        if (_active_ospif_flash_csel_arr[i_ind] == csel) {
            status = -1;
            goto exit_point;
        }
    }

    // Insert new csel into existing device list
    for (int i_ind = 0; i_ind < OSPIF_MAX_ACTIVE_FLASH_DEVICES; i_ind++) {
        if (_active_ospif_flash_csel_arr[i_ind] == NC) {
            _active_ospif_flash_csel_arr[i_ind] = csel;
            break;
        }
    }
    _number_of_active_ospif_flash_csel++;

exit_point:
    _devices_mutex->unlock();
    return status;
}

int OSPIFBlockDevice::remove_csel_instance(PinName csel)
{
    int status = -1;
    _devices_mutex->lock();
    // remove the csel from existing device list
    for (int i_ind = 0; i_ind < OSPIF_MAX_ACTIVE_FLASH_DEVICES; i_ind++) {
        if (_active_ospif_flash_csel_arr[i_ind] == csel) {
            _active_ospif_flash_csel_arr[i_ind] = NC;
            if (_number_of_active_ospif_flash_csel > 0) {
                _number_of_active_ospif_flash_csel--;
            }
            status = 0;
            break;
        }
    }
    _devices_mutex->unlock();
    return status;
}

/*********************************************************/
/********** SFDP Parsing and Detection Functions *********/
/*********************************************************/
int OSPIFBlockDevice::_sfdp_parse_basic_param_table(Callback<int(bd_addr_t, void *, bd_size_t)> sfdp_reader,
                                                    sfdp_hdr_info &sfdp_info)
{
    uint8_t param_table[SFDP_BASIC_PARAMS_TBL_SIZE]; /* Up To 20 DWORDS = 80 Bytes */

    int status = sfdp_reader(sfdp_info.bptbl.addr, param_table, sfdp_info.bptbl.size);
    if (status != OSPI_STATUS_OK) {
        tr_error("Init - Read SFDP First Table Failed");
        return -1;
    }

    // Check that density is not greater than 4 gigabits (i.e. that addressing beyond 4 bytes is not required)
    if (sfdp_detect_addressability(param_table, _sfdp_info.bptbl) < 0) {
        tr_error("Verify 4byte addressing failed");
        return -1;
    }

    if (sfdp_detect_device_density(param_table, _sfdp_info.bptbl) < 0) {
        tr_error("Detecting device density failed");
        return -1;
    }

    // Set Page Size (OSPI write must be done on Page limits)
    _page_size_bytes = sfdp_detect_page_size(param_table, sfdp_info.bptbl.size);

    if (_sfdp_detect_reset_protocol_and_reset(param_table) != OSPIF_BD_ERROR_OK) {
        tr_error("Init - Detecting reset protocol/resetting failed");
        return -1;
    }

    // Detect and Set Erase Types
    bool shouldSetQuadEnable = false;
    bool is_qpi_mode = false;
    bool is_opi_mode = false;

    if (sfdp_detect_erase_types_inst_and_size(param_table, _sfdp_info) < 0) {
        tr_error("Init - Detecting erase types instructions/sizes failed");
        return -1;
    }

    // Detect and Set fastest Bus mode (default 1-1-1)
    _sfdp_detect_best_bus_read_mode(param_table, sfdp_info.bptbl.size, shouldSetQuadEnable, is_qpi_mode, is_opi_mode);
    if (true == is_opi_mode) {
        change_mode(DOPI);
        tr_debug("Init - Setting DOPI mode");
    }
    if (true == shouldSetQuadEnable) {
        if (_needs_fast_mode) {
            _enable_fast_mode();
        }
        // Set Quad Enable and QPI Bus modes if Supported
        tr_debug("Init - Setting Quad Enable");
        if (0 != _sfdp_set_quad_enabled(param_table)) {
            tr_error("Device supports Quad bus, but Quad Enable Failed");
            return -1;
        }
        if (true == is_qpi_mode) {
            tr_debug("Init - Setting QPI mode");
            _sfdp_set_qpi_enabled(param_table);
        }
    }

    if (false == _is_mem_ready()) {
        tr_error("Init - _is_mem_ready Failed");
        return -1;
    }

    return 0;
}

int OSPIFBlockDevice::_sfdp_set_quad_enabled(uint8_t *basic_param_table_ptr)
{
    uint8_t status_reg_setup[OSPI_MAX_STATUS_REGISTERS] = {0};
    uint8_t status_regs[OSPI_MAX_STATUS_REGISTERS] = {0};

    // QUAD Enable procedure is specified by 3 bits
    uint8_t qer_value = (basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_QER_BYTE] & 0x70) >> 4;

    switch (qer_value) {
        case 0:
            tr_debug("Device Does not Have a QE Bit, continue based on Read Inst");
            return 0;
        case 1:
        case 4:
            // Bit 1 of Status Reg 2
            _quad_enable_register_idx = 1;
            _quad_enable_bit = 1;
            tr_debug("Setting QE Bit, Bit 1 of Status Reg 2");
            break;
        case 2:
            // Bit 6 of Status Reg 1
            _quad_enable_register_idx = 0;
            _quad_enable_bit = 6;
            tr_debug("Setting QE Bit, Bit 6 of Status Reg 1");
            break;
        case 3:
            // Bit 7 of Status Reg 1
            _quad_enable_register_idx = 0;
            _quad_enable_bit = 7;
            _write_status_reg_2_inst = 0x3E;
            _read_status_reg_2_inst = 0x3F;
            tr_debug("Setting QE Bit, Bit 7 of Status Reg 1");
            break;
        case 5:
            // Bit 1 of status Reg 2
            _quad_enable_register_idx = 1;
            _quad_enable_bit = 1;
            tr_debug("Setting QE Bit, Bit 1 of Status Reg 2");
            break;
        default:
            tr_warning("Unsupported QER configuration");
            return 0;
    }

    if (_quad_enable_register_idx != OSPIF_NO_QUAD_ENABLE && _quad_enable_bit != OSPIF_NO_QUAD_ENABLE) {
        status_reg_setup[_quad_enable_register_idx] = 1 << _quad_enable_bit;
    }

    // Read existing status register values
    _ospi_read_status_registers(status_regs);

    // Set Bits for Quad Enable
    for (int i = 0; i < OSPI_MAX_STATUS_REGISTERS; i++) {
        status_regs[i] |= status_reg_setup[i];
    }

    // Write new Status Register Setup
    _ospi_write_status_registers(status_regs);

    if (false == _is_mem_ready()) {
        tr_error("Device not ready after write, failed");
        return -1;
    }

    // For Debug
    memset(status_regs, 0, OSPI_MAX_STATUS_REGISTERS);
    _ospi_read_status_registers(status_regs);
    if (((status_regs[0] & status_reg_setup[0]) | (status_regs[1] & status_reg_setup[1])) == 0) {
        tr_error("Status register not set correctly");
        return -1;
    }

    return 0;
}

int OSPIFBlockDevice::_sfdp_set_qpi_enabled(uint8_t *basic_param_table_ptr)
{
    uint8_t config_reg[1];

    // QPI 4-4-4 Enable Procedure is specified in 5 Bits
    uint8_t en_seq_444_value = (((basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_444_MODE_EN_SEQ_BYTE] & 0xF0) >> 4) | ((
                                                                                                                           basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_444_MODE_EN_SEQ_BYTE + 1] & 0x01) << 4));

    switch (en_seq_444_value) {
        case 1:
        case 2:
            tr_debug("_sfdp_set_qpi_enabled - send command 38h");
            if (OSPI_STATUS_OK != _ospi_send_general_command(0x38, OSPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0)) {
                tr_error("_sfdp_set_qpi_enabled - send command 38h Failed");
            }
            break;

        case 4:
            tr_debug("_sfdp_set_qpi_enabled - send command 35h");
            if (OSPI_STATUS_OK != _ospi_send_general_command(0x35, OSPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0)) {
                tr_error("_sfdp_set_qpi_enabled - send command 35h Failed");
            }
            break;

        case 8:
            tr_debug("_sfdp_set_qpi_enabled - set config bit 6 and send command 71h");
            if (OSPI_STATUS_OK != _ospi_send_general_command(0x65, 0x800003, NULL, 0, (char *)config_reg, 1)) {
                tr_error("_sfdp_set_qpi_enabled - set config bit 6 command 65h Failed");
            }
            config_reg[0] |= 0x40; //Set Bit 6
            if (OSPI_STATUS_OK != _ospi_send_general_command(0x71, 0x800003, NULL, 0, (char *)config_reg, 1)) {
                tr_error("_sfdp_set_qpi_enabled - send command 71h Failed");
            }
            break;

        case 16:
            tr_debug("_sfdp_set_qpi_enabled - reset config bits 0-7 and send command 61h");
            if (OSPI_STATUS_OK != _ospi_send_general_command(0x65, OSPI_NO_ADDRESS_COMMAND, NULL, 0, (char *)config_reg, 1)) {
                tr_error("_sfdp_set_qpi_enabled - send command 65h Failed");
            }
            config_reg[0] &= 0x7F; //Reset Bit 7 of CR
            if (OSPI_STATUS_OK != _ospi_send_general_command(0x61, OSPI_NO_ADDRESS_COMMAND, NULL, 0, (char *)config_reg, 1)) {
                tr_error("_sfdp_set_qpi_enabled - send command 61 Failed");
            }
            break;

        default:
            tr_warning("_sfdp_set_qpi_enabled - Unsupported En Seq 444 configuration");
            break;
    }
    return 0;
}

int OSPIFBlockDevice::_sfdp_detect_best_bus_read_mode(uint8_t *basic_param_table_ptr, int basic_param_table_size,
                                                      bool &set_quad_enable, bool &is_qpi_mode, bool &is_opi_mode)
{
    set_quad_enable = false;
    is_qpi_mode = false;
    is_opi_mode = false;
    uint8_t examined_byte;

    do { // compound statement is the loop body
        if (basic_param_table_size > OSPIF_BASIC_PARAM_TABLE_OPI_READ_SUPPORT_BYTE) {
            examined_byte = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_OPI_READ_SUPPORT_BYTE];
            tr_debug("\r\n examined_byte, : 0x%xh", examined_byte);
            if (examined_byte & 0x01) {
                // OPI 8-8-8 Supported
                is_opi_mode = true;
                _dummy_cycles = (basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_888_READ_INST_BYTE - 1] >> 5)
                                + (basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_888_READ_INST_BYTE - 1] & 0x1F);
                tr_debug("Read Bus Mode set to 8-8-8, Instruction: 0x%xh", _read_instruction);
                break;
            }
        }

        if (basic_param_table_size > OSPIF_BASIC_PARAM_TABLE_QPI_READ_SUPPORT_BYTE) {
            examined_byte = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_QPI_READ_SUPPORT_BYTE];
            if (examined_byte & 0x10) {
                // QPI 4-4-4 Supported
                _read_instruction = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_444_READ_INST_BYTE];
                set_quad_enable = true;
                is_qpi_mode = true;
                _dummy_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_444_READ_INST_BYTE - 1] & 0x1F;
                uint8_t mode_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_444_READ_INST_BYTE - 1] >> 5;
                _alt_size = mode_cycles * 4;
                tr_debug("Read Bus Mode set to 4-4-4, Instruction: 0x%xh", _read_instruction);
                _address_width = OSPI_CFG_BUS_QUAD;
                _data_width = OSPI_CFG_BUS_QUAD;
                break;
            }
        }

        examined_byte = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_FAST_READ_SUPPORT_BYTE];
        if (examined_byte & 0x20) {
            //  Fast Read 1-4-4 Supported
            _read_instruction = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_144_READ_INST_BYTE];
            set_quad_enable = true;
            _dummy_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_144_READ_INST_BYTE - 1] & 0x1F;
            uint8_t mode_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_144_READ_INST_BYTE - 1] >> 5;
            _alt_size = mode_cycles * 4;
            _address_width = OSPI_CFG_BUS_QUAD;
            _data_width = OSPI_CFG_BUS_QUAD;
            tr_debug("Read Bus Mode set to 1-4-4, Instruction: 0x%xh", _read_instruction);
            break;
        }

        if (examined_byte & 0x40) {
            //  Fast Read 1-1-4 Supported
            _read_instruction = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_114_READ_INST_BYTE];
            set_quad_enable = true;
            _dummy_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_114_READ_INST_BYTE - 1] & 0x1F;
            uint8_t mode_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_114_READ_INST_BYTE - 1] >> 5;
            _alt_size = mode_cycles;
            _data_width = OSPI_CFG_BUS_QUAD;
            tr_debug("Read Bus Mode set to 1-1-4, Instruction: 0x%xh", _read_instruction);
            break;
        }
        examined_byte = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_QPI_READ_SUPPORT_BYTE];
        if (examined_byte & 0x01) {
            //  Fast Read 2-2-2 Supported
            _read_instruction = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_222_READ_INST_BYTE];
            _dummy_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_222_READ_INST_BYTE - 1] & 0x1F;
            uint8_t mode_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_222_READ_INST_BYTE - 1] >> 5;
            _alt_size = mode_cycles * 2;
            _address_width = OSPI_CFG_BUS_DUAL;
            _data_width = OSPI_CFG_BUS_DUAL;
            tr_debug("Read Bus Mode set to 2-2-2, Instruction: 0x%xh", _read_instruction);
            break;
        }

        examined_byte = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_FAST_READ_SUPPORT_BYTE];
        if (examined_byte & 0x10) {
            //  Fast Read 1-2-2 Supported
            _read_instruction = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_122_READ_INST_BYTE];
            _dummy_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_122_READ_INST_BYTE - 1] & 0x1F;
            uint8_t mode_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_122_READ_INST_BYTE - 1] >> 5;
            _alt_size = mode_cycles * 2;
            _address_width = OSPI_CFG_BUS_DUAL;
            _data_width = OSPI_CFG_BUS_DUAL;
            tr_debug("Read Bus Mode set to 1-2-2, Instruction: 0x%xh", _read_instruction);
            break;
        }
        if (examined_byte & 0x01) {
            // Fast Read 1-1-2 Supported
            _read_instruction = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_112_READ_INST_BYTE];
            _dummy_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_112_READ_INST_BYTE - 1] & 0x1F;
            uint8_t mode_cycles = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_112_READ_INST_BYTE - 1] >> 5;
            _alt_size = mode_cycles;
            _data_width = OSPI_CFG_BUS_DUAL;
            tr_debug("Read Bus Mode set to 1-1-2, Instruction: 0x%xh", _read_instruction);
            break;
        }
        _read_instruction = OSPIF_INST_READ_DEFAULT;
        tr_debug("Read Bus Mode set to 1-1-1, Instruction: 0x%xh", _read_instruction);
    } while (false);

    return 0;
}

int OSPIFBlockDevice::_sfdp_detect_and_enable_4byte_addressing(uint8_t *basic_param_table_ptr, int basic_param_table_size)
{
    int status = OSPIF_BD_ERROR_OK;
    ospi_status_t ospi_status = OSPI_STATUS_OK;

    // Always enable 4-byte addressing if possible
    if (basic_param_table_size > OSPIF_BASIC_PARAM_TABLE_4BYTE_ADDR_BYTE) {
        uint8_t examined_byte = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_4BYTE_ADDR_BYTE];

        if (examined_byte & FOURBYTE_ADDR_ALWAYS_BITMASK) {
            // No need to do anything if 4-byte addressing is always enabled
            _address_size = OSPI_CFG_ADDR_SIZE_32;
        } else if (examined_byte & FOURBYTE_ADDR_B7_BITMASK) {
            // Issue instruction B7h to enable 4-byte addressing
            ospi_status = _ospi_send_general_command(0xB7, OSPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0);
            status = (ospi_status == OSPI_STATUS_OK) ? OSPIF_BD_ERROR_OK : OSPIF_BD_ERROR_PARSING_FAILED;
            if (status == OSPIF_BD_ERROR_OK) {
                _address_size = OSPI_CFG_ADDR_SIZE_32;
            }
        } else if (examined_byte & FOURBYTE_ADDR_B7_WREN_BITMASK) {
            // Issue WREN and then instruction B7h to enable 4-byte addressing
            if (_set_write_enable() == 0) {
                ospi_status = _ospi_send_general_command(0xB7, OSPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0);
                status = (ospi_status == OSPI_STATUS_OK) ? OSPIF_BD_ERROR_OK : OSPIF_BD_ERROR_PARSING_FAILED;

                if (status == OSPIF_BD_ERROR_OK) {
                    _address_size = OSPI_CFG_ADDR_SIZE_32;
                }
            } else {
                tr_error("Write enable failed");
                status = OSPIF_BD_ERROR_WREN_FAILED;
            }
        } else if (examined_byte & FOURBYTE_ADDR_CONF_REG_BITMASK) {
            // Write 1 to bit 0 of a configuration register to enable 4-byte addressing
            // Write to register with instruction B1h, read from register with instruction B5h
            uint8_t conf_register = 0;
            ospi_status = _ospi_send_general_command(0xB5, OSPI_NO_ADDRESS_COMMAND, NULL, 0, (char *) &conf_register, 1);
            status = (ospi_status == OSPI_STATUS_OK) ? OSPIF_BD_ERROR_OK : OSPIF_BD_ERROR_PARSING_FAILED;

            if (status == OSPIF_BD_ERROR_OK) {
                conf_register |= 0b00000001;
                if (_set_write_enable() == 0) {
                    ospi_status_t ospi_status = _ospi_send_general_command(0xB1, OSPI_NO_ADDRESS_COMMAND, (char *) &conf_register, 1, NULL, 0);
                    status = (ospi_status == OSPI_STATUS_OK) ? OSPIF_BD_ERROR_OK : OSPIF_BD_ERROR_PARSING_FAILED;

                    if (status == OSPIF_BD_ERROR_OK) {
                        _address_size = OSPI_CFG_ADDR_SIZE_32;
                    }
                } else {
                    tr_error("Write enable failed");
                    status = OSPIF_BD_ERROR_WREN_FAILED;
                }
            }
        } else if (examined_byte & FOURBYTE_ADDR_BANK_REG_BITMASK) {
            // Write 1 to bit 7 of a bank register to enable 4-byte addressing
            // Write to register with instruction 17h, read from register with instruction 16h
            uint8_t to_write = 0b10000000;
            ospi_status = _ospi_send_general_command(0x17, OSPI_NO_ADDRESS_COMMAND, (char *) &to_write, 1, NULL, 0);
            status = (ospi_status == OSPI_STATUS_OK) ? OSPIF_BD_ERROR_OK : OSPIF_BD_ERROR_PARSING_FAILED;
            if (status == OSPIF_BD_ERROR_OK) {
                _address_size = OSPI_CFG_ADDR_SIZE_32;
            }
        } else if (examined_byte & FOURBYTE_ADDR_EXT_ADDR_REG_BITMASK) {
            // Extended address register stores most significant byte of a 4-byte address
            // Instructions are sent with the lower 3 bytes of the address
            // Write to register with instruction C5h, read from register with instruction C8h
            _4byte_msb_reg_write_inst = 0xC5;
            _address_size = OSPI_CFG_ADDR_SIZE_24;
        } else if (_support_4_byte_inst == true) {
            // Extended address register stores most significant byte of a 4-byte address
            _address_size = OSPI_CFG_ADDR_SIZE_32;
            switch (_read_instruction) {
                case OSPIF_INST_READ_DEFAULT:
                    _read_instruction = OSPIF_INST_READ_4B;
                    break;
                case 0xc2:
                    break;
            }
            _prog_instruction = OSPIF_INST_PROG_4B;
        } else {
            // Either part specific instructions are required to use 4-byte addressing or it isn't supported, so use 3-byte addressing instead
            tr_debug("_sfdp_detect_and_enable_4byte_addressing - 4-byte addressing not supported, falling back to 3-byte addressing");
            _address_size = OSPI_CFG_ADDR_SIZE_24;
        }

        if (_address_size == OSPI_CFG_ADDR_SIZE_32) {
            // Update 1-1-1 format to match new address size
            if (OSPI_STATUS_OK != _ospi.configure_format(OSPI_CFG_BUS_SINGLE, OSPI_CFG_INST_SIZE_8, OSPI_CFG_BUS_SINGLE, _address_size, OSPI_CFG_BUS_SINGLE,
                                                         0, OSPI_CFG_BUS_SINGLE, 0)) {
                tr_error("_ospi_configure_format failed");
                status = OSPIF_BD_ERROR_DEVICE_ERROR;
            }
        }
    }

    return status;
}

#if MBED_CONF_OSPIF_ENABLE_AND_RESET && MBED_CONF_OSPIF_DIRECT_RESET
#error "ospif.enable-and-reset and ospif.direct-reset cannot be both true!"
#endif

#define RESET_SEQUENCE_FROM_SFDP ( !MBED_CONF_OSPIF_ENABLE_AND_RESET && !MBED_CONF_OSPIF_DIRECT_RESET )


int OSPIFBlockDevice::_sfdp_detect_reset_protocol_and_reset(uint8_t *basic_param_table_ptr)
{
    int status = OSPIF_BD_ERROR_OK;

#if RESET_SEQUENCE_FROM_SFDP
    uint8_t examined_byte = basic_param_table_ptr[OSPIF_BASIC_PARAM_TABLE_SOFT_RESET_BYTE];

    // Ignore bit indicating need to exit 0-4-4 mode - should not enter 0-4-4 mode from OSPIFBlockDevice
    if (examined_byte & SOFT_RESET_RESET_INST_BITMASK) {
#endif

#if !MBED_CONF_OSPIF_ENABLE_AND_RESET     // i.e. direct reset, or determined from SFDP
        // Issue instruction 0xF0 to reset the device
        ospi_status_t ospi_status = _ospi_send_general_command(0xF0, OSPI_NO_ADDRESS_COMMAND, // Send reset instruction
                                                               NULL, 0, NULL, 0);
        status = (ospi_status == OSPI_STATUS_OK) ? OSPIF_BD_ERROR_OK : OSPIF_BD_ERROR_PARSING_FAILED;
#endif

#if RESET_SEQUENCE_FROM_SFDP
    } else if (examined_byte & SOFT_RESET_ENABLE_AND_RESET_INST_BITMASK) {
#endif

#if !MBED_CONF_OSPIF_DIRECT_RESET    // i.e. enable and reset, or determined from SFDP
        // Issue instruction 66h to enable resets on the device
        // Then issue instruction 99h to reset the device
        ospi_status_t ospi_status = _ospi_send_general_command(0x66, OSPI_NO_ADDRESS_COMMAND, // Send reset enable instruction
                                                               NULL, 0, NULL, 0);
        if (ospi_status == OSPI_STATUS_OK) {
            ospi_status = _ospi_send_general_command(0x99, OSPI_NO_ADDRESS_COMMAND, // Send reset instruction
                                                     NULL, 0, NULL, 0);
        }
        status = (ospi_status == OSPI_STATUS_OK) ? OSPIF_BD_ERROR_OK : OSPIF_BD_ERROR_PARSING_FAILED;
#endif

#if RESET_SEQUENCE_FROM_SFDP
    } else {
        // Soft reset either is not supported or requires direct control over data lines
        tr_error("Failed to determine soft reset sequence. If your device has a legacy SFDP table, please manually set enable-and-reset or direct-reset.");

        status = OSPIF_BD_ERROR_PARSING_FAILED;
    }
#endif

    if (status == OSPIF_BD_ERROR_OK) {
        if (false == _is_mem_ready()) {
            tr_error("Device not ready, reset failed");
            status = OSPIF_BD_ERROR_READY_FAILED;
        }
    }

    return status;
}

int OSPIFBlockDevice::_sfdp_parse_4_byte_inst_table(Callback<int(bd_addr_t, void *, bd_size_t)> sfdp_reader,
                                                    sfdp_hdr_info &sfdp_info)
{
    uint8_t four_byte_inst_table[SFDP_DEFAULT_4_BYTE_INST_TABLE_SIZE_BYTES]; /* Up To 2 DWORDS = 8 Bytes */

    int status = sfdp_reader(sfdp_info.fbatbl.addr, four_byte_inst_table, sfdp_info.fbatbl.size);
    if (status != OSPI_STATUS_OK) {
        tr_error("Init - Read SFDP Four Byte Inst Table Failed");
        return -1;
    }

    // Support 4-Byte address instructions
    if (four_byte_inst_table[OSPIF_4_BYTE_INST_TABLE_FAST_READ_SUPPORT_BYTE] & FOURBYTE_INST_1_1_1_READ_BITMASK) {
        _support_4_byte_inst = true;
    }

    // Detect erase types 4-Byte instructions
    if (sfdp_info.fbatbl.size > OSPIF_4_BYTE_INST_TABLE_INST_ERASE_TYPE_1_BYTE) {
        // Loop Erase Types 1-4
        for (int i_ind = 0; i_ind < 4; i_ind++) {
            sfdp_info.fbatbl.erase_type_4_byte_inst_arr[i_ind] = OSPI_NO_INST; // Default for unsupported type
            if (four_byte_inst_table[OSPIF_4_BYTE_INST_TABLE_ERASE_SUPPORT_BYTE] & (FOURBYTE_INST_ERASE_TYPE_1_BITMASK << i_ind)) {
                sfdp_info.fbatbl.erase_type_4_byte_inst_arr[i_ind] = four_byte_inst_table[OSPIF_4_BYTE_INST_TABLE_INST_ERASE_TYPE_1_BYTE + i_ind];
            }
            tr_debug("Erase Type %d - Inst: 0x%xh", (i_ind + 1), sfdp_info.fbatbl.erase_type_4_byte_inst_arr[i_ind]);
        }
    }

    return 0;
}

int OSPIFBlockDevice::_handle_vendor_quirks()
{
    uint8_t vendor_device_ids[OSPI_RDID_DATA_LENGTH] = {0};
    /* Read Manufacturer ID (1byte), and Device ID (2bytes) */
    ospi_status_t status = _ospi_send_general_command(OSPIF_INST_RDID, OSPI_NO_ADDRESS_COMMAND,
                                                      NULL, 0,
                                                      (char *) vendor_device_ids, OSPI_RDID_DATA_LENGTH);
    if (OSPI_STATUS_OK != status) {
        tr_error("Read Vendor ID Failed");
        return -1;
    }

    tr_debug("Vendor device ID = 0x%x 0x%x 0x%x", vendor_device_ids[0], vendor_device_ids[1], vendor_device_ids[2]);

    switch (vendor_device_ids[0]) {
        case 0xbf:
            // SST devices come preset with block protection
            // enabled for some regions, issue global protection unlock to clear
            tr_debug("Applying quirks for SST");
            _clear_protection_method = OSPIF_BP_ULBPR;
            break;
        case 0xc2:
            // Macronix devices have several quirks:
            // 1. Have one status register and 2 config registers, with a nonstandard instruction for reading the config registers
            // 2. Require setting a "fast mode" bit in config register 2 to operate at higher clock rates
            // 3. Should never attempt to enable 4-byte addressing (it causes reads and writes to fail)
            tr_debug("Applying quirks for macronix");
            _needs_fast_mode = true;
            _num_status_registers = 3;
            _read_status_reg_2_inst = OSPIF_INST_RDCR;
            break;
    }

    return 0;
}

int OSPIFBlockDevice::_clear_block_protection()
{
    uint8_t status_regs[OSPI_MAX_STATUS_REGISTERS] = {0};

    if (false == _is_mem_ready()) {
        tr_error("Device not ready, clearing block protection failed");
        return -1;
    }
    ospi_status_t status;
    switch (_clear_protection_method) {
        case OSPIF_BP_ULBPR:
            tr_debug("Clearing block protection via ULBPR");
            // SST devices come preset with block protection
            // enabled for some regions, issue global protection unlock to clear
            if (0 != _set_write_enable()) {
                tr_error("Write enable failed");
                return -1;
            }
            status = _ospi_send_general_command(OSPIF_INST_ULBPR, OSPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0);
            if (OSPI_STATUS_OK != status) {
                tr_error("Global block protection unlock failed");
                return -1;
            }
            break;
        case OSPIF_BP_CLEAR_SR:
            // For all other devices, to clear the block protection bits clear all bits
            // in status register 1 that aren't the WIP or WEL bits, or the QE bit (if it is in SR 1)
            tr_debug("Clearing block protection via status register protection bits");
            status = _ospi_read_status_registers(status_regs);
            if (OSPI_STATUS_OK != status) {
                tr_error("_clear_block_protection - Status register read failed");
                return -1;
            }
            uint8_t status_mask = (OSPIF_STATUS_BIT_WIP | OSPIF_STATUS_BIT_WEL);
            if (_quad_enable_register_idx == 0) {
                status_mask |= 1 << _quad_enable_bit;
            }
            status_regs[0] &= status_mask;
            status = _ospi_write_status_registers(status_regs);
            if (OSPI_STATUS_OK != status) {
                tr_error("__clear_block_protection - Status register write failed");
                return -1;
            }
            break;
    }

    if (false == _is_mem_ready()) {
        tr_error("Device not ready, clearing block protection failed");
        return -1;
    }

    return 0;
}

int OSPIFBlockDevice::_set_write_enable()
{
    // Check Status Register Busy Bit to Verify the Device isn't Busy
    uint16_t status_value = 0;
    int status = -1;

    do {
        if (OSPI_STATUS_OK !=  _ospi_send_general_command(OSPIF_INST_WREN, OSPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0)) {
            tr_error("Sending WREN command FAILED");
            break;
        }

        if (false == _is_mem_ready()) {
            tr_error("Device not ready, write failed");
            break;
        }
        //the number of read byte need to be even in octa flash DOPI mode
        if (OSPI_STATUS_OK != _ospi_send_general_command(OSPIF_INST_RSR1, OSPI_NO_ADDRESS_COMMAND,
                                                         NULL, 0,
                                                         (char *) &status_value, 2)) {
            tr_error("Reading Status Register 1 failed");
            break;
        }

        if ((status_value & OSPIF_STATUS_BIT_WEL) == 0) {
            tr_error("_set_write_enable failed - status register 1 value: %u", status_value);
            break;
        }

        status = 0;
    } while (false);

    return status;
}

int OSPIFBlockDevice::_enable_fast_mode()
{
    tr_debug("enabling fast mode");
    MBED_ASSERT(_num_status_registers == 3); // Make sure the register for fast mode enable exists
    uint8_t status_reg[OSPI_MAX_STATUS_REGISTERS] = {0};

    // Bit 1 of config reg 2 (aka "status register 3" in our generic register representation)
    const int QER_REG_IDX = 2;
    const int QER_REG_VALUE = 0x2;

    // Configure  BUS Mode to 1_1_1 for all commands other than Read
    if (OSPI_STATUS_OK != _ospi.configure_format(OSPI_CFG_BUS_SINGLE, OSPI_CFG_INST_SIZE_8, OSPI_CFG_BUS_SINGLE, OSPI_CFG_ADDR_SIZE_24, OSPI_CFG_BUS_SINGLE,
                                                 0, OSPI_CFG_BUS_SINGLE, 0)) {
        tr_error("_ospi_configure_format failed");
        return -1;

    }

    if (OSPI_STATUS_OK == _ospi_read_status_registers(status_reg)) {
        tr_debug("Reading Config Register Success: value = 0x%x", status_reg[2]);
    } else {
        tr_error("Reading Config Register failed");
        return -1;
    }

    // Set Bits for Quad Enable
    status_reg[QER_REG_IDX] |= QER_REG_VALUE;

    // Write new Status Register Setup
    if (_set_write_enable() != 0) {
        tr_error("Write Enable failed");
        return -1;
    }

    if (OSPI_STATUS_OK == _ospi_write_status_registers(status_reg)) {
        tr_debug("fast mode enable - Writing Config Register Success: value = 0x%x",
                 (int)status_reg[2]);
    } else {
        tr_error("fast mode enable - Writing Config Register failed");
        return -1;
    }

    if (false == _is_mem_ready()) {
        tr_error("Device not ready after write, failed");
        return -1;
    }

    // For Debug
    memset(status_reg, 0, OSPI_MAX_STATUS_REGISTERS);
    if (OSPI_STATUS_OK == _ospi_read_status_registers(status_reg)) {
        tr_debug("Verifying Register Success: status = 0x%x config 1 = 0x%x config 2 = 0x%x", (int)status_reg[0], (int)status_reg[1], (int)status_reg[2]);
    } else {
        tr_error("Verifying Config Register failed");
        return -1;
    }

    return 0;
}

bool OSPIFBlockDevice::_is_mem_ready()
{
    // Check Status Register Busy Bit to Verify the Device isn't Busy
    uint16_t status_value = 0;
    int retries = 0;
    bool mem_ready = true;

    do {
        rtos::ThisThread::sleep_for(1ms);
        retries++;
        //Read Status Register 1 from device, the number of read byte need to be even in octa flash DOPI mode
        if (OSPI_STATUS_OK != _ospi_send_general_command(OSPIF_INST_RSR1, OSPI_NO_ADDRESS_COMMAND,
                                                         NULL, 0,
                                                         (char *) &status_value, OSPI_DEFAULT_STATUS_REGISTERS)) { // store received value in status_value
            tr_error("Reading Status Register failed");
        }
    } while ((status_value & OSPIF_STATUS_BIT_WIP) != 0 && retries < IS_MEM_READY_MAX_RETRIES);

    if ((status_value & OSPIF_STATUS_BIT_WIP) != 0) {
        tr_error("_is_mem_ready FALSE: status value = 0x%x ", status_value);
        mem_ready = false;
    }
    return mem_ready;
}

#ifdef MX_FLASH_SUPPORT_RWW
bool OSPIFBlockDevice::_is_mem_ready_rww(bd_addr_t addr, uint8_t rw)
{
    uint16_t cr2_value = 0;
    bool mem_ready = true;
    static uint32_t rww_cnt = 0;   // For testing
    static uint32_t rwe_cnt = 0;   // For testing

    bd_addr_t bank_addr = addr & MX_FLASH_BANK_SIZE_MASK;

    if ((_wait_flag == NOT_STARTED) || (!rw && bank_addr != _busy_bank)) {
        return mem_ready;
    }
    //Read CR2 Register 1 from device, the number of read byte need to be even in octa flash DOPI mode
    if (OSPI_STATUS_OK != _ospi_send_general_command(OSPIF_INST_RDCR2, bank_addr + OSPIF_CR2_BANK_STATUS_ADDR,
                                                     NULL, 0,
                                                     (char *) &cr2_value, OSPI_DEFAULT_STATUS_REGISTERS)) { // store received value in cr2_value
        tr_error("Reading CR2 Register failed");
    }

    cr2_value &= OSPIF_CR2_RWWBS;

    if ((cr2_value == OSPIF_CR2_RWWBS) || (rw && (cr2_value == OSPIF_CR2_RWWDS))) {

        // Wait until device ready
        if (false == _is_mem_ready()) {
            tr_error(" _is_mem_ready Failed");
            mem_ready = false;
        }
        _wait_flag = NOT_STARTED;
    } else if (!rw && (cr2_value == OSPIF_CR2_RWWDS)) {
        // For testing
        if (_wait_flag == WRITE_WAIT_STARTED) {
            rww_cnt++;
            tr_debug("rww_cnt = 0x%x ", rww_cnt);
        } else {
            rwe_cnt++;
            tr_debug("rwe_cnt = 0x%x ", rwe_cnt);
        }
    }

    return mem_ready;
}
#endif

/***************************************************/
/*********** OSPI Driver API Functions *************/
/***************************************************/
ospi_status_t OSPIFBlockDevice::_ospi_set_frequency(int freq)
{
    return _ospi.set_frequency(freq);
}

ospi_status_t OSPIFBlockDevice::_ospi_update_4byte_ext_addr_reg(bd_addr_t addr)
{
    ospi_status_t status = OSPI_STATUS_OK;
    // Only update register if in the extended address register mode
    if (_4byte_msb_reg_write_inst != OSPI_NO_INST) {
        // Set register to the most significant byte of the address
        uint8_t most_significant_byte = addr >> 24;
        if (_set_write_enable() == 0) {
            status = _ospi.command_transfer(_4byte_msb_reg_write_inst, (int) OSPI_NO_ADDRESS_COMMAND,
                                            (char *) &most_significant_byte, 1,
                                            NULL, 0);
        } else {
            tr_error("Write enable failed");
            status = OSPI_STATUS_ERROR;
        }
    } else if ((_address_size != OSPI_CFG_ADDR_SIZE_32) && (addr != OSPI_NO_ADDRESS_COMMAND) && (addr >= (1 << 24))) {
        tr_error("Attempted to use 4-byte address but 4-byte addressing is not supported");
        status = OSPI_STATUS_ERROR;
    }
    return status;
}

ospi_status_t OSPIFBlockDevice::_ospi_send_read_command(ospi_inst_t read_inst, void *buffer,
                                                        bd_addr_t addr, bd_size_t size)
{
    tr_debug("Inst: 0x%xh, addr: %llu, size: %llu", read_inst, addr, size);

    size_t buf_len = size;

    ospi_status_t status = _ospi_update_4byte_ext_addr_reg(addr);
    if (OSPI_STATUS_OK != status) {
        tr_error("OSPI Read - Updating 4-byte addressing extended address register failed");
        return status;
    }

    // Send read command to device driver
    // Read commands use the best bus mode supported by the part
    status = _ospi.configure_format(_inst_width, _inst_size, _address_width, _address_size, _address_width, // Alt width should be the same as address width
                                    _alt_size, _data_width, _dummy_cycles);
    if (OSPI_STATUS_OK != status) {
        tr_error("_ospi_configure_format failed");
        return status;
    }

    // Don't check the read status until after we've configured the format back to 1-1-1, to avoid leaving the interface in an
    // incorrect state if the read fails.
    status = _ospi.read(read_inst, (_alt_size == 0) ? -1 : OSPI_ALT_DEFAULT_VALUE, (unsigned int)addr, (char *)buffer, &buf_len);

    status = _ospi.configure_format(_inst_width, _inst_size, _address_width, _address_size, _address_width, // Alt width should be the same as address width
                                    _alt_size, _data_width, 0);
    if (OSPI_STATUS_OK != status) {
        tr_error("_ospi_configure_format failed");
        return status;
    }

    if (OSPI_STATUS_OK != status) {
        tr_error("OSPI Read failed");
        return status;
    }

    return OSPI_STATUS_OK;
}

ospi_status_t OSPIFBlockDevice::_ospi_send_program_command(ospi_inst_t prog_inst, const void *buffer,
                                                           bd_addr_t addr, bd_size_t *size)
{
    tr_debug("Inst: 0x%xh, addr: %llu, size: %llu", prog_inst, addr, *size);

    ospi_status_t status = _ospi_update_4byte_ext_addr_reg(addr);
    if (OSPI_STATUS_OK != status) {
        tr_error("OSPI Write - Updating 4-byte addressing extended address register failed");
        return status;
    }

    // Send program (write) command to device driver
    status = _ospi.write(prog_inst, -1, addr, (char *)buffer, (size_t *)size);
    if (OSPI_STATUS_OK != status) {
        tr_error("OSPI Write failed");
        return status;
    }

    return OSPI_STATUS_OK;
}

ospi_status_t OSPIFBlockDevice::_ospi_send_erase_command(ospi_inst_t erase_inst, bd_addr_t addr, bd_size_t size)
{
    tr_debug("Inst: 0x%xh, addr: %llu, size: %llu", erase_inst, addr, size);

    ospi_status_t status = _ospi_update_4byte_ext_addr_reg(addr);
    if (OSPI_STATUS_OK != status) {
        tr_error("OSPI Erase - Updating 4-byte addressing extended address register failed");
        return status;
    }

    // Send erase command to driver
    status = _ospi.command_transfer(erase_inst, (int) addr, NULL, 0, NULL, 0);

    if (OSPI_STATUS_OK != status) {
        tr_error("OSPI Erase failed");
        return status;
    }

    return OSPI_STATUS_OK;
}

ospi_status_t OSPIFBlockDevice::_ospi_send_general_command(ospi_inst_t instruction, bd_addr_t addr,
                                                           const char *tx_buffer, bd_size_t tx_length,
                                                           const char *rx_buffer, bd_size_t rx_length)
{
    tr_debug("Inst: 0x%xh, addr: %llu, tx length: %llu, rx length: %llu", instruction, addr, tx_length, rx_length);

    ospi_status_t status = _ospi_update_4byte_ext_addr_reg(addr);
    if (OSPI_STATUS_OK != status) {
        tr_error("OSPI Generic command - Updating 4-byte addressing extended address register failed");
        return status;
    }

    if ((_inst_width == OSPI_CFG_BUS_OCTA) || (_inst_width == OSPI_CFG_BUS_OCTA_DTR)) {
        if ((instruction == OSPIF_INST_RSR1) || (instruction == OSPIF_INST_RDID) ||
                (instruction == OSPIF_INST_RDCR2) || (instruction == OSPIF_INST_RDCR)) {
            _ospi.configure_format(_inst_width, _inst_size, _address_width, _address_size, OSPI_CFG_BUS_SINGLE,
                                   0, _data_width, _dummy_cycles);
            if (instruction != OSPIF_INST_RDCR2) {
                addr = 0;
            }
        } else if ((instruction == OSPIF_INST_WSR1)) {
            addr = 0;
        }
    }

    // Send a general command instruction to driver
    status = _ospi.command_transfer(instruction, (int)addr, tx_buffer, tx_length, rx_buffer, rx_length);
    if (OSPI_STATUS_OK != status) {
        tr_error("Sending Generic command: %x", instruction);
        return status;
    }

    if (((_inst_width == OSPI_CFG_BUS_OCTA) || (_inst_width == OSPI_CFG_BUS_OCTA_DTR)) &&
            ((instruction == OSPIF_INST_RSR1) || (instruction == OSPIF_INST_RDID) ||
             (instruction == OSPIF_INST_RDCR2) || (instruction == OSPIF_INST_RDCR))) {
        _ospi.configure_format(_inst_width, _inst_size, _address_width, _address_size, OSPI_CFG_BUS_SINGLE,
                               0, _data_width, 0);
    }

    return OSPI_STATUS_OK;
}

int OSPIFBlockDevice::_ospi_send_read_sfdp_command(bd_addr_t addr, void *rx_buffer, bd_size_t rx_length)
{
    size_t rx_len = rx_length;
    uint8_t *rx_buffer_tmp = (uint8_t *)rx_buffer;

    // SFDP read instruction requires 1-1-1 bus mode with 8 dummy cycles and a 3-byte address
    ospi_status_t status = _ospi.configure_format(OSPI_CFG_BUS_SINGLE, OSPI_CFG_INST_SIZE_8, OSPI_CFG_BUS_SINGLE, OSPI_CFG_ADDR_SIZE_24, OSPI_CFG_BUS_SINGLE, 0, OSPI_CFG_BUS_SINGLE, OSPIF_RSFDP_DUMMY_CYCLES);
    if (OSPI_STATUS_OK != status) {
        tr_error("_ospi_configure_format failed");
        return status;
    }

    // Don't check the read status until after we've configured the format back to 1-1-1, to avoid leaving the interface in an
    // incorrect state if the read fails.
    status = _ospi.read(OSPIF_INST_RSFDP, -1, (unsigned int) addr, (char *) rx_buffer, &rx_len);

    ospi_status_t format_status = _ospi.configure_format(OSPI_CFG_BUS_SINGLE, OSPI_CFG_INST_SIZE_8, OSPI_CFG_BUS_SINGLE, _address_size, OSPI_CFG_BUS_SINGLE, 0, OSPI_CFG_BUS_SINGLE, 0);
    // All commands other than Read and RSFDP use default 1-1-1 bus mode (Program/Erase are constrained by flash memory performance more than bus performance)
    if (OSPI_STATUS_OK != format_status) {
        tr_error("_ospi_configure_format failed");
        return format_status;
    }

    if (OSPI_STATUS_OK != status) {
        tr_error("Sending SFDP read instruction");
        return status;
    }

    // This is a workaround,
    // The sfdp parameter values in Macronix old octaflash(include the MX25LM51245G on L4R9I_DISCO) are all 0xFF,
    // so we need to define the parameter values by software to support SFDP parsing.
    // The code below can be removed when users test with the new flash.
#ifdef NEED_DEFINE_SFDP_PARA
    if (!(memcmp(rx_buffer, "SFDP", 4) == 0)) {
        if (addr < 30) {
            for (uint8_t i_ind = 0; i_ind < rx_len; i_ind++) {
                *(rx_buffer_tmp + i_ind) = _sfdp_head_table[i_ind + addr];
            }
        } else if (addr == 0x30) {
            for (uint8_t i_ind = 0; i_ind < rx_len; i_ind++) {
                *(rx_buffer_tmp + i_ind) = _sfdp_basic_param_table[i_ind];
            }
        } else if (addr == 0xc0) {
            for (uint8_t i_ind = 0; i_ind < rx_len; i_ind++) {
                *(rx_buffer_tmp + i_ind) = _sfdp_4_byte_inst_table[i_ind];
            }
        }
    }
#endif

    return OSPI_STATUS_OK;
}

ospi_status_t OSPIFBlockDevice::_ospi_read_status_registers(uint8_t *reg_buffer)
{
    // Read Status Register 1, the number of read byte need to be even in octa flash DOPI mode
    ospi_status_t status = _ospi_send_general_command(OSPIF_INST_RSR1, OSPI_NO_ADDRESS_COMMAND,
                                                      NULL, 0,
                                                      (char *) &reg_buffer[0], OSPI_DEFAULT_STATUS_REGISTERS);
    if (OSPI_STATUS_OK == status) {
        tr_debug("Reading Status Register 1 Success: value = 0x%x", (int) reg_buffer[0]);
    } else {
        tr_error("Reading Status Register 1 failed");
        return status;
    }

    // Read Status Register 2 (and beyond, if applicable)
    unsigned int read_length = _num_status_registers - 1; // We already read status reg 1 above
    status = _ospi_send_general_command(_read_status_reg_2_inst, OSPI_NO_ADDRESS_COMMAND,
                                        NULL, 0,
                                        (char *) &reg_buffer[1], read_length);
    if (OSPI_STATUS_OK == status) {
        tr_debug("Reading Status Register 2 Success: value = 0x%x", (int) reg_buffer[1]);
        if (_num_status_registers > 2) {
            tr_debug("Reading Register 3 Success: value = 0x%x", (int) reg_buffer[2]);
        }
    } else {
        tr_error("Reading Status Register 2 failed");
        return status;
    }

    return OSPI_STATUS_OK;
}

ospi_status_t OSPIFBlockDevice::_ospi_write_status_registers(uint8_t *reg_buffer)
{
    ospi_status_t status;

    if (_write_status_reg_2_inst == OSPI_NO_INST) {
        // Status registers are written on different data bytes of the same command
        if (_set_write_enable() != 0) {
            tr_error("Write Enable failed");
            return OSPI_STATUS_ERROR;
        }
        //for octa flash SOPI mode, WRSR just write 1 byte
        if (_read_instruction == DTROSPIF_INST_READ_OCTA_STR) {
            status = _ospi_send_general_command(OSPIF_INST_WSR1, OSPI_NO_ADDRESS_COMMAND,
                                                (char *) reg_buffer, 1,
                                                NULL, 0);
            //for octa flash DOPI mode, WRSR just write 1 byte, but the byte number need to even because of protocol
        } else if (_read_instruction == DTROSPIF_INST_READ_OCTA_DTR) {
            status = _ospi_send_general_command(OSPIF_INST_WSR1, OSPI_NO_ADDRESS_COMMAND,
                                                (char *) reg_buffer, 2,
                                                NULL, 0);
        } else {
            status = _ospi_send_general_command(OSPIF_INST_WSR1, OSPI_NO_ADDRESS_COMMAND,
                                                (char *) reg_buffer, _num_status_registers,
                                                NULL, 0);
        }
        if (OSPI_STATUS_OK == status) {
            tr_debug("Writing Status Registers Success: reg 1 value = 0x%x, reg 2 value = 0x%x",
                     (int) reg_buffer[0], (int) reg_buffer[1]);
            if (_num_status_registers > 2) {
                tr_debug("Writing Register 3 Success: value = 0x%x", (int) reg_buffer[2]);
            }
        } else {
            tr_error("Writing Status Registers failed");
            return status;
        }
    } else {
        // Status registers are written using different commands
        MBED_ASSERT(_num_status_registers == 2); // This flow doesn't support a nonstandard third status/config register

        // Write status register 1
        if (_set_write_enable() != 0) {
            tr_error("Write Enable failed");
            return OSPI_STATUS_ERROR;
        }
        status = _ospi_send_general_command(OSPIF_INST_WSR1, OSPI_NO_ADDRESS_COMMAND,
                                            (char *) &reg_buffer[0], 1,
                                            NULL, 0);
        if (OSPI_STATUS_OK == status) {
            tr_debug("Writing Status Register 1 Success: value = 0x%x",
                     (int) reg_buffer[0]);
        } else {
            tr_error("Writing Status Register 1 failed");
            return status;
        }

        // Write status register 2
        if (_set_write_enable() != 0) {
            tr_error("Write Enable failed");
            return OSPI_STATUS_ERROR;
        }
        status = _ospi_send_general_command(_write_status_reg_2_inst, OSPI_NO_ADDRESS_COMMAND,
                                            (char *) &reg_buffer[0], 1,
                                            NULL, 0);
        if (OSPI_STATUS_OK == status) {
            tr_debug("Writing Status Register 2 Success: value = 0x%x",
                     (int) reg_buffer[1]);
        } else {
            tr_error("Writing Status Register 2 failed");
            return status;
        }
    }

    return OSPI_STATUS_OK;
}
