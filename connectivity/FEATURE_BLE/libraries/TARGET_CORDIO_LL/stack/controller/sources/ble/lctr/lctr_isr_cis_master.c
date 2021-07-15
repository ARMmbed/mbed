/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master connected isochronous stream ISR callbacks.
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
#include "lctr_int_cis_master.h"
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
#include "pal_codec.h"

#if (LL_ENABLE_TESTER)
#include "pal_bb_ble_tester.h"
#endif

#include <string.h>

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
 *  \brief  Initialize master CIS event resources.
 *
 *  \param  pCisCtx   CIS context.
 */
/*************************************************************************************************/
static void lctrMstCisInitIsr(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->subEvtCounter = 0;
  pCisCtx->data.mst.rxFromSlave = FALSE;
  pCisCtx->txDataCounter = 0;
  pCisCtx->rxDataCounter = 0;

  /* List may be empty if BN_S_To_M is 0 */
  if (pCisCtx->rxFtParamList.pHead != NULL)
  {
    pCisCtx->rxFtParamList.pHead->ftParam.subEvtCounter = 0;
  }

  /* List may be empty if BN_M_To_S is 0 */
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
 *  \brief  Setup the resource for the Rx.
 *
 *  \param  pCigCtx   CIG context.
 *  \param  pCisCtx   CIS context.
 */
/*************************************************************************************************/
static void lctrMstCisSetupRx(lctrCigCtx_t *pCigCtx, lctrCisCtx_t *pCisCtx)
{
  uint8_t *pBuf;

  if (!lmgrIsoCb.availRxBuf)
  {
    LL_TRACE_ERR1("!!! lctrMstCisSetupRx, RX buffer not available, cisHandle=%u", pCisCtx->cisHandle);
    return;                    /* flow control Rx */
  }

  /*** Setup for receive ***/

  if ((pBuf = lctrCisRxPduAlloc()) != NULL)
  {
#if (LL_ENABLE_TESTER)
    /* Invalidate access address if trigger is set. */
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
    LL_TRACE_ERR2("!!! OOM while initializing receive buffer at start of CE, cisHandle=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->cisEvtCounter);
    BbSetBodTerminateFlag();
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Setup the resource for the Tx.
 *
 *  \param  pCigCtx   CIG context.
 *  \param  pCisCtx   CIS context.
 */
/*************************************************************************************************/
static void lctrMstCisSetupTx(lctrCigCtx_t *pCigCtx, lctrCisCtx_t *pCisCtx)
{
  (void)lctrCisSetupForTx(pCigCtx, BB_STATUS_SUCCESS /* does not matter */, TRUE);
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

  if (pCisCtx->txHdr.np)
  {
    /* NULL PDU doesn't need to be acked or flushed. */
    return;
  }

  lctrFtParam_t *pFtParam = &pCisCtx->txFtParamList.pHead->ftParam;

  if (pFtParam->pduAcked)
  {
    pFtParam->isPduDone[pFtParam->pduCounter] = TRUE;       /* The PDU is done. */
    pFtParam->pduCounter++;
    lctrCisIncPacketCounterTx(pCisCtx);
  }
  else
  {
    /* Check if this is the last interval before FT. */
    if ((pFtParam->intervalCounter + 1) == pFtParam->intervalTotal)
    {
      if (pFtParam->subEvtCounter == pFtParam->lastSubEvtFt[pFtParam->pduCounter])
      {
        /* PDU needs to be flushed. */
        pCisCtx->dataSn++;
        pFtParam->isPduDone[pFtParam->pduCounter] = TRUE;       /* The PDU is done. */

        if (pFtParam->pduType[pFtParam->pduCounter] == LCTR_CIS_PDU_NON_EMPTY)
        {
          /* Need to remove from ack queue if non-empty PDU. */
          lctrCisTxPduAck(pCisCtx);
          pCisCtx->pduFlushed = TRUE;   /* Set the flag, lctrCisProcessTxAckCleanup will be called to do the cleanup later in the lctrMstCisCigPostSubEvt. */
        }

        lctrCisIncPacketCounterTx(pCisCtx);
        pCisCtx->isoLinkQualStats.txUnAckPkt++;
        pFtParam->pduCounter++;
      }
      else /* Implies that a retransmit will happen. */
      {
        pCisCtx->isoLinkQualStats.retransmitPkt++;
      }
    }
    else
    {
      pCisCtx->isoLinkQualStats.retransmitPkt++;
    }
  }

  /* Set the flag to send NULL packet. */
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
 *  \param  pCisCtx   CIS Context.
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

  /* Only check the head node. */
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
 *  \param  pCisCtx     CIS context.
 *
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
 *  \param  pCisCtx    CIS Context.
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
 *  \brief  Check whether continue the current operation.
 *
 *  \param  pOp          Operation.
 *  \param  pNewCisCtx   A new CIS operation.
 *
 *  \return zero
 */
/*************************************************************************************************/
uint32_t lctrMstCisCheckContOpPostCback(BbOpDesc_t *pOp, bool_t *pNewCisCtx)
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
        lctrMstCisCigPostSubEvt(pOp, BB_STATUS_SUCCESS);
      }
    }
    else
    {
      /* TODO need to check whether this pCisCtx->rxHdr.cie || pCisCtx->txHdr.cie and no sending more PDU for this CIS. */

      /* Get the next CIS in the list. */
      if (pCigCtx->isLoopBack == FALSE)
      {
        lctrFtParam_t txFtParam, rxFtParam;

        if (pCisCtx->bnMToS)
        {
          lctrCisInitFtParam(&txFtParam, pCisCtx->bnMToS, pCisCtx->ftMToS, pCisCtx->nse);
          (void)lctrCisFtInsertTail(&pCisCtx->txFtParamList, &txFtParam); /* Assume there is memory. */
        }

        if (pCisCtx->bnSToM)
        {
          lctrCisInitFtParam(&rxFtParam, pCisCtx->bnSToM, pCisCtx->ftSToM, pCisCtx->nse);
          (void)lctrCisFtInsertTail(&pCisCtx->rxFtParamList, &rxFtParam); /* Assume there is memory. */
        }

        lctrMstCisInitIsr(pCisCtx);
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
          lctrMstCisCigPostSubEvt(pOp, BB_STATUS_SUCCESS);
        }
      }
    }
  }
  return 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Check whether continue the current operation.
 *
 *  \param  pOp          Operation.
 *  \param  pNewCisCtx   A new CIS operation.
 *
 *  \return Offset of the next operation, zero discontinue next operation, otherwise continue.
 */
/*************************************************************************************************/
uint32_t lctrMstCisCheckContOp(BbOpDesc_t *pOp, bool_t *pNewCisCtx)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx = pCigCtx->pCisCtx;
  uint32_t  delayUsec = pCisCtx->delayUsec;
  uint32_t subIntervUsec = pCisCtx->subIntervUsec;

  WSF_ASSERT(pCisCtx);

  BbBleData_t *pBle = &pCisCtx->bleData;

  if (pCigCtx->packing == LL_PACKING_SEQUENTIAL)
  {
    if (pCisCtx->rxHdr.cie || pCisCtx->txHdr.cie)
    {
      /* Skip additional sub-events if receive CIE before reaching the NSE */
      if ((pCisCtx->subEvtCounter + 1) < pCisCtx->nse)        /* Plus one since subEvtCounter has not been updated yet. */
      {
        delayUsec += delayUsec * (pCisCtx->nse - (pCisCtx->subEvtCounter + 1));
      }

      goto SwitchSeq;
    }

    /* Check whether the CIS shall start or not. */
    if (pCisCtx->cisCeRef != 0)
    {
      goto SwitchSeq;
    }

    if (pCisCtx->txDataCounter >= pCisCtx->bnMToS && pCisCtx->rxDataCounter >= pCisCtx->bnSToM && pCisCtx->isTxDone == TRUE)
    {
      /* There is no more data to send. */
      pCisCtx->txHdr.cie = 1;

      if (lmgrGetOpFlag(LL_OP_MODE_FLAG_ENA_MST_CIS_NULL_PDU) == FALSE)
      {
        goto SwitchSeq;
      }
    }

    pCisCtx->subEvtCounter++;

    if (pCisCtx->subEvtCounter == pCisCtx->nse)
    {
      /* There is no more subevent. */
      pCisCtx->txHdr.cie = 1;
      pCisCtx->cisDone = TRUE;
      goto SwitchSeq;
    }

    /*** Continue current CIS ***/

    /* Set next channel index. */
    pBle->chan.chanIdx = pCisCtx->nextSubEvtChanIdx;      /* Next subevent channel index is pre-calculated. */
    return delayUsec;

SwitchSeq:
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

      if (pCisCtx->bnMToS)
      {
        lctrCisInitFtParam(&txFtParam, pCisCtx->bnMToS, pCisCtx->ftMToS, pCisCtx->nse);
        (void)lctrCisFtInsertTail(&pCisCtx->txFtParamList, &txFtParam); /* Assume there is memory. */
      }

      if (pCisCtx->bnSToM)
      {
        lctrCisInitFtParam(&rxFtParam, pCisCtx->bnSToM, pCisCtx->ftSToM, pCisCtx->nse);
        (void)lctrCisFtInsertTail(&pCisCtx->rxFtParamList, &rxFtParam); /* Assume there is memory. */
      }

      lctrMstCisInitIsr(pCisCtx);

      pBle = &pCisCtx->bleData;
      *pNewCisCtx = TRUE;

      /* Move the current CIS to the next CIS. */
      pOp->prot.pBle = pBle;
      pCigCtx->pCisCtx = pCisCtx;
      memcpy(&pBle->chan, &pCigCtx->pCisCtx->bleData.chan, sizeof(pBle->chan));
      pBle->chan.chanIdx = pCigCtx->pCisCtx->chIdx;   /* Set the next channel to the first channel in the next CIS. */

      return delayUsec; /* Return the offset from the old CIS */
    }
  }
  else /* LL_PACKING_INTERLEAVED */
  {
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

    if (pCisCtx->txDataCounter >= pCisCtx->bnMToS && pCisCtx->rxDataCounter >= pCisCtx->bnSToM && pCisCtx->isTxDone == TRUE)
    {
      /* There is no more data to send. */
      pCisCtx->txHdr.cie = 1;

      if (lmgrGetOpFlag(LL_OP_MODE_FLAG_ENA_MST_CIS_NULL_PDU) == FALSE)
      {
        pCisCtx->cisDone = TRUE;
        goto SwitchInt;
      }
    }

    pCisCtx->subEvtCounter++;

    if (pCisCtx->subEvtCounter == pCisCtx->nse)
    {
      /* There is no more subevent. */
      pCisCtx->txHdr.cie = 1;
      pCisCtx->cisDone = TRUE;
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
      pBle->chan.chanIdx = oriNextSubEvtChanIdx;      /* Next subevent channel index is pre-calculated. */
      return subIntervUsec;
    }

    if ((pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx)) == NULL)
    {
      if (lctrCisAreCisCtxDone(&pCigCtx->list))
      {
        goto Done;
      }

      *pNewCisCtx = TRUE;
      /* Processing will continue in the lctrMstCisCheckContOpPostCback() */

      return delayUsec; /* Return the offset from the old CIS */
    }
    else
    {
      if (lctrCisAreCisCtxDone(&pCigCtx->list))
      {
        goto Done;
      }

      /* Check whether the CIS shall start or not. */
      if (pCisCtx->cisCeRef != 0)
      {
        /*** Continue head CIS ***/

        /* Set next channel index. */
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

        return (subIntervUsec - addDelayUsec);
      }

      *pNewCisCtx = TRUE;
      /* Processing will continue in the lctrMstCisCheckContOpPostCback() */

      return delayUsec; /* Return the offset from the old CIS */
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
 *  \brief  Start the CIG operation.
 *
 *  \param  pOp     Begin operation.
 */
/*************************************************************************************************/
void lctrMstCisCigBeginOp(BbOpDesc_t *pOp)
{
  lctrCigCtx_t *pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx;

  pCisCtx = pCigCtx->pCisCtx;
  pCigCtx->isLoopBack = FALSE;

  /* Need to setup Tx/Rx flush timeout parameter first since some field will be used in the lctrSlvCisInitIsr. */
  lctrFtParam_t txFtParam, rxFtParam;

  if (pCisCtx->bnMToS)
  {
    lctrCisInitFtParam(&txFtParam, pCisCtx->bnMToS, pCisCtx->ftMToS, pCisCtx->nse);
    (void)lctrCisFtInsertTail(&pCisCtx->txFtParamList, &txFtParam); /* Assume there is memory. */
  }

  if (pCisCtx->bnSToM)
  {
    lctrCisInitFtParam(&rxFtParam, pCisCtx->bnSToM, pCisCtx->ftSToM, pCisCtx->nse);
    (void)lctrCisFtInsertTail(&pCisCtx->rxFtParamList, &rxFtParam); /* Assume there is memory. */
  }

  lctrMstCisInitIsr(pCisCtx);

  lctrMstCisSetupTx(pCigCtx, pCisCtx);

  /* Setup Tx could fail which leads to terminate BOD. */
  if (!BbGetBodTerminateFlag())
  {
    lctrMstCisSetupRx(pCigCtx, pCisCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Continue the CIG operation.
 *
 *  \param  pOp     Begin operation.
 */
/*************************************************************************************************/
void lctrMstCisCigContOp(BbOpDesc_t *pOp)
{
  lctrCigCtx_t *pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx;

  pCisCtx = pCigCtx->pCisCtx;

  lctrMstCisSetupTx(pCigCtx, pCisCtx);

  /* Setup Tx could fail which leads to terminate BOD. */
  if (!BbGetBodTerminateFlag())
  {
    lctrMstCisSetupRx(pCigCtx, pCisCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Post subevent callback, setup for the next subevent channel index.
 *
 *  \param  pOp       Current operation.
 *  \param  status    Status.
 */
/*************************************************************************************************/
void lctrMstCisCigPostSubEvt(BbOpDesc_t *pOp, uint8_t status)
{
  lctrCigCtx_t *pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx;

  WSF_ASSERT(pCigCtx);
  pCisCtx = pCigCtx->pCisCtx;

  WSF_ASSERT(pCisCtx);

  /* Pre-calculate the next subevent channel. */
  pCisCtx->nextSubEvtChanIdx = LmgrSelectNextSubEvtChannel(&pCisCtx->chanParam);
}

/*************************************************************************************************/
/*!
 *  \brief  Cleanup a connection operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrMstCisCigCleanupOp(BbOpDesc_t *pOp)
{

}

/*************************************************************************************************/
/*!
 *  \brief  End a CIG operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrMstCisCigEndOp(BbOpDesc_t *pOp)
{
  /* Pre-resolve common structures for efficient access. */
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx  = lctrCisGetHeadCis(&pCigCtx->list);
  lctrConnCtx_t * pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);
  WSF_ASSERT(pCisCtx);  /* At least one CIS in the CIG. */

  while (pCisCtx)
  {
    /* Check for unframed flushed PDUs. */
    if ((pCisCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED) &&
        (pCisCtx->isoalRxCtx.pduFlushed == TRUE))
    {
      pCisCtx->isoalRxCtx.pduFlushed = FALSE;
      lctrCisCheckUnframedFlush(pCisCtx);
    }

    pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

    /* LL_CIS_TERMINATION case */
    if ((pCisCtx->isClosing == TRUE) ||
        (pConnCtx->enabled == FALSE) ||
        (lctrResetEnabled == TRUE))
    {
      /* Notify host when terminate happens unless reseted. */
      if (lctrResetEnabled == FALSE)
      {
        if (pConnCtx->enabled == FALSE)
        {
          pCisCtx->reason = HCI_ERR_REMOTE_TERMINATED;
        }
        /* If host initiates the termination, the reason code shall be HCI_ERR_LOCAL_TERMINATED in the disconnect complete event. */
        else if (pCisCtx->hostInitTerm)
        {
          pCisCtx->reason = HCI_ERR_LOCAL_TERMINATED;
          pCisCtx->hostInitTerm = FALSE;
        }

        lctrNotifyHostCisTerm(pCisCtx);
      }

      pCisCtx->isClosing = FALSE;
      pCigCtx->numCisEsted--;
      lctrCleanupCtx(pCisCtx);
    }

    pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx);
  }

  if (pCigCtx->numCisEsted == 0)
  {
    return;
  }

  pCisCtx = lctrCisGetHeadCis(&pCigCtx->list);

  while (pCisCtx)
  {
    pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

    lctrCisCheckFtAfterBod(pCisCtx);

    /* Process event completion */

    if (!pCisCtx->connEst && pCisCtx->data.mst.rxFromSlave)
    {
      WsfTimerStartMs(&pCisCtx->tmrSupTimeout, pCisCtx->supTimeoutMs);

      pCisCtx->connEst = TRUE;

      /* Now that the first CIS has been established,
       * notify host of connection establish and
       * start the next CIS creation, if needed.
       */
      if (lmgrCisMstCb.createCisPend)
      {
        lctrMstCreateCisDone(pCisCtx);
        lctrNotifyHostCisEst(pCisCtx, LL_SUCCESS, pCisCtx->cigSyncDelayUsec);
      }

      if (pCisCtx->powerIndReq && lctrSendPowerChangeIndCback)
      {
        uint8_t txPhy = (pCisCtx->role == LL_ROLE_MASTER) ? pCisCtx->phyMToS : pCisCtx->phySToM;
        lctrSendPowerChangeIndCback(pConnCtx, txPhy, 0, pLctrRtCfg->defTxPwrLvl, TRUE);
      }
    }
    else if (pCisCtx->data.mst.rxFromSlave)
    {
      /* Reset supervision timer. */
      WsfTimerStartMs(&pCisCtx->tmrSupTimeout, pCisCtx->supTimeoutMs);
    }

    /* Failed to receive packet within the first 6 intervals. */
    if ((pCisCtx->firstFromPeer == FALSE) && (pCisCtx->cisEvtCounter == LCTR_FAST_TERM_CNT))
    {
      LL_TRACE_WARN1("CIS terminated due to fast termination timeout, handle=%d", pCisCtx->cisHandle);
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

  /*** Adjust the anchor point if the head CIS is removed ***/

  if (pCigCtx->headCisRmved == TRUE)
  {
    pCigCtx->headCisRmved = FALSE;

    pOp->dueUsec += pCigCtx->offsetUsec;
    pCigCtx->offsetUsec = 0;

    BbBleData_t *pBle = &pCisCtx->bleData;
    /* Move the current CIS to the head CIS. */
    pOp->prot.pBle = pBle;
    pCigCtx->pCisCtx = pCisCtx;
    memcpy(&pBle->chan, &pCigCtx->pCisCtx->bleData.chan, sizeof(pBle->chan));
    pBle->chan.chanIdx = pCigCtx->pCisCtx->chIdx;   /* Set the next channel to the first channel in the next CIS. */
  }

  /*** Update txPower ***/

  pCisCtx->bleData.chan.txPower = LCTR_GET_TXPOWER(pConnCtx, pCisCtx->phyMToS, pCisCtx->bleData.chan.initTxPhyOptions);
#if(LL_ENABLE_TESTER)
  pCisCtx->bleData.chan.txPower += pConnCtx->bleData.chan.txPwrOffset;
#endif

  /*** Reschedule operation ***/

  while (TRUE)
  {
    /* Advance to next interval. */
    pOp->dueUsec += LCTR_ISO_INT_TO_US(pCigCtx->isoInterval);

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
          uint32_t sduInt = pCisCtx->sduIntervalMToS;

          while ((pOp->dueUsec - pCisCtx->testSduTs) >= sduInt)
          {
            lctrCisTxTestPayloadHandler(pCisCtx);
            pCisCtx->testSduTs += sduInt;
          }
        }

        if (pCisCtx->sduSizeMToS &&
            (pCisCtx->dataPathInCtx.id == LL_ISO_DATA_PATH_VS))
        {
          uint8_t *pSduBuf;
          if ((pSduBuf = lctrTxIsoDataPduAlloc()) != NULL)
          {
            WSF_ASSERT(lctrCodecHdlr.inReq);
            lctrCodecHdlr.inReq(pCisCtx->cisHandle, pSduBuf + HCI_ISO_HDR_LEN + HCI_ISO_DL_MAX_LEN, pCisCtx->sduSizeMToS);
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
      }
      else
      {
        pCisCtx->cisCeRef--;
      }
      pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx);
    }

    if (SchInsertAtDueTime(pOp, NULL))
    {
      break;
    }

    pCisCtx = lctrCisGetHeadCis(&pCigCtx->list);
    LL_TRACE_WARN2("!!! CIS master schedule conflict handle=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->cisEvtCounter);
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
void lctrMstCisCigAbortOp(BbOpDesc_t *pOp)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;

  LL_TRACE_WARN1("!!! CIG Master BOD aborted, cigHandle=%u", pCigCtx->cigHandle);

  /* Need to setup Tx/Rx flush timeout parameter first since some field will be used in the lctrSlvCisInitIsr. */

  if (pCigCtx->numCisEsted > 0)
  {
    lctrCisCtx_t *pCisCtx  = lctrCisGetHeadCis(&pCigCtx->list);
    /* It is possible there is no CIS since it is aborting. */

    while (pCisCtx)
    {
      lctrFtParam_t txFtParam, rxFtParam;

      if (pCisCtx->bnMToS)
      {
        lctrCisInitFtParam(&txFtParam, pCisCtx->bnMToS, pCisCtx->ftMToS, pCisCtx->nse);
        (void)lctrCisFtInsertTail(&pCisCtx->txFtParamList, &txFtParam); /* Assume there is memory. */
      }

      if (pCisCtx->bnSToM)
      {
        lctrCisInitFtParam(&rxFtParam, pCisCtx->bnSToM, pCisCtx->ftSToM, pCisCtx->nse);
        (void)lctrCisFtInsertTail(&pCisCtx->rxFtParamList, &rxFtParam); /* Assume there is memory. */
      }

      lctrMstCisInitIsr(pCisCtx);

      pCisCtx = lctrCisGetNextCis(&pCigCtx->list, pCisCtx);
    }
  }

  lctrMstCisCigEndOp(pOp);
}

/*************************************************************************************************/
/*!
 *  \brief      Complete a transmitted data buffer.
 *
 *  \param      pOp     Operation context.
 *  \param      status  Transmit status.
 */
/*************************************************************************************************/
void lctrMstCisCigTxCompletion(BbOpDesc_t *pOp, uint8_t status)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t      *pCisCtx = pCigCtx->pCisCtx;

  if (pCisCtx->txFtParamList.pHead != NULL)
  {
    pCisCtx->txFtParamList.pHead->ftParam.subEvtCounter++;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Complete a received data buffer.
 *
 *  \param      pOp         Operation context.
 *  \param      pRxBuf      Next receive buffer or NULL to flow control.
 *  \param      status      Receive status.
 */
/*************************************************************************************************/
void lctrMstCisCigRxCompletion(BbOpDesc_t *pOp, uint8_t *pRxBuf, uint8_t status)
{
  lctrCigCtx_t * const pCigCtx = pOp->pCtx;
  lctrCisCtx_t *pCisCtx = pCigCtx->pCisCtx;
  lctrConnCtx_t *pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

  if (pCisCtx->txFtParamList.pHead != NULL)
  {
    pCisCtx->txFtParamList.pHead->ftParam.pduAcked = FALSE;
  }

  if (pCisCtx->rxFtParamList.pHead != NULL)
  {
    pCisCtx->rxFtParamList.pHead->ftParam.subEvtCounter++;
    pCisCtx->rxFtParamList.pHead->ftParam.pduRcved = FALSE;
  }

  pCisCtx->validRx = FALSE;
  pCisCtx->txPduIsAcked = FALSE;
  pCisCtx->pduFlushed = FALSE;
  pCisCtx->isoalRxCtx.pduFlushed = FALSE;

  if (status == BB_STATUS_CRC_FAILED)
  {
    pCisCtx->isoLinkQualStats.crcErrPkt++;
  }

  /*** Cancelled processing ***/

  if (status == BB_STATUS_CANCELED)
  {
    lctrCisRxPduFree(pRxBuf);
    goto Done;
  }

  /*** Receive packet pre-processing ***/

  /* Add RSSI to average if monitoring power. */
  if ((pConnCtx->monitoringState == LCTR_PC_MONITOR_ENABLED) &&
      (pConnCtx->powerMonitorScheme == LCTR_PC_MONITOR_AUTO))
  {
    int8_t rssi = (status == LL_SUCCESS) ? pOp->prot.pBle->op.mstCis.rssi : 0x80;

    /* Calculations handled on timer expiration for tmrPowerCtrl. */
    /* Use a positive value to hold accumulated RSSI, as RSSI will never be positive. */
    pConnCtx->cisAccumulatedRssi += (uint32_t) -rssi;
    pConnCtx->cisTotalAccumulatedRssi++;
  }

  switch (status)
  {
  case BB_STATUS_SUCCESS:
    pCisCtx->firstFromPeer = TRUE;
    pCisCtx->data.mst.rxFromSlave = TRUE;
    break;

  case BB_STATUS_CRC_FAILED:
    LL_TRACE_WARN3("lctrMstCisCigRxCompletion: BB failed with status=CRC_FAILED, handle=%u, bleChan=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->bleData.chan.chanIdx, pCisCtx->cisEvtCounter);
    goto PostProcessing;

  case BB_STATUS_RX_TIMEOUT:
    LL_TRACE_WARN3("lctrMstCisCigRxCompletion: BB failed with status=RX_TIMEOUT, handle=%u, bleChan=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->bleData.chan.chanIdx, pCisCtx->cisEvtCounter);
    pCisCtx->validRx = FALSE;
    pCisCtx->txPduIsAcked = FALSE;
    goto PostProcessing;

  case BB_STATUS_FAILED:
    LL_TRACE_WARN3("lctrMstCisCigRxCompletion: BB failed with status=FAILED, handle=%u, bleChan=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->bleData.chan.chanIdx, pCisCtx->cisEvtCounter);
    pCisCtx->validRx = FALSE;
    pCisCtx->txPduIsAcked = FALSE;
    goto PostProcessing;

  default:
    break;
  }

  lctrCisUnpackDataPduHdr(&pCisCtx->rxHdr, pRxBuf);

  /* Check for MIC failure and if this is not a re-transmission */
  if ((status == BB_STATUS_MIC_FAILED) &&
      ((pCisCtx->rxHdr.sn ^ pCisCtx->txHdr.nesn) & 1) == 0)
  {
    LL_TRACE_WARN3("lctrMstCisCigRxCompletion: BB failed with MIC_FAILED, handle=%u, rxPacketNum=%u, eventCounter=%u", pCisCtx->cisHandle, pCisCtx->rxPktCounter, pCisCtx->cisEvtCounter);
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
    pCisCtx->txPduIsAcked = lctrCisProcessTxAck(pCisCtx);
  }

  /*** Packet post-processing ***/

PostProcessing:

  lctrCisCheckFtAfterRx(pCisCtx);

  /* Tx post-processing. */
  lctrCisProcessTxAckCleanup(pCisCtx);

  /* Rx post-processing. */
  lctrCisRxPostProcessing(pCisCtx, pRxBuf);

  return;

  /*** ISR complete ***/

Done:

  lctrCisCheckFtAfterRx(pCisCtx);

  lctrCisProcessTxAckCleanup(pCisCtx);
}
