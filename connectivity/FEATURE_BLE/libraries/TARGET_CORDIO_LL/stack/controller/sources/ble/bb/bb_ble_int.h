/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Internal BLE baseband interface file.
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

#ifndef BB_BLE_INT_H
#define BB_BLE_INT_H

#include "bb_ble_api.h"
#include "bb_ble_api_op.h"
#include "pal_bb_ble.h"
#include "pal_bb.h"
#include "ll_defs.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "cfg_mac_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Maximum length of an advertising packet. */
#define BB_ADVB_MAX_LEN         WSF_MAX(BB_FIXED_ADVB_PKT_LEN, LL_ADVB_MAX_LEN)

/*! \brief      Mark the start of an ISR. */
#define BB_ISR_START()          bbIsrStartTime = PalBbGetCurrentTime()

/*! \brief      Mark the ISR duration, recording the high watermark. */
#define BB_ISR_MARK(x)          x = WSF_MAX(x, PalBbGetCurrentTime() - bbIsrStartTime)

/*! \brief    Event states for advertising operations. */
enum
{
  BB_EVT_STATE_TX_ADV_IND,           /*!< Advertising indication. */
  BB_EVT_STATE_RX_SCAN_OR_CONN_INIT, /*!< Scan or connection init packet. */
  BB_EVT_STATE_TX_SCAN_OR_CONN_RSP,  /*!< Scan or connection response. */
  BB_EVT_STATE_TX_CHAIN_IND          /*!< Chain indication. */
};

/*! \brief    Event states for scanning operations. */
enum
{
  BB_EVT_STATE_RX_ADV_IND,           /*!< Advertising indication. */
  BB_EVT_STATE_TX_SCAN_OR_CONN_INIT, /*!< Scan or connection init packet. */
  BB_EVT_STATE_RX_SCAN_OR_CONN_RSP,  /*!< Scan or connection response. */
  BB_EVT_STATE_RX_CHAIN_IND          /*!< Chain indication. */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Execution operation function. */
typedef void (*bbBleExecOpFn_t)(BbOpDesc_t *pBod, BbBleData_t *pBle);

/*! \brief      BB control block. */
typedef struct
{
  struct
  {
    bbBleExecOpFn_t execOpCback;        /*!< Execute operation handler. */
    bbBleExecOpFn_t cancelOpCback;      /*!< Cancel operation handler. */
  } opCbacks[BB_BLE_OP_NUM];            /*!< Operation handlers. */

  uint8_t evtState;                     /*!< Action state of the currently operating BOD. */
  uint8_t advChIdx;                     /*!< Current advertising channel index. */
  uint8_t numChUsed;                    /*!< Total number of channels visited. */
  uint32_t lastScanStartUsec;           /*!< Last scan start time in microseconds. */

  PalBbBleDataParam_t bbParam;          /*!< Baseband data parameters. */

  uint16_t rxDataLen;                   /*!< Receive data buffer length. */
  uint8_t *pRxDataBuf;                  /*!< Current Rx data buffer. */
} bbBleCtrlBlk_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* BB BLE Control Block. */
extern bbBleCtrlBlk_t bbBleCb;

/* ISR start time. */
extern uint32_t bbIsrStartTime;

/*************************************************************************************************/
/*!
 *  \brief      Register operation handlers.
 *
 *  \param      opType          Operation type.
 *  \param      execOpCback     Execute operation callback.
 *  \param      cancelOpCback   Cancel operation callback.
 */
/*************************************************************************************************/
void bbBleRegisterOp(uint8_t opType, bbBleExecOpFn_t execOpCback, bbBleExecOpFn_t cancelOpCback);

#ifdef __cplusplus
};
#endif

/*************************************************************************************************/
/*!
 *  \brief      Clear IFS in operation.
 *
 *  The BB may choose not to enable IFS after the next Tx or Rx.
 */
/*************************************************************************************************/
static inline void bbBleClrIfs(void)
{
  /* PalBbBleOpParam_t opParams = { .ifsMode = PAL_BB_IFS_MODE_CLR, .ifsTime = 0, .pIfsChan = NULL }; */
  PalBbBleOpParam_t opParams = { 0 };
  PalBbBleSetOpParams(&opParams);
}

/*************************************************************************************************/
/*!
 *  \brief      Set TIFS in operation.
 *
 *  The BB must enable TIFS after the next Tx or Rx.
 */
/*************************************************************************************************/
static inline void bbBleSetTifs(void)
{
  PalBbBleOpParam_t opParams = { .ifsMode = PAL_BB_IFS_MODE_TOGGLE_TIFS, .ifsTime = 0, .pIfsChan = NULL };
  PalBbBleSetOpParams(&opParams);
}

/*************************************************************************************************/
/*!
 *  \brief      Set absolute time IFS in operation.
 *
 *  \param  ifsTime     Due time of the next PDU.
 *  \param  pIfsChan    Channel of the next PDU.
 *
 *  The BB must enable IFS with absolute time after the next Tx or Rx.
 */
/*************************************************************************************************/
static inline void bbBleSetAbsIfs(uint32_t ifsTime, PalBbBleChan_t *pIfsChan)
{
  PalBbBleOpParam_t opParams = { .ifsMode = PAL_BB_IFS_MODE_SAME_ABS, .ifsTime = ifsTime, .pIfsChan = pIfsChan };
  PalBbBleSetOpParams(&opParams);
}

#endif /* BB_BLE_INT_H */
