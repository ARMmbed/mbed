/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Advertising slave BLE baseband porting implementation file.
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

#include "pal_bb.h"
#include "bb_ble_int.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "ll_math.h"
#include "ll_api.h"
#include "lmgr_api_adv_slave.h"
#include <string.h>
#include "bb_ble_sniffer_api.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

BbBleAdvPktStats_t  bbAdvStats;         /*!< Advertising packet statistics. */
uint32_t            bbIsrStartTime;     /*!< ISR start time. */

/*************************************************************************************************/
/*!
 *  \brief      Setup an advertising operation.
 *
 *  \param      pBod            Pointer to the BOD to execute.
 *  \param      pAdv            Pointer to the advertising data.
 *  \param      status          Receive status.
 *  \param      firstOpInSet    First operation flag.
 *
 *  \return     TRUE if the BOD is completed, FALSE if operation continues on the next channel.
 *
 *  This function is called to setup the initial or subsequent advertising operation. Once all
 *  operations are execute in the given channel map, the BOD is considered completed.
 */
/*************************************************************************************************/
static bool_t bbSetupAdvOp(BbOpDesc_t *pBod, BbBleSlvAdvEvent_t *pAdv, uint8_t status, bool_t firstOpInSet)
{
  if (BbGetBodTerminateFlag())
  {
    /* Client terminated. */
    return TRUE;
  }

  BbBleData_t * const pBle = pBod->prot.pBle;

  /* Compute next channel. */
  do
  {
    if (bbBleCb.numChUsed == LL_NUM_CHAN_ADV)
    {
      /* BOD completed. */
      return TRUE;
    }

    /* Optimized calculation: advChIdx = advChIdx % LL_NUM_CHAN_ADV */
    bbBleCb.advChIdx = (bbBleCb.advChIdx < LL_NUM_CHAN_ADV) ?
                        bbBleCb.advChIdx : (bbBleCb.advChIdx - LL_NUM_CHAN_ADV);

    pBle->chan.chanIdx = LL_CHAN_ADV_MIN_IDX + bbBleCb.advChIdx;
    bbBleCb.numChUsed++;

    /* Selected channel in channel map; use this channel. */
  } while (!((1 << bbBleCb.advChIdx++) & pAdv->advChMap));

  if (!firstOpInSet)
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
  }

  PalBbBleSetChannelParam(&pBle->chan);

  if (firstOpInSet)
  {
    bbBleCb.bbParam.dueUsec = BbAdjustTime(pBod->dueUsec);
    pBod->dueUsec = bbBleCb.bbParam.dueUsec;
  }
  else
  {
    if (pAdv->pRxReqBuf)
    {
      /* Schedule with relative frame gap. */
      bbBleCb.bbParam.dueUsec = BbAdjustTime(PalBbGetCurrentTime() + BbGetSchSetupDelayUs());
    }
    else
    {
      /* Schedule with absolute frame gap. */
      uint32_t  advGap = SchBleCalcAdvPktDurationUsec(pBle->chan.txPhy, pBle->chan.initTxPhyOptions, pAdv->txAdvLen) +
                                                      BbGetSchSetupDelayUs();
      uint32_t auxOffsUsec = SchBleGetAlignedAuxOffsUsec(advGap);
      bbBleCb.bbParam.dueUsec = BbAdjustTime(bbBleCb.bbParam.dueUsec + auxOffsUsec);
    }
  }

  bbBleCb.evtState = BB_EVT_STATE_TX_ADV_IND;

  if (pAdv->txAdvSetupCback)
  {
    pAdv->txAdvSetupCback(pBod, BbAdjustTime(bbBleCb.bbParam.dueUsec));
  }

  PalBbBleSetDataParams(&bbBleCb.bbParam);

  if (pAdv->pRxReqBuf)
  {
    bbBleSetTifs();   /* scannable or connectable advertising */
  }
  else
  {
    bbBleClrIfs();    /* non-connectable advertising */
  }
  PalBbBleTxBufDesc_t desc = {.pBuf = pAdv->pTxAdvBuf, .len = pAdv->txAdvLen};
  PalBbBleTxData(&desc, 1);

  /* Tx may fail; no more important statements in the FALSE code path. */

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Tx completion for advertising slave operation.
 *
 *  \param      status      Completion status.
 *
 *  Setup for next action in the operation or complete the operation.
 */
/*************************************************************************************************/
static void bbSlvAdvTxCompCback(uint8_t status)
{
  BB_ISR_START();

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleSlvAdvEvent_t * const pAdv = &pCur->prot.pBle->op.slvAdv;

  bool_t bodComplete = FALSE;

#if (BB_SNIFFER_ENABLED == TRUE)
  BbBleData_t * const pBle = pCur->prot.pBle;

  /* Save evtState to be used later in packet forwarding. */
  uint8_t evtState = bbBleCb.evtState;
  BbBleSnifferPkt_t * pPkt = NULL;
  if (bbSnifferCtx.enabled)
  {
    pPkt = bbSnifferCtx.snifferGetPktFn();
  }

  /* Save channel before it is overwritten. */
  bbSnifferCtx.chanIdx = pBle->chan.chanIdx;
#endif

  if (status != BB_STATUS_SUCCESS)
  {
    BB_INC_STAT(bbAdvStats.errAdv);
    bodComplete = TRUE;
    goto Cleanup;
  }

  switch (bbBleCb.evtState++)
  {
    case BB_EVT_STATE_TX_ADV_IND:
      if (pAdv->pRxReqBuf)
      {
        BB_ISR_MARK(bbAdvStats.rxSetupUsec);

        bbBleSetTifs();     /* set up for Tx SCAN_RSP */
        PalBbBleRxTifsData(pAdv->pRxReqBuf, BB_REQ_PDU_MAX_LEN);   /* reduce max length requirement */
      }
      else
      {
        /* Operation completed. */
        bodComplete = bbSetupAdvOp(pCur, pAdv, status, FALSE);
      }

      BB_INC_STAT(bbAdvStats.txAdv);
      break;

    case BB_EVT_STATE_TX_SCAN_OR_CONN_RSP:
      /* Operation completed. */
      bodComplete = bbSetupAdvOp(pCur, pAdv, status, FALSE);
      BB_INC_STAT(bbAdvStats.txRsp);
      break;

    default:    /* unexpected state */
      WSF_ASSERT(FALSE);
      break;
  }

  /* Tx may fail; no more important statements in the !bodComplete code path */

Cleanup:

  if (bodComplete)
  {
    /* Cancel TIFS timer if active. */
    switch (status)
    {
      case BB_STATUS_SUCCESS:
        PalBbBleCancelTifs();
        break;
      default:
        break;
    }

    BbTerminateBod();
  }

#if (BB_SNIFFER_ENABLED == TRUE)
  if (pPkt)
  {
    pPkt->pktType.meta.type = BB_SNIFF_PKT_TYPE_TX;
    pPkt->pktType.meta.status = status;
    pPkt->pktType.meta.state = evtState;

    bbBleSnifferSlvAdvPktHandler(pCur, pPkt);
  }
#endif

  BB_ISR_MARK(bbAdvStats.txIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Rx completion for advertising slave operation.
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
static void bbSlvAdvRxCompCback(uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp, uint8_t rxPhyOptions)
{
  BB_ISR_START();

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleData_t * const pBle = pCur->prot.pBle;
  BbBleSlvAdvEvent_t * const pAdv = &pBle->op.slvAdv;

  bool_t bodComplete = FALSE;

#if (BB_SNIFFER_ENABLED == TRUE)
  /* Save evtState to be used later in packet forwarding. */
  uint8_t evtState = bbBleCb.evtState;
  BbBleSnifferPkt_t * pPkt = NULL;
  if (bbSnifferCtx.enabled)
  {
    pPkt = bbSnifferCtx.snifferGetPktFn();
  }

  /* Save channel before it is overwritten. */
  bbSnifferCtx.chanIdx = pBle->chan.chanIdx;
#endif

  switch (bbBleCb.evtState++)
  {
    case BB_EVT_STATE_RX_SCAN_OR_CONN_INIT:
      switch (status)
      {
        case BB_STATUS_SUCCESS:
        {
          WSF_ASSERT(pAdv->rxReqCback);
          WSF_ASSERT(pAdv->pRxReqBuf);

          pAdv->reqStartTsUsec = timestamp;

#if (BB_SNIFFER_ENABLED == TRUE)
          /* If the rx was successful, pack the rx buffer here before it is overwritten. */
          if (pPkt)
          {
            memcpy(pPkt->pktType.advPkt.hdr, pAdv->pRxReqBuf, LL_ADV_HDR_LEN);
          }
#endif

          bool_t pduAllow = BbBlePduFiltCheck(pAdv->pRxReqBuf, &pBle->pduFilt, FALSE, &pAdv->filtResults);
          if (pduAllow && pAdv->rxReqCback(pCur, pAdv->pRxReqBuf))
          {
            BB_ISR_MARK(bbAdvStats.txSetupUsec);

            bbBleClrIfs();  /* last operation in event */
            PalBbBleTxBufDesc_t desc = {.pBuf = pAdv->pTxRspBuf, .len = pAdv->txRspLen};
            PalBbBleTxTifsData(&desc, 1);
          }
          else
          {
            /* Operation completed. */
            bodComplete = bbSetupAdvOp(pCur, pAdv, status, FALSE);
          }

          if (pduAllow && pAdv->rxReqPostCback)
          {
            pAdv->rxReqPostCback(pCur, pAdv->pRxReqBuf);
          }

          break;
        }

        case BB_STATUS_RX_TIMEOUT:
        case BB_STATUS_CRC_FAILED:
        case BB_STATUS_FAILED:
        default:
          /* Operation completed. */
          bodComplete = bbSetupAdvOp(pCur, pAdv, status, FALSE);
          break;
      }

      /* Update statistics. */
      switch (status)
      {
        case BB_STATUS_SUCCESS:
          BB_INC_STAT(bbAdvStats.rxReq);
          break;
        case BB_STATUS_RX_TIMEOUT:
          BB_INC_STAT(bbAdvStats.rxReqTimeout);
          break;
        case BB_STATUS_CRC_FAILED:
          BB_INC_STAT(bbAdvStats.rxReqCrc);
          break;
        case BB_STATUS_FAILED:
        default:
          BB_INC_STAT(bbAdvStats.errAdv);
          break;
      }

      break;

    default:    /* unexpected state */
      WSF_ASSERT(FALSE);
      break;
  }

  /* Tx may fail; no more important statements in the !bodComplete code path */

  if (bodComplete)
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

    bbBleSnifferSlvAdvPktHandler(pCur, pPkt);
  }
#endif

  BB_ISR_MARK(bbAdvStats.rxIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Execute advertising slave BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbSlvExecuteAdvOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  BbBleSlvAdvEvent_t * const pAdv = &pBod->prot.pBle->op.slvAdv;

  bbBleCb.bbParam.txCback       = bbSlvAdvTxCompCback;
  bbBleCb.bbParam.rxCback       = bbSlvAdvRxCompCback;
  bbBleCb.bbParam.rxTimeoutUsec = 2 * LL_MAX_TIFS_DEVIATION;
  bbBleCb.evtState = 0;
  bbBleCb.numChUsed = 0;
  bbBleCb.advChIdx = pAdv->firstAdvChIdx;

  if (bbSetupAdvOp(pBod, pAdv, BB_STATUS_SUCCESS, TRUE))
  {
    BbSetBodTerminateFlag();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Cancel advertising slave BOD.
 *
 *  \param      pBod    Pointer to the BOD to cancel.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbSlvCancelAdvOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  PalBbBleCancelData();
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize for advertising slave operations.
 *
 *  Update the operation table with advertising slave operations routines.
 */
/*************************************************************************************************/
void BbBleAdvSlaveInit(void)
{
  bbBleRegisterOp(BB_BLE_OP_SLV_ADV_EVENT, bbSlvExecuteAdvOp, bbSlvCancelAdvOp);

  memset(&bbAdvStats, 0, sizeof(bbAdvStats));
}

/*************************************************************************************************/
/*!
 *  \brief      Get advertising packet statistics.
 */
/*************************************************************************************************/
void BbBleGetAdvStats(BbBleAdvPktStats_t *pStats)
{
  *pStats = bbAdvStats;
}
