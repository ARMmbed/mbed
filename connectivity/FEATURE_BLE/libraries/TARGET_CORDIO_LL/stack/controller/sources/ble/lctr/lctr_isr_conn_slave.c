/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave connection ISR callbacks.
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

#include "lctr_int_conn.h"
#include "lctr_int_conn_slave.h"
#include "lctr_int_adv_slave.h"
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
#include "pal_bb.h"
#include <string.h>

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Assert BB meets data PDU requirements. */
WSF_CT_ASSERT((BB_FIXED_DATA_PKT_LEN == 0) ||
              (BB_FIXED_DATA_PKT_LEN >= LCTR_DATA_PDU_MAX_LEN));

/*! \brief      Slave connection ISR control block. */
static struct
{
  uint16_t numTxData;           /*!< Number of Tx data PDU in a single CE. */
  uint16_t numRxData;           /*!< Number of Rx data PDU in a single CE. */
} lctrSlvConnIsr;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

#if (LL_ENABLE_TESTER)
extern void LctrProcessRxTxAck(lctrConnCtx_t *pCtx, uint8_t *pRxBuf, uint8_t **pNextRxBuf, bool_t *pTxPduIsAcked);
#endif

/*************************************************************************************************/
/*!
 *  \brief  Abort the slave latency and recover connection BOD due time and event counter.
 *
 *  \param  pOp     Connection operation.
 */
/*************************************************************************************************/
static void lctrSlvAbortSlvLatency(BbOpDesc_t *pOp)
{
  lctrConnCtx_t * const pCtx = pOp->pCtx;
  const uint32_t curTime = PalBbGetCurrentTime();
  uint32_t connIntervalUsec = LCTR_CONN_IND_US(pCtx->connInterval);
  uint32_t count = 0;

  if (BbGetTargetTimeDelta(pOp->dueUsec, curTime) == 0)
  {
    /* Slave tries to exit latency while the current BoD is executing, do nothing. */
  }
  else if (BbGetTargetTimeDelta(pOp->dueUsec, curTime) > connIntervalUsec)
  {
    /* If the connection BOD is due in the future and after the next immediate anchor point,
     * adjust the event counter. */
    count = LlMathDivideUint32(BbGetTargetTimeDelta(pOp->dueUsec, curTime), connIntervalUsec);
    pCtx->eventCounter -= count;

    /* Need to adjust the lastChanIdx for channel selection algorithm 1. */
    if (pCtx->usedChSel == LL_CH_SEL_1)
    {
      if (pCtx->lastChanIdx >= (count * pCtx->hopInc))
      {
        pCtx->lastChanIdx -= (count * pCtx->hopInc);
      }
      else
      {
        if (((count * pCtx->hopInc) - pCtx->lastChanIdx) <= LL_MAX_NUM_CHAN_DATA)
        {
          /* Avoid modulo operation. */
          pCtx->lastChanIdx = LL_MAX_NUM_CHAN_DATA - ((count * pCtx->hopInc) - pCtx->lastChanIdx);
        }
        else
        {
          pCtx->lastChanIdx = LL_MAX_NUM_CHAN_DATA - LL_MATH_MOD_37(((count * pCtx->hopInc) - pCtx->lastChanIdx));
        }
      }
    }

    /* The anchor point, due time, minimum duration and Rx timeout will be adjusted accordingly later. */
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Update a connection operation.
 *
 *  \param  pCtx          Connection context.
 *  \param  ignoreOffset  txWinOffset will be ignored if TRUE.
 */
/*************************************************************************************************/
static void lctrSlvConnUpdateOp(lctrConnCtx_t *pCtx, bool_t ignoreOffset)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &pCtx->connBod;
  BbBleData_t * const pBle = &pCtx->bleData;
  BbBleSlvConnEvent_t * const pConn = &pBle->op.slvConn;
  lctrConnUpdInd_t * const pConnUpdInd = &pCtx->connUpd;

  /*** Connection context setup ***/

  const uint16_t connIntervalOld = pCtx->connInterval;
  const uint16_t latencyOld      = pCtx->maxLatency;
  const uint16_t supTimeoutMsOld = pCtx->supTimeoutMs;

  /* Make sure set the anchor point to the instant in the connection update indication. */
  const uint16_t numIntervals = pCtx->eventCounter - pCtx->data.slv.lastActiveEvent + 1;
  const uint32_t timeUsec     = LCTR_CONN_IND_US(connIntervalOld * numIntervals);
  pCtx->data.slv.anchorPointUsec  = pCtx->data.slv.anchorPointUsec + timeUsec;

  if (pCtx->connUpd.instant != pCtx->data.slv.lastActiveEvent)
  {
    /* Save the unsynced time before the connection update. */
    pCtx->data.slv.unsyncedTime = timeUsec;
  }

  pCtx->data.slv.lastActiveEvent = pCtx->eventCounter + 1;   /* guarantee execution of first connection event. */
  /* Update connection parameters. */
  pCtx->connInterval             = pConnUpdInd->interval;
  pCtx->maxLatency               = pConnUpdInd->latency;
  pCtx->supTimeoutMs             = LCTR_CONN_IND_TO_MS(pConnUpdInd->timeout);

  /*** General setup ***/

  if (ignoreOffset == FALSE)
  {
    const uint32_t txWinOffsetUsec    = LCTR_CONN_IND_US(pConnUpdInd->txWinOffset);
    const uint32_t txWinSizeUsec      = LCTR_CONN_IND_US(pConnUpdInd->txWinSize);
    const uint32_t wwTxWinOffsetUsec  = lctrCalcWindowWideningUsec((txWinOffsetUsec + txWinSizeUsec), pCtx->data.slv.totalAcc);

    /* txWinOffset is relative to anchorPoint. */
    pCtx->data.slv.anchorPointUsec += txWinOffsetUsec;

    /* Add additional time due to Tx window offset and subtract WW due to Tx window offset. */
    pOp->dueUsec += txWinOffsetUsec - wwTxWinOffsetUsec;

    pCtx->data.slv.txWinSizeUsec = txWinSizeUsec;

    /* Add additional time due to Tx window size and WW due to Tx window offset. */
    pOp->minDurUsec +=  wwTxWinOffsetUsec;

    /*** BLE general setup ***/

    pConn->rxSyncDelayUsec += txWinSizeUsec + (wwTxWinOffsetUsec << 1);
  }
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

  /* Reset flags. */
  pCtx->data.slv.initAckRcvd = FALSE;

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
static void lctrSlvChanMapUpdateOp(lctrConnCtx_t *pCtx)
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
static void lctrSlvPhyUpdateOp(lctrConnCtx_t *pCtx)
{
  BbBleData_t * pBle = &pCtx->bleData;
  uint8_t txPhyOld = lctrPhyToPhysBit(pBle->chan.txPhy);
  uint8_t rxPhyOld = lctrPhyToPhysBit(pBle->chan.rxPhy);

  /* Notify host only if PHY changed. */
  if ((pCtx->phyUpd.slaveToMasterPhy != LL_PHYS_NONE) &&
      (pCtx->phyUpd.slaveToMasterPhy != txPhyOld))
  {
    pBle->chan.txPhy = lctrPhysBitToPhy(pCtx->phyUpd.slaveToMasterPhy);
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
        lctrSendPowerChangeIndCback(pCtx, pBle->chan.txPhy, 0, pBle->chan.txPower, TRUE);
      }
    }
  }
  if ((pCtx->phyUpd.masterToSlavePhy != LL_PHYS_NONE) &&
      (pCtx->phyUpd.masterToSlavePhy != rxPhyOld))
  {
    pBle->chan.rxPhy = lctrPhysBitToPhy(pCtx->phyUpd.masterToSlavePhy);
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
 *  \brief  Initialize connection event resources.
 *
 *  \param  pCtx        Connection context.
 */
/*************************************************************************************************/
static void lctrSlvInitConnIsr(lctrConnCtx_t *pCtx)
{
  pCtx->data.slv.consCrcFailed = 0;
  pCtx->data.slv.syncWithMaster = FALSE;
  pCtx->data.slv.rxFromMaster = FALSE;
  pCtx->data.slv.firstRxStartTsUsec = 0;

  lctrSlvConnIsr.numTxData = 0;
  lctrSlvConnIsr.numRxData = 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a channel status message.
 *
 *  \param  pBuf        Packed buffer.
 */
/*************************************************************************************************/
static void lctrPackChannelStatusMsg(uint8_t *pBuf)
{
  unsigned int i;
  for (i = 0; i < LL_MAX_NUM_CHAN_DATA; i++)
  {
    UINT8_TO_BSTREAM(pBuf, (((UINT64_C(1) << i) & lmgrCb.chanClass) ? LL_CH_CLASS_UNKNOWN : LL_CH_CLASS_BAD));
  }
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
void lctrSlvConnBeginOp(BbOpDesc_t *pOp)
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

  lctrSlvInitConnIsr(pCtx);

  /*** Setup receiver ***/

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
void lctrSlvConnCleanupOp(BbOpDesc_t *pOp)
{
}

/*************************************************************************************************/
/*!
 *  \brief  End a connection operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrSlvConnEndOp(BbOpDesc_t *pOp)
{
  /* Pre-resolve common structures for efficient access. */
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleSlvConnEvent_t * const pConn = &pBle->op.slvConn;
  lctrConnCtx_t * const pCtx = pOp->pCtx;

#if (LL_ENABLE_TESTER == TRUE)
  pBle->chan.txPower = LCTR_GET_TXPOWER(pCtx, pCtx->bleData.chan.txPhy, pCtx->bleData.chan.initTxPhyOptions);
#endif

  /* Process event completion */
  if (pCtx->data.slv.abortSlvLatency)
  {
    lctrSlvAbortSlvLatency(pOp);
    pCtx->data.slv.abortSlvLatency = FALSE;
  }

  pCtx->rssi = pConn->rssi;
  if (pCtx->monitoringState == LCTR_PC_MONITOR_ENABLED)
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

  if (pCtx->data.slv.syncWithMaster)
  {
    /* Re-sync to master's clock. */
    pCtx->data.slv.anchorPointUsec = pCtx->data.slv.firstRxStartTsUsec;
    pCtx->data.slv.lastActiveEvent = pCtx->eventCounter + 1;
    pCtx->svtState = LCTR_SVT_STATE_IDLE;

    /* Tx window no longer needed. */
    pCtx->data.slv.txWinSizeUsec = 0;

    /* Now sync'd to master, reset the unsynced time before connection update. */
    pCtx->data.slv.unsyncedTime = 0;
  }

  if (!pCtx->connEst && (pCtx->data.slv.rxFromMaster || (pCtx->data.slv.consCrcFailed > 0)))
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
  else if (pCtx->data.slv.rxFromMaster)
  {
    /* Reset supervision timer. */
    WsfTimerStartMs(&pCtx->tmrSupTimeout, pCtx->supTimeoutMs);
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

  if (pLctrVsHdlrs && pLctrVsHdlrs->ceCleanup)
  {
    pLctrVsHdlrs->ceCleanup(LCTR_GET_CONN_HANDLE(pCtx));
  }

  /* Slave initiates the startup LLCP procedure when receiving a non-control PDU from both master and slave.
   * This message shall be sent only once. */
  if ((pCtx->isFirstNonCtrlPdu == TRUE) &&
      (pCtx->isSlvReadySent == FALSE))
  {
    pCtx->isSlvReadySent = TRUE;
    lctrSendConnMsg(pCtx, LCTR_CONN_SLV_INIT_STARTUP_LLCP);
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

  /* Check if channel status update is required. */
  if ((pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD) && lctrSlvCheckConnUpdInstant(pCtx) &&
      (pCtx->eventCounter == pCtx->connUpd.instant))
  {
    lctrSlvConnUpdateOp(pCtx, TRUE);
    LL_TRACE_WARN1("Received connection update at instant, applying immediately at CE=%d", pCtx->eventCounter);
  }

  /* If the channel map has changed since the last time, */
  /* Send master the status indication. */
  if (pCtx->chanStatRptEnable && pCtx->data.slv.queuedChanStatusTs &&
     (BbGetTargetTimeDelta(pOp->dueUsec, pCtx->data.slv.lastStatusSentTs) > pCtx->data.slv.chanStatMinIntUs))
  {
    /* Make sure that the report has not passed the max delay parameter. */
    if (BbGetTargetTimeDelta(pOp->dueUsec, pCtx->data.slv.queuedChanStatusTs) < pCtx->data.slv.chanStatMaxDelay)
    {
      pCtx->data.slv.lastStatusSentTs = pOp->dueUsec;

      lctrMsgChStatusInd_t *pMsg;
      if ((pMsg = (lctrMsgChStatusInd_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        pMsg->hdr.handle = LCTR_GET_CONN_HANDLE(pCtx);
        pMsg->hdr.dispId = LCTR_DISP_CONN;
        pMsg->hdr.event  = LCTR_CONN_LLCP_CHANNEL_STATUS;

        lctrPackChannelStatusMsg(pMsg->chanStatus);

        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
      }
    }

    /* Reset the timestamp to signal that there is no pending status report. */
    pCtx->data.slv.queuedChanStatusTs = 0;
  }

  /*** Update for next operation ***/

  uint16_t numUnsyncIntervals = pCtx->eventCounter - pCtx->data.slv.lastActiveEvent + 1;

  uint16_t numSkipCe = 0;
  uint16_t numSkipSr = 0;
  uint16_t srLatency = pCtx->maxLatency;

  /* Enhanced Connection Update. */
  if (lctrCalcSubrateConnEventsFn)
  {
    numSkipSr = lctrCalcSubrateConnEventsFn(pCtx, lctrSlvConnIsr.numTxData + lctrSlvConnIsr.numRxData);
    srLatency = (pCtx->maxLatency * pCtx->ecu.srFactor) + numSkipSr;
  }

  if ((pCtx->data.slv.abortSlvLatency == FALSE) &&
      lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_ENA_SLV_LATENCY) &&
      pCtx->maxLatency &&
      pCtx->data.slv.initAckRcvd &&
      pCtx->data.slv.rxFromMaster &&
      WsfQueueEmpty(&pCtx->txArqQ) &&
      (pCtx->state != LCTR_CONN_STATE_TERMINATING))
  {
    if (pCtx->llcpActiveProc == LCTR_PROC_INVALID)
    {
      numSkipCe = srLatency;
    }
    else
    {
      /* Still apply the slave latency if the instant is not reached for the following LLCPs. */
      if ((pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD) && lctrSlvCheckConnUpdInstant(pCtx))
      {
        if ((uint16_t)(pCtx->connUpd.instant - pCtx->eventCounter) > (srLatency + 1))
        {
          numSkipCe = srLatency;
        }
        else if ((pCtx->connUpd.instant - pCtx->eventCounter) > 1)
        {
          /*  Serve CE of (instant - 1) so that connection update is applied in the end callback of CE(instant - 1). */
          numSkipCe = pCtx->connUpd.instant - pCtx->eventCounter - 2;
        }
      }
      else if ((pCtx->llcpActiveProc == LCTR_PROC_CMN_CH_MAP_UPD) &&
               (pCtx->cmnState == LCTR_CMN_STATE_BUSY))
      {
        if ((uint16_t)(pCtx->chanMapUpd.instant - pCtx->eventCounter) > (srLatency + 1))
        {
          numSkipCe = srLatency;
        }
        else if ((pCtx->chanMapUpd.instant - pCtx->eventCounter) > 1)
        {
          /*  Serve CE of (instant - 1) so that new channe map is applied in the end callback of CE(instant - 1). */
          numSkipCe = pCtx->chanMapUpd.instant - pCtx->eventCounter - 2;
        }
      }
      else if ((pCtx->llcpActiveProc == LCTR_PROC_PHY_UPD) && (pCtx->isSlvPhyUpdInstant == TRUE))
      {
        if ((uint16_t)(pCtx->phyUpd.instant - pCtx->eventCounter) > (srLatency + 1))
        {
          numSkipCe = srLatency;
        }
        else if ((pCtx->phyUpd.instant - pCtx->eventCounter) > 1)
        {
          /*  Serve CE of (instant - 1) so that phy update is applied in the end callback of CE(instant - 1). */
          numSkipCe = pCtx->phyUpd.instant - pCtx->eventCounter - 2;
        }
      }
    }
  }
  else
  {
    /* No slave latency; listen at the next subrated connection events. */
    numSkipCe = numSkipSr;
  }

  if (numSkipCe > 0)
  {
    if (numSkipCe > numSkipSr)
    {
      LL_TRACE_INFO2("Applying slave latency, last attended eventCounter=%u, wake up at eventCounter=%u", pCtx->eventCounter, pCtx->eventCounter + numSkipCe + 1);
    }

    pCtx->eventCounter += numSkipCe;
    numUnsyncIntervals += numSkipCe;

    lctrChSelHdlr[pCtx->usedChSel](pCtx, numSkipCe - 1);
  }

  while (TRUE)
  {
    pCtx->eventCounter += 1;
    numUnsyncIntervals += 1;

    uint32_t unsyncTimeUsec = LCTR_CONN_IND_US(pCtx->connInterval * numUnsyncIntervals);

    /* Need to add the unsynced time before connection update. */
    unsyncTimeUsec += pCtx->data.slv.unsyncedTime;

    uint32_t wwTotalUsec    = lctrCalcWindowWideningUsec(unsyncTimeUsec, pCtx->data.slv.totalAcc);
    uint32_t connInterUsec  = LCTR_CONN_IND_US(numUnsyncIntervals * pCtx->connInterval);

    if (wwTotalUsec >= ((connInterUsec >> 1) - WSF_MAX(LL_BLE_TIFS_US, BbGetSchSetupDelayUs())))
    {
      LL_TRACE_WARN2("!!! Terminating connection due to excessive WW handle=%u, eventCounter=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->eventCounter);
      lctrSendConnMsg(pCtx, LCTR_CONN_TERMINATED);
      WsfTimerStop(&pCtx->tmrSupTimeout);
      return;
    }

    /* Advance to next interval. */
    pOp->dueUsec = pCtx->data.slv.anchorPointUsec + connInterUsec - wwTotalUsec;

    /* Use small minDurUsec to improve scheduling. */
    pOp->minDurUsec = pCtx->data.slv.txWinSizeUsec + (2 * LL_MAX_DATA_TIME_MIN) + LL_BLE_TIFS_US + wwTotalUsec;

    pConn->rxSyncDelayUsec = pCtx->data.slv.txWinSizeUsec + (wwTotalUsec << 1);

    if ((pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD) &&
        ((pCtx->eventCounter + 1) == pCtx->connUpd.instant))
    {
      /* Last CE with the old connection parameter must be scheduled, */
      /* especially when connection interval and SVT changes into smaller ones. */
      pCtx->svtState = LCTR_SVT_STATE_FATAL;
    }

    if ((pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD) &&
        (pCtx->eventCounter == pCtx->connUpd.instant))
    {
      lctrSlvConnUpdateOp(pCtx, FALSE);
    }
    else if ((pCtx->llcpActiveProc == LCTR_PROC_CMN_CH_MAP_UPD) &&
              (pCtx->eventCounter == pCtx->chanMapUpd.instant))
    {
      lctrSlvChanMapUpdateOp(pCtx);
    }
    else if ((pCtx->llcpActiveProc == LCTR_PROC_PHY_UPD) &&
              (pCtx->eventCounter == pCtx->phyUpd.instant))
    {
      lctrSlvPhyUpdateOp(pCtx);
    }

    pBle->chan.chanIdx = lctrChSelHdlr[pCtx->usedChSel](pCtx, 0);

    /* Checking if SVT is imminent. */
    if (pCtx->svtState != LCTR_SVT_STATE_FATAL)
    {
      uint8_t nthCE = (pCtx->eventCounter + 1) - pCtx->data.slv.lastActiveEvent;  /* Nth CE since last active CE. */
      uint8_t numCEforSVT = ((uint32_t)pCtx->supTimeoutMs * 1000) / (uint32_t)LCTR_CONN_IND_US(pCtx->connInterval);

      if ((nthCE + 1) == numCEforSVT)
      {
        /* 2nd to the last CE before SVT. */
        pCtx->svtState = LCTR_SVT_STATE_URGENT;
      }
      else if (nthCE >= numCEforSVT)
      {
        /* Last CE before SVT. */
        pCtx->svtState = LCTR_SVT_STATE_FATAL;
      }
    }

    if (SchInsertAtDueTime(pOp, lctrConnResolveConflict))
    {
      break;
    }

    /* TODO: When latency is applied, scheduling conflicts should subtract connection intervals. */

    LL_TRACE_WARN2("!!! CE schedule conflict handle=%u, eventCounter=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->eventCounter);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Abort a connection operation.
 *
 *  \param  pOp     Aborted operation.
 */
/*************************************************************************************************/
void lctrSlvConnAbortOp(BbOpDesc_t *pOp)
{
  lctrConnCtx_t * const pCtx = pOp->pCtx;

  LL_TRACE_WARN2("!!! Connection Slave BOD aborted, handle=%u, eventCounter=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->eventCounter);

  lctrSlvInitConnIsr(pCtx);
  lctrSlvConnEndOp(pOp);
}

/*************************************************************************************************/
/*!
 *  \brief      Complete a transmitted data buffer.
 *
 *  \param      pOp     Operation context.
 *  \param      status  Transmit status.
 */
/*************************************************************************************************/
void lctrSlvConnTxCompletion(BbOpDesc_t *pOp, uint8_t status)
{
  if (status == BB_STATUS_SUCCESS)
  {
    lctrConnCtx_t * const pCtx = pOp->pCtx;

    lctrSetControlPduAck(pCtx);

    if (pCtx->txHdr.len)
    {
      lctrSlvConnIsr.numTxData++;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Complete a received data buffer.
 *
 *  \param      pOp     Operation context.
 *  \param      pRxBuf  Receive buffer.
 *  \param      status  Receive status.
 *
 *  \note       1. pRxBuf must be freed or placed into a queue by this function.
 *              2. If the BOD is terminated during this function, no further receive buffer should
 *                 remain allocated for the caller to clean up.
 */
/*************************************************************************************************/
void lctrSlvConnRxCompletion(BbOpDesc_t *pOp, uint8_t *pRxBuf, uint8_t status)
{
  lctrConnCtx_t * const pCtx = pOp->pCtx;

  BbBleData_t * const pBle = &pCtx->bleData;
  BbBleSlvConnEvent_t * const pConn = &pBle->op.slvConn;
  pCtx->lastRxStatus = status;

  /* Local state */
  bool_t loadRxBuf = FALSE;
  bool_t txPduIsAcked = FALSE;
  uint8_t *pNextRxBuf = NULL;
  uint16_t txLen = 0;

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
      LL_TRACE_WARN3("lctrSlvConnRxCompletion: BB failed with status=RX_TIMEOUT, eventCounter=%u, bleChan=%u, handle=%u", pCtx->eventCounter, pCtx->bleData.chan.chanIdx, LCTR_GET_CONN_HANDLE(pCtx));
    }

    if (status == BB_STATUS_FAILED)
    {
      LL_TRACE_ERR3("lctrSlvConnRxCompletion: BB failed with status=FAILED, eventCounter=%u, bleChan=%u, handle=%u", pCtx->eventCounter, pCtx->bleData.chan.chanIdx, LCTR_GET_CONN_HANDLE(pCtx));
    }

    BbSetBodTerminateFlag();
    lctrRxPduFree(pRxBuf);
    goto Done;
  }

  /* Store anchor point. */
  if ((!pCtx->data.slv.syncWithMaster) &&
      ((status == BB_STATUS_SUCCESS) ||
      (!lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_IGNORE_CRC_ERR_TS) && (status == BB_STATUS_CRC_FAILED))))
  {
    pCtx->data.slv.firstRxStartTsUsec = pConn->startTsUsec;
    pCtx->data.slv.syncWithMaster = TRUE;
  }

  /*** Receive packet pre-processing ***/

  if (status == BB_STATUS_SUCCESS)
  {
    pCtx->data.slv.rxFromMaster = TRUE;

    /* Reset consecutive CRC failure counter. */
    pCtx->data.slv.consCrcFailed = 0;
  }
  else if (status == BB_STATUS_CRC_FAILED)
  {
    pCtx->data.slv.consCrcFailed++;

    if (pCtx->data.slv.consCrcFailed >= LCTR_MAX_CONS_CRC)
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
    LL_TRACE_WARN3("lctrSlvConnRxCompletion: BB failed with status=MIC_FAILED, eventCounter=%u, bleChan=%u, handle=%u", pCtx->eventCounter, pCtx->bleData.chan.chanIdx, LCTR_GET_CONN_HANDLE(pCtx));
    lctrSendConnMsg(pCtx, LCTR_CONN_TERM_MIC_FAILED);
    /* Close connection event. */
    BbSetBodTerminateFlag();
    lctrRxPduFree(pRxBuf);
    goto Done;
  }

  if (pCtx->rxHdr.len)
  {
    lctrSlvConnIsr.numRxData++;
  }

  /* Check LLID from master. */
  if ((pCtx->isSlvReadySent == FALSE) &&
      (pCtx->rxHdr.llid != LL_LLID_CTRL_PDU) &&
      (lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_SLV_DELAY_LLCP_STARTUP)))
  {
    pCtx->isFirstNonCtrlPdu = TRUE;
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

  if (!pCtx->data.slv.initAckRcvd &&
      txPduIsAcked)
  {
    pCtx->data.slv.initAckRcvd = TRUE;
  }

  /*** Setup for transmit ***/
SetupTx:

  /* Slave always transmits after receiving. */
  if ((txLen = lctrSetupForTx(pCtx, status, TRUE)) == 0)
  {
    BbSetBodTerminateFlag();
    goto PostProcessing;
  }

  /* Tx LLID is obtained from lctrSetupForTx(). */
  if ((pCtx->isSlvReadySent == FALSE) &&
      (pCtx->isFirstNonCtrlPdu == TRUE) &&
      (lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_SLV_DELAY_LLCP_STARTUP)))
  {
    /* Set the flag when both master and slave send non-control PDU. */
    if (pCtx->txHdr.llid == LL_LLID_CTRL_PDU)
    {
      pCtx->isFirstNonCtrlPdu = FALSE;
    }
  }

  /* If Tx failed, return immediately after queueing Rx data. */
  if (BbGetBodTerminateFlag())
  {
    goto PostProcessing;
  }

  /*** Setup for receive ***/

  loadRxBuf = ((status == BB_STATUS_CRC_FAILED) || pCtx->rxHdr.md || pCtx->txHdr.md) &&
              !lctrExceededMaxDur(pCtx, pConn->startTsUsec, SchBleCalcDataPktDurationUsec(pCtx->bleData.chan.txPhy, txLen) + pCtx->effConnDurUsec);

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
