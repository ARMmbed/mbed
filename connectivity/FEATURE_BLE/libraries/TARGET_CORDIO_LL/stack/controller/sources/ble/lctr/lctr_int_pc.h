/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Internal link layer controller power control interface file.
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

#ifndef LCTR_INT_PC_H
#define LCTR_INT_PC_H

#include "lctr_api.h"
#include "lctr_int_conn_slave.h"
#include "lctr_int_conn_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************************
 * Function Declarations
*************************************************************************************************/

uint8_t lctrCalcPathLossZone(lctrConnCtx_t *pCtx);
void lctrNotifyHostPathLossRpt(lctrConnCtx_t *pCtx);
void lctrPathLossMonitorAct(lctrConnCtx_t *pCtx);

/* Power indication actions. */
void lctrStorePeerPowerInd(lctrConnCtx_t *pCtx);
void lctrSendPeerPowerRsp(lctrConnCtx_t *pCtx);

/* Power control actions. */
void lctrStorePowerControlAction(lctrConnCtx_t *pCtx);
void lctrSendPeerPowerControlReq(lctrConnCtx_t *pCtx);
void lctrStorePeerPowerControlReq(lctrConnCtx_t *pCtx);
void lctrSendPeerPowerControlRsp(lctrConnCtx_t *pCtx);
void lctrStorePeerPowerControlRsp(lctrConnCtx_t *pCtx);

/* Power monitoring actions. */
void lctrConnServicePowerMonitor(lctrConnCtx_t *pCtx);

/* Power reporting actions. */
void lctrNotifyPowerReportInd(lctrConnCtx_t *pCtx, uint8_t status, uint8_t reason, uint8_t phy, int8_t txPower, uint8_t limits, int8_t delta);
int8_t lctrAttemptTxPowerChange(lctrConnCtx_t *pCtx, uint8_t phy, int8_t delta);
void lctrSendPowerChangeInd(lctrConnCtx_t *pCtx, uint8_t phy, int8_t delta, int8_t txPower, bool_t phyChange);

/* State machine */
bool_t lctrLlcpExecutePclSm(lctrConnCtx_t *pCtx, uint8_t event);

#ifdef __cplusplus
};
#endif

#endif /* LCTR_INT_PC_H */
