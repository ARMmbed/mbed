/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  DM Connected Isochronous Stream (CIS) management module.
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
#include "util/bstream.h"
#include "dm_api.h"
#include "dm_dev.h"
#include "dm_main.h"
#include "dm_conn.h"
#include "dm_cis.h"


/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void dmCisReset(void);
static void dmCisMsgHandler(wsfMsgHdr_t *pMsg);
static void dmCisHciHandler(hciEvt_t *pEvent);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! Action set for CIS module */
static const dmCisAct_t dmCisActSetMain[] =
{
  dmCisSmActNone,
  dmCisSmActClose,
  dmCisSmActCisEst,
  dmCisSmActCisEstFailed,
  dmCisSmActCisClosed,
};

/* CIS component function interface */
static const dmFcnIf_t dmCisFcnIf =
{
  dmCisReset,
  dmCisHciHandler,
  dmCisMsgHandler
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Control block */
dmCisCb_t dmCisCb;

/*************************************************************************************************/
/*!
 *  \brief  Allocate a DM CIG control block.
 *
 *  \param  cigId   CIG identifier.
 *
 *  \return Pointer to CIG CB or NULL if failure.
 */
/*************************************************************************************************/
dmCisCigCb_t *dmCisCigCbAlloc(uint8_t cigId)
{
  dmCisCigCb_t *pCigCb = dmCisCb.cisCigCb;
  uint8_t       i;

  WSF_ASSERT(cigId < HCI_MAX_CIG_ID);

  for (i = 0; i < DM_CIG_MAX; i++, pCigCb++)
  {
    if (pCigCb->inUse == FALSE)
    {
      memset(pCigCb, 0, sizeof(dmCisCigCb_t));

      pCigCb->cigId = cigId;

      /* set Cig default values */
      pCigCb->packing = HCI_PACKING_SEQUENTIAL;
      pCigCb->framing = HCI_FRAMING_UNFRAMED;
      pCigCb->sca = HCI_MIN_SCA;
      pCigCb->sduIntervalMToS = pCigCb->sduIntervalSToM = HCI_DEFAULT_SDU_INTERV;
      pCigCb->transLatMToS = pCigCb->transLatSToM = HCI_DEFAULT_CIS_TRANS_LAT;
      pCigCb->inUse = TRUE;

      DM_TRACE_ALLOC1("dmCisCigCbAlloc cigId:%d", cigId);

      return pCigCb;
    }
  }

  DM_TRACE_ERR0("dmCisCigCbAlloc failed");

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a DM CIS connection control block.
 *
 *  \param  cigId   Identfier of CIG that CIS belongs to.
 *  \param  cisId   CIS identfier.
 *  \param  role    Device role.
 *
 *  \return Pointer to CIS CCB or NULL if failure.
 */
/*************************************************************************************************/
dmCisCcb_t *dmCisCcbAlloc(uint8_t cigId, uint8_t cisId, uint8_t role)
{
  dmCisCcb_t   *pCcb = dmCisCb.cisCcb;
  uint8_t       i;

  WSF_ASSERT(cigId < HCI_MAX_CIG_ID);
  WSF_ASSERT(cisId < HCI_MAX_CIS_ID);

  for (i = 0; i < DM_CIS_MAX; i++, pCcb++)
  {
    if (pCcb->inUse == FALSE)
    {
      memset(pCcb, 0, sizeof(dmCisCcb_t));

      pCcb->cigId = cigId;
      pCcb->cisId = cisId;
      pCcb->role = role;
      pCcb->aclHandle = DM_CONN_HCI_HANDLE_NONE;
      pCcb->cisHandle = DM_CONN_HCI_HANDLE_NONE;
      pCcb->inUse = TRUE;

      DM_TRACE_ALLOC2("dmCisCcbAlloc cigId:%d cisId:%d", cigId, cisId);

      return pCcb;
    }
  }

  DM_TRACE_ERR0("dmCisCcbAlloc failed");

  return NULL;
}

/*************************************************************************************************/
/*!
*  \brief  Deallocate a DM CIG control block.
*
*  \param  pCigCb   CIG control block.
*
*  \return None.
*/
/*************************************************************************************************/
void dmCisCigCbDealloc(dmCisCigCb_t *pCigCb)
{
  DM_TRACE_FREE1("dmCigCbDealloc %d", pCigCb->cigId);

  pCigCb->inUse = FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Deallocate a DM CIS connection control block.
 *
 *  \param  pCcb    CIS connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmCisCcbDealloc(dmCisCcb_t *pCcb)
{
  DM_TRACE_FREE2("dmCisCcbDealloc cigId:%d cisId:%d", pCcb->cigId, pCcb->cisId);

  pCcb->inUse = FALSE;
}

/*************************************************************************************************/
/*!
*  \brief  Deallocate all CIS connection control blocks associated with the given CIG ID.
*
*  \param  cigId    CIG identifer.
*
*  \return None.
*/
/*************************************************************************************************/
void dmCisCcbDeallocByCigId(uint8_t cigId)
{
  dmCisCcb_t   *pCcb = dmCisCb.cisCcb;
  uint8_t       i;

  for (i = DM_CIS_MAX; i > 0; i--, pCcb++)
  {
    if (pCcb->inUse && (pCcb->cigId == cigId))
    {
      dmCisCcbDealloc(pCcb);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Find a CIS connection control block with matching handle.
 *
 *  \param  handle  CIS connection handle.
 *
 *  \return Pointer to CIS connection control block. NULL if not found.
 */
/*************************************************************************************************/
dmCisCcb_t *dmCisCcbByHandle(uint16_t handle)
{
  dmCisCcb_t   *pCcb = dmCisCb.cisCcb;
  uint8_t       i;

  for (i = DM_CIS_MAX; i > 0; i--, pCcb++)
  {
    if (pCcb->inUse && (pCcb->cisHandle == handle))
    {
      return pCcb;
    }
  }

  DM_TRACE_INFO1("dmCisCcbByHandle not found %d", handle);

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the CIS connection control block for the given CIG/CIS IDs.
 *
 *  \param  cigId   CIG identifer.
 *  \param  cisId   CIS identifer.
 *
 *  \return Pointer to CIS connection control block. NULL if not found.
 */
/*************************************************************************************************/
dmCisCcb_t *dmCisCcbById(uint8_t cigId, uint8_t cisId)
{
  dmCisCcb_t   *pCcb = dmCisCb.cisCcb;
  uint8_t       i;

  for (i = DM_CIS_MAX; i > 0; i--, pCcb++)
  {
    if (pCcb->inUse && (pCcb->cigId == cigId) && (pCcb->cisId == cisId))
    {
      return pCcb;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
*  \brief  Return the CIG control block for the given CIG ID.
*
*  \param  cigId    CIG identifer.
*
*  \return Pointer to CIG control block. NULL if not found.
*/
/*************************************************************************************************/
dmCisCigCb_t *dmCisCigCbById(uint8_t cigId)
{
  dmCisCigCb_t *pCigCb = dmCisCb.cisCigCb;
  uint8_t       i;

  for (i = DM_CIG_MAX; i > 0; i--, pCigCb++)
  {
    if (pCigCb->inUse && (pCigCb->cigId == cigId))
    {
      return pCigCb;
    }
  }

  return NULL;
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
void dmCisSmActNone(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Close a CIS connection.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmCisSmActClose(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg)
{
  /* close CIS connection */
  HciDisconnectCmd(pCcb->cisHandle, pMsg->apiClose.reason);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a CIS established event from HCI.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmCisSmActCisEst(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg)
{
  pMsg->hdr.event = DM_CIS_OPEN_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a CIS establish failure event from HCI.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmCisSmActCisEstFailed(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg)
{
  pMsg->hdr.event = DM_CIS_CLOSE_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a disconnect complete event from HCI.
 *
 *  \param  pCcb      CIS connection control block.
 *  \param  pMsg      WSF message.
 *  \param  oldState  Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmCisSmActCisClosed(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg)
{
  pMsg->hdr.event = DM_CIS_CLOSE_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}


/*************************************************************************************************/
/*!
 *  \brief  Handle a CIS connection close event.
 *
 *  \param  aclHandle   ACL connection handle.
 *  \param  reason      Reason.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisHandleConnClose(uint16_t aclHandle, uint8_t reason)
{
  dmCisCcb_t              *pCcb = dmCisCb.cisCcb;
  hciDisconnectCmplEvt_t  disconnectCmpl;

  /* generate HCI CIS disconnect complete event */
  disconnectCmpl.hdr.event = HCI_CIS_DISCONNECT_CMPL_CBACK_EVT;
  disconnectCmpl.hdr.status = disconnectCmpl.status = HCI_SUCCESS;
  disconnectCmpl.reason = reason;

  for (uint8_t i = DM_CIS_MAX; i > 0; i--, pCcb++)
  {
    if (!pCcb->inUse)
    {
      continue;
    }
    
    if ((pCcb->aclHandle == aclHandle) || (aclHandle == DM_CONN_HCI_HANDLE_NONE))
    {
      /* set Cis Id */
      disconnectCmpl.hdr.param = disconnectCmpl.handle = pCcb->cisHandle;

      /* handle the event */
      dmCisHciHandler((hciEvt_t *) &disconnectCmpl);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Reset the CIS module.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisReset(void)
{
  dmCisCigCb_t                *pCigCb = dmCisCb.cisCigCb;
  hciLeRemoveCigCmdCmplEvt_t  removeCigCmdCmpl;

  dmCisHandleConnClose(DM_CONN_HCI_HANDLE_NONE, HCI_ERR_LOCAL_TERMINATED);

  /* generate HCI remove CIG command complete event */
  removeCigCmdCmpl.hdr.event = HCI_LE_REMOVE_CIG_CMD_CMPL_CBACK_EVT;
  removeCigCmdCmpl.hdr.status = removeCigCmdCmpl.status = HCI_SUCCESS;

  for (uint8_t i = DM_CIG_MAX; i > 0; i--, pCigCb++)
  {
    if (pCigCb->inUse)
    {
      /* set Cig Id */
      removeCigCmdCmpl.hdr.param = removeCigCmdCmpl.cigId = pCigCb->cigId;

      /* handle the event */
      dmCisCigHciHandler((hciEvt_t *) &removeCigCmdCmpl);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM CIS WSF message handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisMsgHandler(wsfMsgHdr_t *pMsg)
{
  dmCisCcb_t *pCcb;

  /* look up ccb from cis handle */
  if ((pCcb = dmCisCcbByHandle(pMsg->param)) != NULL)
  {
    /* execute state machine */
    dmCisSmExecute(pCcb, (dmCisMsg_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM Conn HCI callback event handler.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmCisHciHandler(hciEvt_t *pEvent)
{
  dmCisCcb_t *pCcb = dmCisCcbByHandle(pEvent->hdr.param);

  /* handle special cases for CIS request event */
  if (pEvent->hdr.event == HCI_LE_CIS_REQ_CBACK_EVT)
  {
    /* first check if ccb exists for this handle */
    if (pCcb == NULL)
    {
      dmConnId_t  connId;

      if ((connId = DmConnIdByHandle(pEvent->leCisReq.aclHandle)) != DM_CONN_ID_NONE)
      {
        /* if slave, allocate new ccb */
        if (DmConnRole(connId) == DM_ROLE_SLAVE)
        {
          pCcb = dmCisCcbAlloc(pEvent->leCisReq.cigId, pEvent->leCisReq.cisId, DM_ROLE_SLAVE);
        }
      }
    }

    /* translate HCI event to state machine event */
    pEvent->hdr.event = DM_CIS_MSG_HCI_LE_CIS_REQ;
  }
  else if (pEvent->hdr.event == HCI_LE_CIS_EST_CBACK_EVT)
  {
    /* translate HCI event to state machine event */
    if (pEvent->hdr.status == HCI_SUCCESS)
    {
      pEvent->hdr.event =  DM_CIS_MSG_HCI_LE_CIS_EST;
    }
    else
    {
      pEvent->hdr.event = DM_CIS_MSG_HCI_LE_CIS_EST_FAIL;
    }
  }
  else /* HCI_CIS_DISCONNECT_CMPL_CBACK_EVT */
  {
    /* translate HCI event to state machine event */
    pEvent->hdr.event = DM_CIS_MSG_HCI_DISCONNECT_CMPL;
  }

  /* if ccb found */
  if (pCcb != NULL)
  {
    /* execute state machine */
    dmCisSmExecute(pCcb, (dmCisMsg_t *) pEvent);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize DM Connected Isochronous Stream (CIS) manager.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmCisInit(void)
{
  WsfTaskLock();

  dmFcnIfTbl[DM_ID_CIS] = (dmFcnIf_t *) &dmCisFcnIf;
  dmCisActSet[DM_CIS_ACT_SET_MAIN] = (dmCisAct_t *) dmCisActSetMain;

  HciSetLeSupFeat(HCI_LE_SUP_FEAT_ISO_HOST_SUPPORT, TRUE);

  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Close the Connected Isochronous Stream (CIS) connection with the given handle.
 *
 *  \param  handle    CIS connection handle.
 *  \param  reason    Reason connection is being closed.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmCisClose(uint16_t handle, uint8_t reason)
{
  dmCisApiClose_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmCisApiClose_t))) != NULL)
  {
    pMsg->hdr.event = DM_CIS_MSG_API_CLOSE;
    pMsg->hdr.param = handle;
    pMsg->hdr.status = pMsg->reason = reason;

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  For internal use only.  Find the Connected Isochronous Stream (CIS) ID with matching 
 *          handle.
 *
 *  \param  handle  CIS connection handle.
 *
 *  \return CIS identifier or DM_CIS_ID_NONE if error.
 */
/*************************************************************************************************/
uint8_t DmCisIdByHandle(uint16_t handle)
{
  dmCisCcb_t  *pCcb;

  if ((pCcb = dmCisCcbByHandle(handle)) != NULL)
  {
    return pCcb->cisId;
  }

  return DM_CIS_ID_NONE;
}

/*************************************************************************************************/
/*!
 *  \brief  For internal use only.  Find the Connected Isochronous Stream (CIS) handle with matching 
 *          CIG and CIS identifiers.
 *
 *  \param  handle  CIG ID.
 *  \param  handle  CIS ID.
 *
 *  \return CIS connection handle or DM_CONN_HCI_HANDLE_NONE if error.
 */
/*************************************************************************************************/
uint16_t DmCisHandleById(uint8_t cigId, uint8_t cisId)
{
  dmCisCcb_t  *pCcb;

  if ((pCcb = dmCisCcbById(cigId, cisId)) != NULL)
  {
    return pCcb->cisHandle;
  }

  return DM_CONN_HCI_HANDLE_NONE;
}

/*************************************************************************************************/
/*!
 *  \brief  For internal use only.  Return TRUE if the Connected Isochronous Stream (CIS) 
 *          connection is in use.
 *
 *  \param  handle  CIS connection handle.
 *
 *  \return TRUE if the CIS connection is in use, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t DmCisConnInUse(uint16_t handle)
{
  dmCisCcb_t  *pCcb;

  if ((pCcb = dmCisCcbByHandle(handle)) != NULL)
  {
    return pCcb->inUse;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  For internal use only.  Return the CIS connection role indicating master or slave.
 *
 *  \param  handle  CIS connection handle.
 *
 *  \return CIS connection role.
 */
/*************************************************************************************************/
uint8_t DmCisConnRole(uint16_t handle)
{
  dmCisCcb_t  *pCcb;

  if ((pCcb = dmCisCcbByHandle(handle)) != NULL)
  {
    return pCcb->role;
  }

  return DM_ROLE_SLAVE;
}

/*************************************************************************************************/
/*!
 *  \brief  For internal use only.  Return TRUE if Connected Isochronous Group (CIG) is in use.
 *
 *  \param  cigId   CIG identifier.
 *
 *  \return TRUE if CIG is in use, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t DmCisCigInUse(uint8_t cigId)
{
  if (dmCisCigCbById(cigId) != NULL)
  {
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
  *  \brief  For internal use only.  Return TRUE if the Connected Isochronous Stream (CIS)
  *          connection is in use.
  *
  *  \param  cigId   CIG identifier.
  *  \param  cisId   CIS identifier.
  *
  *  \return TRUE if the CIS connection is in use, FALSE otherwise.
  */
/*************************************************************************************************/
bool_t DmCisInUse(uint8_t cigId, uint8_t cisId)
{
  if (dmCisCcbById(cigId, cisId) != NULL)
  {
    return TRUE;
  }

  return FALSE;
}
