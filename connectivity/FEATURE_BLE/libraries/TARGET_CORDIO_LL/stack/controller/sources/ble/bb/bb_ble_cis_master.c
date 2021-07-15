/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Connected isochronous stream master baseband porting implementation file.
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

#include "bb_api.h"
#include "pal_bb.h"
#include "bb_ble_int.h"
#include <string.h>

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief    Event states for scan operations. */
enum
{
  BB_EVT_STATE_IDLE,              /*!< Idle state. */
  BB_EVT_STATE_TERMINATING,       /*!< BOD terminating state. */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

extern BbBleDataPktStats_t  bbCisStats;    /*!< CIS packet statistics. */

/*************************************************************************************************/
/*!
 *  \brief      Check whether to continue next operation or not.
 *
 *  \param      pCur        Current BOD.
 *  \param      pCis        Master CIS event.
 *  \param      pNewCisCtx  TRUE if switch to a new CIS context.
 *
 *  \return     TRUE if operation completed; FALSE if it continues.
 */
/*************************************************************************************************/
static bool_t bbMstCisCheckContOp(BbOpDesc_t *pCur, BbBleMstCisEvent_t *pCis, bool_t *pNewCisCtx)
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

  /* Updated channel parameter. */
  BbBleData_t *pBle = pCur->prot.pBle;
  PalBbBleSetChannelParam(&pBle->chan);

  /* Updated due time and data parameters. */
  bbBleCb.bbParam.dueUsec = BbAdjustTime(bbBleCb.bbParam.dueUsec + offsUsec);
  PalBbBleSetDataParams(&bbBleCb.bbParam);

  /* Update the header field and start Tx. */
  pCis->contExecCback(pCur);

  /* Operation continues. */
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Post subevent operation activities.
 *
 *  \param      pCur        Current BOD.
 *  \param      pCis        Master CIS event.
 *  \param      status      Baseband status code.
 */
/*************************************************************************************************/
static void bbMstCisPostSubEvt(BbOpDesc_t *pCur, BbBleMstCisEvent_t *pCis, uint8_t status)
{
  /* Calculate the next sub-event channel index and also setup Rx resource.  */
  pCis->postSubEvtCback(pCur, status);
}

/*************************************************************************************************/
/*!
 *  \brief      Tx completion for CIS master operation.
 *
 *  \param      status      Transmission status
 *
 *  Setup for next action in the operation or complete the operation.
 */
/*************************************************************************************************/
static void bbMstCisTxCompCback(uint8_t status)
{
  BB_ISR_START();

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleMstCisEvent_t * const pCis = &pCur->prot.pBle->op.mstCis;

  pCis->txDataCback(pCur, status);

  if (bbBleCb.pRxDataBuf &&
      (status == BB_STATUS_SUCCESS))
  {
    BB_ISR_MARK(bbCisStats.rxSetupUsec);

    bbBleSetTifs();     /* TODO set only if Tx may follow in CE */
    PalBbBleRxTifsData(bbBleCb.pRxDataBuf, bbBleCb.rxDataLen);
  }
  else
  {
    /* Cancel TIFS timer if active. */
    switch (status)
    {
      case BB_STATUS_SUCCESS:
        PalBbBleCancelTifs();
        break;
      case BB_STATUS_FAILED:
      default:
        /* Free Rx data buffer before BOD end. */
        if (bbBleCb.pRxDataBuf != NULL)        /* buffer should always exist, but still check */
        {
          uint8_t *pBuf = bbBleCb.pRxDataBuf;
          bbBleCb.pRxDataBuf = NULL;
          pCis->rxDataCback(pCur, pBuf, BB_STATUS_CANCELED);
        }
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
 *  \brief      Rx completion for CIS master operation.
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
static void bbMstCisRxCompCback(uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp, uint8_t rxPhyOptions)
{
  BB_ISR_START();

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleMstCisEvent_t * const pCis = &pCur->prot.pBle->op.mstCis;
  bool_t bodComplete = FALSE;
  bool_t newCisCtx = FALSE;

  pCis->rssi = rssi;
  pCis->rxPhyOptions = rxPhyOptions;

  WSF_ASSERT(bbBleCb.pRxDataBuf);
  uint8_t *pBuf = bbBleCb.pRxDataBuf;
  bbBleCb.pRxDataBuf = NULL;

  pCis->rxDataCback(pCur, pBuf, status);

  /* If continue current operation, need to setup the channel parameter here. */
  bodComplete = bbMstCisCheckContOp(pCur, pCis, &newCisCtx);

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

  /* Skip the post subevent callback if switching to the new CIS context. */
  if (newCisCtx == FALSE)
  {
    bbMstCisPostSubEvt(pCur, pCis, status);
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
 *  \brief      Execute CIS master BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbMstExecuteCisOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  BbBleMstCisEvent_t * const pCis = &pBod->prot.pBle->op.mstCis;

  if (BbGetBodTerminateFlag())
  {
    /* Client terminated. */
    return;
  }

  WSF_ASSERT(pBle->op.mstCis.txDataCback);
  WSF_ASSERT(pBle->op.mstCis.rxDataCback);
  WSF_ASSERT(pBle->op.mstCis.execCback);
  WSF_ASSERT(pBle->op.mstCis.checkContOpCback);
  WSF_ASSERT(pBle->op.mstCis.checkContOpPostCback);

  #if(LL_ENABLE_TESTER)
    pBle->chan.txPower += pBle->chan.txPwrOffset;
  #endif
  PalBbBleSetChannelParam(&pBle->chan);

  bbBleCb.bbParam.txCback = bbMstCisTxCompCback;
  bbBleCb.bbParam.rxCback = bbMstCisRxCompCback;
  bbBleCb.bbParam.dueUsec = BbAdjustTime(pBod->dueUsec);
  pBod->dueUsec = bbBleCb.bbParam.dueUsec;
  bbBleCb.bbParam.rxTimeoutUsec = 2 * LL_MAX_TIFS_DEVIATION;

  PalBbBleSetDataParams(&bbBleCb.bbParam);

  bbBleCb.evtState = BB_EVT_STATE_IDLE;

  if (pCis->execCback)
  {
    pCis->execCback(pBod);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Cancel CIS master BOD.
 *
 *  \param      pBod    Pointer to the BOD to cancel.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbMstCancelCisOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  WSF_ASSERT(pBod && pBle);
  WSF_ASSERT(pBle->op.mstCis.rxDataCback);

  PalBbBleCancelData();

  if (bbBleCb.pRxDataBuf)
  {
    uint8_t *pBuf = bbBleCb.pRxDataBuf;
    bbBleCb.pRxDataBuf = NULL;

    /* Buffer free expected to be called during this routine. */
    pBle->op.mstCis.rxDataCback(pBod, pBuf, BB_STATUS_CANCELED);
  }

  pBle->op.mstCis.cancelCback(pBod);
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize for CIS master operations.
 *
 *  Update the operation table with CIS master operations routines.
 */
/*************************************************************************************************/
void BbBleCisMasterInit(void)
{
  bbBleRegisterOp(BB_BLE_OP_MST_CIS_EVENT, bbMstExecuteCisOp, bbMstCancelCisOp);

  memset(&bbCisStats, 0, sizeof(bbCisStats));
}
