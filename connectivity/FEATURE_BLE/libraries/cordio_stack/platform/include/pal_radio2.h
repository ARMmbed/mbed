/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Radio interface file.
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

#ifndef PAL_RADIO_H
#define PAL_RADIO_H

#include <stdint.h>
#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_RADIO
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Use this to enable debugging stuff */
#define PAL_RADIO_DEBUG

/**************************************************************************************************
  Data Types
**************************************************************************************************/

typedef enum
{
  PAL_RADIO_STATE_UNINIT = 0,       /*!< Uninitialized state. */
  PAL_RADIO_STATE_ERROR = 0,        /*!< Error state. */
  PAL_RADIO_STATE_IDLE,             /*!< Idle state. */
  PAL_RADIO_STATE_TX,               /*!< Tx state. */
  PAL_RADIO_STATE_RX,               /*!< Rx state. */
  PAL_RADIO_STATE_DIAG              /*!< Diagnostics state. */
} PalRadioState_t;

typedef enum
{
  PAL_RADIO_STATUS_SUCCESS,         /*!< Operation completed successfully. */
  PAL_RADIO_STATUS_FAILED,          /*!< General failure. */
  PAL_RADIO_STATUS_TIMEOUT,         /*!< Rx timed out. */
  PAL_RADIO_STATUS_CRC_FAILED,      /*!< Rx CRC match failed. */
  PAL_RADIO_STATUS_MIC_FAILED       /*!< Rx MIC match failed. */
} PalRadioStatus_t;

typedef enum
{
  PAL_RADIO_PHY_1M,                 /*!< 1M PHY. */
  PAL_RADIO_PHY_2M,                 /*!< 2M PHY. */
  PAL_RADIO_PHY_CODED_DEFAULT,      /*!< Coded with options not specified. */
  PAL_RADIO_PHY_CODED_S2,           /*!< Coded S2 PHY (500-kbps). */
  PAL_RADIO_PHY_CODED_S8            /*!< Coded S8 PHY (128-kbps). */
} PalRadioPhy_t;

/*! \brief  Tx completion call signature. */
typedef void (*PalRadioTxComp_t)(PalRadioStatus_t status);

/*! \brief  Rx completion call signature. */
typedef void (*PalRadioRxComp_t)(PalRadioStatus_t status, uint32_t startOffs, PalRadioPhy_t phy, int8_t rssi);

/**************************************************************************************************
  Functions
**************************************************************************************************/

/* Initialization */
void PalRadioInit(void);
void PalRadioDeInit(void);

/* Control and Status */
PalRadioState_t PalRadioGetState(void);
void PalRadioRegisterTxComplete(PalRadioTxComp_t cback);
void PalRadioRegisterRxComplete(PalRadioRxComp_t cback);
void PalRadioSetAccessAddress(uint32_t addr);
void PalRadioSetCrcInit(uint32_t crcInit);
void PalRadioSetChannel(uint8_t chIdx);
void PalRadioSetTxPower(int8_t level);
int8_t PalRadioGetTxPower(void);
void PalRadioSetPhy(PalRadioPhy_t txPhy, PalRadioPhy_t rxPhy);
void PalRadioSetDataWhitening(uint8_t ena);
void PalRadioSetPrbs15(uint8_t ena);

/* Encryption */
void PalRadioSetEncryption(uint8_t txEna, uint8_t rxEna);
void PalRadioSetSessionKey(uint8_t *pSK);
void PalRadioSetIV(uint8_t *pIV);
void PalRadioSetDirection(uint8_t dir);
void PalRadioSetEncryptionType(uint8_t type);
void PalRadioSetPacketCounter(uint64_t pktCtr);

/* Data Transfer */
void PalRadioTxStart(uint32_t dueTime);
void PalRadioTxTifs(uint16_t tifsUsec);
void PalRadioTxOffset(uint32_t offsUsec);
void PalRadioTxData(uint8_t *pBuf, uint16_t len);
bool_t PalRadioRxStart(uint32_t dueTime, uint32_t rxTimeoutUsec);
void PalRadioRxTifs(uint16_t tifsUsec, uint32_t rxTimeoutUsec);
bool_t PalRadioRxOffset(uint32_t offsUsec, uint32_t rxTimeoutUsec);
void PalRadioRxData(uint8_t *pBuf, uint16_t len);
void PalRadioStop(void);

/*! \} */    /* PAL_RADIO */

#ifdef __cplusplus
};
#endif

#endif /* PAL_RADIO_H */
