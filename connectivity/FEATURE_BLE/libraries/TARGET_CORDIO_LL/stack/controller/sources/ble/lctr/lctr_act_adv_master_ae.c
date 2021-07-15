/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master scan action routines.
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

#include "lctr_int_adv_master_ae.h"
#include "lctr_int_adv_master.h"
#include "lctr_api_adv_master_ae.h"
#include "lmgr_api_adv_master_ae.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "bb_ble_api_reslist.h"
#include "util/bstream.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief      Setup periodic scanning context.
 *
 *  \param      pPerCreateSync  Create sync control block.
 *  \param      pMsg            Create sync message.
 *  \param      createDispId    Dispatcher ID.
 *
 *  \return     TRUE if successful, FALSE otherwise.
 */
/*************************************************************************************************/
static uint8_t lctrPerScanSetup(lctrPerCreateSyncCtrlBlk_t *pPerCreateSync, lctrPerCreateSyncMsg_t *pMsg, uint8_t createDispId)
{
  lctrPerScanCtx_t *pPerScanCtx;

  if ((pPerScanCtx = lctrAllocPerScanCtx()) == NULL)
  {
    return LL_ERROR_CODE_CONN_LIMIT_EXCEEDED;
  }

  pPerCreateSync->pPerScanCtx = pPerScanCtx;
  pPerScanCtx->filtParam.filterPolicy = pMsg->filterPolicy;
  pPerScanCtx->filtParam.advSID = pMsg->advSID;
  pPerScanCtx->filtParam.advAddrType = pMsg->advAddrType;
  pPerScanCtx->filtParam.advAddr = pMsg->advAddr;
  pPerScanCtx->skip = pMsg->skip;
  pPerScanCtx->syncTimeOutMs = LCTR_PER_SYNC_TIMEOUT_TO_MS(pMsg->syncTimeOut);
  pPerScanCtx->createDispId = createDispId;
  pPerScanCtx->repDisabled = pMsg->repDisabled;
  pPerScanCtx->dupFilterEnable = pMsg->dupFilterEnable;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Pack periodic advertising sync established report.
 *
 *  \param      pPerScanCtx Periodic scan context.
 *  \param      pRpt        Periodic advertising report.
 *
 *  \return     TRUE if pack report successful, FALSE otherwise.
 */
/*************************************************************************************************/
static bool_t lctrPerAdvSyncEstRptPack(lctrPerScanCtx_t *pPerScanCtx, lmgrPerAdvSyncEstdInd_t *pRpt)
{
  memset(pRpt, 0, sizeof(lmgrPerAdvSyncEstdInd_t));

  pRpt->advSID = pPerScanCtx->advSID;
  Bda64ToBstream(pRpt->addr, pPerScanCtx->advAddr);
  pRpt->addrType = pPerScanCtx->advAddrType;
  pRpt->advPhy = pPerScanCtx->rxPhys;
  pRpt->advInterval = LCTR_PER_INTER_TO_MS(pPerScanCtx->perInterUsec);
  pRpt->advClkAccuracy = pPerScanCtx->sca;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Common periodic scan resource cleanup.
 *
 *  \param      pPerScanCtx     Periodic scan context.
 */
/*************************************************************************************************/
static void lctrMstPerScanCleanupOp(lctrPerScanCtx_t *pPerScanCtx)
{
  BbStop(BB_PROT_BLE);

  if (pPerScanCtx->termCback)
  {
    pPerScanCtx->termCback(LCTR_GET_PER_SCAN_HANDLE(pPerScanCtx));
  }

  SchTmRemove(LCTR_GET_PER_SCAN_TM_HANDLE(pPerScanCtx));

  if (pPerScanCtx->filtParam.filterPolicy)
  {
    LmgrDecPeriodiclistRefCount();
  }

  LmgrDecResetRefCount();

  pPerScanCtx->enabled = FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Common scan resource cleanup.
 *
 *  \param      pExtScanCtx     Extended scan context.
 */
/*************************************************************************************************/
static void lctrMstExtScanCleanupOp(lctrExtScanCtx_t *pExtScanCtx)
{
  BbStop(BB_PROT_BLE);

  switch (pExtScanCtx->scanParam.scanFiltPolicy)
  {
    case LL_SCAN_FILTER_WL_BIT:
    case LL_SCAN_FILTER_WL_OR_RES_INIT:
      LmgrDecWhitelistRefCount();
      break;
    default:
      break;
  }

  BbBleMstAdvEvent_t * const pScan = &pExtScanCtx->scanBleData.op.mstAdv;
  if (pScan->pRxAdvBuf)
  {
    WsfMsgFree(pScan->pRxAdvBuf);
    pScan->pRxAdvBuf = NULL;
  }
  if (pScan->pRxRspBuf)
  {
    WsfMsgFree(pScan->pRxRspBuf);
    pScan->pRxRspBuf = NULL;
  }

  WSF_ASSERT(lmgrCb.numScanEnabled > 0);
  lmgrCb.numScanEnabled--;
  LmgrDecResetRefCount();
}

/*************************************************************************************************/
/*!
 *  \brief      Start scan discovery.
 *
 *  \param      pExtScanCtx     Extended scan context.
 */
/*************************************************************************************************/
void lctrExtScanActDiscover(lctrExtScanCtx_t *pExtScanCtx)
{
  BbStart(BB_PROT_BLE);

  switch (pExtScanCtx->scanParam.scanFiltPolicy)
  {
    case LL_SCAN_FILTER_WL_BIT:
    case LL_SCAN_FILTER_WL_OR_RES_INIT:
      LmgrIncWhitelistRefCount();
      break;
    default:
      break;
  }

  /* Reset state. */
  pExtScanCtx->selfTerm = FALSE;
  pExtScanCtx->shutdown = FALSE;
  pExtScanCtx->auxOpPending = FALSE;
  pExtScanCtx->bodTermCnt = 0;
  pExtScanCtx->data.scan.advRptState = LCTR_RPT_STATE_IDLE;

  uint8_t status;

  if (((status = lctrMstExtDiscoverBuildOp(pExtScanCtx)) != LL_SUCCESS) ||
      ((status = lctrMstAuxDiscoverBuildOp(pExtScanCtx)) != LL_SUCCESS))
  {
    LmgrSendExtScanEnableCnf(status);
    lctrSendExtScanMsg(pExtScanCtx, LCTR_EXT_SCAN_MSG_TERMINATE);
    return;
  }

  lmgrCb.numScanEnabled++;
  LmgrIncResetRefCount();

  pExtScanCtx->data.scan.backoffCount = 1;
  pExtScanCtx->data.scan.upperLimit = 1;

  LmgrSendExtScanEnableCnf(LL_SUCCESS);
}

/*************************************************************************************************/
/*!
 *  \brief      Update scan discovery.
 *
 *  \param      pExtScanCtx     Extended scan context.
 */
/*************************************************************************************************/
void lctrExtScanActUpdateDiscover(lctrExtScanCtx_t *pExtScanCtx)
{
  LmgrSendExtScanEnableCnf(LL_SUCCESS);
}

/*************************************************************************************************/
/*!
 *  \brief      Shutdown active scan operation.
 *
 *  \param      pExtScanCtx     Extended scan context.
 */
/*************************************************************************************************/
void lctrExtScanActShutdown(lctrExtScanCtx_t *pExtScanCtx)
{
  const uint8_t scanPhyIndex = (LCTR_GET_EXT_SCAN_HANDLE(pExtScanCtx) == LCTR_SCAN_PHY_CODED) ? LCTR_SCAN_PHY_CODED : LCTR_SCAN_PHY_1M;

  pExtScanCtx->shutdown = TRUE;

  if (scanPhyIndex == lctrActiveExtScan.scanIndex)
  {
    if (!pExtScanCtx->auxOpPending)
    {
      SchRemove(&pExtScanCtx->scanBod);
    }
    else
    {
      SchRemove(&pExtScanCtx->auxScanBod);
    }

    /* Shutdown completes with events generated in BOD end callback. */
  }
  else
  {
    /* BOD of this scan context is not scheduled. No need to remove it. */
    lctrActiveExtScan.scanMask &= ~(1 << scanPhyIndex);
    lctrSendExtScanMsg(pExtScanCtx, LCTR_EXT_SCAN_MSG_TERMINATE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send scan operation confirm.
 *
 *  \param      pExtScanCtx     Extended scan context.
 */
/*************************************************************************************************/
void lctrExtScanActScanCnf(lctrExtScanCtx_t *pExtScanCtx)
{
  LmgrSendExtScanEnableCnf(LL_SUCCESS);
}

/*************************************************************************************************/
/*!
 *  \brief      Terminate a restarting scan (scan in terminate state due to controller-issued terminate).
 *
 *  \param      pExtScanCtx     Extended scan context.
 */
/*************************************************************************************************/
void lctrExtScanHostDisable(lctrExtScanCtx_t *pExtScanCtx)
{
  if (lctrMstExtScan.scanTermByHost > 1)
  {
    LmgrSendExtScanEnableCnf(LL_ERROR_CODE_CMD_DISALLOWED);
    return;
  }

  /* Start/Restart timers. */
  WsfTimerStop(&lctrMstExtScan.tmrScanDur);
  WsfTimerStop(&lctrMstExtScan.tmrScanPer);
}

/*************************************************************************************************/
/*!
 *  \brief      Send disallow scan host notification.
 *
 *  \param      pExtScanCtx     Extended scan context.
 */
/*************************************************************************************************/
void lctrExtScanActDisallowScan(lctrExtScanCtx_t *pExtScanCtx)
{
  if (lctrMstExtScan.scanTermByHost)
  {
    LmgrSendExtScanEnableCnf(LL_ERROR_CODE_CMD_DISALLOWED);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send enable during terminate state.
 *
 *  \param      pExtScanCtx     Extended scan context.
 */
/*************************************************************************************************/
void lctrExtScanActHostEnable(lctrExtScanCtx_t *pExtScanCtx)
{
  if (lctrMstExtScan.scanTermByHost)
  {
    LmgrSendExtScanEnableCnf(LL_ERROR_CODE_CMD_DISALLOWED);
  }
  else
  {
    LmgrSendExtScanEnableCnf(LL_SUCCESS);
  }

}

/*************************************************************************************************/
/*!
 *  \brief      Terminated scan after host scan disable.
 *
 *  \param      pExtScanCtx     Extended scan context.
 */
/*************************************************************************************************/
void lctrExtScanActScanTerm(lctrExtScanCtx_t *pExtScanCtx)
{
  lctrMstExtScanCleanupOp(pExtScanCtx);

  if (lctrMstExtScan.scanTermByHost)
  {
    LmgrSendExtScanEnableCnf(LL_SUCCESS);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Terminated scan after internal scan disable.
 *
 *  \param      pExtScanCtx     Extended scan context.
 */
/*************************************************************************************************/
void lctrExtScanActSelfTerm(lctrExtScanCtx_t *pExtScanCtx)
{
  lctrMstExtScanCleanupOp(pExtScanCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Create sync action function.
 */
/*************************************************************************************************/
void lctrCreateSyncActCreate(void)
{
  lctrPerCreateSyncMsg_t *pMsg  = (lctrPerCreateSyncMsg_t *)pLctrMstPerScanMsg;

  BbStart(BB_PROT_BLE);

  if (pMsg->filterPolicy)
  {
    LmgrIncPeriodiclistRefCount();
  }

  lctrPerCreateSync.filtParam.filterPolicy = pMsg->filterPolicy;
  lctrPerCreateSync.filtParam.advAddr = pMsg->advAddr;
  lctrPerCreateSync.filtParam.advAddrType = pMsg->advAddrType;
  lctrPerCreateSync.filtParam.advSID = pMsg->advSID;

  lctrPerScanSetup(&lctrPerCreateSync, pMsg, LCTR_DISP_PER_CREATE_SYNC);
  lctrMstPerScanBuildOp(lctrPerCreateSync.pPerScanCtx);

  LmgrIncResetRefCount();
}

/*************************************************************************************************/
/*!
 *  \brief      Create sync done action function.
 */
/*************************************************************************************************/
void lctrCreateSyncActDone(void)
{
  if (lctrPerCreateSync.createSyncPending == TRUE)
  {
    lctrPerCreateSync.createSyncPending = FALSE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Create sync cancel action function.
 */
/*************************************************************************************************/
void lctrCreateSyncActCancel(void)
{
  if (lctrPerCreateSync.createSyncPending == TRUE)
  {
    lctrPerCreateSync.pPerScanCtx->cancelCreateSync = TRUE;
    SchRemove(&lctrPerCreateSync.pPerScanCtx->bod);
  }

  lctrPerCreateSync.pPerScanCtx->cancelByHost = TRUE;
  lctrSendCreateSyncMsg(lctrPerCreateSync.pPerScanCtx, LCTR_CREATE_SYNC_MSG_TERMINATE);
}

/*************************************************************************************************/
/*!
 *  \brief      Create sync failed action function.
 */
/*************************************************************************************************/
void lctrCreateSyncActFailed(void)
{
  if (lctrPerCreateSync.createSyncPending == TRUE)
  {
    lctrPerCreateSync.pPerScanCtx->cancelCreateSync = TRUE;
    lctrPerCreateSync.pPerScanCtx->cancelByHost = FALSE;
    SchRemove(&lctrPerCreateSync.pPerScanCtx->bod);

    lctrPerCreateSync.createSyncPending = FALSE;
  }

  /* Pack and notify the host about sync established event with failed status. */
  lmgrPerAdvSyncEstdInd_t rpt;
  lctrPerAdvSyncEstRptPack(lctrPerCreateSync.pPerScanCtx, &rpt);
  LmgrSendSyncEstInd(LL_ERROR_CODE_CONN_FAILED_TO_ESTABLISH, LCTR_GET_PER_SCAN_HANDLE(lctrPerCreateSync.pPerScanCtx), &rpt);
}

/*************************************************************************************************/
/*!
 *  \brief      Create sync terminate action function.
 */
/*************************************************************************************************/
void lctrCreateSyncActTerminate(void)
{
  if (lctrPerCreateSync.pPerScanCtx->cancelCreateSync == TRUE)
  {
    lctrPerCreateSync.pPerScanCtx->cancelCreateSync = FALSE;
  }

  lctrMstPerScanCleanupOp(lctrPerCreateSync.pPerScanCtx);

  if (lctrPerCreateSync.pPerScanCtx->cancelByHost)
  {
    lmgrPerAdvSyncEstdInd_t rpt;
    memset(&rpt, 0, sizeof(lmgrPerAdvSyncEstdInd_t));
    LmgrSendSyncEstInd(LL_ERROR_CODE_OP_CANCELLED_BY_HOST, LCTR_GET_PER_SCAN_HANDLE(lctrPerCreateSync.pPerScanCtx), &rpt);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Transfer sync start action function.
 */
/*************************************************************************************************/
void lctrTransferSyncActStart(void)
{
  lctrPerScanCtx_t *pPerScanCtx;

  lctrPerTransferSyncMsg_t *pMsg  = (lctrPerTransferSyncMsg_t *)pLctrMstPerScanMsg;

  if ((pPerScanCtx = lctrAllocPerScanCtx()) == NULL)
  {
    return;
  }

  BbStart(BB_PROT_BLE);

  lctrPerTransferSync.pPerScanCtx = pPerScanCtx;

  pPerScanCtx->filtParam.filterPolicy = LL_PER_SCAN_FILTER_NONE;    //TODO: to be handled later.
  pPerScanCtx->filtParam.advSID = pMsg->advSID;
  pPerScanCtx->filtParam.advAddrType = pMsg->advAddrType;
  pPerScanCtx->filtParam.advAddr = pMsg->advAddr;
  pPerScanCtx->rxPhys = pMsg->rxPhy;

  pPerScanCtx->syncTimeOutMs = LCTR_PER_SYNC_TIMEOUT_TO_MS(100);    //TODO: to be handled later.
  pPerScanCtx->skip = 0;                                            //TODO: to be handled later.
  pPerScanCtx->createDispId = LCTR_DISP_TRANFER_SYNC;

  /* Decode syncInfo */
  lctrUnpackSyncInfo(&trsfSyncInfo, pMsg->bSyncInfo);
  pPerScanCtx->perInterUsec = LCTR_PER_INTER_TO_US(trsfSyncInfo.syncInter);
  pPerScanCtx->advSID = pMsg->advSID;
  pPerScanCtx->advAddrType = pMsg->advAddrType;
  pPerScanCtx->advAddr = pMsg->advAddr;

  /* Check if received address is resolvable. */
  if ((pMsg->advAddrType & LL_ADDR_RANDOM_BIT) && BDA64_ADDR_IS_RPA(pMsg->advAddr))
  {
    uint64_t peerIdAddr = 0;
    uint8_t peerIdAddrType = 0;

    if (BbBleResListResolvePeer(pMsg->advAddr, &peerIdAddrType, &peerIdAddr))
    {
      pPerScanCtx->advAddrType = peerIdAddrType | LL_ADDR_IDENTITY_BIT;
      pPerScanCtx->advAddr = peerIdAddr;
    }
  }

  LL_TRACE_INFO1("Periodic sync transfer -- syncInfo --, syncOffset=%u", trsfSyncInfo.syncOffset);
  LL_TRACE_INFO1("                                       offsetUnits=%u", trsfSyncInfo.offsetUnits);
  LL_TRACE_INFO1("                                       syncInter=%u", trsfSyncInfo.syncInter);
  LL_TRACE_INFO1("                                       eventCounter=%u", trsfSyncInfo.eventCounter);

  lctrMstPerScanBuildOp(lctrPerTransferSync.pPerScanCtx);

  LmgrIncResetRefCount();

  /* All information is transfered, we can start scanning periodic sync immediately. */
  lctrPerTransferSync.connHandle = pMsg->connHandle;
  lctrPerTransferSync.serviceData = pMsg->id;
  lctrPerTransferSync.ceRef = pMsg->ceRef;
  lctrPerTransferSync.ceRcvd = pMsg->ceRcvd;
  lctrPerTransferSync.syncCe = pMsg->syncConnEvtCounter;
  lctrPerTransferSync.scaB = pMsg->scaB;
  lctrPerTransferSync.lastPECounter = pMsg->lastPECounter;
  lctrMstPerScanTransferOpCommit(pMsg->connHandle);

  lctrMstPerScanIsrInit();
}

/*************************************************************************************************/
/*!
 *  \brief      Transfer sync done action function.
 */
/*************************************************************************************************/
void lctrTransferSyncActDone(void)
{
}

/*************************************************************************************************/
/*!
 *  \brief      Transfer sync failed action function.
 */
/*************************************************************************************************/
void lctrTransferSyncActFailed(void)
{
  lctrPerTransferSync.pPerScanCtx->cancelCreateSync = TRUE;
  SchRemove(&lctrPerTransferSync.pPerScanCtx->bod);

  /* Notify the host that controller failed to receive AUX_SYNC_IND within 6 periodic ADV events. */
  LctrSendPerSyncTrsfRcvdEvt(LL_ERROR_CODE_CONN_FAILED_TO_ESTABLISH, lctrPerTransferSync.pPerScanCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Transfer sync cancel action function.
 */
/*************************************************************************************************/
void lctrTransferSyncActCancel(void)
{
  lctrPerTransferSync.pPerScanCtx->cancelCreateSync = TRUE;
  SchRemove(&lctrPerTransferSync.pPerScanCtx->bod);
}

/*************************************************************************************************/
/*!
 *  \brief      Transfer sync terminate action function.
 */
/*************************************************************************************************/
void lctrTransferSyncActTerminate(void)
{
  if (lctrPerTransferSync.pPerScanCtx->cancelCreateSync == TRUE)
  {
    lctrPerTransferSync.pPerScanCtx->cancelCreateSync = FALSE;
  }

  lctrMstPerScanCleanupOp(lctrPerTransferSync.pPerScanCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Periodic scanning sync established action function.
 *
 *  \param      pPerScanCtx     Periodic scan context.
 */
/*************************************************************************************************/
void lctrPerScanActSyncEstd(lctrPerScanCtx_t *pPerScanCtx)
{
  if (pPerScanCtx->createDispId == LCTR_DISP_PER_CREATE_SYNC)
  {
    /* Pack and notify the host about sync established event. */
    lmgrPerAdvSyncEstdInd_t rpt;

    lctrPerAdvSyncEstRptPack(pPerScanCtx, &rpt);
    LmgrSendSyncEstInd(LL_SUCCESS, LCTR_GET_PER_SCAN_HANDLE(pPerScanCtx), &rpt);
  }
  else if (pPerScanCtx->createDispId == LCTR_DISP_TRANFER_SYNC)
  {
    /* Notify the host about periodic sync transfer received event. */
    LctrSendPerSyncTrsfRcvdEvt(LL_SUCCESS, pPerScanCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Periodic scanning sync terminate action function.
 *
 *  \param      pPerScanCtx     Periodic scan context.
 */
/*************************************************************************************************/
void lctrPerScanActSyncTerminate(lctrPerScanCtx_t *pPerScanCtx)
{
  pPerScanCtx->shutdown = TRUE;

  SchRemove(&pPerScanCtx->bod);

  /* Shutdown completes with events generated in BOD end callback. */
}

/*************************************************************************************************/
/*!
 *  \brief      Periodic scanning sync terminate done action function.
 *
 *  \param      pPerScanCtx     Periodic scan context.
 */
/*************************************************************************************************/
void lctrPerScanActSyncTerminateDone(lctrPerScanCtx_t *pPerScanCtx)
{
  lctrMstPerScanCleanupOp(pPerScanCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Periodic scanning sync terminate action function.
 *
 *  \param      pPerScanCtx     Periodic scan context.
 */
/*************************************************************************************************/
void lctrPerScanActSyncTimeout(lctrPerScanCtx_t *pPerScanCtx)
{
  pPerScanCtx->shutdown = TRUE;

  SchRemove(&pPerScanCtx->bod);

  WsfTimerStop(&pPerScanCtx->tmrSupTimeout);

  LmgrSendSyncLostInd(LCTR_GET_PER_SCAN_HANDLE(pPerScanCtx));

  /* Shutdown completes with events generated in BOD end callback. */
}

/*************************************************************************************************/
/*!
 *  \brief      Process ACAD that need to be serviced.
 *
 *  \param      pMsg     ACAD message.
 */
/*************************************************************************************************/
void lctrPerScanActProcessAcad(lctrAcadMsg_t *pMsg)
{
  lctrPerScanCtx_t *pPerScanCtx = LCTR_GET_PER_SCAN_CTX(pMsg->hdr.handle);

  switch (pMsg->hdr.acadId)
  {
    case LCTR_ACAD_ID_CHAN_MAP_UPDATE:
    {
      LctrAcadChanMapUpd_t *pChanMapUpd = &pPerScanCtx->acadParams[LCTR_ACAD_ID_CHAN_MAP_UPDATE].chanMapUpdate;

      if ((pChanMapUpd->instant - pMsg->hdr.eventCtr) <= pMsg->hdr.skip)
      {
        pPerScanCtx->chanParam.chanMask = pChanMapUpd->chanMask;
        LmgrBuildRemapTable(&pPerScanCtx->chanParam);
        pChanMapUpd->hdr.state = LCTR_ACAD_STATE_DISABLED;
      }
      break;
    }

    case LCTR_ACAD_ID_BIG_INFO:
    {
      LctrAcadBigInfo_t *pBigInfo = &pPerScanCtx->acadParams[LCTR_ACAD_ID_BIG_INFO].bigInfo;
      /* No action required. */
      pBigInfo->hdr.state = LCTR_ACAD_STATE_DISABLED;
      break;
    }

    default:
      break;
  }
}
