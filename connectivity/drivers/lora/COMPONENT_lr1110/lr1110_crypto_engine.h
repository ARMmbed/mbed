/*!
 * @file      lr1110_crypto_engine.h
 *
 * @brief     Cryptographic engine driver definition for LR1110
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

#ifndef LR1110_CRYPTO_ENGINE_H
#define LR1110_CRYPTO_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include "lr1110_crypto_engine_types.h"

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
 * @brief Select the crypto element to be used
 *
 * By default, the internal crypto engine is selected. It is not needed to call this command if one plans to use the
 * internal crypto engine.
 *
 * @param [in] context Chip implementation context
 * @param [in] element The type of crypto element to use
 *
 */
void lr1110_crypto_select( const void* context, const lr1110_crypto_element_t element );

/*!
 * @brief Set a key in the previously selected crypto element.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 * @param [in] key_id The identifier of the key to be set
 * @param [in] key The key to be set
 *
 * @see lr1110_crypto_derive_key
 */
void lr1110_crypto_set_key( const void* context, lr1110_crypto_status_t* status, const uint8_t key_id,
                            const lr1110_crypto_key_t key );

/*!
 * @brief Derive a key previously set.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 * @param [in] src_key_id The identifier of the key to be derived
 * @param [in] dest_key_id The identifier where the derived key will be stored after call to @ref
 * lr1110_crypto_store_to_flash
 * @param [in] nonce The nonce to be used to perform the derivation
 *
 * @see lr1110_crypto_set_key
 */
void lr1110_crypto_derive_key( const void* context, lr1110_crypto_status_t* status, const uint8_t src_key_id,
                               const uint8_t dest_key_id, const lr1110_crypto_nonce_t nonce );

/*!
 * @brief Perform the needed operations to extract the payload from a join accept message.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 * @param [in] dec_key_id The identifier of the key used for message decryption
 * @param [in] ver_key_id The identifier of the key used for MIC verification
 * @param [in] lorawan_version LoRaWAN version to know the size of the header
 * @param [in] header The header to compute (length linked to lorawan_version)
 * @param [in] data The data to compute
 * @param [in] length The length in bytes of the data to compute
 *
 * @param [out] data_out Placeholder for the decrypted data
 *
 */
void lr1110_crypto_process_join_accept( const void* context, lr1110_crypto_status_t* status, const uint8_t dec_key_id,
                                        const uint8_t ver_key_id, const lr1110_crypto_lorawan_version_t lorawan_version,
                                        const uint8_t* header, const uint8_t* data, const uint8_t length,
                                        uint8_t* data_out );

/*!
 * @brief Compute an AES CMAC.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 * @param [in] key_id The identifier of the keyused for the computation
 * @param [in] data The data to compute
 * @param [in] length The length in bytes of the data to compute
 * @param [out] mic Placeholder for the computed MIC (first 4 bytes of the CMAC)
 *
 * @see lr1110_crypto_verify_aes_cmac
 */
void lr1110_crypto_compute_aes_cmac( const void* context, lr1110_crypto_status_t* status, const uint8_t key_id,
                                     const uint8_t* data, const uint16_t length, lr1110_crypto_mic_t mic );

/*!
 * @brief Compute an AES CMAC and make a comparison with a value given as parameter.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 * @param [in] key_id The identifier of the key to be used for the computation
 * @param [in] data The data to compute
 * @param [in] length The length in bytes of the data to compute
 * @param [in] mic The MIC value (first 4 bytes of the CMAC) use for comparison
 *
 * @see lr1110_crypto_compute_aes_cmac
 */
void lr1110_crypto_verify_aes_cmac( const void* context, lr1110_crypto_status_t* status, const uint8_t key_id,
                                    const uint8_t* data, const uint16_t length, const lr1110_crypto_mic_t mic );

/*!
 * @brief Compute an AES encryption with a key ID specified in parameter.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 * @param [in] key_id The identifier of the key to be used for the computation
 * @param [in] data The data to encrypt
 * @param [in] length The length in bytes of the data to encrypt - this value shall be a multiple of 16
 * @param [out] result A pointer to a data buffer that will be filled with the encrypted data. Values of this buffer are
 * meaningful if and only if the return status is LR1110_CRYPTO_STATUS_SUCCESS
 *
 * @see lr1110_crypto_set_key, lr1110_crypto_derive_key
 */
void lr1110_crypto_aes_encrypt_01( const void* context, lr1110_crypto_status_t* status, const uint8_t key_id,
                                   const uint8_t* data, const uint16_t length, uint8_t* result );

/*!
 * @brief Compute an AES encryption with a key ID specified in parameter.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 * @param [in] key_id The identifier of the key to be used for the computation
 * @param [in] data The data to encrypt
 * @param [in] length The length in bytes of the data to encrypt - this value shall be a multiple of 16
 * @param [out] result A pointer to a data buffer that will be filled with the encrypted data. Values of this buffer are
 * meaningful if and only if the return status is LR1110_CRYPTO_STATUS_SUCCESS
 *
 * @see lr1110_crypto_set_key, lr1110_crypto_derive_key
 */
void lr1110_crypto_aes_encrypt( const void* context, lr1110_crypto_status_t* status, const uint8_t key_id,
                                const uint8_t* data, const uint16_t length, uint8_t* result );

/*!
 * @brief Compute an AES decryption with a key ID specified in parameter.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 * @param [in] key_id The identifier of the key to be used for the computation
 * @param [in] data The data to decrypt
 * @param [in] length The length in bytes of the data to decrypt - this value shall be a multiple of 16
 * @param [out] result A pointer to a data buffer that will be filled with the decrypted data. Values of this buffer are
 * meaningful if and only if the return status is LR1110_CRYPTO_STATUS_SUCCESS
 *
 * @see lr1110_crypto_set_key, lr1110_crypto_derive_key
 */
void lr1110_crypto_aes_decrypt( const void* context, lr1110_crypto_status_t* status, const uint8_t key_id,
                                const uint8_t* data, const uint16_t length, uint8_t* result );

/*!
 * @brief Store the crypto data (keys, parameters) from RAM into the flash memory.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 *
 * @see lr1110_crypto_restore_from_flash
 */
void lr1110_crypto_store_to_flash( const void* context, lr1110_crypto_status_t* status );

/*!
 * @brief Restore the crypto data (keys, parameters) from flash memory into RAM.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 *
 * @see lr1110_crypto_store_to_flash
 */
void lr1110_crypto_restore_from_flash( const void* context, lr1110_crypto_status_t* status );

/*!
 * @brief Set a specific parameter identified by param_id in the crypto RAM.
 *
 * This function does not store a parameter in the flash memory. The parameters shall be stored after using @ref
 * lr1110_crypto_store_to_flash command.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 * @param [in] param_id The identifier of the parameter to be set
 * @param [in] parameter The parameter to be set
 *
 * @see lr1110_crypto_get_parameter
 */
void lr1110_crypto_set_parameter( const void* context, lr1110_crypto_status_t* status, const uint8_t param_id,
                                  const lr1110_crypto_param_t parameter );

/*!
 * @brief Get a specific parameter identified by paramID from the crypto RAM.
 *
 * This function does not fetch a parameter from the flash memory. The parameters shall be restored before using @ref
 * lr1110_crypto_restore_from_flash command.
 *
 * @param [in] context Chip implementation context
 * @param [out] status The status returned by the execution of this cryptographic function
 * @param [in] param_id The identifier of the parameter to get
 * @param [out] parameter The placeholder to store the parameter
 *
 * @see lr1110_crypto_set_parameter
 */
void lr1110_crypto_get_parameter( const void* context, lr1110_crypto_status_t* status, const uint8_t param_id,
                                  lr1110_crypto_param_t parameter );

#ifdef __cplusplus
}
#endif

#endif  // LR1110_CRYPTO_ENGINE_H

/* --- EOF ------------------------------------------------------------------ */
