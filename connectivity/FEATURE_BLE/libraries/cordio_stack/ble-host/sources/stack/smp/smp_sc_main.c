/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  SMP Secure Connections main module and utility functions.
 *
 *  Copyright (c) 2010-2019 Arm Ltd. All Rights Reserved.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_trace.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "sec_api.h"
#include "wsf_buf.h"
#include "util/bstream.h"
#include "util/calc128.h"
#include "util/wstr.h"
#include "dm_api.h"
#include "smp_api.h"
#include "smp_defs.h"
#include "smp_main.h"
#include "smp_sc_main.h"
#include "smp_handler.h"

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* LE Secure Connections Control Block (TBD: Make these dynamic) */
smpScCcb_t SMP_ScCcb[DM_CONN_MAX];

/*************************************************************************************************/
/*!
 *  \brief  Allocate LESC Scratch buffers
 *
 *  \param  pCcb      SMP control block.
 *
 *  \return TRUE if success, else FALSE.
 */
/*************************************************************************************************/
bool_t SmpScAllocScratchBuffers(smpCcb_t *pCcb)
{
  if (pCcb->pScCcb->pScratch == NULL)
  {
    pCcb->pScCcb->pScratch = WsfBufAlloc(sizeof(smpScScratch_t));
  }

  if (pCcb->pScCcb->pPeerPublicKey == NULL)
  {
    pCcb->pScCcb->pPeerPublicKey = WsfBufAlloc(sizeof(smpScPubKey_t));
  }

  if (pCcb->pScCcb->pLtk == NULL)
  {
    pCcb->pScCcb->pLtk = WsfBufAlloc(sizeof(smpScLtk_t));
  }

  if (pCcb->pScCcb->pLocalPublicKey == NULL)
  {
    pCcb->pScCcb->pLocalPublicKey = WsfBufAlloc(sizeof(smpScPubKey_t));
  }

  if (pCcb->pScCcb->pPrivateKey == NULL)
  {
    pCcb->pScCcb->pPrivateKey = WsfBufAlloc(SMP_PRIVATE_KEY_LEN);
  }

  if (pCcb->pScCcb->pScratch && pCcb->pScCcb->pPeerPublicKey && pCcb->pScCcb->pLtk &&
      pCcb->pScCcb->pLocalPublicKey && pCcb->pScCcb->pPrivateKey)
  {
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Free LESC Scratch buffers
 *
 *  \param  pCcb      SMP control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SmpScFreeScratchBuffers(smpCcb_t *pCcb)
{
  /* free scratch buffer */
  if (pCcb->pScCcb->pScratch != NULL)
  {
    WsfBufFree(pCcb->pScCcb->pScratch);
    pCcb->pScCcb->pScratch = NULL;
  }

  /* free scratch peer public key buffer */
  if (pCcb->pScCcb->pPeerPublicKey != NULL)
  {
    WsfBufFree(pCcb->pScCcb->pPeerPublicKey);
    pCcb->pScCcb->pPeerPublicKey = NULL;
  }

  /* free scratch LTK buffer */
  if (pCcb->pScCcb->pLtk != NULL)
  {
    WsfBufFree(pCcb->pScCcb->pLtk);
    pCcb->pScCcb->pLtk = NULL;
  }

  /* free scratch local public key buffer */
  if (pCcb->pScCcb->pLocalPublicKey != NULL)
  {
    WsfBufFree(pCcb->pScCcb->pLocalPublicKey);
    pCcb->pScCcb->pLocalPublicKey = NULL;
  }

  /* free scratch private key buffer */
  if (pCcb->pScCcb->pPrivateKey != NULL)
  {
    WsfBufFree(pCcb->pScCcb->pPrivateKey);
    pCcb->pScCcb->pPrivateKey = NULL;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Perform CMAC calculation or send SMP failure
 *
 *  \param  pKey      CMAC Key
 *  \param  pText     Pointer to message text.
 *  \param  textLen   Length of pText in bytes.
 *  \param  pCcb      SMP control block.
 *  \param  pMsg      WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SmpScCmac(const uint8_t *pKey, uint8_t *pText, uint8_t textLen, smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  if (SecCmac(pKey, pText, textLen, smpCb.handlerId, pCcb->connId, SMP_MSG_WSF_CMAC_CMPL) == FALSE)
  {
    WsfBufFree(pText);
    pMsg->hdr.status = SMP_ERR_UNSPECIFIED;
    pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a buffer or send SMP failure
 *
 *  \param  size    Size of buffer to allocate.
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return Allocated buffer.
 */
/*************************************************************************************************/
uint8_t *SmpScAlloc(uint8_t size, smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pBuf = WsfBufAlloc(size);

  if (pBuf == NULL)
  {
    pMsg->hdr.status = SMP_ERR_UNSPECIFIED;
    pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, pMsg);
  }

  return pBuf;
}

/*************************************************************************************************/
/*!
 *  \brief  Free a buffer allocated with SmpScAlloc.
 *
 *  \param  pBuf    Pointer to buffer to free.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SmpScFree(uint8_t *pBuf)
{
  WsfBufFree(pBuf);
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate cryptographic toolkit function F4.
 *
 *  \param  pCcb    SMP control block
 *  \param  pMsg    WSF message.
 *  \param  pU      Pointer to U parameter of F4 function.
 *  \param  pV      Pointer to V parameter of F4 function.
 *  \param  z       Byte with z parameter of F4 function.
 *  \param  pX      Pointer to X parameter of F4 function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SmpScCalcF4(smpCcb_t *pCcb, smpMsg_t *pMsg, uint8_t *pU, uint8_t *pV, uint8_t z, uint8_t *pX)
{
  uint8_t *pCmacText;

  /* f4(pU, pV, pX, z) where f4(U, V, x, Z) = AES-CMACx (U || V || Z) */
  if ((pCmacText = SmpScAlloc(SMP_F4_TEXT_LEN, pCcb, pMsg)) !=  NULL)
  {
    uint8_t *pCatBuf = pCmacText;

    /* Concatinate pU, pV, z */
    pCatBuf = SmpScCat(pCatBuf, pU, SMP_PUB_KEY_LEN);
    pCatBuf = SmpScCat(pCatBuf, pV, SMP_PUB_KEY_LEN);
    *pCatBuf = z;

    /* Execute CMAC with Nb as the key */
    SmpScCmac(pX, pCmacText, SMP_F4_TEXT_LEN, pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize SMP for LESC security.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SmpScInit()
{
  uint8_t i;

  for (i=0; i<DM_CONN_MAX; i++)
  {
    smpCb.ccb[i].pScCcb = &SMP_ScCcb[i];
  }

  smpCb.procPairing = smpScProcPairing;
  smpCb.procAuthReq = smpScAuthReq;
  smpCb.lescSupported = TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Concatinate a buffer and return a pointer to the next byte after concatination.
 *
 *  \param  pDst   Pointer to destination.
 *  \param  pSrc   Pointer to source buffer.
 *  \param  len    Length of pSrc in bytes.
 *
 *  \return Pointer to next byte after concatination.
 */
/*************************************************************************************************/
uint8_t *SmpScCat(uint8_t *pDst, const uint8_t *pSrc, uint8_t len)
{
  memcpy(pDst, pSrc, len);
  return pDst + len;
}

/*************************************************************************************************/
/*!
 *  \brief  Concatinate a 128 bit buffer and return a pointer to the next byte after concatination.
 *
 *  \param  pDst   Pointer to destination.
 *  \param  pSrc   Pointer to source buffer.
 *
 *  \return Pointer to next byte after concatination.
 */
/*************************************************************************************************/
uint8_t *SmpScCat128(uint8_t *pDst, uint8_t *pSrc)
{
  Calc128Cpy(pDst, pSrc);
  return pDst + 16;
}

/*************************************************************************************************/
/*!
 *  \brief  Send public key to peer.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScSendPubKey(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Transmit the public key */
  uint8_t   *pPkt;
  uint8_t   *p;

  /* set connection busy */
  DmConnSetIdle(pCcb->connId, DM_IDLE_SMP_PAIR, DM_CONN_BUSY);

  /* start smp response timer */
  smpStartRspTimer(pCcb);

  /* allocate packet buffer */
  if ((pPkt = smpMsgAlloc(SMP_PUB_KEY_MSG_LEN + L2C_PAYLOAD_START)) != NULL)
  {
    /* build packet */
    p = pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, SMP_CMD_PUBLIC_KEY);

    /* Store Public Key X data in LSB first format */
    WStrReverseCpy(p,  pCcb->pScCcb->pLocalPublicKey->pubKeyX, SMP_PUB_KEY_LEN);

    /* Store Public Key Y data in LSB first format */
    WStrReverseCpy(p+SMP_PUB_KEY_LEN, pCcb->pScCcb->pLocalPublicKey->pubKeyY, SMP_PUB_KEY_LEN);

    /* send packet */
    smpSendPkt(pCcb, pPkt);
  }
  else
  {
    pMsg->hdr.status = SMP_ERR_UNSPECIFIED;
    pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send the DH Key check command to the peer.
 *
 *  \param  pCcb    Connection control block.
 *  \param  pMsg    State machine message.
 *  \param  pCheck  Check data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScSendDHKeyCheck(smpCcb_t *pCcb, smpMsg_t *pMsg, uint8_t *pCheck)
{
  uint8_t   *pPkt;
  uint8_t   *p;

  /* set connection busy */
  DmConnSetIdle(pCcb->connId, DM_IDLE_SMP_PAIR, DM_CONN_BUSY);

  /* start smp response timer */
  smpStartRspTimer(pCcb);

  /* allocate packet buffer */
  if ((pPkt = smpMsgAlloc(SMP_DHKEY_CHECK_MSG_LEN + L2C_PAYLOAD_START)) != NULL)
  {
    /* build packet */
    p = pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, SMP_CMD_DHKEY_CHECK);

    /* DH Key Check data is result of last CMAC operation (LSB first) */
    WStrReverseCpy(p, pCheck, SMP_DHKEY_CHECK_LEN);

    /* send packet */
    smpSendPkt(pCcb, pPkt);
  }
  else
  {
    pMsg->hdr.status = SMP_ERR_UNSPECIFIED;
    pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send a Pair Rand command to the peer
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *  \param  pRand Pointer to rand data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg, uint8_t *pRand)
{
  /* Transmit the Pair Rand */
  uint8_t   *pPkt;
  uint8_t   *p;

  /* set connection busy */
  DmConnSetIdle(pCcb->connId, DM_IDLE_SMP_PAIR, DM_CONN_BUSY);

  /* start smp response timer */
  smpStartRspTimer(pCcb);

  /* allocate packet buffer */
  if ((pPkt = smpMsgAlloc(SMP_PAIR_RAND_LEN + L2C_PAYLOAD_START)) != NULL)
  {
    /* build packet */
    p = pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, SMP_CMD_PAIR_RAND);

    /* Store Random data (LSB first) */
    WStrReverseCpy(p, pRand, SMP_RAND_LEN);

    /* send packet */
    smpSendPkt(pCcb, pPkt);
  }
  else
  {
    pMsg->hdr.status = SMP_ERR_UNSPECIFIED;
    pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send a pairing confirm packet to the peer
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *  \param  pCnf  Point to confirm data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScSendPairCnf(smpCcb_t *pCcb, smpMsg_t *pMsg, uint8_t *pCnf)
{
  uint8_t   *pPkt;
  uint8_t   *p;

  /* set connection busy */
  DmConnSetIdle(pCcb->connId, DM_IDLE_SMP_PAIR, DM_CONN_BUSY);

  /* start smp response timer */
  smpStartRspTimer(pCcb);

  /* allocate packet buffer */
  if ((pPkt = smpMsgAlloc(SMP_PAIR_CNF_LEN + L2C_PAYLOAD_START)) != NULL)
  {
    /* build packet */
    p = pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, SMP_CMD_PAIR_CNF);

    /* Store Confirm data (LSB first) */
    WStrReverseCpy(p, pCnf, SMP_CONFIRM_LEN);

    /* send packet */
    smpSendPkt(pCcb, pPkt);
  }
  else
  {
    pMsg->hdr.status = SMP_ERR_UNSPECIFIED;
    pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Get the next bit used in Passkey calculations.
 *
 *  \param  pCcb  Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
uint8_t smpGetPkBit(smpCcb_t *pCcb)
{
  smpScCcb_t *pScCb = pCcb->pScCcb;
  uint8_t indx = (SMP_RAND_LEN - 1) - pScCb->pkPos / 8;
  uint8_t bit = pScCb->pkPos % 8;

  if (pScCb->pScratch->Ra[indx] & 1<<bit)
    return 0x81;

  return 0x80;
}

/*************************************************************************************************/
/*!
 *  \brief  Get a pointer to the connection's Peer Public Key for LESC pairing
 *
 *  \param  connId  Connection ID.
 *
 *  \return Peer public Key.
 */
/*************************************************************************************************/
smpScPubKey_t *smpGetPeerPublicKey(dmConnId_t connId)
{
  smpCcb_t *pCcb = smpCcbByConnId(connId);

  if (pCcb->pScCcb)
  {
    return pCcb->pScCcb->pPeerPublicKey;
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Set the connection's Peer Public Key for LESC pairing
 *
 *  \param  connId  Connection ID.
 *  \param  pKey    A Pointer to the peer's public key.
 *
 *  \return none.
 */
/*************************************************************************************************/
void smpSetPeerPublicKey(dmConnId_t connId, smpScPubKey_t *pKey)
{
  smpCcb_t *pCcb = smpCcbByConnId(connId);

  if (pCcb->pScCcb)
  {
    memcpy(pCcb->pScCcb->pPeerPublicKey, pKey, sizeof(smpScPubKey_t));
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the OOB Confirm value
 *
 *  \param  connId  Connection ID.
 *  \param  pCnf    OOB Configuration.
 *
 *  \return none.
 */
/*************************************************************************************************/
void SmpScSetOobCfg(dmConnId_t connId, dmSecLescOobCfg_t *pConfig)
{
  smpCcb_t *pCcb = smpCcbByConnId(connId);

  WSF_ASSERT(pCcb->pScCcb->pScratch);

  SMP_TRACE_128("OOB Peer Confirm", pConfig->peerConfirm);
  SMP_TRACE_128("OOB Peer Random", pConfig->peerRandom);
  SMP_TRACE_128("OOB Local Confirm", pConfig->localConfirm);
  SMP_TRACE_128("OOB Local Random", pConfig->localRandom);

  if (pCcb->initiator)
  {
    Calc128Cpy(pCcb->pScCcb->pScratch->PeerCa_Ea, pConfig->localConfirm);
    Calc128Cpy(pCcb->pScCcb->pScratch->Ra, pConfig->localRandom);
    Calc128Cpy(pCcb->pScCcb->pScratch->PeerCb, pConfig->peerConfirm);
    Calc128Cpy(pCcb->pScCcb->pScratch->Rb, pConfig->peerRandom);
  }
  else
  {
    Calc128Cpy(pCcb->pScCcb->pScratch->PeerCb, pConfig->localConfirm);
    Calc128Cpy(pCcb->pScCcb->pScratch->Rb, pConfig->localRandom);
    Calc128Cpy(pCcb->pScCcb->pScratch->PeerCa_Ea, pConfig->peerConfirm);
    Calc128Cpy(pCcb->pScCcb->pScratch->Ra, pConfig->peerRandom);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Format a cancel message with consideration for the attempts counter
 *
 *  \param  connId  Connection Id.
 *  \param  pHdr    Pointer to header of message to fill.
 *  \param  status  Status to include.
 *
 *  \return none.
 */
/*************************************************************************************************/
void SmpScGetCancelMsgWithReattempt(dmConnId_t connId, wsfMsgHdr_t *pHdr, uint8_t status)
{
  smpCcb_t *pCcb = smpCcbByConnId(connId);

  SMP_TRACE_INFO1("SmpScGetCancelMsgWithReattempt: %d", pCcb->attempts);

  /* update repeated attempts count */
  pCcb->attempts++;

  pHdr->param = connId;
  pHdr->status = status;

  SmpDbPairingFailed(connId);

  if (pCcb->attempts == pSmpCfg->maxAttempts)
  {
    /* max attempts reached */
    pHdr->event = SMP_MSG_INT_MAX_ATTEMPTS;
  }
  else
  {
    /* else just fail */
    pHdr->event = SMP_MSG_API_CANCEL_REQ;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process failure and check attempt count
 *
 *  \param  pCcb  Connection control block.
 *  \param  status  Status to include.
 *
 *  \return none.
 */
/*************************************************************************************************/
void smpScFailWithReattempt(smpCcb_t *pCcb, uint8_t status)
{
  wsfMsgHdr_t hdr;

  SmpScGetCancelMsgWithReattempt(pCcb->connId, &hdr, status);

  smpSmExecute(pCcb, (smpMsg_t *)&hdr);
}

/*************************************************************************************************/
/*!
 *  \brief  Convert event into string for diagnostics.
 *
 *  \param  eventId  Event ID
 *
 *  \return Event string.
 */
/*************************************************************************************************/
uint8_t *smpEventStr(uint8_t eventId)
{
  switch(eventId)
  {
    case SMP_MSG_API_PAIR_REQ: return (uint8_t*) "API_PAIR_REQ";
    case SMP_MSG_API_PAIR_RSP: return (uint8_t*) "API_PAIR_RSP";
    case SMP_MSG_API_CANCEL_REQ: return (uint8_t*) "API_CANCEL_REQ";
    case SMP_MSG_API_AUTH_RSP: return (uint8_t*) "API_AUTH_RSP";
    case SMP_MSG_API_SECURITY_REQ: return (uint8_t*) "API_SECURITY_REQ";
    case SMP_MSG_CMD_PKT: return (uint8_t*) "CMD_PKT";
    case SMP_MSG_CMD_PAIRING_FAILED: return (uint8_t*) "CMD_PAIRING_FAILED";
    case SMP_MSG_DM_ENCRYPT_CMPL: return (uint8_t*) "DM_ENCRYPT_CMPL";
    case SMP_MSG_DM_ENCRYPT_FAILED: return (uint8_t*) "DM_ENCRYPT_FAILED";
    case SMP_MSG_DM_CONN_CLOSE: return (uint8_t*) "DM_CONN_CLOSE";
    case SMP_MSG_WSF_AES_CMPL: return (uint8_t*) "WSF_AES_CMPL";
    case SMP_MSG_INT_SEND_NEXT_KEY: return (uint8_t*) "INT_SEND_NEXT_KEY";
    case SMP_MSG_INT_MAX_ATTEMPTS: return (uint8_t*) "INT_MAX_ATTEMPTS";
    case SMP_MSG_INT_PAIRING_CMPL: return (uint8_t*) "INT_PAIRING_CMPL";
    case SMP_MSG_INT_RSP_TIMEOUT: return (uint8_t*) "INT_RSP_TIMEOUT";
    case SMP_MSG_INT_WI_TIMEOUT: return (uint8_t*) "INT_WI_TIMEOUT";
    case SMP_MSG_INT_LESC: return (uint8_t*) "INT_LESC";
    case SMP_MSG_INT_LEGACY: return (uint8_t*) "INT_LEGACY";
    case SMP_MSG_INT_JW_NC: return (uint8_t*) "INT_JW_NC";
    case SMP_MSG_INT_PASSKEY: return (uint8_t*) "INT_PASSKEY";
    case SMP_MSG_INT_OOB: return (uint8_t*) "INT_OOB";
    case SMP_MSG_API_USER_CONFIRM: return (uint8_t*) "API_USER_CONFIRM";
    case SMP_MSG_API_USER_KEYPRESS: return (uint8_t*) "API_USER_KEYPRESS";
    case SMP_MSG_API_KEYPRESS_CMPL: return (uint8_t*) "API_KEYPRESS_CMPL";
    case SMP_MSG_WSF_ECC_CMPL: return (uint8_t*) "WSF_ECC_CMPL";
    case SMP_MSG_INT_PK_NEXT: return (uint8_t*) "INT_PK_NEXT";
    case SMP_MSG_INT_PK_CMPL: return (uint8_t*) "INT_PK_CMPL";
    case SMP_MSG_WSF_CMAC_CMPL: return (uint8_t*) "WSF_CMAC_CMPL";
    case SMP_MSG_DH_CHECK_FAILURE: return (uint8_t*) "DH_CHECK_FAILURE";
    case SMP_MSG_INT_CLEANUP: return (uint8_t*) "INT_CLEANUP";

    default: return (uint8_t*) "Unknown";
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Convert state into string for diagnostics.
 *
 *  \param  state   State ID
 *
 *  \return State string.
 */
/*************************************************************************************************/
uint8_t *smpStateStr(uint8_t state)
{
  uint8_t initiator = smpCb.ccb[0].initiator;

  if (initiator)
  {
    return smpiStateStr(state);
  }

  return smprStateStr(state);
}

/*************************************************************************************************/
/*!
 *  \brief  Write an array of bytes to the log.
 *
 *  \param  str       Prefix string printed before the byte array
 *  \param  pArray    Array of bytes
 *  \param  len       Length of pArray in bytes
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpLogByteArray(char *str, uint8_t *pArray, uint8_t len)
{
#if WSF_TOKEN_ENABLED == TRUE || WSF_TRACE_ENABLED == TRUE

  char buffer[512];
  int i, j=0, pos=0;

  SMP_TRACE_INFO0(str);

  while (j < len)
  {
    int count = 16;

    if (len-j < count)
      count = j;

    buffer[pos++] = '[';

    for (i=0; i<count; i++, j++)
    {
      uint8_t quad;

      if (i && i % 4 == 0)
        buffer[pos++] = ' ';

      quad = (pArray[j] >> 4) & 0xf;

      if (quad < 10)
        buffer[pos++] = '0' + quad;
      else
        buffer[pos++] = 'a' + quad - 10;

      quad = pArray[j] & 0xf;

      if (quad < 10)
        buffer[pos++] = '0' + quad;
      else
        buffer[pos++] = 'a' + quad - 10;
    }

    buffer[pos++] = ']';
    buffer[pos++] = '\0';
    SMP_TRACE_INFO0(buffer);
    pos = 0;
  }

  if (pos)
  {
    buffer[pos++] = ']';
    buffer[pos++] = '\0';

    SMP_TRACE_INFO0(buffer);
  }
#endif
}

