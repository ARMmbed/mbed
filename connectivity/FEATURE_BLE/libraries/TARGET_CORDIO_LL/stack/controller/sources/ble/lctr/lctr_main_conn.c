/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller data path implementation file.
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

#include "lctr_int_conn.h"
#include "lctr_pdu_conn.h"
#include "lmgr_api_conn.h"
#include "ll_math.h"
#include "ll_api.h"
#include "pal_radio.h"
#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Cipher block initialization handler. */
lctrCipherBlkHdlr_t lctrInitCipherBlkHdlr = NULL;

/*! \brief      Packet encrypt handler. */
lctrPktEncHdlr_t lctrPktEncryptHdlr = NULL;

/*! \brief      Packet decrypt handler. */
lctrPktDecHdlr_t lctrPktDecryptHdlr = NULL;

/*! \brief      Set encrypt packet count handler. */
lctrPktCntHdlr_t lctrSetEncryptPktCountHdlr = NULL;

/*! \brief      Set decrypt packet count handler. */
lctrPktCntHdlr_t lctrSetDecryptPktCountHdlr = NULL;

/*! \brief      Pointer to connection context table. */
lctrConnCtx_t *pLctrConnTbl;

/*! \brief      Common data storage used for decoding a received PDU. */
lctrDataPdu_t lctrDataPdu;

/*! \brief      Common storage for task message data (parameter passing to action routines). */
lctrConnMsg_t *pLctrConnMsg;

/*! \brief      Vendor specific PDU handlers. */
const LctrVsHandlers_t *pLctrVsHdlrs = NULL;

/*! \brief      Control PDU handler. */
lctrCtrlPduHdlr_t lctrCtrlPduHdlr = NULL;

/*! \brief      Build remap table handlers. */
LctrChSelHdlr_t lctrChSelHdlr[LCTR_CH_SEL_MAX] = { 0 };

/*! \brief      Check if CIS is enabled function. */
lctrCheckCisEstCisFn_t  lctrCheckCisEstCisFn = NULL;

/*! \brief      Pointer to lctrSendPerSyncFromScan function. */
lctrLlcpEh_t lctrSendPerSyncFromScanFn = NULL;

/*! \brief      Pointer to lctrSendPerSyncFromBcst function. */
lctrLlcpEh_t lctrSendPerSyncFromBcstFn = NULL;

/*! \brief      Pointer to lctrSendPeriodicSyncInd function. */
lctrLlcpEh_t lctrSendPeriodicSyncIndFn = NULL;

/*! \brief      Pointer to lctrStorePeriodicSyncTrsf function. */
lctrLlcpEh_t lctrStorePeriodicSyncTrsfFn = NULL;

/*! \brief      Pointer to lctrReceivePeriodicSyncInd function. */
lctrLlcpEh_t lctrReceivePeriodicSyncIndFn = NULL;

/*! \brief      Pointer to path lctrPathLossMonitorAct function. */
lctrPcMonAct_t lctrPathLossMonitorActFn = NULL;

/*! \brief      Pointer to lctrSendPowerChangeInd function. */
lctrPcPowInd_t lctrSendPowerChangeIndCback = NULL;

/*! \brief      Pointer to lctrNotifyPowerReportInd function. */
lctrPcNotifyPwr_t lctrNotifyPowerReportIndCback = NULL;

/*! \brief      Pointer to lctrCisServicePowerMonitor Function. */
lctrCisServicePowerMonitor_t lctrCisServicePowerMonitorFn = NULL;

/*! \brief      Last channel class map since host update (Used for slave channel status indications). */
static uint64_t lastChanClassMap = LL_CHAN_DATA_ALL;

/*! \brief      Calculate number of subrated connection events. */
lctrCalcSubrateConnEvents_t lctrCalcSubrateConnEventsFn = NULL;

/*! \brief      Calculate number of subrated connection events. */
lctrCheckLlcpOverride_t lctrSlvCheckEncOverridePhyUpdateFn = NULL;

/*************************************************************************************************/
/*!
 *  \brief      Return PHYs supported by LL.
 *
 *  \return     Bit field of supported PHYs.
 */
/*************************************************************************************************/
static uint8_t llGetSupportedPhys(void)
{
  uint8_t supportPhyBits = LL_PHYS_LE_1M_BIT;

  if ((lmgrCb.features & LL_FEAT_LE_2M_PHY))
  {
    supportPhyBits |= LL_PHYS_LE_2M_BIT;
  }

  if ((lmgrCb.features & LL_FEAT_LE_CODED_PHY))
  {
    supportPhyBits |= LL_PHYS_LE_CODED_BIT;
  }

  return supportPhyBits;
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize the connection memory resources.
 *
 *  \param      pFreeMem    Pointer to free memory.
 *  \param      freeMemSize Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 */
/*************************************************************************************************/
uint16_t LctrInitConnMem(uint8_t *pFreeMem, uint32_t freeMemSize)
{
  uint8_t *pAvailMem = pFreeMem;
  uint16_t memUsed;

  /*** Connection Context ***/

  if (((uint32_t)pAvailMem) & 3)
  {
    /* Align to next word. */
    pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
  }

  /* Allocate memory. */
  LL_TRACE_INFO2("    RAM: %u x %u bytes -- connection context", pLctrRtCfg->maxConn, sizeof(lctrConnCtx_t));
  pLctrConnTbl = (lctrConnCtx_t *)pAvailMem;
  pAvailMem += sizeof(lctrConnCtx_t) * pLctrRtCfg->maxConn;

  if (((uint32_t)(pAvailMem - pFreeMem)) > freeMemSize)
  {
    LL_TRACE_ERR2("LctrInitConnMem: failed to allocate connection context, need=%u available=%u", (pAvailMem - pFreeMem), freeMemSize);
    WSF_ASSERT(FALSE);
    return 0;
  }

  /*** Tx Memory ***/

  freeMemSize -= pAvailMem - pFreeMem;
  memUsed = lctrTxInitMem(pAvailMem, freeMemSize);
  if (memUsed == 0)
  {
    LL_TRACE_ERR2("LctrInitConnMem: failed to allocate descriptors, need=%u available=%u", (pAvailMem - pFreeMem), freeMemSize);
    WSF_ASSERT(FALSE);
    return 0;
  }

  pAvailMem += memUsed;

  lmgrPersistCb.connCtxSize = sizeof(lctrConnCtx_t);

  return (pAvailMem - pFreeMem);
}

/*************************************************************************************************/
/*!
 *  \brief  Assign vendor specific PDU handlers.
 *
 *  \param  pHdlrs      Static definition for VS handlers.
 */
/*************************************************************************************************/
void LctrVsConnInit(const LctrVsHandlers_t *pHdlrs)
{
  pLctrVsHdlrs = pHdlrs;
}

/*************************************************************************************************/
/*!
 *  \brief      Validate connection specification parameter range.
 *
 *  \param      pConnSpec       New connection specification.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LctrValidateConnSpec(const LlConnSpec_t *pConnSpec)
{
  /* Connection interval. */
  if ((LL_API_PARAM_CHECK == 1) &&
      ((pConnSpec->connIntervalMin > pConnSpec->connIntervalMax) ||
       (pConnSpec->connIntervalMax < HCI_CONN_INTERVAL_MIN) ||
       (pConnSpec->connIntervalMin < HCI_CONN_INTERVAL_MIN) ||
       (pConnSpec->connIntervalMax > HCI_CONN_INTERVAL_MAX) ||
       (pConnSpec->connIntervalMin > HCI_CONN_INTERVAL_MAX) ))
  {
    LL_TRACE_WARN0("LctrValidateConnSpec: Connection interval is invalid");
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  /* Connection latency. */
  if ((LL_API_PARAM_CHECK == 1) &&
      ((pConnSpec->connLatency > HCI_CONN_LATENCY_MAX)))
  {
    LL_TRACE_WARN0("LctrValidateConnSpec: connection latency is invalid");
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  /* Supervision timeout. */
  uint32_t supTimeoutMin = ((uint32_t) pConnSpec->connLatency + 1) * LCTR_CONN_IND_US((uint32_t) pConnSpec->connIntervalMax) * 2;
  uint32_t supTimeoutUs = LCTR_SUP_TIMEOUT_VAL_TO_US((uint32_t) pConnSpec->supTimeout);
  if ((LL_API_PARAM_CHECK == 1) &&
      ((supTimeoutUs <= supTimeoutMin) ||
       (pConnSpec->supTimeout < HCI_SUP_TIMEOUT_MIN) ||
       (pConnSpec->supTimeout > HCI_SUP_TIMEOUT_MAX)))
  {
    LL_TRACE_WARN0("LctrValidateConnSpec: supervision timeout is invalid");
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Validate modify sca parameters.
 *
 *  \param      action       Action to take.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LctrValidateModifyScaParam(uint8_t action)
{
  int8_t curSca = (int8_t) lctrComputeSca();

  if (((curSca == LL_SCA_MIN_INDEX) && (action == LL_MODIFY_SCA_LESS_ACCURATE)) ||
      ((curSca == LL_SCA_MAX_INDEX) && (action == LL_MODIFY_SCA_MORE_ACCURATE)))
  {
    return LL_ERROR_CODE_LIMIT_REACHED;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Check whether a procedure is already active or pended.
 *
 *  \param  handle      Connection handle.
 *  \param  event       Event.
 *
 *  \return TRUE if the procedure is already active or pended, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LctrIsProcActPended(uint16_t handle, uint8_t event)
{
  uint8_t proc;
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  if ((proc = lctrGetProcId(event)) == LCTR_PROC_INVALID)
  {
    return FALSE;
  }

  if ((pCtx->llcpActiveProc == proc) ||
      (pCtx->llcpPendMask & (1 << proc)))
  {
    LL_TRACE_WARN1("The new procedure=%u is already active or pended.", pCtx->llcpActiveProc);
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Check whether a feature exchange procedure was host-initiated, or initiated as a result of connection startup.
 *
 *  \param  handle      Connection handle.
 *
 *  \return TRUE if the procedure was host-initiated. FALSE if not.
 */
/*************************************************************************************************/
bool_t LctrIsFeatExchHostInit(uint16_t handle)
{
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  if (!pCtx->enabled)
  {
    /* Should not happen. */
    return FALSE;
  }

  return (pCtx->llcpNotifyMask & (1 << LCTR_PROC_CMN_FEAT_EXCH)) ? TRUE : FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Set controller to notify host when feature exchange completes.
 *
 *  \param  handle      Connection handle.
 *
 */
/*************************************************************************************************/
void LctrSetHostNotifyFeatExch(uint16_t handle)
{

  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  pCtx->llcpNotifyMask |= (1 << LCTR_PROC_CMN_FEAT_EXCH);
}

/*************************************************************************************************/
/*!
 *  \brief      Get encryption mode used in a connection.
 *
 *  \param      handle          Connection handle.
 *  \param      pMode           New encryption mode.
 *
 *  Get the encryption mode used by a connection.
 */
/*************************************************************************************************/
void LctrGetEncMode(uint16_t handle, LlEncMode_t *pMode)
{
  WSF_ASSERT(pMode);

  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  pMode->enaAuth = pCtx->bleData.chan.enc.enaAuth;
}

/*************************************************************************************************/
/*!
 *  \brief      Set encryption mode used in a connection.
 *
 *  \param      handle          Connection handle.
 *  \param      pMode           Current encryption mode.
 *
 *  \return     TRUE if successful, FALSE otherwise.
 *
 *  Set the encryption mode used by a connection. Must be called before encryption is started or
 *  when encryption is paused.
 */
/*************************************************************************************************/
bool_t LctrSetEncMode(uint16_t handle, const LlEncMode_t *pMode)
{
  WSF_ASSERT(pMode);

  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  if (pCtx->pendEncMode)
  {
    /* Mode already pending. */
    return FALSE;
  }

  pCtx->newEncMode = *pMode;
  pCtx->pendEncMode = TRUE;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Set connection operational mode flags.
 *
 *  \param      handle  Connection handle.
 *  \param      flags   Flags.
 *  \param      enable  TRUE to set flags or FALSE to clear flags.
 *
 *  Set mode flags governing LL operations of a given connection.
 */
/*************************************************************************************************/
void LctrSetConnOpFlags(uint16_t handle, uint32_t flags, bool_t enable)
{
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  if (enable)
  {
    pCtx->opModeFlags |= flags;
  }
  else
  {
    pCtx->opModeFlags &= ~flags;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a connection context.
 *
 *  \return Connection context.
 */
/*************************************************************************************************/
lctrConnCtx_t *lctrAllocConnCtx(void)
{
  unsigned int connIdx;

  for (connIdx = 0; connIdx < pLctrRtCfg->maxConn; connIdx++)
  {
    if (!pLctrConnTbl[connIdx].enabled)
    {
      lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(connIdx);

      memset(&pLctrConnTbl[connIdx], 0, sizeof(lctrConnCtx_t));
      pCtx->enabled = TRUE;

      pCtx->opModeFlags = lmgrCb.opModeFlags;

      /* Assume all features are supported; if not supported remote sends unknown response. */
      pCtx->usedFeatSet = lmgrCb.features;

      /* Setup supervision timer. */
      pCtx->tmrSupTimeout.handlerId = lmgrPersistCb.handlerId;
      lctrMsgHdr_t *pMsg = (lctrMsgHdr_t *)&pCtx->tmrSupTimeout.msg;
      pMsg->handle = connIdx;
      pMsg->dispId = LCTR_DISP_CONN;
      pMsg->event = LCTR_CONN_TERM_SUP_TIMEOUT;

      /* Setup LLCP timer. */
      pCtx->tmrProcRsp.handlerId = lmgrPersistCb.handlerId;
      pMsg = (lctrMsgHdr_t *)&pCtx->tmrProcRsp.msg;
      pMsg->handle = connIdx;
      pMsg->dispId = LCTR_DISP_CONN;
      pMsg->event = LCTR_CONN_TMR_LLCP_RSP_EXP;

      /* LE Ping period timer. */
      pCtx->tmrPingTimeout.handlerId = lmgrPersistCb.handlerId;
      pMsg = (lctrMsgHdr_t *)&pCtx->tmrPingTimeout.msg;
      pMsg->handle = connIdx;
      pMsg->dispId = LCTR_DISP_CONN;
      pMsg->event = LCTR_CONN_TMR_PING_PERIOD_EXP;

      /* Setup authentication payload timer. */
      pCtx->tmrAuthTimeout.handlerId = lmgrPersistCb.handlerId;
      pMsg = (lctrMsgHdr_t *)&pCtx->tmrAuthTimeout.msg;
      pMsg->handle = connIdx;
      pMsg->dispId = LCTR_DISP_CONN;
      pMsg->event = LCTR_CONN_TMR_AUTH_PAYLOAD_EXP;

      /* Setup power control timer. */
      pCtx->tmrPowerCtrl.handlerId = lmgrPersistCb.handlerId;
      pMsg = (lctrMsgHdr_t *)&pCtx->tmrPowerCtrl.msg;
      pMsg->handle = connIdx;
      pMsg->dispId = LCTR_DISP_CONN;
      pMsg->event = LCTR_CONN_LLCP_PWR_CTRL_SERVICE;

      /* Default packet lengths. */
      pCtx->localDataPdu.maxTxLen = lmgrConnCb.maxTxLen;
      pCtx->localDataPdu.maxRxLen = WSF_MIN(LCTR_MAX_DATA_LEN_MAX, pLctrRtCfg->maxAclLen);
      pCtx->localDataPdu.maxTxTime = lmgrConnCb.maxTxTime;
      /* Limit with absolute time. lctrSendDataLengthPdu() limits time by PHY capability. */
      pCtx->localDataPdu.maxRxTime = LL_DATA_LEN_TO_TIME_CODED_S8(pCtx->localDataPdu.maxRxLen, TRUE);
      pCtx->effDataPdu.maxTxLen = LL_MAX_DATA_LEN_MIN;
      pCtx->effDataPdu.maxRxLen = LL_MAX_DATA_LEN_MIN;
      pCtx->effDataPdu.maxTxTime = LL_MAX_DATA_TIME_MIN;
      pCtx->effDataPdu.maxRxTime = LL_MAX_DATA_TIME_MIN;

      /* Connection event duration (update once PHY is known). */
      pCtx->bleData.chan.txPhy = pCtx->bleData.chan.rxPhy = BB_PHY_BLE_1M;
      pCtx->effConnDurUsec = lctrCalcConnDurationUsec(pCtx, &pCtx->effDataPdu);
      pCtx->localConnDurUsec = lctrCalcConnDurationUsec(pCtx, &pCtx->localDataPdu);

      /* Default PHY. */
      pCtx->allPhys = lmgrConnCb.allPhys;
      pCtx->txPhys = lmgrConnCb.txPhys;
      pCtx->rxPhys = lmgrConnCb.rxPhys;

      /* SCA initialization. */
      pCtx->scaMod = lmgrCb.scaMod;

      /* Default settings for PAST(periodic advertising sync transfer). */
      pCtx->syncMode = lmgrConnCb.syncMode;
      pCtx->syncSkip = lmgrConnCb.syncSkip;
      pCtx->syncTimeout = lmgrConnCb.syncTimeout;

      /* Set default minimum number of used channels. */
      for (unsigned i = 0; i < LL_MAX_PHYS; i++)
      {
        pCtx->peerMinUsedChan[i] = LL_MIN_NUM_CHAN_DATA;
      }

      /* VS initialization. */
      if (pLctrVsHdlrs && pLctrVsHdlrs->connSetup)
      {
        pLctrVsHdlrs->connSetup(connIdx);
      }

      /* Power control initialization. */
      if ((pCtx->usedFeatSet & LL_FEAT_POWER_CONTROL_REQUEST) &&
          !lmgrGetOpFlag(LL_OP_MODE_FLAG_DIS_POWER_MONITOR))
      {
        pCtx->powerMonitorScheme = LCTR_PC_MONITOR_AUTO;
        pCtx->monitoringState    = LCTR_PC_MONITOR_ENABLED;
        pCtx->pclMonitorParam.autoMonitor.highThreshold = pLctrRtCfg->pcHighThreshold;
        pCtx->pclMonitorParam.autoMonitor.lowThreshold = pLctrRtCfg->pcLowThreshold;
        pCtx->pclMonitorParam.autoMonitor.requestVal = LL_PC_REQ_CHANGE_DBM;

        /* Reset average counters. */
        pCtx->pclMonitorParam.autoMonitor.accumulatedRssi = 0;
        pCtx->pclMonitorParam.autoMonitor.totalAccumulatedRssi = 0;
        pCtx->pclMonitorParam.autoMonitor.rssiRunAvg.avgCount = 0;
        pCtx->cisAccumulatedRssi = 0;
        pCtx->cisTotalAccumulatedRssi = 0;
        pCtx->cisRunAvg.avgCount = 0;
      }

      /* Default Enhanced Connection Update. */
      pCtx->ecu.srFactor = 1;
      /* pCtx->ecu.contNum = 0; */
      pCtx->ecu.defSrMin = lmgrConnCb.defSrMin;
      pCtx->ecu.defSrMax = lmgrConnCb.defSrMax;
      pCtx->ecu.defMaxLatency = lmgrConnCb.defMaxLatency;
      pCtx->ecu.defContNum = lmgrConnCb.defContNum;
      pCtx->ecu.defSvt = lmgrConnCb.defSvt;

      LmgrIncResetRefCount();
      lmgrCb.numConnEnabled++;

      /* Enable BB. */
      BbStart(BB_PROT_BLE);

      return LCTR_GET_CONN_CTX(connIdx);
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Free a connection context.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrFreeConnCtx(lctrConnCtx_t *pCtx)
{
  uint8_t *pBuf;
  uint8_t numRxBufs;
  uint8_t numTxBufs;
  wsfHandlerId_t handlerId;

  /* Flush remaining receive packets. */
  numRxBufs = lctrRxConnClear(pCtx);
  lctrDataRxIncAvailBuf(numRxBufs);

  /* Flush remaining transmit packets. */
  numTxBufs = lctrTxQueueClear(pCtx);

  /* Flush remaining transmit packets. */
  while ((pBuf = WsfMsgDeq(&pCtx->txLeuQ, &handlerId)) != NULL)
  {
    if (handlerId != LCTR_CTRL_DATA_HANDLE)
    {
      lctrDataTxIncAvailBuf();
      numTxBufs++;
    }

    WsfMsgFree(pBuf);
  }

  /* Cleanup timers. */
  WsfTimerStop(&pCtx->tmrSupTimeout);
  WsfTimerStop(&pCtx->tmrProcRsp);
  WsfTimerStop(&pCtx->tmrPingTimeout);
  WsfTimerStop(&pCtx->tmrAuthTimeout);
  WsfTimerStop(&pCtx->tmrPowerCtrl);

  uint16_t handle = LCTR_GET_CONN_HANDLE(pCtx);

  /* VS cleanup. */
  if (pLctrVsHdlrs && pLctrVsHdlrs->connCleanup)
  {
    pLctrVsHdlrs->connCleanup(handle);
  }

  pCtx->enabled = FALSE;

  LmgrDecResetRefCount();
  lmgrCb.numConnEnabled--;

  /* Shutdown BB. */
  BbStop(BB_PROT_BLE);

  LL_TRACE_INFO1("    >>> Connection terminated, handle=%u <<<", handle);
}

/*************************************************************************************************/
/*!
 *  \brief  Compute next data channel index.
 *
 *  \param  pCtx    Connection context.
 *  \param  numSkip Number of channels to skip.
 *
 *  \return Next data channel index.
 */
/*************************************************************************************************/
uint8_t lctrSelectNextDataChannel(lctrConnCtx_t *pCtx, uint16_t numSkip)
{
  /* unmappedChannel = (lastUnmappedChannel + hopIncrement) mod 37 */
  uint16_t unmapChan = pCtx->lastChanIdx + ((uint16_t)pCtx->hopInc * (numSkip + 1));
  if (unmapChan > LL_CHAN_DATA_MAX_IDX)
  {
    unmapChan = LL_MATH_MOD_37(unmapChan);
  }

  pCtx->lastChanIdx = unmapChan;

  if (!((UINT64_C(1) << unmapChan) & pCtx->chanMask))
  {
    /* remappingIndex = unmappedChannel mod numUsedChannels */

    uint8_t remapIdx;

    /* Avoid division. */
    if (unmapChan < pCtx->numUsedChan)
    {
      remapIdx = unmapChan;
    }
    else if (pCtx->numUsedChan > 18)    /* max is 37; no double rollovers */
    {
      remapIdx = unmapChan - pCtx->numUsedChan;
    }
    else
    {
      /* Unlikely case where available channels <= 18, loop is required. */
      remapIdx = unmapChan;
      while (remapIdx >= pCtx->numUsedChan)
      {
        remapIdx -= pCtx->numUsedChan;
      }
    }

    return pCtx->chanRemapTbl[remapIdx];
  }

  return unmapChan;
}

/*************************************************************************************************/
/*!
 *  \brief  Build channel remapping table.
 *
 *  \param  pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrBuildRemapTable(lctrConnCtx_t *pCtx)
{
  unsigned int chanIdx;
  unsigned int numUsedChan = 0;

  for (chanIdx = 0; chanIdx < sizeof(pCtx->chanRemapTbl); chanIdx++)
  {
    if (pCtx->chanMask & (UINT64_C(1) << chanIdx))
    {
      pCtx->chanRemapTbl[numUsedChan++] = chanIdx;
    }
  }

  WSF_ASSERT(numUsedChan);          /* must have at least one channel */

  pCtx->numUsedChan = numUsedChan;
}

/*************************************************************************************************/
/*!
 *  \brief      Tx data completed task event handler.
 */
/*************************************************************************************************/
void lctrConnTxCompletedHandler(void)
{
  WSF_CS_INIT(cs);
  uint8_t numBufs[LL_MAX_CONN] = { 0 };
  unsigned int i = pLctrRtCfg->maxConn - 1;

  /* Cache buffer count within single CS. */
  WSF_CS_ENTER(cs);
  do
  {
    lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(i);

    if (pCtx->enabled)
    {
      numBufs[i] = pCtx->numTxComp;
      pCtx->numTxComp = 0;
    }
  } while (i--);
  WSF_CS_EXIT(cs);

  /* Call completion callbacks. */
  i = pLctrRtCfg->maxConn - 1;
  do
  {
    if (numBufs[i])
    {
      if (lmgrPersistCb.sendCompCback)
      {
        lmgrPersistCb.sendCompCback(i, numBufs[i]);
      }
    }

    lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(i);

    if (pCtx->pauseTxData &&
        WsfQueueEmpty(&pCtx->txArqQ))
    {
      lctrSendConnMsg(pCtx, LCTR_CONN_ARQ_Q_FLUSHED);
    }
  } while (i--);
}

/*************************************************************************************************/
/*!
 *  \brief      Rx data pending task event handler.
 */
/*************************************************************************************************/
void lctrConnRxPendingHandler(void)
{
  uint16_t connHandle = 0;
  uint8_t *pRxBuf;

  /* Route and demux received Data PDUs. */

  while ((pRxBuf = lctrRxDeq(&connHandle)) != NULL)
  {
    WSF_ASSERT(pRxBuf);

    WSF_ASSERT(connHandle < pLctrRtCfg->maxConn);
    lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(connHandle);

    if (!pCtx->enabled)
    {
      LL_TRACE_ERR1("!!! Data received on terminated connHandle=%u", connHandle);
      lctrRxPduFree(pRxBuf);
      lctrDataRxIncAvailBuf(1);
      continue;
    }

    /* Disassemble PDU. */
    lctrDataPduHdr_t rxHdr;
    lctrUnpackDataPduHdr(&rxHdr, pRxBuf);

    /* Decrypt PDU. */
    if (pCtx->bleData.chan.enc.enaDecrypt)
    {
      if (lctrPktDecryptHdlr &&
          (!lctrPktDecryptHdlr(&pCtx->bleData.chan.enc, pRxBuf)))
      {
        LL_TRACE_ERR1("!!! MIC verification failed on connHandle=%u", connHandle);
        lctrRxPduFree(pRxBuf);
        lctrDataRxIncAvailBuf(1);
        lctrSendConnMsg(pCtx, LCTR_CONN_TERM_MIC_FAILED);
        continue;
      }

      /* Restart authentication timers. */
      WsfTimerStartMs(&pCtx->tmrAuthTimeout, pCtx->authTimeoutMs);
      WsfTimerStartMs(&pCtx->tmrPingTimeout, pCtx->pingPeriodMs);
    }

    /* Demux PDU. */
    switch (rxHdr.llid)
    {
      case LL_LLID_CTRL_PDU:
      {
#if (LL_ENABLE_TESTER)
        if (llTesterCb.llcpForwardEnabled)
        {
          if (!llTesterCb.llcpLlcpIntercept)
          {
            if (lctrCtrlPduHdlr != NULL)
            {
              lctrCtrlPduHdlr(pCtx, pRxBuf);
            }
          }
          pRxBuf[0] = LL_LLID_VS_PDU;     /* buffer is treated like a VS buffer */
          lctrRxConnEnq(pCtx, pRxBuf);
          /* lctrRxPduFree(pRxBuf); */        /* Freed in rxDataQ handler. */
          /* lctrDataRxIncAvailBuf(); */      /* Incremented in rxDataQ handler. */
        }
        else
#endif
        {
          if (lctrCtrlPduHdlr != NULL)
          {
            lctrCtrlPduHdlr(pCtx, pRxBuf);
          }
          lctrRxPduFree(pRxBuf);
          lctrDataRxIncAvailBuf(1);
        }
        break;
      }

      case LL_LLID_START_PDU:
      case LL_LLID_CONT_PDU:
        if (pCtx->pauseRxData)
        {
          LL_TRACE_ERR1("!!! Data PDU received during data pause, connHandle=%u", connHandle);
          lctrRxPduFree(pRxBuf);
          lctrDataRxIncAvailBuf(1);
          lctrSendConnMsg(pCtx, LCTR_CONN_TERM_MIC_FAILED);
        }
        else
        {
          lctrRxConnEnq(pCtx, pRxBuf);
          /* lctrRxPduFree(pRxBuf); */        /* Freed in rxDataQ handler. */
          /* lctrDataRxIncAvailBuf(); */      /* Incremented in rxDataQ handler. */
        }
        break;

      case LL_LLID_VS_PDU:
      default:
        if (pLctrVsHdlrs && pLctrVsHdlrs->dataRecv)
        {
          pLctrVsHdlrs->dataRecv(connHandle, pRxBuf);
        }
        else
        {
          lctrRxPduFree(pRxBuf);
          lctrDataRxIncAvailBuf(1);
          LL_TRACE_ERR1("!!! Invalid LLID; dropping Rx data PDU, connHandle=%u", connHandle);
        }
        break;
    }
  }

  /* Notify host of pending Rx data. */

  WSF_CS_INIT(cs);
  uint8_t numBufs[LL_MAX_CONN];
  unsigned int i = pLctrRtCfg->maxConn - 1;

  /* Cache buffer count within single CS. */
  WSF_CS_ENTER(cs);
  do
  {
    numBufs[i] = pLctrConnTbl[i].numRxPend;
    pLctrConnTbl[i].numRxPend = 0;
  } while (i--);
  WSF_CS_EXIT(cs);

  /* Call completion callbacks. */
  i = pLctrRtCfg->maxConn - 1;
  do
  {
    if (numBufs[i] && lmgrPersistCb.recvPendCback)
    {
      lmgrPersistCb.recvPendCback(i, numBufs[i]);
    }
  } while (i--);
}

/*************************************************************************************************/
/*!
 *  \brief  Transmit ACL data path.
 *
 *  \param  pAclBuf     ACL buffer.
 */
/*************************************************************************************************/
void LctrTxAcl(uint8_t *pAclBuf)
{
  lctrAclHdr_t aclHdr;

  /*** Disassemble ACL packet. ***/

  lctrUnpackAclHdr(&aclHdr, pAclBuf);

  /*** Resolve Connection context. ***/

  if (aclHdr.connHandle >= pLctrRtCfg->maxConn)
  {
    LL_TRACE_ERR1("Invalid ACL header: connHandle=%u; dropping packet", aclHdr.connHandle);
    WsfMsgFree(pAclBuf);
    lmgrPersistCb.sendCompCback(aclHdr.connHandle, 1);
    return;
  }

  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(aclHdr.connHandle);

  if (!pCtx->enabled)
  {
    LL_TRACE_ERR1("Invalid ACL header: connection disconnected for connHandle=%u; dropping packet", aclHdr.connHandle);
    /* LL shall not inform host of the number of completed event */
    WsfMsgFree(pAclBuf);
    return;
  }

  if (pCtx->state == LCTR_CONN_STATE_TERMINATING)
  {
    LL_TRACE_ERR1("Invalid ACL header: connection terminated for connHandle=%u; dropping packet", aclHdr.connHandle);
    WsfMsgFree(pAclBuf);
    lmgrPersistCb.sendCompCback(aclHdr.connHandle, 1);
    return;
  }

  if (!pLctrVsHdlrs && (aclHdr.pktBound == LCTR_PB_VS_DATA))
  {
    LL_TRACE_ERR1("Invalid ACL header: pktBound=%u not valid; dropping packet", aclHdr.pktBound);
    WsfMsgFree(pAclBuf);
    lmgrPersistCb.sendCompCback(aclHdr.connHandle, 1);
    return;
  }

  if (((aclHdr.pktBound != LCTR_PB_VS_DATA) && (aclHdr.len == 0)) ||
      (aclHdr.len > pLctrRtCfg->maxAclLen))
  {
    LL_TRACE_ERR2("Invalid ACL header: invalid packet length, actLen=%u, maxLen=%u", aclHdr.len, pCtx->effDataPdu.maxTxLen);
    if ((aclHdr.pktBound == LCTR_PB_START_NON_AUTO_FLUSH) && (aclHdr.len == 0))
    {
      pCtx->forceStartPdu = TRUE; /* If this was supposed to be the start fragment, make the next packet a start fragment. */
      LL_TRACE_INFO0("Next ACL header will be forced to a start fragment");
    }
    WsfMsgFree(pAclBuf);
    lmgrPersistCb.sendCompCback(aclHdr.connHandle, 1);
    return;
  }

  lctrDataTxDecAvailBuf();

  if (!pCtx->pauseTxData)
  {
    WSF_ASSERT(WsfQueueEmpty(&pCtx->txLeuQ));

    /*** Queue for transmit. ***/

    uint16_t fragLen = lctrTxFragLen(pCtx);
    lctrTxDataPduQueue(pCtx, fragLen, &aclHdr, pAclBuf);
    WsfSetEvent(lmgrPersistCb.handlerId, (1 << LCTR_EVENT_TX_PENDING));
  }
  else
  {
    /*** Pend in LE-U pause queue. ***/

    WsfMsgEnq(&pCtx->txLeuQ, aclHdr.connHandle, pAclBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Receive ACL data path.
 *
 *  \return Received data PDU buffer or NULL if queue empty.
 */
/*************************************************************************************************/
uint8_t *LctrRxAcl(void)
{
  unsigned int i = pLctrRtCfg->maxConn - 1;
  uint8_t *pAclBuf = NULL;

  do
  {
    if ((pLctrConnTbl[i].enabled) &&
        ((pAclBuf = lctrRxConnDeqAcl(&pLctrConnTbl[i])) != NULL))
    {
      break;
    }

  } while (i--);

  return pAclBuf;
}

/*************************************************************************************************/
/*!
 *  \brief      Indicate that received ACL data buffer has been deallocated
 *
 *  \param      numBufs     Number of completed packets.
 *
 *  Indicate that received ACL data buffer has been deallocated.
 */
/*************************************************************************************************/
void LctrRxAclComplete(uint8_t numBufs)
{
  lctrDataRxIncAvailBuf(numBufs);
}

/*************************************************************************************************/
/*!
 *  \brief  Get enable state of a handle.
 *
 *  \param  handle      Connection handle.
 *
 *  \return TRUE if enabled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LctrIsConnHandleEnabled(uint16_t handle)
{
  return pLctrConnTbl[handle].enabled;
}

/*************************************************************************************************/
/*!
 *  \brief  Get enable state of a handle.
 *
 *  \param  handle      Connection handle.
 *
 *  \return TRUE if enabled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LctrIsCisConnHandleEnabled(uint16_t handle)
{
  if (lctrCheckCisEstCisFn)
  {
    return lctrCheckCisEstCisFn(handle);
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Get role of a connection.
 *
 *  \param  handle      Connection handle.
 *
 *  \return Role.
 */
/*************************************************************************************************/
uint8_t LctrGetRole(uint16_t handle)
{
  return pLctrConnTbl[handle].role;
}

/*************************************************************************************************/
/*!
 *  \brief  Get RSSI value of a connection.
 *
 *  \param  handle      Connection handle.
 *
 *  \return RSSI value.
 */
/*************************************************************************************************/
int8_t LctrGetRssi(uint16_t handle)
{
  return pLctrConnTbl[handle].rssi;
}

/*************************************************************************************************/
/*!
 *  \brief      Get connection's TX power level for the selected phy.
 *
 *  \param      handle          Connection handle.
 *  \param      phy             PHY.
 *
 *  \return     Transmit power level.
 */
/*************************************************************************************************/
int8_t LctrGetPhyTxPowerLevel(uint16_t handle, uint8_t phy)
{
  lctrConnCtx_t * pCtx = LCTR_GET_CONN_CTX(handle);
  uint8_t option = BB_PHY_OPTIONS_BLE_S8;

  if (phy == LL_PC_PHY_CODED_S2)
  {
    phy = LL_PHY_LE_CODED;
    option = BB_PHY_OPTIONS_BLE_S2;
  }

  int8_t txPower = LCTR_GET_TXPOWER(pCtx, phy, option);
  return (txPower == LL_PWR_CTRL_TXPOWER_UNMANAGED) ? 0 : txPower;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the transmit power level of a connection.
 *
 *  \param  handle      Connection handle.
 *
 *  \return Transmit power level.
 */
/*************************************************************************************************/
int8_t LctrGetTxPowerLevel(uint16_t handle)
{
  return pLctrConnTbl[handle].bleData.chan.txPower;
}

/*************************************************************************************************/
/*!
 *  \brief  Set the transmit power level of a connection (all PHYs).
 *
 *  \param  handle      Connection handle.
 *  \param  level       Transmit power level.
 */
/*************************************************************************************************/
void LctrSetTxPowerLevel(uint16_t handle, int8_t level)
{
  lctrConnCtx_t * pCtx = LCTR_GET_CONN_CTX(handle);
  BbBleData_t * pBle = &pCtx->bleData;
  int8_t txPhyPwr = pBle->chan.txPower;
  int8_t txPwrOld[LL_PC_PHY_TOTAL];
  memcpy(txPwrOld, pCtx->phyTxPower, LL_PC_PHY_TOTAL);
  int8_t adjustedLevel = PalRadioGetActualTxPower(level, FALSE);

  if (adjustedLevel != level)
  {
    LL_TRACE_WARN2("Transmit Power set to %d instead of %d, due to hardware limitations.", adjustedLevel, level);
  }

  pBle->chan.txPower = adjustedLevel;
  memset(pCtx->phyTxPower, adjustedLevel, LL_PC_PHY_TOTAL);

  /* If supported, notify peer of power change on currently transmitting PHY. */
  if ((pCtx->state == LCTR_CONN_STATE_ESTABLISHED_READY) &&
      (txPhyPwr != adjustedLevel) &&
      (pCtx->usedFeatSet & LL_FEAT_POWER_CHANGE_IND) &&
      lctrSendPowerChangeIndCback)
  {
    lctrSendPowerChangeIndCback(pCtx, pBle->chan.txPhy, adjustedLevel - txPhyPwr, adjustedLevel, FALSE);
  }

  /* If enabled, notify host of power change on affected PHYs. */
  if (pCtx->powerRptLocal)
  {
    int phy;
    for (phy = 0; phy < LL_PC_PHY_TOTAL; phy++)
    {
      if (txPwrOld[phy] != adjustedLevel)
      {
        if (lctrNotifyPowerReportIndCback)
        {
          lctrNotifyPowerReportIndCback(pCtx, LL_SUCCESS, LL_POWER_REPORT_REASON_LOCAL, phy, adjustedLevel,
                                    lctrGetPowerLimits(adjustedLevel),
                                    adjustedLevel - txPwrOld[phy]);
        }
      }
    }
  }

}

/*************************************************************************************************/
/*!
 *  \brief  Set the transmit power level of a connection.
 *
 *  \param  handle      Connection handle.
 *  \param  level       Transmit power level.
 *  \param  phy         PHY to set power level for.
 */
/*************************************************************************************************/
void LctrSetPhyTxPowerLevel(uint16_t handle, int8_t level, uint8_t phy)
{
  lctrConnCtx_t * pCtx = LCTR_GET_CONN_CTX(handle);
  BbBleData_t * pBle = &pCtx->bleData;
  int8_t txPwrOld = pBle->chan.txPower;
  int8_t adjustedLevel;
  int8_t delta;

  /* Not specifying a PHY will set the current transmitting PHY. */
  if (phy == LL_PHY_NONE)
  {
    phy = pBle->chan.txPhy;
  }

  adjustedLevel = PalRadioGetActualTxPower(level, FALSE);

  if (adjustedLevel != level)
  {
    LL_TRACE_WARN2("Transmit Power set to %d instead of %d, due to hardware limitations.", adjustedLevel, level);
  }

  delta = adjustedLevel - txPwrOld;
  if (delta == 0)
  {
    return;
  }

  if (phy == pBle->chan.txPhy)
  {
    pBle->chan.txPower = adjustedLevel;

    /* If supported, notify peer of power change. */
    if ((pCtx->state == LCTR_CONN_STATE_ESTABLISHED_READY) &&
        (txPwrOld != adjustedLevel) &&
        (pCtx->usedFeatSet & LL_FEAT_POWER_CHANGE_IND) &&
        lctrSendPowerChangeIndCback)
    {
      lctrSendPowerChangeIndCback(pCtx, pBle->chan.txPhy, adjustedLevel - txPwrOld, adjustedLevel, FALSE);
    }
  }

  LCTR_SET_TXPOWER(pCtx, phy, adjustedLevel);


  /* If enabled, notify host of power change. */
  if (pCtx->powerRptLocal)
  {
    if (lctrNotifyPowerReportIndCback)
    {
      lctrNotifyPowerReportIndCback(pCtx, LL_SUCCESS, LL_POWER_REPORT_REASON_LOCAL, pBle->chan.txPhy, adjustedLevel,
                                lctrGetPowerLimits(pBle->chan.txPower),
                                delta);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Get the channel map mask of a connection.
 *
 *  \param  handle      Connection handle.
 *
 *  \return Channel map mask.
 */
/*************************************************************************************************/
uint64_t LctrGetChannelMap(uint16_t handle)
{
  return pLctrConnTbl[handle].chanMask;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the used feature set of a connection.
 *
 *  \param  handle      Connection handle.
 *
 *  \return Used feature set bitmask.
 */
/*************************************************************************************************/
uint64_t LctrGetUsedFeatures(uint16_t handle)
{
  return pLctrConnTbl[handle].usedFeatSet;
}

/*************************************************************************************************/
/*!
 *  \brief      Get transmitter PHY used by a connection.
 *
 *  \param      handle          Connection handle.
 *
 *  \return     Transmitter PHY.
 *
 *  Get transmitter PHY used by a connection.
 */
/*************************************************************************************************/
uint8_t LctrGetTxPhy(uint16_t handle)
{
  return pLctrConnTbl[handle].bleData.chan.txPhy;
}

/*************************************************************************************************/
/*!
 *  \brief      Get receiver PHY used by a connection.
 *
 *  \param      handle          Connection handle.
 *
 *  \return     Receiver PHY.
 *
 *  Get receiver PHY used by a connection.
 */
/*************************************************************************************************/
uint8_t LctrGetRxPhy(uint16_t handle)
{
  return pLctrConnTbl[handle].bleData.chan.rxPhy;
}

/*************************************************************************************************/
/*!
 *  \brief      Get status of CIS termination procedure.
 *
 *  \param      handle          Connection handle.
 *
 *  \return     TRUE if termination is currently pending, FALSE if not.
 */
/*************************************************************************************************/
bool_t LctrCisTerminationInProgress(uint16_t handle)
{
  WSF_ASSERT(handle < pLctrRtCfg->maxConn);

  lctrConnCtx_t *pCtx = &pLctrConnTbl[handle];

  if ((pCtx->llcpActiveProc == LCTR_PROC_CIS_TERM) ||
      (pCtx->llcpPendMask & (1 << LCTR_PROC_CIS_TERM)))
  {
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Get peer minimum number of used channels.
 *
 *  \param      handle          Connection handle.
 *  \param      pPeerMinCh      Peer minimum used channels.
 *
 *  \return     Peer minimum number of used channels.
 *
 *  Get peer minimum number of used channels.
 */
/*************************************************************************************************/
void LctrGetPeerMinUsedChan(uint16_t handle, uint8_t *pPeerMinCh)
{
  for (unsigned i = 0; i < LL_MAX_PHYS; i++)
  {
    pPeerMinCh[i] = pLctrConnTbl[handle].peerMinUsedChan[i];
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Is current a connection context waiting for a host reply?
 *
 *  \param  handle      Connection handle.
 *  \param  reply       Reply bitmask.
 *
 *  \return TRUE if current connection context is waiting for host reply, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LctrIsWaitingForReply(uint16_t handle, uint8_t reply)
{
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  return (pCtx->replyWaitingMsk & reply) ? TRUE : FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal connection subsystem message.
 *
 *  \param  pCtx        Connection context.
 *  \param  event       Connection event.
 */
/*************************************************************************************************/
void lctrSendConnMsg(lctrConnCtx_t *pCtx, uint8_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t))) != NULL)
  {
    pMsg->handle = LCTR_GET_CONN_HANDLE(pCtx);
    pMsg->dispId = LCTR_DISP_CONN;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Compute the LE Ping period value.
 *
 *  \param  pCtx            Connection context.
 *  \param  authTimeoutMs   Authentication timeout value in milliseconds.
 *
 *  \return LE Ping period in milliseconds.
 */
/*************************************************************************************************/
uint32_t lctrCalcPingPeriodMs(lctrConnCtx_t *pCtx, uint32_t authTimeoutMs)
{
  /* LE Ping attempts before the expiration of authentication payload timeout. */
  uint32_t pingAttemptsMs = LCTR_CONN_IND_MS((LCTR_LE_PING_ATTEMPTS + pCtx->maxLatency) * pCtx->connInterval);
  uint32_t pingPeriodMs   = authTimeoutMs; /* fallback */

  if ((authTimeoutMs > pingAttemptsMs) &&  /* Timeout must be longer than time to attempt ping */
      (authTimeoutMs > WSF_MS_PER_TICK))   /*             and longer than one tick.            */
  {
    pingPeriodMs = authTimeoutMs - pingAttemptsMs;

    if (pingPeriodMs < WSF_MS_PER_TICK)   /* minimum */
    {
      /* At least 1 timer tick. */
      pingPeriodMs = WSF_MS_PER_TICK;
    }
  }

  return pingPeriodMs;
}

/*************************************************************************************************/
/*!
 *  \brief  Connection conflict resolution handler.
 *
 *  \param  pNewOp      New BOD.
 *  \param  pExistOp    Existing BOD.
 *
 *  \return Prioritized BOD.
 *
 *  Prioritize BLE connection at most risk for termination.
 */
/*************************************************************************************************/
BbOpDesc_t *lctrConnResolveConflict(BbOpDesc_t *pNewOp, BbOpDesc_t *pExistOp)
{
  lctrConnCtx_t *pNewCtx = pNewOp->pCtx;
  lctrConnCtx_t *pExistCtx = pExistOp->pCtx;

  WSF_ASSERT(pNewOp->protId == BB_PROT_BLE);
  WSF_ASSERT((pNewOp->prot.pBle->chan.opType == BB_BLE_OP_SLV_CONN_EVENT) ||
             (pNewOp->prot.pBle->chan.opType == BB_BLE_OP_MST_CONN_EVENT));

  /* BLE connections. */
  if ((pExistOp->protId != BB_PROT_BLE) ||
      !((pExistOp->prot.pBle->chan.opType == BB_BLE_OP_SLV_CONN_EVENT) ||
        (pExistOp->prot.pBle->chan.opType == BB_BLE_OP_MST_CONN_EVENT)))
  {
    LL_TRACE_WARN1("!!! Scheduling conflict, BLE connections: incoming handle=%u prioritized over non-connection operation", LCTR_GET_CONN_HANDLE(pNewCtx));
    return pNewOp;
  }

  /* Established connections. */
  if (pNewCtx->state == LCTR_CONN_STATE_INITIALIZED)
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, established connection: existing handle=%u prioritized over incoming handle=%u", LCTR_GET_CONN_HANDLE(pExistCtx), LCTR_GET_CONN_HANDLE(pNewCtx));
    return pExistOp;
  }

  /* Supervision timeout is imminent (2 CE). */
  if ((pExistCtx->svtState > pNewCtx->svtState) ||
      ((pExistCtx->tmrSupTimeout.ticks * WSF_MS_PER_TICK * 1000) < (uint32_t)(LCTR_CONN_IND_US(pExistCtx->connInterval) << 1)))
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, imminent SVT: existing handle=%u prioritized over incoming handle=%u", LCTR_GET_CONN_HANDLE(pExistCtx), LCTR_GET_CONN_HANDLE(pNewCtx));
    return pExistOp;
  }

  if ((pNewCtx->svtState != LCTR_SVT_STATE_IDLE) ||
      ((pNewCtx->tmrSupTimeout.ticks * WSF_MS_PER_TICK * 1000) < (uint32_t)(LCTR_CONN_IND_US(pNewCtx->connInterval) << 1)))
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, imminent SVT: incoming handle=%u prioritized over existing handle=%u", LCTR_GET_CONN_HANDLE(pNewCtx), LCTR_GET_CONN_HANDLE(pExistCtx));
    return pNewOp;
  }

  /* Active LLCP is pending. */
  if (pExistCtx->llcpActiveProc != LCTR_PROC_INVALID)
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, LLCP pending: existing handle=%u prioritized over incoming handle=%u", LCTR_GET_CONN_HANDLE(pExistCtx), LCTR_GET_CONN_HANDLE(pNewCtx));
    return pExistOp;
  }
  if (pNewCtx->llcpActiveProc != LCTR_PROC_INVALID)
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, LLCP pending: incoming handle=%u prioritized over existing handle=%u", LCTR_GET_CONN_HANDLE(pNewCtx), LCTR_GET_CONN_HANDLE(pExistCtx));
    return pNewOp;
  }

  /* Data is pending. */
  if (!WsfQueueEmpty(&pExistCtx->txArqQ))
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, data pending: existing handle=%u prioritized over incoming handle=%u", LCTR_GET_CONN_HANDLE(pExistCtx), LCTR_GET_CONN_HANDLE(pNewCtx));
    return pExistOp;
  }
  if (!WsfQueueEmpty(&pNewCtx->txArqQ))
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, data pending: incoming handle=%u prioritized over existing handle=%u", LCTR_GET_CONN_HANDLE(pNewCtx), LCTR_GET_CONN_HANDLE(pExistCtx));
    return pNewOp;
  }

  /* Less frequent connInterval (4x). */
  if ((pExistCtx->connInterval >> 2) > pNewCtx->connInterval)
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, CI frequency: existing handle=%u prioritized over incoming handle=%u", LCTR_GET_CONN_HANDLE(pExistCtx), LCTR_GET_CONN_HANDLE(pNewCtx));
    return pExistOp;
  }
  if ((pNewCtx->connInterval >> 2) > pExistCtx->connInterval)
  {
    LL_TRACE_WARN2("!!! Scheduling conflict, CI frequency: incoming handle=%u prioritized over existing handle=%u", LCTR_GET_CONN_HANDLE(pNewCtx), LCTR_GET_CONN_HANDLE(pExistCtx));
    return pNewOp;
  }

  /* Default. */
  LL_TRACE_WARN2("!!! Scheduling conflict, default: existing handle=%u prioritized over incoming handle=%u", LCTR_GET_CONN_HANDLE(pExistCtx), LCTR_GET_CONN_HANDLE(pNewCtx));
  return pExistOp;
}

/*************************************************************************************************/
/*!
 *  \brief  Compute the master sleep clock accuracy index.
 *
 *  \return SCA index.
 */
/*************************************************************************************************/
uint8_t lctrComputeSca(void)
{
  const uint16_t clkPpm = BbGetClockAccuracy();
  int8_t sca;

       if (clkPpm <=  20) sca = 7;
  else if (clkPpm <=  30) sca = 6;
  else if (clkPpm <=  50) sca = 5;
  else if (clkPpm <=  75) sca = 4;
  else if (clkPpm <= 100) sca = 3;
  else if (clkPpm <= 150) sca = 2;
  else if (clkPpm <= 250) sca = 1;
  else                    sca = 0;

  return (uint8_t) (sca + lmgrCb.scaMod);
}

/*************************************************************************************************/
/*!
 *  \brief      Get reference time (due time) of the connection handle.
 *
 *  \param      connHandle    Connection handle.
 *  \param      pDurUsec      Pointer to duration of the connection BOD.
 *
 *  \return     Due time in microseconds of the connection handle.
 */
/*************************************************************************************************/
uint32_t lctrGetConnRefTime(uint8_t connHandle, uint32_t *pDurUsec)
{
  uint32_t refTime = 0;
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(connHandle);

  if (pCtx->enabled && (pCtx->bleData.chan.opType == BB_BLE_OP_MST_CONN_EVENT))
  {
    refTime = pCtx->connBod.dueUsec;
    if (pDurUsec)
    {
      *pDurUsec = pCtx->connBod.minDurUsec;
    }
  }

  return refTime;
}

/*************************************************************************************************/
/*!
 *  \brief      Get the time of anchor point of the specified connection event counter.
 *
 *  \param      pCtx        Connection context.
 *  \param      ceCounter     Connection event counter.
 *
 *  \return     The time of anchor point of the given connection event counter.
 */
/*************************************************************************************************/
uint32_t lctrConnGetAnchorPoint(lctrConnCtx_t *pCtx, uint16_t ceCounter)
{
  uint16_t numCe;

  if (pCtx->role == LL_ROLE_MASTER)
  {
    if ((uint16_t)(ceCounter - pCtx->eventCounter) < (uint16_t)LCTR_MAX_INSTANT)  /* ceCounter is in the future. */
    {
      numCe = ceCounter - pCtx->eventCounter;
      return pCtx->connBod.dueUsec + LCTR_CONN_IND_US(pCtx->connInterval * numCe);
    }
    else
    {
      numCe = pCtx->eventCounter - ceCounter;
      return pCtx->connBod.dueUsec - LCTR_CONN_IND_US(pCtx->connInterval * numCe);
    }
  }
  else
  {
    if ((uint16_t)(ceCounter - (pCtx->data.slv.lastActiveEvent - 1)) < (uint16_t)LCTR_MAX_INSTANT)  /* ceCounter is in the future. */
    {
      numCe = ceCounter - (pCtx->data.slv.lastActiveEvent - 1);
      return pCtx->data.slv.anchorPointUsec + LCTR_CONN_IND_US(pCtx->connInterval * numCe);
    }
    else
    {
      numCe = (pCtx->data.slv.lastActiveEvent - 1) - ceCounter;
      return pCtx->data.slv.anchorPointUsec - LCTR_CONN_IND_US(pCtx->connInterval * numCe);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check if there is a CIS established for the ACL indicated by the handle.
 *
 *  \param  handle      Connection handle.
 *
 *  \return TRUE if there is one established, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LctrIsCisEnabled(uint16_t handle)
{
  if (pLctrConnTbl[handle].checkCisEstAcl)
  {
    return pLctrConnTbl[handle].checkCisEstAcl(handle);
  }

  return FALSE;
}


/*************************************************************************************************/
/*!
 *  \brief  Set up transmit levels.
 *
 *  \param  pCtx  Connection Context.
 */
/*************************************************************************************************/
void lctrInitPhyTxPower(lctrConnCtx_t *pCtx)
{
  /* All transmit powers start unmanaged or unavailable. */
  uint8_t supportedPhys = llGetSupportedPhys();
  supportedPhys |= (supportedPhys & LL_PHYS_LE_CODED_BIT) ? LL_PC_CODED_S2_BIT : 0;

  uint8_t phyIdx = 0;
  for (uint8_t phy = LL_PC_1M_BIT; phy <= LL_PC_MAX_BIT; phy = phy << 1)
  {
    if (supportedPhys & phy)
    {
      pCtx->phyTxPower[phyIdx++] = LL_PWR_CTRL_TXPOWER_UNMANAGED;
    }
    else
    {
      pCtx->phyTxPower[phyIdx++] = LL_PWR_CTRL_TXPOWER_UNAVAILABLE;
    }
  }

  /* Peer transmit power will start as unavailable until we know it through a remote read or request. */
  pCtx->peerTxPower = LL_PWR_CTRL_TXPOWER_UNAVAILABLE;
}

/*************************************************************************************************/
/*!
 *  \brief  Set transmit power reporting parameters.
 *
 *  \param  handle       Connection handle.
 *  \param  enableLocal  Enable local txPower change reporting.
 *  \param  enableRemote Enable remote txPower change reporting.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t lctrSetTxPowerReporting(uint16_t handle, uint8_t enableLocal, uint8_t enableRemote)
{
  lctrConnCtx_t * pCtx = LCTR_GET_CONN_CTX(handle);

  pCtx->powerRptLocal = enableLocal;
  pCtx->powerRptRemote = enableRemote;

  if (enableRemote)
  {
    pCtx->controllerInitRead = TRUE;
    lctrMsgPwrCtrlReq_t *pMsg;
    if ((pMsg = (lctrMsgPwrCtrlReq_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
    {
      pMsg->hdr.handle = LCTR_GET_CONN_HANDLE(pCtx);
      pMsg->hdr.dispId = LCTR_DISP_CONN;
      pMsg->hdr.event  = LCTR_CONN_MSG_API_PWR_CTRL_REQ;
      pMsg->delta      = 0;
      pMsg->phy        = pCtx->bleData.chan.rxPhy + ((pCtx->bleData.chan.rxPhy == BB_PHY_BLE_CODED) && (pCtx->bleData.chan.initTxPhyOptions == BB_PHY_OPTIONS_BLE_S2)) ? 1 : 0;

      WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
    }
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Set enable state for power monitoring.
 *
 *  \param  handle  Handle identifier for connection.
 *  \param  enable  Enable status for power monitor.
 *
 *  \return Status error code.
 *
 *  \note  Path loss must be disabled.
 */
/*************************************************************************************************/
uint8_t lctrSetPowerMonitorEnable(uint16_t handle, bool_t enable)
{
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  if (!(pCtx->usedFeatSet & LL_FEAT_POWER_CONTROL_REQUEST))
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((pCtx->powerMonitorScheme == LCTR_PC_MONITOR_PATH_LOSS) &&
      (pCtx->monitoringState == LCTR_PC_MONITOR_ENABLED))
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  pCtx->powerMonitorScheme = LCTR_PC_MONITOR_AUTO;
  pCtx->monitoringState    = enable;
  pCtx->pclMonitorParam.autoMonitor.highThreshold = pLctrRtCfg->pcHighThreshold;
  pCtx->pclMonitorParam.autoMonitor.lowThreshold = pLctrRtCfg->pcLowThreshold;
  pCtx->pclMonitorParam.autoMonitor.requestVal = LL_PC_REQ_CHANGE_DBM;

  /* Initialize average function. */
  pCtx->pclMonitorParam.autoMonitor.accumulatedRssi = 0;
  pCtx->pclMonitorParam.autoMonitor.totalAccumulatedRssi = 0;
  pCtx->pclMonitorParam.autoMonitor.rssiRunAvg.avgCount = 0;
  pCtx->cisAccumulatedRssi = 0;
  pCtx->cisTotalAccumulatedRssi = 0;
  pCtx->cisRunAvg.avgCount = 0;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Get power limit bit of specified txPower.
 *
 *  \param  txPower  Current transmit power.
 *
 *  \return Limit status of txPower
 */
/*************************************************************************************************/
uint8_t lctrGetPowerLimits(int8_t txPower)
{
  int8_t min;
  int8_t max;

#if (LL_ENABLE_TESTER == TRUE)
  if (llTesterCb.powerLimits)
  {
    return llTesterCb.powerLimits;
  }
#endif

  PalRadioGetSupTxPower(&min, &max);

  if (txPower == min)
  {
    return LL_PWR_CONTROL_LIMIT_MIN_BIT;
  }
  else if (txPower == max)
  {
    return LL_PWR_CONTROL_LIMIT_MAX_BIT;
  }
  else
  {
    return 0;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Host channel class update handler for connections.
 *
 *  \param  chanMap     Updated channel map.
 *
 *  \return Status code.
 */
/*************************************************************************************************/
uint8_t lctrConnChClassUpdate(uint64_t chanMap)
{
  lctrChanMapUpdate_t *pMsg;
  lctrConnCtx_t *pCtx;
  uint16_t handle;
  uint8_t status = LL_SUCCESS;

  /* Update for connections */
  for (handle = 0; handle < pLctrRtCfg->maxConn; handle++)
  {
    if (LctrIsConnHandleEnabled(handle))
    {
      if (LctrGetRole(handle) == LL_ROLE_MASTER)
      {
        /* Update the channel map for CIS master as well. */
        if (LctrUpdateCisChanMapFn)
        {
          LctrUpdateCisChanMapFn(handle);
        }

        if (LctrIsProcActPended(handle, LCTR_CONN_MSG_API_CHAN_MAP_UPDATE) == TRUE)
        {
          status = LL_ERROR_CODE_CMD_DISALLOWED;
        }

        if ((pMsg = (lctrChanMapUpdate_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
        {
          pMsg->hdr.handle = handle;
          pMsg->hdr.dispId = LCTR_DISP_CONN;
          pMsg->hdr.event  = LCTR_CONN_MSG_API_CHAN_MAP_UPDATE;

          pMsg->chanMap = chanMap;

          WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
        }
        else
        {
          LL_TRACE_ERR0("lctrConnChClassUpdate: out of message buffers");
          return LL_ERROR_CODE_CMD_DISALLOWED;
        }
      }
      else /* LL_ROLE_SLAVE */
      {
        if (lastChanClassMap != chanMap)
        {
          pCtx = LCTR_GET_CONN_CTX(handle);

          if ((pCtx->usedFeatSet & LL_FEAT_CHANNEL_CLASSIFICATION) &&
              pCtx->chanStatRptEnable)
          {
            pCtx->data.slv.queuedChanStatusTs = PalBbGetCurrentTime();
            /* Update is sent to host in lctrSlvConnEndOp(). */
          }

          lastChanClassMap = chanMap;
        }
      }
    }
  }

  return status;
}
