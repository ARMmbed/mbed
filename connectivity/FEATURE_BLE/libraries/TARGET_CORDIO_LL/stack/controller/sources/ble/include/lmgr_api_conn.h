/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer manager connection interface file.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
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

#ifndef LMGR_API_CONN_H
#define LMGR_API_CONN_H

#include "lmgr_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *  \addtogroup LL_LMGR_API_CONN
 *  \{
 */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Slave role device parameter definition. */
typedef struct
{
  uint8_t       availTxBuf;             /*!< Available number of transmit buffers. */
  uint8_t       availRxBuf;             /*!< Available number of receive buffers. */

  uint16_t      maxTxLen;               /*!< Default maximum number of Data PDU bytes. */
  uint16_t      maxTxTime;              /*!< Default maximum microseconds for a Data PDU. */

  wsfQueue_t    rxDataQ;                /*!< Receive Data PDU (LE-U or LE-C) queue. */

  uint32_t      dataPendMsk;            /*!< Bitmask of connection handles with new pending data. */

  /* PHY */
  uint8_t       allPhys;                /*!< Default all PHYs. */
  uint8_t       txPhys;                 /*!< Default transmitter PHYs. */
  uint8_t       rxPhys;                 /*!< Default receiver PHYs. */

  /* PAST */
  uint8_t       syncMode;               /*!< Default sync transfer mode. */
  uint16_t      syncSkip;               /*!< Default sync skip for periodic adv sync transfer. */
  uint16_t      syncTimeout;            /*!< Default sync timeout for periodic adv sync transfer. */

  /* Enhanced Connection Update */
  uint16_t      defSrMin;               /*!< Default subrate minimum value. */
  uint16_t      defSrMax;               /*!< Default subrate maximum value. */
  uint16_t      defMaxLatency;          /*!< Default maximum latency. */
  uint16_t      defContNum;             /*!< Default continuation number. */
  uint16_t      defSvt;                 /*!< Default supervision timeout. */
} lmgrConnCtrlBlk_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

extern lmgrConnCtrlBlk_t lmgrConnCb;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void LmgrConnInit(void);

/*! \} */    /* LL_LMGR_API_CONN */

#ifdef __cplusplus
};
#endif

#endif /* LMGR_API_CONN_H */
