/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  SMP common utility functions and action functions.
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
#include "wsf_buf.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include "util/calc128.h"
#include "dm_api.h"
#include "smp_api.h"
#include "smp_main.h"

#ifndef SMP_EXTRA_TRACE
#define SMP_EXTRA_TRACE FALSE
#endif

/*************************************************************************************************/
/*!
 *  \brief  Start SMP response timer.
 *
 *  \param  pCcb  Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpStartRspTimer(smpCcb_t *pCcb)
{
  /* start smp response timer */
  pCcb->rspTimer.msg.event = SMP_MSG_INT_RSP_TIMEOUT;
  pCcb->rspTimer.msg.status = SMP_ERR_TIMEOUT;
  WsfTimerStartSec(&pCcb->rspTimer, SMP_TIMEOUT);
}

/*************************************************************************************************/
/*!
 *  \brief  No action.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActNone(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Cleanup CCB.
 *
 *  \param  pCcb  Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpCleanup(smpCcb_t *pCcb)
{
  /* free scratch buffer */
  if (pCcb->pScr != NULL)
  {
    WsfBufFree(pCcb->pScr);
    pCcb->pScr = NULL;
  }

  /* stop response timer */
  WsfTimerStop(&pCcb->rspTimer);

  /* stop wait interval timer */
  WsfTimerStop(&pCcb->waitTimer);

  pCcb->secReq = FALSE;
  pCcb->nextCmdCode = (pCcb->initiator) ? SMP_CMD_SECURITY_REQ : SMP_CMD_PAIR_REQ;
  pCcb->lastSentKey = 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Cleanup CCB.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActCleanup(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  smpCleanup(pCcb);
}

/*************************************************************************************************/
/*!
 *  \brief  Send a pairing failed packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  reason    Failure reason.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpSendPairingFailed(smpCcb_t *pCcb, uint8_t reason)
{
  uint8_t *pPacket;
  uint8_t *p;

  if ((pPacket = smpMsgAlloc(L2C_PAYLOAD_START + SMP_PAIR_FAIL_LEN)) != NULL)
  {
    p = pPacket + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, SMP_CMD_PAIR_FAIL);
    UINT8_TO_BSTREAM(p, reason);

    smpSendPkt(pCcb, pPacket);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Pairing failed.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActPairingFailed(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* clean up */
  smpCleanup(pCcb);

  /* set connection idle */
  DmConnSetIdle(pCcb->connId, DM_IDLE_SMP_PAIR, DM_CONN_IDLE);

  /* notify DM of pairing failure */
  pMsg->hdr.event = DM_SEC_PAIR_FAIL_IND;
  DmSmpCbackExec((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Response timeout on security request.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActSecReqTimeout(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* The initiator can ignore security request. Cancel pairing if paring/encryption was not
   * completed, else assume the initiator ignored a request to change keys. */
  if (DmConnSecLevel(pCcb->connId) == DM_SEC_LEVEL_NONE)
  {
    smpActPairingFailed(pCcb, pMsg);
  }
  else
  {
    /* Cleanup and return to IDLE without failure */
    pMsg->hdr.event = SMP_MSG_INT_CLEANUP;
    smpSmExecute(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Pairing cancelled by user.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActPairingCancel(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* send pairing failed packet */
  smpSendPairingFailed(pCcb, pMsg->hdr.status);

  smpActPairingFailed(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Store the authentication data.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActStorePin(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* copy authentication data to scratchpad */
  memcpy(pCcb->pScr->buf.b1, pMsg->dm.authRsp.authData, pMsg->dm.authRsp.authDataLen);

  /* zero out unused pin data */
  if (pMsg->dm.authRsp.authDataLen == SMP_PIN_LEN)
  {
    memset(&pCcb->pScr->buf.b1[SMP_PIN_LEN], 0, SMP_OOB_LEN - SMP_PIN_LEN);
  }
}


/*************************************************************************************************/
/*!
 *  \brief  Process a pairing request and response data.
 *
 *  \param  pCcb      Connection control block.
 *  \param  pOob      Return parameter, TRUE if out-of-band data requested.
 *  \param  pDisplay  Return parameter, TRUE if pin is to be displayed.
 *
 *  \return TRUE on success, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t smpProcPairing(smpCcb_t *pCcb, uint8_t *pOob, uint8_t *pDisplay)
{
  bool_t          justWorks = TRUE;
  uint8_t         localAuth;
  wsfMsgHdr_t     hdr;

  *pDisplay = FALSE;
  *pOob = FALSE;

  /* if OOB available use that */
  if (pCcb->pairReq[SMP_OOB_POS] == SMP_OOB_DATA_PRESENT &&
      pCcb->pairRsp[SMP_OOB_POS] == SMP_OOB_DATA_PRESENT)
  {
    *pOob = SMP_OOB_DATA_PRESENT;
    justWorks = FALSE;
  }
  /* if either device set mitm flag */
  else if ((pCcb->pairReq[SMP_AUTHREQ_POS] & SMP_AUTH_MITM_FLAG) ||
           (pCcb->pairRsp[SMP_AUTHREQ_POS] & SMP_AUTH_MITM_FLAG))
  {
    /* check for compatible I/O settings */
    if ((pCcb->pairReq[SMP_IO_POS] != SMP_IO_NO_IN_NO_OUT) &&     /* initiator has i/o and */
        (pCcb->pairRsp[SMP_IO_POS] != SMP_IO_NO_IN_NO_OUT) &&     /* responder has i/o and */
        !(((pCcb->pairReq[SMP_IO_POS] == SMP_IO_DISP_ONLY) ||     /* both don't have display only */
           (pCcb->pairReq[SMP_IO_POS] == SMP_IO_DISP_YES_NO)) &&
          ((pCcb->pairRsp[SMP_IO_POS] == SMP_IO_DISP_ONLY) ||
           (pCcb->pairRsp[SMP_IO_POS] == SMP_IO_DISP_YES_NO))))
    {
      /* use pin */
      justWorks = FALSE;

      /* check if pin should be displayed (as initiator) */
      *pDisplay =
        ((pCcb->pairReq[SMP_IO_POS] == SMP_IO_DISP_ONLY) ||     /* initiator is display only or */
         (pCcb->pairReq[SMP_IO_POS] == SMP_IO_DISP_YES_NO) ||   /* initiator is display y/n  or */
         ((pCcb->pairReq[SMP_IO_POS] == SMP_IO_KEY_DISP) &&     /* initiator is key/display and */
          (pCcb->pairRsp[SMP_IO_POS] == SMP_IO_KEY_ONLY ||      /* responder is key only or key/display */
           pCcb->pairRsp[SMP_IO_POS] == SMP_IO_KEY_DISP)));

      /* invert display setting if we are not initiator and both are not key only */
      if (!(pCcb->pairRsp[SMP_IO_POS] == SMP_IO_KEY_ONLY &&
            pCcb->pairReq[SMP_IO_POS] == SMP_IO_KEY_ONLY))
      {
        *pDisplay ^= !pCcb->initiator;
      }
    }
  }

  if (!justWorks)
  {
    /* set auth flags with mitm bit set */
    pCcb->auth = (pCcb->pairReq[SMP_AUTHREQ_POS] & pCcb->pairRsp[SMP_AUTHREQ_POS]) | SMP_AUTH_MITM_FLAG;
  }
  else
  {
    /* set auth flags with mitm bit cleared */
    pCcb->auth = pCcb->pairReq[SMP_AUTHREQ_POS] & pCcb->pairRsp[SMP_AUTHREQ_POS] & ~SMP_AUTH_MITM_FLAG;
  }

  /* if we ended up with 'just works' but the device configuration requires authentication */
  localAuth = (pCcb->initiator) ? pCcb->pairReq[SMP_AUTHREQ_POS] : pCcb->pairRsp[SMP_AUTHREQ_POS];
  if (justWorks && (pSmpCfg->auth & localAuth & SMP_AUTH_MITM_FLAG))
  {
    /* cancel pairing */
    hdr.param = pCcb->connId;
    hdr.status = SMP_ERR_AUTH_REQ;
    hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
    return FALSE;
  }

  /* if max encryption key is less than our minimum */
  if (pCcb->pairReq[SMP_MAXKEY_POS] < pSmpCfg->minKeyLen ||
      pCcb->pairRsp[SMP_MAXKEY_POS] < pSmpCfg->minKeyLen)
  {
    /* cancel pairing */
    hdr.param = pCcb->connId;
    hdr.status = SMP_ERR_ENC_KEY_SIZE;
    hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
    return FALSE;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Request authentication data or send ourselves an authentication response.
 *
 *  \param  pCcb      Connection control block.
 *  \param  oob       Out-of-band data requested.
 *  \param  display   TRUE if pin is to be displayed.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpAuthReq(smpCcb_t *pCcb, uint8_t oob, uint8_t display)
{
  /* use a union to save a bit of memory on the stack */
  union
  {
    smpDmAuthRsp_t        authRsp;
    dmSecAuthReqIndEvt_t  authReq;
  } buf;

  /* if authenticated pairing */
  if (pCcb->auth & SMP_AUTH_MITM_FLAG)
  {
    /* request pin or oob from user */
    buf.authReq.hdr.param = pCcb->connId;
    buf.authReq.hdr.event = DM_SEC_AUTH_REQ_IND;
    buf.authReq.oob = oob;
    buf.authReq.display = display;
    DmSmpCbackExec((dmEvt_t *) &buf.authReq);
  }
  else
  {
    /* else use just works; send ourselves a auth rsp with all zero pin */
    buf.authRsp.hdr.param = pCcb->connId;
    buf.authRsp.hdr.event = SMP_MSG_API_AUTH_RSP;
    buf.authRsp.authData[0] = 0;
    buf.authRsp.authData[1] = 0;
    buf.authRsp.authData[2] = 0;
    buf.authRsp.authDataLen = SMP_PIN_LEN;
    smpSmExecute(pCcb, (smpMsg_t *) &buf.authRsp);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Perform first part of pairing confirm calculation.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActPairCnfCalc1(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* store authentication data */
  smpActStorePin(pCcb, pMsg);

  /* get random number to scratchpad */
  SecRand(pCcb->pScr->buf.b4, SMP_RAND_LEN);

  /* execute calculation */
  smpCalcC1Part1(pCcb, pCcb->pScr->buf.b1, pCcb->pScr->buf.b4);
}

/*************************************************************************************************/
/*!
 *  \brief  Perform second part of pairing confirm calculation.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActPairCnfCalc2(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  smpCalcC1Part2(pCcb, pCcb->pScr->buf.b1, pMsg->aes.pCiphertext);
}

/*************************************************************************************************/
/*!
 *  \brief  Send a pairing confirm packet.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActSendPairCnf(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   *pPkt;
  uint8_t   *p;

  /* set next expected packet */
  pCcb->nextCmdCode = (pCcb->initiator) ? SMP_CMD_PAIR_CNF : SMP_CMD_PAIR_RAND;

  /* start smp response timer */
  smpStartRspTimer(pCcb);

  /* allocate packet buffer */
  if ((pPkt = smpMsgAlloc(SMP_PAIR_CNF_LEN + L2C_PAYLOAD_START)) != NULL)
  {
    /* build packet */
    p = pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, SMP_CMD_PAIR_CNF);
    memcpy(p, pMsg->aes.pCiphertext, SMP_CONFIRM_LEN);

    /* send packet */
    smpSendPkt(pCcb, pPkt);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Perform first part of the pairing confirm verification calculation.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActPairCnfVerCalc1(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   *p;

  /* go to start of received pairing random packet */
  p = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* store random value */
  memcpy(pCcb->pScr->buf.b2, p, SMP_RAND_LEN);

  /* execute calculation */
  smpCalcC1Part1(pCcb, pCcb->pScr->buf.b1, p);
}

/*************************************************************************************************/
/*!
 *  \brief  Perform first part of the pairing confirm verification calculation.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActPairCnfVerCalc2(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  smpCalcC1Part2(pCcb, pCcb->pScr->buf.b1, pMsg->aes.pCiphertext);
}

/*************************************************************************************************/
/*!
 *  \brief  Send a key.
 *
 *  \param  pCcb    Connection control block.
 *  \param  keyDist Key distribution mask.
 *
 *  \return TRUE if done sending keys, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t smpSendKey(smpCcb_t *pCcb, uint8_t keyDist)
{
  uint8_t     *pPkt;
  uint8_t     *p;
  wsfMsgHdr_t *pHdr;

  if (smpCb.lescSupported && pCcb->pScCcb->lescEnabled && pCcb->lastSentKey == 0)
  {
    dmSecKeyIndEvt_t keyInd;

    /* pass LTK to app via DM */
    if (DmConnRole(pCcb->connId) == DM_ROLE_MASTER)
    {
      keyInd.type = DM_KEY_PEER_LTK;
    }
    else
    {
      keyInd.type = DM_KEY_LOCAL_LTK;
    }

    keyInd.hdr.event = DM_SEC_KEY_IND;
    keyInd.hdr.param = pCcb->connId;
    keyInd.secLevel = smpGetScSecLevel(pCcb);
    keyInd.keyData.ltk.ediv = 0;
    memset(keyInd.keyData.ltk.rand, 0, SMP_RAND8_LEN);
    Calc128Cpy(keyInd.keyData.ltk.key, pCcb->pScCcb->pLtk->ltk_t);
    DmSmpCbackExec((dmEvt_t *)&keyInd);

    pCcb->lastSentKey = SMP_CMD_MASTER_ID;
  }

  /* check if we're done sending keys */
  if ((keyDist == 0) ||
      (keyDist == SMP_KEY_DIST_ENC && pCcb->lastSentKey == SMP_CMD_MASTER_ID) ||
      (keyDist <= (SMP_KEY_DIST_ENC | SMP_KEY_DIST_ID) && pCcb->lastSentKey == SMP_CMD_ID_ADDR_INFO) ||
      (pCcb->lastSentKey == SMP_CMD_SIGN_INFO))
  {
      return TRUE;
  }

  /* if flow disabled return */
  if (pCcb->flowDisabled)
  {
    return FALSE;
  }

  /* allocate packet buffer for largest packet size */
  if ((pPkt = smpMsgAlloc(SMP_ENC_INFO_LEN + L2C_PAYLOAD_START)) != NULL)
  {
    p = pPkt + L2C_PAYLOAD_START;

    /* determine next key to send */
    if (pCcb->lastSentKey == 0 && (keyDist & SMP_KEY_DIST_ENC))
    {
      /* generate LTK, EDIV, and RAND */
      smpGenerateLtk(pCcb);

      /* send first part of LTK */
      UINT8_TO_BSTREAM(p, SMP_CMD_ENC_INFO);
      Calc128Cpy(p, pCcb->pScr->keyInd.keyData.ltk.key);
    }
    else if (pCcb->lastSentKey == SMP_CMD_ENC_INFO)
    {
      /* send second part of LTK */
      UINT8_TO_BSTREAM(p, SMP_CMD_MASTER_ID);
      UINT16_TO_BSTREAM(p, pCcb->pScr->keyInd.keyData.ltk.ediv);
      memcpy(p, pCcb->pScr->keyInd.keyData.ltk.rand, SMP_RAND8_LEN);
    }
    else if ((keyDist & SMP_KEY_DIST_ID) &&
             (pCcb->lastSentKey == 0 || pCcb->lastSentKey == SMP_CMD_MASTER_ID))
    {
      /* send first part of IRK */
      UINT8_TO_BSTREAM(p, SMP_CMD_ID_INFO);
      Calc128Cpy(p, DmSecGetLocalIrk());
    }
    else if (pCcb->lastSentKey == SMP_CMD_ID_INFO)
    {
      /* send second part of IRK */
      UINT8_TO_BSTREAM(p, SMP_CMD_ID_ADDR_INFO);
      UINT8_TO_BSTREAM(p, DmSecGetLocalIdentityAddrType());
      BDA_TO_BSTREAM(p, DmSecGetLocalIdentityAddr());
    }
    else if ((keyDist & SMP_KEY_DIST_SIGN) &&
             (pCcb->lastSentKey == 0 || pCcb->lastSentKey == SMP_CMD_ID_ADDR_INFO ||
              pCcb->lastSentKey == SMP_CMD_MASTER_ID))
    {
      /* send SRK */
      UINT8_TO_BSTREAM(p, SMP_CMD_SIGN_INFO);
      Calc128Cpy(p, DmSecGetLocalCsrk());
    }
    else
    {
      /* should never get here */
      WsfMsgFree(pPkt);
      SMP_TRACE_WARN2("smpSendKey unexpected state keyDist:%d lastSentKey:%d", keyDist, pCcb->lastSentKey);
      return TRUE;
    }

    /* set last sent key to command code */
    pCcb->lastSentKey = pPkt[L2C_PAYLOAD_START];

    /* send command packet */
    smpSendPkt(pCcb, pPkt);

    /* if flow not disabled set up to send next key */
    if (!pCcb->flowDisabled)
    {
      if ((pHdr = WsfMsgAlloc(sizeof(wsfMsgHdr_t))) != NULL)
      {
        pHdr->event = SMP_MSG_INT_SEND_NEXT_KEY;
        pHdr->param = pCcb->connId;
        WsfMsgSend(smpCb.handlerId, pHdr);
      }
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Process received key packet and pass it to DM if complete.
 *
 *  \param  pCcb      Connection control block.
 *  \param  pKeyInd   Key data structure allocated by caller.
 *  \param  pBuf      Buffer containing packet.
 *  \param  keyDist   Key distribution mask.
 *
 *  \return TRUE if done receiving keys, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t smpProcRcvKey(smpCcb_t *pCcb, dmSecKeyIndEvt_t *pKeyInd, uint8_t *pBuf, uint8_t keyDist)
{
  bool_t    keyIndReady = FALSE;
  bool_t    done = FALSE;
  uint8_t   cmdCode;

  /* go to start of packet */
  pBuf += L2C_PAYLOAD_START;
  cmdCode = *pBuf++;

  if (cmdCode == SMP_CMD_ENC_INFO)
  {
    /* parse encryption information packet */
    Calc128Cpy(pKeyInd->keyData.ltk.key, pBuf);
  }
  else if (cmdCode == SMP_CMD_MASTER_ID)
  {
    /* parse master identification packet */
    BSTREAM_TO_UINT16(pKeyInd->keyData.ltk.ediv, pBuf);
    memcpy(pKeyInd->keyData.ltk.rand, pBuf, SMP_RAND8_LEN);
    pKeyInd->secLevel = (pCcb->auth & SMP_AUTH_MITM_FLAG) ? DM_SEC_LEVEL_ENC_AUTH : DM_SEC_LEVEL_ENC;
    pKeyInd->type = DM_KEY_PEER_LTK;
    keyIndReady = TRUE;
  }
  else if (cmdCode == SMP_CMD_ID_INFO)
  {
    /* parse identity information packet */
    Calc128Cpy(pKeyInd->keyData.irk.key, pBuf);
  }
  else if (cmdCode == SMP_CMD_ID_ADDR_INFO)
  {
    /* parse identity address information packet */
    BSTREAM_TO_UINT8(pKeyInd->keyData.irk.addrType, pBuf);
    BSTREAM_TO_BDA(pKeyInd->keyData.irk.bdAddr, pBuf);
    pKeyInd->type = DM_KEY_IRK;
    keyIndReady = TRUE;
  }
  else if (cmdCode == SMP_CMD_SIGN_INFO)
  {
    /* parse signing information packet */
    Calc128Cpy(pKeyInd->keyData.csrk.key, pBuf);
    pKeyInd->secLevel = (pCcb->auth & SMP_AUTH_MITM_FLAG) ? DM_SEC_LEVEL_ENC_AUTH : DM_SEC_LEVEL_ENC;
    pKeyInd->type = DM_KEY_CSRK;
    keyIndReady = TRUE;
  }

  /* set up to receive next key */

  /* if just got first part of LTK or IRK */
  if (pCcb->nextCmdCode == SMP_CMD_ENC_INFO || pCcb->nextCmdCode == SMP_CMD_ID_INFO)
  {
    /* wait for second part of LTK or IRK info */
    pCcb->nextCmdCode++;
  }
  /* else if got LTK and need IRK */
  else if ((keyDist & SMP_KEY_DIST_ID) && (pCcb->nextCmdCode == SMP_CMD_MASTER_ID))
  {
    /* wait for first part of IRK */
    pCcb->nextCmdCode = SMP_CMD_ID_INFO;
  }
  /* else if got LTK or IRK and need SRK */
  else if ((keyDist & SMP_KEY_DIST_SIGN) &&
           (pCcb->nextCmdCode == SMP_CMD_MASTER_ID || pCcb->nextCmdCode == SMP_CMD_ID_ADDR_INFO))
  {
    /* wait for SRK */
    pCcb->nextCmdCode = SMP_CMD_SIGN_INFO;
  }
  else
  {
    /* done receiving keys */
    done = TRUE;
  }

  /* call callback if key ready */
  if (keyIndReady)
  {
    pKeyInd->hdr.event = DM_SEC_KEY_IND;
    DmSmpCbackExec((dmEvt_t *) pKeyInd);
  }

  return done;
}

/*************************************************************************************************/
/*!
 *  \brief  Maximum unsuccessful pairing attempts reached.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActMaxAttempts(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint32_t timeout;

  /* send paring failed packet; note this stops the timer so call this first */
  smpActPairingCancel(pCcb, pMsg);

  /* Check SMP device DB to determine time to wait before pairing can happen again */
  timeout = SmpDbMaxAttemptReached(pCcb->connId);

  /* start wait interval timer */
  pCcb->waitTimer.msg.event = SMP_MSG_INT_WI_TIMEOUT;
  WsfTimerStartMs(&pCcb->waitTimer, timeout);

  /* clear attempts count */
  pCcb->attempts = 0;
}

/*************************************************************************************************/
/*!
 *  \brief  A pairing attempt was received while in 'repeated attempts' state.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActAttemptRcvd(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* set that attempt was received */
  pCcb->attempts = 1;
}

/*************************************************************************************************/
/*!
 *  \brief  Notify DM of a pairing failure due to max attempt failures.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActNotifyDmAttemptsFailure(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* notify DM of pairing failure */
  pMsg->hdr.status = SMP_ERR_ATTEMPTS;
  pMsg->hdr.event = DM_SEC_PAIR_FAIL_IND;
  DmSmpCbackExec((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Notify DM of a pairing failure due to response timeout.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActNotifyDmRspToFailure(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* notify DM of pairing failure */
  pMsg->hdr.status = SMP_ERR_TIMEOUT;
  pMsg->hdr.event = DM_SEC_PAIR_FAIL_IND;
  DmSmpCbackExec((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Check if any pairing attempts received when leaving 'repeated attempts' state.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActCheckAttempts(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* check if attempt was received */
  if (pCcb->attempts)
  {
    pCcb->attempts = 0;
    smpSendPairingFailed(pCcb, SMP_ERR_ATTEMPTS);

    /* notify DM of pairing failure */
    smpActNotifyDmAttemptsFailure(pCcb, pMsg);

    smpCleanup(pCcb);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Pairing completed successfully.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpActPairingCmpl(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  dmSecPairCmplIndEvt_t pairCmpl;

  smpCleanup(pCcb);

  /* set connection idle */
  DmConnSetIdle(pCcb->connId, DM_IDLE_SMP_PAIR, DM_CONN_IDLE);

  pairCmpl.auth = pCcb->auth;
  pairCmpl.hdr.param = pCcb->connId;
  pairCmpl.hdr.event = DM_SEC_PAIR_CMPL_IND;
  DmSmpCbackExec((dmEvt_t *) &pairCmpl);
}

/*************************************************************************************************/
/*!
 *  \brief  Execute the SMP state machine.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpSmExecute(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  smpTblEntry_t const *pTblEntry;
  smpSmIf_t const *pSmIf;

#if SMP_EXTRA_TRACE == TRUE
  if (smpCb.lescSupported && pCcb->pScCcb && pCcb->pScCcb->lescEnabled)
    SMP_TRACE_INFO2("SMP Exe: evt=%s st=%s", smpEventStr(pMsg->hdr.event), smpStateStr(pCcb->state));
  else
#endif
    SMP_TRACE_INFO2("smpSmExecute event=%d state=%d", pMsg->hdr.event, pCcb->state);

  /* look up state table for state */
  pSmIf = DmConnRole(pCcb->connId) == DM_ROLE_SLAVE? smpCb.pSlave : smpCb.pMaster;
  pTblEntry = pSmIf->pStateTbl[pCcb->state];

  /* run through state machine twice; once with state table for current state
   * and once with the state table for common events
   */
  for(;;)
  {
    /* look for event match and execute action */
    do
    {
      /* if match */
      if ((*pTblEntry)[SMP_SM_POS_EVENT] == pMsg->hdr.event)
      {
        /* set next state */
        pCcb->state = (*pTblEntry)[SMP_SM_POS_NEXT_STATE];

        /* execute action */
        (*pSmIf->pActionTbl[(*pTblEntry)[SMP_SM_POS_ACTION]])(pCcb, pMsg);

        return;
      }

      /* next entry */
      pTblEntry++;

      /* while not at end */
    } while ((*pTblEntry)[SMP_SM_POS_EVENT] != 0);

    /* if we've reached end of the common state table */
    if (pTblEntry == (pSmIf->pCommonTbl + SMP_STATE_TBL_COMMON_MAX - 1))
    {
      /* we're done */
      break;
    }
    /* else we haven't run through common state table yet */
    else
    {
      /* set it up */
      pTblEntry = pSmIf->pCommonTbl;
    }
  }
}
