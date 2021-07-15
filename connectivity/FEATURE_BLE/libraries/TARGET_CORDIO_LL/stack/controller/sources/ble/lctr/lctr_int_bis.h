/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Internal link layer controller connected isochronous interface file.
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

#ifndef LCTR_INT_BIS_H
#define LCTR_INT_BIS_H

#include "lctr_int_adv_slave_ae.h"
#include "lctr_int_adv_master_ae.h"
#include "lctr_int.h"
#include "lctr_int_iso.h"
#include "lctr_pdu_iso.h"
#include "bb_ble_api.h"
#include "pal_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Get encryption ID from BIG context. */
#define LCTR_BIG_CTRL_ENC_ID(pBigCtx)   (LL_MAX_CONN + LL_MAX_CIG + LL_MAX_BIG + (pBigCtx - &pLctrBigTbl[0]))

/*! \brief  ISO Data PDU start offset in a buffer. */
#define LCTR_ISO_SDU_START_OFFSET       (HCI_ISO_HDR_LEN + HCI_ISO_DL_MAX_LEN - LL_ISO_DATA_HDR_LEN)

/**************************************************************************************************
  Data Types
**************************************************************************************************/

struct lctrBigCtx_tag;

/*! \brief      Broadcast Isochronous Stream (BIS) context. */
typedef struct
{
  bool_t        enabled;        /*!< Enable flag. */
  uint8_t       bisNum;         /*!< BIS positional sequence number. */
  uint16_t      handle;         /*!< BIS handle. */
  struct lctrBigCtx_tag *pBigCtx;   /*!< BIG context. */

  union
  {
    struct
    {
      /* Data */
      wsfQueue_t           txDataQ;       /*!< Transmit ISO queue. */
      uint8_t              numTxSduComp;  /*!< Number of Tx completed SDUs. */
      lctrIsoalTxCtx_t     isoalTxCtx;    /*!< ISOAL transmit context. */
      lctrInDataPathCtx_t  dataPathInCtx; /*!< Datapath input context. */
    } slv;                                /*!< BIS slave specific data. */

    struct
    {
      /* Data */
      wsfQueue_t            rxDataQ;        /*!< Receive ISO Data PDU pending queue. */
      lctrIsoalRxCtx_t      isoalRxCtx;     /*!< ISOAL receive context. */
      lctrOutDataPathCtx_t  dataPathOutCtx; /*!< Output data path context. */

      /* ISO test */
      LlIsoTestCtrs_t       stats;          /*!< Rx statistics. */
    } mst;                                  /*!< BIS master specific data. */
  } roleData;                               /*!< Role specific data. */

  /* ISO test */
  struct
  {
    bool_t      enabled;        /*!< TRUE ISO test enabled, FALSE otherwise. */
    bool_t      term;           /*!< Flag for ISO test termination. */
    bool_t      pendInit;       /*!< Receive pending init. */
    union
    {
      struct
      {
        uint32_t    payloadCtr; /*!< Payload counter for framed transmissions. */
      } framed;
      struct
      {
        uint8_t     burstIdx;   /*!< BN Index for unframed transmissions. */
      } unframed;
    } util;
    LlIsoPldType_t pldType:8;   /*!< Test payload type. */
  } test;

  /* BB */
  lmgrChanParam_t chSelInfo;    /*!< Channel selection state. */
  PalBbBleChan_t chan;          /*!< Channelization parameters. */

  /* Data */
  LlIsoLlid_t   lastLlid:8;     /*!< Last LLID. */
} lctrBisCtx_t;

/*! \brief      Broadcast Isochronous Group (BIG) context. */
typedef struct lctrBigCtx_tag
{
  bool_t        enabled;        /*!< Context enabled. */
  uint8_t       state;          /*!< Current state. */
  uint8_t       handle;         /*!< BIG handle. */
  LlRole_t      role:8;         /*!< Role. */

  /* Data */
  uint16_t      maxPdu;         /*!< Maximum size of BIS Data PDU. */
  uint16_t      maxSdu;         /*!< Maximum size of ISO SDU. */

  /* Host */
  uint32_t      sduInterUsec;   /*!< SDU interval in microseconds. */
  uint32_t      bisSpaceUsec;   /*!< BIS space in microseconds. */
  uint32_t      isoInterUsec;   /*!< Isochronous PDU interval in microseconds. */
  uint8_t       bn;             /*!< Burst number. */
  uint8_t       nse;            /*!< Maximum number of subevent in each interval on BIS. */
  uint8_t       pto;            /*!< Pre-transmission offset. */
  uint8_t       irc;            /*!< Immediate repetition count. */
  LlFraming_t   framing:8;      /*!< BIS Data PDU format. */
  LlPacking_t   packing:8;      /*!< Packing sequence scheme. */

  /* ISO Event */
  uint64_t      eventCounter;   /*!< Event counter. */
  uint32_t      syncDelayUsec;  /*!< Synchronization delay in microseconds. */
  uint32_t      transLatUsec;   /*!< The maximum transmission latency, in microseconds. */
  uint32_t      subInterUsec;   /*!< Subevent interval in microseconds. */

  /* BIS */
  uint8_t       numBis;               /*!< Number of BISs. */
  lctrBisCtx_t  *pBisCtx[LL_MAX_BIS]; /*!< BIS contexts. */

  union
  {
    struct
    {
      lctrAdvSet_t *pAdvSet;    /*!< Advertising Set parent. */
      wsfQueue_t txCtrlQ;       /*!< Transmit BIG control queue. */
      bool_t    notifyHostEst;  /*!< Notify host event sent flag. */
    } slv;                      /*!< BIG slave specific data. */

    struct
    {
      /* BIG Create Sync */
      lctrPerScanCtx_t *pPerScanCtx;  /*!< Periodic Scan parent. */
      bool_t    syncLostReason;       /*!< BIG synchronization lost. */
      uint8_t   mse;                  /*!< Maximum number of subevents. */
      uint8_t   numBisIdx;            /*!< Total number of BISes in the BIG. */
      uint8_t   bisIdx[LL_MAX_BIS];   /*!< List of indices of BISes. */

      /* Sync timeout */
      wsfTimer_t bigSyncTmr;          /*!< Synchronization timeout timer. */
      uint32_t   bigSyncTimeoutMs;    /*!< Synchronization timeout in microseconds. */

      /* Encryption */
      uint8_t bcstCode[LL_BC_LEN];    /*!< Broadcast Code. Array address should be word-aligned. */

      /* Event state */
      uint16_t  totalAcc;             /*!< Total clock accuracy. */
      uint16_t  initWwUsec;           /*!< Initial synchronization window widening time in microseconds. */
      uint32_t  rxSyncTime;           /*!< Last received BIG anchor point. */
      uint32_t  anchorPoint;          /*!< BIG anchor point. */
      lctrBisCtx_t *pFirstBisCtx;     /*!< First BIS context (ISR optimization to reduce lookups). */
      lctrBisCtx_t *pSecondBisCtx;    /*!< Second BIS context (ISR optimization to reduce lookups). */
      uint32_t  firstBisOffsUsec;     /*!< First BIS offset from the anchor point in microseconds. */
      uint8_t firstBisEvtIdx;         /*!< First BIS index. */

      /* Reception status. */
      bool_t    lastPduMissed;        /*!< Rx failure on last PDU. */
    } mst;                            /*!< BIG master specific data. */
  } roleData;                         /*!< Role-specific data. */

  /* Control */
  struct
  {
    uint8_t     actMsk;         /*!< Active control procedure bitmask. */
    uint8_t     pendMsk;        /*!< Pending control procedure bitmask. */
    uint8_t     cssn;           /*!< Control Subevent Sequence Number */
    struct
    {
      uint16_t  inst;           /*!< Instant. */
      uint64_t  chanMap;        /*!< Channel map. */
    } chanMapUpd;               /*!< Channel Map Update data. */
    struct
    {
      uint16_t  inst;           /*!< Instant. */
      uint8_t   reason;         /*!< Termination reason. */
    } term;                     /*!< Terminate data. */
  } bcp;                        /*!< BIG Control Procedure data. */

  /* BB */
  PalBbPhy_t    phy:8;          /*!< PHY used for the BIG. */
  BbOpDesc_t    bod;            /*!< BIG BOD. */
  BbBleData_t   bleData;        /*!< BLE BB operation data. */
  uint32_t      seedAccAddr;    /*!< Seed access address. */
  uint16_t      baseCrcInit;    /*!< Base CRC Init. */
  lmgrChanParam_t ctrChSelInfo; /*!< Control channel selection state. */
  PalBbBleChan_t ctrChan;       /*!< BIG Control channelization parameters. */

  /* Encryption */
  /* Note: located at end of structure for non-encryption optimization */
  bool_t        encrypt;                /*!< Encryption enable for BIS. */
  uint8_t       giv[LL_GIV_LEN];        /*!< GIV. */
  uint8_t       gskd[LL_GSKD_LEN];      /*!< GSKD. */
} lctrBigCtx_t;

/*! \brief  ISR subevent context. */
typedef struct
{
  uint8_t bisEvtIdx;            /*!< BIS event index within an event. */
  uint8_t burstIdx;             /*!< Burst index within a subevent. */
  uint8_t repIdx;               /*!< Repeat index within a subevent. */
  uint8_t ptIdx;                /*!< Pre-transmission index within a subevent. */
} lctrSeCtx_t;

/**************************************************************************************************
  Globals
**************************************************************************************************/

extern lctrBisCtx_t *pLctrBisTbl;
extern lctrBigCtx_t *pLctrBigTbl;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* BIG Context */
lctrBigCtx_t *lctrAllocBigCtx(uint8_t bigHandle);
void lctrFreeBigCtx(lctrBigCtx_t *pBigCtx);
uint8_t lctrBigIsPerAdvUsed(uint8_t handle);
lctrBigCtx_t *lctrFindBigByHandle(uint8_t bigHandle);
lctrBigCtx_t *lctrFindBigBySyncHandle(uint16_t syncHandle);
bool_t lctrIsBigSynchronizing(void);

/* BIS Context */
lctrBisCtx_t *lctrAllocBisCtx(lctrBigCtx_t *pBigCtx);
void lctrFreeBisCtx(lctrBisCtx_t *pBisCtx);
lctrBisCtx_t *lctrFindBisByHandle(uint16_t bisHandle);
uint8_t lctrGetNumAvailBisCtx(void);
uint32_t lctrComputeBisAccessAddr(uint32_t seedAccAddr, uint8_t bisNum);
void lctrSetupBisContext(lctrBisCtx_t *pBisCtx, uint32_t seedAccAddr, uint16_t baseCrcInit, uint64_t chMap, LlPhy_t phy);
void lctrSelectBigChannels(lctrBigCtx_t *pBigCtx);
void lctrRemapBigChannels(lctrBigCtx_t *pBigCtx, uint64_t chanMap);

/* BIS Tx Data Path */
void lctrBisTxIsoPduQueue(lctrBisCtx_t *pBisCtx, lctrIsoHdr_t *pIsoHdr, uint8_t *pIsoSdu);
uint8_t lctrBisTxQueuePeek(lctrBisCtx_t *pBisCtx, uint8_t burstIdx, PalBbBleTxBufDesc_t *descs);
void lctrBisTxQueuePopCleanup(lctrBisCtx_t *pBisCtx, uint8_t numFrag);
uint8_t *lctrBigTxCtrlAlloc(uint8_t pduLen);
void lctrBigTxCtrlQueue(lctrBigCtx_t *pBigCtx, uint8_t *pBuf, uint8_t numReTx);
uint8_t *lctrBigTxCtrlQueuePeek(lctrBigCtx_t *pBigCtx);
void lctrBigTxCtrlQueuePop(lctrBigCtx_t *pBigCtx);
void lctrBigTxCtrlQueuePopCleanup(lctrBigCtx_t *pBigCtx);

/* BIS Rx Data Path */
uint8_t *lctrBisRxIsoSduDeq(lctrBisCtx_t *pBisCtx);
uint8_t *lctrBisRxIsoDataPduAlloc(void);
void lctrBisRxIsoDataPduFree(uint8_t *pPdu);
void lctrBisEnqueueRxDataPdu(lctrBisCtx_t *pBisCtx, uint8_t *pRxBuf, uint8_t burstIdx);
uint8_t *lctrBisDequeueRxDataPdu(lctrBisCtx_t *pBisCtx, uint8_t burstIdx);
uint8_t *lctrBisDequeueRxDataPduTop(lctrBisCtx_t *pBisCtx);

/* ISO Test mode */
uint8_t lctrBisTxTest(lctrBisCtx_t *pBisCtx, uint8_t pldType);
uint8_t lctrBisRxTest(lctrBisCtx_t *pBisCtx, uint8_t pldType);
uint8_t LctrBisReadTestCounters(lctrBisCtx_t *pBisCtx, LlIsoTestCtrs_t *pStats);

/* BIS helper functions */
void lctrBisDefaults(void);
void lctrNotifyIsoTxComplete(lctrBigCtx_t *pBigCtx);
void lctrBisCalcGroupSessionKey(const uint8_t *pGSKD, const uint8_t *pBC, uint8_t *pGSK);
bool_t lctrBisCalcNextIdxSequential(lctrBigCtx_t *pBigCtx, lctrSeCtx_t *pSeCtx, uint8_t numSePkts);
bool_t lctrBisCalcNextIdxInterleaved(lctrBigCtx_t *pBigCtx, lctrSeCtx_t *pSeCtx, uint8_t numSePkts);

#ifdef __cplusplus
};
#endif

#endif /* LCTR_INT_BIS_H */
