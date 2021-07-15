/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave encryption connection state machine implementation file.
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

#include "lctr_int_enc_slave.h"
#include "lctr_int_conn.h"
#include "lctr_int_conn_slave.h"
#include "lctr_int_slave_phy.h"
#include "lmgr_api.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Action function call signature. */
typedef void (*lctrActFn_t)(lctrConnCtx_t *pCtx);

/*! \brief      Encryption states. */
enum
{
  LCTR_ENC_STATE_UNENCRYPTED,           /*!< Connection established, unencrypted. */
  LCTR_ENC_STATE_FLUSH_START,           /*!< Wait for Tx queue flush on encryption start. */
  LCTR_ENC_STATE_HOST_LTK_REPLY,        /*!< Wait for host LTK reply command. */
  LCTR_ENC_STATE_PEER_START_ENC_RSP,    /*!< Wait for LL_START_ENC_RSP from peer. */
  LCTR_ENC_STATE_ENCRYPTED,             /*!< Connection encrypted. */
  LCTR_ENC_STATE_FLUSH_RESTART,         /*!< Wait for Tx queue flush on encryption restart. */
  LCTR_ENC_STATE_PEER_PAUSE_ENC_REQ,    /*!< Wait for LL_PAUSE_ENC_REQ from peer. */
  LCTR_ENC_STATE_TOTAL                  /*!< Total encryption states. */
};

/*! \brief      Encryption events. */
enum
{
  LCTR_ENC_EVENT_PEER_ENC_REQ,          /*!< Received peer LL_ENC_REQ. */
  LCTR_ENC_EVENT_PEER_START_ENC_RSP,    /*!< Received peer LL_START_ENC_RSP. */
  LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ,    /*!< Received peer LL_PAUSE_ENC_REQ. */
  LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP,    /*!< Received peer LL_PAUSE_ENC_RSP. */
  LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY,    /*!< Received host LE LTK Request Reply command. */
  LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY,/*!< Received host LE LTK Request Negative Reply command. */
  LCTR_ENC_EVENT_ARQ_FLUSHED,           /*!< Internal ARQ queue flushed. */
  LCTR_ENC_EVENT_TOTAL,                 /*!< Total encryption states. */
  LCTR_ENC_EVENT_INVALID = 0xFF         /*!< Invalid event. */
};

/*! \brief      LE Ping states. */
enum
{
  LCTR_PING_STATE_IDLE,                 /*!< Idle state. */
  LCTR_PING_STATE_WAIT_FOR_RSP          /*!< Waiting for LE_PING_RSP. */
};

/*! \brief      LE Ping events. */
enum
{
  LCTR_PING_EVENT_SEND_REQ,             /*!< Send a LL_LE_PING_REQ. */
  LCTR_PING_EVENT_RECV_REQ,             /*!< Received peer LL_LE_PING_REQ. */
  LCTR_PING_EVENT_RECV_RSP,             /*!< Received peer LL_LE_PING_RSP. */
  LCTR_PING_EVENT_PING_PERIOD_EXP,      /*!< Ping period timer expired. */
  LCTR_PING_EVENT_AUTH_PAYLOAD_EXP,     /*!< Authentication payload timer expired. */
  LCTR_PING_EVENT_TOTAL,                /*!< Total encryption states. */
  LCTR_PING_EVENT_INVALID = 0xFF        /*!< Invalid event. */
};

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received peer LL_ENC_REQ.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPeerEncReq(lctrConnCtx_t *pCtx)
{
  lctrPauseTxData(pCtx);
  lctrPauseRxData(pCtx);
  lctrCheckPauseComplete(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action Tx data queue flushed after a start encryption.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActTxFlushedOnStart(lctrConnCtx_t *pCtx)
{
  lctrDisableTxDataEnc(pCtx);
  lctrDisableRxDataEnc(pCtx);
  lctrGenerateSlvVectors(pCtx);
  lctrSendEncRsp(pCtx);
  lctrStartLlcpTimer(pCtx);

  pCtx->replyWaitingMsk |= LCTR_HOST_REPLY_LTK_REQ;
  lctrEncNotifyHostLtkReqInd(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received host LTK request reply HCI command.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActHostLtkReply(lctrConnCtx_t *pCtx)
{
  pCtx->replyWaitingMsk &= ~LCTR_HOST_REPLY_LTK_REQ;

  lctrStoreLtkReply(pCtx);
  lctrCalcSessionKey(pCtx);
  lctrEnableRxDataEnc(pCtx);
  lctrSendStartEncReq(pCtx);
  lctrStartLlcpTimer(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received host LTK request negative reply HCI command.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActHostLtkNegReply(lctrConnCtx_t *pCtx)
{
  pCtx->replyWaitingMsk &= ~LCTR_HOST_REPLY_LTK_REQ;

  lctrStopLlcpTimer(pCtx);

  if (!pCtx->keyUpdFlag)
  {
    lctrSendRejectInd(pCtx, LL_ERROR_CODE_PIN_KEY_MISSING, FALSE);
  }
  else
  {
    lctrStoreLtkNegRepTerminateReason(pCtx);
    lctrLlcpExecuteCommonSm(pCtx, LCTR_CONN_LLCP_TERM);
  }

  lctrUnpauseTxData(pCtx);
  lctrUnpauseRxData(pCtx);

  pCtx->keyUpdFlag = FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received peer LL_START_ENC_RSP.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPeerStartEncRsp(lctrConnCtx_t *pCtx)
{
  lctrStopLlcpTimer(pCtx);
  lctrEnableTxDataEnc(pCtx);
  lctrSendStartEncRsp(pCtx);
  lctrUnpauseTxData(pCtx);
  lctrUnpauseRxData(pCtx);

  (pCtx->keyUpdFlag) ? lctrNotifyEncKeyRefreshInd(pCtx) : lctrNotifyEncChangeInd(pCtx, LL_SUCCESS);
  pCtx->keyUpdFlag = FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received peer LL_PAUSE_ENC_REQ.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPeerPauseEncReq(lctrConnCtx_t *pCtx)
{
  lctrDisableRxDataEnc(pCtx);
  lctrPauseTxData(pCtx);
  lctrPauseRxData(pCtx);
  lctrCheckPauseComplete(pCtx);

  pCtx->keyUpdFlag = TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Action Tx data queue flushed after a restart encryption.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActTxFlushedOnRestart(lctrConnCtx_t *pCtx)
{
  lctrSendPauseEncRsp(pCtx);
  lctrStartLlcpTimer(pCtx);
}

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      State machine action table. */
static const lctrActFn_t lctrSlvEncActionTbl[LCTR_ENC_STATE_TOTAL][LCTR_ENC_EVENT_TOTAL] =
{
  { /* LCTR_ENC_STATE_UNENCRYPTED */
    lctrActPeerEncReq,                  /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    NULL                                /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_FLUSH_START */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    lctrActTxFlushedOnStart             /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_HOST_LTK_REPLY */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    lctrActHostLtkReply,                /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    lctrActHostLtkNegReply,             /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    NULL                                /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_PEER_START_ENC_RSP */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    lctrActPeerStartEncRsp,             /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    NULL                                /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_ENCRYPTED */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    lctrActPeerPauseEncReq,             /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    NULL                                /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_FLUSH_RESTART */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    lctrActTxFlushedOnRestart           /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_PEER_PAUSE_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    lctrInvalidEncPduSeq,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    lctrDisableTxDataEnc,               /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    NULL,                               /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    NULL                                /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  }
};

/*! \brief      State machine next state table. */
static const uint8_t lctrSlvEncNextStateTbl[LCTR_ENC_STATE_TOTAL][LCTR_ENC_EVENT_TOTAL] =
{
  { /* LCTR_ENC_STATE_UNENCRYPTED */
    LCTR_ENC_STATE_FLUSH_START,         /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    LCTR_ENC_STATE_UNENCRYPTED,         /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    LCTR_ENC_STATE_UNENCRYPTED,         /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    LCTR_ENC_STATE_UNENCRYPTED,         /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    LCTR_ENC_STATE_UNENCRYPTED,         /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    LCTR_ENC_STATE_UNENCRYPTED,         /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    LCTR_ENC_STATE_UNENCRYPTED          /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_FLUSH_START */
    LCTR_ENC_STATE_FLUSH_START,         /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    LCTR_ENC_STATE_FLUSH_START,         /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    LCTR_ENC_STATE_FLUSH_START,         /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    LCTR_ENC_STATE_FLUSH_START,         /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    LCTR_ENC_STATE_FLUSH_START,         /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    LCTR_ENC_STATE_FLUSH_START,         /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    LCTR_ENC_STATE_HOST_LTK_REPLY       /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_HOST_LTK_REPLY */
    LCTR_ENC_STATE_HOST_LTK_REPLY,      /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    LCTR_ENC_STATE_HOST_LTK_REPLY,      /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    LCTR_ENC_STATE_HOST_LTK_REPLY,      /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    LCTR_ENC_STATE_HOST_LTK_REPLY,      /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    LCTR_ENC_STATE_PEER_START_ENC_RSP,  /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    LCTR_ENC_STATE_UNENCRYPTED,         /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    LCTR_ENC_STATE_HOST_LTK_REPLY       /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_PEER_START_ENC_RSP */
    LCTR_ENC_STATE_PEER_START_ENC_RSP,  /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    LCTR_ENC_STATE_ENCRYPTED,           /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    LCTR_ENC_STATE_PEER_START_ENC_RSP,  /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    LCTR_ENC_STATE_PEER_START_ENC_RSP,  /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    LCTR_ENC_STATE_PEER_START_ENC_RSP,  /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    LCTR_ENC_STATE_PEER_START_ENC_RSP,  /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    LCTR_ENC_STATE_PEER_START_ENC_RSP   /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_ENCRYPTED */
    LCTR_ENC_STATE_ENCRYPTED,           /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    LCTR_ENC_STATE_ENCRYPTED,           /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    LCTR_ENC_STATE_FLUSH_RESTART,       /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    LCTR_ENC_STATE_ENCRYPTED,           /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    LCTR_ENC_STATE_ENCRYPTED,           /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    LCTR_ENC_STATE_ENCRYPTED,           /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    LCTR_ENC_STATE_ENCRYPTED            /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_FLUSH_RESTART */
    LCTR_ENC_STATE_FLUSH_RESTART,       /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    LCTR_ENC_STATE_FLUSH_RESTART,       /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    LCTR_ENC_STATE_FLUSH_RESTART,       /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    LCTR_ENC_STATE_FLUSH_RESTART,       /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    LCTR_ENC_STATE_FLUSH_RESTART,       /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    LCTR_ENC_STATE_FLUSH_RESTART,       /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    LCTR_ENC_STATE_PEER_PAUSE_ENC_REQ   /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_ENC_STATE_PEER_PAUSE_ENC_REQ */
    LCTR_ENC_STATE_PEER_PAUSE_ENC_REQ,  /* LCTR_ENC_EVENT_PEER_ENC_REQ */
    LCTR_ENC_STATE_PEER_PAUSE_ENC_REQ,  /* LCTR_ENC_EVENT_PEER_START_ENC_RSP */
    LCTR_ENC_STATE_PEER_PAUSE_ENC_REQ,  /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ */
    LCTR_ENC_STATE_UNENCRYPTED,         /* LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP */
    LCTR_ENC_STATE_PEER_PAUSE_ENC_REQ,  /* LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY */
    LCTR_ENC_STATE_PEER_PAUSE_ENC_REQ,  /* LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY */
    LCTR_ENC_STATE_PEER_PAUSE_ENC_REQ   /* LCTR_ENC_EVENT_ARQ_FLUSHED */
  }
};

/*************************************************************************************************/
/*!
 *  \brief      Get remapped encryption LLCP procedure event.
 *
 *  \param      event   Subsystem event.
 *  \param      pCtx    Connection context.
 *
 *  \return     Encryption LLCP procedure event ID.
 *
 *  This routine remaps events for the encryption LLCP procedure state machine for optimized
 *  compressed state tables.
 */
/*************************************************************************************************/
static uint8_t lctrSlvRemapEncryptEvent(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    /*** Peer messages ***/

    case LCTR_CONN_MSG_RX_LLCP:
      switch (lctrDataPdu.opcode)
      {
        case LL_PDU_ENC_REQ:
          return LCTR_ENC_EVENT_PEER_ENC_REQ;
        case LL_PDU_START_ENC_RSP:
          return LCTR_ENC_EVENT_PEER_START_ENC_RSP;
        case LL_PDU_PAUSE_ENC_REQ:
          return LCTR_ENC_EVENT_PEER_PAUSE_ENC_REQ;
        case LL_PDU_PAUSE_ENC_RSP:
          return LCTR_ENC_EVENT_PEER_PAUSE_ENC_RSP;
        default:
          break;
      }
      break;

    /*** Host messages ***/

    case LCTR_CONN_MSG_API_LTK_REPLY:
      return LCTR_ENC_EVENT_HOST_LTK_REQ_REPLY;
    case LCTR_CONN_MSG_API_LTK_NEG_REPLY:
      return LCTR_ENC_EVENT_HOST_LTK_REQ_NEG_REPLY;

    /*** Internal messages ***/

    case LCTR_CONN_ARQ_Q_FLUSHED:
      if (pCtx->llcpActiveProc == LCTR_PROC_ENCRYPT)
      {
        return LCTR_ENC_EVENT_ARQ_FLUSHED;
      }
      break;

    default:
      break;
  }

  return LCTR_ENC_EVENT_INVALID;
}

/*************************************************************************************************/
/*!
 *  \brief      Check if active procedure can be overridden with encryption procedure.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   Subsystem event.
 *
 *  This routine will pend the active procedure and allow the encryption procedure to override it.
 */
/*************************************************************************************************/
static void lctrSlvCheckProcOverride(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    case LCTR_ENC_EVENT_PEER_ENC_REQ:
      if (lctrSlvCheckEncOverrideConnParam(pCtx) ||
          lctrSlvCheckEncOverrideCommonParam(pCtx) ||
          ((lctrSlvCheckEncOverridePhyUpdateFn == NULL) || lctrSlvCheckEncOverridePhyUpdateFn(pCtx)))
      {
        pCtx->llcpPendMask |= 1 << pCtx->llcpActiveProc;
        pCtx->llcpActiveProc = LCTR_PROC_ENCRYPT;
      }
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get remapped LE Ping LLCP procedure event.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   Subsystem event.
 *
 *  \return     LE Ping LLCP procedure event ID.
 *
 *  This routine remaps events for the LE Ping LLCP procedure state machine for optimized
 *  compressed state tables.
 */
/*************************************************************************************************/
static uint8_t lctrRemapPingEvent(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    /*** Peer messages ***/

    case LCTR_CONN_MSG_RX_LLCP:
      switch (lctrDataPdu.opcode)
      {
        case LL_PDU_PING_REQ:
          return LCTR_PING_EVENT_RECV_REQ;

        case LL_PDU_UNKNOWN_RSP:
          if (lctrDataPdu.pld.unknownRsp.unknownType != LL_PDU_PING_REQ)
          {
            break;
          }
          /* treat UNKNOWN_RSP like LE_PING_RSP */
          /* Fallthrough */
        case LL_PDU_PING_RSP:
          return LCTR_PING_EVENT_RECV_RSP;

        default:
          break;
      }
      break;

    /*** Timers ***/

    case LCTR_CONN_TMR_PING_PERIOD_EXP:
      return LCTR_PING_EVENT_PING_PERIOD_EXP;
    case LCTR_CONN_TMR_AUTH_PAYLOAD_EXP:
      return LCTR_PING_EVENT_AUTH_PAYLOAD_EXP;

    /*** Internal messages ***/

    case LCTR_CONN_LLCP_START_PENDING:
      if (pCtx->llcpPendMask & (1 << LCTR_PROC_LE_PING))
      {
        pCtx->llcpPendMask &= ~(1 << LCTR_PROC_LE_PING);
        return LCTR_PING_EVENT_SEND_REQ;
      }
      break;

    default:
      break;
  }

  return LCTR_PING_EVENT_INVALID;
}

/*************************************************************************************************/
/*!
 *  \brief      Execute encryption slave LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 *
 *  \return     TRUE if peer response pending, FALSE otherwise.
 */
/*************************************************************************************************/
static void lctrSlvEncryptSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  if (lctrSlvEncActionTbl[pCtx->encState][event])
  {
    lctrSlvEncActionTbl[pCtx->encState][event](pCtx);
  }

  pCtx->encState = lctrSlvEncNextStateTbl[pCtx->encState][event];
}

/*************************************************************************************************/
/*!
 *  \brief      Execute encryption slave LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 *
 *  \return     TRUE if handled by this SM, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrSlvExecuteEncryptSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  if (pCtx->pauseRxData == TRUE)        /* During encryption start or encryption pause. */
  {
    switch (event)
    {
      case LCTR_CONN_MSG_RX_LLCP:
        switch (lctrDataPdu.opcode)
        {
          /* Allowed PDUs. */
          case LL_PDU_TERMINATE_IND:
          case LL_PDU_REJECT_IND:
          case LL_PDU_REJECT_EXT_IND:
          case LL_PDU_ENC_REQ:
          case LL_PDU_START_ENC_RSP:
          case LL_PDU_PAUSE_ENC_REQ:
          case LL_PDU_PAUSE_ENC_RSP:
            break;

          /* Illegal PDUs */
          default:
            LL_TRACE_WARN2("Unexpected PDU received during encryption procedure, handle=%u, opcode=%u", LCTR_GET_CONN_HANDLE(pCtx), lctrDataPdu.opcode);
            lctrInvalidEncPduSeq(pCtx);
            return FALSE;
        }
        break;

      /* Illegal PDUs. */
      case LCTR_CONN_MSG_RX_LLCP_UNKNOWN:
      case LCTR_CONN_MSG_RX_LLCP_INVALID_PARAM:
        LL_TRACE_WARN1("Unknown/Invalid PDU received during encryption procedure event=%u", event);
        lctrInvalidEncPduSeq(pCtx); /* Transit to standby state and notify the host */
        return TRUE;                /* Skip the lctrLlcpStatelessEventHandler */

      default:
        break;
    }
  }

  if (((lmgrCb.features & LL_FEAT_ENCRYPTION) == 0) ||
      ((event = lctrSlvRemapEncryptEvent(pCtx, event)) == LCTR_ENC_EVENT_INVALID))
  {
    return FALSE;
  }

  switch (pCtx->llcpState)
  {
    case LCTR_LLCP_STATE_IDLE:
      LL_TRACE_INFO3("lctrSlvExecuteEncryptSm: handle=%u, llcpState=IDLE, encState=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->encState, event);

      lctrSlvEncryptSm(pCtx, event);

      if ((pCtx->encState != LCTR_ENC_STATE_UNENCRYPTED) &&
          (pCtx->encState != LCTR_ENC_STATE_ENCRYPTED))
      {
        pCtx->llcpActiveProc = LCTR_PROC_ENCRYPT;
        pCtx->llcpState = LCTR_LLCP_STATE_BUSY;
      }
      break;

    case LCTR_LLCP_STATE_BUSY:
      LL_TRACE_INFO3("lctrSlvExecuteEncryptSm: handle=%u, llcpState=BUSY, encState=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->encState, event);

      lctrSlvCheckProcOverride(pCtx, event);

      if (lctrCheckActiveOrPend(pCtx, LCTR_PROC_ENCRYPT))
      {
        lctrSlvEncryptSm(pCtx, event);

        if ((pCtx->encState == LCTR_ENC_STATE_ENCRYPTED) ||
            (pCtx->encState == LCTR_ENC_STATE_UNENCRYPTED))
        {
          pCtx->replyWaitingMsk &= ~LCTR_HOST_REPLY_LTK_REQ;
          pCtx->llcpActiveProc = LCTR_PROC_INVALID;
          pCtx->llcpState = LCTR_LLCP_STATE_IDLE;

          lctrStartPendingLlcp(pCtx);
        }
      }
      else
      {
        LL_TRACE_INFO2("Pending ENCRYPT=%u procedure: activeProc=%u", LCTR_PROC_ENCRYPT, pCtx->llcpActiveProc);
      }
      break;

    default:
      break;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Execute LE Ping LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 *
 *  \return     TRUE if peer response pending, FALSE otherwise.
 */
/*************************************************************************************************/
static void lctrPingSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (pCtx->pingState)
  {
    case LCTR_PING_STATE_IDLE:
      switch (event)
      {
        case LCTR_PING_EVENT_SEND_REQ:
        case LCTR_PING_EVENT_PING_PERIOD_EXP:
          /* Send LE_PING_REQ regardless of peer feature support. */
          lctrSendPingReq(pCtx);
          lctrStartLlcpTimer(pCtx);
          pCtx->pingState = LCTR_PING_STATE_WAIT_FOR_RSP;
          break;

        case LCTR_PING_EVENT_RECV_REQ:
          lctrSendPingRsp(pCtx);
          break;

        case LCTR_PING_EVENT_AUTH_PAYLOAD_EXP:
          lctrNotifyAuthPayloadTimeout(pCtx);
          lctrRestartAuthPayloadTimer(pCtx);
          break;

        /* Ignore */
        case LCTR_PING_EVENT_RECV_RSP:
          /* No action required. */
        default:
          break;
      }
      break;

    case LCTR_PING_STATE_WAIT_FOR_RSP:
      switch (event)
      {
        case LCTR_PING_EVENT_RECV_RSP:
          lctrStopLlcpTimer(pCtx);
          pCtx->pingState = LCTR_PING_STATE_IDLE;
          break;

        case LCTR_PING_EVENT_RECV_REQ:
          LL_TRACE_WARN1("LE Ping procedure collision; send LE_PING_RSP anyway, handle=%u", LCTR_GET_CONN_HANDLE(pCtx));
          lctrSendPingRsp(pCtx);
          break;

        case LCTR_PING_EVENT_AUTH_PAYLOAD_EXP:
          lctrNotifyAuthPayloadTimeout(pCtx);
          break;

        /* Ignore */
        case LCTR_PING_EVENT_SEND_REQ:
        case LCTR_PING_EVENT_PING_PERIOD_EXP:
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
 *  \brief      Execute ping LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 *
 *  \return     TRUE if handled by this SM, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrExecutePingSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  if (((lmgrCb.features & LL_FEAT_LE_PING) == 0) ||
      ((event = lctrRemapPingEvent(pCtx, event)) == LCTR_PING_EVENT_INVALID))
  {
    return FALSE;
  }

  switch (pCtx->llcpState)
  {
    case LCTR_LLCP_STATE_IDLE:
      LL_TRACE_INFO3("lctrExecutePingSm: handle=%u, llcpState=IDLE, pingState=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->pingState, event);

      lctrPingSm(pCtx, event);

      if (pCtx->pingState != LCTR_PING_STATE_IDLE)
      {
        pCtx->llcpActiveProc = LCTR_PROC_LE_PING;
        pCtx->llcpState = LCTR_LLCP_STATE_BUSY;
      }
      break;

    case LCTR_LLCP_STATE_BUSY:
      LL_TRACE_INFO3("lctrExecutePingSm: handle=%u, llcpState=BUSY, pingState=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->pingState, event);

      if (lctrCheckActiveOrPend(pCtx, LCTR_PROC_LE_PING))
      {
        lctrPingSm(pCtx, event);

        if (pCtx->pingState == LCTR_PING_STATE_IDLE)
        {
          pCtx->llcpActiveProc = LCTR_PROC_INVALID;
          pCtx->llcpState = LCTR_LLCP_STATE_IDLE;

          lctrStartPendingLlcp(pCtx);
        }
      }
      else
      {
        LL_TRACE_INFO2("Pending LE_PING=%u procedure: activeProc=%u", LCTR_PROC_LE_PING, pCtx->llcpActiveProc);
      }
      break;

    default:
      break;
  }

  return TRUE;
}
