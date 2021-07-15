/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  General purpose queue service.
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

#include "wsf_types.h"
#include "wsf_queue.h"

#include "wsf_assert.h"
#include "wsf_cs.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Get next queue element */
#define WSF_QUEUE_NEXT(p)               (((wsfQueueElem_t *)(p))->pNext)

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Queue element */
typedef struct wsfQueueElem_tag
{
  struct wsfQueueElem_tag *pNext;      /* pointer to next element */
} wsfQueueElem_t;

/*************************************************************************************************/
/*!
 *  \brief  Enqueue and element to the tail of a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  pElem     Pointer to element.
 */
/*************************************************************************************************/
void WsfQueueEnq(wsfQueue_t *pQueue, void *pElem)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);
  WSF_ASSERT(pElem != NULL);

  /* initialize next pointer */
  WSF_QUEUE_NEXT(pElem) = NULL;

  /* enter critical section */
  WSF_CS_ENTER(cs);

  /* if queue empty */
  if (pQueue->pHead == NULL)
  {
    pQueue->pHead = pElem;
    pQueue->pTail = pElem;
  }
  /* else enqueue element to the tail of queue */
  else
  {
    WSF_QUEUE_NEXT(pQueue->pTail) = pElem;
    pQueue->pTail = pElem;
  }

  /* exit critical section */
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief  Dequeue and element from the head of a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *
 *  \return Pointer to element that has been dequeued or NULL if queue is empty.
 */
/*************************************************************************************************/
void *WsfQueueDeq(wsfQueue_t *pQueue)
{
  wsfQueueElem_t  *pElem;

  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);

  /* enter critical section */
  WSF_CS_ENTER(cs);

  pElem = pQueue->pHead;

  /* if queue is not empty */
  if (pElem != NULL)
  {
    /* set head to next element in queue */
    pQueue->pHead = WSF_QUEUE_NEXT(pElem);

    /* check for empty queue */
    if (pQueue->pHead == NULL)
    {
      pQueue->pTail = NULL;
    }
  }

  /* exit critical section */
  WSF_CS_EXIT(cs);

  return pElem;
}

/*************************************************************************************************/
/*!
 *  \brief  Push an element to the head of a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  pElem     Pointer to element.
 */
/*************************************************************************************************/
void WsfQueuePush(wsfQueue_t *pQueue, void *pElem)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);
  WSF_ASSERT(pElem != NULL);

  /* enter critical section */
  WSF_CS_ENTER(cs);

  /* else push element to head of queue */
  WSF_QUEUE_NEXT(pElem) = pQueue->pHead;

  /* if queue was empty set tail */
  if (pQueue->pHead == NULL)
  {
    pQueue->pTail = pElem;
  }

  /* set head */
  pQueue->pHead = pElem;

  /* exit critical section */
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief  Insert an element into a queue.  This function is typically used when iterating
 *          over a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  pElem     Pointer to element to be inserted.
 *  \param  pPrev     Pointer to previous element in the queue before element to be inserted.
 *                    Note:  set pPrev to NULL if pElem is first element in queue.
 */
/*************************************************************************************************/
void WsfQueueInsert(wsfQueue_t *pQueue, void *pElem, void *pPrev)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);
  WSF_ASSERT(pElem != NULL);

  /* enter critical section */
  WSF_CS_ENTER(cs);

  /* if queue empty or inserting at tail */
  if (pQueue->pHead == NULL || pPrev == pQueue->pTail)
  {
    /* queue as normal */
    WsfQueueEnq(pQueue, pElem);
  }
  /* else if inserting at head */
  else if (pPrev == NULL)
  {
    /* push to head */
    WsfQueuePush(pQueue, pElem);
  }
  else
  {
    /* insert in middle of queue */
    WSF_QUEUE_NEXT(pElem) = WSF_QUEUE_NEXT(pPrev);
    WSF_QUEUE_NEXT(pPrev) = pElem;
  }

  /* exit critical section */
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief  Remove an element from a queue.  This function is typically used when iterating
 *          over a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  pElem     Pointer to element to be removed.
 *  \param  pPrev     Pointer to previous element in the queue before element to be removed.
 *                    Note:  set pPrev to NULL if pElem is first element in queue.
 */
/*************************************************************************************************/
void WsfQueueRemove(wsfQueue_t *pQueue, void *pElem, void *pPrev)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);
  WSF_ASSERT(pQueue->pHead != NULL);
  WSF_ASSERT(pElem != NULL);

  /* enter critical section */
  WSF_CS_ENTER(cs);

  /* if first element */
  if (pElem == pQueue->pHead)
  {
    /* remove from head of queue */
    pQueue->pHead = WSF_QUEUE_NEXT(pElem);
  }
  else if (pPrev)
  {
    /* remove from middle of queue, pPrev will never be null */
    WSF_QUEUE_NEXT(pPrev) = WSF_QUEUE_NEXT(pElem);
  }

  /* if last element */
  if (pElem == pQueue->pTail)
  {
    /* update tail */
    pQueue->pTail = pPrev;
  }

  /* exit critical section */
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief  Count the number of elements in a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *
 *  \return Number of elements in queue.
 */
/*************************************************************************************************/
uint16_t WsfQueueCount(wsfQueue_t *pQueue)
{
  wsfQueueElem_t  *pElem;
  uint16_t        count = 0;

  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);

  /* enter critical section */
  WSF_CS_ENTER(cs);

  pElem = pQueue->pHead;

  /* iterate over queue */
  while (pElem != NULL)
  {
    count++;
    pElem = pElem->pNext;
  }

  /* exit critical section */
  WSF_CS_EXIT(cs);

  return count;
}

/*************************************************************************************************/
/*!
 *  \brief  Return TRUE if queue is empty.
 *
 *  \param  pQueue    Pointer to queue.
 *
 *  \return TRUE if queue is empty, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t WsfQueueEmpty(wsfQueue_t *pQueue)
{
  bool_t      empty;

  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);

  /* enter critical section */
  WSF_CS_ENTER(cs);

  empty = (pQueue->pHead == NULL);

  /* exit critical section */
  WSF_CS_EXIT(cs);

  return empty;
}

/*************************************************************************************************/
/*!
 *  \brief  Check for a queue depth of 1 element.
 *
 *  \param  pQueue      Queue.
 *
 *  \return TRUE if Queue only has 1 element, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t WsfIsQueueDepthOne(wsfQueue_t *pQueue)
{
  return pQueue->pHead == pQueue->pTail;
}
