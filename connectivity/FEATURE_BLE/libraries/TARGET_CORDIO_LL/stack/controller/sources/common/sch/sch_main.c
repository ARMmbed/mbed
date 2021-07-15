/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Operation list maintenance implementation file.
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

#include "sch_int.h"
#include "pal_timer.h"
#include "bb_api.h"
#include "wsf_trace.h"
#include <string.h>

/**************************************************************************************************
  Constants
**************************************************************************************************/

#ifndef SCH_ENABLE_CURTAIL
/*! \brief  Enable curtail feature. */
#define SCH_ENABLE_CURTAIL  FALSE
#endif

#ifndef SCH_ENABLE_DEBUG
/*! \brief  Enable scheduler debug statements. */
#define SCH_ENABLE_DEBUG    FALSE
#endif

/*! \brief  Scheduler task events. */
enum
{
  SCH_EVENT_BOD_EXECUTE,                /*!< BOD execute event. */
  SCH_EVENT_BOD_COMPLETE,               /*!< BOD completion event. */
  SCH_EVENT_BOD_ABORT,                  /*!< BOD abort event. */
  SCH_EVENT_BOD_RELOAD                  /*!< BOD reload event. */
};

#if SCH_ENABLE_DEBUG
#define SCH_DBG_TRACE_INFO0(m)          LL_TRACE_INFO0(m)
#define SCH_DBG_TRACE_INFO1(m, v1)      LL_TRACE_INFO1(m, v1)
#include "sch_debug.h"
#define SCH_DBG_TRACE_EVENT(e)          schTraceEventTriggered(PalBbGetCurrentTime(), schCb.state, e)
#define SCH_DBG_TRACE_STATE(t, e)       schTraceStateEvent(t, schCb.state, e)
#else
#define SCH_DBG_TRACE_INFO0(m)
#define SCH_DBG_TRACE_INFO1(m, v1)
#define SCH_DBG_TRACE_EVENT(e)
#define SCH_DBG_TRACE_STATE(t, e)
#endif

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Scheduler control block. */
SchCtrlBlk_t schCb;

/**************************************************************************************************
  Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Push an event to the event queue.
 *
 *  \param      event   Event ID.
 */
/*************************************************************************************************/
static inline void schEventEnqueue(uint8_t event)
{
  /* Use volatile to create memory barrier. */
  volatile uint32_t prodIdx = (schCb.eventQ.prodIdx + 1) & (SCH_EVT_Q_SIZE - 1);

  /* Queue must never be full. */
  WSF_ASSERT(prodIdx != schCb.eventQ.consIdx);

  schCb.eventQ.events[schCb.eventQ.prodIdx] = event;
  schCb.eventQ.prodIdx = prodIdx;

  /* Signal the SM to run. */
  WsfSetEvent(schCb.handlerId, /* not used */ 1);
}

/*************************************************************************************************/
/*!
 *  \brief      Dequeue an event from the event queue.
 *
 *  \param      pEvent  Event ID return value.
 */
/*************************************************************************************************/
static inline bool_t schEventDequeue(uint8_t *pEvent)
{
  if (schCb.eventQ.consIdx == schCb.eventQ.prodIdx)
  {
    /* Queue empty. */
    return FALSE;
  }

  /* Use volatile to create memory barrier. */
  volatile uint32_t consIdx = (schCb.eventQ.consIdx + 1) & (SCH_EVT_Q_SIZE - 1);

  *pEvent = schCb.eventQ.events[schCb.eventQ.consIdx];
  schCb.eventQ.consIdx = consIdx;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Start the BOD operation loop.
 */
/*************************************************************************************************/
void schSetBodStartEvent(void)
{
  switch (schCb.state)
  {
  case SCH_STATE_LOAD:
    PalTimerStop();
    /* Fallthrough */
  case SCH_STATE_IDLE:
    SCH_DBG_TRACE_EVENT(SCH_EVENT_BOD_RELOAD);

    schEventEnqueue(SCH_EVENT_BOD_RELOAD);
    break;

  case SCH_STATE_UNLOAD:
    /* No action required; next head element loaded by SchHandler(). */
    return;

  default:
    LL_TRACE_WARN1("Unexpected scheduler state for event=SCH_EVENT_BOD_RELOAD, pBod=0x%08x", schCb.pHead);
    break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set the BOD abort event.
 */
/*************************************************************************************************/
void schSetBodAbortEvent(void)
{
  switch (schCb.state)
  {
  case SCH_STATE_LOAD:
  case SCH_STATE_EXECUTE:
    SCH_DBG_TRACE_EVENT(SCH_EVENT_BOD_ABORT);

    PalTimerStop();
    schEventEnqueue(SCH_EVENT_BOD_ABORT);
    break;

  default:
    LL_TRACE_WARN1("Unexpected scheduler state for event=SCH_EVENT_BOD_ABORT, pBod=0x%08x", schCb.pHead);
    break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set the BOD execute event.
 */
/*************************************************************************************************/
static void schSetBodExecuteEvent(void)
{
  switch (schCb.state)
  {
  case SCH_STATE_LOAD:
    SCH_DBG_TRACE_EVENT(SCH_EVENT_BOD_EXECUTE);

    schEventEnqueue(SCH_EVENT_BOD_EXECUTE);
    break;

#if SCH_ENABLE_CURTAIL
  case SCH_STATE_EXECUTE:
    SCH_DBG_TRACE_EVENT(SCH_EVENT_BOD_COMPLETE);

    schEventEnqueue(SCH_EVENT_BOD_COMPLETE);
    break;
#endif

  default:
    LL_TRACE_WARN1("Unexpected scheduler state for timer completion, pBod=0x%08x", schCb.pHead);
    break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set the BOD complete event.
 */
/*************************************************************************************************/
static void schSetBodCompleteEvent(void)
{
  switch (schCb.state)
  {
  case SCH_STATE_EXECUTE:
    SCH_DBG_TRACE_EVENT(SCH_EVENT_BOD_COMPLETE);

    schEventEnqueue(SCH_EVENT_BOD_COMPLETE);
    break;

  default:
    LL_TRACE_WARN1("Unexpected scheduler state for event=SCH_EVENT_BOD_COMPLETE, pBod=0x%08x", schCb.pHead);
    break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Calculate the execute delay time of a BOD.
 *
 *  \param      pBod    Target BOD.
 *
 *  \return     Time in microseconds when the given BOD should execute.
 */
/*************************************************************************************************/
static inline bool_t schCalcExecuteDelayUsec(BbOpDesc_t *pBod, uint32_t *pExecTime)
{
  const uint32_t curTime = PalBbGetCurrentTime();
  const uint32_t setupDelayUsec = BbGetSchSetupDelayUs();

  uint32_t deltaUsec = BbGetTargetTimeDelta(pBod->dueUsec, curTime);

  if (deltaUsec < (setupDelayUsec + SCH_TIMER_DELAY_US))
  {
    /* Inadequate delta to start timer. */
    return FALSE;
  }

  *pExecTime = pBod->dueUsec - setupDelayUsec;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Enable curtail event.
 */
/*************************************************************************************************/
static void schEnableCurtailEvent(void)
{
#if SCH_ENABLE_CURTAIL
  BbOpDesc_t* pNextBod = schCb.pHead->pNext;

  /* Check curtail operation is needed. */
  if (pNextBod)
  {
    /* Only enable curtail if current BOD is lower priority than next. */
    if (schCb.pHead->reschPolicy < pNextBod->reschPolicy)
    {
      uint32_t execDlyUsec;

      schCalcExecuteDelayUsec(pNextBod, &execDlyUsec);

      if (execDlyUsec)
      {
        PalTimerStart(execDlyUsec);
      }
    }
  }
#endif
}

/*************************************************************************************************/
/*!
 *  \brief      Execute head BOD.
 */
/*************************************************************************************************/
static void schExecuteHeadBod(void)
{
  BbOpDesc_t *pBod = schCb.pHead;

  if (schDueTimeInFuture(pBod) || (pBod->recoverable == TRUE))
  {
    /* Setup BB services. */
    BbExecuteBod(pBod);

    if (BbGetBodTerminateFlag() == FALSE)
    {
      /* BOD executed successfully. */
      schEnableCurtailEvent();
    }
    else
    {
      LL_TRACE_WARN1("!!! BOD terminated on startup, pBod=0x%08x", pBod);
      schSetBodCompleteEvent();
    }
  }
  else
  {
    LL_TRACE_ERR1("!!! BOD in the past, execution aborted, pBod=0x%08x", pBod);
    LL_TRACE_ERR1("!!!                                     dueUsec=%u", pBod->dueUsec);
    LL_TRACE_ERR1("!!!                                     curTime=%u", PalBbGetCurrentTime());
    schSetBodAbortEvent();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Delay BOD load.
 *
 *  \return     TRUE if load is delayed, FALSE if load is immediate.
 */
/*************************************************************************************************/
static bool_t schDelayBodLoad(void)
{
  BbOpDesc_t* pBod = schCb.pHead;
  WSF_ASSERT(pBod);

  uint32_t execTime;

  if (schCalcExecuteDelayUsec(pBod, &execTime))
  {
    /* Delay BOD load. */
    PalTimerStart(execTime);

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize the scheduler subsystem.
 */
/*************************************************************************************************/
void SchInit(void)
{
  memset(&schCb, 0, sizeof(schCb));
  SchReset();
  PalTimerInit(schSetBodExecuteEvent);
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize the scheduler subsystem.
 *
 *  \param      handlerId  WSF handler ID.
 *
 *  \note       This initialization to used to enable task-based scheduler completions. For
 *              ISR-based scheduler completions, do not call this routine. Instead install an
 *              ISR which calls SchHandler() on BOD completions.
 */
/*************************************************************************************************/
void SchHandlerInit(wsfHandlerId_t handlerId)
{
  SchInit();

  schCb.handlerId = handlerId;

  BbRegister(schSetBodCompleteEvent);
}

/*************************************************************************************************/
/*!
 *  \brief      Reset the scheduler subsystem.
 */
/*************************************************************************************************/
void SchReset(void)
{
  schCb.state = SCH_STATE_IDLE;
  schCb.pHead = NULL;
  schCb.pTail = NULL;
  schCb.schHandlerWatermarkUsec = 0;
}

/*************************************************************************************************/
/*!
 *  \brief      Scheduler message dispatch handler.
 *
 *  \param      event       WSF event.
 *  \param      pMsg        WSF message.
 */
/*************************************************************************************************/
void SchHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
  /* Unused parameters */
  (void)event;
  (void)pMsg;

  /* Assume scheduler clock started. */
  uint32_t startTime = PalBbGetCurrentTime();

  uint8_t schEvent;

  while (schEventDequeue(&schEvent))
  {
    BbOpDesc_t *pBod = schCb.pHead;
    if (pBod == NULL)
    {
      LL_TRACE_ERR0("    !!! SCH SM !!!  out of sequence with pHead=NULL");
      schCb.state = SCH_STATE_IDLE;
      break;
    }

    SCH_DBG_TRACE_STATE(startTime, schEvent);

    switch (schCb.state)
    {
      case SCH_STATE_LOAD:
        switch (schEvent)
        {
          case SCH_EVENT_BOD_EXECUTE:
            schCb.state = SCH_STATE_EXECUTE;
            schExecuteHeadBod();
            break;

          case SCH_EVENT_BOD_RELOAD:
            schCb.state = SCH_STATE_LOAD;
            if (!schDelayBodLoad())
            {
              /* Load BOD immediately. */
              schSetBodExecuteEvent();
            }
            break;

          case SCH_EVENT_BOD_ABORT:
            schRemoveHead();

            schCb.state = SCH_STATE_UNLOAD;
            if (pBod->abortCback)
            {
              pBod->abortCback(pBod);
            }

            if (schCb.pHead)
            {
              schCb.state = SCH_STATE_LOAD;
              if (!schDelayBodLoad())
              {
                /* Load BOD immediately. */
                schSetBodExecuteEvent();
              }
            }
            break;

          default:
            LL_TRACE_ERR1("    !!! SCH SM !!!  Unexpected scheduler event for pBod=0x%08x", pBod);
            break;
        }
        break;

      case SCH_STATE_EXECUTE:
        switch (schEvent)
        {
          case SCH_EVENT_BOD_COMPLETE:
            schRemoveHead();

            schCb.state = SCH_STATE_UNLOAD;
            if (pBod->endCback)
            {
              pBod->endCback(pBod);
            }

            if (schCb.pHead)
            {
              schCb.state = SCH_STATE_LOAD;
              if (!schDelayBodLoad())
              {
                /* Load BOD immediately. */
                schSetBodExecuteEvent();
              }
            }
            break;

          case SCH_EVENT_BOD_ABORT:
            schRemoveHead();

            schCb.state = SCH_STATE_UNLOAD;
            if (pBod->abortCback)
            {
              pBod->abortCback(pBod);
            }

            if (schCb.pHead)
            {
              schCb.state = SCH_STATE_LOAD;
              if (!schDelayBodLoad())
              {
                /* Load BOD immediately. */
                schSetBodExecuteEvent();
              }
            }
            break;

          default:
            LL_TRACE_ERR1("Unexpected scheduler event for pBod=0x%08x", pBod);
            break;
        }
        break;

      case SCH_STATE_IDLE:
        switch (schEvent)
        {
          case SCH_EVENT_BOD_RELOAD:
            schCb.state = SCH_STATE_LOAD;
            SCH_DBG_TRACE_INFO0("    *** SCH SM ***  scheduler loop started");
            if (!schDelayBodLoad())
            {
              /* Load BOD immediately. */
              schSetBodExecuteEvent();
            }
            break;

          default:
            LL_TRACE_ERR1("Unexpected scheduler for pBod=0x%08x", pBod);
            break;
        }
        break;

      default:
        break;
    }

    if (schCb.pHead == NULL)
    {
      SCH_DBG_TRACE_INFO0("    *** SCH SM ***  scheduler loop terminated");
      schCb.state = SCH_STATE_IDLE;
    }
  }

  uint32_t endTime = PalBbGetCurrentTime();
  uint32_t durUsec = BbGetTargetTimeDelta(endTime, startTime);
  if (schCb.schHandlerWatermarkUsec < durUsec)
  {
    schCb.schHandlerWatermarkUsec = durUsec;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get the scheduler handler watermark level.
 *
 *  \return     Watermark level in microseconds.
 */
/*************************************************************************************************/
uint16_t SchStatsGetHandlerWatermarkUsec(void)
{
  return schCb.schHandlerWatermarkUsec;
}
