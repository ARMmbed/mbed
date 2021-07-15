/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  DM scan module.
 *
 *  Copyright (c) 2016-2018 Arm Ltd. All Rights Reserved.
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
#ifndef DM_SCAN_H
#define DM_SCAN_H

#include "wsf_os.h"
#include "wsf_timer.h"
#include "dm_main.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* DM scan event handler messages */
enum
{
  DM_SCAN_MSG_API_START = DM_MSG_START(DM_ID_SCAN),
  DM_SCAN_MSG_API_STOP,
  DM_SCAN_MSG_TIMEOUT
};

/* DM scan states */
enum
{
  DM_SCAN_STATE_IDLE,
  DM_SCAN_STATE_STARTING,
  DM_SCAN_STATE_SCANNING,
  DM_SCAN_STATE_STOPPING
};

/*! Uninitialized HCI sync handle */
#define DM_SYNC_HCI_HANDLE_NONE     0xFFFF

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Data structure for DM_SCAN_MSG_API_START */
typedef struct
{
  wsfMsgHdr_t             hdr;
  uint8_t                 scanPhys;
  uint8_t                 scanType[DM_NUM_PHYS];
  uint8_t                 mode;
  uint16_t                duration;
  uint16_t                period;
  bool_t                  filterDup;
} dmScanApiStart_t;

/* Data structure for DM_SYNC_MSG_API_START */
typedef struct
{
  wsfMsgHdr_t             hdr;
  uint8_t                 advSid;
  uint8_t                 advAddrType;
  bdAddr_t                advAddr;
  uint16_t                skip;
  uint16_t                syncTimeout;
  uint8_t                 syncCteType;
} dmSyncApiStart_t;

/* Data structure for DM_SYNC_MSG_API_ADD_DEV_TO_PER_ADV_LIST */
typedef struct
{
  wsfMsgHdr_t             hdr;
  uint8_t                 advAddrType;
  bdAddr_t                advAddr;
  uint8_t                 advSid;
} dmSyncApiAddDevToPerAdvList_t;

/* Data structure for DM_SYNC_MSG_API_REM_DEV_FROM_PER_ADV_LIST */
typedef struct
{
  wsfMsgHdr_t             hdr;
  uint8_t                 advAddrType;
  bdAddr_t                advAddr;
  uint8_t                 advSid;
} dmSyncApiRemDevFromPerAdvList_t;

/* Union of all scan messages */
typedef union
{
  wsfMsgHdr_t             hdr;
  dmScanApiStart_t        apiStart;
} dmScanMsg_t;

/* Union of all DM Sync state machine messages */
typedef union
{
  wsfMsgHdr_t                  hdr;
  dmSyncApiStart_t             apiSyncStart;
  hciLePerAdvSyncEstEvt_t      perAdvSyncEst;
  hciLePerAdvSyncLostEvt_t     perAdvSyncLost;
  HciLePerAdvSyncTrsfRcvdEvt_t perAdvSyncTrsfEst;
} dmSyncMsg_t;

/* Action function */
typedef void (*dmScanAct_t)(dmScanMsg_t *pMsg);

/* Control block for scan module */
typedef struct
{
  wsfTimer_t              scanTimer;
  uint16_t                scanInterval[DM_NUM_PHYS];
  uint16_t                scanWindow[DM_NUM_PHYS];
  uint8_t                 scanState;
  uint16_t                scanDuration;
  bool_t                  filterNextScanRsp;
  uint8_t                 discFilter;
} dmScanCb_t;

/* Control block for periodic advertising sync module */
typedef struct
{
  uint8_t                 advSid;      /*!< advertising SID */
  bdAddr_t                advAddr;     /*!< advertiser address */
  uint8_t                 advAddrType; /*!< advertiser address type */
  uint16_t                handle;      /*!< sync handle */
  dmSyncId_t              syncId;      /*!< sync id */
  bool_t                  encrypt;     /*!< Unencrypted or Encrypted */
  uint8_t                 state;       /*!< sync state */
  uint8_t                 inUse;       /*!< TRUE if entry in use */
} dmSyncCb_t;

/* Data structure for DM_PAST_MSG_API_SYNC_TRSF and DM_PAST_MSG_API_INFO_TRSF */
typedef struct
{
  wsfMsgHdr_t             hdr;         /*!< Header */
  uint16_t                serviceData; /*!< Value provided by the Host */
  dmConnId_t              connId;      /*!< Connection id */
} dmPastApiTrsf_t;

/* Data structure for DM_PAST_MSG_API_CONFIG and DM_PAST_MSG_API_DEFAULT_CONFIG */
typedef struct
{
  wsfMsgHdr_t             hdr;         /*!< Header */
  uint8_t                 mode;        /*!< Mode */
  uint16_t                skip;        /*!< Skip */
  uint16_t                syncTimeout; /*!< Sync timeout */
  uint8_t                 cteType;     /*!< CTE type */
} dmPastApiCfg_t;

/* Union of all DM PAST API messages */
typedef union
{
  wsfMsgHdr_t             hdr;
  dmPastApiTrsf_t         apiPastTrsf;
  dmPastApiCfg_t          apiPastCfg;
} dmPastMsg_t;

/*! Action function */
typedef void (*dmPastAct_t)(dmPastMsg_t *pMsg);

extern dmScanCb_t dmScanCb;

/**************************************************************************************************
  Function declarations
**************************************************************************************************/

/* common scanning component inteface */
void dmScanInit(void);

/* legacy scanning component inteface */
void dmScanReset(void);
void dmScanMsgHandler(wsfMsgHdr_t *pMsg);
void dmScanHciHandler(hciEvt_t *pEvent);

/* legacy scanning action functions */
void dmScanActStart(dmScanMsg_t *pMsg);
void dmScanActStop(dmScanMsg_t *pMsg);
void dmScanActTimeout(dmScanMsg_t *pMsg);

/* extended scanning component inteface */
void dmExtScanReset(void);
void dmExtScanMsgHandler(wsfMsgHdr_t *pMsg);
void dmExtScanHciHandler(hciEvt_t *pEvent);

/* extended scanning action functions */
void dmExtScanActStart(dmScanMsg_t *pMsg);
void dmExtScanActStop(dmScanMsg_t *pMsg);
void dmExtScanActTimeout(dmScanMsg_t *pMsg);

/* sync and sync transfer action functions */
void dmSyncSmActNone(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg);
void dmSyncSmActStart(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg);
void dmSyncSmActStop(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg);
void dmSyncSmActCancelStart(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg);
void dmSyncSmActSyncEst(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg);
void dmSyncSmActSyncEstFailed(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg);
void dmSyncSmActSyncLost(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg);
void dmSyncSmActSyncTrsfEst(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg);
void dmSyncSmActSyncTrsfEstFailed(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg);

/* sync component inteface */
void dmSyncInit(void);
void dmSyncReset(void);
void dmSyncMsgHandler(wsfMsgHdr_t *pMsg);
void dmSyncHciHandler(hciEvt_t *pEvent);

/* past action functions */
void dmPastActRptRcvEnable(dmPastMsg_t *pMsg);
void dmPastActSyncTsfr(dmPastMsg_t *pMsg);
void dmPastActSetInfoTrsf(dmPastMsg_t *pMsg);
void dmPastActConfig(dmPastMsg_t *pMsg);
void dmPastActDefaultConfig(dmPastMsg_t *pMsg);

/* past component inteface */
void dmPastMsgHandler(wsfMsgHdr_t *pMsg);
void dmPastHciHandler(hciEvt_t *pEvent);

/* sync utility functions */
dmSyncCb_t *dmSyncCbById(dmSyncId_t syncId);

#ifdef __cplusplus
};
#endif

#endif /* DM_SCAN_H */
