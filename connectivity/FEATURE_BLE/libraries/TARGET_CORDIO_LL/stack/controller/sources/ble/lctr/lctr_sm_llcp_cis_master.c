/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller CIS LLCP master state machine implementation file.
 *
 *  Copyright (c) 2016-2018 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_cis_master.h"
#include "lctr_int_conn_master.h"
#include "lctr_int_conn.h"
#include "lmgr_api.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "util/bstream.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Action function call signature. */
typedef void (*lctrActFn_t)(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx);

/*! \brief      CIS establishment procedure events. */
enum
{
  LCTR_CIS_MST_EST_EVENT_HOST_CIS_REQ,                 /*!< Received host LL_CIS_REQ command. */
  LCTR_CIS_MST_EST_EVENT_PEER_CIS_RSP,                 /*!< Received peer LL_CIS_RSP. */
  LCTR_CIS_MST_EST_EVENT_PEER_REJECT,                  /*!< Received peer LL_REJECT_IND_EXT or LL_UNKNOWN_RSP. */
  LCTR_CIS_MST_EST_EVENT_RSP_TIMEOUT,                  /*!< Received internal response timeout. */
  LCTR_CIS_MST_EST_EVENT_LOCAL_REJECT,                 /*!< Received local rejection due to limited resources. */
  LCTR_CIS_MST_EST_EVENT_TOTAL,                        /*!< Total CIS events. */
  LCTR_CIS_MST_EST_EVENT_INVALID = 0xFF                /*!< Invalid event. */
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/
static void lctrMstCisLlcpActRejectCollision(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      CIS establishment state machine action table. */
static const lctrActFn_t lctrMstCisEstActionTbl[LCTR_CIS_MST_EST_STATE_TOTAL][LCTR_CIS_MST_EST_EVENT_TOTAL] =
{
  { /* LCTR_CIS_MST_EST_STATE_IDLE */
      lctrMstCisLlcpActHostCisReq,                /* LCTR_CIS_MST_EST_EVENT_HOST_CIS_REQ */
      lctrMstCisLlcpActRejectCollision,           /* LCTR_CIS_MST_EST_EVENT_PEER_CIS_RSP */   /* Shall not happen */
      lctrMstCisLlcpActRejectCollision,           /* LCTR_CIS_MST_EST_EVENT_PEER_REJECT */    /* Shall not happen */
      NULL,                                       /* LCTR_CIS_MST_EST_EVENT_RSP_TIMEOUT */    /* Shall not happen */
      NULL,                                       /* LCTR_CIS_MST_EST_EVENT_LOCAL_REJECT */   /* Shall not happen */
  },
  { /* LCTR_CIS_MST_EST_STATE_CIS_RSP */
      NULL,                                       /* LCTR_CIS_MST_EST_EVENT_HOST_CIS_REQ */
      lctrMstCisLlcpActPeerCisRsp,                /* LCTR_CIS_MST_EST_EVENT_PEER_CIS_RSP */
      lctrMstCisLlcpActPeerRej,                   /* LCTR_CIS_MST_EST_EVENT_PEER_REJECT */
      lctrMstCisLlcpActRspTimeout,                /* LCTR_CIS_MST_EST_EVENT_RSP_TIMEOUT */
      lctrMstCisLlcpActLocalReject,               /* LCTR_CIS_MST_EST_EVENT_LOCAL_REJECT */
  }
};

/*! \brief      CIS establishment state machine next state table. */
static const uint8_t lctrMstCisEstNextStateTbl[LCTR_CIS_MST_EST_STATE_TOTAL][LCTR_CIS_MST_EST_EVENT_TOTAL] =
{
  { /* LCTR_CIS_MST_EST_STATE_IDLE */
      LCTR_CIS_MST_EST_STATE_CIS_RSP,             /* LCTR_CIS_MST_EST_EVENT_HOST_CIS_REQ */
      LCTR_CIS_MST_EST_STATE_IDLE,                /* LCTR_CIS_MST_EST_EVENT_PEER_CIS_RSP */
      LCTR_CIS_MST_EST_STATE_IDLE,                /* LCTR_CIS_MST_EST_EVENT_PEER_REJECT */
      LCTR_CIS_MST_EST_STATE_IDLE,                /* LCTR_CIS_MST_EST_EVENT_RSP_TIMEOUT */
      LCTR_CIS_MST_EST_STATE_IDLE,                /* LCTR_CIS_MST_EST_EVENT_LOCAL_REJECT */
  },
  { /* LCTR_CIS_MST_EST_STATE_CIS_RSP */
      LCTR_CIS_MST_EST_STATE_CIS_RSP,             /* LCTR_CIS_MST_EST_EVENT_HOST_CIS_REQ */
      LCTR_CIS_MST_EST_STATE_IDLE,                /* LCTR_CIS_MST_EST_EVENT_PEER_CIS_RSP */
      LCTR_CIS_MST_EST_STATE_IDLE,                /* LCTR_CIS_MST_EST_EVENT_PEER_REJECT */
      LCTR_CIS_MST_EST_STATE_IDLE,                /* LCTR_CIS_MST_EST_EVENT_RSP_TIMEOUT */
      LCTR_CIS_MST_EST_STATE_IDLE,                /* LCTR_CIS_MST_EST_EVENT_LOCAL_REJECT */
  }
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Send peer LL_REJECT_IND_EXT due to pending operation.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
static void lctrMstCisLlcpActRejectCollision(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  LL_TRACE_WARN1("Peer requested PHY while procedure pending, handle=%u", LCTR_GET_CONN_HANDLE(pCtx));
  lctrSendRejectInd(pCtx, LL_ERROR_CODE_LMP_ERR_TRANSACTION_COLLISION, TRUE);
}

/*************************************************************************************************/
/*!
 *  \brief      Execute action function.
 *
 *  \param     pCtx    Connection context.
 *  \param     pCisCtx  CIS context.
 *  \param     event   Event ID.
 */
/*************************************************************************************************/
static inline void lctrExecAction(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx, uint8_t event)
{
  if (lctrMstCisEstActionTbl[pCisCtx->estState][event])
  {
    lctrMstCisEstActionTbl[pCisCtx->estState][event](pCtx, pCisCtx);
  }

  pCisCtx->estState = lctrMstCisEstNextStateTbl[pCisCtx->estState][event];
}

/*************************************************************************************************/
/*!
 *  \brief      Get remapped event. This function also update the CIS handle for the LLCP.
 *
 *  \param      event   Subsystem event.
 *  \param      pCtx    Connection context.
 *
 *  \return     Procedure event ID.
 */
/*************************************************************************************************/
static uint8_t lctrRemapEvent(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    /*** Peer messages ***/

    case LCTR_CONN_MSG_RX_LLCP:
      switch (lctrDataPdu.opcode)
      {
        case LL_PDU_CIS_RSP:
          return LCTR_CIS_MST_EST_EVENT_PEER_CIS_RSP;
        case LL_PDU_UNKNOWN_RSP:
          if (lctrDataPdu.pld.unknownRsp.unknownType == LL_PDU_CIS_REQ)
          {
            return LCTR_CIS_MST_EST_EVENT_PEER_REJECT;
          }
          break;
        case LL_PDU_REJECT_EXT_IND:
          if (lctrDataPdu.pld.rejInd.opcode == LL_PDU_CIS_REQ)
          {
            return LCTR_CIS_MST_EST_EVENT_PEER_REJECT;
          }
          break;
        break;

        default:
          break;
      }
      break;

    /*** Host messages ***/

    case LCTR_CONN_MSG_API_CIS_REQ:
      pCtx->llcpCisHandle = pLctrConnMsg->createCis.cisHandle;    /* Update LLCP CIS handle for connection context from host. */
      return LCTR_CIS_MST_EST_EVENT_HOST_CIS_REQ;

    /*** Internal messages ***/
    case LCTR_CONN_LLCP_START_PENDING:
      if (pCtx->llcpPendMask & (1 << LCTR_PROC_CIS_EST))
      {
        pCtx->llcpPendMask &= ~(1 << LCTR_PROC_CIS_EST);
        return LCTR_CIS_MST_EST_EVENT_HOST_CIS_REQ;
      }
      break;

    case LCTR_CONN_TMR_CIS_LLCP_RSP_EXP:
      return LCTR_CIS_MST_EST_EVENT_RSP_TIMEOUT;

    case LCTR_CONN_TERM_CIS_LOCAL_RESOURCE:
      return LCTR_CIS_MST_EST_EVENT_LOCAL_REJECT;

    default:
      break;
  }
  return LCTR_CIS_MST_EST_EVENT_INVALID;
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
    case LCTR_CIS_MST_EST_EVENT_HOST_CIS_REQ:
      pCtx->llcpNotifyMask |= 1 << LCTR_PROC_CIS_EST;
      pCtx->llcpPendMask |= 1 << LCTR_PROC_CIS_EST;
      LL_TRACE_INFO2("Pending CIS_REQ=%u procedure: activeProc=%u", LCTR_PROC_CIS_EST, pCtx->llcpActiveProc);
      break;

    default:
      break;
  }
}

/**************************************************************************************************
  External Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Execute CIS master establishment LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 *
 *  \return     TRUE if the event is handled by this state machine, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrMstLlcpExecuteCisEstSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  if ((event = lctrRemapEvent(pCtx, event)) == LCTR_CIS_MST_EST_EVENT_INVALID)
  {
    return FALSE;
  }

  lctrCisCtx_t *pCisCtx = lctrFindCisByHandle(pCtx->llcpCisHandle);
  if (pCisCtx == NULL)
  {
    LL_TRACE_WARN1("lctrMstLlcpExecuteCisEstSm: CIS context not found cis_handle=%u", pCtx->llcpCisHandle);
    /* TRUE to prevent further processing of this message. */
    return TRUE;
  }

  switch (pCtx->llcpState)
  {
    case LCTR_LLCP_STATE_IDLE:
      LL_TRACE_INFO3("lctrMstLlcpExecuteCisEstSm: cis_handle=%u, llcpState=IDLE, estState=%u, event=%u", pCisCtx->cisHandle, pCisCtx->estState, event);

      lctrExecAction(pCtx, pCisCtx, event);

      if (pCisCtx->estState != LCTR_CIS_MST_EST_STATE_IDLE)
      {
        pCtx->llcpState = LCTR_LLCP_STATE_BUSY;
        pCtx->llcpActiveProc = LCTR_PROC_CIS_EST;
      }
      break;

    case LCTR_LLCP_STATE_BUSY:
      LL_TRACE_INFO3("lctrMstLlcpExecuteCisEstSm: cis_handle=%u, llcpState=BUSY, estState=%u, event=%u", pCisCtx->cisHandle, pCisCtx->estState, event);

      if (pCtx->llcpActiveProc == LCTR_PROC_CIS_EST)
      {
        lctrExecAction(pCtx, pCisCtx, event);

        if (pCisCtx->estState == LCTR_CIS_MST_EST_STATE_IDLE)
        {
          lctrCisStopLlcpTimer(pCtx, pCisCtx);
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

