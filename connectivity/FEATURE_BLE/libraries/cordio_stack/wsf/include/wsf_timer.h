/*************************************************************************************************/
/*!
 *  \file   wsf_timer.h
 *
 *  \brief  Timer service.
 *
 *  Copyright (c) 2009-2019 Arm Ltd. All Rights Reserved.
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
#ifndef WSF_TIMER_H
#define WSF_TIMER_H

#include "wsf_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup WSF_TIMER_API
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

#ifndef WSF_MS_PER_TICK
/*! \brief Default milliseconds per tick rate */
#define WSF_MS_PER_TICK   10
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief Timer ticks data type */
typedef uint32_t wsfTimerTicks_t;

/*! \brief Timer structure */
typedef struct wsfTimer_tag
{
  struct wsfTimer_tag *pNext;             /*!< \brief pointer to next timer in queue */
  wsfMsgHdr_t         msg;                /*!< \brief application-defined timer event parameters */
  wsfTimerTicks_t     ticks;              /*!< \brief number of ticks until expiration */
  wsfHandlerId_t      handlerId;          /*!< \brief event handler for this timer */
  bool_t              isStarted;          /*!< \brief TRUE if timer has been started */
} wsfTimer_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Initialize the timer service.  This function should only be called once
 *          upon system initialization.
 */
/*************************************************************************************************/
void WsfTimerInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Start a timer in units of seconds.  Before this function is called parameter
 *          pTimer->handlerId must be set to the event handler for this timer and parameter
 *          pTimer->msg must be set to any application-defined timer event parameters.
 *
 *  \param  pTimer  Pointer to timer.
 *  \param  sec     Seconds until expiration.
 */
/*************************************************************************************************/
void WsfTimerStartSec(wsfTimer_t *pTimer, wsfTimerTicks_t sec);

/*************************************************************************************************/
/*!
 *  \brief  Start a timer in units of milliseconds.
 *
 *  \param  pTimer  Pointer to timer.
 *  \param  ms      Milliseconds until expiration.
 */
/*************************************************************************************************/
void WsfTimerStartMs(wsfTimer_t *pTimer, wsfTimerTicks_t ms);

/*************************************************************************************************/
/*!
 *  \brief  Stop a timer.
 *
 *  \param  pTimer  Pointer to timer.
 */
/*************************************************************************************************/
void WsfTimerStop(wsfTimer_t *pTimer);

/*************************************************************************************************/
/*!
 *  \brief  Update the timer service with the number of elapsed ticks.  This function is
 *          typically called only from timer porting code.
 *
 *  \param  ticks  Number of ticks since last update.
 */
/*************************************************************************************************/
void WsfTimerUpdate(wsfTimerTicks_t ticks);

/*************************************************************************************************/
/*!
 *  \brief  Return the number of ticks until the next timer expiration.  Note that this
 *          function can return zero even if a timer is running, indicating a timer
 *          has expired but has not yet been serviced.
 *
 *  \param  pTimerRunning   Returns TRUE if a timer is running, FALSE if no timers running.
 *
 *  \return The number of ticks until the next timer expiration.
 */
/*************************************************************************************************/
wsfTimerTicks_t WsfTimerNextExpiration(bool_t *pTimerRunning);

/*************************************************************************************************/
/*!
 *  \brief  Service expired timers for the given task.  This function is typically called only
 *          WSF OS porting code.
 *
 *  \param  taskId      OS Task ID of task servicing timers.
 *
 *  \return Pointer to next expired timer or NULL if there are no expired timers.
 */
/*************************************************************************************************/
wsfTimer_t *WsfTimerServiceExpired(wsfTaskId_t taskId);

/*! \} */    /* WSF_TIMER_API */

#ifdef __cplusplus
};
#endif

#endif /* WSF_TIMER_H */
