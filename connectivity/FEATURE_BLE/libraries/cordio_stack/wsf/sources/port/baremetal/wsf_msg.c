/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Message passing service.
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

#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "wsf_buf.h"
#include "wsf_queue.h"
#include "wsf_os.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief Internal message buf structure */
typedef struct wsfMsg_tag
{
  struct wsfMsg_tag   *pNext;
  wsfHandlerId_t      handlerId;
} wsfMsg_t;

/*! \brief Size of wsfMsg header. */
const uint8_t WSF_MSG_HDR_SIZE = sizeof(wsfMsg_t);

/*************************************************************************************************/
/*!
 *  \brief  Allocate a data message buffer to be sent with WsfMsgSend().
 *
 *  \param  len       Message length in bytes.
 *  \param  tailroom  Tailroom length in bytes.
 *
 *  \return Pointer to data message buffer or NULL if allocation failed.
 */
/*************************************************************************************************/
void *WsfMsgDataAlloc(uint16_t len, uint8_t tailroom)
{
  return WsfMsgAlloc(len + tailroom);
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a message buffer to be sent with WsfMsgSend().
 *
 *  \param  len   Message length in bytes.
 *
 *  \return Pointer to message buffer or NULL if allocation failed.
 */
/*************************************************************************************************/
void *WsfMsgAlloc(uint16_t len)
{
  wsfMsg_t  *pMsg;

  pMsg = WsfBufAlloc(len + sizeof(wsfMsg_t));

  /* hide header */
  if (pMsg != NULL)
  {
    pMsg++;
  }

  return pMsg;
}

/*************************************************************************************************/
/*!
 *  \brief  Free a message buffer allocated with WsfMsgAlloc().
 *
 *  \param  pMsg  Pointer to message buffer.
 */
/*************************************************************************************************/
void WsfMsgFree(void *pMsg)
{
  WsfBufFree(((wsfMsg_t *) pMsg) - 1);
}

/*************************************************************************************************/
/*!
 *  \brief  Send a message to an event handler.
 *
 *  \param  handlerId   Event handler ID.
 *  \param  pMsg        Pointer to message buffer.
 */
/*************************************************************************************************/
void WsfMsgSend(wsfHandlerId_t handlerId, void *pMsg)
{
  WSF_TRACE_MSG1("WsfMsgSend handlerId:%u", handlerId);

  /* get queue for this handler and enqueue message */
  WsfMsgEnq(WsfTaskMsgQueue(handlerId), handlerId, pMsg);

  /* set task for this handler as ready to run */
  WsfTaskSetReady(handlerId, WSF_MSG_QUEUE_EVENT);
}

/*************************************************************************************************/
/*!
 *  \brief  Enqueue a message.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  handerId  Set message handler ID to this value.
 *  \param  pElem     Pointer to message buffer.
 */
/*************************************************************************************************/
void WsfMsgEnq(wsfQueue_t *pQueue, wsfHandlerId_t handlerId, void *pMsg)
{
  wsfMsg_t    *p;

  WSF_ASSERT(pMsg != NULL);

  /* get message header */
  p = ((wsfMsg_t *) pMsg) - 1;

  /* set handler ID */
  p->handlerId = handlerId;

  WsfQueueEnq(pQueue, p);
}

/*************************************************************************************************/
/*!
 *  \brief  Dequeue a message.
 *
 *  \param  pQueue      Pointer to queue.
 *  \param  pHandlerId  Handler ID of returned message; this is a return parameter.
 *
 *  \return Pointer to message that has been dequeued or NULL if queue is empty.
 */
/*************************************************************************************************/
void *WsfMsgDeq(wsfQueue_t *pQueue, wsfHandlerId_t *pHandlerId)
{
  wsfMsg_t *pMsg;

  if ((pMsg = WsfQueueDeq(pQueue)) != NULL)
  {
    *pHandlerId = pMsg->handlerId;

    /* hide header */
    pMsg++;
  }

  return pMsg;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the next message without removing it from the queue.
 *
 *  \param  pQueue      Pointer to queue.
 *  \param  pHandlerId  Handler ID of returned message; this is a return parameter.
 *
 *  \return Pointer to the next message on the queue or NULL if queue is empty.
 */
/*************************************************************************************************/
void *WsfMsgPeek(wsfQueue_t *pQueue, wsfHandlerId_t *pHandlerId)
{
  wsfMsg_t *pMsg = pQueue->pHead;

  if (pMsg != NULL)
  {
    *pHandlerId = pMsg->handlerId;

    /* hide header */
    pMsg++;
  }

  return pMsg;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the Nth message without removing it from the queue.
 *
 *  \param  pQueue      Pointer to queue.
 *  \param  n           Nth item from the top (0 = top element).
 *  \param  pHandlerId  Handler ID of returned message; this is a return parameter.
 *
 *  \return Pointer to the next message on the queue or NULL if queue is empty.
 */
/*************************************************************************************************/
void *WsfMsgNPeek(wsfQueue_t *pQueue, uint8_t n, wsfHandlerId_t *pHandlerId)
{
  wsfMsg_t *pMsg = pQueue->pHead;

  while (pMsg && n--)
  {
    pMsg = pMsg->pNext;
  }

  if (pMsg != NULL)
  {
    *pHandlerId = pMsg->handlerId;

    /* hide header */
    pMsg++;
  }

  return pMsg;
}
