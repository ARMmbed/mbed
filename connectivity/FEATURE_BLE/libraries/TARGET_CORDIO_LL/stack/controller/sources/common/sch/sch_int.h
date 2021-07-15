/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Internal multi-protocol scheduler interface file.
 *
 *  Copyright (c) 2013-2019 ARM Ltd. All Rights Reserved.
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

#ifndef SCH_INT_H
#define SCH_INT_H

#include "sch_api.h"
#include "pal_bb.h"
#include "wsf_assert.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Maximum span of scheduler elements. Half of the boundary time. */
#define SCH_MAX_SPAN            ((BbGetBbTimerBoundaryUs() >> 1) + 1)

/*! \brief      Delay in microseconds to start timer. */
#define SCH_TIMER_DELAY_US      10

/*! \brief      Event queue size, must be multiple of ^2. */
#define SCH_EVT_Q_SIZE          4

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*! \brief      Scheduler states. */
typedef enum
{
  SCH_STATE_IDLE,               /*!< Scheduler idle state. */
  SCH_STATE_LOAD,               /*!< Scheduler load delay state. */
  SCH_STATE_EXECUTE,            /*!< Scheduler execute in progress state. */
  SCH_STATE_UNLOAD              /*!< Scheduler unload state (BOD completion). */
} schState_t;

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Scheduler control block. */
typedef struct
{
  schState_t state:8;           /*!< Current scheduler state. */
  wsfHandlerId_t handlerId;     /*!< System event handler ID. */

  struct
  {
    size_t prodIdx;             /*!< Producer index, must be atomic size. */
    size_t consIdx;             /*!< Consumer index, must be atomic size. */
    uint8_t events[SCH_EVT_Q_SIZE];
                                /*!< Circular queue of events. */
  } eventQ;                     /*!< Lock-free event queue. */

  BbOpDesc_t *pHead;            /*!< Head element of scheduled list of BOD. */
  BbOpDesc_t *pTail;            /*!< Tail element of scheduled list of BOD. */

  uint16_t schHandlerWatermarkUsec; /*!< Statistics: Handler duration watermark in microseconds. */
} SchCtrlBlk_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

extern SchCtrlBlk_t schCb;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void schSetBodStartEvent(void);
void schSetBodAbortEvent(void);
void schRemoveHead(void);

/*************************************************************************************************/
/*!
 *  \brief      Is BOD due time in the future.
 *
 *  \param      pBod    Target BOD.
 *
 *  \return     TRUE if BOD time is in the future, FALSE otherwise.
 */
/*************************************************************************************************/
static inline bool_t schDueTimeInFuture(BbOpDesc_t *pBod)
{
  const uint32_t curTime = PalBbGetCurrentTime();

  return (BbGetTargetTimeDelta(pBod->dueUsec, curTime) > 0);
}

#ifdef __cplusplus
};
#endif

#endif /* SCH_INT_H */
