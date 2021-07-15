/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave PHY update state machine implementation file.
 *
 *  Copyright (c) 2016-2018 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_master_phy.h"
#include "lctr_int_slave_phy.h"
#include "lctr_int_conn.h"
#include "lctr_int_conn_master.h"
#include "lctr_int_conn_slave.h"
#include "lmgr_api.h"
#include "wsf_math.h"
#include "wsf_assert.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Action function call signature. */
typedef void (*lctrActFn_t)(lctrConnCtx_t *pCtx);

/*! \brief      Slave PHY update states. */
enum
{
  LCTR_PU_STATE_IDLE,                   /*!< Idle state. */
  LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ,   /*!< ARQ flush before sending LL_PHY_REQ. */
  LCTR_PU_STATE_HOST_PHY_UPDATE_REQ,    /*!< Host-initiated PHY update state. */
  LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP,   /*!< ARQ flush before sending LL_PHY_RSP. */
  LCTR_PU_STATE_PEER_PHY_UPDATE_IND,    /*!< Peer-initiated PHY update collision. */
  LCTR_PU_STATE_PHY_UPD_INSTANT,        /*!< Wait for PHY update instant state. */
  LCTR_PU_STATE_TOTAL                   /*!< Total PHY update states. */
};

/*! \brief      PHY update events. */
enum
{
  LCTR_PU_EVENT_HOST_PHY_UPD,           /*!< Received host PHY update command. */
  LCTR_PU_EVENT_PEER_PHY_UPDATE_IND,    /*!< Received peer LL_PHY_UPDATE_IND. */
  LCTR_PU_EVENT_PEER_PHY_REQ,           /*!< Received peer LL_PHY_REQ. */
  LCTR_PU_EVENT_PEER_REJECT,            /*!< Received peer LL_REJECT_IND or LL_UNKNOWN_RSP. */
  LCTR_PU_EVENT_INT_PROC_COMP,          /*!< Procedure completion event. */
  LCTR_PU_EVENT_INT_START_PHY_UPD,      /*!< Start pending host PHY update procedure. */
  LCTR_PU_EVENT_ARQ_FLUSHED,            /*!< Internal ARQ queue flushed. */
  LCTR_PU_EVENT_TOTAL,                  /*!< Total PHY update states. */
  LCTR_PU_EVENT_INVALID = 0xFF          /*!< Invalid event. */
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void lctrActPeerRejectPhyReq(lctrConnCtx_t *pCtx);
void lctrActNotifyHostPhyUpdateSuccess(lctrConnCtx_t *pCtx);
uint8_t lctrChoosePreferredPhy(uint8_t val);
uint8_t lctrSuppPhys(void);
void lctrActStartPhyUpdate(lctrConnCtx_t *pCtx);
void lctrActFlushArq(lctrConnCtx_t *pCtx);

/**************************************************************************************************
  Local Functions
**************************************************************************************************/
/*************************************************************************************************/
/*!
 *  \brief      Update effective data packet time after PHY update procedure completes and notify host.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrUpdateDataTime(lctrConnCtx_t *pCtx)
{
  uint16_t          maxTxTime = 0;
  uint16_t          maxRxTime = 0;

  lctrDataLen_t oldEffDataPdu = pCtx->effDataPdu;

  switch (pCtx->bleData.chan.txPhy)
  {
    case BB_PHY_BLE_1M:
    case BB_PHY_BLE_2M:
      maxTxTime = WSF_MIN(LL_DATA_LEN_TO_TIME_1M(pCtx->effDataPdu.maxTxLen, TRUE), pCtx->effDataPdu.maxTxTime);
      break;
    case BB_PHY_BLE_CODED:
      maxTxTime = WSF_MIN(LL_DATA_LEN_TO_TIME_CODED_S8(pCtx->effDataPdu.maxTxLen, TRUE), pCtx->effDataPdu.maxTxTime);
      break;
  }

  switch (pCtx->bleData.chan.rxPhy)
  {
    case BB_PHY_BLE_1M:
    case BB_PHY_BLE_2M:
      maxRxTime = WSF_MIN(LL_DATA_LEN_TO_TIME_1M(pCtx->effDataPdu.maxRxLen, TRUE), pCtx->effDataPdu.maxRxTime);
      break;
    case BB_PHY_BLE_CODED:
      maxRxTime = WSF_MIN(LL_DATA_LEN_TO_TIME_CODED_S8(pCtx->effDataPdu.maxRxLen, TRUE), pCtx->effDataPdu.maxRxTime);
      break;
  }

  pCtx->effDataPdu.maxRxTime = maxRxTime;
  pCtx->effDataPdu.maxTxTime = maxTxTime;

  /* connEffectiveMaxRxTimeCoded - the greater of 2704 and connEffectiveMaxRxTimeUncoded. */
  if (pCtx->bleData.chan.rxPhy == BB_PHY_BLE_CODED)
  {
    pCtx->effDataPdu.maxRxTime = WSF_MAX(pCtx->effDataPdu.maxRxTime, LL_MAX_DATA_TIME_ABS_MIN_CODED);
  }
  else
  {
    pCtx->effDataPdu.maxRxTime = WSF_MAX(pCtx->effDataPdu.maxRxTime, LL_MAX_DATA_TIME_MIN);
  }

  if (pCtx->bleData.chan.txPhy == BB_PHY_BLE_CODED)
  {
    pCtx->effDataPdu.maxTxTime = WSF_MAX(pCtx->effDataPdu.maxTxTime, LL_MAX_DATA_TIME_ABS_MIN_CODED);
  }
  else
  {
    pCtx->effDataPdu.maxTxTime = WSF_MAX(pCtx->effDataPdu.maxTxTime, LL_MAX_DATA_TIME_MIN);
  }

  if (lmgrCb.features & LL_FEAT_DATA_LEN_EXT)
  {
    if ((oldEffDataPdu.maxTxLen  != pCtx->effDataPdu.maxTxLen) ||
        (oldEffDataPdu.maxRxLen  != pCtx->effDataPdu.maxRxLen) ||
        (oldEffDataPdu.maxTxTime != pCtx->effDataPdu.maxTxTime) ||
        (oldEffDataPdu.maxRxTime != pCtx->effDataPdu.maxRxTime))
    {
      lctrNotifyHostDataLengthInd(pCtx, LL_SUCCESS);
    }
  }
}

/**************************************************************************************************
  Global Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Notify host of peer rejected PHY request.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrActPeerRejectPhyReq(lctrConnCtx_t *pCtx)
{
  lctrStopLlcpTimer(pCtx);

  if (pCtx->llcpNotifyMask & (1 << LCTR_PROC_PHY_UPD))
  {
    pCtx->llcpNotifyMask &= ~(1 << LCTR_PROC_PHY_UPD);

    switch (lctrDataPdu.opcode)
    {
      case LL_PDU_REJECT_IND:
      case LL_PDU_REJECT_EXT_IND:
        LL_TRACE_WARN2("Peer rejected PHY update, handle=%u, reason=%u", LCTR_GET_CONN_HANDLE(pCtx), lctrDataPdu.pld.rejInd.reason);
        lctrNotifyHostPhyUpdateInd(pCtx, lctrDataPdu.pld.rejInd.reason);
        break;
      case LL_PDU_UNKNOWN_RSP:
        LL_TRACE_WARN1("Peer rejected PHY update, handle=%u, reason=UNSUPPORTED_REMOTE_FEATURE", LCTR_GET_CONN_HANDLE(pCtx));
        lctrNotifyHostPhyUpdateInd(pCtx, LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE);
        break;
      default:
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of connection update with no change.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrActNotifyHostPhyUpdateNoChange(lctrConnCtx_t *pCtx)
{
  /* Remove packet time restrictions. */
  lctrRemovePacketTimeRestriction(pCtx);

  if ((pCtx->llcpNotifyMask & (1 << LCTR_PROC_PHY_UPD)))
  {
    pCtx->llcpNotifyMask &= ~(1 << LCTR_PROC_PHY_UPD);

    if (pCtx->connEst && (pCtx->state != LCTR_CONN_STATE_TERMINATING))
    {
      /* Only send PHY Update event if still connected. */
      lctrNotifyHostPhyUpdateInd(pCtx, LL_SUCCESS);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of connection update with success status.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrActNotifyHostPhyUpdateSuccess(lctrConnCtx_t *pCtx)
{
  /* Remove packet time restrictions. */
  lctrRemovePacketTimeRestriction(pCtx);

  if (pCtx->llcpNotifyMask & (1 << LCTR_PROC_PHY_UPD))
  {
    pCtx->llcpNotifyMask &= ~(1 << LCTR_PROC_PHY_UPD);

    lctrNotifyHostPhyUpdateInd(pCtx, LL_SUCCESS);

    lctrUpdateDataTime(pCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Choose preferred PHY.
 *
 *  \param      phys    Mask of PHY bits.
 *
 *  \return     PHY bit for preferred PHY.
 */
/*************************************************************************************************/
uint8_t lctrChoosePreferredPhy(uint8_t phys)
{
  /* Note: A different scheme for prioritizing PHYs could be offered. */

  if ((phys & LL_PHYS_LE_2M_BIT) != 0)
  {
    return LL_PHYS_LE_2M_BIT;
  }
  if ((phys & LL_PHYS_LE_1M_BIT) != 0)
  {
    return LL_PHYS_LE_1M_BIT;
  }
  if ((phys & LL_PHYS_LE_CODED_BIT) != 0)
  {
    return LL_PHYS_LE_CODED_BIT;
  }
  return LL_PHYS_NONE;
}

/*************************************************************************************************/
/*!
 *  \brief      Determine supported PHYs.
 *
 *  \return     Supported PHYs.
 */
/*************************************************************************************************/
uint8_t lctrSuppPhys(void)
{
  uint8_t phys = LL_PHYS_LE_1M_BIT;
  if ((lmgrCb.features & LL_FEAT_LE_2M_PHY) != 0)
  {
    phys |= LL_PHYS_LE_2M_BIT;
  }
  if ((lmgrCb.features & LL_FEAT_LE_CODED_PHY) != 0)
  {
    phys |= LL_PHYS_LE_CODED_BIT;
  }
  return phys;
}

/*************************************************************************************************/
/*!
 *  \brief      Adjust PHY preference.
 *
 *  \param      pCtx        Connection context.
 *  \param      allPhys     All PHYs preferences.
 *  \param      pTxPhys     Preferred transmitter PHYs.
 *  \param      pRxPhys     Preferred receiver PHYs.
 *  \param      peerPhys    Peer preferred PHYs.
 */
/*************************************************************************************************/
static void lctrAdjustPhyPreference(lctrConnCtx_t *pCtx, uint8_t allPhys, uint8_t *pTxPhys, uint8_t *pRxPhys, uint8_t peerPhys)
{
  uint8_t txPhys = *pTxPhys;
  uint8_t rxPhys = *pRxPhys;

  /* If no preference, use supported PHYs. */
  if ((allPhys & LL_ALL_PHY_TX_PREFERENCE_BIT) != 0)
  {
    txPhys = lctrSuppPhys();
  }
  if ((allPhys & LL_ALL_PHY_RX_PREFERENCE_BIT) != 0)
  {
    rxPhys = lctrSuppPhys();
  }

  /* If symmetric only, restrict and validate choice. */
  if (BB_SYM_PHY_REQ || lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_REQ_SYM_PHY))
  {
    txPhys &= rxPhys;   /* only common PHYs */
    if (txPhys == 0)
    {
      /* No common PHY; select current. */
      txPhys = lctrPhyToPhysBit(pCtx->bleData.chan.txPhy);
      rxPhys = lctrPhyToPhysBit(pCtx->bleData.chan.rxPhy);
    }
    else
    {
      /* Prefer common with peer. */
      if ((txPhys & peerPhys) != 0)
      {
        txPhys &= peerPhys;
      }

      /* Choose preferred PHY. */
      txPhys = lctrChoosePreferredPhy(txPhys);

      /* Tx and Rx identical. */
      rxPhys = txPhys;
    }
  }

  /* Save preference. */
  *pTxPhys = txPhys;
  *pRxPhys = rxPhys;
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for start PHY update.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrActStartPhyUpdate(lctrConnCtx_t *pCtx)
{
  /* If no preference, procedure is complete. */
  if (((pCtx->allPhys & LL_ALL_PHY_TX_PREFERENCE_BIT) != 0) &&
      ((pCtx->allPhys & LL_ALL_PHY_RX_PREFERENCE_BIT) != 0))
  {
    lctrUnpauseTxData(pCtx);
    lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_PROC_CMPL);
  }
  else
  {
    uint8_t txPhys = pCtx->txPhys;
    uint8_t rxPhys = pCtx->rxPhys;

    lctrAdjustPhyPreference(pCtx, pCtx->allPhys, &txPhys, &rxPhys, LL_PHYS_NONE);

    /* If no change, procedure is complete. */
    if ((txPhys == lctrPhyToPhysBit(pCtx->bleData.chan.txPhy)) &&
        (rxPhys == lctrPhyToPhysBit(pCtx->bleData.chan.rxPhy)))
    {
      lctrUnpauseTxData(pCtx);
      lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_PROC_CMPL);
    }
    else
    {
      /* Set restriction for packet times and allow data again. */
      lctrSetPacketTimeRestriction(pCtx, txPhys);
      lctrUnpauseTxData(pCtx);

      /* Send PHY request. */
      lctrSendPhyReqPdu(pCtx, txPhys, rxPhys);
      lctrStartLlcpTimer(pCtx);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for flushing ARQ.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrActFlushArq(lctrConnCtx_t *pCtx)
{
  lctrPauseTxData(pCtx);
  lctrCheckPauseComplete(pCtx);
}

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received host PHY update command.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActHostPhyUpdate(lctrConnCtx_t *pCtx)
{
  pCtx->llcpNotifyMask |= 1 << LCTR_PROC_PHY_UPD;
  lctrStoreHostPhyUpdate(pCtx);
  lctrActFlushArq(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for start PHY response.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActStartPhyRsp(lctrConnCtx_t *pCtx)
{
  uint8_t txPhys = pCtx->txPhys;
  uint8_t rxPhys = pCtx->rxPhys;

  lctrAdjustPhyPreference(pCtx, pCtx->allPhys, &txPhys, &rxPhys, pCtx->phyReq.txPhys & pCtx->phyReq.rxPhys);

  /* Set restriction for packet times and allow data again. */
  lctrSetPacketTimeRestriction(pCtx, txPhys);
  lctrUnpauseTxData(pCtx);

  /* Send PHY response. */
  lctrSendPhyRspPdu(pCtx, txPhys, rxPhys);
  lctrStartLlcpTimer(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received peer PHY request.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPeerPhyReq(lctrConnCtx_t *pCtx)
{
  lctrStorePeerPhyReq(pCtx);
  lctrActFlushArq(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received peer PHY update request.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPeerPhyUpdateReq(lctrConnCtx_t *pCtx)
{
  lctrStopLlcpTimer(pCtx);    /* procedure completes at instant */

  /* Procedure completes now if neither PHY will change. */
  if ((lctrDataPdu.pld.phyUpdInd.masterToSlavePhy == LL_PHYS_NONE) &&
      (lctrDataPdu.pld.phyUpdInd.slaveToMasterPhy == LL_PHYS_NONE))
  {
    lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_PROC_CMPL);
  }
  else
  {
    lctrStorePeerPhyUpdateInd(pCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Action indirection for received peer PHY request.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPeerPhyReqWithCollision(lctrConnCtx_t *pCtx)
{
  lctrActPeerPhyReq(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of PHY update with disallowed status.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrActPhyUpdateDisallow(lctrConnCtx_t *pCtx)
{
  LL_TRACE_WARN1("Host requested PHY update while procedure pending, handle=%u", LCTR_GET_CONN_HANDLE(pCtx));
  lctrNotifyHostPhyUpdateInd(pCtx, LL_ERROR_CODE_CMD_DISALLOWED);
}

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      State machine action table. */
static const lctrActFn_t lctrSlvPhyUpdateActionTbl[LCTR_PU_STATE_TOTAL][LCTR_PU_EVENT_TOTAL] =
{
  { /* LCTR_PU_STATE_IDLE */
    lctrActHostPhyUpdate,                   /* LCTR_PU_EVENT_HOST_PHY_UPD */
    NULL,                                   /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */     /* out of sequence; ignore */
    lctrActPeerPhyReq,                      /* LCTR_PU_EVENT_PEER_PHY_REQ */
    NULL,                                   /* LCTR_PU_EVENT_PEER_REJECT */             /* should never occur */
    NULL,                                   /* LCTR_PU_EVENT_INT_PROC_COMP */           /* should never occur (not for this SM) */
    lctrActFlushArq,                        /* LCTR_PU_EVENT_INT_START_PHY_UPD */
    NULL                                    /* LCTR_PU_EVENT_ARQ_FLUSHED */             /* should never occur (not for this SM) */
  },
  { /* LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ */
    lctrActPhyUpdateDisallow,               /* LCTR_PU_EVENT_HOST_PHY_UPD */
    NULL,                                   /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */     /* out of sequence; ignore */
    lctrActPeerPhyReqWithCollision,         /* LCTR_PU_EVENT_PEER_PHY_REQ */
    NULL,                                   /* LCTR_PU_EVENT_PEER_REJECT */             /* should never occur; ignore */
    NULL,                                   /* LCTR_PU_EVENT_INT_PROC_COMP */           /* should never occur */
    lctrActFlushArq,                        /* LCTR_PU_EVENT_INT_START_PHY_UPD */       /* occurs on override by other ll procedure */
    lctrActStartPhyUpdate                   /* LCTR_PU_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_PU_STATE_HOST_PHY_UPDATE_REQ */
    lctrActPhyUpdateDisallow,               /* LCTR_PU_EVENT_HOST_PHY_UPD */
    lctrActPeerPhyUpdateReq,                /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */
    lctrActPeerPhyReqWithCollision,         /* LCTR_PU_EVENT_PEER_PHY_REQ */
    lctrActPeerRejectPhyReq,                /* LCTR_PU_EVENT_PEER_REJECT */
    lctrActNotifyHostPhyUpdateNoChange,     /* LCTR_PU_EVENT_INT_PROC_COMP */           /* if no change */
    lctrActFlushArq,                        /* LCTR_PU_EVENT_INT_START_PHY_UPD */       /* occurs on override by other ll procedure */
    NULL                                    /* LCTR_PU_EVENT_ARQ_FLUSHED */             /* should never occur (not for this SM) */
  },
  { /* LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP */
    lctrActPhyUpdateDisallow,               /* LCTR_PU_EVENT_HOST_PHY_UPD */
    NULL,                                   /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */     /* out of sequence; ignore */
    NULL,                                   /* LCTR_PU_EVENT_PEER_PHY_REQ */            /* out of sequence; ignore */
    lctrActPeerRejectPhyReq,                /* LCTR_PU_EVENT_PEER_REJECT */
    NULL,                                   /* LCTR_PU_EVENT_INT_PROC_COMP */           /* should never occur */
    NULL,                                   /* LCTR_PU_EVENT_INT_START_PHY_UPD */       /* should never occur */
    lctrActStartPhyRsp                      /* LCTR_PU_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_PU_STATE_PEER_PHY_UPDATE_IND */
    lctrActPhyUpdateDisallow,               /* LCTR_PU_EVENT_HOST_PHY_UPD */
    lctrActPeerPhyUpdateReq,                /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */
    NULL,                                   /* LCTR_PU_EVENT_PEER_PHY_REQ */            /* out of sequence; ignore */
    lctrActPeerRejectPhyReq,                /* LCTR_PU_EVENT_PEER_REJECT */
    NULL,                                   /* LCTR_PU_EVENT_INT_PROC_COMP */           /* should never occur */
    NULL,                                   /* LCTR_PU_EVENT_INT_START_PHY_UPD */       /* should never occur */
    NULL                                    /* LCTR_PU_EVENT_ARQ_FLUSHED */             /* should never occur (not for this SM) */
  },
  { /* LCTR_PU_STATE_PHY_UPD_INSTANT */
    lctrActPhyUpdateDisallow,               /* LCTR_PU_EVENT_HOST_PHY_UPD */
    NULL,                                   /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */     /* out of sequence; ignore */
    NULL,                                   /* LCTR_PU_EVENT_PEER_PHY_REQ */            /* out of sequence; ignore */
    NULL,                                   /* LCTR_PU_EVENT_PEER_REJECT */             /* should never occur; ignore */
    lctrActNotifyHostPhyUpdateSuccess,      /* LCTR_PU_EVENT_INT_PROC_COMP */
    NULL,                                   /* LCTR_PU_EVENT_INT_START_PHY_UPD */       /* should never occur */
    NULL                                    /* LCTR_PU_EVENT_ARQ_FLUSHED */             /* should never occur (not for this SM) */
  }
};

/*! \brief      State machine next state table. */
static const uint8_t lctrSlvPhyUpdateNextStateTbl[LCTR_PU_STATE_TOTAL][LCTR_PU_EVENT_TOTAL] =
{
  { /* LCTR_PU_STATE_IDLE */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ,     /* LCTR_PU_EVENT_HOST_PHY_UPD */
    LCTR_PU_STATE_IDLE,                     /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */     /* out of sequence; ignore */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP,     /* LCTR_PU_EVENT_PEER_PHY_REQ */
    LCTR_PU_STATE_IDLE,                     /* LCTR_PU_EVENT_PEER_REJECT */             /* should never occur */
    LCTR_PU_STATE_IDLE,                     /* LCTR_PU_EVENT_INT_PROC_COMP */           /* should never occur (not for this SM) */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ,     /* LCTR_PU_EVENT_INT_START_PHY_UPD */
    LCTR_PU_STATE_IDLE                      /* LCTR_PU_EVENT_ARQ_FLUSHED */             /* should never occur (not for this SM) */
  },
  { /* LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ,     /* LCTR_PU_EVENT_HOST_PHY_UPD */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ,     /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */     /* out of sequence; ignore */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP,     /* LCTR_PU_EVENT_PEER_PHY_REQ */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ,     /* LCTR_PU_EVENT_PEER_REJECT */             /* should never occur; ignore */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ,     /* LCTR_PU_EVENT_INT_PROC_COMP */           /* should never occur */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ,     /* LCTR_PU_EVENT_INT_START_PHY_UPD */       /* occurs on override by other ll procedure */
    LCTR_PU_STATE_HOST_PHY_UPDATE_REQ       /* LCTR_PU_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_PU_STATE_HOST_PHY_UPDATE_REQ */
    LCTR_PU_STATE_HOST_PHY_UPDATE_REQ,      /* LCTR_PU_EVENT_HOST_PHY_UPD */
    LCTR_PU_STATE_PHY_UPD_INSTANT,          /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP,     /* LCTR_PU_EVENT_PEER_PHY_REQ */
    LCTR_PU_STATE_IDLE,                     /* LCTR_PU_EVENT_PEER_REJECT */
    LCTR_PU_STATE_IDLE,                     /* LCTR_PU_EVENT_INT_PROC_COMP */           /* if no change */
    LCTR_PU_STATE_HOST_PHY_UPDATE_REQ,      /* LCTR_PU_EVENT_INT_START_PHY_UPD */       /* occurs on override by other ll procedure */
    LCTR_PU_STATE_HOST_PHY_UPDATE_REQ       /* LCTR_PU_EVENT_ARQ_FLUSHED */             /* should never occur (not for this SM) */
  },
  { /* LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP,     /* LCTR_PU_EVENT_HOST_PHY_UPD */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP,     /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */     /* out of sequence; ignore */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP,     /* LCTR_PU_EVENT_PEER_PHY_REQ */            /* out of sequence; ignore */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP,     /* LCTR_PU_EVENT_PEER_REJECT */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP,     /* LCTR_PU_EVENT_INT_PROC_COMP */           /* should never occur */
    LCTR_PU_STATE_FLUSH_BEFORE_PHY_RSP,     /* LCTR_PU_EVENT_INT_START_PHY_UPD */       /* should never occur */
    LCTR_PU_STATE_PEER_PHY_UPDATE_IND       /* LCTR_PU_EVENT_ARQ_FLUSHED */
  },
  { /* LCTR_PU_STATE_PEER_PHY_UPDATE_IND */
    LCTR_PU_STATE_PEER_PHY_UPDATE_IND,      /* LCTR_PU_EVENT_HOST_PHY_UPD */
    LCTR_PU_STATE_PHY_UPD_INSTANT,          /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */
    LCTR_PU_STATE_PEER_PHY_UPDATE_IND,      /* LCTR_PU_EVENT_PEER_PHY_REQ */            /* out of sequence; ignore */
    LCTR_PU_STATE_PEER_PHY_UPDATE_IND,      /* LCTR_PU_EVENT_PEER_REJECT */
    LCTR_PU_STATE_PEER_PHY_UPDATE_IND,      /* LCTR_PU_EVENT_INT_PROC_COMP */           /* should never occur */
    LCTR_PU_STATE_PEER_PHY_UPDATE_IND,      /* LCTR_PU_EVENT_INT_START_PHY_UPD */       /* should never occur */
    LCTR_PU_STATE_PEER_PHY_UPDATE_IND       /* LCTR_PU_EVENT_ARQ_FLUSHED */             /* should never occur (not for this SM) */
  },
  { /* LCTR_PU_STATE_PHY_UPD_INSTANT */
    LCTR_PU_STATE_PHY_UPD_INSTANT,          /* LCTR_PU_EVENT_HOST_PHY_UPD */
    LCTR_PU_STATE_PHY_UPD_INSTANT,          /* LCTR_PU_EVENT_PEER_PHY_UPDATE_IND */     /* out of sequence; ignore */
    LCTR_PU_STATE_PHY_UPD_INSTANT,          /* LCTR_PU_EVENT_PEER_PHY_REQ */            /* out of sequence; ignore */
    LCTR_PU_STATE_PHY_UPD_INSTANT,          /* LCTR_PU_EVENT_PEER_REJECT */             /* should never occur; ignore */
    LCTR_PU_STATE_IDLE,                     /* LCTR_PU_EVENT_INT_PROC_COMP */
    LCTR_PU_STATE_PHY_UPD_INSTANT,          /* LCTR_PU_EVENT_INT_START_PHY_UPD */       /* should never occur */
    LCTR_PU_STATE_PHY_UPD_INSTANT           /* LCTR_PU_EVENT_ARQ_FLUSHED */             /* should never occur (not for this SM) */
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
  if (lctrSlvPhyUpdateActionTbl[pCtx->phyUpdState][event])
  {
    lctrSlvPhyUpdateActionTbl[pCtx->phyUpdState][event](pCtx);
  }

  pCtx->phyUpdState = lctrSlvPhyUpdateNextStateTbl[pCtx->phyUpdState][event];

  if (pCtx->phyUpdState == LCTR_PU_STATE_PHY_UPD_INSTANT)
    pCtx->isSlvPhyUpdInstant = TRUE;
  else
    pCtx->isSlvPhyUpdInstant = FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Get remapped event.
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
        case LL_PDU_PHY_REQ:
          return LCTR_PU_EVENT_PEER_PHY_REQ;
        case LL_PDU_PHY_UPDATE_IND:
          return LCTR_PU_EVENT_PEER_PHY_UPDATE_IND;

        case LL_PDU_UNKNOWN_RSP:
          if (lctrDataPdu.pld.unknownRsp.unknownType == LL_PDU_PHY_REQ)
          {
            return LCTR_PU_EVENT_PEER_REJECT;
          }
          /* Not for this SM. */
          break;
        case LL_PDU_REJECT_IND:
          if (pCtx->llcpActiveProc == LCTR_PROC_PHY_UPD)
          {
            return LCTR_PU_EVENT_PEER_REJECT;
          }
          /* Probably not for this SM. */
          break;
        case LL_PDU_REJECT_EXT_IND:
          if (lctrDataPdu.pld.rejInd.opcode == LL_PDU_PHY_REQ)
          {
            return LCTR_PU_EVENT_PEER_REJECT;
          }
          /* Not for this SM. */
          break;

        default:
          break;
      }
      break;

    /*** Host messages ***/

    case LCTR_CONN_MSG_API_PHY_UPDATE:
      return LCTR_PU_EVENT_HOST_PHY_UPD;

    /*** Internal messages ***/

    case LCTR_CONN_ARQ_Q_FLUSHED:
      if (pCtx->llcpActiveProc == LCTR_PROC_PHY_UPD)
      {
        return LCTR_PU_EVENT_ARQ_FLUSHED;
      }
      break;
    case LCTR_CONN_LLCP_PROC_CMPL:
      if (pCtx->llcpActiveProc == LCTR_PROC_PHY_UPD)
      {
        return LCTR_PU_EVENT_INT_PROC_COMP;
      }
      break;
    case LCTR_CONN_LLCP_START_PENDING:
      if (pCtx->llcpPendMask == (1 << LCTR_PROC_PHY_UPD))
      {
        pCtx->llcpPendMask &= ~(1 << LCTR_PROC_PHY_UPD);
        return LCTR_PU_EVENT_INT_START_PHY_UPD;
      }
      break;
    default:
      break;
  }
  return LCTR_PU_EVENT_INVALID;
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
    case LCTR_PU_EVENT_PEER_PHY_REQ:
      LL_TRACE_WARN2("Rejecting PHY_UPD=%u procedure: activeProc=%u, reason=DIFFERENT_TRANSACTION_COLLISION", LCTR_PROC_PHY_UPD, pCtx->llcpActiveProc);
      lctrSendRejectInd(pCtx, LL_ERROR_CODE_DIFFERENT_TRANSACTION_COLLISION, TRUE);
      break;

    case LCTR_PU_EVENT_HOST_PHY_UPD:
      lctrStoreHostPhyUpdate(pCtx);
      pCtx->llcpNotifyMask |= 1 << LCTR_PROC_PHY_UPD;
      pCtx->llcpPendMask |= 1 << LCTR_PROC_PHY_UPD;
      LL_TRACE_INFO2("Pending PHY_UPD=%u procedure: activeProc=%u", LCTR_PROC_PHY_UPD, pCtx->llcpActiveProc);
      break;

    case LCTR_PU_EVENT_PEER_REJECT:
      pCtx->llcpPendMask &= ~(1 << LCTR_PROC_PHY_UPD);
      if (pCtx->phyUpdState != LCTR_PU_STATE_IDLE)
      {
        pCtx->phyUpdState = LCTR_PU_STATE_IDLE;
      }
      lctrNotifyHostPhyUpdateInd(pCtx, lctrDataPdu.pld.rejInd.reason);
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check if active procedure can be overridden with phy update procedure.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   Subsystem event.
 *
 *  This routine will pend the active procedure and allow the phy update procedure to
 *  override it.
 */
/*************************************************************************************************/
static void lctrSlvCheckProcOverride(lctrConnCtx_t *pCtx, uint8_t event)
{
  switch (event)
  {
    case LCTR_PU_EVENT_PEER_PHY_REQ:
      /* Only the procedure without instant fields can be overridden. */
      switch (pCtx->llcpActiveProc)
      {
        case LCTR_PROC_CMN_VER_EXCH:
        case LCTR_PROC_CMN_FEAT_EXCH:
        case LCTR_PROC_CMN_DATA_LEN_UPD:
        case LCTR_PROC_CMN_REQ_PEER_SCA:
          pCtx->llcpPendMask |= 1 << pCtx->llcpActiveProc;
          pCtx->llcpActiveProc = LCTR_PROC_PHY_UPD;
          pCtx->llcpIsOverridden = TRUE;
          break;
        case LCTR_PROC_CONN_UPD:
          pCtx->llcpPendMask |= 1 << pCtx->llcpActiveProc;
          pCtx->llcpActiveProc = LCTR_PROC_PHY_UPD;
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
 *  \brief      Execute slave PHY update LLCP state machine.
 *
 *  \param      pCtx    Connection context.
 *  \param      event   State machine event.
 *
 *  \return     TRUE if peer response pending, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrSlvLlcpExecutePhyUpdateSm(lctrConnCtx_t *pCtx, uint8_t event)
{
  if ((event = lctrRemapEvent(pCtx, event)) == LCTR_PU_EVENT_INVALID)
  {
    return FALSE;
  }

  switch (pCtx->llcpState)
  {
    case LCTR_LLCP_STATE_IDLE:
      LL_TRACE_INFO3("lctrSlvLlcpExecutePhyUpdateSm: handle=%u, llcpState=IDLE, phyUpdState=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->phyUpdState, event);

      lctrExecAction(pCtx, event);

      if (pCtx->phyUpdState != LCTR_PU_STATE_IDLE)
      {
        pCtx->llcpState = LCTR_LLCP_STATE_BUSY;
        pCtx->llcpActiveProc = LCTR_PROC_PHY_UPD;
        pCtx->llcpInstantComp = FALSE;
      }
      break;

    case LCTR_LLCP_STATE_BUSY:
      LL_TRACE_INFO3("lctrSlvLlcpExecutePhyUpdateSm: handle=%u, llcpState=BUSY, phyUpdState=%u, event=%u", LCTR_GET_CONN_HANDLE(pCtx), pCtx->phyUpdState, event);

      lctrSlvCheckProcOverride(pCtx, event);

      if (pCtx->llcpActiveProc == LCTR_PROC_PHY_UPD)
      {
        lctrExecAction(pCtx, event);

        if (pCtx->phyUpdState == LCTR_PU_STATE_IDLE)
        {
          lctrStopLlcpTimer(pCtx);
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
 *  \brief      Check if encryption can override phy update procedure.
 *
 *  \param      pCtx    Connection context.
 *
 *  \return     TRUE if override possible, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrSlvCheckEncOverridePhyUpdate(lctrConnCtx_t *pCtx)
{
  if (pCtx->llcpActiveProc == LCTR_PROC_PHY_UPD)
  {
    if ((pCtx->phyUpdState == LCTR_PU_STATE_FLUSH_BEFORE_PHY_REQ) ||
        (pCtx->phyUpdState == LCTR_PU_STATE_HOST_PHY_UPDATE_REQ))
    {
      return TRUE;
    }
  }
  return FALSE;
}
