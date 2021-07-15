/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  SMP initiator state machine action functions.
 *
 *  Copyright (c) 2010-2018 Arm Ltd. All Rights Reserved.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_buf.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include "util/calc128.h"
#include "smp_api.h"
#include "smp_main.h"
#include "smpi_main.h"
#include "dm_api.h"

/*************************************************************************************************/
/*!
 *  \brief  Initiate a pairing request.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiActPairReq(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   *pPkt;
  uint8_t   *p;

  /* set connection busy */
  DmConnSetIdle(pCcb->connId, DM_IDLE_SMP_PAIR, DM_CONN_BUSY);

  /* set next expected packet */
  pCcb->nextCmdCode = SMP_CMD_PAIR_RSP;

  /* start smp response timer */
  smpStartRspTimer(pCcb);

  /* allocate scratch buffer */
  pCcb->pScr = WsfBufAlloc(sizeof(smpScratch_t));
  /* handle alloc failure */

  /* allocate packet buffer */
  if ((pPkt = smpMsgAlloc(SMP_PAIR_REQ_LEN + L2C_PAYLOAD_START)) != NULL)
  {
    /* build packet */
    p = pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, SMP_CMD_PAIR_REQ);
    UINT8_TO_BSTREAM(p, pSmpCfg->ioCap);
    UINT8_TO_BSTREAM(p, pMsg->dm.pair.oob);
    UINT8_TO_BSTREAM(p, pMsg->dm.pair.auth);
    UINT8_TO_BSTREAM(p, pSmpCfg->maxKeyLen);
    UINT8_TO_BSTREAM(p, pMsg->dm.pair.iKeyDist);
    UINT8_TO_BSTREAM(p, pMsg->dm.pair.rKeyDist);

    /* store pair req data */
    memcpy(pCcb->pairReq, pPkt + L2C_PAYLOAD_START, SMP_PAIR_REQ_LEN);

    /* send packet */
    smpSendPkt(pCcb, pPkt);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check if a security request has been received when pairing is cancelled.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiActCheckSecurityReq(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* if security req received send pairing failed */
  if (pCcb->secReq)
  {
    pCcb->secReq = FALSE;
    smpSendPairingFailed(pCcb, pMsg->hdr.status);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process a security request packet.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiActProcSecurityReq(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  dmSecSlaveIndEvt_t  slaveInd;

  pCcb->secReq = TRUE;

  /* parse packet */
  slaveInd.auth = *(pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN);

  /* pass to DM */
  slaveInd.hdr.param = pCcb->connId;
  slaveInd.hdr.event = DM_SEC_SLAVE_REQ_IND;
  DmSmpCbackExec((dmEvt_t *) &slaveInd);
}

/*************************************************************************************************/
/*!
 *  \brief  Process a pairing response packet.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiActProcPairRsp(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t         *p;
  uint8_t         oob;
  uint8_t         display;

  /* go to start of packet */
  p = pMsg->data.pPacket + L2C_PAYLOAD_START;

  /* store packet parameters */
  memcpy(pCcb->pairRsp, p, SMP_PAIR_RSP_LEN);

  /* verify no new key distribution bits are set */
  if (((~(pCcb->pairReq[SMP_IKEYDIST_POS]) & p[SMP_IKEYDIST_POS]) != 0) ||
      ((~(pCcb->pairReq[SMP_RKEYDIST_POS]) & p[SMP_RKEYDIST_POS]) != 0))
  {
    /* invalid parameters; cancel pairing */
    pMsg->hdr.status = SMP_ERR_INVALID_PARAM;
    pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, pMsg);
  }
  else
  {
    /* proceed to process pairing */
    if (smpCb.procPairing(pCcb, &oob, &display))
    {
      /* clear key ready */
      pCcb->keyReady = FALSE;

      smpCb.procAuthReq(pCcb, oob, display);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process a pairing confirm packet.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiActProcPairCnf(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   *pPkt;
  uint8_t   *p;

  /* go to start of packet */
  p = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* store confirm value */
  memcpy(pCcb->pScr->buf.b3, p, SMP_CONFIRM_LEN);

  /* set next expected packet */
  pCcb->nextCmdCode = SMP_CMD_PAIR_RAND;

  /* start smp response timer */
  smpStartRspTimer(pCcb);

  /* allocate packet buffer and send pairing random packet */
  if ((pPkt = smpMsgAlloc(SMP_PAIR_RAND_LEN + L2C_PAYLOAD_START)) != NULL)
  {
    /* build packet */
    p = pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, SMP_CMD_PAIR_RAND);
    memcpy(p, pCcb->pScr->buf.b4, SMP_RAND_LEN);

    /* send packet */
    smpSendPkt(pCcb, pPkt);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Verify the calculated confirm value.  If ok, proceed with STK calculcation.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiActCnfVerify(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* compare calculated confirm value with value received earlier */
  if (memcmp(pMsg->aes.pCiphertext, pCcb->pScr->buf.b3, SMP_CONFIRM_LEN) != 0)
  {
    pMsg->hdr.status = SMP_ERR_CONFIRM_VALUE;

    /* confirm values don't match; update repeated attempts count */
    pCcb->attempts++;
    SmpDbPairingFailed(pCcb->connId);

    if (pCcb->attempts == pSmpCfg->maxAttempts)
    {
      /* max attempts reached */
      pMsg->hdr.event = SMP_MSG_INT_MAX_ATTEMPTS;
    }
    else
    {
      /* else just fail */
      pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
    }

    smpSmExecute(pCcb, pMsg);
    return;
  }

  /* do STK calculation: key, responder rand, initiator rand */
  smpCalcS1(pCcb, pCcb->pScr->buf.b1, pCcb->pScr->buf.b2, pCcb->pScr->buf.b4);
}

/*************************************************************************************************/
/*!
 *  \brief  Encrypt link with STK.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiActStkEncrypt(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t buf[SMP_KEY_LEN];
  uint8_t encKeyLen;
  uint8_t secLevel;

  encKeyLen = (pCcb->pairReq[SMP_MAXKEY_POS] < pCcb->pairRsp[SMP_MAXKEY_POS]) ?
               pCcb->pairReq[SMP_MAXKEY_POS] : pCcb->pairRsp[SMP_MAXKEY_POS];

  /* adjust key based on max key length */
  memcpy(buf, pMsg->aes.pCiphertext, encKeyLen);
  memset((buf + encKeyLen), 0, (SMP_KEY_LEN - encKeyLen));
  pCcb->keyReady = TRUE;

  secLevel = (pCcb->auth & SMP_AUTH_MITM_FLAG) ? DM_SEC_LEVEL_ENC_AUTH : DM_SEC_LEVEL_ENC;
  DmSmpEncryptReq(pCcb->connId, secLevel, buf);
}

/*************************************************************************************************/
/*!
 *  \brief  Set up key distribution.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiActSetupKeyDist(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   rKeyDist;

  /* start smp response timer once for entire key distribution phase */
  smpStartRspTimer(pCcb);

  /* initialize parameters in key ind struct */
  pCcb->pScr->keyInd.hdr.param = pCcb->connId;
  pCcb->pScr->keyInd.secLevel = (pCcb->auth & SMP_AUTH_MITM_FLAG) ?
                                DM_SEC_LEVEL_ENC_AUTH : DM_SEC_LEVEL_ENC;
  pCcb->pScr->keyInd.encKeyLen =
    (pCcb->pairReq[SMP_MAXKEY_POS] < pCcb->pairRsp[SMP_MAXKEY_POS]) ?
     pCcb->pairReq[SMP_MAXKEY_POS] : pCcb->pairRsp[SMP_MAXKEY_POS];

  pCcb->nextCmdCode = 0;

  /* get negotiated responder key distribution */
  rKeyDist = pCcb->pairReq[SMP_RKEYDIST_POS] & pCcb->pairRsp[SMP_RKEYDIST_POS];

  /* set up to receive first key distribution packet */
  if (rKeyDist & SMP_KEY_DIST_ENC)
  {
      if (smpCb.lescSupported && pCcb->pScCcb->lescEnabled)
      {
        if (rKeyDist & SMP_KEY_DIST_ID)
        {
          pCcb->nextCmdCode = SMP_CMD_ID_INFO;
        }
      }
      else
      {
        pCcb->nextCmdCode = SMP_CMD_ENC_INFO;
      }
  }
  else if (rKeyDist & SMP_KEY_DIST_ID)
  {
    pCcb->nextCmdCode = SMP_CMD_ID_INFO;
  }
  else if (rKeyDist & SMP_KEY_DIST_SIGN)
  {
    pCcb->nextCmdCode = SMP_CMD_SIGN_INFO;
  }

  if (pCcb->nextCmdCode == 0)
  {
    /* no responder keys to be distributed; start sending keys */
    pMsg->hdr.event = SMP_MSG_INT_SEND_NEXT_KEY;
    smpSmExecute(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Receive a key.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiActRcvKey(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   keyDist;

  /* get responder key distribution */
  keyDist = pCcb->pairReq[SMP_RKEYDIST_POS] & pCcb->pairRsp[SMP_RKEYDIST_POS];

  /* process received key */
  if (smpProcRcvKey(pCcb, &pCcb->pScr->keyInd, pMsg->data.pPacket, keyDist))
  {
    /* no responder keys to be distributed; start sending keys */
    pCcb->nextCmdCode = 0;

    pMsg->hdr.event = SMP_MSG_INT_SEND_NEXT_KEY;
    smpSmExecute(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send a key.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiActSendKey(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t     keyDist;

  /* get initiator key distribution */
  keyDist = pCcb->pairReq[SMP_IKEYDIST_POS] & pCcb->pairRsp[SMP_IKEYDIST_POS];

  /* send next key */
  if ((pCcb->nextCmdCode == 0) && smpSendKey(pCcb, keyDist))
  {
      /* done sending keys; send ourselves pairing complete msg */
      pMsg->hdr.event = SMP_MSG_INT_PAIRING_CMPL;
      smpSmExecute(pCcb, pMsg);
      return;
  }
}
