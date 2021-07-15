/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Connected isochronous stream baseband porting implementation file.
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

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

BbBleDataPktStats_t bbCisStats;          /*!< CIS packet statistics. */

/*************************************************************************************************/
/*!
 *  \brief      Transmit data PDU at next transmit slot.
 *
 *  \param      descs       Array of transmit buffer descriptor.
 *  \param      cnt         Number of descriptors.
 *
 *  \note       This function is expected to be called during the call context of
 *              \ref BbBleMstCisEvent_t::rxDataCback or \ref BbBleSlvCisEvent_t::rxDataCback
 *              callback routine.
 */
/*************************************************************************************************/
void BbBleCisTxData(PalBbBleTxBufDesc_t descs[], uint8_t cnt)
{
  if ((BbGetCurrentBod()->prot.pBle->chan.opType == BB_BLE_OP_MST_CIS_EVENT) &&
      (bbBleCb.evtState == 0))
  {
    bbBleSetTifs();     /* master always Rx's after Tx */
    PalBbBleTxData(descs, cnt);
  }
  else
  {
    BB_ISR_MARK(bbCisStats.txSetupUsec);
    /* TODO set only if master or if slave and Rx may follow in CE. */
    bbBleSetTifs();
    PalBbBleTxTifsData(descs, cnt);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set receive data buffer for next receive slot.
 *
 *  \param      pBuf        Receive data buffer.
 *  \param      len         Maximum length of data buffer.
 *
 *  \note       This function is expected to be called during the call context of
 *              \ref BbBleMstCisEvent_t::rxDataCback or
 *              \ref BbBleSlvCisEvent_t::rxDataCback callback routine.
 *
 *  \note       BB must always call the \ref BbBleMstCisEvent_t::rxDataCback or
 *              \ref BbBleSlvCisEvent_t::rxDataCback callback routine of the
 *              currently executing BOD with the given buffer.
 */
/*************************************************************************************************/
void BbBleCisRxData(uint8_t *pBuf, uint16_t len)
{
  WSF_ASSERT(!bbBleCb.pRxDataBuf);

  bbBleCb.pRxDataBuf = pBuf;
  bbBleCb.rxDataLen = len;

  if ((BbGetCurrentBod()->prot.pBle->chan.opType == BB_BLE_OP_SLV_CIS_EVENT) &&
      (bbBleCb.evtState == 0))
  {
    bbBleSetTifs();       /* slave always Tx's after Rx */
    PalBbBleRxData(pBuf, len);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get CIS packet statistics.
 */
/*************************************************************************************************/
void BbBleGetCisStats(BbBleDataPktStats_t *pStats)
{
  *pStats = bbCisStats;
}
