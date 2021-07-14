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
#ifndef MBED_SPINAND_BLOCK_DEVICE_H
#define MBED_SPINAND_BLOCK_DEVICE_H

#include "drivers/QSPI.h"
#include "blockdevice/BlockDevice.h"
#include "platform/Callback.h"

#ifndef MBED_CONF_SPINAND_QSPI_IO0
#define MBED_CONF_SPINAND_QSPI_IO0 NC
#endif
#ifndef MBED_CONF_SPINAND_QSPI_IO1
#define MBED_CONF_SPINAND_QSPI_IO1 NC
#endif
#ifndef MBED_CONF_SPINAND_QSPI_IO2
#define MBED_CONF_SPINAND_QSPI_IO2 NC
#endif
#ifndef MBED_CONF_SPINAND_QSPI_IO3
#define MBED_CONF_SPINAND_QSPI_IO3 NC
#endif
#ifndef MBED_CONF_SPINAND_QSPI_SCK
#define MBED_CONF_SPINAND_QSPI_SCK NC
#endif
#ifndef MBED_CONF_SPINAND_QSPI_CSN
#define MBED_CONF_SPINAND_QSPI_CSN NC
#endif
#ifndef MBED_CONF_SPINAND_QSPI_POLARITY_MODE
#define MBED_CONF_SPINAND_QSPI_POLARITY_MODE 0
#endif
#ifndef MBED_CONF_SPINAND_QSPI_FREQ
#define MBED_CONF_SPINAND_QSPI_FREQ 40000000
#endif

/** Enum spinand standard error codes
 *
 *  @enum spinand_bd_error
 */
enum spinand_bd_error {
    SPINAND_BD_ERROR_OK                    = 0,     /*!< no error */
    SPINAND_BD_ERROR_DEVICE_ERROR          = BD_ERROR_DEVICE_ERROR, /*!< device specific error -4001 */
    SPINAND_BD_ERROR_PARSING_FAILED        = -4002, /* SFDP Parsing failed */
    SPINAND_BD_ERROR_READY_FAILED          = -4003, /* Wait for Mem Ready failed */
    SPINAND_BD_ERROR_WREN_FAILED           = -4004, /* Write Enable Failed */
    SPINAND_BD_ERROR_INVALID_ERASE_PARAMS  = -4005, /* Erase command not on sector aligned addresses or exceeds device size */
    SPINAND_BD_ERROR_DEVICE_NOT_UNIQUE     = -4006, /* Only one instance per csel is allowed */
    SPINAND_BD_ERROR_DEVICE_MAX_EXCEED     = -4007  /* Max active SPINAND devices exceeded */
};

/** Enum spinand polarity mode
 *
 *  @enum spinand_polarity_mode
 */
enum spinand_polarity_mode {
    SPINAND_POLARITY_MODE_0 = 0, /* CPOL=0, CPHA=0 */
    SPINAND_POLARITY_MODE_1      /* CPOL=1, CPHA=1 */
};

#define SPINAND_MAX_ACTIVE_FLASH_DEVICES 10

/** BlockDevice for SPI NAND flash devices over QSPI bus
 *
 *  @code
 *  // Here's an example using SPI NAND flash device on DISCO_L4R9I target
 *  #include "mbed.h"
 *  #include "SPINANDBlockDevice.h"
 *
 *  SPINANDBlockDevice block_device(SPINAND_FLASH1_IO0, SPINAND_FLASH1_IO1, SPINAND_FLASH1_IO2, SPINAND_FLASH1_IO3,
 *                                  SPINAND_FLASH1_SCK, SPINAND_FLASH1_CSN, SPINAND_POLARITY_MODE_0, MBED_CONF_SPINAND_QSPI_FREQ);
 *
 *  int main()
 *  {
 *      printf("SPI NAND Flash Block Device example\n");
 *
 *      // Initialize the SPI NAND flash device and print the memory layout
 *      block_device.init();
 *      bd_size_t sector_size_at_address_0 = block_device.get_erase_size(0);
 *
 *      printf("SPINAND BD size: %llu\n",         block_device.size());
 *      printf("SPINAND BD read size: %llu\n",    block_device.get_read_size());
 *      printf("SPINAND BD program size: %llu\n", block_device.get_program_size());
 *      printf("SPINAND BD erase size (at address 0): %llu\n", sector_size_at_address_0);
 *
 *      // Write "Hello World!" to the first block
 *      char *buffer = (char *) malloc(sector_size_at_address_0);
 *      sprintf(buffer, "Hello World!\n");
 *      block_device.erase(0, sector_size_at_address_0);
 *      block_device.program(buffer, 0, sector_size_at_address_0);
 *
 *      // Read back what was stored
 *      block_device.read(buffer, 0, sector_size_at_address_0);
 *      printf("%s", buffer);
 *
 *      // Deinitialize the device
 *      block_device.deinit();
 *  }
 *  @endcode
 */
class SPINANDBlockDevice : public mbed::BlockDevice {
public:
    /** Create SPINANDBlockDevice - An SPI NAND Flash Block Device over QSPI bus
     *
     *  @param io0 1st IO pin used for sending/receiving data during data phase of a transaction
     *  @param io1 2nd IO pin used for sending/receiving data during data phase of a transaction
     *  @param io2 3rd IO pin used for sending/receiving data during data phase of a transaction
     *  @param io3 4th IO pin used for sending/receiving data during data phase of a transaction
     *  @param sclk QSPI Clock pin
     *  @param csel QSPI chip select pin
     *  @param clock_mode specifies the QSPI Clock Polarity mode (SPINAND_POLARITY_MODE_0/SPINAND_POLARITY_MODE_1)
     *         default value = 0
     *  @param freq Clock frequency of the QSPI bus (defaults to 40MHz)
     */
    SPINANDBlockDevice(PinName io0 = MBED_CONF_SPINAND_QSPI_IO0,
                       PinName io1 = MBED_CONF_SPINAND_QSPI_IO1,
                       PinName io2 = MBED_CONF_SPINAND_QSPI_IO2,
                       PinName io3 = MBED_CONF_SPINAND_QSPI_IO3,
                       PinName sclk = MBED_CONF_SPINAND_QSPI_SCK,
                       PinName csel = MBED_CONF_SPINAND_QSPI_CSN,
                       int clock_mode = MBED_CONF_SPINAND_QSPI_POLARITY_MODE,
                       int freq = MBED_CONF_SPINAND_QSPI_FREQ);

    /** Initialize a block device
     *
     *  @return         SPINAND_BD_ERROR_OK(0) - success
     *                  SPINAND_BD_ERROR_DEVICE_ERROR - device driver transaction failed
     *                  SPINAND_BD_ERROR_READY_FAILED - Waiting for Memory ready failed or timedout
     */
    virtual int init();

    /** Deinitialize a block device
     *
     *  @return         SPINAND_BD_ERROR_OK(0) - success
     *                  SPINAND_BD_ERROR_DEVICE_ERROR - device driver transaction failed
     */
    virtual int deinit();

    /** Destruct SPINANDBlockDevie
      */
    ~SPINANDBlockDevice()
    {
        deinit();
    }

    /** Read blocks from a block device
     *
     *  @param buffer   Buffer to write blocks to
     *  @param addr     Address of block to begin reading from
     *  @param size     Size to read in bytes, must be a multiple of read block size
     *  @return         SPINAND_BD_ERROR_OK(0) - success
     *                  SPINAND_BD_ERROR_DEVICE_ERROR - device driver transaction failed
     */
    virtual int read(void *buffer, mbed::bd_addr_t addr, mbed::bd_size_t size);

    /** Program blocks to a block device
     *
     *  The blocks must have been erased prior to being programmed
     *
     *  @param buffer   Buffer of data to write to blocks
     *  @param addr     Address of block to begin writing to
     *  @param size     Size to write in bytes, must be a multiple of program block size
     *  @return         SPINAND_BD_ERROR_OK(0) - success
     *                  SPINAND_BD_ERROR_DEVICE_ERROR - device driver transaction failed
     *                  SPINAND_BD_ERROR_READY_FAILED - Waiting for Memory ready failed or timed out
     *                  SPINAND_BD_ERROR_WREN_FAILED - Write Enable failed
     */
    virtual int program(const void *buffer, mbed::bd_addr_t addr, mbed::bd_size_t size);

    /** Erase blocks on a block device
     *
     *  The state of an erased block is undefined until it has been programmed
     *
     *  @param addr     Address of block to begin erasing
     *  @param size     Size to erase in bytes, must be a multiple of erase block size
     *  @return         SPINAND_BD_ERROR_OK(0) - success
     *                  SPINAND_BD_ERROR_DEVICE_ERROR - device driver transaction failed
     *                  SPINAND_BD_ERROR_READY_FAILED - Waiting for Memory ready failed or timed out
     *                  SPINAND_BD_ERROR_WREN_FAILED - Write Enable failed
     *                  SPINAND_BD_ERROR_INVALID_ERASE_PARAMS - Trying to erase unaligned address or size
     */
    virtual int erase(mbed::bd_addr_t addr, mbed::bd_size_t size);

    /** Get the size of a readable block
     *
     *  @return         Size of a readable block in bytes
     */
    virtual mbed::bd_size_t get_read_size() const;

    /** Get the size of a programable block
     *
     *  @return         Size of a program block size in bytes
     *  @note Must be a multiple of the read size
     */
    virtual mbed::bd_size_t get_program_size() const;

    /** Get the size of a eraseable block
     *
     *  @return         Size of a minimal erase block, common to all regions, in bytes
     *  @note Must be a multiple of the program size
     */
    virtual mbed::bd_size_t get_erase_size() const;
    virtual mbed::bd_size_t get_erase_size(bd_addr_t addr) const;

    /** Get the value of storage byte after it was erased
     *
     *  If get_erase_value returns a non-negative byte value, the underlying
     *  storage is set to that value when erased, and storage containing
     *  that value can be programmed without another erase.
     *
     *  @return         The value of storage when erased, or -1 if you can't
     *                  rely on the value of erased storage
     */
    virtual int get_erase_value() const;

    /** Get the total size of the underlying device
     *
     *  @return         Size of the underlying device in bytes
     */
    virtual mbed::bd_size_t size() const;

    /** Get the BlockDevice class type.
     *
     *  @return         A string represent the BlockDevice class type.
     */
    virtual const char *get_type() const;

private:
    /********************************/
    /*   Different Device Csel Mgmt */
    /********************************/
    // Add a new SPI NAND device CS to existing devices list.
    // Only one SPINANDBlockDevice instance per CS is allowed
    int add_new_csel_instance(PinName csel);

    // Remove device CS from existing device list upon destroying object (last deinit is called)
    int remove_csel_instance(PinName csel);

    /********************************/
    /*   Calls to QSPI Driver APIs  */
    /********************************/
    // Send Program/Write command to Driver
    qspi_status_t _qspi_send_program_command(mbed::qspi_inst_t prog_instruction, const void *buffer,
                                             mbed::bd_addr_t addr, mbed::bd_size_t *size);

    // Send Read command to Driver
    qspi_status_t _qspi_send_read_command(mbed::qspi_inst_t read_instruction, void *buffer, mbed::bd_addr_t addr, mbed::bd_size_t size);

    // Send Erase Instruction using command_transfer command to Driver
    qspi_status_t _qspi_send_erase_command(mbed::qspi_inst_t erase_instruction, mbed::bd_addr_t addr, mbed::bd_size_t size);

    // Send Generic command_transfer command to Driver
    qspi_status_t _qspi_send_general_command(mbed::qspi_inst_t instruction_int, mbed::bd_addr_t addr, const char *tx_buffer,
                                             mbed::bd_size_t tx_length, const char *rx_buffer, mbed::bd_size_t rx_length);

    // Send set_frequency command to Driver
    qspi_status_t _qspi_set_frequency(int freq);

    /*********************************/
    /* Flash Configuration Functions */
    /*********************************/

    // Quad Enable in Security Register
    int _set_quad_enable();

    // Clear the device's block protection
    int _clear_block_protection();

    // Configure Write Enable in Status Register
    int _set_write_enable();

    // Wait on status register until write not-in-progress
    bool _is_mem_ready();

private:

    // QSPI Driver Object
    mbed::QSPI _qspi;

    // Static List of different QSPI based Flash devices csel that already exist
    // Each QSPI Flash device csel can have only 1 SPINANDBlockDevice instance
    // _devices_mutex is used to lock csel list - only one SPINANDBlockDevice instance per csel is allowed
    static SingletonPtr<PlatformMutex> _devices_mutex;
    static int _number_of_active_spinand_flash_csel;
    static PinName *_active_spinand_flash_csel_arr;

    int _unique_device_status;
    PinName _csel;

    // Mutex is used to protect Flash device for some QSPI Driver commands that must be done sequentially with no other commands in between
    // e.g. (1)Set Write Enable, (2)Program, (3)Wait Memory Ready
    PlatformMutex _mutex;

    // Command Instructions
    mbed::qspi_inst_t _read_instruction;
    mbed::qspi_inst_t _program_instruction;

    int _freq;

    // Bus speed configuration
    qspi_bus_width_t _inst_width; //Bus width for Instruction phase
    qspi_bus_width_t _address_width; //Bus width for Address phase
    qspi_address_size_t _address_size; //Number of bits for address
    qspi_alt_size_t _alt_size; //Number of bits for alt
    bool _alt_enabled; //Whether alt is enabled
    uint8_t _dummy_cycles; //Number of Dummy cycles required by Current Bus Mode
    qspi_bus_width_t _data_width; //Bus width for Data phase

    uint32_t _init_ref_count;
    bool _is_initialized;
};

#endif
