/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Scanning master BLE baseband porting implementation file.
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

#include "bb_api.h"
#include "pal_bb.h"
#include "bb_ble_int.h"
#include "sch_api.h"
#include "wsf_math.h"
#include <string.h>
#include "bb_ble_sniffer_api.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

BbBleScanPktStats_t bbScanStats;          /*!< Scan packet statistics. */

extern const BbRtCfg_t *pBbRtCfg;

/*************************************************************************************************/
/*!
 *  \brief      Calculate remaining scan duration.
 *
 *  \param      pBod        Pointer to the BOD.
 *  \param      pScan       Pointer to the scan event operation data.
 *  \param      refTime     Reference time.
 *  \param      setupUsec   Setup delay required in microseconds.
 *
 *  \return     Scan duration in microseconds.
 *
 *  This function calculates the duration of the scan examining:
 *      - total elapsed scan time
 *      - neighboring BOD
 *      - maximum scan period
 */
/*************************************************************************************************/
static uint32_t bbBleCalcScanDurationUsec(BbOpDesc_t *pBod, BbBleMstAdvEvent_t *pScan, uint32_t refTime, uint32_t setupUsec)
{
  BbBleData_t * const pBle = pBod->prot.pBle;

  WSF_ASSERT(pBod->maxDurUsec > 0);

  /* Guard time at the end of a scan window to the next BOD. */
  /* Backoff for RX of preamble and AA which are compensated in BB driver. */
  uint32_t totalGapUsec = BbGetSchSetupDelayUs() + setupUsec;

  switch (pBle->chan.rxPhy)
  {
    case BB_PHY_BLE_1M:
      totalGapUsec += (LL_BLE_US_PER_BYTE_1M * LL_PREAMBLE_LEN_1M) + \
                      (LL_BLE_US_PER_BYTE_1M * LL_AA_LEN);
      break;
    case BB_PHY_BLE_2M:
      totalGapUsec += (LL_BLE_US_PER_BYTE_2M * LL_PREAMBLE_LEN_2M) + \
                      (LL_BLE_US_PER_BYTE_2M * LL_AA_LEN);
      break;
    case BB_PHY_BLE_CODED:
      totalGapUsec += (LL_BLE_US_PER_BIT_CODED_S8 * LL_PREAMBLE_LEN_CODED_BITS) + \
                      (LL_BLE_US_PER_BYTE_CODED_S8 * LL_AA_LEN);
      break;
    default:
      break;
  }

  if ((pScan->elapsedUsec + totalGapUsec) > pBod->maxDurUsec)
  {
    /* Scan window completed. */
    pScan->elapsedUsec = pBod->maxDurUsec;
    return 0;
  }

  uint32_t remTimeUsec = pBod->maxDurUsec - pScan->elapsedUsec;

  if (pBod->pNext)
  {
    uint32_t timeToNextOpUsec = BbGetTargetTimeDelta(pBod->pNext->dueUsec, refTime);

    /* Limit scanning to the edge of neighboring BOD. */
    remTimeUsec = WSF_MIN(remTimeUsec, timeToNextOpUsec);

    if (remTimeUsec < totalGapUsec)
    {
      pScan->elapsedUsec += remTimeUsec;
      return 0;
    }
  }

  /* Reduce time such that advertising exchange can complete before end of the receive window. */
  remTimeUsec -= totalGapUsec;

  /* Limit scanning to the maximum scan period. */
  remTimeUsec = WSF_MIN(remTimeUsec, (pBbRtCfg->maxScanPeriodMs * 1000));

  return remTimeUsec;
}

/*************************************************************************************************/
/*!
 *  \brief      Continue scan operation.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pScan   Pointer to the scan event operation data.
 *
 *  \return     TRUE if the BOD is completed, FALSE if operation continues.
 *
 *  This function is called to continue scan operation.
 */
/*************************************************************************************************/
static bool_t bbContScanOp(BbOpDesc_t *pBod, BbBleMstAdvEvent_t *pScan)
{
  if (BbGetBodTerminateFlag())
  {
    /* Client terminated. */
    return TRUE;
  }

  uint32_t curTime = PalBbGetCurrentTime();
  pScan->elapsedUsec += BbGetTargetTimeDelta(curTime, bbBleCb.lastScanStartUsec);
  bbBleCb.lastScanStartUsec = curTime;

  uint32_t scanDurUsec;

  if ((scanDurUsec = bbBleCalcScanDurationUsec(pBod, pScan, curTime, BbGetSchSetupDelayUs())) == 0)
  {
    return TRUE;
  }

  /* Check if an Aux Scan needs to be started ASAP because of a short auxOffset. */
  if (pScan->auxScanCheckCback && pScan->auxScanCheckCback(pBod, curTime, scanDurUsec))
  {
    pBod->prot.pBle->op.mstAdv.auxScanOpRunning = TRUE;
    return FALSE;
  }

  bbBleCb.bbParam.dueUsec = BbAdjustTime(bbBleCb.lastScanStartUsec + BbGetSchSetupDelayUs());
  bbBleCb.bbParam.rxTimeoutUsec = scanDurUsec;
  PalBbBleSetDataParams(&bbBleCb.bbParam);

  bbBleCb.evtState = BB_EVT_STATE_RX_ADV_IND;

  if (pScan->pTxReqBuf)
  {
    bbBleSetTifs();   /* active scan or initiating */
  }
  else
  {
    bbBleClrIfs();    /* passive scan */
  }
  PalBbBleRxData(pScan->pRxAdvBuf, BB_ADVB_MAX_LEN);

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Tx completion for scanning master operation.
 *
 *  \param      status      Completion status.
 *
 *  Setup for next action in the operation or complete the operation.
 */
/*************************************************************************************************/
static void bbMstScanTxCompCback(uint8_t status)
{
  BB_ISR_START();

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleMstAdvEvent_t * const pScan = &pCur->prot.pBle->op.mstAdv;

  bool_t bodComplete = FALSE;
  bool_t bodCont = FALSE;

  if (pScan->auxScanTxCompCback && (pScan->auxScanOpRunning == TRUE))
  {
    /* This is a linked operation. Call the Aux Scan callback. */
    if (pScan->auxScanTxCompCback(pScan->auxScanBod, status) == TRUE)
    {
      /* BOD complete. Clear the linked BOD. */
      pScan->auxScanBod = NULL;
      pScan->auxScanOpRunning = FALSE;
    }
    return;
  }

#if (BB_SNIFFER_ENABLED == TRUE)
  /* Save evtState to be used later in packet forwarding. */
  uint8_t evtState = bbBleCb.evtState;
  BbBleSnifferPkt_t * pPkt = NULL;
  if (bbSnifferCtx.enabled)
  {
    pPkt = bbSnifferCtx.snifferGetPktFn();
  }
#endif

  switch (bbBleCb.evtState++)
  {
    case BB_EVT_STATE_TX_SCAN_OR_CONN_INIT:
    {
      switch (status)
      {
        case BB_STATUS_SUCCESS:
        {
          WSF_ASSERT(pScan->txReqCback);
          WSF_ASSERT(pScan->pTxReqBuf);

          if (pScan->txReqCback(pCur, pScan->pTxReqBuf))
          {
            WSF_ASSERT(pScan->pRxRspBuf);

            BB_ISR_MARK(bbScanStats.rxSetupUsec);

            bbBleClrIfs();      /* last operation in event */
            PalBbBleRxTifsData(pScan->pRxRspBuf, BB_ADVB_MAX_LEN);

            /* Rx may fail; no more important statements in the !bodComplete code path */
          }
          else
          {
            bodCont = TRUE;
          }

          break;
        }

        case BB_STATUS_FAILED:
        default:
          bodComplete = TRUE;
          break;
      }

      /* Update statistics. */
      switch (status)
      {
        case BB_STATUS_SUCCESS:
          BB_INC_STAT(bbScanStats.txReq);
          break;
        default:
          BB_INC_STAT(bbScanStats.errScan);
          break;
      }

      break;
    }

    default:          /* unexpected state */
      WSF_ASSERT(FALSE);
      break;
  }

  if (bodCont)
  {
    /* Cancel TIFS timer if active. */
    /* coverity[dead_error_condition] */
    switch (status)
    {
      case BB_STATUS_SUCCESS:
        PalBbBleCancelTifs();
        break;
      default:
        /* coverity[dead_error_begin] */
        break;
    }

    bodComplete = bbContScanOp(pCur, pScan);
  }

  if (bodComplete)
  {
    BbTerminateBod();
  }

#if (BB_SNIFFER_ENABLED == TRUE)
  if (pPkt)
  {
    pPkt->pktType.meta.type = BB_SNIFF_PKT_TYPE_TX;
    pPkt->pktType.meta.status = status;
    pPkt->pktType.meta.state = evtState;

    bbBleSnifferMstScanPktHandler(pCur, pPkt);
  }
#endif

  BB_ISR_MARK(bbScanStats.txIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Rx completion for scanning master operation.
 *
 *  \param      status          Reception status.
 *  \param      rssi            RSSI value.
 *  \param      crc             CRC value.
 *  \param      timestamp       Start of packet timestamp in microseconds.
 *  \param      rxPhyOptions    Rx PHY options.
 *
 *  Setup for next action in the operation or complete the operation.
 */
/*************************************************************************************************/
static void bbMstScanRxCompCback(uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp, uint8_t rxPhyOptions)
{
  BB_ISR_START();

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleData_t * const pBle = pCur->prot.pBle;
  BbBleMstAdvEvent_t * const pScan = &pBle->op.mstAdv;

  bool_t bodComplete = FALSE;
  bool_t bodCont = FALSE;

  if (pScan->auxScanRxCompCback && pScan->auxScanOpRunning == TRUE)
  {
    /* This is a linked operation. Call the Aux Scan callback. */
    if (pScan->auxScanRxCompCback(pScan->auxScanBod, status, rssi, crc, timestamp, rxPhyOptions) == TRUE)
    {
      /* BOD complete. Clear the linked BOD. */
      pScan->auxScanBod = NULL;
      pScan->auxScanOpRunning = FALSE;
    }

    return;
  }

#if (BB_SNIFFER_ENABLED == TRUE)
  /* Save evtState to be used later in packet forwarding. */
  uint8_t evtState = bbBleCb.evtState;
  BbBleSnifferPkt_t * pPkt = NULL;
  if (bbSnifferCtx.enabled)
  {
    pPkt = bbSnifferCtx.snifferGetPktFn();
  }
#endif

  switch (bbBleCb.evtState++)
  {
    case BB_EVT_STATE_RX_ADV_IND:
    {
      switch (status)
      {
        case BB_STATUS_SUCCESS:
        {
          WSF_ASSERT(pScan->rxAdvCback);
          WSF_ASSERT(pScan->pRxAdvBuf);

          pScan->advRssi = rssi;
          pScan->advCrc = crc;
          pScan->advStartTsUsec = timestamp;
          pScan->advRxPhyOptions = rxPhyOptions;

          bool_t pduAllow = BbBlePduFiltCheck(pScan->pRxAdvBuf, &pBle->pduFilt, FALSE, &pScan->filtResults);

          if (pduAllow && pScan->rxAdvCback(pCur, pScan->pRxAdvBuf))
          {
            WSF_ASSERT(pScan->pTxReqBuf);

            BB_ISR_MARK(bbScanStats.txSetupUsec);

            PalBbBleTxBufDesc_t desc = {.pBuf = pScan->pTxReqBuf, .len = pScan->txReqLen};

            bbBleSetTifs();
            PalBbBleTxTifsData(&desc, 1);

            /* Tx may fail; no more important statements in the !bodComplete code path */
          }
          else
          {
            bodCont = TRUE;
          }

#if (BB_SNIFFER_ENABLED == TRUE)
          /* Pack the rx buffer here before it is overwritten. */
          if (pPkt)
          {
            memcpy(pPkt->pktType.advPkt.hdr, pScan->pRxAdvBuf, LL_ADV_HDR_LEN);
          }
#endif

          if (pduAllow && pScan->rxAdvPostCback)
          {
            pScan->rxAdvPostCback(pCur, pScan->pRxAdvBuf);
          }

          break;
        }

        case BB_STATUS_RX_TIMEOUT:
        case BB_STATUS_CRC_FAILED:
          bodCont = TRUE;
          break;

        case BB_STATUS_FAILED:
        default:
          bodComplete = TRUE;
          break;
      }

      /* Update statistics. */
      switch (status)
      {
        case BB_STATUS_SUCCESS:
          BB_INC_STAT(bbScanStats.rxAdv);
          break;
        case BB_STATUS_RX_TIMEOUT:
          BB_INC_STAT(bbScanStats.rxAdvTimeout);
          break;
        case BB_STATUS_CRC_FAILED:
          BB_INC_STAT(bbScanStats.rxAdvCrc);
          break;
        case BB_STATUS_FAILED:
        default:
          BB_INC_STAT(bbScanStats.errScan);
          break;
      }

      break;
    }

    case BB_EVT_STATE_RX_SCAN_OR_CONN_RSP:
    {
      WSF_ASSERT(pScan->rxRspCback);
      WSF_ASSERT(pScan->pRxRspBuf);

      switch (status)
      {
        case BB_STATUS_SUCCESS:
#if (BB_SNIFFER_ENABLED == TRUE)
          /* If the rx was successful, pack the rx sniffer header here before it is overwritten. */
          if (pPkt)
          {
            memcpy(pPkt->pktType.advPkt.hdr, pScan->pRxRspBuf, LL_ADV_HDR_LEN);
          }
#endif

          if (BbBlePduFiltCheck(pScan->pRxRspBuf, &pBle->pduFilt, FALSE, &pScan->filtResults))
          {
            pScan->rxRspCback(pCur, pScan->pRxRspBuf);
          }
          bodCont = TRUE;
          break;

        case BB_STATUS_RX_TIMEOUT:
        case BB_STATUS_CRC_FAILED:
          pScan->rxRspCback(pCur, NULL);
          bodCont = TRUE;
          break;

        case BB_STATUS_FAILED:
        default:
          bodComplete = TRUE;
          break;
      }

      /* Update statistics. */
      switch (status)
      {
        case BB_STATUS_SUCCESS:
          BB_INC_STAT(bbScanStats.rxRsp);
          break;
        case BB_STATUS_RX_TIMEOUT:
          BB_INC_STAT(bbScanStats.rxRspTimeout);
          break;
        case BB_STATUS_CRC_FAILED:
          BB_INC_STAT(bbScanStats.rxRspCrc);
          break;
        case BB_STATUS_FAILED:
        default:
          BB_INC_STAT(bbScanStats.errScan);
          break;
      }

      break;
    }

    default:        /* invalid state */
      WSF_ASSERT(FALSE);
      break;
  }

  if (bodCont)
  {
    /* Cancel TIFS timer if active. */
    switch (status)
    {
      case BB_STATUS_SUCCESS:
      case BB_STATUS_CRC_FAILED:
        PalBbBleCancelTifs();
        break;
      default:
        break;
    }

    bodComplete = bbContScanOp(pCur, pScan);
  }

  if (bodComplete)
  {
    BbTerminateBod();
  }

#if (BB_SNIFFER_ENABLED == TRUE)
  if (pPkt)
  {
    pPkt->pktType.meta.type = BB_SNIFF_PKT_TYPE_RX;
    pPkt->pktType.meta.rssi = rssi;
    pPkt->pktType.meta.timeStamp = timestamp;
    pPkt->pktType.meta.status = status;
    pPkt->pktType.meta.state = evtState;

    bbBleSnifferMstScanPktHandler(pCur, pPkt);
  }
#endif

  BB_ISR_MARK(bbScanStats.rxIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Execute scanning master BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbMstExecuteScanOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  BbBleMstAdvEvent_t * const pScan = &pBod->prot.pBle->op.mstAdv;
  uint32_t curTime = PalBbGetCurrentTime();

  /* Check if dueUsec time is in the past. */
  if (BbGetTargetTimeDelta(pBod->dueUsec, curTime) == 0)
  {
    /* Update dueUsec time and start Scan for the remaining duration. */
    bbBleCb.lastScanStartUsec = pBod->dueUsec;
    pBod->dueUsec = curTime + BbGetSchSetupDelayUs();
    pScan->elapsedUsec = BbGetTargetTimeDelta(curTime, bbBleCb.lastScanStartUsec);
  }

  if (pScan->preExecCback)
  {
    pScan->preExecCback(pBod);
  }

  uint32_t scanDurUsec;

  if ((scanDurUsec = bbBleCalcScanDurationUsec(pBod, pScan, pBod->dueUsec, 0)) == 0)
  {
    BbSetBodTerminateFlag();
    return;
  }

  bbBleCb.lastScanStartUsec = pBod->dueUsec;

  PalBbBleSetChannelParam(&pBle->chan);

  bbBleCb.bbParam.txCback       = bbMstScanTxCompCback;
  bbBleCb.bbParam.rxCback       = bbMstScanRxCompCback;
  bbBleCb.bbParam.rxTimeoutUsec = scanDurUsec;
  bbBleCb.bbParam.dueUsec       = BbAdjustTime(pBod->dueUsec);
  pBod->dueUsec = bbBleCb.bbParam.dueUsec;
  PalBbBleSetDataParams(&bbBleCb.bbParam);

  bbBleCb.evtState = 0;

  if (pScan->pTxReqBuf)
  {
    bbBleSetTifs();   /* active scan or initiating */
  }
  else
  {
    bbBleClrIfs();    /* passive scan */
  }
  PalBbBleRxData(pScan->pRxAdvBuf, BB_ADVB_MAX_LEN);

  /* Rx may fail; no more important statements in the !bodComplete code path */
}

/*************************************************************************************************/
/*!
 *  \brief      Cancel scan BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbMstCancelScanOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  PalBbBleCancelData();
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize for scanning master operations.
 *
 *  Update the operation table with scanning master operations routines.
 */
/*************************************************************************************************/
void BbBleScanMasterInit(void)
{
  bbBleRegisterOp(BB_BLE_OP_MST_ADV_EVENT, bbMstExecuteScanOp, bbMstCancelScanOp);

  memset(&bbScanStats, 0, sizeof(bbScanStats));
}

/*************************************************************************************************/
/*!
 *  \brief      Get advertising packet statistics.
 */
/*************************************************************************************************/
void BbBleGetScanStats(BbBleScanPktStats_t *pStats)
{
  *pStats = bbScanStats;
}
