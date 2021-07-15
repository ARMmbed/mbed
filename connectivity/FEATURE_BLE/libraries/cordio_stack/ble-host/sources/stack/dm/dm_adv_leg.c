/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Device manager advertising module.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "dm_api.h"
#include "dm_adv.h"
#include "dm_dev.h"
#include "dm_main.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Control block for legacy advertising module */
typedef struct
{
  uint8_t             advType;         /*!< Advertising type. */
} dmLegAdvCb_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* action function table */
static const dmAdvAct_t dmAdvAct[] =
{
  dmAdvActConfig,
  dmAdvActSetData,
  dmAdvActStart,
  dmAdvActStop,
  dmAdvActRemoveSet,
  dmAdvActClearSets,
  dmAdvActSetRandAddr,
  dmAdvActTimeout
};

/* Component function interface */
static const dmFcnIf_t dmAdvFcnIf =
{
  dmAdvReset,
  dmAdvHciHandler,
  dmAdvMsgHandler
};

/* control block */
static dmLegAdvCb_t dmLegAdvCb;

/*************************************************************************************************/
/*!
 *  \brief  Set the advertising parameters using the given advertising type, and peer address.
 *
 *  \param  advType       Advertising type.
 *  \param  peerAddrType  Peer address type.
 *  \param  pPeerAddr     Peer address.
 *
 *  \return None.
*/
/*************************************************************************************************/
static void dmAdvConfig(uint8_t advType, uint8_t peerAddrType, uint8_t *pPeerAddr)
{
  /* set advertising parameters */
  HciLeSetAdvParamCmd(dmAdvCb.intervalMin[DM_ADV_HANDLE_DEFAULT], /* advIntervalMin */
                      dmAdvCb.intervalMax[DM_ADV_HANDLE_DEFAULT], /* advIntervalMax */
                      advType,                                    /* advType */
                      DmLlAddrType(dmCb.advAddrType),             /* ownAddrType */
                      peerAddrType,                               /* peerAddrType */
                      pPeerAddr,                                  /* pPeerAddr */
                      dmAdvCb.channelMap[DM_ADV_HANDLE_DEFAULT],  /* advChanMap */
                      dmCb.advFiltPolicy[DM_ADV_HANDLE_DEFAULT]); /* advFiltPolicy */

  /* store advertising type */
  dmLegAdvCb.advType = advType;
}

/*************************************************************************************************/
/*!
 *  \brief  Start advertising action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvActConfig(dmAdvMsg_t *pMsg)
{
  DM_TRACE_INFO1("dmAdvActConfig: state: %d", dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT]);

  if (dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_IDLE)
  {
    /* if doing directed advertising ignore the request */
    if ((dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] == DM_ADV_CONN_DIRECT) ||
        (dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] == DM_ADV_CONN_DIRECT_LO_DUTY))
    {
      DM_TRACE_WARN0("DmAdvConfig during directed advertising!");
      return;
    }

    /* set advertising parameters */
    dmAdvConfig(pMsg->apiConfig.advType, pMsg->apiConfig.peerAddrType, pMsg->apiConfig.peerAddr);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the advertising or scan response data to the given data.
 *
 *  \param  location  Data location.
 *  \param  len       Length of the data.  Maximum length is 31 bytes.
 *  \param  pData     Pointer to the data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvActSetData(dmAdvMsg_t *pMsg)
{
  WSF_ASSERT(pMsg->apiSetData.len <= HCI_ADV_DATA_LEN);

  DM_TRACE_INFO1("dmAdvActSetData: state: %d", dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT]);

  /* set new data in HCI */
  if (pMsg->apiSetData.location == DM_DATA_LOC_ADV)
  {
    HciLeSetAdvDataCmd(pMsg->apiSetData.len, pMsg->apiSetData.pData);
  }
  else
  {
    HciLeSetScanRespDataCmd(pMsg->apiSetData.len, pMsg->apiSetData.pData);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Start advertising action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvActStart(dmAdvMsg_t *pMsg)
{
  DM_TRACE_INFO1("dmAdvActStart: state: %d", dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT]);

  if (dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_IDLE)
  {
    /* if doing directed advertising ignore the request */
    if ((dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] == DM_ADV_CONN_DIRECT) ||
        (dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] == DM_ADV_CONN_DIRECT_LO_DUTY))
    {
      DM_TRACE_WARN0("dmAdvActStart during directed advertising!");
      return;
    }

    /* start advertising */
    dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = DM_ADV_STATE_STARTING;
    dmAdvCb.advDuration[DM_ADV_HANDLE_DEFAULT] = pMsg->apiStart.duration[DM_ADV_HANDLE_DEFAULT];
    HciLeSetAdvEnableCmd(TRUE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop advertising action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvActStop(dmAdvMsg_t *pMsg)
{
  DM_TRACE_INFO1("dmAdvActStop: state: %d", dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT]);

  if (dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_ADVERTISING)
  {
    /* if doing directed advertising ignore the request */
    if ((dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] == DM_ADV_CONN_DIRECT) ||
        (dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] == DM_ADV_CONN_DIRECT_LO_DUTY))
    {
      DM_TRACE_WARN0("DmAdvStop during directed advertising!");
      return;
    }

    /* disable advertising */
    dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = DM_ADV_STATE_STOPPING;
    HciLeSetAdvEnableCmd(FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Remove an advertising set action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvActRemoveSet(dmAdvMsg_t *pMsg)
{
  /* empty */
}

/*************************************************************************************************/
/*!
 *  \brief  Clear advertising sets action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvActClearSets(dmAdvMsg_t *pMsg)
{
  /* empty */
}

/*************************************************************************************************/
/*!
*  \brief  Set the random device address for a given advertising set.
*
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
void dmAdvActSetRandAddr(dmAdvMsg_t *pMsg)
{
  /* empty */
}

/*************************************************************************************************/
/*!
 *  \brief  Handle an advertising timeout.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvActTimeout(dmAdvMsg_t *pMsg)
{
  DM_TRACE_INFO0("dmAdvActTimeout!");

  if (dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_ADVERTISING)
  {
    /* disable advertising */
    dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = DM_ADV_STATE_STOPPING;
    HciLeSetAdvEnableCmd(FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Reset the legacy adv module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvReset(void)
{
  wsfMsgHdr_t advStop;

  /* if stopping undirected advertisement or advertising but not high duty cycle directed adv */
  if ((dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_STOPPING)     ||
      ((dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_ADVERTISING) &&
       (dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] != DM_ADV_CONN_DIRECT)))
  {
    /* stop advertising timer */
    WsfTimerStop(&dmAdvCb.advTimer);

    /* generate advertising stop event */
    advStop.status = HCI_SUCCESS;
    advStop.event = DM_ADV_STOP_IND;

    /* call callback */
    (*dmCb.cback)((dmEvt_t *) &advStop);
  }

  /* reset legacy adv module */
  dmAdvInit();
}

/*************************************************************************************************/
/*!
 *  \brief  DM adv HCI event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvHciHandler(hciEvt_t *pEvent)
{
  if (pEvent->hdr.event == HCI_LE_ADV_ENABLE_CMD_CMPL_CBACK_EVT)
  {
    uint8_t cbackEvent = 0;

    DM_TRACE_INFO1("HCI_LE_ADV_ENABLE_CMD_CMPL_CBACK_EVT: state: %d", dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT]);

    switch (dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT])
    {
    case DM_ADV_STATE_STARTING:
    case DM_ADV_STATE_STARTING_DIRECTED:
      if (pEvent->hdr.status == HCI_SUCCESS)
      {
        if (dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_STARTING)
        {
          /* start advertising timer if applicable */
          if (dmAdvCb.advDuration[DM_ADV_HANDLE_DEFAULT] > 0)
          {
            dmAdvCb.advTimer.msg.event = DM_ADV_MSG_TIMEOUT;
            WsfTimerStartMs(&dmAdvCb.advTimer, dmAdvCb.advDuration[DM_ADV_HANDLE_DEFAULT]);
          }

          /* Application callbacks only sent in undirected state */
          if (dmLegAdvCb.advType != DM_ADV_CONN_DIRECT_LO_DUTY)
          {
            cbackEvent = DM_ADV_START_IND;
          }
        }

        /* pass advertising start event to dev priv */
        dmDevPassEvtToDevPriv(DM_DEV_PRIV_MSG_RPA_START, DM_ADV_START_IND, 0, 0);

        /* store advertising type and state */
        dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] = dmLegAdvCb.advType;
        dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = DM_ADV_STATE_ADVERTISING;
      }
      else
      {
        dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = DM_ADV_STATE_IDLE;
      }
      break;

    case DM_ADV_STATE_STOPPING:
    case DM_ADV_STATE_STOPPING_DIRECTED:
      if (pEvent->hdr.status == HCI_SUCCESS)
      {
        if (dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_STOPPING)
        {
          /* stop advertising timer */
          WsfTimerStop(&dmAdvCb.advTimer);

          /* Application and DM callbacks only sent in undirected or low duty cycle directed state */
          if (dmLegAdvCb.advType == DM_ADV_CONN_DIRECT_LO_DUTY)
          {
            cbackEvent = HCI_LE_ENHANCED_CONN_CMPL_CBACK_EVT;
          }
          else
          {
            cbackEvent = DM_ADV_STOP_IND;
          }
        }

        /* pass advertising stop event to dev priv */
        dmDevPassEvtToDevPriv(DM_DEV_PRIV_MSG_RPA_STOP, DM_ADV_STOP_IND, 0, 0);

        /* store advertising type and state */
        dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] = DM_ADV_NONE;
        dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = DM_ADV_STATE_IDLE;
      }
      else
      {
        dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = DM_ADV_STATE_ADVERTISING;
      }
      break;

    default:
      /* ignore the event */
      break;
    }

    /* if DM conn notify needed */
    if (cbackEvent == HCI_LE_ENHANCED_CONN_CMPL_CBACK_EVT)
    {
      dmAdvGenConnCmpl(DM_ADV_HANDLE_DEFAULT, HCI_ERR_ADV_TIMEOUT);
    }
    /* else if app callback needed */
    else if (cbackEvent)
    {
      pEvent->hdr.event = cbackEvent;
      (*dmCb.cback)((dmEvt_t *) pEvent);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM adv event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvMsgHandler(wsfMsgHdr_t *pMsg)
{
  /* execute action function */
  (*dmAdvAct[DM_MSG_MASK(pMsg->event)])((dmAdvMsg_t *)pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Start directed advertising.
 *
 *  \param  advType       Advertising type.
 *  \param  duration      Advertising duration (in ms).
 *  \param  addrType      Address type.
 *  \param  pAddr         Peer device address.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvStartDirected(uint8_t advType, uint16_t duration, uint8_t addrType, uint8_t *pAddr)
{
  DM_TRACE_INFO1("dmAdvStartDirected: state: %d", dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT]);

  /* if not advertising */
  if (dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_IDLE)
  {
    /* store advertising info */
    dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = (advType == DM_ADV_CONN_DIRECT) ? \
                                              DM_ADV_STATE_STARTING_DIRECTED : DM_ADV_STATE_STARTING;

    dmAdvCb.advDuration[DM_ADV_HANDLE_DEFAULT] = duration;
    BdaCpy(dmAdvCb.peerAddr[DM_ADV_HANDLE_DEFAULT], pAddr);
    dmAdvCb.peerAddrType[DM_ADV_HANDLE_DEFAULT] = addrType;

    /* start advertising */
    HciLeSetAdvEnableCmd(TRUE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop directed advertising.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvStopDirected(void)
{
  DM_TRACE_INFO1("dmAdvStopDirected: state: %d", dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT]);

  /* if advertising or starting advertising */
  if ((dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_ADVERTISING) ||
      (dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_STARTING)    ||
      (dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] == DM_ADV_STATE_STARTING_DIRECTED))
  {
    /* disable advertising */
    dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = (dmLegAdvCb.advType == DM_ADV_CONN_DIRECT) ? \
                                              DM_ADV_STATE_STOPPING_DIRECTED : DM_ADV_STATE_STOPPING;
    HciLeSetAdvEnableCmd(FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  This function is called when a connection is established from directed advertising.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvConnected(void)
{
  DM_TRACE_INFO1("dmAdvConnected: state: %d", dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT]);

  WsfTimerStop(&dmAdvCb.advTimer);

  /* pass advertising stop event to dev priv */
  dmDevPassEvtToDevPriv(DM_DEV_PRIV_MSG_RPA_STOP, DM_ADV_STOP_IND, 0, 0);

  dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] = DM_ADV_NONE;
  dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = DM_ADV_STATE_IDLE;
}

/*************************************************************************************************/
/*!
 *  \brief  This function is called when a directed advertising connection fails.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvConnectFailed(void)
{
  DM_TRACE_INFO1("dmAdvConnectFailed: state: %d", dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT]);

  WsfTimerStop(&dmAdvCb.advTimer);

  /* pass advertising stop event to dev priv */
  dmDevPassEvtToDevPriv(DM_DEV_PRIV_MSG_RPA_STOP, DM_ADV_STOP_IND, 0, 0);

  dmAdvCb.advType[DM_ADV_HANDLE_DEFAULT] = DM_ADV_NONE;
  dmAdvCb.advState[DM_ADV_HANDLE_DEFAULT] = DM_ADV_STATE_IDLE;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize DM legacy advertising.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvInit(void)
{
  WsfTaskLock();

  /* set function interface table */
  dmFcnIfTbl[DM_ID_ADV] = (dmFcnIf_t *) &dmAdvFcnIf;

  /* initialize legacy adv module */
  dmAdvInit();

  /* clear set advertising set random address callback */
  dmDevCb.advSetRandAddrCback = NULL;

  /* initialize HCI VS module */
  HciVsInit(0);

  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Whether DM advertising is in legacy mode.
 *
 *  \return TRUE if DM advertising is in legacy mode. FALSE, otherwise.
 */
/*************************************************************************************************/
bool_t DmAdvModeLeg(void)
{
  return (dmFcnIfTbl[DM_ID_ADV] == (dmFcnIf_t *) &dmAdvFcnIf) ? TRUE : FALSE;
}
