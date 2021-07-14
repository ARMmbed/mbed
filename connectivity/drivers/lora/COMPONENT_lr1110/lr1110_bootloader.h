/*!
 * @file      lr1110_bootloader.h
 *
 * @brief     Bootloader driver definition for LR1110
 *
 * Revised BSD License
 * Copyright Semtech Corporation 2020. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LR1110_BOOTLOADER_H
#define LR1110_BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include "lr1110_bootloader_types.h"
#include "lr1110_types.h"

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC MACROS -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC CONSTANTS --------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

/*!
 * @brief Return the status register
 *
 * @param [in] context Chip implementation context
 *
 * @param [out] status Pointer to a structure holding the status register
 */
void lr1110_bootloader_get_status( const void* context );

/*!
 * @brief Return the version of the system (hardware and software)
 *
 * @param [in] context Chip implementation context
 *
 * @param [out] version Pointer to the structure holding the system version
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_get_version( const void* context, lr1110_bootloader_version_t* version );

/*!
 * @brief Erase the whole flash memory of the chip
 *
 * This function shall be called before any attempt to write a new firmware in flash memory
 *
 * @param [in] context Chip implementation context
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_erase_flash( const void* context );

/*!
 * @brief Erase the specified page in the flash memory
 *
 * @param [in] context Chip implementation context
 *
 * @param [in] page_number The index of the page to erase
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_erase_page( const void* context, const uint8_t page_number );

/*!
 * @brief Write data in program flash memory of the chip
 *
 * This function shall be used when updating the flash content of the LR1110.
 * The flash payload to transfer shall be represented as an array of words (ie 4 bytes values).
 *
 * @param [in] context Chip implementation context
 *
 * @param [in] offset The offset from start register of flash
 *
 * @param [in] buffer A pointer to the buffer holding the content of flash to transfert. Its size in words must be at
 * least length
 *
 * @param [in] length Number of words (i.e. 4 bytes) in the buffer to transfer
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_write_flash( const void* context, const uint32_t offset, const uint32_t* buffer,
                                               const uint8_t length );

/*!
 * @brief Write data in program flash memory of the chip
 *
 * This function shall be used when updating the flash content of the LR1110.
 * The flash payload to transfer shall be represented as an array of words (i.e. 4 bytes values).
 *
 * @param [in] context Chip implementation context
 *
 * @param [in] offset The offset from start register of flash
 *
 * @param [in] buffer A pointer to the buffer holding the content of flash to transfert. Its size in words must be at
 * least length
 *
 * @param [in] length Number of words (i.e. 4 bytes) in the buffer to transfer
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_write_flash_full( const void* context, const uint32_t offset, const uint32_t* buffer,
                                                    const uint32_t length );

/*!
 * @brief Write encrypted data in program flash memory of the chip
 *
 * This function shall be used when updating the encrypted flash content of the LR1110.
 * The encrypted flash payload to transfer shall be represented as an array of words (i.e. 4 bytes values).
 *
 * @param [in] context Chip implementation context
 *
 * @param [in] offset The offset from start register of flash
 *
 * @param [in] buffer A pointer to the buffer holding the encrypted content of
 * flash to transfert. Its size in words must be at least length
 *
 * @param [in] length Number of words (i.e. 4 bytes) in the buffer to transfer
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_write_flash_encrypted( const void* context, const uint32_t offset,
                                                         const uint32_t* buffer, const uint8_t length );

/*!
 * @brief Write encrypted data in program flash memory of the chip
 *
 * This function shall be used when updating the encrypted flash content of the LR1110.
 * The encrypted flash payload to transfer shall be represented as an array of words (ie 4 * bytes values).
 *
 * @param [in] context Chip implementation context
 *
 * @param [in] offset The offset from start register of flash
 *
 * @param [in] buffer A pointer to the buffer holding the encrypted content of
 * flash to transfert. Its size in words must be at least length
 *
 * @param [in] length Number of words (i.e. 4 bytes) in the buffer to transfer
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_write_flash_encrypted_full( const void* context, const uint32_t offset,
                                                              const uint32_t* buffer, const uint32_t length );

/*!
 * @brief Get calculated hash of flash content.
 *
 * This method should be used to get the hash of flash content.
 *
 * @param [in] context Chip implementation context
 *
 * @param [out] hash Pointer to the hash array to be populated with hash value
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_get_hash( const void* context, lr1110_bootloader_hash_t hash );

/*!
 * @brief Software reset of the chip.
 *
 * This method should be used to reboot the chip in a specified mode.
 * Rebooting in flash mode presumes that the content in flash memory is not corrupted (i.e. the integrity check
 * performed by the bootloader before executing the first instruction in flash is OK).
 *
 * @param [in] context Chip implementation context
 *
 * @param [in] stay_in_bootloader Selector to stay in bootloader or execute flash code after reboot. If true, the
 * bootloader will not execute the flash code but activate SPI interface to allow firmware upgrade
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_reboot( const void* context, const bool stay_in_bootloader );

/*!
 * @brief Returns the 4-byte PIN which can be used to register a device on cloud
 * services.
 *
 * @param [in] context Chip implementation context
 *
 * @param [out] pin Pointer to the array to be populated with the PIN
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_read_pin( const void* context, lr1110_bootloader_pin_t pin );

/*!
 * @brief Read and return the Chip EUI
 *
 * @param [in] context Chip implementation context
 *
 * @param [out] chip_eui The buffer to be filled with chip EUI of the LR1110. It is up to the application to ensure
 * join_eui is long enough to hold the chip EUI
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_read_chip_eui( const void* context, lr1110_bootloader_chip_eui_t chip_eui );

/*!
 * @brief Read and return the Join EUI
 *
 * @param [in] context Chip implementation context
 *
 * @param [out] join_eui The buffer to be filled with Join EUI of the LR1110. It is up to the application to ensure
 * join_eui is long enough to hold the join EUI
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_bootloader_read_join_eui( const void* context, lr1110_bootloader_join_eui_t join_eui );

#ifdef __cplusplus
}
#endif

#endif  // LR1110_BOOTLOADER_H

/* --- EOF ------------------------------------------------------------------ */
