/*************************************************************************************************/
/*!
 *  \file   wsf_os.c
 *
 *  \brief  Software foundation OS main module.
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

#ifdef __IAR_SYSTEMS_ICC__
#include <intrinsics.h>
#endif
#include <string.h>
#include "wsf_types.h"
#include "wsf_os.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "wsf_timer.h"
#include "wsf_queue.h"
#include "wsf_buf.h"
#include "wsf_msg.h"
#include "wsf_cs.h"

#include "wsf_mbed_os_adaptation.h"

#if defined (RTOS_CMSIS_RTX) && (RTOS_CMSIS_RTX == 1)
#include "cmsis_os2.h"
#endif

/**************************************************************************************************
  Compile time assert checks
**************************************************************************************************/

WSF_CT_ASSERT(sizeof(uint8_t) == 1);
WSF_CT_ASSERT(sizeof(uint16_t) == 2);
WSF_CT_ASSERT(sizeof(uint32_t) == 4);

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* maximum number of event handlers per task */
#ifndef WSF_MAX_HANDLERS
#define WSF_MAX_HANDLERS                          16
#endif

#if WSF_OS_DIAG == TRUE
#define WSF_OS_SET_ACTIVE_HANDLER_ID(id)          (WsfActiveHandler = id);
#else
#define WSF_OS_SET_ACTIVE_HANDLER_ID(id)
#endif /* WSF_OS_DIAG */

#if defined (RTOS_CMSIS_RTX) && (RTOS_CMSIS_RTX == 1)
/*! \brief Thread sleep flag */
#define WSF_OS_THREAD_SLEEP_WAKEUP_FLAG           0x0001
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief  Task structure */
typedef struct
{
  wsfEventHandler_t     handler[WSF_MAX_HANDLERS];
  wsfEventMask_t        handlerEventMask[WSF_MAX_HANDLERS];
  wsfQueue_t            msgQueue;
  wsfTaskEvent_t        taskEventMask;
  uint8_t               numHandler;
} wsfOsTask_t;

/*! \brief  OS structure */
typedef struct
{
  wsfOsTask_t           task;
  /* PORTING: sleep checking funcs removed as not needed as handled by mbedos */
} wsfOs_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! \brief  OS context. */
wsfOs_t wsfOs;

#if WSF_OS_DIAG == TRUE
/*! Active task handler ID. */
wsfHandlerId_t WsfActiveHandler;
#endif /* WSF_OS_DIAG */

#if defined (RTOS_CMSIS_RTX) && (RTOS_CMSIS_RTX == 1)
static osThreadId_t wsfOsThreadId;
#endif

/*************************************************************************************************/
/*!
 *  \brief  Lock task scheduling.
 */
/*************************************************************************************************/
void WsfTaskLock(void)
{
  WsfCsEnter();
}

/*************************************************************************************************/
/*!
 *  \brief  Unock task scheduling.
 */
/*************************************************************************************************/
void WsfTaskUnlock(void)
{
  WsfCsExit();
}

/*************************************************************************************************/
/*!
 *  \brief  Set an event for an event handler.
 *
 *  \param  handlerId   Handler ID.
 *  \param  event       Event or events to set.
 */
/*************************************************************************************************/
void WsfSetEvent(wsfHandlerId_t handlerId, wsfEventMask_t event)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(WSF_HANDLER_FROM_ID(handlerId) < WSF_MAX_HANDLERS);

  WSF_TRACE_INFO2("WsfSetEvent handlerId:%u event:%u", handlerId, event);

  WSF_CS_ENTER(cs);
  wsfOs.task.handlerEventMask[WSF_HANDLER_FROM_ID(handlerId)] |= event;
  wsfOs.task.taskEventMask |= WSF_HANDLER_EVENT;
  WSF_CS_EXIT(cs);

  wsf_mbed_ble_signal_event();
}

/*************************************************************************************************/
/*!
 *  \brief  Set the task used by the given handler as ready to run.
 *
 *  \param  handlerId   Event handler ID.
 *  \param  event       Task event mask.
 */
/*************************************************************************************************/
void WsfTaskSetReady(wsfHandlerId_t handlerId, wsfTaskEvent_t event)
{
  /* Unused parameter */
  (void)handlerId;

  WSF_CS_INIT(cs);

  WSF_CS_ENTER(cs);
  wsfOs.task.taskEventMask |= event;
  WSF_CS_EXIT(cs);
  
  wsf_mbed_ble_signal_event();
}

/*************************************************************************************************/
/*!
 *  \brief  Return the message queue used by the given handler.
 *
 *  \param  handlerId   Event handler ID.
 *
 *  \return Task message queue.
 */
/*************************************************************************************************/
wsfQueue_t *WsfTaskMsgQueue(wsfHandlerId_t handlerId)
{
  /* Unused parameter */
  (void)handlerId;

  return &(wsfOs.task.msgQueue);
}

/*************************************************************************************************/
/*!
 *  \brief  Set the next WSF handler function in the WSF OS handler array.  This function
 *          should only be called as part of the stack initialization procedure.
 *
 *  \param  handler    WSF handler function.
 *
 *  \return WSF handler ID for this handler.
 */
/*************************************************************************************************/
wsfHandlerId_t WsfOsSetNextHandler(wsfEventHandler_t handler)
{
  wsfHandlerId_t handlerId = wsfOs.task.numHandler++;

  WSF_ASSERT(handlerId < WSF_MAX_HANDLERS);

  wsfOs.task.handler[handlerId] = handler;

  return handlerId;
}

/*************************************************************************************************/
/*!
 *  \brief  Check if WSF is ready to sleep.  This function should be called when interrupts
 *          are disabled.
 *
 *  \return Return TRUE if there are no pending WSF task events set, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t wsfOsReadyToSleep(void)
{
  return (wsfOs.task.taskEventMask == 0);
}
/*************************************************************************************************/
/*!
*  \brief  Initialize OS control structure.
*
*  \return None.
*/
/*************************************************************************************************/
void WsfOsInit(void)
{
  memset(&wsfOs, 0, sizeof(wsfOs));

#if defined (RTOS_CMSIS_RTX) && (RTOS_CMSIS_RTX == 1)
  osKernelInitialize();                        /* Initialize CMSIS-RTOS. */
#endif
}

/*************************************************************************************************/
/*!
 *  \brief  Event dispatched.  Designed to be called repeatedly from infinite loop.
 */
/*************************************************************************************************/
void wsfOsDispatcher(void)
{
  wsfOsTask_t       *pTask;
  void              *pMsg;
  wsfTimer_t        *pTimer;
  wsfEventMask_t    eventMask;
  wsfTaskEvent_t    taskEventMask;
  wsfHandlerId_t    handlerId;
  uint8_t           i;

  WSF_CS_INIT(cs);

  pTask = &wsfOs.task;

  /* get and then clear task event mask */
  WSF_CS_ENTER(cs);
  taskEventMask = pTask->taskEventMask;
  pTask->taskEventMask = 0;
  WSF_CS_EXIT(cs);

  if (taskEventMask & WSF_MSG_QUEUE_EVENT)
  {
    /* handle msg queue */
    while ((pMsg = WsfMsgDeq(&pTask->msgQueue, &handlerId)) != NULL)
    {
      WSF_ASSERT(handlerId < WSF_MAX_HANDLERS);
      WSF_OS_SET_ACTIVE_HANDLER_ID(handlerId);
      (*pTask->handler[handlerId])(0, pMsg);
      WsfMsgFree(pMsg);
    }
  }

  if (taskEventMask & WSF_TIMER_EVENT)
  {
    /* service timers */
    while ((pTimer = WsfTimerServiceExpired(0)) != NULL)
    {
      WSF_ASSERT(pTimer->handlerId < WSF_MAX_HANDLERS);
      WSF_OS_SET_ACTIVE_HANDLER_ID(pTimer->handlerId);
      (*pTask->handler[pTimer->handlerId])(0, &pTimer->msg);
    }
  }

  if (taskEventMask & WSF_HANDLER_EVENT)
  {
    /* service handlers */
    for (i = 0; i < WSF_MAX_HANDLERS; i++)
    {
      if ((pTask->handlerEventMask[i] != 0) && (pTask->handler[i] != NULL))
      {
        WSF_CS_ENTER(cs);
        eventMask = pTask->handlerEventMask[i];
        pTask->handlerEventMask[i] = 0;
        WSF_OS_SET_ACTIVE_HANDLER_ID(i);
        WSF_CS_EXIT(cs);

        (*pTask->handler[i])(eventMask, NULL);
      }
    }
  }
}

#if defined (RTOS_CMSIS_RTX) && (RTOS_CMSIS_RTX == 1)
/*************************************************************************************************/
/*!
 *  \brief  Idle thread.
 *
 *  \param  pArg   Pointer to argument.
 */
/*************************************************************************************************/
void osRtxIdleThread(void *pArg)
{
  bool_t activeFlag = FALSE;

  (void) pArg;

  while(TRUE)
  {
    activeFlag = FALSE;

    for (unsigned int i = 0; i < wsfOs.numFunc; i++)
    {
      if (wsfOs.sleepCheckFuncs[i])
      {
        activeFlag |= wsfOs.sleepCheckFuncs[i]();
      }
    }

    if (!activeFlag)
    {
      WsfTimerSleep();
    }
    osThreadFlagsSet(wsfOsThreadId, WSF_OS_THREAD_SLEEP_WAKEUP_FLAG);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Main thread.
 *
 *  \param  pArg   Pointer to argument.
 */
/*************************************************************************************************/
void wsfThread(void *pArg)
{
  (void) pArg;

  while(TRUE)
  {
    WsfTimerSleepUpdate();
    wsfOsDispatcher();
    osThreadFlagsWait(WSF_OS_THREAD_SLEEP_WAKEUP_FLAG, osFlagsWaitAny ,osWaitForever);
  }
}
#endif

/* PORTING: main loop and sleep are redundant as the they are handled by mbedos */

