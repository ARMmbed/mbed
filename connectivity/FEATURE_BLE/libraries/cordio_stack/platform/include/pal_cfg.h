 /*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      System configuration definition.
 *
 *  Copyright (c) 2018-2019 Arm Ltd. All Rights Reserved.
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

#ifndef PAL_CFG_H
#define PAL_CFG_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_CFG
 *  \{ */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Configuration ID. */
typedef enum
{
  PAL_CFG_ID_BD_ADDR,                       /*!< BD address. */
  PAL_CFG_ID_BLE_PHY,                       /*!< BLE PHY. */
  PAL_CFG_ID_LL_PARAM,                      /*!< LL parameters. */
  PAL_CFG_ID_MAC_ADDR,                      /*!< MAC address. */
  PAL_CFG_ID_UUID,                          /*!< UUID. */
  PAL_CFG_ID_BOOTSTRAP                      /*!< Bootstrap. */
} PalCfgId_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void PalCfgLoadData(uint8_t cfgId, void *pBuf, uint32_t len);
void PalCfgSetDeviceUuid(uint8_t *pBuf);

/*! \} */    /* PAL_CFG */

#ifdef __cplusplus
};
#endif

#endif /* PAL_CFG_H */
