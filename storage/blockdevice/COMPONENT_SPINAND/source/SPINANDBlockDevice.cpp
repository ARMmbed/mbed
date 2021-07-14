/* mbed Microcontroller Library
 * Copyright (c) 2021 ARM Limited
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

#include "platform/Callback.h"
#include "SPINANDBlockDevice.h"
#include <string.h>
#include "rtos/ThisThread.h"

#ifndef MBED_CONF_MBED_TRACE_ENABLE
#define MBED_CONF_MBED_TRACE_ENABLE        0
#endif

#include "mbed_trace.h"
#define TRACE_GROUP "SPINAND"

using namespace std::chrono;
using namespace mbed;

/* SPINAND Parameters */
/****************************/
#ifndef UINT64_MAX
#define UINT64_MAX -1
#endif
#define QSPI_NO_ADDRESS_COMMAND UINT64_MAX
#define QSPI_ALT_DEFAULT_VALUE  0

// Get/Set Feature Address Definition
#define FEATURES_ADDR_BLOCK_PROTECTION  0xA0
#define FEATURES_ADDR_SECURE_OTP        0xB0
#define FEATURES_ADDR_STATUS            0xC0

// Status Register Bits
#define SPINAND_STATUS_BIT_WIP             0x1  // Write In Progress
#define SPINAND_STATUS_BIT_WEL             0x2  // Write Enable Latch
#define SPINAND_STATUS_BIT_ERASE_FAIL      0x4  // Erase failed 
#define SPINAND_STATUS_BIT_PROGRAM_FAIL    0x8  // Program failed
#define SPINAND_STATUS_BIT_ECC_STATUS_MASK 0x30 // ECC status
#define SPINAND_STATUS_ECC_STATUS_NO_ERR     0x00
#define SPINAND_STATUS_ECC_STATUS_ERR_COR    0x00
#define SPINAND_STATUS_ECC_STATUS_ERR_NO_COR 0x00

// Secure OTP Register Bits
#define SPINAND_SECURE_BIT_QE          0x01  // Quad enable
#define SPINAND_SECURE_BIT_ECC_EN      0x10  // On-die ECC enable
#define SPINAND_SECURE_BIT_OTP_EN      0x40  // 
#define SPINAND_SECURE_BIT_OTP_PROT    0x80  // 

// Block Protection Register Bits
#define  SPINAND_BLOCK_PROT_BIT_SP      0x01
#define  SPINAND_BLOCK_PROT_BIT_COMPLE  0x02
#define  SPINAND_BLOCK_PROT_BIT_INVERT  0x04
#define  SPINAND_BLOCK_PROT_BIT_BP0     0x08
#define  SPINAND_BLOCK_PROT_BIT_BP1     0x10
#define  SPINAND_BLOCK_PROT_BIT_BP2     0x20
#define  SPINAND_BLOCK_PROT_BIT_BPRWD   0x80
#define  SPINAND_BLOCK_PROT_BIT_BP_MASK 0x38

#define  SPINAND_BLOCK_PROT_BP_OFFSET     3
#define  SPINAND_BLOCK_PROT_COMPLE_OFFSET 1

#define IS_MEM_READY_MAX_RETRIES 10000

// General SPI NAND Flash instructions
#define SPINAND_INST_RDID            0x9F // Read Manufacturer and JDEC Device ID
#define SPINAND_INST_RSR1            0x05 // Read status register 1

#define SPINAND_INST_PAGE_READ       0x13 // Read data from array to cache
#define SPINAND_INST_READ_CACHE      0x03 // Read data from cache
#define SPINAND_INST_READ_CACHE2     0x3B
#define SPINAND_INST_READ_CACHE4     0x6B
#define SPINAND_INST_READ_CACHE_SEQ  0x31
#define SPINAND_INST_READ_CACHE_END  0x3F

#define SPINAND_INST_WREN            0x06 // Write enable
#define SPINAND_INST_WRDI            0x04 // Write disable
#define SPINAND_INST_PP_LOAD         0x02
#define SPINAND_INST_PP_RAND_LOAD    0x84
#define SPINAND_INST_4PP_LOAD        0x32
#define SPINAND_INST_4PP_RAND_LOAD   0x34
#define SPINAND_INST_PROGRAM_EXEC    0x10
#define SPINAND_INST_BE              0xD8

#define SPINAND_INST_GET_FEATURE     0x0F
#define SPINAND_INST_SET_FEATURE     0x1F
#define SPINAND_INST_RESET           0xFF
#define SPINAND_INST_ECC_STAT_READ   0x7C

// Default read/legacy erase instructions
//#define SPINAND_INST_READ_DEFAULT          SPINAND_INST_READ_CACHE
//#define SPINAND_INST_READ_DEFAULT          SPINAND_INST_READ_CACHE2
#define SPINAND_INST_READ_DEFAULT          SPINAND_INST_READ_CACHE4
//#define SPINAND_INST_PROGRAM_DEFAULT       SPINAND_INST_PP_LOAD
#define SPINAND_INST_PROGRAM_DEFAULT       SPINAND_INST_4PP_LOAD

#define SPINAND_BLOCK_OFFSET  0x40000
#define SPINAND_PAGE_OFFSET   0x1000

#define SPI_NAND_ROW_ADDR_SIZE    QSPI_CFG_ADDR_SIZE_16
#define SPI_NAND_COLUMN_ADDR_SIZE QSPI_CFG_ADDR_SIZE_24

/* Init function to initialize Different Devices CS static list */
static PinName *generate_initialized_active_spinand_csel_arr();
// Static Members for different devices csel
// _devices_mutex is used to lock csel list - only one SPINANDBlockDevice instance per csel is allowed
SingletonPtr<PlatformMutex> SPINANDBlockDevice::_devices_mutex;
int SPINANDBlockDevice::_number_of_active_spinand_flash_csel = 0;
PinName *SPINANDBlockDevice::_active_spinand_flash_csel_arr = generate_initialized_active_spinand_csel_arr();

/********* Public API Functions *********/
/****************************************/
SPINANDBlockDevice::SPINANDBlockDevice(PinName io0, PinName io1, PinName io2, PinName io3, PinName sclk, PinName csel,
                                       int clock_mode,
                                       int freq)
    :
    _qspi(io0, io1, io2, io3, sclk, csel, clock_mode), _csel(csel), _freq(freq),
    _init_ref_count(0),
    _is_initialized(false)
{
    _unique_device_status = add_new_csel_instance(csel);

    if (_unique_device_status == 0) {
        tr_debug("Adding a new SPINANDBlockDevice csel: %d", (int)csel);
    } else if (_unique_device_status == -1) {
        tr_error("SPINANDBlockDevice with the same csel(%d) already exists", (int)csel);
    } else {
        tr_error("Too many different SPINANDBlockDevice devices - max allowed: %d", SPINAND_MAX_ACTIVE_FLASH_DEVICES);
    }

    // Default Bus Setup 1_1_1 with 0 dummy and mode cycles
    _inst_width = QSPI_CFG_BUS_SINGLE;
    _address_width = QSPI_CFG_BUS_SINGLE;
    _address_size =  QSPI_CFG_ADDR_SIZE_8;
    _alt_size = 0;
    _dummy_cycles = 8;
    _data_width = QSPI_CFG_BUS_SINGLE;

    // Set default read/erase instructions
    _read_instruction = SPINAND_INST_READ_DEFAULT;
    _program_instruction = SPINAND_INST_PROGRAM_DEFAULT;
}

int SPINANDBlockDevice::init()
{
    int status = SPINAND_BD_ERROR_OK;

    if (_unique_device_status == 0) {
        tr_debug("SPINANDBlockDevice csel: %d", (int)_csel);
    } else if (_unique_device_status == -1) {
        tr_error("SPINANDBlockDevice with the same csel(%d) already exists", (int)_csel);
        return SPINAND_BD_ERROR_DEVICE_NOT_UNIQUE;
    } else {
        tr_error("Too many different SPINANDBlockDevice devices - max allowed: %d", SPINAND_MAX_ACTIVE_FLASH_DEVICES);
        return SPINAND_BD_ERROR_DEVICE_MAX_EXCEED;
    }

    _mutex.lock();

    // All commands other than Read and RSFDP use default 1-1-1 bus mode (Program/Erase are constrained by flash memory performance more than bus performance)
    if (QSPI_STATUS_OK != _qspi.configure_format(QSPI_CFG_BUS_SINGLE, QSPI_CFG_BUS_SINGLE, _address_size, QSPI_CFG_BUS_SINGLE,
                                                 0, QSPI_CFG_BUS_SINGLE, 0)) {
        tr_error("_qspi_configure_format failed");
        status = SPINAND_BD_ERROR_DEVICE_ERROR;
        goto exit_point;
    }

    if (!_is_initialized) {
        _init_ref_count = 0;
    }

    _init_ref_count++;

    if (_init_ref_count != 1) {
        goto exit_point;
    }

    _alt_size = 0;
    _dummy_cycles = 8;
    if (QSPI_STATUS_OK != _qspi_set_frequency(_freq)) {
        tr_error("QSPI Set Frequency Failed");
        status = SPINAND_BD_ERROR_DEVICE_ERROR;
        goto exit_point;
    }

    // Synchronize Device
    if (false == _is_mem_ready()) {
        tr_error("Init - _is_mem_ready Failed");
        status = SPINAND_BD_ERROR_READY_FAILED;
        goto exit_point;
    }

    if (0 != _clear_block_protection()) {
        tr_error("Init - clearing block protection failed");
        status = SPINAND_BD_ERROR_PARSING_FAILED;
        goto exit_point;
    }

    if (_read_instruction == SPINAND_INST_READ_CACHE4) {
        if (QSPI_STATUS_OK != _set_quad_enable()) {
            tr_error("SPI NAND Set Quad enable Failed");
            status = SPINAND_BD_ERROR_DEVICE_ERROR;
            goto exit_point;
        }
    }

    _is_initialized = true;

exit_point:
    _mutex.unlock();

    return status;
}

int SPINANDBlockDevice::deinit()
{
    int result = SPINAND_BD_ERROR_OK;

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

    // Disable Device for Writing
    qspi_status_t status = _qspi_send_general_command(SPINAND_INST_WRDI, QSPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0);
    if (status != QSPI_STATUS_OK)  {
        tr_error("Write Disable failed");
        result = SPINAND_BD_ERROR_DEVICE_ERROR;
    }

    _is_initialized = false;

    _mutex.unlock();

    if (_unique_device_status == 0) {
        remove_csel_instance(_csel);
    }

    return result;
}

int SPINANDBlockDevice::read(void *buffer, bd_addr_t addr, bd_size_t size)
{
    int status = SPINAND_BD_ERROR_OK;
    uint32_t offset = 0;
    uint32_t chunk = 0;
    bd_size_t read_bytes = 0;

    tr_debug("Read Inst: 0x%xh", _read_instruction);

    while (size > 0) {
        // Read on _page_size_bytes boundaries (Default 2048 bytes a page)
        offset = addr % MBED_CONF_SPINAND_SPINAND_PAGE_SIZE;
        chunk = (offset + size < MBED_CONF_SPINAND_SPINAND_PAGE_SIZE) ? size : (MBED_CONF_SPINAND_SPINAND_PAGE_SIZE - offset);
        read_bytes = chunk;

        _mutex.lock();

        if (QSPI_STATUS_OK != _qspi_send_read_command(_read_instruction, buffer, addr, read_bytes)) {
            tr_error("Read Command failed");
            status = SPINAND_BD_ERROR_DEVICE_ERROR;
        }

        buffer = static_cast< uint8_t *>(buffer) + chunk;
        addr += SPINAND_PAGE_OFFSET;
        size -= chunk;

        _mutex.unlock();
    }

    return status;
}

int SPINANDBlockDevice::program(const void *buffer, bd_addr_t addr, bd_size_t size)
{
    qspi_status_t result = QSPI_STATUS_OK;
    bool program_failed = false;
    int status = SPINAND_BD_ERROR_OK;
    uint32_t offset = 0;
    uint32_t chunk = 0;
    bd_size_t written_bytes = 0;

    tr_debug("Program - Buff: %p, addr: %llu, size: %llu", buffer, addr, size);

    while (size > 0) {
        // Write on _page_size_bytes boundaries (Default 2048 bytes a page)
        offset = addr % MBED_CONF_SPINAND_SPINAND_PAGE_SIZE;
        chunk = (offset + size < MBED_CONF_SPINAND_SPINAND_PAGE_SIZE) ? size : (MBED_CONF_SPINAND_SPINAND_PAGE_SIZE - offset);
        written_bytes = chunk;

        _mutex.lock();

        //Send WREN
        if (_set_write_enable() != 0) {
            tr_error("Write Enable failed");
            program_failed = true;
            status = SPINAND_BD_ERROR_WREN_FAILED;
            goto exit_point;
        }

        result = _qspi_send_program_command(_program_instruction, buffer, addr, &written_bytes);
        if ((result != QSPI_STATUS_OK) || (chunk != written_bytes)) {
            tr_error("Write failed");
            program_failed = true;
            status = SPINAND_BD_ERROR_DEVICE_ERROR;
            goto exit_point;
        }

        buffer = static_cast<const uint8_t *>(buffer) + chunk;
        addr += SPINAND_PAGE_OFFSET;
        size -= chunk;

        if (false == _is_mem_ready()) {
            tr_error("Device not ready after write, failed");
            program_failed = true;
            status = SPINAND_BD_ERROR_READY_FAILED;
            goto exit_point;
        }
        _mutex.unlock();
    }

exit_point:
    if (program_failed) {
        _mutex.unlock();
    }

    return status;
}

int SPINANDBlockDevice::erase(bd_addr_t addr, bd_size_t size)
{
    bool erase_failed = false;
    int status = SPINAND_BD_ERROR_OK;

    tr_debug("Erase - addr: %llu, size: %llu", addr, size);

    if ((addr + size) > MBED_CONF_SPINAND_SPINAND_FLASH_SIZE) {
        tr_error("Erase exceeds flash device size");
        return SPINAND_BD_ERROR_INVALID_ERASE_PARAMS;
    }

    if (((addr % SPINAND_BLOCK_OFFSET) != 0) || ((size % get_erase_size()) != 0)) {
        tr_error("Invalid erase - unaligned address and size");
        return SPINAND_BD_ERROR_INVALID_ERASE_PARAMS;
    }

    while (size > 0) {

        _mutex.lock();

        if (_set_write_enable() != 0) {
            tr_error("SPI NAND Erase Device not ready - failed");
            erase_failed = true;
            status = SPINAND_BD_ERROR_WREN_FAILED;
            goto exit_point;
        }

        if (QSPI_STATUS_OK != _qspi_send_erase_command(SPINAND_INST_BE, addr, size)) {
            tr_error("SPI NAND Erase command failed!");
            erase_failed = true;
            status = SPINAND_BD_ERROR_DEVICE_ERROR;
            goto exit_point;
        }

        addr += SPINAND_BLOCK_OFFSET;
        if (size > MBED_CONF_SPINAND_SPINAND_BLOCK_SIZE) {
            size -= MBED_CONF_SPINAND_SPINAND_BLOCK_SIZE;
        } else {
            size = 0;
        }

        if (false == _is_mem_ready()) {
            tr_error("SPI NAND After Erase Device not ready - failed");
            erase_failed = true;
            status = SPINAND_BD_ERROR_READY_FAILED;
            goto exit_point;
        }

        _mutex.unlock();
    }

exit_point:
    if (erase_failed) {
        _mutex.unlock();
    }

    return status;
}

bd_size_t SPINANDBlockDevice::get_read_size() const
{
    // Return minimum read size in bytes for the device
    return MBED_CONF_SPINAND_SPINAND_MIN_READ_SIZE;
}

bd_size_t SPINANDBlockDevice::get_program_size() const
{
    // Return minimum program/write size in bytes for the device
    return MBED_CONF_SPINAND_SPINAND_MIN_PROG_SIZE;
}

bd_size_t SPINANDBlockDevice::get_erase_size() const
{
    return MBED_CONF_SPINAND_SPINAND_BLOCK_SIZE;
}

bd_size_t SPINANDBlockDevice::get_erase_size(bd_addr_t addr) const
{
    return MBED_CONF_SPINAND_SPINAND_BLOCK_SIZE;
}

const char *SPINANDBlockDevice::get_type() const
{
    return "SPINAND";
}

bd_size_t SPINANDBlockDevice::size() const
{
    return MBED_CONF_SPINAND_SPINAND_FLASH_SIZE;
}

int SPINANDBlockDevice::get_erase_value() const
{
    return 0xFF;
}

/********************************/
/*   Different Device Csel Mgmt */
/********************************/
static PinName *generate_initialized_active_spinand_csel_arr()
{
    PinName *init_arr = new PinName[SPINAND_MAX_ACTIVE_FLASH_DEVICES];
    for (int i_ind = 0; i_ind < SPINAND_MAX_ACTIVE_FLASH_DEVICES; i_ind++) {
        init_arr[i_ind] = NC;
    }
    return init_arr;
}

int SPINANDBlockDevice::add_new_csel_instance(PinName csel)
{
    int status = 0;
    _devices_mutex->lock();
    if (_number_of_active_spinand_flash_csel >= SPINAND_MAX_ACTIVE_FLASH_DEVICES) {
        status = -2;
        goto exit_point;
    }

    // verify the device is unique(no identical csel already exists)
    for (int i_ind = 0; i_ind < SPINAND_MAX_ACTIVE_FLASH_DEVICES; i_ind++) {
        if (_active_spinand_flash_csel_arr[i_ind] == csel) {
            status = -1;
            goto exit_point;
        }
    }

    // Insert new csel into existing device list
    for (int i_ind = 0; i_ind < SPINAND_MAX_ACTIVE_FLASH_DEVICES; i_ind++) {
        if (_active_spinand_flash_csel_arr[i_ind] == NC) {
            _active_spinand_flash_csel_arr[i_ind] = csel;
            break;
        }
    }
    _number_of_active_spinand_flash_csel++;

exit_point:
    _devices_mutex->unlock();
    return status;
}

int SPINANDBlockDevice::remove_csel_instance(PinName csel)
{
    int status = -1;
    _devices_mutex->lock();
    // remove the csel from existing device list
    for (int i_ind = 0; i_ind < SPINAND_MAX_ACTIVE_FLASH_DEVICES; i_ind++) {
        if (_active_spinand_flash_csel_arr[i_ind] == csel) {
            _active_spinand_flash_csel_arr[i_ind] = NC;
            if (_number_of_active_spinand_flash_csel > 0) {
                _number_of_active_spinand_flash_csel--;
            }
            status = 0;
            break;
        }
    }
    _devices_mutex->unlock();
    return status;
}

int SPINANDBlockDevice::_set_quad_enable()
{
    uint8_t secur_reg = 0;

    if (false == _is_mem_ready()) {
        tr_error("Device not ready, set quad enable failed");
        return -1;
    }

    if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_GET_FEATURE, FEATURES_ADDR_SECURE_OTP,
                                                     NULL, 0, (char *) &secur_reg, 1)) {
        tr_error("Reading Security Register failed");
    }

    secur_reg |= SPINAND_SECURE_BIT_QE;

    if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_SET_FEATURE, FEATURES_ADDR_SECURE_OTP,
                                                     (char *) &secur_reg, 1, NULL, 0)) {
        tr_error("Writing Security Register failed");
    }
    if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_GET_FEATURE, FEATURES_ADDR_SECURE_OTP,
                                                     NULL, 0, (char *) &secur_reg, 1)) {
        tr_error("Reading Security Register failed");
    }
    if (false == _is_mem_ready()) {
        tr_error("Device not ready, set quad enable failed");
        return -1;
    }

    return 0;
}

int SPINANDBlockDevice::_clear_block_protection()
{
    uint8_t block_protection_reg = 0;

    if (false == _is_mem_ready()) {
        tr_error("Device not ready, clearing block protection failed");
        return -1;
    }

    if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_GET_FEATURE, FEATURES_ADDR_BLOCK_PROTECTION,
                                                     NULL, 0, (char *) &block_protection_reg, 1)) {
        tr_error("Reading Block Protection Register failed");
    }

    block_protection_reg &= ~SPINAND_BLOCK_PROT_BIT_BP_MASK;

    if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_SET_FEATURE, FEATURES_ADDR_BLOCK_PROTECTION,
                                                     (char *) &block_protection_reg, 1, NULL, 0)) {
        tr_error("Writing Block Protection Register failed");
    }
    if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_GET_FEATURE, FEATURES_ADDR_BLOCK_PROTECTION,
                                                     NULL, 0, (char *) &block_protection_reg, 1)) {
        tr_error("Reading Block Protection Register failed");
    }
    if (false == _is_mem_ready()) {
        tr_error("Device not ready, clearing block protection failed");
        return -1;
    }

    return 0;
}

int SPINANDBlockDevice::_set_write_enable()
{
    // Check Status Register Busy Bit to Verify the Device isn't Busy
    uint8_t status_value = 0;
    int status = -1;

    do {
        if (QSPI_STATUS_OK !=  _qspi_send_general_command(SPINAND_INST_WREN, QSPI_NO_ADDRESS_COMMAND, NULL, 0, NULL, 0)) {
            tr_error("Sending WREN command FAILED");
            break;
        }

        if (false == _is_mem_ready()) {
            tr_error("Device not ready, write failed");
            break;
        }

        if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_GET_FEATURE, FEATURES_ADDR_STATUS,
                                                         NULL, 0,
                                                         (char *) &status_value, 1)) { // store received value in status_value
            tr_error("Reading Status Register failed");
        }

        if ((status_value & SPINAND_STATUS_BIT_WEL) == 0) {
            tr_error("_set_write_enable failed - status register 1 value: %u", status_value);
            break;
        }

        status = 0;
    } while (false);

    return status;
}

bool SPINANDBlockDevice::_is_mem_ready()
{
    // Check Status Register Busy Bit to Verify the Device isn't Busy
    uint8_t status_value = 0;
    int retries = 0;
    bool mem_ready = true;

    do {
        rtos::ThisThread::sleep_for(1ms);
        retries++;
        //Read Status Register 1 from device
        if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_GET_FEATURE, FEATURES_ADDR_STATUS,
                                                         NULL, 0,
                                                         (char *) &status_value, 1)) { // store received value in status_value
            tr_error("Reading Status Register failed");
        }
    } while ((status_value & SPINAND_STATUS_BIT_WIP) != 0 && retries < IS_MEM_READY_MAX_RETRIES);

    if ((status_value & SPINAND_STATUS_BIT_WIP) != 0) {
        tr_error("_is_mem_ready FALSE: status value = 0x%x ", status_value);
        mem_ready = false;
    }
    return mem_ready;
}

/***************************************************/
/*********** QSPI Driver API Functions *************/
/***************************************************/
qspi_status_t SPINANDBlockDevice::_qspi_set_frequency(int freq)
{
    return _qspi.set_frequency(freq);
}

qspi_status_t SPINANDBlockDevice::_qspi_send_read_command(qspi_inst_t read_inst, void *buffer,
                                                          bd_addr_t addr, bd_size_t size)
{
    tr_debug("Inst: 0x%xh, addr: %llu, size: %llu", read_inst, addr, size);

    size_t buf_len = size;

    qspi_bus_width_t data_width;
    if (read_inst == SPINAND_INST_READ_CACHE) {
        data_width = QSPI_CFG_BUS_SINGLE;
    } else if (read_inst == SPINAND_INST_READ_CACHE2) {
        data_width = QSPI_CFG_BUS_DUAL;
    } else if (read_inst == SPINAND_INST_READ_CACHE4) {
        data_width = QSPI_CFG_BUS_QUAD;
    }

    // Send read command to device driver
    // Read commands use the best bus mode supported by the part
    qspi_status_t status = _qspi.configure_format(_inst_width, _address_width, SPI_NAND_COLUMN_ADDR_SIZE, // Alt width should be the same as address width
                                                  _address_width, _alt_size, _data_width, 0);
    if (QSPI_STATUS_OK != status) {
        tr_error("_qspi_configure_format failed");
        return status;
    }

    if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_PAGE_READ, addr >> 12, NULL, 0, NULL, 0)) {
        tr_error("Read page from array failed");
    }

    status = _qspi.configure_format(_inst_width, _address_width, _address_size, _address_width, // Alt width should be the same as address width
                                    _alt_size, _data_width, 0);
    if (QSPI_STATUS_OK != status) {
        tr_error("_qspi_configure_format failed");
        return status;
    }

    if (false == _is_mem_ready()) {
        tr_error("Device not ready, clearing block protection failed");
        return QSPI_STATUS_ERROR;
    }

    status = _qspi.configure_format(_inst_width, _address_width, SPI_NAND_ROW_ADDR_SIZE, _address_width, // Alt width should be the same as address width
                                    _alt_size, data_width, _dummy_cycles);
    if (QSPI_STATUS_OK != status) {
        tr_error("_qspi_configure_format failed");
        return status;
    }

    // Don't check the read status until after we've configured the format back to 1-1-1, to avoid leaving the interface in an
    // incorrect state if the read fails.
    status = _qspi.read(read_inst, (_alt_size == 0) ? -1 : QSPI_ALT_DEFAULT_VALUE, (unsigned int)addr, (char *)buffer, &buf_len);

    // All commands other than Read use default 1-1-1 bus mode (Program/Erase are constrained by flash memory performance more than bus performance)
    qspi_status_t format_status = _qspi.configure_format(QSPI_CFG_BUS_SINGLE, QSPI_CFG_BUS_SINGLE, _address_size, QSPI_CFG_BUS_SINGLE, 0, QSPI_CFG_BUS_SINGLE, 0);
    if (QSPI_STATUS_OK != format_status) {
        tr_error("_qspi_configure_format failed");
        return format_status;
    }

    if (QSPI_STATUS_OK != status) {
        tr_error("QSPI Read failed");
        return status;
    }

    return QSPI_STATUS_OK;
}

qspi_status_t SPINANDBlockDevice::_qspi_send_program_command(qspi_inst_t prog_inst, const void *buffer,
                                                             bd_addr_t addr, bd_size_t *size)
{
    tr_debug("Inst: 0x%xh, addr: %llu, size: %llu", prog_inst, addr, *size);

    qspi_bus_width_t data_width;

    if (prog_inst == SPINAND_INST_PP_LOAD) {
        data_width = QSPI_CFG_BUS_SINGLE;
    } else if (prog_inst == SPINAND_INST_4PP_LOAD) {
        data_width = QSPI_CFG_BUS_QUAD;
    }

    // Program load commands need 16 bit row address
    qspi_status_t status = _qspi.configure_format(_inst_width, _address_width, SPI_NAND_ROW_ADDR_SIZE, // Alt width should be the same as address width
                                                  _address_width, _alt_size, data_width, 0);
    if (QSPI_STATUS_OK != status) {
        tr_error("_qspi_configure_format failed");
        return status;
    }

    // Send program (write) command to device driver
    status = _qspi.write(prog_inst, -1, addr, (char *)buffer, (size_t *)size);
    if (QSPI_STATUS_OK != status) {
        tr_error("QSPI Write failed");
        return status;
    }

    // Program execute command need 24 bit column address
    qspi_status_t format_status = _qspi.configure_format(QSPI_CFG_BUS_SINGLE, QSPI_CFG_BUS_SINGLE, SPI_NAND_COLUMN_ADDR_SIZE, QSPI_CFG_BUS_SINGLE,
                                                         0, QSPI_CFG_BUS_SINGLE, 0);
    if (QSPI_STATUS_OK != format_status) {
        tr_error("_qspi_configure_format failed");
        return format_status;
    }

    // Program execute command
    if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_PROGRAM_EXEC, addr >> 12, NULL, 0, NULL, 0)) {
        tr_error("Read page from array failed");
    }

    status = _qspi.configure_format(_inst_width, _address_width, _address_size, _address_width, // Alt width should be the same as address width
                                    _alt_size, _data_width, 0);
    if (QSPI_STATUS_OK != status) {
        tr_error("_qspi_configure_format failed");
        return status;
    }
    uint8_t status_value = 0;
    if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_GET_FEATURE, FEATURES_ADDR_STATUS,
                                                     NULL, 0,
                                                     (char *) &status_value, 1)) { // store received value in status_value
        tr_error("Reading Status Register failed");
    }
    return QSPI_STATUS_OK;
}

qspi_status_t SPINANDBlockDevice::_qspi_send_erase_command(qspi_inst_t erase_inst, bd_addr_t addr, bd_size_t size)
{
    tr_debug("Inst: 0x%xh, addr: %llu, size: %llu", erase_inst, addr, size);

    qspi_status_t status = _qspi.configure_format(_inst_width, _address_width, SPI_NAND_COLUMN_ADDR_SIZE,// Alt width should be the same as address width
                                                  _address_width,  _alt_size, _data_width, 0);
    if (QSPI_STATUS_OK != status) {
        tr_error("_qspi_configure_format failed");
        return status;
    }

    // Send erase command to driver
    status = _qspi.command_transfer(erase_inst, (int)(addr >> 12), NULL, 0, NULL, 0);

    if (QSPI_STATUS_OK != status) {
        tr_error("QSPI Erase failed");
        return status;
    }

    status = _qspi.configure_format(_inst_width, _address_width, _address_size, _address_width, // Alt width should be the same as address width
                                    _alt_size, _data_width, 0);
    if (QSPI_STATUS_OK != status) {
        tr_error("_qspi_configure_format failed");
        return status;
    }
    uint8_t status_value = 0;
    if (QSPI_STATUS_OK != _qspi_send_general_command(SPINAND_INST_GET_FEATURE, FEATURES_ADDR_STATUS,
                                                     NULL, 0,
                                                     (char *) &status_value, 1)) { // store received value in status_value
        tr_error("Reading Status Register failed");
    }
    return QSPI_STATUS_OK;
}

qspi_status_t SPINANDBlockDevice::_qspi_send_general_command(qspi_inst_t instruction, bd_addr_t addr,
                                                             const char *tx_buffer, bd_size_t tx_length,
                                                             const char *rx_buffer, bd_size_t rx_length)
{
    tr_debug("Inst: 0x%xh, addr: %llu, tx length: %llu, rx length: %llu", instruction, addr, tx_length, rx_length);

    // Send a general command instruction to driver
    qspi_status_t status = _qspi.command_transfer(instruction, (int)addr, tx_buffer, tx_length, rx_buffer, rx_length);
    if (QSPI_STATUS_OK != status) {
        tr_error("Sending Generic command: %x", instruction);
        return status;
    }

    return QSPI_STATUS_OK;
}


