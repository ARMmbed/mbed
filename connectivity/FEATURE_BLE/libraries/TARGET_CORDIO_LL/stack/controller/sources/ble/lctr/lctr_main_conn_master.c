/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master connection operation builder implementation file.
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

#include "lctr_int.h"
#include "lctr_int_conn.h"
#include "lctr_int_conn_slave.h"
#include "lctr_int_conn_master.h"
#include "lctr_int_adv_master.h"
#include "lmgr_api_conn.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "wsf_trace.h"
#include <string.h>

/**************************************************************************************************
  Globals
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Process a received data channel PDU in master role.
 *
 *  \param      pCtx    Connection context.
 *  \param      pBuf    PDU buffer.
 */
/*************************************************************************************************/
static void lctrMstProcessDataPdu(lctrConnCtx_t *pCtx, uint8_t *pBuf)
{
  if (pCtx->enabled)
  {
    uint8_t result = lctrDecodeCtrlPdu(&lctrDataPdu, pBuf, pCtx->role);

    switch (result)
    {
      case LL_SUCCESS:
#if (LL_ENABLE_TESTER)
        if ((llTesterCb.rxLlcpFilter & (1 << lctrDataPdu.opcode)) != 0)
        {
          return;
        }
#endif
        if (pCtx->role == LL_ROLE_MASTER)
        {
          lctrMstConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP);
        }
        else
        {
          lctrSlvConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP);
        }
        break;
      case LL_ERROR_CODE_INVALID_LMP_PARAMS:
        if (pCtx->role == LL_ROLE_MASTER)
        {
          lctrMstConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP_INVALID_PARAM);
        }
        else
        {
          lctrSlvConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP_INVALID_PARAM);
        }
        break;
      default:
        if (pCtx->role == LL_ROLE_MASTER)
        {
          lctrMstConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP_UNKNOWN);
        }
        else
        {
          lctrSlvConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP_UNKNOWN);
        }
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Master Tx data pending task event handler.
 */
/*************************************************************************************************/
static void lctrMstConnTxPendingHandler(void)
{
  unsigned int i;

  for (i = 0; i < pLctrRtCfg->maxConn; i++)
  {
    lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(i);
    if ((pCtx->enabled) &&
        !WsfQueueEmpty(&pCtx->txArqQ))
    {
      if (pCtx->role == LL_ROLE_MASTER)
      {
        lctrMstConnExecuteSm(pCtx, LCTR_CONN_DATA_PENDING);
      }
      else
      {
        /* Event is not used by the slave. */
        /* lctrSlvConnExecuteSm(pCtx, LCTR_CONN_DATA_PENDING); */
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Master connection reset handler.
 */
/*************************************************************************************************/
static void lctrMstConnResetHandler(void)
{
  BbBleConnSlaveInit();
  BbBleConnMasterInit();
  SchRmInit();
  SchTmInit();
  lctrConnDefaults();
  LmgrConnInit();
}

/*************************************************************************************************/
/*!
 *  \brief      Execute master state machine.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstConnExecute(lctrConnMsg_t *pMsg)
{
  lctrConnCtx_t *pCtx;
  pCtx = LCTR_GET_CONN_CTX(pMsg->hdr.handle);

  if (pCtx->enabled)
  {
    pLctrConnMsg = pMsg;

    if (pCtx->role == LL_ROLE_MASTER)
    {
      lctrMstConnExecuteSm(pCtx, pMsg->hdr.event);
    }
    else
    {
      lctrSlvConnExecuteSm(pCtx, pMsg->hdr.event);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Master connection message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstConnDisp(lctrConnMsg_t *pMsg)
{
  if (pMsg->hdr.dispId != LCTR_DISP_BCST)
  {
    pLctrConnMsg = pMsg;

    WSF_ASSERT(pMsg->hdr.handle < (pLctrRtCfg->maxConn + pLctrRtCfg->maxCis));
    lctrMstConnExecute(pMsg);
  }
  else
  {
    for (pMsg->hdr.handle = 0; pMsg->hdr.handle < pLctrRtCfg->maxConn; pMsg->hdr.handle++)
    {
      pLctrConnMsg = pMsg;

      lctrMstConnExecute(pMsg);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Build a connection operation.
 *
 *  \param  pCtx        Connection context.
 *  \param  pConnInd    Connection indication.
 */
/*************************************************************************************************/
void lctrMstConnBuildOp(lctrConnCtx_t *pCtx, lctrConnInd_t *pConnInd)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &pCtx->connBod;
  BbBleData_t * const pBle = &pCtx->bleData;
  BbBleMstConnEvent_t * const pConn = &pBle->op.mstConn;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));
  memset(pConn, 0, sizeof(BbBleMstConnEvent_t));

  /*** Connection context setup ***/

  pCtx->lastChanIdx = 0;
  pCtx->eventCounter = 0;
  pCtx->chanMask = pConnInd->chanMask;
  pCtx->hopInc = pConnInd->hopInc;
  pCtx->connInterval = pConnInd->interval;
  /* pCtx->txHdr.sn = 0; */                 /* cleared in alloc */
  /* pCtx->txHdr.nesn = 0; */               /* cleared in alloc */
  pCtx->maxLatency = pConnInd->latency;
  pCtx->supTimeoutMs = LCTR_CONN_IND_TO_MS(pConnInd->timeout);
  pCtx->authTimeoutMs = LL_DEF_AUTH_TO_MS;
  pCtx->pingPeriodMs = lctrCalcPingPeriodMs(pCtx, LL_DEF_AUTH_TO_MS);
  /* pCtx->llcpState = LCTR_LLCP_STATE_IDLE; */
  pCtx->llcpActiveProc = LCTR_PROC_INVALID;
  pCtx->crcInit = pConnInd->crcInit;
  pCtx->accessAddr = pConnInd->accessAddr;

  lctrBuildRemapTable(pCtx);
  pCtx->chIdentifier = (pConnInd->accessAddr >> 16) ^
                       (pConnInd->accessAddr >> 0);

  /*** BLE general setup ***/

  pBle->chan.opType = BB_BLE_OP_MST_CONN_EVENT;

  /* pBle->chan.chanIdx = 0; */             /* deferred assignment */
  pBle->chan.txPower = pLctrRtCfg->defTxPwrLvl;
  pBle->chan.accAddr = pConnInd->accessAddr;
  pBle->chan.crcInit = pConnInd->crcInit;
  pBle->chan.txPhy = BB_PHY_BLE_1M;  /* same PHY as CONNECT_REQ */
  pBle->chan.rxPhy = BB_PHY_BLE_1M;  /* same PHY as CONNECT_REQ */

  /* Set PHY options to default behavior for connection. */
  pBle->chan.initTxPhyOptions = BB_PHY_OPTIONS_BLE_S8; /* TODO: Change to scanned PHY options from extended advertiser. */
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

  pOp->minDurUsec = pCtx->effConnDurUsec;
  pOp->maxDurUsec = LCTR_CONN_IND_US(pCtx->connInterval);

  /* pOp->due = scanRefTime + BB_US_TO_BB_TICKS(maxOffsetUsec); */  /* set in lctrMstConnAdjustOpStart() */
  pOp->reschPolicy = BB_RESCH_FIXED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrMstConnEndOp;
  pOp->abortCback = lctrMstConnAbortOp;
  pOp->pCtx = pCtx;

  /*** BLE connection setup ***/

  pConn->execCback = lctrMstConnBeginOp;
  pConn->cancelCback = lctrMstConnCleanupOp;
  pConn->txDataCback = lctrMstConnTxCompletion;
  pConn->rxDataCback = lctrMstConnRxCompletion;

  /*** Commit operation ***/

  /* Postponed in lctrMstConnAdjustOpStart() when scan operation begins. */
}

/*************************************************************************************************/
/*!
 *  \brief      Set the establish connection state.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrMstSetEstablishConn(lctrConnCtx_t *pCtx)
{
  lctrConnInd_t * const pConnInd = &pLctrConnMsg->connEstablish.connInd;

  const uint16_t txWinOffsetCnt = pConnInd->txWinOffset + LCTR_DATA_CHAN_DLY;

  /* Initially use fast termination. */
  /* The first CE starts after transmitWindowDelay + transmitWindowOffset + transmitWindowSize. 
     Then an additional 5 CI are required + the duration of the last connection event. */
  uint32_t fastTermCnt = txWinOffsetCnt + pConnInd->txWinSize +
                         ((LCTR_FAST_TERM_CNT - 1) * pConnInd->interval) + (pConnInd->interval >> 1);
  WsfTimerStartMs(&pCtx->tmrSupTimeout, LCTR_CONN_IND_MS(fastTermCnt));

  /* Set initial channel. */
  pCtx->usedChSel = pLctrConnMsg->connEstablish.usedChSel;
  pCtx->bleData.chan.txPhy = pCtx->bleData.chan.rxPhy = pLctrConnMsg->connEstablish.phy;
  pCtx->bleData.chan.chanIdx = lctrChSelHdlr[pCtx->usedChSel](pCtx, 0);
  /* Update BOD using new channel selection algorithm. */
  SchReload(&pCtx->connBod);

  LL_TRACE_INFO1("    >>> Connection established, handle=%u <<<", LCTR_GET_CONN_HANDLE(pCtx));
  LL_TRACE_INFO1("                                connIntervalUsec=%u", LCTR_CONN_IND_US(pCtx->connInterval));
  LL_TRACE_INFO1("                                dueUsec=%u", pCtx->connBod.dueUsec);
  LL_TRACE_INFO1("                                pBod=0x%08x", &pCtx->connBod);
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for connectable master.
 */
/*************************************************************************************************/
void LctrMstConnInit(void)
{
  /* Add reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_CONN] = lctrMstConnResetHandler;

  /* Add connection message dispatcher. */
  lctrMsgDispTbl[LCTR_DISP_CONN] = (LctrMsgDisp_t)lctrMstConnDisp;

  /* Add connection event handlers. */
  lctrEventHdlrTbl[LCTR_EVENT_RX_PENDING]  = lctrConnRxPendingHandler;
  lctrEventHdlrTbl[LCTR_EVENT_TX_PENDING]  = lctrMstConnTxPendingHandler;
  lctrEventHdlrTbl[LCTR_EVENT_TX_COMPLETE] = lctrConnTxCompletedHandler;

  /* Add LLCP SM handler. */
  lctrMstLlcpSmTbl[LCTR_LLCP_SM_CONN_UPD] = lctrMstLlcpExecuteConnUpdSm;
  lctrMstLlcpSmTbl[LCTR_LLCP_SM_CMN]      = lctrLlcpExecuteCommonSm;

  /* Add control PDU handler. */
  lctrCtrlPduHdlr = lctrMstProcessDataPdu;

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
 *  \brief  Compute new CRC init.
 *
 *  \return New CRC init value.
 */
/*************************************************************************************************/
uint32_t lctrComputeCrcInit(void)
{
  return LlMathRandNum() & 0xFFFFFF;
}

/*************************************************************************************************/
/*!
 *  \brief  Compute hop increment value.
 *
 *  \return Hop increment value.
 */
/*************************************************************************************************/
uint8_t lctrComputeHopInc(void)
{
  uint32_t randNum = LlMathRandNum();

  /* Valid value range is 5 to 16. */
  return (((randNum >> 0) & 7) + ((randNum >> 8) & 3) + ((randNum >> 16) & 1)) + 5;
}
