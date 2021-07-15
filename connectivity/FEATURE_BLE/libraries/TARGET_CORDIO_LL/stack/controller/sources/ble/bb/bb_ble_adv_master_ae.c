/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Extended scanning master BLE baseband porting implementation file.
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

#include "bb_ble_int.h"
#include "sch_api.h"
#include <string.h>
#include "bb_ble_sniffer_api.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Auxiliary scan packet statistics. */
static BbBleAuxScanPktStats_t bbAuxScanStats;

/*! \brief      Periodic scan packet statistics. */
static BbBlePerScanPktStats_t bbPerScanStats;

/*! \brief      Auxiliary advertising buffer. */
static uint8_t bbAuxAdvBuf[LL_EXT_ADVB_MAX_LEN];

/*! \brief      Periodic scanning buffer. */
static uint8_t bbPerScanBuf[LL_EXT_ADVB_MAX_LEN];

/*************************************************************************************************/
/*!
 *  \brief      Tx completion for auxiliary scanning master operation.
 *
 *  \param      status      Completion status.
 *  \param      pBod        Pointer to the BOD
 *
 *  Setup for next action in the operation or complete the operation.
 *
 *  \return     TRUE if BOD is complete, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t BbMstAuxScanTxCompHandler(BbOpDesc_t *pBod, uint8_t status)
{
  WSF_ASSERT(pBod);

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
    case BB_EVT_STATE_TX_SCAN_OR_CONN_INIT:
    {
      switch (status)
      {
        case BB_STATUS_SUCCESS:
        {
          BB_ISR_MARK(bbAuxScanStats.rxSetupUsec);

          bbBleClrIfs();      /* Prepare for SCAN_OR_CONN_RSP */
          PalBbBleRxTifsData(bbAuxAdvBuf, sizeof(bbAuxAdvBuf));

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
          BB_INC_STAT(bbAuxScanStats.txReq);
          break;
        default:
          BB_INC_STAT(bbAuxScanStats.errScan);
          break;
      }

      break;
    }

    default:          /* unexpected state */
      WSF_ASSERT(FALSE);
      break;
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

    bbBleSnifferMstAuxScanPktHandler(pBod, pPkt);
  }
#else
  (void)pBod;
#endif

  return bodComplete;
}

/*************************************************************************************************/
/*!
 *  \brief      Tx completion for auxiliary scanning master operation.
 *
 *  \param      status      Completion status.
 *
 *  Setup for next action in the operation or complete the operation.
 */
/*************************************************************************************************/
static void bbMstAuxScanTxCompCback(uint8_t status)
{
  BB_ISR_START();

  (void)BbMstAuxScanTxCompHandler(BbGetCurrentBod(), status);

  BB_ISR_MARK(bbAuxScanStats.txIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Rx completion for auxiliary scanning master operation.
 *
 *  \param      pCur            Pointer to the BOD
 *  \param      status          Reception status.
 *  \param      rssi            RSSI value.
 *  \param      crc             CRC value.
 *  \param      timestamp       Start of packet timestamp in microseconds.
 *  \param      rxPhyOptions    Rx PHY options.
 *
 *  Setup for next action in the operation or complete the operation.
 *
 *  \return     TRUE if BOD is complete, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t BbMstAuxScanRxCompHandler(BbOpDesc_t * const pCur, uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp, uint8_t rxPhyOptions)
{
  WSF_ASSERT(pCur);

  BbBleData_t * const pBle = pCur->prot.pBle;
  BbBleMstAuxAdvEvent_t * const pAuxScan = &pBle->op.mstAuxAdv;

  bool_t bodComplete = FALSE;
  bool_t bodCont = FALSE;

#if (BB_SNIFFER_ENABLED == TRUE)
  /* Save evtState to be used later in packet forwarding. */
  uint8_t evtState = bbBleCb.evtState;
  BbBleSnifferPkt_t * pPkt = NULL;
  if (bbSnifferCtx.enabled)
  {
    pPkt = bbSnifferCtx.snifferGetPktFn();
  }
#endif

  switch (bbBleCb.evtState)
  {
    case BB_EVT_STATE_RX_ADV_IND:
    {
      switch (status)
      {
        case BB_STATUS_SUCCESS:
        {
          WSF_ASSERT(pAuxScan->rxAuxAdvCback);

          pAuxScan->auxAdvRssi = rssi;
          pAuxScan->auxAdvCrc = crc;
          pAuxScan->auxStartTsUsec = timestamp;
          pAuxScan->auxRxPhyOptions = rxPhyOptions;

#if (BB_SNIFFER_ENABLED == TRUE)
          /* Pack the rx buffer before it is overwritten. */
          if (pPkt)
          {
            memcpy(pPkt->pktType.advPkt.hdr, bbAuxAdvBuf, LL_ADV_HDR_LEN);
          }
#endif

          uint32_t auxOffsetUsec;
          bool_t txScanReq = pAuxScan->rxAuxAdvCback(pCur, bbAuxAdvBuf);
          if ((txScanReq) && (pAuxScan->pTxAuxReqBuf))
          {
            /* Tx response PDU. */

            bbBleCb.evtState = BB_EVT_STATE_TX_SCAN_OR_CONN_INIT;

            BB_ISR_MARK(bbAuxScanStats.txSetupUsec);

            PalBbBleTxBufDesc_t desc = {.pBuf = pAuxScan->pTxAuxReqBuf, .len = pAuxScan->txAuxReqLen};

            bbBleSetTifs();
            PalBbBleTxTifsData(&desc, 1);
          }
          else
          {
            PalBbBleCancelTifs();
          }

          if (pAuxScan->rxAuxAdvPostCback)
          {
            pAuxScan->rxAuxAdvPostCback(pCur, bbAuxAdvBuf);
          }

          if (!txScanReq)
          {
            if ((pAuxScan->rxAuxChainCback) &&
                ((auxOffsetUsec = pAuxScan->rxAuxChainCback(pCur, bbAuxAdvBuf)) > 0))
            {
              /* Rx chain indication PDU. */

              bbBleCb.evtState = BB_EVT_STATE_RX_CHAIN_IND;

              /* Cancel Tifs operation is needed for passive scan and non connectable/scannable adv with chain. */
              PalBbBleCancelTifs();

              PalBbBleSetChannelParam(&pBle->chan);
              bbBleCb.bbParam.dueUsec = BbAdjustTime(timestamp + auxOffsetUsec);
              PalBbBleSetDataParams(&bbBleCb.bbParam);

              BB_ISR_MARK(bbAuxScanStats.rxSetupUsec);

              bbBleClrIfs();        /* CHAIN_IND does not use TIFS. */
              PalBbBleRxData(bbAuxAdvBuf, sizeof(bbAuxAdvBuf));

              WSF_ASSERT(pAuxScan->rxAuxChainPostCback);
              if (pAuxScan->rxAuxChainPostCback(pCur, bbAuxAdvBuf) == FALSE)
              {
                bodCont = TRUE;
              }
            }
            else
            {
              if (pAuxScan->rxAuxChainPostCback)
              {
                pAuxScan->rxAuxChainPostCback(pCur, bbAuxAdvBuf);
              }
              bodCont = TRUE;
            }
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
          BB_INC_STAT(bbAuxScanStats.rxAdv);
          break;
        case BB_STATUS_RX_TIMEOUT:
          BB_INC_STAT(bbAuxScanStats.rxAdvTimeout);
          break;
        case BB_STATUS_CRC_FAILED:
          BB_INC_STAT(bbAuxScanStats.rxAdvCrc);
          break;
        case BB_STATUS_FAILED:
        default:
          BB_INC_STAT(bbAuxScanStats.errScan);
          break;
      }

      break;
    }

    case BB_EVT_STATE_RX_SCAN_OR_CONN_RSP:
    {
      bbBleCb.evtState = BB_EVT_STATE_RX_CHAIN_IND;

      WSF_ASSERT(pAuxScan->rxAuxRspCback);

      switch (status)
      {
        case BB_STATUS_SUCCESS:
          pAuxScan->auxAdvRssi = rssi;
          pAuxScan->auxAdvCrc = crc;
          pAuxScan->auxStartTsUsec = timestamp;
          pAuxScan->auxRxPhyOptions = rxPhyOptions;

#if (BB_SNIFFER_ENABLED == TRUE)
          /* Pack the rx buffer before it is overwritten. */
          if (pPkt)
          {
            memcpy(pPkt->pktType.advPkt.hdr, bbAuxAdvBuf, LL_ADV_HDR_LEN);
          }
#endif

          uint32_t auxOffsetUsec;

          pAuxScan->rxAuxRspCback(pCur, bbAuxAdvBuf);
          if ((pAuxScan->rxAuxChainCback) &&
              ((auxOffsetUsec = pAuxScan->rxAuxChainCback(pCur, bbAuxAdvBuf)) > 0))
          {
            PalBbBleSetChannelParam(&pBle->chan);
            bbBleCb.bbParam.dueUsec = BbAdjustTime(timestamp + auxOffsetUsec);
            PalBbBleSetDataParams(&bbBleCb.bbParam);

            BB_ISR_MARK(bbAuxScanStats.rxSetupUsec);

            bbBleClrIfs();        /* CHAIN_IND does not use TIFS. */
            PalBbBleRxData(bbAuxAdvBuf, sizeof(bbAuxAdvBuf));

            WSF_ASSERT(pAuxScan->rxAuxChainPostCback);
            if (pAuxScan->rxAuxChainPostCback(pCur, bbAuxAdvBuf) == FALSE)
            {
              bodCont = TRUE;
            }
          }
          else
          {
            if (pAuxScan->rxAuxChainPostCback)
            {
              /* Only apply to scan response. */
              pAuxScan->rxAuxChainPostCback(pCur, bbAuxAdvBuf);
            }
            bodCont = TRUE;
          }
          break;

        case BB_STATUS_RX_TIMEOUT:
        case BB_STATUS_CRC_FAILED:
          pAuxScan->rxAuxRspCback(pCur, NULL);
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
          BB_INC_STAT(bbAuxScanStats.rxRsp);
          break;
        case BB_STATUS_RX_TIMEOUT:
          BB_INC_STAT(bbAuxScanStats.rxRspTimeout);
          break;
        case BB_STATUS_CRC_FAILED:
          BB_INC_STAT(bbAuxScanStats.rxRspCrc);
          break;
        case BB_STATUS_FAILED:
        default:
          BB_INC_STAT(bbAuxScanStats.errScan);
          break;
      }

      break;
    }

    case BB_EVT_STATE_RX_CHAIN_IND:
    {
      /* Same state. */
      /* bbBleCb.evtState = BB_EVT_STATE_RX_CHAIN_IND; */

      WSF_ASSERT(pAuxScan->rxAuxChainCback);

      switch (status)
      {
        case BB_STATUS_SUCCESS:
        {
#if (BB_SNIFFER_ENABLED == TRUE)
          /* Pack the rx buffer before it is overwritten. */
          if (pPkt)
          {
            memcpy(pPkt->pktType.advPkt.hdr, bbAuxAdvBuf, LL_ADV_HDR_LEN);
          }
#endif

          uint32_t auxOffsetUsec;
          if ((auxOffsetUsec = pAuxScan->rxAuxChainCback(pCur, bbAuxAdvBuf)) > 0)
          {
            PalBbBleSetChannelParam(&pBle->chan);
            bbBleCb.bbParam.dueUsec = BbAdjustTime(timestamp + auxOffsetUsec);
            PalBbBleSetDataParams(&bbBleCb.bbParam);

            BB_ISR_MARK(bbAuxScanStats.rxSetupUsec);

            bbBleClrIfs();        /* CHAIN_IND does not use TIFS. */
            PalBbBleRxData(bbAuxAdvBuf, sizeof(bbAuxAdvBuf));

            WSF_ASSERT(pAuxScan->rxAuxChainPostCback);
            if (pAuxScan->rxAuxChainPostCback(pCur, bbAuxAdvBuf) == FALSE)
            {
              bodCont = TRUE;
            }
          }
          else
          {
            if (pAuxScan->rxAuxChainPostCback)
            {
              pAuxScan->rxAuxChainPostCback(pCur, bbAuxAdvBuf);
            }
            bodCont = TRUE;
          }
          break;
        }
        case BB_STATUS_RX_TIMEOUT:
        case BB_STATUS_CRC_FAILED:
          pAuxScan->rxAuxChainCback(pCur, NULL);
          if (pAuxScan->rxAuxChainPostCback)
          {
            pAuxScan->rxAuxChainPostCback(pCur, NULL);
          }
          bodCont = TRUE;
          break;

        case BB_STATUS_FAILED:
        default:
          pAuxScan->rxAuxChainCback(pCur, NULL);
          if (pAuxScan->rxAuxChainPostCback)
          {
            pAuxScan->rxAuxChainPostCback(pCur, NULL);
          }
          bodComplete = TRUE;
          break;
      }

      /* Update statistics. */
      switch (status)
      {
        case BB_STATUS_SUCCESS:
          BB_INC_STAT(bbAuxScanStats.rxChain);
          break;
        case BB_STATUS_RX_TIMEOUT:
          BB_INC_STAT(bbAuxScanStats.rxChainTimeout);
          break;
        case BB_STATUS_CRC_FAILED:
          BB_INC_STAT(bbAuxScanStats.rxChainCrc);
          break;
        case BB_STATUS_FAILED:
        default:
          BB_INC_STAT(bbAuxScanStats.errScan);
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

    bodComplete = TRUE;
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

    bbBleSnifferMstAuxScanPktHandler(pCur, pPkt);
  }
#endif

  return bodComplete;
}

/*************************************************************************************************/
/*!
 *  \brief      Rx completion for auxiliary scanning master operation.
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
static void bbMstAuxScanRxCompCback(uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp, uint8_t rxPhyOptions)
{
  BB_ISR_START();

  (void)BbMstAuxScanRxCompHandler(BbGetCurrentBod(), status, rssi, crc, timestamp, rxPhyOptions);

  BB_ISR_MARK(bbAuxScanStats.rxIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Execute auxiliary scanning master BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbMstExecuteAuxScanOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  BbBleMstAuxAdvEvent_t * const pAuxScan = &pBod->prot.pBle->op.mstAuxAdv;

  PalBbBleSetChannelParam(&pBle->chan);

  bbBleCb.bbParam.txCback       = bbMstAuxScanTxCompCback;
  bbBleCb.bbParam.rxCback       = bbMstAuxScanRxCompCback;
  bbBleCb.bbParam.rxTimeoutUsec = pAuxScan->rxSyncDelayUsec;
  bbBleCb.bbParam.dueUsec       = BbAdjustTime(pBod->dueUsec);
  pBod->dueUsec = bbBleCb.bbParam.dueUsec;
  PalBbBleSetDataParams(&bbBleCb.bbParam);

  bbBleCb.evtState = 0;

  if (pAuxScan->pTxAuxReqBuf)
  {
    bbBleSetTifs();   /* active scan or initiating */
  }
  else
  {
    bbBleClrIfs();    /* passive scan */
  }
  PalBbBleRxData(bbAuxAdvBuf, sizeof(bbAuxAdvBuf));
}

/*************************************************************************************************/
/*!
 *  \brief      Rx completion for periodic scanning master operation.
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
static void bbMstPerScanRxCompCback(uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp, uint8_t rxPhyOptions)
{
  BB_ISR_START();

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleData_t * const pBle = pCur->prot.pBle;
  BbBleMstPerScanEvent_t * const pPerScan = &pBle->op.mstPerScan;

  bool_t bodComplete = FALSE;
  bool_t bodCont = FALSE;

#if (BB_SNIFFER_ENABLED == TRUE)
  /* Save evtState to be used later in packet forwarding. */
  uint8_t evtState = bbBleCb.evtState;
  BbBleSnifferPkt_t * pPkt = NULL;
  if (bbSnifferCtx.enabled)
  {
    pPkt = bbSnifferCtx.snifferGetPktFn();
  }
#endif

  switch (bbBleCb.evtState)
  {
    case BB_EVT_STATE_RX_ADV_IND:
    {
      switch (status)
      {
        case BB_STATUS_SUCCESS:
        {
          WSF_ASSERT(pPerScan->rxPerAdvCback);
          WSF_ASSERT(pPerScan->rxPerAdvPostCback);

          bbBleCb.evtState = BB_EVT_STATE_RX_CHAIN_IND;
          pPerScan->perAdvRssi = rssi;
          pPerScan->perAdvCrc = crc;
          pPerScan->perStartTsUsec = timestamp;
          pPerScan->perIsFirstTs = TRUE;
          pPerScan->perRxPhyOptions = rxPhyOptions;

#if (BB_SNIFFER_ENABLED == TRUE)
          /* Pack the rx buffer before it is overwritten. */
          if (pPkt)
          {
            memcpy(pPkt->pktType.advPkt.hdr, bbPerScanBuf, LL_ADV_HDR_LEN);
          }
#endif

          uint32_t auxOffsetUsec;

          if ((auxOffsetUsec = pPerScan->rxPerAdvCback(pCur, bbPerScanBuf, status)) > 0)
          {
            PalBbBleSetChannelParam(&pBle->chan);
            bbBleCb.bbParam.dueUsec = BbAdjustTime(timestamp + auxOffsetUsec);
            PalBbBleSetDataParams(&bbBleCb.bbParam);
            BB_ISR_MARK(bbPerScanStats.rxSetupUsec);

            bbBleClrIfs();        /* SYNC_IND does not use TIFS. */
            PalBbBleRxData(bbPerScanBuf, sizeof(bbPerScanBuf));

            if (pPerScan->rxPerAdvPostCback(pCur, bbAuxAdvBuf) == FALSE)
            {
              bodCont = TRUE;
            }
          }
          else
          {
            pPerScan->rxPerAdvPostCback(pCur, bbPerScanBuf);
            bodCont = TRUE;
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
          BB_INC_STAT(bbPerScanStats.rxAdv);
          break;
        case BB_STATUS_RX_TIMEOUT:
          BB_INC_STAT(bbPerScanStats.rxAdvTimeout);
          break;
        case BB_STATUS_CRC_FAILED:
          BB_INC_STAT(bbPerScanStats.rxAdvCrc);
          break;
        case BB_STATUS_FAILED:
        default:
          BB_INC_STAT(bbPerScanStats.errScan);
          break;
      }

      break;
    }

    case BB_EVT_STATE_RX_CHAIN_IND:
    {
      switch (status)
      {
        case BB_STATUS_SUCCESS:
        {
          WSF_ASSERT(pPerScan->rxPerAdvCback);
          WSF_ASSERT(pPerScan->rxPerAdvPostCback);

          uint32_t auxOffsetUsec;
          pPerScan->perIsFirstTs = FALSE;
          pPerScan->perAdvRssi = rssi;
          pPerScan->perRxPhyOptions = rxPhyOptions;

#if (BB_SNIFFER_ENABLED == TRUE)
          /* Pack the rx buffer before it is overwritten. */
          if (pPkt)
          {
            memcpy(pPkt->pktType.advPkt.hdr, bbPerScanBuf, LL_ADV_HDR_LEN);
          }
#endif

          if ((auxOffsetUsec = pPerScan->rxPerAdvCback(pCur, bbPerScanBuf, status)) > 0)
          {
            /* Continue BOD with the CHAIN_IND and adjust the channel parameters. */
            PalBbBleSetChannelParam(&pBle->chan);
            bbBleCb.bbParam.dueUsec = BbAdjustTime(timestamp + auxOffsetUsec);
            PalBbBleSetDataParams(&bbBleCb.bbParam);

            BB_ISR_MARK(bbPerScanStats.rxSetupUsec);

            bbBleClrIfs();        /* SYNC_IND does not use TIFS. */
            PalBbBleRxData(bbPerScanBuf, sizeof(bbPerScanBuf));

            if (pPerScan->rxPerAdvPostCback(pCur, bbAuxAdvBuf) == FALSE)
            {
              bodCont = TRUE;
            }
          }
          else
          {
            pPerScan->rxPerAdvPostCback(pCur, bbPerScanBuf);
            bodCont = TRUE;
          }
          break;
        }
        case BB_STATUS_RX_TIMEOUT:
        case BB_STATUS_CRC_FAILED:
          pPerScan->rxPerAdvCback(pCur, NULL, status);
          pPerScan->rxPerAdvPostCback(pCur, NULL);
          bodCont = TRUE;
          break;

        case BB_STATUS_FAILED:
        default:
          pPerScan->rxPerAdvCback(pCur, NULL, status);
          pPerScan->rxPerAdvPostCback(pCur, NULL);
          bodComplete = TRUE;
          break;
      }

      /* Update statistics. */
      switch (status)
      {
        case BB_STATUS_SUCCESS:
          BB_INC_STAT(bbPerScanStats.rxChain);
          break;
        case BB_STATUS_RX_TIMEOUT:
          BB_INC_STAT(bbPerScanStats.rxChainTimeout);
          break;
        case BB_STATUS_CRC_FAILED:
          BB_INC_STAT(bbPerScanStats.rxChainCrc);
          break;
        case BB_STATUS_FAILED:
        default:
          BB_INC_STAT(bbPerScanStats.errScan);
          break;
      }

      /* Same state. */
      /* bbBleCb.evtState = BB_EVT_STATE_RX_CHAIN_IND; */
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

    bodComplete = TRUE;
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

    bbBleSnifferMstPerScanPktHandler(pCur, pPkt);
  }
#endif

  BB_ISR_MARK(bbPerScanStats.rxIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Execute periodic scanning master BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbMstExecutePerScanOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  BbBleMstPerScanEvent_t * const pPerScan = &pBod->prot.pBle->op.mstPerScan;

  PalBbBleSetChannelParam(&pBle->chan);

  bbBleCb.bbParam.rxCback       = bbMstPerScanRxCompCback;
  bbBleCb.bbParam.rxTimeoutUsec = pPerScan->rxSyncDelayUsec;
  bbBleCb.bbParam.dueUsec       = BbAdjustTime(pBod->dueUsec);
  pBod->dueUsec = bbBleCb.bbParam.dueUsec;
  PalBbBleSetDataParams(&bbBleCb.bbParam);

  bbBleCb.evtState = 0;

  bbBleClrIfs();    /* passive scan */

  PalBbBleRxData(bbPerScanBuf, sizeof(bbPerScanBuf));
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize for scanning master operations.
 *
 *  Update the operation table with scanning master operations routines.
 */
/*************************************************************************************************/
void BbBleAuxScanMasterInit(void)
{
  bbBleRegisterOp(BB_BLE_OP_MST_AUX_ADV_EVENT, bbMstExecuteAuxScanOp, NULL);

  memset(&bbAuxScanStats, 0, sizeof(bbAuxScanStats));
}

/*************************************************************************************************/
/*!
 *  \brief      Get advertising packet statistics.
 *
 *  \param      pStats      Auxiliary scan statistics.
 */
/*************************************************************************************************/
void BbBleGetAuxScanStats(BbBleAuxScanPktStats_t *pStats)
{
  *pStats = bbAuxScanStats;
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize for periodic scanning master operations.
 *
 *  Update the operation table with periodic scanning master operations routines.
 */
/*************************************************************************************************/
void BbBlePerScanMasterInit(void)
{
  bbBleRegisterOp(BB_BLE_OP_MST_PER_SCAN_EVENT, bbMstExecutePerScanOp, NULL);

  memset(&bbPerScanStats, 0, sizeof(bbPerScanStats));
}

/*************************************************************************************************/
/*!
 *  \brief      Get periodic scanning packet statistics.
 *
 *  \param      pStats      periodic scan statistics.
 */
/*************************************************************************************************/
void BbBleGetPerScanStats(BbBlePerScanPktStats_t *pStats)
{
  *pStats = bbPerScanStats;
}

/*************************************************************************************************/
/*!
 *  \brief      Execute auxiliary scanning master BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
void BbMstExecuteLinkedAuxScanOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  BbBleMstAuxAdvEvent_t * const pAuxScan = &pBod->prot.pBle->op.mstAuxAdv;

  PalBbBleSetChannelParam(&pBle->chan);

  bbBleCb.bbParam.rxTimeoutUsec = pAuxScan->rxSyncDelayUsec;
  bbBleCb.bbParam.dueUsec       = BbAdjustTime(pBod->dueUsec);
  pBod->dueUsec = bbBleCb.bbParam.dueUsec;
  PalBbBleSetDataParams(&bbBleCb.bbParam);

  bbBleCb.evtState = 0;

  if (pAuxScan->pTxAuxReqBuf)
  {
    bbBleSetTifs();   /* active scan or initiating */
  }
  else
  {
    bbBleClrIfs();    /* passive scan */
  }
  PalBbBleRxData(bbAuxAdvBuf, sizeof(bbAuxAdvBuf));
}
