/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Device manager periodic advertising sync transfer (PAST) module.
 *
 *  Copyright (c) 2018 Arm Ltd. All Rights Reserved.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "dm_api.h"
#include "dm_main.h"
#include "dm_adv.h"
#include "dm_conn.h"
#include "dm_scan.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! DM past event handler messages */
enum
{
  /* messages from API */
  DM_PAST_MSG_API_RCV_ENABLE = DM_MSG_START(DM_ID_PAST), /*!< Enable receiving report */
  DM_PAST_MSG_API_SYNC_TRSF,                             /*!< Transfer sync */
  DM_PAST_MSG_API_SET_INFO_TRSF,                         /*!< Transfer set info */
  DM_PAST_MSG_API_CFG,                                   /*!< Configure PAST parameters */
  DM_PAST_MSG_API_DEFAULT_CFG                            /*!< Configure PAST default parameters */
};


/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* action function table */
static const dmPastAct_t dmPastAct[] =
{
  dmPastActRptRcvEnable,
  dmPastActSyncTsfr,
  dmPastActSetInfoTrsf,
  dmPastActConfig,
  dmPastActDefaultConfig
};

/*! DM PAST component function interface */
static const dmFcnIf_t dmPastFcnIf =
{
  dmEmptyReset,
  dmPastHciHandler,
  dmPastMsgHandler
};

/*************************************************************************************************/
/*!
 *  \brief  Initialize DM Periodic Advertising Sync Transfer (PAST) module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPastInit(void)
{
  WsfTaskLock();

  /* set function interface table */
  dmFcnIfTbl[DM_ID_PAST] = (dmFcnIf_t *) &dmPastFcnIf;

  HciSetLeSupFeat((HCI_LE_SUP_FEAT_RECV_CTE | HCI_LE_SUP_FEAT_PAST_SENDER | HCI_LE_SUP_FEAT_PAST_RECIPIENT), TRUE);

  WsfTaskUnlock();
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
void dmPastHciHandler(hciEvt_t *pEvent)
{
  dmConnCcb_t *pCcb = dmConnCcbByHandle(pEvent->hdr.param);

  /* if ccb found */
  if (pCcb != NULL)
  {
    /* set conn id */
    pEvent->hdr.param = pCcb->connId;

    if (pEvent->hdr.event == HCI_LE_PER_ADV_SYNC_TRSF_CMD_CMPL_CBACK_EVT)
    {
      pEvent->hdr.event = DM_PER_ADV_SYNC_TRSF_IND;
      (*dmCb.cback)((dmEvt_t *) pEvent);
    }
    else if (pEvent->hdr.event == HCI_LE_PER_ADV_SET_INFO_TRSF_CMD_CMPL_CBACK_EVT)
    {
      pEvent->hdr.event = DM_PER_ADV_SET_INFO_TRSF_IND;
      (*dmCb.cback)((dmEvt_t *) pEvent);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM PAST event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmPastMsgHandler(wsfMsgHdr_t *pMsg)
{
  /* execute action function */
  (*dmPastAct[DM_MSG_MASK(pMsg->event)])((dmPastMsg_t *) pMsg);
}

/*************************************************************************************************/
/*!
*  \brief  Enable receiving report action function.
*
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
void dmPastActRptRcvEnable(dmPastMsg_t *pMsg)
{
  dmSyncCb_t *pScb;

  /* look up scb from sync id */
  if ((pScb = dmSyncCbById((dmSyncId_t) pMsg->hdr.param)) != NULL)
  {
    HciLeSetPerAdvRcvEnableCmd(pScb->handle, (pMsg->hdr.status ? TRUE : FALSE));
  }
}

/*************************************************************************************************/
/*!
*  \brief  Transfer sync action function.
*
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
void dmPastActSyncTsfr(dmPastMsg_t *pMsg)
{
  dmSyncCb_t *pScb;

  /* look up scb from sync id */
  if ((pScb = dmSyncCbById((dmSyncId_t) pMsg->hdr.param)) != NULL)
  {
    dmConnCcb_t *pCcb;

    /* look up ccb from conn id */
    if ((pCcb = dmConnCcbById(pMsg->apiPastTrsf.connId)) != NULL)
    {
      HciLePerAdvSyncTrsfCmd(pCcb->handle, pMsg->apiPastTrsf.serviceData, pScb->handle);
    }
  }
}

/*************************************************************************************************/
/*!
*  \brief  Transfer set info action function.
*
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
void dmPastActSetInfoTrsf(dmPastMsg_t *pMsg)
{
  uint8_t advHandle = (uint8_t) pMsg->hdr.param;

  /* if periodic advertising is currently in progress for the advertising set */
  if (dmPerAdvState(advHandle) == DM_ADV_PER_STATE_ADVERTISING)
  {
    dmConnCcb_t *pCcb;

    /* look up ccb from conn id */
    if ((pCcb = dmConnCcbById(pMsg->apiPastTrsf.connId)) != NULL)
    {
      HciLePerAdvSetInfoTrsfCmd(pCcb->handle, pMsg->apiPastTrsf.serviceData, advHandle);
    }
  }
}

/*************************************************************************************************/
/*!
*  \brief  Configure PAST action function.
*
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
void dmPastActConfig(dmPastMsg_t *pMsg)
{
  dmConnCcb_t *pCcb;

  /* look up ccb from conn id */
  if ((pCcb = dmConnCcbById((dmConnId_t) pMsg->hdr.param)) != NULL)
  {
    HciLeSetPerAdvSyncTrsfParamsCmd(pCcb->handle, pMsg->apiPastCfg.mode, pMsg->apiPastCfg.skip,
                                    pMsg->apiPastCfg.syncTimeout, pMsg->apiPastCfg.cteType);
  }
}

/*************************************************************************************************/
/*!
*  \brief  Configure default PAST action function.
*
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
void dmPastActDefaultConfig(dmPastMsg_t *pMsg)
{
  HciLeSetDefaultPerAdvSyncTrsfParamsCmd(pMsg->apiPastCfg.mode, pMsg->apiPastCfg.skip,
                                         pMsg->apiPastCfg.syncTimeout, pMsg->apiPastCfg.cteType);
}

/*************************************************************************************************/
/*!
 *  \brief  Enable or disable reports for the periodic advertising identified by the sync id.
 *
 *  \param  syncId           Sync identifier.
 *  \param  enable           TRUE to enable reporting, FALSE to disable reporting.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPastRptRcvEnable(dmSyncId_t syncId, bool_t enable)
{
  wsfMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(wsfMsgHdr_t))) != NULL)
  {
    pMsg->param = syncId;
    pMsg->event = DM_PAST_MSG_API_RCV_ENABLE;
    pMsg->status = enable;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send synchronization information about the periodic advertising identified by the
 *          sync id to a connected device.
 *
 *  \param  connId           Connection identifier.
 *  \param  serviceData      Value provided by the Host.
 *  \param  syncId           Sync identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPastSyncTrsf(dmConnId_t connId, uint16_t serviceData, dmSyncId_t syncId)
{
  dmPastApiTrsf_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmPastApiTrsf_t))) != NULL)
  {
    pMsg->hdr.param = syncId;
    pMsg->hdr.event = DM_PAST_MSG_API_SYNC_TRSF;
    pMsg->serviceData = serviceData;
    pMsg->connId = connId;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send synchronization information about the periodic advertising in an advertising
 *          set to a connected device.
 *
 *  \param  connId           Connection identifier.
 *  \param  serviceData      Value provided by the Host.
 *  \param  advHandle        Advertising handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPastSetInfoTrsf(dmConnId_t connId, uint16_t serviceData, uint8_t advHandle)
{
  dmPastApiTrsf_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmPastApiTrsf_t))) != NULL)
  {
    pMsg->hdr.param = advHandle;
    pMsg->hdr.event = DM_PAST_MSG_API_SET_INFO_TRSF;
    pMsg->serviceData = serviceData;
    pMsg->connId = connId;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Specify how the Controller should process periodic advertising synchronization
 *          information received from the device identified by the connection handle.
 *
 *  \param  connId           Connection identifier.
 *  \param  mode             Action to be taken when periodic advertising info is received.
 *  \param  skip             Number of consecutive periodic advertising packets that the receiver
 *                           may skip after successfully receiving a periodic advertising packet.
 *  \param  syncTimeout      Maximum permitted time between successful receives. If this time is
 *                           exceeded, synchronization is lost.
 *  \param  cteType          Whether to only synchronize to periodic advertising with certain
 *                           types of Constant Tone Extension (0 indicates that the presence or
 *                           absence of a Constant Tone Extension is irrelevant).
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPastConfig(dmConnId_t connId, uint8_t mode, uint16_t skip, uint16_t syncTimeout,
                  uint8_t cteType)
{
  dmPastApiCfg_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmPastApiCfg_t))) != NULL)
  {
    pMsg->hdr.param = connId;
    pMsg->hdr.event = DM_PAST_MSG_API_CFG;
    pMsg->mode = mode;
    pMsg->skip = skip;
    pMsg->syncTimeout = syncTimeout;
    pMsg->cteType = cteType;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Specify the initial value for the mode, skip, timeout, and Constant Tone Extension type
 *          to be used for all subsequent connections over the LE transport.
 *
 *  \param  mode             Action to be taken when periodic advertising info is received.
 *  \param  skip             Number of consecutive periodic advertising packets that the receiver
 *                           may skip after successfully receiving a periodic advertising packet.
 *  \param  syncTimeout      Maximum permitted time between successful receives. If this time is
 *                           exceeded, synchronization is lost.
 *  \param  cteType          Whether to only synchronize to periodic advertising with certain
 *                           types of Constant Tone Extension (0 indicates that the presence or
 *                           absence of a Constant Tone Extension is irrelevant).
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPastDefaultConfig(uint8_t mode, uint16_t skip, uint16_t syncTimeout, uint8_t cteType)
{
  dmPastApiCfg_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmPastApiCfg_t))) != NULL)
  {
    pMsg->hdr.event = DM_PAST_MSG_API_DEFAULT_CFG;
    pMsg->mode = mode;
    pMsg->skip = skip;
    pMsg->syncTimeout = syncTimeout;
    pMsg->cteType = cteType;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}
