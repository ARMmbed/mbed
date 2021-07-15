/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  DM Connected Isochronous Stream (CIS) management for master.
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
#include "wsf_trace.h"
#include "wsf_msg.h"
#include "wsf_os.h"
#include "dm_api.h"
#include "dm_dev.h"
#include "dm_main.h"
#include "dm_conn.h"
#include "dm_cis.h"

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void dmCisSmActOpen(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg);
static void dmCisSmActCancelOpen(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg);

static void dmCisCigSmActNone(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg);
static void dmCisCigSmActConfig(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg);
static void dmCisCigSmActConfiged(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg);
static void dmCisCigSmActConfigFailed(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg);
static void dmCisCigSmActRemove(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg);
static void dmCisCigSmActRemoved(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg);
static void dmCisCigSmActRemoveFailed(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Action set for CIS master module */
static const dmCisAct_t dmCisActSetMaster[] =
{
  dmCisSmActOpen,
  dmCisSmActCancelOpen
};

/* CIS CIG component function interface */
static const dmFcnIf_t dmCisCigFcnIf =
{
  dmCisCigReset,
  dmCisCigHciHandler,
  dmCisCigMsgHandler
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! Action function table for CIG master module */
const dmCisCigAct_t dmCisCigAct[] =
{
  dmCisCigSmActNone,
  dmCisCigSmActConfig,
  dmCisCigSmActConfiged,
  dmCisCigSmActConfigFailed,
  dmCisCigSmActRemove,
  dmCisCigSmActRemoved,
  dmCisCigSmActRemoveFailed
};

/*************************************************************************************************/
/*!
 *  \brief      Check if the CIS has already been created (established or is pending establishment).
 *
 *  \param      pCcb      CIS connection control block.
 *
 *  \return     TRUE if CIS is created. FALSE, otherwise.
 */
/*************************************************************************************************/
bool_t dmCisCreated(dmCisCcb_t *pCcb)
{
  if ((pCcb->state == DM_CIS_SM_ST_CONNECTING) || (pCcb->state == DM_CIS_SM_ST_CONNECTED))
  {
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Check if the CIS has pending to be established or disconnected.
 *
 *  \param      pCcb      CIS connection control block.
 *
 *  \return     TRUE if CIS is created. FALSE, otherwise.
 */
/*************************************************************************************************/
bool_t dmCisPending(dmCisCcb_t *pCcb)
{
  if ((pCcb->state == DM_CIS_SM_ST_CONNECTING) || (pCcb->state == DM_CIS_SM_ST_DISCONNECTING))
  {
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of available CISes.
 *
 *  \param      None.
 *
 *  \return     Number of available CISes.
 */
/*************************************************************************************************/
uint8_t dmCisNumAvail(void)
{
  dmCisCcb_t   *pCcb = dmCisCb.cisCcb;
  uint8_t       i, cnt;

  for (i = DM_CIS_MAX, cnt = 0; i > 0; i--, pCcb++)
  {
    if (pCcb->inUse == FALSE)
    {
      cnt++;
    }
  }

  return cnt;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of CISes that are pending to be established or disconnected.
 *
 *  \param      pCcb      CIS connection control block.
 *
 *  \return     Number of CISes that are already set up (enabled).
 */
/*************************************************************************************************/
uint8_t dmCisNumPending(dmCisCcb_t *pCcb)
{
  dmCisCcb_t   *pCisCcb = dmCisCb.cisCcb;
  uint8_t       i, cnt;

  for (i = DM_CIS_MAX, cnt = 0; i > 0; i--, pCisCcb++)
  {
    if (pCisCcb->inUse && (pCisCcb != pCcb) && dmCisPending(pCisCcb))
    {
      cnt++;
    }
  }

  return cnt;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of CISes that are already set up (enabled) for the given CIG).
 *
 *  \param      cigId      CIG identifier.
 *  \param      numCis     Number of CISes.
 *  \param      pCisParam  Set CIG CIS parameters.
 *
 *  \return     Number of CISes that are already set up (enabled).
 */
/*************************************************************************************************/
static uint8_t dmCisNumEnabled(uint8_t cigId, uint8_t numCis, HciCisCisParams_t *pCisParam)
{
  uint8_t       i, cnt;

  for (i = numCis, cnt = 0; i > 0; i--, pCisParam++)
  {
    if (dmCisCcbById(cigId, pCisParam->cisId) != NULL)
    {
      cnt++;
    }
  }

  return cnt;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of CIS connetions that are created for the given CIG.
 *
 *  \param      cigId      CIG identifier.
 *  \param      numCis     Number of CISes.
 *  \param      pCisParam  Set CIG CIS parameters.
 *
 *  \return     Number of CIS connections that are created.
 */
/*************************************************************************************************/
uint8_t dmCisNumCreatedByCigId(uint8_t cigId, uint8_t numCis, HciCisCisParams_t *pCisParam)
{
  uint8_t       i, cnt;

  for (i = numCis, cnt = 0; i > 0; i--, pCisParam++)
  {
    dmCisCcb_t *pCcb;

    if ((pCcb = dmCisCcbById(cigId, pCisParam->cisId)) != NULL)
    {
      if (dmCisCreated(pCcb))
      {
        cnt++;
      }
    }
  }

  return cnt;
}

/*************************************************************************************************/
/*!
 *  \brief  Empty action.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisCigSmActNone(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg)
{
  return;
}

/*************************************************************************************************/
/*!
  *  \brief  Configure (set) CIG parameters for the given CIG.
  *
  *  \param  pCcb      CIS connection control block.
  *  \param  pMsg      WSF message.
  *  \param  oldState  Old state.
  *
  *  \return None.
  */
/*************************************************************************************************/
static void dmCisCigSmActConfig(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg)
{
  dmCisCigApiConfig_t *pConfig = &pMsg->apiConfig;
  uint8_t             status = HCI_SUCCESS;

  /* if none of the CISes in the CIG have been created */
  if (dmCisNumCreatedByCigId(pCigCb->cigId, pConfig->numCis, pConfig->pCisParam) == 0)
  {
    uint8_t numEnabled;

    if ((numEnabled = dmCisNumEnabled(pCigCb->cigId, pConfig->numCis, pConfig->pCisParam)) == 0)
    {
      /* Set up new CISes. */
      if (pConfig->numCis <= dmCisNumAvail())
      {
        pCigCb->numCis = pConfig->numCis;

        for (uint8_t i = 0; i < pConfig->numCis; i++)
        {
          dmCisCcbAlloc(pCigCb->cigId, pConfig->pCisParam[i].cisId, DM_ROLE_MASTER);

          /* remember CIS IDs being added or modified */
          pCigCb->cisId[i] = pConfig->pCisParam[i].cisId;
        }
      }
      else
      {
        DM_TRACE_WARN0("dmCisCigSmActConfig: there isn't enough CISes available");
        status = HCI_ERR_CONN_LIMIT;
      }
    }
    else
    {
      if ((pConfig->numCis - numEnabled) <= dmCisNumAvail())
      {
        pCigCb->numCis = pConfig->numCis;

        for (uint8_t i = 0; i < pConfig->numCis; i++)
        {
          /* allocate entries only for new CISes */
          if (dmCisCcbById(pCigCb->cigId, pConfig->pCisParam[i].cisId) == NULL)
          {
            dmCisCcbAlloc(pCigCb->cigId, pConfig->pCisParam[i].cisId, DM_ROLE_MASTER);
          }

          /* remember CIS IDs being added or modified */
          pCigCb->cisId[i] = pConfig->pCisParam[i].cisId;
        }
      }
      else
      {
        DM_TRACE_WARN0("dmCisCigSmActConfig: there isn't enough CISes available");
        status = HCI_ERR_CONN_LIMIT;
      }
    }
  }
  else
  {
    DM_TRACE_WARN0("dmCisCigSmActConfig: there're CISes already created");
    status = HCI_ERR_CMD_DISALLOWED;
  }

  if (status == HCI_SUCCESS)
  {
    HciCisCigParams_t cigParam;

    cigParam.cigId = pCigCb->cigId;
    cigParam.sduIntervalMToS = pCigCb->sduIntervalMToS;
    cigParam.sduIntervalSToM = pCigCb->sduIntervalSToM;
    cigParam.sca = pCigCb->sca;
    cigParam.packing = pCigCb->packing;
    cigParam.framing = pCigCb->framing;
    cigParam.transLatMToS = pCigCb->transLatMToS;
    cigParam.transLatSToM = pCigCb->transLatSToM;
    cigParam.numCis = pMsg->apiConfig.numCis;
    cigParam.pCisParam = pMsg->apiConfig.pCisParam;

    /* set CIG parameters */
    HciLeSetCigParamsCmd(&cigParam);
  }
  else
  {
    pMsg->hdr.status = status;

    /* notify app about failure */
    dmCisCigSmActConfigFailed(pCigCb, pMsg);

    /* restore old state */
    pCigCb->state = DM_CIS_SM_RESTORE_OLD_STATE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a CIG configured event from HCI.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisCigSmActConfiged(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg)
{
  /* store connection handles of CISes that have been created */
  for (uint8_t i = 0; i < pMsg->hciLeSetCigParamsCmdCmpl.numCis; i++)
  {
    dmCisCcb_t  *pCcb;

    if ((pCcb = dmCisCcbById(pCigCb->cigId, pCigCb->cisId[i])) != NULL)
    {
      pCcb->cisHandle = pMsg->hciLeSetCigParamsCmdCmpl.cisHandle[i];
    }
  }

  pMsg->hdr.event = DM_CIS_CIG_CONFIG_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a CIG configure failure event from HCI.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisCigSmActConfigFailed(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg)
{
  if (pCigCb->state != DM_CIS_CIG_SM_ST_CONFIGED)
  {
    dmCisCigCbDealloc(pCigCb);
  }

  pMsg->hdr.event = DM_CIS_CIG_CONFIG_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
  *  \brief  Remove CIG parameters for the given CIG.
  *
  *  \param  pCcb      CIS connection control block.
  *  \param  pMsg      WSF message.
  *  \param  oldState  Old state.
  *
  *  \return None.
  */
/*************************************************************************************************/
static void dmCisCigSmActRemove(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg)
{
  dmCisCcb_t  *pCcb = dmCisCb.cisCcb;
  uint8_t      status = HCI_SUCCESS;

  /* if none of the CISes in the CIG have been established or is pending establishment */
  for (uint8_t i = DM_CIS_MAX; i > 0; i--, pCcb++)
  {
    if ((pCcb->inUse == TRUE) && (pCcb->cigId == pCigCb->cigId))
    {
      /* if CIS in slave role */
      if (pCcb->role == DM_ROLE_SLAVE)
      {
        DM_TRACE_WARN0("dmCisCigSmActRemove: invalid role");
        status = HCI_ERR_CMD_DISALLOWED;
        break;
      }

      /* if any CIS in the CIG has already been established or is pending establishment */
      if (dmCisCreated(pCcb))
      {
        DM_TRACE_WARN0("dmCisCigSmActRemove: there're CISes established or pending establishment");
        status = HCI_ERR_CMD_DISALLOWED;
        break;
      }
    }
  }

  if (status == HCI_SUCCESS)
  {
    /* remove CIG */
    HciLeRemoveCigCmd(pCigCb->cigId);
  }
  else
  {
    pMsg->hdr.status = status;

    /* notify app about failure */
    dmCisCigSmActRemoveFailed(pCigCb, pMsg);

    /* restore old state */
    pCigCb->state = DM_CIS_SM_RESTORE_OLD_STATE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a CIG removed event from HCI.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisCigSmActRemoved(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg)
{
  /* deallocate associated CIS and CIG control blocks */
  dmCisCcbDeallocByCigId(pCigCb->cigId);
  dmCisCigCbDealloc(pCigCb);

  pMsg->hdr.event = DM_CIS_CIG_REMOVE_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a CIG remove failure event from HCI.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisCigSmActRemoveFailed(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg)
{
  pMsg->hdr.event = DM_CIS_CIG_REMOVE_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Open a CIS connection.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisSmActOpen(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg)
{
  dmCisApiOpen_t  *pOpen = &pMsg->apiOpen;
  uint16_t        aclHandle[DM_CIS_MAX];
  uint8_t         status = HCI_SUCCESS;

  /* if the previous create CIS isn't pending or hasn't been cancelled */
  if (dmCisNumPending(pCcb) == 0)
  {
    for (uint8_t i = 0; i < pOpen->numCis; i++)
    {
      dmConnCcb_t *pConnCcb;

      /* if the ACL connection handle doesn't exist */
      if ((pConnCcb = dmConnCcbById(pOpen->pConnId[i])) == NULL)
      {
        DM_TRACE_WARN1("dmCisSmActOpen: ACL connection id not found (connId:%d)", pOpen->pConnId[i]);
        status = HCI_ERR_UNKNOWN_HANDLE;
        break;
      }

      /* if slave */
      if (pConnCcb->role == DM_ROLE_SLAVE)
      {
        DM_TRACE_WARN0("dmCisSmActOpen: invalid role");
        status = HCI_ERR_CMD_DISALLOWED;
        break;
      }

      /* if the CIS connection handle does not exist */
      if (dmCisCcbByHandle(pOpen->pCisHandle[i]) == NULL)
      {
        DM_TRACE_WARN1("dmCisSmActOpen: CIS handle not found (handle:%d)", pOpen->pCisHandle[i]);
        status = HCI_ERR_UNKNOWN_HANDLE;
        break;
      }

      /* save ACL handle */
      aclHandle[i] = pConnCcb->handle;
    }
  }
  else
  {
    DM_TRACE_WARN0("dmCisSmActOpen: there're CISes pending to be established or disconnected");
    status = HCI_ERR_CMD_DISALLOWED;
  }

  if (status == HCI_SUCCESS)
  {
    HciCisCreateCisParams_t createCisParam;

    createCisParam.pAclHandle = aclHandle;
    createCisParam.pCisHandle = pOpen->pCisHandle;

    /* create CIS */
    HciLeCreateCisCmd(pOpen->numCis, &createCisParam);

    /* set new state and ACL handle for all CISes */
    for (uint8_t i = 0; i < pOpen->numCis; i++)
    {
      dmCisCcb_t *pCisCcb;

      if ((pCisCcb = dmCisCcbByHandle(pOpen->pCisHandle[i])) != NULL)
      {
        pCisCcb->state = pCcb->state;
        pCisCcb->aclHandle = aclHandle[i];
      }
    }
  }
  else
  {
    pMsg->hdr.status = status;

    /* notify app about failure */
    dmCisSmActCisEstFailed(pCcb, pMsg);

    /* restore old state */
    pCcb->state = DM_CIS_SM_RESTORE_OLD_STATE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Cancel a CIS opening connection.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisSmActCancelOpen(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg)
{
  /* cancel CIS create connection */
  HciDisconnectCmd(pMsg->hdr.param, pMsg->apiClose.reason);
}

/*************************************************************************************************/
/*!
 *  \brief  Reset the CIS CIG module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmCisCigReset(void)
{
  dmCisCigCb_t  *pCigCb = dmCisCb.cisCigCb;
  uint8_t       i;

  for (i = 0; i < DM_CIG_MAX; i++, pCigCb++)
  {
    if (pCigCb->inUse)
    {
      dmCisCcbDeallocByCigId(pCigCb->cigId);
      dmCisCigCbDealloc(pCigCb);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM CIS CIG WSF message handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmCisCigMsgHandler(wsfMsgHdr_t *pMsg)
{
  dmCisCigCb_t *pCigCb;

  /* look up cb from cig id */
  if ((pCigCb = dmCisCigCbById((uint8_t) pMsg->param)) != NULL)
  {
    /* execute state machine */
    dmCisCigSmExecute(pCigCb, (dmCisMsg_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM CIS CIG HCI callback event handler.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmCisCigHciHandler(hciEvt_t *pEvent)
{
  dmCisCigCb_t *pCigCb = dmCisCigCbById((uint8_t) pEvent->hdr.param);

  /* translate HCI event to state machine event */
  if (pEvent->hdr.event == HCI_LE_SET_CIG_PARAMS_CMD_CMPL_CBACK_EVT)
  {
    if (pEvent->hdr.status == HCI_SUCCESS)
    {
      pEvent->hdr.event =  DM_CIS_MSG_HCI_LE_SET_CIG_PARAMS_CMD_CMPL;
    }
    else
    {
      pEvent->hdr.event = DM_CIS_MSG_HCI_LE_SET_CIG_PARAMS_CMD_CMPL_FAIL;
    }
  }
  else /* HCI_LE_REMOVE_CIG_CMD_CMPL_CBACK_EVT */
  {
    if (pEvent->hdr.status == HCI_SUCCESS)
    {
      pEvent->hdr.event =  DM_CIS_MSG_HCI_LE_REMOVE_CIG_CMD_CMPL;
    }
    else
    {
      pEvent->hdr.event = DM_CIS_MSG_HCI_LE_REMOVE_CIG_CMD_CMPL_FAIL;
    }
  }

  /* if cig cb found */
  if (pCigCb != NULL)
  {
    /* execute state machine */
    dmCisCigSmExecute(pCigCb, (dmCisMsg_t *) pEvent);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the parameters of one or more Connected Isochronous Streams (CISes) that are 
 *          associated with the given Connected Isochronous Group (CIG).
 *
 *  \param  cigId       CIG identifier.
 *  \param  numCis      Number of CIS to be configured.
 *  \param  pCisParam   CIS parameters.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmCisCigConfig(uint8_t cigId, dmConnId_t numCis, HciCisCisParams_t *pCisParam)
{
  dmCisCigCb_t        *pCigCb = NULL;
  dmCisCigApiConfig_t *pMsg;

  WSF_ASSERT(cigId < HCI_MAX_CIG_ID);
  WSF_ASSERT((numCis > 0 ) && (numCis <= DM_CIS_MAX));

  /* make sure Cig cb not already allocated */
  WsfTaskLock();
  if ((pCigCb = dmCisCigCbById(cigId)) == NULL)
  {
    /* allocate Cig cb */
    pCigCb = dmCisCigCbAlloc(cigId);
  }
  WsfTaskUnlock();

  if (pCigCb != NULL)
  {
    if ((pMsg = WsfMsgAlloc(sizeof(dmCisCigApiConfig_t) + (numCis * sizeof(HciCisCisParams_t)))) != NULL)
    {
      pMsg->hdr.event = DM_CIS_CIG_MSG_API_CONFIG;
      pMsg->hdr.param = cigId;
      pMsg->numCis = numCis;
      pMsg->pCisParam = (HciCisCisParams_t *) (pMsg + 1);

      memcpy(pMsg->pCisParam, pCisParam, (numCis * sizeof(HciCisCisParams_t)));

      WsfMsgSend(dmCb.handlerId, pMsg);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Remove all the Connected Isochronous Streams (CISes) associated with the given 
 *          Connected Isochronous Group (CIG).
 *
 *  \param  cigId       CIG identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmCisCigRemove(uint8_t cigId)
{
  wsfMsgHdr_t *pMsg;

  WSF_ASSERT(cigId < HCI_MAX_CIG_ID);

  if ((pMsg = WsfMsgAlloc(sizeof(wsfMsgHdr_t))) != NULL)
  {
    pMsg->event = DM_CIS_CIG_MSG_API_REMOVE;
    pMsg->param = cigId;

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Create one or more Connected Isochronous Streams (CISes) using the connections 
 *          identified by the ACL connection handles.
 *
 *  \param  numCis      Total number of CISes to be created.
 *  \param  pCisHandle  List of connection handles of CISes.
 *  \param  pConnId     List of DM connection identifiers.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmCisOpen(uint8_t numCis, uint16_t *pCisHandle, dmConnId_t *pConnId)
{
  dmCisApiOpen_t *pMsg;

  WSF_ASSERT((numCis > 0 ) && (numCis <= DM_CIS_MAX));

  if ((pMsg = WsfMsgAlloc(sizeof(dmCisApiOpen_t) + (numCis * (sizeof(uint16_t) + sizeof(dmConnId_t))))) != NULL)
  {
    pMsg->hdr.event = DM_CIS_MSG_API_OPEN;
    pMsg->hdr.param = pCisHandle[0];
    pMsg->numCis = numCis;

    pMsg->pCisHandle = (uint16_t *) (pMsg + 1);
    memcpy(pMsg->pCisHandle, pCisHandle, (numCis * sizeof(uint16_t)));

    pMsg->pConnId = (dmConnId_t *) (pMsg->pCisHandle + numCis);
    memcpy(pMsg->pConnId, pConnId, (numCis * sizeof(dmConnId_t)));

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the interval, in microseconds, of periodic SDUs for the given Connected Isochronous
 *          Group (CIG).
 *
 *  \param  cigId            CIG ID.
 *  \param  sduIntervalMToS  Time interval between start of consecutive SDUs from master Host.
 *  \param  sduIntervalSToM  Time interval between start of consecutive SDUs from slave Host.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmCisCigSetSduInterval(uint8_t cigId, uint32_t sduIntervalMToS, uint32_t sduIntervalSToM)
{
  dmCisCigCb_t  *pCigCb;

  WSF_ASSERT(cigId < HCI_MAX_CIG_ID);

  /* make sure Cig cb not already allocated */
  WsfTaskLock();
  if ((pCigCb = dmCisCigCbById(cigId)) == NULL)
  {
    /* allocate Cig cb */
    pCigCb = dmCisCigCbAlloc(cigId);
  }

  if (pCigCb != NULL)
  {
    pCigCb->sduIntervalMToS = sduIntervalMToS;
    pCigCb->sduIntervalSToM = sduIntervalSToM;
  }
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set the slaves clock accuracy for the given Connected Isochronous Group (CIG).
 *
 *  \param  cigId   CIG identifier.
 *  \param  sca     Slaves clck accuracy (0 if unknown).
 *
 *  \return None.
 *
 *  \note   The slaves clock accuracy must which must be the worst-case sleep clock accuracy of the
 *          slaves that will participate in the CIG.
 */
/*************************************************************************************************/
void DmCisCigSetSca(uint8_t cigId, uint8_t sca)
{
  dmCisCigCb_t  *pCigCb;

  WSF_ASSERT(cigId < HCI_MAX_CIG_ID);

  /* make sure Cig cb not already allocated */
  WsfTaskLock();
  if ((pCigCb = dmCisCigCbById(cigId)) == NULL)
  {
    /* allocate Cig cb */
    pCigCb = dmCisCigCbAlloc(cigId);
  }

  if (pCigCb != NULL)
  {
    pCigCb->sca = sca;
  }
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set the packing scheme and framing format for the given Connected Isochronous Group
 *          (CIG).
 *
 *  \param  cigId       CIG identifier.
 *  \param  packing     Packing scheme.
 *  \param  framing     Indicates format of CIS Data PDUs.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmCisCigSetPackingFraming(uint8_t cigId, uint8_t packing, uint8_t framing)
{
  dmCisCigCb_t  *pCigCb;

  WSF_ASSERT(cigId < HCI_MAX_CIG_ID);

  /* make sure Cig cb not already allocated */
  WsfTaskLock();
  if ((pCigCb = dmCisCigCbById(cigId)) == NULL)
  {
    /* allocate Cig cb */
    pCigCb = dmCisCigCbAlloc(cigId);
  }

  if (pCigCb != NULL)
  {
    pCigCb->packing = packing;
    pCigCb->framing = framing;
  }
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set the maximum transport latency, in microseconds, for the given Connected Isochronous
 *          Group (CIG).
 *
 *  \param  cigId        CIG identifier.
 *  \param  transLatMToS Maximum time for SDU to be transported from master Controller to slave Controller.
 *  \param  transLatSToM Maximum time for SDU to be transported from slave Controller to master Controller.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmCisCigSetTransLatInterval(uint8_t cigId, uint16_t transLatMToS, uint16_t transLatSToM)
{
  dmCisCigCb_t  *pCigCb;

  WSF_ASSERT(cigId < HCI_MAX_CIG_ID);

  /* make sure Cig cb not already allocated */
  WsfTaskLock();
  if ((pCigCb = dmCisCigCbById(cigId)) == NULL)
  {
    /* allocate Cig cb */
    pCigCb = dmCisCigCbAlloc(cigId);
  }

  if (pCigCb != NULL)
  {
    pCigCb->transLatMToS = transLatMToS;
    pCigCb->transLatSToM = transLatSToM;
  }
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize DM Connected Isochronous Stream (CIS) manager for operation as master.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmCisMasterInit(void)
{
  WsfTaskLock();

  dmFcnIfTbl[DM_ID_CIS_CIG] = (dmFcnIf_t *) &dmCisCigFcnIf;
  dmCisActSet[DM_CIS_ACT_SET_MASTER] = (dmCisAct_t *) dmCisActSetMaster;

  HciSetLeSupFeat(HCI_LE_SUP_FEAT_CIS_MASTER, TRUE);

  WsfTaskUnlock();
}
