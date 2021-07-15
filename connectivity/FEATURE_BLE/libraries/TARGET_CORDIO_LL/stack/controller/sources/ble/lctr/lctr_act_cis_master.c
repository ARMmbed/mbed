/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer connected isochronous stream master state machine action routines.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_cis_master.h"
#include "lctr_api_conn.h"
#include "lctr_api_cis_master.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Calculate the next referenced connection event for the first CIS.
 *
 *  \param      pCtx        Connection context.
 *  \param      pCisCtx     CIS context.
 */
/*************************************************************************************************/
static void lctrCalCeRefFirstCis(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  BbOpDesc_t *pConnBod = &pCtx->connBod;
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  uint32_t refTime;

  pCisCtx->ceRef = pCtx->eventCounter +
                   ((LL_MIN_INSTANT + 1 +     /* +1 for next CE */
                     pCtx->maxLatency) *      /* ensure slave will listen to this packet */
                    pCtx->ecu.srFactor);      /* include subrating factor */
  pCisCtx->cisCeRef = 0;

  refTime = pConnBod->dueUsec + (pCisCtx->ceRef - pCtx->eventCounter) * LCTR_CONN_IND_US(pCtx->connInterval);
  pCisCtx->offsetUsec = SchRmGetOffsetUsec(LCTR_ISO_INT_TO_US(pCigCtx->isoInterval),
                                           LCTR_GET_CIG_RM_HANDLE(pCigCtx), refTime);

  /* Adjust offset make sure CisOffMaxUsec shall be less than
   * (connInterval – ((NSE – 1) × Sub_Interval + MPTM + T_IFS + MPTS + T_MSS)) */
  while (pCisCtx->offsetUsec >= (LCTR_CONN_IND_US(pCtx->connInterval) - (pCisCtx->nse * pCisCtx->subIntervUsec - pLctrRtCfg->cisSubEvtSpaceDelay)))
  {
    pCisCtx->offsetUsec -= LCTR_CONN_IND_US(pCtx->connInterval);
    pCisCtx->ceRef++;
  }

#if (LL_ENABLE_TESTER)
  if (llTesterCb.cisRspEnabled)
  {
    pCisCtx->offsetUsec = llTesterCb.cisOffMinUsec;
    pCisCtx->ceRef     += llTesterCb.cisCeRef;

    llTesterCb.cisRspEnabled = FALSE;
  }
#endif
}

/*************************************************************************************************/
/*!
 *  \brief      Calculate the next referenced connection event for CIS after first one.
 *
 *  \param      pCtx        Connection context.
 *  \param      pCisCtx     CIS context.
 */
/*************************************************************************************************/
static void lctrCalCeRefNotFirstCis(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  BbOpDesc_t *pConnBod = &pCtx->connBod;
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  uint32_t aclRefTime, cigRefTime;
  bool_t      result = FALSE;
  uint32_t offsetUsec = 0;

  pCisCtx->ceRef = pCtx->eventCounter +
                   ((LL_MIN_INSTANT + 1 +     /* +1 for next CE */
                     pCtx->maxLatency) *      /* ensure slave will listen to this packet */
                    pCtx->ecu.srFactor);      /* include subrating factor */

  aclRefTime = pConnBod->dueUsec + (pCisCtx->ceRef - pCtx->eventCounter) * LCTR_CONN_IND_US(pCtx->connInterval);

  /* Find the reference event when CIS shall be transfered. */
  pCisCtx->cisCeRef = 0;

  while (result == FALSE)
  {
    pCisCtx->cisCeRef++;
    cigRefTime = pCigCtx->cigBod.dueUsec + pCisCtx->cisCeRef * LCTR_ISO_INT_TO_US(pCigCtx->isoInterval);

    if (BbGetTargetTimeDelta(cigRefTime, aclRefTime) > 0)
    {
      result = TRUE;
    }
  }

  pCisCtx->offsetUsec = BbGetTargetTimeDelta(cigRefTime, aclRefTime);

  /* Adjust offset make sure CisOffMaxUsec shall be less than
   * (connInterval – ((NSE – 1) × Sub_Interval + MPTM + T_IFS + MPTS + T_MSS)) */
  while (pCisCtx->offsetUsec >= (LCTR_CONN_IND_US(pCtx->connInterval) - (pCisCtx->nse * pCisCtx->subIntervUsec - pLctrRtCfg->cisSubEvtSpaceDelay)))
  {
    pCisCtx->offsetUsec -= LCTR_CONN_IND_US(pCtx->connInterval);
    pCisCtx->ceRef++;
  }

  if (pCigCtx->packing == LL_PACKING_SEQUENTIAL)
  {
    lctrCisNode_t *pTempNode = pCigCtx->list.pHead;
    WSF_ASSERT(pTempNode);   /* There is at least one CIS in the list. */

    while (pTempNode)
    {
      lctrCisCtx_t *pTempCtx = pTempNode->pCisCtx;

      offsetUsec += pTempCtx->subIntervUsec * pTempCtx->nse;
      pTempNode = pTempNode->pNext;
    }

  }
  else if (pCigCtx->packing == LL_PACKING_INTERLEAVED)
  {
    lctrCisNode_t *pTempNode = pCigCtx->list.pHead;
    WSF_ASSERT(pTempNode);   /* There is at least one CIS in the list. */

    while (pTempNode)
    {
      lctrCisCtx_t *pTempCtx = pTempNode->pCisCtx;

      offsetUsec += pTempCtx->delayUsec;
      pTempNode = pTempNode->pNext;
    }
  }
  else
  {
    LL_TRACE_WARN1("Invalid packing scheme=%d", pCigCtx->packing);
  }

  pCisCtx->cisSyncDelayUsec = pCisCtx->cigSyncDelayUsec - offsetUsec;
  pCisCtx->offsetUsec += offsetUsec;
}

/*************************************************************************************************/
/*!
 *  \brief      Check whether the value in the CIS_RSP is valid or not
 *
 *  \param      pCisCtx     CIS context.
 *
 *  \return     TRUE if valid, otherwise FALSE.
 */
/*************************************************************************************************/
static bool_t lctrCheckPeerCisRsp(lctrCisCtx_t *pCisCtx)
{
  if (lctrDataPdu.pld.cisRsp.cisOffMaxUsec < lctrDataPdu.pld.cisRsp.cisOffMinUsec)
  {
    return FALSE;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Send CIS request.
 *
 *  \param      pCtx        Connection context.
 *  \param      pCisCtx     CIS context.
 *  \param      numCis      Total number of CIS in CIG.
 */
/*************************************************************************************************/
static void lctrSendCisReq(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx, uint8_t numCis)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_CIS_REQ_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU ***/

    UINT8_TO_BSTREAM (pBuf, LL_PDU_CIS_REQ);
    UINT8_TO_BSTREAM (pBuf, pCisCtx->cigId);
    UINT8_TO_BSTREAM (pBuf, pCisCtx->cisId);

    /* PHY_Info */
    UINT8_TO_BSTREAM (pBuf, lctrPhyToPhysBit(pCisCtx->phyMToS));
    UINT8_TO_BSTREAM (pBuf, lctrPhyToPhysBit(pCisCtx->phySToM));

    /* SDU_Parameters */
    UINT16_TO_BSTREAM(pBuf, (pCisCtx->sduSizeMToS & 0x0FFF) |
                            ((pCisCtx->framing & 0x01) << 15));     /* Max_SDU_Size_M_To_S | ISOAL_PDU_Type */
    UINT16_TO_BSTREAM(pBuf, (pCisCtx->sduSizeSToM & 0x0FFF));       /* Max_SDU_Size_S_To_M */
    UINT24_TO_BSTREAM(pBuf, (pCisCtx->sduIntervalMToS & 0xFFFFF));  /* SDU_Interval_M_To_S */
    UINT24_TO_BSTREAM(pBuf, (pCisCtx->sduIntervalSToM & 0xFFFFF));  /* SDU_Interval_M_To_S */

    /* CIS_Parameters */
    UINT16_TO_BSTREAM(pBuf, pCisCtx->localDataPdu.maxTxLen);
    UINT16_TO_BSTREAM(pBuf, pCisCtx->localDataPdu.maxRxLen);
    UINT8_TO_BSTREAM (pBuf, pCisCtx->nse);
    UINT24_TO_BSTREAM(pBuf, pCisCtx->subIntervUsec);                /* Subevent interval */
    UINT8_TO_BSTREAM (pBuf, ((pCisCtx->bnSToM & 0x0F) << 4) |
                             (pCisCtx->bnMToS & 0x0F));             /* BN_M_To_S */  /* BN_S_To_M */
    UINT8_TO_BSTREAM (pBuf, pCisCtx->ftMToS);
    UINT8_TO_BSTREAM (pBuf, pCisCtx->ftSToM);
    UINT16_TO_BSTREAM(pBuf, pCisCtx->isoInterval);

    /* CIS_Offset */
    UINT24_TO_BSTREAM(pBuf, pCisCtx->offsetUsec);                   /* CIS_OFFSET_MIN */
    UINT24_TO_BSTREAM(pBuf, pCisCtx->offsetUsec
#if (LL_ENABLE_TESTER)
        + llTesterCb.cisOffMaxUsec
#endif
        );                   /* CIS_OFFSET_MAX */

    /* ACL_Conn_Event_Count */
    UINT16_TO_BSTREAM(pBuf, pCisCtx->ceRef);                        /* Event counter */

    /*** Queue for transmit ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }

  pCisCtx->isCisReqPend = TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Send CIS indication PDU.
 *
 *  \param      pCtx        Connection context.
 *  \param      pCisCtx     CIS context.
 */
/*************************************************************************************************/
static void lctrSendCisInd(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_CIS_IND_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU ***/

    UINT8_TO_BSTREAM (pBuf, LL_PDU_CIS_IND);
    UINT32_TO_BSTREAM(pBuf, pCisCtx->accessAddr);         /* Access address */
    UINT24_TO_BSTREAM(pBuf, pCisCtx->offsetUsec);         /* CIS offset */
    UINT24_TO_BSTREAM(pBuf, pCisCtx->cigSyncDelayUsec);   /* CIG sync delay */
    UINT24_TO_BSTREAM(pBuf, pCisCtx->cisSyncDelayUsec);   /* CIS sync delay */
    UINT16_TO_BSTREAM(pBuf, pCisCtx->ceRef);              /* CE */

    /*** Queue for transmit ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/**************************************************************************************************
  External Functions
**************************************************************************************************/

/**************************************************************************************************
  CIS master LLCP state machine action functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Action function for received host CIS request command.
 *
 *  \param      pCtx      Connection context.
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrMstCisLlcpActHostCisReq(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  WSF_ASSERT(pCigCtx);

  if (pCigCtx->numCisEsted == 0)
  {
    bool_t result = TRUE;

    /* Check whether the parameter for the BOD is valid or not. */
    if (pCigCtx->isValid == FALSE)
    {
      LL_TRACE_WARN0("lctrMstCisLlcpActHostCisReq: Fail to create CIS due to invalid parameters");
      lctrCisStoreLocalLowResourceTerminateReason(pCisCtx);
      result = FALSE;
    }

    /* Check whether the BOD can be scheduled or not. */
    if ((result == TRUE) && (pCigCtx->isRmAdded == FALSE))
    {
      if (!SchRmAdd(LCTR_GET_CIG_RM_HANDLE(pCigCtx),
                    SCH_RM_PREF_PERFORMANCE,
                    LCTR_ISO_INT_TO_US(pCigCtx->isoInterval),
                    LCTR_ISO_INT_TO_US(pCigCtx->isoInterval),
                    pCigCtx->cigSyncDelayUsec,
                    NULL,
                    lctrGetCigRefTime))
      {
        LL_TRACE_WARN0("lctrMstCisLlcpActHostCisReq: Fail to create CIS due to scheduling limitation");
        lctrCisStoreLocalLowResourceTerminateReason(pCisCtx);
        result = FALSE;
      }
      else
      {
        pCigCtx->isRmAdded = TRUE;
      }
    }

    if (!result)
    {
      lctrMsgHdr_t *pTermMsg;

      /* Send SM a terminate event. */
      if ((pTermMsg = (lctrMsgHdr_t *)WsfMsgAlloc(sizeof(*pTermMsg))) != NULL)
      {
        pTermMsg->handle = pCisCtx->aclHandle;
        pTermMsg->dispId = LCTR_DISP_CONN;
        pTermMsg->event = LCTR_CONN_TERM_CIS_LOCAL_RESOURCE;

        WsfMsgSend(lmgrPersistCb.handlerId, pTermMsg);
      }
      return;
    }

    lctrCalCeRefFirstCis(pCtx, pCisCtx);

    lctrCisInsertHead(&pCigCtx->list, pCisCtx);
  }
  else
  {
    lctrCalCeRefNotFirstCis(pCtx, pCisCtx);

    lctrCisInsertTail(&pCigCtx->list, pCisCtx);
  }

  /* Each ACL may have different check CIS link termination function. */
  pCtx->checkCisTerm = lctrCheckForCisLinkTerm;
  pCtx->checkCisEstAcl = lctrCheckIsCisEstAcl;
  lctrCheckCisEstCisFn = lctrCheckIsCisEstCis;

  pCisCtx->aclHandle = pLctrConnMsg->createCis.hdr.handle;    /* Save ACL handle to the CIS context. */
  /* Update CRCInit and supervision timeout from the ACL the CIS is on. */
  pCisCtx->crcInit = pCtx->crcInit;
  pCisCtx->supTimeoutMs = pCtx->supTimeoutMs;

  lctrCisSetupChanParam(pCisCtx, pCtx->chanMask);

  lctrSendCisReq(pCtx, pCisCtx, pCigCtx->roleData.mst.numCis);
  lctrCisStartLlcpTimer(pCtx, pCisCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received peer LL_CIS_REJ_EXT command.
 *
 *  \param      pCtx      Connection context.
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrMstCisLlcpActPeerRej(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  WSF_ASSERT(pCigCtx);

  pCisCtx->isCisReqPend = FALSE;
  lctrCisStoreLlcpPeerRejTerminateReason(pCisCtx);

  lctrMstCreateCisDone(pCisCtx);
  lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_EST_FAIL);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received peer LL_CIS_RSP command.
 *
 *  \param      pCtx      Connection context.
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrMstCisLlcpActPeerCisRsp(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  WSF_ASSERT(pCigCtx);

  if (lctrCheckPeerCisRsp(pCisCtx) == FALSE)
  {
    /* CIS establishment fails when values in the CIS_RSP are invalid. */
    lctrSendRejectInd(pCtx, LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES, TRUE);
    lctrCisStopLlcpTimer(pCtx, pCisCtx);

    pCisCtx->isCisReqPend = FALSE;
    lctrCisStoreLocalLowResourceTerminateReason(pCisCtx);
    lctrCisRemove(&pCigCtx->list, pCisCtx);
    lctrMstCreateCisDone(pCisCtx);

    lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_EST_FAIL);
    return;
  }

  if (pCigCtx->isBodBuilt == FALSE)
  {
    WSF_ASSERT(pCisCtx->cisCeRef == 0);

    lctrMstCisBuildCigOp(pCigCtx);
  }
  else
  {
    lctrMstCisBuildCisData(pCisCtx);
  }

  lctrCisSetupEncrypt(pCisCtx);

  if (pCigCtx->isBodStarted == FALSE)
  {
    /* Commit the BOD and calculate the offset. */
    lctrMstCisCigOpCommit(pCigCtx, pCtx, pCisCtx);
  }
  else
  {
    /* TODO */
    /* BOD is already committed, calculate the offset. */
  }

  pCisCtx->isCisReqPend = FALSE;
  lctrSendCisInd(pCtx, pCisCtx);
  lctrCisStopLlcpTimer(pCtx, pCisCtx);
  lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_EST);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received internal LLCP response timeout.
 *
 *  \param      pCtx      Connection context.
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrMstCisLlcpActRspTimeout(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  WSF_ASSERT(pCigCtx);

  lctrCisStopLlcpTimer(pCtx, pCisCtx);
  lctrCisStoreLlcpTimeoutTerminateReason(pCisCtx);
  lctrCisRemove(&pCigCtx->list, pCisCtx);
  lctrMstCreateCisDone(pCisCtx);
  lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_EST_FAIL);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received internal LLCP reject.
 *
 *  \param      pCtx      Connection context.
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrMstCisLlcpActLocalReject(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrMstCreateCisDone(pCisCtx);
  lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_EST_FAIL);
}
