/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master connection state machine implementation file.
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

#include "lctr_int_conn_master.h"
#include "lctr_int_conn_slave.h"
#include "wsf_trace.h"

#include <string.h>

/**************************************************************************************************
  Type Definitions
**************************************************************************************************/

/*! \brief      Action function call signature. */
typedef void (*lctrActFn_t)(lctrConnCtx_t *pCtx);

/*! \brief      Connection parameter/update states. */
enum
{
  LCTR_CU_STATE_IDLE,                   /*!< Idle state. */
  LCTR_CU_STATE_HOST_REPLY,             /*!< Wait for host reply state. */
  LCTR_CU_STATE_CONN_PARAM_RSP,         /*!< Wait for LL_CONN_PARAM_RSP state. */
  LCTR_CU_STATE_CONN_UPD_INSTANT,       /*!< Wait for connection update instant state. */
  LCTR_CU_STATE_TOTAL                   /*!< Total connection parameter/update states. */
};

/*! \brief      Connection parameter/update events. */
enum
{
  LCTR_CU_EVENT_HOST_CONN_UPD,          /*!< Received host connection update command. */
  LCTR_CU_EVENT_HOST_REPLY,             /*!< Received host connection parameter reply. */
  LCTR_CU_EVENT_HOST_NEG_REPLY,         /*!< Received host connection parameter negative reply. */
  LCTR_CU_EVENT_PEER_CONN_PARAM_REQ,    /*!< Received peer LL_CONN_PARAM_REQ. */
  LCTR_CU_EVENT_PEER_CONN_PARAM_RSP,    /*!< Received peer LL_CONN_PARAM_RSP. */
  LCTR_CU_EVENT_PEER_REJECT,            /*!< Received peer LL_REJECT_IND or LL_UNKNOWN_RSP. */
  LCTR_CU_EVENT_INT_PROC_COMP,          /*!< Procedure completion event. */
  LCTR_CU_EVENT_INT_SKIP_CONN_PARAM,    /*!< Skip connection parameter procedure. */
  LCTR_CU_EVENT_INT_START_CONN_UPD,     /*!< Start pending host connection update procedure. */
  LCTR_CU_EVENT_INT_START_CONN_PARAM,   /*!< Start pending peer connection parameter procedure. */
  LCTR_CU_EVENT_INT_REJECT_CONN_UPD,    /*!< Reject connection update procedure. */
  LCTR_CU_EVENT_TOTAL,                  /*!< Total connection parameter/update events. */
  LCTR_CU_EVENT_INVALID = 0xFF          /*!< Invalid event. */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Master LLCP state machine table. */
LctrLlcpHdlr_t lctrMstLlcpSmTbl[LCTR_LLCP_SM_TOTAL];

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for start connection update.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActStartConnUpd(lctrConnCtx_t *pCtx)
{
#if (LL_ENABLE_TESTER)
  if (llTesterCb.connUpdIndEnabled)
  {
    /* Transition SM to send LL_CONN_UPD_IND. */
    lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_SKIP_CONN_PARAM);
    return;
  }
#endif

  /* Zero values to ignore connParamReq fields if bypassed. */
  memset(&pCtx->connParam, 0, sizeof(pCtx->connParam));

  if (pCtx->usedFeatSet & LL_FEAT_CONN_PARAM_REQ_PROC)
  {
    lctrSendConnParamReq(pCtx);
    lctrStartLlcpTimer(pCtx);
  }
  else
  {
    /* Transition SM to send LL_CONN_UPD_IND. */
    lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_SKIP_CONN_PARAM);
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
  pCtx->replyWaitingMsk |= LCTR_HOST_REPLY_CONN_PARAM_REQ;
  lctrStoreConnParamReq(pCtx);
  lctrNotifyHostConnParamInd(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for start connection parameter command.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActStartConnParam(lctrConnCtx_t *pCtx)
{
  pCtx->replyWaitingMsk |= LCTR_HOST_REPLY_CONN_PARAM_REQ;
  lctrNotifyHostConnParamInd(pCtx);
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
  pCtx->peerReplyWaiting = TRUE;
  pCtx->replyWaitingMsk &= ~LCTR_HOST_REPLY_CONN_PARAM_REQ;
  lctrStopLlcpTimer(pCtx);
  lctrStoreConnParamSpec(pCtx);
  lctrSendConnUpdateInd(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received peer LL_CONN_PARAM_RSP.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPeerConnParamRsp(lctrConnCtx_t *pCtx)
{
  pCtx->peerReplyWaiting = TRUE;
  lctrStopLlcpTimer(pCtx);
  lctrStoreConnParamRsp(pCtx);
  lctrSendConnUpdateInd(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for skipping peer LL_CONN_PARAM_RSP.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActSkipConnParamRsp(lctrConnCtx_t *pCtx)
{
  lctrStopLlcpTimer(pCtx);
  lctrSendConnUpdateInd(pCtx);
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

/*************************************************************************************************/
/*!
 *  \brief      Notify host of local rejected connection parameter procedure.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActLocalRejectConnParam(lctrConnCtx_t *pCtx)
{
  if (pCtx->llcpNotifyMask &= (1 << LCTR_PROC_CONN_UPD))
  {
    pCtx->llcpNotifyMask &= ~(1 << LCTR_PROC_CONN_UPD);
    lctrNotifyHostConnUpdateInd(pCtx, LL_ERROR_CODE_UNACCEPTABLE_CONN_INTERVAL);
  }

  if (pCtx->peerReplyWaiting)
  {
    pCtx->peerReplyWaiting = FALSE;
    lctrSendRejectInd(pCtx, LL_ERROR_CODE_UNACCEPTABLE_CONN_INTERVAL, TRUE);
  }
}

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      State machine action table. */
static const lctrActFn_t lctrMstConnUpdActionTbl[LCTR_CU_STATE_TOTAL][LCTR_CU_EVENT_TOTAL] =
{
  { /* LCTR_CU_STATE_IDLE */
    lctrActHostConnUpd,                 /* LCTR_CU_EVENT_HOST_CONN_UPD */
    NULL,                               /* LCTR_CU_EVENT_HOST_REPLY */
    NULL,                               /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    lctrActPeerConnParam,               /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    NULL,                               /* LCTR_CU_EVENT_PEER_CONN_PARAM_RSP */
    NULL,                               /* LCTR_CU_EVENT_PEER_REJECT */
    NULL,                               /* LCTR_CU_EVENT_INT_PROC_COMP */
    NULL,                               /* LCTR_CU_EVENT_INT_SKIP_CONN_PARAM */
    lctrActStartConnUpd,                /* LCTR_CU_EVENT_INT_START_CONN_UPD */
    lctrActStartConnParam,              /* LCTR_CU_EVENT_INT_START_CONN_PARAM */
    NULL                                /* LCTR_CU_EVENT_INT_REJECT_CONN_UPD */
  },
  { /* LCTR_CU_STATE_HOST_REPLY */
    lctrActHostConnUpd,                 /* LCTR_CU_EVENT_HOST_CONN_UPD */
    lctrActHostReply,                   /* LCTR_CU_EVENT_HOST_REPLY */
    lctrActHostNegReply,                /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    NULL,                               /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    NULL,                               /* LCTR_CU_EVENT_PEER_CONN_PARAM_RSP */
    NULL,                               /* LCTR_CU_EVENT_PEER_REJECT */
    NULL,                               /* LCTR_CU_EVENT_INT_PROC_COMP */
    NULL,                               /* LCTR_CU_EVENT_INT_SKIP_CONN_PARAM */
    NULL,                               /* LCTR_CU_EVENT_INT_START_CONN_UPD */
    NULL,                               /* LCTR_CU_EVENT_INT_START_CONN_PARAM */
    NULL                                /* LCTR_CU_EVENT_INT_REJECT_CONN_UPD */
  },
  { /* LCTR_CU_STATE_CONN_PARAM_RSP */
    lctrActConnUpdDisallow,             /* LCTR_CU_EVENT_HOST_CONN_UPD */
    NULL,                               /* LCTR_CU_EVENT_HOST_REPLY */
    NULL,                               /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    lctrActRejectCollision,             /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    lctrActPeerConnParamRsp,            /* LCTR_CU_EVENT_PEER_CONN_PARAM_RSP */
    lctrActPeerRejectConnParam,         /* LCTR_CU_EVENT_PEER_REJECT */
    NULL,                               /* LCTR_CU_EVENT_INT_PROC_COMP */
    lctrActSkipConnParamRsp,            /* LCTR_CU_EVENT_INT_SKIP_CONN_PARAM */
    NULL,                               /* LCTR_CU_EVENT_INT_START_CONN_UPD */
    NULL,                               /* LCTR_CU_EVENT_INT_START_CONN_PARAM */
    NULL                                /* LCTR_CU_EVENT_INT_REJECT_CONN_UPD */
  },
  { /* LCTR_CU_STATE_CONN_UPD_INSTANT */
    lctrActConnUpdDisallow,             /* LCTR_CU_EVENT_HOST_CONN_UPD */
    NULL,                               /* LCTR_CU_EVENT_HOST_REPLY */
    NULL,                               /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    lctrActRejectCollision,             /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    lctrActRejectCollision,             /* LCTR_CU_EVENT_PEER_CONN_PARAM_RSP */
    NULL,                               /* LCTR_CU_EVENT_PEER_REJECT */
    lctrActNotifyHostConnUpdSuccess,    /* LCTR_CU_EVENT_INT_PROC_COMP */
    NULL,                               /* LCTR_CU_EVENT_INT_SKIP_CONN_PARAM */
    NULL,                               /* LCTR_CU_EVENT_INT_START_CONN_UPD */
    NULL,                               /* LCTR_CU_EVENT_INT_START_CONN_PARAM */
    lctrActLocalRejectConnParam         /* LCTR_CU_EVENT_INT_REJECT_CONN_UPD */
  }
};

/*! \brief      State machine next state table. */
static const uint8_t lctrMstConnUpdNextStateTbl[LCTR_CU_STATE_TOTAL][LCTR_CU_EVENT_TOTAL] =
{
  { /* LCTR_CU_STATE_IDLE */
    LCTR_CU_STATE_CONN_PARAM_RSP,       /* LCTR_CU_EVENT_HOST_CONN_UPD */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_HOST_REPLY */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_PEER_CONN_PARAM_RSP */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_PEER_REJECT */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_INT_PROC_COMP */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_INT_SKIP_CONN_PARAM */
    LCTR_CU_STATE_CONN_PARAM_RSP,       /* LCTR_CU_EVENT_INT_START_CONN_UPD */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_INT_START_CONN_PARAM */
    LCTR_CU_STATE_IDLE                  /* LCTR_CU_EVENT_INT_REJECT_CONN_UPD */
  },
  { /* LCTR_CU_STATE_HOST_REPLY */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_HOST_CONN_UPD */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_HOST_REPLY */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_PEER_CONN_PARAM_RSP */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_PEER_REJECT */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_INT_PROC_COMP */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_INT_SKIP_CONN_PARAM */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_INT_START_CONN_UPD */
    LCTR_CU_STATE_HOST_REPLY,           /* LCTR_CU_EVENT_INT_START_CONN_PARAM */
    LCTR_CU_STATE_HOST_REPLY            /* LCTR_CU_EVENT_INT_REJECT_CONN_UPD */
  },
  { /* LCTR_CU_STATE_CONN_PARAM_RSP */
    LCTR_CU_STATE_CONN_PARAM_RSP,       /* LCTR_CU_EVENT_HOST_CONN_UPD */
    LCTR_CU_STATE_CONN_PARAM_RSP,       /* LCTR_CU_EVENT_HOST_REPLY */
    LCTR_CU_STATE_CONN_PARAM_RSP,       /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    LCTR_CU_STATE_CONN_PARAM_RSP,       /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_PEER_CONN_PARAM_RSP */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_PEER_REJECT */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_INT_PROC_COMP */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_INT_SKIP_CONN_PARAM */
    LCTR_CU_STATE_CONN_PARAM_RSP,       /* LCTR_CU_EVENT_INT_START_CONN_UPD */
    LCTR_CU_STATE_CONN_PARAM_RSP,       /* LCTR_CU_EVENT_INT_START_CONN_PARAM */
    LCTR_CU_STATE_CONN_PARAM_RSP        /* LCTR_CU_EVENT_INT_REJECT_CONN_UPD */
  },
  { /* LCTR_CU_STATE_CONN_UPD_INSTANT */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_HOST_CONN_UPD */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_HOST_REPLY */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_HOST_NEG_REPLY */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_PEER_CONN_PARAM_REQ */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_PEER_CONN_PARAM_RSP */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_PEER_REJECT */
    LCTR_CU_STATE_IDLE,                 /* LCTR_CU_EVENT_INT_PROC_COMP */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_INT_SKIP_CONN_PARAM */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_INT_START_CONN_UPD */
    LCTR_CU_STATE_CONN_UPD_INSTANT,     /* LCTR_CU_EVENT_INT_START_CONN_PARAM */
    LCTR_CU_STATE_IDLE                  /* LCTR_CU_EVENT_INT_REJECT_CONN_UPD */
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
  if (lctrMstConnUpdActionTbl[pCtx->connUpdState][event])
  {
    lctrMstConnUpdActionTbl[pCtx->connUpdState][event](pCtx);
  }

  pCtx->connUpdState = lctrMstConnUpdNextStateTbl[pCtx->connUpdState][event];
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
static uint8_t lctrMstConnUpdRemapEvent(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    /*** Peer messages ***/

    case LCTR_CONN_MSG_RX_LLCP:
      switch (lctrDataPdu.opcode)
      {
        case LL_PDU_CONN_PARAM_REQ:
          return LCTR_CU_EVENT_PEER_CONN_PARAM_REQ;
        case LL_PDU_CONN_PARAM_RSP:
          return LCTR_CU_EVENT_PEER_CONN_PARAM_RSP;

        case LL_PDU_UNKNOWN_RSP:
          if (lctrDataPdu.pld.unknownRsp.unknownType == LL_PDU_CONN_PARAM_REQ)
          {
            /* Remember this remote device does not support Connection Parameters Request procedure. */
            pCtx->usedFeatSet &= ~LL_FEAT_CONN_PARAM_REQ_PROC;
            return LCTR_CU_EVENT_INT_SKIP_CONN_PARAM;
          }
          /* Not for this SM. */
          break;
        case LL_PDU_REJECT_IND:
          if (pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD)
          {
            return LCTR_CU_EVENT_INT_SKIP_CONN_PARAM;
          }
          /* Probably not for this SM. */
          break;
        case LL_PDU_REJECT_EXT_IND:
          if (lctrDataPdu.pld.rejInd.opcode == LL_PDU_CONN_PARAM_REQ)
          {
            return LCTR_CU_EVENT_INT_SKIP_CONN_PARAM;
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
    case LCTR_CONN_LLCP_SKIP_CONN_PARAM:
      if (pCtx->llcpActiveProc == LCTR_PROC_CONN_UPD)
      {
        return LCTR_CU_EVENT_INT_SKIP_CONN_PARAM;
      }
      break;
    case LCTR_CONN_LLCP_START_PENDING:
      if (pCtx->llcpPendMask == (1 << LCTR_PROC_CONN_UPD))
      {
        pCtx->llcpPendMask &= ~(1 << LCTR_PROC_CONN_UPD);
        return LCTR_CU_EVENT_INT_START_CONN_UPD;
      }
      if (pCtx->llcpPendMask == (1 << LCTR_PROC_CONN_PARAM))
      {
        pCtx->llcpPendMask &= ~(1 << LCTR_PROC_CONN_PARAM);
        return LCTR_CU_EVENT_INT_START_CONN_PARAM;
      }
      break;
    case LCTR_CONN_LLCP_REJECT_CONN_UPD:
      return LCTR_CU_EVENT_INT_REJECT_CONN_UPD;
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
    case LCTR_CU_EVENT_PEER_CONN_PARAM_REQ:
      if (pCtx->llcpActiveProc == LCTR_PROC_ENCRYPT)
      {
        lctrStoreConnParamReq(pCtx);
        pCtx->llcpPendMask |= 1 << LCTR_PROC_CONN_PARAM;
        LL_TRACE_INFO2("Pending CONN_PARAM=%u procedure: activeProc=%u", LCTR_PROC_CONN_PARAM, pCtx->llcpActiveProc);
      }
      else
      {
        lctrSendRejectInd(pCtx, LL_ERROR_CODE_DIFFERENT_TRANSACTION_COLLISION, TRUE);
      }
      break;

    case LCTR_CU_EVENT_HOST_CONN_UPD:
      lctrStoreConnUpdateSpec(pCtx);
      pCtx->llcpPendMask |= 1 << LCTR_PROC_CONN_UPD;
      LL_TRACE_INFO2("Pending CONN_UPD=%u procedure: activeProc=%u", LCTR_PROC_CONN_UPD, pCtx->llcpActiveProc);
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
static void lctrMstCheckProcOverride(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
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
 *  \brief      Execute master connection update LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 *
 *  \return     TRUE if peer response pending, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrMstLlcpExecuteConnUpdSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  if ((event = lctrMstConnUpdRemapEvent(pCtx, event)) == LCTR_CU_EVENT_INVALID)
  {
    return FALSE;
  }

  switch (pCtx->llcpState)
  {
    case LCTR_LLCP_STATE_IDLE:
      LL_TRACE_INFO3("lctrMstLlcpExecuteConnUpdSm: handle=%u, llcpState=IDLE, connUpdState=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->connUpdState, event);

      lctrExecAction(pCtx, event);

      if (pCtx->connUpdState != LCTR_CU_STATE_IDLE)
      {
        pCtx->llcpState = LCTR_LLCP_STATE_BUSY;
        pCtx->llcpActiveProc = LCTR_PROC_CONN_UPD;
        pCtx->llcpInstantComp = FALSE;
      }
      break;

    case LCTR_LLCP_STATE_BUSY:
      LL_TRACE_INFO3("lctrMstLlcpExecuteConnUpdSm: handle=%u, llcpState=BUSY, connUpdState=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->connUpdState, event);

      lctrMstCheckProcOverride(pCtx, event);

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
 *  \brief      Execute master LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 */
/*************************************************************************************************/
void lctrMstLlcpExecuteSm(lctrConnCtx_t *pCtx, uint8_t event)
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

  if (!(lctrMstLlcpSmTbl[LCTR_LLCP_SM_ENCRYPT]      && lctrMstLlcpSmTbl[LCTR_LLCP_SM_ENCRYPT](pCtx, event)) &&
      !(lctrMstLlcpSmTbl[LCTR_LLCP_SM_PING]         && lctrMstLlcpSmTbl[LCTR_LLCP_SM_PING](pCtx, event)) &&
      !(lctrMstLlcpSmTbl[LCTR_LLCP_SM_ENH_CONN_UPD] && lctrMstLlcpSmTbl[LCTR_LLCP_SM_ENH_CONN_UPD](pCtx, event)) &&
      !(lctrMstLlcpSmTbl[LCTR_LLCP_SM_CONN_UPD]     && lctrMstLlcpSmTbl[LCTR_LLCP_SM_CONN_UPD](pCtx, event)) &&
      !(lctrMstLlcpSmTbl[LCTR_LLCP_SM_PHY_UPD]      && lctrMstLlcpSmTbl[LCTR_LLCP_SM_PHY_UPD](pCtx, event)) &&
      !(lctrMstLlcpSmTbl[LCTR_LLCP_SM_CIS_EST]      && lctrMstLlcpSmTbl[LCTR_LLCP_SM_CIS_EST](pCtx, event)) &&
      !(lctrMstLlcpSmTbl[LCTR_LLCP_SM_CIS_TERM]     && lctrMstLlcpSmTbl[LCTR_LLCP_SM_CIS_TERM](pCtx, event)) &&
      !(lctrMstLlcpSmTbl[LCTR_LLCP_SM_PC]           && lctrMstLlcpSmTbl[LCTR_LLCP_SM_PC](pCtx, event)) &&
      !(lctrMstLlcpSmTbl[LCTR_LLCP_SM_CMN]          && lctrMstLlcpSmTbl[LCTR_LLCP_SM_CMN](pCtx, event)))
  {
    lctrLlcpStatelessEventHandler(pCtx, event);
  }
}
