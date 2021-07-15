/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Operation list maintenance implementation file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2021 Packetcraft, Inc.
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
#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Total BOD time including setup delay. */
#define SCH_TOTAL_DUR(p)                (p->minDurUsec + BbGetSchSetupDelayUs())

/*! \brief      Time immediately after the given BOD. */
#define SCH_END_TIME(p)                 (p->dueUsec + SCH_TOTAL_DUR(p))

/*! \brief      Is BOD[a] due time before BOD[b] due time (rt = reference time). */
#define SCH_IS_DUE_BEFORE(a, b)         (BbGetTargetTimeDelta(((b)->dueUsec), ((a)->dueUsec)) > 0)

/*! \brief      Is BOD[a] completion time before BOD[b] due time (rt = reference time). */
#define SCH_IS_DONE_BEFORE(a, b)        (BbGetTargetTimeDelta(SCH_END_TIME(a), ((b)->dueUsec)) == 0)

/*! \brief      Is BOD[a] due time after BOD[b] completion time. */
#define SCH_IS_DUE_AFTER(a, b)          (BbGetTargetTimeDelta(SCH_END_TIME(b), ((a)->dueUsec)) == 0)

#ifndef SCH_TRACE_ENABLE
/*! \brief      Enable scheduler trace. */
#define SCH_TRACE_ENABLE                FALSE
#endif

#if SCH_TRACE_ENABLE
#define SCH_TRACE_INFO0(msg)             WSF_TRACE0("SCH", "INFO", msg)
#define SCH_TRACE_INFO1(msg, var1)       WSF_TRACE1("SCH", "INFO", msg, var1)
#define SCH_TRACE_WARN0(msg)             WSF_TRACE0("SCH", "WARN", msg)
#define SCH_TRACE_WARN1(msg, var1)       WSF_TRACE1("SCH", "WARN", msg, var1)
#else
/*! \brief      Information trace with 0 parameters. */
#define SCH_TRACE_INFO0(msg)
/*! \brief      Information trace with 1 parameters. */
#define SCH_TRACE_INFO1(msg, var1)
/*! \brief      Warning trace with 0 parameters. */
#define SCH_TRACE_WARN0(msg)
/*! \brief      Warning trace with 1 parameters. */
#define SCH_TRACE_WARN1(msg, var1)
#endif

#ifndef SCH_CHECK_LIST_INTEGRITY
/*! \brief      Check list requirements upon insertions and removals. */
#define SCH_CHECK_LIST_INTEGRITY        FALSE
#endif

#if (SCH_CHECK_LIST_INTEGRITY)
/*************************************************************************************************/
/*!
 *  \brief      Ensure BOD is not already inserted in the list.
 *
 *  \param      pBod    Target BOD.
 */
/*************************************************************************************************/
static inline void SchCheckIsNotInserted(BbOpDesc_t *pBod)
{
  BbOpDesc_t *pCur = schCb.pHead;

  while (pCur != NULL)
  {
    WSF_ASSERT(pCur != pBod);
    pCur = pCur->pNext;
  }
}
#endif

/*************************************************************************************************/
/*!
 *  \brief      Check whether BOD is inserted in the list.
 *
 *  \param      pBod    Target BOD.
 *
 *  \return     True if BOD is inserted.
 */
/*************************************************************************************************/
static inline bool_t SchCheckIsInserted(BbOpDesc_t *pBod)
{
  BbOpDesc_t *pCur = schCb.pHead;

  while (pCur != NULL)
  {
    if (pCur == pBod)
    {
      /* pBod found in the list. */
      return TRUE;
    }
    pCur = pCur->pNext;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Check and return the status of whether it is ok to cancel the head BOD.
 *
 *  \return     TRUE if successful, FALSE otherwise.
 */
/*************************************************************************************************/
static inline bool_t schCheckCancelHead(void)
{
  return (schCb.state != SCH_STATE_EXECUTE);
}

/*************************************************************************************************/
/*!
 *  \brief      Insert item into an empty list.
 *
 *  \param      pItem   Item to insert.
 */
/*************************************************************************************************/
static inline void schInsertToEmptyList(BbOpDesc_t *pItem)
{
  WSF_ASSERT(pItem);
  WSF_ASSERT(schCb.pHead == NULL);
  WSF_ASSERT(schCb.pTail == NULL);

  schCb.pHead = pItem;
  schCb.pTail = pItem;

  pItem->pPrev = NULL;
  pItem->pNext = NULL;

  SCH_TRACE_INFO1("++| schInsertToEmptyList |++ pBod=0x%08x", (uint32_t)pItem);
  SCH_TRACE_INFO1("++|                      |++     .dueUsec=%u", pItem->dueUsec);
  SCH_TRACE_INFO1("++|                      |++     .minDurUsec=%u", pItem->minDurUsec);
  SCH_TRACE_INFO1("++|                      |++     .maxDurUsec=%u", pItem->maxDurUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Insert item before the given target position.
 *
 *  \param      pItem   Item to insert.
 *  \param      pTgt    Target position.
 */
/*************************************************************************************************/
static inline void schInsertBefore(BbOpDesc_t *pItem, BbOpDesc_t *pTgt)
{
  WSF_ASSERT(pTgt);
  WSF_ASSERT(pItem);

  pItem->pNext = pTgt;
  pItem->pPrev = pTgt->pPrev;
  pTgt->pPrev = pItem;

  if (pItem->pPrev)
  {
    pItem->pPrev->pNext = pItem;
  }
  else
  {
    schCb.pHead = pItem;
  }

  SCH_TRACE_INFO1("++| schInsertBefore      |++ pBod=0x%08x", (uint32_t)pItem);
  SCH_TRACE_INFO1("++|                      |++     .dueUsec=%u", pItem->dueUsec);
  SCH_TRACE_INFO1("++|                      |++     .minDurUsec=%u", pItem->minDurUsec);
  SCH_TRACE_INFO1("++|                      |++     .maxDurUsec=%u", pItem->maxDurUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Insert item after the given target position.
 *
 *  \param      pItem   Item to insert.
 *  \param      pTgt    Target position.
 */
/*************************************************************************************************/
static inline void schInsertAfter(BbOpDesc_t *pItem, BbOpDesc_t *pTgt)
{
  WSF_ASSERT(pTgt);
  WSF_ASSERT(pItem);

  pItem->pPrev = pTgt;
  pItem->pNext = pTgt->pNext;
  pTgt->pNext = pItem;

  if (pItem->pNext)
  {
    pItem->pNext->pPrev = pItem;
  }
  else
  {
    schCb.pTail = pItem;
  }

  SCH_TRACE_INFO1("++| schInsertAfter       |++ pBod=0x%08x", (uint32_t)pItem);
  SCH_TRACE_INFO1("++|                      |++     .dueUsec=%u", pItem->dueUsec);
  SCH_TRACE_INFO1("++|                      |++     .minDurUsec=%u", pItem->minDurUsec);
  SCH_TRACE_INFO1("++|                      |++     .maxDurUsec=%u", pItem->maxDurUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Remove head item from BOD list.
 */
/*************************************************************************************************/
void schRemoveHead(void)
{
  WSF_ASSERT(schCb.pHead);

  schCb.pHead = schCb.pHead->pNext;

  if (schCb.pHead)
  {
    schCb.pHead->pPrev = NULL;
  }
  else
  {
    /* Now empty list */
    schCb.pTail = NULL;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Remove non-head item from BOD list.
 *
 *  \param      pBod      Element to remove.
 */
/*************************************************************************************************/
static void schRemoveMiddle(BbOpDesc_t *pBod)
{
  if (schCb.pTail == pBod)
  {
    /* Last element */
    schCb.pTail = schCb.pTail->pPrev;

    if (schCb.pTail)
    {
      schCb.pTail->pNext = NULL;
    }
    else
    {
      /* Now empty list */
      schCb.pHead = NULL;
    }
  }
  else
  {
    /* Linkage is intact. */
    WSF_ASSERT(pBod->pPrev);
    WSF_ASSERT(pBod->pNext);

    /* Middle element */
    pBod->pPrev->pNext = pBod->pNext;
    pBod->pNext->pPrev = pBod->pPrev;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check whether the conflict between BODs is resolvable.
 *
 *  \param      pItem           Item to insert.
 *  \param      pTgt            Target position.
 *  \param      conflictCback   Conflict action callback.
 *
 *  \return     TRUE if conflict is resolvable, FALSE otherwise.
 */
/*************************************************************************************************/
static bool_t SchIsBodConflictResolvable(BbOpDesc_t *pItem, BbOpDesc_t *pTgt, BbConflictAct_t conflictCback)
{
  bool_t result = FALSE;

  if (pItem->reschPolicy < pTgt->reschPolicy)
  {
    result = TRUE;
  }
  else if ((pItem->reschPolicy == pTgt->reschPolicy) &&
           conflictCback)
  {
    if (conflictCback(pItem, pTgt) == pItem)
    {
      result = TRUE;
    }
  }
  else
  {
    /* pItem is lower priority; no insertion. */
    LL_TRACE_WARN2("!!! Scheduling conflict: existing policy=%u prioritized over incoming policy=%u", pTgt->reschPolicy, pItem->reschPolicy);
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Resolve conflict between BODs.
 *
 *  \param      pItem   Item to insert.
 *  \param      pTgt    Target position.
 *
 *  \note       Atypical modification to the head element occurs for conflict only. This routine
 *              will attempt to remove and modify the head element in order for the a new BOD
 *              to be inserted correctly in time order.
 *
 *  \note       Resolve the conflict between the pItem and pTgt. It is possible that
 *              there is a conflict between the pItem and BODs after pTgt. But these conflict
 *              resolutions will be delayed until the BODs are executed.
 *
 *  \return     TRUE if \a pItem is inserted, FALSE for no change.
 */
/*************************************************************************************************/
static bool_t schResolveConflict(BbOpDesc_t *pItem, BbOpDesc_t *pTgt)
{
  WSF_ASSERT(pTgt);
  WSF_ASSERT(pItem);

  /* List of aborted BODs. */
  BbOpDesc_t *pRemHead = pTgt;
  BbOpDesc_t *pRemTail = pTgt;

  if (pTgt == schCb.pHead)
  {
    while (schCb.pHead)
    {
      if (SCH_IS_DONE_BEFORE(pItem, schCb.pHead))
      {
        /* No more conflicts. */
        break;
      }

      pRemTail = schCb.pHead;
      schRemoveHead();

      /* Return to IDLE since old BOD at head was removed. */
      schCb.state = SCH_STATE_IDLE;
    }

    /* Insert new BOD. */
    if (schCb.pHead)
    {
      schInsertBefore(pItem, schCb.pHead);
    }
    else
    {
      schInsertToEmptyList(pItem);
    }
  }
  else
  {
    /* Linkage may change with every loop; use BOD prior to target as start reference. */
    BbOpDesc_t *pStart = pTgt->pPrev;

    while (pStart->pNext)
    {
      if (SCH_IS_DONE_BEFORE(pItem, pStart->pNext))
      {
        /* No more conflicts. */
        break;
      }

      pRemTail = pStart->pNext;
      schRemoveMiddle(pStart->pNext);
    }

    /* Insert new BOD. */
    schInsertAfter(pItem, pStart);
  }

  /* Now that the new BOD is inserted, the removed BODs may be aborted. Insertion of the
   * higher priority BOD must occur first such that the abort() callbacks can manipulate
   * the BOD list appropriately. */
  while (TRUE)
  {
    /* Store next BOD since aborting may adjust linkage. */
    BbOpDesc_t *pCur = pRemHead;
    pRemHead = pRemHead->pNext;

    if (pCur->abortCback)
    {
      pCur->abortCback(pCur);
    }

    if (pCur == pRemTail)
    {
      /* End of removal list. */
      break;
    }
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Check whether the conflicts between pItem and BODs starting at pTgt are
 *              resolvable.
 *
 *  \param      pItem           Item to insert.
 *  \param      pTgt            Target position.
 *  \param      conflictCback   Conflict action callback
 *
 *  \return     TRUE if conflict is resolvable, FALSE otherwise.
 */
/*************************************************************************************************/
static bool_t SchIsConflictResolvable(BbOpDesc_t *pItem, BbOpDesc_t *pTgt, BbConflictAct_t conflictCback)
{
  bool_t result;

  BbOpDesc_t *pCur = pTgt;

  WSF_ASSERT(pTgt)
  WSF_ASSERT(pItem);

  while (TRUE)
  {
    if ((pCur->pNext == NULL) ||                                /* pCur is the tail. */
        (SCH_IS_DONE_BEFORE(pItem, pCur->pNext)))     /* Only conflict with pCur. */
    {
      /* Only 1 conflicting BOD. */
      result = SchIsBodConflictResolvable(pItem, pCur, conflictCback);
      break;
    }
    else
    {
      /* Check all conflicting BODs. */
      if ((result = SchIsBodConflictResolvable(pItem, pCur, conflictCback)) == FALSE)
      {
        break;
      }
    }
    /* Traverse to the next BOD. */
    pCur = pCur->pNext;
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Insert item into BOD list at the next available opportunity.
 *
 *  \param      pBod    Element to insert.
 *
 *  Insert this BOD in the active BOD list where its duration can be accommodated.
 */
/*************************************************************************************************/
void SchInsertNextAvailable(BbOpDesc_t *pBod)
{
#if (SCH_CHECK_LIST_INTEGRITY)
  SchCheckIsNotInserted(pBod);
#endif

  pBod->dueUsec = PalBbGetCurrentTime() + BbGetSchSetupDelayUs();

  if (schCb.pHead == NULL)
  {
    schInsertToEmptyList(pBod);
  }
  else if (SCH_IS_DONE_BEFORE(pBod, schCb.pHead) &&
           schCheckCancelHead())
  {
    /* Insert at head */
    WSF_ASSERT(pBod != schCb.pHead);
    schInsertBefore(pBod, schCb.pHead);
  }
  else
  {
    BbOpDesc_t *pCur = schCb.pHead;

    while (TRUE)
    {
      WSF_ASSERT(pBod != pCur);

      if ((pCur == schCb.pHead) &&
          (pCur->maxDurUsec > 0))
      {
        /* Allow current head BOD to complete naturally. */
        pBod->dueUsec = pCur->dueUsec + pCur->maxDurUsec + BbGetSchSetupDelayUs();
      }
      else
      {
        pBod->dueUsec = SCH_END_TIME(pCur);
      }

      if ((pCur->pNext == NULL) ||                          /* insert at tail */
          SCH_IS_DONE_BEFORE(pBod, pCur->pNext))
      {
        schInsertAfter(pBod, pCur);
        break;
      }

      pCur = pCur->pNext;
    }
  }

  if (pBod == schCb.pHead)
  {
    /* Restart the scheduler since the head element is modified. */
    schSetBodStartEvent();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Insert item into BOD list sorted by due time.
 *
 *  \param      pBod            Element to insert.
 *  \param      conflictCback   Conflict callback.
 *
 *  \return     Return TRUE if inserted successfully, FALSE otherwise.
 *
 *  Insert BOD in the active list at the specified due time.
 */
/*************************************************************************************************/
bool_t SchInsertAtDueTime(BbOpDesc_t *pBod, BbConflictAct_t conflictCback)
{
  bool_t result = FALSE;

#if (SCH_CHECK_LIST_INTEGRITY)
  SchCheckIsNotInserted(pBod);
#endif

  if (!schDueTimeInFuture(pBod))
  {
    return FALSE;
  }

  if (schCb.pHead == NULL)
  {
    /* No conflict when list is empty. */
    WSF_ASSERT(pBod != schCb.pHead);
    schInsertToEmptyList(pBod);
    result = TRUE;
  }
  else
  {
    /* List is not empty. */
    BbOpDesc_t *pCur = schCb.pHead;

    while (TRUE)
    {
      if (SCH_IS_DONE_BEFORE(pBod, pCur))          /* pBod is due and done before pCur with no overlap. */
      {
        if ((pCur == schCb.pHead) &&
            (!schCheckCancelHead()))
        {
          /* Cannot modify head. */
          break;
        }

        schInsertBefore(pBod, pCur);
        result = TRUE;
        break;
      }
      else if (!SCH_IS_DONE_BEFORE(pCur, pBod))    /* pCur has overlap with pBod, check priority and resolve conflict. */
      {
        if ((pCur == schCb.pHead) &&
            (!schCheckCancelHead()))
        {
          /* Cannot modify head. */
          break;
        }

        if ((result = SchIsConflictResolvable(pBod, pCur, conflictCback)) == TRUE)
        {
          result = schResolveConflict(pBod, pCur);
        }
        break;
      }
      else if (pCur->pNext == NULL)                /* BOD is due after pCur and pCur is tail, insert after. */
      {
        schInsertAfter(pBod, pCur);
        result = TRUE;
        break;
      }

      /* Traverse to the next BOD. */
      pCur = pCur->pNext;
    }
  }

  if (result &&
      (pBod == schCb.pHead))
  {
    /* Halt execution of previous head BOD. */
    PalTimerStop();

    /* Restart the scheduler since the head element is modified. */
    schSetBodStartEvent();
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Insert item into BOD list at the earliest possible opportunity.
 *
 *  \param      pBod    Element to insert.
 *  \param      min     Minimum operation interval from BOD due time.
 *  \param      max     Maximum operation interval from BOD due time.
 *
 *  \return     Return TRUE if inserted successfully, FALSE otherwise.
 *
 *  Insert this BOD in the active BOD list where its duration can be accommodated at the earliest
 *  possible due time.
 */
/*************************************************************************************************/
bool_t SchInsertEarlyAsPossible(BbOpDesc_t *pBod, uint32_t min, uint32_t max)
{
  WSF_ASSERT(min <= max);

#if (SCH_CHECK_LIST_INTEGRITY)
  SchCheckIsNotInserted(pBod);
#endif

  bool_t result = FALSE;

  uint32_t dueOrigin = pBod->dueUsec;

  /* Try inserting at minimum interval. */
  pBod->dueUsec += min;

  if (!schDueTimeInFuture(pBod))
  {
    if (max < SCH_MAX_SPAN)
    {
      /* Reset due time origin. */
      pBod->dueUsec = dueOrigin;
      return FALSE;
    }
    else
    {
      /* When max=SCH_MAX_SPAN, this function will insert the BOD regardless of the current due. */
      pBod->dueUsec = PalBbGetCurrentTime() + BbGetSchSetupDelayUs();
      dueOrigin = pBod->dueUsec;
    }
  }

  if (schCb.pHead == NULL)
  {
    schInsertToEmptyList(pBod);
    result = TRUE;
  }
  else if (SCH_IS_DUE_BEFORE (pBod, schCb.pHead) &&
           SCH_IS_DONE_BEFORE(pBod, schCb.pHead) &&
           schCheckCancelHead())
  {
    /* Insert at head */
    WSF_ASSERT(pBod != schCb.pHead);
    schInsertBefore(pBod, schCb.pHead);
    result = TRUE;
  }
  else if (SCH_IS_DONE_BEFORE(schCb.pTail, pBod))
  {
    /* Insert at tail */
    WSF_ASSERT(pBod != schCb.pTail);
    schInsertAfter(pBod, schCb.pTail);
    result = TRUE;
  }
  else
  {
    BbOpDesc_t *pCur = schCb.pHead;

    while (pCur)
    {
      WSF_ASSERT(pBod != pCur);

      /* Only update due time when pCur ends in the future. */
      if (!SCH_IS_DONE_BEFORE(pCur, pBod))
      {
        pBod->dueUsec = SCH_END_TIME(pCur);
      }
      uint32_t nextAvailInter = pBod->dueUsec - dueOrigin;

      if ((nextAvailInter >= min) &&
          (nextAvailInter <= max))
      {
        if (pCur->pNext == NULL)
        {
          /* Insert at tail */
          schInsertAfter(pBod, pCur);
          result = TRUE;
          break;
        }
        else if (SCH_IS_DONE_BEFORE(pBod, pCur->pNext))
        {
          /* Insert middle. */
          schInsertBefore(pBod, pCur->pNext);
          result = TRUE;
          break;
        }
      }

      pCur = pCur->pNext;
    }
  }

  if (result &&
      (pBod == schCb.pHead))
  {
    /* Restart the scheduler since the head element is modified. */
    schSetBodStartEvent();
  }

  if (!result)
  {
    /* Reset due time origin. */
    pBod->dueUsec = dueOrigin;
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Insert item into BOD list at the latest possible opportunity.
 *
 *  \param      pBod    Element to insert.
 *  \param      min     Minimum operation interval from BOD due time.
 *  \param      max     Maximum operation interval from BOD due time.
 *
 *  \return     Return TRUE if inserted successfully, FALSE otherwise.
 *
 *  Insert this BOD in the active BOD list where its duration can be accommodated at the latest
 *  possible due time.
 */
/*************************************************************************************************/
bool_t SchInsertLateAsPossible(BbOpDesc_t *pBod, uint32_t min, uint32_t max)
{
  WSF_ASSERT(min <= max);

  bool_t result = FALSE;

#if (SCH_CHECK_LIST_INTEGRITY)
  SchCheckIsNotInserted(pBod);
#endif

  const uint32_t dueOrigin = pBod->dueUsec;

  /* Try inserting at maximum interval. */
  pBod->dueUsec = dueOrigin + max;

  if (schCb.pTail == NULL)
  {
    if (schDueTimeInFuture(pBod))
    {
      schInsertToEmptyList(pBod);
      result = TRUE;
    }
  }
  else if (SCH_IS_DUE_AFTER(pBod, schCb.pTail))
  {
    if (schDueTimeInFuture(pBod))
    {
      /* Insert at tail. */
      WSF_ASSERT(pBod != schCb.pTail);
      schInsertAfter(pBod, schCb.pTail);
      result = TRUE;
    }
  }
  else
  {
    BbOpDesc_t *pCur = schCb.pTail;

    while (pCur)
    {
      WSF_ASSERT(pBod != pCur);

      /* The current BOD is the head BOD, check to see if there is time to schedule it before. */
      if (pCur->pPrev == NULL)
      {
        uint32_t nextAvailInter = BbGetTargetTimeDelta((pCur->dueUsec - SCH_TOTAL_DUR(pBod)), dueOrigin);
        uint32_t targetOffset = WSF_MIN(max, nextAvailInter);
        pBod->dueUsec = dueOrigin + targetOffset;

        if ((targetOffset >= min) &&
            SCH_IS_DUE_BEFORE(pBod, pCur) &&
            schDueTimeInFuture(pBod) &&
            schCheckCancelHead())
        {
          /* Insert at head. */
          schInsertBefore(pBod, pCur);
          result = TRUE;
          break;
        }
      }
      /* The current BOD is a intermediate BOD, check to see if we can insert in-between BODs. */
      else
      {
        pBod->dueUsec = SCH_END_TIME(pCur->pPrev);

        if (!schDueTimeInFuture(pBod))
        {
          break;
        }

        uint32_t nextAvailInter = BbGetTargetTimeDelta(pBod->dueUsec, dueOrigin);

        if ((nextAvailInter >= min) &&
            (nextAvailInter <= max) &&
            SCH_IS_DONE_BEFORE(pBod, pCur))
        {
          /* Insert middle. */
          schInsertBefore(pBod, pCur);
          result = TRUE;
          break;
        }
      }

      pCur = pCur->pPrev;
    }
  }

  if (result &&
      (pBod == schCb.pHead))
  {
    /* Restart the scheduler since the head element is modified. */
    schSetBodStartEvent();
  }

  if (!result)
  {
    /* Reset due time origin. */
    pBod->dueUsec = dueOrigin;
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Request BOD removal from list.
 *
 *  \param      pBod    Element to remove.
 *
 *  Request BOD removal from list. Request will require BOD list manipulation in the SchHandler() .
 */
/*************************************************************************************************/
void SchRemove(BbOpDesc_t *pBod)
{
  WSF_ASSERT(pBod);

  if (!SchCheckIsInserted(pBod))
  {
    LL_TRACE_WARN1("Remove failed, BOD not found, pBod=0x%08x", pBod);
    return;
  }

  if (schCb.pHead == pBod)
  {
    switch (schCb.state)
    {
    case SCH_STATE_EXECUTE:
      /* Gracefully complete BOD before removal. */
      BbSetBodTerminateFlag();
      break;

    case SCH_STATE_LOAD:
      schSetBodAbortEvent();
      break;

    default:
      schRemoveHead();
      if (pBod->abortCback)
      {
        pBod->abortCback(pBod);
      }
      break;
    }
  }
  /* TODO Consider resetting the curtail timer. */
  else
  {
    /* Call callback after removing from list. */
    schRemoveMiddle(pBod);
    if (pBod->abortCback)
    {
      pBod->abortCback(pBod);
    }
  }

  SCH_TRACE_INFO1("--| SchRemove            |-- pBod=0x%08x", (uint32_t)pBod);
  SCH_TRACE_INFO1("--|                      |--     .dueUsec=%u", pBod->dueUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Reload BOD.
 *
 *  \param      pBod    Element to reload.
 *
 *  Only if the head operation, cancel the operation and re-start it.
 */
/*************************************************************************************************/
void SchReload(BbOpDesc_t *pBod)
{
  if ((schCb.pHead == pBod) &&
      schCheckCancelHead())
  {
    /* Restart the scheduler since the head element is modified. */
    schSetBodStartEvent();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check if BOD can be cancelled.
 *
 *  \param      pBod    Element to be cancelled
 *
 *  \return     TRUE if BOD can be cancelled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t SchIsBodCancellable(BbOpDesc_t *pBod)
{
  if ((schCb.pHead != pBod) ||
      schCheckCancelHead())
  {
    return TRUE;
  }

  return FALSE;
}
