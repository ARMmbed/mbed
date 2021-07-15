/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Radio interface file.
 *
 *  Copyright (c) 2018-2019 Arm Ltd. All Rights Reserved.
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

#ifndef PAL_RADIO_H
#define PAL_RADIO_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_RADIO
 *  \{ */

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Initialize RF path compensation.
 */
/*************************************************************************************************/
void PalRadioInitPathComp(void);

/*************************************************************************************************/
/*!
 *  \brief      Get the actual Tx power at the antenna (expressed in 1dBm units).
 *
 *  \param      txPwr           Tx power provided by the host (expressed in 1dBm units).
 *  \param      compFlag        Flag to apply Tx path compensation or not.
 *
 *  \return     Actual Tx power at the antenna (expressed in 1dBm units).
 *
 *  Tx path compensation is only used for extended ADV header.
 *  Compensation is not considered when filling in HCI events.
 */
/*************************************************************************************************/
int8_t PalRadioGetActualTxPower(int8_t txPwr, bool_t compFlag);

/*************************************************************************************************/
/*!
 *  \brief      Request an increase in power.
 *
 *  \param      reqPwr           Requested Power.
 *  \param      delta            Delta
 *
 *  \return     TxPower to be set
 *
 *              If increasing power: the controller will increase one step if possible.
 *              If decreasing power: the controller will only decrease to the ceiling step.
 */
/*************************************************************************************************/
int8_t PalRadioIncreasePower(int8_t reqPwr, int8_t delta);

/*************************************************************************************************/
/*!
 *  \brief      Get the next acceptable power reduction step.
 *
 *  \param      txPwr           Tx Power(expressed in 1dBm units).
 *
 *  \return     Lowest acceptable power reduction size.
 *
 */
/*************************************************************************************************/
int8_t PalRadioGetAcceptablePowerReduction(int8_t txPwr);

/*************************************************************************************************/
/*!
 *  \brief      Set RF path compensation.
 *
 *  \param      txPathComp      RF transmit path compensation value (expressed in 0.1dBm units).
 *  \param      rxPathComp      RF receive path compensation value (expressed in 0.1dBm units).
 *
 *  \return     TRUE if successful, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t PalRadioWriteRfPathComp(int16_t txPathComp, int16_t rxPathComp);

/*************************************************************************************************/
/*!
 *  \brief      Get supported transmit power.
 *
 *  \param      pMinTxPwr   Return buffer for minimum transmit power (expressed in 1dBm units).
 *  \param      pMaxTxPwr   Return buffer for maximum transmit power (expressed in 1dBm units).
 */
/*************************************************************************************************/
void PalRadioGetSupTxPower(int8_t *pMinTxPwr, int8_t *pMaxTxPwr);

/*************************************************************************************************/
/*!
 *  \brief      Read RF path compensation.
 *
 *  \param      pTxPathComp Return buffer for RF transmit path compensation value (expressed in 0.1dBm units).
 *  \param      pRxPathComp Return buffer for RF receive path compensation value (expressed in 0.1dBm units).
 */
/*************************************************************************************************/
void PalRadioReadRfPathComp(int16_t *pTxPathComp, int16_t *pRxPathComp);

/*! \} */    /* PAL_RADIO */

#ifdef __cplusplus
};
#endif

#endif /* PAL_RADIO_H */
