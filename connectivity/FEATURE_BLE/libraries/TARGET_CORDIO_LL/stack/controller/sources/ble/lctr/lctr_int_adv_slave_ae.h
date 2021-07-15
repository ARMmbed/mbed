/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Internal link layer controller slave extended advertising interface file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#ifndef LCTR_INT_ADV_SLAVE_AE_H
#define LCTR_INT_ADV_SLAVE_AE_H

#include "lctr_int_adv_ae.h"
#include "lctr_int.h"
#include "lctr_api_adv_slave_ae.h"
#include "lmgr_api_adv_slave_ae.h"
#include "bb_ble_api.h"
#include "bb_ble_api_op.h"
#include "lctr_pdu_adv.h"
#include "wsf_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Maximum value of the AuxPtr offset field. */
#define LCTR_AUX_PTR_MAX_OFFSET         0x1FFF

/*! \brief      Number of shifted bytes for Used PHY field from the AUX Offset. */
#define LCTR_AUX_OFFS_USED_PHY_SHIFT    13

/*! \brief      Extended Advertising packet length. */
#define LCTR_EXT_ADVB_LEN(ehLen, dLen)  (LL_ADV_HDR_LEN + ehLen + dLen)

/*! \brief      Extended header common field length. */
#define LCTR_EXT_HDR_CMN_LEN            1

/*! \brief      Extended header flags field length. */
#define LCTR_EXT_HDR_FLAG_LEN           1

/*! \brief      Maximum size of a single complete advertising data buffer. */
#define LCTR_COMP_EXT_ADV_DATA_MAX_LEN  251         /* TODO: cfg_mac_ble.h configuration */

/*! \brief      Resolve the extended advertising index from the context pointer. */
#define LCTR_GET_EXT_ADV_INDEX(pAdvSet)   (pAdvSet - &pLctrAdvSetTbl[0])

/*! \brief      Get reservation manager handle for periodic ADV from the context pointer. */
#define LCTR_GET_PER_RM_HANDLE(pAdvSet)   (LL_MAX_CONN + LCTR_GET_EXT_ADV_INDEX(pAdvSet))

/*! \brief       Call signature for slave ACAD BIG related function. */
typedef void (*lctrAcadBigHdlr_t)(uint8_t advHandle);

/*! \brief       Call signature for AdvSet Remove event handler. */
typedef void (*lctrRemoveHdlr_t)(uint8_t advHandle);

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*! \brief      Slave extended advertising states. */
enum
{
  LCTR_EXT_ADV_STATE_DISABLED,          /*!< Extended advertising disabled state. */
  LCTR_EXT_ADV_STATE_ENABLED,           /*!< Extended advertising enabled state. */
  LCTR_EXT_ADV_STATE_SHUTDOWN,          /*!< Extended advertising shutdown in progress. */
  LCTR_EXT_ADV_STATE_RESET,             /*!< Extended advertising reset in progress. */
  LCTR_EXT_ADV_STATE_TOTAL              /*!< Total number of extended advertising states. */
};

/*! \brief      Slave periodic advertising states. */
enum
{
  LCTR_PER_ADV_STATE_DISABLED,          /*!< Periodic advertising disabled state. */
  LCTR_PER_ADV_STATE_ENABLED,           /*!< Periodic advertising enabled state. */
  LCTR_PER_ADV_STATE_SHUTDOWN,          /*!< Periodic advertising shutdown in progress. */
  LCTR_PER_ADV_STATE_RESET,             /*!< Periodic advertising reset in progress. */
  LCTR_PER_ADV_STATE_TOTAL              /*!< Total number of Periodic advertising states. */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Extended advertising parameters. */
typedef struct
{
  uint16_t      advEventProp;           /*!< Advertising Event Properties. */
  uint32_t      priAdvInterMinUsec;     /*!< Primary Advertising Interval Minimum in microseconds. */
  uint32_t      priAdvInterMaxUsec;     /*!< Primary Advertising Interval Maximum in microseconds. */
  uint32_t      priAdvTermCntDownUsec;  /*!< Primary Advertising termination count down in microseconds. */
  uint64_t      peerAddr;               /*!< Peer Address. */
  uint8_t       priAdvChanMap;          /*!< Primary Advertising Channel Map. */
  uint8_t       ownAddrType;            /*!< Own Address Type. */
  uint8_t       peerAddrType;           /*!< Peer Address Type. */
  uint8_t       advFiltPolicy;          /*!< Advertising Filter Policy. */
  int8_t        advTxPwr;               /*!< Advertising Tx Power. */
  uint8_t       priAdvPhy;              /*!< Primary Advertising PHY. */
  uint8_t       secAdvMaxSkip;          /*!< Secondary Advertising Maximum Skip. */
  uint8_t       secAdvPhy;              /*!< Secondary Advertising PHY. */
  uint16_t      advDID;                 /*!< Advertising Data ID. */
  uint8_t       advSID;                 /*!< Advertising SID. */
  uint8_t       scanReqNotifEna;        /*!< Scan Request Notification Enable. */
} lctrExtAdvParam_t;

/*! \brief      Periodic advertising parameters. */
typedef struct
{
  uint8_t       perState;           /*!< Current periodic adv state. */
  bool_t        perAdvEnabled;      /*!< Periodic advertising enabled or not. */
  uint32_t      perAccessAddr;      /*!< Periodic advertising access address. */
  uint16_t      perEventCounter;    /*!< Periodic event counter. */
  bool_t        perAuxStart;        /*!< True if need to start aux BOD due to periodic adv. */

  BbOpDesc_t    perAdvBod;          /*!< Periodic advertising BOD. */
  BbBleData_t   perBleData;         /*!< Periodic advertising BLE data. */
  uint32_t      perOffsUsec;        /*!< Offset in microseconds to the next periodic PDU. */
  uint8_t       perChHopInc;        /*!< Periodic channel hop increment value. */
  uint8_t       perChIdx;           /*!< Periodic LL Channel. */
  bool_t        shutdown;           /*!< Client initiated shutdown flag. */
  uint32_t      perAdvInterUsec;    /*!< Periodic advertising interval in microseconds. */

  bool_t        enableAdi;          /*!< Enable ADI field in Periodic Advertising. */
  uint16_t      advDID;             /*!< Advertising Data ID. */

  uint32_t      advInterMinUsec;    /*!< Periodic Advertising Interval Minimum in microseconds. */
  uint32_t      advInterMaxUsec;    /*!< Periodic Advertising Interval Maximum in BB ticks. */
  uint16_t      advEventProp;       /*!< Periodic Advertising Event Properties. */
  bool_t        advParamReady;      /*!< Periodic Advertising Parameter is ready or not. */

  /* Channel parameters */
  lmgrChanParam_t   perChanParam;   /*!< Periodic Advertising Channel parameter. */
  uint64_t          updChanMask;    /*!< Last updated channel mask */
} lctrPerAdvParam_t;

/*! \brief      Advertising data buffer descriptor. */
typedef struct
{
  uint16_t    len;              /*!< Advertising data length. */
  uint8_t     *pBuf;            /*!< Advertising data buffer. */
  bool_t      ready;            /*!< Advertising data buffer complete. */
  uint16_t    txOffs;           /*!< Tx buffer offset. */
  uint8_t     fragPref;         /*!< Host fragmentation preference. */

  union
  {
    struct
    {
      bool_t    modified;       /*!< Content modified by host. */
      uint8_t   len;            /*!< Advertising data buffer length. */
      uint16_t  did;            /*!< Advertising Data ID. */
      uint8_t   buf[LCTR_COMP_EXT_ADV_DATA_MAX_LEN];
                                /*!< Advertising data buffer. */
      uint8_t   fragPref;       /*!< Host fragmentation preference. */
    } ext;                      /*!< Extended advertising data buffer cache (temporary storage during active advertising). */

    struct
    {
      bool_t    modified;       /*!< Content modified by host. */
      uint8_t   len;            /*!< Advertising data buffer length. */
      uint8_t   buf[LL_ADVBU_MAX_LEN];
                                /*!< Advertising data buffer. */
    } legacy;                   /*!< Legacy advertising data buffer. */
  } alt;                        /*!< Alternate data storage. */
} lctrAdvDataBuf_t;

/*! \brief      Advertising set. */
typedef struct
{
  bool_t      enabled;          /*!< Enable flag. */
  uint8_t     handle;           /*!< Advertising handle. */

  uint8_t     state;            /*!< Current state. */

  /* Host parameters */
  bool_t      bdAddrRndMod;     /*!< Modified random address. */
  bool_t      bdAddrRndValid;   /*!< Random device address valid. */
  uint64_t    bdAddrRnd;        /*!< Random device address. */
  uint64_t    advA;             /*!< Advertiser address. */
  uint64_t    tgtA;             /*!< Target address. */
  lctrExtAdvParam_t param;      /*!< Extended advertising parameters. */
  uint32_t    auxDelayUsec;     /*!< Auxiliary advertising event delay. */
  uint8_t     advDataFragLen;   /*!< Advertising data fragmentation length. */

  /* ACAD control block */
  lctrAcadParam_t acadParams[LCTR_ACAD_NUM_ID];  /*!< ACAD parameters. */

  /* Periodic advertising parameters */
  lctrPerAdvParam_t perParam;   /*!< Periodic advertising parameters. */

  /* Enable parameters */
  wsfTimer_t  tmrAdvDur;        /*!< Advertising duration timer. */
  uint8_t     maxEvents;        /*!< Maximum number of AE. */

  /* Advertising task context */
  uint8_t     numEvents;        /*!< Number of completed AE. */
  uint8_t     termReason;       /*!< Termination reason. */
  uint8_t     extHdrFlags;      /*!< Extended header flags. */
  uint8_t     *pExtAdvAuxPtr;   /*!< Extended advertising PDU buffer location of AuxPtr field. */
  uint8_t     auxChHopInc;      /*!< Auxiliary channel hop increment value. */
  uint32_t    auxSkipInterUsec;     /*!< Total skip time in microseconds. */

  /* Buffers */
  uint8_t     advHdrBuf[LCTR_EXT_ADVB_LEN(LL_EXT_ADV_HDR_MAX_LEN, 0)];
                                /*!< Primary channel legacy advertising and extended advertising header buffer. */
  uint8_t     scanRspHdrBuf[LL_ADVB_MAX_LEN];
                                /*!< Primary channel legacy scan response buffer. */
  uint8_t     auxAdvHdrBuf[LCTR_EXT_ADVB_LEN(LL_EXT_ADV_HDR_MAX_LEN, 0)];
                                /*!< Auxiliary extended advertising header buffer. */
  uint8_t     auxRspHdrBuf[LCTR_EXT_ADVB_LEN(LL_EXT_ADV_HDR_MAX_LEN, 0)];
                                /*!< Auxiliary scan or connect response header buffer. */
  uint8_t     perAdvHdrBuf[LCTR_EXT_ADVB_LEN(LL_EXT_ADV_HDR_MAX_LEN, 0)];
                                /*!< Periodic advertising header buffer. */
  lctrAdvDataBuf_t advData;     /*!< Advertising data buffer. */
  lctrAdvDataBuf_t scanRspData; /*!< Scan response data buffer. */
  lctrAdvDataBuf_t perAdvData;  /*!< Periodic advertising data buffer. */

  /* Connection context. */
  bool_t      isAuxConnReq;     /*!< True if AUX_CONN_REQ is received, False if CONN_IND is received. */
  uint32_t    connIndEndTsUsec; /*!< Connection indication end timestamp in microseconds. */
  bool_t      connIndRcvd;      /*!< Connection request received flag. */
  uint8_t     usedChSel;        /*!< Used channel selection. */

  /* BB/ISR context */
  bool_t      shutdown;         /*!< Client initiated shutdown flag. */
  BbOpDesc_t  advBod;           /*!< Advertising BOD. */
  BbBleData_t bleData;          /*!< BLE data. */
  BbOpDesc_t  auxAdvBod;        /*!< Auxiliary advertising BOD. */
  BbBleData_t auxBleData;       /*!< Auxiliary BLE data. */
  uint32_t    auxOffsUsec;      /*!< Offset in microseconds to the next auxiliary PDU. */
  uint8_t     auxChIdx;         /*!< AUX LL Channel. */
  bool_t      auxBodUsed;       /*!< Auxiliary BOD in use flag. */
  bool_t      didPerUpdate;     /*!< Data ID update due to periodic enable or disable. */
  bool_t      advBodAbort;      /*!< TRUE if extended advertising BOD is aborted. */
  lctrAdvbPduHdr_t  rspPduHdr;  /*!< Response PDU header. */

  /* BIG */
  lctrAcadBigHdlr_t  bigCreated;    /*!< Function pointer to the BIG created action function. */
  lctrAcadBigHdlr_t  bigTerminated; /*!< Function pointer to the BIG terminated action function. */
  lctrRemoveHdlr_t   removeCback;   /*!< Function pointer to the AdvSet remove action function. */
} lctrAdvSet_t;

/*! \brief      Slave extended advertising state context. */
typedef struct
{
  wsfQueue_t  rxScanReqQ;        /*!< Received SCAN_REQ queue. */
} lctrSlvExtAdvCtx_t;

/**************************************************************************************************
  Globals
**************************************************************************************************/

extern lctrAdvSet_t *pLctrAdvSetTbl;
extern LctrExtAdvMsg_t *pLctrSlvExtAdvMsg;
extern lctrSlvExtAdvCtx_t lctrSlvExtAdv;
extern LctrPerAdvMsg_t *pLctrSlvPerAdvMsg;
extern lctrAcadSlvMsg_t *pLctrAcadSlvMsg;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Context */
void lctrFreeAdvSet(lctrAdvSet_t *pAdvSet);
lctrAdvSet_t *lctrFindAdvSet(uint8_t handle);

/* Address selection */
void lctrChooseSetAdvA(lctrAdvbPduHdr_t *pPduHdr, BbBleData_t * const pBle, lctrAdvSet_t *pAdvSet);
void lctrChooseSetPeerA(lctrAdvbPduHdr_t *pPduHdr, BbBleData_t * const pBle, lctrAdvSet_t *pAdvSet);

/* Builder */
uint8_t lctrSlvExtAdvBuildOp(lctrAdvSet_t *pAdvSet, uint32_t maxStartMs);
void lctrSlvAuxNonConnNonScanBuildOp(lctrAdvSet_t *pAdvSet);
void lctrSlvAuxScanBuildOp(lctrAdvSet_t *pAdvSet);
void lctrSlvAuxConnBuildOp(lctrAdvSet_t *pAdvSet);
void lctrSlvAuxRescheduleOp(lctrAdvSet_t *pAdvSet, BbOpDesc_t * const pOp);
uint8_t lctrSlvPeriodicAdvBuildOp(lctrAdvSet_t *pAdvSet);

/* Channel */
void lctrSelectNextAuxChannel(lctrAdvSet_t *pAdvSet);
void lctrSelectNextPerChannel(lctrAdvSet_t *pAdvSet);

/* ISR: Packet handlers */
void lctrSlvTxSetupExtAdvHandler(BbOpDesc_t *pOp, uint32_t txTime);
uint32_t lctrSlvTxSetupAuxAdvDataHandler(BbOpDesc_t *pOp, bool_t isChainInd);
uint32_t lctrSlvTxSetupAuxScanRspDataHandler(BbOpDesc_t *pOp, bool_t isChainInd);
bool_t lctrSlvRxAuxScanReqHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf);
void lctrSlvRxAuxScanReqPostProcessHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf);
void lctrSlvRxLegacyScanReqPostProcessHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf);
bool_t lctrSlvRxAuxConnReqHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf);
bool_t lctrSlvRxLegacyReqHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf);
void lctrSlvRxLegacyReqPostProcessHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf);
uint32_t lctrSlvTxSetupPeriodicAdvDataHandler(BbOpDesc_t *pOp, bool_t isChainInd);
void lctrSlvAcadHandler(lctrAdvSet_t *pAdvSet);
void lctrSlvAcadDisable(lctrAcadParam_t *pAcadParam);

/* ISR: BOD handlers */
void lctrSlvExtAdvEndOp(BbOpDesc_t *pOp);
void lctrSlvExtAdvAbortOp(BbOpDesc_t *pOp);
void lctrSlvAuxAdvEndOp(BbOpDesc_t *pOp);
void lctrSlvPeriodicAdvEndOp(BbOpDesc_t *pOp);
void lctrSlvPeriodicAdvAbortOp(BbOpDesc_t *pOp);

/* Action routines */
void lctrExtAdvActStart(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActSelfStart(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActRestart(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActShutdown(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActResetShutdown(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActAdvCnf(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActDisallowAdvCnf(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActSelfTerm(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActAdvTerm(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActReset(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActResetTerm(lctrAdvSet_t *pAdvSet);
void lctrExtAdvActDurationExpired(lctrAdvSet_t *pAdvSet);
void lctrPeriodicAdvActStart(lctrAdvSet_t *pAdvSet);
void lctrPeriodicAdvActUpdate(lctrAdvSet_t *pAdvSet);
void lctrPeriodicAdvActDisallowAdvCnf(lctrAdvSet_t *pAdvSet);
void lctrPeriodicAdvActShutdown(lctrAdvSet_t *pAdvSet);
void lctrPeriodicAdvActResetTerm(lctrAdvSet_t *pAdvSet);
void lctrPeriodicAdvActAdvTerm(lctrAdvSet_t *pAdvSet);
void lctrSlvAcadActChanMapUpdateStart(lctrAdvSet_t *pAdvSet);
void lctrSlvAcadActChanMapUpdateFinish(lctrAdvSet_t *pAdvSet);
void lctrSlvAcadActBigCreated(lctrAdvSet_t *pAdvSet);
void lctrSlvAcadActBigTerminated(lctrAdvSet_t *pAdvSet);

/* Reservation */
uint32_t lctrGetPerRefTime(uint8_t perHandle, uint32_t *pDurUsec);

/* State machine */
void lctrSlvExtAdvExecuteSm(lctrAdvSet_t *pAdvSet, uint8_t event);
void lctrSlvPeriodicAdvExecuteSm(lctrAdvSet_t *pAdvSet, uint8_t event);
void lctrSlvAcadExecuteSm(lctrAdvSet_t *pAdvSet, uint8_t event);

/* Messaging */
void lctrSendAdvSetMsg(lctrAdvSet_t *pAdvSet, uint8_t event);
void lctrSendPeriodicAdvSetMsg(lctrAdvSet_t *pAdvSet, uint8_t event);

#ifdef __cplusplus
};
#endif

#endif /* LCTR_INT_ADV_SLAVE_AE_H */
