/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave extended advertising ISR callbacks.
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

#include "lctr_int_adv_slave_ae.h"
#include "lctr_int_adv_slave.h"
#include "lctr_pdu_adv_ae.h"
#include "lctr_pdu_adv.h"
#include "lctr_int.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "bb_api.h"
#include "bb_ble_api_reslist.h"
#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_msg.h"
#include "wsf_math.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Advertising header (lifetime single ISR context). */
static lctrAdvbPduHdr_t lctrAdvIsrAdvHdr;

/*! \brief      Scan request PDU (lifetime single ISR context). */
static lctrScanReq_t lctrAdvIsrScanReq;

/**************************************************************************************************
  Functions: Packet handlers
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Transmit setup complete for an extended advertising primary channel operation.
 *
 *  \param  pOp             Completed operation.
 *  \param  advTxTimeUsec   Start time of advertising packet in microseconds.
 */
/*************************************************************************************************/
void lctrSlvTxSetupExtAdvHandler(BbOpDesc_t *pOp, uint32_t advTxTimeUsec)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;

  if (pAdvSet->pExtAdvAuxPtr && pAdvSet->auxBodUsed)
  {
    uint32_t auxOffsetUsec = BbGetTargetTimeDelta(pAdvSet->auxAdvBod.dueUsec, advTxTimeUsec);

    /* Compute EXT_ADV_IND PDU OTA time. */
    uint32_t txAdvUsec;

    txAdvUsec = SchBleCalcAdvPktDurationUsec(pAdvSet->advBod.prot.pBle->chan.txPhy,
                                             pAdvSet->bleData.chan.initTxPhyOptions,
                                             pAdvSet->advBod.prot.pBle->op.slvAdv.txAdvLen);

    if (auxOffsetUsec < (txAdvUsec + LL_BLE_MAFS_US))
    {
      LL_TRACE_WARN1("AUX Offset does not meet T_MAFS, afsUsec=%u", (auxOffsetUsec - txAdvUsec));
    }

    /* The time pointed by auxPtr is earlier than aux bod due time by auxPtrOffsetUsec. */
    lctrPackAuxPtr(pAdvSet, auxOffsetUsec, pAdvSet->auxChIdx, pAdvSet->pExtAdvAuxPtr);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Transmit setup for an auxiliary advertising data.
 *
 *  \param  pOp         Setup operation.
 *  \param  isChainInd  Is packet a CHAIN_IND.
 *
 *  \return Next auxiliary offset in microseconds.
 */
/*************************************************************************************************/
uint32_t lctrSlvTxSetupAuxAdvDataHandler(BbOpDesc_t *pOp, bool_t isChainInd)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = &pAdvSet->auxBleData;
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBle->op.slvAuxAdv;

  if ((isChainInd == TRUE) && (pAdvSet->auxOffsUsec == 0))
  {
    /* Tell the caller(BB) there is no more chained packet. */
    return 0;
  }

  /* Store/use current AuxPtr values. */
  /* Due time of the aux packet is delayed by auxPtrOffsetUsec from the time specified by auxPtr. */
  uint32_t auxOffsUsec = pAdvSet->auxOffsUsec + pLctrRtCfg->auxPtrOffsetUsec;
  pBle->chan.chanIdx = pAdvSet->auxChIdx;

  if (!isChainInd)
  {
    /* Reset AdvData to initial fragment. */
    pAdvSet->advData.txOffs = 0;

    /* Compute new AuxPtr values. */
    lctrSelectNextAuxChannel(pAdvSet);
    pAuxAdv->txAuxAdvPdu[0].len = lctrPackAuxAdvIndPdu(pAdvSet, pAdvSet->auxAdvHdrBuf, &pAdvSet->advData, FALSE);

    /* Set initial fragment. */
    pAuxAdv->txAuxAdvPdu[1].pBuf = pAdvSet->advData.pBuf;
    pAuxAdv->txAuxAdvPdu[1].len = pAdvSet->advData.txOffs;
  }
  else
  {
    /* Store/use current AuxPtr values. */
    uint16_t advDataOffs = pAdvSet->advData.txOffs;

    /* Compute new AuxPtr values. */
    if (pAdvSet->advData.txOffs < pAdvSet->advData.len)
    {
      lctrSelectNextAuxChannel(pAdvSet);
    }
    pAuxAdv->txAuxChainPdu[0].len = lctrPackAuxChainIndPdu(pAdvSet, pAdvSet->auxAdvHdrBuf, &pAdvSet->advData, FALSE);

    /* Set next fragment. */
    pAuxAdv->txAuxChainPdu[1].pBuf = pAdvSet->advData.pBuf + advDataOffs;
    pAuxAdv->txAuxChainPdu[1].len = pAdvSet->advData.txOffs - advDataOffs;
  }

  return auxOffsUsec;
}

/*************************************************************************************************/
/*!
 *  \brief  Transmit setup for an periodic advertising data.
 *
 *  \param  pOp         Setup operation.
 *  \param  isChainInd  Is packet a CHAIN_IND.
 *
 *  \return Next periodic offset in microseconds.
 */
/*************************************************************************************************/
uint32_t lctrSlvTxSetupPeriodicAdvDataHandler(BbOpDesc_t *pOp, bool_t isChainInd)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = &pAdvSet->perParam.perBleData;
  BbBleSlvAuxAdvEvent_t * const pPerAdv = &pBle->op.slvPerAdv;

  /* Store/use current AuxPtr values. */
  uint32_t perOffsUsec = pAdvSet->perParam.perOffsUsec;

  pBle->chan.chanIdx = pAdvSet->perParam.perChIdx;

  if (!isChainInd)
  {
    /* Reset AdvData to initial fragment. */
    pAdvSet->perAdvData.txOffs = 0;

    /* Compute new chanIdx for next chain packet. */
    lctrSelectNextPerChannel(pAdvSet);

    /* Update BIG Info timing values. */
    LctrAcadBigInfo_t * const pBigInfo = &pAdvSet->acadParams[LCTR_ACAD_ID_BIG_INFO].bigInfo;
    if (pBigInfo->hdr.state == LCTR_ACAD_STATE_ENABLED)
    {
      uint32_t bigOffsUsec = pBigInfo->bigAnchorPoint - pOp->dueUsec;

      if (bigOffsUsec < 600)
      {
        bigOffsUsec += pBigInfo->isoInter * 1250;
        pBigInfo->bisPldCtr += pBigInfo->nse;
      }

      if (bigOffsUsec < 491460)
      {
        /* use 30us units */
        pBigInfo->bigOffsUnits = 0;
        pBigInfo->bigOffs = LL_MATH_DIV_30(bigOffsUsec);
      }
      else
      {
        /* use 300us units */
        pBigInfo->bigOffsUnits = 1;
        pBigInfo->bigOffs = LL_MATH_DIV_300(bigOffsUsec);
      }
    }

    pPerAdv->txAuxAdvPdu[0].len = lctrPackSyncIndPdu(pAdvSet, pAdvSet->perAdvHdrBuf, &pAdvSet->perAdvData, TRUE);

    /* Set initial fragment. */
    pPerAdv->txAuxAdvPdu[1].pBuf = pAdvSet->perAdvData.pBuf;
    pPerAdv->txAuxAdvPdu[1].len = pAdvSet->perAdvData.txOffs;
  }
  else
  {
    /* Store/use current AuxPtr values. */
    uint16_t advDataOffs = pAdvSet->perAdvData.txOffs;

    /* Compute new chanIdx for next chain packet if there is any. */
    if (pAdvSet->perAdvData.txOffs < pAdvSet->perAdvData.len)
    {
      lctrSelectNextPerChannel(pAdvSet);
    }

    pPerAdv->txAuxChainPdu[0].len = lctrPackAuxChainIndPdu(pAdvSet, pAdvSet->perAdvHdrBuf, &pAdvSet->perAdvData, TRUE);

    /* Set next fragment. */
    pPerAdv->txAuxChainPdu[1].pBuf = pAdvSet->perAdvData.pBuf + advDataOffs;
    pPerAdv->txAuxChainPdu[1].len = pAdvSet->perAdvData.txOffs - advDataOffs;
  }

  return perOffsUsec;
}

/*************************************************************************************************/
/*!
 *  \brief  Transmit setup for an auxiliary scan response data.
 *
 *  \param  pOp         Setup operation.
 *  \param  isChainInd  Packet is a chain indication.
 *
 *  \return Next auxiliary offset in microseconds.
 */
/*************************************************************************************************/
uint32_t lctrSlvTxSetupAuxScanRspDataHandler(BbOpDesc_t *pOp, bool_t isChainInd)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = &pAdvSet->auxBleData;
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBle->op.slvAuxAdv;

  if ((isChainInd == TRUE) && (pAdvSet->auxOffsUsec == 0))
  {
    /* Tell the caller(BB) there is no more chained packet. */
    return 0;
  }

  /* Store/use current AuxPtr values. */
  /* Due time of the aux packet is delayed by auxPtrOffsetUsec from the time specified by auxPtr. */
  uint32_t auxOffsUsec = pAdvSet->auxOffsUsec + pLctrRtCfg->auxPtrOffsetUsec;
  pBle->chan.chanIdx = pAdvSet->auxChIdx;

  if (!isChainInd)
  {
    /* Reset ScanRspData to initial fragment. */
    pAdvSet->scanRspData.txOffs = 0;

    /* Compute new AuxPtr values. */
    lctrSelectNextAuxChannel(pAdvSet);
    pAuxAdv->txAuxRspPdu[0].len = lctrPackAuxScanRspPdu(pAdvSet, pAdvSet->auxRspHdrBuf, FALSE);

    /* Set initial fragment. */
    pAuxAdv->txAuxRspPdu[1].pBuf = pAdvSet->scanRspData.pBuf;
    pAuxAdv->txAuxRspPdu[1].len = pAdvSet->scanRspData.txOffs;
  }
  else
  {
    /* Store/use current AuxPtr values. */
    uint16_t scanRspDataOffs = pAdvSet->scanRspData.txOffs;

    /* Compute new AuxPtr values. */
    if (pAdvSet->scanRspData.txOffs < pAdvSet->scanRspData.len)
    {
      lctrSelectNextAuxChannel(pAdvSet);
    }
    pAuxAdv->txAuxChainPdu[0].len = lctrPackAuxChainIndPdu(pAdvSet, pAdvSet->auxRspHdrBuf, &pAdvSet->scanRspData, FALSE);

    /* Set next fragment. */
    pAuxAdv->txAuxChainPdu[1].pBuf = pAdvSet->scanRspData.pBuf + scanRspDataOffs;
    pAuxAdv->txAuxChainPdu[1].len = pAdvSet->scanRspData.txOffs - scanRspDataOffs;
  }

  return auxOffsUsec;
}

/*************************************************************************************************/
/*!
 *  \brief      Slave auxiliary scan request packet receive completion handler.
 *
 *  \param      pOp     Originating operation.
 *  \param      pReqBuf Received request buffer.
 *
 *  \return     TRUE if response transmission is required, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrSlvRxAuxScanReqHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf)
{
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBle->op.slvAuxAdv;

  lctrUnpackAdvbPduHdr(&lctrAdvIsrAdvHdr, pReqBuf);

  /* PDU type and length match. */
  if ((lctrAdvIsrAdvHdr.pduType != LL_PDU_AUX_SCAN_REQ) ||  /* LL_PDU_AUX_SCAN_REQ has the same value as LL_PDU_SCAN_REQ */
      (lctrAdvIsrAdvHdr.len != LL_SCAN_REQ_PDU_LEN))
  {
    return FALSE;
  }

  lctrUnpackScanReqPdu(&lctrAdvIsrScanReq, pReqBuf + LL_ADV_HDR_LEN);

  /*** Extended advertising event filtering. ***/
  bbBlePduExtFiltParams_t params;

  memset(&params, 0, sizeof(bbBlePduExtFiltParams_t));
  params.pduType = lctrAdvIsrAdvHdr.pduType;
  params.extHdrFlags |= (LL_EXT_HDR_ADV_ADDR_BIT | LL_EXT_HDR_TGT_ADDR_BIT);    /* ScanA and AdvA are mandatory. */
  params.peerAddr = lctrAdvIsrScanReq.scanAddr;
  params.peerAddrRand = lctrAdvIsrAdvHdr.txAddrRnd;
  params.localAddr = lctrAdvIsrScanReq.advAddr;
  params.localAddrRand = lctrAdvIsrAdvHdr.rxAddrRnd;

  if (BbBleExtPduFiltCheck(&params, &pOp->prot.pBle->pduFilt, FALSE, &pAuxAdv->filtResults) == FALSE)
  {
    return FALSE;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Slave auxiliary advertising packet receive post process handler.
 *
 *  \param      pOp     Originating operation.
 *  \param      pReqBuf Received request buffer.
 */
/*************************************************************************************************/
void lctrSlvRxAuxScanReqPostProcessHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBle->op.slvAuxAdv;

  if (pAdvSet->param.scanReqNotifEna)
  {
    uint64_t peerIdAddr = 0;
    uint8_t peerIdAddrType = 0;

    BbBlePduFiltResultsGetPeerIdAddr(&pAuxAdv->filtResults, &peerIdAddr, &peerIdAddrType);

    /* TODO Offload report to task context. */
    LmgrSendScanReqReceivedInd(pAdvSet->handle,
                               peerIdAddrType,
                               peerIdAddr);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Slave auxiliary advertising packet receive post process handler for legacy scan request.
 *
 *  \param      pOp     Originating operation.
 *  \param      pReqBuf Received request buffer.
 */
/*************************************************************************************************/
void lctrSlvRxLegacyScanReqPostProcessHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleSlvAdvEvent_t * const pAdv = &pBle->op.slvAdv;

  if (pAdvSet->param.scanReqNotifEna)
  {
    uint64_t peerIdAddr = 0;
    uint8_t peerIdAddrType = 0;

    BbBlePduFiltResultsGetPeerIdAddr(&pAdv->filtResults, &peerIdAddr, &peerIdAddrType);

    /* TODO Offload report to task context. */
    LmgrSendScanReqReceivedInd(pAdvSet->handle,
                               peerIdAddrType,
                               peerIdAddr);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Slave auxiliary connect request packet receive completion handler.
 *
 *  \param      pOp     Originating operation.
 *  \param      pReqBuf Received request buffer.
 *
 *  \return     TRUE if response transmission is required, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrSlvRxAuxConnReqHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = &pAdvSet->auxBleData;
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBle->op.slvAuxAdv;

  bool_t sendRsp = FALSE;
  const uint8_t *pBuf;
  lctrConnInd_t connInd;

  pBuf = pReqBuf;
  pBuf += lctrUnpackAdvbPduHdr(&lctrAdvIsrAdvHdr, pBuf);
  lctrUnpackConnIndPdu(&connInd, pBuf);

  if ((lctrAdvIsrAdvHdr.pduType == LL_PDU_AUX_CONNECT_REQ) &&
      (lctrMsgDispTbl[LCTR_DISP_CONN]) &&
      (lctrAdvIsrAdvHdr.len == LL_CONN_IND_PDU_LEN) &&
      (lctrValidateConnIndPdu(&connInd)))
  {

    /*** Extended advertising event filtering. ***/
    bbBlePduExtFiltParams_t params;

    memset(&params, 0, sizeof(bbBlePduExtFiltParams_t));
    params.pduType = lctrAdvIsrAdvHdr.pduType;
    params.extHdrFlags |= (LL_EXT_HDR_ADV_ADDR_BIT | LL_EXT_HDR_TGT_ADDR_BIT);    /* ScanA and AdvA are mandatory. */
    params.peerAddr = connInd.initAddr;
    params.peerAddrRand = lctrAdvIsrAdvHdr.txAddrRnd;
    params.localAddr = connInd.advAddr;
    params.localAddrRand = lctrAdvIsrAdvHdr.rxAddrRnd;

    if (BbBleExtPduFiltCheck(&params, &pOp->prot.pBle->pduFilt, FALSE, &pAuxAdv->filtResults) == FALSE)
    {
      return sendRsp;
    }

    /* Update auxiliary connection response header's target address. */
    uint8_t *pAuxConnRspTgtA = pAdvSet->auxRspHdrBuf + LL_ADV_HDR_LEN + LCTR_EXT_HDR_CMN_LEN + LCTR_EXT_HDR_FLAG_LEN + BDA_ADDR_LEN;
    Bda64ToBstream(pAuxConnRspTgtA, pAuxAdv->filtResults.peerAddr);
    pAdvSet->rspPduHdr.rxAddrRnd = pAuxAdv->filtResults.peerAddrRand;

    /* Update auxiliary connection response header's advertiser address. */
    bool_t  localAddrRand;
    uint64_t localAddr;

    localAddrRand = lctrAdvIsrAdvHdr.rxAddrRnd;
    localAddr     = BstreamToBda64(pReqBuf + LL_ADV_HDR_LEN + BDA_ADDR_LEN);

    if (pAdvSet->param.ownAddrType & LL_ADDR_IDENTITY_BIT)
    {
      /* Update the local RPA if the received one is RPA. */
      if (BDA64_ADDR_IS_RPA(localAddr))
      {
        lmgrSlvAdvCb.localRpa = localAddr;
        BbBleResListUpdateLocal(pAuxAdv->filtResults.peerIdAddrRand, pAuxAdv->filtResults.peerIdAddr, &localAddr);
      }
    }
    else
    {
      lmgrSlvAdvCb.localRpa  = 0;
    }
    uint8_t *pAuxConnRspAdvA = pAdvSet->auxRspHdrBuf + LL_ADV_HDR_LEN + LCTR_EXT_HDR_CMN_LEN + LCTR_EXT_HDR_FLAG_LEN;
    Bda64ToBstream(pAuxConnRspAdvA, localAddr);
    pAdvSet->rspPduHdr.txAddrRnd = localAddrRand;

#if (LL_ENABLE_TESTER)
    if (llTesterCb.auxRsp.len)
    {
      /* Overriding AUX_CONNECT_RSP from test script. */
      memcpy(pAuxConnRspAdvA, llTesterCb.auxRsp.buf + LL_ADV_HDR_LEN, llTesterCb.auxRsp.len - LL_ADV_HDR_LEN);  /* Exclude LL_ADV_HDR_LEN */

      uint16_t hdr;
      uint8_t  *ptr = llTesterCb.auxRsp.buf;
      BSTREAM_TO_UINT16(hdr, ptr);

      pAdvSet->rspPduHdr.txAddrRnd   = (hdr >> LCTR_ADV_HDR_TX_ADD_SHIFT)   & 0x0001;
      pAdvSet->rspPduHdr.rxAddrRnd   = (hdr >> LCTR_ADV_HDR_RX_ADD_SHIFT)   & 0x0001;
    }
#endif

    lctrPackAdvbPduHdr(pAdvSet->auxRspHdrBuf, &pAdvSet->rspPduHdr);

    /* Stop advertising. */
    pAdvSet->connIndRcvd = TRUE;

    pAdvSet->isAuxConnReq = TRUE;

    /* CONN_REQ packet delivered when advertising termination completes.
     * cf. lctrExtAdvActSelfTerm() */

    /*** Received advertising PDU post-processing. ***/

    pAdvSet->usedChSel = LL_CH_SEL_2;       /* LL_PDU_AUX_CONNECT_REQ always uses Channel Selection #2. */
    pAdvSet->connIndEndTsUsec = pAuxAdv->auxReqStartTsUsec +
                            SchBleCalcAdvPktDurationUsec(pBle->chan.rxPhy, pAuxAdv->auxRxPhyOptions,
                                                                           LL_ADV_HDR_LEN + LL_CONN_IND_PDU_LEN);

    sendRsp = TRUE;
  }

  return sendRsp;
}

/*************************************************************************************************/
/*!
 *  \brief      Slave legacy advertising packet receive completion handler.
 *
 *  \param      pOp     Originating operation.
 *  \param      pReqBuf Received request buffer.
 *
 *  \return     TRUE if response transmission is required, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrSlvRxLegacyReqHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleSlvAdvEvent_t * const pAdv = &pBle->op.slvAdv;

  bool_t sendRsp = FALSE;

  /* Unwanted PDUs must have been filtered in the BB. */
  uint8_t pduType = pAdv->filtResults.pduType;
  switch (pduType)
  {
    case LL_PDU_SCAN_REQ:
      /* Require that the peer address matched. */
      if (pAdv->filtResults.peerMatch)
      {
        sendRsp = lctrScanReqHandler(pOp, pAdv->filtResults.pduLen);
      }
      break;
    case LL_PDU_CONNECT_IND:
      lctrUnpackAdvbPduHdr(&lctrAdvIsrAdvHdr, pReqBuf);

      if ((lctrAdvIsrAdvHdr.pduType == LL_PDU_CONNECT_IND) &&
          (lctrMsgDispTbl[LCTR_DISP_CONN]) &&
          (lctrAdvIsrAdvHdr.len == LL_CONN_IND_PDU_LEN))
      {
        /* Stop advertising. */
        pAdvSet->connIndRcvd = TRUE;
        pAdvSet->isAuxConnReq = FALSE;
        BbSetBodTerminateFlag();
      }
      break;
    default:
      break;
  }

  return sendRsp;
}

/*************************************************************************************************/
/*!
 *  \brief      Slave advertising packet receive post process handler.
 *
 *  \param      pOp     Originating operation.
 *  \param      pReqBuf Received request buffer.
 */
/*************************************************************************************************/
void lctrSlvRxLegacyReqPostProcessHandler(BbOpDesc_t *pOp, const uint8_t *pReqBuf)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleSlvAdvEvent_t * const pAdv = &pBle->op.slvAdv;

  /* Unwanted PDUs must have been filtered in the BB. */
  switch (pAdv->filtResults.pduType)
  {
    case LL_PDU_SCAN_REQ:
      /* Require that the peer address matched. */
      if (pAdv->filtResults.peerMatch)
      {
        lctrSlvRxLegacyScanReqPostProcessHandler(pOp, pReqBuf);
      }
      break;
    case LL_PDU_CONNECT_IND:
      if ((pAdvSet->advHdrBuf[0] & (1 << LCTR_ADV_HDR_CH_SEL_SHIFT)) &&   /* Local advertiser supports CS#2. */
          lctrAdvIsrAdvHdr.chSel)                                         /* Peer initiator supports CS#2. */
      {
        pAdvSet->usedChSel = LL_CH_SEL_2;
      }
      else
      {
        pAdvSet->usedChSel = LL_CH_SEL_1;
      }

      pAdvSet->connIndEndTsUsec = pAdv->reqStartTsUsec +
                                  LCTR_CONN_IND_PKT_1M_US;
      break;
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      ACAD handler for post op.
 *
 *  \param      pAdvSet     Advertising Set.
 */
/*************************************************************************************************/
void lctrSlvAcadHandler(lctrAdvSet_t *pAdvSet)
{
  for (unsigned int acadId = 0; acadId < LCTR_ACAD_NUM_ID; acadId++)
  {
    lctrAcadParam_t *pAcad = &pAdvSet->acadParams[acadId];

    if (pAcad->hdr.state != LCTR_ACAD_STATE_ENABLED)
    {
      continue;
    }

    /* coverity[dead_error_condition] */
    switch (acadId)
    {
      case LCTR_ACAD_ID_CHAN_MAP_UPDATE:
      {
        if (pAcad->chanMapUpdate.instant == pAdvSet->perParam.perEventCounter)
        {
          /* Update the channel map. */
          pAdvSet->perParam.perChanParam.chanMask = pAcad->chanMapUpdate.chanMask;
          LmgrBuildRemapTable(&pAdvSet->perParam.perChanParam);

          /* Disable the ACAD. */
          lctrSlvAcadExecuteSm(pAdvSet, LCTR_ACAD_MSG_CHAN_UPDATE_FINISH);

          /* Update Channel Map in BIG Info. */
          pAcad->bigInfo.chanMap = pAdvSet->perParam.perChanParam.chanMask;

          /* Check for updated Channel Map. */
          if (pAdvSet->perParam.perChanParam.chanMask != pAdvSet->perParam.updChanMask)
          {
            lctrSlvAcadExecuteSm(pAdvSet, LCTR_ACAD_MSG_CHAN_UPDATE);
          }
        }
        break;
      }

      case LCTR_ACAD_ID_BIG_INFO:
      {
        /* Updated in lctrSlvAcadHandler(), upon Channel Map changes. */
        /* pAcad->bigInfo.chanMap = pAdvSet->perParam.perChanParam.chanMask; */

        /* Updated in lctrSlvBigEndOp(), upon timing changes. */
        /* pAcad->bigInfo.bigAnchorPoint = pOp->dueUsec; */
        /* pAcad->bigInfo.bisPldCtr = pBigCtx->eventCounter; */

        break;
      }

      default:
        break;
    }
  }
}

/**************************************************************************************************
  Functions: BOD handlers
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  End an extended advertising primary channel operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrSlvExtAdvEndOp(BbOpDesc_t *pOp)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleSlvAdvEvent_t * const pAdv = &pBle->op.slvAdv;

  WSF_ASSERT(pOp->protId == BB_PROT_BLE);
  WSF_ASSERT(pOp->prot.pBle->chan.opType == BB_BLE_OP_SLV_ADV_EVENT);

  do
  {
    if (pAdvSet->shutdown || pAdvSet->connIndRcvd)
    {
    }
    else if (!pAdvSet->auxBodUsed && pAdvSet->maxEvents &&
             (++pAdvSet->numEvents >= pAdvSet->maxEvents))
    {
      pAdvSet->termReason = LL_ERROR_CODE_LIMIT_REACHED;
    }
    else
    {
      /* Continue operation. */
      break;
    }

    /* Disable (do not reschedule) advertising PDU. */
    pAdvSet->shutdown = TRUE;

    if (pAdvSet->auxBodUsed)
    {
      pAdvSet->auxBodUsed = FALSE;
      /* Ensure all BODs are de-scheduled. */
      SchRemove(&pAdvSet->auxAdvBod);
    }

    /* Last BOD to terminate; send terminate event. */
    lctrSendAdvSetMsg(pAdvSet, LCTR_EXT_ADV_MSG_TERMINATE);

    return;

  } while (FALSE);

  if (pBle->chan.tifsTxPhyOptions != BB_PHY_OPTIONS_DEFAULT)
  {
    /* Set PHY options to host defined behavior for ADV_EXT_IND. */
    pBle->chan.initTxPhyOptions = pBle->chan.tifsTxPhyOptions;
  }
  else
  {
    /* Set PHY options to default behavior for ADV_EXT_IND. */
    pBle->chan.initTxPhyOptions = lmgrSlvAdvCb.defTxPhyOpts;
  }

  /*** Update advertising data ***/

  if ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) == 0)
  {
    bool_t modified = pAdvSet->advData.alt.ext.modified;

    /* Update ADV set before building the PDU. */
    if (pAdvSet->advData.alt.ext.modified)
    {
      pAdvSet->advData.alt.ext.modified = FALSE;
      memcpy(pAdvSet->advData.pBuf, pAdvSet->advData.alt.ext.buf, pAdvSet->advData.alt.ext.len);
      pAdvSet->advData.len = pAdvSet->advData.alt.ext.len;
      pAdvSet->param.advDID = pAdvSet->advData.alt.ext.did;
      pAdvSet->advData.fragPref = pAdvSet->advData.alt.ext.fragPref;
    }

    /* Update superior PDU including AdvA and TgtA. */
    pAdv->txAdvLen = lctrPackAdvExtIndPdu(pAdvSet, pAdvSet->advHdrBuf, FALSE);

    if (modified && !pAdvSet->auxBodUsed)
    {
      /* Advertising offloading to auxiliary channel. */
      if (pAdvSet->pExtAdvAuxPtr)
      {
        /* Enable auxiliary PDU. */
        lctrSlvAuxNonConnNonScanBuildOp(pAdvSet);
        pAdvSet->auxBodUsed = TRUE;
      }
    }

    /* Schedule auxiliary BOD if periodic advertising is enabled. */
    if ((pAdvSet->perParam.perAuxStart == TRUE) &&
        pAdvSet->perParam.perAdvEnabled &&
        pAdvSet->pExtAdvAuxPtr &&
        (pAdvSet->auxBodUsed == FALSE))
    {
      pAdvSet->perParam.perAuxStart = FALSE;
      lctrSlvAuxNonConnNonScanBuildOp(pAdvSet);
      pAdvSet->auxBodUsed = TRUE;
    }

    /* Update Data ID when periodic advertising is enabled or disabled. */
    if ((pAdvSet->didPerUpdate == TRUE) && (pAdvSet->auxBodUsed == FALSE))
    {
      pAdvSet->param.advDID = pAdvSet->advData.alt.ext.did;
      pAdvSet->didPerUpdate = FALSE;
    }
  }
  else /* (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) */
  {
    /* Update local private address. */
    bool_t   update = FALSE;
    uint64_t newAddr = 0;
    if (pAdvSet->bdAddrRndMod &&
        (lmgrSlvAdvCb.ownAddrType == LL_ADDR_RANDOM))
    {
      pAdvSet->bdAddrRndMod = FALSE;
      pAdvSet->advA = pAdvSet->bdAddrRnd;

      update  = TRUE;
      newAddr = pAdvSet->bdAddrRnd;
    }
    else if (lmgrSlvAdvCb.ownAddrType & LL_ADDR_IDENTITY_BIT)
    {
      uint64_t localRpa;
      if (BbBleResListReadLocal(pAdvSet->param.peerAddrType, pAdvSet->param.peerAddr, &localRpa))
      {
        update = TRUE;
        newAddr = localRpa;
        pAdvSet->advA = localRpa;
        lmgrSlvAdvCb.localRpa    = localRpa;
      }
    }

    if (update)
    {
      uint8_t *pBuf = pAdvSet->advHdrBuf + LL_ADV_HDR_LEN;

      BDA64_TO_BSTREAM(pBuf, newAddr);                    /* update adv PDU */
      memcpy(pAdvSet->scanRspHdrBuf + LL_ADV_HDR_LEN,     /* update scan response PDU */
             pAdvSet->advHdrBuf + LL_ADV_HDR_LEN,
             BDA_ADDR_LEN);
      pBle->pduFilt.localAddrMatch = newAddr;
    }

    /* Update peer address. */
    if ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_DIRECT_ADV_BIT) == LL_ADV_EVT_PROP_DIRECT_ADV_BIT)
    {
      if (BB_BLE_PDU_FILT_FLAG_IS_SET(&pBle->pduFilt, PEER_ADDR_MATCH_ENA) &&
          BB_BLE_PDU_FILT_FLAG_IS_SET(&pBle->pduFilt, PEER_ADDR_RES_ENA))
      {
        uint64_t peerRpa;

        if (BbBleResListReadPeer(pAdvSet->param.peerAddrType, pAdvSet->param.peerAddr, &peerRpa))
        {
          uint8_t *pBuf = pAdvSet->advHdrBuf + LL_ADV_HDR_LEN + BDA_ADDR_LEN;
          BDA64_TO_BSTREAM(pBuf, peerRpa);                /* update adv PDU */
          pAdvSet->tgtA    = peerRpa;
        }
      }
    }

    if (pAdvSet->advData.alt.legacy.modified)
    {
      pAdvSet->advData.alt.legacy.modified = FALSE;

      switch ((pAdv->pTxAdvBuf[0] >> LCTR_ADV_HDR_PDU_TYPE_SHIFT) & 0xF)
      {
        case LL_PDU_ADV_IND:
        case LL_PDU_ADV_NONCONN_IND:
        case LL_PDU_ADV_SCAN_IND:
          pAdv->txAdvLen = LL_ADVB_MIN_LEN + pAdvSet->advData.alt.legacy.len;
          pAdv->pTxAdvBuf[LL_ADV_HDR_LEN_OFFS] = LL_ADV_PREFIX_LEN + pAdvSet->advData.alt.legacy.len;
          memcpy(pAdv->pTxAdvBuf + LL_ADVB_MIN_LEN,
                 pAdvSet->advData.alt.legacy.buf,
                 pAdvSet->advData.alt.legacy.len);
          #if (LL_ENABLE_TESTER)
            if (pAdv->pTxAdvBuf == llTesterCb.txAdvPdu)
            {
              pAdv->txAdvLen = llTesterCb.txAdvPduLen;
            }
          #endif
          break;
        case LL_PDU_ADV_DIRECT_IND:
        default:
          break;
      }
    }

    if (pAdvSet->scanRspData.alt.legacy.modified)
    {
      pAdvSet->scanRspData.alt.legacy.modified = FALSE;
      pAdv->txRspLen = LL_ADVB_MIN_LEN + pAdvSet->scanRspData.alt.legacy.len;
      pAdv->pTxRspBuf[LL_ADV_HDR_LEN_OFFS] = LL_SCAN_PREFIX_LEN + pAdvSet->scanRspData.alt.legacy.len;
      memcpy(pAdv->pTxRspBuf + LL_ADVB_MIN_LEN,
             pAdvSet->scanRspData.alt.legacy.buf,
             pAdvSet->scanRspData.alt.legacy.len);
      #if (LL_ENABLE_TESTER)
        if (pAdv->pTxAdvBuf == llTesterCb.txAdvPdu)
        {
          pAdv->txRspLen = llTesterCb.txAdvPduLen;
        }
      #endif
    }
  }

  SchBleCalcAdvOpDuration(pOp, 0);

  /*** Reschedule operation ***/

  const uint8_t LEGACY_HIGH_DUTY = (LL_ADV_EVT_PROP_LEGACY_ADV_BIT | LL_ADV_EVT_PROP_HIGH_DUTY_ADV_BIT |
                                    LL_ADV_EVT_PROP_DIRECT_ADV_BIT | LL_ADV_EVT_PROP_CONN_ADV_BIT);

  if ((pAdvSet->param.advEventProp & LEGACY_HIGH_DUTY) == LEGACY_HIGH_DUTY)
  {
    {
      uint32_t advEventStartUsec = pOp->dueUsec;
      uint32_t advTermCntDown = pAdvSet->param.priAdvTermCntDownUsec;
      bool_t result = FALSE;

      while (!result && pAdvSet->param.priAdvTermCntDownUsec)
      {
        result = SchInsertLateAsPossible(pOp, pAdvSet->param.priAdvInterMinUsec, pAdvSet->param.priAdvInterMaxUsec);
        if (!result)
        {
          pOp->dueUsec += pAdvSet->param.priAdvInterMaxUsec;
        }

        uint32_t advEventDur = BbGetTargetTimeDelta(pOp->dueUsec, advEventStartUsec);

        if ((advEventDur + pAdvSet->param.priAdvInterMaxUsec) < advTermCntDown)
        {
          pAdvSet->param.priAdvTermCntDownUsec = advTermCntDown - advEventDur;
        }
        else
        {
          /* Terminate at end of next advertising event. */
          pAdvSet->param.priAdvTermCntDownUsec = 0;
        }
      }

      if (!result && !pAdvSet->param.priAdvTermCntDownUsec)
      {
        pAdvSet->termReason = LL_ERROR_CODE_ADV_TIMEOUT;
        lctrSendAdvSetMsg(pAdvSet, LCTR_EXT_ADV_MSG_TERMINATE);
      }
    }
  }
  else
  {
    uint32_t totalDuration = pOp->minDurUsec;
    uint32_t prefIntervalUsec;

    if (pAdvSet->auxBodUsed)
    {
      totalDuration += pAdvSet->auxAdvBod.minDurUsec;
    }

    /* Pick an interval so that advertising (primary + aux) BOD would take less than half of total bandwidth. */
    prefIntervalUsec = WSF_MAX(totalDuration * 2, pAdvSet->param.priAdvInterMinUsec);

    if (pAdvSet->advBodAbort)
    {
      pAdvSet->advBodAbort = FALSE;
      SchInsertNextAvailable(pOp);
    }
    else
    {
      if (lmgrGetOpFlag(LL_OP_MODE_FLAG_ENA_ADV_DLY))
      {
        pOp->dueUsec += BB_BLE_TO_US(lctrCalcAdvDelay());
      }

      /* Scheduling must succeed with LCTR_SCH_MAX_SPAN. */
      (void)SchInsertEarlyAsPossible(pOp, prefIntervalUsec, LCTR_SCH_MAX_SPAN);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Abort an extended advertising primary channel operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrSlvExtAdvAbortOp(BbOpDesc_t *pOp)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  pAdvSet->advBodAbort = TRUE;

  lctrSlvExtAdvEndOp(pOp);
}

/*************************************************************************************************/
/*!
 *  \brief  End an extended advertising secondary channel operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrSlvAuxAdvEndOp(BbOpDesc_t *pOp)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = pOp->prot.pBle;
  WSF_ASSERT(pOp->protId == BB_PROT_BLE);
  WSF_ASSERT(pOp->prot.pBle->chan.opType == BB_BLE_OP_SLV_AUX_ADV_EVENT);

  do
  {
    if (pAdvSet->shutdown || pAdvSet->connIndRcvd)
    {
    }
    else if (pAdvSet->maxEvents &&
             (++pAdvSet->numEvents >= pAdvSet->maxEvents))
    {
      pAdvSet->termReason = LL_ERROR_CODE_LIMIT_REACHED;
    }
    else
    {
      /* Continue operation. */
      break;
    }

    /* Disable (do not reschedule) auxiliary PDU. */
    pAdvSet->shutdown = TRUE;
    pAdvSet->auxBodUsed = FALSE;

    /* Ensure all BODs are de-scheduled. */
    SchRemove(&pAdvSet->advBod);

    /* Last BOD to terminate; send terminate event. */
    lctrSendAdvSetMsg(pAdvSet, LCTR_EXT_ADV_MSG_TERMINATE);

    return;

  } while (FALSE);

  if (pBle->chan.tifsTxPhyOptions != BB_PHY_OPTIONS_DEFAULT)
  {
    /* Set PHY options to host defined behavior for AUX_ADV_IND. */
    pBle->chan.initTxPhyOptions = pBle->chan.tifsTxPhyOptions;
  }
  else
  {
    /* Set PHY options to default behavior for AUX_ADV_IND. */
    pBle->chan.initTxPhyOptions = lmgrSlvAdvCb.defTxPhyOpts;
  }

  /*** Update advertising data ***/

  if (pAdvSet->advData.alt.ext.modified)
  {
    pAdvSet->advData.alt.ext.modified = FALSE;
    memcpy(pAdvSet->advData.pBuf, pAdvSet->advData.alt.ext.buf, pAdvSet->advData.alt.ext.len);
    pAdvSet->advData.len = pAdvSet->advData.alt.ext.len;
    pAdvSet->param.advDID = pAdvSet->advData.alt.ext.did;
    pAdvSet->advData.fragPref = pAdvSet->advData.alt.ext.fragPref;

    /* Update superior PDU. */
    BbBleSlvAdvEvent_t * const pAdv = &pAdvSet->bleData.op.slvAdv;
    pAdv->txAdvLen = lctrPackAdvExtIndPdu(pAdvSet, pAdvSet->advHdrBuf, FALSE);

    if (!pAdvSet->pExtAdvAuxPtr)
    {
      /* Disable (do not reschedule) auxiliary PDU. */
      pAdvSet->auxBodUsed = FALSE;
      return;
    }
  }

  if (pAdvSet->scanRspData.alt.ext.modified)
  {
    pAdvSet->scanRspData.alt.ext.modified = FALSE;
    memcpy(pAdvSet->scanRspData.pBuf, pAdvSet->scanRspData.alt.ext.buf, pAdvSet->scanRspData.alt.ext.len);
    pAdvSet->scanRspData.len = pAdvSet->scanRspData.alt.ext.len;
    pAdvSet->param.advDID = pAdvSet->scanRspData.alt.ext.did;
    pAdvSet->scanRspData.fragPref = pAdvSet->scanRspData.alt.ext.fragPref;
  }

  /* Update Data ID when periodic advertising is enabled or disabled. */
  if (pAdvSet->didPerUpdate == TRUE)
  {
    pAdvSet->param.advDID = pAdvSet->advData.alt.ext.did;
    pAdvSet->didPerUpdate = FALSE;
  }

  /*** Update operation ***/
  if (BbBleResListIsRpaUpd(pAdvSet->param.peerAddrType, pAdvSet->param.peerAddr))
  {
    /* Update AUX_ADV_IND in case advA or/and tgtA are changed, AUX_ADV_IND for non-conn and non-scan will be updated again if necessary in the lctrSlvTxSetupAuxAdvDataHandler. */
    BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBle->op.slvAuxAdv;

    pAdvSet->advData.txOffs = 0;
    pAuxAdv->txAuxAdvPdu[0].len = lctrPackAuxAdvIndPdu(pAdvSet, pAdvSet->auxAdvHdrBuf, &pAdvSet->advData, FALSE);
    pAuxAdv->txAuxAdvPdu[1].pBuf = pAdvSet->advData.pBuf;
    pAuxAdv->txAuxAdvPdu[1].len = pAdvSet->advData.len;
  }
  /*** Reschedule operation ***/

  lctrSlvAuxRescheduleOp(pAdvSet, pOp);
}

/*************************************************************************************************/
/*!
 *  \brief  End an periodic advertising channel operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrSlvPeriodicAdvEndOp(BbOpDesc_t *pOp)
{
  lctrAdvSet_t * const pAdvSet = pOp->pCtx;
  BbBleData_t * const pBle = pOp->prot.pBle;

  WSF_ASSERT(pOp->protId == BB_PROT_BLE);
  WSF_ASSERT(pOp->prot.pBle->chan.opType == BB_BLE_OP_SLV_PER_ADV_EVENT);

  if (pAdvSet->perParam.shutdown)
  {
    /* Last BOD to terminate; send terminate event. */
    lctrSendPeriodicAdvSetMsg(pAdvSet, LCTR_PER_ADV_MSG_TERMINATE);
    return;
  }

  if (pBle->chan.tifsTxPhyOptions != BB_PHY_OPTIONS_DEFAULT)
  {
    /* Set PHY options to host defined behavior for AUX_SYNC_IND. */
    pBle->chan.initTxPhyOptions = pBle->chan.tifsTxPhyOptions;
  }
  else
  {
    /* Set PHY options to default behavior for AUX_SYNC_IND. */
    pBle->chan.initTxPhyOptions = lmgrSlvAdvCb.defTxPhyOpts;
  }

  /*** Update advertising data ***/

  if (pAdvSet->perAdvData.alt.ext.modified)
  {
    pAdvSet->perAdvData.alt.ext.modified = FALSE;
    memcpy(pAdvSet->perAdvData.pBuf, pAdvSet->perAdvData.alt.ext.buf, pAdvSet->perAdvData.alt.ext.len);
    pAdvSet->perAdvData.len = pAdvSet->perAdvData.alt.ext.len;
    pAdvSet->perParam.advDID = pAdvSet->perAdvData.alt.ext.did;
  }

  /*** Update operation ***/

  /* Updated in lctrSlvTxSetupPeriodicAdvDataHandler(). */

  /*** Reschedule operation ***/

  while (TRUE)
  {
    pOp->dueUsec += pAdvSet->perParam.perAdvInterUsec;

    pAdvSet->perParam.perEventCounter++;

    /*** Process ACAD fields ***/

    lctrSlvAcadHandler(pAdvSet);

    pAdvSet->perParam.perChIdx = lctrPeriodicSelectNextChannel(&pAdvSet->perParam.perChanParam,
                                                               pAdvSet->perParam.perEventCounter);

    if (SchInsertAtDueTime(pOp, NULL))
    {
      break;
    }

    LL_TRACE_WARN1("!!! Periodic advertising schedule conflict eventCounter=%u", pAdvSet->perParam.perEventCounter);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Abort an periodic advertising channel operation.
 *
 *  \param  pOp     Aborted operation.
 */
/*************************************************************************************************/
void lctrSlvPeriodicAdvAbortOp(BbOpDesc_t *pOp)
{
  WSF_ASSERT(pOp->protId == BB_PROT_BLE);
  WSF_ASSERT(pOp->prot.pBle->chan.opType == BB_BLE_OP_SLV_PER_ADV_EVENT);

  LL_TRACE_WARN1("!!! Periodic advertising BOD aborted, eventCounter=%u", ((lctrAdvSet_t *)pOp->pCtx)->perParam.perEventCounter);

  lctrSlvPeriodicAdvEndOp(pOp);
}
