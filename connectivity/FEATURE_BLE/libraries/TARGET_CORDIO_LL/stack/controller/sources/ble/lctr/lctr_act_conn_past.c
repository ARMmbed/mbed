/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller connection state machine action routines for PAST feature.
 *
 *  Copyright (c) 2018 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_conn.h"
#include "lctr_int_adv_slave.h"
#include "lctr_int_adv_master_ae.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "lmgr_api_conn.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_timer.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief      Send periodic sync indication from scanner to peer.
 *
 *  \param      pCtx    Connection context.
 *
 *  This function is called from BOD end callback of master/slave connection.
 */
/*************************************************************************************************/
void lctrSendPerSyncFromScan(lctrConnCtx_t *pCtx)
{
  uint8_t *pPdu;
  lctrPerScanCtx_t *pPerScanCtx = LCTR_GET_PER_SCAN_CTX(pCtx->perSyncHandle);

  WSF_ASSERT((pPerScanCtx->enabled == TRUE) && (pPerScanCtx->state == LCTR_PER_SCAN_STATE_SYNC_ESTD));

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PERIODIC_SYNC_PDU_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;
    uint8_t field8 = 0;
    /* Use the same logic as connection update to set connEventCounter. */
    uint16_t ceOffset = LL_MIN_INSTANT + 1 +          /* +1 for next CE */
                        pCtx->maxLatency;             /* ensure slave will listen this packet */

#if (LL_ENABLE_TESTER == TRUE)
    if (llTesterCb.eventCounterOverride == TRUE)
    {
      /* ceOffset can be in the range of -0x3FFF ~ +0x3FFF. */
      ceOffset = llTesterCb.eventCounterOffset + 1;         /* +1 for next CE */
    }
#endif

    /* Find PEa, event counter of PE whose starting time is after CEref. */
    uint16_t paEventCounter = pPerScanCtx->lastActiveEvent;
    uint32_t paAnchor = pPerScanCtx->lastAnchorPointUsec;
    uint32_t ceRefStart, ceRefEnd;

    /* Calculate the end time of CEref. */
    ceRefStart = lctrConnGetAnchorPoint(pCtx, (pCtx->eventCounter + ceOffset));
    ceRefEnd = ceRefStart + pCtx->effConnDurUsec + BbGetSchSetupDelayUs();

    /* paAnchor is for the first PE in the future from ceRefEnd. */
    if (BbGetTargetTimeDelta(ceRefEnd, paAnchor) > 0)
    {
      uint16_t numPE = BbGetTargetTimeDelta(ceRefEnd, paAnchor) / pPerScanCtx->perInterUsec + 1;

      paAnchor += numPE * pPerScanCtx->perInterUsec;
      paEventCounter += numPE;
    }
    else
    {
      uint16_t numPE = BbGetTargetTimeDelta(paAnchor, ceRefEnd) / pPerScanCtx->perInterUsec;

      paAnchor -= numPE * pPerScanCtx->perInterUsec;
      paEventCounter -= numPE;
    }

    uint8_t offsUnits;
    uint8_t offsAdjust = 0;
    uint16_t offs;
    uint32_t offsUsec = BbGetTargetTimeDelta(paAnchor, ceRefStart);

    if (offsUsec < LL_30_USEC_OFFS_MAX_USEC)
    {
      offsUnits = LCTR_OFFS_UNITS_30_USEC;
      offs = LL_MATH_DIV_30(offsUsec);
    }
    else
    {
      if (offsUsec >= LL_SYNC_OFFS_ADJUST_USEC)
      {
        offsAdjust = 1;
        offsUsec -= LL_SYNC_OFFS_ADJUST_USEC;
      }

      offsUnits = LCTR_OFFS_UNITS_300_USEC;
      offs = LL_MATH_DIV_300(offsUsec);
    }

    LL_TRACE_INFO1("LL_PERIODIC_SYNC_IND from SCAN ceRef anchor point = %u", ceRefStart);
    LL_TRACE_INFO1("                               PA lastAnchorPoint = %u", pPerScanCtx->lastAnchorPointUsec);
    LL_TRACE_INFO1("                               PA REF paAnchor = %u", paAnchor);
    LL_TRACE_INFO1("                               offsUsec = %u", offsUsec);

    /*** Assemble control PDU. ***/
    UINT8_TO_BSTREAM (pBuf, LL_PDU_PERIODIC_SYNC_IND);
    UINT16_TO_BSTREAM(pBuf, pCtx->perServiceData);  /* ID */

    /* Fill in syncInfo (18 octets) */
    UINT16_TO_BSTREAM(pBuf, offs |
                            (offsUnits << 13) |       /* Offset units. */
                            (offsAdjust << 14));      /* Offset adjust. */

    UINT16_TO_BSTREAM(pBuf, LCTR_PER_INTER_TO_MS(pPerScanCtx->perInterUsec));   /* Interval */

    uint64_t temp = pPerScanCtx->chanParam.chanMask |       /* SyncInfo - ChMap */
                    ((uint64_t)pPerScanCtx->sca << 37);     /* SyncInfo - SCA of the device sending AUX_SYNC_IND. */
    UINT40_TO_BSTREAM(pBuf, temp);
    UINT32_TO_BSTREAM(pBuf, pPerScanCtx->bleData.chan.accAddr);
    UINT24_TO_BSTREAM(pBuf, pPerScanCtx->bleData.chan.crcInit);
    UINT16_TO_BSTREAM(pBuf, paEventCounter);                /* SyncInfo - Event Counter */

    UINT16_TO_BSTREAM(pBuf, (pCtx->eventCounter + ceOffset));   /* ref connEventCounter */
    UINT16_TO_BSTREAM(pBuf, pPerScanCtx->lastActiveEvent);      /* lastPaEventCounter */

    field8 = (pPerScanCtx->advSID       << 0) |
             (pPerScanCtx->trsfAddrType << 4) |
             (lctrComputeSca()          << 5);    /* SCA of the device sending this PDU. */
    UINT8_TO_BSTREAM(pBuf, field8);
    UINT8_TO_BSTREAM(pBuf, lctrPhyToPhysBit(pPerScanCtx->rxPhys));
    BDA64_TO_BSTREAM(pBuf, pPerScanCtx->trsfAdvAddr);
    UINT16_TO_BSTREAM(pBuf, pCtx->eventCounter);                /* syncConnEventCounter */

    /*** Queue for transmit. ***/
    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send periodic sync indication from broadcaster to peer.
 *
 *  \param      pCtx    Connection context.
 *
 *  This function is called from BOD end callback of master/slave connection.
 */
/*************************************************************************************************/
void lctrSendPerSyncFromBcst(lctrConnCtx_t *pCtx)
{
  uint8_t *pPdu;
  lctrAdvSet_t *pAdvSet = lctrFindAdvSet((uint8_t)pCtx->perSyncHandle);

  WSF_ASSERT(pAdvSet);
  WSF_ASSERT(pAdvSet->perParam.perAdvEnabled == TRUE);

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PERIODIC_SYNC_PDU_LEN)) != NULL)
  {
    BbOpDesc_t * const pPerOp = &pAdvSet->perParam.perAdvBod;
    BbBleData_t *pBle = &pAdvSet->perParam.perBleData;
    uint8_t *pBuf = pPdu;
    uint8_t field8 = 0;
    /* Use the same logic as connection update to set connEventCounter. */
    uint16_t ceOffset = LL_MIN_INSTANT + 1 +          /* +1 for next CE */
                        pCtx->maxLatency;             /* ensure slave will listen this packet */

#if (LL_ENABLE_TESTER == TRUE)
    if (llTesterCb.eventCounterOverride == TRUE)
    {
      /* ceOffset can be in the range of -0x3FFF ~ +0x3FFF. */
      ceOffset = llTesterCb.eventCounterOffset + 1;         /* +1 for next CE */
    }
#endif

    /* Find PEa, event counter of PE whose starting time is after CEref. */
    uint16_t paEventCounter = pAdvSet->perParam.perEventCounter;
    uint32_t paAnchor = pPerOp->dueUsec;
    uint32_t ceRefStart, ceRefEnd;

    /* Calculate the end time of CEref. */
    ceRefStart = lctrConnGetAnchorPoint(pCtx, (pCtx->eventCounter + ceOffset));
    ceRefEnd = ceRefStart + pCtx->effConnDurUsec + BbGetSchSetupDelayUs();

    /* paAnchor is for the first PE in the future from ceRefEnd. */
    if (BbGetTargetTimeDelta(ceRefEnd, paAnchor) > 0)
    {
      uint16_t numPE = BbGetTargetTimeDelta(ceRefEnd, paAnchor) / pAdvSet->perParam.perAdvInterUsec + 1;

      paAnchor += numPE * pAdvSet->perParam.perAdvInterUsec;
      paEventCounter += numPE;
    }
    else
    {
      uint16_t numPE = BbGetTargetTimeDelta(paAnchor, ceRefEnd) / pAdvSet->perParam.perAdvInterUsec;

      paAnchor -= numPE * pAdvSet->perParam.perAdvInterUsec;
      paEventCounter -= numPE;
    }

    uint8_t offsUnits;
    uint8_t offsAdjust = 0;
    uint16_t offs;
    uint32_t offsUsec = BbGetTargetTimeDelta(paAnchor, ceRefStart);

    if (offsUsec < LL_30_USEC_OFFS_MAX_USEC)
    {
      offsUnits = LCTR_OFFS_UNITS_30_USEC;
      offs = LL_MATH_DIV_30(offsUsec);
    }
    else
    {
      if (offsUsec >= LL_SYNC_OFFS_ADJUST_USEC)
      {
        offsAdjust = 1;
        offsUsec -= LL_SYNC_OFFS_ADJUST_USEC;
      }

      offsUnits = LCTR_OFFS_UNITS_300_USEC;
      offs = LL_MATH_DIV_300(offsUsec);
    }

    LL_TRACE_INFO1("LL_PERIODIC_SYNC_IND from BCST ceRef anchor point = %u", ceRefStart);
    LL_TRACE_INFO1("                               PA lastAnchorPoint = %u", pPerOp->dueUsec);
    LL_TRACE_INFO1("                               PA REF paAnchor = %u", paAnchor);
    LL_TRACE_INFO1("                               offsUsec = %u", offsUsec);

    /*** Assemble control PDU. ***/
    UINT8_TO_BSTREAM (pBuf, LL_PDU_PERIODIC_SYNC_IND);
    UINT16_TO_BSTREAM(pBuf, pCtx->perServiceData);  /* ID */

    /* Fill in syncInfo (18 octets) */
    UINT16_TO_BSTREAM(pBuf, offs |
                            (offsUnits << 13) |       /* Offset units. */
                            (offsAdjust << 14));      /* Offset adjust. */

    UINT16_TO_BSTREAM(pBuf, LCTR_PER_INTER_TO_MS(pAdvSet->perParam.perAdvInterUsec));   /* Interval */

    uint64_t temp = pAdvSet->perParam.perChanParam.chanMask |   /* SyncInfo - ChMap */
                    ((uint64_t)lctrComputeSca() << 37);         /* SyncInfo - SCA of the device sending AUX_SYNC_IND. */
    UINT40_TO_BSTREAM(pBuf, temp);
    UINT32_TO_BSTREAM(pBuf, pBle->chan.accAddr);
    UINT24_TO_BSTREAM(pBuf, pBle->chan.crcInit);
    UINT16_TO_BSTREAM(pBuf, paEventCounter);                    /* SyncInfo - Event Counter */

    UINT16_TO_BSTREAM(pBuf, (pCtx->eventCounter + ceOffset));
    /* If the Periodic Advertiser is sending the PDU,                                         */
    /* lastPaEventCount shall be set to the same value as the EventCounter field of SyncInfo. */
    UINT16_TO_BSTREAM(pBuf, pAdvSet->perParam.perEventCounter); /* lastPaEventCounter */

    uint8_t aType = 0;  /* 0 for public address, 1 for random address */
    if ((pAdvSet->param.ownAddrType & LL_ADDR_RANDOM_BIT) && (pAdvSet->bdAddrRndValid == TRUE))
    {
      aType = 1;
    }
    else if ((pAdvSet->param.ownAddrType == LL_ADDR_PUBLIC_IDENTITY) && (BDA64_ADDR_IS_RPA(pAdvSet->advA)))
    {
      aType = 1;
    }

    field8 = (pAdvSet->param.advSID << 0) |
             (aType                 << 4) |
             (lctrComputeSca()      << 5);    /* SCA of the device sending this PDU. */
    UINT8_TO_BSTREAM(pBuf, field8);
    UINT8_TO_BSTREAM(pBuf, lctrPhyToPhysBit(pBle->chan.txPhy));
    BDA64_TO_BSTREAM(pBuf, pAdvSet->advA);
    UINT16_TO_BSTREAM(pBuf, pCtx->eventCounter);                /* syncConnEventCounter */

    /*** Queue for transmit. ***/
    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Store periodic advertising sync transfer parameters.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrStorePeriodicSyncTrsf(lctrConnCtx_t *pCtx)
{
  pCtx->perSyncSrc = pLctrConnMsg->perAdvSyncTrsf.syncSource;
  pCtx->perServiceData  = pLctrConnMsg->perAdvSyncTrsf.serviceData;
  pCtx->perSyncHandle  = pLctrConnMsg->perAdvSyncTrsf.syncHandle;
}

/*************************************************************************************************/
/*!
 *  \brief      Send periodic sync indication PDU to peer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendPeriodicSyncInd(lctrConnCtx_t *pCtx)
{
  if (!pCtx->sendPerSync)
  {
    /* The PDU will be queued at the next BOD end callback. */
    pCtx->sendPerSync = TRUE;

    /* The procedure completes after sending out the indication. */
    pCtx->llcpNotifyMask &= ~(1 << LCTR_PROC_CMN_PER_ADV_SYNC_TRSF);
    lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_PROC_CMPL);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Handle received periodic sync indication PDU.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrReceivePeriodicSyncInd(lctrConnCtx_t *pCtx)
{
  lctrPerTransferSyncMsg_t *pMsg;

  /* Check if the controller is already sync with the same periodic advertiser. */
  if (LctrMstPerIsSync(lctrDataPdu.pld.perSyncInd.sid, lctrDataPdu.pld.perSyncInd.aType, lctrDataPdu.pld.perSyncInd.advA))
  {
    LL_TRACE_WARN0("    ---- Ignore LL_PERIODIC_SYNC_IND, already in sync");
    return;
  }

  LL_TRACE_INFO1("### LlEvent ###  Periodic sync transfer received, handle=%u", LCTR_GET_CONN_HANDLE(pCtx));
  LL_TRACE_INFO1("                 ceRef=%u", lctrDataPdu.pld.perSyncInd.ceCounter);
  LL_TRACE_INFO1("                 ceRcvd=%u", pCtx->eventCounter);
  LL_TRACE_INFO1("                 lastPECounter=%u", lctrDataPdu.pld.perSyncInd.lastPECounter);
  LL_TRACE_INFO1("                 ID=%u", lctrDataPdu.pld.perSyncInd.id);

  if (pCtx->syncMode != LL_SYNC_TRSF_MODE_OFF)
  {
    if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
    {
      pMsg->hdr.dispId = LCTR_DISP_TRANFER_SYNC;
      pMsg->hdr.event  = LCTR_TRANSFER_SYNC_MSG_START;
      pMsg->id = lctrDataPdu.pld.perSyncInd.id;
      memcpy(pMsg->bSyncInfo, lctrDataPdu.pld.perSyncInd.syncInfo, LL_SYNC_INFO_LEN);
      pMsg->connHandle = LCTR_GET_CONN_HANDLE(pCtx);
      pMsg->ceRef = lctrDataPdu.pld.perSyncInd.ceCounter;
      pMsg->ceRcvd = pCtx->eventCounter;
      pMsg->lastPECounter = lctrDataPdu.pld.perSyncInd.lastPECounter;
      pMsg->advSID = lctrDataPdu.pld.perSyncInd.sid;
      pMsg->advAddrType = lctrDataPdu.pld.perSyncInd.aType;
      pMsg->scaB = lctrDataPdu.pld.perSyncInd.sca;
      pMsg->rxPhy = lctrPhysBitToPhy(lctrDataPdu.pld.perSyncInd.phy);
      pMsg->advAddr = lctrDataPdu.pld.perSyncInd.advA;
      pMsg->syncConnEvtCounter = lctrDataPdu.pld.perSyncInd.syncConnEvtCounter;

      WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
    }
  }
  else
  {
    LL_TRACE_INFO1("    ---- Ignore LL_PERIODIC_SYNC_IND, syncMode = %u", pCtx->syncMode);
  }

  /* The procedure completes without further action. */
  lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_PROC_CMPL);
}
