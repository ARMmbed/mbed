/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Power control LLCP state machine implementation file.
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
#include "lmgr_api.h"
#include "lctr_int_pc.h"
#include "util/bstream.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Power control LLCP types. */
enum
{
  LCTR_PC_PROC_IND,       /*!< Power Indication process. */
  LCTR_PC_PROC_CTRL,      /*!< Power control process. */

  LCTR_PC_PROC_TOTAL      /*!< Total power control procedures. */
};

/*! \brief      Power control LLCP actions. */
enum
{
  LCTR_PC_ACT_API_PARAM,      /*!< Store host API parameter action. */
  LCTR_PC_ACT_SEND_REQ,       /*!< Send request action. */
  LCTR_PC_ACT_RECV_REQ,       /*!< Receive request action. */
  LCTR_PC_ACT_SEND_RSP,       /*!< Send response action. */
  LCTR_PC_ACT_RECV_RSP,       /*!< Receive response action. */
  LCTR_PC_ACT_TOTAL           /*!< Total power control procedure actions. */
};

/*! \brief      Power control LLCP events. */
enum
{
  LCTR_PC_EVT_HOST_START,     /*!< Host initiated start procedure event. */
  LCTR_PC_EVT_INT_START,      /*!< LL initiated start procedure event. */
  LCTR_PC_EVT_INT_INCOMP,     /*!< Complete an interrupted procedure event. */
  LCTR_PC_EVT_RECV_IND,       /*!< Receive indication event. */
  LCTR_PC_EVT_RECV_REQ,       /*!< Receive request event. */
  LCTR_PC_EVT_RECV_RSP,       /*!< Receive response event. */
  LCTR_PC_EVT_PROC_COMP,      /*!< Procedure completed event. */
  LCTR_PC_EVT_REJECT,         /*!< Procedure rejected event. */
  LCTR_PC_EVT_TOTAL,          /*!< Total power control procedure events. */
  LCTR_PC_EVT_INVALID = 0xFF  /*!< Invalid event. */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Power control procedure state machine action table. */
const lctrLlcpEh_t lctrPclProcTbl[LCTR_PC_PROC_TOTAL][LCTR_PC_ACT_TOTAL] =
{
  /* LCTR_PROC_PWR_IND */
  {
    NULL,                               /* LCTR_PC_ACT_API_PARAM */
    NULL,                               /* LCTR_PC_ACT_SEND_REQ */
    lctrStorePeerPowerInd,              /* LCTR_PC_ACT_RECV_REQ */
    lctrSendPeerPowerRsp,               /* LCTR_PC_ACT_SEND_RSP */
    NULL,                               /* LCTR_PC_ACT_RECV_RSP */
  },
  /* LCTR_PROC_PWR_CTRL */
  {
    lctrStorePowerControlAction,        /* LCTR_PC_ACT_API_PARAM */
    lctrSendPeerPowerControlReq,        /* LCTR_PC_ACT_SEND_REQ */
    lctrStorePeerPowerControlReq,       /* LCTR_PC_ACT_RECV_REQ */
    lctrSendPeerPowerControlRsp,        /* LCTR_PC_ACT_SEND_RSP */
    lctrStorePeerPowerControlRsp,       /* LCTR_PC_ACT_RECV_RSP */
  }
};

/*************************************************************************************************/
/*!
 *  \brief      Get power control state machine index from process ID
 *
 *  \param      proc  Process ID.
 *
 *  \return     Index of power control state machine.
 */
/*************************************************************************************************/
static uint8_t lctrGetPclProcSmIndex(uint8_t proc)
{
  if (proc == LCTR_PROC_PWR_IND)
  {
    return LCTR_PC_PROC_IND;
  }
  else if (proc == LCTR_PROC_PWR_CTRL)
  {
    return LCTR_PC_PROC_CTRL;
  }
  else
  {
    LL_TRACE_ERR1("lctrGetPclProcSmIndex, invalid proc=%d", proc);
    return LCTR_PROC_INVALID;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Execute action function.
 *
 *  \param      pCtx    Connection context.
 *  \param      proc    Procedure ID.
 *  \param      act     Action ID.
 */
/*************************************************************************************************/
static inline void lctrExecAction(lctrConnCtx_t *pCtx, uint8_t proc, uint8_t act)
{
  uint8_t pclProc = lctrGetPclProcSmIndex(proc);

  if (lctrPclProcTbl[pclProc][act])
  {
    lctrPclProcTbl[pclProc][act](pCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get power control LLCP procedure ID.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   Event.
 *
 *  \return     LLCP procedure event ID.
 */
/*************************************************************************************************/
static uint8_t lctrGetPclProcId(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    case LCTR_CONN_MSG_API_PWR_CTRL_REQ:
      return LCTR_PROC_PWR_CTRL;

    case LCTR_CONN_MSG_RX_LLCP:
      switch (lctrDataPdu.opcode)
      {
        case LL_PDU_PWR_CHANGE_IND:
          return LCTR_PROC_PWR_IND;

        case LL_PDU_PWR_CTRL_REQ:
        case LL_PDU_PWR_CTRL_RSP:
          return LCTR_PROC_PWR_CTRL;

        case LL_PDU_REJECT_IND:
        case LL_PDU_REJECT_EXT_IND:
          return pCtx->llcpActiveProc;

        case LL_PDU_UNKNOWN_RSP:
          switch (lctrDataPdu.pld.unknownRsp.unknownType)
          {
            case LL_PDU_PWR_CTRL_REQ:
            case LL_PDU_PWR_CTRL_RSP:
            case LL_PDU_PWR_CHANGE_IND:
              return LCTR_PC_EVT_REJECT;
          }
          break;

        default:
          break;
      }

      break;

    case LCTR_CONN_LLCP_START_PENDING:
      if (pCtx->llcpPendMask & (1 << LCTR_PROC_PWR_IND))
      {
        pCtx->llcpPendMask &= ~(1 << LCTR_PROC_PWR_IND);
        return LCTR_PROC_PWR_IND;
      }
      if (pCtx->llcpPendMask & (1 << LCTR_PROC_PWR_CTRL))
      {
        pCtx->llcpPendMask &= ~(1 << LCTR_PROC_PWR_CTRL);
        return LCTR_PROC_PWR_CTRL;
      }
      break;

    default:
      break;
  }

  return LCTR_PROC_INVALID;
}

/*************************************************************************************************/
/*!
 *  \brief      Get remapped power control LLCP procedure event.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   Subsystem event.
 *
 *  \return     Power control LLCP procedure action ID.
 *
 *  This routine remaps events for the power control LLCP procedure state machine for optimized
 *  compressed state tables.
 */
/*************************************************************************************************/
static uint8_t lctrRemapPcEvent(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    case LCTR_CONN_MSG_API_PWR_CTRL_REQ:
      return LCTR_PC_EVT_HOST_START;

    case LCTR_CONN_MSG_RX_LLCP_UNKNOWN:
      return LCTR_PC_EVT_RECV_REQ;

    case LCTR_CONN_MSG_RX_LLCP:
      switch (lctrDataPdu.opcode)
      {
        case LL_PDU_PWR_CTRL_REQ:
          return LCTR_PC_EVT_RECV_REQ;

        case LL_PDU_PWR_CHANGE_IND:
          return LCTR_PC_EVT_RECV_IND;

        case LL_PDU_PWR_CTRL_RSP:
          return LCTR_PC_EVT_RECV_RSP;

        case LL_PDU_UNKNOWN_RSP:
        case LL_PDU_REJECT_IND:
        case LL_PDU_REJECT_EXT_IND:
          return LCTR_PC_EVT_REJECT;

        default:
          return LCTR_PC_EVT_INVALID;
      }

    case LCTR_CONN_LLCP_START_PENDING:
      if (pCtx->llcpIncompMask)
      {
        return LCTR_PC_EVT_INT_INCOMP;
      }
      else
      {
        return LCTR_PC_EVT_INT_START;
      }

    default:
      break;
  }

  return LCTR_PC_EVT_INVALID;
}

/*************************************************************************************************/
/*!
 *  \brief      Check for feature availability.
 *
 *  \param      pCtx    Connection context.
 *  \param      proc    Requesting procedure.
 *  \param      event   Event.
 *
 *  \return     TRUE if available, FALSE otherwise.
 */
/*************************************************************************************************/
static bool_t lctrFeatureAvail(lctrConnCtx_t *pCtx, uint8_t proc, uint8_t event)
{
  bool_t result = TRUE;

  switch (proc)
  {
    case LCTR_PROC_PWR_IND:
      if ((lmgrCb.features & LL_FEAT_POWER_CHANGE_IND) == 0)
      {
        LL_TRACE_WARN1("Requested LCTR_PROC_PWR_IND not available, FeatSet=0x%08x", lmgrCb.features);
        result = FALSE;
      }
      break;
    case LCTR_PROC_PWR_CTRL:
      if ((lmgrCb.features & LL_FEAT_POWER_CONTROL_REQUEST) == 0)
      {
        LL_TRACE_WARN1("Requested LCTR_PROC_PWR_CTRL not available, FeatSet=0x%08x", lmgrCb.features);
        result = FALSE;
      }
      break;
    default:
      break;
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of successful procedure.
 *
 *  \param      pCtx    Connection context.
 *  \param      proc    Completed procedure.
 */
/*************************************************************************************************/
static void lctrNotifyHostSuccess(lctrConnCtx_t *pCtx, uint8_t proc)
{
  if ((pCtx->llcpNotifyMask & (1 << proc)) == 0)
  {
    return;
  }

  pCtx->llcpNotifyMask &= ~(1 << proc);

  switch (proc)
  {
    case LCTR_PROC_PWR_CTRL:
      if (pCtx->readRemoteTxPower)
      {
        lctrNotifyPowerReportInd(pCtx, LL_SUCCESS, LL_POWER_REPORT_REASON_READ_REMOTE,
                                 pCtx->reqPhy, pCtx->peerTxPower, pCtx->peerPwrLimits,
                                 pCtx->delta);
        pCtx->readRemoteTxPower = FALSE;
      }
      break;
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of failed procedure.
 *
 *  \param      pCtx    Connection context.
 *  \param      proc    Complete procedure.
 */
/*************************************************************************************************/
static void lctrNotifyHostReject(lctrConnCtx_t *pCtx, uint8_t proc)
{
  if ((pCtx->llcpNotifyMask & (1 << proc)) == 0)
  {
    return;
  }

  pCtx->llcpNotifyMask &= ~(1 << proc);

  uint8_t reason = LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE;

  switch (lctrDataPdu.opcode)
  {
    case LL_PDU_REJECT_IND:
    case LL_PDU_REJECT_EXT_IND:
      reason = lctrDataPdu.pld.rejInd.reason;
      break;
    case LL_PDU_UNKNOWN_RSP:
      /* Do not transmit another PCL request if the peer does not recognize it. */
      if (proc == LCTR_PROC_PWR_CTRL)
      {
        pCtx->usedFeatSet &= ~LL_FEAT_POWER_CONTROL_REQUEST;
      }
      if (proc == LCTR_PROC_PWR_IND)
      {
        pCtx->usedFeatSet &= ~LL_FEAT_POWER_CHANGE_IND;
      }
      break;
    default:
      break;
  }
  switch (proc)
  {
    case LCTR_PROC_PWR_CTRL:
      if (pCtx->readRemoteTxPower)
      {
        lctrNotifyPowerReportInd(pCtx, reason, LL_POWER_REPORT_REASON_READ_REMOTE, 0, 0, 0, 0);
      }
      break;
    case LCTR_PROC_PWR_IND:
      break;
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Execute power control LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 *
 *  \return     TRUE if handled by this SM, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrLlcpExecutePclSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  uint8_t proc;
  uint8_t llEvent = event;

  /*** State-less events ***/

  if (event == LCTR_CONN_LLCP_PWR_CTRL_SERVICE)
  {
    proc = LCTR_PROC_PWR_CTRL;

    if (lctrFeatureAvail(pCtx, proc, event) &&
        (pCtx->monitoringState == LCTR_PC_MONITOR_ENABLED) &&
        (pCtx->powerMonitorScheme == LCTR_PC_MONITOR_AUTO))
    {
      /* Restart timer. */
      WsfTimerStartMs(&pCtx->tmrPowerCtrl, LL_PC_SERVICE_MS);
      lctrConnServicePowerMonitor(pCtx);

      /* If CIS is enabled, monitor those power levels if needed. */
      if (lctrCisServicePowerMonitorFn)
      {
        lctrCisServicePowerMonitorFn(pCtx);
      }
    }
    else
    {
      WsfTimerStop(&pCtx->tmrPowerCtrl);
    }

    return TRUE;
  }

  /*** LLCP state machine ***/

  if ((event = lctrRemapPcEvent(pCtx, event)) == LCTR_PC_EVT_INVALID)
  {
    return FALSE;
  }

  if ((proc = lctrGetPclProcId(pCtx, llEvent)) == LCTR_PROC_INVALID)
  {
    return FALSE;
  }

  switch (pCtx->llcpState)
  {
    case LCTR_LLCP_STATE_IDLE:
      LL_TRACE_INFO3("lctrLlcpExecutePclSm: handle=%u, proc=%u, state=IDLE, event=%u", LCTR_GET_CONN_HANDLE(pCtx), proc, event);

      switch (event)
      {
        case LCTR_PC_EVT_HOST_START:
          pCtx->llcpNotifyMask |= 1 << proc;
          lctrExecAction(pCtx, proc, LCTR_PC_ACT_API_PARAM);
          /* Fallthrough */
        case LCTR_PC_EVT_INT_START:
          if (lctrFeatureAvail(pCtx, proc, event))
          {
            lctrExecAction(pCtx, proc, LCTR_PC_ACT_SEND_REQ);
            lctrStartLlcpTimer(pCtx);
            pCtx->llcpActiveProc = proc;
            pCtx->cmnState = LCTR_CMN_STATE_BUSY;
            pCtx->llcpState = LCTR_LLCP_STATE_BUSY;
          }
          else
          {
            lctrNotifyHostReject(pCtx, proc);
          }
          break;

        case LCTR_PC_EVT_INT_INCOMP:
        case LCTR_PC_EVT_RECV_IND:
        case LCTR_PC_EVT_RECV_REQ:
          if (lctrFeatureAvail(pCtx, proc, event))
          {
            if (event != LCTR_PC_EVT_INT_INCOMP)
            {
              lctrExecAction(pCtx, proc, LCTR_PC_ACT_RECV_REQ);
            }

            if (lctrPclProcTbl[lctrGetPclProcSmIndex(proc)][LCTR_PC_ACT_SEND_RSP])
            {
              /* Procedure completes; no transition to BUSY. */
              lctrPclProcTbl[lctrGetPclProcSmIndex(proc)][LCTR_PC_ACT_SEND_RSP](pCtx);
            }
            else
            {
              /* No response; procedure completes at CE instant. */
              pCtx->llcpActiveProc = proc;
              pCtx->llcpInstantComp = FALSE;
              pCtx->cmnState = LCTR_CMN_STATE_BUSY;
              pCtx->llcpState = LCTR_LLCP_STATE_BUSY;
            }
          }
          else
          {
            lctrSendRejectInd(pCtx, LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE, FALSE);
          }
          break;

        case LCTR_PC_EVT_RECV_RSP:
        case LCTR_PC_EVT_PROC_COMP:
        case LCTR_PC_EVT_REJECT:
        default:
          /* No action required. */
          break;
      }
      break;

    case LCTR_LLCP_STATE_BUSY:
      LL_TRACE_INFO3("lctrLlcpExecutePclSm: handle=%u, proc=%u, state=BUSY, event=%u", LCTR_GET_CONN_HANDLE(pCtx), proc, event);

      switch (event)
      {
        case LCTR_PC_EVT_HOST_START:
        case LCTR_PC_EVT_INT_START:
          if (lctrFeatureAvail(pCtx, proc, event))
          {
            if (event == LCTR_PC_EVT_HOST_START)
            {
              pCtx->llcpNotifyMask |= 1 << proc;
            }

            if (pCtx->llcpActiveProc != proc)
            {
              LL_TRACE_INFO1("Pending procedure in progress: activeProc=%u", pCtx->llcpActiveProc);
              pCtx->llcpPendMask |= 1 << proc;
            }
            if (event == LCTR_PC_EVT_HOST_START)
            {
              lctrExecAction(pCtx, proc, LCTR_PC_ACT_API_PARAM);
            }
          }
          else
          {
            lctrNotifyHostReject(pCtx, proc);
          }
          break;

        case LCTR_PC_EVT_RECV_IND:        /* Completion from message */
          if (proc != pCtx->llcpActiveProc)
          {
            if (lctrFeatureAvail(pCtx, proc, event))
            {
              LL_TRACE_WARN1("Procedure collision; pending incomplete request, activeProc=%u", pCtx->llcpActiveProc);
              lctrExecAction(pCtx, proc, LCTR_PC_ACT_RECV_REQ);
              pCtx->llcpIncompMask |= 1 << proc;
            }
            break;
          }
          /* Fallthrough */
        case LCTR_PC_EVT_RECV_RSP:
          if (proc == pCtx->llcpActiveProc)
          {
            /* Incoming procedure matches the active one. */
            lctrExecAction(pCtx, proc, LCTR_PC_ACT_RECV_RSP);
          }
          else if (pCtx->llcpPendMask & (1 << proc))
          {
            /* Incoming procedure matches one of the pended one. */
            pCtx->llcpPendMask &= ~(1 << proc);
            if (pCtx->llcpIsOverridden == TRUE)
            {
              pCtx->llcpIsOverridden = FALSE;
            }
            lctrExecAction(pCtx, proc, LCTR_PC_ACT_RECV_RSP);
            lctrNotifyHostSuccess(pCtx, proc);
            break;
          }
          else
          {
            LL_TRACE_ERR1("Unexpected response packet, expected llcpActiveProc=%u", pCtx->llcpActiveProc);
            break;
          }
          /* Fallthrough */
        case LCTR_PC_EVT_PROC_COMP:       /* Completion from CE */
        case LCTR_PC_EVT_REJECT:          /* Failed completion */
          if (event == LCTR_PC_EVT_REJECT)
          {
            lctrNotifyHostReject(pCtx, proc);
          }
          else
          {
            lctrNotifyHostSuccess(pCtx, proc);
          }

          lctrStopLlcpTimer(pCtx);

          pCtx->llcpActiveProc = LCTR_PROC_INVALID;
          pCtx->cmnState = LCTR_CMN_STATE_IDLE;
          pCtx->llcpState = LCTR_LLCP_STATE_IDLE;

          lctrStartPendingLlcp(pCtx);
          break;

        case LCTR_PC_EVT_RECV_REQ:
          LL_TRACE_WARN1("Procedure collision; pending incomplete request, activeProc=%u", pCtx->llcpActiveProc);
          lctrExecAction(pCtx, proc, LCTR_PC_ACT_RECV_REQ);
          if (lctrFeatureAvail(pCtx, proc, event))
          {
            lctrPclProcTbl[lctrGetPclProcSmIndex(proc)][LCTR_PC_ACT_SEND_RSP](pCtx);
          }
          else
          {
            lctrSendRejectInd(pCtx, LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE, FALSE);
          }
          break;

        default:
          /* No action. */
          break;
      }
      break;

    default:
      break;
  }

  return TRUE;
}
