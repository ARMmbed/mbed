/*!
 * @file      lr1110_bootloader_types.h
 *
 * @brief     Bootloader driver types for LR1110
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

#ifndef LR1110_BOOTLOADER_TYPES_H
#define LR1110_BOOTLOADER_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC MACROS -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC CONSTANTS --------------------------------------------------------
 */

/*!
 * @brief Length in byte of the LR1110 version blob
 */
#define LR1110_BL_VERSION_LENGTH ( 4 )

/*!
 * @brief Length in bytes of a hash value
 */
#define LR1110_BL_HASH_LENGTH 0x10

/*!
 * @brief Length in bytes of a PIN
 */
#define LR1110_BL_PIN_LENGTH 0x04

/*!
 * @brief Length in bytes of a chip EUI
 */
#define LR1110_BL_CHIP_EUI_LENGTH ( 8 )

/*!
 * @brief Length in bytes of a join EUI
 */
#define LR1110_BL_JOIN_EUI_LENGTH ( 8 )

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */

typedef uint8_t lr1110_bootloader_hash_t[LR1110_BL_HASH_LENGTH];
typedef uint8_t lr1110_bootloader_pin_t[LR1110_BL_PIN_LENGTH];
typedef uint8_t lr1110_bootloader_chip_eui_t[LR1110_BL_CHIP_EUI_LENGTH];
typedef uint8_t lr1110_bootloader_join_eui_t[LR1110_BL_JOIN_EUI_LENGTH];

typedef struct lr1110_bootloader_version_s
{
    uint8_t  hw;
    uint8_t  type;
    uint16_t fw;
} lr1110_bootloader_version_t;

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

#ifdef __cplusplus
}
#endif

#endif  // LR1110_BOOTLOADER_TYPES_H

/* --- EOF ------------------------------------------------------------------ */
