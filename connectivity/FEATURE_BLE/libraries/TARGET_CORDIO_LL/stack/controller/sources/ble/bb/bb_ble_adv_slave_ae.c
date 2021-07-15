/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Extended advertising slave BLE baseband porting implementation file.
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

#include "bb_ble_int.h"
#include "bb_ble_sniffer_api.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include <string.h>

/**************************************************************************************************
  Macros
**************************************************************************************************/

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

BbBleAuxAdvPktStats_t bbAuxAdvStats; /*!< Auxiliary advertising packet statistics. */

/*************************************************************************************************/
/*!
 *  \brief      Setup Tx CHAIN_IND.
 *
 *  \param      pCur        Current BOD.
 *  \param      pAuxAdv     Auxiliary advertising context.
 *
 *  \return     TRUE if operation completed; FALSE if it continues.
 */
/*************************************************************************************************/
static bool_t bbSlvAdvSetupTxAuxChainInd(BbOpDesc_t *pCur, BbBleSlvAuxAdvEvent_t *pAuxAdv)
{
  if (pAuxAdv->txAuxSetupCback == NULL)
  {
    /* Operation completed. */
    return TRUE;
  }

  uint32_t auxOffsUsec = pAuxAdv->txAuxSetupCback(pCur, TRUE);

  if (auxOffsUsec == 0)
  {
    /* Operation completed. */
    return TRUE;
  }

  /* Set updated auxiliary channel. */
  PalBbBleSetChannelParam(&pCur->prot.pBle->chan);

  /* Offset may be up to 1 unit earlier than actual transmission. */
  bbBleCb.bbParam.dueUsec = BbAdjustTime(bbBleCb.bbParam.dueUsec + auxOffsUsec);

  PalBbBleSetDataParams(&bbBleCb.bbParam);

  BB_ISR_MARK(bbAuxAdvStats.rxSetupUsec);
  bbBleClrIfs();      /* CHAIN_IND does not use TIFS. */
  PalBbBleTxData(pAuxAdv->txAuxChainPdu, 2);

  /* Operation continues. */
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Tx completion for auxiliary advertising slave operation.
 *
 *  \param      status      Completion status.
 *
 *  Setup for next action in the operation or complete the operation.
 */
/*************************************************************************************************/
static void bbSlvAuxAdvTxCompCback(uint8_t status)
{
  BB_ISR_START();

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pCur->prot.pBle->op.slvAuxAdv;
  BbBleData_t * const pBle = pCur->prot.pBle;

  bool_t bodComplete = FALSE;

#if (BB_SNIFFER_ENABLED == TRUE)
  /* Save evtState to be used later in packet forwarding. */
  uint8_t evtState = bbBleCb.evtState;
  BbBleSnifferPkt_t * pPkt = NULL;
  if (bbSnifferCtx.enabled)
  {
    pPkt = bbSnifferCtx.snifferGetPktFn();
  }
#endif

  if (status != BB_STATUS_SUCCESS)
  {
    BB_INC_STAT(bbAuxAdvStats.errAdv);
    bodComplete = TRUE;
    goto Cleanup;
  }

  switch (bbBleCb.evtState)
  {
    case BB_EVT_STATE_TX_ADV_IND:
#if (BB_SNIFFER_ENABLED == TRUE)
      if (pPkt)
      {
        /* Save buffer to sniffer packet before it is overwritten. */
        memcpy(pPkt->pktType.advPkt.hdr, pAuxAdv->txAuxAdvPdu->pBuf, LL_ADV_HDR_LEN);
      }
#endif

      if (!pAuxAdv->pRxAuxReqBuf)
      {
        /* Non-connectable and non-scannable operation. */
        bbBleCb.evtState = BB_EVT_STATE_TX_CHAIN_IND;
        bodComplete = bbSlvAdvSetupTxAuxChainInd(pCur, pAuxAdv);
      }
      else
      {
        /* Scannable or connectable operation. */
        bbBleCb.evtState = BB_EVT_STATE_RX_SCAN_OR_CONN_INIT;
        BB_ISR_MARK(bbAuxAdvStats.rxSetupUsec);
        bbBleSetTifs();     /* set up for Tx SCAN_RSP */
        PalBbBleRxTifsData(pAuxAdv->pRxAuxReqBuf, BB_REQ_PDU_MAX_LEN);   /* reduce max length requirement */
      }
      BB_INC_STAT(bbAuxAdvStats.txAdv);
      break;

    case BB_EVT_STATE_TX_SCAN_OR_CONN_RSP:
      bbBleCb.evtState = BB_EVT_STATE_TX_CHAIN_IND;
      bbBleCb.bbParam.dueUsec = BbAdjustTime(pAuxAdv->auxReqStartTsUsec +
                                SchBleCalcAdvPktDurationUsec(pBle->chan.rxPhy, pAuxAdv->auxRxPhyOptions, LL_ADV_HDR_LEN + LL_SCAN_REQ_PDU_LEN) +
                                LL_BLE_TIFS_US);
      bodComplete = bbSlvAdvSetupTxAuxChainInd(pCur, pAuxAdv);
      BB_INC_STAT(bbAuxAdvStats.txRsp);
      break;

    case BB_EVT_STATE_TX_CHAIN_IND:
#if (BB_SNIFFER_ENABLED == TRUE)
      if (pPkt)
      {
        /* Save buffer to sniffer packet before it is overwritten. */
        memcpy(pPkt->pktType.advPkt.hdr, pAuxAdv->txAuxChainPdu->pBuf, LL_ADV_HDR_LEN);
      }
#endif

      /* bbBleCb.evtState = BB_EVT_STATE_TX_CHAIN_IND; */   /* Same state. */
      bodComplete = bbSlvAdvSetupTxAuxChainInd(pCur, pAuxAdv);
      BB_INC_STAT(bbAuxAdvStats.txChain);
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

    bbBleSnifferSlvAuxAdvPktHandler(pCur, pPkt);
  }
#endif

  BB_ISR_MARK(bbAuxAdvStats.txIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Rx completion for auxiliary advertising slave operation.
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
static void bbSlvAuxAdvRxCompCback(uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp, uint8_t rxPhyOptions)
{
  BB_ISR_START();
  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleData_t * const pBle = pCur->prot.pBle;
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBle->op.slvAuxAdv;

  bool_t bodComplete = FALSE;

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
    case BB_EVT_STATE_RX_SCAN_OR_CONN_INIT:
      switch (status)
      {
        case BB_STATUS_SUCCESS:
          WSF_ASSERT(pAuxAdv->rxAuxReqCback);
          WSF_ASSERT(pAuxAdv->pRxAuxReqBuf);

          pAuxAdv->auxReqStartTsUsec = timestamp;
          pAuxAdv->auxRxPhyOptions = rxPhyOptions;

#if (BB_SNIFFER_ENABLED == TRUE)
          /* Pack the rx buffer before it is overwritten. */
          if (pPkt)
          {
            memcpy(pPkt->pktType.advPkt.hdr, pAuxAdv->pRxAuxReqBuf, LL_ADV_HDR_LEN);
          }
#endif

          if (pAuxAdv->rxAuxReqCback(pCur, pAuxAdv->pRxAuxReqBuf))
          {
            BB_ISR_MARK(bbAuxAdvStats.txSetupUsec);

            bbBleClrIfs();  /* last operation in event */
            PalBbBleTxTifsData(pAuxAdv->txAuxRspPdu, 2);

            if (pAuxAdv->rxAuxReqPostCback)
            {
              pAuxAdv->rxAuxReqPostCback(pCur, pAuxAdv->pRxAuxReqBuf);
            }
          }
          else
          {
            /* Operation completed. */
            bodComplete = TRUE;
          }
          break;

        case BB_STATUS_RX_TIMEOUT:
        case BB_STATUS_CRC_FAILED:
        case BB_STATUS_FAILED:
        default:
          /* Operation completed. */
          bodComplete = TRUE;
          break;
      }

      /* Update statistics. */
      switch (status)
      {
        case BB_STATUS_SUCCESS:
          BB_INC_STAT(bbAuxAdvStats.rxReq);
          break;
        case BB_STATUS_RX_TIMEOUT:
          BB_INC_STAT(bbAuxAdvStats.rxReqTimeout);
          break;
        case BB_STATUS_CRC_FAILED:
          BB_INC_STAT(bbAuxAdvStats.rxReqCrc);
          break;
        case BB_STATUS_FAILED:
        default:
          BB_INC_STAT(bbAuxAdvStats.errAdv);
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

    bbBleSnifferSlvAuxAdvPktHandler(pCur, pPkt);
  }
#endif

  BB_ISR_MARK(bbAuxAdvStats.rxIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Execute auxiliary advertising slave BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbSlvExecuteAuxAdvOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBod->prot.pBle->op.slvAuxAdv;

  bbBleCb.bbParam.txCback       = bbSlvAuxAdvTxCompCback;
  bbBleCb.bbParam.rxCback       = bbSlvAuxAdvRxCompCback;
  bbBleCb.bbParam.rxTimeoutUsec = 2 * LL_MAX_TIFS_DEVIATION;

  bbBleCb.advChIdx = 0;
  bbBleCb.evtState = 0;

  if (BbGetBodTerminateFlag())
  {
    /* Client terminated. */
    return;
  }

  if (pAuxAdv->txAuxSetupCback)
  {
    /* AuxPtr offset return value not used for AUX_IND PDU. */
    pAuxAdv->txAuxSetupCback(pBod, FALSE);
  }

  PalBbBleSetChannelParam(&pBle->chan);

  bbBleCb.bbParam.dueUsec = BbAdjustTime(pBod->dueUsec);
  pBod->dueUsec = bbBleCb.bbParam.dueUsec;
  bbBleCb.evtState = BB_EVT_STATE_TX_ADV_IND;
  PalBbBleSetDataParams(&bbBleCb.bbParam);

  if (pAuxAdv->pRxAuxReqBuf)
  {
    /* Request expected (scannable or connectable operation). */
    bbBleSetTifs();
  }
  else
  {
    /* CHAIN_IND (i.e. >=MAFS) expected or no request expected. */
    bbBleClrIfs();
  }

  PalBbBleTxData(pAuxAdv->txAuxAdvPdu, 2);
}

/*************************************************************************************************/
/*!
 *  \brief      Execute periodic advertising slave BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbSlvExecutePerAdvOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBod->prot.pBle->op.slvPerAdv;

  bbBleCb.bbParam.txCback       = bbSlvAuxAdvTxCompCback;
  bbBleCb.bbParam.rxCback       = bbSlvAuxAdvRxCompCback;
  bbBleCb.bbParam.rxTimeoutUsec = 2 * LL_MAX_TIFS_DEVIATION;

  bbBleCb.advChIdx = 0;
  bbBleCb.evtState = 0;

  if (BbGetBodTerminateFlag())
  {
    /* Client terminated. */
    return;
  }

  if (pAuxAdv->txAuxSetupCback)
  {
    /* AuxPtr offset return value not used for AUX_IND PDU. */
    pAuxAdv->txAuxSetupCback(pBod, FALSE);
  }

  PalBbBleSetChannelParam(&pBle->chan);

  bbBleCb.bbParam.dueUsec = BbAdjustTime(pBod->dueUsec);
  pBod->dueUsec = bbBleCb.bbParam.dueUsec;
  bbBleCb.evtState = BB_EVT_STATE_TX_ADV_IND;
  PalBbBleSetDataParams(&bbBleCb.bbParam);

  if (pAuxAdv->pRxAuxReqBuf)
  {
    /* Request expected (scannable or connectable operation). */
    bbBleSetTifs();
  }
  else
  {
    /* CHAIN_IND (i.e. >=MAFS) expected or no request expected. */
    bbBleClrIfs();
  }

  PalBbBleTxData(pAuxAdv->txAuxAdvPdu, 2);
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize for advertising slave operations.
 *
 *  Update the operation table with advertising slave operations routines.
 */
/*************************************************************************************************/
void BbBleAuxAdvSlaveInit(void)
{
  bbBleRegisterOp(BB_BLE_OP_SLV_AUX_ADV_EVENT, bbSlvExecuteAuxAdvOp, NULL);
  bbBleRegisterOp(BB_BLE_OP_SLV_PER_ADV_EVENT, bbSlvExecutePerAdvOp, NULL);
  memset(&bbAuxAdvStats, 0, sizeof(bbAuxAdvStats));
}

/*************************************************************************************************/
/*!
 *  \brief      Get auxiliary advertising packet statistics.
 *
 *  \param      pStats      Auxiliary advertising statistics.
 */
/*************************************************************************************************/
void BbBleGetAuxAdvStats(BbBleAuxAdvPktStats_t *pStats)
{
  *pStats = bbAuxAdvStats;
}
