/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave connection state machine implementation file.
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

#include "lctr_int_conn_slave.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Type Definitions
**************************************************************************************************/

/*! \brief      Action function call signature. */
typedef void (*lctrActFn_t)(lctrConnCtx_t *pCtx);

/*! \brief      Slave connection parameter/update states. */
enum
{
  LCTR_CU_STATE_IDLE,                   /*!< Idle state. */
  LCTR_CU_STATE_HOST_REPLY,             /*!< Wait for host reply state. */
  LCTR_CU_STATE_CONN_UPD_IND,           /*!< Wait for LL_CONN_PARAM_REQ state. */
  LCTR_CU_STATE_CONN_UPD_INSTANT,       /*!< Wait for connection update instant state. */
  LCTR_CU_STATE_TOTAL                   /*!< Total connection parameter/update states. */
};

/*! \brief      Connection parameter/update events. */
enum
{
  LCTR_CU_EVENT_HOST_CONN_UPD,          /*!< Received host connection update command. */
  LCTR_CU_EVENT_PEER_CONN_PARAM_REQ,    /*!< Received peer LL_CONN_PARAM_REQ. */
  LCTR_CU_EVENT_HOST_REPLY,             /*!< Received host connection parameter reply. */
  LCTR_CU_EVENT_HOST_NEG_REPLY,         /*!< Received host connection parameter negative reply. */
  LCTR_CU_EVENT_PEER_CONN_UPD_IND,      /*!< Received peer LL_CONN_UPD_IND. */
  LCTR_CU_EVENT_PEER_REJECT,            /*!< Received peer LL_REJECT_IND or LL_UNKNOWN_RSP. */
  LCTR_CU_EVENT_INT_PROC_COMP,          /*!< Procedure completion event. */
  LCTR_CU_EVENT_INT_START_CONN_UPD,     /*!< Start pending connection update procedure. */
  LCTR_CU_EVENT_TOTAL,                  /*!< Total connection parameter/update events. */
  LCTR_CU_EVENT_INVALID = 0xFF          /*!< Invalid event. */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Slave LLCP state machine table. */
LctrLlcpHdlr_t lctrSlvLlcpSmTbl[LCTR_LLCP_SM_TOTAL];

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for start connection update.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActStartConnUpd(lctrConnCtx_t *pCtx)
{
  if (pCtx->usedFeatSet & LL_FEAT_CONN_PARAM_REQ_PROC)
  {
    lctrSendConnParamReq(pCtx);
    lctrStartLlcpTimer(pCtx);
  }
  else
  {
    lctrNotifyHostConnUpdateInd(pCtx, LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE);
    LL_TRACE_WARN1("Connection parameter request feature not supported by connection, handle=%u", LCTR_GET_CONN_HANDLE(pCtx));
    lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_PROC_CMPL);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received host connection update command.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActHostConnUpd(lctrConnCtx_t *pCtx)
{
  pCtx->llcpNotifyMask |= 1 << LCTR_PROC_CONN_UPD;
  lctrStoreConnUpdateSpec(pCtx);
  lctrActStartConnUpd(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received host connection update command.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPeerConnParam(lctrConnCtx_t *pCtx)
{
  lctrStoreConnParamReq(pCtx);

  pCtx->replyWaitingMsk |= LCTR_HOST_REPLY_CONN_PARAM_REQ;
  lctrNotifyHostConnParamInd(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received host connection update command with collision.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPeerConnParamWithCollision(lctrConnCtx_t *pCtx)
{
  lctrActPeerConnParam(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received peer LL_CONN_IND.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPeerConnInd(lctrConnCtx_t *pCtx)
{
  lctrStopLlcpTimer(pCtx);
  lctrStoreConnUpdate(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for host reply command.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActHostReply(lctrConnCtx_t *pCtx)
{
  pCtx->replyWaitingMsk &= ~LCTR_HOST_REPLY_CONN_PARAM_REQ;
  lctrSendConnParamRsp(pCtx);
  lctrStartLlcpTimer(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for host negative reply command.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrActHostNegReply(lctrConnCtx_t *pCtx)
{
  pCtx->replyWaitingMsk &= ~LCTR_HOST_REPLY_CONN_PARAM_REQ;
  lctrSendRejectInd(pCtx, pLctrConnMsg->connParamNegReply.reason, TRUE);
  lctrStopLlcpTimer(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of peer rejected connection parameter procedure.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrActPeerRejectConnParam(lctrConnCtx_t *pCtx)
{
  pCtx->llcpNotifyMask &= ~(1 << LCTR_PROC_CONN_UPD);

  switch (lctrDataPdu.opcode)
  {
    case LL_PDU_REJECT_IND:
    case LL_PDU_REJECT_EXT_IND:
      LL_TRACE_WARN2("Peer rejected connection parameter request, handle=%u, reason=%u", LCTR_GET_CONN_HANDLE(pCtx), lctrDataPdu.pld.rejInd.reason);
      lctrNotifyHostConnUpdateInd(pCtx, lctrDataPdu.pld.rejInd.reason);
      break;
    case LL_PDU_UNKNOWN_RSP:
      LL_TRACE_WARN1("Peer rejected connection parameter request, handle=%u, reason=UNSUPPORTED_REMOTE_FEATURE", LCTR_GET_CONN_HANDLE(pCtx));
      lctrNotifyHostConnUpdateInd(pCtx, LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE);
      break;
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of connection update with success status.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrActNotifyHostConnUpdSuccess(lctrConnCtx_t *pCtx)
{
  if (pCtx->llcpNotifyMask & (1 << LCTR_PROC_CONN_UPD))
  {
    pCtx->llcpNotifyMask &= ~(1 << LCTR_PROC_CONN_UPD);
    lctrNotifyHostConnUpdateInd(pCtx, LL_SUCCESS);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host connection completed due to colliding connection parameter from peer.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActNotifyHostColliding(lctrConnCtx_t *pCtx)
{
  LL_TRACE_WARN1("Host requested connection update collided with peer, handle=%u", LCTR_GET_CONN_HANDLE(pCtx));
  lctrNotifyHostConnUpdateInd(pCtx, lctrDataPdu.pld.rejInd.reason);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of connection update with disallowed status.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActConnUpdDisallow(lctrConnCtx_t *pCtx)
{
  LL_TRACE_WARN1("Host requested connection update while procedure pending, handle=%u", LCTR_GET_CONN_HANDLE(pCtx));
  lctrNotifyHostConnUpdateInd(pCtx, LL_ERROR_CODE_CMD_DISALLOWED);
}

/*************************************************************************************************/
/*!
 *  \brief      Send peer LL_REJECT_IND due to pending operation.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActRejectCollision(lctrConnCtx_t *pCtx)
{
  LL_TRACE_WARN1("Peer requested connection parameter/update while procedure pending, handle=%u", LCTR_GET_CONN_HANDLE(pCtx));
  lctrSendRejectInd(pCtx, LL_ERROR_CODE_LMP_ERR_TRANSACTION_COLLISION, TRUE);
}

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      State machine action table. */
static const lctrActFn_t lctrSlvConnUpdActionTbl[LCTR_CU_STATE_TOTAL][LCTR_CU_EVENT_TOTAL] =
{
  { /* LCTR_CU_STATE_IDLE */
    lctrActHostConnUpd,                 /* LCTR_CU_EVENT_HOST_CONN_UPD */
    lctrActPeerConnParam,               /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    NULL,                               /* LCTR_CU_EVENT_HOST_REPLY */
    NULL,                               /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    lctrActPeerConnInd,                 /* LCTR_CU_EVENT_PEER_CONN_UPD_IND */
    NULL,                               /* LCTR_CU_EVENT_PEER_REJECT */
    NULL,                               /* LCTR_CU_EVENT_INT_PROC_COMP */
    lctrActStartConnUpd                 /* LCTR_CU_EVENT_INT_START_CONN_UPD */
  },
  { /* LCTR_CU_STATE_HOST_REPLY */
    lctrActConnUpdDisallow,             /* LCTR_CU_EVENT_HOST_CONN_UPD */
    lctrActRejectCollision,             /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    lctrActHostReply,                   /* LCTR_CU_EVENT_HOST_REPLY */
    lctrActHostNegReply,                /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    lctrActRejectCollision,             /* LCTR_CU_EVENT_PEER_CONN_UPD_IND */
    lctrActNotifyHostColliding,         /* LCTR_CU_EVENT_PEER_REJECT */
    NULL,                               /* LCTR_CU_EVENT_INT_PROC_COMP */
    lctrActStartConnUpd                 /* LCTR_CU_EVENT_INT_START_CONN_UPD */
  },
  { /* LCTR_CU_STATE_CONN_UPD_IND */
    lctrActConnUpdDisallow,             /* LCTR_CU_EVENT_HOST_CONN_UPD */
    lctrActPeerConnParamWithCollision,  /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    NULL,                               /* LCTR_CU_EVENT_HOST_REPLY */
    NULL,                               /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    lctrActPeerConnInd,                 /* LCTR_CU_EVENT_PEER_CONN_UPD_IND */
    lctrActPeerRejectConnParam,         /* LCTR_CU_EVENT_PEER_REJECT */
    NULL,                               /* LCTR_CU_EVENT_INT_PROC_COMP */
    NULL                                /* LCTR_CU_EVENT_INT_START_CONN_UPD */
  },
  { /* LCTR_CU_STATE_CONN_UPD_INSTANT */
    lctrActConnUpdDisallow,             /* LCTR_CU_EVENT_HOST_CONN_UPD */
    lctrActRejectCollision,             /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    NULL,                               /* LCTR_CU_EVENT_HOST_REPLY */
    NULL,                               /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    lctrActRejectCollision,             /* LCTR_CU_EVENT_PEER_CONN_UPD_IND */
    NULL,                               /* LCTR_CU_EVENT_PEER_REJECT */
    lctrActNotifyHostConnUpdSuccess,    /* LCTR_CU_EVENT_INT_PROC_COMP */
    NULL                                /* LCTR_CU_EVENT_INT_START_CONN_UPD */
  }
};

/*! \brief      State machine next state table. */
static const uint8_t lctrSlvConnUpdNextStateTbl[LCTR_CU_STATE_TOTAL][LCTR_CU_EVENT_TOTAL] =
{
  { /* LCTR_CU_STATE_IDLE */
    LCTR_CU_STATE_CONN_UPD_IND,         /* LCTR_CU_EVENT_HOST_CONN_UPD */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_HOST_REPLY */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_PEER_CONN_UPD_IND */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_PEER_REJECT */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_INT_PROC_COMP */
    LCTR_CU_STATE_IDLE                  /* LCTR_CU_EVENT_INT_START_CONN_UPD */
  },
  { /* LCTR_CU_STATE_HOST_REPLY */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_HOST_CONN_UPD */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    LCTR_CU_STATE_CONN_UPD_IND,         /* LCTR_CU_EVENT_HOST_REPLY */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_PEER_CONN_UPD_IND */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_PEER_REJECT */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_INT_PROC_COMP */
    LCTR_CU_STATE_HOST_REPLY            /* LCTR_CU_EVENT_INT_START_CONN_UPD */
  },
  { /* LCTR_CU_STATE_CONN_UPD_IND */
    LCTR_CU_STATE_CONN_UPD_IND,         /* LCTR_CU_EVENT_HOST_CONN_UPD */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    LCTR_CU_STATE_CONN_UPD_IND,         /* LCTR_CU_EVENT_HOST_REPLY */
    LCTR_CU_STATE_CONN_UPD_IND,         /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_PEER_CONN_UPD_IND */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_PEER_REJECT */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_INT_PROC_COMP */
    LCTR_CU_STATE_CONN_UPD_IND          /* LCTR_CU_EVENT_INT_START_CONN_UPD */
  },
  { /* LCTR_CU_STATE_CONN_UPD_INSTANT */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_HOST_CONN_UPD */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_HOST_REPLY */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_PEER_CONN_UPD_IND */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_PEER_REJECT */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_INT_PROC_COMP */
    LCTR_CU_STATE_CONN_UPD_INSTANT      /* LCTR_CU_EVENT_INT_START_CONN_UPD */
  }
};

/*************************************************************************************************/
/*!
 *  \brief      Execute action function.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   Event ID.
 */
/*************************************************************************************************/
static inline void lctrExecAction(lctrConnCtx_t *pCtx, uint8_t event)
{
  if (lctrSlvConnUpdActionTbl[pCtx->connUpdState][event])
  {
    lctrSlvConnUpdActionTbl[pCtx->connUpdState][event](pCtx);
  }

  pCtx->connUpdState = lctrSlvConnUpdNextStateTbl[pCtx->connUpdState][event];
}

/*************************************************************************************************/
/*!
 *  \brief      Get remapped connection update/parameter LLCP procedure event.
 *
 *  \param      event   Subsystem event.
 *  \param      pCtx    Connection context.
 *
 *  \return     Connection update/parameter LLCP procedure event ID.
 *
 *  This routine remaps events for the connection update LLCP procedure state machine for optimized
 *  compressed state tables.
 */
/*************************************************************************************************/
static uint8_t lctrSlvConnUpdRemapEvent(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    /*** Peer messages ***/

    case LCTR_CONN_MSG_RX_LLCP:
      switch (lctrDataPdu.opcode)
      {
        case LL_PDU_CONN_UPDATE_IND:
          return LCTR_CU_EVENT_PEER_CONN_UPD_IND;
        case LL_PDU_CONN_PARAM_REQ:
          return LCTR_CU_EVENT_PEER_CONN_PARAM_REQ;

        case LL_PDU_UNKNOWN_RSP:
          if (lctrDataPdu.pld.unknownRsp.unknownType == LL_PDU_CONN_PARAM_REQ)
          {
            /* Remember this remote device does not support Connection Parameters Request procedure. */
            pCtx->usedFeatSet &= ~LL_FEAT_CONN_PARAM_REQ_PROC;
            return LCTR_CU_EVENT_PEER_REJECT;
          }
          /* Not for this SM. */
          break;
        case LL_PDU_REJECT_IND:
          if (pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD)
          {
            return LCTR_CU_EVENT_PEER_REJECT;
          }
          /* Probably not for this SM. */
          break;
        case LL_PDU_REJECT_EXT_IND:
          if (lctrDataPdu.pld.rejInd.opcode == LL_PDU_CONN_PARAM_REQ)
          {
            return LCTR_CU_EVENT_PEER_REJECT;
          }
          if (lctrDataPdu.pld.rejInd.opcode == LL_PDU_CONN_PARAM_RSP)
          {
            return LCTR_CU_EVENT_PEER_REJECT;
          }
          /* Not for this SM. */
          break;

        default:
          break;
      }
      break;

    /*** Host messages ***/

    case LCTR_CONN_MSG_API_CONN_UPDATE:
      return LCTR_CU_EVENT_HOST_CONN_UPD;
    case LCTR_CONN_MSG_API_CONN_PARAM_REPLY:
      return LCTR_CU_EVENT_HOST_REPLY;
    case LCTR_CONN_MSG_API_CONN_PARAM_NEG_REPLY:
      return LCTR_CU_EVENT_HOST_NEG_REPLY;

    /*** Internal messages ***/

    case LCTR_CONN_LLCP_PROC_CMPL:
      if (pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD)
      {
        return LCTR_CU_EVENT_INT_PROC_COMP;
      }
      break;
    case LCTR_CONN_LLCP_START_PENDING:
      if (pCtx->llcpPendMask == (1 << LCTR_PROC_CONN_UPD))
      {
        /* Connection update SM is pending; continue the procedure. */
        pCtx->llcpPendMask &= ~(1 << LCTR_PROC_CONN_UPD);
        return LCTR_CU_EVENT_INT_START_CONN_UPD;
      }
      break;

    default:
      break;
  }

  return LCTR_CU_EVENT_INVALID;
}

/*************************************************************************************************/
/*!
 *  \brief      Resolve procedure collision.
 *
 *  \param      event   Subsystem event.
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrResolveCollision(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    case LCTR_CU_EVENT_PEER_CONN_UPD_IND:
      LL_TRACE_WARN2("Rejecting CONN_UPD=%u procedure: activeProc=%u, reason=DIFFERENT_TRANSACTION_COLLISION", LCTR_PROC_CONN_UPD, pCtx->llcpActiveProc);
      lctrSendRejectInd(pCtx, LL_ERROR_CODE_DIFFERENT_TRANSACTION_COLLISION, FALSE);
      break;

    case LCTR_CU_EVENT_PEER_CONN_PARAM_REQ:
      LL_TRACE_WARN2("Rejecting CONN_PARAM=%u procedure: activeProc=%u, reason=DIFFERENT_TRANSACTION_COLLISION", LCTR_PROC_CONN_PARAM, pCtx->llcpActiveProc);
      lctrSendRejectInd(pCtx, LL_ERROR_CODE_DIFFERENT_TRANSACTION_COLLISION, TRUE);
      break;

    case LCTR_CU_EVENT_HOST_CONN_UPD:
      lctrStoreConnUpdateSpec(pCtx);
      pCtx->llcpPendMask |= 1 << LCTR_PROC_CONN_UPD;
      LL_TRACE_INFO2("Pending CONN_UPD=%u procedure: activeProc=%u", LCTR_PROC_CONN_UPD, pCtx->llcpActiveProc);
      break;

    case LCTR_CU_EVENT_PEER_REJECT:
      pCtx->llcpPendMask &= ~(1 << LCTR_PROC_CONN_UPD);
      if (pCtx->connUpdState != LCTR_CU_STATE_IDLE)
      {
        pCtx->connUpdState = LCTR_CU_STATE_IDLE;
      }
      lctrNotifyHostConnUpdateInd(pCtx, lctrDataPdu.pld.rejInd.reason);
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check if active procedure can be overridden with connection update procedure.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   Subsystem event.
 *
 *  This routine will pend the active procedure and allow the connection update procedure to
 *  override it.
 */
/*************************************************************************************************/
static void lctrSlvCheckProcOverride(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    case LCTR_CU_EVENT_PEER_CONN_UPD_IND:
    case LCTR_CU_EVENT_PEER_CONN_PARAM_REQ:
      /* Only the procedure without instant fields can be overridden. */
      switch (pCtx->llcpActiveProc)
      {
        case LCTR_PROC_CMN_VER_EXCH:
        case LCTR_PROC_CMN_FEAT_EXCH:
        case LCTR_PROC_CMN_DATA_LEN_UPD:
        case LCTR_PROC_CMN_REQ_PEER_SCA:
          pCtx->llcpPendMask |= 1 << pCtx->llcpActiveProc;
          pCtx->llcpActiveProc = LCTR_PROC_CONN_UPD;
          pCtx->llcpIsOverridden = TRUE;
          break;
        case LCTR_PROC_PHY_UPD:
          pCtx->llcpPendMask |= 1 << pCtx->llcpActiveProc;
          pCtx->llcpActiveProc = LCTR_PROC_CONN_UPD;
          break;
        default:
          break;
      }
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Execute slave connection update LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 *
 *  \return     TRUE if event belongs to this SM.
 */
/*************************************************************************************************/
bool_t lctrSlvLlcpExecuteConnUpdSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  if ((event = lctrSlvConnUpdRemapEvent(pCtx, event)) == LCTR_CU_EVENT_INVALID)
  {
    return FALSE;
  }

  switch (pCtx->llcpState)
  {
    case LCTR_LLCP_STATE_IDLE:
      LL_TRACE_INFO3("lctrSlvLlcpExecuteConnUpdSm: handle=%u, llcpState=IDLE, connUpdState=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->connUpdState, event);
      lctrExecAction(pCtx, event);

      if (pCtx->connUpdState != LCTR_CU_STATE_IDLE)
      {
        pCtx->llcpState = LCTR_LLCP_STATE_BUSY;
        pCtx->llcpActiveProc = LCTR_PROC_CONN_UPD;
        pCtx->llcpInstantComp = FALSE;
      }
      break;

    case LCTR_LLCP_STATE_BUSY:
      LL_TRACE_INFO3("lctrSlvLlcpExecuteConnUpdSm: handle=%u, llcpState=BUSY, connUpdState=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->connUpdState, event);

      lctrSlvCheckProcOverride(pCtx, event);

      if (pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD)
      {
        lctrExecAction(pCtx, event);

        if (pCtx->connUpdState == LCTR_CU_STATE_IDLE)
        {
          lctrStopLlcpTimer(pCtx);
          pCtx->replyWaitingMsk &= ~LCTR_HOST_REPLY_CONN_PARAM_REQ;
          pCtx->llcpState = LCTR_LLCP_STATE_IDLE;
          pCtx->llcpActiveProc = LCTR_PROC_INVALID;

          lctrStartPendingLlcp(pCtx);
        }
      }
      else
      {
        lctrResolveCollision(pCtx, event);
      }

      break;

    default:
      break;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Execute stateless LLCP event handler.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   Control event.
 */
/*************************************************************************************************/
void lctrLlcpStatelessEventHandler(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    case LCTR_CONN_MSG_RX_LLCP_UNKNOWN:
      LL_TRACE_WARN2("Received unknown LLCP packet handle=%u, opcode=%u", LCTR_GET_CONN_HANDLE(pCtx), lctrDataPdu.opcode);
      lctrSendUnknownRsp(pCtx);
      break;
    case LCTR_CONN_MSG_RX_LLCP_INVALID_PARAM:
      LL_TRACE_WARN2("Received LLCP packet with invalid parameter handle=%u, opcode=%u", LCTR_GET_CONN_HANDLE(pCtx), lctrDataPdu.opcode);
      lctrSendRejectInd(pCtx, LL_ERROR_CODE_INVALID_LMP_PARAMS, FALSE);
      break;
    case LCTR_CONN_TERM_INST_PASSED:
      lctrStoreInstantPassedTerminateReason(pCtx);
      lctrFlagLinkTerm(pCtx);
      break;
    case LCTR_CONN_TMR_LLCP_RSP_EXP:
      lctrStoreLlcpTimeoutTerminateReason(pCtx);
      lctrFlagLinkTerm(pCtx);
      break;
    case LCTR_CONN_MSG_RX_LLCP:
      /* Mismatch between parser and available SM. */
      LL_TRACE_WARN2("Unhandled LLCP packet handle=%u, opcode=%u", LCTR_GET_CONN_HANDLE(pCtx), lctrDataPdu.opcode);
      break;
    case LCTR_CONN_LLCP_START_PENDING:
      /* No action required. */
      break;
    default:
      LL_TRACE_WARN2("Unhandled LLCP SM event, handle=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), event);
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Execute slave LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 */
/*************************************************************************************************/
void lctrSlvLlcpExecuteSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  /* Override state machine */
  switch (event)
  {
    case LCTR_CONN_MSG_API_REMOTE_VERSION:
      if (pCtx->remoteVerValid)
      {
        /* Use cached remote version data. */
        lctrNotifyHostReadRemoteVerCnf(pCtx);
        return;
      }
      break;

    default:
      break;
  }

  if (!(lctrSlvLlcpSmTbl[LCTR_LLCP_SM_ENCRYPT]      && lctrSlvLlcpSmTbl[LCTR_LLCP_SM_ENCRYPT](pCtx, event)) &&
      !(lctrSlvLlcpSmTbl[LCTR_LLCP_SM_PING]         && lctrSlvLlcpSmTbl[LCTR_LLCP_SM_PING](pCtx, event)) &&
      !(lctrSlvLlcpSmTbl[LCTR_LLCP_SM_ENH_CONN_UPD] && lctrSlvLlcpSmTbl[LCTR_LLCP_SM_ENH_CONN_UPD](pCtx, event)) &&
      !(lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CONN_UPD]     && lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CONN_UPD](pCtx, event)) &&
      !(lctrSlvLlcpSmTbl[LCTR_LLCP_SM_PHY_UPD]      && lctrSlvLlcpSmTbl[LCTR_LLCP_SM_PHY_UPD](pCtx, event)) &&
      !(lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CIS_EST]      && lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CIS_EST](pCtx, event)) &&
      !(lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CIS_TERM]     && lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CIS_TERM](pCtx, event)) &&
      !(lctrSlvLlcpSmTbl[LCTR_LLCP_SM_PC]           && lctrSlvLlcpSmTbl[LCTR_LLCP_SM_PC](pCtx, event)) &&
      !(lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CMN]          && lctrSlvLlcpSmTbl[LCTR_LLCP_SM_CMN](pCtx, event)) )
  {
    lctrLlcpStatelessEventHandler(pCtx, event);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check if encryption can override connection update procedure.
 *
 *  \param      pCtx    Connection context.
 *
 *  \return     TRUE if override possible, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrSlvCheckEncOverrideConnParam(lctrConnCtx_t *pCtx)
{
  if ((pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD) &&
      (pCtx->connUpdState == LCTR_CU_STATE_CONN_UPD_IND))
  {
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Check if Slave connection update state machine is at CONN_UPD_INSTANT.
 *
 *  \param      pCtx    Connection context.
 *
 *  \return     TRUE if state machine is at CONN_UPD_INSTANT, FALSE otherwise.
 */
/*************************************************************************************************/
inline bool_t lctrSlvCheckConnUpdInstant(lctrConnCtx_t *pCtx)
{
  return (pCtx->connUpdState == LCTR_CU_STATE_CONN_UPD_INSTANT);
}
