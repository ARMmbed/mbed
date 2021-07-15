/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master scanning operation builder implementation file.
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

#include "lctr_int_adv_master_ae.h"
#include "lctr_int_adv_master.h"
#include "lctr_api_adv_master_ae.h"
#include "lmgr_api_adv_master_ae.h"
#include "lctr_int_conn_master.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "bb_ble_api_reslist.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "wsf_math.h"
#include "wsf_buf.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Globals
**************************************************************************************************/

/*! \brief      Extended scan context for 1m */
lctrExtScanCtx_t lctrMstExtScanOneMCtx;

/*! \brief      Extended scan context for coded */
lctrExtScanCtx_t lctrMstExtScanCodedCtx;

/*! \brief      Extended scan operational context. */
lctrExtScanCtx_t * lctrMstExtScanTbl[LCTR_SCAN_PHY_TOTAL] =
  {
    &lctrMstExtScanOneMCtx,
    NULL,                      /* 2M does not have extended scanning functionality. */
    &lctrMstExtScanCodedCtx    /* TODO: share memory with legacy lctrMstScan */
  };

/*! \brief      Extended scan control block. */
lctrExtScanCtrlBlk_t lctrMstExtScan;

/*! \brief      Periodic sync create control block. */
lctrPerCreateSyncCtrlBlk_t lctrPerCreateSync;

/*! \brief      Periodic sync transfer control block. */
lctrPerTransferSyncCtrlBlk_t lctrPerTransferSync;

/*! \brief      Periodic Advertising message data. */
LctrPerScanMsg_t *pLctrMstPerScanMsg;

/*! \brief      Periodic scan context table. */
lctrPerScanCtx_t lctrMstPerScanTbl[LL_MAX_PER_SCAN];

/*! \brief      Extended scan data buffer table. */
static uint8_t *lctrMstExtScanDataBufTbl[LCTR_SCAN_PHY_TOTAL];

/*! \brief      Periodic scan data buffer table. */
static uint8_t *lctrMstPerScanDataBufTbl[LL_MAX_PER_SCAN];

/*! \brief      SyncInfo for periodic sync transfer. */
lctrSyncInfo_t trsfSyncInfo;

/*! \brief      Active extended scan contexts. */
lctrActiveExtScan_t lctrActiveExtScan;

/*************************************************************************************************/
/*!
 *  \brief      Master create sync message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstCreateSyncDisp(LctrPerScanMsg_t *pMsg)
{
  pLctrMstPerScanMsg = pMsg;

  lctrMstCreateSyncExecuteSm(pMsg->hdr.event);
}

/*************************************************************************************************/
/*!
 *  \brief      Periodic sync transfer recipient message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstTransferSyncDisp(LctrPerScanMsg_t *pMsg)
{
  pLctrMstPerScanMsg = pMsg;

  lctrMstTransferSyncExecuteSm(pMsg->hdr.event);
}

/*************************************************************************************************/
/*!
 *  \brief      Periodic scanning message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstPerScanDisp(LctrPerScanMsg_t *pMsg)
{
  pLctrMstPerScanMsg = pMsg;

  if (pMsg->hdr.dispId != LCTR_DISP_BCST)
  {
    WSF_ASSERT(pMsg->hdr.handle < LL_MAX_PER_SCAN);
    lctrPerScanCtx_t *pPerScanCtx = LCTR_GET_PER_SCAN_CTX(pMsg->hdr.handle);

    lctrMstPerScanExecuteSm(pPerScanCtx, pMsg->hdr.event);
  }
  else
  {
    /* Broadcast message to all contexts. */
    for (unsigned int i = 0; i < LL_MAX_PER_SCAN; i++)
    {
      lctrMstPerScanExecuteSm(LCTR_GET_PER_SCAN_CTX(i), pMsg->hdr.event);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Periodic scanning reset handler.
 */
/*************************************************************************************************/
static void lctrMstPerScanResetHandler(void)
{
  BbBlePerScanMasterInit();
}

/*************************************************************************************************/
/*!
 *  \brief      Master extended scan reset handler.
 */
/*************************************************************************************************/
static void lctrMstExtScanResetHandler(void)
{
  BbBleScanMasterInit();
  BbBleAuxScanMasterInit();
  LctrMstExtScanDefaults();
}

/*************************************************************************************************/
/*!
 *  \brief      Execute common master scan state machine.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstExtScanExecuteCommonSm(LctrExtScanMsg_t *pMsg)
{
  /* Subsystem event handling. */
  switch (pMsg->hdr.event)
  {
    case LCTR_EXT_SCAN_MSG_DISCOVER_ENABLE:
      LL_TRACE_INFO2("lctrMstExtScanExecuteCommonSm: numScanEnabled=%u, scanMode=%u, event=DISCOVER_ENABLE", lmgrCb.numScanEnabled, lmgrCb.scanMode);

      lctrMstExtScan.scanTermByHost = 0;

      /* Enable filter. */
      lctrMstExtScan.filtDup = pMsg->enable.filtDup;
      lctrAdvRptEnable(&lctrMstExtScan.advFilt,
                       (lctrMstExtScan.filtDup == LL_SCAN_FILTER_DUP_DISABLE) ? FALSE: TRUE);

      /* Start/Restart timers. */
      WsfTimerStop(&lctrMstExtScan.tmrScanDur);
      WsfTimerStop(&lctrMstExtScan.tmrScanPer);

      lctrMstExtScan.scanDurMs = pMsg->enable.durMs;
      lctrMstExtScan.scanPerMs = pMsg->enable.perMs;
      if (lctrMstExtScan.scanDurMs)
      {
        WsfTimerStartMs(&lctrMstExtScan.tmrScanDur, lctrMstExtScan.scanDurMs);
        if (lctrMstExtScan.scanPerMs)
        {
          WsfTimerStartMs(&lctrMstExtScan.tmrScanPer, lctrMstExtScan.scanPerMs);
        }
      }
      break;

    case LCTR_EXT_SCAN_MSG_DISCOVER_DISABLE:
      LL_TRACE_INFO2("lctrMstExtScanExecuteCommonSm: numScanEnabled=%u, scanMode=%u, event=DISCOVER_DISABLE", lmgrCb.numScanEnabled, lmgrCb.scanMode);

      /* Disable timers. */
      WsfTimerStop(&lctrMstExtScan.tmrScanDur);
      WsfTimerStop(&lctrMstExtScan.tmrScanPer);
      break;

    case LCTR_EXT_SCAN_MSG_TERMINATE:
      LL_TRACE_INFO2("lctrMstExtScanExecuteCommonSm: numScanEnabled=%u, scanMode=%u, event=TERMINATE", lmgrCb.numScanEnabled, lmgrCb.scanMode);
      if (!lctrMstExtScan.scanTermByHost &&
          (lctrMstExtScan.scanPerMs == 0))
      {
        LmgrSendScanTimeoutInd();
      }
      break;

    case LCTR_EXT_SCAN_MSG_TMR_DUR_EXP:
      LL_TRACE_INFO2("lctrMstExtScanExecuteCommonSm: numScanEnabled=%u, scanMode=%u, event=TMR_DUR_EXP", lmgrCb.numScanEnabled, lmgrCb.scanMode);
      break;

    case LCTR_EXT_SCAN_MSG_TMR_PER_EXP:
      LL_TRACE_INFO2("lctrMstExtScanExecuteCommonSm: numScanEnabled=%u, scanMode=%u, event=TMR_PER_EXP", lmgrCb.numScanEnabled, lmgrCb.scanMode);
      /* Reset filter. */
      if (lctrMstExtScan.filtDup == LL_SCAN_FILTER_DUP_ENABLE_PERIODIC)
      {
        lctrAdvRptEnable(&lctrMstExtScan.advFilt,
                         (lctrMstExtScan.filtDup == LL_SCAN_FILTER_DUP_DISABLE) ? FALSE: TRUE);
      }

      /* Restart timers. */
      WsfTimerStartMs(&lctrMstExtScan.tmrScanDur, lctrMstExtScan.scanDurMs);
      WsfTimerStartMs(&lctrMstExtScan.tmrScanPer, lctrMstExtScan.scanPerMs);
      break;

    case LCTR_EXT_SCAN_MSG_RESET:
      LL_TRACE_INFO2("lctrMstExtScanExecuteCommonSm: numScanEnabled=%u, scanMode=%u, event=RESET", lmgrCb.numScanEnabled, lmgrCb.scanMode);
      break;

    default:
      LL_TRACE_ERR3("lctrMstExtScanExecuteCommonSm: numScanEnabled=%u, scanMode=%u, event=%u -- unknown event", lmgrCb.numScanEnabled, lmgrCb.scanMode, pMsg->hdr.event);
      /* No action required. */
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Master extended scan message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstExtScanDisp(LctrExtScanMsg_t *pMsg)
{
  lctrExtScanCtx_t *pExtScanCtx;
  uint8_t event = pMsg->hdr.event;

  bool_t isBcstMsg = FALSE;

  if (pMsg->hdr.dispId == LCTR_DISP_BCST)
  {
    /* Global broadcast message. */
    isBcstMsg = TRUE;
  }

  /* Set message property. */
  switch (event)
  {
    case LCTR_EXT_SCAN_MSG_DISCOVER_DISABLE:
      lctrMstExtScan.scanTermByHost++;
      break;
    default:
      break;
  }

  /* Remap subsystem message to context message. */
  switch (event)
  {
    case LCTR_EXT_SCAN_MSG_TMR_DUR_EXP:
      event = LCTR_EXT_SCAN_MSG_DISCOVER_DISABLE;
      break;
    case LCTR_EXT_SCAN_MSG_TMR_PER_EXP:
      event = LCTR_EXT_SCAN_MSG_DISCOVER_ENABLE;
      break;
    default:
      break;
  }

  /* Broadcast message. */
  switch (event)
  {
    case LCTR_EXT_SCAN_MSG_DISCOVER_ENABLE:
    case LCTR_EXT_SCAN_MSG_DISCOVER_DISABLE:
      isBcstMsg = TRUE;
      break;
    default:
      break;
  }
  if (pMsg->hdr.handle == LCTR_SCAN_PHY_ALL)
  {
    isBcstMsg = TRUE;
  }

  /* Route message to SM. */
  if (!isBcstMsg)
  {
    WSF_ASSERT(pMsg->hdr.handle < LCTR_SCAN_PHY_TOTAL);
    if (lctrMstExtScan.enaPhys & (1 << pMsg->hdr.handle))
    {
      pExtScanCtx = LCTR_GET_EXT_SCAN_CTX(pMsg->hdr.handle);
      lctrMstExtScanExecuteSm(pExtScanCtx, event);
    }
  }
  else
  {
    for (unsigned int i = 0; i < LCTR_SCAN_PHY_TOTAL; i++)
    {
      if (lctrMstExtScan.enaPhys & (1 << i))
      {
        pExtScanCtx = LCTR_GET_EXT_SCAN_CTX(i);
        lctrMstExtScanExecuteSm(pExtScanCtx, event);
      }
    }
  }

  lctrMstExtScanExecuteCommonSm(pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief      Send pending extended advertising report.
 */
/*************************************************************************************************/
static void lctrMstSendPendingAdvRptHandler(void)
{
  if (LmgrIsExtCommandAllowed())
  {
    /* Extended advertising report. */
    for (unsigned int i = 0; i < LCTR_SCAN_PHY_TOTAL; i++)
    {
      if (!(lctrActiveExtScan.scanMask & (1 << i)))
      {
        continue;
      }

      lctrExtScanCtx_t *pExtScanCtx = LCTR_GET_EXT_SCAN_CTX(i);


      if (pExtScanCtx->data.scan.auxAdvRptState == LCTR_RPT_STATE_COMP)
      {
        LmgrSendExtAdvRptInd(&pExtScanCtx->data.scan.auxAdvRpt);
        pExtScanCtx->data.scan.auxAdvRptState = LCTR_RPT_STATE_IDLE;
      }

      if (pExtScanCtx->data.scan.advRptState == LCTR_RPT_STATE_COMP)
      {
        LmgrSendExtAdvRptInd(&pExtScanCtx->data.scan.advRpt);
        pExtScanCtx->data.scan.advRptState = LCTR_RPT_STATE_IDLE;
      }
    }

    /* Periodic advertising report. */
    for (unsigned int i = 0; i < LL_MAX_PER_SCAN; i++)
    {
      lctrPerScanCtx_t *pPerScanCtx = &lctrMstPerScanTbl[i];

      if (pPerScanCtx->advRptState == LCTR_RPT_STATE_COMP)
      {
        if (!pPerScanCtx->repDisabled)
        {
          LmgrSendPerAdvRptInd(&pPerScanCtx->advRpt);
        }

        pPerScanCtx->advRptState = LCTR_RPT_STATE_IDLE;
      }
    }
  }
  else
  {
    /* Legacy mode. */
    lctrMstRxAdvBPduHandler();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get reference time(due time) of the periodic scan.
 *
 *  \param      tmHandle    Handle from Topology manager.
 *
 *  \return     Due time in microseconds of the periodic scan.
 */
/*************************************************************************************************/
static uint32_t lctrGetPerScanRefTime(uint8_t tmHandle)
{
  uint32_t refTime = 0;

  WSF_ASSERT(tmHandle >= LL_MAX_CONN);

  lctrPerScanCtx_t *pPerScanCtx = LCTR_GET_PER_SCAN_CTX(tmHandle - LL_MAX_CONN);
  BbOpDesc_t * const pOp = &pPerScanCtx->bod;
  BbBleData_t * const pBle = &pPerScanCtx->bleData;

  if (pPerScanCtx->enabled && (pBle->chan.opType == BB_BLE_OP_MST_PER_SCAN_EVENT))
  {
    refTime = pOp->dueUsec;
  }

  return refTime;
}

/*************************************************************************************************/
/*!
 *  \brief  Build extended scan discovery operation.
 *
 *  \param  pExtScanCtx     Extended scan context.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t lctrMstExtDiscoverBuildOp(lctrExtScanCtx_t *pExtScanCtx)
{
  BbOpDesc_t * const pOp = &pExtScanCtx->scanBod;
  BbBleData_t * const pBle = &pExtScanCtx->scanBleData;
  BbBleMstAdvEvent_t * const pScan = &pBle->op.mstAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_MOVEABLE;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrMstExtDiscoverEndOp;
  pOp->abortCback = lctrMstExtDiscoverAbortOp;
  pOp->pCtx = pExtScanCtx;
  pOp->recoverable = TRUE;

  /*** BLE General Setup ***/

  pBle->chan.opType = BB_BLE_OP_MST_ADV_EVENT;

  pBle->chan.chanIdx = lctrScanChanSelectInit(lmgrMstScanCb.scanChanMap);
  pBle->chan.txPower = lmgrCb.advTxPwr;
  pBle->chan.accAddr = LL_ADV_ACCESS_ADDR;
  pBle->chan.crcInit = LL_ADV_CRC_INIT;
  switch (LCTR_GET_EXT_SCAN_HANDLE(pExtScanCtx))
  {
    case LCTR_SCAN_PHY_1M:
    default:
      pBle->chan.txPhy = pBle->chan.rxPhy = BB_PHY_BLE_1M;
      break;
    case LCTR_SCAN_PHY_CODED:
      pBle->chan.txPhy = pBle->chan.rxPhy = BB_PHY_BLE_CODED;
      break;
  }

#if (LL_ENABLE_TESTER == TRUE)
  pBle->chan.accAddrRx = llTesterCb.advAccessAddrRx ^ pBle->chan.accAddr;
  pBle->chan.accAddrTx = llTesterCb.advAccessAddrTx ^ pBle->chan.accAddr;
  pBle->chan.crcInitRx = llTesterCb.advCrcInitRx ^ pBle->chan.crcInit;
  pBle->chan.crcInitTx = llTesterCb.advCrcInitTx ^ pBle->chan.crcInit;
#endif

  pBle->pduFilt.pduTypeFilt = (1 << LL_PDU_ADV_IND) |
                              (1 << LL_PDU_ADV_DIRECT_IND) |
                              (1 << LL_PDU_ADV_NONCONN_IND) |
                              (1 << LL_PDU_SCAN_RSP) |
                              (1 << LL_PDU_ADV_SCAN_IND) |
                              (1 << LL_PDU_ADV_EXT_IND);
  if (pExtScanCtx->scanParam.scanFiltPolicy & LL_SCAN_FILTER_WL_BIT)
  {
    pBle->pduFilt.wlPduTypeFilt = pBle->pduFilt.pduTypeFilt;
  }
  /* Local addresses that are resolvable and cannot be resolved are optionally allowed. */
  if (pExtScanCtx->scanParam.scanFiltPolicy & LL_SCAN_FILTER_RES_INIT_BIT)
  {
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_RES_OPT);
  }

  /*** BLE Scan Setup: Rx advertising packet ***/

  pScan->scanChMap = lmgrMstScanCb.scanChanMap;

  pScan->rxAdvCback = lctrMstDiscoverRxExtAdvPktHandler;
  pScan->rxAdvPostCback = lctrMstDiscoverRxExtAdvPktPostProcessHandler;
  pScan->auxScanCheckCback = lctrMstLinkAuxOffsetScanSetup;
  pScan->auxScanTxCompCback = BbMstAuxScanTxCompHandler;
  pScan->auxScanRxCompCback = BbMstAuxScanRxCompHandler;

  if ((pScan->pRxAdvBuf = WsfMsgAlloc(LL_ADV_HDR_LEN + LL_EXT_ADV_HDR_MAX_LEN)) == NULL)
  {
    LL_TRACE_ERR0("Could not allocate advertising buffer");
    return LL_ERROR_CODE_UNSPECIFIED_ERROR;
  }

  /*** BLE Scan Setup: Tx scan request packet ***/

  pScan->txReqCback = lctrMstDiscoverTxLegacyScanReqHandler;

  /* Always match local address in PDU to initiator's address (in directed advertisements). */
  if (pExtScanCtx->scanParam.ownAddrType & LL_ADDR_RANDOM_BIT)
  {
    WSF_ASSERT(lmgrCb.bdAddrRndValid);    /* No further verification after scan starts. */
    pBle->pduFilt.localAddrMatch = lmgrCb.bdAddrRnd;
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_MATCH_RAND);
  }
  else
  {
    pBle->pduFilt.localAddrMatch = lmgrPersistCb.bdAddr;
  }
  BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_MATCH_ENA);

  /* Potentially resolve peer & local addresses. */
  if (lmgrCb.addrResEna)
  {
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_RES_ENA);
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_RES_ENA);
  }

  switch (pExtScanCtx->scanParam.scanType)
  {
    case LL_SCAN_ACTIVE:
    {
      lctrScanReq_t scanReq = { 0 };

      pExtScanCtx->reqPduHdr.pduType = LL_PDU_SCAN_REQ;
      pExtScanCtx->reqPduHdr.len = LL_SCAN_REQ_PDU_LEN;

      pExtScanCtx->reqPduHdr.txAddrRnd = BB_BLE_PDU_FILT_FLAG_IS_SET(&pBle->pduFilt, LOCAL_ADDR_MATCH_RAND);
      scanReq.scanAddr = pBle->pduFilt.localAddrMatch;

      /* Pack only known packet information, advertiser's address resolved in Rx handler. */
      uint8_t *pBuf;
      pBuf  = pExtScanCtx->reqBuf;
      pBuf += LL_ADV_HDR_LEN;
      /* pBuf += */ lctrPackScanReqPdu(pBuf, &scanReq);

      pScan->pTxReqBuf = pExtScanCtx->reqBuf;
      pScan->txReqLen = LL_ADV_HDR_LEN + LL_SCAN_REQ_PDU_LEN;

#if (LL_ENABLE_TESTER)
      if (llTesterCb.txScanReqPduLen)
      {
        pScan->pTxReqBuf = llTesterCb.txScanReqPdu;
        pScan->txReqLen = llTesterCb.txScanReqPduLen;
      }
#endif

      break;
    }
    case LL_SCAN_PASSIVE:
    default:
      pScan->pTxReqBuf = NULL;
      break;
  }

  /*** BLE Scan Setup: Rx scan response packet ***/

  pScan->rxRspCback = lctrMstDiscoverRxLegacyScanRspHandler;

  switch (pExtScanCtx->scanParam.scanType)
  {
    case LL_SCAN_ACTIVE:
    {
      /* Allow only legacy scan response. */
      if ((pScan->pRxRspBuf = WsfMsgAlloc(LL_ADVB_MAX_LEN)) == NULL)
      {
        WsfMsgFree(pScan->pRxAdvBuf);
        pScan->pRxAdvBuf = NULL;

        LL_TRACE_ERR0("Could not allocate scan response buffer");
        return LL_ERROR_CODE_UNSPECIFIED_ERROR;
      }

      break;
    }
    case LL_SCAN_PASSIVE:
    default:
      /* pScan->pRxRspBuf = NULL; */        /* cleared in alloc */
      break;
  }

  /*** Commit operation ***/

  pOp->minDurUsec = LCTR_MIN_SCAN_USEC;
  pOp->maxDurUsec = LCTR_BLE_TO_US(pExtScanCtx->scanParam.scanWindow);

  pExtScanCtx->selfTerm = FALSE;
  pExtScanCtx->shutdown = FALSE;

  uint8_t scanPhyIndex = (LCTR_GET_EXT_SCAN_HANDLE(pExtScanCtx) == LCTR_SCAN_PHY_CODED) ? LCTR_SCAN_PHY_CODED : LCTR_SCAN_PHY_1M;

  /* The first scan context is scheduled immediately. */
  /* Otherwise, it will be scheduled from the scheduler end callback. */
  if (lctrActiveExtScan.scanMask == 0)
  {
    SchInsertNextAvailable(pOp);
    pExtScanCtx->scanWinStartUsec = pOp->dueUsec;

    lctrActiveExtScan.scanIndex = scanPhyIndex;
    lctrActiveExtScan.bodSchMask |= (1 << scanPhyIndex);
  }
  else
  {
    lctrActiveExtScan.bodSchMask &= ~(1 << scanPhyIndex);
  }

  lctrActiveExtScan.scanMask |= (1 << scanPhyIndex);

  LL_TRACE_INFO1("    >>> ExtDiscover started, handle=%u <<<", LCTR_GET_EXT_SCAN_HANDLE(pExtScanCtx));
  LL_TRACE_INFO1("                             dueUsec=%u, 0=deferred", pOp->dueUsec);
  LL_TRACE_INFO1("                             pBod=0x%08x", pOp);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Build auxiliary scan discovery operation.
 *
 *  \param  pExtScanCtx     Extended scan context.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t lctrMstAuxDiscoverBuildOp(lctrExtScanCtx_t *pExtScanCtx)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &pExtScanCtx->auxScanBod;
  BbBleData_t * const pBle = &pExtScanCtx->auxBleData;
  BbBleMstAuxAdvEvent_t * const pAuxScan = &pBle->op.mstAuxAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_MOVEABLE_PREFERRED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrMstAuxDiscoverEndOp;
  pOp->abortCback = lctrMstAuxDiscoverEndOp;
  pOp->pCtx = pExtScanCtx;

  /*** BLE General Setup ***/

  pBle->chan.opType = BB_BLE_OP_MST_AUX_ADV_EVENT;

  /* pBle->chan.chanIdx = 0; */     /* write after ADV_EXT_IND is received */
  pBle->chan.txPower = lmgrCb.advTxPwr;
  pBle->chan.accAddr = LL_ADV_ACCESS_ADDR;
  pBle->chan.crcInit = LL_ADV_CRC_INIT;
  /* pBle->chan.txPhy = 0; */       /* write after ADV_EXT_IND is received */
  /* pBle->chan.rxPhy = 0; */       /* write after ADV_EXT_IND is received */
  /* pBle->chan.phyOptions = 0; */  /* write after ADV_EXT_IND is received */

#if (LL_ENABLE_TESTER == TRUE)
  pBle->chan.accAddrRx = llTesterCb.advAccessAddrRx ^ pBle->chan.accAddr;
  pBle->chan.accAddrTx = llTesterCb.advAccessAddrTx ^ pBle->chan.accAddr;
  pBle->chan.crcInitRx = llTesterCb.advCrcInitRx ^ pBle->chan.crcInit;
  pBle->chan.crcInitTx = llTesterCb.advCrcInitTx ^ pBle->chan.crcInit;
#endif

  pBle->pduFilt.pduTypeFilt = (1 << LL_PDU_AUX_ADV_IND) |
                              (1 << LL_PDU_AUX_SCAN_RSP);
  if (pExtScanCtx->scanParam.scanFiltPolicy & LL_SCAN_FILTER_WL_BIT)
  {
    pBle->pduFilt.wlPduTypeFilt = pBle->pduFilt.pduTypeFilt;
  }
  /* Local addresses that are resolvable and cannot be resolved are optionally allowed. */
  if (pExtScanCtx->scanParam.scanFiltPolicy & LL_SCAN_FILTER_RES_INIT_BIT)
  {
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_RES_OPT);
  }

  /*** BLE Scan Setup: Rx packets ***/

  pAuxScan->isInit = FALSE;
  pAuxScan->rxAuxAdvCback = lctrMstDiscoverRxAuxAdvPktHandler;
  pAuxScan->rxAuxAdvPostCback = lctrMstDiscoverRxAuxAdvPktPostProcessHandler;

  /*** BLE Scan Setup: Tx scan request packet ***/

  /* Always match local address in PDU to initiator's address (in directed advertisements). */
  if (pExtScanCtx->scanParam.ownAddrType & LL_ADDR_RANDOM_BIT)
  {
    WSF_ASSERT(lmgrCb.bdAddrRndValid);    /* No further verification after scan starts. */
    pBle->pduFilt.localAddrMatch = lmgrCb.bdAddrRnd;
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_MATCH_RAND);
  }
  else
  {
    pBle->pduFilt.localAddrMatch = lmgrPersistCb.bdAddr;
  }
  BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_MATCH_ENA);

  /* Potentially resolve peer & local addresses. */
  if (lmgrCb.addrResEna)
  {
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_RES_ENA);
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_RES_ENA);
  }

  switch (pExtScanCtx->scanParam.scanType)
  {
    case LL_SCAN_ACTIVE:
    {
      /* Use primary channel's SCAN_REQ PDU. */
      pAuxScan->pTxAuxReqBuf = pExtScanCtx->reqBuf;
      pAuxScan->txAuxReqLen = sizeof(pExtScanCtx->reqBuf);

#if (LL_ENABLE_TESTER)
      if (llTesterCb.txScanReqPduLen)
      {
        pAuxScan->pTxAuxReqBuf = llTesterCb.txScanReqPdu;
        pAuxScan->txAuxReqLen = llTesterCb.txScanReqPduLen;
      }
#endif

      break;
    }
    case LL_SCAN_PASSIVE:
    default:
      pAuxScan->pTxAuxReqBuf = NULL;
      break;
  }

  /*** BLE Scan Setup: Rx scan response packet ***/

  pAuxScan->rxAuxRspCback = lctrMstDiscoverRxAuxScanRspHandler;

  /*** BLE Scan Setup: Rx chain packet ***/

  pAuxScan->rxAuxChainCback = lctrMstDiscoverRxAuxChainHandler;
  pAuxScan->rxAuxChainPostCback = lctrMstDiscoverRxAuxChainPostProcessHandler;

  /*** Commit operation ***/

  /* pOp->minDurUsec = 0; */  /* Defer assignment until AuxPtr is received, lctrMstAuxDiscoverOpCommit(). */
  /* pOp->maxDurUsec = 0; */  /* Not used for aux scan. */

  pExtScanCtx->selfTerm = FALSE;
  pExtScanCtx->shutdown = FALSE;
  pExtScanCtx->auxOpPending = FALSE;

  /* Defer scheduling until AuxPtr is received. */

  LL_TRACE_INFO1("    >>> AuxDiscover ready, pBod=0x%08x <<<", pOp);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Commit auxiliary discovery operation.
 *
 *  \param  pExtScanCtx     Extended scan context.
 *  \param  pAuxPtr         Auxiliary Pointer.
 *  \param  startTs         Start of ADV_EXT_IND packet (offset origin).
 *  \param  endTs           End of ADV_EXT_IND packet.
 */
/*************************************************************************************************/
void lctrMstAuxDiscoverOpCommit(lctrExtScanCtx_t *pExtScanCtx, lctrAuxPtr_t *pAuxPtr, uint32_t startTs, uint32_t endTs)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &pExtScanCtx->auxScanBod;
  BbBleData_t * const pBle = &pExtScanCtx->auxBleData;

  /*** BLE General Setup ***/

  pBle->chan.chanIdx = pAuxPtr->auxChIdx;
  if (pExtScanCtx->extAdvHdr.extHdrFlags & LL_EXT_HDR_TX_PWR_BIT)
  {
    pBle->chan.txPower = pExtScanCtx->extAdvHdr.txPwr;
  }
  pBle->chan.txPhy = pBle->chan.rxPhy = lctrConvertAuxPtrPhyToBbPhy(pAuxPtr->auxPhy);

  /*** Commit operation ***/

  WSF_ASSERT(pExtScanCtx->auxOpPending == FALSE);

  uint32_t auxOffsetUsec;
  lctrMstComputeAuxOffset(pAuxPtr, &auxOffsetUsec, &pBle->op.mstAuxAdv.rxSyncDelayUsec);

  if (auxOffsetUsec < LL_BLE_MAFS_US)
  {
    LL_TRACE_WARN1("Peer requested AUX offset does not meet T_MAFS, actual afsUsec=%u", pOp->dueUsec - endTs);
  }

  pOp->dueUsec = startTs + auxOffsetUsec;
  SchBleCalcAdvOpDuration(pOp, 0);

  if (auxOffsetUsec < BbGetSchSetupDelayUs())
  {
    /* Signal the lower layer to start Aux Scan ASAP, since the auxOffset is too small to run the scheduler. */
    pExtScanCtx->scheduleAuxAsap = TRUE;
  }
  else if (SchInsertAtDueTime(pOp, NULL))
  {
    pExtScanCtx->auxOpPending = TRUE;
  }
  else
  {
    LL_TRACE_WARN1("Fail to schedule auxiliary scan, scanHandle=%u", LCTR_GET_EXT_SCAN_HANDLE(pExtScanCtx));
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for scanning master.
 */
/*************************************************************************************************/
void LctrMstExtScanInit(void)
{
  /* Add extended scan message dispatchers. */
  lctrResetHdlrTbl[LCTR_DISP_EXT_SCAN] = lctrMstExtScanResetHandler;

  /* Add extended scan message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_EXT_SCAN] = (LctrMsgDisp_t)lctrMstExtScanDisp;

  /* Add extended scan event dispatchers. */
  lctrEventHdlrTbl[LCTR_EVENT_RX_ADVB] = lctrMstSendPendingAdvRptHandler;

  /* Add utility function pointers. */
  LctrMstExtScanEnabled = LctrMstExtScanIsEnabled;

  LctrMstExtScanDefaults();

  lmgrPersistCb.extScanCtxSize = sizeof(lctrExtScanCtx_t);
}

/*************************************************************************************************/
/*!
 *  \brief      Set default values for scanning master.
 */
/*************************************************************************************************/
void LctrMstExtScanDefaults(void)
{
  static const LlScanParam_t defScanParam =
  {
    .scanType       = LL_SCAN_PASSIVE,
    .scanInterval   = 0x0010,
    .scanWindow     = 0x0010,
    .ownAddrType    = LL_ADDR_PUBLIC,
    .scanFiltPolicy = LL_SCAN_FILTER_NONE
  };

  memset(&lctrMstExtScan, 0, sizeof(lctrMstExtScan));

  /* Assign buffers. */
  for (unsigned int i = 0; i < LCTR_SCAN_PHY_TOTAL; i++)
  {
    if (lctrMstExtScanTbl[i])
    {
      memset(lctrMstExtScanTbl[i], 0, sizeof(lctrExtScanCtx_t));
      lctrMstExtScanTbl[i]->handle = i;
      lctrMstExtScanTbl[i]->pExtAdvData = lctrMstExtScanDataBufTbl[i];
    }
  }

  lmgrCb.numExtScanPhys = 1;
  lctrMstExtScanTbl[LCTR_SCAN_PHY_1M]->scanParam = defScanParam;
  lctrMstExtScan.enaPhys = 1 << LCTR_SCAN_PHY_1M;

  /* Setup timers. */
  lctrMsgHdr_t *pMsg;
  lctrMstExtScan.tmrScanDur.handlerId = lmgrPersistCb.handlerId;
  pMsg = (lctrMsgHdr_t *)&lctrMstExtScan.tmrScanDur.msg;
  /* pMsg->handle = 0; */   /* Unused. */
  pMsg->dispId = LCTR_DISP_EXT_SCAN;
  pMsg->event = LCTR_EXT_SCAN_MSG_TMR_DUR_EXP;
  lctrMstExtScan.tmrScanPer.handlerId = lmgrPersistCb.handlerId;
  pMsg = (lctrMsgHdr_t *)&lctrMstExtScan.tmrScanPer.msg;
  /* pMsg->handle = 0; */   /* Unused. */
  pMsg->dispId = LCTR_DISP_EXT_SCAN;
  pMsg->event = LCTR_EXT_SCAN_MSG_TMR_PER_EXP;

  lctrActiveExtScan.scanMask = 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize extended scanner memory resources.
 *
 *  \param  pFreeMem    Pointer to free memory.
 *  \param  freeMemSize Size of pFreeMem.
 *
 *  \return Amount of free memory consumed.
 */
/*************************************************************************************************/
uint16_t LctrInitExtScanMem(uint8_t *pFreeMem, uint32_t freeMemSize)
{
  /* Extended Scanning requires receiving at least 251 bytes. */
  WSF_ASSERT(pLctrRtCfg->maxExtScanDataLen >= WSF_MIN(LL_EXT_ADVBU_MAX_LEN, BB_ADV_PLD_MAX_LEN));

  uint8_t *pAvailMem = pFreeMem;

  /* Allocate extended scan buffer memory. */
  for (unsigned int i = 0; i < LCTR_SCAN_PHY_TOTAL; i++)
  {
    if (((uint32_t)pAvailMem) & 3)
    {
      /* Align to next word. */
      pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
    }

    lctrMstExtScanDataBufTbl[i] = pAvailMem;
    pAvailMem += pLctrRtCfg->maxExtScanDataLen;
  }

  /* Allocate periodic scan buffer memory. */
  for (unsigned int i = 0; i < LL_MAX_PER_SCAN; i++)
  {
    if (((uint32_t)pAvailMem) & 3)
    {
      /* Align to next word. */
      pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
    }

    lctrMstPerScanDataBufTbl[i] = pAvailMem;
    pAvailMem += pLctrRtCfg->maxExtScanDataLen;
  }

  if (((uint32_t)(pAvailMem - pFreeMem)) > freeMemSize)
  {
    LL_TRACE_ERR2("LctrInitExtScanMem: failed to allocate scan buffers, need=%u available=%u", (pAvailMem - pFreeMem), freeMemSize);
    WSF_ASSERT(FALSE);
    return 0;
  }

  return (pAvailMem - pFreeMem);
}

/*************************************************************************************************/
/*!
 *  \brief      Validate all scan parameters.
 *
 *  \return     TRUE if parameters are valid, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LctrMstExtScanValidateParam(void)
{
  if (!lmgrCb.bdAddrRndValid)
  {
    /* Check for use of random address. */
    for (unsigned int i = 0; i < LCTR_SCAN_PHY_TOTAL; i++)
    {
      if (lctrMstExtScan.enaPhys & (1 << i))
      {
        if (!LmgrIsAddressTypeAvailable(lctrMstExtScanTbl[i]->scanParam.ownAddrType))
        {
          LL_TRACE_WARN1("Address type invalid or not available, ownAddrType=%u", lctrMstExtScanTbl[i]->scanParam.ownAddrType);
          return FALSE;
        }
      }
    }
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Set enabled scanning PHY.
 *
 *  \param      scanPhy     Enabled scanning PHY.
 */
/*************************************************************************************************/
void LctrMstExtScanSetScanPhy(uint8_t scanPhy)
{
  WSF_ASSERT(scanPhy < LCTR_SCAN_PHY_TOTAL);

  lctrMstExtScan.enaPhys |= 1 << scanPhy;
}

/*************************************************************************************************/
/*!
 *  \brief      Clear (disable) scanning PHY.
 *
 *  \param      scanPhy     Disabled scanning PHY.
 */
/*************************************************************************************************/
void LctrMstExtScanClearScanPhy(uint8_t scanPhy)
{
  WSF_ASSERT(scanPhy < LCTR_SCAN_PHY_TOTAL);

  lctrMstExtScan.enaPhys &= ~(1 << scanPhy);
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended scan parameters.
 *
 *  \param      scanPhy         Extended scanning PHY.
 *  \param      ownAddrType     Address type used by this device.
 *  \param      scanFiltPolicy  Scan filter policy.
 *  \param      pParam          Extended scanning parameters.
 */
/*************************************************************************************************/
void LctrMstExtScanSetParam(uint8_t scanPhy, uint8_t ownAddrType, uint8_t scanFiltPolicy, const LlExtScanParam_t *pParam)
{
  WSF_ASSERT(scanPhy < LCTR_SCAN_PHY_TOTAL);

  lctrMstExtScanTbl[scanPhy]->scanParam.scanInterval = pParam->scanInterval;
  lctrMstExtScanTbl[scanPhy]->scanParam.scanWindow = pParam->scanWindow;
  lctrMstExtScanTbl[scanPhy]->scanParam.scanType = pParam->scanType;
  lctrMstExtScanTbl[scanPhy]->scanParam.ownAddrType = ownAddrType;
  lctrMstExtScanTbl[scanPhy]->scanParam.scanFiltPolicy = scanFiltPolicy;
}

/*************************************************************************************************/
/*!
 *  \brief      Check whether ext scan is enabled or not.
 *
 *  \param      scanPhy         Extended scanning PHY.
 *
 *  \return     True if scan is not disabled, false if not.
 */
/*************************************************************************************************/
bool_t LctrMstExtScanIsEnabled(uint8_t scanPhy)
{
  return (lctrMstExtScanTbl[scanPhy]->state != LCTR_EXT_SCAN_STATE_DISABLED);
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal extended scan subsystem message.
 *
 *  \param  pExtScanCtx Extended scan context.
 *  \param  event       Extended scan event.
 */
/*************************************************************************************************/
void lctrSendExtScanMsg(lctrExtScanCtx_t *pExtScanCtx, uint8_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t))) != NULL)
  {
    pMsg->handle = (pExtScanCtx) ? LCTR_GET_EXT_SCAN_HANDLE(pExtScanCtx) : LCTR_SCAN_PHY_ALL;
    pMsg->dispId = LCTR_DISP_EXT_SCAN;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal periodic create sync subsystem message.
 *
 *  \param  pCtx        Periodic scanning context.
 *  \param  event       Periodic scan event.
 */
/*************************************************************************************************/
void lctrSendCreateSyncMsg(lctrPerScanCtx_t *pCtx, uint8_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t))) != NULL)
  {
    pMsg->dispId = pCtx->createDispId;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal periodic advertising subsystem message.
 *
 *  \param  pCtx        Periodic scanning context.
 *  \param  event       Periodic advertising event.
 */
/*************************************************************************************************/
void lctrSendPerScanMsg(lctrPerScanCtx_t *pCtx, uint8_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t))) != NULL)
  {
    pMsg->handle = LCTR_GET_PER_SCAN_HANDLE(pCtx);
    pMsg->dispId = LCTR_DISP_PER_SCAN;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check if create sync is pending or not.
 *
 *  \return     TRUE if create sync is pending, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LctrMstPerIsSyncPending(void)
{
  return (lctrPerCreateSync.state == LCTR_CREATE_SYNC_STATE_DISCOVER);
}

/*************************************************************************************************/
/*!
 *  \brief      Check if create sync is disabled.
 *
 *  \return     TRUE if create sync is disabled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LctrMstPerIsSyncDisabled(void)
{
  return (lctrPerCreateSync.state == LCTR_CREATE_SYNC_STATE_DISABLED);
}

/*************************************************************************************************/
/*!
 *  \brief      Check if scanner is already sync with the same advertiser with the same advertising
 *              set.
 *
 *  \param      advSID      Advertising SID.
 *  \param      advAddrType Advertiser address type.
 *  \param      advAddr     Advertiser address.
 *
 *  \return     TRUE if already sync, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LctrMstPerIsSync(uint8_t advSID, uint8_t advAddrType, uint64_t advAddr)
{
  for (unsigned int index = 0; index < LL_MAX_PER_SCAN; index++)
  {
    if (lctrMstPerScanTbl[index].enabled)
    {
      lctrPerScanCtx_t *pPerScanCtx = LCTR_GET_PER_SCAN_CTX(index);

      if ((pPerScanCtx->advSID == advSID) &&
          (pPerScanCtx->advAddrType == advAddrType) &&
          (pPerScanCtx->advAddr == advAddr))
      {
        return TRUE;
      }
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize link layer controller resources for master create sync.
 */
/*************************************************************************************************/
void LctrMstPerCreateSyncInit(void)
{
  /* Add create sync reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_PER_CREATE_SYNC] = NULL;   /* Not needed. */

  /* Add create sync task message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_PER_CREATE_SYNC] = (LctrMsgDisp_t)lctrMstCreateSyncDisp;

  LctrMstPerSyncPending = LctrMstPerIsSyncPending;

  /* Set supported features. */
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_0)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_LE_PER_ADV;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize link layer controller resources for periodic sync transfer recipient.
 */
/*************************************************************************************************/
void LctrMstPerTransferSyncInit(void)
{
  /* Add transfer sync reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_TRANFER_SYNC] = NULL;   /* Not needed. */

  /* Add transfer sync task message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_TRANFER_SYNC] = (LctrMsgDisp_t)lctrMstTransferSyncDisp;

  /* Set supported features. */
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_1)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_PAST_RECIPIENT;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize link layer controller resources for master periodic scanning.
 */
/*************************************************************************************************/
void LctrMstPerScanInit(void)
{
  /* Add periodic scanning reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_PER_SCAN] = lctrMstPerScanResetHandler;

  /* Add periodic scanning task message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_PER_SCAN] = (LctrMsgDisp_t)lctrMstPerScanDisp;

  /* Set supported features. */
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_0)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_LE_PER_ADV;
  }

  lmgrPersistCb.perScanCtxSize = sizeof(lctrPerScanCtx_t);
}

/*************************************************************************************************/
/*!
 *  \brief  Build periodic scan operation.
 *
 *  \param  pPerScanCtx     Periodic scan context.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t lctrMstPerScanBuildOp(lctrPerScanCtx_t *pPerScanCtx)
{
  BbOpDesc_t * const pOp = &pPerScanCtx->bod;
  BbBleData_t * const pBle = &pPerScanCtx->bleData;
  BbBleMstPerScanEvent_t * const pPerScan = &pBle->op.mstPerScan;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_FIXED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrMstPerScanEndOp;
  pOp->abortCback = lctrMstPerScanAbortOp;
  pOp->pCtx = pPerScanCtx;

  /*** BLE General Setup ***/

  pBle->chan.opType = BB_BLE_OP_MST_PER_SCAN_EVENT;

  /*** BLE Scan Setup: Rx advertising packet ***/

  /*** BLE Scan Setup: Rx chain packet ***/

  pPerScan->rxPerAdvCback = lctrMstPerScanRxPerAdvPktHandler;
  pPerScan->rxPerAdvPostCback = lctrMstPerScanRxPerAdvPktPostHandler;

  pPerScanCtx->shutdown = FALSE;

  /* Defer scheduling until SyncInfo is received. */

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Commit periodic scan discovery operation.
 *
 *  \param  pExtScanCtx     Extended scan context.
 *  \param  pAuxPtr         Auxiliary Pointer.
 *  \param  pSyncInfo       Sync info.
 *  \param  startTs         Start of AUX_ADV_IND packet (offset origin).
 *  \param  endTs           End of AUX_ADV_IND packet.
 */
/*************************************************************************************************/
void lctrMstPerScanOpCommit(lctrExtScanCtx_t *pExtScanCtx, lctrAuxPtr_t *pAuxPtr,
                            lctrSyncInfo_t *pSyncInfo, uint32_t startTs, uint32_t endTs)
{
  /* Pre-resolve common structures for efficient access. */
  lctrPerScanCtx_t *pPerScanCtx = lctrPerCreateSync.pPerScanCtx;
  BbOpDesc_t * const pOp = &pPerScanCtx->bod;
  BbBleData_t * const pBle = &pPerScanCtx->bleData;

  /*** BLE General Setup for Channel ***/

  pPerScanCtx->chanParam.chanMask = pSyncInfo->chanMap;
  pPerScanCtx->chanParam.usedChSel = LL_CH_SEL_2;

  LmgrBuildRemapTable(&pPerScanCtx->chanParam);
  pPerScanCtx->chanParam.chIdentifier = (pSyncInfo->accAddr >> 16) ^
                                        (pSyncInfo->accAddr >> 0);

  pBle->chan.chanIdx = lctrPeriodicSelectNextChannel(&pPerScanCtx->chanParam, pPerScanCtx->eventCounter);

  if (pExtScanCtx->extAdvHdr.extHdrFlags & LL_EXT_HDR_TX_PWR_BIT)
  {
    pBle->chan.txPower = pExtScanCtx->extAdvHdr.txPwr;
  }

  pBle->chan.accAddr = pSyncInfo->accAddr;
  pBle->chan.crcInit = pSyncInfo->crcInit;
  pBle->chan.txPhy = pBle->chan.rxPhy = lctrConvertAuxPtrPhyToBbPhy(pAuxPtr->auxPhy);

#if (LL_ENABLE_TESTER == TRUE)
  pBle->chan.accAddrRx = llTesterCb.advAccessAddrRx ^ pBle->chan.accAddr;
  pBle->chan.accAddrTx = llTesterCb.advAccessAddrTx ^ pBle->chan.accAddr;
  pBle->chan.crcInitRx = llTesterCb.advCrcInitRx ^ pBle->chan.crcInit;
  pBle->chan.crcInitTx = llTesterCb.advCrcInitTx ^ pBle->chan.crcInit;
#endif

  /*** Commit operation ***/

  uint32_t syncOffsetUsec;
  uint32_t offsetUsec = pSyncInfo->syncOffset * ((pSyncInfo->offsetUnits == LCTR_OFFS_UNITS_30_USEC) ? 30 : 300);
  offsetUsec += (LL_SYNC_OFFS_ADJUST_USEC * pSyncInfo->offsetAdjust);
  pPerScanCtx->lastAnchorPointUsec = startTs + offsetUsec;
  pPerScanCtx->lastActiveEvent = pPerScanCtx->eventCounter;
  uint32_t caPpm = lctrCalcTotalAccuracy(pSyncInfo->sca);
  uint32_t wwUsec = lctrCalcWindowWideningUsec(offsetUsec + ((pSyncInfo->offsetUnits == LCTR_OFFS_UNITS_30_USEC) ? 30 : 300), caPpm);
  syncOffsetUsec = offsetUsec - wwUsec;
  pPerScanCtx->rxSyncDelayUsec = pBle->op.mstPerScan.rxSyncDelayUsec = (wwUsec << 1) + ((pSyncInfo->offsetUnits == LCTR_OFFS_UNITS_30_USEC) ? 30 : 300);    /* rounding compensation */

  if (syncOffsetUsec < LL_BLE_MAFS_US)
  {
    LL_TRACE_WARN1("Peer requested AuxPtr offset does not meet T_MAFS, actual afsUsec=%u", pOp->dueUsec - endTs);
    return;
  }

  pOp->dueUsec = startTs + syncOffsetUsec;

  /* Ensure minimum packet is scheduled; allow PerAdv to schedule tight operations. */
  pPerScanCtx->minDurUsec = pOp->minDurUsec = SchBleCalcAdvPktDurationUsec(pBle->chan.rxPhy, BB_PHY_OPTIONS_DEFAULT, LL_EXT_ADV_HDR_MIN_LEN);
  uint16_t numUnsyncIntervals = 0;

  while (TRUE)
  {
    if (SchInsertAtDueTime(pOp, lctrPerScanResolveConflict))
    {
      LL_TRACE_INFO1("    >>> Periodic scan started, handle=%u <<<", LCTR_GET_PER_SCAN_HANDLE(pPerScanCtx));
      LL_TRACE_INFO1("                               dueUsec=%u", pOp->dueUsec);
      LL_TRACE_INFO1("                               eventCounter=%u", pPerScanCtx->eventCounter);
      LL_TRACE_INFO1("                               chanIdx=%u", pBle->chan.chanIdx);
      LL_TRACE_INFO1("                               pBod=0x%08x", pOp);

      break;
    }

    LL_TRACE_WARN0("!!! Start periodic scanning schedule conflict");

    pPerScanCtx->eventCounter++;
    pBle->chan.chanIdx = lctrPeriodicSelectNextChannel(&pPerScanCtx->chanParam, pPerScanCtx->eventCounter);
    numUnsyncIntervals++;

    uint32_t unsyncTimeUsec = pPerScanCtx->perInterUsec * numUnsyncIntervals;
    uint32_t wwTotalUsec    = lctrCalcWindowWideningUsec(unsyncTimeUsec, caPpm);

    /* Advance to next interval. */
    pOp->dueUsec = pPerScanCtx->lastAnchorPointUsec + unsyncTimeUsec - wwTotalUsec;

    pOp->minDurUsec = pPerScanCtx->minDurUsec + wwTotalUsec;
    pBle->op.mstPerScan.rxSyncDelayUsec = pPerScanCtx->rxSyncDelayUsec + (wwTotalUsec << 1);
  }

  /* Update topology manager information. */
  SchTmAdd(LCTR_GET_PER_SCAN_TM_HANDLE(pPerScanCtx), pPerScanCtx->perInterUsec, pPerScanCtx->minDurUsec, FALSE, lctrGetPerScanRefTime);

  lctrPerCreateSync.createSyncPending = TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Commit periodic sync transferred scan operation.
 *
 *  \param  connHandle    Connection handle.
 */
/*************************************************************************************************/
void lctrMstPerScanTransferOpCommit(uint16_t connHandle)
{
  lctrConnCtx_t *pConnCtx = LCTR_GET_CONN_CTX(connHandle);
  BbOpDesc_t * const pConnOp = &pConnCtx->connBod;
  uint32_t refTime;
  uint16_t peC;       /* paEventCounter for the AUX_SYNC_IND PDU that we are attempting to receive. */
  uint32_t startTs;   /* Anchor point of the connection event. */
  uint32_t numInterval;

  /* Pre-resolve common structures for efficient access. */
  lctrPerScanCtx_t *pPerScanCtx = lctrPerTransferSync.pPerScanCtx;
  BbOpDesc_t * const pOp = &pPerScanCtx->bod;
  BbBleData_t * const pBle = &pPerScanCtx->bleData;

  /* reporting can be initially disabled. */
  if (pConnCtx->syncMode == LL_SYNC_TRSF_MODE_REP_DISABLED)
  {
    pPerScanCtx->repDisabled = TRUE;
  }

  if (pConnCtx->syncMode == LL_SYNC_TRSF_MODE_DUP_FILTERED)
  {
    pPerScanCtx->dupFilterEnable = TRUE;
    pPerScanCtx->lastDid = LL_DID_NOT_PRESENT;
  }

  pPerScanCtx->syncTimeOutMs = LCTR_PER_SYNC_TIMEOUT_TO_MS(pConnCtx->syncTimeout);
  pPerScanCtx->skip = pConnCtx->syncSkip;
  pPerScanCtx->sca = trsfSyncInfo.sca;

  /*** BLE General Setup for Channel ***/

  pPerScanCtx->chanParam.chanMask = trsfSyncInfo.chanMap;
  pPerScanCtx->chanParam.usedChSel = LL_CH_SEL_2;

  lctrPeriodicBuildRemapTable(&pPerScanCtx->chanParam);
  pPerScanCtx->chanParam.chIdentifier = (trsfSyncInfo.accAddr >> 16) ^
                                        (trsfSyncInfo.accAddr >> 0);

  pBle->chan.accAddr = trsfSyncInfo.accAddr;
  pBle->chan.crcInit = trsfSyncInfo.crcInit;
  pBle->chan.txPhy = pBle->chan.rxPhy = pPerScanCtx->rxPhys;

#if (LL_ENABLE_TESTER == TRUE)
  pBle->chan.accAddrRx = llTesterCb.advAccessAddrRx ^ pBle->chan.accAddr;
  pBle->chan.accAddrTx = llTesterCb.advAccessAddrTx ^ pBle->chan.accAddr;
  pBle->chan.crcInitRx = llTesterCb.advCrcInitRx ^ pBle->chan.crcInit;
  pBle->chan.crcInitTx = llTesterCb.advCrcInitTx ^ pBle->chan.crcInit;
#endif

  /* Offset from ceRef to PEa(paEventCounter). */
  uint32_t offsetUsec = trsfSyncInfo.syncOffset * ((trsfSyncInfo.offsetUnits == LCTR_OFFS_UNITS_30_USEC) ? 30 : 300);
  offsetUsec += (LL_SYNC_OFFS_ADJUST_USEC * trsfSyncInfo.offsetAdjust);

  /* Calculate reference time: ceRef + offset. */
  refTime = lctrConnGetAnchorPoint(pConnCtx, lctrPerTransferSync.ceRef);
  refTime += offsetUsec;

  /* refTime needs to be future from the next connection event. */
  startTs = lctrConnGetAnchorPoint(pConnCtx, pConnCtx->eventCounter);
  peC = trsfSyncInfo.eventCounter;

  if ((uint16_t)(lctrPerTransferSync.ceRef - pConnCtx->eventCounter) < (uint16_t)LCTR_MAX_INSTANT) /* ceCounter is in the future. */
  {
    numInterval = ((uint16_t)(lctrPerTransferSync.ceRef - pConnCtx->eventCounter) * LCTR_CONN_IND_US(pConnCtx->connInterval) + pConnOp->minDurUsec) / pPerScanCtx->perInterUsec;
    refTime -= numInterval * pPerScanCtx->perInterUsec;
    peC -= numInterval;
    offsetUsec = BbGetTargetTimeDelta(refTime , startTs);
  }
  else  /* refTime is in the past. */
  {
    numInterval = 1 + ((uint16_t)(pConnCtx->eventCounter - lctrPerTransferSync.ceRef) * LCTR_CONN_IND_US(pConnCtx->connInterval) + pConnOp->minDurUsec) / pPerScanCtx->perInterUsec;
    refTime += numInterval * pPerScanCtx->perInterUsec;
    peC += numInterval;
    offsetUsec = BbGetTargetTimeDelta(refTime , startTs);
  }

  pPerScanCtx->eventCounter = peC;
  pBle->chan.chanIdx = lctrPeriodicSelectNextChannel(&pPerScanCtx->chanParam, pPerScanCtx->eventCounter);

  /* Total drift D = (Da + Db) x (1 + CAa + CAb + CAc)  */
  /* Da : Drift of the periodic advertising             */
  /* Db : Drift of B's clock between CEs and PEb        */
  uint16_t scaPpmA = (trsfSyncInfo.sca < LCTR_MAX_SCA) ? scaPpmTbl[trsfSyncInfo.sca] : scaPpmTbl[LCTR_MAX_SCA];
  uint16_t scaPpmB = (lctrPerTransferSync.scaB < LCTR_MAX_SCA) ? scaPpmTbl[lctrPerTransferSync.scaB] : scaPpmTbl[LCTR_MAX_SCA];
  uint16_t scaPpmC = BbGetClockAccuracy();

  /* Da = |PEc – PEb| × PAI × (CAa + CAc) */
  uint32_t deltaPA = ((uint16_t)(peC - lctrPerTransferSync.lastPECounter) < LCTR_MAX_INSTANT) ? (uint16_t)(peC - lctrPerTransferSync.lastPECounter) : 0;
  deltaPA *= pPerScanCtx->perInterUsec;
  uint32_t dA = lctrCalcWindowWideningUsec(deltaPA, (scaPpmA + scaPpmC));

  /* Db = |CEt – CEs| × CI × (CAb + CAc) */
  uint32_t deltaCE = ((uint16_t)(lctrPerTransferSync.ceRcvd - lctrPerTransferSync.syncCe) < LCTR_MAX_INSTANT) ? (uint16_t)(lctrPerTransferSync.ceRcvd - lctrPerTransferSync.syncCe) : 0;
  deltaCE *= LCTR_CONN_IND_US(pConnCtx->connInterval);
  uint32_t dB = lctrCalcWindowWideningUsec(deltaCE, (scaPpmB + scaPpmC));

  uint32_t wwUsec = 16 + lctrCalcWindowWideningUsec((dA + dB), (1 + scaPpmA + scaPpmB + scaPpmC));
  pPerScanCtx->rxSyncDelayUsec = pBle->op.mstPerScan.rxSyncDelayUsec = (wwUsec << 1) + ((trsfSyncInfo.offsetUnits == LCTR_OFFS_UNITS_30_USEC) ? 30 : 300);    /* rounding compensation */

  LL_TRACE_WARN3("Periodic scan transfer WW=%u: Da=%u, Db=%u", wwUsec, dA, dB);

  pPerScanCtx->lastAnchorPointUsec = startTs + offsetUsec;
  pPerScanCtx->lastActiveEvent = pPerScanCtx->eventCounter;
  pPerScanCtx->initEventCounter = pPerScanCtx->eventCounter;

  pOp->dueUsec = startTs + offsetUsec - wwUsec;
  SchBleCalcAdvOpDuration(pOp, 0);
  pPerScanCtx->minDurUsec = pOp->minDurUsec;
  uint16_t numUnsyncIntervals = 0;

  while (TRUE)
  {
    if (SchInsertAtDueTime(pOp, lctrPerScanResolveConflict))
    {
      LL_TRACE_INFO1("    >>> Periodic scan from transfer started, handle=%u <<<", LCTR_GET_PER_SCAN_HANDLE(pPerScanCtx));
      LL_TRACE_INFO1("                                             dueUsec=%u", pOp->dueUsec);
      LL_TRACE_INFO1("                                             eventCounter=%u", pPerScanCtx->eventCounter);
      LL_TRACE_INFO1("                                             pBle->chan.chanIdx=%u", pBle->chan.chanIdx);
      LL_TRACE_INFO1("                                             pBod=0x%08x", pOp);
      break;
    }

    LL_TRACE_WARN0("!!! Start periodic scanning from transfer schedule conflict");

    pPerScanCtx->eventCounter++;
    pBle->chan.chanIdx = lctrPeriodicSelectNextChannel(&pPerScanCtx->chanParam, pPerScanCtx->eventCounter);
    numUnsyncIntervals++;

    uint32_t unsyncTimeUsec = pPerScanCtx->perInterUsec * numUnsyncIntervals;

    dA = lctrCalcWindowWideningUsec((deltaPA + unsyncTimeUsec), (scaPpmA + scaPpmC));
    wwUsec = 16 + lctrCalcWindowWideningUsec((dA + dB), (1 + scaPpmA + scaPpmB + scaPpmC));

    /* Advance to next interval. */
    pOp->dueUsec = pPerScanCtx->lastAnchorPointUsec + unsyncTimeUsec - wwUsec;
    pOp->minDurUsec = pPerScanCtx->minDurUsec + wwUsec;
    pBle->op.mstPerScan.rxSyncDelayUsec = pPerScanCtx->rxSyncDelayUsec + (wwUsec << 1);
  }

  /* Update topology manager information. */
  SchTmAdd(LCTR_GET_PER_SCAN_TM_HANDLE(pPerScanCtx), pPerScanCtx->perInterUsec, pPerScanCtx->minDurUsec, FALSE, lctrGetPerScanRefTime);
}

/*************************************************************************************************/
/*!
 *  \brief  Get number of allocated periodic scan contexts.
 *
 *  \return Number of allocated periodic scan contexts.
 */
/*************************************************************************************************/
uint8_t lctrMstPerGetNumPerScanCtx(void)
{
  uint8_t numPerScan = 0;

  for (unsigned int index = 0; index < LL_MAX_PER_SCAN; index++)
  {
    if (lctrMstPerScanTbl[index].enabled)
    {
      numPerScan++;
    }
  }

  return numPerScan;
}

/*************************************************************************************************/
/*!
 *  \brief  Check if the sync handle is valid or not.
 *
 *  \param      syncHandle        Periodic sync handle.
 *
 *  \return True is sync handle is valid, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrMstPerIsSyncHandleValid(uint16_t syncHandle)
{
  if (syncHandle >= LL_MAX_PER_SCAN)
  {
    return FALSE;
  }

  return lctrMstPerScanTbl[syncHandle].enabled;
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a periodic scan context.
 *
 *  \return Periodic sync context.
 */
/*************************************************************************************************/
lctrPerScanCtx_t *lctrAllocPerScanCtx(void)
{
  for (unsigned int index = 0; index < LL_MAX_PER_SCAN; index++)
  {
    if (!lctrMstPerScanTbl[index].enabled)
    {
      lctrPerScanCtx_t *pCtx = LCTR_GET_PER_SCAN_CTX(index);

      memset(pCtx, 0, sizeof(lctrPerScanCtx_t));

      pCtx->enabled = TRUE;

      /* Setup periodic advertising data buffer. */
      pCtx->pPerAdvData = lctrMstPerScanDataBufTbl[index];

      /* Setup supervision timer. */
      pCtx->tmrSupTimeout.handlerId = lmgrPersistCb.handlerId;
      lctrMsgHdr_t *pMsg = (lctrMsgHdr_t *)&pCtx->tmrSupTimeout.msg;
      pMsg->handle = index;
      pMsg->dispId = LCTR_DISP_PER_SCAN;
      pMsg->event = LCTR_PER_SCAN_SUP_TIMEOUT;

      /* Update once PHY is known. */
      pCtx->bleData.chan.txPhy = pCtx->bleData.chan.rxPhy = BB_PHY_BLE_1M;

      /* Default PHY. */
      pCtx->rxPhys = lmgrConnCb.rxPhys;

      /* ADI filtering. */
      pCtx->lastDid = LL_DID_NOT_PRESENT;

      return pCtx;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Periodic scan conflict resolution handler.
 *
 *  \param  pNewOp      New BOD.
 *  \param  pExistOp    Existing BOD.
 *
 *  \return Prioritized BOD.
 *
 *  Prioritize BLE periodic scan at most risk for termination.
 */
/*************************************************************************************************/
BbOpDesc_t *lctrPerScanResolveConflict(BbOpDesc_t *pNewOp, BbOpDesc_t *pExistOp)
{
  lctrPerScanCtx_t *pNewCtx = pNewOp->pCtx;
  lctrPerScanCtx_t *pExistCtx = pExistOp->pCtx;

  /* Only BLE uses periodic scan. */
  WSF_ASSERT((pNewOp->protId == BB_PROT_BLE) && (pExistOp->protId == BB_PROT_BLE));
  WSF_ASSERT(pNewOp->prot.pBle->chan.opType == BB_BLE_OP_MST_PER_SCAN_EVENT);

  if (pExistOp->prot.pBle->chan.opType != BB_BLE_OP_MST_PER_SCAN_EVENT)
  {
    return pExistOp;
  }

  /* Supervision timeout is imminent (2 PI). */
  if ((pExistCtx->tmrSupTimeout.ticks * WSF_MS_PER_TICK * 1000) < (uint32_t)(pExistCtx->perInterUsec << 1))
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, imminent SVT: existing handle=%u prioritized over incoming handle=%u", LCTR_GET_PER_SCAN_HANDLE(pExistCtx), LCTR_GET_PER_SCAN_HANDLE(pNewCtx));
    return pExistOp;
  }

  if ((pNewCtx->tmrSupTimeout.ticks * WSF_MS_PER_TICK * 1000) < (uint32_t)(pNewCtx->perInterUsec << 1))
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, imminent SVT: incoming handle=%u prioritized over existing handle=%u", LCTR_GET_PER_SCAN_HANDLE(pNewCtx), LCTR_GET_PER_SCAN_HANDLE(pExistCtx));
    return pNewOp;
  }

  /* Less frequent perInterval (4x). */
  if ((LCTR_PER_INTER_TO_MS(pExistCtx->perInterUsec) >> 2) > LCTR_PER_INTER_TO_MS(pNewCtx->perInterUsec))
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, PI frequency: existing handle=%u prioritized over incoming handle=%u", LCTR_GET_PER_SCAN_HANDLE(pExistCtx), LCTR_GET_PER_SCAN_HANDLE(pNewCtx));
    return pExistOp;
  }
  if ((LCTR_PER_INTER_TO_MS(pNewCtx->perInterUsec) >> 2) > LCTR_PER_INTER_TO_MS(pExistCtx->perInterUsec))
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, PI frequency: incoming handle=%u prioritized over existing handle=%u", LCTR_GET_PER_SCAN_HANDLE(pNewCtx), LCTR_GET_PER_SCAN_HANDLE(pExistCtx));
    return pNewOp;
  }

  /* Default. */
  LL_TRACE_WARN2("!!! Scheduling conflict, default: existing handle=%u prioritized over incoming handle=%u", LCTR_GET_PER_SCAN_HANDLE(pExistCtx), LCTR_GET_PER_SCAN_HANDLE(pNewCtx));
  return pExistOp;
}

/*************************************************************************************************/
/*!
 *  \brief  Get channel map of periodic scanner.
 *
 *  \param  handle      Handle of scanner channel map.
 *
 *  \return 64-bit formatted channel map mask
 */
/*************************************************************************************************/
uint64_t LctrGetPerScanChanMap(uint16_t handle)
{
  lctrPerScanCtx_t *pCtx;
  if (handle >= LL_MAX_PER_SCAN)
  {
    return 0;
  }

  pCtx = LCTR_GET_PER_SCAN_CTX(handle);

  return pCtx->chanParam.chanMask;
}

/*************************************************************************************************/
/*!
 *  \brief      Send periodic advertising sync transfer received event.
 *
 *  \param      status        Status.
 *  \param      pPerScanCtx   Periodic scan context.
 */
/*************************************************************************************************/
void LctrSendPerSyncTrsfRcvdEvt(uint8_t status, lctrPerScanCtx_t *pPerScanCtx)
{
  WSF_ASSERT(pPerScanCtx != NULL);

  LlPerSyncTrsfRcvdInd_t evt =
  {
    .hdr =
    {
      .event  = LL_PER_SYNC_TRSF_RCVD_IND,
    },
    .status = status,
    .connHandle = lctrPerTransferSync.connHandle,
    .serviceData = lctrPerTransferSync.serviceData,
    .syncHandle = LCTR_GET_PER_SCAN_HANDLE(pPerScanCtx),
    .advSID = pPerScanCtx->advSID,
    .addrType = pPerScanCtx->advAddrType,
    .advPhy = pPerScanCtx->rxPhys,
    .perAdvInterval = LCTR_PER_INTER_TO_MS(pPerScanCtx->perInterUsec),
    .advClkAccuracy = pPerScanCtx->sca
  };

  Bda64ToBstream(evt.addr, pPerScanCtx->advAddr);

  LL_TRACE_INFO2("### LlEvent ###  LL_PER_ADV_SYNC_TRSF_RCVD_EVT, connHandle=%u status=%u", evt.connHandle, evt.status);
  LL_TRACE_INFO2("### LlEvent ###                                 syncHandle=%u advSID=%u", evt.syncHandle, evt.advSID);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Enable or disable reports for the periodic advertising sync.
 *
 *  \param      syncHandle    Periodic sync handle.
 *  \param      enable        Enable options.
 */
/*************************************************************************************************/
void LctrMstPerSetRcvEnable(uint16_t syncHandle, uint8_t enable)
{
  WSF_ASSERT(syncHandle < LL_MAX_PER_SCAN);
  lctrPerScanCtx_t *pPerScanCtx = LCTR_GET_PER_SCAN_CTX(syncHandle);

  pPerScanCtx->repDisabled = !((bool_t) ((enable >> LCTR_PER_RECV_ENABLE_REPORT_BIT_POS) & 1));
  if (!pPerScanCtx->repDisabled)
  {
    pPerScanCtx->dupFilterEnable = (bool_t) ((enable >> LCTR_PER_RECV_ENABLE_FILTERING_BIT_POS) & 1);
  }
}
