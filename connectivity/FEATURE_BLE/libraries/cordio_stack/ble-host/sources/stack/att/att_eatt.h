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
#ifndef EATT_MAIN_H
#define EATT_MAIN_H

#include "wsf_queue.h"
#include "wsf_timer.h"
#include "l2c_api.h"
#include "dm_api.h"
#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* EATT channel control block. */
typedef struct
{
  uint16_t      cid;                          /* L2CAP channel identifier. */
  uint16_t      priority;                     /* Slot priority */
  uint16_t      peerMtu;                      /* Peer MTU of L2CAP channel */
  uint16_t      localMtu;                     /* Local  MTU of L2CAP channel */
  uint16_t      localMps;                     /* Local MPS of L2CAP channel */
  bool_t        inUse;                        /* L2CAP channel is open */
} eattChanCb_t;

/* EATT connection control block. */
typedef struct
{
  uint8_t       state;                        /* Connection state. */
  uint16_t      pendingMtu;                   /* Pending MTU */
  uint16_t      pendingMps;                   /* Pending MPS */
  uint16_t      connInterval;                 /* The connection interval. */
  wsfTimer_t    backoffTimer;                 /* Backoff timer for L2CAP en conn req collisions. */
  eattChanCb_t  *pChanCb;                     /* Channel control block list. */
} eattConnCb_t;

/* EATT main control block. */
typedef struct
{
  eattConnCb_t  ccb[DM_CONN_MAX];             /* Connection control blocks. */
  l2cCocRegId_t cocRegId;                     /* L2CAP COC registration ID. */
} eattCb_t;

extern eattCb_t eattCb;

/**************************************************************************************************
  Function Prototypes
**************************************************************************************************/

uint16_t eattGetCid(dmConnId_t connId, uint8_t slot);
uint8_t eattGetSlotId(dmConnId_t connId, uint16_t cid);
eattConnCb_t *eattGetConnCb(dmConnId_t connId);
void eattHandler(wsfMsgHdr_t *pMsg);
void eattExecCallback(dmConnId_t connId, uint8_t event, uint8_t status);
void attEmptyL2cCocDataCback(l2cCocEvt_t *pMsg);

#ifdef __cplusplus
};
#endif

#endif /* EATT_MAIN_H */
