/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Internal link layer controller isochronous master interface file.
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

#ifndef LCTR_INT_BIS_MASTER_H
#define LCTR_INT_BIS_MASTER_H

#include "lctr_api_bis_master.h"
#include "lctr_int_bis.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Get reservation manager handle for BIG from the context pointer. */
#define LCTR_GET_BIG_RM_HANDLE(pBigCtx)   (LL_MAX_CONN + LL_MAX_ADV_SETS + LL_MAX_BIG + (pBigCtx - &pLctrBigTbl[0]))

/*! \brief      Resolve BIG context pointer from the reservation manager handle. */
#define LCTR_GET_BIG_RM_CTX(rmHandle)     &(pLctrBigTbl[rmHandle - (LL_MAX_CONN + LL_MAX_ADV_SETS + LL_MAX_BIG)])

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*! \brief      Slave BIG broadcasting states. */
typedef enum
{
  LCTR_MST_BIG_STATE_DISABLED,          /*!< BIG master disabled state. */
  LCTR_MST_BIG_STATE_SYNCING,           /*!< BIG master synchronizing state. */
  LCTR_MST_BIG_STATE_SYNCED,            /*!< BIG master synchronized state. */
  LCTR_MST_BIG_STATE_SHUTDOWN,          /*!< BIG master shutdown in progress. */
  LCTR_MST_BIG_STATE_RESET,             /*!< BIG master reset in progress. */
  LCTR_MST_BIG_STATE_TOTAL              /*!< Total number of BIG master states. */
} lctrMstBigState_t;

/**************************************************************************************************
  Globals
**************************************************************************************************/

extern lctrMstBigMsg_t *pLctrMstBigMsg;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Host Events */
void lctrNotifyHostBigTerminateComplete(LlStatus_t status, uint8_t bigHandle);
void lctrNotifyHostSyncLost(uint8_t bigHandle, LlStatus_t reason);

/* State machine for BIG master */
void lctrMstBigSendMsg(lctrBigCtx_t *pBigCtx, LctrMstBigMsg_t event);
void lctrMstBigExecuteSm(lctrBigCtx_t *pBigCtx, LctrMstBigMsg_t event);

/* Action routines */
void lctrMstBigActStart(lctrBigCtx_t *pBigCtx);
void lctrMstBigActBigSync(lctrBigCtx_t *pBigCtx);
void lctrMstBigActTerm(lctrBigCtx_t *pBigCtx);
void lctrMstBigActShutdown(lctrBigCtx_t *pBigCtx);
void lctrMstBigActSyncLost(lctrBigCtx_t *pBigCtx);
void lctrMstBigActMicFailed(lctrBigCtx_t *pBigCtx);
void lctrMstBigActCleanup(lctrBigCtx_t *pBigCtx);

/* Builder */
void lctrMstBigBuildOp(lctrBigCtx_t *pBigCtx, LctrAcadBigInfo_t *pBigInfo);
void lctrMstSetupBigContext(lctrBigCtx_t *pBigCtx, LctrAcadBigInfo_t *pBigInfo);
void lctrMstSetupBigChannel(lctrBigCtx_t *pBigCtx, LctrAcadBigInfo_t *pBigInfo);

/* ISR: BOD handlers */
void lctrMstBisRxCompletion(BbOpDesc_t *pBod, uint8_t *pBuf, uint8_t status);
void lctrMstBigBeginOp(BbOpDesc_t *pOp);
void lctrMstBigAbortOp(BbOpDesc_t *pOp);
void lctrMstBigEndOp(BbOpDesc_t *pOp);

#ifdef __cplusplus
};
#endif

#endif /* LCTR_INT_BIS_MASTER_H */
