/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Internal scheduler resource manager interface file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#ifndef SCH_INT_RM_H
#define SCH_INT_RM_H

#include "sch_api.h"
#include "sch_api_ble.h"
#include "ll_math.h"
#include "cfg_mac_ble.h"
#include "pal_bb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Maximum number of reservations per unit of preferred periodicity. */
#define SCH_RM_MAX_RSVN_PREF_PER    2           /* TODO allow run-time configuration */

/*! \brief      Binary divide by preferred periodicity (10,000us) (n[max]=2^32). */
#define SCH_RM_DIV_PREF_PER(n)      ((uint32_t)(((uint64_t)(n) * UINT64_C(1717987)) >> 34))

/*! \brief      Minimum offset unit of reservation manager in microseconds. (Half of minimum isochronous interval) */
#define SCH_RM_MIN_OFFSET_UNIT_US   2500

/*! \brief      Default distance in microseconds between common reservation and uncommon reservation. */
#define SCH_RM_OFFSET_UNCOMMON_US   3750

/*! \brief      Margin in microseconds from the duration of the common reservation to uncommon reservation. */
#define SCH_RM_MARGIN_UNCOMMON_US   50

/*! \brief      Maximum depth to be searched between intervals. (4 means 16 times(2^4) */
#define SCH_RM_MAX_SEARCH_DEPTH     4

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Reservation descriptor. */
typedef struct
{
  uint8_t handle;           /*!< Reservation handle. */
  bool_t cmdInterUsed;      /*!< Reservation is controlled by common interval. */
  uint8_t offsetBit;        /*!< Offset bit location. */
  uint32_t interUsec;       /*!< Interval in microseconds. */
  uint32_t durUsec;         /*!< Duration in microseconds. */
  GetRefTimeCb_t refTimeCb; /*!< Callback function to get reference time of the handle. */
} schRmRsvn_t;

/*! \brief      Scheduler resource manager control block. */
typedef struct
{
  uint8_t numRsvn;                              /*!< Number of reservations. */
  uint8_t refHandle;                            /*!< Handle to be used to get reference time. */
  uint8_t offsetDepth;                          /*!< Offset depth from common interval. */
  uint8_t indexUncommon;                        /*!< Index of uncommon reservations. */
  uint32_t rmStatus;                            /*!< Reservation status. */
  uint32_t cmnInter;                            /*!< Common interval. */
  uint32_t rsvnInterUsec[SCH_RM_MAX_RSVN];      /*!< Reserved intervals indexed by handle. */
  schRmRsvn_t rsvn[SCH_RM_MAX_RSVN];            /*!< Reservation info for each handle. */
} SchRmCb_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

extern SchRmCb_t schRmCb;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void schRmSortListDescending(uint32_t item[], uint8_t numItems);
uint8_t schRmIntCalculateDepth(uint32_t intLarge, uint32_t intSmall);

#ifdef __cplusplus
};
#endif

#endif /* SCH_INT_RM_H */
