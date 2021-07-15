/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master operation builder implementation file.
 *
 *  Copyright (c) 2019 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_bis_master.h"
#include "lctr_api_bis_master.h"
#include "lctr_int_iso.h"
#include "sch_api.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_timer.h"
#include <string.h>

/**************************************************************************************************
  Globals
**************************************************************************************************/

/*! \brief      BIG message data. */
lctrMstBigMsg_t *pLctrMstBigMsg;

/*************************************************************************************************/
/*!
 *  \brief      BIS master message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstBigDisp(lctrMsgHdr_t *pMsg)
{
  pLctrMstBigMsg = (lctrMstBigMsg_t *)pMsg;

  if ((pMsg->dispId != LCTR_DISP_BCST) &&
      (pMsg->event != LCTR_MST_BIG_ACAD_BIG_INFO))
  {
    lctrBigCtx_t *pBigCtx;

    if ((pBigCtx = lctrFindBigByHandle(pMsg->handle)) != NULL)
    {
      lctrMstBigExecuteSm(pBigCtx, pMsg->event);
    }
  }
  else
  {
    for (unsigned int i = 0; i < pLctrRtCfg->maxBig; i++)
    {
      if (pLctrBigTbl[i].enabled && (pLctrBigTbl[i].role == LL_ROLE_MASTER))
      {
        pMsg->handle = pLctrBigTbl[i].handle;
        lctrMstBigExecuteSm(&pLctrBigTbl[i], pMsg->event);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      BIS master reset handler.
 */
/*************************************************************************************************/
static void lctrMstBisResetHandler(void)
{
  lctrBisDefaults();
}

/*************************************************************************************************/
/*!
 *  \brief      Periodic sync termination handler.
 *
 *  \param      syncHandle  Periodic Sync handle.
 */
/*************************************************************************************************/
static void lctrMstPerTermSync(uint16_t syncHandle)
{
  lctrBigCtx_t *pBigCtx;

  if ((pBigCtx = lctrFindBigBySyncHandle(syncHandle)) != NULL)
  {
    WSF_ASSERT(pBigCtx->role == LL_ROLE_MASTER);

    if (pBigCtx->roleData.mst.pPerScanCtx)
    {
      pBigCtx->roleData.mst.pPerScanCtx = NULL;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for BIS master.
 */
/*************************************************************************************************/
void LctrMstBisInit(void)
{
  /* Add reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_BIG_SYNC] = lctrMstBisResetHandler;

  /* Add BIS slave message dispatcher. */
  lctrMsgDispTbl[LCTR_DISP_BIG_SYNC] = (LctrMsgDisp_t)lctrMstBigDisp;

  lctrBisDefaults();

  /* Set supported features. */
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_1)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_ISO_SYNC;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  BIG create sync
 *
 *  \param  pParam    BIG Create Sync parameters.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrMstBigCreateSync(LlBigCreateSync_t *pParam)
{
  lctrBigCtx_t *pBigCtx;
  LctrAcadBigInfo_t *pBigInfo;
  lctrPerScanCtx_t *pPerScanCtx;

  if (lctrIsBigSynchronizing())
  {
    LL_TRACE_WARN0("LctrMstBigCreateSync: BIG synchronization in progress");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (lctrFindBigByHandle(pParam->bigHandle) != NULL)
  {
    LL_TRACE_WARN1("LctrMstBigCreateSync: bigHandle=%u already in use", pParam->bigHandle);
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  pPerScanCtx = LCTR_GET_PER_SCAN_CTX(pParam->syncHandle);

  if ((pParam->syncHandle > LL_MAX_PER_SCAN) ||
      (pPerScanCtx->enabled == FALSE))
  {
    LL_TRACE_WARN1("LctrMstBigCreateSync: syncHandle=%u not found", pParam->syncHandle);
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  if (lctrFindBigBySyncHandle(pParam->syncHandle) != NULL)
  {
    LL_TRACE_WARN1("LctrMstBigCreateSync: syncHandle=%u already in use", pParam->syncHandle);
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }

  if (pPerScanCtx->acadParams[LCTR_ACAD_ID_BIG_INFO].hdr.len == 0)   /* BIG Info received */
  {
    LL_TRACE_WARN0("LctrMstBigCreateSync: synchronizer not ready; BIG Info not received");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  pBigInfo = &pPerScanCtx->acadParams[LCTR_ACAD_ID_BIG_INFO].bigInfo;
  if (pParam->numBis > pBigInfo->numBis)
  {
    LL_TRACE_WARN2("LctrMstBigCreateSync: invalid value for numBis=%u for this sync handle, validRange=1..%u", pParam->numBis, pBigInfo->numBis);
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }

  if (pParam->numBis > LL_MAX_BIS)
  {
    LL_TRACE_WARN2("LctrMstBigCreateSync: invalid value for numBis=%u, validRange=1..%u", pParam->numBis, LL_MAX_BIS);
    return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
  }

  if (pParam->numBis > lctrGetNumAvailBisCtx())
  {
    LL_TRACE_WARN0("LctrMstBigCreateSync: insufficient BIS context");
    return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
  }

  for (unsigned int i = 0; i < pParam->numBis; i++)
  {
    if ((pParam->bis[i] < LL_MIN_BIS) ||
        (pParam->bis[i] > LL_MAX_BIS))
    {
      LL_TRACE_WARN2("BIS=%u at index=%u out of range", pParam->bis[i], i);
      return LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE;
    }
  }

  if (pParam->encrypt != pBigInfo->encrypt)
  {
    LL_TRACE_WARN0("LctrMstBigCreateSync: encryption mode mismatch");
    return LL_ERROR_CODE_ENCRYPT_MODE_NOT_ACCEPTABLE;
  }

  if ((pBigCtx = lctrAllocBigCtx(pParam->bigHandle)) == NULL)
  {
    LL_TRACE_WARN0("LctrMstBigCreateSync: insufficient BIG context");
    return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
  }

  pBigCtx->handle = pParam->bigHandle;
  pPerScanCtx->termCback = lctrMstPerTermSync;
  pBigCtx->roleData.mst.pPerScanCtx = pPerScanCtx;
  pBigCtx->roleData.mst.mse = pParam->mse;
  pBigCtx->roleData.mst.bigSyncTimeoutMs = pParam->bigSyncTimeout * 10;
  pBigCtx->roleData.mst.numBisIdx = pParam->numBis;
  memcpy(pBigCtx->roleData.mst.bisIdx, pParam->bis, pParam->numBis);
  pBigCtx->encrypt = pParam->encrypt;
  memcpy(pBigCtx->roleData.mst.bcstCode, pParam->bcstCode, LL_BC_LEN);

  /* Setup timer. */
  pBigCtx->roleData.mst.bigSyncTmr.handlerId = lmgrPersistCb.handlerId;
  lctrMsgHdr_t *pMsg = (lctrMsgHdr_t *)&pBigCtx->roleData.mst.bigSyncTmr.msg;
  pMsg->handle = pBigCtx->handle;
  pMsg->dispId = LCTR_DISP_BIG_SYNC;
  pMsg->event = LCTR_MST_BIG_INT_SYNC_TIMEOUT;

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->handle = pParam->bigHandle;
    pMsg->dispId = LCTR_DISP_BIG_SYNC;
    pMsg->event = LCTR_MST_BIG_API_CREATE_SYNC;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  BIG terminate sync
 *
 *  \param      bigHandle     Used to identify the BIG.
 */
/*************************************************************************************************/
void LctrMstBigTerminateSync(uint8_t bigHandle)
{
  lctrBigCtx_t *pBigCtx;

  if ((pBigCtx = lctrFindBigByHandle(bigHandle)) == NULL)
  {
    LL_TRACE_WARN1("LctrMstBigTerminateSync: unknown handle bigHandle=%u", bigHandle);
    lctrNotifyHostBigTerminateComplete(LL_ERROR_CODE_UNKNOWN_ADV_ID, bigHandle);
    return;
  }

  if (pBigCtx->role != LL_ROLE_MASTER)
  {
    LL_TRACE_WARN0("LctrMstBigTerminateSync: invalid role");
    lctrNotifyHostBigTerminateComplete(LL_ERROR_CODE_CMD_DISALLOWED, bigHandle);
    return;
  }

  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->handle = bigHandle;
    pMsg->dispId = LCTR_DISP_BIG_SYNC;
    pMsg->event = LCTR_MST_BIG_API_TERMINATE_SYNC;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Build  BIG operation.
 *
 *  \param  pBigCtx     BIG Context.
 *  \param  pBigInfo    BIG Info parameters from Broadcaster.
 */
/*************************************************************************************************/
void lctrMstBigBuildOp(lctrBigCtx_t *pBigCtx, LctrAcadBigInfo_t *pBigInfo)
{
  lctrBisCtx_t * const pBisCtx = pBigCtx->roleData.mst.pFirstBisCtx;

  BbOpDesc_t * const pOp = &pBigCtx->bod;
  BbBleData_t * const pBle = &pBigCtx->bleData;
  BbBleMstBisEvent_t * const pBis = &pBle->op.mstBis;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));
  memset(pBis, 0, sizeof(BbBleMstBisEvent_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_FIXED_PREFERRED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrMstBigEndOp;
  pOp->abortCback = lctrMstBigAbortOp;
  pOp->pCtx = pBigCtx;

  /*** BLE General Setup ***/

  pBle->chan = pBisCtx->chan;

  /*** General setup ***/

  pOp->minDurUsec = pBigInfo->subEvtInterUsec;
  pOp->maxDurUsec = pBigInfo->subEvtInterUsec * pBigInfo->bn;

  /*** BIS setup ***/

  /* pBis->rxSyncDelayUsec = 0; */  /* Deferred until BOD scheduled. */
  pBis->execCback = lctrMstBigBeginOp;

  switch (pBigCtx->packing)
  {
    case LL_PACKING_INTERLEAVED:
      pBis->rxDataCback = lctrMstBisRxCompletion;
      break;

    case LL_PACKING_SEQUENTIAL:
    default:
      pBis->rxDataCback = lctrMstBisRxCompletion;
      break;
  }

  /*** Commit operation ***/

  /* Originate from peer's N-1 anchor point timing. */
  pBigCtx->roleData.mst.anchorPoint = pBigInfo->bigAnchorPoint;
  pBigCtx->roleData.mst.rxSyncTime = pBigCtx->roleData.mst.anchorPoint - pBigCtx->isoInterUsec;
  /* Expand receive window for initial synchronization due to resolution uncertainty. */
  pBigCtx->roleData.mst.initWwUsec = (pBigInfo->bigOffsUnits == 0) ? 30 : 300;
  uint32_t unsyncTimeUsec = 0;

  while (TRUE)
  {
    uint32_t wwTotalUsec = lctrCalcWindowWideningUsec(unsyncTimeUsec, pBigCtx->roleData.mst.totalAcc)
                           + pBigCtx->roleData.mst.initWwUsec;
    /* TODO Limit to half the ISO Interval size */

    pOp->dueUsec = pBigCtx->roleData.mst.anchorPoint + pBigCtx->roleData.mst.firstBisOffsUsec - wwTotalUsec;
    /* Multiply 2 for before and after BIG Anchor Point. */
    pBis->rxSyncDelayUsec = (wwTotalUsec << 1);

    lctrSelectBigChannels(pBigCtx);

    if (SchInsertAtDueTime(pOp, NULL))
    {
      break;
    }

    LL_TRACE_WARN2("!!! BIG schedule conflict handle=%u, ec[15:0]=%u", pBigCtx->handle, pBigCtx->eventCounter);

    /* Advance to next interval. */
    pBigCtx->eventCounter += 1;
    pBigCtx->roleData.mst.anchorPoint += pBigCtx->isoInterUsec;
    unsyncTimeUsec += pBigCtx->isoInterUsec;
  }

  LL_TRACE_INFO2("    >>> BIG synced, handle=%u, numBis=%u <<<", pBigCtx->handle, pBigCtx->numBis);
  LL_TRACE_INFO3("                    bn=%u, nse=%u, irc=%u", pBigCtx->bn, pBigCtx->nse, pBigCtx->irc);
  LL_TRACE_INFO3("                    pto=%u, framing=%u, packing=%u", pBigCtx->pto, pBigCtx->framing, pBigCtx->packing);
  LL_TRACE_INFO1("                    isoInterUsec=%u", pBigCtx->isoInterUsec);
  LL_TRACE_INFO1("                    bisSpaceUsec=%u", pBigCtx->bisSpaceUsec);
  LL_TRACE_INFO1("                    subInterUsec=%u", pBigCtx->subInterUsec);
  LL_TRACE_INFO1("                    eventCounter=%u", pBigCtx->eventCounter);
  LL_TRACE_INFO1("                    seedAccAddr=0x%08x", pBigCtx->seedAccAddr);
  LL_TRACE_INFO1("                    pBod=0x%08x", pOp);
}

/*************************************************************************************************/
/*!
 *  \brief      Setup BIG context.
 *
 *  \param      pBigCtx         BIG context.
 *  \param      pBigInfo        BIG Info.
 */
/*************************************************************************************************/
void lctrMstSetupBigContext(lctrBigCtx_t *pBigCtx, LctrAcadBigInfo_t *pBigInfo)
{
  /* Packet */
  pBigCtx->maxPdu = pBigInfo->maxPdu;
  pBigCtx->maxSdu = pBigInfo->maxSdu;
  pBigCtx->seedAccAddr = pBigInfo->seedAccAddr;
  pBigCtx->baseCrcInit = pBigInfo->baseCrcInit;

  /* Event */
  pBigCtx->bn  = pBigInfo->bn;
  pBigCtx->nse = pBigInfo->nse;
  pBigCtx->irc = pBigInfo->irc;
  pBigCtx->pto = pBigInfo->pto;
  pBigCtx->framing = pBigInfo->framing;
  pBigCtx->eventCounter = pBigInfo->bisPldCtr / pBigInfo->bn;  /* TODO use optimized 64-bit divide */
  if (pBigInfo->bisSpaceUsec >= pBigInfo->subEvtInterUsec)
  {
    pBigCtx->packing = LL_PACKING_SEQUENTIAL;
  }
  else
  {
    pBigCtx->packing = LL_PACKING_INTERLEAVED;
  }

  /* Control */
  pBigCtx->bcp.cssn = 0xFF;     /* ensure reception of initial BIS Control PDU */

  /* Radio */
  pBigCtx->phy = pBigInfo->phy;

  /* Encryption */
  pBigCtx->encrypt = pBigInfo->encrypt;
  if (pBigCtx->encrypt)
  {
    memcpy(pBigCtx->giv, pBigInfo->giv, sizeof(pBigCtx->giv));
    memcpy(pBigCtx->gskd, pBigInfo->gskd, sizeof(pBigCtx->gskd));

    /* Setup encryption parameters. */
    lctrBisCalcGroupSessionKey(pBigCtx->gskd, pBigCtx->roleData.mst.bcstCode, pBigCtx->bleData.chan.enc.sk);
  }

  /* Timing */
  pBigCtx->isoInterUsec = pBigInfo->isoInter * 1250;
  pBigCtx->bisSpaceUsec = pBigInfo->bisSpaceUsec;
  pBigCtx->subInterUsec = pBigInfo->subEvtInterUsec;
  /* pBigCtx->syncDelayUsec = N/A; */
  pBigCtx->sduInterUsec = pBigInfo->sduInterUsec;
  pBigCtx->roleData.mst.totalAcc = lctrCalcTotalAccuracy(LL_MCA_20_PPM);
}

/*************************************************************************************************/
/*!
 *  \brief      Setup BIG channelization parameters.
 *
 *  \param      pBigCtx         BIG context.
 *  \param      pBigInfo        BIG Info.
 */
/*************************************************************************************************/
void lctrMstSetupBigChannel(lctrBigCtx_t *pBigCtx, LctrAcadBigInfo_t *pBigInfo)
{
  pBigCtx->ctrChan.opType = BB_BLE_OP_MST_BIS_EVENT;
  pBigCtx->ctrChan.accAddr = lctrComputeBisAccessAddr(pBigInfo->seedAccAddr, 0);

  pBigCtx->ctrChSelInfo.chanMask = pBigInfo->chanMap;
  pBigCtx->ctrChSelInfo.usedChSel = LL_CH_SEL_2;
  pBigCtx->ctrChSelInfo.chIdentifier = (uint16_t)(pBigCtx->ctrChan.accAddr >> 16) ^
                                       (uint16_t)(pBigCtx->ctrChan.accAddr >> 0);
  LmgrBuildRemapTable(&pBigCtx->ctrChSelInfo);

  pBigCtx->ctrChan.crcInit = (pBigCtx->baseCrcInit << 8) | 0;
  pBigCtx->ctrChan.rxPhy = pBigCtx->phy;

#if (LL_ENABLE_TESTER)
  pBigCtx->ctrChan.accAddrRx = pBigCtx->ctrChan.accAddr ^ llTesterCb.dataAccessAddrRx;
  pBigCtx->ctrChan.accAddrTx = pBigCtx->ctrChan.accAddr ^ llTesterCb.dataAccessAddrTx;
  pBigCtx->ctrChan.crcInitRx = pBigCtx->ctrChan.crcInit ^ llTesterCb.dataCrcInitRx;
  pBigCtx->ctrChan.crcInitTx = pBigCtx->ctrChan.crcInit ^ llTesterCb.dataCrcInitTx;
#endif

  if (pBigCtx->encrypt)
  {
    pBigCtx->ctrChan.enc.enaDecrypt = TRUE;
    pBigCtx->ctrChan.enc.enaAuth = TRUE;
    pBigCtx->ctrChan.enc.nonceMode = PAL_BB_NONCE_MODE_EXT64_CNTR;

    memcpy(pBigCtx->ctrChan.enc.iv, pBigCtx->giv, LL_IV_LEN);
    pBigCtx->ctrChan.enc.iv[0] ^= pBigCtx->ctrChan.accAddr >>  0;
    pBigCtx->ctrChan.enc.iv[1] ^= pBigCtx->ctrChan.accAddr >>  8;
    pBigCtx->ctrChan.enc.iv[2] ^= pBigCtx->ctrChan.accAddr >> 16;
    pBigCtx->ctrChan.enc.iv[3] ^= pBigCtx->ctrChan.accAddr >> 24;

    memcpy(pBigCtx->ctrChan.enc.sk, pBigCtx->bleData.chan.enc.sk, PAL_CRYPTO_LL_KEY_LEN);

    /* The directionBit shall be set to 1 for Broadcast Isochronous PDUs. */
    pBigCtx->ctrChan.enc.dir = 1;
    pBigCtx->ctrChan.enc.type = PAL_BB_TYPE_BIS;

    if (lctrInitCipherBlkHdlr)
    {
      lctrInitCipherBlkHdlr(&pBigCtx->ctrChan.enc, LCTR_BIG_CTRL_ENC_ID(pBigCtx), 1);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal master BIG subsystem message.
 *
 *  \param  pBigCtx     BIG context.
 *  \param  event       Master BIG event.
 */
/*************************************************************************************************/
void lctrMstBigSendMsg(lctrBigCtx_t *pBigCtx, LctrMstBigMsg_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t))) != NULL)
  {
    pMsg->handle = pBigCtx->handle;
    pMsg->dispId = LCTR_DISP_BIG_SYNC;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set default values for BIS.
 */
/*************************************************************************************************/
void lctrBisDefaults(void)
{
  memset(pLctrBisTbl, 0, sizeof(lctrBisCtx_t) * pLctrRtCfg->maxBis);
  memset(pLctrBigTbl, 0, sizeof(lctrBigCtx_t) * pLctrRtCfg->maxBig);
}
