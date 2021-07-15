/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Device manager periodic advertising synchronization management and state machine
 *          module.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "dm_api.h"
#include "dm_main.h"
#include "dm_scan.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! DM sync event handler messages for state machine */
enum
{
  /* messages from API */
  DM_SYNC_MSG_API_START = DM_MSG_START(DM_ID_SYNC), /*!< Start Sync */
  DM_SYNC_MSG_API_STOP,                             /*!< Stop Sync */

  /* messages from HCI */
  DM_SYNC_MSG_HCI_LE_SYNC_EST_FAIL,                 /*!< HCI LE Sync Establishment Failed */
  DM_SYNC_MSG_HCI_LE_SYNC_EST,                      /*!< HCI LE Sync Established */
  DM_SYNC_MSG_HCI_LE_SYNC_LOST,                     /*!< HCI LE Sync Lost */
  DM_SYNC_MSG_HCI_LE_SYNC_TRSF_EST_FAIL,            /*!< HCI LE Sync Transfer Establishment Failed */
  DM_SYNC_MSG_HCI_LE_SYNC_TRSF_EST                  /*!< HCI LE Sync Transfer Established */
};

/*! State machine states */
enum
{
  DM_SYNC_SM_ST_IDLE,                               /*!< Idle State */
  DM_SYNC_SM_ST_SYNCING,                            /*!< Synchronizing State */
  DM_SYNC_SM_ST_SYNCED,                             /*!< Synced State */
  DM_SYNC_SM_ST_DESYNCING,                          /*!< Desynchronizing State */

  DM_SYNC_SM_NUM_STATES
};

/*! State machine actions */
enum
{
  DM_SYNC_SM_ACT_NONE,                              /*!< No Action */
  DM_SYNC_SM_ACT_START,                             /*!< Process Start Sync */
  DM_SYNC_SM_ACT_STOP,                              /*!< Process Stop Sync */
  DM_SYNC_SM_ACT_CANCEL_START,                      /*!< Process Cancel Start Sync */
  DM_SYNC_SM_ACT_SYNC_EST,                          /*!< Process Sync Established */
  DM_SYNC_SM_ACT_SYNC_EST_FAILED,                   /*!< Process Sync Establishment Failed */
  DM_SYNC_SM_ACT_SYNC_LOST,                         /*!< Process Sync Lost */
  DM_SYNC_SM_ACT_SYNC_TRSF_EST,                     /*!< Process Sync Transfer Established */
  DM_SYNC_SM_ACT_SYNC_TRSF_EST_FAILED               /*!< Process Sync Transfer Establishment Failed */
};

/*! Column position of next state */
#define DM_SYNC_NEXT_STATE          0

/*! Column position of action */
#define DM_SYNC_ACTION              1

/*! Number of columns in the state machine state tables */
#define DM_SYNC_NUM_COLS            2

/*! Number of messages */
#define DM_SYNC_NUM_MSGS            (DM_SYNC_MSG_HCI_LE_SYNC_TRSF_EST - DM_SYNC_MSG_API_START + 1)

/*! Translate HCI event to state machine message */
#define DM_SYNC_HCI_EVT_2_MSG(evt)  (DM_SYNC_MSG_HCI_LE_SYNC_LOST - HCI_LE_PER_ADV_SYNC_LOST_CBACK_EVT + (evt))

/*! Action function */
typedef void (*dmSyncAct_t)(dmSyncCb_t *pCcb, dmSyncMsg_t *pMsg);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! DM Sync state machine state tables */
static const uint8_t dmSyncStateTbl[DM_SYNC_SM_NUM_STATES][DM_SYNC_NUM_MSGS][DM_SYNC_NUM_COLS] =
{
  /* Idle state */
  {
  /* Event                        Next state                    Action */
  /* API_START */                 {DM_SYNC_SM_ST_SYNCING,       DM_SYNC_SM_ACT_START},
  /* API_STOP */                  {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_NONE},
  /* HCI_LE_SYNC_EST_FAIL */      {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_NONE},
  /* HCI_LE_SYNC_EST */           {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_NONE},
  /* HCI_LE_SYNC_LOST */          {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_NONE},
  /* HCI_LE_SYNC_TRSF_EST_FAIL */ {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_SYNC_TRSF_EST_FAILED},
  /* HCI_LE_SYNC_TRSF_EST */      {DM_SYNC_SM_ST_SYNCED,        DM_SYNC_SM_ACT_SYNC_TRSF_EST}
  },
  /* Syncing state */
  {
  /* Event                        Next state                    Action */
  /* API_START */                 {DM_SYNC_SM_ST_SYNCING,       DM_SYNC_SM_ACT_NONE},
  /* API_STOP */                  {DM_SYNC_SM_ST_DESYNCING,     DM_SYNC_SM_ACT_CANCEL_START},
  /* HCI_LE_SYNC_EST_FAIL */      {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_SYNC_EST_FAILED},
  /* HCI_LE_SYNC_EST */           {DM_SYNC_SM_ST_SYNCED,        DM_SYNC_SM_ACT_SYNC_EST},
  /* HCI_LE_SYNC_LOST */          {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_SYNC_EST_FAILED},
  /* HCI_LE_SYNC_TRSF_EST_FAIL */ {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_SYNC_TRSF_EST_FAILED},
  /* HCI_LE_SYNC_TRSF_EST */      {DM_SYNC_SM_ST_SYNCED,        DM_SYNC_SM_ACT_SYNC_TRSF_EST},
  },
  /* Synced state */
  {
  /* Event                        Next state                    Action */
  /* API_START */                 {DM_SYNC_SM_ST_SYNCED,        DM_SYNC_SM_ACT_NONE},
  /* API_STOP */                  {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_STOP},
  /* HCI_LE_SYNC_EST_FAIL */      {DM_SYNC_SM_ST_SYNCED,        DM_SYNC_SM_ACT_NONE},
  /* HCI_LE_SYNC_EST */           {DM_SYNC_SM_ST_SYNCED,        DM_SYNC_SM_ACT_NONE},
  /* HCI_LE_SYNC_LOST */          {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_SYNC_LOST},
  /* HCI_LE_SYNC_TRSF_EST_FAIL */ {DM_SYNC_SM_ST_SYNCED,        DM_SYNC_SM_ACT_NONE},
  /* HCI_LE_SYNC_TRSF_EST */      {DM_SYNC_SM_ST_SYNCED,        DM_SYNC_SM_ACT_NONE},
  },
  /* Desyncing state */
  {
  /* Event                        Next state                    Action */
  /* API_START */                 {DM_SYNC_SM_ST_DESYNCING,     DM_SYNC_SM_ACT_NONE},
  /* API_STOP */                  {DM_SYNC_SM_ST_DESYNCING,     DM_SYNC_SM_ACT_NONE},
  /* HCI_LE_SYNC_EST_FAIL */      {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_SYNC_EST_FAILED},
  /* HCI_LE_SYNC_EST */           {DM_SYNC_SM_ST_DESYNCING,     DM_SYNC_SM_ACT_STOP},
  /* HCI_LE_SYNC_LOST */          {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_SYNC_LOST},
  /* HCI_LE_SYNC_TRSF_EST_FAIL */ {DM_SYNC_SM_ST_IDLE,          DM_SYNC_SM_ACT_SYNC_TRSF_EST_FAILED},
  /* HCI_LE_SYNC_TRSF_EST */      {DM_SYNC_SM_ST_DESYNCING,     DM_SYNC_SM_ACT_STOP},
  }
};

/*! DM Sync action function table */
static const dmSyncAct_t dmSyncAct[] =
{
  dmSyncSmActNone,
  dmSyncSmActStart,
  dmSyncSmActStop,
  dmSyncSmActCancelStart,
  dmSyncSmActSyncEst,
  dmSyncSmActSyncEstFailed,
  dmSyncSmActSyncLost,
  dmSyncSmActSyncTrsfEst,
  dmSyncSmActSyncTrsfEstFailed
};

/*! DM Sync component function interface */
static const dmFcnIf_t dmSyncFcnIf =
{
  dmSyncReset,
  dmSyncHciHandler,
  dmSyncMsgHandler
};

/*! DM Periodic advertising sync control block */
static dmSyncCb_t dmSyncCb[DM_SYNC_MAX];

/*************************************************************************************************/
/*!
 *  \brief  Return the SCB with particular state conditions.
 *
 *  \return Pointer to SCB or NULL if failure.
 */
/*************************************************************************************************/
static dmSyncCb_t *dmSyncCmplState(void)
{
  dmSyncCb_t   *pScb = dmSyncCb;
  uint8_t       i;

  /* if there's a scb in accepting state */
  for (i = DM_SYNC_MAX; i > 0; i--, pScb++)
  {
    /* look for sync in desyncing state, cancelled sync */
    if (pScb->inUse                              &&
        (pScb->state == DM_SYNC_SM_ST_DESYNCING) &&
        (pScb->handle == DM_SYNC_HCI_HANDLE_NONE))
    {
      DM_TRACE_INFO1("dmSyncCmplState %d", pScb->syncId);
      return pScb;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
*  \brief  Allocate a DM sync control block.
*
*  \param  sid    Advertising Sid.
*  \param  pAddr  Advertiser BD address.
*
*  \return Pointer to sync CB or NULL if failure.
*/
/*************************************************************************************************/
static dmSyncCb_t *dmSyncCbAlloc(uint8_t sid, const uint8_t *pAddr)
{
  dmSyncCb_t   *pScb = dmSyncCb;
  uint8_t      i;

  for (i = 0; i < DM_SYNC_MAX; i++, pScb++)
  {
    if (pScb->inUse == FALSE)
    {
      memset(pScb, 0, sizeof(dmSyncCb_t));

      pScb->advSid = sid;
      BdaCpy(pScb->advAddr, pAddr);
      pScb->handle = DM_SYNC_HCI_HANDLE_NONE;
      pScb->syncId = i + 1;
      pScb->inUse = TRUE;

      DM_TRACE_ALLOC1("dmSyncCbAlloc %d", pScb->syncId);

      return pScb;
    }
  }

  DM_TRACE_ERR0("dmSyncCbAlloc failed");

  return NULL;
}

/*************************************************************************************************/
/*!
*  \brief  Deallocate a DM sync control block.
*
*  \param  pCb   Sync control block.
*
*  \return None.
*/
/*************************************************************************************************/
static void dmSyncCbDealloc(dmSyncCb_t *pCb)
{
  DM_TRACE_FREE1("dmSyncCbDealloc %d", pCb->syncId);

  pCb->inUse = FALSE;
}

/*************************************************************************************************/
/*!
*  \brief  Find a sync control block with matching handle.
*
*  \param  handle  Handle to find.
*
*  \return Pointer to sync CB or NULL if failure.
*/
/*************************************************************************************************/
static dmSyncCb_t *dmSyncCbByHandle(uint16_t handle)
{
  dmSyncCb_t   *pScb = dmSyncCb;
  uint8_t      i;

  for (i = DM_SYNC_MAX; i > 0; i--, pScb++)
  {
    if (pScb->inUse && (pScb->handle == handle))
    {
      return pScb;
    }
  }

  DM_TRACE_WARN1("dmSyncCbByHandle not found 0x%04x", handle);

  return NULL;
}

/*************************************************************************************************/
/*!
*  \brief  Find a sync control block with advertising SID and advertiser BD address.
*
*  \param  sid    Sid to find.
*  \param  pAddr  BD address to find.
*
*  \return Pointer to CB or NULL if failure.
*/
/*************************************************************************************************/
dmSyncCb_t *dmSyncCbBySidBdAddr(uint8_t sid, const uint8_t *pAddr)
{
  dmSyncCb_t   *pScb = dmSyncCb;
  uint8_t       i;

  for (i = DM_SYNC_MAX; i > 0; i--, pScb++)
  {
    if (pScb->inUse && (pScb->advSid == sid) && BdaCmp(pScb->advAddr, pAddr))
    {
      return pScb;
    }
  }

  DM_TRACE_INFO0("dmSyncCbBySidBdAddr not found");

  return NULL;
}

/*************************************************************************************************/
/*!
*  \brief  Return the sync control block for the given sync ID.
*
*  \param  syncId  Sync ID.
*
*  \return Pointer to CB or NULL if failure.
*/
/*************************************************************************************************/
dmSyncCb_t *dmSyncCbById(dmSyncId_t syncId)
{
  WSF_ASSERT((syncId > 0) && (syncId <= DM_SYNC_MAX));

  syncId--;
  if (dmSyncCb[syncId].inUse)
  {
    return &dmSyncCb[syncId];
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Empty action.
 *
 *  \param  pScb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncSmActNone(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Synchronize with periodic advertising from the given advertiser.
 *
 *  \param  pScb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncSmActStart(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg)
{
  HciLePerAdvCreateSyncCmd(dmCb.syncOptions, pMsg->apiSyncStart.advSid,
                           pMsg->apiSyncStart.advAddrType, pMsg->apiSyncStart.advAddr,
                           pMsg->apiSyncStart.skip, pMsg->apiSyncStart.syncTimeout,
                           pMsg->apiSyncStart.syncCteType);
}

/*************************************************************************************************/
/*!
 *  \brief  Stop reception of the periodic advertising identified by the given sync handle.
 *
 *  \param  pScb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncSmActStop(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg)
{
  HciLePerAdvTerminateSyncCmd(pScb->handle);

  dmSyncSmActSyncLost(pScb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Cancel creation of a sync while it's pending.
 *
 *  \param  pScb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncSmActCancelStart(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg)
{
  HciLePerAdvCreateSyncCancelCmd();
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a sync established event from HCI.
 *
 *  \param  pScb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncSmActSyncEst(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg)
{
  /* store adv sid, adv address and sync handle */
  pScb->advSid = pMsg->perAdvSyncEst.advSid;
  BdaCpy(pScb->advAddr, pMsg->perAdvSyncEst.advAddr);
  pScb->advAddrType = DmHostAddrType(pMsg->perAdvSyncEst.advAddrType);
  pScb->handle = pMsg->perAdvSyncEst.syncHandle;

  pMsg->hdr.event = DM_PER_ADV_SYNC_EST_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a sync established failure event from HCI.
 *
 *  \param  pScb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncSmActSyncEstFailed(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg)
{
  /* deallocate scb */
  dmSyncCbDealloc(pScb);

  pMsg->hdr.event = DM_PER_ADV_SYNC_EST_FAIL_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a sync lost event from HCI.
 *
 *  \param  pScb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncSmActSyncLost(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg)
{
  /* set sync handle */
  pMsg->perAdvSyncLost.syncHandle = pScb->handle;

  /* deallocate scb */
  dmSyncCbDealloc(pScb);

  pMsg->hdr.event = DM_PER_ADV_SYNC_LOST_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a sync transfer established event from HCI.
 *
 *  \param  pScb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncSmActSyncTrsfEst(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg)
{
  /* store adv sid, adv address and sync handle */
  pScb->advSid = pMsg->perAdvSyncTrsfEst.advSid;
  BdaCpy(pScb->advAddr, pMsg->perAdvSyncTrsfEst.advAddr);
  pScb->advAddrType = DmHostAddrType(pMsg->perAdvSyncTrsfEst.advAddrType);
  pScb->handle = pMsg->perAdvSyncTrsfEst.syncHandle;

  pMsg->hdr.event = DM_PER_ADV_SYNC_TRSF_EST_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a sync transfer established failure event from HCI.
 *
 *  \param  pScb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncSmActSyncTrsfEstFailed(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg)
{
  /* deallocate scb */
  dmSyncCbDealloc(pScb);

  pMsg->hdr.event = DM_PER_ADV_SYNC_TRSF_EST_FAIL_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Execute the DM sync state machine.
 *
 *  \param  pScb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncSmExecute(dmSyncCb_t *pScb, dmSyncMsg_t *pMsg)
{
  uint8_t action;
  uint8_t event;

  DM_TRACE_INFO2("dmSyncSmExecute event=%d state=%d", pMsg->hdr.event, pScb->state);

  /* get the event */
  event = DM_MSG_MASK(pMsg->hdr.event);

  /* get action */
  action = dmSyncStateTbl[pScb->state][event][DM_SYNC_ACTION];

  /* set next state */
  pScb->state = dmSyncStateTbl[pScb->state][event][DM_SYNC_NEXT_STATE];

  /* execute action function */
  (*dmSyncAct[action])(pScb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the sync module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncInit(void)
{
  dmFcnIfTbl[DM_ID_SYNC] = (dmFcnIf_t *) &dmSyncFcnIf;
}

/*************************************************************************************************/
/*!
 *  \brief  Reset the sync module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncReset(void)
{
  dmSyncCb_t                *pScb = dmSyncCb;
  hciLePerAdvSyncLostEvt_t  syncLost;
  uint8_t                   i;

  /* generate HCI sync lost event */
  syncLost.hdr.event = HCI_LE_PER_ADV_SYNC_LOST_CBACK_EVT;
  syncLost.hdr.status = HCI_SUCCESS;

  for (i = DM_SYNC_MAX; i > 0; i--, pScb++)
  {
    if (pScb->inUse)
    {
      /* set sync handle */
      syncLost.hdr.param = pScb->syncId;
      syncLost.syncHandle = pScb->handle;

      /* handle the event */
      dmSyncHciHandler((hciEvt_t *) &syncLost);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM sync HCI event handler.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncHciHandler(hciEvt_t *pEvent)
{
  dmSyncCb_t *pScb;

  /* handle special case for periodic advertising report event */
  if (pEvent->hdr.event == HCI_LE_PER_ADV_REPORT_CBACK_EVT)
  {
    if ((pScb = dmSyncCbByHandle(pEvent->hdr.param)) != NULL)
    {
      /* set sync id */
      pEvent->hdr.param = pScb->syncId;

      pEvent->hdr.event = DM_PER_ADV_REPORT_IND;
      (*dmCb.cback)((dmEvt_t *) pEvent);
    }

    return;
  }
  
  /* handle special case for BIGInfo advertising report event */
  if (pEvent->hdr.event == HCI_LE_BIG_INFO_ADV_REPORT_CBACK_EVT)
  {
    if ((pScb = dmSyncCbByHandle(pEvent->hdr.param)) != NULL)
    {
      /* store sync info */
      pScb->encrypt = pEvent->leBigInfoAdvRpt.encrypt;

      /* set sync id */
      pEvent->hdr.param = pScb->syncId;

      pEvent->hdr.event = DM_BIG_INFO_ADV_REPORT_IND;
      (*dmCb.cback)((dmEvt_t *) pEvent);
    }

    return;
  }

  /* handle special cases for sync established event */
  if (pEvent->hdr.event == HCI_LE_PER_ADV_SYNC_EST_CBACK_EVT)
  {
    /* first check if scb exists for this sid and bd addr */
    pScb = dmSyncCbBySidBdAddr(pEvent->lePerAdvSyncEst.advSid, pEvent->lePerAdvSyncEst.advAddr);

    /* if scb not found */
    if (pScb == NULL)
    {
      /* check for special case state */
      pScb = dmSyncCmplState();
    }

    /* translate HCI event to state machine event */
    if (pEvent->hdr.status == HCI_SUCCESS)
    {
      pEvent->hdr.event = DM_SYNC_MSG_HCI_LE_SYNC_EST;
    }
    else
    {
      pEvent->hdr.event = DM_SYNC_MSG_HCI_LE_SYNC_EST_FAIL;
    }
  }
  /* handle special cases for past received event */
  else if (pEvent->hdr.event == HCI_LE_PER_SYNC_TRSF_RCVD_CBACK_EVT)
  {
    /* first check if scb exists for this sid and bd addr */
    pScb = dmSyncCbBySidBdAddr(pEvent->lePerAdvSyncTrsfRcvd.advSid, pEvent->lePerAdvSyncTrsfRcvd.advAddr);

    /* if scb not found */
    if (pScb == NULL)
    {
      /* allocate scb */
      pScb = dmSyncCbAlloc(pEvent->lePerAdvSyncTrsfRcvd.advSid, pEvent->lePerAdvSyncTrsfRcvd.advAddr);
    }

    /* translate HCI event to state machine event */
    if (pEvent->hdr.status == HCI_SUCCESS)
    {
      pEvent->hdr.event = DM_SYNC_MSG_HCI_LE_SYNC_TRSF_EST;
    }
    else
    {
      pEvent->hdr.event = DM_SYNC_MSG_HCI_LE_SYNC_TRSF_EST_FAIL;
    }
  }
  else
  {
    pScb = dmSyncCbByHandle(pEvent->hdr.param);

    /* translate HCI event to state machine message */
    pEvent->hdr.event = DM_SYNC_HCI_EVT_2_MSG(pEvent->hdr.event);
  }

  /* if scb found */
  if (pScb != NULL)
  {
    /* set sync id */
    pEvent->hdr.param = pScb->syncId;

    /* execute state machine */
    dmSyncSmExecute(pScb, (dmSyncMsg_t *) pEvent);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM sync event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSyncMsgHandler(wsfMsgHdr_t *pMsg)
{
  dmSyncCb_t *pScb;

  /* look up scb from sync id */
  if ((pScb = dmSyncCbById((dmSyncId_t) pMsg->param)) != NULL)
  {
    /* execute state machine */
    dmSyncSmExecute(pScb, (dmSyncMsg_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Synchronize with periodic advertising from the given advertiser, and start receiving
 *          periodic advertising packets.
 *
 *          Note: The synchronization filter policy is used to determine whether the periodic
 *                advertiser list is used. If the periodic advertiser list is not used, the
 *                advertising SID, advertiser address type, and advertiser address parameters
 *                specify the periodic advertising device to listen to; otherwise these parameters
 *                are ignored.
 *
 *  \param  advSid        Advertising SID.
 *  \param  advAddrType   Advertiser address type.
 *  \param  pAdvAddr      Advertiser address.
 *  \param  skip          Number of periodic advertising packets that can be skipped after
 *                        successful receive.
 *  \param  syncTimeout   Synchronization timeout, in the units of 10ms.
 *  \param  syncCteType   Whether to only synchronize to periodic advertising with certain types
 *                        of Constant Tone Extension (0 indicates that the presence or absence of
 *                        a Constant Tone Extension is irrelevant).
 *
 *  \return Sync identifier.
 */
/*************************************************************************************************/
dmSyncId_t DmSyncStart(uint8_t advSid, uint8_t advAddrType, const uint8_t *pAdvAddr, uint16_t skip,
                       uint16_t syncTimeout, uint8_t syncCteType)
{
  dmSyncCb_t       *pScb = NULL;
  dmSyncApiStart_t *pMsg;

  /* make sure scb not already allocated */
  WsfTaskLock();
  if ((pScb = dmSyncCbBySidBdAddr(advSid, pAdvAddr)) == NULL)
  {
    /* allocate scb */
    pScb = dmSyncCbAlloc(advSid, pAdvAddr);
  }
  WsfTaskUnlock();

  if (pScb != NULL)
  {
    if ((pMsg = WsfMsgAlloc(sizeof(dmSyncApiStart_t))) != NULL)
    {
      pMsg->hdr.param = pScb->syncId;
      pMsg->hdr.event = DM_SYNC_MSG_API_START;
      pMsg->advSid = advSid;
      pMsg->advAddrType = advAddrType;
      BdaCpy(pMsg->advAddr, pAdvAddr);
      pMsg->skip = skip;
      pMsg->syncTimeout = syncTimeout;
      pMsg->syncCteType = syncCteType;
      WsfMsgSend(dmCb.handlerId, pMsg);

      /* return sync id */
      return pScb->syncId;
    }
    else
    {
      WsfTaskLock();
      dmSyncCbDealloc(pScb);
      WsfTaskUnlock();
    }
  }

  /* open failed */
  return DM_SYNC_ID_NONE;
}

/*************************************************************************************************/
/*!
 *  \brief  Stop reception of the periodic advertising identified by the given sync identifier.
 *
 *  \param  syncId        Sync identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSyncStop(dmSyncId_t syncId)
{
  wsfMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(wsfMsgHdr_t))) != NULL)
  {
    pMsg->param = syncId;
    pMsg->event = DM_SYNC_MSG_API_STOP;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the encryption mode of the BIG corresponding to the periodic advertising train
 *          identified by the sync handle.
 *
 *  \param  syncHandle   Synch handle.
 *  \param  encrypt      FALSE (Unencrypted) or FALSE (Encrypted).
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSyncSetEncrypt(uint16_t syncHandle, bool_t encrypt)
{
  dmSyncCb_t *pScb;

  if ((pScb = dmSyncCbByHandle(syncHandle)) != NULL)
  {
    pScb->encrypt = encrypt;
  }
}

/*************************************************************************************************/
/*!
*  \brief  Get the encryption mode of the BIG corresponding to the periodic advertising train
*          identified by the sync handle.
*
*  \param  syncHandle   Synch handle.
*
*  \return TRUE if sync encrypted. FALSE, otherwise.
*/
/*************************************************************************************************/
bool_t DmSyncEncrypted(uint16_t syncHandle)
{
  dmSyncCb_t *pScb;

  if ((pScb = dmSyncCbByHandle(syncHandle)) != NULL)
  {
    return pScb->encrypt;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Get status of sync identified by the handle.
 *
 *  \param  syncHandle   Synch handle.
 *
 *  \return TRUE if sync is enabled for that handle. FALSE, otherwise.
 */
/*************************************************************************************************/
bool_t DmSyncEnabled(uint16_t syncHandle)
{
  return (dmSyncCbByHandle(syncHandle) != NULL) ? TRUE : FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Add device to periodic advertiser list.
 *
 *  \param  advAddrType   Advertiser address type.
 *  \param  pAdvAddr      Advertiser address.
 *  \param  advSid        Advertising SID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAddDeviceToPerAdvList(uint8_t advAddrType, uint8_t *pAdvAddr, uint8_t advSid)
{
  HciLeAddDeviceToPerAdvListCmd(advAddrType, pAdvAddr, advSid);
}

/*************************************************************************************************/
/*!
 *  \brief  DM remove device from periodic advertiser list.
 *
 *  \param  advAddrType   Advertiser address type.
 *  \param  pAdvAddr      Advertiser address.
 *  \param  advSid        Advertising SID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmRemoveDeviceFromPerAdvList(uint8_t advAddrType, uint8_t *pAdvAddr, uint8_t advSid)
{
  HciLeRemoveDeviceFromPerAdvListCmd(advAddrType, pAdvAddr, advSid);
}

/*************************************************************************************************/
/*!
 *  \brief  DM clear periodic advertiser list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmClearPerAdvList(void)
{
  HciLeClearPerAdvListCmd();
}

/*************************************************************************************************/
/*!
 *  \brief  DM enable or disable initial periodic advertising reports once synchronized.
 *
 *  \param  enable        TRUE to enable initial reporting, FALSE to disable initial reporting.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSyncInitialRptEnable(bool_t enable)
{
  WsfTaskLock();
  if (enable)
  {
    /* Enable initial periodic advertisement reporting */
    dmCb.syncOptions &= ~HCI_OPTIONS_INIT_RPT_ENABLE_BIT;
  }
  else
  {
    /* Disable initial periodic advertisement reporting */
    dmCb.syncOptions |= HCI_OPTIONS_INIT_RPT_ENABLE_BIT;
  }
  WsfTaskUnlock();
}
