/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  DM Broadcast Isochronous Stream (BIS) management for slave.
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
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "wsf_msg.h"
#include "wsf_math.h"
#include "wsf_os.h"
#include "dm_api.h"
#include "dm_dev.h"
#include "dm_main.h"
#include "dm_adv.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! BIS event handler messages for state machine */
enum
{
  /* messages from API */
  DM_BIS_MSG_API_BIG_START = DM_MSG_START(DM_ID_BIS),   /*!< Start BIG */
  DM_BIS_MSG_API_BIG_STOP,                              /*!< Stop BIG */

  /* messages from HCI */
  DM_BIS_MSG_HCI_LE_CREATE_BIG_CMPL_FAIL,               /*!< HCI LE Create BIG Complete Failed Event */
  DM_BIS_MSG_HCI_LE_CREATE_BIG_CMPL,                    /*!< HCI LE Create BIG Complete Event */
  DM_BIS_MSG_HCI_LE_TERM_BIG_CMPL                       /*!< HCI LE Terminate BIG Complete Event */
};

/*! BIS state machine states */
enum
{
  DM_BIS_SM_ST_IDLE,                                    /*!< Idle State */
  DM_BIS_SM_ST_BIG_STARTING,                            /*!< BIG Starting State */
  DM_BIS_SM_ST_BIG_STARTED,                             /*!< BIG Started State */
  DM_BIS_SM_ST_BIG_STOPPING,                            /*!< BIG Stopping State */

  DM_BIS_SM_NUM_STATES
};

/*! BIS state machine actions */
enum
{
  DM_BIS_SM_ACT_NONE,                                   /*!< No Action */
  DM_BIS_SM_ACT_BIG_START,                              /*!< Process Start BIG */
  DM_BIS_SM_ACT_BIG_STOP,                               /*!< Process Stop BIG */
  DM_BIS_SM_ACT_BIG_CANCEL_START,                       /*!< Process Cancel Start BIG */
  DM_BIS_SM_ACT_BIG_STARTED,                            /*!< Process BIG Started */
  DM_BIS_SM_ACT_BIG_START_FAILED,                       /*!< Process BIG Start Failed */
  DM_BIS_SM_ACT_BIG_STOPPED                             /*!< Process BIG Stopped */
};

/*! Column position of next state */
#define DM_BIS_NEXT_STATE          0

/*! Column position of action */
#define DM_BIS_ACTION              1

/*! Number of columns in the state machine state tables */
#define DM_BIS_NUM_COLS            2

/* Number of BIS messages */
#define DM_BIS_NUM_MSGS            (DM_BIS_MSG_HCI_LE_TERM_BIG_CMPL - DM_BIS_MSG_API_BIG_START + 1)

/*! Translate HCI event to state machine message */
#define DM_BIS_HCI_EVT_2_MSG(evt)  (DM_BIS_MSG_HCI_LE_TERM_BIG_CMPL - HCI_LE_TERM_BIG_CMPL_CBACK_EVT + (evt))

/* Uninitialized HCI BIS handle */
#define DM_BIS_HCI_HANDLE_NONE     0xFFFF

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Data structure for DM_BIS_MSG_API_BIG_START */
typedef struct
{
  wsfMsgHdr_t                 hdr;                    /*!< \brief Header structure. */
  uint8_t                     advHandle;              /*!< \brief Used to identify the periodic advertising train. */
  uint8_t                     numBis;                 /*!< \brief Total number of BISes in the BIG. */
  uint32_t                    sduInterval;            /*!< \brief Interval, in microseconds, of BIG SDUs. */
  uint16_t                    maxSdu;                 /*!< \brief Maximum size of SDU. */
  uint16_t                    mtlMs;                  /*!< \brief Maximum time, in milliseconds, for transmitting SDU. */
  uint8_t                     rtn;                    /*!< \brief Retransmission number. */
} dmBisApiBigStart_t;

/* Data structure for DM_BIS_MSG_API_BIG_STOP */
typedef struct
{
  wsfMsgHdr_t                 hdr;                    /*!< \brief Header structure. */
  uint8_t                     reason;                 /*!< \brief Reason BIG is terminated. */
} dmBisApiBigStop_t;

/* Union of all DM BIS state machine messages */
typedef union
{
  wsfMsgHdr_t                 hdr;                    /*!< \brief Header structure. */
  dmBisApiBigStart_t          apiBigStart;            /*!< \brief API BIG start message. */
  dmBisApiBigStop_t           apiBigStop;             /*!< \brief API BIG stop message. */
  HciLeCreateBigCmplEvt_t     createBigCmpl;          /*!< \brief HCI create BIG complete event. */
  HciLeTerminateBigCmplEvt_t  terminateBigCmpl;       /*!< \brief HCI terminate BIG complete event. */
} dmBisMsg_t;

/* Control block for BIG */
typedef struct
{
  uint8_t                     state;                  /*!< \brief BIG state. */
  uint8_t                     advHandle;              /*!< \brief Periodic advertising train associated with BIG. */
  uint8_t                     phyBits;                /*!< \brief Transmitter PHYs of packets. */
  uint8_t                     packing;                /*!< \brief Packing scheme. */
  uint8_t                     framing;                /*!< \brief Format of BIS Data PDUs. */
  uint8_t                     secLevel;               /*!< \brief Security level. */
  bool_t                      encrypt;                /*!< \brief Unencrypted or Encrypted. */
  bool_t                      authen;                 /*!< \brief Unauthenticated or Authenticated. */
  uint8_t                     bcastCode[HCI_BC_LEN];  /*!< \brief Braodcast code. */

  /* BIS */
  uint8_t                     numBis;                 /*!< \brief Total number of BISes in the BIG. */
  //uint8_t                     bis[DM_BIS_MAX];        /*!< \brief List of indices of BISes (in ascending order). */
  uint16_t                    bisHandle[DM_BIS_MAX];  /*!< \brief BIS handles. */
} dmBigCb_t;

/*! Action function */
typedef void (*dmBisAct_t)(dmBigCb_t *pCcb, dmBisMsg_t *pMsg);

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void dmBisSmActNone(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigStart(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigStop(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigCancelStart(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigStarted(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigStartFailed(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg);
static void dmBisSmActBigStopped(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg);

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
    /* Event                          Next state                    Action */
    /* API_BIG_START */               {DM_BIS_SM_ST_BIG_STARTING,   DM_BIS_SM_ACT_BIG_START},
    /* API_BIG_STOP */                {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_NONE},
    /* HCI_LE_CREATE_BIG_CMPL_FAIL */ {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_NONE},
    /* HCI_LE_CREATE_BIG_CMPL */      {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_NONE},
    /* HCI_LE_TERM_BIG_CMPL */        {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_NONE}
  },
  /* Starting state */
  {
    /* Event                          Next state                    Action */
    /* API_BIG_START */               {DM_BIS_SM_ST_BIG_STARTING,   DM_BIS_SM_ACT_NONE},
    /* API_BIG_STOP */                {DM_BIS_SM_ST_BIG_STOPPING,   DM_BIS_SM_ACT_BIG_CANCEL_START},
    /* HCI_LE_CREATE_BIG_CMPL_FAIL */ {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_START_FAILED},
    /* HCI_LE_CREATE_BIG_CMPL */      {DM_BIS_SM_ST_BIG_STARTED,    DM_BIS_SM_ACT_BIG_STARTED},
    /* HCI_LE_TERM_BIG_CMPL */        {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_START_FAILED}
  },
  /* Started state */
  {
    /* Event                          Next state                    Action */
    /* API_BIG_START */               {DM_BIS_SM_ST_BIG_STARTED,    DM_BIS_SM_ACT_NONE},
    /* API_BIG_STOP */                {DM_BIS_SM_ST_BIG_STOPPING,   DM_BIS_SM_ACT_BIG_STOP},
    /* HCI_LE_CREATE_BIG_CMPL_FAIL */ {DM_BIS_SM_ST_BIG_STARTED,    DM_BIS_SM_ACT_NONE},
    /* HCI_LE_CREATE_BIG_CMPL */      {DM_BIS_SM_ST_BIG_STARTED,    DM_BIS_SM_ACT_NONE},
    /* HCI_LE_TERM_BIG_CMPL */        {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_STOPPED}
  },
  /* Stopping state */
  {
    /* Event                          Next state                    Action */
    /* API_BIG_START */               {DM_BIS_SM_ST_BIG_STOPPING,   DM_BIS_SM_ACT_NONE},
    /* API_BIG_STOP */                {DM_BIS_SM_ST_BIG_STOPPING,   DM_BIS_SM_ACT_NONE},
    /* HCI_LE_CREATE_BIG_CMPL_FAIL */ {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_START_FAILED},
    /* HCI_LE_CREATE_BIG_CMPL */      {DM_BIS_SM_ST_BIG_STOPPING,   DM_BIS_SM_ACT_BIG_STOP},
    /* HCI_LE_TERM_BIG_CMPL */        {DM_BIS_SM_ST_IDLE,           DM_BIS_SM_ACT_BIG_STOPPED}
  }
 };

/*! DM BIS action function table */
static const dmBisAct_t dmBisAct[] =
{
  dmBisSmActNone,
  dmBisSmActBigStart,
  dmBisSmActBigStop,
  dmBisSmActBigCancelStart,
  dmBisSmActBigStarted,
  dmBisSmActBigStartFailed,
  dmBisSmActBigStopped
};

/*! DM BIG component function interface */
static const dmFcnIf_t dmBisFcnIf =
{
  dmBisReset,
  dmBisHciHandler,
  dmBisMsgHandler
};

/*! DM BIG control block */
static dmBigCb_t dmBigCb[DM_BIG_MAX];

/*************************************************************************************************/
/*!
 *  \brief  Initialize the BIG CB for a given handle.
 *
 *  \param  pBigCb    BIG CB.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmBigCbInit(dmBigCb_t *pBigCb)
{
  /* set BIG default values */
  pBigCb->state = DM_BIS_SM_ST_IDLE;
  pBigCb->advHandle = DM_ADV_HCI_HANDLE_NONE;
  pBigCb->numBis = 0;
  pBigCb->phyBits = HCI_TRANS_PHY_LE_1M_BIT;
  pBigCb->packing = HCI_PACKING_SEQUENTIAL;
  pBigCb->framing = HCI_FRAMING_UNFRAMED;
  pBigCb->encrypt = FALSE;
  memset(pBigCb->bcastCode, 0x000, HCI_BC_LEN);

  pBigCb->numBis = 0;
  for (uint8_t i = 0; i < DM_BIS_MAX; i++)
  {
    pBigCb->bisHandle[i] = DM_BIS_HCI_HANDLE_NONE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Get the BIG CB for a given handle.
 *
 *  \param  bigHandle        BIG handle.
 *
 *  \return Pointer to BIG control block if BIG handle valid. NULL, otherwise.
 */
/*************************************************************************************************/
static dmBigCb_t *dmBigCbByHandle(uint8_t bigHandle)
{
  WSF_ASSERT(bigHandle < DM_BIG_MAX);

  if (bigHandle < DM_BIG_MAX)
  {
    return &dmBigCb[bigHandle];
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Check if the periodic advertising handle is associated with another BIG.
 *
 *  \param      advHandle       Periodic advertising handle.
 *
 *  \return     TRUE if periodic advertising handle is associated with another BIG. FALSE, otherwise.
 */
/*************************************************************************************************/
static bool_t dmBigPerAdvUsed(uint8_t advHandle)
{
  for (uint8_t i = 0; i < DM_BIG_MAX; i++)
  {
    dmBigCb_t *pBigCb = &dmBigCb[i];

    if ((pBigCb->state != DM_BIS_SM_ST_IDLE) && (pBigCb->advHandle == advHandle))
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
static uint8_t dmBigGetNumAvailBis(void)
{
  uint8_t cnt = 0;

  for (uint8_t i = 0; i < DM_BIG_MAX; i++)
  {
    dmBigCb_t *pBigCb = &dmBigCb[i];

    if (pBigCb->state != DM_BIS_SM_ST_IDLE)
    {
      cnt += pBigCb->numBis;
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
*  \param  pBigCb  CIG control block.
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
static void dmBisSmActNone(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Create BIG for the given BIG.
 *
 *  \param  pBigCb  CIG control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmBisSmActBigStart(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg)
{
  dmBisApiBigStart_t  *pBigStart = &pMsg->apiBigStart;
  uint8_t             bigHandle = (uint8_t) pMsg->hdr.param;
  uint8_t             status = HCI_SUCCESS;

  if (DmPerAdvEnabled(pBigStart->advHandle))
  {
    if (!dmBigPerAdvUsed(pBigStart->advHandle))
    {
      if ((pBigStart->numBis > 0) && (pBigStart->numBis <= DM_BIS_MAX))
      {
        if (pBigStart->numBis > dmBigGetNumAvailBis())
        {
          DM_TRACE_WARN0("dmBisSmActStart: insufficient BISes");
          status = HCI_ERR_REJ_RESOURCES;
        }
      }
      else
      {
        DM_TRACE_WARN2("dmBisSmActStart: invalid value for numBis = %u, validRange = 1..%u",
                       pBigStart->numBis, DM_BIS_MAX);
        status = HCI_ERR_UNSUP_FEAT;
      }
    }
    else
    {
      DM_TRACE_WARN0("dmBisSmActStart: periodic advertising train is associated with another BIG");
      status = HCI_ERR_UNKNOWN_ADV_ID;
    }
  }
  else
  {
    DM_TRACE_WARN0("dmBisSmActStart: advHandle does not identify a periodic advertising train");
    status = HCI_ERR_UNKNOWN_ADV_ID;
  }

  if (status == HCI_SUCCESS)
  {
    HciCreateBig_t  createBig;

    /* build BIG parameters */
    createBig.bigHandle = bigHandle;
    createBig.advHandle = pBigStart->advHandle;
    createBig.numBis = pBigStart->numBis;
    createBig.sduInterUsec = pBigStart->sduInterval;
    createBig.maxSdu = pBigStart->maxSdu;
    createBig.mtlMs = pBigStart->mtlMs;
    createBig.rtn = pBigStart->rtn;
    createBig.phys = dmBigCb[bigHandle].phyBits;
    createBig.packing = dmBigCb[bigHandle].packing;
    createBig.framing = dmBigCb[bigHandle].framing;
    createBig.encrypt = dmBigCb[bigHandle].encrypt;
    memcpy(createBig.bcstCode, dmBigCb[bigHandle].bcastCode, HCI_BC_LEN);

    /* create BIG */
    HciLeCreateBigCmd(&createBig);

    /* update BIG CB */
    dmBigCb[bigHandle].advHandle = pBigStart->advHandle;
    dmBigCb[bigHandle].numBis = pBigStart->numBis;
  }
  else
  {
    pMsg->hdr.status = status;

    /* notify app about failure */
    dmBisSmActBigStartFailed(pBigCb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Terminate BIG for the given BIG.
 *
 *  \param  pBigCb  CIG control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmBisSmActBigStop(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg)
{
  dmBisApiBigStop_t *pBigStop = &pMsg->apiBigStop;

  HciTerminateBigCmd((uint8_t) pMsg->hdr.param, pBigStop->reason);
}

/*************************************************************************************************/
/*!
*  \brief  Cancel a create BIG.
*
*  \param  pCcb    CIS connection control block.
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
static void dmBisSmActBigCancelStart(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg)
{
  dmBisApiBigStop_t *pBigStop = &pMsg->apiBigStop;

  HciTerminateBigCmd((uint8_t) pMsg->hdr.param, pBigStop->reason);
}

/*************************************************************************************************/
/*!
*  \brief  Handle a create BIG complete event from HCI.
*
*  \param  pBigCb  CIG control block.
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
static void dmBisSmActBigStarted(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg)
{
  /* save connection handles of BISes */
  pBigCb->numBis = WSF_MIN(pMsg->createBigCmpl.numBis, DM_BIS_MAX);
  for (uint8_t i = 0; i < pBigCb->numBis; i++)
  {
    pBigCb->bisHandle[i] = pMsg->createBigCmpl.bisHandle[i];
  }

  pMsg->hdr.event = DM_BIG_START_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
*  \brief  Handle a create BIG failure complete event from HCI.
*
*  \param  pBigCb  CIG control block.
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
static void dmBisSmActBigStartFailed(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg)
{
  /* reset BIG cb */
  dmBigCbInit(pBigCb);

  pMsg->hdr.event = DM_BIG_START_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
*  \brief  Handle a terminate BIG complete event from HCI.
*
*  \param  pBigCb  CIG control block.
*  \param  pMsg    WSF message.
*
*  \return None.
*/
/*************************************************************************************************/
static void dmBisSmActBigStopped(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg)
{
  /* reset BIG cb */
  dmBigCbInit(pBigCb);

  pMsg->hdr.event = DM_BIG_STOP_IND;
  (*dmCb.cback)((dmEvt_t *) pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Execute the DM BIG state machine.
 *
 *  \param  pBigCb  BIG control block.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmBisSmExecute(dmBigCb_t *pBigCb, dmBisMsg_t *pMsg)
{
  uint8_t action;
  uint8_t event;

  DM_TRACE_INFO2("dmBisSmExecute event=%d state=%d", pMsg->hdr.event, pBigCb->state);

  /* get the event */
  event = DM_MSG_MASK(pMsg->hdr.event);

  /* get action */
  action = dmBisStateTbl[pBigCb->state][event][DM_BIS_ACTION];

  /* set next state */
  pBigCb->state = dmBisStateTbl[pBigCb->state][event][DM_BIS_NEXT_STATE];

  /* execute action function */
  (*dmBisAct[action])(pBigCb, pMsg);
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
  dmBigCb_t                   *pBigCb = dmBigCb;
  HciLeTerminateBigCmplEvt_t  terminateBigCmpl;

  /* generate HCI BIG sync lost event */
  terminateBigCmpl.hdr.event = HCI_LE_TERM_BIG_CMPL_CBACK_EVT;
  terminateBigCmpl.hdr.status = HCI_SUCCESS;

  for (uint8_t i = DM_BIG_MAX; i > 0; i--, pBigCb++)
  {
    if (pBigCb->state != DM_BIS_SM_ST_IDLE)
    {
      /* set BIG sync handle */
      terminateBigCmpl.hdr.param = terminateBigCmpl.bigHandle = i;

      /* handle the event */
      dmBisHciHandler((hciEvt_t *) &terminateBigCmpl);
    }
  }
}

/*************************************************************************************************/
/*!
*  \brief  DM BIS HCI callback event handler.
*
*  \param  pEvent  Pointer to HCI callback event structure.
*
*  \return None.
*/
/*************************************************************************************************/
static void dmBisHciHandler(hciEvt_t *pEvent)
{
  dmBigCb_t *pBigCb = dmBigCbByHandle((uint8_t) pEvent->hdr.param);

  /* translate HCI event to state machine event */
  if (pEvent->hdr.event == HCI_LE_CREATE_BIG_CMPL_CBACK_EVT)
  {
    if (pEvent->hdr.status == HCI_SUCCESS)
    {
      pEvent->hdr.event =  DM_BIS_MSG_HCI_LE_CREATE_BIG_CMPL;
    }
    else
    {
      pEvent->hdr.event = DM_BIS_MSG_HCI_LE_CREATE_BIG_CMPL_FAIL;
    }
  }
  else /* HCI_LE_TERMINATE_BIG_CMPL_CBACK_EVT */
  {
    pEvent->hdr.event =  DM_BIS_MSG_HCI_LE_TERM_BIG_CMPL;
  }

  /* if BIG cb found */
  if (pBigCb != NULL)
  {
    /* execute state machine */
    dmBisSmExecute(pBigCb, (dmBisMsg_t *) pEvent);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM BIS WSF message handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmBisMsgHandler(wsfMsgHdr_t *pMsg)
{
  dmBigCb_t *pBigCb;

  /* look up CB from BIG handle */
  if ((pBigCb = dmBigCbByHandle((uint8_t) pMsg->param)) != NULL)
  {
    /* execute state machine */
    dmBisSmExecute(pBigCb, (dmBisMsg_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Start a Broadcast Isochronous Group (BIG) with one or more Broadcast Isochronous 
 *          Streams (BISes).
 *
 *  \param  bigHandle     BIG identifier.
 *  \param  advHandle     Used to identify the periodic advertising train.
 *  \param  numBis;       Total number of BISes in the BIG.
 *  \param  sduInterval   Interval, in microseconds, of BIG SDUs.
 *  \param  maxSdu        Maximum size of SDU.
 *  \param  mtlMs         Maximum time, in milliseconds, for transmitting SDU.
 *  \param  rtn           Retransmission number.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmBigStart(uint8_t bigHandle, uint8_t advHandle, uint8_t numBis, uint32_t sduInterval,
                uint16_t maxSdu, uint16_t mtlMs, uint8_t rtn)
{
  dmBisApiBigStart_t *pMsg;

  WSF_ASSERT(bigHandle < DM_BIG_MAX);
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);
  WSF_ASSERT((numBis > 0 ) && (numBis <= DM_BIS_MAX));

  if ((pMsg = WsfMsgAlloc(sizeof(dmBisApiBigStart_t))) != NULL)
  {
    pMsg->hdr.event = DM_BIS_MSG_API_BIG_START;
    pMsg->hdr.param = bigHandle;
    pMsg->advHandle = advHandle;
    pMsg->numBis = numBis;
    pMsg->advHandle = advHandle;
    pMsg->numBis = numBis;
    pMsg->sduInterval = sduInterval;
    pMsg->maxSdu = maxSdu;
    pMsg->mtlMs = mtlMs;
    pMsg->rtn = rtn;

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop a Broadcast Isochronous Group (BIG) identified for the given handle.
 *
 *  \param  bigHandle   BIG identifier.
 *  \param  reason      Reason BIG is terminated.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmBigStop(uint8_t bigHandle, uint8_t reason)
{
  dmBisApiBigStop_t *pMsg;

  WSF_ASSERT(bigHandle < DM_BIG_MAX);

  if ((pMsg = WsfMsgAlloc(sizeof(dmBisApiBigStop_t))) != NULL)
  {
    pMsg->hdr.event = DM_BIS_MSG_API_BIG_STOP;
    pMsg->hdr.param = bigHandle;
    pMsg->reason = reason;

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  For internal use only.  Return TRUE if the BIS is in use.
 *
 *  \param  handle  BIS connection handle.
 *
 *  \return TRUE if the BIS is in use, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t DmBisInUse(uint16_t handle)
{
  dmBigCb_t *pBigCb = dmBigCb;

  for (uint8_t i = DM_BIG_MAX; i > 0; i--, pBigCb++)
  {
    if (pBigCb->state == DM_BIS_SM_ST_IDLE)
    {
      continue;
    }

    for (uint8_t j = 0; j < pBigCb->numBis; j++)
    {
      if (pBigCb->bisHandle[j] == handle)
      {
        return TRUE;
      }
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Set the PHYs used for transmission of PDUs of Broadcast Isochronous Streams (BISes) in
 *          Broadcast Isochronous Group (BIG).
 *
 *  \param  bigHandle   BIG handle.
 *  \param  phyBits     PHY bit field.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmBigSetPhy(uint8_t bigHandle, uint8_t phyBits)
{
  const uint8_t MIN_PHY = 0x01;
  const uint8_t MAX_PHY = 0x07;

  WSF_ASSERT(bigHandle < DM_BIG_MAX);
  WSF_ASSERT((phyBits >= MIN_PHY) && (phyBits <= MAX_PHY))

  WsfTaskLock();

  dmBigCb[bigHandle].phyBits = phyBits;

  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set the packing scheme and framing format for the given Broadcast Isochronous Group
 *          (BIG).
 *
 *  \param  bigHandle   BIG handle.
 *  \param  packing     Packing scheme.
 *  \param  framing     Indicates format of BIS Data PDUs.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmBigSetPackingFraming(uint8_t bigHandle, uint8_t packing, uint32_t framing)
{
  WSF_ASSERT(bigHandle < DM_BIG_MAX);

  WsfTaskLock();

  dmBigCb[bigHandle].packing = packing;
  dmBigCb[bigHandle].framing = framing;

  WsfTaskUnlock();
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
void DmBigSetBcastCode(uint8_t bigHandle, bool_t encrypt, bool_t authen, uint8_t *pBcastCode)
{
  WSF_ASSERT(bigHandle < DM_BIG_MAX);

  WsfTaskLock();

  if (encrypt)
  {
    dmBigCb[bigHandle].encrypt = TRUE;
    dmBigCb[bigHandle].authen = authen;
    memcpy(dmBigCb[bigHandle].bcastCode, pBcastCode, HCI_BC_LEN);
  }
  else
  {
    dmBigCb[bigHandle].encrypt = FALSE;
    dmBigCb[bigHandle].authen = FALSE;
    memset(dmBigCb[bigHandle].bcastCode, 0x000, HCI_BC_LEN);
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
void DmBigSetSecLevel(uint8_t bigHandle, uint8_t secLevel)
{
  WSF_ASSERT(bigHandle < DM_BIG_MAX);
  WSF_ASSERT(secLevel <= DM_SEC_LEVEL_BCAST_AUTH);

  WsfTaskLock();

  dmBigCb[bigHandle].secLevel = secLevel;

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
uint8_t DmBigGetSecLevel(uint16_t handle)
{
  dmBigCb_t *pBigCb = dmBigCb;

  for (uint8_t i = DM_BIG_MAX; i > 0; i--, pBigCb++)
  {
    if (pBigCb->state == DM_BIS_SM_ST_IDLE)
    {
      continue;
    }

    for (uint8_t j = 0; j < pBigCb->numBis; j++)
    {
      if (pBigCb->bisHandle[j] == handle)
      {
        return pBigCb->secLevel;
      }
    }
  }

  return DM_SEC_LEVEL_BCAST_NONE;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize DM BIS manager for operation as slave.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmBisSlaveInit(void)
{
  WsfTaskLock();

  dmBigCb_t *pBigCb = dmBigCb;

  dmFcnIfTbl[DM_ID_BIS] = (dmFcnIf_t *) &dmBisFcnIf;

  for (uint8_t i = DM_BIG_MAX; i > 0; i--, pBigCb++)
  {
    dmBigCbInit(pBigCb);
  }

  HciSetLeSupFeat((HCI_LE_SUP_FEAT_ISO_BROADCASTER | HCI_LE_SUP_FEAT_ISO_HOST_SUPPORT), TRUE);

  WsfTaskUnlock();
}
