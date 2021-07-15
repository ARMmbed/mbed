/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave encryption action routines.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_enc_slave.h"
#include "lmgr_api.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include "pal_crypto.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief      Modify encryption mode.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrModifyEncMode(lctrConnCtx_t *pCtx)
{
  if (pCtx->pendEncMode)
  {
    /*** Apply new encryption mode. ***/

    LL_TRACE_INFO3("    >>> Modifying encryption mode, handle=%u, enaAuth=%u, nonceMode=%u <<<", LCTR_GET_CONN_HANDLE(pCtx), pCtx->newEncMode.enaAuth, pCtx->newEncMode.nonceMode);

    pCtx->bleData.chan.enc.enaAuth = pCtx->newEncMode.enaAuth;
    pCtx->bleData.chan.enc.nonceMode = pCtx->newEncMode.nonceMode;

    if (pCtx->newEncMode.nonceMode == LL_NONCE_MODE_EVT_CNTR)
    {
      pCtx->bleData.chan.enc.pEventCounter = &pCtx->eventCounter;
    }
    else
    {
      pCtx->bleData.chan.enc.pEventCounter = NULL;
    }
  }

  pCtx->pendEncMode = FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Enable Tx data encryption.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrEnableTxDataEnc(lctrConnCtx_t *pCtx)
{
  lctrModifyEncMode(pCtx);

  pCtx->bleData.chan.enc.enaEncrypt = TRUE;
  LL_TRACE_INFO1("    >>> Tx/Encryption Enabled, handle=%u <<<", LCTR_GET_CONN_HANDLE(pCtx));
}

/*************************************************************************************************/
/*!
 *  \brief      Enable Tx data encryption.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrDisableTxDataEnc(lctrConnCtx_t *pCtx)
{
  pCtx->bleData.chan.enc.enaEncrypt = FALSE;
  LL_TRACE_INFO1("    >>> Tx/Encryption Disabled, handle=%u <<<", LCTR_GET_CONN_HANDLE(pCtx));
}

/*************************************************************************************************/
/*!
 *  \brief      Enable Rx data encryption.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrEnableRxDataEnc(lctrConnCtx_t *pCtx)
{
  lctrModifyEncMode(pCtx);

  pCtx->bleData.chan.enc.enaDecrypt = TRUE;
  LL_TRACE_INFO1("    >>> Rx/Decryption Enabled, handle=%u <<<", LCTR_GET_CONN_HANDLE(pCtx));
}

/*************************************************************************************************/
/*!
 *  \brief      Enable Rx data encryption.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrDisableRxDataEnc(lctrConnCtx_t *pCtx)
{
  pCtx->bleData.chan.enc.enaDecrypt = FALSE;
  LL_TRACE_INFO1("    >>> Rx/Decryption Disabled, handle=%u <<<", LCTR_GET_CONN_HANDLE(pCtx));
}

/*************************************************************************************************/
/*!
 *  \brief      Generate slave encryption vectors.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrGenerateSlvVectors(lctrConnCtx_t *pCtx)
{
  memcpy(pCtx->rand, lctrDataPdu.pld.encReq.rand, sizeof(pCtx->rand));
  pCtx->ediv = lctrDataPdu.pld.encReq.ediv;

  /* Store master part. */
  memcpy(pCtx->skd, lctrDataPdu.pld.encReq.skd_m + LCTR_SKD_M_OFFS, LL_SKD_LEN / 2);
  memcpy(pCtx->iv, lctrDataPdu.pld.encReq.iv_m + LCTR_IV_M_OFFS,  LL_IV_LEN / 2);

  /* Generate slave part of IV. */
  PalCryptoGenerateRandomNumber(pCtx->iv + LCTR_IV_S_OFFS, LL_IV_LEN / 2);

  /* Generate slave part of SKD. */
  PalCryptoGenerateRandomNumber(pCtx->skd + LCTR_SKD_S_OFFS, LL_SKD_LEN / 2);
}

/*************************************************************************************************/
/*!
 *  \brief      Store LTK reply.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrStoreLtkReply(lctrConnCtx_t *pCtx)
{
  memcpy(pCtx->ltk, pLctrConnMsg->ltkReply.key, sizeof(pCtx->ltk));
}

/*************************************************************************************************/
/*!
 *  \brief      Store LTK negative reply termination reason.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrStoreLtkNegRepTerminateReason(lctrConnCtx_t *pCtx)
{
  pCtx->termReason = LL_ERROR_CODE_PIN_KEY_MISSING;
}

/*************************************************************************************************/
/*!
 *  \brief      Calculate session keys.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrCalcSessionKey(lctrConnCtx_t *pCtx)
{
  PalCryptoEnc_t * const pEnc = &pCtx->bleData.chan.enc;

  /* Use AES to transform LTK to session key using session key diversifier as seed. */
  PalCryptoAesEcb(pCtx->ltk, pEnc->sk, pCtx->skd);

  memcpy(pEnc->iv, pCtx->iv, sizeof(pEnc->iv));
  pEnc->dir = (pCtx->role == LL_ROLE_MASTER) ? 1 : 0;     /* master = 1; slave = 0 */
  pEnc->type = PAL_BB_TYPE_ACL;
  pCtx->txPktCounter = 0;
  pCtx->rxPktCounter = 0;
  
  if (lctrInitCipherBlkHdlr)
  {
    lctrInitCipherBlkHdlr(pEnc, LCTR_GET_CONN_HANDLE(pCtx), pEnc->dir);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send feature response PDU to peer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrInvalidEncPduSeq(lctrConnCtx_t *pCtx)
{
  LL_TRACE_WARN3("Out of sequence LLCP packet received, dropping connection, handle=%u, encState=%u, opcode=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->encState, lctrDataPdu.opcode);
  lctrSendConnMsg(pCtx, LCTR_CONN_TERM_MIC_FAILED);
}

/*************************************************************************************************/
/*!
 *  \brief      Send feature response PDU to peer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendEncRsp(lctrConnCtx_t *pCtx)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_ENC_RSP_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/

    UINT8_TO_BSTREAM(pBuf, LL_PDU_ENC_RSP);

    memcpy(pBuf, pCtx->skd + LCTR_SKD_S_OFFS, LL_SKD_LEN / 2);
    pBuf += LL_SKD_LEN/2;

    memcpy(pBuf, pCtx->iv + LCTR_IV_S_OFFS, LL_IV_LEN / 2);

    /*** Queue for transmit. ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send start encryption request PDU to peer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendStartEncReq(lctrConnCtx_t *pCtx)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_START_ENC_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/

    UINT8_TO_BSTREAM(pBuf, LL_PDU_START_ENC_REQ);

    /*** Queue for transmit. ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send start encryption response PDU to peer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendStartEncRsp(lctrConnCtx_t *pCtx)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_START_ENC_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/

    UINT8_TO_BSTREAM(pBuf, LL_PDU_START_ENC_RSP);

    /*** Queue for transmit. ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send pause encryption request PDU to peer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendPauseEncReq(lctrConnCtx_t *pCtx)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PAUSE_ENC_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/

    UINT8_TO_BSTREAM(pBuf, LL_PDU_PAUSE_ENC_REQ);

    /*** Queue for transmit. ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send pause encryption response PDU to peer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendPauseEncRsp(lctrConnCtx_t *pCtx)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PAUSE_ENC_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/

    UINT8_TO_BSTREAM(pBuf, LL_PDU_PAUSE_ENC_RSP);

    /*** Queue for transmit. ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send ping request PDU to peer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendPingReq(lctrConnCtx_t *pCtx)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PING_PDU_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/

    UINT8_TO_BSTREAM(pBuf, LL_PDU_PING_REQ);

    /*** Queue for transmit. ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send ping response PDU to peer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendPingRsp(lctrConnCtx_t *pCtx)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PING_PDU_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/

    UINT8_TO_BSTREAM(pBuf, LL_PDU_PING_RSP);

    /*** Queue for transmit. ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Notify slave host of connect indication.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrEncNotifyHostLtkReqInd(lctrConnCtx_t *pCtx)
{
  const uint16_t handle = LCTR_GET_CONN_HANDLE(pCtx);

  LlLtkReqInd_t evt =
  {
    .hdr =
    {
      .param        = handle,
      .event        = LL_LTK_REQ_IND,
      .status       = LL_SUCCESS
    },

    .handle         = handle,
    .encDiversifier = pCtx->ediv
  };

  memcpy(evt.randNum, pCtx->rand, sizeof(evt.randNum));

  LL_TRACE_INFO1("### LlEvent ###  LL_LTK_REQ_IND, handle=%u, status=LL_SUCCESS", handle);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of encryption change.
 *
 *  \param      pCtx    Connection context.
 *  \param      status  Status code.
 */
/*************************************************************************************************/
void lctrNotifyEncChangeInd(lctrConnCtx_t *pCtx, uint8_t status)
{
  const uint16_t handle = LCTR_GET_CONN_HANDLE(pCtx);
  PalCryptoEnc_t * const pEnc = &pCtx->bleData.chan.enc;

  LlEncChangeInd_t evt =
  {
    .hdr =
    {
      .param        = handle,
      .event        = LL_ENC_CHANGE_IND,
      .status       = status
    },

    .status         = status,
    .handle         = handle,
    .enabled        = pEnc->enaDecrypt
  };

  LL_TRACE_INFO3("### LlEvent ###  LL_ENC_CHANGE_IND, handle=%u, status=%u, enabled=%u", handle, status, evt.enabled);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of key refreshed.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrNotifyEncKeyRefreshInd(lctrConnCtx_t *pCtx)
{
  const uint16_t handle = LCTR_GET_CONN_HANDLE(pCtx);

  LlEncKeyRefreshInd_t evt =
  {
    .hdr =
    {
      .param        = handle,
      .event        = LL_ENC_KEY_REFRESH_IND,
      .status       = LL_SUCCESS
    },

    .status         = LL_SUCCESS,
    .handle         = handle,
  };

  LL_TRACE_INFO1("### LlEvent ###  LL_ENC_KEY_REFRESH_IND, handle=%u, status=LL_SUCCESS", handle);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of authentication payload timeout expired.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrNotifyAuthPayloadTimeout(lctrConnCtx_t *pCtx)
{
  const uint16_t handle = LCTR_GET_CONN_HANDLE(pCtx);

  LlAuthPayloadTimeoutInd_t evt =
  {
    .hdr =
    {
      .param        = handle,
      .event        = LL_AUTH_PAYLOAD_TIMEOUT_IND,
      .status       = LL_SUCCESS
    },

    .handle         = handle,
  };

  LL_TRACE_INFO1("### LlEvent ###  LL_AUTH_PAYLOAD_TIMEOUT_IND, handle=%u, status=LL_SUCCESS", handle);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Restart authentication payload timeout timer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrRestartAuthPayloadTimer(lctrConnCtx_t *pCtx)
{
  WsfTimerStartMs(&pCtx->tmrAuthTimeout, pCtx->authTimeoutMs);
}
