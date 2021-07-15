/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  LLCP state machine implementation file.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
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

/*************************************************************************************************/
/*!
 *  This state machine combines several functionally common LLCP procedures into a single state
 *  machine for the purpose of optimizing code size and provide common code path for for similar
 *  procedures.
 *
 *  Both master and slave operations are handled. Common state behavior is defined in
 *  lctrLlcpExecuteCommonSm() which is typically a host initiated procedure, request exchange,
 *  response exchange and host completion notification.
 *
 *  Actions for each common procedure is defined in tables.
 */
/*************************************************************************************************/

#include "lctr_int_conn.h"
#include "lctr_int_conn_slave.h"
#include "ll_init_api.h"
#include "lmgr_api.h"
#include "util/bstream.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Common LLCP events. */
enum
{
  LCTR_PROC_CMN_EVT_HOST_START, /*!< Host initiated start procedure event. */
  LCTR_PROC_CMN_EVT_INT_START,  /*!< LL initiated start procedure event. */
  LCTR_PROC_CMN_EVT_INT_INCOMP, /*!< Complete an interrupted procedure event. */
  LCTR_PROC_CMN_EVT_RECV_IND,   /*!< Receive indication event. */
  LCTR_PROC_CMN_EVT_RECV_REQ,   /*!< Receive request event. */
  LCTR_PROC_CMN_EVT_RECV_RSP,   /*!< Receive response event. */
  LCTR_PROC_CMN_EVT_PROC_COMP,  /*!< Procedure completed event. */
  LCTR_PROC_CMN_EVT_REJECT,     /*!< Procedure rejected event. */
  LCTR_PROC_CMN_EVT_TOTAL,      /*!< Total common procedure events. */
  LCTR_PROC_CMN_EVT_INVALID = 0xFF  /*!< Invalid event. */
};

/*! \brief      Common LLCP actions. */
enum
{
  LCTR_PROC_CMN_ACT_API_PARAM,  /*!< Store host API parameter action. */
  LCTR_PROC_CMN_ACT_SEND_REQ,   /*!< Send request action. */
  LCTR_PROC_CMN_ACT_RECV_REQ,   /*!< Receive request action. */
  LCTR_PROC_CMN_ACT_SEND_RSP,   /*!< Send response action. */
  LCTR_PROC_CMN_ACT_RECV_RSP,   /*!< Receive response action. */
  LCTR_PROC_CMN_ACT_TOTAL       /*!< Total common procedure actions. */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Common procedure state machine action table. */
const lctrLlcpEh_t lctrCmnProcTbl[LCTR_PROC_CMN_TOTAL][LCTR_PROC_CMN_ACT_TOTAL] =
{
  /* LCTR_PROC_CMN_TERM */
  {
    lctrStoreDisconnectReason,  /* LCTR_PROC_CMN_ACT_API_PARAM */
    lctrSendTerminateInd,       /* LCTR_PROC_CMN_ACT_SEND_REQ */
    lctrStoreTerminateReason,   /* LCTR_PROC_CMN_ACT_RECV_REQ */
    NULL,                       /* LCTR_PROC_CMN_ACT_SEND_RSP */
    NULL                        /* LCTR_PROC_CMN_ACT_RECV_RSP */
  },
  /* LCTR_PROC_CMN_CH_MAP_UPD */
  {
    lctrStoreChanMapUpdate,     /* LCTR_PROC_CMN_ACT_API_PARAM */
    lctrSendChanMapUpdateInd,   /* LCTR_PROC_CMN_ACT_SEND_REQ */
    lctrStoreChanMap,           /* LCTR_PROC_CMN_ACT_RECV_REQ */
    NULL,                       /* LCTR_PROC_CMN_ACT_SEND_RSP */
    NULL                        /* LCTR_PROC_CMN_ACT_RECV_RSP */
  },
  /* LCTR_PROC_CMN_FEAT_EXCH */
  {
    NULL,                       /* LCTR_PROC_CMN_ACT_API_PARAM */
    lctrSendFeatureReq,         /* LCTR_PROC_CMN_ACT_SEND_REQ */
    lctrStoreUsedFeatures,      /* LCTR_PROC_CMN_ACT_RECV_REQ */
    lctrSendFeatureRsp,         /* LCTR_PROC_CMN_ACT_SEND_RSP */
    lctrStoreUsedFeatures       /* LCTR_PROC_CMN_ACT_RECV_RSP */
  },
  /* LCTR_PROC_CMN_VER_EXCH */
  {
    NULL,                       /* LCTR_PROC_CMN_ACT_API_PARAM */
    lctrSendVersionInd,         /* LCTR_PROC_CMN_ACT_SEND_REQ */
    lctrStoreRemoteVer,         /* LCTR_PROC_CMN_ACT_RECV_REQ */
    lctrSendVersionInd,         /* LCTR_PROC_CMN_ACT_SEND_RSP */
    lctrStoreRemoteVer          /* LCTR_PROC_CMN_ACT_RECV_RSP */
  },
  /* LCTR_PROC_CMN_DATA_LEN_UPD */
  {
    lctrStoreLocalDataLength,   /* LCTR_PROC_CMN_ACT_API_PARAM */
    lctrSendDataLengthReq,      /* LCTR_PROC_CMN_ACT_SEND_REQ */
    lctrStoreRemoteDataLength,  /* LCTR_PROC_CMN_ACT_RECV_REQ */
    lctrSendDataLengthRsp,      /* LCTR_PROC_CMN_ACT_SEND_RSP */
    lctrStoreRemoteDataLength   /* LCTR_PROC_CMN_ACT_RECV_RSP */
  },
  /* LCTR_PROC_CMN_SET_MIN_USED_CHAN */
  {
    NULL,                       /* LCTR_PROC_CMN_ACT_API_PARAM */
    lctrSendSetMinUsedChanInd,  /* LCTR_PROC_CMN_ACT_SEND_REQ */
    lctrStoreSetMinUsedChan,    /* LCTR_PROC_CMN_ACT_RECV_REQ */
    NULL,                       /* LCTR_PROC_CMN_ACT_SEND_RSP */
    NULL                        /* LCTR_PROC_CMN_ACT_RECV_RSP */
  },
  /* LCTR_PROC_CMN_PER_ADV_SYNC_TRSF */
  {
    lctrActStorePeriodicSyncTrsf, /* LCTR_PROC_CMN_ACT_API_PARAM */
    lctrActSendPeriodicSyncInd,   /* LCTR_PROC_CMN_ACT_SEND_REQ */
    lctrActReceivePeriodicSyncInd,/* LCTR_PROC_CMN_ACT_RECV_REQ */
    NULL,                         /* LCTR_PROC_CMN_ACT_SEND_RSP */
    NULL                          /* LCTR_PROC_CMN_ACT_RECV_RSP */
  },
  /* LCTR_PROC_CMN_REQ_PEER_SCA */
  {
    lctrStoreScaAction,         /* LCTR_PROC_CMN_ACT_API_PARAM */
    lctrSendPeerScaReq,         /* LCTR_PROC_CMN_ACT_SEND_REQ */
    lctrStorePeerSca,           /* LCTR_PROC_CMN_ACT_RECV_REQ */
    lctrSendPeerScaRsp,         /* LCTR_PROC_CMN_ACT_SEND_RSP */
    lctrStorePeerSca,           /* LCTR_PROC_CMN_ACT_RECV_RSP */
  },
  /* LCTR_PROC_CMN_CH_CLASS_REPORTING */
  {
    NULL,                       /* LCTR_PROC_CMN_ACT_API_PARAM */
    lctrSendChannelReportingInd,
                                /* LCTR_PROC_CMN_ACT_SEND_REQ */
    lctrStoreChannelReportingInd,
                                /* LCTR_PROC_CMN_ACT_RECV_REQ */
    NULL,                       /* LCTR_PROC_CMN_ACT_SEND_RSP */
    NULL                        /* LCTR_PROC_CMN_ACT_RECV_RSP */
  },
  /* LCTR_PROC_CMN_CH_STATUS_REPORT */
  {
    NULL,                       /* LCTR_PROC_CMN_ACT_API_PARAM */
    lctrSendChannelStatusInd,   /* LCTR_PROC_CMN_ACT_SEND_REQ */
    lctrStoreChannelStatusInd,  /* LCTR_PROC_CMN_ACT_RECV_REQ */
    NULL,                       /* LCTR_PROC_CMN_ACT_SEND_RSP */
    NULL                        /* LCTR_PROC_CMN_ACT_RECV_RSP */
  }
};

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
  if (lctrCmnProcTbl[proc][act])
  {
    lctrCmnProcTbl[proc][act](pCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get next pending procedure.
 *
 *  \param      pCtx    Connection context.
 *
 *  \return     Next procedure ID or LCTR_PROC_INVALID if none pending.
 */
/*************************************************************************************************/
static uint8_t lctrGetNextPendingProc(lctrConnCtx_t *pCtx)
{
  uint8_t proc;

  if (pCtx->state == LCTR_CONN_STATE_TERMINATING)
  {
    /* Do not start any new procedures when terminating. */
    return LCTR_PROC_INVALID;
  }

  /* Only start termination procedure when termination pending. */
  if (pCtx->llcpIncompMask & (1 << LCTR_PROC_CMN_TERM))
  {
    pCtx->llcpIncompMask &= ~(1 << LCTR_PROC_CMN_TERM);
    return LCTR_PROC_CMN_TERM;
  }
  if (pCtx->llcpPendMask & (1 << LCTR_PROC_CMN_TERM))
  {
    pCtx->llcpPendMask &= ~(1 << LCTR_PROC_CMN_TERM);
    return LCTR_PROC_CMN_TERM;
  }

  /* First, complete incomplete procedures. */
  proc = 0;
  do
  {
    if (pCtx->llcpIncompMask & (1 << proc))
    {
      pCtx->llcpIncompMask &= ~(1 << proc);
      return proc;
    }
  } while (++proc < LCTR_PROC_CMN_TOTAL);

  /* Next, complete pending procedures. */
  proc = 0;
  do
  {
    if (pCtx->llcpPendMask & (1 << proc))
    {
      pCtx->llcpPendMask &= ~(1 << proc);
      /* Restore the states and continue the previous procedure. */
      if (pCtx->llcpIsOverridden == TRUE)
      {
        pCtx->llcpIsOverridden = FALSE;
        if (pCtx->cmnState != LCTR_CMN_STATE_IDLE)
        {
          pCtx->llcpState = LCTR_LLCP_STATE_BUSY;
        }
        pCtx->llcpActiveProc = proc;
      }
      return proc;
    }
  } while (++proc < LCTR_PROC_CMN_TOTAL);

  return LCTR_PROC_INVALID;
}

/*************************************************************************************************/
/*!
 *  \brief      Get common LLCP procedure ID.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   Event.
 *
 *  \return     LLCP procedure event ID.
 */
/*************************************************************************************************/
static uint8_t lctrGetCmnProcId(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    case LCTR_CONN_MSG_API_CHAN_MAP_UPDATE:
      return LCTR_PROC_CMN_CH_MAP_UPD;

    case LCTR_CONN_MSG_API_REMOTE_FEATURE:
    case LCTR_CONN_LLCP_FEATURE_EXCH:
      return LCTR_PROC_CMN_FEAT_EXCH;

    case LCTR_CONN_MSG_API_SET_MIN_USED_CHAN:
      return LCTR_PROC_CMN_SET_MIN_USED_CHAN;

    case LCTR_CONN_MSG_API_PER_ADV_SYNC_TRSF:
      return LCTR_PROC_CMN_PER_ADV_SYNC_TRSF;

    case LCTR_CONN_MSG_API_REMOTE_VERSION:
    case LCTR_CONN_LLCP_VERSION_EXCH:
      return LCTR_PROC_CMN_VER_EXCH;

    case LCTR_CONN_MSG_API_REQ_PEER_SCA:
      return LCTR_PROC_CMN_REQ_PEER_SCA;

    case LCTR_CONN_LLCP_TERM:
    case LCTR_CONN_MSG_API_DISCONNECT:
      return LCTR_PROC_CMN_TERM;

    case LCTR_CONN_MSG_API_DATA_LEN_CHANGE:
    case LCTR_CONN_LLCP_LENGTH_EXCH:
      return LCTR_PROC_CMN_DATA_LEN_UPD;

    case LCTR_CONN_LLCP_CHANNEL_STATUS:
      return LCTR_PROC_CMN_CH_STATUS_REPORT;

    case LCTR_CONN_LLCP_CHANNEL_REPORTING:
      return LCTR_PROC_CMN_CH_CLASS_REPORTING;

    case LCTR_CONN_LLCP_PROC_CMPL:
      return pCtx->llcpActiveProc;

    case LCTR_CONN_MSG_RX_LLCP:
      switch (lctrDataPdu.opcode)
      {
        case LL_PDU_CHANNEL_MAP_IND:
          return LCTR_PROC_CMN_CH_MAP_UPD;

        case LL_PDU_FEATURE_REQ:
        case LL_PDU_FEATURE_RSP:
        case LL_PDU_SLV_FEATURE_REQ:
          return LCTR_PROC_CMN_FEAT_EXCH;

        case LL_PDU_VERSION_IND:
          return LCTR_PROC_CMN_VER_EXCH;

        case LL_PDU_TERMINATE_IND:
          return LCTR_PROC_CMN_TERM;

        case LL_PDU_LENGTH_REQ:
        case LL_PDU_LENGTH_RSP:
          return LCTR_PROC_CMN_DATA_LEN_UPD;

        case LL_PDU_MIN_USED_CHAN_IND:
          return LCTR_PROC_CMN_SET_MIN_USED_CHAN;

        case LL_PDU_PERIODIC_SYNC_IND:
          return LCTR_PROC_CMN_PER_ADV_SYNC_TRSF;

        case LL_PDU_PEER_SCA_REQ:
        case LL_PDU_PEER_SCA_RSP:
          return LCTR_PROC_CMN_REQ_PEER_SCA;

        case LL_PDU_CH_REPORTING_IND:
          return LCTR_PROC_CMN_CH_CLASS_REPORTING;

        case LL_PDU_CH_STATUS_IND:
          return LCTR_PROC_CMN_CH_STATUS_REPORT;

        case LL_PDU_UNKNOWN_RSP:
          switch (lctrDataPdu.pld.unknownRsp.unknownType)
          {
            case LL_PDU_CHANNEL_MAP_IND:
              return LCTR_PROC_CMN_CH_MAP_UPD;

            case LL_PDU_FEATURE_REQ:
            case LL_PDU_FEATURE_RSP:
            case LL_PDU_SLV_FEATURE_REQ:
              return LCTR_PROC_CMN_FEAT_EXCH;

            case LL_PDU_VERSION_IND:
              return LCTR_PROC_CMN_VER_EXCH;

            case LL_PDU_TERMINATE_IND:
              return LCTR_PROC_CMN_TERM;

            case LL_PDU_LENGTH_REQ:
            case LL_PDU_LENGTH_RSP:
              return LCTR_PROC_CMN_DATA_LEN_UPD;

            case LL_PDU_PWR_CHANGE_IND:
              return LCTR_PROC_PWR_IND;

            case LL_PDU_PWR_CTRL_RSP:
            case LL_PDU_PWR_CTRL_REQ:
              return LCTR_PROC_PWR_CTRL;

            case LL_PDU_CH_REPORTING_IND:
              return LCTR_PROC_CMN_CH_CLASS_REPORTING;

            case LL_PDU_CH_STATUS_IND:
              return LCTR_PROC_CMN_CH_STATUS_REPORT;

            default:
              break;
          }
          break;

        case LL_PDU_REJECT_IND:
        case LL_PDU_REJECT_EXT_IND:
          return pCtx->llcpActiveProc;

        default:
          break;
      }

      break;

    case LCTR_CONN_LLCP_START_PENDING:
      return lctrGetNextPendingProc(pCtx);

    default:
      break;
  }

  return LCTR_PROC_INVALID;
}

/*************************************************************************************************/
/*!
 *  \brief      Get remapped common LLCP procedure event.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   Subsystem event.
 *
 *  \return     Common LLCP procedure action ID.
 *
 *  This routine remaps events for the common LLCP procedure state machine for optimized
 *  compressed state tables.
 */
/*************************************************************************************************/
static uint8_t lctrRemapCmnProcEvent(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    case LCTR_CONN_MSG_API_CHAN_MAP_UPDATE:
    case LCTR_CONN_MSG_API_REMOTE_FEATURE:
    case LCTR_CONN_MSG_API_REMOTE_VERSION:
    case LCTR_CONN_MSG_API_DISCONNECT:
    case LCTR_CONN_MSG_API_DATA_LEN_CHANGE:
    case LCTR_CONN_MSG_API_SET_MIN_USED_CHAN:
    case LCTR_CONN_MSG_API_PER_ADV_SYNC_TRSF:
    case LCTR_CONN_MSG_API_REQ_PEER_SCA:
      return LCTR_PROC_CMN_EVT_HOST_START;

    case LCTR_CONN_LLCP_FEATURE_EXCH:
    case LCTR_CONN_LLCP_VERSION_EXCH:
    case LCTR_CONN_LLCP_LENGTH_EXCH:
    case LCTR_CONN_LLCP_TERM:
    case LCTR_CONN_LLCP_CHANNEL_STATUS:
    case LCTR_CONN_LLCP_CHANNEL_REPORTING:
      return LCTR_PROC_CMN_EVT_INT_START;

    case LCTR_CONN_LLCP_PROC_CMPL:
      return LCTR_PROC_CMN_EVT_PROC_COMP;

    case LCTR_CONN_MSG_RX_LLCP_UNKNOWN:
      return LCTR_PROC_CMN_EVT_RECV_REQ;

    case LCTR_CONN_MSG_RX_LLCP:
      switch (lctrDataPdu.opcode)
      {
        case LL_PDU_CHANNEL_MAP_IND:
        case LL_PDU_FEATURE_REQ:
        case LL_PDU_SLV_FEATURE_REQ:
        case LL_PDU_LENGTH_REQ:
        case LL_PDU_PEER_SCA_REQ:
        case LL_PDU_CH_REPORTING_IND:
        case LL_PDU_CH_STATUS_IND:
          return LCTR_PROC_CMN_EVT_RECV_REQ;

        case LL_PDU_TERMINATE_IND:
        case LL_PDU_VERSION_IND:
        case LL_PDU_MIN_USED_CHAN_IND:
        case LL_PDU_PERIODIC_SYNC_IND:
          return LCTR_PROC_CMN_EVT_RECV_IND;

        case LL_PDU_FEATURE_RSP:
        case LL_PDU_LENGTH_RSP:
        case LL_PDU_PEER_SCA_RSP:
          return LCTR_PROC_CMN_EVT_RECV_RSP;

        case LL_PDU_UNKNOWN_RSP:
        case LL_PDU_REJECT_IND:
        case LL_PDU_REJECT_EXT_IND:
          return LCTR_PROC_CMN_EVT_REJECT;

        default:
          return LCTR_PROC_CMN_EVT_INVALID;
      }

    case LCTR_CONN_LLCP_START_PENDING:
      if (pCtx->llcpIncompMask)
      {
        return LCTR_PROC_CMN_EVT_INT_INCOMP;
      }
      else
      {
        return LCTR_PROC_CMN_EVT_INT_START;
      }

    default:
      break;
  }

  return LCTR_PROC_CMN_EVT_INVALID;
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
    case LCTR_PROC_CMN_VER_EXCH:
      if (event == LCTR_PROC_CMN_EVT_INT_INCOMP)
      {
        result = (pCtx->numSentVerInd == 0) ? TRUE : FALSE;
      }
      else
      {
        result = !pCtx->remoteVerValid;
      }
      break;
    case LCTR_PROC_CMN_FEAT_EXCH:
      if ((pCtx->role == LL_ROLE_SLAVE) &&
          (event == LCTR_PROC_CMN_EVT_HOST_START) &&
          ((pCtx->usedFeatSet & LL_FEAT_SLV_INIT_FEAT_EXCH) == 0))
      {
        LL_TRACE_WARN1("Requested LL_FEAT_SLV_INIT_FEAT_EXCH not available, usedFeatSet=0x%08x", pCtx->usedFeatSet);
        result = FALSE;
      }
      break;
    case LCTR_PROC_CMN_DATA_LEN_UPD:
      if ((pCtx->usedFeatSet & LL_FEAT_DATA_LEN_EXT) == 0)
      {
        LL_TRACE_WARN1("Requested LCTR_PROC_CMN_DATA_LEN_UPD not available, usedFeatSet=0x%08x", pCtx->usedFeatSet);
        result = FALSE;
      }
      break;
    case LCTR_PROC_CMN_CH_MAP_UPD:
      if (pCtx->role == LL_ROLE_SLAVE)
      {
        if (event == LCTR_PROC_CMN_EVT_HOST_START)
        {
          LL_TRACE_WARN1("Slave cannot initiate LLCP proc=%u", proc);
          result = FALSE;
        }
      }
      else
      {
        if (event == LCTR_PROC_CMN_EVT_RECV_REQ)
        {
          LL_TRACE_WARN1("Master can only initiate LLCP proc=%u", proc);
          result = FALSE;
        }
      }
      break;
    case LCTR_PROC_CMN_SET_MIN_USED_CHAN:
      if (pCtx->role == LL_ROLE_MASTER)
      {
        if (event == LCTR_PROC_CMN_EVT_HOST_START)
        {
          LL_TRACE_WARN1("Master cannot initiate this LLCP proc=%u", proc);
          result = FALSE;
        }
      }
      else
      {
        if (event == LCTR_PROC_CMN_EVT_RECV_REQ)
        {
          LL_TRACE_WARN1("Slave cannot receive this LLCP proc=%u", proc);
          result = FALSE;
        }
        else if ((event == LCTR_PROC_CMN_EVT_HOST_START) &&
                 (pCtx->usedFeatSet & LL_FEAT_MIN_NUM_USED_CHAN) == 0)
        {
          LL_TRACE_WARN1("Requested LCTR_PROC_CMN_SET_MIN_USED_CHAN not available, usedFeatSet=0x%08x", pCtx->usedFeatSet);
          result = FALSE;
        }
      }
      break;
    case LCTR_PROC_CMN_PER_ADV_SYNC_TRSF:
      if ((event == LCTR_PROC_CMN_EVT_HOST_START) &&
          ((lmgrCb.features & LL_FEAT_PAST_SENDER) == 0))
      {
        LL_TRACE_WARN1("Requested LCTR_PROC_CMN_PER_ADV_SYNC_TRSF not available, FeatSet=0x%08x", lmgrCb.features);
        result = FALSE;
      }
      else if (event == LCTR_PROC_CMN_EVT_RECV_REQ)
      {
        LL_TRACE_WARN1("Slave cannot receive this LLCP proc=%u", proc);
        result = FALSE;
      }
      break;
    case LCTR_PROC_CMN_REQ_PEER_SCA:
      if ((pCtx->usedFeatSet & LL_FEAT_SCA_UPDATE) == 0)
      {
        LL_TRACE_WARN1("Requested LCTR_PROC_CMN_REQ_PEER_SCA not available, usedFeatSet=0x%08x", pCtx->usedFeatSet);
        result = FALSE;
      }
      break;
    case LCTR_PROC_CMN_CH_CLASS_REPORTING:
    case LCTR_PROC_CMN_CH_STATUS_REPORT:
      if ((pCtx->usedFeatSet & LL_FEAT_CHANNEL_CLASSIFICATION) == 0)
      {
        LL_TRACE_WARN1("Requested LL_FEAT_CHANNEL_CLASSIFICATION not available, usedFeatSet=0x%08x", pCtx->usedFeatSet);
        result = FALSE;
      }
      break;
    case LCTR_PROC_CMN_TERM:
    default:
      break;
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of read remote feature confirm.
 *
 *  \param      pCtx    Connection context.
 *  \param      status  Status.
 */
/*************************************************************************************************/
static void lctrNotifyHostReadRemoteFeatCnf(lctrConnCtx_t *pCtx, uint8_t status)
{
  const uint16_t handle = LCTR_GET_CONN_HANDLE(pCtx);

  LlReadRemoteFeatCnf_t evt =
  {
    .hdr =
    {
      .param        = handle,
      .event        = LL_READ_REMOTE_FEAT_CNF,
      .status       = status
    },

    .handle = handle,
    .status = status
  };

  uint8_t *pBuf = evt.features;
  if (BT_VER < LL_VER_BT_CORE_SPEC_5_0)
  {
    UINT64_TO_BSTREAM(pBuf, pCtx->usedFeatSet);
  }
  else
  {
    UINT64_TO_BSTREAM(pBuf, pCtx->peerFeatures);
  }

  LL_TRACE_INFO2("### LlEvent ###  LL_READ_REMOTE_FEAT_CNF, handle=%u, status=%u", handle, status);

  LmgrSendEvent((LlEvt_t *)&evt);
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
    case LCTR_PROC_CMN_FEAT_EXCH:
      lctrNotifyHostReadRemoteFeatCnf(pCtx, LL_SUCCESS);
      break;
    case LCTR_PROC_CMN_VER_EXCH:
      lctrNotifyHostReadRemoteVerCnf(pCtx);
      break;
    case LCTR_PROC_CMN_DATA_LEN_UPD:
      /* We have already notified the host if the effective lengths changed. */
      break;
    case LCTR_PROC_CMN_REQ_PEER_SCA:
      lctrNotifyHostPeerScaCnf(pCtx);
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
      /* Do not transmit another length request if the peer does not recognize it. */
      if (proc == LCTR_PROC_CMN_DATA_LEN_UPD)
      {
        pCtx->usedFeatSet &= ~LL_FEAT_DATA_LEN_EXT;
      }
      break;
    default:
      break;
  }

  switch (proc)
  {
    case LCTR_PROC_CMN_FEAT_EXCH:
      lctrNotifyHostReadRemoteFeatCnf(pCtx, reason);
      break;
    case LCTR_PROC_CMN_DATA_LEN_UPD:
      /* Do not notify host; the event has no status parameter. */
      break;
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Execute common LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 *
 *  \return     TRUE if handled by this SM, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrLlcpExecuteCommonSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  uint8_t proc;
  uint8_t llEvent = event;

  if ((event = lctrRemapCmnProcEvent(pCtx, event)) == LCTR_PROC_CMN_EVT_INVALID)
  {
    return FALSE;
  }

  if ((proc = lctrGetCmnProcId(pCtx, llEvent)) == LCTR_PROC_INVALID)
  {
    return FALSE;
  }

  switch (pCtx->llcpState)
  {
    case LCTR_LLCP_STATE_IDLE:
      LL_TRACE_INFO3("lctrLlcpExecuteCommonSm: handle=%u, proc=%u, state=IDLE, event=%u", LCTR_GET_CONN_HANDLE(pCtx), proc, event);

      switch (event)
      {
        case LCTR_PROC_CMN_EVT_HOST_START:
          pCtx->llcpNotifyMask |= 1 << proc;
          lctrExecAction(pCtx, proc, LCTR_PROC_CMN_ACT_API_PARAM);
          /* Fallthrough */
        case LCTR_PROC_CMN_EVT_INT_START:
          if (lctrFeatureAvail(pCtx, proc, event))
          {
            lctrExecAction(pCtx, proc, LCTR_PROC_CMN_ACT_SEND_REQ);
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

        case LCTR_PROC_CMN_EVT_INT_INCOMP:
        case LCTR_PROC_CMN_EVT_RECV_IND:
        case LCTR_PROC_CMN_EVT_RECV_REQ:
          if (lctrFeatureAvail(pCtx, proc, event))
          {
            if (event != LCTR_PROC_CMN_EVT_INT_INCOMP)
            {
              lctrExecAction(pCtx, proc, LCTR_PROC_CMN_ACT_RECV_REQ);
            }

            if (lctrCmnProcTbl[proc][LCTR_PROC_CMN_ACT_SEND_RSP])
            {
              /* Procedure completes; no transition to BUSY. */
              lctrCmnProcTbl[proc][LCTR_PROC_CMN_ACT_SEND_RSP](pCtx);
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

        case LCTR_PROC_CMN_EVT_RECV_RSP:
        case LCTR_PROC_CMN_EVT_PROC_COMP:
        case LCTR_PROC_CMN_EVT_REJECT:
        default:
          /* No action required. */
          break;
      }
      break;

    case LCTR_LLCP_STATE_BUSY:
      LL_TRACE_INFO3("lctrLlcpExecuteCommonSm: handle=%u, proc=%u, state=BUSY, event=%u", LCTR_GET_CONN_HANDLE(pCtx), proc, event);

      switch (event)
      {
        case LCTR_PROC_CMN_EVT_HOST_START:
        case LCTR_PROC_CMN_EVT_INT_START:
          if (lctrFeatureAvail(pCtx, proc, event))
          {
            if (event == LCTR_PROC_CMN_EVT_HOST_START)
            {
              pCtx->llcpNotifyMask |= 1 << proc;
            }

            if (pCtx->llcpActiveProc != proc)
            {
              LL_TRACE_INFO1("Pending procedure in progress: activeProc=%u", pCtx->llcpActiveProc);
              pCtx->llcpPendMask |= 1 << proc;
            }
            if (event == LCTR_PROC_CMN_EVT_HOST_START)
            {
              lctrExecAction(pCtx, proc, LCTR_PROC_CMN_ACT_API_PARAM);
            }
            if (proc == LCTR_PROC_CMN_TERM)
            {
              pCtx->llcpPendMask &= ~(1 << proc);
              pCtx->llcpIncompMask |= 1 << proc;
              lctrExecAction(pCtx, proc, LCTR_PROC_CMN_ACT_SEND_REQ);
            }
          }
          else
          {
            lctrNotifyHostReject(pCtx, proc);
          }
          break;

        case LCTR_PROC_CMN_EVT_RECV_IND:        /* Completion from message */
          if (proc != pCtx->llcpActiveProc)
          {
            if (lctrFeatureAvail(pCtx, proc, event))
            {
              LL_TRACE_WARN1("Procedure collision; pending incomplete request, activeProc=%u", pCtx->llcpActiveProc);
              lctrExecAction(pCtx, proc, LCTR_PROC_CMN_ACT_RECV_REQ);
              pCtx->llcpIncompMask |= 1 << proc;
            }
            break;
          }
          /* Fallthrough */
        case LCTR_PROC_CMN_EVT_RECV_RSP:
          if (proc == pCtx->llcpActiveProc)
          {
            /* Incoming procedure matches the active one. */
            lctrExecAction(pCtx, proc, LCTR_PROC_CMN_ACT_RECV_RSP);
          }
          else if (pCtx->llcpPendMask & (1 << proc))
          {
            /* Incoming procedure matches one of the pended one. */
            pCtx->llcpPendMask &= ~(1 << proc);
            if (pCtx->llcpIsOverridden == TRUE)
            {
              pCtx->llcpIsOverridden = FALSE;
            }
            lctrExecAction(pCtx, proc, LCTR_PROC_CMN_ACT_RECV_RSP);
            lctrNotifyHostSuccess(pCtx, proc);
            break;
          }
          else
          {
            LL_TRACE_ERR1("Unexpected response packet, expected llcpActiveProc=%u", pCtx->llcpActiveProc);
            break;
          }
          /* Fallthrough */
        case LCTR_PROC_CMN_EVT_PROC_COMP:       /* Completion from CE */
        case LCTR_PROC_CMN_EVT_REJECT:          /* Failed completion */
          if (event == LCTR_PROC_CMN_EVT_REJECT)
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

        case LCTR_PROC_CMN_EVT_RECV_REQ:
          LL_TRACE_WARN1("Procedure collision; pending incomplete request, activeProc=%u", pCtx->llcpActiveProc);
          lctrExecAction(pCtx, proc, LCTR_PROC_CMN_ACT_RECV_REQ);
          if ((pCtx->llcpActiveProc != LCTR_PROC_ENCRYPT) &&
              lctrCmnProcTbl[proc][LCTR_PROC_CMN_ACT_SEND_RSP])
          {
            if (lctrFeatureAvail(pCtx, proc, event))
            {
              lctrCmnProcTbl[proc][LCTR_PROC_CMN_ACT_SEND_RSP](pCtx);
            }
            else
            {
              lctrSendRejectInd(pCtx, LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE, FALSE);
            }
          }
          else
          {
            pCtx->llcpIncompMask |= 1 << proc;
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

  /* Enter terminating state if and only if TERMINATE_IND has been Rx'd or queued for Tx. */
  if (proc == LCTR_PROC_CMN_TERM)
  {
    if ((pCtx->llcpActiveProc == LCTR_PROC_CMN_TERM) || (pCtx->llcpIncompMask & (1 << LCTR_PROC_CMN_TERM)))
    {
      pCtx->state = LCTR_CONN_STATE_TERMINATING;    /* update parent state */
    }
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Check if encryption can override common procedures.
 *
 *  \param      pCtx    Connection context.
 *
 *  \return     TRUE if override possible, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrSlvCheckEncOverrideCommonParam(lctrConnCtx_t *pCtx)
{
  bool_t result = FALSE;

  switch (pCtx->llcpActiveProc)
  {
    case LCTR_PROC_CMN_FEAT_EXCH:
    case LCTR_PROC_CMN_VER_EXCH:
    case LCTR_PROC_CMN_DATA_LEN_UPD:
      result = TRUE;
      break;

    default:
      break;
  }

  return result;
}
