/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  DM Isochronous (ISO) data path management.
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
#include "util/bstream.h"
#include "dm_api.h"
#include "dm_main.h"
#include "dm_conn.h"
#include "dm_cis.h"
#include "hci_core_iso.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Column position of next state */
#define DM_ISO_NEXT_STATE           0

/*! Column position of action */
#define DM_ISO_ACTION               1

/*! Number of columns in the state machine state tables */
#define DM_ISO_NUM_COLS             2

/*!  Data path direction none */
#define DM_ISO_DATA_DIR_NONE        0xFF

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! DM ISO event handler messages for state machine */
enum
{
  /* messages from API */
  DM_ISO_MSG_API_SETUP = DM_MSG_START(DM_ID_ISO),       /*!< Setup ISO Data Path */
  DM_ISO_MSG_API_REMOVE,                                /*!< Remove ISO Data Path */

  /* messages from HCI */
  DM_ISO_MSG_HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL_FAIL,  /*!< HCI LE Set ISO Data Path Command Complete Failure Event */
  DM_ISO_MSG_HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL,       /*!< HCI LE Set ISO Data Path Command Complete Event */
  DM_ISO_MSG_HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL_FAIL, /*!< HCI LE Remove ISO Data Path Command Complete Failure Event */
  DM_ISO_MSG_HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL       /*!< HCI LE Remove ISO Data Path Command Complete Event */
};

/* Number of ISO messages */
#define DM_ISO_NUM_MSGS             (DM_ISO_MSG_HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL - DM_ISO_MSG_API_SETUP + 1)

/*! CIS CIG state machine actions */
enum
{
  DM_ISO_SM_ACT_NONE,                                   /*!< No Action */
  DM_ISO_SM_ACT_SETUP,                                  /*!< Process Setup */
  DM_ISO_SM_ACT_SETUP_SUCCESS,                          /*!< Process Setup Successful */
  DM_ISO_SM_ACT_SETUP_FAILED,                           /*!< Process Setup Failed */
  DM_ISO_SM_ACT_REMOVE,                                 /*!< Process Remove */
  DM_ISO_SM_ACT_REMOVED,                                /*!< Process Removed */
  DM_ISO_SM_ACT_REMOVE_FAILED,                          /*!< Process Remove Failed */
};

/*! CIS CIG state machine states */
enum
{
  DM_ISO_SM_ST_IDLE,                                    /*!< Idle State */
  DM_ISO_SM_ST_CONFIGING,                               /*!< Configuring State */
  DM_ISO_SM_ST_CONFIGED,                                /*!< Configured State */
  DM_ISO_SM_ST_REMOVING,                                /*!< Removing State */

  DM_ISO_SM_NUM_STATES
};

/* Data structure for DM_ISO_MSG_API_SETUP */
typedef struct
{
  wsfMsgHdr_t                         hdr;
  HciIsoSetupDataPath_t               dataPathParam;
} dmIsoApiSetup_t;

/* Data structure for DM_ISO_MSG_API_REMOVE */
typedef struct
{
  wsfMsgHdr_t                         hdr;
  uint8_t                             directionBits;
} dmIsoApiRemove_t;

/*! Union of all DM ISO state machine messages */
typedef union
{
  wsfMsgHdr_t                         hdr;

  /* API messages */
  dmIsoApiSetup_t                     apiSetup;
  dmIsoApiRemove_t                    apiRemove;

  /* HCI LE events */
  hciLeSetupIsoDataPathCmdCmplEvt_t   hciLeSetupIsoDataPathCmdCmpl;
  hciLeRemoveIsoDataPathCmdCmplEvt_t  hciLeRemoveIsoDataPathCmdCmpl;
} dmIsoMsg_t;

/*! ISO connection control block */
typedef struct
{
  uint16_t                            handle;                   /*!< Connection handle of CIS or BIS. */
  uint8_t                             state[DM_ISO_NUM_DIR];    /*!< Main state for each direction. */
  uint8_t                             dpDir;                    /*!< Direction being setup. */
  uint8_t                             directionBits;            /*!< Directions being removed. */
  uint8_t                             inUse;                    /*!< TRUE if entry in use. */
} dmIsoConnCb_t;

/*! ISO control block */
typedef struct
{
  dmIsoConnCb_t                       ccb[DM_ISO_DATA_PATH_MAX];/*!< ISO connection control block. */
  hciIsoCback_t                       cisCback;                 /*!< ISO CIS callback. */
  hciIsoCback_t                       bisCback;                 /*!< ISO BIS callback. */
  dmIsoDataPathSetupCback_t           setupCback;               /*!< ISO data path setup callback. */
  dmIsoDataPathRemoveCback_t          removeCback;              /*!< ISO data path remove callback. */
} dmIsoCb_t;

/*! Action functions */
typedef void (*dmIsoAct_t)(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState);

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void dmIsoReset(void);
static void dmIsoMsgHandler(wsfMsgHdr_t *pMsg);
static void dmIsoHciHandler(hciEvt_t *pEvent);

static void dmIsoSmActNone(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState);
static void dmIsoSmActSetup(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState);
static void dmIsoSmActSetupSuccess(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState);
static void dmIsoSmActSetupFailed(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState);
static void dmIsoSmActRemove(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState);
static void dmIsoSmActRemoved(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState);
static void dmIsoSmActRemoveFailed(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! DM ISO state machine state tables */
static const uint8_t dmIsoStateTbl[DM_ISO_SM_NUM_STATES][DM_ISO_NUM_MSGS][DM_ISO_NUM_COLS] =
{
  /* Idle state */
  {
  /* Event                                        Next state                Action */
  /* API_SETUP */                                 {DM_ISO_SM_ST_CONFIGING,  DM_ISO_SM_ACT_SETUP},
  /* API_REMOVE */                                {DM_ISO_SM_ST_IDLE,       DM_ISO_SM_ACT_NONE},
  /* HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL_FAIL */  {DM_ISO_SM_ST_IDLE,       DM_ISO_SM_ACT_NONE},
  /* HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL */       {DM_ISO_SM_ST_CONFIGED,   DM_ISO_SM_ACT_SETUP_SUCCESS},
  /* HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL_FAIL */ {DM_ISO_SM_ST_IDLE,       DM_ISO_SM_ACT_NONE},
  /* HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL */      {DM_ISO_SM_ST_IDLE,       DM_ISO_SM_ACT_NONE}
  },
  /* Configuring state */
  {
  /* Event                                        Next state                Action */
  /* API_SETUP */                                 {DM_ISO_SM_ST_CONFIGING,  DM_ISO_SM_ACT_NONE},
  /* API_REMOVE */                                {DM_ISO_SM_ST_CONFIGING,  DM_ISO_SM_ACT_NONE},
  /* HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL_FAIL */  {DM_ISO_SM_ST_IDLE,       DM_ISO_SM_ACT_SETUP_FAILED},
  /* HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL */       {DM_ISO_SM_ST_CONFIGED,   DM_ISO_SM_ACT_SETUP_SUCCESS},
  /* HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL_FAIL */ {DM_ISO_SM_ST_CONFIGING,  DM_CIS_SM_ACT_NONE},
  /* HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL */      {DM_ISO_SM_ST_IDLE,       DM_ISO_SM_ACT_SETUP_FAILED}
  },
  /* Configured state */
  {
  /* Event                                        Next state                Action */
  /* API_SETUP */                                 {DM_ISO_SM_ST_CONFIGED,   DM_ISO_SM_ACT_SETUP},
  /* API_REMOVE */                                {DM_ISO_SM_ST_REMOVING,   DM_ISO_SM_ACT_REMOVE},
  /* HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL_FAIL */  {DM_ISO_SM_ST_CONFIGED,   DM_ISO_SM_ACT_SETUP_FAILED},
  /* HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL */       {DM_ISO_SM_ST_CONFIGED,   DM_ISO_SM_ACT_SETUP_SUCCESS},
  /* HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL_FAIL */ {DM_ISO_SM_ST_CONFIGED,   DM_ISO_SM_ACT_SETUP_FAILED},
  /* HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL */      {DM_ISO_SM_ST_IDLE,       DM_ISO_SM_ACT_REMOVED}
  },
  /* Removing state */
  {
  /* Event                                        Next state                Action */
  /* API_SETUP */                                 {DM_ISO_SM_ST_REMOVING,   DM_ISO_SM_ACT_NONE},
  /* API_REMOVE */                                {DM_ISO_SM_ST_REMOVING,   DM_ISO_SM_ACT_NONE},
  /* HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL_FAIL */  {DM_ISO_SM_ST_REMOVING,   DM_ISO_SM_ACT_NONE},
  /* HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL */       {DM_ISO_SM_ST_REMOVING,   DM_ISO_SM_ACT_REMOVE},
  /* HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL_FAIL */ {DM_ISO_SM_ST_CONFIGED,   DM_ISO_SM_ACT_REMOVE_FAILED},
  /* HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL */      {DM_ISO_SM_ST_IDLE,       DM_ISO_SM_ACT_REMOVED}
  }
};

/*! DM ISO action function table */
static const dmIsoAct_t dmIsoAct[] =
{
  dmIsoSmActNone,
  dmIsoSmActSetup,
  dmIsoSmActSetupSuccess,
  dmIsoSmActSetupFailed,
  dmIsoSmActRemove,
  dmIsoSmActRemoved,
  dmIsoSmActRemoveFailed
};

/* ISO component function interface */
static const dmFcnIf_t dmIsoFcnIf =
{
  dmIsoReset,
  dmIsoHciHandler,
  dmIsoMsgHandler
};

/*! DM ISO control block */
static dmIsoCb_t dmIsoCb;

/*************************************************************************************************/
/*!
 *  \brief  Set an ISO data path setup event.
 *
 *  \param  pIsoCcb   CIG control block.
 *  \param  pMsg     WSF message.
 *  \param  pEvt     Event to be set.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoSetDataPathSetupEvt(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg,
                                     dmSetupIsoDataPathEvt_t *pEvt)
{
  pEvt->hdr.event = DM_ISO_DATA_PATH_SETUP_IND;
  pEvt->hdr.param = pMsg->hdr.param;
  pEvt->hdr.status = pMsg->hdr.status;

  pEvt->handle = pMsg->hciLeSetupIsoDataPathCmdCmpl.handle;
  pEvt->status = pMsg->hciLeSetupIsoDataPathCmdCmpl.status;
  pEvt->dpDir = pIsoCcb->dpDir;
}

/*************************************************************************************************/
/*!
 *  \brief  Set an ISO data path remove event.
 *
 *  \param  pIsoCcb   CIG control block.
 *  \param  pMsg     WSF message.
 *  \param  pEvt     Event to be set.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoSetDataPathRemoveEvt(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg,
                                      dmRemoveIsoDataPathEvt_t *pEvt)
{
  pEvt->hdr.event = DM_ISO_DATA_PATH_REMOVE_IND;
  pEvt->hdr.param = pMsg->hdr.param;
  pEvt->hdr.status = pMsg->hdr.status;

  pEvt->handle = pMsg->hciLeRemoveIsoDataPathCmdCmpl.handle;
  pEvt->status = pMsg->hciLeRemoveIsoDataPathCmdCmpl.status;
  pEvt->directionBits = pIsoCcb->directionBits;
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a DM ISO control block.
 *
 *  \param  handle   Connection handle of CIS or BIS.
 *
 *  \return Pointer to ISO control block or NULL if failure.
 */
/*************************************************************************************************/
dmIsoConnCb_t *dmIsoCbAlloc(uint16_t handle)
{
  dmIsoConnCb_t *pIsoCcb = dmIsoCb.ccb;
  uint8_t       i;

  for (i = 0; i < DM_ISO_DATA_PATH_MAX; i++, pIsoCcb++)
  {
    if (pIsoCcb->inUse == FALSE)
    {
      memset(pIsoCcb, 0, sizeof(dmIsoConnCb_t));

      pIsoCcb->handle = handle;
      pIsoCcb->inUse = TRUE;
      pIsoCcb->dpDir = DM_ISO_DATA_DIR_NONE;

      DM_TRACE_ALLOC1("dmIsoCbAlloc %d", handle);

      return pIsoCcb;
    }
  }

  DM_TRACE_ERR0("dmIsoCbAlloc failed");

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Deallocate a DM ISO control block.
 *
 *  \param  pIsoCcb   ISO control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmIsoCbDealloc(dmIsoConnCb_t *pIsoCcb)
{
  DM_TRACE_FREE1("dmIsoCbDealloc %d", pIsoCcb->handle);

  pIsoCcb->inUse = FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Find a ISO data path control block with matching handle.
 *
 *  \param  handle     Connection handle of CIS or BIS.
 *
 *  \return Pointer to ISO control block. NULL if not found.
 */
/*************************************************************************************************/
dmIsoConnCb_t *dmIsoCbByHandle(uint16_t handle)
{
  dmIsoConnCb_t   *pIsoCcb = dmIsoCb.ccb;
  uint8_t      i;

  for (i = DM_ISO_DATA_PATH_MAX; i > 0; i--, pIsoCcb++)
  {
    if (pIsoCcb->inUse && (pIsoCcb->handle == handle))
    {
      return pIsoCcb;
    }
  }

  DM_TRACE_WARN1("dmIsoCcbByHandle not found %d", handle);

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Empty action.
 *
 *  \param  pIsoCcb   CIG control block.
 *  \param  pMsg     WSF message.
 *  \param  oldState Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoSmActNone(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Setup ISO data path for the given connection handle.
 *
 *  \param  pIsoCcb   CIG control block.
 *  \param  pMsg     WSF message.
 *  \param  oldState Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoSmActSetup(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState)
{
  dmIsoApiSetup_t *pSetup = &pMsg->apiSetup;

  /* if connection handle is for CIS, BIS or BIS Sync */
  if (DmCisConnInUse(pIsoCcb->handle) || DmBisInUse(pIsoCcb->handle) || DmBisSyncInUse(pIsoCcb->handle))
  {
    /* setup ISO data path */
    HciLeSetupIsoDataPathCmd(&pSetup->dataPathParam);

    /* if also need to setup ISO data path in the Host */
    if (dmIsoCb.setupCback != NULL)
    {
      (*dmIsoCb.setupCback)(&pSetup->dataPathParam);
    }

    /* if no pending setup command */
    if (pIsoCcb->dpDir == DM_ISO_DATA_DIR_NONE)
    {
      /* save data path direction */
      pIsoCcb->dpDir = pSetup->dataPathParam.dpDir;
    }
    /* else direction will be set when command complete received */
  }
  else
  {
    DM_TRACE_WARN1("dmIsoSmActSetup: connection handle (%d) isn't for CIS or BIS", pIsoCcb->handle);

    pMsg->hdr.status = HCI_ERR_UNKNOWN_HANDLE;

    /* restore old state */
    pIsoCcb->state[pSetup->dataPathParam.dpDir] = oldState;

    /* notify app about failure */
    dmIsoSmActSetupFailed(pIsoCcb, pMsg, oldState);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a ISO data path setup successful event from HCI.
 *
 *  \param  pIsoCcb   CIG control block.
 *  \param  pMsg     WSF message.
 *  \param  oldState Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoSmActSetupSuccess(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState)
{
  dmSetupIsoDataPathEvt_t evt;

  dmIsoSetDataPathSetupEvt(pIsoCcb, pMsg, &evt);

  (*dmCb.cback)((dmEvt_t *) &evt);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a ISO data path setup failure event from HCI.
 *
 *  \param  pIsoCcb   CIG control block.
 *  \param  pMsg     WSF message.
 *  \param  oldState Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoSmActSetupFailed(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState)
{
  dmSetupIsoDataPathEvt_t evt;

  dmIsoSetDataPathSetupEvt(pIsoCcb, pMsg, &evt);

  /* if both data path directions in idle state */
  if ((pIsoCcb->state[HCI_ISO_DATA_DIR_INPUT]  == DM_ISO_SM_ST_IDLE) &&
      (pIsoCcb->state[HCI_ISO_DATA_DIR_OUTPUT] == DM_ISO_SM_ST_IDLE))
  {
    dmIsoCbDealloc(pIsoCcb);
  }

  (*dmCb.cback)((dmEvt_t *) &evt);
}

/*************************************************************************************************/
/*!
 *  \brief  Remove ISO data path for the given connection handle.
 *
 *  \param  pIsoCcb   CIG control block.
 *  \param  pMsg     WSF message.
 *  \param  oldState Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoSmActRemove(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState)
{
  dmIsoApiRemove_t *pRemove = &pMsg->apiRemove;

  /* if remove data path request hasn't been sent down for both directions */
  if (pIsoCcb->directionBits != (HCI_ISO_DATA_PATH_INPUT_BIT | HCI_ISO_DATA_PATH_OUTPUT_BIT))
  {
    /* remove ISO data path */
    HciLeRemoveIsoDataPathCmd(pMsg->hdr.param, pRemove->directionBits);

    /* if also need to remove ISO data path in the Host */
    if (dmIsoCb.removeCback != NULL)
    {
      (*dmIsoCb.removeCback)(pMsg->hdr.param, pRemove->directionBits);
    }

    /* if no pending remove command */
    if (pIsoCcb->directionBits == 0)
    {
      /* save data path directions */
      pIsoCcb->directionBits = pRemove->directionBits;
    }
    /* else direction will be set when command complete received */
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a ISO data path removed event from HCI.
 *
 *  \param  pIsoCcb   CIG control block.
 *  \param  pMsg     WSF message.
 *  \param  oldState Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoSmActRemoved(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState)
{
  /* if remove data path event hasn't been sent up for this handle */
  if (pIsoCcb->directionBits != 0)
  {
    dmRemoveIsoDataPathEvt_t evt;

    dmIsoSetDataPathRemoveEvt(pIsoCcb, pMsg, &evt);

    (*dmCb.cback)((dmEvt_t *) &evt);

    pIsoCcb->directionBits = 0;
  }

  /* if both data path directions in idle state */
  if ((pIsoCcb->state[HCI_ISO_DATA_DIR_INPUT]  == DM_ISO_SM_ST_IDLE) &&
      (pIsoCcb->state[HCI_ISO_DATA_DIR_OUTPUT] == DM_ISO_SM_ST_IDLE))
  {
    dmIsoCbDealloc(pIsoCcb);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Handle a ISO data path remove failure event from HCI.
 *
 *  \param  pIsoCcb   CIG control block.
 *  \param  pMsg     WSF message.
 *  \param  oldState Old state.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoSmActRemoveFailed(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t oldState)
{
  if (pIsoCcb->directionBits != 0)
  {
    dmRemoveIsoDataPathEvt_t evt;

    dmIsoSetDataPathRemoveEvt(pIsoCcb, pMsg, &evt);

    (*dmCb.cback)((dmEvt_t *) &evt);

    pIsoCcb->directionBits = 0;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Execute the DM ISO state machine.
 *
 *  \param  pIsoCcb     ISO control block.
 *  \param  pMsg       WSF message.
 *  \param  direction  Data path direction.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmIsoSmExecute(dmIsoConnCb_t *pIsoCcb, dmIsoMsg_t *pMsg, uint8_t direction)
{
  uint8_t action;
  uint8_t event;
  uint8_t state = pIsoCcb->state[direction];

  DM_TRACE_INFO2("dmIsoSmExecute event=%d state=%d", pMsg->hdr.event, state);

  /* get the event */
  event = DM_MSG_MASK(pMsg->hdr.event);

  /* get action */
  action = dmIsoStateTbl[state][event][DM_ISO_ACTION];

  /* set next state */
  pIsoCcb->state[direction] = dmIsoStateTbl[state][event][DM_ISO_NEXT_STATE];

  /* execute action function */
  (*dmIsoAct[action])(pIsoCcb, pMsg, state);
}

/*************************************************************************************************/
/*!
 *  \brief  Reset the OSI module.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoReset(void)
{
  dmIsoConnCb_t  *pIsoCcb = dmIsoCb.ccb;
  uint8_t    i;

  for (i = 0; i < DM_ISO_DATA_PATH_MAX; i++, pIsoCcb++)
  {
    if (pIsoCcb->inUse)
    {
      dmIsoCbDealloc(pIsoCcb);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM ISO WSF message handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoMsgHandler(wsfMsgHdr_t *pMsg)
{
  dmIsoConnCb_t *pIsoCcb;

  /* look up cb from handle */
  if ((pIsoCcb = dmIsoCbByHandle(pMsg->param)) != NULL)
  {
    if (pMsg->event == DM_ISO_MSG_API_SETUP)
    {
      /* execute state machine */
      dmIsoSmExecute(pIsoCcb, (dmIsoMsg_t *) pMsg, ((dmIsoApiSetup_t *) pMsg)->dataPathParam.dpDir);
    }
    else /* DM_ISO_MSG_API_REMOVE */
    {
      uint8_t directionBits = ((dmIsoApiRemove_t *) pMsg)->directionBits;

      /* if input direction being removed */
      if (directionBits & HCI_ISO_DATA_PATH_INPUT_BIT)
      {
        /* execute state machine */
        dmIsoSmExecute(pIsoCcb, (dmIsoMsg_t *) pMsg, HCI_ISO_DATA_DIR_INPUT);
      }

      /* if output direction being removed */
      if (directionBits & HCI_ISO_DATA_PATH_OUTPUT_BIT)
      {
        /* execute state machine */
        dmIsoSmExecute(pIsoCcb, (dmIsoMsg_t *) pMsg, HCI_ISO_DATA_DIR_OUTPUT);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM ISO data path HCI callback event handler.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoDpHciHandler(hciEvt_t *pEvent)
{
  dmIsoConnCb_t *pIsoCcb;

  /* look up cb from handle */
  if ((pIsoCcb = dmIsoCbByHandle(pEvent->hdr.param)) != NULL)
  {
    /* translate HCI event to state machine event */
    if (pEvent->hdr.event == HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL_CBACK_EVT)
    {
      if (pEvent->hdr.status == HCI_SUCCESS)
      {
        pEvent->hdr.event =  DM_ISO_MSG_HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL;
      }
      else
      {
        pEvent->hdr.event = DM_ISO_MSG_HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL_FAIL;
      }

      /* execute state machine */
      dmIsoSmExecute(pIsoCcb, (dmIsoMsg_t *) pEvent, pIsoCcb->dpDir);

      /* if there's pending setup command for other direction */
      if (pIsoCcb->state[HCI_ISO_DATA_DIR_INPUT] == DM_ISO_SM_ST_CONFIGING)
      {
        pIsoCcb->dpDir = HCI_ISO_DATA_DIR_INPUT;
      }
      else if (pIsoCcb->state[HCI_ISO_DATA_DIR_OUTPUT] == DM_ISO_SM_ST_CONFIGING)
      {
        pIsoCcb->dpDir = HCI_ISO_DATA_DIR_OUTPUT;
      }
      else
      {
        pIsoCcb->dpDir = DM_ISO_DATA_DIR_NONE;
      }
    }
    else /* HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL_CBACK_EVT */
    {
      uint8_t directionBits = pIsoCcb->directionBits;

      if (pEvent->hdr.status == HCI_SUCCESS)
      {
        pEvent->hdr.event =  DM_ISO_MSG_HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL;
      }
      else
      {
        pEvent->hdr.event = DM_ISO_MSG_HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL_FAIL;
      }

      /* if input direction being removed */
      if (directionBits & HCI_ISO_DATA_PATH_INPUT_BIT)
      {
        /* execute state machine */
        dmIsoSmExecute(pIsoCcb, (dmIsoMsg_t *) pEvent, HCI_ISO_DATA_DIR_INPUT);
      }

      /* if output direction being removed */
      if (directionBits & HCI_ISO_DATA_PATH_OUTPUT_BIT)
      {
        /* execute state machine */
        dmIsoSmExecute(pIsoCcb, (dmIsoMsg_t *) pEvent, HCI_ISO_DATA_DIR_OUTPUT);
      }

      /* if there's pending remove command for other direction */
      if (pIsoCcb->state[HCI_ISO_DATA_DIR_INPUT] == DM_ISO_SM_ST_REMOVING)
      {
        pIsoCcb->directionBits = HCI_ISO_DATA_PATH_INPUT_BIT;
      }
      else if (pIsoCcb->state[HCI_ISO_DATA_DIR_OUTPUT] == DM_ISO_SM_ST_REMOVING)
      {
        pIsoCcb->directionBits = HCI_ISO_DATA_PATH_OUTPUT_BIT;
      }
      else
      {
        pIsoCcb->directionBits = 0;
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM ISO HCI callback event handler.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoHciHandler(hciEvt_t *pEvent)
{
  /* if ISO data path event */
  if ((pEvent->hdr.event == HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL_CBACK_EVT) ||
      (pEvent->hdr.event == HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL_CBACK_EVT))
  {
    dmIsoDpHciHandler(pEvent);
  }
  else
  {
    /* handle incoming event */
    switch (pEvent->hdr.event)
    {
    case HCI_CONFIG_DATA_PATH_CMD_CMPL_CBACK_EVT:
      pEvent->hdr.event = DM_DATA_PATH_CONFIG_IND;
      break;

    case HCI_READ_LOCAL_SUP_CODECS_CMD_CMPL_CBACK_EVT:
      pEvent->hdr.event = DM_READ_LOCAL_SUP_CODECS_IND;
      break;

    case HCI_READ_LOCAL_SUP_CODEC_CAP_CMD_CMPL_CBACK_EVT:
      pEvent->hdr.event = DM_READ_LOCAL_SUP_CODEC_CAP_IND;
      break;

    case HCI_READ_LOCAL_SUP_CTR_DLY_CMD_CMPL_CBACK_EVT:
      pEvent->hdr.event = DM_READ_LOCAL_SUP_CTR_DLY_IND;
      break;

    default:
      /* should never get here */
      return;
    }

    (*dmCb.cback)((dmEvt_t *) pEvent);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Default HCI ISO data callback function for CIS connections.
 *
 *  \param  pPacket   A buffer containing an ISO packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoCisCback(uint8_t *pPacket)
{
  hciIsoDataPkt_t isoPkt;

  hciCoreUnpackIsoDataPkt(pPacket, &isoPkt);

  if (isoPkt.ps == HCI_SUCCESS)
  {
    DM_TRACE_INFO2("dmIsoCisCback: CIS Packet of isoLen %u received for handle = %d",
                   isoPkt.isoLen, isoPkt.handle);

    /* If role is master, loopback data to slave */
    if (DmCisConnRole(isoPkt.handle) == DM_ROLE_MASTER)
    {
      /* Make new buffer and copy ISO packet over. */
      uint8_t *pBuf = WsfMsgAlloc(HCI_GET_SDU_HDR_LEN(FALSE) + isoPkt.sduLen);
      if (pBuf)
      {
        memcpy(pBuf + HCI_GET_SDU_HDR_LEN(FALSE), isoPkt.pIsoSdu, isoPkt.isoLen);

        DmSendIsoData(isoPkt.handle, isoPkt.sduLen, pBuf, FALSE, 0);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Default HCI ISO data callback function for BIS connections.
 *
 *  \param  pPacket   A buffer containing an ISO packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoBisCback(uint8_t *pPacket)
{
  hciIsoDataPkt_t isoPkt;

  hciCoreUnpackIsoDataPkt(pPacket, &isoPkt);

  if (isoPkt.ps == HCI_SUCCESS)
  {
    /* if insufficient security for Broadcast_Code */
    if (DmBigSyncGetSecLevel(isoPkt.handle) > DM_SEC_LEVEL_BCAST_NONE)
    {
      DM_TRACE_INFO2("dmIsoBisCback: BIS packet of isoLen %d dropped for handle = %d",
                     isoPkt.isoLen, isoPkt.handle);
      return;
    }

    /* Handle BIS packet here. */
    DM_TRACE_INFO2("dmIsoBisCback: BIS Packet of sduLen %d received for handle = %d",
                   isoPkt.sduLen, isoPkt.handle);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  HCI ISO data callback function.
 *
 *  \param  pPacket   A buffer containing an ISO packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoHciIsoCback(uint8_t *pPacket)
{
  uint16_t  handle;

  /* parse HCI handle */
  BYTES_TO_UINT16(handle, pPacket);
  handle &= HCI_HANDLE_MASK;

  /* if connection handle is for BIS sync */
  if (DmBisSyncInUse(handle))
  {
    (*dmIsoCb.bisCback)(pPacket);
  }
  /* if connection handle is for CIS */
  else if (DmCisConnInUse(handle))
  {
    (*dmIsoCb.cisCback)(pPacket);
  }
  else
  {
    /* Packet was not handled. */
    DM_TRACE_WARN1("dmIsoHciHandleIsoPkt: Packet not handled. handle = %u", handle);
  }

  /* Packet successfully handled. Free pointer. */
  WsfMsgFree(pPacket);
}

/*************************************************************************************************/
/*!
 *  \brief  HCI flow control callback function.
 *
 *  \param  handle        The connection handle.
 *  \param  flowDisabled  TRUE if data flow is disabled.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmIsoHciFlowCback(uint16_t handle, bool_t flowDisabled)
{
  DM_TRACE_INFO2("flowDisabled=%u handle=%u", flowDisabled, handle);
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize DM ISO manager.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmIsoInit(void)
{
  WsfTaskLock();

  dmFcnIfTbl[DM_ID_ISO] = (dmFcnIf_t *) &dmIsoFcnIf;

  /* Initialize default ISO data callbacks */
  dmIsoCb.cisCback = dmIsoCisCback;
  dmIsoCb.bisCback = dmIsoBisCback;

  /* Register with HCI */
  HciIsoRegister(dmIsoHciIsoCback, dmIsoHciFlowCback);

  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Register CIS, BIS and setup callbacks for the HCI ISO data path.
 *
 *  \param  cisCback    CIS data callback function (may be NULL).
 *  \param  bisCback    BIS data callback function (may be NULL).
 *  \param  setupCback  ISO data path setup callback function (NULL when the codec in the LL).
 *  \param  removeCback ISO data path remove callback function (NULL when the codec in the LL).
 *
 *  \return None.
 */
 /*************************************************************************************************/
void DmIsoRegister(hciIsoCback_t cisCback, hciIsoCback_t bisCback,
                   dmIsoDataPathSetupCback_t setupCback, dmIsoDataPathRemoveCback_t removeCback)
{
  if (cisCback != NULL)
  {
    dmIsoCb.cisCback = cisCback;
  }

  if (bisCback != NULL)
  {
    dmIsoCb.bisCback = bisCback;
  }

  if (setupCback != NULL)
  {
    dmIsoCb.setupCback = setupCback;
  }

  if (removeCback != NULL)
  {
    dmIsoCb.removeCback = removeCback;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Setup the isochronous data path between the Host and the Controller for an established
 *          CIS or BIS identified by the connection handle parameter.
 *
 *  \param  pDataPathParam  Parameters to setup ISO data path.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmIsoDataPathSetup(HciIsoSetupDataPath_t *pDataPathParam)
{
  dmIsoConnCb_t    *pIsoCcb = NULL;
  uint16_t         handle = pDataPathParam->handle;
  dmIsoApiSetup_t  *pMsg;

  WSF_ASSERT(pDataPathParam->dpDir < DM_ISO_NUM_DIR);

  /* make sure ISO cb not already allocated */
  WsfTaskLock();
  if ((pIsoCcb = dmIsoCbByHandle(handle)) == NULL)
  {
    /* allocate Cig cb */
    pIsoCcb = dmIsoCbAlloc(handle);
  }
  WsfTaskUnlock();

  if (pIsoCcb != NULL)
  {
    if ((pMsg = WsfMsgAlloc(sizeof(dmIsoApiSetup_t) + pDataPathParam->codecConfigLen)) != NULL)
    {
      pMsg->hdr.event = DM_ISO_MSG_API_SETUP;
      pMsg->hdr.param = handle;

      memcpy(&pMsg->dataPathParam, pDataPathParam, sizeof(HciIsoSetupDataPath_t));

      if (pDataPathParam->codecConfigLen > 0)
      {
        pMsg->dataPathParam.pCodecConfig = (uint8_t *)(pMsg + 1);
        memcpy(pMsg->dataPathParam.pCodecConfig, pDataPathParam->pCodecConfig, pDataPathParam->codecConfigLen);
      }
      else
      {
        pMsg->dataPathParam.pCodecConfig = NULL;
      }

      WsfMsgSend(dmCb.handlerId, pMsg);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Remove the input and/or output data path(s) associated with a CIS or BIS identified
 *          by the connection handle parameter.
 *
 *  \param  handle         Connection handle of CIS or BIS.
 *  \param  directionBits  Data path direction bits.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmIsoDataPathRemove(uint16_t handle, uint8_t directionBits)
{
  dmIsoApiRemove_t *pMsg;

  WSF_ASSERT((directionBits & HCI_ISO_DATA_PATH_INPUT_BIT) || (directionBits & HCI_ISO_DATA_PATH_OUTPUT_BIT))

  if ((pMsg = WsfMsgAlloc(sizeof(dmIsoApiRemove_t))) != NULL)
  {
    pMsg->hdr.event = DM_ISO_MSG_API_REMOVE;
    pMsg->hdr.param = handle;
    pMsg->directionBits = directionBits;

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Request the Controller to configure the data transport path in a given direction
 *          between the Controller and the Host.
 *
 *  \param  pDataPathParam  Parameters for configuring data path.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmDataPathConfig(HciConfigDataPath_t *pDataPathParam)
{
  HciConfigDataPathCmd(pDataPathParam);
}

/*************************************************************************************************/
/*!
 *  \brief  Read a list of the codecs supported by the Controller, as well as vendor specific
 *          codecs, which are defined by an individual manufacturer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmReadLocalSupCodecs(void)
{
  HciReadLocalSupCodecsCmd();
}

/*************************************************************************************************/
/*!
 *  \brief  Read a list of codec capabilities supported by the Controller for a given codec.
 *
 *  \param  pCodecParam  Parameters for reading local supported codec capabilities.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmReadLocalSupCodecCap(HciReadLocalSupCodecCaps_t *pCodecParam)
{
  HciReadLocalSupCodecCapCmd(pCodecParam);
}

/*************************************************************************************************/
/*!
 *  \brief  Read the range of supported Controller delays for the codec specified by Codec ID on
 *          a given transport type specified by Logical Transport Type, in the direction specified
 *          by Direction, and with the codec configuration specified by Codec Configuration.
 *
 *  \param  pDelayParam  Parameters for reading local supported controller delay.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmReadLocalSupCtrDly(HciReadLocalSupControllerDly_t *pDelayParam)
{
  HciReadLocalSupControllerDlyCmd(pDelayParam);
}

/*************************************************************************************************/
/*!
 *  \brief  Send ISO Data.
 *
 *  \param  handle  ISO Handle to send data.
 *  \param  len     Length of data (Excluding header).
 *  \param  pData   Pointer to start of ISO buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSendIsoData(uint16_t handle, uint16_t len, uint8_t *pData, bool_t useTs, uint32_t ts)
{
  hciSendIsoData(handle, len, pData, useTs, ts);
}
