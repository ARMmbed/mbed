/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master scanning operation builder implementation file.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
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
#include "lctr_int_conn_master.h"
#include "lctr_int.h"
#include "sch_api.h"
#include "bb_ble_api_reslist.h"
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

/*! \brief      Scan operational context. */
lctrMstScanCtx_t lctrMstScan;

/*************************************************************************************************/
/*!
 *  \brief      Master scan reset handler.
 */
/*************************************************************************************************/
static void lctrMstScanResetHandler(void)
{
  BbBleScanMasterInit();
  LctrMstScanDefaults();
  LmgrMstInit();
}

/*************************************************************************************************/
/*!
 *  \brief      Master scan message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrMstScanDisp(lctrMsgHdr_t *pMsg)
{
  lctrMstScanExecuteSm(pMsg->event);
}

/*************************************************************************************************/
/*!
 *  \brief      Advertising report notification.
 *
 *  \param      pRpt      Advertising report.
 */
/*************************************************************************************************/
static void lctrMstScanAdvRptNotify(LlAdvReportInd_t *pRpt)
{
  uint64_t hash;
  lctrAdvRptGenerateLegacyHash(&hash, pRpt->addrType, BstreamToBda64(pRpt->addr), pRpt->eventType);

  if (lctrAdvRptCheckDuplicate(&lctrMstScan.data.disc.advFilt, hash))
  {
    /* Duplicate found, just exit. */
    return;
  }

  lctrAdvRptAddEntry(&lctrMstScan.data.disc.advFilt, hash);

  if (lctrMstScan.state == LCTR_SCAN_STATE_DISCOVER)
  {
    pRpt->hdr.param = 0;
    pRpt->hdr.event = LL_ADV_REPORT_IND;
    pRpt->hdr.status = LL_SUCCESS;

    LL_TRACE_INFO1("### LlEvent ###  LL_ADV_REPORT_IND, status=LL_SUCCESS addressType=%u", pRpt->addrType);

    LmgrSendEvent((LlEvt_t *)pRpt);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Create advertising report.
 *
 *  \param      pAdvBuf       Received ADV buffer.
 *  \param      pRpt          Advertising report indication.
 *  \param      pLocalMatch   Checks if the received ADV packet matches with local address.
 *
 *  \return  TRUE if report generated, FALSE if not.
 */
/*************************************************************************************************/
static bool_t lctrMstCreateAdvRpt(uint8_t *pAdvBuf, LlAdvReportInd_t *pRpt, bool_t *pLocalMatch)
{
  lctrAdvbPduHdr_t advHdr;
  const bbBlePduFiltParams_t *pFiltParams = &lctrMstScan.bleData.pduFilt;

  lctrUnpackAdvbPduHdr(&advHdr, pAdvBuf);
  if ((advHdr.len < LL_ADV_PREFIX_LEN) ||      /* invalid packet length. */
      (advHdr.len > (LL_ADVB_MAX_LEN - LL_ADV_HDR_LEN)))
  {
    return FALSE;
  }

  /*** Construct report ***/

  /* Default advertising data values. */
  uint8_t *pData = pAdvBuf + LL_ADV_HDR_LEN + LL_ADV_PREFIX_LEN;
  uint8_t dataLen = advHdr.len - LL_ADV_PREFIX_LEN;

  switch (advHdr.pduType)
  {
    case LL_PDU_ADV_IND:
      pRpt->eventType = LL_RPT_TYPE_ADV_IND;
      break;
    case LL_PDU_ADV_DIRECT_IND:
      pRpt->eventType = LL_RPT_TYPE_ADV_DIRECT_IND;
      dataLen = 0;        /* no payload */
      break;
    case LL_PDU_ADV_NONCONN_IND:
      pRpt->eventType = LL_RPT_TYPE_ADV_NONCONN_IND;
      break;
    case LL_PDU_SCAN_RSP:
      pRpt->eventType = LL_RPT_TYPE_SCAN_RSP;
      break;
    case LL_PDU_ADV_SCAN_IND:
      pRpt->eventType = LL_RPT_TYPE_ADV_SCAN_IND;
      break;
    default:
      /* Invalid packet type. */
      WSF_ASSERT(FALSE);
      return FALSE;
  }

  pRpt->pData = pData;
  pRpt->len   = dataLen;
  pRpt->rssi  = pAdvBuf[LCTR_ADVB_BUF_OFFSET_RSSI];
  /* BYTES_TO_UINT32(hash, pAdvBuf + LCTR_ADVB_BUF_OFFSET_CRC); */  /* TODO: use hash */

  /*** Process PDU again ***/

  /*
   * Note: Peer RPA may not have been resolved in TIFS.  The PDU must be reprocessed to provide
   *       a final result.
   */

  bbBlePduFiltResults_t filtResults;

  /* Pass through filters. */
  if (!BbBlePduFiltCheck(pAdvBuf, pFiltParams, TRUE, &filtResults))
  {
    return FALSE;
  }

  uint64_t peerIdAddr = 0;
  uint8_t peerIdAddrType = 0;

  BbBlePduFiltResultsGetPeerIdAddr(&filtResults, &peerIdAddr, &peerIdAddrType);

  pRpt->addrType = peerIdAddrType;
  Bda64ToBstream(pRpt->addr, peerIdAddr);

  *pLocalMatch = filtResults.localMatch;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      ADVB packet post-processing.
 */
/*************************************************************************************************/
void lctrMstRxAdvBPduHandler(void)
{
  uint8_t *pAdvBuf;
  wsfHandlerId_t handlerId;

  while ((pAdvBuf = WsfMsgDeq(&lctrMstScan.rxAdvbQ, &handlerId)) != NULL)
  {
    LlAdvReportInd_t rpt;
    bool_t localMatch;

    if (lctrMstCreateAdvRpt(pAdvBuf, &rpt, &localMatch))
    {
      rpt.directAddrType = 0xFF;

      lctrMstScanAdvRptNotify(&rpt);
    }

    WsfMsgFree(pAdvBuf);
    lctrAdvReportsDec();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Direct ADVB packet post-processing.
 */
/*************************************************************************************************/
void lctrMstRxDirectAdvBPduHandler(void)
{
  uint8_t *pAdvBuf;
  wsfHandlerId_t handlerId;

  while ((pAdvBuf = WsfMsgDeq(&lctrMstScan.rxDirectAdvbQ, &handlerId)) != NULL)
  {
    LlAdvReportInd_t rpt;
    bool_t localMatch;

    if (lctrMstCreateAdvRpt(pAdvBuf, &rpt, &localMatch))
    {
      WSF_ASSERT(rpt.eventType == LL_RPT_TYPE_ADV_DIRECT_IND);

      const uint8_t *pInitA = pAdvBuf + LL_ADV_HDR_LEN + BDA_ADDR_LEN;
      rpt.directAddrType = LL_ADDR_RANDOM;
      memcpy(rpt.directAddr, pInitA, BDA_ADDR_LEN);

      /* Local address may have actually not have been resolved in TIFS. */
      if (localMatch)
      {
        /* Generate normal advertising report. */
        rpt.directAddrType = 0xFF;
      }

      lctrMstScanAdvRptNotify(&rpt);
    }

    WsfMsgFree(pAdvBuf);
    lctrAdvReportsDec();
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Build scan discovery operation.
 */
/*************************************************************************************************/
void lctrMstDiscoverBuildOp(void)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &lctrMstScan.scanBod;
  BbBleData_t * const pBle = &lctrMstScan.bleData;
  BbBleMstAdvEvent_t * const pScan = &pBle->op.mstAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  uint8_t *pBuf;

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_MOVEABLE;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->pCtx = &lctrMstScan;
  pOp->endCback = lctrMstDiscoverEndOp;
  pOp->abortCback = lctrMstDiscoverEndOp;
  pOp->recoverable = TRUE;

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
                              (1 << LL_PDU_ADV_DIRECT_IND) |
                              (1 << LL_PDU_ADV_NONCONN_IND) |
                              (1 << LL_PDU_SCAN_RSP) |
                              (1 << LL_PDU_ADV_SCAN_IND);
  if (lctrMstScan.scanParam.scanFiltPolicy & LL_SCAN_FILTER_WL_BIT)
  {
    pBle->pduFilt.wlPduTypeFilt = pBle->pduFilt.pduTypeFilt;
  }
  /* Local addresses that are resolvable and cannot be resolved are optionally allowed. */
  if (lctrMstScan.scanParam.scanFiltPolicy & LL_SCAN_FILTER_RES_INIT_BIT)
  {
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_RES_OPT);
  }

  /*** BLE Scan Setup: Rx packets ***/

  pScan->scanChMap = lmgrMstScanCb.scanChanMap;

  pScan->rxAdvCback = lctrMstDiscoverAdvPktHandler;
  pScan->rxAdvPostCback = lctrMstDiscoverAdvPktPostProcessHandler;
  pScan->txReqCback = lctrMstScanReqTxCompHandler;
  pScan->rxRspCback = lctrMstScanRspRxCompHandler;

  if ((pScan->pRxAdvBuf = WsfMsgAlloc(LCTR_ADVB_BUF_SIZE)) == NULL)
  {
    /* Attempt to obtain buffer on next advertising operation. */
    LL_TRACE_ERR0("Could not allocate advertising buffer");
    /* TODO need OOM recovery */
    WSF_ASSERT(FALSE);
  }

  switch (lctrMstScan.scanParam.scanType)
  {
    case LL_SCAN_ACTIVE:
    {
      if ((pScan->pRxRspBuf = WsfMsgAlloc(LCTR_ADVB_BUF_SIZE)) == NULL)
      {
        /* Attempt to obtain buffer on next advertising operation. */
        LL_TRACE_ERR0("Could not allocate advertising buffer");
        /* TODO need OOM recovery */
        WSF_ASSERT(FALSE);
      }

      break;
    }
    case LL_SCAN_PASSIVE:
    default:
      break;
  }

  /*** BLE Scan Setup: Tx scan request packet ***/

  /* Always match local address in PDU to initiator's address (in directed advertisements). */
  if (lctrMstScan.scanParam.ownAddrType & LL_ADDR_RANDOM_BIT)
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

  switch (lctrMstScan.scanParam.scanType)
  {
    case LL_SCAN_ACTIVE:
    {
      lctrScanReq_t scanReq = { 0 };

      lctrMstScan.reqPduHdr.pduType = LL_PDU_SCAN_REQ;
      lctrMstScan.reqPduHdr.len = LL_SCAN_REQ_PDU_LEN;

      lctrMstScan.reqPduHdr.txAddrRnd = BB_BLE_PDU_FILT_FLAG_IS_SET(&pBle->pduFilt, LOCAL_ADDR_MATCH_RAND);
      scanReq.scanAddr = pBle->pduFilt.localAddrMatch;

      /* Pack only known packet information, advertiser's address resolved in Rx handler. */
      pBuf  = lctrMstScan.reqBuf;
      pBuf += LL_ADV_HDR_LEN;
      /* pBuf += */ lctrPackScanReqPdu(pBuf, &scanReq);

      pScan->pTxReqBuf = lctrMstScan.reqBuf;
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

  lctrMstScan.reqPduHdr.chSel = LL_CH_SEL_1;

  /*** Commit operation ***/

  pOp->minDurUsec = LCTR_MIN_SCAN_USEC;
  pOp->maxDurUsec = LCTR_BLE_TO_US(lctrMstScan.scanParam.scanWindow);

  lctrMstScan.selfTerm = FALSE;
  lctrMstScan.shutdown = FALSE;

  SchInsertNextAvailable(pOp);
  lctrMstScan.scanWinStartUsec = pOp->dueUsec;

  LL_TRACE_INFO1("    >>> Discover started, dueUsec=%u <<<", pOp->dueUsec);
  LL_TRACE_INFO1("                          pBod=0x%08x", pOp);
}


/*************************************************************************************************/
/*!
 *  \brief      Cleanup resources on advertising operation termination.
 *
 *  \param      pCtx    Scan context.
 */
/*************************************************************************************************/
void lctrMstScanCleanupOp(lctrMstScanCtx_t *pCtx)
{
  uint8_t *pBuf;

  pBuf = pCtx->bleData.op.mstAdv.pRxAdvBuf;
  pCtx->bleData.op.mstAdv.pRxAdvBuf = NULL;

  if (pBuf)
  {
    /* Recover header. */
    WsfMsgFree(pBuf);
  }

  pBuf = pCtx->bleData.op.mstAdv.pRxRspBuf;
  pCtx->bleData.op.mstAdv.pRxRspBuf = NULL;

  if (pBuf)
  {
    /* Recover header. */
    WsfMsgFree(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Select initial scan channel.
 *
 *  \param      chanMap     Scan channel map.
 *
 *  \return     Scan channel.
 */
/*************************************************************************************************/
uint8_t lctrScanChanSelectInit(uint8_t chanMap)
{
  uint8_t chanIdx = LL_CHAN_ADV_MIN_IDX;

  /* Compute next channel. */
  while (!((1 << (chanIdx - LL_CHAN_ADV_MIN_IDX)) & chanMap))
  {
    if (++chanIdx > LL_CHAN_ADV_MAX_IDX)
    {
      return LL_CHAN_ADV_MIN_IDX;
    }
  }

  return chanIdx;
}

/*************************************************************************************************/
/*!
 *  \brief      Select next scan channel.
 *
 *  \param      chanIdx     Current scan channel.
 *  \param      chanMap     Scan channel map.
 *
 *  \return     Next scan channel.
 */
/*************************************************************************************************/
uint8_t lctrScanChanSelectNext(uint8_t chanIdx, uint8_t chanMap)
{
  if (!chanMap)
  {
    return LL_CHAN_ADV_MIN_IDX;
  }

  /* Compute next channel. */
  do
  {
    if (++chanIdx > LL_CHAN_ADV_MAX_IDX)
    {
      chanIdx = LL_CHAN_ADV_MIN_IDX;
    }
  }
  while (!((1 << (chanIdx - LL_CHAN_ADV_MIN_IDX)) & chanMap));

  return chanIdx;
}

/*************************************************************************************************/
/*!
 *  \brief      Send initiate error host notification.
 *
 *  \param      reason          Status code.
 *  \param      peerAddrType    Peer address type.
 *  \param      peerAddr        Peer address.
 */
/*************************************************************************************************/
void lctrScanNotifyHostInitiateError(uint8_t reason, uint8_t peerAddrType, uint64_t peerAddr)
{
  LlConnInd_t evt =
  {
    .hdr =
    {
      .event  = LL_CONN_IND,
      .status = reason
    },

    .status = reason,

    .addrType = peerAddrType
  };

  Bda64ToBstream(evt.peerAddr, peerAddr);

  LL_TRACE_INFO1("### LlEvent ###  LL_CONN_IND, status=%u", reason);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Enable advertising report filtering.
 *
 *  \param      pAdvFilt    Advertising report filter data.
 *  \param      filtEna     Enable advertising report filtering.
 */
/*************************************************************************************************/
void lctrAdvRptEnable(lctrAdvRptFilt_t *pAdvFilt, bool_t filtEna)
{
  memset(pAdvFilt, 0, sizeof(lctrAdvRptFilt_t));
  pAdvFilt->enable = filtEna;
}

/*************************************************************************************************/
/*!
 *  \brief      Create legacy advertising hash.
 *
 *  \param      pHash       Storage for hash.
 *  \param      addrType    Address type.
 *  \param      addr        Address.
 *  \param      eventType   Event type.
 *
 *  \return     TRUE if duplicate, FALSE is unique or filter is disabled.
 */
/*************************************************************************************************/
void lctrAdvRptGenerateLegacyHash(uint64_t *pHash, uint8_t addrType, uint64_t addr, uint8_t eventType)
{
  addrType &= 0x3;      /* 2 valid bits. */
  eventType &= 0x07;    /* 3 valid bits. */

  *pHash = addr;                           /* 48 bits. */
  *pHash |= ((uint64_t)addrType  << 48);   /*  2 bits. */
  *pHash |= ((uint64_t)eventType << 50);   /*  3 bits. */
}

/*************************************************************************************************/
/*!
 *  \brief      Create extended advertising hash.
 *
 *  \param      pHash       Storage for hash.
 *  \param      addrType    Address type.
 *  \param      addr        Address.
 *  \param      eventType   Event type.
 *  \param      sid         Advertising Set ID (0 for legacy).
 *  \param      did         Advertising Data ID (0 for legacy).
 *
 *  \return     TRUE if duplicate, FALSE is unique or filter is disabled.
 */
/*************************************************************************************************/
void lctrAdvRptGenerateExtHash(uint64_t *pHash, uint8_t addrType, uint64_t addr, uint8_t eventType,
                               uint8_t sid, uint16_t did)
{
  addrType &= 0x3;      /*  2 valid bits. */
  eventType &= 0x1F;    /*  5 valid bits; ignore Data Status. */
  sid &= 0xF;           /*  4 valid bits. */
  did &= 0xFFF;         /* 12 valid bits. */

  *pHash = addr & 0x1FFFFFFFFFF;            /* 41 LSB bits. */
  *pHash |= ((uint64_t)addrType   << 41);   /*  2 bits. */
  *pHash |= ((uint64_t)eventType  << 43);   /*  5 bits. */
  *pHash |= ((uint64_t)sid        << 48);   /*  4 bits. */
  *pHash |= ((uint64_t)did        << 52);   /* 12 bits. */
}

/*************************************************************************************************/
/*!
 *  \brief      Check for duplicate report.
 *
 *  \param      pAdvFilt    Advertising report filter data.
 *  \param      hash        Advertising report hash.
 *
 *  \return     TRUE if duplicate, FALSE is unique or filter is disabled.
 */
/*************************************************************************************************/
bool_t lctrAdvRptCheckDuplicate(lctrAdvRptFilt_t *pAdvFilt, uint64_t hash)
{
  if (!pAdvFilt->enable)
  {
    return FALSE;
  }

  uint8_t i;
  uint8_t headIdx = pAdvFilt->headIdx;
  uint32_t validMask = pAdvFilt->validMask;

  for (i = 0; i < LL_NUM_ADV_FILT; i++)
  {
    if (((validMask & (1 << i)) != 0) && (pAdvFilt->filtTbl[i] == hash))
    {
      /* If this advertisement was the last received, do not modify table. */
      if (i != headIdx)
      {
        /* Rotate older entries into duplicate location. */
        unsigned int j = i;
        unsigned int k = (j == LL_NUM_ADV_FILT - 1) ? 0 : (j + 1);
        while (k != headIdx)
        {
          /* Stop when next entry not valid. */
          if ((validMask & (1 << k)) == 0)
          {
            validMask &= ~(1 << j);
            break;
          }

          /* Copy next entry to current location. */
          pAdvFilt->filtTbl[j] = pAdvFilt->filtTbl[k];

          /* Move to next entry. */
          j = k;
          k = (j == LL_NUM_ADV_FILT - 1) ? 0 : (j + 1);
        }

        /* Back head to previous entry. */
        headIdx = (headIdx == 0) ? (LL_NUM_ADV_FILT - 1) : (headIdx - 1);
        pAdvFilt->headIdx = headIdx;

        /* Copy entry to head of table. */
        pAdvFilt->validMask = validMask | (1 << headIdx);
        pAdvFilt->filtTbl[headIdx] = hash;
      }

      /* Duplicate found. */
      return TRUE;
    }
  }

  pAdvFilt->addToFiltTbl = TRUE;
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Add the new hash to the filter table. Using ring buffer to store the entries.
 *
 *  \param      pAdvFilt    Advertising report filter data.
 *  \param      hash        Advertising report hash.
 */
/*************************************************************************************************/
void lctrAdvRptAddEntry(lctrAdvRptFilt_t *pAdvFilt, uint64_t hash)
{
  if (!pAdvFilt->enable)
  {
    return;
  }

  uint8_t headIdx = pAdvFilt->headIdx;
  uint32_t validMask = pAdvFilt->validMask;

  WSF_ASSERT(pAdvFilt->addToFiltTbl == TRUE);

  /* Store advertiser address for filtering. */

  /* Back head to previous entry. */
  headIdx = (headIdx == 0) ? (LL_NUM_ADV_FILT - 1) : (headIdx - 1);
  pAdvFilt->headIdx = headIdx;

  /* Copy entry to head of table. */
  pAdvFilt->validMask = validMask | (1 << headIdx);
  pAdvFilt->filtTbl[headIdx] = hash;
  pAdvFilt->addToFiltTbl = FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for scanning master.
 */
/*************************************************************************************************/
void LctrMstScanInit(void)
{
  /* Add scan reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_SCAN] = lctrMstScanResetHandler;

  /* Add scan message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_SCAN] = (LctrMsgDisp_t)lctrMstScanDisp;

  /* Add scan event handlers. */
  lctrEventHdlrTbl[LCTR_EVENT_RX_ADVB] = lctrMstRxAdvBPduHandler;
  lctrEventHdlrTbl[LCTR_EVENT_RX_DIRECT_ADVB] = lctrMstRxDirectAdvBPduHandler;

  LctrMstScanDefaults();

  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_4_2)
  {
    lmgrPersistCb.featuresDefault |=
        LL_FEAT_EXT_SCAN_FILT_POLICY;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set default values for scanning master.
 */
/*************************************************************************************************/
void LctrMstScanDefaults(void)
{
  memset(&lctrMstScan, 0, sizeof(lctrMstScan));
}

/*************************************************************************************************/
/*!
 *  \brief      Increment number of pending advertising reports.
 */
/*************************************************************************************************/
void lctrAdvReportsInc(void)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(lmgrMstScanCb.numAdvReport < pLctrRtCfg->maxAdvReports);

  WSF_CS_ENTER(cs);
  lmgrMstScanCb.numAdvReport++;
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief      Decrement number of pending advertising reports.
 */
/*************************************************************************************************/
void lctrAdvReportsDec(void)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(lmgrMstScanCb.numAdvReport > 0);

  WSF_CS_ENTER(cs);
  lmgrMstScanCb.numAdvReport--;
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief      Check whether scan is enabled or not.
 *
 *  \return     True if scanner enabled. False if not.
 */
/*************************************************************************************************/
bool_t LctrMstScanIsEnabled(void)
{
  return (lctrMstScan.state != LCTR_SCAN_STATE_DISABLED);
}

/*************************************************************************************************/
/*!
 *  \brief      Check whether private address is used for scanner
 *
 *  \return     Returns True if scanner is using private addresses. False if not.
 */
/*************************************************************************************************/
bool_t LctrMstScanIsPrivAddr(void)
{
  /* Check for private Addr bit. */
  return (lctrMstScan.scanParam.ownAddrType & LL_ADDR_RANDOM_BIT);
}
