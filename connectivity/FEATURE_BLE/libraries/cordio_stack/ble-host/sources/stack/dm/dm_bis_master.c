/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  DM Broadcast Isochronous Stream (BIS) management for master.
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
#include "wsf_math.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "dm_api.h"
#include "dm_main.h"
#include "dm_scan.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! BIS event handler messages for state machine */
enum
{
  /* messages from API */
  DM_BIS_MSG_API_BIG_SYNC_START = DM_MSG_START(DM_ID_BIS_SYNC), /*!< Start BIG Sync */
  DM_BIS_MSG_API_BIG_SYNC_STOP,                                 /*!< Stop BIG Sync */

  /* messages from HCI */
  DM_BIS_MSG_HCI_LE_BIG_SYNC_EST_FAIL,                          /*!< HCI LE BIG Sync Establishment Failed Event */
  DM_BIS_MSG_HCI_LE_BIG_SYNC_EST,                               /*!< HCI LE BIG Sync Established Event */
  DM_BIS_MSG_HCI_LE_BIG_SYNC_LOST,                              /*!< HCI LE BIG Sync Lost Event */
  DM_BIS_MSG_HCI_LE_BIG_TERM_SYNC_CMPL                          /*!< HCI LE BIG Terminate Sync Complete Event */
};

/*! BIS tate machine states */
enum
{
  DM_BIS_SM_ST_IDLE,                                            /*!< Idle State */
  DM_BIS_SM_ST_BIG_SYNCING,                                     /*!< BIG Synchronizing State */
  DM_BIS_SM_ST_BIG_SYNCED,                                      /*!< BIG Synced State */
  DM_BIS_SM_ST_BIG_DESYNCING,                                   /*!< BIG Desynchronizing State */

  DM_BIS_SM_NUM_STATES
};

/*! BIS state machine actions */
enum
{
  DM_BIS_SM_ACT_NONE,                                           /*!< No Action */
  DM_BIS_SM_ACT_BIG_SYNC_START,                                 /*!< Process Start BG Sync */
  DM_BIS_SM_ACT_BIG_SYNC_STOP,                                  /*!< Process Stop BIG Sync */
  DM_BIS_SM_ACT_BIG_SYNC_CANCEL_START,                          /*!< Process Cancel Start BIG Sync */
  DM_BIS_SM_ACT_BIG_SYNC_EST,                                   /*!< Process BIG Sync Established */
  DM_BIS_SM_ACT_BIG_SYNC_EST_FAILED,                            /*!< Process BIG Sync Establishment Failed */
  DM_BIS_SM_ACT_BIG_SYNC_LOST,                                  /*!< Process BIG Sync Lost */
  DM_BIS_SM_ACT_BIG_SYNC_STOPPED                                /*!< Process BIG Terminate Sync Complete  */
};

/*! Column position of next state */
#define DM_BIS_NEXT_STATE          0

/*! Column position of action */
#define DM_BIS_ACTION              1

/*! Number of columns in the state machine state tables */
#define DM_BIS_NUM_COLS            2

/*! Number of messages */
#define DM_BIS_NUM_MSGS            (DM_BIS_MSG_HCI_LE_BIG_TERM_SYNC_CMPL - DM_BIS_MSG_API_BIG_SYNC_START + 1)

/*! Translate HCI event to state machine message */
#define DM_BIS_HCI_EVT_2_MSG(evt)  (DM_BIS_MSG_HCI_LE_BIG_SYNC_LOST - HCI_LE_BIG_SYNC_LOST_CBACK_EVT + (evt))

/* Uninitialized HCI handle */
#define DM_BIS_HCI_HANDLE_NONE     0xFFFF

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Data structure for DM_BIS_MSG_API_BIG_SYNC_START */
typedef struct
{
  wsfMsgHdr_t                 hdr;                    /*!< \brief Header structure. */
  uint16_t                    syncHandle;             /*!< \brief Periodic advertising train handle. */
  uint8_t                     mse;                    /*!< \brief Maximum number of subevents. */
  uint16_t                    bigSyncTimeout;         /*!< \brief Synchronization timeout for the BIS, in the units of 10ms. */
  uint8_t                     numBis;                 /*!< \brief Total number of BISes in the BIG. */
  uint8_t                     bis[DM_BIS_MAX];        /*!< \brief List of indices of BISes (in ascending order). */
} dmBisApiBigSyncStart_t;

/* Union of all DM BIS state machine messages */
typedef union
{
  wsfMsgHdr_t                 hdr;                    /*!< \brief Header structure. */
  dmBisApiBigSyncStart_t      apiBigSyncStart;        /*!< \brief API BIG sync start message. */
  HciLeBigTermSyncCmplEvt_t   bigTermSyncCmpl;        /*!< \brief LE BIG terminate sync complete. */
  HciLeBigSyncEstEvt_t        bigSyncEst;             /*!< \brief LE BIG sync established. */
  HciLeBigSyncLostEvt_t       bigSyncLost;            /*!< \brief LE BIG sync lost. */
  HciLeBigInfoAdvRptEvt_t     bigInfoAdvRpt;          /*!< \brief LE BIG info advertising report. */
} dmBisMsg_t;

/* Control block for BIG sync */
typedef struct
{
  uint8_t                     state;                  /*!< \brief BIG state. */
  uint16_t                    syncHandle;             /*!< \brief Periodic advertising train handle. */
  uint8_t                     secLevel;               /*!< \brief Security level. */
  bool_t                      encrypt;                /*!< \brief Unencrypted or Encrypted. */
  bool_t                      authen;                 /*!< \brief Unauthenticated or Authenticated. */
  uint8_t                     bcastCode[HCI_BC_LEN];  /*!< \brief Broadcast code. */

  /* BIS */
  uint8_t                     numBis;                 /*!< \brief Total number of BISes in the BIG. */
  uint16_t                    bisHandle[DM_BIS_MAX];  /*!< \brief BIS handles. */
} dmBigSyncCb_t;

/*! Action function */
typedef void (*dmBisAct_t)(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg);

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void dmBisSmActNone(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigSyncStart(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigSyncStop(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigSyncCancelStart(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigSyncEst(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigSyncEstFailed(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigSyncLost(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigSyncStopped(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg);

static void dmBisReset(void);
static void dmBisMsgHandler(wsfMsgHdr_t *pMsg);
static void dmBisHciHandler(hciEvt_t *pEvent);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! DM BIS state machine state tables */
static const uint8_t dmBisStateTbl[DM_BIS_SM_NUM_STATES][DM_BIS_NUM_MSGS][DM_BIS_NUM_COLS] =
{
  /* Idle state */
  {
  /* Event                        Next state                    Action */
  /* API_BIG_SYNC_START */        {DM_BIS_SM_ST_BIG_SYNCING,    DM_BIS_SM_ACT_BIG_SYNC_START},
  /* API_BIG_SYNC_STOP */         {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_NONE},
  /* HCI_LE_BIG_SYNC_EST_FAIL */  {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_NONE},
  /* HCI_LE_BIG_SYNC_EST */       {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_NONE},
  /* HCI_LE_BIG_SYNC_LOST */      {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_NONE},
  /* HCI_LE_BIG_TERM_SYNC_CMPL */ {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_NONE}
  },
  /* BIG Syncing state */
  {
  /* Event                        Next state                    Action */
  /* API_BIG_SYNC_START */        {DM_BIS_SM_ST_BIG_SYNCING,    DM_BIS_SM_ACT_NONE},
  /* API_BIG_SYNC_STOP */         {DM_BIS_SM_ST_BIG_DESYNCING,  DM_BIS_SM_ACT_BIG_SYNC_CANCEL_START},
  /* HCI_LE_BIG_SYNC_EST_FAIL */  {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_SYNC_EST_FAILED},
  /* HCI_LE_BIG_SYNC_EST */       {DM_BIS_SM_ST_BIG_SYNCED,     DM_BIS_SM_ACT_BIG_SYNC_EST},
  /* HCI_LE_BIG_SYNC_LOST */      {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_SYNC_EST_FAILED},
  /* HCI_LE_BIG_TERM_SYNC_CMPL */ {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_SYNC_EST_FAILED}
  },
  /* BIG Synced state */
  {
  /* Event                        Next state                    Action */
  /* API_BIG_SYNC_START */        {DM_BIS_SM_ST_BIG_SYNCED,     DM_BIS_SM_ACT_NONE},
  /* API_BIG_SYNC_STOP */         {DM_BIS_SM_ST_BIG_DESYNCING,  DM_BIS_SM_ACT_BIG_SYNC_STOP},
  /* HCI_LE_BIG_SYNC_EST_FAIL */  {DM_BIS_SM_ST_BIG_SYNCED,     DM_BIS_SM_ACT_NONE},
  /* HCI_LE_BIG_SYNC_EST */       {DM_BIS_SM_ST_BIG_SYNCED,     DM_BIS_SM_ACT_NONE},
  /* HCI_LE_BIG_SYNC_LOST */      {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_SYNC_LOST},
  /* HCI_LE_BIG_TERM_SYNC_CMPL */ {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_SYNC_STOPPED}
  },
  /* BIG Desyncing state */
  {
  /* Event                        Next state                    Action */
  /* API_BIG_SYNC_START */        {DM_BIS_SM_ST_BIG_DESYNCING,  DM_BIS_SM_ACT_NONE},
  /* API_BIG_SYNC_STOP */         {DM_BIS_SM_ST_BIG_DESYNCING,  DM_BIS_SM_ACT_NONE},
  /* HCI_LE_BIG_SYNC_EST_FAIL */  {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_SYNC_EST_FAILED},
  /* HCI_LE_BIG_SYNC_EST */       {DM_BIS_SM_ST_BIG_DESYNCING,  DM_BIS_SM_ACT_BIG_SYNC_STOP},
  /* HCI_LE_BIG_SYNC_LOST */      {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_SYNC_LOST},
  /* HCI_LE_BIG_TERM_SYNC_CMPL */ {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_SYNC_STOPPED}
  }
};

/*! DM BIS action function table */
static const dmBisAct_t dmBisAct[] =
{
  dmBisSmActNone,
  dmBisSmActBigSyncStart,
  dmBisSmActBigSyncStop,
  dmBisSmActBigSyncCancelStart,
  dmBisSmActBigSyncEst,
  dmBisSmActBigSyncEstFailed,
  dmBisSmActBigSyncLost,
  dmBisSmActBigSyncStopped
};

/*! DM BIS component function interface */
static const dmFcnIf_t dmBisFcnIf =
{
  dmBisReset,
  dmBisHciHandler,
  dmBisMsgHandler
};

/*!< DM BIG sync control block. */
static dmBigSyncCb_t dmBigSyncCb[DM_BIG_MAX];

/*************************************************************************************************/
/*!
 *  \brief  Initialize the BIG sync CB for a given handle.
 *
 *  \param  pBigSyncCb  BIG sync CB.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmBigSyncCbInit(dmBigSyncCb_t *pBigSyncCb)
{
  /* set BIG sync default values */
  pBigSyncCb->state = DM_BIS_SM_ST_IDLE;
  pBigSyncCb->syncHandle = DM_SYNC_HCI_HANDLE_NONE;
  pBigSyncCb->encrypt = FALSE;
  memset(pBigSyncCb->bcastCode, 0x000, HCI_BC_LEN);

  pBigSyncCb->numBis = 0;
  for (uint8_t i = 0; i < DM_BIS_MAX; i++)
  {
    pBigSyncCb->bisHandle[i] = DM_BIS_HCI_HANDLE_NONE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Get the BIG sync CB for a given handle.
 *
 *  \param  bigHandle        BIG handle.
 *
 *  \return Pointer to BIG sync control block if BIG handle valid. NULL, otherwise.
 */
/*************************************************************************************************/
static dmBigSyncCb_t *dmBigSyncCbByHandle(uint8_t bigHandle)
{
  WSF_ASSERT(bigHandle < DM_BIG_MAX);

  if (bigHandle < DM_BIG_MAX)
  {
    return &dmBigSyncCb[bigHandle];
  }

  DM_TRACE_WARN1("dmBigSyncCbByHandle: out of range bigHandle = %d", bigHandle);

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Check if the sync handle is associated with another BIG sync.
 *
 *  \param      syncHandle       Sync handle.
 *
 *  \return     TRUE if sync handle is associated with another BIG sync. FALSE, otherwise.
 */
/*************************************************************************************************/
static bool_t dmBigSyncUsed(uint16_t syncHandle)
{
  for (uint8_t i = 0; i < DM_BIG_MAX; i++)
  {
    dmBigSyncCb_t *pBigSyncCb = &dmBigSyncCb[i];

    if ((pBigSyncCb->state != DM_BIS_SM_ST_IDLE) && (pBigSyncCb->syncHandle == syncHandle))
    {
      return TRUE;
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of available BISes.
 *
 *  \return     Number of available BISes.
 */
/*************************************************************************************************/
static uint8_t dmBigSyncGetNumAvailBis(void)
{
  uint8_t cnt = 0;

  for (uint8_t i = 0; i < DM_BIG_MAX; i++)
  {
    dmBigSyncCb_t *pBigSyncCb = &dmBigSyncCb[i];

    if (pBigSyncCb->state != DM_BIS_SM_ST_IDLE)
    {
      cnt += pBigSyncCb->numBis;
    }
  }

  if (cnt >= DM_BIS_MAX)
  {
    return 0;
  }

  return (DM_BIS_MAX - cnt);
}

/*************************************************************************************************/
/*!
 *  \brief  Empty action.
 *
 *  \param  pBigSyncCb  BIG Sync control block.
 *  \param  pMsg        WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmBisSmActNone(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Start BIG sync for the given BIG.
 *
 *  \param  pBigSyncCb  BIG Sync control block.
 *  \param  pMsg        WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmBisSmActBigSyncStart(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg)
{
  dmBisApiBigSyncStart_t  *pBigSyncStart = &pMsg->apiBigSyncStart;
  uint8_t                 bigHandle = (uint8_t) pMsg->hdr.param;
  uint8_t                 status = HCI_SUCCESS;

  if (DmSyncEnabled(pBigSyncStart->syncHandle))
  {
    if (!dmBigSyncUsed(pBigSyncStart->syncHandle))
    {
      if ((pBigSyncStart->numBis > 0) && (pBigSyncStart->numBis <= DM_BIS_MAX))
      {
        if (dmBigSyncCb[bigHandle].encrypt == DmSyncEncrypted(pBigSyncStart->syncHandle))
        {
          if (pBigSyncStart->numBis > dmBigSyncGetNumAvailBis())
          {
            DM_TRACE_WARN0("dmBisSmActBigSyncStart: insufficient BISes");
            status = HCI_ERR_REJ_RESOURCES;
          }
        }
        else
        {
          DM_TRACE_WARN0("dmBisSmActBigSyncStart: encryption mode mismatch");
          status = HCI_ERR_ENCRYPT_MODE;
        }
      }
      else
      {
        DM_TRACE_WARN2("dmBisSmActBigSyncStart: invalid value for numBis = %u, validRange = 1..%u",
                       pBigSyncStart->numBis, DM_BIS_MAX);
        status = HCI_ERR_UNSUP_FEAT;
      }
    }
    else
    {
      DM_TRACE_WARN0("dmBisSmActBigSyncStart: periodic advertising train is associated with another BIG");
      status = HCI_ERR_UNKNOWN_ADV_ID;
    }
  }
  else
  {
    DM_TRACE_WARN1("dmBisSmActBigSyncStart: syncHandle = %u not found", pBigSyncStart->syncHandle);
    status = HCI_ERR_UNKNOWN_ADV_ID;
  }

  if (status == HCI_SUCCESS)
  {
    HciBigCreateSync_t  createSync;

    /* build BIG parameters */
    createSync.bigHandle = bigHandle;
    createSync.syncHandle = pBigSyncStart->syncHandle;
    createSync.encrypt = dmBigSyncCb[bigHandle].encrypt;
    memcpy(createSync.bcstCode, dmBigSyncCb[bigHandle].bcastCode, HCI_BC_LEN);
    createSync.mse = pBigSyncStart->mse;
    createSync.bigSyncTimeout = pBigSyncStart->bigSyncTimeout;
    createSync.numBis = pBigSyncStart->numBis;
    memcpy(createSync.bis, pBigSyncStart->bis, pBigSyncStart->numBis);

    /* create BIG sync */
    HciLeBigCreateSyncCmd(&createSync);

    /* update BIG sync CB */
    dmBigSyncCb[bigHandle].syncHandle = pBigSyncStart->syncHandle;
    dmBigSyncCb[bigHandle].numBis = pBigSyncStart->numBis;
  }
  else
  {
    pMsg->hdr.status = status;

    /* notify app about failure */
    dmBisSmActBigSyncEstFailed(pBigSyncCb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop BIG sync for the given BIG.
 *
 *  \param  pBigSyncCb  BIG Sync control block.
 *  \param  pMsg        WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmBisSmActBigSyncStop(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg)
{
  HciLeBigTerminateSync((uint8_t) pMsg->hdr.param);
}

/*************************************************************************************************/
/*!
 *  \brief  Cancel creation of a BIG sync while it's pending.
 *
 *  \param  pBigSyncCb    Sync control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmBisSmActBigSyncCancelStart(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg)
{
  HciLeBigTerminateSync((uint8_t) pMsg->hdr.param);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a BIG sync established event from HCI.
 *
 *  \param  pBigSyncCb  BIG Sync control block.
 *  \param  pMsg        WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmBisSmActBigSyncEst(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg)
{
  /* save connection handles of BISes */
  pBigSyncCb->numBis = WSF_MIN(pMsg->bigSyncEst.numBis, DM_BIS_MAX);
  for (uint8_t i = 0; i < pBigSyncCb->numBis; i++)
  {
    pBigSyncCb->bisHandle[i] = pMsg->bigSyncEst.bisHandle[i];
  }

  /* TODO: create BIS HCI context. */

  pMsg->hdr.event = DM_BIG_SYNC_EST_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a BIG sync established failure event from HCI.
 *
 *  \param  pBigSyncCb  BIG Sync control block.
 *  \param  pMsg        WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmBisSmActBigSyncEstFailed(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg)
{
  /* reset BIG sync cb */
  dmBigSyncCbInit(pBigSyncCb);

  pMsg->hdr.event = DM_BIG_SYNC_EST_FAIL_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a BIG sync lost event from HCI.
 *
 *  \param  pBigSyncCb  BIG Sync control block.
 *  \param  pMsg        WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmBisSmActBigSyncLost(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg)
{
  /* reset BIG sync cb */
  dmBigSyncCbInit(pBigSyncCb);

  /* TODO: clear BIS HCI context. */

  pMsg->hdr.event = DM_BIG_SYNC_LOST_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a BIG terminate sync complete event from HCI.
 *
 *  \param  pBigSyncCb  BIG Sync control block.
 *  \param  pMsg        WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmBisSmActBigSyncStopped(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg)
{
  /* reset BIG sync cb */
  dmBigSyncCbInit(pBigSyncCb);

  pMsg->hdr.event = DM_BIG_SYNC_STOP_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Execute the DM BIS state machine.
 *
 *  \param  pBigSyncCb  BIG Sync control block.
 *  \param  pMsg        WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmBisSmExecute(dmBigSyncCb_t *pBigSyncCb, dmBisMsg_t *pMsg)
{
  uint8_t action;
  uint8_t event;

  DM_TRACE_INFO2("dmBisSmExecute event=%d state=%d", pMsg->hdr.event, pBigSyncCb->state);

  /* get the event */
  event = DM_MSG_MASK(pMsg->hdr.event);

  /* get action */
  action = dmBisStateTbl[pBigSyncCb->state][event][DM_BIS_ACTION];

  /* set next state */
  pBigSyncCb->state = dmBisStateTbl[pBigSyncCb->state][event][DM_BIS_NEXT_STATE];

  /* execute action function */
  (*dmBisAct[action])(pBigSyncCb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Reset the BIS module.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmBisReset(void)
{
  dmBigSyncCb_t         *pBigSyncCb = dmBigSyncCb;
  HciLeBigSyncLostEvt_t bigSyncLost;

  /* generate HCI BIG sync lost event */
  bigSyncLost.hdr.event = HCI_LE_BIG_SYNC_LOST_CBACK_EVT;
  bigSyncLost.hdr.status = HCI_SUCCESS;

  for (uint8_t i = DM_BIG_MAX; i > 0; i--, pBigSyncCb++)
  {
    if (pBigSyncCb->state != DM_BIS_SM_ST_IDLE)
    {
      /* set BIG sync handle */
      bigSyncLost.hdr.param = bigSyncLost.bigHandle = i;

      /* handle the event */
      dmBisHciHandler((hciEvt_t *) &bigSyncLost);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM BIS HCI event handler.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmBisHciHandler(hciEvt_t *pEvent)
{
  dmBigSyncCb_t *pBigSyncCb = dmBigSyncCbByHandle((uint8_t) pEvent->hdr.param);

  /* handle special cases for BIG sync established event */
  if (pEvent->hdr.event == HCI_LE_BIG_SYNC_EST_CBACK_EVT)
  {
    /* translate HCI event to state machine event */
    if (pEvent->hdr.status == HCI_SUCCESS)
    {
      pEvent->hdr.event = DM_BIS_MSG_HCI_LE_BIG_SYNC_EST;
    }
    else
    {
      pEvent->hdr.event = DM_BIS_MSG_HCI_LE_BIG_SYNC_EST_FAIL;
    }
  }
  else /* HCI_LE_BIG_SYNC_LOST_CBACK_EVT */
  {
    /* translate HCI event to state machine message */
    pEvent->hdr.event = DM_BIS_HCI_EVT_2_MSG(pEvent->hdr.event);
  }

  /* if scb found */
  if (pBigSyncCb != NULL)
  {
    /* execute state machine */
    dmBisSmExecute(pBigSyncCb, (dmBisMsg_t *) pEvent);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM BIG event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmBisMsgHandler(wsfMsgHdr_t *pMsg)
{
  dmBigSyncCb_t *pBigSyncCb;

  /* look up CB from BIG handle */
  if ((pBigSyncCb = dmBigSyncCbByHandle((uint8_t) pMsg->param)) != NULL)
  {
    /* execute state machine */
    dmBisSmExecute(pBigSyncCb, (dmBisMsg_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Synchronize to a Broadcast Isochronous Group (BIG) described in the periodic
 *          advertising train specified by the sync handle.
 *
 *  \param  bigHandle       BIG handle.
 *  \param  syncHandle      Periodic advertising train handle.
 *  \param  mse             Maximum number of subevents.
 *  \param  bigSyncTimeout  Synchronization timeout for the BIS, in the units of 10ms.
 *  \param  numBis          Total number of BISes in the BIG.
 *  \param  pBis            List of indices of BISes (in ascending order).
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmBigSyncStart(uint8_t bigHandle, uint16_t syncHandle, uint8_t mse, uint16_t bigSyncTimeout,
                    uint8_t numBis, uint8_t *pBis)
{
  dmBisApiBigSyncStart_t  *pMsg;

  WSF_ASSERT(bigHandle < DM_BIG_MAX);
  WSF_ASSERT((numBis > 0 ) && (numBis <= DM_BIS_MAX));

  if ((pMsg = WsfMsgAlloc(sizeof(dmBisApiBigSyncStart_t))) != NULL)
  {
    pMsg->hdr.param = bigHandle;
    pMsg->hdr.event = DM_BIS_MSG_API_BIG_SYNC_START;
    pMsg->syncHandle = syncHandle;
    pMsg->mse = mse;
    pMsg->bigSyncTimeout = bigSyncTimeout;
    pMsg->numBis = numBis;
    memcpy(pMsg->bis, pBis, numBis);

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop synchronizing or cancel the process of synchronizing to the Broadcast Isochronous
 *          Group (BIG) identified by the handle.
 *
 *  \note   The command also terminates the reception of BISes in the BIG specified in \ref
 *          DmBigSyncStart, destroys the associated connection handles of the BISes in the BIG
 *          and removes the data paths for all BISes in the BIG.
 *
 *  \param  bigHandle       BIG handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmBigSyncStop(uint8_t bigHandle)
{
  wsfMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(wsfMsgHdr_t))) != NULL)
  {
    pMsg->param = bigHandle;
    pMsg->event = DM_BIS_MSG_API_BIG_SYNC_STOP;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  For internal use only.  Return TRUE if the BIS sync is in use.
 *
 *  \param  handle  BIS connection handle.
 *
 *  \return TRUE if the BIS sync is in use, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t DmBisSyncInUse(uint16_t handle)
{
  dmBigSyncCb_t *pBigSyncCb = dmBigSyncCb;

  for (uint8_t i = DM_BIG_MAX; i > 0; i--, pBigSyncCb++)
  {
    if (pBigSyncCb->state == DM_BIS_SM_ST_IDLE)
    {
      continue;
    }

    for (uint8_t j = 0; j < pBigSyncCb->numBis; j++)
    {
      if (pBigSyncCb->bisHandle[j] == handle)
      {
        return TRUE;
      }
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Set the Broadcast Code for the given Broadcast Isochronous Group (BIG).
 *
 *  \param  bigHandle    BIG handle.
 *  \param  encrypt      FALSE (Unencrypted) or TRUE (Encrypted).
 *  \param  authen       FALSE (Unauthenticated) or TRUE (Authenticated).
 *  \param  pBcastCode   Broadcast code.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmBigSyncSetBcastCode(uint8_t bigHandle, bool_t encrypt, bool_t authen, uint8_t *pBcastCode)
{
  WSF_ASSERT(bigHandle < DM_BIG_MAX);

  WsfTaskLock();

  if (encrypt)
  {
    dmBigSyncCb[bigHandle].encrypt = TRUE;
    dmBigSyncCb[bigHandle].authen = authen;
    memcpy(dmBigSyncCb[bigHandle].bcastCode, pBcastCode, HCI_BC_LEN);
  }
  else
  {
    dmBigSyncCb[bigHandle].encrypt = FALSE;
    dmBigSyncCb[bigHandle].authen = FALSE;
    memset(dmBigSyncCb[bigHandle].bcastCode, 0x000, HCI_BC_LEN);
  }

  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set the security level of the LE Security Mode 3 for the given Broadcast Isochronous
 *          Group (BIG).
 *
 *  \param  bigHandle    BIG handle.
 *  \param  secLevel     Security level.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmBigSyncSetSecLevel(uint8_t bigHandle, uint8_t secLevel)
{
  WSF_ASSERT(bigHandle < DM_BIG_MAX);
  WSF_ASSERT(secLevel <= DM_SEC_LEVEL_BCAST_AUTH);

  WsfTaskLock();

  dmBigSyncCb[bigHandle].secLevel = secLevel;

  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Get the security level of the LE Security Mode 3 for the given Broadcast Isochronous
 *          Group (BIG) connection handle.
 *
 *  \param  handle  BIS connection handle.
 *
 *  \return Security level.
 */
/*************************************************************************************************/
uint8_t DmBigSyncGetSecLevel(uint16_t handle)
{
  dmBigSyncCb_t *pBigSyncCb = dmBigSyncCb;

  for (uint8_t i = DM_BIG_MAX; i > 0; i--, pBigSyncCb++)
  {
    if (pBigSyncCb->state == DM_BIS_SM_ST_IDLE)
    {
      continue;
    }

    for (uint8_t j = 0; j < pBigSyncCb->numBis; j++)
    {
      if (pBigSyncCb->bisHandle[j] == handle)
      {
        return pBigSyncCb->secLevel;
      }
    }
  }

  return DM_SEC_LEVEL_BCAST_NONE;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize DM BIS manager for operation as master.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmBisMasterInit(void)
{
  WsfTaskLock();

  dmBigSyncCb_t *pBigSyncCb = dmBigSyncCb;

  dmFcnIfTbl[DM_ID_BIS_SYNC] = (dmFcnIf_t *) &dmBisFcnIf;

  for (uint8_t i = DM_BIG_MAX; i > 0; i--, pBigSyncCb++)
  {
    dmBigSyncCbInit(pBigSyncCb);
  }

  HciSetLeSupFeat((HCI_LE_SUP_FEAT_ISO_SYNC_RECEIVER | HCI_LE_SUP_FEAT_ISO_HOST_SUPPORT), TRUE);

  WsfTaskUnlock();
}
