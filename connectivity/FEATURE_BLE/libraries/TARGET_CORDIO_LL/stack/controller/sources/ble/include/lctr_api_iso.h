/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller common ISO interface file.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
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

#ifndef LCTR_API_ISO_H
#define LCTR_API_ISO_H

#include "lctr_api.h"
#include "cfg_mac_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
uint16_t LctrInitIsoMem(uint8_t *pFreeMem, uint32_t freeMemSize);
void LctrInitCodec(void);

/* Data path */
void LctrTxIso(uint8_t *pIsoBuf);
uint8_t *LctrRxIso(void);
void LctrRxIsoComplete(uint8_t numBufs);
uint8_t LctrReadIsoTxSync(uint16_t handle, uint16_t *pPktSn, uint32_t *pTs, uint32_t *pTimeOffs);
uint8_t LctrConfigureDataPath(LlIsoConfigDataPath_t *pConfigDataPath);
uint8_t LctrSetupIsoDataPath(LlIsoSetupDataPath_t *pSetupDataPath);
uint8_t LctrRemoveIsoDataPath(uint16_t handle, uint8_t dpDir);
uint8_t LctrIsoTxTest(uint16_t handle, uint8_t pldType);
uint8_t LctrIsoRxTest(uint16_t handle, uint8_t pldType);
uint8_t LctrIsoReadTestCounter(uint16_t handle, LlIsoTestCtrs_t *pStats);
uint8_t LctrIsoTestEnd(uint16_t handle, LlIsoTestCtrs_t *pStats);
uint8_t LctrReadIsoLinkQual(uint16_t handle, LlIsoLinkQual_t *pStats);

#ifdef __cplusplus
};
#endif

#endif /* LCTR_API_ISO_H */
