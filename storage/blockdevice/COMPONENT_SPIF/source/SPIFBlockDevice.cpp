/* mbed Microcontroller Library
 * Copyright (c) 2018 ARM Limited
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
#include "SPIFBlockDevice.h"
#include "rtos/ThisThread.h"
#include "mbed_critical.h"

#include <string.h>
#include <inttypes.h>

#include "mbed_trace.h"
#define TRACE_GROUP "SPIF"
using namespace std::chrono;
using namespace mbed;

/* Default SPIF Parameters */
/****************************/
#define SPIF_DEFAULT_READ_SIZE  1
#define SPIF_DEFAULT_PROG_SIZE  1
#define SPIF_DEFAULT_SE_SIZE    4096
#define SPI_MAX_STATUS_REGISTER_SIZE 2
#ifndef UINT64_MAX
#define UINT64_MAX -1
#endif
#define SPI_NO_ADDRESS_COMMAND UINT64_MAX
// Status Register Bits
#define SPIF_STATUS_BIT_WIP 0x1 //Write In Progress
#define SPIF_STATUS_BIT_WEL 0x2 // Write Enable Latch

/* Basic Parameters Table Parsing */
/**********************************/
//READ Instruction support according to BUS Configuration
#define SPIF_BASIC_PARAM_TABLE_FAST_READ_SUPPORT_BYTE 2
#define SPIF_BASIC_PARAM_TABLE_QPI_READ_SUPPORT_BYTE 16
#define SPIF_BASIC_PARAM_TABLE_222_READ_INST_BYTE 23
#define SPIF_BASIC_PARAM_TABLE_122_READ_INST_BYTE 15
#define SPIF_BASIC_PARAM_TABLE_112_READ_INST_BYTE 13
// Address Length
#define SPIF_ADDR_SIZE_3_BYTES 3
#define SPIF_ADDR_SIZE_4_BYTES 4

// Default read/legacy erase instructions
#define SPIF_INST_READ_DEFAULT          0x03
#define SPIF_INST_LEGACY_ERASE_DEFAULT  (-1)


#define IS_MEM_READY_MAX_RETRIES 10000

enum spif_default_instructions {
    SPIF_NOP = 0x00, // No operation
    SPIF_PP = 0x02, // Page Program data
    SPIF_READ = 0x03, // Read data
    SPIF_SE   = 0x20, // 4KB Sector Erase
    SPIF_SFDP = 0x5a, // Read SFDP
    SPIF_WRSR = 0x01, // Write Status/Configuration Register
    SPIF_WRDI = 0x04, // Write Disable
    SPIF_RDSR = 0x05, // Read Status Register
    SPIF_WREN = 0x06, // Write Enable
    SPIF_RSTEN = 0x66, // Reset Enable
    SPIF_RST = 0x99, // Reset
    SPIF_RDID = 0x9f, // Read Manufacturer and JDEC Device ID
    SPIF_ULBPR = 0x98, // Clears all write-protection bits in the Block-Protection register,
    SPIF_4BEN = 0xB7, // Enable 4-byte address mode
    SPIF_4BDIS = 0xE9, // Disable 4-byte address mode
};

// Mutex is used for some SPI Driver commands that must be done sequentially with no other commands in between
// e.g. (1)Set Write Enable, (2)Program, (3)Wait Memory Ready
SingletonPtr<PlatformMutex> SPIFBlockDevice::_mutex;

//***********************
// SPIF Block Device APIs
//***********************
SPIFBlockDevice::SPIFBlockDevice(PinName mosi, PinName miso, PinName sclk, PinName csel, int freq)
    :
    _spi(mosi, miso, sclk, csel, use_gpio_ssel), _prog_instruction(0), _erase_instruction(0),
    _page_size_bytes(0), _init_ref_count(0), _is_initialized(false)
{
    _address_size = SPIF_ADDR_SIZE_3_BYTES;
    // Initial SFDP read tables are read with 8 dummy cycles
    // Default Bus Setup 1_1_1 with 0 dummy and mode cycles
    _read_dummy_and_mode_cycles = 8;
    _write_dummy_and_mode_cycles = 0;
    _dummy_and_mode_cycles = _read_dummy_and_mode_cycles;

    _sfdp_info.bptbl.device_size_bytes = 0;
    _sfdp_info.bptbl.legacy_erase_instruction = SPIF_INST_LEGACY_ERASE_DEFAULT;
    _sfdp_info.smptbl.regions_min_common_erase_size = 0;
    _sfdp_info.smptbl.region_cnt = 1;
    _sfdp_info.smptbl.region_erase_types_bitfld[0] = SFDP_ERASE_BITMASK_NONE;

    // Set default read/erase instructions
    _read_instruction = SPIF_INST_READ_DEFAULT;

    if (SPIF_BD_ERROR_OK != _spi_set_frequency(freq)) {
        tr_error("SPI Set Frequency Failed");
    }
}

int SPIFBlockDevice::init()
{
    int status = SPIF_BD_ERROR_OK;

    _mutex->lock();

    if (!_is_initialized) {
        _init_ref_count = 0;
    }

    _init_ref_count++;

    if (_init_ref_count != 1) {
        goto exit_point;
    }

    // Soft Reset
    if (-1 == _reset_flash_mem()) {
        tr_error("init - Unable to initialize flash memory, tests failed");
        status = SPIF_BD_ERROR_DEVICE_ERROR;
        goto exit_point;
    } else {
        tr_debug("Initialize flash memory OK");
    }

    if (_handle_vendor_quirks() < 0) {
        tr_error("Init - Could not read vendor id");
        status = SPIF_BD_ERROR_DEVICE_ERROR;
        goto exit_point;
    }

    //Synchronize Device
    if (false == _is_mem_ready()) {
        tr_error("init - _is_mem_ready Failed");
        status = SPIF_BD_ERROR_READY_FAILED;
        goto exit_point;
    }

    /**************************** Parse SFDP headers and tables ***********************************/
    {
        _sfdp_info.bptbl.addr = 0x0;
        _sfdp_info.bptbl.size = 0;
        _sfdp_info.smptbl.addr = 0x0;
        _sfdp_info.smptbl.size = 0;

        if (sfdp_parse_headers(callback(this, &SPIFBlockDevice::_spi_send_read_sfdp_command), _sfdp_info) < 0) {
            tr_error("init - Parse SFDP Headers Failed");
            status = SPIF_BD_ERROR_PARSING_FAILED;
            goto exit_point;
        }

        if (_sfdp_parse_basic_param_table(callback(this, &SPIFBlockDevice::_spi_send_read_sfdp_command), _sfdp_info) < 0) {
            tr_error("init - Parse Basic Param Table Failed");
            status = SPIF_BD_ERROR_PARSING_FAILED;
            goto exit_point;
        }

        if (sfdp_parse_sector_map_table(callback(this, &SPIFBlockDevice::_spi_send_read_sfdp_command), _sfdp_info) < 0) {
            tr_error("init - Parse Sector Map Table Failed");
            status = SPIF_BD_ERROR_PARSING_FAILED;
            goto exit_point;
        }
    }

    // Configure  BUS Mode to 1_1_1 for all commands other than Read
    // Dummy And Mode Cycles Back default 0
    _dummy_and_mode_cycles = _write_dummy_and_mode_cycles;
    _is_initialized = true;
    tr_debug("Device size: %llu Kbytes", _sfdp_info.bptbl.device_size_bytes / 1024);

    if (_sfdp_info.bptbl.device_size_bytes > (1 << 24)) {
        tr_debug("Size is bigger than 16MB and thus address does not fit in 3 byte, switch to 4 byte address mode");
        _spi_send_general_command(SPIF_4BEN, SPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0);
        _address_size = SPIF_ADDR_SIZE_4_BYTES;
    }

exit_point:
    _mutex->unlock();

    return status;
}


int SPIFBlockDevice::deinit()
{
    spif_bd_error status = SPIF_BD_ERROR_OK;

    _mutex->lock();

    if (!_is_initialized) {
        _init_ref_count = 0;
        goto exit_point;
    }

    _init_ref_count--;

    if (_init_ref_count) {
        goto exit_point;
    }

    // Disable Device for Writing
    status = _spi_send_general_command(SPIF_WRDI, SPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0);
    if (status != SPIF_BD_ERROR_OK)  {
        tr_error("Write Disable failed");
    }
    _address_size = SPIF_ADDR_SIZE_3_BYTES;
    _is_initialized = false;

exit_point:
    _mutex->unlock();

    return status;
}

int SPIFBlockDevice::read(void *buffer, bd_addr_t addr, bd_size_t size)
{
    if (!_is_initialized) {
        return BD_ERROR_DEVICE_ERROR;
    }

    int status = SPIF_BD_ERROR_OK;
    tr_debug("Read - Inst: 0x%xh", _read_instruction);
    _mutex->lock();

    // Set Dummy Cycles for Specific Read Command Mode
    _dummy_and_mode_cycles = _read_dummy_and_mode_cycles;

    status = _spi_send_read_command(_read_instruction, static_cast<uint8_t *>(buffer), addr, size);

    // Set Dummy Cycles for all other command modes
    _dummy_and_mode_cycles = _write_dummy_and_mode_cycles;

    _mutex->unlock();
    return status;
}

int SPIFBlockDevice::program(const void *buffer, bd_addr_t addr, bd_size_t size)
{
    if (!_is_initialized) {
        return BD_ERROR_DEVICE_ERROR;
    }

    bool program_failed = false;
    int status = SPIF_BD_ERROR_OK;
    uint32_t offset = 0;
    uint32_t chunk = 0;

    tr_debug("program - Buff: 0x%" PRIx32 "h, addr: %llu, size: %llu", (uint32_t)buffer, addr, size);

    while (size > 0) {

        // Write on _page_size_bytes boundaries (Default 256 bytes a page)
        offset = addr % _page_size_bytes;
        chunk = (offset + size < _page_size_bytes) ? size : (_page_size_bytes - offset);

        _mutex->lock();

        //Send WREN
        if (_set_write_enable() != 0) {
            tr_error("Write Enabe failed");
            program_failed = true;
            status = SPIF_BD_ERROR_WREN_FAILED;
            goto exit_point;
        }

        _spi_send_program_command(_prog_instruction, buffer, addr, chunk);

        buffer = static_cast<const uint8_t *>(buffer) + chunk;
        addr += chunk;
        size -= chunk;

        if (false == _is_mem_ready()) {
            tr_error("Device not ready after write, failed");
            program_failed = true;
            status = SPIF_BD_ERROR_READY_FAILED;
            goto exit_point;
        }
        _mutex->unlock();
    }

exit_point:
    if (program_failed) {
        _mutex->unlock();
    }

    return status;
}

int SPIFBlockDevice::erase(bd_addr_t addr, bd_size_t size)
{
    if (!_is_initialized) {
        return BD_ERROR_DEVICE_ERROR;
    }

    int type = 0;
    int cur_erase_inst = _erase_instruction;
    unsigned int curr_erase_size = 0;
    bool erase_failed = false;
    int status = SPIF_BD_ERROR_OK;
    // Find region of erased address
    int region = sfdp_find_addr_region(addr, _sfdp_info);
    if (region < 0) {
        tr_error("no region found for address %llu", addr);
        return SPIF_BD_ERROR_INVALID_ERASE_PARAMS;
    }
    // Erase Types of selected region
    uint8_t bitfield = _sfdp_info.smptbl.region_erase_types_bitfld[region];

    tr_debug("erase - addr: %llu, size: %llu", addr, size);

    if ((addr + size) > _sfdp_info.bptbl.device_size_bytes) {
        tr_error("erase exceeds flash device size");
        return SPIF_BD_ERROR_INVALID_ERASE_PARAMS;
    }

    if (((addr % get_erase_size(addr)) != 0) || (((addr + size) % get_erase_size(addr + size - 1)) != 0)) {
        tr_error("invalid erase - unaligned address and size");
        return SPIF_BD_ERROR_INVALID_ERASE_PARAMS;
    }

    // For each iteration erase the largest section supported by current region
    while (size > 0) {

        // iterate to find next Largest erase type ( a. supported by region, b. smaller than size)
        // find the matching instruction and erase size chunk for that type.
        type = sfdp_iterate_next_largest_erase_type(bitfield, size, addr, region, _sfdp_info.smptbl);
        cur_erase_inst = _sfdp_info.smptbl.erase_type_inst_arr[type];
        curr_erase_size = _sfdp_info.smptbl.erase_type_size_arr[type];
        if (addr % curr_erase_size != 0 || addr + size < curr_erase_size) {
            // Should not happen if the erase table parsing
            // and alignment checks were performed correctly
            tr_error("internal error: address %llu not aligned to erase size %u (type %d)",
                     addr, curr_erase_size, type);
        }

        tr_debug("erase - addr: %llu, size:%llu, Inst: 0x%xh, erase size: %u",
                 addr, size, cur_erase_inst, curr_erase_size);
        tr_debug("erase - Region: %d, Type:%d",
                 region, type);

        _mutex->lock();

        if (_set_write_enable() != 0) {
            tr_error("SPI Erase Device not ready - failed");
            erase_failed = true;
            status = SPIF_BD_ERROR_READY_FAILED;
            goto exit_point;
        }

        _spi_send_erase_command(cur_erase_inst, addr, size);

        addr += curr_erase_size;
        size -= curr_erase_size;

        if ((size > 0) && (addr > _sfdp_info.smptbl.region_high_boundary[region])) {
            // erase crossed to next region
            region++;
            bitfield = _sfdp_info.smptbl.region_erase_types_bitfld[region];
        }

        if (false == _is_mem_ready()) {
            tr_error("SPI After Erase Device not ready - failed");
            erase_failed = true;
            status = SPIF_BD_ERROR_READY_FAILED;
            goto exit_point;
        }

        _mutex->unlock();
    }

exit_point:
    if (erase_failed) {
        _mutex->unlock();
    }

    return status;
}

bd_size_t SPIFBlockDevice::get_read_size() const
{
    // Assuming all devices support 1byte read granularity
    return SPIF_DEFAULT_READ_SIZE;
}

bd_size_t SPIFBlockDevice::get_program_size() const
{
    // Assuming all devices support 1byte program granularity
    return SPIF_DEFAULT_PROG_SIZE;
}

bd_size_t SPIFBlockDevice::get_erase_size() const
{
    // return minimal erase size supported by all regions (0 if none exists)
    return _sfdp_info.smptbl.regions_min_common_erase_size;
}

// Find minimal erase size supported by the region to which the address belongs to
bd_size_t SPIFBlockDevice::get_erase_size(bd_addr_t addr) const
{
    // Find region of current address
    int region = sfdp_find_addr_region(addr, _sfdp_info);

    unsigned int min_region_erase_size = _sfdp_info.smptbl.regions_min_common_erase_size;
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
            tr_error("no erase type was found for region addr");
        }
    }

    return (bd_size_t)min_region_erase_size;
}

bd_size_t SPIFBlockDevice::size() const
{
    if (!_is_initialized) {
        return 0;
    }

    return _sfdp_info.bptbl.device_size_bytes;
}

int SPIFBlockDevice::get_erase_value() const
{
    return 0xFF;
}

const char *SPIFBlockDevice::get_type() const
{
    return "SPIF";
}

/***************************************************/
/*********** SPI Driver API Functions **************/
/***************************************************/
spif_bd_error SPIFBlockDevice::_spi_set_frequency(int freq)
{
    _spi.frequency(freq);
    return SPIF_BD_ERROR_OK;
}

spif_bd_error SPIFBlockDevice::_spi_send_read_command(int read_inst, uint8_t *buffer, bd_addr_t addr, bd_size_t size)
{
    uint32_t dummy_bytes = _dummy_and_mode_cycles / 8;
    int dummy_byte = 0;

    _spi.select();

    // Write 1 byte Instruction
    _spi.write(read_inst);

    // Write Address (can be either 3 or 4 bytes long)
    for (int address_shift = ((_address_size - 1) * 8); address_shift >= 0; address_shift -= 8) {
        _spi.write((addr >> address_shift) & 0xFF);
    }

    // Write Dummy Cycles Bytes
    for (uint32_t i = 0; i < dummy_bytes; i++) {
        _spi.write(dummy_byte);
    }

    // Read Data
    for (bd_size_t i = 0; i < size; i++) {
        buffer[i] = _spi.write(0);
    }

    _spi.deselect();

    return SPIF_BD_ERROR_OK;
}

int SPIFBlockDevice::_spi_send_read_sfdp_command(bd_addr_t addr, void *rx_buffer, bd_size_t rx_length)
{
    // Set 1-1-1 bus mode for SFDP header parsing
    // Initial SFDP read tables are read with 8 dummy cycles
    _dummy_and_mode_cycles = 8;

    int status = _spi_send_read_command(SPIF_SFDP, (uint8_t *)rx_buffer, addr, rx_length);
    if (status < 0) {
        tr_error("_spi_send_read_sfdp_command failed");
    }

    return status;
}

spif_bd_error SPIFBlockDevice::_spi_send_program_command(int prog_inst, const void *buffer, bd_addr_t addr,
                                                         bd_size_t size)
{
    // Send Program (write) command to device driver
    uint32_t dummy_bytes = _dummy_and_mode_cycles / 8;
    int dummy_byte = 0;
    uint8_t *data = (uint8_t *)buffer;

    _spi.select();

    // Write 1 byte Instruction
    _spi.write(prog_inst);

    // Write Address (can be either 3 or 4 bytes long)
    for (int address_shift = ((_address_size - 1) * 8); address_shift >= 0; address_shift -= 8) {
        _spi.write((addr >> address_shift) & 0xFF);
    }

    // Write Dummy Cycles Bytes
    for (uint32_t i = 0; i < dummy_bytes; i++) {
        _spi.write(dummy_byte);
    }

    // Write Data
    for (bd_size_t i = 0; i < size; i++) {
        _spi.write(data[i]);
    }

    _spi.deselect();

    return SPIF_BD_ERROR_OK;
}

spif_bd_error SPIFBlockDevice::_spi_send_erase_command(int erase_inst, bd_addr_t addr, bd_size_t size)
{
    tr_debug("Erase Inst: 0x%xh, addr: %llu, size: %llu", erase_inst, addr, size);
    addr = (((int)addr) & 0xFFFFF000);
    _spi_send_general_command(erase_inst, addr, NULL, 0, NULL, 0);
    return SPIF_BD_ERROR_OK;
}

spif_bd_error SPIFBlockDevice::_spi_send_general_command(int instruction, bd_addr_t addr, char *tx_buffer,
                                                         size_t tx_length, char *rx_buffer, size_t rx_length)
{
    // Send a general command Instruction to driver
    uint32_t dummy_bytes = _dummy_and_mode_cycles / 8;
    uint8_t dummy_byte = 0x00;

    _spi.select();

    // Write 1 byte Instruction
    _spi.write(instruction);

    // Reading SPI Bus registers does not require Flash Address
    if (addr != SPI_NO_ADDRESS_COMMAND) {
        // Write Address (can be either 3 or 4 bytes long)
        for (int address_shift = ((_address_size - 1) * 8); address_shift >= 0; address_shift -= 8) {
            _spi.write((addr >> address_shift) & 0xFF);
        }

        // Write Dummy Cycles Bytes
        for (uint32_t i = 0; i < dummy_bytes; i++) {
            _spi.write(dummy_byte);
        }
    }

    // Read/Write Data
    _spi.write(tx_buffer, (int)tx_length, rx_buffer, (int)rx_length);

    _spi.deselect();

    return SPIF_BD_ERROR_OK;
}

/*********************************************************/
/********** SFDP Parsing and Detection Functions *********/
/*********************************************************/
int SPIFBlockDevice::_sfdp_parse_basic_param_table(Callback<int(bd_addr_t, void *, bd_size_t)> sfdp_reader,
                                                   mbed::sfdp_hdr_info &sfdp_info)
{
    uint8_t param_table[SFDP_BASIC_PARAMS_TBL_SIZE]; /* Up To 20 DWORDS = 80 Bytes */

    int status = sfdp_reader(sfdp_info.bptbl.addr, param_table, sfdp_info.bptbl.size);
    if (status != SPIF_BD_ERROR_OK) {
        tr_error("init - Read SFDP First Table Failed");
        return -1;
    }

    // Check address size, currently only supports 3byte addresses
    if (sfdp_detect_addressability(param_table, _sfdp_info.bptbl) < 0) {
        tr_error("Verify 3byte addressing failed");
        return -1;
    }

    if (sfdp_detect_device_density(param_table, _sfdp_info.bptbl) < 0) {
        tr_error("Detecting device density failed");
        return -1;
    }

    // Set Default read/program/erase Instructions
    _read_instruction = SPIF_READ;
    _prog_instruction = SPIF_PP;
    _erase_instruction = SPIF_SE;

    // Set Page Size (SPI write must be done on Page limits)
    _page_size_bytes = sfdp_detect_page_size(param_table, sfdp_info.bptbl.size);

    // Detect and Set Erase Types
    if (sfdp_detect_erase_types_inst_and_size(param_table, sfdp_info) < 0) {
        tr_error("Init - Detecting erase types instructions/sizes failed");
        return -1;
    }

    _erase_instruction = sfdp_info.bptbl.legacy_erase_instruction;

    // Detect and Set fastest Bus mode (default 1-1-1)
    _sfdp_detect_best_bus_read_mode(param_table, sfdp_info.bptbl.size, _read_instruction);

    return 0;
}

int SPIFBlockDevice::_sfdp_detect_best_bus_read_mode(uint8_t *basic_param_table_ptr, int basic_param_table_size,
                                                     int &read_inst)
{
    do {

        // TBD - SPIF Dual Read Modes Require SPI driver support
        _read_dummy_and_mode_cycles = 0;
        tr_debug("Read Bus Mode set to 1-1-1, Instruction: 0x%xh", read_inst);
    } while (false);

    return 0;
}

int SPIFBlockDevice::_reset_flash_mem()
{
    // Perform Soft Reset of the Device prior to initialization
    int status = 0;
    char status_value[2] = {0};
    tr_info("_reset_flash_mem:");
    //Read the Status Register from device
    if (SPIF_BD_ERROR_OK == _spi_send_general_command(SPIF_RDSR, SPI_NO_ADDRESS_COMMAND, NULL, 0, status_value, 1)) {
        // store received values in status_value
        tr_debug("Reading Status Register Success: value = 0x%x", (int)status_value[0]);
    } else {
        tr_error("Reading Status Register failed");
        status = -1;
    }

    if (0 == status) {
        //Send Reset Enable
        if (SPIF_BD_ERROR_OK == _spi_send_general_command(SPIF_RSTEN, SPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0)) {
            // store received values in status_value
            tr_debug("Sending RSTEN Success");
        } else {
            tr_error("Sending RSTEN failed");
            status = -1;
        }

        if (0 == status) {
            //Send Reset
            if (SPIF_BD_ERROR_OK == _spi_send_general_command(SPIF_RST, SPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0)) {
                // store received values in status_value
                tr_debug("Sending RST Success");
            } else {
                tr_error("Sending RST failed");
                status = -1;
            }
            if (false == _is_mem_ready()) {
                tr_error("Device not ready, write failed");
                status = -1;
            }
        }
    }

    return status;
}

bool SPIFBlockDevice::_is_mem_ready()
{
    // Check Status Register Busy Bit to Verify the Device isn't Busy
    char status_value[2];
    int retries = 0;
    bool mem_ready = true;

    do {
        rtos::ThisThread::sleep_for(1ms);
        retries++;
        //Read the Status Register from device
        if (SPIF_BD_ERROR_OK != _spi_send_general_command(SPIF_RDSR, SPI_NO_ADDRESS_COMMAND, NULL, 0, status_value,
                                                          1)) {   // store received values in status_value
            tr_error("Reading Status Register failed");
        }
    } while ((status_value[0] & SPIF_STATUS_BIT_WIP) != 0 && retries < IS_MEM_READY_MAX_RETRIES);

    if ((status_value[0] & SPIF_STATUS_BIT_WIP) != 0) {
        tr_error("_is_mem_ready FALSE");
        mem_ready = false;
    }
    return mem_ready;
}

int SPIFBlockDevice::_set_write_enable()
{
    // Check Status Register Busy Bit to Verify the Device isn't Busy
    char status_value[2];
    int status = -1;

    do {
        if (SPIF_BD_ERROR_OK !=  _spi_send_general_command(SPIF_WREN, SPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0)) {
            tr_error("Sending WREN command FAILED");
            break;
        }

        if (false == _is_mem_ready()) {
            tr_error("Device not ready, write failed");
            break;
        }

        memset(status_value, 0, 2);
        if (SPIF_BD_ERROR_OK != _spi_send_general_command(SPIF_RDSR, SPI_NO_ADDRESS_COMMAND, NULL, 0, status_value,
                                                          1)) {   // store received values in status_value
            tr_error("Reading Status Register failed");
            break;
        }

        if ((status_value[0] & SPIF_STATUS_BIT_WEL) == 0) {
            tr_error("_set_write_enable failed");
            break;
        }
        status = 0;
    } while (false);
    return status;
}

int SPIFBlockDevice::_handle_vendor_quirks()
{
    uint8_t vendor_device_ids[4];
    size_t data_length = 3;

    /* Read Manufacturer ID (1byte), and Device ID (2bytes)*/
    spif_bd_error spi_status = _spi_send_general_command(SPIF_RDID, SPI_NO_ADDRESS_COMMAND, NULL, 0,
                                                         (char *)vendor_device_ids,
                                                         data_length);

    if (spi_status != SPIF_BD_ERROR_OK) {
        tr_error("Read Vendor ID Failed");
        return -1;
    }

    tr_debug("Vendor device ID = 0x%x 0x%x 0x%x", vendor_device_ids[0], vendor_device_ids[1], vendor_device_ids[2]);

    switch (vendor_device_ids[0]) {
        case 0xbf:
            // SST devices come preset with block protection
            // enabled for some regions, issue global protection unlock to clear
            _set_write_enable();
            _spi_send_general_command(SPIF_ULBPR, SPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0);
            break;
    }

    return 0;
}
