/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller connection power control state machine action routines.
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

#include "ll_defs.h"
#include "lctr_int_conn.h"
#include "lctr_int_adv_slave.h"
#include "lctr_int_adv_master_ae.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "lmgr_api_conn.h"
#include "pal_radio.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_timer.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>
#include "lctr_int_pc.h"

/**************************************************************************************************
  Constants
**************************************************************************************************/

#ifndef PC_ENABLE_DEBUG
/*! \brief  Enable power control debug statements. */
#define PC_ENABLE_DEBUG     FALSE
#endif

#if PC_ENABLE_DEBUG
#define LCTR_PC_DBG_TRACE_INFO0(m)              LL_TRACE_INFO0(m)
#define LCTR_PC_DBG_TRACE_INFO1(m, v1)          LL_TRACE_INFO1(m, v1)
#define LCTR_PC_DBG_TRACE_INFO2(m, v1, v2)      LL_TRACE_INFO2(m, v1, v2)
#define LCTR_PC_DBG_TRACE_INFO3(m, v1, v2, v3)  LL_TRACE_INFO3(m, v1, v2, v3)
#else
#define LCTR_PC_DBG_TRACE_INFO0(m)
#define LCTR_PC_DBG_TRACE_INFO1(m, v1)
#define LCTR_PC_DBG_TRACE_INFO2(m, v1, v2)
#define LCTR_PC_DBG_TRACE_INFO3(m, v1, v2, v3)
#endif

/*************************************************************************************************/
/*!
 *  \brief  Return the power control index of a phy bit.
 *
 *  \param  phyBit  Phy bit.
 *
 *  \return Power control phy index, or invalid if not valid.
 */
/*************************************************************************************************/
static uint8_t lctrPhyBitToPclPhyIndex(uint8_t phyBit)
{
  uint8_t phyIndex;
  for (phyIndex = LL_PC_PHY_1M; phyIndex <= LL_PC_PHY_TOTAL; phyIndex++)
  {
    if ((1 << (phyIndex - 1)) == phyBit)
    {
      return phyIndex;
    }
  }
  return LL_PC_PHY_INVALID;
}

/*************************************************************************************************/
/*!
 *  \brief      Attempt a Tx power change.
 *
 *  \param      pCtx    Connection context.
 *  \param      phy     PHY.
 *  \param      delta   Delta to current PHY txPower.
 *
 *  \return     Delta of the new txPower.
 */
/*************************************************************************************************/
int8_t lctrAttemptTxPowerChange(lctrConnCtx_t *pCtx, uint8_t phy, int8_t delta)
{
  BbBleData_t * pBle = &pCtx->bleData;

  uint8_t option = (phy > LL_PHY_LE_CODED) ? BB_PHY_OPTIONS_BLE_S2 : BB_PHY_OPTIONS_BLE_S8;
  if (phy > LL_PHY_LE_CODED)
  {
    phy = LL_PHY_LE_CODED;
  }

  int8_t reqTxPower, curTxPower, newTxPower;
  if (LCTR_GET_TXPOWER(pCtx, phy, option) == LL_PWR_CTRL_TXPOWER_UNAVAILABLE)
  {
    /* The current controller does not support this PHY. */
    return 0;
  }
  else if (LCTR_GET_TXPOWER(pCtx, phy, option) == LL_PWR_CTRL_TXPOWER_UNMANAGED)
  {
    curTxPower = pLctrRtCfg->defTxPwrLvl;
    reqTxPower = pLctrRtCfg->defTxPwrLvl + delta;
  }
  else
  {
    curTxPower = PalRadioGetActualTxPower(LCTR_GET_TXPOWER(pCtx, phy, option), FALSE);
    reqTxPower = LCTR_GET_TXPOWER(pCtx, phy, option) + delta;
  }

  /* Overflow catch condition. */
  if ((delta > 0) &&
      (reqTxPower < curTxPower))
  {
    reqTxPower = LL_PWR_CTRL_TXPOWER_MAX;
  }

  newTxPower = PalRadioIncreasePower(reqTxPower, delta);

  /* Update txPower. */
  LCTR_PC_DBG_TRACE_INFO3("lctrAttemptTxPowerChange: Power change -> handle=%d phy=%d txPow=%d", LCTR_GET_CONN_HANDLE(pCtx), phy + (option == BB_PHY_OPTIONS_BLE_S2) ? 1 : 0, newTxPower);
  LCTR_SET_TXPOWER(pCtx, phy + (((phy == LL_PHY_LE_CODED) && (option == BB_PHY_OPTIONS_BLE_S2)) ? 1 : 0), newTxPower);

  /* Update current txPower if necessary. */
  if (phy == pBle->chan.txPhy)
  {
    pBle->chan.txPower = LCTR_GET_TXPOWER(pCtx, phy, option);
  }

  return newTxPower - curTxPower;
}

/*************************************************************************************************/
/*!
 *  \brief      Send peer power control request PDU.
 *
 *  \param      pCtx    Connection context.
 *  \param      delta   Requested delta.
 */
/*************************************************************************************************/
static void lctrSendPowerCtrlReqPdu(lctrConnCtx_t *pCtx, int8_t delta)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PWR_CTRL_REQ_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/
    UINT8_TO_BSTREAM (pBuf, LL_PDU_PWR_CTRL_REQ);
    UINT8_TO_BSTREAM (pBuf, (1 << (pCtx->reqPhy - 1)));
    UINT8_TO_BSTREAM (pBuf, delta);
    UINT8_TO_BSTREAM (pBuf, LCTR_GET_TXPOWER(pCtx, pCtx->reqPhy, pCtx->bleData.chan.initTxPhyOptions));

    /*** Queue for transmit. ***/
    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
  return;
}

/*************************************************************************************************/
/*!
 *  \brief      Calculate Apr field for a power control response.
 *
 *  \param      pCtx    Connection context.
 *
 *  \return     APR to be packed in response PDU.
 */
/*************************************************************************************************/
static uint8_t lctrCalculateAprField(lctrConnCtx_t *pCtx)
{
  return LL_PWR_CTRL_APR_UNDEF;
}

/*************************************************************************************************/
/*!
 *  \brief      Send peer power control response PDU.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrSendPowerCtrlRspPdu(lctrConnCtx_t *pCtx)
{
  uint8_t *pPdu;
  uint8_t option = (pCtx->reqPhy == LL_PC_PHY_CODED_S2) ? BB_PHY_OPTIONS_BLE_S2 : BB_PHY_OPTIONS_BLE_S8;
  uint8_t txPhy = (pCtx->reqPhy < LL_PC_PHY_CODED_S2) ? pCtx->reqPhy : LL_PHY_LE_CODED;
  int8_t txPower = LCTR_GET_TXPOWER(pCtx, txPhy, option);

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PWR_CTRL_RSP_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/
    UINT8_TO_BSTREAM (pBuf, LL_PDU_PWR_CTRL_RSP);
    UINT8_TO_BSTREAM (pBuf, lctrGetPowerLimits(txPower));
    UINT8_TO_BSTREAM (pBuf, pCtx->delta);
    UINT8_TO_BSTREAM (pBuf, txPower);
    UINT8_TO_BSTREAM (pBuf, lctrCalculateAprField(pCtx));

    /*** Queue for transmit. ***/
    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
  return;
}

/*************************************************************************************************/
/*!
 *  \brief      Update action for power control processing.
 *
 *  \param      pCtx    Connection Context.
 */
/*************************************************************************************************/
void lctrStorePowerControlAction(lctrConnCtx_t *pCtx)
{
  pCtx->delta = pLctrConnMsg->pwrCtrlReq.delta;
  pCtx->reqPhy = pLctrConnMsg->pwrCtrlReq.phy;

  LCTR_PC_DBG_TRACE_INFO2("lctrStorePowerControlAction, phy=%u delta=%u", pCtx->reqPhy, pCtx->delta);

  /* If the power wasn't managed before, start managing it now. */
  if (LCTR_GET_TXPOWER(pCtx, pCtx->reqPhy, BB_PHY_OPTIONS_BLE_S2) == LL_PWR_CTRL_TXPOWER_UNMANAGED)
  {
    LCTR_SET_TXPOWER(pCtx, pCtx->reqPhy, pLctrRtCfg->defTxPwrLvl);

    if (pCtx->reqPhy >= LL_PC_PHY_CODED_S8)
    {
      LCTR_SET_TXPOWER(pCtx, (pCtx->reqPhy == LL_PC_PHY_CODED_S8) ? LL_PC_PHY_CODED_S2 : LL_PC_PHY_CODED_S8, pLctrRtCfg->defTxPwrLvl);
    }
  }

  if (!pCtx->controllerInitRead && (pCtx->delta == 0))
  {
    pCtx->readRemoteTxPower = TRUE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send peer power control request.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendPeerPowerControlReq(lctrConnCtx_t *pCtx)
{
  lctrSendPowerCtrlReqPdu(pCtx, pCtx->delta);
}

/*************************************************************************************************/
/*!
 *  \brief      Store peer power control request.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrStorePeerPowerControlReq(lctrConnCtx_t *pCtx)
{
  lctrPwrCtrlReq_t pld = lctrDataPdu.pld.pwrCtrlReq;

  pCtx->reqErrCode = LL_SUCCESS;

  uint8_t phyIdx = lctrPhyBitToPclPhyIndex(pld.phy);

  /* Verify txPower field is valid. */
  if (phyIdx == LL_PC_PHY_INVALID)
  {
    pCtx->reqErrCode = LL_ERROR_CODE_UNSUPPORTED_LMP_PARAM_VAL;
    return;
  }

  if (pld.txPower == LL_PWR_CTRL_TXPOWER_UNMANAGED)
  {
    pCtx->reqErrCode = LL_ERROR_CODE_INVALID_LMP_PARAMS;
    return;
  }

  /* Notify host of remote change if needed. */
  if ((pCtx->powerRptRemote) &&
      (pCtx->bleData.chan.rxPhy == phyIdx) &&
      (pCtx->peerTxPower != pld.txPower))
  {
    lctrNotifyPowerReportInd(pCtx, LL_SUCCESS, LL_POWER_REPORT_REASON_REMOTE, phyIdx, pld.txPower,
                              lctrGetPowerLimits(pld.txPower),
                              pld.txPower - pCtx->peerTxPower);
  }

  /* Update peer Tx power. */
  if (pCtx->bleData.chan.rxPhy == phyIdx)
  {
    pCtx->peerTxPower = pld.txPower;

    if ((pCtx->peerTxPower == LL_PWR_CTRL_TXPOWER_UNAVAILABLE) ||
         (pCtx->peerTxPower == LL_PWR_CTRL_TXPOWER_UNMANAGED))
    {
      LCTR_PC_DBG_TRACE_INFO1("lctrStorePeerPowerControlReq: txPower unmanaged or unavailable, phy=%d", pld.phy);
    }
    else
    {
      LCTR_PC_DBG_TRACE_INFO1("lctrStorePeerPowerControlReq: txPower=%d", pCtx->peerTxPower);
    }
  }

#if (LL_ENABLE_TESTER == TRUE)
  if (llTesterCb.powerLimits)
  {
    pCtx->delta = 0;
    return;
  }
#endif

  /* Attempt the txPower change, store delta for the response. */
  pCtx->delta = lctrAttemptTxPowerChange(pCtx, phyIdx, lctrDataPdu.pld.pwrCtrlReq.delta);
  pCtx->reqPhy = phyIdx;

  /* Notify host of local change if needed. */
  if ((pCtx->delta != 0) &&
       (pCtx->powerRptLocal))
  {
    uint8_t option = BB_PHY_OPTIONS_BLE_S8;
    uint8_t phy = phyIdx;

    if (phy == LL_PC_PHY_CODED_S2)
    {
      option = BB_PHY_OPTIONS_BLE_S2;
      phy = LL_PHY_LE_CODED;
    }

    int8_t newTxPower = LCTR_GET_TXPOWER(pCtx, phy, option);
    lctrNotifyPowerReportInd(pCtx, LL_SUCCESS, LL_POWER_REPORT_REASON_LOCAL, phyIdx, newTxPower,
                              lctrGetPowerLimits(newTxPower),
                              pCtx->delta);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send peer power control response.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendPeerPowerControlRsp(lctrConnCtx_t *pCtx)
{
  if (pCtx->reqErrCode == LL_SUCCESS)
  {
    lctrSendPowerCtrlRspPdu(pCtx);

    pCtx->peerReqRecvd = TRUE;
  }
  else
  {
    LL_TRACE_WARN0("lctrSendPeerPowerControlRsp: peer sent invalid parameters for power control request");
    lctrSendRejectInd(pCtx, pCtx->reqErrCode, TRUE);
    pCtx->reqErrCode = LL_SUCCESS;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Store peer power control response.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrStorePeerPowerControlRsp(lctrConnCtx_t *pCtx)
{
  lctrPwrCtrlRsp_t pld = lctrDataPdu.pld.pwrCtrlRsp;

  /* Notify host if needed. */
  if ((pCtx->powerRptRemote) &&
      (pCtx->peerTxPower != pld.txPower))
  {
    lctrNotifyPowerReportInd(pCtx, LL_SUCCESS, LL_POWER_REPORT_REASON_REMOTE, pCtx->reqPhy, pld.txPower,
                              pld.limits,
                              pld.txPower - pCtx->peerTxPower);
  }

  /* Update peer information, if needed. */
  if (pCtx->bleData.chan.rxPhy == pCtx->reqPhy)
  {
    pCtx->peerTxPower = pld.txPower;
    pCtx->peerPwrLimits = pld.limits;
    pCtx->peerApr[pCtx->reqPhy] = pld.apr;

    if ((pCtx->peerTxPower == LL_PWR_CTRL_TXPOWER_UNAVAILABLE) ||
         (pCtx->peerTxPower == LL_PWR_CTRL_TXPOWER_UNMANAGED))
    {
      LCTR_PC_DBG_TRACE_INFO1("lctrStorePeerPowerControlReq: txPower unmanaged or unavailable, phy=%d", pCtx->reqPhy);
    }
    else
    {
      LCTR_PC_DBG_TRACE_INFO1("lctrStorePeerPowerControlReq: txPower=%d", pCtx->peerTxPower);

      if (pCtx->controllerInitRead)
      {
        pCtx->controllerInitRead = FALSE;
      }

      if (pCtx->powerMonitorScheme == LCTR_PC_MONITOR_PATH_LOSS)
      {
        if (pCtx->pclMonitorParam.pathLoss.initialPathLossRead)
        {
          pCtx->pclMonitorParam.pathLoss.initialPathLossRead = FALSE;
          pCtx->pclMonitorParam.pathLoss.curZone = lctrCalcPathLossZone(pCtx);
          lctrNotifyHostPathLossRpt(pCtx);
        }
      }
    }
  }

  /* Store delta for reporting. */
  pCtx->delta = pld.delta;

  LCTR_PC_DBG_TRACE_INFO3("lctrStorePeerPowerControlRsp: peerTxPower=%d, peerPwrLimits=%d, peerApr=%d", pld.txPower, pld.limits, pld.apr);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of power report indication.
 *
 *  \param      pCtx    Connection context.
 *  \param      status  Status code.
 *  \param      reason  Reason this indication was sent.
 *  \param      phy     PHY.
 *  \param      txPower Current txPower.
 *  \param      limits  Power limits.
 *  \param      delta   Delta from last txPower.
 */
/*************************************************************************************************/
void lctrNotifyPowerReportInd(lctrConnCtx_t *pCtx, uint8_t status, uint8_t reason, uint8_t phy, int8_t txPower, uint8_t limits, int8_t delta)
{
  const uint16_t handle = LCTR_GET_CONN_HANDLE(pCtx);

  LlPowerReportInd_t evt =
  {
    .hdr =
    {
      .param        = handle,
      .event        = LL_TX_POWER_REPORTING_IND,
      .status       = status
    },

    .status         = LL_SUCCESS,
    .connHandle     = handle,
    .reason         = reason,
    .phy            = phy,
    .txPower        = txPower,
    .txPowerLimits  = limits,
    .delta          = delta
  };

  if (status == LL_SUCCESS)
  {
    LL_TRACE_INFO2("### LlEvent ### lctrNotifyPowerReportInd, handle=%u, reason=%d, status=LL_SUCCESS", handle, reason);
  }
  else
  {
    LL_TRACE_INFO1("### LlEvent ### lctrNotifyPowerReportInd, Power procedure rejected, status=%d", status);
  }


  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Send power change indication.
 *
 *  \param      pCtx       Connection context.
 *  \param      opcode     PDU opcode.
 *  \param      phyVal     PHY.
 *  \param      delta      Delta from previous txPower.
 *  \param      txPower    Current txPower.
 *  \param      phyChange  The indication was caused by a PHY change.
 */
/*************************************************************************************************/
static void lctrSendPowerChangePdu(lctrConnCtx_t *pCtx, uint8_t opcode, uint8_t phyVal, int8_t delta, int8_t txPower, bool_t phyChange)
{
  if (pCtx->peerReqRecvd == FALSE) /* Shall not be sent without a request received. */
  {
    return;
  }

  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PWR_CHANGE_IND_LEN)) != NULL)
  {
    bool_t seperateIndNeeded = FALSE;
    uint8_t *pBuf = pPdu;
    uint8_t phy = (1 << (phyVal - 1));

    /* Attempt to pack coded ind in one ind. */
    if ((phy == LL_PC_CODED_S8_BIT) && (phyChange == TRUE))
    {
      if (LCTR_GET_TXPOWER(pCtx, LL_PHY_LE_CODED, BB_PHY_OPTIONS_BLE_S2) == LCTR_GET_TXPOWER(pCtx, LL_PHY_LE_CODED, BB_PHY_OPTIONS_BLE_S8))
      {
        phy |= LL_PC_CODED_S2_BIT;
      }
      else
      {
        seperateIndNeeded = TRUE;
      }
    }

    /*** Assemble control PDU. ***/
    UINT8_TO_BSTREAM (pBuf, opcode);
    UINT8_TO_BSTREAM (pBuf, phy);
    UINT8_TO_BSTREAM (pBuf, lctrGetPowerLimits(txPower));
    UINT8_TO_BSTREAM (pBuf, delta);
    UINT8_TO_BSTREAM (pBuf, txPower);

    /*** Queue for transmit. ***/
    lctrTxCtrlPduQueue(pCtx, pPdu);

    /* For coded, we need to send S2 txPower as well. */
    if (seperateIndNeeded)
    {
      txPower = LCTR_GET_TXPOWER(pCtx, LL_PHY_LE_CODED, BB_PHY_OPTIONS_BLE_S2);
      pBuf = pPdu + 1;
      UINT8_TO_BSTREAM (pBuf, LL_PC_CODED_S2_BIT);
      UINT8_TO_BSTREAM (pBuf, lctrGetPowerLimits(txPower));
      UINT8_TO_BSTREAM (pBuf, delta);
      UINT8_TO_BSTREAM (pBuf, txPower);

      /*** Queue for transmit. ***/
      lctrTxCtrlPduQueue(pCtx, pPdu);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send power control indication.
 *
 *  \param      pCtx       Connection context.
 *  \param      phy        PHY.
 *  \param      delta      Delta from previous txPower.
 *  \param      txPower    Current txPower.
 *  \param      phyChange  The indication was a result of a PHY change.
 */
/*************************************************************************************************/
void lctrSendPowerChangeInd(lctrConnCtx_t *pCtx, uint8_t phy, int8_t delta, int8_t txPower, bool_t phyChange)
{
  lctrSendPowerChangePdu(pCtx, LL_PDU_PWR_CHANGE_IND, phy, delta, txPower, phyChange);
}

/*************************************************************************************************/
/*!
 *  \brief      Store peer power indication.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrStorePeerPowerInd(lctrConnCtx_t *pCtx)
{
  lctrPwrChngInd_t * pPdu = &lctrDataPdu.pld.pwrChngInd;
  LCTR_PC_DBG_TRACE_INFO3("lctrStorePeerPowerInd: phy=%d delta=%d txPower=%d", pPdu->phy, pPdu->delta, pPdu->txPower);

  if (pPdu->phy & (1 << (pCtx->bleData.chan.rxPhy - 1)))
  {
    if (pCtx->powerRptRemote)
    {
      lctrNotifyPowerReportInd(pCtx, LL_SUCCESS,  LL_POWER_REPORT_REASON_REMOTE,
                               pCtx->bleData.chan.rxPhy, pPdu->txPower, pPdu->limits,
                               pPdu->txPower - pCtx->peerTxPower);
    }

    if (pPdu->phy == pCtx->bleData.chan.rxPhy)
    {
      pCtx->peerTxPower = pPdu->txPower;

      if ((pCtx->peerTxPower == LL_PWR_CTRL_TXPOWER_UNAVAILABLE) ||
           (pCtx->peerTxPower == LL_PWR_CTRL_TXPOWER_UNMANAGED))
      {
        LCTR_PC_DBG_TRACE_INFO1("lctrStorePeerPowerInd: txPower unmanaged or unavailable, phy=%d", pPdu->phy);
      }
      else
      {
        LCTR_PC_DBG_TRACE_INFO1("lctrStorePeerPowerInd: txPower=%d", pCtx->peerTxPower);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send peer power indication response (Included for state machine conformance).
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrSendPeerPowerRsp(lctrConnCtx_t *pCtx)
{
  /* No need to reply to power change indications. */
  return;
}

/*************************************************************************************************/
/*!
 *  \brief      Service connection power monitor.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrConnServicePowerMonitor(lctrConnCtx_t *pCtx)
{
  if (!(pCtx->usedFeatSet & LL_FEAT_POWER_CONTROL_REQUEST) ||
      lmgrGetOpFlag(LL_OP_MODE_FLAG_DIS_POWER_MONITOR))
  {
    pCtx->monitoringState = LCTR_PC_MONITOR_DISABLED;
    return;
  }

  int8_t sendReqDelta = 0;

  /* Restore the sign after finding the absolute value average RSSI. */
  lctrRssiAddAveragePoint(&pCtx->pclMonitorParam.autoMonitor.rssiRunAvg,
                          -((int8_t) (pCtx->pclMonitorParam.autoMonitor.accumulatedRssi / pCtx->pclMonitorParam.autoMonitor.totalAccumulatedRssi)));
  pCtx->pclMonitorParam.autoMonitor.accumulatedRssi = 0;
  pCtx->pclMonitorParam.autoMonitor.totalAccumulatedRssi = 0;

  if (pCtx->pclMonitorParam.autoMonitor.rssiRunAvg.avgCount >= LL_PC_TBL_LEN)
  {
    int8_t averageRunning = lctrRssiGetAverage(&pCtx->pclMonitorParam.autoMonitor.rssiRunAvg);

    if ((averageRunning > pCtx->pclMonitorParam.autoMonitor.highThreshold) &&
        (!(pCtx->peerPwrLimits & LL_PWR_CONTROL_LIMIT_MIN_BIT)))
    {
      sendReqDelta = -(pCtx->pclMonitorParam.autoMonitor.requestVal);
    }
    else if ((averageRunning < pCtx->pclMonitorParam.autoMonitor.lowThreshold) &&
             !(pCtx->peerPwrLimits & LL_PWR_CONTROL_LIMIT_MAX_BIT))
    {
      sendReqDelta = pCtx->pclMonitorParam.autoMonitor.requestVal;
    }
  }

  if (sendReqDelta != 0)
  {
    if ((pCtx->llcpActiveProc == LCTR_PROC_PWR_CTRL) ||
        (pCtx->llcpPendMask & (1 << LCTR_PROC_PWR_CTRL)))
    {
      LL_TRACE_WARN0("Power control LLCP already pending -- try increasing service interval");
      return;
    }

    uint8_t reqPhy = pCtx->bleData.chan.rxPhy + (((pCtx->bleData.chan.rxPhy == BB_PHY_BLE_CODED) && (pCtx->bleData.chan.initTxPhyOptions == BB_PHY_OPTIONS_BLE_S2)) ? 1 : 0);

    lctrMsgPwrCtrlReq_t *pMsg;
    if ((pMsg = (lctrMsgPwrCtrlReq_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
    {
      pMsg->hdr.handle = LCTR_GET_CONN_HANDLE(pCtx);
      pMsg->hdr.dispId = LCTR_DISP_CONN;
      pMsg->hdr.event  = LCTR_CONN_MSG_API_PWR_CTRL_REQ;
      pMsg->delta      = sendReqDelta;
      pMsg->phy        = reqPhy;
      WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
    }

    pCtx->pclMonitorParam.autoMonitor.rssiRunAvg.avgCount = 0;
  }
}
