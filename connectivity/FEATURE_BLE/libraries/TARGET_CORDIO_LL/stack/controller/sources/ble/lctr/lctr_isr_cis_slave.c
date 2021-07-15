/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave connected isochronous stream ISR callbacks.
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

#include "lctr_int.h"
#include "lctr_int_cis.h"
#include "lctr_int_cis_slave.h"
#include "lctr_int_iso.h"
#include "lmgr_api_conn.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "bb_ble_api.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_os.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>
#include "pal_codec.h"

#if (LL_ENABLE_TESTER)
#include "pal_bb_ble_tester.h"
#endif

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

#if (LL_ENABLE_TESTER)
void LctrCisProcessRxTxAck(lctrCisCtx_t *pCtx, bool_t *pValidRx, bool_t *pTxPduIsAcked);
#endif

/*************************************************************************************************/
/*!
 *  \brief  Initialize slave CIS ISR resources.
 *
 *  \param  pCisCtx    CIS Context.
 */
/*************************************************************************************************/
static void lctrSlvCisInitIsr(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->data.slv.syncWithMaster = FALSE;
  pCisCtx->data.slv.rxFromMaster = FALSE;
  pCisCtx->data.slv.firstRxFromMaster = TRUE;
  pCisCtx->data.slv.rxStatus = BB_STATUS_RX_TIMEOUT;
  pCisCtx->subEvtCounter = 0;
  pCisCtx->txDataCounter = 0;
  pCisCtx->rxDataCounter = 0;

  /* List may be empty if BN M_S is 0 */
  if (pCisCtx->rxFtParamList.pHead != NULL)
  {
    pCisCtx->rxFtParamList.pHead->ftParam.subEvtCounter = 0;
  }

  /* List may be empty if BN S_M is 0 */
  if (pCisCtx->txFtParamList.pHead != NULL)
  {
    pCisCtx->txFtParamList.pHead->ftParam.subEvtCounter = 0;

    if (pCisCtx->txFtParamList.pHead->ftParam.bn == 0)
    {
      pCisCtx->isTxDone = TRUE;
    }
    else
    {
      pCisCtx->isTxDone = FALSE;
    }
  }
  else
  {
    pCisCtx->isTxDone = TRUE;
  }

  /*** Setup for transmit ***/

  pCisCtx->txHdr.cie  = 0;
  pCisCtx->txHdr.np   = 0;

  /*** Setup for receive ***/

  pCisCtx->rxHdr.cie  = 0;
  pCisCtx->rxHdr.np   = 0;
  pCisCtx->rxHdr.len  = 0;
  pCisCtx->rxHdr.llid = LL_LLID_ISO_UNF_END_PDU;
}

/*************************************************************************************************/
/*!
 *  \brief  Execution CIG operation
 *
 *  \param  pCisCtx     CIS Context.
 */
/*************************************************************************************************/
static void lctrSlvCisCigExecOp(lctrCisCtx_t *pCisCtx)
{
  uint8_t *pBuf;

  if (!lmgrIsoCb.availRxBuf)
  {
    LL_TRACE_ERR1("!!! lctrSlvCisCigExecOp, RX buffer not available, cisHandle=%u", pCisCtx->cisHandle);
    return;                    /* flow control Rx */
  }

  /*** Setup receiver ***/

  if ((pBuf = lctrCisRxPduAlloc()) != NULL)
  {
#if (LL_ENABLE_TESTER)
    if (llTesterCb.isoAccAddrSeTrigMask &&
        (llTesterCb.isoAccAddrSeTrigMask & (1 << pCisCtx->subEvtCounter)) &&
        llTesterCb.isoAccAddrInvForRx)
    {
      PalBbTesterInvalidateNextAccAddr(TRUE);

      if (llTesterCb.isoAccAddrInvNumTrig)
      {
        llTesterCb.isoAccAddrInvNumTrig--;
        if (llTesterCb.isoAccAddrInvNumTrig == 0)
        {
          llTesterCb.isoAccAddrSeTrigMask = 0;
        }
      }
    }
#endif

    lctrSetBbCisPacketCounterRx(pCisCtx);
    BbBleCisRxData(pBuf, LCTR_CIS_DATA_PDU_LEN(pCisCtx->localDataPdu.maxRxLen));
    /* Rx may fail; no more important statements in this code path */
  }
  else
  {
    LL_TRACE_ERR1("!!! OOM while initializing receive buffer at start of CIS, cisHandle=%u", pCisCtx->cisHandle);
    BbSetBodTerminateFlag();
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check Tx flush timeout and flush PDU if necessary after RX.
 *
*  \param  pCisCtx         CIS context.
 */
/*************************************************************************************************/
static void lctrCisCheckTxFtAfterRx(lctrCisCtx_t *pCisCtx)
{
  if (lctrCisFtIsListEmpty(&pCisCtx->txFtParamList))
  {
    /* List is empty. */
    return;
  }

  if (pCisCtx->data.slv.lastTxNull)
  {
    /* NULL PDU doesn't need to be acked or flushed. */
    return;
  }

  lctrFtParam_t *pFtParam = &pCisCtx->txFtParamList.pHead->ftParam;

  if (pFtParam->pduAcked)
  {
    pFtParam->isPduDone[pFtParam->pduCounter] = TRUE;       /* The PDU is done. */
    lctrCisIncPacketCounterTx(pCisCtx);
    pFtParam->pduCounter++;
  }
  else
  {
    /* Check if this is the last FT interval. */
    if ((pFtParam->intervalCounter + 1) == pFtParam->intervalTotal)
    {
      if (pFtParam->subEvtCounter == pFtParam->lastSubEvtFt[pFtParam->pduCounter])
      {
        /* PDU needs to be flushed. */
        pCisCtx->dataSn++;

        lctrCisIncPacketCounterTx(pCisCtx);
        pFtParam->isPduDone[pFtParam->pduCounter] = TRUE;       /* The PDU is done. */
        pFtParam->pduCounter++;
        pCisCtx->isoLinkQualStats.txUnAckPkt++;

        if (pFtParam->pduType[pFtParam->pduCounter] == LCTR_CIS_PDU_NON_EMPTY)
        {
          /* Need to remove from ack queue if non-empty PDU. */
          lctrCisTxPduAck(pCisCtx);
          pCisCtx->pduFlushed = TRUE;   /* Set the flag, lctrCisProcessTxAckCleanup will be called to do the cleanup later in the lctrSlvCisCigPostSubEvt. */
        }
      }
      else /* Implies that a retransmit will happen. */
      {
        pCisCtx->isoLinkQualStats.retransmitPkt++;
      }
    }
    else /* Implies that a retransmit will happen. */
    {
      pCisCtx->isoLinkQualStats.retransmitPkt++;
    }
  }

  /* Remove the node if all the items are done. */
  bool_t removeFromList = TRUE;
  for (unsigned int i = 0; i < WSF_MIN(pFtParam->bn, LCTR_MAX_BN); i++)
  {
    if (pFtParam->isPduDone[i] == FALSE)
    {
      removeFromList = FALSE;
      break;
    }
  }
  if (removeFromList == TRUE)
  {
    lctrCisFtRemoveHead(&pCisCtx->txFtParamList);
  }

  /* No more data to send, send NULL packet. */
  if (lctrCisFtIsListEmpty(&pCisCtx->txFtParamList))
  {
    pCisCtx->isTxDone = TRUE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check Rx flush timeout and flush PDU if necessary after RX.
 *
*  \param  pCisCtx         CIS context.
 */
/*************************************************************************************************/
static void lctrCisCheckRxFtAfterRx(lctrCisCtx_t *pCisCtx)
{
  if (lctrCisFtIsListEmpty(&pCisCtx->rxFtParamList))
  {
    /* List is empty. */
    return;
  }

  lctrFtParam_t *pFtParam = &pCisCtx->rxFtParamList.pHead->ftParam;

  if (pFtParam->pduRcved)
  {
    pFtParam->isPduDone[pFtParam->pduCounter] = TRUE;       /* The PDU is done. */
    pFtParam->pduCounter++;
  }
  else
  {
    /* Check if this is the last interval before FT. */
    if ((pFtParam->intervalCounter + 1) == pFtParam->intervalTotal)
    {
      if (pFtParam->subEvtCounter == pFtParam->lastSubEvtFt[pFtParam->pduCounter])
      {
        /* PDU needs to be flushed. */
        pCisCtx->txHdr.nesn++;
        lctrCisIncPacketCounterRx(pCisCtx);
        pFtParam->isPduDone[pFtParam->pduCounter] = TRUE;       /* The PDU is done. */
        pFtParam->pduCounter++;
        pCisCtx->isoLinkQualStats.rxUnreceivedPkt++;
        pCisCtx->isoalRxCtx.pduFlushed = TRUE;
        pCisCtx->isoalRxCtx.packetSequence++;
        pCisCtx->numRxMissed++;
        if (pCisCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED)
        {
          /* Indicate SDU has missing data. */
          pCisCtx->isoalRxCtx.data.unframed.ps = LCTR_PS_INVALID;
        }
      }
    }
  }

  /* Remove the node if all the items are done. */
  bool_t removeFromList = TRUE;
  for (unsigned int i = 0; i < WSF_MIN(pFtParam->bn, LCTR_MAX_BN); i++)
  {
    if (pFtParam->isPduDone[i] == FALSE)
    {
      removeFromList = FALSE;
      break;
    }
  }
  if (removeFromList == TRUE)
  {
    lctrCisFtRemoveHead(&pCisCtx->rxFtParamList);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check flush timeout and flush PDU if necessary after end of BOD.
 *
 *  \param  pCisCtx    CIS context.
 */
/*************************************************************************************************/
static void lctrCisCheckTxFtAfterBod(lctrCisCtx_t *pCisCtx)
{
  if (lctrCisFtIsListEmpty(&pCisCtx->txFtParamList))
  {
    /* List is empty. */
    return;
  }

  lctrFtParamNode_t *pNode = pCisCtx->txFtParamList.pHead;

  /* Increment interval counter for all the node in the list. */
  while (pNode)
  {
    pNode->ftParam.intervalCounter++;
    pNode = pNode->pNext;
  }

  /* Only check the head node */
  lctrFtParam_t *pFtParam = &pCisCtx->txFtParamList.pHead->ftParam;
  for (unsigned int i = 0; i < WSF_MIN(pFtParam->bn, LCTR_MAX_BN); i++)
  {
    if (pFtParam->isPduDone[i] == FALSE &&                          /* If the PDU is not done. */
        pFtParam->intervalCounter == pFtParam->intervalTotal)      /* Check if the PDU needs to be flush in this interval. */
    {
      pFtParam->isPduDone[i] = TRUE;
      pCisCtx->dataSn++;

      if (pFtParam->pduType[i] == LCTR_CIS_PDU_NON_EMPTY)
      {
        /* Need to remove from ack queue if non-empty PDU. */
        lctrCisTxPduAck(pCisCtx);
        lctrCisTxQueuePopCleanup(pCisCtx);
      }
      pFtParam->pduCounter++;

      lctrCisIncPacketCounterTx(pCisCtx);
    }
  }

  /* Remove the node if all the items are done. */
  bool_t removeFromList = TRUE;
  for (unsigned int i = 0; i < WSF_MIN(pFtParam->bn, LCTR_MAX_BN); i++)
  {
    if (pFtParam->isPduDone[i] == FALSE)
    {
      removeFromList = FALSE;
      break;
    }
  }
  if (removeFromList == TRUE)
  {
    /* pCisCtx->isTxDone = TRUE; No need since BOD is done. */
    lctrCisFtRemoveHead(&pCisCtx->txFtParamList);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check flush timeout and flush PDU if necessary after end of BOD.
 *
 *  \param  pCisCtx    CIS Context.
 */
/*************************************************************************************************/
static void lctrCisCheckRxFtAfterBod(lctrCisCtx_t *pCisCtx)
{
  if (lctrCisFtIsListEmpty(&pCisCtx->rxFtParamList))
  {
    /* List is empty. */
    return;
  }

  lctrFtParamNode_t *pNode = pCisCtx->rxFtParamList.pHead;

  /* Increment interval counter for all the node in the list. */
  while (pNode)
  {
    pNode->ftParam.intervalCounter++;
    pNode = pNode->pNext;
  }

  /* Only check the head node */
  lctrFtParam_t *pFtParam = &pCisCtx->rxFtParamList.pHead->ftParam;
  for (unsigned int i = 0; i < WSF_MIN(pFtParam->bn, LCTR_MAX_BN); i++)
  {
    if (pFtParam->isPduDone[i] == FALSE &&                          /* If the PDU is not done. */
        pFtParam->intervalCounter == pFtParam->intervalTotal)      /* Check if the PDU needs to be flush in this interval. */
    {
      pFtParam->isPduDone[i] = TRUE;
      pCisCtx->txHdr.nesn++;
      lctrCisIncPacketCounterRx(pCisCtx);
      pFtParam->pduCounter++;
      pCisCtx->isoalRxCtx.pduFlushed = TRUE;
    }
  }

  /* Remove the node if all the items are done. */
  bool_t removeFromList = TRUE;
  for (unsigned int i = 0; i < WSF_MIN(pFtParam->bn, LCTR_MAX_BN); i++)
  {
    if (pFtParam->isPduDone[i] == FALSE)
    {
      removeFromList = FALSE;
      break;
    }
  }
  if (removeFromList == TRUE)
  {
    lctrCisFtRemoveHead(&pCisCtx->rxFtParamList);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check flush timeout and flush PDU if necessary after RX.
 *
*  \param  pCisCtx         CIS context.
 */
/*************************************************************************************************/
static void lctrCisCheckFtAfterRx(lctrCisCtx_t *pCisCtx)
{
  lctrCisCheckTxFtAfterRx(pCisCtx);
  lctrCisCheckRxFtAfterRx(pCisCtx);
}

/*************************************************************************************************/
/*!
 *  \brief  Check flush timeout and flush PDU if necessary after end of BOD.
 *
 *  \param  pCisCtx    CIS context.
 */
/*************************************************************************************************/
static void lctrCisCheckFtAfterBod(lctrCisCtx_t *pCisCtx)
{
  lctrCisCheckTxFtAfterBod(pCisCtx);
  lctrCisCheckRxFtAfterBod(pCisCtx);
}

/**************************************************************************************************
  External Functions
**************************************************************************************************/
/*************************************************************************************************/
/*!
 *  \brief  Setup for the next current operation.
 *
 *  \param  pOp         Begin operation.
 *  \param  pNewCisCtx  New CIS context to begin.
 *
 *  \return zero.
 */
/*************************************************************************************************/
uint32_t lctrSlvCisCheckContOpPostCback(BbOpDesc_t *pOp, bool_t *pNewCisCtx)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx = pCigCtx->pCisCtx;
  BbBleData_t *pBle = &pCisCtx->bleData;

  if (*pNewCisCtx == FALSE)
  {
    return 0;
  }

  if (pCigCtx->packing == LL_PACKING_INTERLEAVED)
  {
    if ((pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx)) == NULL)
    {
      /* End of the list, loop back to the head of the CIS. */
      pCisCtx = lctrCisGetHeadCis(&pCigCtx->list);
      pCigCtx->isLoopBack = TRUE;

      pBle = &pCisCtx->bleData;

      /* Point to the next CIS. */
      pOp->prot.pBle = pBle;
      pCigCtx->pCisCtx = pCisCtx;
      memcpy(&pBle->chan, &pCigCtx->pCisCtx->bleData.chan, sizeof(pBle->chan));
      pBle->chan.chanIdx = pCisCtx->nextSubEvtChanIdx;  /* Next subevent channel index is pre-calculated. */

      /* For interleaved CIS there is always a switch to a new CisCtx, so nextSubEvtChanIdx is not updated in bbMstCisRxCompCback()*/
      if (pCisCtx->subEvtCounter <= pCisCtx->nse)
      {
        lctrSlvCisCigPostSubEvt(pOp, BB_STATUS_SUCCESS);
      }
    }
    else
    {
      if (pCigCtx->isLoopBack == FALSE)
      {
        lctrFtParam_t txFtParam, rxFtParam;

        if (pCisCtx->bnSToM)
        {
          lctrCisInitFtParam(&txFtParam, pCisCtx->bnSToM, pCisCtx->ftSToM, pCisCtx->nse);
          (void)lctrCisFtInsertTail(&pCisCtx->txFtParamList, &txFtParam); /* Assume there is memory. */
        }

        if (pCisCtx->bnMToS)
        {
          lctrCisInitFtParam(&rxFtParam, pCisCtx->bnMToS, pCisCtx->ftMToS, pCisCtx->nse);
          (void)lctrCisFtInsertTail(&pCisCtx->rxFtParamList, &rxFtParam); /* Assume there is memory. */
        }

        lctrSlvCisInitIsr(pCisCtx);
      }

      pBle = &pCisCtx->bleData;

      /* Point to the next CIS. */
      pOp->prot.pBle = pBle;
      pCigCtx->pCisCtx = pCisCtx;
      memcpy(&pBle->chan, &pCigCtx->pCisCtx->bleData.chan, sizeof(pBle->chan));
      if (pCigCtx->isLoopBack == FALSE)
      {
        pBle->chan.chanIdx = pCigCtx->pCisCtx->chIdx;     /* Set the next channel to the first channel in the next CIS. */
      }
      else
      {
        pBle->chan.chanIdx = pCisCtx->nextSubEvtChanIdx;  /* Next subevent channel index is pre-calculated. */

        /* For interleaved CIS there is always a switch to a new CisCtx, so nextSubEvtChanIdx is not updated in bbMstCisRxCompCback()*/
        if (pCisCtx->subEvtCounter <= pCisCtx->nse)
        {
          lctrSlvCisCigPostSubEvt(pOp, BB_STATUS_SUCCESS);
        }
      }
    }
  }

  return 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Setup for the next current operation.
 *
 *  \param  pOp         Begin operation.
 *  \param  pNewCisCtx  New CIS context to begin.
 *
 *  \return Offset in usec.
 */
/*************************************************************************************************/
uint32_t lctrSlvCisCheckContOp(BbOpDesc_t *pOp, bool_t *pNewCisCtx)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx = pCigCtx->pCisCtx;
  uint32_t wwTotalUsec;
  uint32_t offsetUsec;
  uint32_t delayUsec = pCisCtx->delayUsec;
  bool_t isSuccess = (pCisCtx->data.slv.rxStatus == BB_STATUS_SUCCESS ? TRUE : FALSE);

  WSF_ASSERT(pCisCtx);

  BbBleData_t *pBle = &pCisCtx->bleData;
  BbBleSlvCisEvent_t * const pCis = &pBle->op.slvCis;

  if (pCigCtx->packing == LL_PACKING_SEQUENTIAL)
  {
    uint32_t subIntervUsec = pCisCtx->subIntervUsec;

    pCisCtx->subEvtCounter++;

    if (pCisCtx->rxHdr.cie || pCisCtx->txHdr.cie)
    {
      goto SwitchSeq;
    }

    /* Check whether the CIS shall start or not. */
    if (pCisCtx->cisCeRef != 0)
    {
      goto SwitchSeq;
    }

    if (pCisCtx->subEvtCounter == pCisCtx->nse)
    {
      pCisCtx->txHdr.cie = 1;
      /* There is no more subevent. */
      goto SwitchSeq;
    }

    /*** Continue current CIS ***/

    /* Set next channel index. */
    pBle->chan.chanIdx = pCisCtx->nextSubEvtChanIdx;      /* Next subevent channel index is pre-calculated. */

    /* Only apply WW when rx success. */
    if (isSuccess)
    {
      wwTotalUsec = lctrCalcWindowWideningUsec(subIntervUsec, pCigCtx->roleData.slv.totalAcc);
      offsetUsec = subIntervUsec - wwTotalUsec;
      pCis->rxSyncDelayUsec = (wwTotalUsec << 1);
    }
    else
    {
      offsetUsec = subIntervUsec;
    }
    return offsetUsec;

SwitchSeq:
    /* Skip additional sub-events if switching CIS, might not be used if next CIS is NULL */
    subIntervUsec = pCisCtx->nextCisOffsetUsec - subIntervUsec * (pCisCtx->subEvtCounter - 1);  /* pCisCtx->subEvtCounter is already incremented */

    if ((pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx)) == NULL)
    {
      goto Done;
    }
    else
    {
      /* Check whether the CIS shall start or not. */
      if (pCisCtx->cisCeRef != 0)
      {
        goto Done;
      }

      lctrFtParam_t txFtParam, rxFtParam;

      if (pCisCtx->bnSToM)
      {
        lctrCisInitFtParam(&txFtParam, pCisCtx->bnSToM, pCisCtx->ftSToM, pCisCtx->nse);
        (void)lctrCisFtInsertTail(&pCisCtx->txFtParamList, &txFtParam); /* Assume there is memory. */
      }

      if (pCisCtx->bnMToS)
      {
        lctrCisInitFtParam(&rxFtParam, pCisCtx->bnMToS, pCisCtx->ftMToS, pCisCtx->nse);
        (void)lctrCisFtInsertTail(&pCisCtx->rxFtParamList, &rxFtParam); /* Assume there is memory. */
      }

      lctrSlvCisInitIsr(pCisCtx);

      pBle = &pCisCtx->bleData;
      *pNewCisCtx = TRUE;

      /* Continue on the next subevent. */
      pBle->op.slvCis.rxTsUsec = pCis->rxTsUsec;

      /* Move the current CIS to the next CIS. */
      pOp->prot.pBle = pBle;
      pCigCtx->pCisCtx = pCisCtx;
      memcpy(&pBle->chan, &pCigCtx->pCisCtx->bleData.chan, sizeof(pBle->chan));
      pBle->chan.chanIdx = pCigCtx->pCisCtx->chIdx;   /* Set the next channel to the first channel in the next CIS. */

      /* Only apply WW when rx success. */
      if (isSuccess)
      {
        wwTotalUsec = lctrCalcWindowWideningUsec(subIntervUsec, pCigCtx->roleData.slv.totalAcc);
        offsetUsec = subIntervUsec - wwTotalUsec;
        pCis->rxSyncDelayUsec = (wwTotalUsec << 1);
      }
      else
      {
        offsetUsec = subIntervUsec;
      }

      return offsetUsec;
    }
  }
  else
  {
    uint32_t subIntervUsec = pCisCtx->subIntervUsec;
    uint8_t oriNextSubEvtChanIdx = pCisCtx->nextSubEvtChanIdx;

    if (pCisCtx->rxHdr.cie || pCisCtx->txHdr.cie)
    {
      pCisCtx->cisDone = TRUE;
      goto SwitchInt;
    }

    /* Check whether the CIS shall start or not. */
    if (pCisCtx->cisCeRef != 0)
    {
      lctrCisSetCisDone(&pCigCtx->list, pCisCtx);
      goto SwitchInt;
    }

    pCisCtx->subEvtCounter++;

    if (pCisCtx->subEvtCounter == pCisCtx->nse)
    {
      pCisCtx->txHdr.cie = 1;
      pCisCtx->cisDone = TRUE;
      /* There is no more subevent. */
      goto SwitchInt;
    }

SwitchInt:
    if (lctrCisGetListCount(&pCigCtx->list) == 1)
    {
      if (lctrCisAreCisCtxDone(&pCigCtx->list))
      {
        goto Done;
      }

      /*** Continue current CIS ***/

      /* Set next channel index. */
      pBle->chan.chanIdx = oriNextSubEvtChanIdx;        /* Next subevent channel index is pre-calculated. */
      /* Only apply WW when rx success. */
      if (isSuccess)
      {
        wwTotalUsec = lctrCalcWindowWideningUsec(subIntervUsec, pCigCtx->roleData.slv.totalAcc);
        offsetUsec = subIntervUsec - wwTotalUsec;
        pCis->rxSyncDelayUsec = (wwTotalUsec << 1);
      }
      else
      {
        offsetUsec = subIntervUsec;
      }

      return offsetUsec;
    }

    if ((pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx)) == NULL)
    {
      if (lctrCisAreCisCtxDone(&pCigCtx->list))
      {
        goto Done;
      }

      *pNewCisCtx = TRUE;
      /* Processing will continue in the lctrSlvCisCheckContOpPostCback() */

      /* Only apply WW when rx success. */
      if (isSuccess)
      {
        wwTotalUsec = lctrCalcWindowWideningUsec(delayUsec, pCigCtx->roleData.slv.totalAcc);
        offsetUsec = delayUsec - wwTotalUsec;
        pCis->rxSyncDelayUsec = (wwTotalUsec << 1);
      }
      else
      {
        offsetUsec = delayUsec;
      }

      return offsetUsec;
    }
    else
    {
      /* Get the next CIS in the list. */
      if (lctrCisAreCisCtxDone(&pCigCtx->list))
      {
        goto Done;
      }

      /* Check whether the CIS shall start or not. */
      if (pCisCtx->cisCeRef != 0)
      {
        /*** Continue head CIS ***/

        lctrCisSetCisDone(&pCigCtx->list, pCisCtx);

        lctrCisCtx_t *pNextCtx, *pTempCtx;
        uint32_t addDelayUsec = 0;

        pTempCtx = lctrCisGetHeadCis(&pCigCtx->list);
        pCigCtx->pCisCtx = pTempCtx;
        pBle = &pTempCtx->bleData;
        memcpy(&pBle->chan, &pTempCtx->bleData.chan, sizeof(pBle->chan));
        pBle->chan.chanIdx = pTempCtx->nextSubEvtChanIdx;        /* Next subevent channel index is pre-calculated. */
        pOp->prot.pBle = pBle;
        pCigCtx->isLoopBack = TRUE;

        while (pTempCtx)
        {
          pNextCtx = lctrCisGetNextCis(&pCigCtx->list, pTempCtx);

          if (pNextCtx == NULL)
          {
            break;
          }

          if (pNextCtx == pCisCtx)
          {
            break;
          }

          addDelayUsec += pTempCtx->delayUsec;
          pTempCtx = pNextCtx;
        }

        /* Only apply WW when Rx success. */
        if (isSuccess)
        {
          wwTotalUsec = lctrCalcWindowWideningUsec(subIntervUsec, pCigCtx->roleData.slv.totalAcc);
          offsetUsec = subIntervUsec - wwTotalUsec;
          pCis->rxSyncDelayUsec = (wwTotalUsec << 1);
        }
        else
        {
          offsetUsec = subIntervUsec;
        }

        return (offsetUsec - addDelayUsec);
      }

      *pNewCisCtx = TRUE;
      /* Processing will continue in the lctrSlvCisCheckContOpPostCback() */

      /* Only apply WW when rx success. */
      if (isSuccess)
      {
        wwTotalUsec = lctrCalcWindowWideningUsec(delayUsec, pCigCtx->roleData.slv.totalAcc);
        offsetUsec = delayUsec - wwTotalUsec;
        pCis->rxSyncDelayUsec = (wwTotalUsec << 1);
      }
      else
      {
        offsetUsec = delayUsec;
      }
      return offsetUsec;
    }
  }

Done:
  /* All CISs are done. Set next CIS to the head of the list. */
  pCigCtx->pCisCtx = lctrCisGetHeadCis(&pCigCtx->list);
  pOp->prot.pBle = &pCigCtx->pCisCtx->bleData;
  *pNewCisCtx = TRUE;

  lctrCisClearCisDone(&pCigCtx->list);
  return 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Cleanup a connection operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrSlvCisCigCleanupOp(BbOpDesc_t *pOp)
{

}

/*************************************************************************************************/
/*!
 *  \brief  End a connection operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrSlvCisCigEndOp(BbOpDesc_t *pOp)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx  = lctrCisGetHeadCis(&pCigCtx->list);
  lctrConnCtx_t *pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);
  WSF_ASSERT(pCisCtx);  /* At least one CIS in the CIG. */

  if (pCisCtx->data.slv.syncWithMaster)
  {
    /* Re-sync to master's clock. */
    pCigCtx->roleData.slv.anchorPointUsec = pCisCtx->data.slv.firstRxStartTsUsec;
    pCigCtx->roleData.slv.lastActiveEvent = pCigCtx->roleData.slv.cigEvtCounter;

    /* Reset ISR variables. */
    pCisCtx->data.slv.syncWithMaster = FALSE;
  }

  while (pCisCtx)
  {
    /* Check for unframed flushed PDUs. */
    if ((pCisCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED) &&
        (pCisCtx->isoalRxCtx.pduFlushed == TRUE))
    {
      pCisCtx->isoalRxCtx.pduFlushed = FALSE;
      lctrCisCheckUnframedFlush(pCisCtx);
    }

    /* LL_CIS_TERMINATION case */
    if ((pCisCtx->isClosing == TRUE) ||
        (pConnCtx->enabled == FALSE) ||
        (lctrResetEnabled == TRUE))
    {
      /* Notify host when terminate happens unless reset occurred. */
      if (lctrResetEnabled == FALSE)
      {
        if (pConnCtx->enabled == FALSE)
        {
          pCisCtx->reason = pConnCtx->termReason;
        }
        /* If host initiates the termination, the reason code shall be HCI_ERR_LOCAL_TERMINATED in the disconnect complete event. */
        else if (pCisCtx->hostInitTerm)
        {
          pCisCtx->reason = HCI_ERR_LOCAL_TERMINATED;
          pCisCtx->hostInitTerm = FALSE;
        }

        lctrNotifyHostCisTerm(pCisCtx);
      }

      pCigCtx->numCisEsted--;
      pCisCtx->isClosing = FALSE;
      lctrCleanupCtx(pCisCtx);
    }

    pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx);
  }

  if (pCigCtx->numCisEsted == 0)
  {
    return;
  }

  pCisCtx  = lctrCisGetHeadCis(&pCigCtx->list);

  while (pCisCtx)
  {
    lctrCisCheckFtAfterBod(pCisCtx);

    if (!pCisCtx->connEst && pCisCtx->data.slv.rxFromMaster)
    {
      WsfTimerStartMs(&pCisCtx->tmrSupTimeout, pCisCtx->supTimeoutMs);
      pCisCtx->connEst = TRUE;

      if (pCisCtx->powerIndReq && lctrSendPowerChangeIndCback)
      {
        uint8_t txPhy = (pCisCtx->role == LL_ROLE_MASTER) ? pCisCtx->phyMToS : pCisCtx->phySToM;
        lctrSendPowerChangeIndCback(pConnCtx, txPhy, 0, pLctrRtCfg->defTxPwrLvl, TRUE);
      }

      lctrNotifyHostCisEst(pCisCtx, LL_SUCCESS, pCisCtx->cigSyncDelayUsec);
    }
    else if (pCisCtx->data.slv.rxFromMaster)
    {
      /* Reset supervision timer. */
      WsfTimerStartMs(&pCisCtx->tmrSupTimeout, pCisCtx->supTimeoutMs);
    }

    /* Failed to receive packet within the first 6 intervals. */
    if ((pCisCtx->firstFromPeer == FALSE) && (pCisCtx->cisEvtCounter == LCTR_FAST_TERM_CNT))
    {
      LL_TRACE_WARN0("CIS terminated due to fast termination timeout");
      lctrCisStoreConnFailEstablishTerminateReason(pCisCtx);
      lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_CLOSED);

      if (pCigCtx->numCisEsted == 1)
      {
        /* If the last CIS to close, schedule no more operation. */
        return;
      }
    }

    /* Terminate connection */
    if (lctrCheckForCisLinkTerm(pCisCtx->aclHandle))
    {
      lctrSendCisLlcpMsg(pCisCtx, LCTR_CIS_TERM_EVENT_CIS_TERM);
    }

    pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx);
  }

  pCisCtx  = lctrCisGetHeadCis(&pCigCtx->list);

  if (pCigCtx->headCisRmved == TRUE)
  {
    BbBleData_t *pBle = &pCisCtx->bleData;
    /* Move the current CIS to the head CIS. */
    pOp->prot.pBle = pBle;
    pCigCtx->pCisCtx = pCisCtx;
    memcpy(&pBle->chan, &pCigCtx->pCisCtx->bleData.chan, sizeof(pBle->chan));
    pBle->chan.chanIdx = pCigCtx->pCisCtx->chIdx;   /* Set the next channel to the first channel in the next CIS. */
  }

  uint16_t numUnsyncIntervals = pCigCtx->roleData.slv.cigEvtCounter - pCigCtx->roleData.slv.lastActiveEvent;

  /* Update txPower. */
  pCisCtx->bleData.chan.txPower = LCTR_GET_TXPOWER(pConnCtx, pCisCtx->phySToM, pCisCtx->bleData.chan.initTxPhyOptions);
#if(LL_ENABLE_TESTER)
  pCisCtx->bleData.chan.txPower += pConnCtx->bleData.chan.txPwrOffset;
#endif

  while (TRUE)
  {
    pCigCtx->roleData.slv.cigEvtCounter++;
    numUnsyncIntervals += 1;

    uint32_t cigInterUsec   = LCTR_ISO_INT_TO_US(pCigCtx->isoInterval);
    uint32_t unsyncTimeUsec = cigInterUsec * numUnsyncIntervals;
    uint32_t wwTotalUsec    = lctrCalcWindowWideningUsec(unsyncTimeUsec, pCigCtx->roleData.slv.totalAcc);

    pCisCtx = lctrCisGetHeadCis(&pCigCtx->list);
    WSF_ASSERT(pCisCtx);  /* At least one CIS in the CIG. */

    while (pCisCtx)
    {
      if (pCisCtx->cisCeRef == 0)
      {
        BbBleData_t * const pBle = &pCisCtx->bleData;
        pCisCtx->cisEvtCounter++;
        pCisCtx->chIdx = pBle->chan.chanIdx = LmgrSelectNextChannel(&pCisCtx->chanParam, pCisCtx->cisEvtCounter, 0, TRUE);
        pCisCtx->nextSubEvtChanIdx = LmgrSelectNextSubEvtChannel(&pCisCtx->chanParam);

        if (pCisCtx->txTestEnabled)
        {
          uint32_t sduInt = pCisCtx->sduIntervalSToM;

          while ((pOp->dueUsec - pCisCtx->testSduTs) >= sduInt)
          {
            lctrCisTxTestPayloadHandler(pCisCtx);
            pCisCtx->testSduTs += sduInt;
          }
        }

        if (pCisCtx->sduSizeSToM &&
            (pCisCtx->dataPathInCtx.id == LL_ISO_DATA_PATH_VS))
        {
          uint8_t *pSduBuf;
          if ((pSduBuf = WsfMsgAlloc(pLctrRtCfg->maxIsoSduLen + HCI_ISO_DL_MAX_LEN)) != NULL)
          {
            WSF_ASSERT(lctrCodecHdlr.inReq);
            lctrCodecHdlr.inReq(pCisCtx->cisHandle, pSduBuf + HCI_ISO_HDR_LEN + HCI_ISO_DL_MAX_LEN, pCisCtx->sduSizeSToM);
          }
          else
          {
            LL_TRACE_WARN1("!!! Out of memory; dropping input SDU, handle=%u", pCisCtx->cisHandle);
          }
        }

        /* Assemble framed data. */
        if ((pCisCtx->framing == LL_ISO_PDU_TYPE_FRAMED) &&
             pCisCtx->isoalTxCtx.pendQueueSize)
        {
          uint8_t *pDataBuf = lctrTxIsoDataPduAlloc();

          if (pDataBuf == NULL)
          {
            /* TODO optimize statement as the call to WsfMsgFree() has no effect and should not continue to following statement. */
            LL_TRACE_WARN0("LctrTxIso, Unable to allocate PDU for transmit");
            WsfMsgFree(pDataBuf);
          }

          lctrIsoHdr_t isoHdr;

          isoHdr.sduLen = lctrAssembleTxFramedPdu(&pCisCtx->isoalTxCtx, pDataBuf, pCisCtx->localDataPdu.maxTxLen);
          isoHdr.tsFlag = 0;

          lctrIsoSduTxDecAvailBuf();
          lctrCisTxDataPduQueue(pCisCtx, &isoHdr, pDataBuf);
        }

        /* TODO: resolve magic number */
        if (pCisCtx->nse < 3)
        {
          if (wwTotalUsec >= ((cigInterUsec >> 1) - WSF_MAX(LL_BLE_TIFS_US, BbGetSchSetupDelayUs())))
          {
            LL_TRACE_WARN2("!!! Terminating CIG due to excessive WW handle=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->cisEvtCounter);
            lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_CLOSED);

            if (pCigCtx->numCisEsted == 1)
            {
              /* If the last CIS to close, schedule no more operation. */
              return;
            }
          }
        }
        else
        {
          if (wwTotalUsec >= pCisCtx->subIntervUsec)
          {
            LL_TRACE_WARN2("!!! Terminating CIG due to excessive WW handle=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->cisEvtCounter);
            lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_CLOSED);

            if (pCigCtx->numCisEsted == 1)
            {
              /* If the last CIS to close, schedule no more operation. */
              return;
            }
          }
        }
      }
      else
      {
        pCisCtx->cisCeRef--;
      }

      pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx);
    }

    /* Advance to next interval. */
    pOp->dueUsec = pCigCtx->roleData.slv.anchorPointUsec + unsyncTimeUsec - wwTotalUsec;

    /* Use small minDurUsec to improve scheduling. */
    pOp->minDurUsec = pCigCtx->list.pHead->pCisCtx->subIntervUsec + wwTotalUsec;

    /* TODO: confirm extra rxTimeout time allocation. */
    pCigCtx->pCisCtx->bleData.op.slvCis.rxSyncDelayUsec = (wwTotalUsec << 1) + 10;

    if (pCigCtx->headCisRmved == TRUE)
    {
      pCigCtx->headCisRmved = FALSE;
      pOp->dueUsec += pCigCtx->offsetUsec;
      pCigCtx->offsetUsec = 0;
    }

    if (SchInsertAtDueTime(pOp, lctrCisResolveConflict))
    {
      break;
    }

    pCisCtx = lctrCisGetHeadCis(&pCigCtx->list);
    LL_TRACE_WARN1("!!! CIG slave schedule conflict eventCounter=%u", pCisCtx->cisEvtCounter);
  }

  if (lmgrCb.sendIsoCmplEvt)
  {
    lctrNotifyHostIsoEventComplete(pCigCtx->cigHandle, (uint32_t) pCisCtx->cisEvtCounter);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Abort a connection operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrSlvCisCigAbortOp(BbOpDesc_t *pOp)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;

  LL_TRACE_WARN1("!!! CIG Slave BOD aborted, cigHandle=%u", pCigCtx->cigHandle);

  if (pCigCtx->numCisEsted > 0)
  {
    lctrCisCtx_t *pCisCtx = lctrCisGetHeadCis(&pCigCtx->list);
    /* It is possible there is no CIS since it is aborting. */

    while (pCisCtx)
    {
      lctrFtParam_t txFtParam, rxFtParam;

      /* Need to setup Tx/Rx flush timeout parameter first since some field will be used in the lctrSlvCisInitIsr. */
      if (pCisCtx->bnMToS)
      {
        lctrCisInitFtParam(&txFtParam, pCisCtx->bnSToM, pCisCtx->ftSToM, pCisCtx->nse);
        (void)lctrCisFtInsertTail(&pCisCtx->txFtParamList, &txFtParam); /* Assume there is memory. */
      }

      if (pCisCtx->bnSToM)
      {
        lctrCisInitFtParam(&rxFtParam, pCisCtx->bnMToS, pCisCtx->ftMToS, pCisCtx->nse);
        (void)lctrCisFtInsertTail(&pCisCtx->rxFtParamList, &rxFtParam); /* Assume there is memory. */
      }

      lctrSlvCisInitIsr(pCisCtx);

      pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx);
    }
  }

  lctrSlvCisCigEndOp(pOp);
}

/*************************************************************************************************/
/*!
 *  \brief      Complete a transmitted data buffer.
 *
 *  \param      pOp     Operation context.
 *  \param      status  Transmit status.
 */
/*************************************************************************************************/
void lctrSlvCisCigTxCompletion(BbOpDesc_t *pOp, uint8_t status)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx = pCigCtx->pCisCtx;

  if (pCisCtx->txFtParamList.pHead != NULL)
  {
    pCisCtx->txFtParamList.pHead->ftParam.subEvtCounter++;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Complete a received data buffer.
 *
 *  \param      pOp     Operation context.
 *  \param      pRxBuf  Next receive buffer or NULL to flow control.
 *  \param      status  Receive status.
 */
/*************************************************************************************************/
void lctrSlvCisCigRxCompletion(BbOpDesc_t *pOp, uint8_t *pRxBuf, uint8_t status)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx = pCigCtx->pCisCtx;
  lctrConnCtx_t * pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);
  BbBleData_t * const pBle = &pCisCtx->bleData;
  BbBleSlvCisEvent_t * const pCis = &pBle->op.slvCis;

  pCisCtx->data.slv.rxStatus = status;

  if (pCisCtx->rxFtParamList.pHead != NULL)
  {
    pCisCtx->rxFtParamList.pHead->ftParam.subEvtCounter++;
    pCisCtx->rxFtParamList.pHead->ftParam.pduRcved = FALSE;
  }

  if (pCisCtx->txFtParamList.pHead != NULL)
  {
    pCisCtx->txFtParamList.pHead->ftParam.pduAcked = FALSE;
  }

  pCisCtx->validRx = FALSE;
  pCisCtx->txPduIsAcked = FALSE;
  pCisCtx->pduFlushed = FALSE;
  pCisCtx->isoalRxCtx.pduFlushed = FALSE;

  /*** Cancelled processing ***/

  if (status == BB_STATUS_CANCELED)
  {
    lctrCisRxPduFree(pRxBuf);
    goto Done;
  }

  /*** CIS event pre-processing ***/

  /* Add RSSI to average if monitoring power. */
  if ((pConnCtx->monitoringState == LCTR_PC_MONITOR_ENABLED) &&
      (pConnCtx->powerMonitorScheme == LCTR_PC_MONITOR_AUTO))
  {
    int8_t rssi = (status == LL_SUCCESS) ? pOp->prot.pBle->op.slvCis.rssi : 0x80;

    /* Calculations handled on timer expiration for tmrPowerCtrl. */
    /* Use a positive value to hold accumulated RSSI, as RSSI will never be positive. */
    pConnCtx->cisAccumulatedRssi += (uint32_t) (-(rssi));
    pConnCtx->cisTotalAccumulatedRssi++;
  }

  /* Save time stamp for the first Rx. */
  if (pCisCtx->data.slv.firstRxFromMaster == TRUE)
  {
    pCisCtx->data.slv.firstRxFromMaster = FALSE;
    pCisCtx->data.slv.firstRxStartTsUsec = pCis->startTsUsec;
  }

  if ((!pCisCtx->data.slv.syncWithMaster) &&
      (status == BB_STATUS_SUCCESS))
  {
    /* Receive successful packet, update anchor point later. */
    pCisCtx->firstFromPeer = TRUE;
    pCisCtx->data.slv.syncWithMaster = TRUE;
  }

  /*** Receive packet pre-processing ***/

  switch (status)
  {
  case BB_STATUS_SUCCESS:
    pCisCtx->data.slv.rxFromMaster = TRUE;
    pCisCtx->data.slv.consCrcFailed = 0;
    break;

  case BB_STATUS_CRC_FAILED:
    LL_TRACE_WARN3("lctrSlvCisCigRxCompletion: BB failed with status=CRC_FAILED, handle=%u, bleChan=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->bleData.chan.chanIdx, pCisCtx->cisEvtCounter);
    pCisCtx->isoLinkQualStats.crcErrPkt++;
    pCisCtx->validRx = FALSE;
    pCisCtx->txPduIsAcked = FALSE;
    goto SetupTx;

  case BB_STATUS_RX_TIMEOUT:
    LL_TRACE_WARN3("lctrSlvCisCigRxCompletion: BB failed with status=RX_TIMEOUT, handle=%u, bleChan=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->bleData.chan.chanIdx, pCisCtx->cisEvtCounter);
    pCisCtx->validRx = FALSE;
    pCisCtx->txPduIsAcked = FALSE;
    lctrCisRxPduFree(pRxBuf);
    goto Done;

  case BB_STATUS_FAILED:
    LL_TRACE_WARN3("lctrSlvCisCigRxCompletion: BB failed with status=FAILED, handle=%u, bleChan=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->bleData.chan.chanIdx, pCisCtx->cisEvtCounter);
    pCisCtx->validRx = FALSE;
    pCisCtx->txPduIsAcked = FALSE;
    lctrCisRxPduFree(pRxBuf);
    goto Done;

  default:
    break;
  }

  lctrCisUnpackDataPduHdr(&pCisCtx->rxHdr, pRxBuf);

  /* Check for MIC failure and if this is not a re-transmission */
  if ((status == BB_STATUS_MIC_FAILED) &&
      ((pCisCtx->rxHdr.sn ^ pCisCtx->txHdr.nesn) & 1) == 0)
  {
    LL_TRACE_WARN3("lctrSlvCisCigRxCompletion: BB failed with MIC_FAILED, handle=%u, rxPacketNum=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->rxPktCounter, pCisCtx->cisEvtCounter);
    lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_TERM_MIC_FAILED);
    lctrCisRxPduFree(pRxBuf);
    BbSetBodTerminateFlag();
    goto Done;
  }

#if (LL_ENABLE_TESTER)
  if ((llTesterCb.cisAckMode != LL_TESTER_ACK_MODE_NORMAL) ||
      (llTesterCb.cisFwdPkt == TRUE))
  {
    LctrCisProcessRxTxAck(pCisCtx, &pCisCtx->validRx, &pCisCtx->txPduIsAcked);

    /* Don't report the empty packet. */
    if (pCisCtx->rxHdr.len == 0)
    {
      pCisCtx->validRx = FALSE;
    }
  }
  else
#endif
  {
    pCisCtx->validRx = lctrCisProcessRxAck(pCisCtx);
    pCisCtx->txPduIsAcked = lctrCisProcessTxAck(pCisCtx);             /* Always Tx after Rx. */
  }

  /*** Setup for transmit ***/

SetupTx:

  lctrCisCheckFtAfterRx(pCisCtx);

  /* Slave always transmits after receiving. */
  lctrCisSetupForTx(pCigCtx, status, TRUE);
  pCisCtx->data.slv.lastTxNull = pCisCtx->txHdr.np;

  /* Tx post-processing. */
  lctrCisProcessTxAckCleanup(pCisCtx);

  /* Rx post-processing. */
  lctrCisRxPostProcessing(pCisCtx, pRxBuf);

  return;

  /*** ISR complete ***/

Done:

  lctrCisCheckFtAfterRx(pCisCtx);

  pCisCtx->data.slv.lastTxNull = pCisCtx->isTxDone;

  lctrCisProcessTxAckCleanup(pCisCtx);
}

/*************************************************************************************************/
/*!
 *  \brief  Begin a connection operation.
 *
 *  \param  pOp     Begin operation.
 *
 *  \note   Scheduler may call this routine multiple times in the following situation:
 *          this BOD is pending and a BOD is inserted before.
 */
/*************************************************************************************************/
void lctrSlvCisCigBeginOp(BbOpDesc_t *pOp)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx;
  pCisCtx = pCigCtx->pCisCtx;
  pCigCtx->isLoopBack = FALSE;

  /* Need to setup Tx/Rx flush timeout parameter first since some field will be used in the lctrSlvCisInitIsr. */
  lctrFtParam_t txFtParam, rxFtParam;

  if (pCisCtx->bnSToM)
  {
    lctrCisInitFtParam(&txFtParam, pCisCtx->bnSToM, pCisCtx->ftSToM, pCisCtx->nse);
    (void)lctrCisFtInsertTail(&pCisCtx->txFtParamList, &txFtParam); /* Assume there is memory. */
  }

  if (pCisCtx->bnMToS)
  {
    lctrCisInitFtParam(&rxFtParam, pCisCtx->bnMToS, pCisCtx->ftMToS, pCisCtx->nse);
    (void)lctrCisFtInsertTail(&pCisCtx->rxFtParamList, &rxFtParam); /* Assume there is memory. */
  }

  lctrSlvCisInitIsr(pCisCtx);

  lctrSlvCisCigExecOp(pCisCtx);
}

/************************************************* ************************************************/
/*!
 *  \brief  Begin a connection operation.
 *
 *  \param  pOp     Begin operation.
 *
 *  \note   Scheduler may call this routine multiple times in the following situation:
 *          this BOD is pending and a BOD is inserted before.
 */
/*************************************************************************************************/
void lctrSlvCisCigContOp(BbOpDesc_t *pOp)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx;
  pCisCtx = pCigCtx->pCisCtx;

  lctrSlvCisCigExecOp(pCisCtx);
}

/*************************************************************************************************/
/*!
 *  \brief  Post subevent callback, setup for the next subevent channel index.
 *
 *  \param  pOp       Current operation.
 *  \param  status    Status.
 */
/*************************************************************************************************/
void lctrSlvCisCigPostSubEvt(BbOpDesc_t *pOp, uint8_t status)
{
  lctrCigCtx_t *pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx;

  WSF_ASSERT(pCigCtx);
  pCisCtx = pCigCtx->pCisCtx;

  WSF_ASSERT(pCisCtx);
  pCisCtx->nextSubEvtChanIdx = LmgrSelectNextSubEvtChannel(&pCisCtx->chanParam);
}
