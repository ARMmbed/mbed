/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      BLE RF path compensation implementation file.
 *
 *  Copyright (c) 2016-2019 Arm Ltd. All Rights Reserved.
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

#include "pal_types.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Minimum RF path compensation (in 0.1dBm units). */
#define BB_BLE_MIN_PATH_COMP        -1280

/*! \brief      Minimum RF path compensation (in 1dBm units). */
#define BB_BLE_MIN_PATH_COMP_DBM    -128

/*! \brief      Maximum RF path compensation (in 0.1dBm units). */
#define BB_BLE_MAX_PATH_COMP        1280

/*! \brief      Binary divide with 10 divisor (n[max]=0xFFFFFFFF). */
#define BB_BLE_MATH_DIV_10(n)       ((uint32_t)(((uint64_t)(n) * UINT64_C(419431)) >> 22))

/**************************************************************************************************
  Constants
**************************************************************************************************/

#if defined(NRF52840_XXAA) || defined(NRF52832_XXAA)
/* \brief   Minimum Tx power level (expressed in 1dBm units). */
static const int8_t bbBleMinTxPwr = -40;  /* -40dBm */
#else
/* \brief   Minimum Tx power level (expressed in 1dBm units). */
static const int8_t bbBleMinTxPwr = -30;  /* -30dBm */
#endif

#if defined(NRF52840_XXAA)
/* \brief   Maximum Tx power level (expressed in 1dBm units). */
static const int8_t bbBleMaxTxPwr = 9;    /*  +9dBm */
#else
/* \brief   Maximum Tx power level (expressed in 1dBm units). */
static const int8_t bbBleMaxTxPwr = 4;    /*  +4dBm */
#endif

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Current Tx path compensation value. */
int16_t bbBleTxPathComp = -1280;

/*! \brief      Current Rx path compensation value. */
int16_t bbBleRxPathComp = -1280;

/*************************************************************************************************/
/*!
 *  \brief      Get transmit RF path compensation.
 *
 *  \return     Transmit RF path compensation (in 1-dBm units).
 */
/*************************************************************************************************/
int8_t palRadioGetTxRfPathComp(void)
{
  uint16_t pathCompUnsigned = (uint16_t)(bbBleTxPathComp - BB_BLE_MIN_PATH_COMP);

  return (int16_t)BB_BLE_MATH_DIV_10(pathCompUnsigned) + BB_BLE_MIN_PATH_COMP_DBM;
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize RF path compensation.
 */
/*************************************************************************************************/
void PalRadioInitPathComp(void)
{
  bbBleTxPathComp = 0;
  bbBleRxPathComp = 0;
}

/*************************************************************************************************/
/*!
 *  \brief      Get supported transmit power.
 *
 *  \param      pMinTxPwr   Return buffer for minimum transmit power (expressed in 1dBm units).
 *  \param      pMaxTxPwr   Return buffer for maximum transmit power (expressed in 1dBm units).
 */
/*************************************************************************************************/
void PalRadioGetSupTxPower(int8_t *pMinTxPwr, int8_t *pMaxTxPwr)
{
  *pMinTxPwr = bbBleMinTxPwr;
  *pMaxTxPwr = bbBleMaxTxPwr;
}

/*************************************************************************************************/
/*!
 *  \brief      Get RF path compensation.
 *
 *  \param      pTxPathComp Return buffer for RF transmit path compensation value (expressed in 0.1dBm units).
 *  \param      pRxPathComp Return buffer for RF receive path compensation value (expressed in 0.1dBm units).
 */
/*************************************************************************************************/
void PalRadioReadRfPathComp(int16_t *pTxPathComp, int16_t *pRxPathComp)
{
  *pTxPathComp = bbBleTxPathComp;
  *pRxPathComp = bbBleRxPathComp;
}

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
bool_t PalRadioWriteRfPathComp(int16_t txPathComp, int16_t rxPathComp)
{
  bbBleTxPathComp = txPathComp;
  bbBleRxPathComp = rxPathComp;

  return TRUE;
}

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
int8_t PalRadioGetActualTxPower(int8_t txPwr, bool_t compFlag)
{
#if defined(NRF52840_XXAA)
  if      (txPwr >=   9)  {  txPwr =   9;  }
  else if (txPwr >=   8)  {  txPwr =   8;  }
  else if (txPwr >=   7)  {  txPwr =   7;  }
  else if (txPwr >=   6)  {  txPwr =   6;  }
  else if (txPwr >=   5)  {  txPwr =   5;  }
  else if (txPwr >=   4)  {  txPwr =   4;  }
  else if (txPwr >=   3)  {  txPwr =   3;  }
  else if (txPwr >=   2)  {  txPwr =   2;  }
  else if (txPwr >=   0)  {  txPwr =   0;  }
  else if (txPwr >=  -4)  {  txPwr =  -4;  }
  else if (txPwr >=  -8)  {  txPwr =  -8;  }
  else if (txPwr >= -12)  {  txPwr = -12;  }
  else if (txPwr >= -16)  {  txPwr = -16;  }
  else if (txPwr >= -20)  {  txPwr = -20;  }
  else                    {  txPwr = -40;  }
#elif defined(NRF52832_XXAA)
  if      (txPwr >=   4)  {  txPwr =   4;  }
  else if (txPwr >=   3)  {  txPwr =   3;  }
  else if (txPwr >=   0)  {  txPwr =   0;  }
  else if (txPwr >=  -4)  {  txPwr =  -4;  }
  else if (txPwr >=  -8)  {  txPwr =  -8;  }
  else if (txPwr >= -12)  {  txPwr = -12;  }
  else if (txPwr >= -16)  {  txPwr = -16;  }
  else if (txPwr >= -20)  {  txPwr = -20;  }
  else                    {  txPwr = -40;  }
#else
  if      (txPwr >=   4)  {  txPwr =   4;  }
  else if (txPwr >=   0)  {  txPwr =   0;  }
  else if (txPwr >=  -4)  {  txPwr =  -4;  }
  else if (txPwr >=  -8)  {  txPwr =  -8;  }
  else if (txPwr >= -12)  {  txPwr = -12;  }
  else if (txPwr >= -16)  {  txPwr = -16;  }
  else if (txPwr >= -20)  {  txPwr = -20;  }
  else                    {  txPwr = -30;  }
#endif

  if (compFlag)
  {
    txPwr += palRadioGetTxRfPathComp();
  }

  return txPwr;
}

/*************************************************************************************************/
/*!
 *  \brief      Request an increase in power.

 *  \param      reqPwr           Requested Power.
 *  \param      delta            Delta
 *
 *  \return     TxPower to be set
 *
 *              If increasing power: the controller will increase one step if possible.
 *              If decreasing power: the controller will only decrease to the ceiling step.
 */
/*************************************************************************************************/
int8_t PalRadioIncreasePower(int8_t reqPwr, int8_t delta)
{
  /* An increase in power. The controller will always increase one step if possible. */
  if (delta > 0)
  {
#if defined(NRF52840_XXAA)
         if (reqPwr >    8)  {  reqPwr =   9;  }
    else if (reqPwr >   7)  {  reqPwr =    8;  }
    else if (reqPwr >   6)  {  reqPwr =    7;  }
    else if (reqPwr >   5)  {  reqPwr =    6;  }
    else if (reqPwr >   4)  {  reqPwr =    5;  }
    else if (reqPwr >   3)  {  reqPwr =    4;  }
    else if (reqPwr >   2)  {  reqPwr =    3;  }
    else if (reqPwr >   0)  {  reqPwr =    2;  }
    else if (reqPwr >  -4)  {  reqPwr =    0;  }
    else if (reqPwr >  -8)  {  reqPwr =   -4;  }
    else if (reqPwr > -12)  {  reqPwr =   -8;  }
    else if (reqPwr > -16)  {  reqPwr =  -12;  }
    else if (reqPwr > -20)  {  reqPwr =  -16;  }
    else                     {  reqPwr = -20;  }
#elif defined(NRF52832_XXAA)
    if      (reqPwr >   3)  {  reqPwr =   4;  }
    else if (reqPwr >   2)  {  reqPwr =   3;  }
    else if (reqPwr >   0)  {  reqPwr =   2;  }
    else if (reqPwr >  -4)  {  reqPwr =   0;  }
    else if (reqPwr >  -8)  {  reqPwr =  -4;  }
    else if (reqPwr > -12)  {  reqPwr =  -8;  }
    else if (reqPwr > -16)  {  reqPwr = -12;  }
    else if (reqPwr > -20)  {  reqPwr = -16;  }
    else                    {  reqPwr = -20;  }
#else
         if (reqPwr >   0)  {  reqPwr =   4;  }
    else if (reqPwr >  -4)  {  reqPwr =   0;  }
    else if (reqPwr >  -8)  {  reqPwr =  -4;  }
    else if (reqPwr > -12)  {  reqPwr =  -8;  }
    else if (reqPwr > -16)  {  reqPwr = -12;  }
    else if (reqPwr > -20)  {  reqPwr = -16;  }
    else                    {  reqPwr = -20;  }
#endif
  }
  /* A decrease in power. The controller will decrease to higher step if reqPwer is inbetween two steps. */
  else if (delta < 0)
  {
#if defined(NRF52840_XXAA)
    if      (reqPwr <= -40) { reqPwr = -40; }
    else if (reqPwr <= -20) { reqPwr = -20; }
    else if (reqPwr <= -16) { reqPwr = -16; }
    else if (reqPwr <= -12) { reqPwr = -12; }
    else if (reqPwr <=  -8) { reqPwr =  -8; }
    else if (reqPwr <=  -4) { reqPwr =  -4; }
    else if (reqPwr <=   0) { reqPwr =   0; }
    else if (reqPwr <=   2) { reqPwr =   2; }
    else if (reqPwr <=   3) { reqPwr =   3; }
    else if (reqPwr <=   4) { reqPwr =   4; }
    else if (reqPwr <=   5) { reqPwr =   5; }
    else if (reqPwr <=   6) { reqPwr =   6; }
    else if (reqPwr <=   7) { reqPwr =   7; }
    else if (reqPwr <=   8) { reqPwr =   8; }
    else                    { reqPwr =   9; }
#elif defined(NRF52832_XXAA)
    if      (reqPwr <= -40) { reqPwr = -40; }
    else if (reqPwr <= -20) {reqPwr = -20; }
    else if (reqPwr <= -16) {reqPwr = -16; }
    else if (reqPwr <= -12) {reqPwr = -12; }
    else if (reqPwr <=  -8) {reqPwr =  -8; }
    else if (reqPwr <=  -4) {reqPwr =  -4; }
    else if (reqPwr <=   0) {reqPwr =   0; }
    else if (reqPwr <=   3) {reqPwr =   3; }
    else                    {reqPwr =   4; }
#else
    if      (reqPwr <= -30) {reqPwr = -30; }
    else if (reqPwr <= -20) {reqPwr = -20; }
    else if (reqPwr <= -16) {reqPwr = -16; }
    else if (reqPwr <= -12) {reqPwr = -12; }
    else if (reqPwr <=  -8) {reqPwr =  -8; }
    else if (reqPwr <=  -4) {reqPwr =  -4; }
    else if (reqPwr <=   0) {reqPwr =   0; }
    else                    {reqPwr =   4; }
#endif
  }
  else
  {
    /* No change. */
  }

  return reqPwr;
}
