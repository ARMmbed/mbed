/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      BLE baseband interface file.
 *
 *  Copyright (c) 2016-2019 Arm Ltd. All Rights Reserved.
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

#ifndef BB_BLE_API_OP_H
#define BB_BLE_API_OP_H

#include "bb_api.h"
#include "pal_bb_ble.h"
#include "bb_ble_api_pdufilt.h"
#include "ll_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup BB_API_BLE
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Convert BLE protocol ticks to BB ticks. */
#define BB_BLE_TO_US(n)             ((n) * LL_BLE_US_PER_TICK)

/*! \brief      Increment statistics counter. */
#define BB_INC_STAT(s)              s++

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*! \brief      Operation types. */
typedef enum
{
  BB_BLE_OP_TEST_TX,                /*!< Continuous Tx test mode. */
  BB_BLE_OP_TEST_RX,                /*!< Continuous Rx test mode. */
  BB_BLE_OP_MST_ADV_EVENT,          /*!< Master advertising event. */
  BB_BLE_OP_SLV_ADV_EVENT,          /*!< Slave advertising event. */
  BB_BLE_OP_MST_CONN_EVENT,         /*!< Master connection event. */
  BB_BLE_OP_SLV_CONN_EVENT,         /*!< Slave connection event. */
  BB_BLE_OP_MST_AUX_ADV_EVENT,      /*!< Master auxiliary advertising event. */
  BB_BLE_OP_SLV_AUX_ADV_EVENT,      /*!< Slave auxiliary advertising event. */
  BB_BLE_OP_SLV_PER_ADV_EVENT,      /*!< Slave periodic advertising event. */
  BB_BLE_OP_MST_PER_SCAN_EVENT,     /*!< Master periodic scanning event. */
  BB_BLE_OP_MST_CIS_EVENT,          /*!< Master CIS event. */
  BB_BLE_OP_SLV_CIS_EVENT,          /*!< Slave CIS event. */
  BB_BLE_OP_MST_BIS_EVENT,          /*!< Master BIS event. */
  BB_BLE_OP_SLV_BIS_EVENT,          /*!< Slave BIS event. */
  BB_BLE_OP_NUM                     /*!< Total number of operations. */
} BbBleOp_t;

/*! \brief      Maximum request PDU length (MAX(LL_SCAN_REQ_PDU_LEN, LL_CONN_IND_PDU_LEN)). */
#define BB_REQ_PDU_MAX_LEN          (LL_ADV_HDR_LEN + LL_CONN_IND_PDU_LEN)

/*! \brief      Minimum scan time to cover one advertise data exchange. */
#define BB_MIN_SCAN_US              (LL_ADV_PKT_MAX_USEC  + LL_BLE_TIFS_US + \
                                     LL_SCAN_REQ_MAX_USEC + LL_BLE_TIFS_US + \
                                     LL_SCAN_RSP_MAX_USEC + \
                                     BbGetSchSetupDelayUs())

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Pre-execute callback signature. */
typedef void (*BbBlePreExec_t)(BbOpDesc_t *pBod);

/*! \brief      Execute callback signature. */
typedef void (*BbBleExec_t)(BbOpDesc_t *pBod);

/*! \brief      Cancel callback signature. */
typedef void (*BbBleCancel_t)(BbOpDesc_t *pBod);

/*! \brief      Advertising PDU transmit setup call signature. */
typedef void (*BbBleTxAdvSetup_t)(BbOpDesc_t *pBod, uint32_t advTxTime);

/*! \brief      Chain indication PDU transmit setup call signature. */
typedef uint32_t (*BbBleTxAuxSetup_t)(BbOpDesc_t *pBod, bool_t isChainInd);

/*! \brief      Aux Rx setup call signature. Returns TRUE if Scan was programmed. */
typedef bool_t (*BbBleRxAuxSetup_t)(BbOpDesc_t *pBod, uint32_t refTime, uint32_t remScanDur);

/*! \brief      Aux Tx complete call signature. Returns TRUE if BOD is complete. */
typedef bool_t (*BbBleAuxTxComp_t)(BbOpDesc_t *pBod, uint8_t status);

/*! \brief      Aux Rx complete call signature. Returns TRUE if BOD is complete. */
typedef bool_t (*BbBleAuxRxComp_t)(BbOpDesc_t * const pCur, uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp, uint8_t rxPhyOptions);

/*! \brief      Returns TRUE if an scan request/response required. */
typedef bool_t (*BbBleAdvComp_t)(BbOpDesc_t *pBod, const uint8_t *pBuf);

/*! \brief      Rx completion post processing call signature. */
typedef void (*BbBleAdvPost_t)(BbOpDesc_t *pBod, const uint8_t *pBuf);

/*! \brief      Chain indication PDU received call signature. */
typedef uint32_t (*BbBleRxChain_t)(BbOpDesc_t *pBod, const uint8_t *pBuf);

/*! \brief      Chain indication PDU received post call signature. */
typedef bool_t (*BbBleRxChainPost_t)(BbOpDesc_t *pBod, const uint8_t *pBuf);

/*! \brief      Data transmit completion callback signature. */
typedef void (*BbBleTxDataComp_t)(BbOpDesc_t *pBod, uint8_t status);

/*! \brief      Data receive completion callback signature. */
typedef void (*BbBleRxDataComp_t)(BbOpDesc_t *pBod, uint8_t *pBuf, uint8_t status);

/*! \brief      CIS check whether to continue current operation call signature. */
typedef uint32_t (*BbBleCisCheckContOp_t)(BbOpDesc_t *pBod, bool_t *pNewCisCtx);

/*! \brief      CIS post execute callback signature. */
typedef void (*BbBleCisPostExec_t)(BbOpDesc_t *pBod, uint8_t status);

/*! \brief      CIS data receive completion callback signature. */
typedef void (*BbBleCisRxDataComp_t)(BbOpDesc_t *pBod, uint8_t *pBuf, uint8_t status);

/*! \brief      Test completion callback signature. */
typedef bool_t (*BbBleTestComp_t)(BbOpDesc_t *pBod, uint8_t status);

/*! \brief      Periodic PDU Rx complete call signature. */
typedef uint32_t (*BbBlePerComp_t)(BbOpDesc_t *pBod, const uint8_t *pBuf, uint8_t status);

/*! \brief      Periodic PDU Rx complete post call signature. */
typedef bool_t (*BbBlePerPostComp_t)(BbOpDesc_t *pBod, const uint8_t *pBuf);

/*!
 * \brief       Master advertising event operation data (\ref BB_BLE_OP_MST_ADV_EVENT).
 *
 * \note        BB assumes maximum adversing and scan response payload is 39 bytes.
 */
typedef struct
{
  uint8_t                 *pRxAdvBuf;         /*!< Advertising buffer. */
  uint8_t                 *pTxReqBuf;         /*!< Scan request buffer. */
  uint8_t                 *pRxRspBuf;         /*!< Scan response buffer. */

  BbBlePreExec_t          preExecCback;       /*!< Pre-execute callback. */
  BbBleAdvComp_t          rxAdvCback;         /*!< Advertising completion callback. */
  BbBleAdvPost_t          rxAdvPostCback;     /*!< Advertising completion post processing callback. */
  BbBleAdvComp_t          txReqCback;         /*!< Scan request completion callback. */
  BbBleAdvComp_t          rxRspCback;         /*!< Scan response completion callback. */

  /* Aux Scan BOD link */
  BbOpDesc_t              *auxScanBod;        /*!< Pointer to the Aux Scan BOD */
  BbBleRxAuxSetup_t       auxScanCheckCback;  /*!< Setup an Aux Scan that needs to be programmed ASAP. */
  BbBleAuxTxComp_t        auxScanTxCompCback; /*!< Aux Scan Tx complete. */
  BbBleAuxRxComp_t        auxScanRxCompCback; /*!< Aux Scan Rx complete. */
  bool_t                  auxScanOpRunning;   /*!< Informs if an Aux Scan operation is running in the context of the Ext Scan BOD. */

  uint8_t                 txReqLen;           /*!< Scan request buffer length. */

  uint8_t                 scanChMap;          /*!< Scan channel map. */

  /* Return parameters. */
  int8_t                  advRssi;            /*!< RSSI of advertisement. */
  uint8_t                 advRxPhyOptions;    /*!< Rx PHY options. */
  uint32_t                advCrc;             /*!< CRC of advertisement. */
  uint32_t                advStartTsUsec;     /*!< Start of advertising packet timestamp in microseconds. */
  uint32_t                elapsedUsec;        /*!< Elapsed time of a single scan window in microseconds. */

  /* Filter results. */
  bbBlePduFiltResults_t   filtResults;        /*!< Results from PDU filtering. */
} BbBleMstAdvEvent_t;

/*!
 * \brief       Slave advertising event operation data (\ref BB_BLE_OP_SLV_ADV_EVENT).
 *
 * \note        BB assumes maximum scan request payload is 39 bytes.
 */
typedef struct
{
  uint8_t                 *pTxAdvBuf;         /*!< Advertising buffer. */
  uint8_t                 *pRxReqBuf;         /*!< Scan request buffer (must be size of BB_REQ_PDU_MAX_LEN). */
  uint8_t                 *pTxRspBuf;         /*!< Scan response buffer. */

  BbBleTxAdvSetup_t       txAdvSetupCback;    /*!< Adv PDU transmit setup callback. */
  BbBleAdvComp_t          rxReqCback;         /*!< Scan/Connect request receive completion callback. */
  BbBleAdvPost_t          rxReqPostCback;     /*!< Scan/Connect request receive post processing callback. */

  uint8_t                 txAdvLen;           /*!< Advertising buffer length. */
  uint8_t                 txRspLen;           /*!< Scan response buffer length. */

  uint8_t                 firstAdvChIdx;      /*!< first advertising channel index. */

  uint8_t                 advChMap;           /*!< Advertising channel map. */

  /* Return parameters. */
  uint32_t                reqStartTsUsec;     /*!< Start of request packet timestamp in microseconds. */

  /* Filter results. */
  bbBlePduFiltResults_t   filtResults;        /*!< Results from PDU filtering. */
} BbBleSlvAdvEvent_t;

/*! \brief       Master auxiliary advertising event operation data (\ref BB_BLE_OP_MST_AUX_ADV_EVENT). */
typedef struct
{
  uint8_t                 *pTxAuxReqBuf;      /*!< Scan request buffer. */

  BbBleAdvComp_t          rxAuxAdvCback;      /*!< Advertising completion callback. */
  BbBleAdvPost_t          rxAuxAdvPostCback;  /*!< Advertising completion post callback. */
  BbBleAdvComp_t          rxAuxRspCback;      /*!< Scan response completion callback. */
  BbBleRxChain_t          rxAuxChainCback;    /*!< Chain completion callback. */
  BbBleRxChainPost_t      rxAuxChainPostCback;/*!< Chain completion post callback. */

  uint8_t                 txAuxReqLen;        /*!< Scan request buffer length. */

  uint32_t                rxSyncDelayUsec;    /*!< Receive timeout in microseconds. */
  bool_t                  isInit;             /*!< TRUE if the event is for an initiation. */

  /* Return parameters. */
  int8_t                  auxAdvRssi;         /*!< RSSI of advertisement. */
  uint8_t                 auxRxPhyOptions;    /*!< Rx PHY options. */
  uint32_t                auxAdvCrc;          /*!< CRC of advertisement. */
  uint32_t                auxStartTsUsec;     /*!< Start of auxiliary advertising packet timestamp in microseconds. */

  /* Filter results. */
  bbBlePduFiltResults_t   filtResults;        /*!< Results from PDU filtering. */
} BbBleMstAuxAdvEvent_t;

/*! \brief       Master periodic scanning event operation data (\ref BB_BLE_OP_MST_PER_SCAN_EVENT). */
typedef struct
{
  BbBlePerComp_t          rxPerAdvCback;      /*!< Periodic scanning completion callback. */
  BbBlePerPostComp_t      rxPerAdvPostCback;  /*!< Periodic scanning completion post callback. */
  uint32_t                rxSyncDelayUsec;    /*!< Receive timeout in microseconds. */

  /* Return parameters. */
  uint8_t                 rxStatus;           /*!< RX status. */
  int8_t                  perAdvRssi;         /*!< RSSI of advertisement. */
  uint8_t                 perRxPhyOptions;    /*!< Rx PHY options. */
  uint32_t                perAdvCrc;          /*!< CRC of advertisement. */
  uint32_t                perStartTsUsec;     /*!< Start of periodic advertising packet timestamp in microseconds. */
  bool_t                  perIsFirstTs;       /*!< True if it is the first timestamp for a serial of periodic packets. */
} BbBleMstPerScanEvent_t;

/*! \brief       Slave auxiliary advertising event operation data (\ref BB_BLE_OP_SLV_AUX_ADV_EVENT). */
typedef struct
{
  /* TODO BbBleSlvAuxAdvEvent_t hide buffer descriptors in BB layer. */
  PalBbBleTxBufDesc_t     txAuxAdvPdu[2];     /*!< Advertising PDU descriptor. */
  uint8_t                 *pRxAuxReqBuf;      /*!< Auxiliary request buffer (must be size of BB_REQ_PDU_MAX_LEN). */
  PalBbBleTxBufDesc_t     txAuxRspPdu[2];     /*!< Response PDU descriptor. */
  PalBbBleTxBufDesc_t     txAuxChainPdu[2];   /*!< Auxiliary chain PDU descriptor. */

  BbBleAdvComp_t          rxAuxReqCback;      /*!< Auxiliary request receive completion callback. */
  BbBleAdvPost_t          rxAuxReqPostCback;  /*!< Auxiliary scan/connect request receive post processing callback. */
  BbBleTxAuxSetup_t       txAuxSetupCback;    /*!< Auxiliary chain indication setup callback. */

  uint8_t                 auxAdvCh;           /*!< Advertising channel map. */

  /* Return parameters. */
  uint8_t                 auxRxPhyOptions;    /*!< Rx PHY options. */
  uint32_t                auxReqStartTsUsec;  /*!< Start of request packet timestamp in microseconds. */

  /* Filter results. */
  bbBlePduFiltResults_t   filtResults;        /*!< Results from PDU filtering. */
} BbBleSlvAuxAdvEvent_t;

/*! \brief      Connection event operation data (\ref BB_BLE_OP_MST_CONN_EVENT). */
typedef struct
{
  BbBleExec_t             execCback;          /*!< Execute callback. */
  BbBleCancel_t           cancelCback;        /*!< Cancel callback. */
  BbBleTxDataComp_t       txDataCback;        /*!< Transmit completion callback. */
  BbBleRxDataComp_t       rxDataCback;        /*!< Receive completion callback. */

  /* Return parameters. */
  int8_t                  rssi;               /*!< RSSI of the last received packet. */
  uint8_t                 rxPhyOptions;       /*!< Rx PHY options. */
} BbBleMstConnEvent_t;

/*! \brief      Connection event operation data (\ref BB_BLE_OP_SLV_CONN_EVENT). */
typedef struct
{
  uint32_t                rxSyncDelayUsec;    /*!< Receive timeout in microseconds. */
  BbBleExec_t             execCback;          /*!< Execute callback. */
  BbBleCancel_t           cancelCback;        /*!< Cancel callback. */
  BbBleTxDataComp_t       txDataCback;        /*!< Transmit completion callback. */
  BbBleRxDataComp_t       rxDataCback;        /*!< Receive completion callback. */

  /* Return parameters. */
  uint32_t                startTsUsec;        /*!< Start timestamp of the first received packet in microseconds. */
  int8_t                  rssi;               /*!< RSSI of the last received packet. */
  uint8_t                 rxPhyOptions;       /*!< Rx PHY options. */
} BbBleSlvConnEvent_t;

/*! \brief      CIS master event operation data (\ref BB_BLE_OP_MST_CIS_EVENT). */
typedef struct
{
  BbBleCisCheckContOp_t   checkContOpCback;     /*!< Check whether to continue current operation callback. */
  BbBleCisCheckContOp_t   checkContOpPostCback; /*!< Check whether to continue current operation callback. */
  BbBleExec_t             execCback;            /*!< Execute callback. */
  BbBleExec_t             contExecCback;        /*!< Continue execute callback. */
  BbBleCisPostExec_t      postSubEvtCback;      /*!< Post subevent callback. */
  BbBleCancel_t           cancelCback;          /*!< Cancel callback. */
  BbBleTxDataComp_t       txDataCback;          /*!< Transmit completion callback. */
  BbBleCisRxDataComp_t    rxDataCback;          /*!< Receive completion callback. */
  /* Return parameters. */
  int8_t                  rssi;               /*!< RSSI of the last received packet. */
  uint8_t                 rxPhyOptions;       /*!< Rx PHY options. */
} BbBleMstCisEvent_t;

/*! \brief      CIS slave event operation data (\ref BB_BLE_OP_SLV_CIS_EVENT). */
typedef struct
{
  BbBleCisCheckContOp_t   checkContOpCback;     /*!< Check whether to continue current operation callback. */
  BbBleCisCheckContOp_t   checkContOpPostCback; /*!< Check whether to continue current operation callback. */
  BbBleExec_t             execCback;            /*!< Execute callback. */
  BbBleExec_t             contExecCback;        /*!< Continue execute callback. */
  BbBleCisPostExec_t      postSubEvtCback;      /*!< Post subevent callback. */
  BbBleCancel_t           cancelCback;          /*!< Cancel callback. */
  BbBleTxDataComp_t       txDataCback;          /*!< Transmit completion callback. */
  BbBleRxDataComp_t       rxDataCback;          /*!< Receive completion callback. */

  /* Return parameters. */
  bool_t                  isFirstTs;          /*!< True for the first timestamp. */
  uint32_t                startTsUsec;        /*!< Start timestamp of the first received packet in microseconds. */
  uint32_t                rxTsUsec;           /*!< Timestamp of the received packet. in microseconds */
  int8_t                  rssi;               /*!< RSSI of the last received packet. */
  uint8_t                 rxPhyOptions;       /*!< Rx PHY options. */
  uint32_t                rxSyncDelayUsec;    /*!< Receive timeout in microseconds. */
} BbBleSlvCisEvent_t;

/*! \brief      BIS master event operation data (\ref BB_BLE_OP_MST_BIS_EVENT). */
typedef struct
{
  uint32_t                rxSyncDelayUsec;    /*!< Receive timeout in microseconds. */
  BbBleExec_t             execCback;          /*!< Execute callback. */
  BbBleRxDataComp_t       rxDataCback;        /*!< Recieve completion callback. */

  /* Return parameters. */
  uint32_t                startTsUsec;        /*!< First Rx timestamp. */
} BbBleMstBisEvent_t;

/*! \brief      BIS slave event operation data (\ref BB_BLE_OP_SLV_BIS_EVENT). */
typedef struct
{
  BbBleExec_t             execCback;          /*!< Execute callback. */
  BbBleTxDataComp_t       txDataCback;        /*!< Transmit completion callback. */

  /* Return parameters. */
  /* None */
} BbBleSlvBisEvent_t;

/*! \brief      Continuous transmit operation data (\ref BB_BLE_OP_TEST_TX). */
typedef struct
{
  BbBleTestComp_t         testCback;          /*!< Test callback. */

  uint8_t                 *pTxBuf;            /*!< Transmit data buffer. */
  uint16_t                txLen;              /*!< Transmit data buffer length. */
  uint16_t                pktInterUsec;       /*!< Transmit packet interval. */
} BbBleTestTx_t;

/*! \brief      Continuous receive operation data (\ref BB_BLE_OP_TEST_RX). */
typedef struct
{
  uint32_t                rxSyncDelayUsec;    /*!< Synchronization delay in microseconds. */
  BbBleTestComp_t         testCback;          /*!< Test callback. */

  uint8_t                 *pRxBuf;            /*!< Receive data buffer. */
  uint16_t                rxLen;              /*!< Receive data buffer length. */
} BbBleTestRx_t;

/*! \brief      Bluetooth Low Energy protocol specific operation parameters. */
typedef struct BbBleData_tag
{
  PalBbBleChan_t          chan;               /*!< Channelization parameters. */
  bbBlePduFiltParams_t    pduFilt;            /*!< PDU filter parameters. */

  union
  {
    BbBleMstAdvEvent_t    mstAdv;             /*!< Master advertising event data. */
    BbBleSlvAdvEvent_t    slvAdv;             /*!< Slave advertising event data. */
    BbBleMstAuxAdvEvent_t mstAuxAdv;          /*!< Master auxiliary advertising event data. */
    BbBleSlvAuxAdvEvent_t slvAuxAdv;          /*!< Slave auxiliary advertising event data. */
    BbBleSlvAuxAdvEvent_t slvPerAdv;          /*!< Slave periodic advertising event data. */
    BbBleMstConnEvent_t   mstConn;            /*!< Master connection event data. */
    BbBleSlvConnEvent_t   slvConn;            /*!< Slave connection event data. */
    BbBleMstPerScanEvent_t mstPerScan;        /*!< Master periodic scanning event data. */
    BbBleMstCisEvent_t    mstCis;             /*!< Master CIS event data. */
    BbBleSlvCisEvent_t    slvCis;             /*!< Slave CIS event data. */
    BbBleMstBisEvent_t    mstBis;             /*!< Master BIS event data. */
    BbBleSlvBisEvent_t    slvBis;             /*!< Slave BIS event data. */
    BbBleTestTx_t         testTx;             /*!< Transmit test data. */
    BbBleTestRx_t         testRx;             /*!< Receive test data. */
  } op;                                       /*!< Operation specific data. */
} BbBleData_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Transmit data PDU at next transmit slot.
 *
 *  \param      descs       Array of transmit buffer descriptor.
 *  \param      cnt         Number of descriptors.
 *
 *  \note       This function is expected to be called during the call context of
 *              \ref BbBleSlvConnEvent_t::rxDataCback callback routine.
 */
/*************************************************************************************************/
void BbBleTxData(PalBbBleTxBufDesc_t descs[], uint8_t cnt);

/*************************************************************************************************/
/*!
 *  \brief      Set receive data buffer for next receive slot.
 *
 *  \param      pBuf        Receive data buffer.
 *  \param      len         Maximum length of data buffer.
 *
 *  \note       This function is expected to be called during the call context of
 *              \ref BbBleSlvConnEvent_t::rxDataCback callback routine.
 *
 *  \note       BB must always call the BbSlvConnEvent_t::rxDataCback callback routine of the
 *              currently executing BOD with the given buffer.
 */
/*************************************************************************************************/
void BbBleRxData(uint8_t *pBuf, uint16_t len);

/*************************************************************************************************/
/*!
 *  \brief      Transmit CIS PDU at next transmit slot.
 *
 *  \param      descs       Array of transmit buffer descriptor.
 *  \param      cnt         Number of descriptors.
 *
 *  \note       This function is expected to be called during the call context of
 *              \ref BbBleMstCisEvent_t::rxDataCback or \ref BbBleSlvCisEvent_t::rxDataCback
 *              callback routine.
 */
/*************************************************************************************************/
void BbBleCisTxData(PalBbBleTxBufDesc_t descs[], uint8_t cnt);

/*************************************************************************************************/
/*!
 *  \brief      Set receive data buffer for next receive slot.
 *
 *  \param      pBuf        Receive data buffer.
 *  \param      len         Maximum length of data buffer.
 *
 *  \note       This function is expected to be called during the call context of
 *              \ref BbBleSlvCisEvent_t::rxDataCback callback routine.
 *
 *  \note       BB must always call the BbBleSlvCisEvent_t::rxDataCback callback routine of the
 *              currently executing BOD with the given buffer.
 */
/*************************************************************************************************/
void BbBleCisRxData(uint8_t *pBuf, uint16_t len);

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
void BbBleBisTxData(PalBbBleTxBufDesc_t descs[], uint8_t cnt, uint32_t nextPduTime, PalBbBleChan_t *pNextChan);

/*************************************************************************************************/
/*!
 *  \brief      Set receive BIS Data PDU buffer for next receive slot.
 *
 *  \param      pBuf        Receive data buffer.
 *  \param      len         Maximum length of data buffer.
 *  \param      nextPduTime Next PDU time.
 *  \param      pNextChan   Next PDU channel.
 *  \param      reAcq       Rx train re-acquisition required.
 */
/*************************************************************************************************/
void BbBleBisRxData(uint8_t *pBuf, uint16_t len, uint32_t nextPduTime, PalBbBleChan_t *pNextChan, bool_t reAcq);

/*************************************************************************************************/
/*!
 *  \brief      Receive data re-acquisition.
 *
 *  \param      syncTime    Due time for the next Rx operation.
 *  \param      pChan       Channel parameters.
 *
 *  Update due time for next Rx operation. Called after a missed Rx operation for re-acquisition
 *  of the receive train.
 */
/*************************************************************************************************/
void BbBleBisRxDataReAcq(uint32_t syncTime, PalBbBleChan_t *pChan);

/*************************************************************************************************/
/*!
 *  \brief      Execute auxiliary scanning master BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      pBle    BLE operation parameters.
 */
/*************************************************************************************************/
void BbMstExecuteLinkedAuxScanOp(BbOpDesc_t *pBod, BbBleData_t *pBle);

/*************************************************************************************************/
/*!
 *  \brief      Tx completion for auxiliary scanning master operation.
 *
 *  \param      status      Completion status.
 *  \param      pBod        Pointer to the BOD
 *
 *  Setup for next action in the operation or complete the operation.
 *
 *  \return     TRUE if BOD is complete, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t BbMstAuxScanTxCompHandler(BbOpDesc_t * const pBod, uint8_t status);

/*************************************************************************************************/
/*!
 *  \brief      Rx completion for auxiliary scanning master operation.
 *
 *  \param      pCur            Pointer to the BOD
 *  \param      status          Reception status.
 *  \param      rssi            RSSI value.
 *  \param      crc             CRC value.
 *  \param      timestamp       Start of packet timestamp in microseconds.
 *  \param      rxPhyOptions    Rx PHY options.
 *
 *  Setup for next action in the operation or complete the operation.
 *
 *  \return     TRUE if BOD is complete, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t BbMstAuxScanRxCompHandler(BbOpDesc_t * const pCur, uint8_t status, int8_t rssi, uint32_t crc,
                                 uint32_t timestamp, uint8_t rxPhyOptions);

/*! \} */    /* BB_API_BLE */

#ifdef __cplusplus
};
#endif

#endif /* BB_BLE_API_OP_H */
