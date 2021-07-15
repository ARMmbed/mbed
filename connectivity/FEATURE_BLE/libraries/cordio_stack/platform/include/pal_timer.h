/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Timer interface file.
 *
 *  Copyright (c) 2016-2019 Arm Ltd. All Rights Reserved.
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

#ifndef PAL_TIMER_H
#define PAL_TIMER_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_TIMER
 *  \{ */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Completion callback. */
typedef void (*PalTimerCompCback_t)(void);

/*! \brief      Timer states. */
typedef enum
{
  PAL_TIMER_STATE_UNINIT = 0,
  PAL_TIMER_STATE_ERROR = 0,
  PAL_TIMER_STATE_READY,
  PAL_TIMER_STATE_BUSY
} PalTimerState_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void PalTimerInit(PalTimerCompCback_t expCback);
void PalTimerDeInit(void);

/* Control and Status */
PalTimerState_t PalTimerGetState(void);
uint32_t PalTimerGetTimeToNextExpiration(void);
void PalTimerStart(uint32_t expUsec);
void PalTimerStop(void);

/*! \} */    /* PAL_TIMER */

#ifdef __cplusplus
};
#endif

#endif
