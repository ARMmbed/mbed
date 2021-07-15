/*************************************************************************************************/
/*!
 *  \file   wsf_os.h
 *
 *  \brief  Software foundation OS API.
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
#ifndef WSF_OS_H
#define WSF_OS_H

#include "wsf_types.h"
#include "wsf_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup WSF_OS_API
 *  \{ */

/**************************************************************************************************
  Configuration
**************************************************************************************************/

/*! \brief OS Diagnostics */
#ifndef WSF_OS_DIAG
#define WSF_OS_DIAG                             FALSE
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief Derive task from handler ID */
#define WSF_TASK_FROM_ID(handlerID)       (((handlerID) >> 4) & 0x0F)

/*! \brief Derive handler from handler ID */
#define WSF_HANDLER_FROM_ID(handlerID)    ((handlerID) & 0x0F)

/*! \brief Invalid Task Identifier */
#define WSF_INVALID_TASK_ID                     0xFF

/*! \brief Get Diagnostic Task Identifier */
#if WSF_OS_DIAG == TRUE
#define WSF_OS_GET_ACTIVE_HANDLER_ID()          WsfActiveHandler
#else
#define WSF_OS_GET_ACTIVE_HANDLER_ID()          WSF_INVALID_TASK_ID
#endif /* WSF_OS_DIAG */

/** @name WSF Task Events
 *
 */
/**@{*/
#define WSF_MSG_QUEUE_EVENT   0x01        /*!< \brief Message queued for event handler */
#define WSF_TIMER_EVENT       0x02        /*!< \brief Timer expired for event handler */
#define WSF_HANDLER_EVENT     0x04        /*!< \brief Event set for event handler */
/**@}*/

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief Event handler ID data type */
typedef uint8_t  wsfHandlerId_t;

/*! \brief Event handler event mask data type */
typedef uint16_t  wsfEventMask_t;

/*! \brief Task ID data type */
typedef wsfHandlerId_t  wsfTaskId_t;

/*! \brief Task event mask data type */
typedef uint8_t wsfTaskEvent_t;

/*! \brief      Idle check function. */
typedef bool_t (*WsfOsIdleHandler_t)(void);

/**************************************************************************************************
  External Variables
**************************************************************************************************/

/*! \brief Diagnostic Task Identifier */
extern wsfHandlerId_t WsfActiveHandler;

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief Common message structure passed to event handler */
typedef struct
{
  uint16_t        param;          /*!< \brief General purpose parameter passed to event handler */
  uint8_t         event;          /*!< \brief General purpose event value passed to event handler */
  uint8_t         status;         /*!< \brief General purpose status value passed to event handler */
} wsfMsgHdr_t;

/**************************************************************************************************
  Callback Function Types
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Event handler callback function.
 *
 *  \param  event    Mask of events set for the event handler.
 *  \param  pMsg     Pointer to message for the event handler.
 */
/*************************************************************************************************/
typedef void (*wsfEventHandler_t)(wsfEventMask_t event, wsfMsgHdr_t *pMsg);

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Set an event for an event handler.
 *
 *  \param  handlerId   Handler ID.
 *  \param  event       Event or events to set.
 */
/*************************************************************************************************/
void WsfSetEvent(wsfHandlerId_t handlerId, wsfEventMask_t event);

/*************************************************************************************************/
/*!
 *  \brief  Lock task scheduling.
 */
/*************************************************************************************************/
void WsfTaskLock(void);

/*************************************************************************************************/
/*!
 *  \brief  Unlock task scheduling.
 */
/*************************************************************************************************/
void WsfTaskUnlock(void);

/*************************************************************************************************/
/*!
 *  \brief  Set the task used by the given handler as ready to run.
 *
 *  \param  handlerId   Event handler ID.
 *  \param  event       Task event mask.
 */
/*************************************************************************************************/
void WsfTaskSetReady(wsfHandlerId_t handlerId, wsfTaskEvent_t event);

/*************************************************************************************************/
/*!
 *  \brief  Return the task message queue used by the given handler.
 *
 *  \param  handlerId   Event handler ID.
 *
 *  \return Task message queue.
 */
/*************************************************************************************************/
wsfQueue_t *WsfTaskMsgQueue(wsfHandlerId_t handlerId);

/*************************************************************************************************/
/*!
 *  \brief  Set the next WSF handler function in the WSF OS handler array.  This function
 *          should only be called as part of the OS initialization procedure.
 *
 *  \param  handler    WSF handler function.
 *
 *  \return WSF handler ID for this handler.
 */
/*************************************************************************************************/
wsfHandlerId_t WsfOsSetNextHandler(wsfEventHandler_t handler);

/*************************************************************************************************/
/*!
*  \brief  Initialize OS control structure.
*
*  \return None.
*/
/*************************************************************************************************/
void WsfOsInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Check if WSF is ready to sleep.  This function should be called when interrupts
 *          are disabled.
 *
 *  \return Return TRUE if there are no pending WSF task events set, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t wsfOsReadyToSleep(void);

/*************************************************************************************************/
/*!
 *  \brief  Event dispatched.  Designed to be called repeatedly from infinite loop.
 */
/*************************************************************************************************/
void wsfOsDispatcher(void);

/*! \} */    /* WSF_OS_API */

#ifdef __cplusplus
};
#endif

#endif /* WSF_OS_H */
