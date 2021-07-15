/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  DM security module.
 *
 *  Copyright (c) 2009-2018 Arm Ltd. All Rights Reserved.
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
#ifndef DM_SEC_H
#define DM_SEC_H

#include "wsf_os.h"
#include "smp_api.h"
#include "dm_main.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* DM sec event handler messages */
enum
{
  DM_SEC_MSG_API_ENCRYPT_REQ = DM_MSG_START(DM_ID_SEC),
  DM_SEC_MSG_API_LTK_RSP
};

/* DM lesc sec event handler messages */
enum
{
  DM_SEC_MSG_CALC_OOB_CNF = DM_MSG_START(DM_ID_LESC),
  DM_SEC_MSG_ECC_KEY_CNF
};

/**************************************************************************************************
  Data types
**************************************************************************************************/

/* Data type for DM_SEC_MSG_API_ENCRYPT_REQ */
typedef struct
{
  wsfMsgHdr_t           hdr;
  dmSecLtk_t            ltk;
  uint8_t               secLevel;
} dmSecApiEncryptReq_t;

/* Data type for DM_SEC_MSG_API_LTK_RSP */
typedef struct
{
  wsfMsgHdr_t           hdr;
  uint8_t               key[SMP_KEY_LEN];
  bool_t                keyFound;
  uint8_t               secLevel;
} dmSecApiLtkRsp_t;

typedef union
{
  wsfMsgHdr_t           hdr;
  dmSecApiEncryptReq_t  encryptReq;
  dmSecApiLtkRsp_t      ltkRsp;
} dmSecMsg_t;

/* Security control block type */
typedef struct
{
  uint8_t               *pIrk;        /* Pointer to local Identity Resolving Key (IRK) */
  uint8_t               *pCsrk;       /* Pointer to local Connection Signature Resolving Key (CSRK) */
  bdAddr_t              identityAddr; /* Local identity address */
  uint8_t               addrType;     /* Local identity address type */
} dmSecCb_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Control block */
extern dmSecCb_t dmSecCb;

/**************************************************************************************************
  Function declarations
**************************************************************************************************/

/* component interface */
void dmSecHciHandler(hciEvt_t *pEvent);
void dmSecMsgHandler(dmSecMsg_t *pMsg);
void dmSecReset(void);

/* component interface */
void dmSecLescMsgHandler(dmSecMsg_t *pMsg);

#ifdef __cplusplus
};
#endif

#endif /* DM_SEC_H */
