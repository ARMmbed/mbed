/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Internal link layer controller extended scanning master interface file.
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

#ifndef LCTR_INT_ADV_MASTER_AE_H
#define LCTR_INT_ADV_MASTER_AE_H

#include "lctr_int_adv_ae.h"
#include "lctr_int.h"
#include "lctr_api_adv_master_ae.h"
#include "lctr_int_adv_master.h"
#include "lctr_pdu_adv_ae.h"
#include "bb_ble_api.h"
#include "ll_defs.h"
#include "wsf_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Resolve the extended scan handle from the context pointer. */
#define LCTR_GET_EXT_SCAN_HANDLE(pCtx)  (pCtx->handle)

/*! \brief      Resolve the extended scan context from the handle. */
#define LCTR_GET_EXT_SCAN_CTX(h)        (lctrMstExtScanTbl[h])

/*! \brief      Resolve the periodic scanning handle from the context pointer. */
#define LCTR_GET_PER_SCAN_HANDLE(pCtx)  (pCtx - lctrMstPerScanTbl)

/*! \brief      Get topology manager handle from the periodic scanning context pointer. */
#define LCTR_GET_PER_SCAN_TM_HANDLE(pCtx) (LL_MAX_CONN + LCTR_GET_PER_SCAN_HANDLE(pCtx))

/*! \brief      Resolve the periodic scanning context from the handle. */
#define LCTR_GET_PER_SCAN_CTX(h)        (&lctrMstPerScanTbl[h])

/*! \brief      Valid active scan mask. */
#define LCTR_VALID_ACTIVE_SCAN_MASK     ((1 << LCTR_SCAN_PHY_1M) |  (1 << LCTR_SCAN_PHY_CODED))

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*! \brief      Master scan states. */
enum
{
  LCTR_EXT_SCAN_STATE_DISABLED,         /*!< Scan disabled state. */
  LCTR_EXT_SCAN_STATE_DISCOVER,         /*!< Scan enabled state. */
  LCTR_EXT_SCAN_STATE_SHUTDOWN,         /*!< Scan shutdown in progress. */
  LCTR_EXT_SCAN_STATE_RESET,            /*!< Scan reset in progress. */
  LCTR_EXT_SCAN_STATE_TOTAL             /*!< Total number of scan states. */
};

/*! \brief      Create sync states. */
enum
{
  LCTR_CREATE_SYNC_STATE_DISABLED,          /*!< Create sync disabled state. */
  LCTR_CREATE_SYNC_STATE_DISCOVER,          /*!< Create sync enabled state. */
  LCTR_CREATE_SYNC_STATE_SHUTDOWN,          /*!< Create sync shutdown in process state. */
  LCTR_CREATE_SYNC_STATE_RESET,             /*!< Create sync reset in progress. */
  LCTR_CREATE_SYNC_STATE_TOTAL              /*!< Total number of Create sync states. */
};

/*! \brief      Transfer sync states. */
enum
{
  LCTR_TRANSFER_SYNC_STATE_DISABLED = LCTR_CREATE_SYNC_STATE_DISABLED,      /*!< Transfer sync disabled state. */
  LCTR_TRANSFER_SYNC_STATE_DISCOVER = LCTR_CREATE_SYNC_STATE_DISCOVER,      /*!< Transfer sync enabled state. */
  LCTR_TRANSFER_SYNC_STATE_SHUTDOWN = LCTR_CREATE_SYNC_STATE_SHUTDOWN,      /*!< Transfer sync shutdown in process state. */
  LCTR_TRANSFER_SYNC_STATE_RESET    = LCTR_CREATE_SYNC_STATE_RESET,         /*!< Transfer sync reset in progress. */
  LCTR_TRANSFER_SYNC_STATE_TOTAL    = LCTR_CREATE_SYNC_STATE_TOTAL          /*!< Total number of Transfer sync states. */
};

/*! \brief      Periodic scanning states. */
enum
{
  LCTR_PER_SCAN_STATE_DISABLE,              /*!< Periodic scanning disabled state. */
  LCTR_PER_SCAN_STATE_SYNC_ESTD,            /*!< Periodic scanning sync established state. */
  LCTR_PER_SCAN_STATE_SYNC_TERMINATE,       /*!< Periodic scanning sync terminate in process state. */
  LCTR_PER_SCAN_STATE_RESET,                /*!< Periodic scanning sync reset in progress. */
  LCTR_PER_SCAN_STATE_TOTAL                 /*!< Total number of Periodic scanning states. */
};

/*! \brief      Internal common (non-context, non-broadcast) events. */
enum
{
  LCTR_EXT_SCAN_MSG_NON_SM = LCTR_EXT_SCAN_MSG_TOTAL,
  LCTR_EXT_SCAN_MSG_TMR_DUR_EXP,        /*!< Duration timer expired event. */
  LCTR_EXT_SCAN_MSG_TMR_PER_EXP         /*!< Period timer expired event. */
};

/*! \brief      Extended advertising report assembly state. */
typedef enum
{
  LCTR_RPT_STATE_IDLE,                  /*!< No report assembly in progress. */
  LCTR_RPT_STATE_IN_PROGRESS,           /*!< Report assembly in progress. */
  LCTR_RPT_STATE_COMP,                  /*!< Report assembly completed. */
} lctrRptState_t;

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Extended scanning context. */
typedef struct
{
  /* Scan state. */
  uint8_t           state;              /*!< Scan state. */
  bool_t            selfTerm;           /*!< Self-termination flag. */
  bool_t            shutdown;           /*!< Client initiated shutdown flag. */
  bool_t            bodAborted;         /*!< True if BOD was aborted by scheduler. */
  uint32_t          scanWinStartUsec;   /*!< Scan window origin in microseconds. */
  LlScanParam_t     scanParam;          /*!< Scan parameters. */
                                        /* N.B. Scan parameters must persist after initiate. */
  uint8_t           handle;             /*!< Scan handle. */
  union
  {
    struct
    {
      /* Report handling. */
      LlExtAdvReportInd_t advRpt;       /*!< Advertising report. */
      lctrRptState_t advRptState;       /*!< Advertising report state. */
      LlExtAdvReportInd_t auxAdvRpt;    /*!< Auxiliary Advertising report (only used with scannable advertising). */
      lctrRptState_t auxAdvRptState;    /*!< Auxiliary Advertising report state. */

      /* Backoff. */
      uint16_t      upperLimit;         /*!< Scan backoff upper limit. */
      uint8_t       backoffCount;       /*!< Scan backoff count. */
      uint8_t       consRspSuccess;     /*!< Number of consecutive scan response received. */
      uint8_t       consRspFailure;     /*!< Number of consecutive scan response failures. */
      uint64_t      scanReqAdvAddr;     /*!< Advertiser address in the scan request. */
    } scan;                             /*!< Extended scan data. */

    struct
    {
      LlExtInitScanParam_t  param;      /*!< Extended initiating scan parameters. */
      LlConnSpec_t  connSpec;           /*!< Connection specification. */
      lctrConnInd_t connInd;            /*!< Connection indication. */
      uint64_t      localRpa;           /*!< Local RPA. */
      uint16_t      connHandle;         /*!< Connection handle. */
      uint16_t      connInterval;       /*!< Connection interval. */
      uint32_t      scanWinStartUsec;   /*!< Scan window origin in microseconds. */
      bool_t        isLegacy;           /*!< TRUE if legacy advertising PDU is received. */
      uint8_t       usedChSel;          /*!< Used channel selection. */
      uint8_t       filtPolicy;         /*!< Initiate filter policy. */
      uint8_t       ownAddrType;        /*!< Own address type. */
      uint8_t       phy;                /*!< PHY selection. */
    } init;                             /*!< Extended initiate data. */
  } data;                               /*!< Extended scan or extended initiate data. */

  /* Scan buffer (placed here to 32-bit align) */
  uint8_t           reqBuf[BB_REQ_PDU_MAX_LEN];
                                        /*!< Scan host data buffer. */
  uint8_t           auxRspBuf[LL_EXT_ADVB_MAX_LEN];
                                        /*!< Auxiliary response buffer. */
  uint8_t           *pExtAdvData;       /*!< Advertising data reassembly buffer. */

  /* Packet state. */
  lctrExtAdvHdr_t   extAdvHdr;          /*!< Coalesced extended advertising header. */
  lctrAdvbPduHdr_t  reqPduHdr;          /*!< Request PDU header. */
  lctrAuxPtr_t      priChAuxPtr;        /*!< Primary channel AuxPtr. */
  lctrSyncInfo_t    secSyncInfo;        /*!< Secondary channel SyncInfo. */

  /* BB/ISR. */
  bool_t            auxOpPending;       /*!< Auxiliary operation pending. */
  uint8_t           bodTermCnt;         /*!< Number of BOD terminated. */
  BbOpDesc_t        scanBod;            /*!< Scan BOD. */
  BbBleData_t       scanBleData;        /*!< BLE BB operation data. */
  BbOpDesc_t        auxScanBod;         /*!< Auxiliary scan BOD. */
  BbBleData_t       auxBleData;         /*!< Auxiliary BLE BB operation data. */
  bool_t            scheduleAuxAsap;    /*!< Signal BB to program Aux Scan. */
} lctrExtScanCtx_t;

/*! \brief      Extended scanning control block. */
typedef struct
{
  /* State. */
  uint8_t           enaPhys;            /*!< Enabled PHYs. */
  uint8_t           scanTermByHost;     /*!< Times host initiated scan disable. */
  uint32_t          nextScanWinStart;   /*!< Next scan window origin. */

  /* Report */
  uint8_t           termReason;         /*!< Termination reason. */
  uint8_t           filtDup;            /*!< Advertising report filter mode. */
  lctrAdvRptFilt_t  advFilt;            /*!< Advertising filter data. */

  /* Timers. */
  uint32_t          scanDurMs;          /*!< Scan duration in milliseconds. */
  uint32_t          scanPerMs;          /*!< Scan period in milliseconds. */
  wsfTimer_t        tmrScanDur;         /*!< Scan duration timer. */
  wsfTimer_t        tmrScanPer;         /*!< Scan period timer. */
} lctrExtScanCtrlBlk_t;

/*! \brief      Active extended scanning context. */
typedef struct
{
  uint8_t           scanMask;           /*!< Mask for active scan contexts. */
  uint8_t           scanIndex;          /*!< Index of the active scan context. */
  uint8_t           bodSchMask;         /*!< Mask for BOD scheduling for each phy. */
} lctrActiveExtScan_t;

/*! \brief      Termination event handler call signature. */
typedef void (*lctrTermHdlr_t)(uint16_t syncHandle);

/*! \brief      Periodic scanning context. */
typedef struct
{
  bool_t            enabled;            /*!< Context enabled. */
  uint8_t           state;              /*!< Current state. */
  bool_t            shutdown;           /*!< Client initiated shutdown flag. */
  bool_t            cancelCreateSync;   /*!< Shut down due to create sync cancel. */
  bool_t            cancelByHost;       /*!< Cancel command was issued from host. */
  bool_t            firstPerAdvRcv;     /*!< True if first periodic advertising packet is received. */
  bool_t            repDisabled;        /*!< Reporting disabled. */
  bool_t            dupFilterEnable;    /*!< Filtering by ADI field enabled. */
  bool_t            bodAborted;         /*!< Tue if periodic scan BOD was aborted. */
  uint8_t           createDispId;       /*!< Dispatcher id to tell if periodic sync was created or transferred. */
  lctrTermHdlr_t    termCback;          /*!< Termination callback. */

  /* Report handling. */
  LlPerAdvReportInd_t advRpt;           /*!< Periodic advertising report. */
  lctrRptState_t    advRptState:8;      /*!< Periodic advertising report state. */
  uint8_t           *pPerAdvData;       /*!< Periodic data reassembly buffer. */

  /* BB data */
  BbOpDesc_t        bod;                /*!< Periodic scanning BOD. */
  BbBleData_t       bleData;            /*!< BLE BB operation data. */

  /* Peer periodic advertising parameters */
  uint16_t          eventCounter;       /*!< Connection event counter. */
  uint32_t          perInterUsec;       /*!< Periodic scanning interval in microseconds. */
  uint8_t           sca;                /*!< Sleep clock accuracy. */
  uint32_t          skipInterUsec;      /*!< Skip interval in microseconds. */
  uint32_t          minDurUsec;         /*!< Minimum required duration in microseconds. */
  uint32_t          rxSyncDelayUsec;    /*!< Receive timeout in microseconds. */
  uint32_t          lastAnchorPointUsec;/*!< Last anchor point in microseconds. */
  uint16_t          lastActiveEvent;    /*!< Last active event counter. */
  uint16_t          initEventCounter;   /*!< Initial event counter received from sync_info. */
  uint16_t          lastDid;            /*!< Last ADI received, if applicable. */

  /* ACAD */
  lctrAcadParam_t   acadParams[LCTR_ACAD_NUM_ID]; /*!< ACAD control block array. */

  /* Local periodic scanning parameters */
  uint16_t          skip;               /*!< Skip. */
  uint32_t          syncTimeOutMs;      /*!< Synchronization Timeout in Milliseconds. */

  /* Filtering parameters */
  bbBlePerPduFiltParams_t   filtParam;  /*!< Periodic scan filter parameters. */

  /* RF parameters */
  int8_t            rssi;               /*!< RSSI. */
  lmgrChanParam_t   chanParam;          /*!< Channel parameters. */

  /* Supervision */
  wsfTimer_t        tmrSupTimeout;      /*!< Supervision timer. */

  /* Peer device info */
  uint8_t           advSID;             /*!< Advertising SID. */
  uint8_t           advAddrType;        /*!< Advertiser Address Type. */
  uint8_t           trsfAddrType;       /*!< Advertiser Address Type to be used for sync transfer. */
  uint64_t          advAddr;            /*!< Advertiser Address. */
  uint64_t          trsfAdvAddr;        /*!< Advertiser Address to be used for sync transfer. */

  /* Packet state. */
  lctrExtAdvHdr_t   extAdvHdr;          /*!< Coalesced extended advertising header. */

  /* PHY */
  uint8_t           rxPhys;             /*!< Default receiver PHYs. */

} lctrPerScanCtx_t;

/*! \brief      Master scan state context. */
typedef struct
{
  uint8_t                       state;              /*!< Periodic scan state. */
  bool_t                        createSyncPending;  /*!< Create sync is pending. */
  bbBlePerPduFiltParams_t       filtParam;          /*!< Periodic scan filter parameters. */
  lctrPerScanCtx_t              *pPerScanCtx;       /*!< Current synchronous context. */
} lctrPerCreateSyncCtrlBlk_t;

/*! \brief      ACAD message header. */
typedef struct
{
  uint16_t          eventCtr;         /*!< Current event counter. */
  uint16_t          skip;             /*!< Skip amount. */
  uint8_t           acadId;           /*!< ACAD ID being processed. */
  uint16_t          handle;           /*!< Active Handle. */
} lctrAcadMsgHdr_t;

/*! \brief      ACAD message generic type. */
typedef union
{
  lctrAcadMsgHdr_t     hdr; /*!< Header of an ACAD message. */
} lctrAcadMsg_t;

/*! \brief      Periodic sync transfer state context. */
typedef struct
{
  uint8_t                       state;              /*!< Periodic sync transfer state. */
  uint16_t                      connHandle;         /*!< Connection handle. */
  uint16_t                      serviceData;        /*!< Service Data. */
  uint16_t                      ceRef;              /*!< Reference connection event counter. */
  uint16_t                      ceRcvd;             /*!< Connection event counter when LL_PERIODIC_SYNC_IND was received. */
  uint16_t                      syncCe;             /*!< Connection event counter when the contents of the PDU is determined. */
  uint8_t                       scaB;               /*!< Sleep clock accuracy of the device sending LL_PERIODIC_SYNC_IND. */
  uint16_t                      lastPECounter;      /*!< Last PA event counter. */
  lctrPerScanCtx_t              *pPerScanCtx;       /*!< Current synchronous context. */
} lctrPerTransferSyncCtrlBlk_t;

/**************************************************************************************************
  Globals
**************************************************************************************************/

extern lctrExtScanCtx_t * lctrMstExtScanTbl[LCTR_SCAN_PHY_TOTAL];
extern lctrExtScanCtrlBlk_t lctrMstExtScan;
extern lctrActiveExtScan_t lctrActiveExtScan;
extern lctrPerCreateSyncCtrlBlk_t lctrPerCreateSync;
extern lctrPerTransferSyncCtrlBlk_t lctrPerTransferSync;
extern lctrPerScanCtx_t lctrMstPerScanTbl[LL_MAX_PER_SCAN];
extern lctrSyncInfo_t trsfSyncInfo;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Builder */
uint8_t lctrMstExtDiscoverBuildOp(lctrExtScanCtx_t *pExtScanCtx);
uint8_t lctrMstAuxDiscoverBuildOp(lctrExtScanCtx_t *pExtScanCtx);
void lctrMstAuxDiscoverOpCommit(lctrExtScanCtx_t *pExtScanCtx, lctrAuxPtr_t *pAuxPtr, uint32_t startTs, uint32_t endTs);
uint8_t lctrMstPerScanBuildOp(lctrPerScanCtx_t *pPerScanCtx);
void lctrMstPerScanOpCommit(lctrExtScanCtx_t *pExtScanCtx, lctrAuxPtr_t *pAuxPtr, lctrSyncInfo_t *pSyncInfo, uint32_t startTs, uint32_t endTs);
void lctrMstPerScanTransferOpCommit(uint16_t connHandle);

/* ISR: Discovery packet handlers */
bool_t lctrMstDiscoverRxExtAdvPktHandler(BbOpDesc_t *pOp, const uint8_t *pAdvBuf);
void lctrMstDiscoverRxExtAdvPktPostProcessHandler(BbOpDesc_t *pOp, const uint8_t *pAdvBuf);
bool_t lctrMstDiscoverRxAuxAdvPktHandler(BbOpDesc_t *pOp, const uint8_t *pAdvBuf);
void lctrMstDiscoverRxAuxAdvPktPostProcessHandler(BbOpDesc_t *pOp, const uint8_t *pAdvBuf);
bool_t lctrMstDiscoverRxAuxScanRspHandler(BbOpDesc_t *pOp, const uint8_t *pRspBuf);
uint32_t lctrMstDiscoverRxAuxChainHandler(BbOpDesc_t *pOp, const uint8_t *pChainBuf);
bool_t lctrMstDiscoverRxAuxChainPostProcessHandler(BbOpDesc_t *pOp, const uint8_t *pChainBuf);
bool_t lctrMstDiscoverRxLegacyAdvPktHandler(BbOpDesc_t *pOp, const uint8_t *pAdvBuf);
bool_t lctrMstDiscoverTxLegacyScanReqHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf);
bool_t lctrMstDiscoverRxLegacyScanRspHandler(BbOpDesc_t *pOp, const uint8_t *pRspBuf);
bool_t lctrMstLinkAuxOffsetScanSetup(BbOpDesc_t *pBod, uint32_t refTime, uint32_t remScanDur);
/* ISR: Discovery BOD handlers */
void lctrMstExtDiscoverEndOp(BbOpDesc_t *pOp);
void lctrMstExtDiscoverAbortOp(BbOpDesc_t *pOp);
void lctrMstAuxDiscoverEndOp(BbOpDesc_t *pOp);
void lctrMstPerScanEndOp(BbOpDesc_t *pOp);
void lctrMstPerScanAbortOp(BbOpDesc_t *pOp);
uint32_t lctrMstPerScanRxPerAdvPktHandler(BbOpDesc_t *pOp, const uint8_t *pAdvBuf, uint8_t status);
bool_t lctrMstPerScanRxPerAdvPktPostHandler(BbOpDesc_t *pOp, const uint8_t *pAdvBuf);

/* Action routines. */
void lctrExtScanActDiscover(lctrExtScanCtx_t *pExtScanCtx);
void lctrExtScanActShutdown(lctrExtScanCtx_t *pExtScanCtx);
void lctrExtScanActScanCnf(lctrExtScanCtx_t *pExtScanCtx);
void lctrExtScanHostDisable(lctrExtScanCtx_t *pExtScanCtx);
void lctrExtScanActDisallowScan(lctrExtScanCtx_t *pExtScanCtx);
void lctrExtScanActHostEnable(lctrExtScanCtx_t *pExtScanCtx);
void lctrExtScanActScanTerm(lctrExtScanCtx_t *pExtScanCtx);
void lctrExtScanActSelfTerm(lctrExtScanCtx_t *pExtScanCtx);
void lctrExtScanActUpdateDiscover(lctrExtScanCtx_t *pExtScanCtx);
void lctrCreateSyncActCreate(void);
void lctrCreateSyncActCancel(void);
void lctrCreateSyncActFailed(void);
void lctrCreateSyncActTerminate(void);
void lctrCreateSyncActDone(void);
void lctrTransferSyncActStart(void);
void lctrTransferSyncActDone(void);
void lctrTransferSyncActFailed(void);
void lctrTransferSyncActCancel(void);
void lctrTransferSyncActTerminate(void);

void lctrPerScanActSyncEstd(lctrPerScanCtx_t *pPerScanCtx);
void lctrPerScanActSyncTerminate(lctrPerScanCtx_t *pPerScanCtx);
void lctrPerScanActSyncTerminateDone(lctrPerScanCtx_t *pPerScanCtx);
void lctrPerScanActSyncTimeout(lctrPerScanCtx_t *pPerScanCtx);
void lctrPerScanActProcessAcad(lctrAcadMsg_t *pMsg);

/* State machine */
void lctrMstExtScanExecuteSm(lctrExtScanCtx_t *pExtScanCtx, uint8_t event);
void lctrMstCreateSyncExecuteSm(uint8_t event);
void lctrMstTransferSyncExecuteSm(uint8_t event);
void lctrMstPerScanExecuteSm(lctrPerScanCtx_t *pPerScanCtx, uint8_t event);

/* Helpers */
lctrPerScanCtx_t *lctrAllocPerScanCtx(void);
BbOpDesc_t *lctrPerScanResolveConflict(BbOpDesc_t *pNewOp, BbOpDesc_t *pExistOp);
void lctrMstPerScanIsrInit(void);

/* Messaging */
void lctrSendExtScanMsg(lctrExtScanCtx_t *pExtScanCtx, uint8_t event);
void lctrSendCreateSyncMsg(lctrPerScanCtx_t *pCtx, uint8_t event);
void lctrSendPerScanMsg(lctrPerScanCtx_t *pCtx, uint8_t event);
void LctrSendPerSyncTrsfRcvdEvt(uint8_t status, lctrPerScanCtx_t *pPerScanCtx);

/*************************************************************************************************/
/*!
 *  \brief      Convert PHY value to PHY value in extended advertising report.
 *
 *  \param      auxPtrPhy       Auxiliary Pointer PHY field.
 *
 *  \return     LL PHY value.
 */
/*************************************************************************************************/
static inline uint8_t lctrConvertAuxPtrPhyToAdvRptPhy(uint8_t auxPtrPhy)
{
  switch (auxPtrPhy)
  {
    case 0: /* LE_1M */
    default:
      return LL_PHY_LE_1M;
    case 1: /* LE_2M */
      return LL_PHY_LE_2M;
    case 2: /* LE_Coded */
      return LL_PHY_LE_CODED;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Convert AuxPtr PHY value to PHY value in extended advertising report.
 *
 *  \param      auxPtrPhy       Auxiliary Pointer PHY field.
 *
 *  \return     BB PHY value.
 */
/*************************************************************************************************/
static inline uint8_t lctrConvertAuxPtrPhyToBbPhy(uint8_t auxPtrPhy)
{
  switch (auxPtrPhy)
  {
    case 0: /* LE_1M */
    default:
      return BB_PHY_BLE_1M;
    case 1: /* LE_2M */
      return BB_PHY_BLE_2M;
    case 2: /* LE_Coded */
      return BB_PHY_BLE_CODED;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Compute auxiliary offset.
 *
 *  \param  pAuxPtr         Auxiliary Pointer.
 *  \param  pOffsetUsec     Return auxiliary offset in microseconds.
 *  \param  pSyncDelayUsec  Return synchronization delay in microseconds.
 */
/*************************************************************************************************/
static inline void lctrMstComputeAuxOffset(lctrAuxPtr_t *pAuxPtr, uint32_t *pOffsetUsec, uint32_t *pSyncDelayUsec)
{
  uint32_t offsetUsec = pAuxPtr->auxOffset * ((pAuxPtr->offsetUnits == LCTR_OFFS_UNITS_30_USEC) ? 30 : 300);
  uint32_t caPpm = BbGetClockAccuracy() + ((pAuxPtr->ca == LCTR_CLK_ACC_0_50_PPM) ? 50 : 500);
  uint32_t wwUsec = lctrCalcWindowWideningUsec(offsetUsec + ((pAuxPtr->offsetUnits == LCTR_OFFS_UNITS_30_USEC) ? 30 : 300), caPpm);

  *pOffsetUsec = offsetUsec - wwUsec;
  *pSyncDelayUsec = (wwUsec << 1) + ((pAuxPtr->offsetUnits == LCTR_OFFS_UNITS_30_USEC) ? 30 : 300);    /* rounding compensation */
}

#ifdef __cplusplus
};
#endif

#endif /* LCTR_INT_ADV_MASTER_H */
