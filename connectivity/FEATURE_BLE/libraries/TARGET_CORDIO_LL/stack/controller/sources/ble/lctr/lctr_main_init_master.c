/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master scanning operation builder implementation file.
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

#include "lctr_int_adv_master.h"
#include "lctr_int_init_master.h"
#include "lctr_int.h"
#include "sch_api.h"
#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>
#include <stddef.h>

/**************************************************************************************************
  Globals
**************************************************************************************************/
/*! \brief      Initiate operational context. */
lctrMstScanCtx_t lctrMstInit;

/*************************************************************************************************/
/*!
 *  \brief      Master initiate reset handler.
 */
/*************************************************************************************************/
static void lctrMstInitResetHandler(void)
{
  LctrMstInitDefaults();
}

/*************************************************************************************************/
/*!
 *  \brief      Master initiate message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstInitDisp(lctrMsgHdr_t *pMsg)
{
  lctrMstInitExecuteSm(pMsg->event);
}

/*************************************************************************************************/
/*!
 *  \brief  Build initiate connection operation.
 *
 *  \param  pConnSpec       Connection spec.
 *  \param  peerAddrType    Peer address type.
 *  \param  peerAddr        Peer address.
 */
/*************************************************************************************************/
void lctrMstInitiateBuildOp(LlConnSpec_t *pConnSpec, uint8_t peerAddrType, uint64_t peerAddr)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &lctrMstInit.scanBod;
  BbBleData_t * const pBle = &lctrMstInit.bleData;
  BbBleMstAdvEvent_t * const pScan = &pBle->op.mstAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  uint8_t *pBuf;

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_MOVEABLE_PREFERRED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrMstInitiateEndOp;
  pOp->abortCback = lctrMstInitiateEndOp;

  /*** BLE General Setup ***/

  pBle->chan.opType = BB_BLE_OP_MST_ADV_EVENT;

  pBle->chan.chanIdx = lctrScanChanSelectInit(lmgrMstScanCb.scanChanMap);
  pBle->chan.txPower = lmgrCb.advTxPwr;
  pBle->chan.accAddr = LL_ADV_ACCESS_ADDR;
  pBle->chan.crcInit = LL_ADV_CRC_INIT;
  pBle->chan.rxPhy = BB_PHY_BLE_1M;
  pBle->chan.txPhy = BB_PHY_BLE_1M;

#if (LL_ENABLE_TESTER)
  pBle->chan.accAddrRx = llTesterCb.advAccessAddrRx ^ pBle->chan.accAddr;
  pBle->chan.accAddrTx = llTesterCb.advAccessAddrTx ^ pBle->chan.accAddr;
  pBle->chan.crcInitRx = llTesterCb.advCrcInitRx ^ pBle->chan.crcInit;
  pBle->chan.crcInitTx = llTesterCb.advCrcInitTx ^ pBle->chan.crcInit;
#endif

  pBle->pduFilt.pduTypeFilt = (1 << LL_PDU_ADV_IND) |
                              (1 << LL_PDU_ADV_DIRECT_IND);
  if (lctrMstInit.scanParam.scanFiltPolicy != LL_SCAN_FILTER_NONE)
  {
    pBle->pduFilt.wlPduTypeFilt = pBle->pduFilt.pduTypeFilt;
  }

  /*** BLE Scan Setup: Rx packets ***/

  pScan->scanChMap = lmgrMstScanCb.scanChanMap;

  pScan->rxAdvCback = lctrMstInitiateAdvPktHandler;

  if ((pScan->pRxAdvBuf = WsfMsgAlloc(LCTR_ADVB_BUF_SIZE)) == NULL)
  {
    /* Attempt to obtain buffer on next advertising operation. */
    LL_TRACE_ERR0("Could not allocate advertising buffer");
    /* TODO need OOM recovery */
    WSF_ASSERT(FALSE);
  }

  /*** BLE Scan Setup: Tx connect indication packet ***/

  pScan->txReqCback = lctrMstConnIndTxCompHandler;

  lctrConnInd_t * const pConnInd = &lctrMstInit.data.init.connInd;

  pConnInd->accessAddr = lctrComputeAccessAddr();
  pConnInd->crcInit = lctrComputeCrcInit();
  pConnInd->txWinSize = LL_MIN_TX_WIN_SIZE;              /* minimum size */
  /* pConnInd->txWinOffset = 0; */      /* Updated in ISR immediately prior to Tx LL_CONN_IND. */
  pConnInd->interval = lctrMstInit.data.init.connInterval;
  pConnInd->latency = pConnSpec->connLatency;
  pConnInd->timeout = pConnSpec->supTimeout;
  pConnInd->chanMask = lmgrCb.chanClass;
  pConnInd->hopInc = lctrComputeHopInc();
  pConnInd->masterSca = lctrComputeSca();

#if (LL_ENABLE_TESTER)
  if (llTesterCb.connIndEnabled)
  {
    memcpy(&pConnInd->accessAddr,
           &llTesterCb.connInd.accessAddr,
           sizeof(lctrConnInd_t) - offsetof(lctrConnInd_t, accessAddr));

    llTesterCb.connIndEnabled = FALSE;
  }
#endif

  lctrMstInit.reqPduHdr.pduType = LL_PDU_CONNECT_IND;
  lctrMstInit.reqPduHdr.len = LL_CONN_IND_PDU_LEN;

  if (lmgrCb.features & LL_FEAT_CH_SEL_2)
  {
    lctrMstInit.reqPduHdr.chSel = LL_CH_SEL_2;
  }
  else
  {
    lctrMstInit.reqPduHdr.chSel = LL_CH_SEL_1;
  }

  /* Always match local address in PDU to initiator's address (in directed advertisements). */
  if (lctrMstInit.scanParam.ownAddrType & LL_ADDR_RANDOM_BIT)
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

  /* Choose initiator's address. */
  lctrMstInit.reqPduHdr.txAddrRnd = BB_BLE_PDU_FILT_FLAG_IS_SET(&pBle->pduFilt, LOCAL_ADDR_MATCH_RAND);
  pConnInd->initAddr = pBle->pduFilt.localAddrMatch;

  /* peerAddrType and pPeerAddr only valid when filter policy is set to none */
  if (lctrMstInit.scanParam.scanFiltPolicy == LL_SCAN_FILTER_NONE)
  {
    /* Set advertiser's address. */
    lctrMstInit.reqPduHdr.rxAddrRnd = peerAddrType & LL_ADDR_RANDOM_BIT;
    pConnInd->advAddr = peerAddr;

    pBle->pduFilt.peerAddrMatch = peerAddr;
    if (peerAddrType & LL_ADDR_RANDOM_BIT)
    {
      BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_MATCH_RAND);
    }
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_MATCH_ENA);
  }

  pBuf  = lctrMstInit.reqBuf;
  pBuf += lctrPackAdvbPduHdr(pBuf, &lctrMstInit.reqPduHdr);
  /* pBuf += */ lctrPackConnIndPdu(pBuf, pConnInd);

  pScan->pTxReqBuf = lctrMstInit.reqBuf;
  pScan->txReqLen = LL_ADV_HDR_LEN + LL_CONN_IND_PDU_LEN;

  /*** Commit operation ***/

  /* Postponed in lctrMstInitiateOpCommit() until after connBod is built. */
}

/*************************************************************************************************/
/*!
 *  \brief  Commit initiate connection operation.
 */
/*************************************************************************************************/
void lctrMstInitiateOpCommit(void)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &lctrMstInit.scanBod;

  /*** Commit operation ***/

  pOp->minDurUsec = LCTR_MIN_SCAN_USEC;
  pOp->maxDurUsec = LCTR_BLE_TO_US(lctrMstInit.scanParam.scanWindow);

  lctrMstInit.selfTerm = FALSE;
  lctrMstInit.shutdown = FALSE;

  SchInsertNextAvailable(pOp);
  lctrMstInit.scanWinStartUsec = pOp->dueUsec;

  LL_TRACE_INFO1("    >>> Initiate started, dueUsec=%u <<<", pOp->dueUsec);
  LL_TRACE_INFO1("                          pBod=0x%08x", pOp);
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for initiating master.
 */
/*************************************************************************************************/
void LctrMstInitInit(void)
{
  /* Add initiate reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_INIT] = lctrMstInitResetHandler;

  /* Add initiate message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_INIT] = (LctrMsgDisp_t)lctrMstInitDisp;

  LctrMstInitDefaults();
}

/*************************************************************************************************/
/*!
 *  \brief      Set default values for scanning master.
 */
/*************************************************************************************************/
void LctrMstInitDefaults(void)
{
  memset(&lctrMstInit, 0, sizeof(lctrMstScan));
}


/*************************************************************************************************/
/*!
 *  \brief      Check whether init is enabled or not.
 *
 *  \return     True if scanner enabled. False if not.
 */
/*************************************************************************************************/
bool_t LctrMstInitIsEnabled(void)
{
  return (lctrMstInit.state != LCTR_SCAN_STATE_DISABLED);
}

/*************************************************************************************************/
/*!
 *  \brief      Check if private address is being used for scanner
 *
 *  \return     Returns True if scanner is using private addresses. False if not.
 */
/*************************************************************************************************/
bool_t LctrMstInitIsPrivAddr(void)
{
  /* Check if private address bit is set. */
  return (lctrMstInit.scanParam.ownAddrType & LL_ADDR_RANDOM_BIT);
}
