/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Internal link layer controller PHY features (slave) interface file.
 *
 *  Copyright (c) 2016-2017 ARM Ltd. All Rights Reserved.
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

#ifndef LCTR_INT_PHY_SLAVE_H
#define LCTR_INT_PHY_SLAVE_H

#include "lctr_int.h"
#include "lctr_int_conn.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* State machine */
bool_t lctrSlvLlcpExecutePhyUpdateSm(lctrConnCtx_t *pCtx, uint8_t event);

/* Action routines */
void lctrStoreHostPhyUpdate(lctrConnCtx_t *pCtx);
void lctrStorePeerPhyReq(lctrConnCtx_t *pCtx);
void lctrStorePeerPhyUpdateInd(lctrConnCtx_t *pCtx);

void lctrSendPhyReqPdu(lctrConnCtx_t *pCtx, uint8_t txPhys, uint8_t rxPhys);
void lctrSendPhyRspPdu(lctrConnCtx_t *pCtx, uint8_t txPhys, uint8_t rxPhys);
void lctrSendPhyUpdateIndPdu(lctrConnCtx_t *pCtx, uint8_t txPhys, uint8_t rxPhys);

void lctrNotifyHostPhyUpdateInd(lctrConnCtx_t *pCtx, uint8_t status);

bool_t lctrSlvCheckEncOverridePhyUpdate(lctrConnCtx_t *pCtx);

#ifdef __cplusplus
};
#endif

#endif /* LCTR_INT_PHY_SLAVE_H */
