/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  SMP main module header file.
 *
 *  Copyright (c) 2010-2018 Arm Ltd. All Rights Reserved.
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
#ifndef SMP_SC_MAIN_H
#define SMP_SC_MAIN_H

#include "smp_main.h"
#include "smp_defs.h"
#include "smp_api.h"
#include "wsf_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

#if WSF_TOKEN_ENABLED == TRUE || WSF_TRACE_ENABLED == TRUE
#define SMP_TRACE_128(msg, ptr)            smpLogByteArray(msg, ptr, 16)
#define SMP_TRACE_256(msg, ptr)            smpLogByteArray(msg, ptr, 32)
#else
#define SMP_TRACE_128(msg, ptr)
#define SMP_TRACE_256(msg, ptr)
#endif

/**************************************************************************************************
  Constants
**************************************************************************************************/

/* Number of bits in the Passkey */
#define  SMP_PK_BIT_COUNT               20

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* common state machine action functions */
void smpScActCleanup(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActPairingFailed(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActPairingCancel(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActAuthSelect(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActPkSetup(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActJwncCalcF4(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActJwncCalcG2(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActJwncDisplay(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActPkKeypress(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActPkSendKeypress(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActCalcSharedSecret(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActCalcF5TKey(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActCalcF5MacKey(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActCalcF5Ltk(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActDHKeyCalcF6Ea(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScActDHKeyCalcF6Eb(smpCcb_t *pCcb, smpMsg_t *pMsg);

/* initiator state machine action functions */
void smpiScActSendPubKey(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActAuthSelect(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActJwncSetup(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActJwncSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActJwncCalcF4(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActJwncCalcG2(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActPkCalcCa(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActPkCalcCb(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActPkSendCnf(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActPkSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActPkCheck(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActOobCalcCb(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActOobSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActOobProcRand(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActDHKeyCheckSend(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpiScActDHKeyCheckVerify(smpCcb_t *pCcb, smpMsg_t *pMsg);

/* responder state machine action functions */
void smprScActStoreLescPin(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActSendPubKey(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActJwncSetup(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActJwncCalcG2(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActJwncDisplay(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActJwncSendCnf(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActPkStoreCnf(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActPkStoreCnfAndCalcCb(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActPkStorePinAndCalcCb(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActPkCalcCb(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActPkSendCnf(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActPkCalcCa(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActPkSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActOobSetup(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActOobCalcCa(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActOobSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActStoreDhCheck(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActWaitDhCheck(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActDHKeyCheckSend(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smprScActCalcDHKey(smpCcb_t *pCcb, smpMsg_t *pMsg);

/* common functions */
bool_t SmpScAllocScratchBuffers(smpCcb_t *pCcb);
void SmpScFreeScratchBuffers(smpCcb_t *pCcb);
bool_t smpScProcPairing(smpCcb_t *pCcb, uint8_t *pOob, uint8_t *pDisplay);
void SmpReverseCpy(uint8_t *pBuf1, uint8_t *pBuf2, uint8_t len);
void SmpScCmac(const uint8_t *pKey, uint8_t *pText, uint8_t textLen, smpCcb_t *pCcb, smpMsg_t *pMsg);
uint8_t *SmpScAlloc(uint8_t size, smpCcb_t *pCcb, smpMsg_t *pMsg);
void SmpScFree(uint8_t *pBuf);
void SmpScCalcF4(smpCcb_t *pCcb, smpMsg_t *pMsg, uint8_t *pU, uint8_t *pV, uint8_t z, uint8_t *pX);
uint8_t *SmpScCat(uint8_t *pDst, const uint8_t *pSrc, uint8_t len);
uint8_t *SmpScCat128(uint8_t *pDst, uint8_t *pSrc);
uint8_t smpGetPkBit(smpCcb_t *pCcb);
smpScPubKey_t *smpGetPeerPublicKey(dmConnId_t connId);
void smpSetPeerPublicKey(dmConnId_t connId, smpScPubKey_t *pKey);
void SmpScSetOobCfg(dmConnId_t connId, dmSecLescOobCfg_t *pConfig);
void smpScAuthReq(smpCcb_t *pCcb, uint8_t oob, uint8_t display);
void smpScFailWithReattempt(smpCcb_t *pCcb, uint8_t status);
void SmpScInit(void);

/* common command send functoins */
void smpScSendPubKey(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpScSendDHKeyCheck(smpCcb_t *pCcb, smpMsg_t *pMsg, uint8_t *pCheck);
void smpScSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg, uint8_t *pRand);
void smpScSendPairCnf(smpCcb_t *pCcb, smpMsg_t *pMsg, uint8_t *pCnf);

/* diagnostics utility functions */
void smpLogByteArray(char *str, uint8_t *pArray, uint8_t len);
uint8_t *smpiStateStr(uint8_t state);
uint8_t *smprStateStr(uint8_t state);

#ifdef __cplusplus
};
#endif

#endif /* SMP_SC_MAIN_H */
