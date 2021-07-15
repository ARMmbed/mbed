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
#ifndef DM_CIS_H
#define DM_CIS_H

#include "dm_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Uninitialized HCI handle */
#define DM_CIS_HCI_HANDLE_NONE              0xFFFF

/* Action set initializer */
#define DM_CIS_ACT_SET_INIT(n)              ((n) << 4)

/* Get action set ID from action */
#define DM_CIS_ACT_SET_ID(action)           ((action) >> 4)

/* Get action ID from action */
#define DM_CIS_ACT_ID(action)               ((action) & 0x0F)

/* Restore old state */
#define DM_CIS_SM_RESTORE_OLD_STATE         0xFF

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! DM CIS event handler messages for state machine */
enum
{
  /* messages from API */
  DM_CIS_MSG_API_OPEN = DM_MSG_START(DM_ID_CIS),                    /*!< Open CIS Connection */
  DM_CIS_MSG_API_CLOSE,                                             /*!< Close CIS Connection */
  DM_CIS_MSG_API_ACCEPT,                                            /*!< Accept CIS Connection */
  DM_CIS_MSG_API_REJECT,                                            /*!< Reject CIS Connection */

  /* messages from HCI */
  DM_CIS_MSG_HCI_LE_CIS_EST_FAIL,                                   /*!< HCI LE CIS Establish Failure Event */
  DM_CIS_MSG_HCI_LE_CIS_EST,                                        /*!< HCI LE CIS Established Event */
  DM_CIS_MSG_HCI_DISCONNECT_CMPL,                                   /*!< HCI Disconnection Complete Event */
  DM_CIS_MSG_HCI_LE_CIS_REQ                                         /*!< HCI LE CIS Request Event */
};

/* Number of CIS messages */
#define DM_CIS_NUM_MSGS                     (DM_CIS_MSG_HCI_LE_CIS_REQ - DM_CIS_MSG_API_OPEN + 1)

/* CIS state machine action function sets */
enum
{
  DM_CIS_ACT_SET_MAIN,                                              /*!< Main action functions */
  DM_CIS_ACT_SET_MASTER,                                            /*!< Master action functions */
  DM_CIS_ACT_SET_SLAVE,                                             /*!< Slave action functions */

  DM_CIS_NUM_ACT_SETS
};

/*! CIS state machine actions */
enum
{
  DM_CIS_SM_ACT_NONE = DM_CIS_ACT_SET_INIT(DM_CIS_ACT_SET_MAIN),    /*!< No Action */
  DM_CIS_SM_ACT_CLOSE,                                              /*!< Process Close */
  DM_CIS_SM_ACT_CIS_EST,                                            /*!< Procoss CIS Established */
  DM_CIS_SM_ACT_CIS_EST_FAILED,                                     /*!< Process CIS Establish Failed */
  DM_CIS_SM_ACT_CIS_CLOSED,                                         /*!< Process CIS Closed */

  DM_CIS_SM_ACT_OPEN = DM_CIS_ACT_SET_INIT(DM_CIS_ACT_SET_MASTER),  /*!< Process Master Open */
  DM_CIS_SM_ACT_CANCEL_OPEN,                                        /*!< Process Master Cancel Open */

  DM_CIS_SM_ACT_REQUEST = DM_CIS_ACT_SET_INIT(DM_CIS_ACT_SET_SLAVE),/*!< Process Slave Request */
  DM_CIS_SM_ACT_ACCEPT,                                             /*!< Process Slave Accept */
  DM_CIS_SM_ACT_REJECT,                                             /*!< Process Slave Reject */
};

/*! CIS state machine states */
enum
{
  DM_CIS_SM_ST_IDLE,                                                /*!< Idle State */
  DM_CIS_SM_ST_CONNECTING,                                          /*!< Connecting State */
  DM_CIS_SM_ST_REQUESTING,                                          /*!< Requesting State */
  DM_CIS_SM_ST_ACCEPTING,                                           /*!< Accepting State */
  DM_CIS_SM_ST_CONNECTED,                                           /*!< Connected State */
  DM_CIS_SM_ST_DISCONNECTING,                                       /*!< Disconnecting State */

  DM_CIS_SM_NUM_STATES
};

/*! DM CIS CIG event handler messages for state machine */
enum
{
  /* messages from API */
  DM_CIS_CIG_MSG_API_CONFIG = DM_MSG_START(DM_ID_CIS_CIG),          /*!< Configure CIG */
  DM_CIS_CIG_MSG_API_REMOVE,                                        /*!< Remove CIG */

  /* messages from HCI */
  DM_CIS_MSG_HCI_LE_SET_CIG_PARAMS_CMD_CMPL_FAIL,                   /*!< HCI LE Set CIG Parameters Command Complete Failure Event */
  DM_CIS_MSG_HCI_LE_SET_CIG_PARAMS_CMD_CMPL,                        /*!< HCI LE Set CIG Parameters Command Complete Event */
  DM_CIS_MSG_HCI_LE_REMOVE_CIG_CMD_CMPL_FAIL,                       /*!< HCI LE Remove CIG Command Complete Failure Event */
  DM_CIS_MSG_HCI_LE_REMOVE_CIG_CMD_CMPL                             /*!< HCI LE Remove CIG Command Complete Event */
};

/* Number of CIS CIG messages */
#define DM_CIS_CIG_NUM_MSGS                 (DM_CIS_MSG_HCI_LE_REMOVE_CIG_CMD_CMPL - DM_CIS_CIG_MSG_API_CONFIG + 1)

/*! CIS CIG state machine actions */
enum
{
  DM_CIS_CIG_SM_ACT_NONE,                                           /*!< No Action */
  DM_CIS_CIG_SM_ACT_CONFIG,                                         /*!< Process Configure */
  DM_CIS_CIG_SM_ACT_CONFIGED,                                       /*!< Process Configured */
  DM_CIS_CIG_SM_ACT_CONFIG_FAILED,                                  /*!< Process Configure Failed */
  DM_CIS_CIG_SM_ACT_REMOVE,                                         /*!< Process Remove */
  DM_CIS_CIG_SM_ACT_REMOVED,                                        /*!< Process Removed */
  DM_CIS_CIG_SM_ACT_REMOVE_FAILED,                                  /*!< Process Remove Failed */
};

/*! CIS CIG state machine states */
enum
{
  DM_CIS_CIG_SM_ST_IDLE,                                            /*!< Idle State */
  DM_CIS_CIG_SM_ST_CONFIGING,                                       /*!< Configuring State */
  DM_CIS_CIG_SM_ST_CONFIGED,                                        /*!< Configured State */
  DM_CIS_CIG_SM_ST_REMOVING,                                        /*!< Removing State */

  DM_CIS_CIG_SM_NUM_STATES
};

/* Data structure for DM_CIS_CIG_MSG_API_CONFIG */
typedef struct
{
  wsfMsgHdr_t                   hdr;                                /*!< Message Header */
  uint8_t                       numCis;                             /*!< Number of CIS to be configured */
  HciCisCisParams_t             *pCisParam;                         /*!< CIS parameters */
} dmCisCigApiConfig_t;

/* Data structure for DM_CIS_MSG_API_OPEN */
typedef struct
{
  wsfMsgHdr_t                   hdr;                                /*!< Message Header */
  uint8_t                       numCis;                             /*!< Total number of CISes to be created */
  uint16_t                      *pCisHandle;                        /*!< List of connection handles of CISes */
  dmConnId_t                    *pConnId;                           /*!< List of DM connection identifiers */
} dmCisApiOpen_t;

/*! Data structure for DM_CIS_MSG_API_CLOSE */
typedef struct
{
  wsfMsgHdr_t                   hdr;                                /*!< Message Header */
  uint8_t                       reason;                             /*!< Reason connection is being closed */
} dmCisApiClose_t;

/*! Data structure for DM_CIS_MSG_API_REJECT */
typedef struct
{
  wsfMsgHdr_t                   hdr;                                /*!< Message Header */
  uint8_t                       reason;                             /*!< Reason CIS request was rejected */
} dmCisApiReject_t;

/*! Union of all DM CIS state machine messages */
typedef union
{
  wsfMsgHdr_t                   hdr;                                /*!< Message Header */

  /* API messages */
  dmCisCigApiConfig_t           apiConfig;                          /*!< Configure CIG */
  dmCisApiOpen_t                apiOpen;                            /*!< Open CIS Connection */
  dmCisApiClose_t               apiClose;                           /*!< Close CIS Connection */
  dmCisApiReject_t              apiReject;                          /*!< Reject CIS Connection */

  /* HCI LE events */
  HciLeCisEstEvt_t              hciLeCisEst;                        /*!< CIS Established */
  HciLeCisReqEvt_t              hciLeCisReq;                        /*!< CIS Request */
  hciLeSetCigParamsCmdCmplEvt_t hciLeSetCigParamsCmdCmpl;           /*!< Set CIS Parameters Command Complete */
  hciLeRemoveCigCmdCmplEvt_t    hciLeRemoveCigCmdCmpl;              /*!< Remove CIG Command Complete */
} dmCisMsg_t;

/*! CIS connection control block */
typedef struct
{
  uint8_t                       cigId;                              /*!< Used to identify the connected isochronous group. */
  uint8_t                       cisId;                              /*!< Used to identify a connected isochronous stream. */
  uint16_t                      aclHandle;                          /*!< ACL handle. */
  uint16_t                      cisHandle;                          /*!< CIS handle. */
  dmConnId_t                    connId;                             /*!< DM connection ID */
  uint8_t                       role;                               /*!< Role. */
  uint8_t                       state;                              /*!< Main state. */
  uint8_t                       inUse;                              /*!< TRUE if entry in use. */
} dmCisCcb_t;

/*! CIS CIG control block */
typedef struct
{
  uint8_t                       cigId;                              /*!< Used to identify the connected isochronous group. */
  uint32_t                      sduIntervalMToS;                    /*!< Time interval between the start of consecutive SDUs from the master Host (in usec).  */
  uint32_t                      sduIntervalSToM;                    /*!< Time interval between the start of consecutive SDUs from the slave Host (in usec).  */
  uint8_t                       sca;                                /*!< Sleep clock accuracy. */
  uint8_t                       packing;                            /*!< Packing scheme. */
  uint8_t                       framing;                            /*!< Indicates the format of CIS Data PDUs.  */
  uint16_t                      transLatMToS;                       /*!< Maximum time for an SDU to be transported from the master Controller to slave Controller (in msec). */
  uint16_t                      transLatSToM;                       /*!< Maximum time for an SDU to be transported from the slave Controller to master Controller (in msec). */
  uint8_t                       numCis;                             /*!< Total number of CISes in the CIG being added or modified. */
  uint8_t                       cisId[DM_CIS_MAX];                  /*!< List of CIS IDs being added or modified. */
  uint8_t                       state;                              /*!< Main state. */
  uint8_t                       inUse;                              /*!< TRUE if entry in use. */
} dmCisCigCb_t;

/*! Action functions */
typedef void (*dmCisAct_t)(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg);
typedef void (*dmCisCigAct_t)(dmCisCigCb_t *pCigCb, dmCisMsg_t *pMsg);

/*! Control block of the DM CIS module */
typedef struct
{
  dmCisCcb_t                    cisCcb[DM_CIS_MAX];                 /*!< CIS connection control block. */
  dmCisCigCb_t                  cisCigCb[DM_CIG_MAX];               /*!< CIS CIG control block. */
} dmCisCb_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! State machine action sets */
extern dmCisAct_t *dmCisActSet[DM_CIS_NUM_ACT_SETS];

/*! Action function table for CIG master module */
extern const dmCisCigAct_t dmCisCigAct[];

/*! Control block */
extern dmCisCb_t dmCisCb;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* utility functions */
dmCisCigCb_t *dmCisCigCbAlloc(uint8_t cigId);
void dmCisCigCbDealloc(dmCisCigCb_t *pCigCb);
dmCisCcb_t *dmCisCcbAlloc(uint8_t cigId, uint8_t cisId, uint8_t role);
void dmCisCcbDealloc(dmCisCcb_t *pCcb);
void dmCisCcbDeallocByCigId(uint8_t cigId);
dmCisCcb_t *dmCisCcbByHandle(uint16_t handle);
dmCisCcb_t *dmCisCcbById(uint8_t cigId, uint8_t cisId);
dmCisCigCb_t *dmCisCigCbById(uint8_t cigId);

/* CIS main action functions */
void dmCisSmActClose(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg);
void dmCisSmActCisEst(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg);
void dmCisSmActCisEstFailed(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg);
void dmCisSmActCisClosed(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg);

/* CIG main action functions */
void dmCisCigReset(void);
void dmCisCigMsgHandler(wsfMsgHdr_t *pMsg);
void dmCisCigHciHandler(hciEvt_t *pEvent);

/* state machine */
void dmCisSmExecute(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg);
void dmCisCigSmExecute(dmCisCigCb_t *pCcb, dmCisMsg_t *pMsg);

/* main action functions */
void dmCisSmActNone(dmCisCcb_t *pCcb, dmCisMsg_t *pMsg);

#ifdef __cplusplus
};
#endif

#endif /* DM_CIS_H */
