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
#ifndef MBED_VEEPROM_BLOCK_DEVICE_H
#define MBED_VEEPROM_BLOCK_DEVICE_H

#include "blockdevice/BlockDevice.h"
#include "platform/Callback.h"
#include "platform/PlatformMutex.h"

#if MBED_CONF_RTOS_API_PRESENT
#include "rtos/rtos.h"
#endif

#if COMPONENT_SPIF
#include "SPIFBlockDevice.h"
#endif

#if COMPONENT_QSPIF
#include "QSPIFBlockDevice.h"
#endif

#if COMPONENT_OSPIF
#include "OSPIFBlockDevice.h"
#endif

#if COMPONENT_SPINAND
#include "SPINANDBlockDevice.h"
#endif
/** Enum veef standard error codes
 *
 *  @enum veef_bd_error
 */
enum veef_bd_error {
    VEEF_BD_ERROR_OK              = 0,     /*!< no error */
    VEEF_BD_ERROR_DEVICE_ERROR    = BD_ERROR_DEVICE_ERROR, /*!< device specific error -4001 */
    VEEF_BD_ERROR_EINVAL          = -4002, /* Invalid argument */
    VEEF_BD_ERROR_EFAULT          = -4003, /* Bad address */
    VEEF_BD_ERROR_ENOSPC          = -4004, /* No space left on device */
    VEEF_BD_ERROR_ENODEV          = -4005, /* No such device */
    VEEF_BD_ERROR_ENOMEM          = -4006, /* Out of memory */
    VEEF_BD_ERROR_EIO             = -4007, /* I/O error */
    VEEF_BD_ERROR_ENXIO           = -4008, /* No such device or address */
    VEEF_BD_ERROR_ENOFS           = -4009, /* No valid file system */
    VEEF_BD_ERROR_EECC            = -4010, /* ECC failure */
    VEEF_BD_ERROR_EPERM           = -4011, /* Operation not permitted */
    VEEF_BD_ERROR_EOS             = -4012, /* OS error */
};

/* EEPROM physical layout */
#define MX_EEPROM_CLUSTERS_PER_BANK     (1)
#define MX_EEPROM_BANKS                 (4)
#define MX_EEPROM_CLUSTER_SIZE          (MX_FLASH_SECTOR_SIZE * MX_EEPROM_SECTORS_PER_CLUSTER)
#define MX_EEPROM_BANK_SIZE             (MX_EEPROM_CLUSTER_SIZE * MX_EEPROM_CLUSTERS_PER_BANK)

#if COMPONENT_OSPIF
#define MX_EEPROM_SECTORS_PER_CLUSTER   (16)
#elif COMPONENT_SPINAND
#define MX_EEPROM_SECTOR_SIZE           MX_FLASH_BLOCK_SIZE
#define MX_EEPROM_SECTOR_OFFSET         MX_FLASH_BLOCK_OFFSET
#define MX_EEPROM_SECTORS_PER_CLUSTER   (6)
#define MX_EEPROM_CLUSTER_OFFSET        (MX_FLASH_SECTOR_OFFSET * MX_EEPROM_SECTORS_PER_CLUSTER)
#endif

#if defined(MX_FLASH_BANKS) && (MX_EEPROM_BANKS > MX_FLASH_BANKS)
#error "too many banks!"
#endif

#if (MX_EEPROM_SECTORS_PER_CLUSTER > 256)
#error "too many sectors per cluster!"
#endif

/* EEPROM parameters */
#define MX_EEPROM_HEADER_SIZE           (4)
#define MX_EEPROM_ENTRY_SIZE            (MX_FLASH_PAGE_SIZE)
#define MX_EEPROM_PAGE_SIZE             (MX_EEPROM_ENTRY_SIZE - MX_EEPROM_HEADER_SIZE)
#define MX_EEPROM_SYSTEM_SECTOR         (MX_EEPROM_SECTORS_PER_CLUSTER - 1)
#define MX_EEPROM_SYSTEM_SECTOR_OFFSET  (MX_EEPROM_SYSTEM_SECTOR * MX_FLASH_SECTOR_SIZE)
#define MX_EEPROM_SYSTEM_ENTRY_SIZE     (16)
#define MX_EEPROM_SYSTEM_ENTRIES        (MX_FLASH_SECTOR_SIZE / MX_EEPROM_SYSTEM_ENTRY_SIZE)
#define MX_EEPROM_DATA_SECTORS          (MX_EEPROM_SYSTEM_SECTOR)
#define MX_EEPROM_FREE_SECTORS          (1)
#define MX_EEPROM_ENTRIES_PER_CLUSTER   (MX_EEPROM_ENTRIES_PER_SECTOR * MX_EEPROM_DATA_SECTORS)
#define MX_EEPROM_BLOCK_SIZE            (MX_EEPROM_PAGE_SIZE * MX_EEPROM_LPAS_PER_CLUSTER)
#define MX_EEPROM_BLOCKS                (MX_EEPROM_CLUSTERS_PER_BANK)
#define MX_EEPROM_SIZE                  (MX_EEPROM_BLOCK_SIZE * MX_EEPROM_BLOCKS)
#define MX_EEPROMS                      (MX_EEPROM_BANKS)
#define MX_EEPROM_TOTAL_SIZE            (MX_EEPROM_SIZE * MX_EEPROMS)

#if COMPONENT_OSPIF
#define MX_EEPROM_ENTRIES_PER_SECTOR    (MX_FLASH_SECTOR_SIZE / MX_EEPROM_ENTRY_SIZE)
#define MX_EEPROM_LPAS_PER_CLUSTER      (MX_EEPROM_DATA_SECTORS - MX_EEPROM_FREE_SECTORS)
#elif COMPONENT_SPINAND
#define MX_EEPROM_ENTRIES_PER_SECTOR    ((MX_FLASH_SECTOR_SIZE / MX_EEPROM_ENTRY_SIZE) - 6)
#define MX_EEPROM_SYSTEM_ENTRY_OFFSET   (0x800)
#define MX_EEPROM_LPAS_PER_SECTOR       (8)
#define MX_EEPROM_LPAS_PER_CLUSTER      (MX_EEPROM_DATA_SECTORS - MX_EEPROM_FREE_SECTORS)
#endif

#if (MX_EEPROM_ENTRY_SIZE < MX_FLASH_CHUNK_SIZE)
#error "too small data entry size!"
#endif

#if (MX_EEPROM_ENTRIES_PER_SECTOR > 256)
#error "too many entries per sector!"
#endif

#if (MX_EEPROM_SYSTEM_ENTRY_SIZE < MX_FLASH_CHUNK_SIZE)
#error "too small system entry size!"
#endif

#define MX_EEPROM_HASH_CROSSBANK        0    /* Page hash */
#define MX_EEPROM_HASH_HYBRID           1    /* Block hash */
#define MX_EEPROM_HASH_SEQUENTIAL       2    /* Bank hash */

/* Address hash algorithm */
#if COMPONENT_OSPIF
#define MX_EEPROM_HASH_AlGORITHM        MX_EEPROM_HASH_CROSSBANK
#elif COMPONENT_SPINAND
#define MX_EEPROM_HASH_AlGORITHM        MX_EEPROM_HASH_SEQUENTIAL
#endif

/* Wear leveling interval */
#define MX_EEPROM_WL_INTERVAL           10000

/* Check on-die ECC after each read */
//#define MX_EEPROM_ECC_CHECK

/* read recovery */
//#define MX_EEPROM_READ_ROLLBACK

/* Power cycling protection */
#define MX_EEPROM_PC_PROTECTION

#ifdef MX_EEPROM_PC_PROTECTION

#if !defined(MX_FLASH_SUPPORT_RWW) && !defined(MX_EEPROM_ECC_CHECK)
#define MX_EEPROM_ECC_CHECK
#endif

#ifndef MX_EEPROM_READ_ROLLBACK
#define MX_EEPROM_READ_ROLLBACK
#endif

#endif

#if defined(MX_FLASH_SUPPORT_RWW) && defined(MX_EEPROM_ECC_CHECK)
#error "cannot read ECC status in RWW mode!"
#endif

#define MX_EEPROM_WRITE_RETRIES         2    /* number of write retries */
#define MX_EEPROM_READ_RETRIES          2    /* number of read retries */

#if defined(MX_EEPROM_READ_ROLLBACK) && (MX_EEPROM_READ_RETRIES == 0)
#error "please set the number of read retries!"
#endif

/* RWWEE ID: "MX" */
#define MFTL_ID            0x4D58

/* Maximum unsigned value */
#define DATA_NONE8         0xff
#define DATA_NONE16        0xffff
#define DATA_NONE32        0xffffffffUL

/* Macros */
#undef min_t
#define min_t(type, x, y) ({ \
    type __min1 = (x);       \
    type __min2 = (y);       \
    __min1 < __min2 ? __min1: __min2; })

/* RWWEE operations */
typedef enum {
    OPS_READ        = 0x5244,
    OPS_WRITE       = 0x7772,
    OPS_ERASE_BEGIN = 0x4553,
    OPS_ERASE_END   = 0x6565,
#if COMPONENT_SPINAND
    OPS_GC_CP_BEGIN = 0xAAAA,
    OPS_GC_CP_END   = 0x5555,
#endif
    OPS_NONE        = 0x4E4E,
} ee_ops;

/* System Entry */
struct system_entry {
    uint16_t id;
    uint16_t ops;
    uint16_t arg;
    uint16_t cksum;
//  uint8_t pad[MX_EEPROM_SYSTEM_ENTRY_SIZE - 8];
};

/* Data entry header */
struct eeprom_header {
    uint8_t LPA;
    uint8_t LPA_inv;
    uint16_t crc;
    uint8_t pad[MX_EEPROM_HEADER_SIZE - 4];
};

#if COMPONENT_SPINAND
/* System Entry Addr*/
// OPS_ERASE_END
#define SYS_ENTRY_ADDR_E_E      0
// OPS_GC_CP_BEGIN + source sector addr (write in Gc destination sector)
#define SYS_ENTRY_ADDR_G_B_D    0 + MX_EEPROM_SYSTEM_ENTRY_SIZE
// OPS_GC_CP_BEGIN + des sector addr (write in Gc source sector)
#define SYS_ENTRY_ADDR_G_B_S    63 * MX_FLASH_PAGE_OFFSET + 2048
// OPS_GC_CP_END (write in Gc source sector)
#define SYS_ENTRY_ADDR_G_E      63 * MX_FLASH_PAGE_OFFSET + 2048 + MX_EEPROM_SYSTEM_ENTRY_SIZE
// OPS_ERASE_END
#define SYS_ENTRY_ADDR_E_B      63 * MX_FLASH_PAGE_OFFSET + 2048 + MX_EEPROM_SYSTEM_ENTRY_SIZE * 2
#endif

/* Data Entry */
struct eeprom_entry {
    struct eeprom_header header;
    uint8_t data[MX_EEPROM_PAGE_SIZE];
};

/* Bank information */
struct bank_info {
    uint32_t bank;              /* current bank */
    uint32_t bank_offset;       /* bank address */

    uint32_t block;             /* current block */
    uint32_t block_offset;      /* block address */
    struct eeprom_entry cache;  /* entry cache */
    bool cache_dirty;           /* cache status */

    /* address mapping */
    uint8_t l2ps[MX_EEPROM_LPAS_PER_CLUSTER];
    uint8_t l2pe[MX_EEPROM_LPAS_PER_CLUSTER];
    uint8_t p2l[MX_EEPROM_DATA_SECTORS];    /* TODO: bitmap */
#if COMPONENT_SPINAND
    uint8_t latest_used_entry_per_sector[MX_EEPROM_DATA_SECTORS];
    uint8_t l2ps_group[MX_EEPROM_DATA_SECTORS];
#endif

    uint32_t dirty_block;       /* obsoleted sector to be erased */
    uint32_t dirty_sector;      /* obsoleted sector to be erased */

    /* system entry address */
    uint32_t sys_entry[MX_EEPROM_BLOCKS];
};

/* EEPROM information */
struct eeprom_info {
    struct bank_info bi[MX_EEPROMS];  /* bank info */
    uint32_t rwCnt;                   /* User R/W statistics */
};

/** BlockDevice for Virtual EEPROM Emulation
 *
 *  @code
 *  // Here's an example using VEE Block device
 *  #include "mbed.h"
 *  #include "VEEBlockDevice.h"
 *
 *  // Create Virtual EEPROM Emulation device
 *  VEEBlockDevice vee(bd, 32*1024);
 *
 *  int main() {
 *      printf("vee test\n");
 *
 *      // Initialize the device and print the memory layout
 *      vee.init();
 *      printf("vee size: %llu\n",         vee.size());
 *      printf("vee read size: %llu\n",    vee.get_read_size());
 *      printf("vee program size: %llu\n", vee.get_program_size());
 *      printf("vee erase size: %llu\n",   vee.get_erase_size());
 *
 *      // Write "Hello World!" to the first block
 *      char *buffer = (char*)malloc(vee.get_erase_size());
 *      sprintf(buffer, "Hello World!\n");
 *      vee.erase(0, vee.get_erase_size());
 *      vee.program(buffer, 0, vee.get_erase_size());
 *
 *      // Read back what was stored
 *      vee.read(buffer, 0, vee.get_erase_size());
 *      printf("%s", buffer);
 *
 *      // Deinitialize the device
 *      vee.deinit();
 *  }
 *  @endcode
 */
class VEEBlockDevice : public BlockDevice {
public:
    /** Constructor to create an VEEBlockDevice
     *
     *  @param bd           Block device to back the FlashSimBlockDevice
     *  @param size         The size of the device in bytes
     */
    VEEBlockDevice(BlockDevice *bd);

    virtual ~VEEBlockDevice();

    /** Initialize a block device
     *
     *  @return         0 on success or a negative error code on failure
     */
    virtual int init();

    /** Deinitialize a block device
     *
     *  @return         0 on success or a negative error code on failure
     */
    virtual int deinit();

    /** Read blocks from a block device
     *
     *  @param buffer   Buffer to write blocks to
     *  @param addr     Address of block to begin reading from
     *  @param size     Size to read in bytes, must be a multiple of read block size
     *  @return         0 on success, negative error code on failure
     */
    virtual int read(void *buffer, bd_addr_t addr, bd_size_t size);

    /** Program blocks to a block device
     *
     *  The blocks must have been erased prior to being programmed
     *
     *  @param buffer   Buffer of data to write to blocks
     *  @param addr     Address of block to begin writing to
     *  @param size     Size to write in bytes, must be a multiple of program block size
     *  @return         0 on success, negative error code on failure
     */
    virtual int program(const void *buffer, bd_addr_t addr, bd_size_t size);

    /** EEPROM cache write back API.
     *
     * @return         0 on success, negative error code on failure
     */
    virtual int program_back(void);

    /** EEPROM sync write API.
     *
     *  @param buffer   Buffer of data to write to blocks
     *  @param addr     Address of block to begin writing to
     *  @param size     Size to write in bytes, must be a multiple of program block size
     *  @return         0 on success, negative error code on failure
     */
    virtual int sync_program(const void *buffer, bd_addr_t addr, bd_size_t size);

    /** EEPROM user cache and meta data flush API.
     *
     *  NOTE: Call this API just before power down.
     * @return         0 on success, negative error code on failure
     */
    virtual int flush(void);

    /** EEPROM background task API.
     *  NOTE: Call this API just before MCU sleep.
     */
    virtual void background(void);

    /** Erase blocks on a block device
     *
     *  The state of an erased block is undefined until it has been programmed
     *
     *  @param addr     Address of block to begin erasing
     *  @param size     Size to erase in bytes, must be a multiple of erase block size
     *  @return         0 on success, negative error code on failure
     */
    virtual int erase(bd_addr_t addr, bd_size_t size);

    /** Get the size of a readable block
     *
     *  @return         Size of a readable block in bytes
     */
    virtual bd_size_t get_read_size() const;

    /** Get the size of a programable block
     *
     *  @return         Size of a programable block in bytes
     *  @note Must be a multiple of the read size
     */
    virtual bd_size_t get_program_size() const;

    /** Get the size of a eraseable block
     *
     *  @return         Size of a eraseable block in bytes
     *  @note Must be a multiple of the program size
     */
    virtual bd_size_t get_erase_size() const;

    /** Get the total size of the underlying device
     *
     *  @return         Size of the underlying device in bytes
     */
    virtual bd_size_t size() const;

    /** Get the BlockDevice class type.
     *
     *  @return         A string representation of the BlockDevice class type.
     */
    virtual const char *get_type() const;

    /** Format EEPROM Emulator.
     *
     * @return 0 on success, negative error code on failure
     */
    virtual int format(void);

private:
    // Read NOR flash.
    int _ee_device_read(bd_addr_t addr, bd_size_t size, void *buffer);

    // Write NOR flash.
    int _ee_device_write(bd_addr_t addr, bd_size_t size, void *buffer);

    // Erase NOR flash.
    int _ee_device_erase(bd_addr_t addr, bd_size_t size);

#if COMPONENT_OSPIF
    // Read system entry of current block of current bank.
    int _ee_read_sys(struct bank_info *bi, uint32_t entry, struct system_entry *sys);

    // Update system entry of current block of current bank.
    int _ee_update_sys(struct bank_info *bi, uint32_t block, ee_ops ops, uint32_t arg);
#elif COMPONENT_SPINAND
    // Read system entry of current block of current bank.
    int _ee_read_sys(struct bank_info *bi, uint32_t sector, uint32_t system_entry_addr,
                     struct system_entry *sys);

    // Update system entry of current block of current bank.
    int _ee_update_sys(struct bank_info *bi, uint32_t sector, uint32_t system_entry_addr,
                       ee_ops ops, uint32_t arg);
#endif
    // Read the specified entry of current block of current bank.
    int _ee_read(struct bank_info *bi, uint32_t entry, void *buf, bool header);

    // Write the specified entry of current block of current bank.
    int _ee_write(struct bank_info *bi, uint32_t entry, void *buf);

    // Erase the obsoleted sector of current bank.
    int _ee_erase(struct bank_info *bi);

    // Locate the latest version of specified logical page.
    uint32_t _ee_find_latest(struct bank_info *bi, uint32_t LPA, bool free);

    // Scan current block to build mapping table.
    int _ee_build_mapping(struct bank_info *bi, uint32_t block);

    // Find a free entry for given logical page.
    uint32_t _ee_search_free(struct bank_info *bi, uint32_t LPA);

    // Read specified logical page of current block of current bank.
    int _ee_read_page(struct bank_info *bi, uint32_t LPA);

    // Write specified logical page of current block of current bank.
    int _ee_write_page(struct bank_info *bi, uint32_t LPA);

    // Handle buffer and cache.
    int _ee_rw_buffer(struct bank_info *bi, bd_addr_t addr, bd_size_t size,
                      uint8_t *buf, bool rw);

    // Distribute continuous logical address into different banks.
    int _ee_rw(bd_addr_t addr, bd_size_t size, uint8_t *buf, bool rw);

    // Write dirty cache back (For internal use only).
    int _eeprom_wb(struct bank_info *bi);

    // Handle wear leveling.
    int _eeprom_wear_leveling(void);

    // Handle insufficient sector erase.
    int _ee_check_erase(struct bank_info *bi, uint32_t sector);

    // Check system info and handle power cycling.
    int _ee_check_sys(void);

    // copy data
    uint32_t _ee_gc_cp(struct bank_info *bi, uint8_t src_sector, uint8_t des_sector);

    // garbage collection
    uint32_t _ee_gc(struct bank_info *bi, uint8_t src_sector, uint8_t des_sector);

    // check if power is failed during gc
    int _ee_check_gc_power_fail(void);

    BlockDevice *_bd;
    bool _is_initialized;
    PlatformMutex _bank_mutex[4];
    eeprom_info _eeprom;
};

#endif  /* MBED_SD_BLOCK_DEVICE_H */
