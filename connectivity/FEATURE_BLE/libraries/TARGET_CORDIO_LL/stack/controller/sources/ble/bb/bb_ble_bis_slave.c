/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      BIG slave baseband porting implementation file.
 *
 *  Copyright (c) 2019 ARM Ltd. All Rights Reserved.
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
#include "bb_ble_api.h"
#include "pal_bb.h"
#include "bb_ble_int.h"
#include <string.h>

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

BbBleDataPktStats_t bbBisStats;   /*!< BIS packet statistics. */

/*************************************************************************************************/
/*!
 *  \brief      Tx completion for BIS slave operation.
 *
 *  \param      status      Completion status.
 *
 *  Setup for next action in the operation or complete the operation.
 */
/*************************************************************************************************/
static void bbSlvBisTxCompCback(uint8_t status)
{
  BB_ISR_START();

  bbBleCb.evtState += 1;

  WSF_ASSERT(BbGetCurrentBod());

  BbOpDesc_t * const pCur = BbGetCurrentBod();
  BbBleSlvBisEvent_t * const pBis = &pCur->prot.pBle->op.slvBis;

  pBis->txDataCback(pCur, status);
  BB_INC_STAT(bbBisStats.txData);

  if (BbGetBodTerminateFlag())
  {
    PalBbBleCancelTifs();
    BbTerminateBod();
  }

  BB_ISR_MARK(bbBisStats.txIsrUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Cancel BIS slave BOD.
 *
 *  \param      pBod    Pointer to the BOD to cancel.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbSlvCancelBisOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  PalBbBleCancelData();
}

/*************************************************************************************************/
/*!
 *  \brief      Execute BIS slave BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
static void bbSlvExecuteBisOp(BbOpDesc_t *pBod, BbBleData_t *pBle)
{
  BbBleSlvBisEvent_t * const pBis = &pBod->prot.pBle->op.slvBis;

  PalBbBleSetChannelParam(&pBle->chan);

  bbBleCb.bbParam.txCback = bbSlvBisTxCompCback;
  /* bbBleCb.bbParam.rxCback = NULL; */         /* Unused */
  bbBleCb.bbParam.dueUsec = BbAdjustTime(pBod->dueUsec);
  pBod->dueUsec = bbBleCb.bbParam.dueUsec;
  /* bbBleCb.bbParam.rxTimeoutUsec = 0; */      /* Unused */
  PalBbBleSetDataParams(&bbBleCb.bbParam);

  bbBleCb.evtState = 0;

  pBis->execCback(pBod);

  if (BbGetBodTerminateFlag() &&    /* Client signaled cancel. */
      BbGetCurrentBod())            /* Termination still pending. */
  {
    BbTerminateBod();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize for BIS slave operations.
 *
 *  Update the operation table with BIS slave operations routines.
 */
/*************************************************************************************************/
void BbBleBisSlaveInit(void)
{
  bbBleRegisterOp(BB_BLE_OP_SLV_BIS_EVENT, bbSlvExecuteBisOp, bbSlvCancelBisOp);

  memset(&bbBisStats, 0, sizeof(bbBisStats));
}

/*************************************************************************************************/
/*!
 *  \brief      Transmit BIS Data PDU at next transmit slot.
 *
 *  \param      descs       Array of transmit buffer descriptor.
 *  \param      cnt         Number of descriptors.
 *  \param      nextPduTime Next PDU time.
 *  \param      pNextChan   Next PDU channel.
 */
/*************************************************************************************************/
void BbBleBisTxData(PalBbBleTxBufDesc_t descs[], uint8_t cnt, uint32_t nextPduTime, PalBbBleChan_t *pNextChan)
{
  if (pNextChan)
  {
    bbBleSetAbsIfs(nextPduTime, pNextChan);
  }
  else
  {
    bbBleClrIfs();
  }

  if (bbBleCb.evtState == 0)
  {
    PalBbBleTxData(descs, cnt);
  }
  else
  {
    BB_ISR_MARK(bbBisStats.txSetupUsec);
    PalBbBleTxTifsData(descs, cnt);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get BIS packet statistics.
 */
/*************************************************************************************************/
void BbBleGetBisStats(BbBleDataPktStats_t *pStats)
{
  *pStats = bbBisStats;
}
