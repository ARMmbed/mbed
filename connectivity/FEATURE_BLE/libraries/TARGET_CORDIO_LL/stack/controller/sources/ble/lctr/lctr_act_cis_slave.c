/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer connected isochronous stream slave state machine action routines.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_cis_slave.h"
#include "lhci_api.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "wsf_trace.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Check if CIS request parameters are in range or not.
 *
 *  \param      pCtx          ACL connection context.
 *  \param      pCisCtx       CIS connection context.
 *  \param      pCisReq       CIS request parameter.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
static uint8_t LctrCheckCisReq(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx, const lctrCisReq_t *pCisReq)
{
  if ((pCisReq->isoInterval < LL_MIN_ISO_INTERV) ||
      (pCisReq->isoInterval > LL_MAX_ISO_INTERV) ||
      (pCisReq->nse > LL_MAX_CIS_NSE) ||
      (pCisReq->framing > LL_MAX_ISOAL_PDU_TYPE) ||
      (pCisReq->sduSizeMToS > LL_MAX_SDU_SIZE) ||
      (pCisReq->sduSizeSToM > LL_MAX_SDU_SIZE) ||
      (pCisReq->sduIntervalMToS > LL_MAX_SDU_INTERV) ||
      (pCisReq->sduIntervalSToM > LL_MAX_SDU_INTERV) ||
      (pCisReq->plMToS > LL_MAX_CIS_PL) ||
      (pCisReq->plSToM > LL_MAX_CIS_PL) ||
      (pCisReq->phyMToS > (1 << LL_MAX_CIS_PHY_BIT)) ||
      (pCisReq->phySToM > (1 << LL_MAX_CIS_PHY_BIT)) ||
      (pCisReq->ftMToS < LL_MIN_CIS_FT) ||
      (pCisReq->ftMToS > LL_MAX_CIS_FT) ||
      (pCisReq->ftSToM < LL_MIN_CIS_FT) ||
      (pCisReq->ftSToM > LL_MAX_CIS_FT) ||
      (pCisReq->bnMToS > LL_MAX_CIS_BN) ||
      (pCisReq->bnSToM > LL_MAX_CIS_BN))
  {
    LL_TRACE_WARN0("LctrCheckCisReq: invalid parameters, parameter out of range");
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  /* CisOffMaxUsec shall be greater than or equal to CIS_Offset_Min */
  if (pCisReq->cisOffMaxUsec < pCisReq->cisOffMinUsec)
  {
    LL_TRACE_WARN0("LctrCheckCisReq: invalid parameters, cisOffMaxUsec shall be greater than cisOffMinUsec");
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }


  /* CisOffMaxUsec shall be less than (connInterval – ((NSE – 1) × Sub_Interval + MPTM + T_IFS + MPTS + T_MSS)) */
  if (pCisReq->cisOffMaxUsec > (LCTR_CONN_IND_US(pCtx->connInterval) - (pCisReq->nse * pCisReq->subIntervUsec - pLctrRtCfg->cisSubEvtSpaceDelay)))
  {
    LL_TRACE_WARN0("LctrCheckCisReq: invalid parameters, cisOffMaxUsec is too big");
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }
  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Send CIS response PDU.
 *
 *  \param      pCtx          Connection context.
 *  \param      opcode        PDU Opcode.
 */
/*************************************************************************************************/
static void lctrSendCisRspPdu(lctrConnCtx_t *pCtx, uint8_t opcode)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_CIS_RSP_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;
    uint32_t cisOffMinUsec = lctrDataPdu.pld.cisReq.cisOffMinUsec;
    uint32_t cisOffMaxUsec = lctrDataPdu.pld.cisReq.cisOffMaxUsec;
    uint16_t ceRef = lctrDataPdu.pld.cisReq.ceRef;

    if (cisOffMinUsec != cisOffMaxUsec)
    {
      cisOffMinUsec = WSF_MAX(cisOffMinUsec, pCtx->connBod.minDurUsec + BbGetSchSetupDelayUs());

      if (cisOffMinUsec > cisOffMaxUsec)
      {
        /* Cannot schedule the CIS. */
        LL_TRACE_WARN1("lctrSendCisRspPdu: Proposed offset cannot be scheduled within cisOffMaxUsec=%d", cisOffMaxUsec);
        lctrSendRejectInd(pCtx, LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE, TRUE);
        return;
      }
    }

    /*** Assemble control PDU ***/

    UINT8_TO_BSTREAM (pBuf, opcode);

    #if (LL_ENABLE_TESTER)
      if (llTesterCb.cisRspEnabled)
      {
        cisOffMinUsec = llTesterCb.cisOffMinUsec;
        cisOffMaxUsec = llTesterCb.cisOffMaxUsec;
        ceRef         = llTesterCb.cisCeRef;

        llTesterCb.cisRspEnabled = FALSE;
      }
    #endif
    UINT24_TO_BSTREAM (pBuf, cisOffMinUsec);
    UINT24_TO_BSTREAM (pBuf, cisOffMaxUsec);
    UINT16_TO_BSTREAM (pBuf, ceRef);

    /*** Queue for transmit ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send CIS response.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrSendCisRsp(lctrConnCtx_t *pCtx)
{
  lctrSendCisRspPdu(pCtx, LL_PDU_CIS_RSP);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of peer CIS request event.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
static void lctrNotifyHostCisReq(lctrCisCtx_t *pCisCtx)
{
  LlCisReqInd_t evt =
  {
    .hdr =
    {
      .param         = pCisCtx->cisHandle,
      .event         = LL_CIS_REQ_IND,
      .status        = LL_SUCCESS
    }
  };

  evt.aclHandle     = pCisCtx->aclHandle;
  evt.cisHandle     = pCisCtx->cisHandle;
  evt.cigId         = pCisCtx->cigId;
  evt.cisId         = pCisCtx->cisId;

  LL_TRACE_INFO3("### LlEvent ###  LL_CIS_REQ_IND, cisHandle=%u cigId=%u cisId=%u", pCisCtx->cisHandle, pCisCtx->cigId, pCisCtx->cisId);

  bool_t evtSent = LmgrSendEvent((LlEvt_t *)&evt);

  if (!evtSent)
  {
    LlRejectCisReq(pCisCtx->cisHandle, LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE);
    LL_TRACE_WARN1("Host event masked; reply with reason=UNSUPPORTED_REMOTE_FEATURE, cisHandle=%u", pCisCtx->cisHandle);
  }
}

/**************************************************************************************************
  External Functions
**************************************************************************************************/

/**************************************************************************************************
  CIS slave LLCP state machine action functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Action function for received peer CIS request.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrSlvCisLlcpActPeerCisReq(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);

  WSF_ASSERT(pCigCtx);

  if (LctrCheckCisReq(pCtx, pCisCtx, &lctrDataPdu.pld.cisReq) != LL_SUCCESS)
  {
    lctrSendRejectInd(pCtx, LL_ERROR_CODE_INVALID_LMP_PARAMS, TRUE);
    lctrFreeCisCtx(pCisCtx);
    LL_TRACE_WARN1("Invalid CIS request parameters; reply with reason=INVALID_LMP_PARAMS, cisHandle=%u", pCisCtx->cisHandle);
    return;
  }

  if ((lmgrCb.features & LL_FEAT_ISO_HOST_SUPPORT) == 0)
  {
    lctrSendRejectInd(pCtx, LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE, TRUE);
    lctrFreeCisCtx(pCisCtx);
    LL_TRACE_WARN0("Host did not set ISO channel support");
    return;
  }

  lctrCisStorePeerCisReq(pCtx, pCisCtx);

  lctrNotifyHostCisReq(pCisCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received reject peer CIS request.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrSlvCisLlcpActRejCisReq(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrSendRejectInd(pCtx, pLctrConnMsg->rejCisReq.reason, TRUE);

  lctrCleanupCtx(pCisCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received accept peer CIS request.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrSlvCisLlcpActAcpCisReq(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrSendCisRsp(pCtx);
  lctrCisStartLlcpTimer(pCtx, pCisCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received peer CIS indication.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrSlvCisLlcpActPeerCisInd(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  pCisCtx->accessAddr = lctrDataPdu.pld.cisInd.accessAddr;
  pCisCtx->ceRef = lctrDataPdu.pld.cisInd.ceRef;
  pCisCtx->crcInit = pCtx->crcInit;
  pCisCtx->supTimeoutMs = pCtx->supTimeoutMs;
  pCisCtx->offsetUsec = lctrDataPdu.pld.cisInd.cisOffUsec;
  pCisCtx->cigSyncDelayUsec = lctrDataPdu.pld.cisInd.cigSyncDelayUsec;
  pCisCtx->cisSyncDelayUsec = lctrDataPdu.pld.cisInd.cisSyncDelayUsec;

  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);

  WSF_ASSERT(pCigCtx);

  if (pCigCtx->isBodBuilt == FALSE)
  {
    pCigCtx->cigSyncDelayUsec = lctrDataPdu.pld.cisInd.cigSyncDelayUsec;
    pCigCtx->isoInterval = pCisCtx->isoInterval;    /* All CISs have the same ISO interval. */

    lctrCisSetupChanParam(pCisCtx, pCtx->chanMask);
    lctrCisInsertHead(&pCigCtx->list, pCisCtx);
    lctrSlvCisBuildCigOp(pCigCtx);
    lctrCisSetupEncrypt(pCisCtx);
  }
  else
  {
    lctrCisSetupChanParam(pCisCtx, pCtx->chanMask);
    lctrCisInsertTail(&pCigCtx->list, pCisCtx);
    lctrSlvCisBuildCisData(pCisCtx);
    lctrCisSetupEncrypt(pCisCtx);
  }

  /* Calculate the offset for the stream. */
  if (pCisCtx->ceRef - pCtx->eventCounter >= 0x8000)
  {
    LL_TRACE_INFO0("lctrSlvCisLlcpActPeerCisInd, first anchor in the past");
  }
  else
  {
    LL_TRACE_INFO0("lctrSlvCisLlcpActPeerCisInd, first anchor in the future");
    pCisCtx->data.slv.anchorOffsetUsec = (pCisCtx->ceRef - pCtx->eventCounter) * LCTR_CONN_IND_US(pCtx->connInterval) + pCisCtx->offsetUsec;
  }

  if (pCigCtx->isBodStarted == FALSE)
  {
    pCigCtx->packing = LL_PACKING_SEQUENTIAL; /* One stream is the same as sequential. */

    /* Commit BOD */
    lctrSlvCisCigOpCommit(pCigCtx, pCtx, pCisCtx);
    pCisCtx->cisCeRef = 0;
  }
  else
  {
    /* Find the CIS reference event when CIS shall start transfer the CIS. */
    bool_t result = FALSE;
    uint32_t aclRefTime, cigRefTime;

    aclRefTime = pCtx->data.slv.anchorPointUsec + (pCisCtx->ceRef - pCtx->eventCounter + 1) * LCTR_CONN_IND_US(pCtx->connInterval);  /* Reference from the */
    pCisCtx->cisCeRef = 0;

    while (result == FALSE)
    {
      pCisCtx->cisCeRef++;

      if (pCigCtx->roleData.slv.cigEvtCounter == 0)
      {
        /* BOD is not started yet. */
        cigRefTime = pCigCtx->roleData.slv.anchorPointUsec + pCisCtx->cisCeRef * LCTR_ISO_INT_TO_US(pCigCtx->isoInterval);
      }
      else
      {
        cigRefTime = pCigCtx->roleData.slv.anchorPointUsec + (pCisCtx->cisCeRef + 1) * LCTR_ISO_INT_TO_US(pCigCtx->isoInterval);
      }

      if (cigRefTime > aclRefTime)
      {
        result = TRUE;
      }
    }

    uint8_t cisCount = lctrCisGetListCount(&pCigCtx->list);

    /* Set packing scheme once when the second CISs is added, since all the rest follow the same scheme. */
    if (cisCount >= 2)
    {
      lctrCisCtx_t *pHeadCisCtx = lctrCisGetHeadCis(&pCigCtx->list);

      if (cisCount == 2)
      {
        /* If the anchor point of the stream is before end of the first stream, it is interleaved packing scheme. */
        if (BbGetTargetTimeDelta(cigRefTime + pHeadCisCtx->subIntervUsec,
                                 pCisCtx->data.slv.anchorOffsetUsec + pCtx->data.slv.anchorPointUsec + LCTR_CONN_IND_US(pCtx->connInterval)) > 0)
        {
          pCigCtx->packing = LL_PACKING_INTERLEAVED;
        }
      }

      /* Adjust delayUsec for two or more interleaved CISs, one for the original last one and one for the new last one. */
      if (pCigCtx->packing == LL_PACKING_INTERLEAVED)
      {
        lctrCisCtx_t *pCurCisCtx = lctrCisGetHeadCis(&pCigCtx->list);                 /* original last CIS */
        lctrCisCtx_t *pNextCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCurCisCtx);    /* new last last CIS */

        uint32_t prevDelayUsec = 0;

        while (cisCount > 2)
        {
          prevDelayUsec += pCurCisCtx->delayUsec;
          pCurCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCurCisCtx);     /* original last CIS */
          pNextCisCtx = lctrCisGetNextCis(&pCigCtx->list, pNextCisCtx);   /* new last last CIS */

          cisCount--;
        }

        pCurCisCtx->delayUsec = pCisCtx->data.slv.anchorOffsetUsec + pCtx->data.slv.anchorPointUsec + LCTR_CONN_IND_US(pCtx->connInterval) - cigRefTime - prevDelayUsec;
        pNextCisCtx->delayUsec = cigRefTime + pHeadCisCtx->subIntervUsec - (pCisCtx->data.slv.anchorOffsetUsec + pCtx->data.slv.anchorPointUsec + LCTR_CONN_IND_US(pCtx->connInterval));

        LL_TRACE_ERR2("lctrSlvCisLlcpActPeerCisInd, handle=%d pCurCisCtx->delayUsec=%d", pCurCisCtx->cisHandle, pCurCisCtx->delayUsec);
        LL_TRACE_ERR2("lctrSlvCisLlcpActPeerCisInd, handle=%d pNextCisCtx->delayUsec=%d", pNextCisCtx->cisHandle, pNextCisCtx->delayUsec);
      }
      else
      {
        lctrCisCtx_t *pPreCisCtx = lctrCisGetPreCis(&pCigCtx->list, pCisCtx);
        lctrCisCtx_t *pTempCisCtx = pPreCisCtx;

        uint32_t prevDelayUsec = 0;

        while (cisCount > 2)
        {
          pTempCisCtx = lctrCisGetPreCis(&pCigCtx->list, pTempCisCtx);   /* new last last CIS */
          prevDelayUsec += pTempCisCtx->nextCisOffsetUsec;
          cisCount--;
        }

        pPreCisCtx->nextCisOffsetUsec = pCisCtx->cigSyncDelayUsec - pCisCtx->cisSyncDelayUsec - prevDelayUsec;

        LL_TRACE_ERR1("lctrSlvCisLlcpActPeerCisInd, pPreCisCtx->nextCisOffsetUsec=%d", pPreCisCtx->nextCisOffsetUsec);
      }
    }


    /* BOD is already committed, do nothing. */
  }

  lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_EST);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for store peer CIS request.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrCisStorePeerCisReq(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  memcpy(&pCisCtx->data.slv.cisReq, &lctrDataPdu.pld.cisReq, sizeof(lctrCisReq_t));

  pCisCtx->cisId = lctrDataPdu.pld.cisReq.cisId;
  pCisCtx->phyMToS = lctrPhysBitToPhy(lctrDataPdu.pld.cisReq.phyMToS);
  pCisCtx->phySToM = lctrPhysBitToPhy(lctrDataPdu.pld.cisReq.phySToM);
  pCisCtx->framing = lctrDataPdu.pld.cisReq.framing;
  pCisCtx->sduSizeMToS = lctrDataPdu.pld.cisReq.sduSizeMToS;
  pCisCtx->sduSizeSToM = lctrDataPdu.pld.cisReq.sduSizeSToM;
  pCisCtx->sduIntervalMToS = lctrDataPdu.pld.cisReq.sduIntervalMToS;
  pCisCtx->sduIntervalSToM = lctrDataPdu.pld.cisReq.sduIntervalSToM;
  pCisCtx->localDataPdu.maxRxLen = lctrDataPdu.pld.cisReq.plMToS;
  pCisCtx->localDataPdu.maxTxLen = lctrDataPdu.pld.cisReq.plSToM;
  pCisCtx->nse = lctrDataPdu.pld.cisReq.nse;
  pCisCtx->subIntervUsec = lctrDataPdu.pld.cisReq.subIntervUsec;
  pCisCtx->bnMToS = lctrDataPdu.pld.cisReq.bnMToS;
  pCisCtx->bnSToM = lctrDataPdu.pld.cisReq.bnSToM;
  pCisCtx->ftMToS = lctrDataPdu.pld.cisReq.ftMToS;
  pCisCtx->ftSToM = lctrDataPdu.pld.cisReq.ftSToM;
  pCisCtx->isoInterval = lctrDataPdu.pld.cisReq.isoInterval;
  pCisCtx->delayUsec = pCisCtx->subIntervUsec;  /* If LL_PACKING_INTERLEAVED,  delayUsec will be updated in lctrSlvCisLlcpActPeerCisInd */
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received peer CIS rejection.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrSlvCisLlcpActPeerCisRej(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  pCisCtx->reason = lctrDataPdu.pld.rejInd.reason;

  lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_EST_FAIL);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for internal received peer CIS request.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrSlvCisLlcpActIntPeerCisReq(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  if (LctrCheckCisReq(pCtx, pCisCtx, &pCisCtx->data.slv.cisReq) != LL_SUCCESS)
  {
    lctrSendRejectInd(pCtx, LL_ERROR_CODE_INVALID_LMP_PARAMS, TRUE);
    lctrFreeCisCtx(pCisCtx);
    LL_TRACE_ERR1("Invalid CIS request parameters; reply with reason=INVALID_LMP_PARAMS, cisHandle=%u", pCisCtx->cisHandle);
    return;
  }

  if ((lmgrCb.features & LL_FEAT_ISO_HOST_SUPPORT) == 0)
  {
    lctrSendRejectInd(pCtx, LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE, TRUE);
    lctrFreeCisCtx(pCisCtx);
    LL_TRACE_WARN0("Host did not set ISO channel support");
    return;
  }

  lctrCigCtx_t *pCigCtx;
  if ((pCigCtx = lctrFindCigById(lctrDataPdu.pld.cisReq.cigId)) == NULL)
  {
    if ((pCigCtx = lctrAllocCigCtx(lctrDataPdu.pld.cisReq.cigId)) == NULL)
    {
      lctrSendRejectInd(pCtx, LL_ERROR_CODE_LIMIT_REACHED, TRUE);
      lctrFreeCisCtx(pCisCtx);
      LL_TRACE_WARN0("Not able to allocate a CIG context");
      return;
    }
  }

  lctrNotifyHostCisReq(pCisCtx);
}
