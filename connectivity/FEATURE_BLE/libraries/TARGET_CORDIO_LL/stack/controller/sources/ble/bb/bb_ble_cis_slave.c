/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Connected isochronous stream slave baseband porting implementation file.
 *
 *  Copyright (c) 2013-2019 ARM Ltd. All Rights Reserved.
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

#include "bb_api.h"
#include "pal_bb.h"
#include "bb_ble_int.h"
#include "wsf_trace.h"
#include <string.h>

/*! \brief    Event states for scan operations. */
enum
{
  BB_EVT_STATE_IDLE,              /*!< Idle state. */
  BB_EVT_STATE_TERMINATING,       /*!< BOD terminating state. */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

extern BbBleDataPktStats_t  bbCisStats;    /*!< Connection packet statistics. */

/*************************************************************************************************/
/*!
 *  \brief      Post subevent operation activities.
 *
 *  \param      pCur        Current BOD.
 *  \param      pCis        Slave CIS event.
 *  \param      status      status.
 */
/*************************************************************************************************/
static void bbSlvCisPostSubEvt(BbOpDesc_t *pCur, BbBleSlvCisEvent_t *pCis, uint8_t status)
{
  /* Calculate the next sub-event channel index. */
  pCis->postSubEvtCback(pCur, status);
}

/*************************************************************************************************/
/*!
 *  \brief      Check whether to continue next operation or not.
 *
 *  \param      pCur        Current BOD.
 *  \param      pCis        Slave CIS event.
 *  \param      pNewCisCtx  TRUE if switch to a new CIS context.
 *
 *  \return     TRUE if operation completed; FALSE if it continues.
 */
/*************************************************************************************************/
static bool_t bbSlvCisCheckNextOp(BbOpDesc_t *pCur, BbBleSlvCisEvent_t *pCis, bool_t *pNewCisCtx)
{
  if (BbGetBodTerminateFlag())
  {
    return TRUE;
  }

  /* Update new channel index and due time. */
  uint32_t offsUsec = pCis->checkContOpCback(pCur, pNewCisCtx);

  if (offsUsec == 0)
  {
    /* Operation completed. */
    return TRUE;
  }

  /* Cancel TIFS timer. */
  PalBbBleCancelTifs();

  (void)pCis->checkContOpPostCback(pCur, pNewCisCtx);

  /* Update channel parameter. */
  BbBleData_t *pBle = pCur->prot.pBle;
  PalBbBleSetChannelParam(&pBle->chan);

  /* Note setting radio requires setting up the channel and due time, need to be done first. */
  pCis->rxTsUsec = BbAdjustTime(pCis->rxTsUsec + offsUsec);
  pCur->dueUsec = pCis->rxTsUsec;

  bbBleCb.bbParam.dueUsec = pCur->dueUsec;
  bbBleCb.bbParam.rxTimeoutUsec = pCis->rxSyncDelayUsec;

  PalBbBleSetDataParams(&bbBleCb.bbParam);

  /* Update the header field and start Rx. */
  pCis->contExecCback(pCur);

  /* Operation continues. */
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Tx completion for CIS slave operation.
 *
 *  \param      status      Transmission status
 *
 *  Setup for next action in the operation or complete the operation.
 */
/*************************************************************************************************/
static void bbSlvCisTxCompCback(uint8_t status)
{
  BB_ISR_START();

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleSlvCisEvent_t * const pCis = &pCur->prot.pBle->op.slvCis;

  pCis->txDataCback(pCur, status);

  bool_t bodComplete = FALSE;
  bool_t newCisCtx = FALSE;

  bodComplete = bbSlvCisCheckNextOp(pCur, pCis, &newCisCtx);

  if (status == BB_STATUS_SUCCESS)
  {
    if (BbGetBodTerminateFlag() || bodComplete)
    {
      /* Cancel TIFS timer if active. */
      PalBbBleCancelTifs();

      /* Tx completion is end of BOD. */
      if (bbBleCb.evtState == BB_EVT_STATE_IDLE)
      {
        /* Avoid double terminating BOD */
        BbTerminateBod();
        bbBleCb.evtState = BB_EVT_STATE_TERMINATING;
      }
    }
  }
  else if (status == BB_STATUS_FAILED)
  {
    if (bodComplete)
    {
      if (bbBleCb.pRxDataBuf != NULL)
      {
        uint8_t *pBuf = bbBleCb.pRxDataBuf;
        bbBleCb.pRxDataBuf = NULL;
        pCis->rxDataCback(pCur, pBuf, BB_STATUS_CANCELED);
      }

      /* Cancel TIFS timer if active. */
      PalBbBleCancelTifs();

      if (bbBleCb.evtState == BB_EVT_STATE_IDLE)
      {
        /* Avoid double terminating BOD */
        BbTerminateBod();
        bbBleCb.evtState = BB_EVT_STATE_TERMINATING;
      }
    }
  }
  else
  {
    if (bbBleCb.pRxDataBuf != NULL)
    {
      uint8_t *pBuf = bbBleCb.pRxDataBuf;
      bbBleCb.pRxDataBuf = NULL;
      pCis->rxDataCback(pCur, pBuf, BB_STATUS_CANCELED);
    }

    if (bbBleCb.evtState == BB_EVT_STATE_IDLE)
    {
      /* Avoid double terminating BOD */
      BbTerminateBod();
      bbBleCb.evtState = BB_EVT_STATE_TERMINATING;
    }
  }

  /* Skip the post subevent callback if switching to the new CIS context. */
  if (newCisCtx == FALSE)
  {
    bbSlvCisPostSubEvt(pCur, pCis, status);
  }

  /* Update statistics. */
  switch (status)
  {
    case BB_STATUS_SUCCESS:
      BB_INC_STAT(bbCisStats.txData);
      break;
    case BB_STATUS_FAILED:
    default:
      BB_INC_STAT(bbCisStats.errData);
      break;
  }

  BB_ISR_MARK(bbCisStats.txIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Rx completion for CIS slave operation.
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
static void bbSlvCisRxCompCback(uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp, uint8_t rxPhyOptions)
{
  BB_ISR_START();

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleSlvCisEvent_t * const pCis = &pCur->prot.pBle->op.slvCis;

  pCis->rssi = rssi;
  pCis->rxPhyOptions = rxPhyOptions;

  if (pCis->isFirstTs == TRUE)
  {
    /* Update startTs for the successful Rx, otherwise use the due time. */
    if (status == BB_STATUS_SUCCESS)
    {
      pCis->startTsUsec = timestamp;
    }
    else
    {
      pCis->startTsUsec = pCur->dueUsec;
    }
    pCis->isFirstTs = FALSE;
    pCis->rxTsUsec = pCis->startTsUsec;
  }
  else
  {
    /* Update rxTs for the successful Rx, otherwise use the due time. */
    if (status == BB_STATUS_SUCCESS)
    {
      pCis->rxTsUsec = timestamp;
    }
  }

  WSF_ASSERT(bbBleCb.pRxDataBuf);

  uint8_t *pBuf = bbBleCb.pRxDataBuf;
  bbBleCb.pRxDataBuf = NULL;

  /* Set Tx buffer or BOD cancel expected to be called during this routine. */
  pCis->rxDataCback(pCur, pBuf, status);

  bool_t bodComplete = FALSE;
  bool_t newCisCtx = FALSE;

  switch (status)
  {
    case BB_STATUS_RX_TIMEOUT:
    case BB_STATUS_FAILED:
      bodComplete = bbSlvCisCheckNextOp(pCur, pCis, &newCisCtx);

      /* Skip the post subevent callback if switching to the new CIS context. */
      if (newCisCtx == FALSE)
      {
        bbSlvCisPostSubEvt(pCur, pCis, status);
      }
      break;
    default:
      break;
  }

  if (BbGetBodTerminateFlag() || bodComplete)
  {
    WSF_ASSERT(!bbBleCb.pRxDataBuf);

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

    if (bbBleCb.evtState == BB_EVT_STATE_IDLE)
    {
      /* Avoid double terminating BOD */
      BbTerminateBod();
      bbBleCb.evtState = BB_EVT_STATE_TERMINATING;
    }
  }

  /* Update statistics. */
  switch (status)
  {
    case BB_STATUS_SUCCESS:
      BB_INC_STAT(bbCisStats.rxData);
      break;
    case BB_STATUS_RX_TIMEOUT:
      BB_INC_STAT(bbCisStats.rxDataTimeout);
      break;
    case BB_STATUS_CRC_FAILED:
      BB_INC_STAT(bbCisStats.rxDataCrc);
      break;
    case BB_STATUS_FAILED:
    default:
      BB_INC_STAT(bbCisStats.errData);
      break;
  }

  BB_ISR_MARK(bbCisStats.rxIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Execute CIS slave BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbSlvExecuteCisOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  BbBleSlvCisEvent_t * const pCis = &pBod->prot.pBle->op.slvCis;

  WSF_ASSERT(pCis->txDataCback);
  WSF_ASSERT(pCis->rxDataCback);
  WSF_ASSERT(pCis->execCback);
  WSF_ASSERT(pCis->checkContOpCback);
  WSF_ASSERT(pCis->checkContOpPostCback);

  pCis->isFirstTs = TRUE;
  pCis->rxTsUsec = pBod->dueUsec;

  #if(LL_ENABLE_TESTER)
    pBle->chan.txPower += pBle->chan.txPwrOffset;
  #endif
  PalBbBleSetChannelParam(&pBle->chan);

  bbBleCb.bbParam.txCback = bbSlvCisTxCompCback;
  bbBleCb.bbParam.rxCback = bbSlvCisRxCompCback;
  bbBleCb.bbParam.dueUsec = BbAdjustTime(pCis->rxTsUsec);
  pCis->rxTsUsec = bbBleCb.bbParam.dueUsec;
  bbBleCb.bbParam.rxTimeoutUsec = pCis->rxSyncDelayUsec;

  PalBbBleSetDataParams(&bbBleCb.bbParam);

  bbBleCb.evtState = BB_EVT_STATE_IDLE;

  pBle->op.slvCis.execCback(pBod);
}

/*************************************************************************************************/
/*!
 *  \brief      Cancel CIS slave BOD.
 *
 *  \param      pBod    Pointer to the BOD to cancel.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbSlvCancelCisOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  WSF_ASSERT(pBod && pBle);
  WSF_ASSERT(pBle->op.slvCis.rxDataCback);

  PalBbBleCancelData();

  if (bbBleCb.pRxDataBuf)
  {
    uint8_t *pBuf = bbBleCb.pRxDataBuf;
    bbBleCb.pRxDataBuf = NULL;

    /* Buffer free expected to be called during this routine. */
    pBle->op.slvCis.rxDataCback(pBod, pBuf, BB_STATUS_CANCELED);
  }

  pBle->op.slvCis.cancelCback(pBod);
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize for connected isochronous stream slave operations.
 *
 *  Update the operation table with onnected isochronous stream slave operations routines.
 */
/*************************************************************************************************/
void BbBleCisSlaveInit(void)
{
  bbBleRegisterOp(BB_BLE_OP_SLV_CIS_EVENT, bbSlvExecuteCisOp, bbSlvCancelCisOp);

  memset(&bbCisStats, 0, sizeof(bbCisStats));
}
