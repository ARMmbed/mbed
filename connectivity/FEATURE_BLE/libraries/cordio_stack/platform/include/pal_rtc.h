/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      RTC timer interface file.
 *
 *  Copyright (c) 2018 Arm Ltd. All Rights Reserved.
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

#ifndef PAL_RTC_H
#define PAL_RTC_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_RTC
 *  \{ */

/**************************************************************************************************
  Type Definitions
**************************************************************************************************/

/*! \brief      Operational states. */
typedef enum
{
  PAL_RTC_STATE_UNINIT = 0,     /*!< Uninitialized state. */
  PAL_RTC_STATE_ERROR  = 0,     /*!< Error state. */
  PAL_RTC_STATE_READY  = 1      /*!< Ready state. */
} PalRtcState_t;

/*! \brief  Platform RTC callback. */
typedef void (*palRtcIrqCback_t)(void);

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void PalRtcInit(void);

/* Control and Status */
void PalRtcEnableCompareIrq(uint8_t channelId);
void PalRtcDisableCompareIrq(uint8_t channelId);
uint32_t PalRtcCounterGet(void);
void PalRtcCompareSet(uint8_t channelId, uint32_t value);

/*! \} */    /* PAL_RTC */

#ifdef __cplusplus
};
#endif

#endif
