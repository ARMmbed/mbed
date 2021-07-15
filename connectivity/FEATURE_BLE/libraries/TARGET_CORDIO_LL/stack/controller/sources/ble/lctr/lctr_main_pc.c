/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller power control implementation file.
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
#include "lmgr_api_conn.h"
#include "ll_api.h"
#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>
#include "lctr_api_pc.h"
#include "lctr_int_pc.h"

/*************************************************************************************************/
/*!
 *  \brief      Initialize power control resources.
 */
/*************************************************************************************************/
void LctrPowerControlInit(void)
{
  /* Initialize power control. */
  lmgrPersistCb.featuresDefault |=  (LL_FEAT_POWER_CHANGE_IND | LL_FEAT_POWER_CONTROL_REQUEST);

  /* Initialize path loss. */
  lmgrPersistCb.featuresDefault |= (LL_FEAT_PATH_LOSS_MONITOR);
  lctrPathLossMonitorActFn = lctrPathLossMonitorAct;

  /* Initialize state machines. */
  lctrSlvLlcpSmTbl[LCTR_LLCP_SM_PC] = lctrLlcpExecutePclSm;
  lctrMstLlcpSmTbl[LCTR_LLCP_SM_PC] = lctrLlcpExecutePclSm;

  /* Initialize function pointers. */
  lctrSendPowerChangeIndCback = lctrSendPowerChangeInd;
  lctrNotifyPowerReportIndCback = lctrNotifyPowerReportInd;
}

/*************************************************************************************************/
/*!
 *  \brief      Calculate path loss of a connection context.
 *
 *  \param      pCtx    Connection context.
 *
 *  \return     Current path loss
 */
/*************************************************************************************************/
static uint8_t lctrCalcPathLoss(lctrConnCtx_t *pCtx)
{
  int8_t txPower = pCtx->peerTxPower;
  int8_t lastRssi = pCtx->rssi;

  return (uint8_t) (WSF_MAX(txPower, lastRssi) - WSF_MIN(txPower, lastRssi));
}

/*************************************************************************************************/
/*!
 *  \brief      Calculate the path loss zone of a connection context.
 *
 *  \param      pCtx    Connection context.
 *
 *  \return     Path loss zone.
 */
/*************************************************************************************************/
uint8_t lctrCalcPathLossZone(lctrConnCtx_t *pCtx)
{
  uint8_t pathLoss = lctrCalcPathLoss(pCtx);
  uint8_t curZone = pCtx->pclMonitorParam.pathLoss.curZone;

  uint8_t highUpperBound = pCtx->pclMonitorParam.pathLoss.highThreshold + pCtx->pclMonitorParam.pathLoss.highHysteresis;
  uint8_t highLowerBound = pCtx->pclMonitorParam.pathLoss.highThreshold - pCtx->pclMonitorParam.pathLoss.highHysteresis;

  uint8_t lowUpperBound = pCtx->pclMonitorParam.pathLoss.lowThreshold + pCtx->pclMonitorParam.pathLoss.lowHysteresis;
  uint8_t lowLowerBound = pCtx->pclMonitorParam.pathLoss.lowThreshold - pCtx->pclMonitorParam.pathLoss.lowHysteresis;

  switch (curZone)
  {
    case LL_PATH_LOSS_ZONE_LOW:
    {
      if (pathLoss > highUpperBound)
      {
        return LL_PATH_LOSS_ZONE_HIGH;
      }
      else if (pathLoss > lowUpperBound)
      {
        return LL_PATH_LOSS_ZONE_MID;
      }
      else
      {
        return LL_PATH_LOSS_ZONE_LOW;
      }
    }

    case LL_PATH_LOSS_ZONE_MID:
    {
      if (pathLoss > highUpperBound)
      {
        return LL_PATH_LOSS_ZONE_HIGH;
      }
      else if (pathLoss < lowLowerBound)
      {
        return LL_PATH_LOSS_ZONE_LOW;
      }
      else
      {
        return LL_PATH_LOSS_ZONE_MID;
      }
    }

    case LL_PATH_LOSS_ZONE_HIGH:
    {
      if (pathLoss < lowLowerBound)
      {
        return LL_PATH_LOSS_ZONE_LOW;
      }
      else if (pathLoss < highLowerBound)
      {
        return LL_PATH_LOSS_ZONE_MID;
      }
      else
      {
        return LL_PATH_LOSS_ZONE_HIGH;
      }
    }

    default:
      LL_TRACE_ERR1("lctrCalcPathLossZone: Path loss invalid zone: %d", curZone);
      return LL_PATH_LOSS_ZONE_MID;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set path loss reporting parameters
 *
 *  \param      handle      Connection handle.
 *  \param      highThresh  High threshold.
 *  \param      highHyst    High hysteresis.
 *  \param      lowThresh   Low threshold.
 *  \param      lowHyst     Low hysteresis.
 *  \param      minTime     Minimum time spent to trigger event.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t lctrSetPathLossReportingParams(uint16_t handle, uint8_t highThresh, uint8_t highHyst, uint8_t lowThresh, uint8_t lowHyst, uint16_t minTime)
{
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  memset(&pCtx->pclMonitorParam.pathLoss, 0, sizeof(pCtx->pclMonitorParam.pathLoss));
  pCtx->pclMonitorParam.pathLoss.highThreshold = highThresh;
  pCtx->pclMonitorParam.pathLoss.highHysteresis = highHyst;
  pCtx->pclMonitorParam.pathLoss.lowThreshold = lowThresh;
  pCtx->pclMonitorParam.pathLoss.lowHysteresis = lowHyst;
  pCtx->pclMonitorParam.pathLoss.minTimeSpent = minTime;
  pCtx->pclMonitorParam.pathLoss.curZone = LL_PATH_LOSS_ZONE_MID;

  if ((pCtx->powerMonitorScheme == LCTR_PC_MONITOR_PATH_LOSS) &&
      (pCtx->monitoringState = LCTR_PC_MONITOR_ENABLED))
  {
    pCtx->pclMonitorParam.pathLoss.curZone = lctrCalcPathLossZone(pCtx);
    lctrNotifyHostPathLossRpt(pCtx);
  }
  else
  {
    pCtx->powerMonitorScheme = LCTR_PC_MONITOR_PATH_LOSS;
    pCtx->monitoringState = LCTR_PC_MONITOR_READY;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set path loss enable.
 *
 *  \param      handle    Connection handle.
 *  \param      enable    Enable.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t lctrSetPathLossReportingEnable(uint16_t handle, uint8_t enable)
{
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);
  if (pCtx->monitoringState != LCTR_PC_MONITOR_READY)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  /* If peer Tx power is unavailable, read the peer tx power. */
  if (pCtx->peerTxPower == LL_PWR_CTRL_TXPOWER_UNAVAILABLE)
  {
    pCtx->pclMonitorParam.pathLoss.initialPathLossRead = TRUE;
    pCtx->controllerInitRead = TRUE;
    uint8_t reqPhy = pCtx->bleData.chan.rxPhy + (((pCtx->bleData.chan.rxPhy == BB_PHY_BLE_CODED) && (pCtx->bleData.chan.initTxPhyOptions == BB_PHY_OPTIONS_BLE_S2)) ? 1 : 0);

    lctrMsgPwrCtrlReq_t *pMsg;
    if ((pMsg = (lctrMsgPwrCtrlReq_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
    {
      pMsg->hdr.handle = LCTR_GET_CONN_HANDLE(pCtx);
      pMsg->hdr.dispId = LCTR_DISP_CONN;
      pMsg->hdr.event  = LCTR_CONN_MSG_API_PWR_CTRL_REQ;
      pMsg->delta      = 0;
      pMsg->phy        = reqPhy;
      WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
    }
    else
    {
      LL_TRACE_WARN0("lctrSetPathLossReportingEnable: Failed to alloc power control read request.");
      return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
    }
  }
  else
  {
    pCtx->pclMonitorParam.pathLoss.curZone = lctrCalcPathLossZone(pCtx);
    lctrNotifyHostPathLossRpt(pCtx);
  }

  pCtx->monitoringState = LCTR_PC_MONITOR_ENABLED;
  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Notify the host of a path loss zone change event.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrNotifyHostPathLossRpt(lctrConnCtx_t *pCtx)
{
  const uint16_t handle = LCTR_GET_CONN_HANDLE(pCtx);
  uint8_t curPathLoss = lctrCalcPathLoss(pCtx);

  LlPathLossThresholdInd_t evt =
  {
    .hdr =
    {
      .param        = handle,
      .event        = LL_PATH_LOSS_REPORTING_IND,
      .status       = LL_SUCCESS
    },

    .connHandle     = handle,
    .curPathLoss    = curPathLoss,
    .zoneEntered    = pCtx->pclMonitorParam.pathLoss.curZone,
  };

  LL_TRACE_INFO3("### LlEvent ### lctrNotifyHostPathLossRpt , handle=%u, zoneEntered=%d, curPathLoss=%d", handle, pCtx->pclMonitorParam.pathLoss.curZone, curPathLoss);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Path loss monitoring action.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrPathLossMonitorAct(lctrConnCtx_t *pCtx)
{
  /* If the first path loss zone has not been generated yet, skip monitoring. */
  if (pCtx->pclMonitorParam.pathLoss.initialPathLossRead == TRUE)
  {
    return;
  }

  uint8_t newZone = lctrCalcPathLossZone(pCtx);
  uint8_t curZone = pCtx->pclMonitorParam.pathLoss.curZone;

  if (newZone == curZone)
  {
    pCtx->pclMonitorParam.pathLoss.curTimeSpent = 0;
    pCtx->pclMonitorParam.pathLoss.newZone = newZone;
    return;
  }
  else if (newZone != pCtx->pclMonitorParam.pathLoss.newZone)
  {
    pCtx->pclMonitorParam.pathLoss.curTimeSpent = 0;
    pCtx->pclMonitorParam.pathLoss.newZone = newZone;
    return;
  }

  /* Implies that the newZone is a consecutive event. */
  pCtx->pclMonitorParam.pathLoss.curTimeSpent++;

  if (pCtx->pclMonitorParam.pathLoss.curTimeSpent >=
      pCtx->pclMonitorParam.pathLoss.minTimeSpent)
  {
    LL_TRACE_INFO2("lctrPathLossMonitorAct: new zone entered. newZone=%u, pathLoss=%u", newZone, lctrCalcPathLoss(pCtx));
    pCtx->pclMonitorParam.pathLoss.curTimeSpent = 0;
    pCtx->pclMonitorParam.pathLoss.curZone = newZone;

    lctrNotifyHostPathLossRpt(pCtx);
  }
}
