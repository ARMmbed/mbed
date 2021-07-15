/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave connection operation builder implementation file.
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

#include "lctr_int_conn.h"
#include "lctr_int_conn_slave.h"
#include "lctr_int_adv_slave.h"
#include "lmgr_api_conn.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "bb_ble_api.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_os.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      SCA PPM table. */
const uint16_t scaPpmTbl[] =
{
  500,    /*!< LL_MCA_500_PPM */
  250,    /*!< LL_MCA_250_PPM */
  150,    /*!< LL_MCA_150_PPM */
  100,    /*!< LL_MCA_100_PPM */
  75,     /*!< LL_MCA_75_PPM */
  50,     /*!< LL_MCA_50_PPM */
  30,     /*!< LL_MCA_30_PPM */
  20      /*!< LL_MCA_20_PPM */
};

/*************************************************************************************************/
/*!
 *  \brief      Process a received connection indication PDU in slave role.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrSlvProcessConnInd(lctrConnMsg_t *pMsg)
{
  lctrConnEstablish_t *pConnEst = &pMsg->connEstablish;
  lctrConnCtx_t *pCtx;

  if ((pCtx = lctrAllocConnCtx()) != NULL)
  {
    pLctrConnMsg = pMsg;
    pCtx->role = LL_ROLE_SLAVE;
    lctrSlvConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_CONNECT_IND);
  }
  else
  {
    lctrNotifyHostConnectInd(0, LL_ROLE_SLAVE, &pConnEst->connInd, pConnEst->peerIdAddrType,
                             pConnEst->peerIdAddr, pConnEst->peerRpa, pConnEst->localRpa,
                             LL_ERROR_CODE_CONN_LIMIT_EXCEEDED, 0);
  }

  /* Advertising has stopped. */
  BbStop(BB_PROT_BLE);
  if (pConnEst->sendAdvSetTerm)
  {
    LmgrSendAdvSetTermInd(pConnEst->hdr.handle, LL_SUCCESS, LCTR_GET_CONN_HANDLE(pCtx), pConnEst->numExtAdvEvents);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Process a received data channel PDU in slave role.
 *
 *  \param      pCtx    Connection context.
 *  \param      pBuf    PDU buffer.
 */
/*************************************************************************************************/
static void lctrSlvProcessDataPdu(lctrConnCtx_t *pCtx, uint8_t *pBuf)
{
  uint8_t result;

  if (pCtx->enabled)
  {
    result = lctrDecodeCtrlPdu(&lctrDataPdu, pBuf, pCtx->role);

    switch (result)
    {
      case LL_SUCCESS:
#if (LL_ENABLE_TESTER)
        if ((llTesterCb.rxLlcpFilter & (1 << lctrDataPdu.opcode)) != 0)
        {
          return;
        }
#endif
        lctrSlvConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP);
        break;
      case LL_ERROR_CODE_INVALID_LMP_PARAMS:
        lctrSlvConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP_INVALID_PARAM);
        break;
      default:
        lctrSlvConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP_UNKNOWN);
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Slave connection reset handler.
 */
/*************************************************************************************************/
static void lctrSlvConnResetHandler(void)
{
  BbBleConnSlaveInit();
  SchRmInit();
  SchTmInit();
  lctrConnDefaults();
  LmgrConnInit();
}

/*************************************************************************************************/
/*!
 *  \brief      Execute slave state machine.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrSlvConnExecute(lctrConnMsg_t *pMsg)
{
  lctrConnCtx_t *pCtx;
  pCtx = LCTR_GET_CONN_CTX(pMsg->hdr.handle);

  if (pCtx->enabled)
  {
    pLctrConnMsg = pMsg;

    lctrSlvConnExecuteSm(pCtx, pMsg->hdr.event);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Slave connection message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrSlvConnDisp(lctrConnMsg_t *pMsg)
{
  if (pMsg->hdr.dispId != LCTR_DISP_BCST)
  {
    pLctrConnMsg = pMsg;

    WSF_ASSERT(pMsg->hdr.handle < pLctrRtCfg->maxConn);
    lctrSlvConnExecute(pMsg);
  }
  else
  {
    for (pMsg->hdr.handle = 0; pMsg->hdr.handle < pLctrRtCfg->maxConn; pMsg->hdr.handle++)
    {
      pLctrConnMsg = pMsg;

      lctrSlvConnExecute(pMsg);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get reference time(due time) of the slave connection handle.
 *
 *  \param      connHandle    Connection handle.
 *
 *  \return     Due time in microseconds of the connection handle.
 */
/*************************************************************************************************/
static uint32_t lctrGetSlvConnRefTime(uint8_t connHandle)
{
  uint32_t refTime = 0;
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(connHandle);

  if (pCtx->enabled && (pCtx->bleData.chan.opType == BB_BLE_OP_SLV_CONN_EVENT))
  {
    refTime = pCtx->connBod.dueUsec;
  }

  return refTime;
}

/*************************************************************************************************/
/*!
 *  \brief  Compute the total clock accuracy.
 *
 *  \param  mstScaIdx   Master sleep clock accuracy index.
 *
 *  \return Combined sleep clock inaccuracy.
 */
/*************************************************************************************************/
uint16_t lctrCalcTotalAccuracy(uint8_t mstScaIdx)
{
  const uint16_t clkPpm = BbGetClockAccuracy();

  if (mstScaIdx >= (sizeof(scaPpmTbl) / sizeof(scaPpmTbl[0])))
  {
    /* Cap to highest index. */
    mstScaIdx = LL_MCA_20_PPM;
  }

  return scaPpmTbl[mstScaIdx] + clkPpm;
}

/*************************************************************************************************/
/*!
 *  \brief  Build a connection operation.
 *
 *  \param  pCtx        Connection context.
 *
 *  This routine is called in response to a received CONNECTION_REQ PDU. The PDU must already
 *  be decoded in the lctrPduSlvAdvb variable prior to this call.
 */
/*************************************************************************************************/
void lctrSlvConnBuildOp(lctrConnCtx_t *pCtx)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &pCtx->connBod;
  BbBleData_t * const pBle = &pCtx->bleData;
  BbBleSlvConnEvent_t * const pConn = &pBle->op.slvConn;
  lctrConnInd_t * const pConnInd = &pLctrConnMsg->connEstablish.connInd;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));
  memset(pConn, 0, sizeof(BbBleSlvConnEvent_t));

  /*** Connection context setup ***/
  uint16_t  txWinDelay;
  if (pLctrConnMsg->connEstablish.isAuxConnReq)
  {
    switch (pLctrConnMsg->connEstablish.phy)
    {
      case BB_PHY_BLE_1M:
      case BB_PHY_BLE_2M:
      default:
        txWinDelay = LCTR_DATA_CHAN_DLY_AUX_UNCODED;
        break;
      case BB_PHY_BLE_CODED:
        txWinDelay = LCTR_DATA_CHAN_DLY_AUX_CODED;
        break;
    }
  }
  else
  {
    txWinDelay = LCTR_DATA_CHAN_DLY;
  }

  const uint16_t txWinOffsetCnt = pConnInd->txWinOffset + txWinDelay;

  /* pCtx->lastChanIdx = 0; */              /* cleared in alloc */
  /* pCtx->eventCounter = 0; */             /* cleared in alloc */
  pCtx->data.slv.lastActiveEvent = 1;       /* guarantee execution of first connection event. */

  pCtx->usedChSel = pLctrConnMsg->connEstablish.usedChSel;

  pCtx->chanMask = pConnInd->chanMask;
  pCtx->hopInc = pConnInd->hopInc;
  pCtx->connInterval = pConnInd->interval;
  /* pCtx->txHdr.sn = 0; */                 /* cleared in alloc */
  /* pCtx->txHdr.nesn = 0; */               /* cleared in alloc */
  pCtx->data.slv.totalAcc = lctrCalcTotalAccuracy(pConnInd->masterSca);
  pCtx->maxLatency = pConnInd->latency;
  pCtx->supTimeoutMs = LCTR_CONN_IND_TO_MS(pConnInd->timeout);
  pCtx->authTimeoutMs = LL_DEF_AUTH_TO_MS;
  pCtx->pingPeriodMs = lctrCalcPingPeriodMs(pCtx, LL_DEF_AUTH_TO_MS);
  /* pCtx->llcpState = LCTR_LLCP_STATE_IDLE; */
  pCtx->llcpActiveProc = LCTR_PROC_INVALID;
  pCtx->crcInit = pConnInd->crcInit;

  /* Initially use fast termination. */
  uint32_t fastTermCnt = txWinOffsetCnt + pConnInd->txWinSize +
                         (LCTR_FAST_TERM_CNT * pConnInd->interval);
  WsfTimerStartMs(&pCtx->tmrSupTimeout, LCTR_CONN_IND_MS(fastTermCnt));

  lctrBuildRemapTable(pCtx);
  pCtx->chIdentifier = (pConnInd->accessAddr >> 16) ^
                       (pConnInd->accessAddr >> 0);

  /*** BLE general setup ***/

  pBle->chan.opType = BB_BLE_OP_SLV_CONN_EVENT;

  /* pBle->chan.chanIdx = 0; */             /* deferred assignment */
  pBle->chan.txPower = pLctrRtCfg->defTxPwrLvl;
  pBle->chan.accAddr = pConnInd->accessAddr;
  pBle->chan.crcInit = pConnInd->crcInit;
  pBle->chan.txPhy = pBle->chan.rxPhy = pLctrConnMsg->connEstablish.phy;
  lctrInitPhyTxPower(pCtx);

#if (LL_ENABLE_TESTER)
  pBle->chan.accAddrRx = pConnInd->accessAddr ^ llTesterCb.dataAccessAddrRx;
  pBle->chan.accAddrTx = pConnInd->accessAddr ^ llTesterCb.dataAccessAddrTx;
  pBle->chan.crcInitRx = pConnInd->crcInit    ^ llTesterCb.dataCrcInitRx;
  pBle->chan.crcInitTx = pConnInd->crcInit    ^ llTesterCb.dataCrcInitTx;
#endif

  /* pBle->chan.enc.enaEncrypt = FALSE; */  /* cleared in alloc */
  /* pBle->chan.enc.enaDecrypt = FALSE; */
  pBle->chan.enc.enaAuth = TRUE;
  /* pBle->chan.enc.nonceMode = PAL_BB_NONCE_MODE_PKT_CNTR; */  /* cleared in alloc */

  pCtx->txHdr.llid = ~LL_LLID_VS_PDU;     /* reset last PDU LLID */

  pCtx->effConnDurUsec = lctrCalcConnDurationUsec(pCtx, &pCtx->effDataPdu);      /* actual PHY */
  pCtx->localConnDurUsec = lctrCalcConnDurationUsec(pCtx, &pCtx->localDataPdu);  /* actual PHY */

   /*** General setup ***/

  const uint32_t txWinOffsetUsec = LCTR_CONN_IND_US(txWinOffsetCnt);
  const uint32_t txWinSizeUsec   = LCTR_CONN_IND_US(pConnInd->txWinSize);
  const uint32_t wwOffsetUsec    = lctrCalcWindowWideningUsec((txWinOffsetUsec + txWinSizeUsec), pCtx->data.slv.totalAcc);

  pCtx->data.slv.anchorPointUsec = pLctrConnMsg->connEstablish.connIndEndTsUsec + txWinOffsetUsec;    /* estimated initial anchor point */
  pCtx->data.slv.txWinSizeUsec = txWinSizeUsec;

  pOp->dueUsec = pCtx->data.slv.anchorPointUsec - wwOffsetUsec;
  pOp->minDurUsec = txWinSizeUsec + pCtx->effConnDurUsec + (wwOffsetUsec << 1);
  /* pOp->maxDurUsec = 0; */                  /* cleared in alloc */

  pOp->reschPolicy = BB_RESCH_FIXED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrSlvConnEndOp;
  pOp->abortCback = lctrSlvConnAbortOp;
  pOp->pCtx = pCtx;

  /*** BLE connection setup ***/

  pConn->rxSyncDelayUsec = txWinSizeUsec + (wwOffsetUsec << 1) +
                           1;    /* rounding compensation when computing reqEndTs */
  pConn->execCback   = lctrSlvConnBeginOp;
  pConn->cancelCback = lctrSlvConnCleanupOp;
  pConn->txDataCback = lctrSlvConnTxCompletion;
  pConn->rxDataCback = lctrSlvConnRxCompletion;

  /*** Commit operation ***/

  const uint32_t ceDurUsec = pOp->minDurUsec;
  const uint32_t ceSyncDlyUsec = pConn->rxSyncDelayUsec;

  while (TRUE)
  {
    pBle->chan.chanIdx = lctrChSelHdlr[pCtx->usedChSel](pCtx, 0);

    if (SchInsertAtDueTime(pOp, lctrConnResolveConflict))
    {
      LL_TRACE_INFO1("    >>> Connection established, handle=%u <<<", LCTR_GET_CONN_HANDLE(pCtx));
      LL_TRACE_INFO1("                                connIntervalUsec=%u", LCTR_CONN_IND_US(pCtx->connInterval));
      LL_TRACE_INFO1("                                dueUsec=%u", pOp->dueUsec);
      LL_TRACE_INFO1("                                minDurUsec=%u", pOp->minDurUsec);
      LL_TRACE_INFO1("                                pBod=0x%08x", pOp);
      break;
    }

    LL_TRACE_WARN1("!!! Establish CE schedule conflict handle=%u", LCTR_GET_CONN_HANDLE(pCtx));

    pCtx->eventCounter += 1;

    /* Initial eventCounter starts at 0; equivalent to unsynchronized intervals. */
    uint32_t unsyncTimeUsec = LCTR_CONN_IND_US(pCtx->connInterval * pCtx->eventCounter);
    uint32_t wwTotalUsec    = lctrCalcWindowWideningUsec(unsyncTimeUsec, pCtx->data.slv.totalAcc);

    /* Advance to next interval. */
    pOp->dueUsec = pCtx->data.slv.anchorPointUsec + unsyncTimeUsec - wwTotalUsec;

    pOp->minDurUsec = ceDurUsec + wwTotalUsec;
    pConn->rxSyncDelayUsec = ceSyncDlyUsec + (wwTotalUsec << 1);
  }

  /* Update topology manager information. */
  SchTmAdd(LCTR_GET_CONN_HANDLE(pCtx), LCTR_CONN_IND_US(pCtx->connInterval), pCtx->effConnDurUsec, TRUE, lctrGetSlvConnRefTime);
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for connectable slave.
 */
/*************************************************************************************************/
void LctrSlvConnInit(void)
{
  /* Add reset handler (values set by master prevails). */
  lctrResetHdlrTbl[LCTR_DISP_CONN] = lctrSlvConnResetHandler;

  /* Add connection message dispatchers (values set by master prevails). */
  lctrMsgDispTbl[LCTR_DISP_CONN_IND] = (LctrMsgDisp_t)lctrSlvProcessConnInd;
  if (!lctrMsgDispTbl[LCTR_DISP_CONN])
  {
    lctrMsgDispTbl[LCTR_DISP_CONN] = (LctrMsgDisp_t)lctrSlvConnDisp;
  }

  /* Add connection event handlers. */
  lctrEventHdlrTbl[LCTR_EVENT_RX_PENDING]  = lctrConnRxPendingHandler;
  /* lctrEventHdlrTbl[LCTR_EVENT_TX_PENDING]  = lctrSlvConnTxPendingHandler; */     /* not used by slave */
  lctrEventHdlrTbl[LCTR_EVENT_TX_COMPLETE] = lctrConnTxCompletedHandler;

  /* Add LLCP SM handler. */
  lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CONN_UPD] = lctrSlvLlcpExecuteConnUpdSm;
  lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CMN]      = lctrLlcpExecuteCommonSm;

  /* Add control PDU handler. */
  if (!lctrCtrlPduHdlr)
  {
    lctrCtrlPduHdlr = lctrSlvProcessDataPdu;
  }

  /* Add channel selection handler. */
  lctrChSelHdlr[LL_CH_SEL_1] = lctrSelectNextDataChannel;

  /* Register channel class update handler. */
  lctrRegisterChClassHandler(lctrConnChClassUpdate);

  lctrConnDefaults();

  /* Set supported features. */
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_4_1)
  {
    lmgrPersistCb.featuresDefault |=
      LL_FEAT_CONN_PARAM_REQ_PROC |
      LL_FEAT_EXT_REJECT_IND |
      LL_FEAT_SLV_INIT_FEAT_EXCH;
  }
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_4_2)
  {
    lmgrPersistCb.featuresDefault |=
      LL_FEAT_DATA_LEN_EXT;
  }
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_0)
  {
    lmgrPersistCb.featuresDefault |=
      LL_FEAT_MIN_NUM_USED_CHAN;
  }
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_1)
  {
    lmgrPersistCb.featuresDefault |=
      LL_FEAT_PAST_SENDER |
      LL_FEAT_SCA_UPDATE;
  }
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_SYDNEY)
  {
    lmgrPersistCb.featuresDefault |=
      LL_FEAT_CHANNEL_CLASSIFICATION;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set default values for connection.
 */
/*************************************************************************************************/
void lctrConnDefaults(void)
{
  memset(pLctrConnTbl, 0, (sizeof(lctrConnCtx_t) * pLctrRtCfg->maxConn));
}
