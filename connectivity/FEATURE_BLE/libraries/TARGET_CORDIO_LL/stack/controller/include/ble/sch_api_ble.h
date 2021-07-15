/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer scheduler interface file.
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

#ifndef SCH_API_BLE_H
#define SCH_API_BLE_H

#include "wsf_types.h"
#include "bb_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Preferred periodicity. */
#define SCH_RM_PREF_PER_USEC        10000

/*! \brief      Lowest perferred periodicity (for cases where intervalMax < SCH_RM_PREF_PER_USEC). */
#define SCH_RM_PREF_PER_USEC_LOWEST 5000

/*! \brief      Preferred periodicity for connections. */
#define SCH_RM_PREF_PER_CONN_USEC   (SCH_RM_PREF_PER_USEC)

/*! \brief      Preferred periodicity for periodic sync. */
#define SCH_RM_PREF_PER_SYNC_USEC   (SCH_RM_PREF_PER_USEC * 4)

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*! \brief      Resource manager preference for setting interval. */
enum
{
  SCH_RM_PREF_PERFORMANCE,            /*!< Performance is preferred, search from minimum interval. */
  SCH_RM_PREF_CAPACITY                /*!< Capacity is preferred, search from maximum interval. */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Get reference time callback signature. */
typedef uint32_t (*GetRefTimeCb_t)(uint8_t handle, uint32_t *pDurUsec);

/*! \brief      Get topology reference time callback signature. */
typedef uint32_t (*GetTopRefTimeCb_t)(uint8_t handle);

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Resource manager */
void SchRmInit(void);
uint32_t SchRmCalcCommonPeriodicityUsec(uint32_t peerPerUsec);
bool_t SchRmAdd(uint8_t handle, uint8_t pref, uint32_t minUsec, uint32_t maxUsec, uint32_t durUsec, uint32_t *pInterUsec, GetRefTimeCb_t refTimeCb);
bool_t SchRmStartUpdate(uint8_t handle, uint32_t minUsec, uint32_t maxUsec, uint32_t perfPerUsec, uint32_t durUsec, uint32_t *pInterUsec);
void SchRmCommitUpdate(uint8_t handle);
void SchRmRemove(uint8_t handle);
uint32_t SchRmGetOffsetUsec(uint32_t maxOffsUsec, uint8_t handle, uint32_t refTime);

/* Topology manager */
void SchTmInit(void);
void SchTmAdd(uint8_t handle, uint32_t interUsec, uint32_t durUsec, bool_t movable, GetTopRefTimeCb_t refTimeCb);
void SchTmRemove(uint8_t handle);

/* BLE time utilities */
uint32_t SchBleCalcDataPktDurationUsec(uint8_t phy, uint16_t len);
uint32_t SchBleCalcAdvPktDurationUsec(uint8_t phy, uint8_t phyOptions, uint16_t len);
uint32_t SchBleCalcPerAdvDurationUsec(uint8_t txPhy, uint8_t fragLen, uint16_t addMafDelay, uint16_t len, bool_t worstCase, uint8_t phyOptions);
uint32_t SchBleCalcAuxPktDurationUsec(uint8_t phy, uint8_t phyOptions, uint16_t len);
void SchBleCalcAdvOpDuration(BbOpDesc_t *pBod, uint8_t fragLen);
bool_t SchBleGetNextMstConnDueTime(uint32_t *pDueTime);
uint32_t SchBleGetAlignedAuxOffsUsec(uint32_t auxOffsUsec);

#ifdef __cplusplus
};
#endif

#endif /* SCH_API_BLE_H */
