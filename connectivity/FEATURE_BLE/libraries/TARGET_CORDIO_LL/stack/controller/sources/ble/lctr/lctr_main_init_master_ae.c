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

#include "lctr_int_init_master_ae.h"
#include "lctr_int.h"
#include "sch_api_ble.h"
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
  Macros
**************************************************************************************************/
/*! \brief      Resolve the extended initiate handle from the context pointer. */
#define LCTR_GET_EXT_INIT_HANDLE(pCtx)  (pCtx - lctrMstExtInitTbl)

/*! \brief      Typical PDU length of ADV_EXT_IND when initiating connection. */
#define LCTR_INIT_ADV_EXT_IND_LEN       7       /*!< Includes Flags + ADI + AuxPtr. */

/*! \brief      Typical PDU length of AUX_ADV_IND when initiating connection. */
#define LCTR_INIT_AUX_ADV_IND_LEN       10      /*!< Includes Flags + AdvA + ADI. */

/*! \brief      Typical PDU length of ADV_IND when initiating connection with legacy packets. */
#define LCTR_INIT_ADV_IND_LEN           6

/**************************************************************************************************
  Globals
**************************************************************************************************/
/*! \brief      Initiate operational context. */
/*! \brief      Extended scan operational context. */
lctrExtScanCtx_t lctrMstExtInitTbl[LCTR_SCAN_PHY_TOTAL];

/*! \brief      Extended initiator control block. */
lctrExtInitCtrlBlk_t lctrMstExtInit;

/*************************************************************************************************/
/*!
 *  \brief      Master initiate reset handler.
 */
/*************************************************************************************************/
static void lctrMstExtInitResetHandler(void)
{
  LctrMstExtInitDefaults();
}

/*************************************************************************************************/
/*!
 *  \brief      Execute common master initiate state machine.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstExtInitExecuteCommonSm(LctrExtScanMsg_t *pMsg)
{
  /* Subsystem event handling. */
  switch (pMsg->hdr.event)
  {
    case LCTR_EXT_INIT_MSG_INITIATE:
    {
      LL_TRACE_INFO1("lctrMstExtInitExecuteCommonSm: scanMode=%u, event=INITIATE", lmgrCb.scanMode);
      lctrExtInitiateMsg_t *pExtInitMsg = (lctrExtInitiateMsg_t *)pLctrMsg;

      uint8_t status = 0;
      unsigned int i;

      /* Check worst case connSpec among all PHYs. */
      for (i = 0; i < LCTR_SCAN_PHY_TOTAL; i++)
      {
        if (lctrMstExtInit.enaPhys & (1 << i))
        {
          /* TODO: pre-check all connSpec with RM. */
        }
      }

      for (i = 0; i < LCTR_SCAN_PHY_TOTAL; i++)
      {
        if (lctrMstExtInit.enaPhys & (1 << i))
        {
          if ((status = lctrExtInitSetupInitiate(&lctrMstExtInitTbl[i], pExtInitMsg->peerAddrType, pExtInitMsg->peerAddr,
                                                 pExtInitMsg->filterPolicy, pExtInitMsg->ownAddrType)) != LL_SUCCESS)
          {
            /* TODO for multiple scanners, cleanup upon failure */
            break;
          }
        }
      }

      if (status != LL_SUCCESS)
      {
        break;
      }

      lctrMstExtInit.estConnPhys = 0;
      lctrMstExtInit.peerAddr = pExtInitMsg->peerAddr;
      lctrMstExtInit.peerAddrType = pExtInitMsg->peerAddrType;

      for (i = 0; i < LCTR_SCAN_PHY_TOTAL; i++)
      {
        if (lctrMstExtInit.enaPhys & (1 << i))
        {
          lctrMstConnBuildOp(LCTR_GET_CONN_CTX(lctrMstExtInitTbl[i].data.init.connHandle),
                             &lctrMstExtInitTbl[i].data.init.connInd);
          lctrMstExtInitiateOpCommit(&lctrMstExtInitTbl[i]);
        }
      }
      break;
    }

    case LCTR_EXT_INIT_MSG_INITIATE_CANCEL:
      LL_TRACE_INFO1("lctrMstExtInitExecuteCommonSm: scanMode=%u, event=INITIATE_CANCEL", lmgrCb.scanMode);
      break;

    case LCTR_EXT_INIT_MSG_RESET:
      LL_TRACE_INFO1("lctrMstExtInitExecuteCommonSm: scanMode=%u, event=RESET", lmgrCb.scanMode);
      break;

    case LCTR_EXT_INIT_MSG_TERMINATE:
      LL_TRACE_INFO1("lctrMstExtInitExecuteCommonSm: scanMode=%u, event=TERMINATE", lmgrCb.scanMode);
      break;

    default:
      LL_TRACE_ERR2("lctrMstExtInitExecuteCommonSm: scanMode=%u, event=%u -- unknown event", lmgrCb.scanMode, pMsg->hdr.event);
      /* No action required. */
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Master initiate message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstExtInitDisp(LctrExtScanMsg_t *pMsg)
{
  lctrExtScanCtx_t *pExtInitCtx;
  uint8_t event = pMsg->hdr.event;

  bool_t isBcstMsg = FALSE;

  if (pMsg->hdr.dispId == LCTR_DISP_BCST)
  {
    /* Global broadcast message. */
    isBcstMsg = TRUE;
  }

  /* Broadcast message. */
  switch (event)
  {
    case LCTR_EXT_INIT_MSG_INITIATE:
    case LCTR_EXT_INIT_MSG_INITIATE_CANCEL:
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
    if (lctrMstExtInit.enaPhys & (1 << pMsg->hdr.handle))
    {
      pExtInitCtx = &lctrMstExtInitTbl[pMsg->hdr.handle];
      lctrMstExtInitExecuteSm(pExtInitCtx, event);
    }
  }
  else
  {
    for (unsigned int i = 0; i < LCTR_SCAN_PHY_TOTAL; i++)
    {
      if (lctrMstExtInit.enaPhys & (1 << i))
      {
        pExtInitCtx = &lctrMstExtInitTbl[i];
        lctrMstExtInitExecuteSm(pExtInitCtx, event);
      }
    }
  }

  lctrMstExtInitExecuteCommonSm(pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Build extended initiate operation.
 *
 *  \param  pExtInitCtx     Extended initiate context.
 *  \param  pConnSpec       Connection spec.
 *  \param  peerAddrType    Peer address type.
 *  \param  peerAddr        Peer address.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t lctrMstExtInitiateBuildOp(lctrExtScanCtx_t *pExtInitCtx, LlConnSpec_t *pConnSpec, uint64_t peerAddr, uint8_t peerAddrType)
{
  BbOpDesc_t * const pOp = &pExtInitCtx->scanBod;
  BbBleData_t * const pBle = &pExtInitCtx->scanBleData;
  BbBleMstAdvEvent_t * const pScan = &pBle->op.mstAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_MOVEABLE;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrMstExtInitiateEndOp;
  pOp->abortCback = lctrMstExtInitiateEndOp;
  pOp->pCtx = pExtInitCtx;

  /*** BLE General Setup ***/

  pBle->chan.opType = BB_BLE_OP_MST_ADV_EVENT;

  pBle->chan.chanIdx = lctrScanChanSelectInit(lmgrMstScanCb.scanChanMap);
  pBle->chan.txPower = lmgrCb.advTxPwr;
  pBle->chan.accAddr = LL_ADV_ACCESS_ADDR;
  pBle->chan.crcInit = LL_ADV_CRC_INIT;
  switch (LCTR_GET_EXT_INIT_HANDLE(pExtInitCtx))
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
                              (1 << LL_PDU_ADV_EXT_IND);
  if (pExtInitCtx->data.init.filtPolicy & LL_SCAN_FILTER_WL_BIT)
  {
    pBle->pduFilt.wlPduTypeFilt = pBle->pduFilt.pduTypeFilt;
  }

  /*** BLE Scan Setup: Rx advertising packet ***/

  pScan->scanChMap = lmgrMstScanCb.scanChanMap;

  pScan->rxAdvCback = lctrMstInitiateRxExtAdvPktHandler;
  pScan->rxAdvPostCback = lctrMstInitiateRxExtAdvPktPostProcessHandler;

  if ((pScan->pRxAdvBuf = WsfMsgAlloc(LL_ADV_HDR_LEN + LL_EXT_ADV_HDR_MAX_LEN)) == NULL)
  {
    LL_TRACE_ERR0("Could not allocate advertising buffer");
    return LL_ERROR_CODE_UNSPECIFIED_ERROR;
  }

  /*** BLE Scan Setup: Tx connect indication packet ***/

  pScan->txReqCback = lctrMstExtConnIndTxCompHandler;

  /* TODO move to common/shared init */
  lctrConnInd_t * const pConnInd = &pExtInitCtx->data.init.connInd;

  pConnInd->accessAddr = lctrComputeAccessAddr();
  pConnInd->crcInit = lctrComputeCrcInit();
  pConnInd->txWinSize = LL_MIN_TX_WIN_SIZE;             /* minimum size */
  /* pConnInd->txWinOffset = 0; */                      /* Updated in ISR immediately prior to Tx LL_CONN_IND. */
  pConnInd->interval = pExtInitCtx->data.init.connInterval;
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

  pExtInitCtx->reqPduHdr.pduType = LL_PDU_CONNECT_IND;
  pExtInitCtx->reqPduHdr.len = LL_CONN_IND_PDU_LEN;

  /* Always match local address in PDU to initiator's address (in directed advertisements). */
  if (pExtInitCtx->data.init.ownAddrType & LL_ADDR_RANDOM_BIT)
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
  pExtInitCtx->reqPduHdr.txAddrRnd = BB_BLE_PDU_FILT_FLAG_IS_SET(&pBle->pduFilt, LOCAL_ADDR_MATCH_RAND);
  pConnInd->initAddr = pBle->pduFilt.localAddrMatch;

  /* peerAddrType and pPeerAddr only valid when filter policy is set to none */
  if (pExtInitCtx->data.init.filtPolicy == LL_SCAN_FILTER_NONE)
  {
    /* Set advertiser's address. */
    pExtInitCtx->reqPduHdr.rxAddrRnd = peerAddrType & LL_ADDR_RANDOM_BIT;
    pConnInd->advAddr = peerAddr;

    pBle->pduFilt.peerAddrMatch = peerAddr;
    if (peerAddrType & LL_ADDR_RANDOM_BIT)
    {
      BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_MATCH_RAND);
    }
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_MATCH_ENA);
  }

  uint8_t *pBuf;
  pBuf  = pExtInitCtx->reqBuf;
  pBuf += lctrPackAdvbPduHdr(pBuf, &pExtInitCtx->reqPduHdr);
  /* pBuf += */ lctrPackConnIndPdu(pBuf, pConnInd);

  pScan->pTxReqBuf = pExtInitCtx->reqBuf;
  pScan->txReqLen = LL_ADV_HDR_LEN + LL_CONN_IND_PDU_LEN;

  /*** Commit operation ***/

  /* Postponed in lctrMstExtInitiateOpCommit() until after connBod is built. */

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Commit initiate connection operation.
 *
 *  \param  pExtInitCtx     Extended scan context of the initiator.
 */
/*************************************************************************************************/
void lctrMstExtInitiateOpCommit(lctrExtScanCtx_t *pExtInitCtx)
{
  BbOpDesc_t * const pOp = &pExtInitCtx->scanBod;

  /*** Commit operation ***/

  /* Postponed in lctrMstInitiateOpCommit() until after connBod is built. */

  pOp->minDurUsec = LCTR_MIN_SCAN_USEC;
  pOp->maxDurUsec = LCTR_BLE_TO_US(pExtInitCtx->data.init.param.scanWindow);

  pExtInitCtx->selfTerm = FALSE;
  pExtInitCtx->shutdown = FALSE;

  SchInsertNextAvailable(pOp);
  pExtInitCtx->data.init.scanWinStartUsec = pOp->dueUsec;

  LL_TRACE_INFO1("    >>> ExtInitiate started, dueUsec=%u <<<", pOp->dueUsec);
  LL_TRACE_INFO1("                             pBod=0x%08x", pOp);
}

/*************************************************************************************************/
/*!
 *  \brief  Build auxiliary scan discovery operation.
 *
 *  \param  pExtInitCtx     Extended scan context of the initiator.
 *  \param  pConnSpec       Connection spec.
 *  \param  peerAddrType    Peer address type.
 *  \param  peerAddr        Peer address.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t lctrMstAuxInitiateBuildOp(lctrExtScanCtx_t *pExtInitCtx, LlConnSpec_t *pConnSpec, uint64_t peerAddr, uint8_t peerAddrType)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &pExtInitCtx->auxScanBod;
  BbBleData_t * const pBle = &pExtInitCtx->auxBleData;
  BbBleMstAuxAdvEvent_t * const pAuxScan = &pBle->op.mstAuxAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_MOVEABLE_PREFERRED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrMstAuxInitiateEndOp;
  pOp->abortCback = lctrMstAuxInitiateEndOp;
  pOp->pCtx = pExtInitCtx;

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
                              (1 << LL_PDU_AUX_CONNECT_RSP);
  if (pExtInitCtx->data.init.filtPolicy & LL_SCAN_FILTER_WL_BIT)
  {
    pBle->pduFilt.wlPduTypeFilt = pBle->pduFilt.pduTypeFilt;
  }

  /*** BLE Scan Setup: Rx packets ***/
  pAuxScan->isInit = TRUE;
  pAuxScan->rxAuxAdvCback = lctrMstInitiateRxAuxAdvPktHandler;
  pAuxScan->rxAuxAdvPostCback = NULL;

  /*** BLE Scan Setup: Tx connect request packet ***/

  /* Always match local address in PDU to initiator's address (in directed advertisements). */
  if (pExtInitCtx->data.init.ownAddrType & LL_ADDR_RANDOM_BIT)
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

  /* peerAddrType and pPeerAddr only valid when filter policy is set to none */
  if (pExtInitCtx->data.init.filtPolicy == LL_SCAN_FILTER_NONE)
  {
    pBle->pduFilt.peerAddrMatch = peerAddr;
    if (peerAddrType & LL_ADDR_RANDOM_BIT)
    {
      BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_MATCH_RAND);
    }
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_MATCH_ENA);
  }

  /* Use primary channel's CONNECT_REQ PDU. */
  pAuxScan->pTxAuxReqBuf = pExtInitCtx->reqBuf;
  pAuxScan->txAuxReqLen = LL_ADV_HDR_LEN + LL_CONN_IND_PDU_LEN;

#if (LL_ENABLE_TESTER)
  if (llTesterCb.auxReq.len)
  {
    memcpy(pAuxScan->pTxAuxReqBuf, llTesterCb.auxReq.buf, llTesterCb.auxReq.len);
    pAuxScan->txAuxReqLen = llTesterCb.auxReq.len;
  }
#endif

  /*** BLE Scan Setup: Rx auxiliary connection response packet ***/

  pAuxScan->rxAuxRspCback = lctrMstInitiateRxAuxConnRspHandler;

  /*** BLE Scan Setup: Rx chain packet ***/

  /* pAuxScan->rxAuxChainCback = NULL; */    /* No CHAIN_IND when inititating. */

  /*** Commit operation ***/

  /* pOp->minDurUsec = 0; */  /* Defer assignment until AuxPtr is received. */
  /* pOp->maxDurUsec = 0; */  /* Not used for aux scan. */

  pExtInitCtx->selfTerm = FALSE;
  pExtInitCtx->shutdown = FALSE;
  pExtInitCtx->auxOpPending = FALSE;

  /* Defer scheduling until AuxPtr is received. */

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for initiating master.
 */
/*************************************************************************************************/
void LctrMstExtInitInit(void)
{
  /* Add initiate reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_INIT] = lctrMstExtInitResetHandler;

  /* Add initiate message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_EXT_INIT] = (LctrMsgDisp_t)lctrMstExtInitDisp;


  /* Add utility function pointers. */
  LctrMstExtInitEnabled = LctrMstExtInitIsEnabled;

  LctrMstExtInitDefaults();

  lmgrPersistCb.extInitCtxSize = sizeof(lctrExtScanCtx_t);
}

/*************************************************************************************************/
/*!
 *  \brief      Set default values for scanning master.
 */
/*************************************************************************************************/
void LctrMstExtInitDefaults(void)
{
  memset(&lctrMstExtInitTbl, 0, sizeof(lctrMstExtInitTbl));
  memset(&lctrMstExtInit, 0, sizeof(lctrMstExtInit));

  lmgrCb.numExtScanPhys = 1;
  lctrMstExtInit.enaPhys = 1 << LCTR_SCAN_PHY_1M;
}

/*************************************************************************************************/
/*!
 *  \brief      Set enabled initiate scanning PHY.
 *
 *  \param      scanPhy     Enabled scanning PHY.
 */
/*************************************************************************************************/
void LctrMstExtInitSetScanPhy(uint8_t scanPhy)
{
  WSF_ASSERT(scanPhy < LCTR_SCAN_PHY_TOTAL);

  lctrMstExtInit.enaPhys |= 1 << scanPhy;
}

/*************************************************************************************************/
/*!
 *  \brief      Clear (disable) scanning PHY.
 *
 *  \param      scanPhy     Disabled scanning PHY.
 */
/*************************************************************************************************/
void LctrMstExtInitClearScanPhy(uint8_t scanPhy)
{
  WSF_ASSERT(scanPhy < LCTR_SCAN_PHY_TOTAL);

  lctrMstExtInit.enaPhys &= ~(1 << scanPhy);
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended initiate parameters.
 *
 *  \param      initPhy     Extended initiating PHY.
 *  \param      pScanParam  Extended initiating scan parameters.
 *  \param      pConnSpec   Connection specification.
 */
/*************************************************************************************************/
void LctrMstExtInitParam(uint8_t initPhy, const LlExtInitScanParam_t *pScanParam, const LlConnSpec_t *pConnSpec)
{
  WSF_ASSERT(initPhy < LCTR_SCAN_PHY_TOTAL);

  if (pScanParam)
  {
    lctrMstExtInitTbl[initPhy].data.init.param = *pScanParam;
  }
  lctrMstExtInitTbl[initPhy].data.init.connSpec = *pConnSpec;
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal extended initiate subsystem message.
 *
 *  \param  pExtInitCtx Extended initiate context.
 *  \param  event       Extended scan event.
 */
/*************************************************************************************************/
void lctrSendExtInitMsg(lctrExtScanCtx_t *pExtInitCtx, uint8_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t))) != NULL)
  {
    pMsg->handle = (pExtInitCtx) ? LCTR_GET_EXT_INIT_HANDLE(pExtInitCtx) : LCTR_SCAN_PHY_ALL;
    pMsg->dispId = LCTR_DISP_EXT_INIT;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check if scanner is enabled or not.
 *
 *  \param      scanPhy   scanner Phy.
 *
 *  \return     True if scanner enabled. False if not.
 */
/*************************************************************************************************/
bool_t LctrMstExtInitIsEnabled(uint8_t scanPhy)
{
  return (lctrMstExtInitTbl[scanPhy].state != LCTR_EXT_SCAN_STATE_DISABLED);
}

/*************************************************************************************************/
/*!
 *  \brief      Check if private addresses are being used.
 *
 *  \param      scanPhy   scanner Phy.
 *
 *  \return     Returns True if scanner is using private addresses. False if not.
 */
/*************************************************************************************************/
bool_t LctrMstExtInitIsPrivAddr(uint8_t scanPhy)
{
  /* Check if private address bit is enabled. */
  return (lctrMstExtInitTbl[scanPhy].scanParam.ownAddrType & LL_ADDR_RANDOM_BIT);
}
