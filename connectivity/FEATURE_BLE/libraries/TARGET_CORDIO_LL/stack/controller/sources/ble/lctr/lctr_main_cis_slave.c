/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller connected isochronous stream slave operation builder implementation file.
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

#include "lctr_int_iso.h"
#include "lctr_int_cis_slave.h"
#include "lctr_int_conn_slave.h"
#include "lctr_int_conn.h"
#include "lctr_int.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "bb_api.h"
#include "pal_bb.h"
#include "wsf_assert.h"
#include "wsf_buf.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Globals
**************************************************************************************************/

/**************************************************************************************************
  Internal functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      CIS slave reset handler.
 */
/*************************************************************************************************/
static void lctrSlvCisResetHandler(void)
{
  BbBleCisSlaveInit();
  BbBleCisMasterInit();
  lctrCisDefaults();
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for CIS slave.
 */
/*************************************************************************************************/
void LctrCisSlvInit(void)
{
  /* Add reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_CIS] = lctrSlvCisResetHandler;

  /* Add connection message dispatcher. */
  lctrMsgDispTbl[LCTR_DISP_CIS] = (LctrMsgDisp_t)lctrCisDisp;

  /* Add CIS function pointers */
  LctrUpdateCisChanMapFn = LctrCisUpdateChanMap;
  lctrCisServicePowerMonitorFn = lctrCisServicePowerMonitor;

  /* Add connection event handlers. */
  lctrEventHdlrTbl[LCTR_EVENT_CIS_RX_PENDING]  = lctrCisRxPendingHandler;
  lctrEventHdlrTbl[LCTR_EVENT_ISO_TX_COMPLETE] = lctrIsoTxCompletedHandler;

  /* Add LLCP SM handler. */
  lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CIS_EST]      = lctrSlvLlcpExecuteCisEstSm;
  lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CIS_TERM]      = lctrLlcpExecuteCisTermSm;

  lctrCisDefaults();

  /* Set supported features. */
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_1)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_CIS_SLAVE_ROLE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Reject CIS request.
 *
 *  \param  cisHandle     CIS handle.
 *  \param  reason        Reason.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrRejectCisReq(uint16_t cisHandle, uint8_t reason)
{
  lctrCisCtx_t *pCisCtx = lctrFindCisByHandle(cisHandle);

  if (pCisCtx == NULL)
  {
    LL_TRACE_WARN0("LctrRejectCisReq, invalid CIS handle");
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

  if (pCtx == NULL)
  {
    LL_TRACE_WARN0("LctrRejectCisReq, invalid ACL handle");
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  if (pCtx->role == LL_ROLE_MASTER)
  {
    LL_TRACE_WARN0("LctrRejectCisReq, invalid role");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  lctrRejCisReq_t *pMsg;

  if ((pMsg = (lctrRejCisReq_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.handle = pCisCtx->aclHandle;
    pMsg->hdr.dispId = LCTR_DISP_CONN;
    pMsg->hdr.event = LCTR_CONN_MSG_API_CIS_REQ_REJECT;
    pMsg->reason = reason;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Accept CIS request.
 *
 *  \param  cisHandle     CIS handle.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrAcceptCisReq(uint16_t cisHandle)
{
  lctrCisCtx_t *pCisCtx = lctrFindCisByHandle(cisHandle);

  if (pCisCtx == NULL)
  {
    LL_TRACE_WARN0("LctrAcceptCisReq, invalid CIS handle");
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

  if (pCtx == NULL)
  {
    LL_TRACE_WARN0("LctrAcceptCisReq, invalid ACL handle");
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  if (pCtx->role == LL_ROLE_MASTER)
  {
    LL_TRACE_WARN0("LctrAcceptCisReq, invalid role");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  lctrMsgHdr_t *pMsg;

  if ((pMsg = (lctrMsgHdr_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->handle = pCisCtx->aclHandle;
    pMsg->dispId = LCTR_DISP_CONN;
    pMsg->event = LCTR_CONN_MSG_API_CIS_REQ_ACCEPT;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Build a CIG operation.
 *
 *  \param  pCigCtx     CIG context.
 */
/*************************************************************************************************/
void lctrSlvCisBuildCigOp(lctrCigCtx_t *pCigCtx)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &pCigCtx->cigBod;

  /* When build the BOD, always setup channel use the first CIS context in the CIG. */
  lctrCisCtx_t  *pCisCtx = lctrCisGetHeadCis(&pCigCtx->list);
  pCigCtx->pCisCtx = pCisCtx;

  BbBleData_t * const pBle = &pCisCtx->bleData;
  BbBleSlvCisEvent_t * const pCis = &pBle->op.slvCis;
  lctrConnCtx_t * const pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));
  memset(pCis, 0, sizeof(BbBleMstCisEvent_t));

  /*** CIG context setup ***/

  /* pCigCtx->cigEvtCounter = 0; */            /* cleared in alloc. */

  /*** CIS context setup ***/

  /* pCisCtx->cisEvtCounter = 0; */            /* cleared in alloc. */
  /* pCisCtx->txHdr.sn = 0; */                 /* cleared in alloc */
  /* pCisCtx->txHdr.nesn = 0; */               /* cleared in alloc */

  /*** BLE general setup ***/

  pBle->chan.opType = BB_BLE_OP_SLV_CIS_EVENT;

  pBle->chan.chanIdx = pCisCtx->chIdx;         /* Set in the lctrCisSetupChanParam. */
  pBle->chan.txPower = pLctrRtCfg->defTxPwrLvl;
  pBle->chan.accAddr = pCisCtx->accessAddr;
  pBle->chan.crcInit = pCisCtx->crcInit;
  pBle->chan.txPhy = pCisCtx->phySToM;
  pBle->chan.rxPhy = pCisCtx->phyMToS;
  pBle->chan.peerTxStableModIdx = TRUE;
  pBle->chan.peerRxStableModIdx = TRUE;

  if (lmgrGetOpFlag(LL_OP_MODE_FLAG_FORCE_CIS_CODED_PHY_S2))
  {
    /* Force Coded PHY option to S2. */
    pBle->chan.initTxPhyOptions = BB_PHY_OPTIONS_BLE_S2;
    pBle->chan.tifsTxPhyOptions = BB_PHY_OPTIONS_BLE_S2;
  }
  else
  {
    /* Set PHY options to mirror ACL connection option. */
    pBle->chan.initTxPhyOptions = pConnCtx->bleData.chan.initTxPhyOptions;
    pBle->chan.tifsTxPhyOptions = pConnCtx->bleData.chan.tifsTxPhyOptions;
  }

#if (LL_ENABLE_TESTER)
  pBle->chan.accAddrRx = pCisCtx->accessAddr ^ llTesterCb.cisAccessAddrRx;
  pBle->chan.accAddrTx = pCisCtx->accessAddr ^ llTesterCb.cisAccessAddrRx;
  pBle->chan.crcInitRx = pCisCtx->crcInit    ^ llTesterCb.cisCrcInitRx;
  pBle->chan.crcInitTx = pCisCtx->crcInit    ^ llTesterCb.cisCrcInitTx;
#endif

  /* pBle->chan.enc.enaEncrypt = FALSE; */  /* cleared in alloc */
  /* pBle->chan.enc.enaDecrypt = FALSE; */  /* cleared in alloc */
  pBle->chan.enc.enaAuth = TRUE;
  pBle->chan.enc.nonceMode = PAL_BB_NONCE_MODE_EXT64_CNTR;
  pBle->chan.enc.pTxPktCounter = &pCisCtx->txPktCounter;
  pBle->chan.enc.pRxPktCounter = &pCisCtx->rxPktCounter;

  /*** General setup ***/

  /* Use small minDurUsec to improve scheduling. */
  pOp->minDurUsec = pCisCtx->subIntervUsec;
  pOp->maxDurUsec = pCigCtx->cigSyncDelayUsec;

  /* pOp->due = 0 */  /* set in lctrCisMstCigOpCommit() */
  pOp->reschPolicy = BB_RESCH_FIXED_PREFERRED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrSlvCisCigEndOp;
  pOp->abortCback = lctrSlvCisCigAbortOp;
  pOp->pCtx = pCigCtx;

  /*** BLE stream setup ***/

  pCis->checkContOpCback = lctrSlvCisCheckContOp;
  pCis->checkContOpPostCback = lctrSlvCisCheckContOpPostCback;
  pCis->execCback = lctrSlvCisCigBeginOp;
  pCis->contExecCback = lctrSlvCisCigContOp;
  pCis->postSubEvtCback = lctrSlvCisCigPostSubEvt;
  pCis->cancelCback = lctrSlvCisCigCleanupOp;
  pCis->txDataCback = lctrSlvCisCigTxCompletion;
  pCis->rxDataCback = lctrSlvCisCigRxCompletion;

  /*** Commit operation ***/

  /* Postponed in lctrCisMstCigOpCommit(). */
  pCigCtx->isBodBuilt = TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Build a CIG operation.
 *
 *  \param  pCisCtx     CIG context.
 */
/*************************************************************************************************/
void lctrSlvCisBuildCisData(lctrCisCtx_t *pCisCtx)
{
  BbBleData_t * const pBle = &pCisCtx->bleData;
  BbBleSlvCisEvent_t * const pCis = &pBle->op.slvCis;
  lctrConnCtx_t * const pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

  memset(pBle, 0, sizeof(BbBleData_t));
  memset(pCis, 0, sizeof(BbBleMstCisEvent_t));

  /*** CIS context setup ***/

  /* pCisCtx->cisEvtCounter = 0; */            /* cleared in alloc. */
  /* pCisCtx->txHdr.sn = 0; */                 /* cleared in alloc */
  /* pCisCtx->txHdr.nesn = 0; */               /* cleared in alloc */

  /*** BLE general setup ***/

  pBle->chan.opType = BB_BLE_OP_SLV_CIS_EVENT;

  pBle->chan.chanIdx = pCisCtx->chIdx;         /* Set in the lctrCisSetupChanParam. */
  pBle->chan.txPower = pLctrRtCfg->defTxPwrLvl;
  pBle->chan.accAddr = pCisCtx->accessAddr;
  pBle->chan.crcInit = pCisCtx->crcInit;
  pBle->chan.txPhy = pCisCtx->phySToM;
  pBle->chan.rxPhy = pCisCtx->phyMToS;
  pBle->chan.peerTxStableModIdx = TRUE;
  pBle->chan.peerRxStableModIdx = TRUE;

  if (lmgrGetOpFlag(LL_OP_MODE_FLAG_FORCE_CIS_CODED_PHY_S2))
  {
    /* Force Coded PHY option to S2. */
    pBle->chan.initTxPhyOptions = BB_PHY_OPTIONS_BLE_S2;
    pBle->chan.tifsTxPhyOptions = BB_PHY_OPTIONS_BLE_S2;
  }
  else
  {
    /* Set PHY options to mirror ACL connection option. */
    pBle->chan.initTxPhyOptions = pConnCtx->bleData.chan.initTxPhyOptions;
    pBle->chan.tifsTxPhyOptions = pConnCtx->bleData.chan.tifsTxPhyOptions;
  }

#if (LL_ENABLE_TESTER)
  pBle->chan.accAddrRx = pCisCtx->accessAddr ^ llTesterCb.cisAccessAddrRx;
  pBle->chan.accAddrTx = pCisCtx->accessAddr ^ llTesterCb.cisAccessAddrRx;
  pBle->chan.crcInitRx = pCisCtx->crcInit    ^ llTesterCb.cisCrcInitRx;
  pBle->chan.crcInitTx = pCisCtx->crcInit    ^ llTesterCb.cisCrcInitTx;
#endif

  /* pBle->chan.enc.enaEncrypt = FALSE; */  /* cleared in alloc */
  /* pBle->chan.enc.enaDecrypt = FALSE; */  /* cleared in alloc */
  pBle->chan.enc.enaAuth = TRUE;
  pBle->chan.enc.nonceMode = PAL_BB_NONCE_MODE_EXT64_CNTR;
  pBle->chan.enc.pTxPktCounter = &pCisCtx->txPktCounter;
  pBle->chan.enc.pRxPktCounter = &pCisCtx->rxPktCounter;

  /*** BLE stream setup ***/

  pCis->checkContOpCback = lctrSlvCisCheckContOp;
  pCis->checkContOpPostCback = lctrSlvCisCheckContOpPostCback;
  pCis->execCback   = lctrSlvCisCigBeginOp;
  pCis->contExecCback   = lctrSlvCisCigContOp;
  pCis->postSubEvtCback = lctrSlvCisCigPostSubEvt;
  pCis->cancelCback = lctrSlvCisCigCleanupOp;
  pCis->txDataCback = lctrSlvCisCigTxCompletion;
  pCis->rxDataCback = lctrSlvCisCigRxCompletion;
}

/*************************************************************************************************/
/*!
 *  \brief  Commit a CIG operation.
 *
 *  \param  pCigCtx     CIG context.
 *  \param  pCtx        ACL Context.
 *  \param  pCisCtx     CIS context.
 */
/*************************************************************************************************/
void lctrSlvCisCigOpCommit(lctrCigCtx_t *pCigCtx, lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  BbOpDesc_t * const pOp = &pCigCtx->cigBod;
  /* When commit the BOD, always setup channel use the first CIS context in the CIG. */
  lctrCisCtx_t  *pFirstCisCtx = lctrCisGetHeadCis(&pCigCtx->list);
  BbBleData_t * const pBle = &pFirstCisCtx->bleData;
  BbBleSlvCisEvent_t * const pCis = &pBle->op.slvCis;

  pCigCtx->roleData.slv.totalAcc = pCtx->data.slv.totalAcc;

  pCigCtx->roleData.slv.anchorPointUsec = pCtx->data.slv.anchorPointUsec + LCTR_CONN_IND_US(pCtx->connInterval) + pCisCtx->data.slv.anchorOffsetUsec;

  /* Add WW to the offset. */
  const uint32_t wwOffsetUsec = lctrCalcWindowWideningUsec(pCisCtx->data.slv.anchorOffsetUsec, pCigCtx->roleData.slv.totalAcc);
  pOp->dueUsec = pCigCtx->roleData.slv.anchorPointUsec - wwOffsetUsec;
  pOp->minDurUsec += (wwOffsetUsec << 1);

  /*** BLE CIS setup ***/

  pCis->rxSyncDelayUsec = (wwOffsetUsec << 1) + 1;    /* rounding compensation when computing reqEndTs */

  /*** Commit operation ***/

  const uint32_t ceDurUsec = pOp->minDurUsec;
  const uint32_t ceSyncDlyUsec = pCis->rxSyncDelayUsec;

  while (TRUE)
  {
    if (SchInsertAtDueTime(pOp, lctrCisResolveConflict))
    {
      LL_TRACE_INFO1("    >>> CIS established, cisHandle=%u <<<", pCisCtx->cisHandle);
      LL_TRACE_INFO1("                         isoInterval=%u", LCTR_ISO_INT_TO_US(pCigCtx->isoInterval));
      LL_TRACE_INFO1("                         dueUsec=%u", pOp->dueUsec);
      pCigCtx->isBodStarted = TRUE;
      break;
    }

    LL_TRACE_WARN1("!!! Establish CIS schedule conflict handle=%u", LCTR_GET_CONN_HANDLE(pCtx));

    pFirstCisCtx->cisEvtCounter++;
    pBle->chan.chanIdx = LmgrSelectNextChannel(&pCisCtx->chanParam, pFirstCisCtx->cisEvtCounter, 0, TRUE);
    pCisCtx->nextSubEvtChanIdx = LmgrSelectNextSubEvtChannel(&pCisCtx->chanParam);

    /* Initial eventCounter starts at 0; equivalent to unsynchronized intervals. */
    uint32_t unsyncTimeUsec = LCTR_ISO_INT_TO_US(pCigCtx->isoInterval * pFirstCisCtx->cisEvtCounter);
    uint32_t wwTotalUsec    = lctrCalcWindowWideningUsec(unsyncTimeUsec, pCtx->data.slv.totalAcc);

    /* Advance to next interval. */
    pOp->dueUsec = pCigCtx->roleData.slv.anchorPointUsec + unsyncTimeUsec - wwTotalUsec;
    pOp->minDurUsec = ceDurUsec + wwTotalUsec;
    pCis->rxSyncDelayUsec = ceSyncDlyUsec + (wwTotalUsec << 1);
  }
}
