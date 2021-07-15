/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      FRC interface file.
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

#ifndef PAL_FRC_H
#define PAL_FRC_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_FRC
 *  \{ */

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void PalFrcInit(void);

/* Control and Status */
uint32_t PalFrcGetCurrentTime(void);
void palFrcSetWakeup(uint32_t nextWakeMs);
uint32_t PalFrcMsToTicks(uint32_t deltaMs);
uint32_t PalFrcDeltaMs(uint32_t endTime, uint32_t startTime);
uint32_t PalFrcDeltaUs(uint32_t endTime, uint32_t startTime);

/*! \} */    /* PAL_FRC */

#ifdef __cplusplus
};
#endif

#endif
