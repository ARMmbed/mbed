/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  SMP responder state machine action functions.
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
#include "smpr_main.h"
#include "dm_api.h"

/*************************************************************************************************/
/*!
 *  \brief  Send a slave security request.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprActSendSecurityReq(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   *pPkt;
  uint8_t   *p;

  /* start smp response timer */
  smpStartRspTimer(pCcb);

  /* allocate packet buffer */
  if ((pPkt = smpMsgAlloc(SMP_SECURITY_REQ_LEN + L2C_PAYLOAD_START)) != NULL)
  {
    /* build packet */
    p = pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, SMP_CMD_SECURITY_REQ);
    UINT8_TO_BSTREAM(p, pMsg->dm.securityReq.auth);

    /* send packet */
    smpSendPkt(pCcb, pPkt);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process a pairing request packet.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprActProcPairReq(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  dmSecPairIndEvt_t   pairInd;
  uint8_t             *p;

  /* allocate scratch buffer */
  if (pCcb->pScr == NULL)
  {
    if ((pCcb->pScr = WsfBufAlloc(sizeof(smpScratch_t))) == NULL)
    {
      /* alloc failed; cancel pairing */
      pMsg->hdr.status = SMP_ERR_UNSPECIFIED;
      pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
      smpSmExecute(pCcb, pMsg);
      return;
    }
  }
  else
  {
    /* should not happen */
    SMP_TRACE_ERR0("pScr already allocated");
  }

  /* set connection busy */
  DmConnSetIdle(pCcb->connId, DM_IDLE_SMP_PAIR, DM_CONN_BUSY);

  p = pMsg->data.pPacket + L2C_PAYLOAD_START;

  /* store packet for later */
  memcpy(pCcb->pairReq, p, SMP_PAIR_REQ_LEN);

  /* parse packet to callback event structure */
  p++;    /* skip command code */
  p++;    /* skip IO capabilities */
  BSTREAM_TO_UINT8(pairInd.oob, p);
  BSTREAM_TO_UINT8(pairInd.auth, p);
  p++;    /* skip max key len */
  BSTREAM_TO_UINT8(pairInd.iKeyDist, p);
  BSTREAM_TO_UINT8(pairInd.rKeyDist, p);

  /* call app callback */
  pairInd.hdr.param = pCcb->connId;
  pairInd.hdr.event = DM_SEC_PAIR_IND;
  DmSmpCbackExec((dmEvt_t *) &pairInd);
}

/*************************************************************************************************/
/*!
 *  \brief  Send a pairing response packet.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprActSendPairRsp(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   *pPkt;
  uint8_t   *p;
  uint8_t   oob;
  uint8_t   display;

  /* build packet to pairing response buffer in ccb */
  p = pCcb->pairRsp;
  UINT8_TO_BSTREAM(p, SMP_CMD_PAIR_RSP);
  UINT8_TO_BSTREAM(p, pSmpCfg->ioCap);
  UINT8_TO_BSTREAM(p, pMsg->dm.pair.oob);
  UINT8_TO_BSTREAM(p, pMsg->dm.pair.auth);
  UINT8_TO_BSTREAM(p, pSmpCfg->maxKeyLen);
  UINT8_TO_BSTREAM(p, pMsg->dm.pair.iKeyDist);
  UINT8_TO_BSTREAM(p, pMsg->dm.pair.rKeyDist);

  /* process pairing request and response data */
  if (smpCb.procPairing(pCcb, &oob, &display))
  {
    /* clear key ready */
    pCcb->keyReady = FALSE;

    /* set next expected packet */
    if ((pCcb->pairReq[SMP_AUTHREQ_POS] & pMsg->dm.pair.auth & SMP_AUTH_SC_FLAG) == SMP_AUTH_SC_FLAG)
    {
      pCcb->nextCmdCode = SMP_CMD_PUBLIC_KEY;
    }
    else
    {
      pCcb->nextCmdCode = SMP_CMD_PAIR_CNF;
    }

    /* start smp response timer */
    smpStartRspTimer(pCcb);

    /* send pairing response; allocate packet buffer */
    if ((pPkt = smpMsgAlloc(SMP_PAIR_RSP_LEN + L2C_PAYLOAD_START)) != NULL)
    {
      /* build packet from pairing response buffer */
      memcpy(pPkt + L2C_PAYLOAD_START, pCcb->pairRsp, SMP_PAIR_RSP_LEN);

      /* send packet */
      smpSendPkt(pCcb, pPkt);
    }

    /* request authentication data */
    smpCb.procAuthReq(pCcb, oob, display);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Store pairing confirm value.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprActProcPairCnf(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   *p;

  /* go to start of packet */
  p = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* store confirm value */
  memcpy(pCcb->pScr->buf.b3, p, SMP_CONFIRM_LEN);

  /* discard any packets received erroneously at this point */
  pCcb->nextCmdCode = 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Store pairing confirm value and perform first part of pairing confirm calculation.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprActProcPairCnfCalc1(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  smprActProcPairCnf(pCcb, pMsg);

  /* get random number to scratchpad */
  SecRand(pCcb->pScr->buf.b4, SMP_RAND_LEN);

  /* execute calculation */
  smpCalcC1Part1(pCcb, pCcb->pScr->buf.b1, pCcb->pScr->buf.b4);
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
void smprActCnfVerify(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* compare calculated confirm value with value received earlier */
  if (memcmp(pMsg->aes.pCiphertext, pCcb->pScr->buf.b3, SMP_CONFIRM_LEN) != 0)
  {
    /* confirm values don't match; update repeated attempts count */
    pCcb->attempts++;
    SmpDbPairingFailed(pCcb->connId);
    pMsg->hdr.status = SMP_ERR_CONFIRM_VALUE;

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
  smpCalcS1(pCcb, pCcb->pScr->buf.b1, pCcb->pScr->buf.b4, pCcb->pScr->buf.b2);
}

/*************************************************************************************************/
/*!
 *  \brief  Store STK and then send a pairing random packet.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprActSendPairRandom(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   *pPkt;
  uint8_t   *p;
  uint8_t   encKeyLen;

  /* get max STK length */
  encKeyLen = (pCcb->pairReq[SMP_MAXKEY_POS] < pCcb->pairRsp[SMP_MAXKEY_POS]) ?
               pCcb->pairReq[SMP_MAXKEY_POS] : pCcb->pairRsp[SMP_MAXKEY_POS];

  /* store STK and adjust based on max key length */
  memcpy(pCcb->pScr->buf.b3, pMsg->aes.pCiphertext, encKeyLen);
  memset((pCcb->pScr->buf.b3 + encKeyLen), 0, (SMP_KEY_LEN - encKeyLen));
  pCcb->keyReady = TRUE;

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
 *  \brief  Set up key distribution.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprActSetupKeyDist(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* don't receive anything yet */
  pCcb->nextCmdCode = 0;

  /* start smp response timer once for entire key distribution phase */
  smpStartRspTimer(pCcb);

  /* initialize parameters in key ind struct */
  pCcb->pScr->keyInd.hdr.param = pCcb->connId;
  pCcb->pScr->keyInd.secLevel = (pCcb->auth & SMP_AUTH_MITM_FLAG) ?
                                DM_SEC_LEVEL_ENC_AUTH : DM_SEC_LEVEL_ENC;
  pCcb->pScr->keyInd.encKeyLen =
    (pCcb->pairReq[SMP_MAXKEY_POS] < pCcb->pairRsp[SMP_MAXKEY_POS]) ?
     pCcb->pairReq[SMP_MAXKEY_POS] : pCcb->pairRsp[SMP_MAXKEY_POS];

  /* start key distribution */
  smprActSendKey(pCcb, pMsg);
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
void smprActSendKey(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t     keyDist;

  /* get responder key distribution */
  keyDist = pCcb->pairReq[SMP_RKEYDIST_POS] & pCcb->pairRsp[SMP_RKEYDIST_POS];

  /* send next key; if done sending keys set up to receive keys */
  if ((pCcb->nextCmdCode == 0) && smpSendKey(pCcb, keyDist))
  {
    pCcb->nextCmdCode = 0;

    /* get initiator key distribution */
    keyDist = pCcb->pairReq[SMP_IKEYDIST_POS] & pCcb->pairRsp[SMP_IKEYDIST_POS];

    /* set up to receive first key distribution packet */
    if (keyDist & SMP_KEY_DIST_ENC)
    {
      if (smpCb.lescSupported && pCcb->pScCcb->lescEnabled)
      {
        if (keyDist & SMP_KEY_DIST_ID)
        {
          pCcb->nextCmdCode = SMP_CMD_ID_INFO;
        }
      }
      else
      {
        pCcb->nextCmdCode = SMP_CMD_ENC_INFO;
      }
    }
    else if (keyDist & SMP_KEY_DIST_ID)
    {
      pCcb->nextCmdCode = SMP_CMD_ID_INFO;
    }
    else if (keyDist & SMP_KEY_DIST_SIGN)
    {
      pCcb->nextCmdCode = SMP_CMD_SIGN_INFO;
    }

    if (pCcb->nextCmdCode == 0)
    {
      /* no keys to receive; send ourselves pairing complete msg */
      pMsg->hdr.event = SMP_MSG_INT_PAIRING_CMPL;
      smpSmExecute(pCcb, pMsg);
    }
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
void smprActRcvKey(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t   keyDist;

  /* get initiator key distribution */
  keyDist = pCcb->pairReq[SMP_IKEYDIST_POS] & pCcb->pairRsp[SMP_IKEYDIST_POS];

  /* process received key */
  if (smpProcRcvKey(pCcb, &pCcb->pScr->keyInd, pMsg->data.pPacket, keyDist))
  {
    /* no more keys to receive; send ourselves pairing complete msg */
    pMsg->hdr.event = SMP_MSG_INT_PAIRING_CMPL;
    smpSmExecute(pCcb, pMsg);
  }
}
