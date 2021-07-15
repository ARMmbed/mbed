/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Crypto driver definition.
 *
 *  Copyright (c) 2018-2019 ARM Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2020 Packetcraft, Inc.
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*************************************************************************************************/

#ifndef PAL_CRYPTO_H
#define PAL_CRYPTO_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_CRYPTO
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      AES block size. */
#define PAL_CRYPTO_AES_BLOCK_SIZE                16

#define PAL_CRYPTO_LL_KEY_LEN                    16      /*!< Encryption key length. */
#define PAL_CRYPTO_LL_IV_LEN                     8       /*!< Initialization vector length. */
#define PAL_CRYPTO_LL_DATA_MIC_LEN               4       /*!< Data channel PDU MIC length. */

/*! \brief      CCM-Mode algorithm lengths. */
#define SEC_CCM_KEY_LEN                          16

/*! \brief      CCM-Mode algorithm maximum additional length. */
#define SEC_CCM_MAX_ADDITIONAL_LEN               ((1<<16) - (1<<8))

/*! \brief      CCM-Mode algorithm length. */
#define SEC_CCM_L                                2

/*! \brief      CCM-Mode algorithm nonce length. */
#define SEC_CCM_NONCE_LEN                        (15-SEC_CCM_L)

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Operational states. */
typedef enum
{
  PAL_CRYPTO_STATE_UNINIT = 0,      /*!< Uninitialized state. */
  PAL_CRYPTO_STATE_ERROR  = 0,      /*!< Error state. */
  PAL_CRYPTO_STATE_READY            /*!< Ready state. */
} PalCryptoState_t;

/*! \brief      Encryption data. */
typedef struct
{
  /* SK placed here for 32-bit alignment. */
  uint8_t       sk[PAL_CRYPTO_LL_KEY_LEN]; /*!< Session/Encryption key. */
  uint8_t       iv[PAL_CRYPTO_LL_IV_LEN];  /*!< Initialization vector. */
  bool_t        enaEncrypt;         /*!< Tx/Encryption enabled flag. */
  bool_t        enaDecrypt;         /*!< Rx/Decryption enabled flag. */
  bool_t        enaAuth;            /*!< Enable authentication. */
  uint8_t       nonceMode;          /*!< Nonce mode. */
  uint16_t      *pEventCounter;     /*!< Connection event counter. */
  uint64_t      *pTxPktCounter;     /*!< Tx packet counter. Set when nonceMode = PAL_BB_NONCE_MODE_EXT64_CNTR. */
  uint64_t      *pRxPktCounter;     /*!< Rx packet counter. Set when nonceMode = PAL_BB_NONCE_MODE_EXT64_CNTR. */
  uint8_t       dir;                /*!< Direction value. */
  uint8_t       type;               /*!< Type, ACL, CIS, BIS */
  void          *pEncryptCtx;       /*!< Tx/Encryption context. */
  void          *pDecryptCtx;       /*!< Rx/Decryption context. */
} PalCryptoEnc_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void PalCryptoInit(void);
void PalCryptoDeInit(void);

/* Key generation */
void PalCryptoGenerateP256KeyPair(const uint8_t *pPrivKey, uint8_t *pPubKey);
void PalCryptoGenerateDhKey(const uint8_t *pPubKey, const uint8_t *pPrivKey, uint8_t *pDhKey);
bool_t PalCryptoValidatePublicKey(const uint8_t *pPubKey, bool_t generateKey);
void PalCryptoGenerateRandomNumber(uint8_t *pBuf, uint8_t len);

/* CCM */
uint32_t PalCryptoCcmDec(const uint8_t *pKey, uint8_t *pNonce, uint8_t *pCypherText, uint16_t textLen,
                         uint8_t *pClear, uint16_t clearLen, uint8_t *pMic, uint8_t micLen,
                         uint8_t *pResult, uint8_t handlerId, uint16_t param, uint8_t event);
void PalCryptoCcmEnc(const uint8_t *pKey, uint8_t *pNonce, uint8_t *pPlainText, uint16_t textLen,
              uint8_t *pClear, uint16_t clearLen, uint8_t micLen, uint8_t *pResult,
              uint8_t handlerId, uint16_t param, uint8_t event);

/* Crypto AES */
void PalCryptoAesEcb(const uint8_t *pKey, uint8_t *pOut, const uint8_t *pIn);
void PalCryptoAesCmac(const uint8_t *pKey, uint8_t *pOut, const uint8_t *pIn, uint16_t len);
void PalCryptoAesEnable(PalCryptoEnc_t *pEnc, uint8_t id, uint8_t localDir);
bool_t PalCryptoAesCcmEncrypt(PalCryptoEnc_t *pEnc, uint8_t *pHdr, uint8_t *pBuf, uint8_t *pMic);
bool_t PalCryptoAesCcmDecrypt(PalCryptoEnc_t *pEnc, uint8_t *pBuf);

/*! \} */    /* PAL_CRYPTO */

#ifdef __cplusplus
};
#endif

#endif /* PAL_CRYPTO_H */
