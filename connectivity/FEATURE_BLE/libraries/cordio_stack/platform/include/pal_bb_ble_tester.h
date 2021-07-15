/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      BLE Baseband tester interface file.
 *
 *  Copyright (c) 2018 Arm Ltd. All Rights Reserved.
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

#ifndef PAL_BB_BLE_TESTER_H
#define PAL_BB_BLE_TESTER_H

#include "pal_types.h"

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Enable bypassing PDU filtering.
 *
 *  \param      enable      If TRUE enable bypass PDU filtering, FALSE otherwise.
 *
 *  Enable bypassing PDU filtering.
 */
/*************************************************************************************************/
void PalBbTesterEnablePduFilterBypass(bool_t enable);

/*************************************************************************************************/
/*!
 *  \brief      Invalidate next access address.
 *
 *  \param      forRx    For Rx or Tx boolean.
 */
/*************************************************************************************************/
void PalBbTesterInvalidateNextAccAddr(bool_t forRx);

/*************************************************************************************************/
/*!
 *  \brief      Invalidate access address value.
 *
 *  \param      chanMask    Invalidate channel mask.
 *  \param      invalidMask Enable mask for invalidating access address (0 to disable).
 *  \param      shiftMask   TRUE if corrupting AA by one bit and corrupted bit location is shifted every TX/RX.
 *  \param      forRx       TRUE for Rx, FALSE for Tx.
 *
 *  Force the receiver to receive a miss a packet if the receive channel is in
 *  \b chanMask while stepping through the invalid pattern in \b invalidMask.
 */
/*************************************************************************************************/
void PalBbTesterSetInvalidAccessAddress(uint64_t chanMask, uint32_t invalidMask, bool_t shiftMask, bool_t forRx);

/*************************************************************************************************/
/*!
 *  \brief      Invalidate CRC initialization value.
 *
 *  \param      chanMask    Invalidate channel mask.
 *  \param      adjMask     Number of adjustments (0 to disable).
 *  \param      forRx       TRUE for Rx, FALSE for Tx.
 *
 *  Force the receiver to receive a packet with CRC error if the receive channel is in
 *  \b chanMask while stepping through the invalid pattern in \b invalidMask.
 */
/*************************************************************************************************/
void PalBbTesterSetInvalidCrcInit(uint64_t chanMask, uint32_t adjMask, bool_t forRx);

/*************************************************************************************************/
/*!
 *  \brief      Trigger channel modifications on matching Tx packet header.
 *
 *  \param      hdrMask     Header mask.
 *  \param      hdrValue    Match value.
 *
 *  Modify the transmit channel parameters of a packet only when the Tx packet header matches
 *  the given parameters. This applies to the modification parameter provided by the following
 *  routines:
 *
 *      - \ref PalBbTesterSetInvalidCrcInit()
 *      - \ref PalBbTesterSetInvalidAccessAddress()
 */
/*************************************************************************************************/
void PalBbTesterSetModifyTxPktTrigger(uint16_t hdrMask, uint16_t hdrValue);

/*************************************************************************************************/
/*!
 *  \brief      Adjust Tx TIFS timing value.
 *
 *  \param      adjNs       Adjustment value in nanoseconds.
 *
 *  Adjust the TIFS timing of transmit by the given signed value of timer ticks.
 *  If adjustment value is out of range, maximum allowed value is used.
 */
/*************************************************************************************************/
void PalBbTesterAdjTxTifsNs(int16_t adjNs);

#ifdef __cplusplus
};
#endif

#endif /* PAL_BB_BLE_TESTER_H */
