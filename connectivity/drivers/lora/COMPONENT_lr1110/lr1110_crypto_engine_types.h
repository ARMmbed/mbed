/*!
 * @file      lr1110_crypto_engine_types.h
 *
 * @brief     Cryptographic engine driver types for LR1110
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

#ifndef LR1110_CRYPTO_ENGINE_TYPES_H
#define LR1110_CRYPTO_ENGINE_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

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
 * @brief Length in bytes of a MIC
 */
#define LR1110_CRYPTO_MIC_LENGTH 0x04

/*!
 * @brief Length in bytes of a AES CMAC
 */
#define LR1110_CRYPTO_AES_CMAC_LENGTH 0x10

/*!
 * @brief Maximum length in bytes of data to be encrypted / decrypted
 */
#define LR1110_CRYPTO_DATA_MAX_LENGTH 0x0100

/*!
 * @brief Length in bytes of a key for AES computation
 */
#define LR1110_CRYPTO_KEY_LENGTH 0x10

/*!
 * @brief Length in bytes of a nonce
 */
#define LR1110_CRYPTO_NONCE_LENGTH 0x10

/*!
 * @brief Length in bytes of a crypto parameter
 */
#define LR1110_CRYPTO_PARAMETER_LENGTH 0x04

/*!
 * @brief Length in bytes of the status returned by an API
 */
#define LR1110_CRYPTO_STATUS_LENGTH 0x01

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */

/*!
 * @brief Fixed-length array to store an AES CMAC
 */
typedef uint8_t lr1110_crypto_mic_t[LR1110_CRYPTO_MIC_LENGTH];

/*!
 * @brief Fixed-length array to store an AES CMAC
 */
typedef uint8_t lr1110_crypto_aes_cmac_t[LR1110_CRYPTO_AES_CMAC_LENGTH];

/*!
 * @brief Fixed-length array to store a crypto key
 */
typedef uint8_t lr1110_crypto_key_t[LR1110_CRYPTO_KEY_LENGTH];

/*!
 * @brief Fixed-length array to store a crypto nonce
 */
typedef uint8_t lr1110_crypto_nonce_t[LR1110_CRYPTO_NONCE_LENGTH];

/*!
 * @brief Fixed-length array to store a crypto parameter
 */
typedef uint8_t lr1110_crypto_param_t[LR1110_CRYPTO_PARAMETER_LENGTH];

/*!
 * @brief The supported crypto elements
 */
typedef enum
{
    LR1110_CRYPTO_ELEMENT_CRYPTO_ENGINE  = 0x00,  //!< Internal crypto engine (default)
    LR1110_CRYPTO_ELEMENT_SECURE_ELEMENT = 0x01,  //!< External secure element
} lr1110_crypto_element_t;

/*!
 * @brief The status returned by the crypto API
 */
typedef enum
{
    LR1110_CRYPTO_STATUS_SUCCESS              = 0x00,  //!< The API command was successful
    LR1110_CRYPTO_STATUS_ERROR_FAIL_CMAC      = 0x01,  //!< AES-CMAC invalid or comparison failed
    LR1110_CRYPTO_STATUS_ERROR_INVALID_KEY_ID = 0x03,  //!< Invalid key ID (source, destination)
    LR1110_CRYPTO_STATUS_ERROR_BUFFER_SIZE    = 0x05,  //!< Invalid data buffer size
    LR1110_CRYPTO_STATUS_ERROR                = 0x06,  //!< Other error
} lr1110_crypto_status_t;

/*!
 * @brief The supported LoRaWAN versions
 */
typedef enum
{
    LR1110_CRYPTO_LORAWAN_VERSION_1_0_X = 0x00,
    LR1110_CRYPTO_LORAWAN_VERSION_1_1_X = 0x01,
} lr1110_crypto_lorawan_version_t;

/*!
 * @brief Crypto keys table index definition.
 */
typedef enum lr1110_crypto_keys_idx_e
{
    LR1110_CRYPTO_KEYS_IDX_MOTHER_KEY      = 1,
    LR1110_CRYPTO_KEYS_IDX_NWK_KEY         = 2,
    LR1110_CRYPTO_KEYS_IDX_APP_KEY         = 3,
    LR1110_CRYPTO_KEYS_IDX_J_S_ENC_KEY     = 4,
    LR1110_CRYPTO_KEYS_IDX_J_S_INT_KEY     = 5,
    LR1110_CRYPTO_KEYS_IDX_GP_KE_KEY_0     = 6,
    LR1110_CRYPTO_KEYS_IDX_GP_KE_KEY_1     = 7,
    LR1110_CRYPTO_KEYS_IDX_GP_KE_KEY_2     = 8,
    LR1110_CRYPTO_KEYS_IDX_GP_KE_KEY_3     = 9,
    LR1110_CRYPTO_KEYS_IDX_GP_KE_KEY_4     = 10,
    LR1110_CRYPTO_KEYS_IDX_GP_KE_KEY_5     = 11,
    LR1110_CRYPTO_KEYS_IDX_APP_S_KEY       = 12,
    LR1110_CRYPTO_KEYS_IDX_F_NWK_S_INT_KEY = 13,
    LR1110_CRYPTO_KEYS_IDX_S_NWK_S_INT_KEY = 14,
    LR1110_CRYPTO_KEYS_IDX_NWK_S_ENC_KEY   = 15,
    LR1110_CRYPTO_KEYS_IDX_RFU_0           = 16,
    LR1110_CRYPTO_KEYS_IDX_RFU_1           = 17,
    LR1110_CRYPTO_KEYS_IDX_MC_APP_S_KEY_0  = 18,
    LR1110_CRYPTO_KEYS_IDX_MC_APP_S_KEY_1  = 19,
    LR1110_CRYPTO_KEYS_IDX_MC_APP_S_KEY_2  = 20,
    LR1110_CRYPTO_KEYS_IDX_MC_APP_S_KEY_3  = 21,
    LR1110_CRYPTO_KEYS_IDX_MC_NWK_S_KEY_0  = 22,
    LR1110_CRYPTO_KEYS_IDX_MC_NWK_S_KEY_1  = 23,
    LR1110_CRYPTO_KEYS_IDX_MC_NWK_S_KEY_2  = 24,
    LR1110_CRYPTO_KEYS_IDX_MC_NWK_S_KEY_3  = 25,
    LR1110_CRYPTO_KEYS_IDX_GP0             = 26,
    LR1110_CRYPTO_KEYS_IDX_GP1             = 27,
} lr1110_crypto_keys_idx_t;

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

#ifdef __cplusplus
}
#endif

#endif  // LR1110_CRYPTO_ENGINE_TYPES_H

/* --- EOF ------------------------------------------------------------------ */
