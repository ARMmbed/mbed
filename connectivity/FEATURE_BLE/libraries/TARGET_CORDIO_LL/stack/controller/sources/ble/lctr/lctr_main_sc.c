/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller secure connections implementation file.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2021 Packetcraft, Inc.
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

#include "lctr_int.h"
#include "lctr_api.h"
#include "lctr_api_sc.h"
#include "lmgr_api.h"
#include "lmgr_api_sc.h"
#include "wsf_assert.h"
#include "ll_math.h"
#include "wsf_trace.h"
#include "pal_bb_ble.h"
#include <string.h>

#if ENABLE_UECC
  #include "uECC_ll.h"
#endif

#if ENABLE_UECC_TINYCRYPT
  #include "tinycrypt/ecc.h"
  #include "tinycrypt/ecc_dh.h"
#endif

/*************************************************************************************************/
/*!
 *  \brief      Baseband driver ECC service function.
 *
 *  \param      op      Operation to service.
 */
/*************************************************************************************************/
static void lctrScBbDrvEcc(uint8_t op)
{
  switch (op)
  {
    case LL_MATH_ECC_OP_GENERATE_P256_KEY_PAIR:
      WsfSetEvent(lmgrPersistCb.handlerId, (1 << LCTR_EVENT_SC_GENERATE_P256_KEY_PAIR));
      break;

    case LL_MATH_ECC_OP_GENERATE_DH_KEY:
      WsfSetEvent(lmgrPersistCb.handlerId, (1 << LCTR_EVENT_SC_GENERATE_DHKEY));
      break;

    default:
      WSF_ASSERT(FALSE);
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Random number generator used by uECC.
 *
 *  \param  pDest       Buffer to hold random number
 *  \param  size        Size of pDest in bytes .
 *
 *  \return TRUE if successful.
 */
/*************************************************************************************************/
static int lctrRng(uint8_t *pDest, unsigned size)
{
  PalCryptoGenerateRandomNumber(pDest, size);

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Copy octets to another buffer, reversing order.
 *
 *  \param  pDest   Destination buffer.
 *  \param  pSrc    Source buffer.
 *  \param  len     Length of data, in octets.
 */
/*************************************************************************************************/
static void lctrReverseCopy(uint8_t *pDest, const uint8_t *pSrc, uint16_t len)
{
  pSrc += len - 1;
  while (len-- > 0)
  {
    *pDest++ = *pSrc--;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Reverse order of octets in buffer.
 *
 *  \param  p       Buffer.
 *  \param  len     Length of data, in octets.
 */
/*************************************************************************************************/
static void lctrReverse(uint8_t *p, uint16_t len)
{
  uint8_t *pStart = p;
  uint8_t *pEnd = p + len - 1;
  uint8_t temp;

  len /= 2;
  while (len-- > 0)
  {
    temp      = *pStart;
    *pStart++ = *pEnd;
    *pEnd--   = temp;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of key generation.
 *
 *  \param      pDhKey    Diffie-Hellman key.
 */
/*************************************************************************************************/
static void lctrNotifyGenerateDhKeyInd(const uint8_t *pDhKey)
{
  LlGenerateDhKeyInd_t evt =
  {
    .hdr =
    {
      .param  = 0,
      .event  = LL_GENERATE_DHKEY_CMPL_IND,
      .status = LL_SUCCESS
    }
  };

  evt.status = lmgrScCb.pubKeyValid ? LL_SUCCESS : LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  memcpy(evt.dhKey, pDhKey, sizeof(evt.dhKey));

  LL_TRACE_INFO0("### LlEvent ###  LL_GENERATE_DHKEY_CMPL_IND, status=LL_SUCCESS");

  LmgrSendEvent((LlEvt_t *)&evt);
}

#if ENABLE_UECC
/*************************************************************************************************/
/*!
 *  \brief  Start generating P-256 key pair.
 */
/*************************************************************************************************/
void lctrGenerateP256KeyPairStart(void)
{
  uint8_t privKey[LL_ECC_KEY_LEN];

  /* Generate private key. */
  lctrRng(privKey, sizeof(privKey));

  /* Start public key generation. */
  uECC_set_rng_ll(lctrRng);
  uECC_make_key_start(privKey);

  lctrScBbDrvEcc(LL_MATH_ECC_OP_GENERATE_P256_KEY_PAIR);
}

/*************************************************************************************************/
/*!
 *  \brief  Start generating P-256 public key with a specified private key.
 *
 *  \param  pPrivKey      Private key.
 */
/*************************************************************************************************/
void lctrGenerateP256PublicKeyStart(const uint8_t *pPrivKey)
{
  uint8_t privKey[LL_ECC_KEY_LEN];

  /* Reverse private key (to big endian). */
  lctrReverseCopy(privKey, pPrivKey, LL_ECC_KEY_LEN);

  /* Start public key generation. */
  uECC_set_rng_ll(lctrRng);
  uECC_make_key_start(privKey);

  lctrScBbDrvEcc(LL_MATH_ECC_OP_GENERATE_P256_KEY_PAIR);
}

/*************************************************************************************************/
/*!
 *  \brief  Continue generating P-256 key pair.
 *
 *  \return TRUE if key generation complete.
 */
/*************************************************************************************************/
bool_t lctrGenerateP256KeyPairContinue(void)
{
  if (uECC_make_key_continue())
  {
    return TRUE;
  }

  lctrScBbDrvEcc(LL_MATH_ECC_OP_GENERATE_P256_KEY_PAIR);
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Get results from generating P-256 key pair.
 *
 *  \param  pPubKey     Storage for public key.
 *  \param  pPrivKey    Storage for private key.
 */
/*************************************************************************************************/
void lctrGenerateP256KeyPairComplete(uint8_t *pPubKey, uint8_t *pPrivKey)
{
  /* Complete key generation. */
  uECC_make_key_complete(pPubKey, pPrivKey);

  /* Reverse keys (to little endian). */
  lctrReverse(pPubKey, LL_ECC_KEY_LEN);
  lctrReverse(pPubKey + LL_ECC_KEY_LEN, LL_ECC_KEY_LEN);
  lctrReverse(pPrivKey, LL_ECC_KEY_LEN);
}
#endif

/*************************************************************************************************/
/*!
 *  \brief  Start generating Diffie-Hellman key.
 *
 *  \param  pPubKey     Public key.
 *  \param  pPrivKey    Private key.
 */
/*************************************************************************************************/
void lctrGenerateDhKeyStart(const uint8_t *pPubKey, const uint8_t *pPrivKey)
{
  uint8_t pubKey[LL_ECC_KEY_LEN * 2];
  uint8_t privKey[LL_ECC_KEY_LEN];

  /* Reverse keys (to big endian). */
  lctrReverseCopy(pubKey, pPubKey, LL_ECC_KEY_LEN);
  lctrReverseCopy(pubKey + LL_ECC_KEY_LEN, pPubKey + LL_ECC_KEY_LEN, LL_ECC_KEY_LEN);
  lctrReverseCopy(privKey, pPrivKey, LL_ECC_KEY_LEN);

  /* Start shared secret generation. */
#if ENABLE_UECC
  uECC_set_rng_ll(lctrRng);
  uECC_shared_secret_start(pubKey, privKey);

  lctrScBbDrvEcc(LL_MATH_ECC_OP_GENERATE_DH_KEY);
#endif

#if ENABLE_UECC_TINYCRYPT
  uint8_t dhKey[LL_ECC_KEY_LEN];
  uECC_set_rng(lctrRng);
  const struct uECC_Curve_t* pCurve = uECC_secp256r1();
  uECC_shared_secret(pPubKey, pPrivKey, dhKey, pCurve);

  /* Reverse shared secret (to little endian). */
  lctrReverse(dhKey, LL_ECC_KEY_LEN);

  /* Notify host that the key was generated. */
  lctrNotifyGenerateDhKeyInd(dhKey);

  lmgrScCb.eccOpActive = FALSE;
#endif
}

#if ENABLE_UECC
/*************************************************************************************************/
/*!
 *  \brief  Continue generating Diffie-Hellman key.
 *
 *  \return TRUE if Diffie-Hellman key generation complete.
 */
/*************************************************************************************************/
bool_t lctrGenerateDhKeyContinue(void)
{
  if (uECC_shared_secret_continue())
  {
    return TRUE;
  }

  lctrScBbDrvEcc(LL_MATH_ECC_OP_GENERATE_DH_KEY);
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Get results from generating Diffie-Hellman key.
 *
 *  \param  pDhKey      Storage for Diffie-Hellman key.
 */
/*************************************************************************************************/
void lctrGenerateDhKeyComplete(uint8_t *pDhKey)
{
  /* Complete shared secret generation. */
  uECC_shared_secret_complete(pDhKey);

  /* Reverse shared secret (to little endian). */
  lctrReverse(pDhKey, LL_ECC_KEY_LEN);
}
#endif

/*************************************************************************************************/
/*!
 *  \brief  Validate public key.
 *
 *  \param  pPubKey      Public key.
 *  \param  generateKey  Generate DHKey event if true.
 *
 *  \return TRUE if public key is valid.
 */
/*************************************************************************************************/
bool_t lctrValidatePublicKey(const uint8_t *pPubKey, bool_t generateKey)
{
  bool_t pubKeyValid;

  uint8_t pubKey[LL_ECC_KEY_LEN * 2];
  /* Reverse keys (to big endian). */
  lctrReverseCopy(pubKey, pPubKey, LL_ECC_KEY_LEN);
  lctrReverseCopy(pubKey + LL_ECC_KEY_LEN, pPubKey + LL_ECC_KEY_LEN, LL_ECC_KEY_LEN);

#if ENABLE_UECC
  pubKeyValid = (bool_t)uECC_valid_public_key_ll(pubKey);
#endif

#if ENABLE_UECC_TINYCRYPT
  const struct uECC_Curve_t* pCurve = uECC_secp256r1();
  pubKeyValid = (bool_t)uECC_valid_public_key(pubKey, pCurve);
#endif

  if (!pubKeyValid && generateKey)
  {
    lctrScBbDrvEcc(LL_MATH_ECC_OP_GENERATE_DH_KEY);
  }

  return pubKeyValid;
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of key generation.
 *
 *  \param      pPubKey     Public key.
 */
/*************************************************************************************************/
static void lctrNotifyReadLocalP256PubKeyInd(const uint8_t *pPubKey)
{
  LlReadLocalP256PubKeyInd_t evt =
  {
    .hdr =
    {
      .param  = 0,
      .event  = LL_READ_LOCAL_P256_PUB_KEY_CMPL_IND,
      .status = LL_SUCCESS
    }
  };

  evt.status = LL_SUCCESS;
  memcpy(evt.pubKey_x, pPubKey, sizeof(evt.pubKey_x));
  memcpy(evt.pubKey_y, pPubKey + LL_ECC_KEY_LEN, sizeof(evt.pubKey_y));

  LL_TRACE_INFO0("### LlEvent ###  LL_READ_LOCAL_P256_PUB_KEY_CMPL_IND, status=LL_SUCCESS");

  LmgrSendEvent((LlEvt_t *)&evt);
}

#if ENABLE_UECC
/*************************************************************************************************/
/*!
 *  \brief      P-256 key pair generation.
 */
/*************************************************************************************************/
static void lctrScGenerateP256KeyPairContinue(void)
{
  if (lctrGenerateP256KeyPairContinue())
  {
    uint8_t pubKey[LL_ECC_KEY_LEN * 2];

    /* Set the newly-generated private key as the local key. */
    lctrGenerateP256KeyPairComplete(pubKey, lmgrScCb.privKey);

    /* Notify host that the key was generated. */
    lctrNotifyReadLocalP256PubKeyInd(pubKey);

    lmgrScCb.eccOpActive = FALSE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      DH Key generation.
 */
/*************************************************************************************************/
static void lctrScGenerateDhKeyContinue(void)
{
  if (!lmgrScCb.pubKeyValid)
  {
    uint8_t dhKeyInvalid[LL_ECC_KEY_LEN];

    memset(dhKeyInvalid, 0xFF, LL_ECC_KEY_LEN);

    lctrNotifyGenerateDhKeyInd(dhKeyInvalid);

    return;
  }

  if (lctrGenerateDhKeyContinue())
  {
    uint8_t dhKey[LL_ECC_KEY_LEN];

    lctrGenerateDhKeyComplete(dhKey);

    /* Notify host that the key was generated. */
    lctrNotifyGenerateDhKeyInd(dhKey);

    lmgrScCb.eccOpActive = FALSE;
  }
}
#endif

/*************************************************************************************************/
/*!
 *  \brief      Generate a P-256 public/private key pair.
 *
 *  \return     Status error code.
 *
 *  Generate a P-256 public/private key pair.  If another ECC operation (P-256 key pair generation
 *  or Diffie-Hellman key generation) is ongoing, an error will be returned.
 */
/*************************************************************************************************/
uint8_t LctrGenerateP256KeyPair(void)
{
  /* Allow only one key pair generation at a time. */
  if (lmgrScCb.eccOpActive)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  /* Start operation. */
  lmgrScCb.eccOpActive = TRUE;

#if ENABLE_UECC
  if (lmgrScCb.privKeySet)
  {
    lctrGenerateP256PublicKeyStart(lmgrScCb.privKey);
  }
  else
  {
    lctrGenerateP256KeyPairStart();
  }
#endif

#if ENABLE_UECC_TINYCRYPT
  uint8_t pubKey[LL_ECC_KEY_LEN * 2];
  uint8_t privKey[LL_ECC_KEY_LEN];

  if (lmgrScCb.privKeySet)
  {
    /* Reverse private key (to big endian). */
    lctrReverseCopy(privKey, lmgrScCb.privKey, LL_ECC_KEY_LEN);
  }
  else
  {
    /* Generate private key. */
    lctrRng(privKey, sizeof(privKey));
  }

  uECC_set_rng(lctrRng);
  const struct uECC_Curve_t* pCurve = uECC_secp256r1();
  uECC_make_key(pubKey, privKey, pCurve);

  /* Reverse keys (to little endian). */
  lctrReverse(pubKey, LL_ECC_KEY_LEN);
  lctrReverse(pubKey + LL_ECC_KEY_LEN, LL_ECC_KEY_LEN);
  lctrReverseCopy(lmgrScCb.privKey, privKey, LL_ECC_KEY_LEN);

  /* Notify host that the key was generated. */
  lctrNotifyReadLocalP256PubKeyInd(pubKey);

  lmgrScCb.eccOpActive = FALSE;
#endif

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Generate a Diffie-Hellman key.
 *
 *  \param      pPubKey     Public key.
 *  \param      pPrivKey    Private key.
 *
 *  \return     Status error code.
 *
 *  Generate a Diffie-Hellman key from a remote public key and the local private key.  If another
 *  ECC operation (P-256 key pair generation or Diffie-Hellman key generation) is ongoing, an error
 *  will be returned.
 */
/*************************************************************************************************/
uint8_t LctrGenerateDhKey(const uint8_t *pPubKey, const uint8_t *pPrivKey)
{
  /* Allow only one key pair generation at a time. */
  if (lmgrScCb.eccOpActive)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  bool_t generateKey = ((lmgrScCb.validatePubKeyMode == KEY_VALIDATE_MODE_ALT2) ? TRUE: FALSE);

  /* Check if public key is valid first. */
  lmgrScCb.pubKeyValid = lctrValidatePublicKey(pPubKey, generateKey);

  if (!lmgrScCb.pubKeyValid)
  {
    return (generateKey ? LL_SUCCESS: LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS);
  }

  /* Start operation. */
  lmgrScCb.eccOpActive = TRUE;
  lctrGenerateDhKeyStart(pPubKey, pPrivKey);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Generate a debug Diffie-Hellman key using the spec-defined debug keys.
 *
 *  \return     Status error code.
 *
 */
/*************************************************************************************************/
uint8_t LctrGenerateDebugDhKey()
{
  const uint8_t privKey[LL_ECC_KEY_LEN] = {
                                            0xbd, 0x1a, 0x3c, 0xcd,  0xa6, 0xb8, 0x99, 0x58,
                                            0x99, 0xb7, 0x40, 0xeb,  0x7b, 0x60, 0xff, 0x4a,
                                            0x50, 0x3f, 0x10, 0xd2,  0xe3, 0xb3, 0xc9, 0x74,
                                            0x38, 0x5f, 0xc5, 0xa3,  0xd4, 0xf6, 0x49, 0x3f
                                          };

  const uint8_t pubKey[LL_ECC_KEY_LEN * 2] = {
                                               /* pubDebugKey_x */
                                               0xe6, 0x9d, 0x35, 0x0e,  0x48, 0x01, 0x03, 0xcc,
                                               0xdb, 0xfd, 0xf4, 0xac,  0x11, 0x91, 0xf4, 0xef,
                                               0xb9, 0xa5, 0xf9, 0xe9,  0xa7, 0x83, 0x2c, 0x5e,
                                               0x2c, 0xbe, 0x97, 0xf2,  0xd2, 0x03, 0xb0, 0x20,
                                               /* pubDebuKey_y */
                                               0x8b, 0xd2, 0x89, 0x15,  0xd0, 0x8e, 0x1c, 0x74,
                                               0x24, 0x30, 0xed, 0x8f,  0xc2, 0x45, 0x63, 0x76,
                                               0x5c, 0x15, 0x52, 0x5a,  0xbf, 0x9a, 0x32, 0x63,
                                               0x6d, 0xeb, 0x2a, 0x65,  0x49, 0x9c, 0x80, 0xdc
                                             };

   /* Allow only one key pair generation at a time. */
  if (lmgrScCb.eccOpActive)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  /* Debug key is always valid. */
  lmgrScCb.pubKeyValid = TRUE;

  /* Start operation. */
  lmgrScCb.eccOpActive = TRUE;
  lctrGenerateDhKeyStart(pubKey, privKey);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set validate public key mode.
 *
 *  \param      validateMode   ALT1 or ALT2.
 *
 *  \return     Status error code.
 *
 */
/*************************************************************************************************/
uint8_t LctrSetValidatePublicKeyMode(uint8_t validateMode)
{
  lmgrScCb.validatePubKeyMode = validateMode;
  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for secure connections.
 */
/*************************************************************************************************/
void LctrScInit(void)
{
#if ENABLE_UECC
  lctrEventHdlrTbl[LCTR_EVENT_SC_GENERATE_P256_KEY_PAIR] = lctrScGenerateP256KeyPairContinue;
  lctrEventHdlrTbl[LCTR_EVENT_SC_GENERATE_DHKEY] = lctrScGenerateDhKeyContinue;
#endif

  lmgrPersistCb.featuresDefault |= LL_FEAT_REMOTE_PUB_KEY_VALIDATION;
  lmgrScCb.validatePubKeyMode = KEY_VALIDATE_MODE_ALT2;
}
