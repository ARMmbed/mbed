/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      BLE baseband interface file.
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

#ifndef BB_BLE_API_H
#define BB_BLE_API_H

#include "wsf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup BB_API_BLE
 *  \{ */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Advertising packet statistics. */
typedef struct
{
  uint32_t  txAdv;                  /*!< Number of sent advertising packets. */
  uint32_t  rxReq;                  /*!< Number of successfully received advertising requests. */
  uint32_t  rxReqCrc;               /*!< Number of received advertising requests with CRC errors. */
  uint32_t  rxReqTimeout;           /*!< Number of timed out received advertising requests (receive timeout). */
  uint32_t  txRsp;                  /*!< Number of sent response packets. */
  uint32_t  errAdv;                 /*!< Number of advertising transaction errors. */
  uint16_t  rxSetupUsec;            /*!< Rx packet setup watermark in microseconds. */
  uint16_t  txSetupUsec;            /*!< Tx packet setup watermark in microseconds. */
  uint16_t  rxIsrUsec;              /*!< Rx ISR processing watermark in microseconds. */
  uint16_t  txIsrUsec;              /*!< Tx ISR processing watermark in microseconds. */
} BbBleAdvPktStats_t;

/*! \brief      Auxiliary advertising packet statistics. */
typedef struct
{
  uint32_t  txAdv;                  /*!< Number of sent advertising packets. */
  uint32_t  rxReq;                  /*!< Number of successfully received advertising requests. */
  uint32_t  rxReqCrc;               /*!< Number of received advertising requests with CRC errors. */
  uint32_t  rxReqTimeout;           /*!< Number of timed out received advertising requests (receive timeout). */
  uint32_t  txRsp;                  /*!< Number of sent response packets. */
  uint32_t  txChain;                /*!< Number of sent chain packets. */
  uint32_t  errAdv;                 /*!< Number of advertising transaction errors. */
  uint16_t  rxSetupUsec;            /*!< Rx packet setup watermark in microseconds. */
  uint16_t  txSetupUsec;            /*!< Tx packet setup watermark in microseconds. */
  uint16_t  rxIsrUsec;              /*!< Rx ISR processing watermark in microseconds. */
  uint16_t  txIsrUsec;              /*!< Tx ISR processing watermark in microseconds. */
} BbBleAuxAdvPktStats_t;

/*! \brief      Scan packet statistics. */
typedef struct
{
  uint32_t  rxAdv;                  /*!< Number of successfully received advertising packets. */
  uint32_t  rxAdvCrc;               /*!< Number of received advertising packets with CRC errors. */
  uint32_t  rxAdvTimeout;           /*!< Number of timed out advertising packets (receive timeout). */
  uint32_t  txReq;                  /*!< Number of sent advertising requests. */
  uint32_t  rxRsp;                  /*!< Number of successfully received advertising response packets. */
  uint32_t  rxRspCrc;               /*!< Number of received advertising response packets with CRC errors. */
  uint32_t  rxRspTimeout;           /*!< Number of timed out advertising response packets (receive timeout). */
  uint32_t  errScan;                /*!< Number of scan transaction errors. */
  uint16_t  rxSetupUsec;            /*!< Rx packet setup watermark in microseconds. */
  uint16_t  txSetupUsec;            /*!< Tx packet setup watermark in microseconds. */
  uint16_t  rxIsrUsec;              /*!< Rx ISR processing watermark in microseconds. */
  uint16_t  txIsrUsec;              /*!< Tx ISR processing watermark in microseconds. */
} BbBleScanPktStats_t;

/*! \brief      Scan packet statistics. */
typedef struct
{
  uint32_t  rxAdv;                  /*!< Number of successfully received advertising packets. */
  uint32_t  rxAdvCrc;               /*!< Number of received advertising packets with CRC errors. */
  uint32_t  rxAdvTimeout;           /*!< Number of timed out advertising packets (receive timeout). */
  uint32_t  txReq;                  /*!< Number of sent advertising requests. */
  uint32_t  rxRsp;                  /*!< Number of successfully received advertising response packets. */
  uint32_t  rxRspCrc;               /*!< Number of received advertising response packets with CRC errors. */
  uint32_t  rxRspTimeout;           /*!< Number of timed out advertising response packets (receive timeout). */
  uint32_t  rxChain;                /*!< Number of successfully received chain packets. */
  uint32_t  rxChainCrc;             /*!< Number of received chain packets with CRC errors. */
  uint32_t  rxChainTimeout;         /*!< Number of timed out chain packets (receive timeout). */
  uint32_t  errScan;                /*!< Number of scan transaction errors. */
  uint16_t  rxSetupUsec;            /*!< Rx packet setup watermark in microseconds. */
  uint16_t  txSetupUsec;            /*!< Tx packet setup watermark in microseconds. */
  uint16_t  rxIsrUsec;              /*!< Rx ISR processing watermark in microseconds. */
  uint16_t  txIsrUsec;              /*!< Tx ISR processing watermark in microseconds. */
} BbBleAuxScanPktStats_t;

/*! \brief      Periodic scan packet statistics. */
typedef struct
{
  uint32_t  rxAdv;                  /*!< Number of successfully received advertising packets. */
  uint32_t  rxAdvCrc;               /*!< Number of received advertising packets with CRC errors. */
  uint32_t  rxAdvTimeout;           /*!< Number of timed out advertising packets (receive timeout). */
  uint32_t  rxChain;                /*!< Number of successfully received chain packets. */
  uint32_t  rxChainCrc;             /*!< Number of received chain packets with CRC errors. */
  uint32_t  rxChainTimeout;         /*!< Number of timed out chain packets (receive timeout). */
  uint32_t  errScan;                /*!< Number of scan transaction errors. */
  uint16_t  rxSetupUsec;            /*!< Rx packet setup watermark in microseconds. */
  uint16_t  txSetupUsec;            /*!< Tx packet setup watermark in microseconds. */
  uint16_t  rxIsrUsec;              /*!< Rx ISR processing watermark in microseconds. */
  uint16_t  txIsrUsec;              /*!< Tx ISR processing watermark in microseconds. */
} BbBlePerScanPktStats_t;

/*! \brief      Data packet statistics. */
typedef struct
{
  uint32_t  rxData;                 /*!< Number of successfully received data packets. */
  uint32_t  rxDataCrc;              /*!< Number of received data packets with CRC errors. */
  uint32_t  rxDataTimeout;          /*!< Number of timed out data packets (receive timeout). */
  uint32_t  txData;                 /*!< Number of sent data packets. */
  uint32_t  errData;                /*!< Number of data transaction errors. */
  uint16_t  rxSetupUsec;            /*!< Rx packet setup watermark in microseconds. */
  uint16_t  txSetupUsec;            /*!< Tx packet setup watermark in microseconds. */
  uint16_t  rxIsrUsec;              /*!< Rx ISR processing watermark in microseconds. */
  uint16_t  txIsrUsec;              /*!< Tx ISR processing watermark in microseconds. */
} BbBleDataPktStats_t;

/*! \brief      PDU filtering statistics. */
typedef struct
{
  uint16_t  failPduTypeFilt;        /*!< Number of PDUs failing PDU type filter. */
  uint16_t  passPduTypeFilt;        /*!< Number of PDUs passing PDU type filter. */
  uint16_t  failWlFilt;             /*!< Number of PDUs failing whitelist filter. */
  uint16_t  passWlFilt;             /*!< Number of PDUs passing whitelist filter. */
  uint16_t  failPeerAddrMatch;      /*!< Number of PDUS failing peer address match. */
  uint16_t  passPeerAddrMatch;      /*!< Number of PDUs passing peer address match. */
  uint16_t  failLocalAddrMatch;     /*!< Number of PDUS failing local address match. */
  uint16_t  passLocalAddrMatch;     /*!< Number of PDUs passing local address match. */
  uint16_t  failPeerRpaVerify;      /*!< Number of peer RPAs failing verification. */
  uint16_t  passPeerRpaVerify;      /*!< Number of peer RPAs passing verification. */
  uint16_t  failLocalRpaVerify;     /*!< Number of local RPAs failing verification. */
  uint16_t  passLocalRpaVerify;     /*!< Number of local RPAs passing verification. */
  uint16_t  failPeerPrivAddrReq;    /*!< Number of peer addresses failing requirement to be RPAs. */
  uint16_t  failLocalPrivAddrReq;   /*!< Number of local addresses failing requirement to be RPAs. */
  uint16_t  failPeerAddrResReq;     /*!< Number of PDUs failing required peer address resolution. */
  uint16_t  passPeerAddrResOpt;     /*!< Number of PDUs passing optional peer address resolution. */
  uint16_t  passLocalAddrResOpt;    /*!< Number of PDUs passing optional local address resolution. */
  uint16_t  peerResAddrPend;        /*!< Number of peer address resolutions pended. */
  uint16_t  localResAddrPend;       /*!< Number of local address resolutions pended. */
} BbBlePduFiltStats_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Initialize the BLE BB.
 *
 *  Initialize baseband resources.
 */
/*************************************************************************************************/
void BbBleInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for scanning master operations.
 *
 *  Update the operation table with scanning master operations routines.
 */
/*************************************************************************************************/
void BbBleScanMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for auxiliary scanning master operations.
 *
 *  Update the operation table with auxiliary scanning master operations routines.
 */
/*************************************************************************************************/
void BbBleAuxScanMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for periodic scanning master operations.
 *
 *  Update the operation table with periodic scanning master operations routines.
 */
/*************************************************************************************************/
void BbBlePerScanMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for connectable master operations.
 *
 *  Update the operation table with connectable master operations routines.
 */
/*************************************************************************************************/
void BbBleConnMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for advertising slave operations.
 *
 *  Update the operation table with advertising slave operations routines.
 */
/*************************************************************************************************/
void BbBleAdvSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for auxiliary advertising slave operations.
 *
 *  Update the operation table with auxiliary advertising slave operations routines.
 */
/*************************************************************************************************/
void BbBleAuxAdvSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for connectable slave operations.
 *
 *  Update the operation table with connectable slave operations routines.
 */
/*************************************************************************************************/
void BbBleConnSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for test operations.
 *
 *  Update the operation table with test operations routines.
 */
/*************************************************************************************************/
void BbBleTestInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize white list.
 *
 *  \param      numEntries      Number of white list entries to provide.
 *  \param      pFreeMem        Pointer to free memory.
 *  \param      freeMemSize     Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 *
 *  This function initializes the white list.
 *
 *  \note       This function must be called once before initializing the LL.
 */
/*************************************************************************************************/
uint16_t BbBleInitWhiteList(uint8_t numEntries, uint8_t *pFreeMem, uint32_t freeMemSize);

/*************************************************************************************************/
/*!
 *  \brief      Initialize resolving list.
 *
 *  \param      numEntries      Number of resolving list entries to provide.
 *  \param      pFreeMem        Pointer to free memory.
 *  \param      freeMemSize     Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 *
 *  This function initializes the resolving list.
 *
 *  \note       This function must be called once before initializing the LL.
 */
/*************************************************************************************************/
uint16_t BbBleInitResolvingList(uint8_t numEntries, uint8_t *pFreeMem, uint32_t freeMemSize);

/*************************************************************************************************/
/*!
 *  \brief      Initialize periodic list.
 *
 *  \param      numEntries      Number of periodic list entries to provide.
 *  \param      pFreeMem        Pointer to free memory.
 *  \param      freeMemSize     Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 *
 *  This function initializes the periodic list.
 *
 *  \note       This function must be called once before initializing the LL.
 */
/*************************************************************************************************/
uint16_t BbBleInitPeriodicList(uint8_t numEntries, uint8_t *pFreeMem, uint32_t freeMemSize);

/*************************************************************************************************/
/*!
 *  \brief      Get advertising packet statistics.
 *
 *  \param      pStats      Advertising statistics.
 */
/*************************************************************************************************/
void BbBleGetAdvStats(BbBleAdvPktStats_t *pStats);

/*************************************************************************************************/
/*!
 *  \brief      Get scan packet statistics.
 *
 *  \param      pStats      Scan statistics.
 */
/*************************************************************************************************/
void BbBleGetScanStats(BbBleScanPktStats_t *pStats);

/*************************************************************************************************/
/*!
 *  \brief      Get auxiliary advertising packet statistics.
 *
 *  \param      pStats      Auxiliary advertising statistics.
 */
/*************************************************************************************************/
void BbBleGetAuxAdvStats(BbBleAuxAdvPktStats_t *pStats);

/*************************************************************************************************/
/*!
 *  \brief      Get auxiliary scan packet statistics.
 *
 *  \param      pStats      Auxiliary scan statistics.
 */
/*************************************************************************************************/
void BbBleGetAuxScanStats(BbBleAuxScanPktStats_t *pStats);

/*************************************************************************************************/
/*!
 *  \brief      Get periodic scan packet statistics.
 *
 *  \param      pStats      Periodic scan statistics.
 */
/*************************************************************************************************/
void BbBleGetPerScanStats(BbBlePerScanPktStats_t *pStats);

/*************************************************************************************************/
/*!
 *  \brief      Get connection packet statistics.
 *
 *  \param      pStats      Connection data statistics.
 */
/*************************************************************************************************/
void BbBleGetConnStats(BbBleDataPktStats_t *pStats);

/*************************************************************************************************/
/*!
 *  \brief      Get CIS packet statistics.
 *
 *  \param      pStats      CIS data statistics.
 */
/*************************************************************************************************/
void BbBleGetCisStats(BbBleDataPktStats_t *pStats);

/*************************************************************************************************/
/*!
 *  \brief      Get BIS packet statistics.
 *
 *  \param      pStats      BIS data statistics.
 */
/*************************************************************************************************/
void BbBleGetBisStats(BbBleDataPktStats_t *pStats);

/*************************************************************************************************/
/*!
 *  \brief      Get test mode packet statistics.
 *
 *  \param      pStats      Test data statistics.
 */
/*************************************************************************************************/
void BbBleGetTestStats(BbBleDataPktStats_t *pStats);

/*************************************************************************************************/
/*!
 *  \brief      Get PDU filter statistics.
 *
 *  \param      pStats      PDU filter statistics.
 */
/*************************************************************************************************/
void BbBleGetPduFiltStats(BbBlePduFiltStats_t *pStats);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for connected isochronous stream master operations.
 *
 *  Update the operation table with CIS master operations routines.
 */
/*************************************************************************************************/
void BbBleCisMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for connected isochronous stream slave operations.
 *
 *  Update the operation table with CIS slave operations routines.
 */
/*************************************************************************************************/
void BbBleCisSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for Broadcast isochronous stream master operations.
 *
 *  Update the operation table with CIS master operations routines.
 */
/*************************************************************************************************/
void BbBleBisMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize for Broadcast isochronous stream slave operations.
 *
 *  Update the operation table with CIS slave operations routines.
 */
/*************************************************************************************************/
void BbBleBisSlaveInit(void);

/*! \} */    /* BB_API_BLE */

#ifdef __cplusplus
};
#endif

#endif /* BB_BLE_API_H */
