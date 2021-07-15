/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Device manager Connection Constant Tone Extension (CTE) module.
 *
 *  Copyright (c) 2018-2019 Arm Ltd. All Rights Reserved.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "dm_api.h"
#include "dm_dev.h"
#include "dm_main.h"
#include "dm_conn.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Data structure for DM_CONN_CTE_MSG_API_RX_SAMPLE_START */
typedef struct
{
  wsfMsgHdr_t                 hdr;              /*!< Header */
  dmConnId_t                  connId;           /*!< Connection id */
  uint8_t                     slotDurations;    /*!< Switching and sampling slot durations */
  uint8_t                     switchPatternLen; /*!< Number of Antenna IDs in switching pattern */
  uint8_t                     *pAntennaIDs;     /*!< List of Antenna IDs in switching pattern */
} dmConnCteApiRxSampleStart_t;

/*! Data structure for DM_CONN_CTE_MSG_API_RX_SAMPLE_STOP */
typedef struct
{
  wsfMsgHdr_t                 hdr;              /*!< Header */
  dmConnId_t                  connId;           /*!< Connection id */
} dmConnCteApiRxSampleStop_t;

/*! Data structure for DM_CONN_CTE_MSG_API_TX_CFG */
typedef struct
{
  wsfMsgHdr_t                 hdr;              /*!< Header */
  dmConnId_t                  connId;           /*!< Connection id */
  uint8_t                     cteTypeBits;      /*!< Permitted CTE type bits */
  uint8_t                     switchPatternLen; /*!< Number of Antenna IDs in switching pattern */
  uint8_t                     *pAntennaIDs;     /*!< List of Antenna IDs in switching pattern */
} dmConnCteApiTxConfig_t;

/*! Data structure for DM_CONN_CTE_MSG_API_REQ_START */
typedef struct
{
  wsfMsgHdr_t                 hdr;              /*!< Header */
  dmConnId_t                  connId;           /*!< Connection id */
  uint16_t                    cteReqInt;        /*!< CTE request interval */
  uint8_t                     reqCteLen;        /*!< Minimum length of CTE being requested in 8 us units */
  uint8_t                     reqCteType;       /*!< Requested CTE type */
} dmConnCteApiReqStart_t;

/*! Data structure for DM_CONN_CTE_MSG_API_REQ_STOP */
typedef struct
{
  wsfMsgHdr_t                 hdr;              /*!< Header */
  dmConnId_t                  connId;           /*!< Connection id */
} dmConnCteApiReqStop_t;

/*! Data structure for DM_CONN_CTE_MSG_API_RSP_START and DM_CONN_CTE_MSG_API_RSP_STOP */
typedef struct
{
  wsfMsgHdr_t                 hdr;              /*!< Header */
  dmConnId_t                  connId;           /*!< Connection id */
} dmConnCteApiRspEnable_t;

/*! Union of all DM Connection CTE API messages */
typedef union
{
  wsfMsgHdr_t                 hdr;              /*!< Header */
  dmConnCteApiRxSampleStart_t rxSampleStart;    /*!< Start sampling received CTE, and configure CTE Rx parameters to be used */
  dmConnCteApiRxSampleStop_t  rxSampleStop;     /*!< Stop sampling received CTE */
  dmConnCteApiTxConfig_t      txCfg;            /*!< Configure CTE Tx parameters */
  dmConnCteApiReqStart_t      reqStart;         /*!< Start initiating CTE request */
  dmConnCteApiReqStop_t       reqStop;          /*!< Stop initiating CTE request */
  dmConnCteApiRspEnable_t     rspEnable;        /*!< Start or stop responding to CTE request */
} dmConnCteMsg_t;

/*! Action function */
typedef void (*dmConnCteAct_t)(dmConnCteMsg_t *pMsg);

/*! Control block for connection CTE module */
typedef struct
{
  uint8_t                     rxSampleState;    /*!< Sampling received CTE state */
  uint8_t                     reqState;         /*!< Connection CTE Request state */
  uint8_t                     rspState;         /*!< Connection CTE Response state */
} dmConnCteCb_t;

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

void dmConnCteInit(void);
void dmConnCteReset(void);
void dmConnCteHciHandler(hciEvt_t *pEvent);
void dmConnCteMsgHandler(wsfMsgHdr_t *pMsg);

/*! Action functions */
static void dmConnCteActRxSampleStart(dmConnCteMsg_t *pMsg);
static void dmConnCteActRxSampleStop(dmConnCteMsg_t *pMsg);
static void dmConnCteActTxCfg(dmConnCteMsg_t *pMsg);
static void dmConnCteActReqStart(dmConnCteMsg_t *pMsg);
static void dmConnCteActReqStop(dmConnCteMsg_t *pMsg);
static void dmConnCteActRspStart(dmConnCteMsg_t *pMsg);
static void dmConnCteActRspStop(dmConnCteMsg_t *pMsg);
static void dmConnCteActState(dmConnCteMsg_t *pMsg);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! Action function table */
static const dmConnCteAct_t dmConnCteAct[] =
{
  dmConnCteActRxSampleStart,
  dmConnCteActRxSampleStop,
  dmConnCteActTxCfg,
  dmConnCteActReqStart,
  dmConnCteActReqStop,
  dmConnCteActRspStart,
  dmConnCteActRspStop,
  dmConnCteActState
};

/*! DM Connection CTE component function interface */
static const dmFcnIf_t dmConnCteFcnIf =
{
  dmConnCteReset,
  dmConnCteHciHandler,
  dmConnCteMsgHandler
};

/*! Connection CTE control block */
static dmConnCteCb_t dmConnCteCb[DM_CONN_MAX];

/*************************************************************************************************/
/*!
 *  \brief  Initialize connection CTE control block entry.
 *
 *  \param  pCteCb  Pointer to connection CTE control block structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmConnCteCbInit(dmConnCteCb_t *pCteCb)
{
  pCteCb->rxSampleState = DM_CONN_CTE_STATE_IDLE;
  pCteCb->reqState = DM_CONN_CTE_STATE_IDLE;
  pCteCb->rspState = DM_CONN_CTE_STATE_IDLE;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize connection CTE control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmConnCteInit(void)
{
  uint8_t i;

  /* initialize control block */
  for (i = 0; i < DM_CONN_MAX; i++)
  {
    dmConnCteCbInit(&dmConnCteCb[i]);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize DM Connection Constant Tone Extension (CTE) module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmConnCteInit(void)
{
  WsfTaskLock();

  /* set function interface table */
  dmFcnIfTbl[DM_ID_CONN_CTE] = (dmFcnIf_t *) &dmConnCteFcnIf;

  /* initialize control block */
  dmConnCteInit();

  HciSetLeSupFeat((HCI_LE_SUP_FEAT_CONN_CTE_REQ | HCI_LE_SUP_FEAT_CONN_CTE_RSP), TRUE);

  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
*  \brief  Reset the connection CTE module.
*
*  \return None.
*/
/*************************************************************************************************/
void dmConnCteReset(void)
{
  /* reset control block */
  dmConnCteInit();
}

/*************************************************************************************************/
/*!
 *  \brief  DM PAST HCI event handler.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmConnCteHciHandler(hciEvt_t *pEvent)
{
  dmConnCcb_t *pCcb;

  if (pEvent->hdr.event == HCI_LE_READ_ANTENNA_INFO_CMD_CMPL_CBACK_EVT)
  {
    pEvent->hdr.event = DM_READ_ANTENNA_INFO_IND;
    (*dmCb.cback)((dmEvt_t *) pEvent);
    return;
  }

  /* if ccb found */
  if ((pCcb = dmConnCcbByHandle(pEvent->hdr.param)) != NULL)
  {
    dmConnCteCb_t *pCteCb = &dmConnCteCb[pCcb->connId - 1];

    /* set conn id */
    pEvent->hdr.param = pCcb->connId;

    switch (pEvent->hdr.event)
    {
      case HCI_LE_CONN_IQ_REPORT_CBACK_EVT:
        pEvent->hdr.event = DM_CONN_IQ_REPORT_IND;
        (*dmCb.cback)((dmEvt_t *) pEvent);
        break;

      case HCI_LE_CTE_REQ_FAILED_CBACK_EVT:
        pEvent->hdr.event = DM_CTE_REQ_FAIL_IND;
        (*dmCb.cback)((dmEvt_t *) pEvent);
        break;

      case HCI_LE_SET_CONN_CTE_RX_PARAMS_CMD_CMPL_CBACK_EVT:
        /* if enabling sampling */
        if (pCteCb->rxSampleState == DM_CONN_CTE_STATE_STARTING)
        {
          if (pEvent->hdr.status == HCI_SUCCESS)
          {
            pCteCb->rxSampleState = DM_CONN_CTE_STATE_SAMPLING;
          }
          else
          {
            pCteCb->rxSampleState = DM_CONN_CTE_STATE_IDLE;
          }

          pEvent->hdr.event = DM_CONN_CTE_RX_SAMPLE_START_IND;
        }
        /* else if disabling sampling */
        else if (pCteCb->rxSampleState == DM_CONN_CTE_STATE_STOPPING)
        {
          if (pEvent->hdr.status == HCI_SUCCESS)
          {
            pCteCb->rxSampleState = DM_CONN_CTE_STATE_IDLE;
          }
          else
          {
            pCteCb->rxSampleState = DM_CONN_CTE_STATE_SAMPLING;
          }

          pEvent->hdr.event = DM_CONN_CTE_RX_SAMPLE_STOP_IND;
        }

        (*dmCb.cback)((dmEvt_t *) pEvent);
        break;

      case HCI_LE_SET_CONN_CTE_TX_PARAMS_CMD_CMPL_CBACK_EVT:
        pEvent->hdr.event = DM_CONN_CTE_TX_CFG_IND;
        (*dmCb.cback)((dmEvt_t *) pEvent);
        break;

      case HCI_LE_CONN_CTE_REQ_ENABLE_CMD_CMPL_CBACK_EVT:
        /* if enabling request */
        if (pCteCb->reqState == DM_CONN_CTE_STATE_STARTING)
        {
          if (pEvent->hdr.status == HCI_SUCCESS)
          {
            pCteCb->reqState = DM_CONN_CTE_STATE_INITIATING;
          }
          else
          {
            pCteCb->reqState = DM_CONN_CTE_STATE_IDLE;
          }

          pEvent->hdr.event = DM_CONN_CTE_REQ_START_IND;
        }
        /* else if disabing request */
        else if (pCteCb->reqState == DM_CONN_CTE_STATE_STOPPING)
        {
          if (pEvent->hdr.status == HCI_SUCCESS)
          {
            pCteCb->reqState = DM_CONN_CTE_STATE_IDLE;
          }
          else
          {
            pCteCb->reqState = DM_CONN_CTE_STATE_INITIATING;
          }

          pEvent->hdr.event = DM_CONN_CTE_REQ_STOP_IND;
        }

        (*dmCb.cback)((dmEvt_t *) pEvent);
        break;

      case HCI_LE_CONN_CTE_RSP_ENABLE_CMD_CMPL_CBACK_EVT:
        /* if enabling response */
        if (pCteCb->rspState == DM_CONN_CTE_STATE_STARTING)
        {
          if (pEvent->hdr.status == HCI_SUCCESS)
          {
            pCteCb->rspState = DM_CONN_CTE_STATE_RESPONDING;
          }
          else
          {
            pCteCb->rspState = DM_CONN_CTE_STATE_IDLE;
          }

          pEvent->hdr.event = DM_CONN_CTE_RSP_START_IND;
        }
        /* else if disabling response */
        else if (pCteCb->rspState == DM_CONN_CTE_STATE_STOPPING)
        {
          if (pEvent->hdr.status == HCI_SUCCESS)
          {
            pCteCb->rspState = DM_CONN_CTE_STATE_IDLE;
          }
          else
          {
            pCteCb->rspState = DM_CONN_CTE_STATE_RESPONDING;
          }

          pEvent->hdr.event = DM_CONN_CTE_RSP_STOP_IND;
        }

        (*dmCb.cback)((dmEvt_t *) pEvent);
        break;

      default:
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM Connection CTE event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmConnCteMsgHandler(wsfMsgHdr_t *pMsg)
{
  /* execute action function */
  (*dmConnCteAct[DM_MSG_MASK(pMsg->event)])((dmConnCteMsg_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Start sampling received CTE, and configure CTE receive parameters action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmConnCteActRxSampleStart(dmConnCteMsg_t *pMsg)
{
  dmConnCcb_t *pCcb;

  /* look up ccb from conn id */
  if ((pCcb = dmConnCcbById(pMsg->rxSampleStart.connId)) != NULL)
  {
    dmConnCteCb_t *pCteCb = &dmConnCteCb[pCcb->connId - 1];

    /* if currently idle */
    if (pCteCb->rxSampleState == DM_CONN_CTE_STATE_IDLE)
    {
      pCteCb->rxSampleState = DM_CONN_CTE_STATE_STARTING;

      /* set connection CTE receive parameters, and enable Connection IQ sampling */
      HciLeSetConnCteRxParamsCmd(pCcb->handle, TRUE, pMsg->rxSampleStart.slotDurations,
                                 pMsg->rxSampleStart.switchPatternLen,
                                 pMsg->rxSampleStart.pAntennaIDs);
    }
    else
    {
      DM_TRACE_WARN0("DmConnCteRxSampleStart ignored due to rxSampleState or pending command complete");
    }
  }
}

/*************************************************************************************************/
/*!
*  \brief  Stop sampling received CTE action function.
*
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
static void dmConnCteActRxSampleStop(dmConnCteMsg_t *pMsg)
{
  dmConnCcb_t *pCcb;

  /* look up ccb from conn id */
  if ((pCcb = dmConnCcbById(pMsg->rxSampleStop.connId)) != NULL)
  {
    dmConnCteCb_t *pCteCb = &dmConnCteCb[pCcb->connId - 1];

    /* if currently sampling */
    if (pCteCb->rxSampleState == DM_CONN_CTE_STATE_SAMPLING)
    {
      pCteCb->rxSampleState = DM_CONN_CTE_STATE_STOPPING;

      /* disable Connection IQ sampling */
      HciLeSetConnCteRxParamsCmd(pCcb->handle, FALSE, 0, 0, NULL);
    }
    else
    {
      DM_TRACE_WARN0("DmConnCteRxSampleStop ignored due to rxSampleState or pending command complete");
    }
  }
}

/*************************************************************************************************/
/*!
*  \brief  Configure connection CTE transmit action function.
*
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
static void dmConnCteActTxCfg(dmConnCteMsg_t *pMsg)
{
  dmConnCcb_t *pCcb;

  /* look up ccb from conn id */
  if ((pCcb = dmConnCcbById(pMsg->txCfg.connId)) != NULL)
  {
    HciLeSetConnCteTxParamsCmd(pCcb->handle, pMsg->txCfg.cteTypeBits, pMsg->txCfg.switchPatternLen,
                               pMsg->txCfg.pAntennaIDs);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Start initiating CTE request action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmConnCteActReqStart(dmConnCteMsg_t *pMsg)
{
  dmConnCcb_t *pCcb;

  /* look up ccb from conn id */
  if ((pCcb = dmConnCcbById(pMsg->reqStart.connId)) != NULL)
  {
    dmConnCteCb_t *pCteCb = &dmConnCteCb[pCcb->connId - 1];

    /* if currently idle */
    if (pCteCb->reqState == DM_CONN_CTE_STATE_IDLE)
    {
      pCteCb->reqState = DM_CONN_CTE_STATE_STARTING;

      /* enable connection CTE request */
      HciLeConnCteReqEnableCmd(pCcb->handle, TRUE, pMsg->reqStart.cteReqInt,
                               pMsg->reqStart.reqCteLen, pMsg->reqStart.reqCteType);
    }
    else
    {
      DM_TRACE_WARN0("DmConnCteReqStart ignored due to reqState or pending command complete");
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop initiating CTE request action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmConnCteActReqStop(dmConnCteMsg_t *pMsg)
{
  dmConnCcb_t *pCcb;

  /* look up ccb from conn id */
  if ((pCcb = dmConnCcbById(pMsg->reqStop.connId)) != NULL)
  {
    dmConnCteCb_t *pCteCb = &dmConnCteCb[pCcb->connId - 1];

    /* if currently initiating */
    if (pCteCb->reqState == DM_CONN_CTE_STATE_INITIATING)
    {
      pCteCb->reqState = DM_CONN_CTE_STATE_STOPPING;

      /* disable connection CTE request */
      HciLeConnCteReqEnableCmd(pCcb->handle, FALSE, 0, 0, 0);
    }
    else
    {
      DM_TRACE_WARN0("DmConnCteReqStop ignored due to reqState or pending command complete");
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Start responding to CTE request action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmConnCteActRspStart(dmConnCteMsg_t *pMsg)
{
  dmConnCcb_t *pCcb;

  /* look up ccb from conn id */
  if ((pCcb = dmConnCcbById(pMsg->rspEnable.connId)) != NULL)
  {
    dmConnCteCb_t *pCteCb = &dmConnCteCb[pCcb->connId - 1];

    /* if currently idle */
    if (pCteCb->rspState == DM_CONN_CTE_STATE_IDLE)
    {
      pCteCb->rspState = DM_CONN_CTE_STATE_STARTING;

      /* enable connection CTE response */
      HciLeConnCteRspEnableCmd(pCcb->handle, TRUE);
    }
    else
    {
      DM_TRACE_WARN0("DmConnCteRspStart ignored due to rspState or pending command complete");
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop responding to CTE request action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmConnCteActRspStop(dmConnCteMsg_t *pMsg)
{
  dmConnCcb_t *pCcb;

  /* look up ccb from conn id */
  if ((pCcb = dmConnCcbById(pMsg->rspEnable.connId)) != NULL)
  {
    dmConnCteCb_t *pCteCb = &dmConnCteCb[pCcb->connId - 1];

    /* if currently responding */
    if (pCteCb->rspState == DM_CONN_CTE_STATE_RESPONDING)
    {
      pCteCb->rspState = DM_CONN_CTE_STATE_STOPPING;

      /* disable connection CTE response */
      HciLeConnCteRspEnableCmd(pCcb->handle, FALSE);
    }
    else
    {
      DM_TRACE_WARN0("DmConnCteRspStop ignored due to rspState or pending command complete");
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Connection state change action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmConnCteActState(dmConnCteMsg_t *pMsg)
{
  /* if connection closed */
  if (pMsg->hdr.status == DM_CONN_CLOSE_IND)
  {
    /* reset all states */
    dmConnCteCbInit(&dmConnCteCb[pMsg->hdr.param - 1]);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Enable sampling received CTE fields on the specified connection, and configure the
 *          antenna switching pattern, and switching and sampling slot durations to be used.
 *
 *  \param  connId           Connection identifier.
 *  \param  slotDurations    Switching and sampling slot durations to be used while receiving CTE.
 *  \param  switchPatternLen Number of Antenna IDs in switching pattern.
 *  \param  pAntennaIDs      List of Antenna IDs in switching pattern.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmConnCteRxSampleStart(dmConnId_t connId, uint8_t slotDurations, uint8_t switchPatternLen,
                            uint8_t *pAntennaIDs)
{
  dmConnCteApiRxSampleStart_t *pMsg;

  WSF_ASSERT((switchPatternLen >= HCI_MIN_NUM_ANTENNA_IDS) && \
             (switchPatternLen <= HCI_MAX_NUM_ANTENNA_IDS));

  if ((pMsg = WsfMsgAlloc(sizeof(dmConnCteApiRxSampleStart_t) + switchPatternLen)) != NULL)
  {
    pMsg->hdr.event = DM_CONN_CTE_MSG_API_RX_SAMPLE_START;
    pMsg->connId = connId;
    pMsg->slotDurations = slotDurations;
    pMsg->switchPatternLen = switchPatternLen;

    /* Copy antenna IDs to space after end of config struct */
    pMsg->pAntennaIDs = (uint8_t *)(pMsg + 1);
    memcpy(pMsg->pAntennaIDs, pAntennaIDs, switchPatternLen);

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Disable sampling received CTE fields on the specified connection.
 *
 *  \param  connId           Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmConnCteRxSampleStop(dmConnId_t connId)
{
  dmConnCteApiRxSampleStop_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmConnCteApiRxSampleStop_t))) != NULL)
  {
    pMsg->hdr.event = DM_CONN_CTE_MSG_API_RX_SAMPLE_STOP;
    pMsg->connId = connId;

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Configure the antenna switching pattern, and permitted CTE types used for transmitting
 *          CTEs requested by the peer device on the specified connection.
 *
 *  \param  connId           Connection identifier.
 *  \param  cteTypeBits      Permitted CTE type bits used for transmitting CTEs requested by peer.
 *  \param  switchPatternLen Number of Antenna IDs in switching pattern.
 *  \param  pAntennaIDs      List of Antenna IDs in switching pattern.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmConnCteTxConfig(dmConnId_t connId, uint8_t cteTypeBits, uint8_t switchPatternLen,
                       uint8_t *pAntennaIDs)
{
  dmConnCteApiTxConfig_t *pMsg;

  WSF_ASSERT((switchPatternLen >= HCI_MIN_NUM_ANTENNA_IDS) && \
             (switchPatternLen <= HCI_MAX_NUM_ANTENNA_IDS));

  if ((pMsg = WsfMsgAlloc(sizeof(dmConnCteApiTxConfig_t) + switchPatternLen)) != NULL)
  {
    pMsg->hdr.event = DM_CONN_CTE_MSG_API_TX_CFG;
    pMsg->connId = connId;
    pMsg->cteTypeBits = cteTypeBits;
    pMsg->switchPatternLen = switchPatternLen;

    /* Copy antenna IDs to space after end of config struct */
    pMsg->pAntennaIDs = (uint8_t *)(pMsg + 1);
    memcpy(pMsg->pAntennaIDs, pAntennaIDs, switchPatternLen);

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate the CTE Request procedure on the specified connection.
 *
 *  \param  connId           Connection identifier.
 *  \param  cteReqInt        CTE request interval.
 *  \param  reqCteLen        Minimum length of CTE being requested in 8 us units.
 *  \param  reqCteType       Requested CTE type.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmConnCteReqStart(dmConnId_t connId, uint16_t cteReqInt, uint8_t reqCteLen,
                       uint8_t reqCteType)
{
  dmConnCteApiReqStart_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmConnCteApiReqStart_t))) != NULL)
  {
    pMsg->hdr.event = DM_CONN_CTE_MSG_API_REQ_START;
    pMsg->connId = connId;
    pMsg->cteReqInt = cteReqInt;
    pMsg->reqCteLen = reqCteLen;
    pMsg->reqCteType = reqCteType;

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop initiating the CTE Request procedure on the specified connection.
 *
 *  \param  connId           Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmConnCteReqStop(dmConnId_t connId)
{
  dmConnCteApiReqStop_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmConnCteApiReqStop_t))) != NULL)
  {
    pMsg->hdr.event = DM_CONN_CTE_MSG_API_REQ_STOP;
    pMsg->connId = connId;

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Start responding to LL_CTE_REQ PDUs with LL_CTE_RSP PDUs on the specified connection.
 *
 *  \param  connId           Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmConnCteRspStart(dmConnId_t connId)
{
  dmConnCteApiRspEnable_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmConnCteApiRspEnable_t))) != NULL)
  {
    pMsg->hdr.event = DM_CONN_CTE_MSG_API_RSP_START;
    pMsg->connId = connId;

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop responding to LL_CTE_REQ PDUs with LL_CTE_RSP PDUs on the specified connection.
 *
 *  \param  connId           Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmConnCteRspStop(dmConnId_t connId)
{
  dmConnCteApiRspEnable_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmConnCteApiRspEnable_t))) != NULL)
  {
    pMsg->hdr.event = DM_CONN_CTE_MSG_API_RSP_STOP;
    pMsg->connId = connId;

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Read the switching rates, the sampling rates, the number of antennae, and the maximum
 *          length of a transmitted Constant Tone Extension supported by the Controller.
 *
 *  \return None.
 *
 *  \note   The antenna info will be returned with DM indication \ref DM_READ_ANTENNA_INFO_IND.
 */
/*************************************************************************************************/
void DmReadAntennaInfo(void)
{
  HciLeReadAntennaInfoCmd();
}

/*************************************************************************************************/
/*!
 *  \brief  Returns the device manager's CTE request state for a given connection.
 *
 *  \param  connId           Connection identifier.
 *
 *  \return The CTE request state.
 */
/*************************************************************************************************/
uint8_t DmConnCteGetReqState(dmConnId_t connId)
{
  WSF_ASSERT((connId > DM_CONN_ID_NONE) && (connId <= DM_CONN_MAX));

  return dmConnCteCb[connId-1].reqState;
}

/*************************************************************************************************/
/*!
 *  \brief  Returns the device manager's CTE response state for a given connection.
 *
 *  \param  connId           Connection identifier.
 *
 *  \return The CTE response state.
 */
/*************************************************************************************************/
uint8_t DmConnCteGetRspState(dmConnId_t connId)
{
  WSF_ASSERT((connId > DM_CONN_ID_NONE) && (connId <= DM_CONN_MAX));

  return dmConnCteCb[connId-1].rspState;
}
