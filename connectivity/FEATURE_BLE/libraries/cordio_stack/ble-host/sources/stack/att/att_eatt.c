/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Enhanced ATT (EATT) main module.
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
#include "wsf_timer.h"
#include "wsf_msg.h"
#include "wsf_math.h"
#include "wsf_os.h"
#include "util/bstream.h"
#include "att_api.h"
#include "eatt_api.h"
#include "att_main.h"
#include "att_eatt.h"
#include "l2c_api.h"
#include "l2c_main.h"
#include "dm_api.h"
#include "cfg_stack.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* EATT PSM identifier */
#define EATT_PSM                        0x0027

/* Initial number of credits on EATT L2CAP COC channels */
#define EATT_INIT_CREDITS               1

/* EATT Connect state machine states */
enum {
  EATT_CONN_STATE_IDLE = 0,             /* Idle state */
  EATT_CONN_STATE_INITIATING,           /* Initiating connections state */
  EATT_CONN_STATE_RECONFIG,             /* Reconfiguring MTU/MPS state */
  EATT_CONN_STATE_ACCEPTING             /* Accepting connections state */
};

/* EATT Connect state machine events */
#define EATT_BACKOFF_EVT                EATT_MSG_START

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

eattCb_t eattCb;

/* Channel control block list. */
#if EATT_CONN_CHAN_MAX > 0
eattChanCb_t eattChanCb[DM_CONN_MAX][EATT_CONN_CHAN_MAX];
#endif

/* Default component function inteface */
const eattFcnIf_t eattFcnDefault =
{
  attEmptyL2cCocDataCback,
  attEmptyL2cCocCback,
  (attMsgHandler_t) attEmptyHandler,
  attEmptyConnCback
};

/**************************************************************************************************
  External Variables
**************************************************************************************************/

/* EATT application configuration */
extern eattCfg_t *pEattCfg;

/* ATT control block */
extern attCb_t attCb;

/*************************************************************************************************/
/*!
 *  \brief  Calculate a random backoff period for establishing enhanced channels.
 *
 *  \return Backoff period.
 */
/*************************************************************************************************/
static wsfTimerTicks_t eattBackoffPeriod(uint16_t connInterval)
{
  uint16_t randMs;
  uint16_t minInterval = (uint16_t) (2 * 1.25 * connInterval);

  /* Get a random value */
  SecRand((uint8_t*) &randMs, sizeof(randMs));

  /* Limit random value to 1024ms */
  randMs &= 0x3FF;

  /* Ensure the random time is greater than 2x the connection interval */
  randMs = (randMs > minInterval) ? randMs : randMs + minInterval;

  return randMs;
}

/*************************************************************************************************/
/*!
 *  \brief  Get an EATT connection control block.
 *
 *  \param  connId  DM connection identifier.
 *
 *  \return Pointer to control block.
 */
/*************************************************************************************************/
eattConnCb_t *eattGetConnCb(dmConnId_t connId)
{
  WSF_ASSERT((connId > DM_CONN_ID_NONE) && (connId <= DM_CONN_MAX));

  return &eattCb.ccb[connId - 1];
}

/*************************************************************************************************/
/*!
 *  \brief  Get an EATT bearer control block given a bearer slot identifier.
 *
 *  \param  connId  DM connection identifier.
 *  \param  slot    Bearer slot identifier.
 *
 *  \return Pointer to control block.
 */
/*************************************************************************************************/
eattChanCb_t *eattGetChanCbBySlot(dmConnId_t connId, uint8_t slot)
{
  WSF_ASSERT((connId > DM_CONN_ID_NONE) && (connId <= DM_CONN_MAX));
  WSF_ASSERT((slot > 0) && (slot <= EATT_CONN_CHAN_MAX));

  return &eattCb.ccb[connId - 1].pChanCb[slot - 1];
}

/*************************************************************************************************/
/*!
 *  \brief  Get an EATT bearer control block given an L2CAP channel identifier.
 *
 *  \param  connId  DM connection identifier.
 *  \param  cid     L2CAP channel identifier.
 *
 *  \return Pointer to control block.
 */
/*************************************************************************************************/
static eattChanCb_t *eattGetChanCbByCid(dmConnId_t connId, uint16_t cid)
{
  eattConnCb_t *pConnCb = eattGetConnCb(connId);
  uint8_t      i;

  if (pConnCb)
  {
    for (i = 0; i < EATT_CONN_CHAN_MAX; i++)
    {
      if (pConnCb->pChanCb[i].cid == cid)
      {
        return &pConnCb->pChanCb[i];
      }
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Returns an EATT slot identifier given a connection ID and L2CAP channel identifier.
 *
 *  \param  connId  DM connection identifier.
 *  \param  cid     L2CAP channel identifier.
 *
 *  \return slot identifier.
 */
/*************************************************************************************************/
uint8_t eattGetSlotId(dmConnId_t connId, uint16_t cid)
{
  eattConnCb_t *pCcb = eattGetConnCb(connId);
  uint8_t      i;

  if (cid == L2C_CID_ATT)
  {
    return ATT_BEARER_SLOT_ID;
  }

  if (pCcb)
  {
    for (i = 0; i < EATT_CONN_CHAN_MAX; i++)
    {
      if (pCcb->pChanCb[i].cid == cid)
      {
        return i + 1;
      }
    }
  }

  return ATT_BEARER_SLOT_INVALID;
}

/*************************************************************************************************/
/*!
 *  \brief  Finds an unused slot on a given connection.
 *
 *  \param  connId  DM connection identifier.
 *
 *  \return EATT slot identifier or ATT_BEARER_SLOT_INVALID.
 */
/*************************************************************************************************/
static uint8_t eattGetUnusedSlot(dmConnId_t connId)
{
  eattConnCb_t *pCcb = eattGetConnCb(connId);
  uint8_t      i;

  if (pCcb)
  {
    for (i = 0; i < EATT_CONN_CHAN_MAX; i++)
    {
      if (pCcb->pChanCb[i].inUse == FALSE)
      {
        return i + 1;
      }
    }
  }

  return ATT_BEARER_SLOT_INVALID;
}

/*************************************************************************************************/
/*!
 *  \brief  Update the MTU in the ATT control block.
 *
 *  \param  connId  DM connection identifier.
 *  \param  slot    EATT slot identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattUpdateMtu(dmConnId_t connId, uint8_t slot)
{
  eattChanCb_t *pChanCb = eattGetChanCbBySlot(connId, slot);

  if (pChanCb)
  {
    /* The peer MTU must be greater than or equal to ATT_DEFAULT_MTU.  */
    uint16_t peerMtu = WSF_MAX(ATT_DEFAULT_MTU, pChanCb->peerMtu);

    /* Set the ATT mtu to the minimum to the local and peer MTU */
    attCb.ccb[connId-1].sccb[slot].mtu = WSF_MIN(pChanCb->localMtu, peerMtu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  L2CAP channel accept callback.
 *
 *  \param  connId    DM connection identifier.
 *  \param  numChans  Number of channels requested by the L2CAP.
 *
 *  \return number of channels permitted by EATT.
 */
/*************************************************************************************************/
static uint8_t eattL2cCocAcceptCback(dmConnId_t connId, uint8_t numChans)
{
  eattConnCb_t *pCcb = eattGetConnCb(connId);

  if ((pCcb->state == EATT_CONN_STATE_INITIATING) || (pCcb->state == EATT_CONN_STATE_RECONFIG))
  {
    // Reject all requests while busy connecting and configuring channels
    return 0;
  }

  // Limit the number of channels to value define by application in pEattCfg->numChans 
  uint8_t maxChans = pEattCfg->numChans - EattGetNumChannelsInUse(connId);
  return (maxChans > numChans) ? maxChans : numChans;
}

/*************************************************************************************************/
/*!
 *  \brief  Reconfigure the next set of L2CAP COC channels.
 *
 *  \param  connId  DM connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattReconfigureNextChannels(dmConnId_t connId)
{
  eattConnCb_t *pConnCb = eattGetConnCb(connId);
  
  if (pConnCb)
  {
    uint16_t chanList[L2C_MAX_EN_CHAN];
    uint8_t  numChan = 0;
    uint8_t  i = 0;

    while ((i < EATT_CONN_CHAN_MAX) && (numChan < L2C_MAX_EN_CHAN))
    {
      eattChanCb_t *pChanCb = eattGetChanCbBySlot(connId, i + 1);

      if (pChanCb->inUse)
      {
        if ((pChanCb->localMtu <= pConnCb->pendingMtu) && (pChanCb->localMps <= pConnCb->pendingMps))
        {
          chanList[numChan++] = pChanCb->cid;
        }
      }

      i++;
    }

    if (numChan > 0)
    {
      bool_t success = L2cCocEnhancedReconfigReq(connId, pConnCb->pendingMtu, pConnCb->pendingMps,
                                                 numChan, chanList);

      if (!success)
      {
        /* Reconfigure failed, notify callback */
        pConnCb->state = EATT_CONN_STATE_IDLE;
        eattExecCallback(connId, ATT_EATT_RECONFIG_CMPL_IND, ATT_ERR_RESOURCES);
      }
    }
    else
    {
      /* Reconfigure complete. */
      pConnCb->state = EATT_CONN_STATE_IDLE;
      eattExecCallback(connId, ATT_EATT_RECONFIG_CMPL_IND, ATT_SUCCESS);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Request the next set of channels from the L2CAP.
 *
 *  \param  connId  DM connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattReqNextChannels(dmConnId_t connId)
{
  bool_t        success;
  eattConnCb_t  *pConnCb = eattGetConnCb(connId);
  uint8_t       numChans = pEattCfg->numChans - EattGetNumChannelsInUse(connId);
  
  numChans = (numChans > L2C_MAX_EN_CHAN) ? L2C_MAX_EN_CHAN : numChans;
  
  EATT_TRACE_INFO1("eattReqNextChannels: numChans: %d", numChans);

  if (numChans > 0)
  {
    /* Request L2CAP enhanced channels */
    success = L2cCocEnhancedConnectReq(connId, eattCb.cocRegId, EATT_PSM, EATT_INIT_CREDITS, numChans);

    if (!success)
    {
      /* Connect failed, notify callback */
      pConnCb->state = EATT_CONN_STATE_IDLE;
      eattExecCallback(connId, ATT_EATT_CONN_CMPL_IND, ATT_ERR_RESOURCES);
    }
  }
  else
  {
    /* No more channels to create. */
    pConnCb->state = EATT_CONN_STATE_IDLE;
    eattExecCallback(connId, ATT_EATT_CONN_CMPL_IND, ATT_SUCCESS);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process an L2CAP enhanced channel connect indication.
 *
 *  \param  pMsg    L2Cap connect message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattL2cEnChanInd(l2cCocEnConnectInd_t *pMsg)
{
  dmConnId_t    connId = (dmConnId_t) pMsg->hdr.param;
  eattConnCb_t  *pCcb = eattGetConnCb(connId);
  uint8_t       i;

  if (pCcb)
  {
    if (pMsg->req && (pCcb->state == EATT_CONN_STATE_INITIATING))
    {
      /* Shouldn't happen */
      WSF_ASSERT(0);
    }

    /* Check status */
    if ((pMsg->hdr.status == L2C_CONN_SUCCESS) || (pMsg->hdr.status == L2C_CONN_FAIL_RES) ||
        (pMsg->hdr.status == L2C_CONN_FAIL_INVALID_SCID) || (pMsg->hdr.status == L2C_CONN_FAIL_ALLOCATED_SCID))
    {
      /* Store connection identifiers */
      for (i = 0; i < L2C_MAX_EN_CHAN; i++)
      {
        if (pMsg->cidList[i])
        {
          uint8_t slot = eattGetUnusedSlot(connId);

          if (slot != ATT_BEARER_SLOT_INVALID)
          {
            eattChanCb_t *pChanCb = eattGetChanCbBySlot(connId, slot);

            if (pChanCb)
            {
              pChanCb->cid = pMsg->cidList[i];
              pChanCb->peerMtu = pMsg->mtu;
              pChanCb->localMtu = pEattCfg->mtu;

              if (pEattCfg->pPriorityTbl)
              {
                pChanCb->priority = pEattCfg->pPriorityTbl[slot - 1];
              }
              else
              {
                pChanCb->priority = slot;
              }

              pChanCb->inUse = TRUE;

              eattUpdateMtu(connId, slot);
            }
            else
            {
              /* Shouldn't happen */
              WSF_ASSERT(0);
            }
          }
        }
      }

      /* When initiating, check if more channels need to be created */
      if (pCcb->state == EATT_CONN_STATE_INITIATING)
      {
        if (pMsg->hdr.status != L2C_CONN_FAIL_RES)
        {
          eattReqNextChannels((dmConnId_t)pMsg->hdr.param);
        }
        else
        {
          /* Peer will not accept any more channels */
          pCcb->state = EATT_CONN_STATE_IDLE;
        }
      }
      else
      {
        if (EattGetNumChannelsInUse(connId) == pEattCfg->numChans)
        {
          /* Max channels reached. */
          pCcb->state = EATT_CONN_STATE_IDLE;
          WsfTimerStop(&pCcb->backoffTimer);
        }
      }
    }
    else
    {
      /* Peer cannot accept more channels. */
      pCcb->state = EATT_CONN_STATE_IDLE;
      eattExecCallback(connId, ATT_EATT_CONN_CMPL_IND, ATT_SUCCESS);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process an L2CAP enhanced channel reconfigure indication.
 *
 *  \param  pMsg    L2Cap reconfigure message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattL2cEnReconfigInd(l2cCocEnConnectInd_t *pMsg)
{
  dmConnId_t    connId = (dmConnId_t) pMsg->hdr.param;
  eattConnCb_t  *pConnCb = eattGetConnCb(connId);
  uint8_t       i;

  if (pConnCb)
  {
    for (i = 0; i < L2C_MAX_EN_CHAN; i++)
    {
      if (pMsg->cidList[i])
      {
        eattChanCb_t *pChanCb = eattGetChanCbByCid(connId, pMsg->cidList[i]);

        if (pChanCb)
        {
          if (pMsg->req)
          {
            /* The peer changed their MTU */
            pChanCb->peerMtu = pMsg->mtu;
          }
          else
          {
            pChanCb->localMtu = pConnCb->pendingMtu;
            pChanCb->localMps = pConnCb->pendingMps;
          }

          eattUpdateMtu(connId, i + 1);
        }
      }
    }

    if (!pMsg->req)
    {
      /* If this was a response, send the next reconfiguration */
      eattReconfigureNextChannels(connId);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process an L2CAP data indication.
 *
 *  \param  pMsg    L2Cap indication message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattL2cDataInd(l2cCocDataInd_t *pMsg)
{
  /* parse PDU type */
  uint8_t pduType = *pMsg->pData;

  /* if from server */
  if ((pduType & ATT_PDU_MASK_SERVER) != 0)
  {
    /* call client data callback */
    (*attCb.pEnClient->l2cCocData)((l2cCocEvt_t*) pMsg);
  }
  /* else from client */
  else
  {
    /* call server data callback */
    (*attCb.pEnServer->l2cCocData)((l2cCocEvt_t*) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process an L2CAP data confirmation.
 *
 *  \param  pMsg    L2Cap confirmation message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattL2cDataCnf(l2cCocDataCnf_t *pMsg)
{
  attCcb_t *pCcb = attCcbByConnId((dmConnId_t) pMsg->hdr.param);

  /* verify connection is open */
  if (pCcb && pCcb->connId != DM_CONN_ID_NONE)
  {
    uint8_t slot = eattGetSlotId((dmConnId_t) pMsg->hdr.param, pMsg->cid);

    if (slot != ATT_BEARER_SLOT_INVALID)
    {
      EATT_TRACE_INFO2("eattL2cDataCnf: slot: %d, status %d", slot, pMsg->hdr.status);

      if (pMsg->hdr.status == L2C_COC_DATA_ERR_OVERFLOW)
      {
        for (uint8_t i = 0; i < EATT_CONN_CHAN_MAX; i++)
        {
          /* flow disabled */
          pCcb->sccb[i].control |= ATT_CCB_STATUS_FLOW_DISABLED;
        }
      }
      else
      {
        for (uint8_t i = 0; i < EATT_CONN_CHAN_MAX; i++)
        {
          /* flow enabled */
          pCcb->sccb[i].control &= ~ATT_CCB_STATUS_FLOW_DISABLED;
        }

        /* call server control callback */
        (*attCb.pEnServer->l2cCocCnf)((l2cCocEvt_t*) pMsg);

        /* check flow again; could be changed recursively */
        if (!(pCcb->sccb[slot].control & ATT_CCB_STATUS_FLOW_DISABLED))
        {
          /* call client control callback */
          (*attCb.pEnClient->l2cCocCnf)((l2cCocEvt_t*) pMsg);
        }
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process an L2CAP disconnect indication.
 *
 *  \param  pMsg    L2Cap confirmation message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattL2cDisconnectInd(l2cCocDisconnectInd_t *pMsg)
{
  eattChanCb_t *pCcb = eattGetChanCbByCid((dmConnId_t) pMsg->hdr.param, pMsg->cid);

  if (pCcb)
  {
    pCcb->inUse = FALSE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  The L2CAP CoC callback function.
 *
 *  \param  pMsg    Pointer to message structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattL2cCocCback(l2cCocEvt_t *pMsg)
{
  switch (pMsg->hdr.event)
  {
  case L2C_COC_EN_CONNECT_IND:
    eattL2cEnChanInd(&pMsg->enConnectInd);
    break;

  case L2C_COC_DISCONNECT_IND:
    eattL2cDisconnectInd(&pMsg->disconnectInd);
    break;

  case L2C_COC_EN_RECONFIG_IND:
    eattL2cEnReconfigInd(&pMsg->enConnectInd);
    break;

  case L2C_COC_DATA_IND:
    eattL2cDataInd(&pMsg->dataInd);
    break;

  case L2C_COC_DATA_CNF:
    eattL2cDataCnf(&pMsg->dataCnf);
    break;

  default:
    break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM connection callback for EATT.
 *
 *  \param  pDmEvt  DM callback event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void eattOnConnOpen(dmEvt_t *pDmEvt)
{
  eattConnCb_t *pCcb = eattGetConnCb((dmConnId_t) pDmEvt->hdr.param);

  if (pCcb)
  {
    /* Initialize channel control block. */
    memset(&pCcb->backoffTimer, 0, sizeof(wsfTimer_t));
    pCcb->state = EATT_CONN_STATE_IDLE;
    pCcb->connInterval = pDmEvt->connOpen.connInterval;
    pCcb->pendingMtu = 0;
    pCcb->pendingMps = 0;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Begin requesting EATT L2CAP coc channels.
 *
 *  \param  connId  DM connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattEstablishChannels(dmConnId_t connId)
{
  if (DmConnInUse(connId))
  {
    eattConnCb_t *pCcb = eattGetConnCb(connId);

    EATT_TRACE_INFO1("EattEstablishChannels: connId: 0x%x", connId);

    if (pCcb)
    {
      /* Request first set of channels. */
      if (DmConnRole(connId) == DM_ROLE_MASTER)
      {
        pCcb->state = EATT_CONN_STATE_INITIATING;
        eattReqNextChannels(connId);
      }
      else
      {
        /* Set a timer to initiate creation of channels */
        pCcb->state = EATT_CONN_STATE_ACCEPTING;

        pCcb->backoffTimer.msg.event = EATT_BACKOFF_EVT;
        pCcb->backoffTimer.msg.param = connId;
        pCcb->backoffTimer.handlerId = attCb.handlerId;
        WsfTimerStartMs(&pCcb->backoffTimer, eattBackoffPeriod(pCcb->connInterval));
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Returns the number of open EATT channels on a given connection.
 *
 *  \param  connId  DM connection identifier.
 *
 *  \return Number of open EATT channels.
 */
/*************************************************************************************************/
uint8_t EattGetNumChannelsInUse(dmConnId_t connId)
{
  eattConnCb_t *pCcb = eattGetConnCb(connId);
  uint8_t      count = 0;
  uint8_t      i;

  if (pCcb)
  {
    for (i = 0; i < EATT_CONN_CHAN_MAX; i++)
    {
      if (pCcb->pChanCb[i].inUse == TRUE)
      {
        count++;
      }
    }
  }

  return count;
}

/*************************************************************************************************/
/*!
 *  \brief  DM connection callback for EATT.
 *
 *  \param  pDmEvt  DM callback event.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattDmCback(dmEvt_t *pDmEvt)
{
  dmConnId_t   connId = (dmConnId_t) pDmEvt->hdr.param;
  eattConnCb_t *pCcb = eattGetConnCb(connId);

  switch (pDmEvt->hdr.event)
  {
  case DM_CONN_OPEN_IND:
    eattOnConnOpen(pDmEvt);

    if (pEattCfg->initiateEatt)
    {
      EattEstablishChannels(connId);
    }
    break;

  case DM_CONN_CLOSE_IND:
    WsfTimerStop(&pCcb->backoffTimer);
    break;

  case DM_CONN_UPDATE_IND:
    if (pCcb)
    {
      pCcb->connInterval =  pDmEvt->connUpdate.connInterval;
    }
    break;

  default:
    break;
  }

}

/*************************************************************************************************/
/*!
 *  \brief  Get L2CAP CID given DM channel ID and EATT slot.
 *
 *  \param  connId  DM channel ID.
 *  \param  slot    EATT slot.
 *
 *  \return None
 */
/*************************************************************************************************/
uint16_t eattGetCid(dmConnId_t connId, uint8_t slot)
{
  if (slot == ATT_BEARER_SLOT_ID)
  {
    return L2C_CID_ATT;
  }
  else
  {
    eattConnCb_t *pCcb = eattGetConnCb(connId);
    return pCcb->pChanCb[slot-1].cid;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Execute application callback function.
 *
 *  \param  connId  DM connection ID.
 *  \param  event   Callback event ID.
 *  \param  status  Status of event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void eattExecCallback(dmConnId_t connId, uint8_t event, uint8_t status)
{
  if (attCb.cback)
  {
    attEvt_t evt;

    memset(&evt, 0, sizeof(evt));
    evt.hdr.param = connId;
    evt.hdr.event = event;
    evt.hdr.status = status;

    (*attCb.cback)(&evt);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  WSF event handler for EATT.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void eattHandler(wsfMsgHdr_t *pMsg)
{
  if (pMsg->event == EATT_BACKOFF_EVT)
  {
    dmConnId_t connId = (dmConnId_t) pMsg->param;
    eattConnCb_t *pCcb = eattGetConnCb(connId);

    if (pCcb)
    {
      pCcb->state = EATT_CONN_STATE_INITIATING;
      eattReqNextChannels(connId);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Reconfigure the connection's EATT mtu and mps.
 *
 *  \param  connId  DM channel ID.
 *  \param  mtu     New MTU.
 *  \param  mps     New MPS.
 *
 *  \return None
 */
/*************************************************************************************************/
void EattReconfigureChannels(dmConnId_t connId, uint16_t mtu, uint16_t mps)
{
  eattConnCb_t *pConnCb = eattGetConnCb(connId);
  
  if (pConnCb && (pConnCb->state == EATT_CONN_STATE_IDLE))
  {
    pConnCb->pendingMtu = mtu;
    pConnCb->pendingMps = mps;

    eattReconfigureNextChannels(connId);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send an L2CAP data packet on the given eatt slott.
 *
 *  \param  pCcb      ATT control block.
 *  \param  slot      EATT channel slot.
 *  \param  handle    The connection handle.  The client receives this handle from DM.
 *  \param  len       The length of the payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattL2cDataReq(attCcb_t *pCcb, uint8_t slot, uint16_t len, uint8_t *pPacket)
{
  /* send EATT packet to L2CAP via COC channel for the slot */
  uint16_t cid = eattGetCid(pCcb->connId, slot);

  L2cCocDataReq(cid, len, pPacket + L2C_PAYLOAD_START);
  WsfMsgFree(pPacket);
}

/*************************************************************************************************/
/*!
 *  \brief  Empty l2c coc callback for ATT.
 *
 *  \param  pMsg      L2CAP coc event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attEmptyL2cCocDataCback(l2cCocEvt_t *pMsg)
{
  l2cCocDataInd_t *pDataInd = &pMsg->dataInd;
  dmConnId_t       connId = (dmConnId_t) pDataInd->hdr.param;
  uint8_t          slot = eattGetSlotId(connId, pDataInd->cid);
  attCcb_t        *pCcb = attCcbByConnId(connId);

  /* get connection control block for this connId, ignore packet if not found */
  if (pCcb)
  {
    /* parse opcode */
    uint8_t opcode = *(pDataInd->pData);

    attSendOpNotSupportedErr(pCcb, slot, opcode);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the Enhanced ATT subsystem.
 *
 *  \return None
 *
 *  \note   EattInit must be called after the ATT task is created and before calling EattsInit 
 *          and/or EattcInit.
 */
/*************************************************************************************************/
void EattInit(uint8_t roleBits)
{
  l2cCocReg_t reg;

  /* EATT_CONN_CHAN_MAX must be greater than 0 */
  WSF_ASSERT(EATT_CONN_CHAN_MAX);

  /* Register with L2CAP */
  reg.psm = EATT_PSM;
  reg.mtu = pEattCfg->mtu;
  reg.mps = pEattCfg->mps;
  reg.credits = EATT_INIT_CREDITS;
  reg.authoriz = pEattCfg->authoriz;
  reg.secLevel = pEattCfg->secLevel;
  reg.role = roleBits;

  eattCb.cocRegId = L2cCocRegister(eattL2cCocCback, &reg);
  L2cCocSetAcceptCback(eattCb.cocRegId, eattL2cCocAcceptCback);

  /* Register with DM */
  attCb.eattDmCback = eattDmCback;

  /* Register functions with ATT control block */
  attCb.eattHandler = eattHandler;
  attCb.eattL2cDataReq = eattL2cDataReq;
  
  attCb.pEnServer = &eattFcnDefault;
  attCb.pEnClient = &eattFcnDefault;

  /* Set the channel control blocks in the connection control blocks */
#if EATT_CONN_CHAN_MAX > 0
  for (uint8_t i = 0; i < DM_CONN_MAX; i++)
  {
    eattCb.ccb[i].pChanCb = eattChanCb[i];
  }
#endif
}
