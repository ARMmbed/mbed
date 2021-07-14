/*!
 * @file      lr1110_regmem.h
 *
 * @brief     Register/memory driver definition for LR1110
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

#ifndef LR1110_REGMEM_H
#define LR1110_REGMEM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stdint.h>
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
 * @brief Write words into auxiliary memory space of LR1110.
 *
 * A word is 32-bit long. The writing operations write contiguously in auxiliary memory, starting at the address
 * provided.
 *
 * @param [in] context Chip implementation context
 * @param [in] address The auxiliary memory address to start writing operation
 * @param [in] data The buffer of words to write into memory. Its size must be enough to contain length words.
 * @param [in] length Number of words to write into memory
 *
 * @returns Operation status
 *
 * @see lr1110_regmem_read_auxreg32
 */
lr1110_status_t lr1110_regmem_write_auxreg32( const void* context, const uint32_t address, const uint32_t* buffer,
                                              const uint8_t length );

/*!
 * @brief Read words into auxiliary memory space of LR1110.
 *
 * A word is 32-bit long. The reading operations read contiguously from auxiliary memory, starting at the address
 * provided.
 *
 * @param [in] context Chip implementation context
 * @param [in] address The auxiliary memory address to start reading operation
 * @param [in] length Number of words to read from memory
 * @param [out] buffer Pointer to a words array to be filled with content from memory. Its size must be enough to
 * contain at least length words.
 *
 * @returns Operation status
 *
 * @see lr1110_regmem_write_auxreg32
 */
lr1110_status_t lr1110_regmem_read_auxreg32( const void* context, const uint32_t address, uint32_t* buffer,
                                             const uint8_t length );

/*!
 * @brief Write words into register memory space of LR1110.
 *
 * A word is 32-bit long. The writing operations write contiguously in register memory, starting at the address
 * provided.
 *
 * @param [in] context Chip implementation context
 * @param [in] address The register memory address to start writing operation
 * @param [in] data The buffer of words to write into memory. Its size must be enough to contain length words.
 * @param [in] length Number of words to write into memory
 *
 * @returns Operation status
 *
 * @see lr1110_regmem_read_regmem32
 */
lr1110_status_t lr1110_regmem_write_regmem32( const void* context, const uint32_t address, const uint32_t* buffer,
                                              const uint8_t length );

/*!
 * @brief Read words into register memory space of LR1110.
 *
 * A word is 32-bit long. The reading operations read contiguously from register memory, starting at the address
 * provided.
 *
 * @param [in] context Chip implementation context
 * @param [in] address The register memory address to start reading operation
 * @param [in] length Number of words to read from memory
 * @param [out] buffer Pointer to a words array to be filled with content from memory. Its size must be enough to
 * contain at least length words.
 *
 * @returns Operation status
 *
 * @see lr1110_regmem_write_regmem32
 */
lr1110_status_t lr1110_regmem_read_regmem32( const void* context, const uint32_t address, uint32_t* buffer,
                                             const uint8_t length );

/*!
 * @brief Write bytes into register memory space of LR1110.
 *
 * A byte is 8-bit long. The writing operations write contiguously in register memory, starting at the address provided.
 *
 * @param [in] context Chip implementation context
 * @param [in] address The register memory address to start writing operation
 * @param [in] data The buffer of bytes to write into memory. Its size must be enough to contain length bytes
 * @param [in] length Number of bytes to write into memory
 *
 * @returns Operation status
 *
 * @see lr1110_regmem_read_mem8
 */
lr1110_status_t lr1110_regmem_write_mem8( const void* context, const uint32_t address, const uint8_t* buffer,
                                          const uint8_t length );

/*!
 * @brief Read bytes into register memory space of LR1110.
 *
 * A byte is 8-bit long. The reading operations read contiguously from register memory, starting at the address
 * provided.
 *
 * @param [in] context Chip implementation context
 * @param [in] address The register memory address to start reading operation
 * @param [in] length Number of bytes to read from memory
 * @param [in] buffer Pointer to a byte array to be filled with content from memory. Its size must be enough to contain
 * at least length bytes
 *
 * @returns Operation status
 *
 * @see lr1110_regmem_write_mem8
 */
lr1110_status_t lr1110_regmem_read_mem8( const void* context, const uint32_t address, uint8_t* buffer,
                                         const uint8_t length );

/*!
 * @brief Write bytes into radio TX buffer memory space of LR1110.
 *
 * @param [in] context Chip implementation context
 * @param [in] data The buffer of bytes to write into radio buffer. Its size must be enough to contain length bytes
 * @param [in] length Number of bytes to write into radio buffer
 *
 * @returns Operation status
 *
 * @see lr1110_regmem_read_buffer8
 */
lr1110_status_t lr1110_regmem_write_buffer8( const void* context, const uint8_t* buffer, const uint8_t length );

/*!
 * @brief Read bytes from radio RX buffer memory space of LR1110.
 *
 * @param [in] context Chip implementation context
 * @param [in] offset Memory offset to start reading
 * @param [in] length Number of bytes to read from radio buffer
 * @param [in] data Pointer to a byte array to be filled with content from radio buffer. Its size must be enough to
 * contain at least length bytes
 *
 * @returns Operation status
 *
 * @see lr1110_regmem_write_buffer8
 */
lr1110_status_t lr1110_regmem_read_buffer8( const void* context, uint8_t* buffer, const uint8_t offset,
                                            const uint8_t length );

/*!
 * @brief Clear radio RX buffer
 *
 * Set to 0x00 all content of the radio RX buffer
 *
 * @param [in] context Chip implementation context
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_regmem_clear_rxbuffer( const void* context );

/*!
 * @brief Read-modify-write data at given register/memory address
 *
 * @param [in] context Chip implementation context
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_regmem_write_regmem32_mask( const void* context, const uint32_t address, const uint32_t mask,
                                                   const uint32_t data );

#ifdef __cplusplus
}
#endif

#endif  // LR1110_REGMEM_H

/* --- EOF ------------------------------------------------------------------ */
