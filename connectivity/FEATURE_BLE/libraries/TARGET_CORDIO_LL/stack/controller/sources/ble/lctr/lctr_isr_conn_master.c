/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master connection ISR callbacks.
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
#include "lctr_int_conn.h"
#include "lctr_int_conn_master.h"
#include "lctr_int_adv_master.h"
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

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Master connection ISR control block. */
static struct
{
  uint8_t consCrcFailed;        /*!< Number of consecutive CRC failures. Used only by active operation. */
  bool_t rxFromSlave;           /*!< At least one packet received from slave. */
  uint16_t numTxData;           /*!< Number of Tx data PDU in a single CE. */
  uint16_t numRxData;           /*!< Number of Rx data PDU in a single CE. */
} lctrMstConnIsr;

/*! \brief  Check BB meets data PDU requirements. */
WSF_CT_ASSERT((BB_FIXED_DATA_PKT_LEN == 0) ||
              (BB_FIXED_DATA_PKT_LEN >= LCTR_DATA_PDU_MAX_LEN));

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

#if (LL_ENABLE_TESTER)
extern void LctrProcessRxTxAck(lctrConnCtx_t *pCtx, uint8_t *pRxBuf, uint8_t **pNextRxBuf, bool_t *pTxPduIsAcked);
#endif

/*************************************************************************************************/
/*!
 *  \brief  Update a connection operation.
 *
 *  \param  pCtx        Connection context.
 */
/*************************************************************************************************/
static void lctrMstConnUpdateOp(lctrConnCtx_t *pCtx)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &pCtx->connBod;
  lctrConnUpdInd_t * const pConnUpdInd = &pCtx->connUpd;

  /*** Connection context setup ***/

  const uint16_t connIntervalOld = pCtx->connInterval;
  const uint16_t latencyOld      = pCtx->maxLatency;
  const uint16_t supTimeoutMsOld = pCtx->supTimeoutMs;

  pCtx->connInterval = pConnUpdInd->interval;
  pCtx->maxLatency   = pConnUpdInd->latency;
  pCtx->supTimeoutMs = LCTR_CONN_IND_TO_MS(pConnUpdInd->timeout);

  SchRmCommitUpdate(LCTR_GET_CONN_HANDLE(pCtx));

  /*** General setup ***/

  pOp->dueUsec += BB_BLE_TO_US(LCTR_CONN_IND_TICKS(pConnUpdInd->txWinOffset));
  pOp->maxDurUsec = LCTR_CONN_IND_US(pCtx->connInterval);

  /* Unconditionally reset supervision timer with transitional value.
   *     connIntervalOld + supervisionTimeoutNew */
  WsfTimerStartMs(&pCtx->tmrSupTimeout, LCTR_CONN_IND_MS(connIntervalOld) + pCtx->supTimeoutMs);

  /*** Notifications ***/

  /* Notify host only if connection parameters changed. */
  if ((connIntervalOld != pCtx->connInterval) ||
      (latencyOld      != pCtx->maxLatency) ||
      (supTimeoutMsOld != pCtx->supTimeoutMs))
  {
    pCtx->llcpNotifyMask |= 1 << LCTR_PROC_CONN_UPD;
  }

  /* Delay notification until CE starts. */
  pCtx->llcpInstantComp = TRUE;

  LL_TRACE_INFO2("    >>> Connection updated, handle=%u, eventCounter=%u <<<", LCTR_GET_CONN_HANDLE(pCtx), pCtx->eventCounter);
  LL_TRACE_INFO1("                            connIntervalUsec=%u", LCTR_CONN_IND_US(pCtx->connInterval));
}

/*************************************************************************************************/
/*!
 *  \brief  Update the channel map.
 *
 *  \param  pCtx        Connection context.
 */
/*************************************************************************************************/
static void lctrMstChanMapUpdateOp(lctrConnCtx_t *pCtx)
{
  pCtx->chanMask = pCtx->chanMapUpd.chanMask;
  lctrBuildRemapTable(pCtx);

  /* Delay notification until CE starts. */
  pCtx->llcpInstantComp = TRUE;

  /* Update the channel map for CIS master as well. */
  if (LctrUpdateCisChanMapFn)
  {
    LctrUpdateCisChanMapFn(LCTR_GET_CONN_HANDLE(pCtx));
  }

  LL_TRACE_INFO2("    >>> Channel map updated, handle=%u, eventCounter=%u <<<", LCTR_GET_CONN_HANDLE(pCtx), pCtx->eventCounter);
}

/*************************************************************************************************/
/*!
 *  \brief  Update the selected PHY.
 *
 *  \param  pCtx        Connection context.
 */
/*************************************************************************************************/
static void lctrMstPhyUpdateOp(lctrConnCtx_t *pCtx)
{
  BbBleData_t * pBle = &pCtx->bleData;
  uint8_t txPhyOld = lctrPhyToPhysBit(pBle->chan.txPhy);
  uint8_t rxPhyOld = lctrPhyToPhysBit(pBle->chan.rxPhy);

  /* Notify host only if PHY changed. */
  if ((pCtx->phyUpd.masterToSlavePhy != LL_PHYS_NONE) &&
      (pCtx->phyUpd.masterToSlavePhy != txPhyOld))
  {
    pBle->chan.txPhy = lctrPhysBitToPhy(pCtx->phyUpd.masterToSlavePhy);
    pBle->chan.txPower = LCTR_GET_TXPOWER(pCtx, pCtx->bleData.chan.txPhy, pBle->chan.initTxPhyOptions);
    pCtx->llcpNotifyMask    |= 1 << LCTR_PROC_PHY_UPD;

    if (pBle->chan.txPower == LL_PWR_CTRL_TXPOWER_UNMANAGED)
    {
      pBle->chan.txPower = pLctrRtCfg->defTxPwrLvl;
      LCTR_SET_TXPOWER(pCtx, pBle->chan.txPhy, pLctrRtCfg->defTxPwrLvl);
      if (pBle->chan.txPhy == BB_PHY_BLE_CODED)
      {
        LCTR_SET_TXPOWER(pCtx, LL_PC_PHY_CODED_S2, pLctrRtCfg->defTxPwrLvl);
      }

      if ((pCtx->usedFeatSet & LL_FEAT_POWER_CHANGE_IND) && lctrSendPowerChangeIndCback)
      {
        lctrSendPowerChangeIndCback(pCtx, pBle->chan.txPhy, 0, pLctrRtCfg->defTxPwrLvl, TRUE);
      }
    }
  }
  if ((pCtx->phyUpd.slaveToMasterPhy != LL_PHYS_NONE) &&
      (pCtx->phyUpd.slaveToMasterPhy != rxPhyOld))
  {
    pBle->chan.rxPhy = lctrPhysBitToPhy(pCtx->phyUpd.slaveToMasterPhy);
    pCtx->llcpNotifyMask    |= 1 << LCTR_PROC_PHY_UPD;
  }

  /* Delay notification until CE starts. */
  pCtx->llcpInstantComp = TRUE;

  /* Update connection event durations. */
  pCtx->effConnDurUsec   = lctrCalcConnDurationUsec(pCtx, &pCtx->effDataPdu);
  pCtx->localConnDurUsec = lctrCalcConnDurationUsec(pCtx, &pCtx->localDataPdu);

  LL_TRACE_INFO2("    >>> PHY updated, handle=%u, eventCounter=%u <<<", LCTR_GET_CONN_HANDLE(pCtx), pCtx->eventCounter);
}

/*************************************************************************************************/
/*!
 *  \brief  Begin a connection operation.
 *
 *  \param  pOp     Begin operation.
 *
 *  \note   Scheduler may call this routine multiple times in the following situations:
 *          (1) this BOD is pending and a BOD is inserted before, or (2) a Data PDU is queued
 *          into an empty list.
 */
/*************************************************************************************************/
void lctrMstConnBeginOp(BbOpDesc_t *pOp)
{
  lctrConnCtx_t * const pCtx = pOp->pCtx;
  uint8_t *pBuf;

  if (lctrCheckForLinkTerm(pCtx))
  {
    BbSetBodTerminateFlag();
    return;
  }

  if (pLctrVsHdlrs && pLctrVsHdlrs->ceSetup)
  {
    pLctrVsHdlrs->ceSetup(LCTR_GET_CONN_HANDLE(pCtx));
  }

  /*** Initialize connection event resources ***/

  lctrMstConnIsr.consCrcFailed = 0;
  lctrMstConnIsr.rxFromSlave = FALSE;
  lctrMstConnIsr.numTxData = 0;
  lctrMstConnIsr.numRxData = 0;

  /*** Setup for transmit ***/

  pCtx->rxHdr.md   = 0;                                       /* clear Rx header */
  pCtx->rxHdr.len  = 0;
  pCtx->rxHdr.llid = LL_LLID_EMPTY_PDU;

  /* Guarantee Tx at least 1 packet per CE. */
  (void)lctrSetupForTx(pCtx, BB_STATUS_SUCCESS /* does not matter */, TRUE);    /* always returns TRUE */

  /* Check for Tx failure. */
  if (BbGetBodTerminateFlag())
  {
    return;
  }

  /*** Setup for receive ***/

  if ((pBuf = lctrRxPduAlloc(pCtx->localDataPdu.maxRxLen)) != NULL)
  {
    lctrSetBbPacketCounterRx(pCtx);
    BbBleRxData(pBuf, LCTR_DATA_PDU_LEN(pCtx->localDataPdu.maxRxLen));

    /* Rx may fail; no more important statements in this code path */
  }
  else
  {
    LL_TRACE_ERR1("!!! OOM while initializing receive buffer at start of CE, handle=%u", LCTR_GET_CONN_HANDLE(pCtx));
    BbSetBodTerminateFlag();
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Cleanup a connection operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrMstConnCleanupOp(BbOpDesc_t *pOp)
{
  lctrConnCtx_t * const pCtx = pOp->pCtx;

  /* Reset operation to state before BOD began */
  if (pCtx->emptyPduPend &&
      pCtx->emptyPduFirstAtt)
  {
    /* A Tx was setup for a first empty PDU, but that empty PDU was never transmitted. */
    pCtx->emptyPduPend = FALSE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  End a connection operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrMstConnEndOp(BbOpDesc_t *pOp)
{
  /* Pre-resolve common structures for efficient access. */
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleMstConnEvent_t * const pConn = &pBle->op.mstConn;
  lctrConnCtx_t * const pCtx = pOp->pCtx;
  /* Process event completion */

#if (LL_ENABLE_TESTER == TRUE)
  pBle->chan.txPower = LCTR_GET_TXPOWER(pCtx, pCtx->bleData.chan.txPhy, pCtx->bleData.chan.initTxPhyOptions);
#endif
  if (!pCtx->connEst && (lctrMstConnIsr.rxFromSlave || (lctrMstConnIsr.consCrcFailed > 0)))
  {
    lctrStoreConnTimeoutTerminateReason(pCtx);
    WsfTimerStartMs(&pCtx->tmrSupTimeout, pCtx->supTimeoutMs);

    if (!lmgrGetOpFlag(LL_OP_MODE_FLAG_ENA_FEAT_LLCP_STARTUP) &&
        (pCtx->monitoringState == LCTR_PC_MONITOR_ENABLED) &&
        (pCtx->powerMonitorScheme == LCTR_PC_MONITOR_AUTO) &&
        !lmgrGetOpFlag(LL_OP_MODE_FLAG_DIS_POWER_MONITOR))
    {
      WsfTimerStartMs(&pCtx->tmrPowerCtrl, LL_PC_SERVICE_MS);
    }

    pCtx->connEst = TRUE;
  }
  else if (lctrMstConnIsr.rxFromSlave)
  {
    /* Reset supervision timer. */
    WsfTimerStartMs(&pCtx->tmrSupTimeout, pCtx->supTimeoutMs);
  }

  pCtx->rssi = pConn->rssi;
  if ((pCtx->monitoringState == LCTR_PC_MONITOR_ENABLED))
  {
    /* If a packet did not receive for whatever reason, add a datapoint with the lowest RSSI. */
    if (pCtx->lastRxStatus != BB_STATUS_SUCCESS)
    {
      pCtx->rssi = 0x80; /* Most negative 8 bit number. */
    }

    switch (pCtx->powerMonitorScheme)
    {
      case LCTR_PC_MONITOR_AUTO:
      {
        /* Calculations handled on timer expiration for tmrPowerCtrl. */
        /* Use a positive value to hold accumulated RSSI, as RSSI will never be positive. */
        pCtx->pclMonitorParam.autoMonitor.accumulatedRssi += (uint32_t) (-(pCtx->rssi));
        pCtx->pclMonitorParam.autoMonitor.totalAccumulatedRssi++;
        break;
      }
      case LCTR_PC_MONITOR_PATH_LOSS:
      {
        /* Power control monitoring being enabled implies that this function exists. */
        lctrPathLossMonitorActFn(pCtx);
        break;
      }
      default:
        /* Should not happen. */
        WSF_ASSERT(FALSE);
        break;
    }
  }

  if (pCtx->checkCisTerm)
  {
    (pCtx->checkCisTerm)(LCTR_GET_CONN_HANDLE(pCtx));
  }

  /* Terminate connection */
  if (lctrCheckForLinkTerm(pCtx))
  {
    lctrSendConnMsg(pCtx, LCTR_CONN_TERMINATED);
    WsfTimerStop(&pCtx->tmrSupTimeout);
    return;
  }

  /* Enhanced Connection Update. */
  uint16_t numIntervals = 0;
  if (lctrCalcSubrateConnEventsFn)
  {
    if ((numIntervals = lctrCalcSubrateConnEventsFn(pCtx, lctrMstConnIsr.numTxData + lctrMstConnIsr.numRxData)) > 0)
    {
      pCtx->eventCounter += numIntervals;
      lctrChSelHdlr[pCtx->usedChSel](pCtx, numIntervals - 1);
    }
  }

  if (pCtx->data.mst.sendConnUpdInd)
  {
    uint8_t *pPdu;

    if ((pPdu = lctrTxCtrlPduAlloc(LL_CONN_UPD_IND_PDU_LEN)) != NULL)
    {
      pCtx->data.mst.sendConnUpdInd = FALSE;

      uint16_t ceOffset;
#if (LL_ENABLE_TESTER)
      if (llTesterCb.eventCounterOverride == TRUE)
      {
        ceOffset = llTesterCb.eventCounterOffset + 1;          /* +1 for next CE */
      }
      else
#endif
      {
        ceOffset = ((LL_MIN_INSTANT + 1 +     /* +1 for next CE */
                     pCtx->maxLatency) *      /* ensure slave will listen to this packet */
                    pCtx->ecu.srFactor);      /* include subrating factor */

        /* TODO: accommodate pCtx->connParam.offset[]. */
      }
      pCtx->connUpd.instant = pCtx->eventCounter + ceOffset;

      /* Use smallest txWindowOffset (i.e. 0) to minimize data loss. */
      uint32_t txWinOffsetUsec = SchRmGetOffsetUsec(0, LCTR_GET_CONN_HANDLE(pCtx), pOp->dueUsec);
      pCtx->connUpd.txWinOffset = LCTR_US_TO_CONN_IND(txWinOffsetUsec);

      lctrPackConnUpdInd(pPdu, &pCtx->connUpd);
      lctrTxCtrlPduQueue(pCtx, pPdu);
    }
    /* else retry at next lctrMstConnEndOp() event. */
  }

  if (pCtx->sendPerSync)
  {
    pCtx->sendPerSync = FALSE;
    if (pCtx->perSyncSrc == LCTR_SYNC_SRC_SCAN)
    {
      if (lctrSendPerSyncFromScanFn)
      {
        lctrSendPerSyncFromScanFn(pCtx);
      }
    }
    else  /* (pCtx->perSyncSrc == LCTR_SYNC_SRC_BCST) */
    {
      if (lctrSendPerSyncFromBcstFn)
      {
        lctrSendPerSyncFromBcstFn(pCtx);
      }
    }
  }

  /*** Update for next operation ***/

  uint32_t anchorPointUsec = pOp->dueUsec;

  if (pBle->chan.tifsTxPhyOptions != BB_PHY_OPTIONS_DEFAULT)
  {
    /* Set PHY options to host defined behavior. */
    pBle->chan.initTxPhyOptions = pBle->chan.tifsTxPhyOptions;
  }
  else
  {
    /* Set PHY options to RX PHY Options*/
    pBle->chan.initTxPhyOptions = pConn->rxPhyOptions;
  }

  if (pBle->chan.txPhy == BB_PHY_BLE_CODED)
  {
    pBle->chan.txPower = LCTR_GET_TXPOWER(pCtx, pBle->chan.txPhy, pBle->chan.initTxPhyOptions);
  }

  while (TRUE)
  {
    numIntervals += 1;
    pCtx->eventCounter += 1;

    uint32_t connInterUsec = LCTR_CONN_IND_US(numIntervals * pCtx->connInterval);
#if (LL_ENABLE_TESTER)
    if (llTesterCb.connIntervalUs)
    {
      connInterUsec = (numIntervals * llTesterCb.connIntervalUs);
    }
#endif

    /* Advance to next interval. */
    pOp->dueUsec = anchorPointUsec + connInterUsec;

    if ((pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD) &&
        (pCtx->eventCounter == pCtx->connUpd.instant))
    {
      lctrMstConnUpdateOp(pCtx);
    }
    else if ((pCtx->llcpActiveProc == LCTR_PROC_CMN_CH_MAP_UPD) &&
             (pCtx->eventCounter == pCtx->chanMapUpd.instant))
    {
      lctrMstChanMapUpdateOp(pCtx);
    }
    else if ((pCtx->llcpActiveProc == LCTR_PROC_PHY_UPD) &&
             (pCtx->eventCounter == pCtx->phyUpd.instant))
    {
      lctrMstPhyUpdateOp(pCtx);
    }

    pBle->chan.chanIdx = lctrChSelHdlr[pCtx->usedChSel](pCtx, 0);

    if (SchInsertAtDueTime(pOp, lctrConnResolveConflict))
    {
      break;
    }

    LL_TRACE_WARN2("!!! CE schedule conflict handle=%u, eventCounter=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->eventCounter);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Abort a connection operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrMstConnAbortOp(BbOpDesc_t *pOp)
{
  lctrConnCtx_t * const pCtx = pOp->pCtx;

  LL_TRACE_WARN2("!!! Connection Master BOD aborted, handle=%u, eventCounter=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->eventCounter);

  /* Reset operation to state before BOD began */
  if (pCtx->emptyPduPend &&
      pCtx->emptyPduFirstAtt)
  {
    /* A Tx was setup for a first empty PDU, but that empty PDU was never transmitted. */
    pCtx->emptyPduPend = FALSE;
  }

  if (pCtx->state == LCTR_CONN_STATE_INITIALIZED)
  {
    return;
  }

  lctrMstConnIsr.consCrcFailed = 0;
  lctrMstConnIsr.rxFromSlave = FALSE;

  lctrMstConnEndOp(pOp);
}

/*************************************************************************************************/
/*!
 *  \brief      Complete a transmitted data buffer.
 *
 *  \param      pOp     Operation context.
 *  \param      status  Transmit status.
 */
/*************************************************************************************************/
void lctrMstConnTxCompletion(BbOpDesc_t *pOp, uint8_t status)
{
  if (status == BB_STATUS_SUCCESS)
  {
    lctrConnCtx_t * const pCtx = pOp->pCtx;

    lctrSetControlPduAck(pCtx);

    if (pCtx->txHdr.len)
    {
      lctrMstConnIsr.numTxData++;
    }
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
void lctrMstConnRxCompletion(BbOpDesc_t *pOp, uint8_t *pRxBuf, uint8_t status)
{
  lctrConnCtx_t * const pCtx = pOp->pCtx;
  pCtx->lastRxStatus = status;

  /* Local state */
  uint8_t *pNextRxBuf = NULL;
  bool_t loadRxBuf = FALSE;
  bool_t txPduIsAcked = FALSE;

  /*** LLCP instant completion processing ***/

  if (pCtx->llcpInstantComp)
  {
    pCtx->llcpInstantComp = FALSE;
    lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_PROC_CMPL);
  }

  /*** Cancellation processing ***/

  if (status == BB_STATUS_CANCELED)
  {
    lctrRxPduFree(pRxBuf);
    goto Done;
  }

  /*** Connection event pre-processing ***/

  if (lctrCheckForLinkTerm(pCtx) ||
      (status == BB_STATUS_FAILED) ||
      (status == BB_STATUS_RX_TIMEOUT))
  {
    if (status == BB_STATUS_RX_TIMEOUT)
    {
      LL_TRACE_WARN3("lctrMstConnRxCompletion: BB failed with status=RX_TIMEOUT, eventCounter=%u, bleChan=%u, handle=%u", pCtx->eventCounter, pCtx->bleData.chan.chanIdx, LCTR_GET_CONN_HANDLE(pCtx));
    }

    if (status == BB_STATUS_FAILED)
    {
      LL_TRACE_ERR3("lctrMstConnRxCompletion: BB failed with status=FAILED, eventCounter=%u, bleChan=%u, handle=%u", pCtx->eventCounter, pCtx->bleData.chan.chanIdx, LCTR_GET_CONN_HANDLE(pCtx));
    }

    BbSetBodTerminateFlag();
    lctrRxPduFree(pRxBuf);
    goto Done;
  }

  /*** Receive packet pre-processing ***/

  if (status == BB_STATUS_SUCCESS)
  {
    lctrMstConnIsr.rxFromSlave = TRUE;

    /* Reset consecutive CRC failure counter. */
    lctrMstConnIsr.consCrcFailed = 0;
  }
  else if (status == BB_STATUS_CRC_FAILED)
  {
    lctrMstConnIsr.consCrcFailed++;
    if (lctrMstConnIsr.consCrcFailed >= LCTR_MAX_CONS_CRC)
    {
      /* Close connection event. */
      BbSetBodTerminateFlag();
      lctrRxPduFree(pRxBuf);
      goto Done;
    }
    goto SetupTx;
  }

  lctrUnpackDataPduHdr(&pCtx->rxHdr, pRxBuf);

  /* Check for MIC failure and if this is not a re-transmission */
  if ((status == BB_STATUS_MIC_FAILED) &&
      ((pCtx->rxHdr.sn ^ pCtx->txHdr.nesn) & 1) == 0)
  {
    LL_TRACE_WARN3("lctrMstConnRxCompletion: BB failed with status=MIC_FAILED, eventCounter=%u, bleChan=%u, handle=%u", pCtx->eventCounter, pCtx->bleData.chan.chanIdx, LCTR_GET_CONN_HANDLE(pCtx));
    lctrSendConnMsg(pCtx, LCTR_CONN_TERM_MIC_FAILED);
    /* Close connection event. */
    BbSetBodTerminateFlag();
    lctrRxPduFree(pRxBuf);
    goto Done;
  }

  if (pCtx->rxHdr.len)
  {
    lctrMstConnIsr.numRxData++;
  }

#if (LL_ENABLE_TESTER)
  if (llTesterCb.ackMode != LL_TESTER_ACK_MODE_NORMAL)
  {
    LctrProcessRxTxAck(pCtx, pRxBuf, &pNextRxBuf, &txPduIsAcked);
  }
  else
#endif
  {
    pNextRxBuf = lctrProcessRxAck(pCtx);
    txPduIsAcked = lctrProcessTxAck(pCtx);
  }

  /*** Scheduler termination ***/

  if (lctrExceededMaxDur(pCtx, pOp->dueUsec, pCtx->effConnDurUsec))
  {
    BbSetBodTerminateFlag();
    goto PostProcessing;
  }

  /*** Setup for transmit ***/
SetupTx:

  /* Optionally require Tx if last packet was NACK'ed. */
  txPduIsAcked = txPduIsAcked || !lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_MST_RETX_AFTER_RX_NACK);
  if (lctrSetupForTx(pCtx, status, !txPduIsAcked) == 0)
  {
    BbSetBodTerminateFlag();
    goto PostProcessing;
  }

  /* If Tx failed, return immediately after queueing Rx data. */
  if (BbGetBodTerminateFlag())
  {
    goto PostProcessing;
  }

  /*** Setup for receive ***/

  /* Always Rx after a Tx. */
  loadRxBuf = TRUE;

  /*** Packet post-processing ***/
PostProcessing:

  /* Tx post-processing. */
  lctrProcessTxAckCleanup(pCtx);

  /* Rx post-processing. */
  lctrCheckControlPduAck(pCtx);
  lctrRxPostProcessing(pCtx, pRxBuf, pNextRxBuf, loadRxBuf);

  /*** ISR complete ***/
Done:
  return;
}
