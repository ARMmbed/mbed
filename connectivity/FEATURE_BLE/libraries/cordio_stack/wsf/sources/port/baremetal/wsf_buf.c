/*************************************************************************************************/
/*!
 *  \file   wsf_buf.c
 *
 *  \brief  Buffer pool service.
 *
 *  Copyright (c) 2009-2018 Arm Ltd. All Rights Reserved.
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
#include "wsf_buf.h"

#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_trace.h"
#include "wsf_heap.h"
#include "wsf_math.h"
#include "wsf_os.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Magic number used to check for free buffer. */
#define WSF_BUF_FREE_NUM            0xFAABD00D

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Unit of memory storage-- a structure containing a pointer. */
typedef struct wsfBufMem_tag
{
  struct wsfBufMem_tag  *pNext;
#if WSF_BUF_FREE_CHECK_ASSERT == TRUE
  uint32_t              free;
#endif
} wsfBufMem_t;

/* Internal buffer pool. */
typedef struct
{
  wsfBufPoolDesc_t  desc;           /* Number of buffers and length. */
  wsfBufMem_t       *pStart;        /* Start of pool. */
  wsfBufMem_t       *pFree;         /* First free buffer in pool. */
#if WSF_BUF_STATS == TRUE
  uint8_t           numAlloc;       /* Number of buffers currently allocated from pool. */
  uint8_t           maxAlloc;       /* Maximum buffers ever allocated from pool. */
  uint16_t          maxReqLen;      /* Maximum request length from pool. */
#endif
} wsfBufPool_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Number of pools. */
uint8_t wsfBufNumPools;

/* Memory used for pools. */
wsfBufMem_t *wsfBufMem = NULL;

/* Currently use for debugging only. */
uint32_t wsfBufMemLen;

#if WSF_BUF_STATS_HIST == TRUE
/* Buffer allocation counter. */
uint8_t wsfBufAllocCount[WSF_BUF_STATS_MAX_LEN];

/* Pool Overflow counter. */
uint8_t wsfPoolOverFlowCount[WSF_BUF_STATS_MAX_POOL];
#endif

#if WSF_OS_DIAG == TRUE
/* WSF buffer diagnostic callback function. */
static WsfBufDiagCback_t wsfBufDiagCback = NULL;
#endif

/* The number of outstanding memory pool buffers */
static uint32_t wsfOutstandingBufCount = 0;

/*************************************************************************************************/
/*!
 *  \brief  Calculate size required by the buffer pool.
 *
 *  \param  numPools  Number of buffer pools.
 *  \param  pDesc     Array of buffer pool descriptors, one for each pool.
 *
 *  \return Amount of pBufMem used.
 */
/*************************************************************************************************/
uint32_t WsfBufCalcSize(uint8_t numPools, wsfBufPoolDesc_t *pDesc)
{
  uint32_t      len;
  uint32_t      descLen;
  wsfBufPool_t  *pPool;
  wsfBufMem_t   *pStart;
  uint8_t       i;

  wsfBufMem = (wsfBufMem_t *)0;
  pPool = (wsfBufPool_t *)wsfBufMem;

  /* Buffer storage starts after the pool structs. */
  pStart = (wsfBufMem_t *) (pPool + numPools);

  /* Create each pool; see loop exit condition below. */
  while (TRUE)
  {
    /* Exit loop after verification check. */
    if (numPools-- == 0)
    {
      break;
    }

    /* Adjust pool lengths for minimum size and alignment. */
    if (pDesc->len < sizeof(wsfBufMem_t))
    {
      descLen = sizeof(wsfBufMem_t);
    }
    else if ((pDesc->len % sizeof(wsfBufMem_t)) != 0)
    {
      descLen = pDesc->len + sizeof(wsfBufMem_t) - (pDesc->len % sizeof(wsfBufMem_t));
    }
    else
    {
      descLen = pDesc->len;
    }

    len = descLen / sizeof(wsfBufMem_t);
    for (i = pDesc->num; i > 0; i--)
    {
      /* Pointer to the next free buffer is stored in the buffer itself. */
      pStart += len;
    }
    pDesc++;
  }

  return (uint8_t *)pStart - (uint8_t *)wsfBufMem;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the buffer pool service.  This function should only be called once
 *          upon system initialization.
 *
 *  \param  numPools  Number of buffer pools.
 *  \param  pDesc     Array of buffer pool descriptors, one for each pool.
 *
 *  \return Amount of pBufMem used or 0 for failures.
 */
/*************************************************************************************************/
uint32_t WsfBufInit(uint8_t numPools, wsfBufPoolDesc_t *pDesc)
{
  wsfBufPool_t  *pPool;
  wsfBufMem_t   *pStart;
  uint16_t      len;
  uint8_t       i;

  wsfBufMem = (wsfBufMem_t *) WsfHeapGetFreeStartAddress();
  pPool = (wsfBufPool_t *) wsfBufMem;

  /* Buffer storage starts after the pool structs. */
  pStart = (wsfBufMem_t *) (pPool + numPools);

  wsfBufNumPools = numPools;

  /* Create each pool; see loop exit condition below. */
  while (TRUE)
  {
    /* Verify we didn't overrun memory; if we did, abort. */
    if (pStart > &wsfBufMem[WsfHeapCountAvailable() / sizeof(wsfBufMem_t)])
    {
      WSF_ASSERT(FALSE);
      return 0;
    }

    /* Exit loop after verification check. */
    if (numPools-- == 0)
    {
      break;
    }

    /* Adjust pool lengths for minimum size and alignment. */
    if (pDesc->len < sizeof(wsfBufMem_t))
    {
      pPool->desc.len = sizeof(wsfBufMem_t);
    }
    else if ((pDesc->len % sizeof(wsfBufMem_t)) != 0)
    {
      pPool->desc.len = pDesc->len + sizeof(wsfBufMem_t) - (pDesc->len % sizeof(wsfBufMem_t));
    }
    else
    {
      pPool->desc.len = pDesc->len;
    }

    pPool->desc.num = pDesc->num;
    pDesc++;

    pPool->pStart = pStart;
    pPool->pFree = pStart;
#if WSF_BUF_STATS == TRUE
    pPool->numAlloc = 0;
    pPool->maxAlloc = 0;
    pPool->maxReqLen = 0;
#endif

    WSF_TRACE_INFO2("Creating pool len=%u num=%u", pPool->desc.len, pPool->desc.num);
    WSF_TRACE_INFO1("              pStart=0x%x", (uint32_t)pPool->pStart);

    /* Initialize free list. */
    len = pPool->desc.len / sizeof(wsfBufMem_t);
    for (i = pPool->desc.num; i > 1; i--)
    {
      /* Verify we didn't overrun memory; if we did, abort. */
      if (pStart > &wsfBufMem[WsfHeapCountAvailable() / sizeof(wsfBufMem_t)])
      {
        WSF_ASSERT(FALSE);
        return 0;
      }
      /* Pointer to the next free buffer is stored in the buffer itself. */
      pStart->pNext = pStart + len;
      pStart += len;
    }

    /* Verify we didn't overrun memory; if we did, abort. */
    if (pStart > &wsfBufMem[WsfHeapCountAvailable() / sizeof(wsfBufMem_t)])
    {
      WSF_ASSERT(FALSE);
      return 0;
    }
    /* Last one in list points to NULL. */
    pStart->pNext = NULL;
    pStart += len;

    /* Next pool. */
    pPool++;
  }

  wsfBufMemLen = (uint8_t *) pStart - (uint8_t *) wsfBufMem;
  WSF_TRACE_INFO1("Created buffer pools; using %u bytes", wsfBufMemLen);

  return wsfBufMemLen;
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a buffer.
 *
 *  \param  len     Length of buffer to allocate.
 *
 *  \return Pointer to allocated buffer or NULL if allocation fails.
 */
/*************************************************************************************************/
void *WsfBufAlloc(uint16_t len)
{
  wsfBufPool_t  *pPool;
  wsfBufMem_t   *pBuf;
  uint8_t       i;

  WSF_CS_INIT(cs);

  WSF_ASSERT(len > 0);

  pPool = (wsfBufPool_t *) wsfBufMem;

  for (i = wsfBufNumPools; i > 0; i--, pPool++)
  {
    /* Check if buffer is big enough. */
    if (len <= pPool->desc.len)
    {
      /* Enter critical section. */
      WSF_CS_ENTER(cs);

      /* Check if buffers are available. */
      if (pPool->pFree != NULL)
      {
        /* Allocation succeeded. */
        pBuf = pPool->pFree;

        /* Next free buffer is stored inside current free buffer. */
        pPool->pFree = pBuf->pNext;

        /* Increment the number of allocated buffers */
        wsfOutstandingBufCount++;

#if WSF_BUF_FREE_CHECK_ASSERT == TRUE
        pBuf->free = 0;
#endif
#if WSF_BUF_STATS_HIST == TRUE
        /* Increment count for buffers of this length. */
        if (len < WSF_BUF_STATS_MAX_LEN)
        {
          wsfBufAllocCount[len]++;
        }
          else
        {
          wsfBufAllocCount[0]++;
        }
#endif
#if WSF_BUF_STATS == TRUE
        if (++pPool->numAlloc > pPool->maxAlloc)
        {
          pPool->maxAlloc = pPool->numAlloc;
        }
        pPool->maxReqLen = WSF_MAX(pPool->maxReqLen, len);
#endif
        /* Exit critical section. */
        WSF_CS_EXIT(cs);

        WSF_TRACE_ALLOC2("WsfBufAlloc len:%u pBuf:%08x", pPool->desc.len, pBuf);

        return pBuf;
      }
#if WSF_BUF_STATS_HIST == TRUE
      else
      {
        /* Pool overflow: increment count of overflow for current pool. */
        wsfPoolOverFlowCount[wsfBufNumPools-i]++;
      }
#endif
      /* Exit critical section. */
      WSF_CS_EXIT(cs);

#if WSF_BUF_ALLOC_BEST_FIT_FAIL_ASSERT == TRUE
      WSF_ASSERT(FALSE);
#endif
    }
  }

  /* Allocation failed. */
#if WSF_OS_DIAG == TRUE
  if (wsfBufDiagCback != NULL)
  {
    WsfBufDiag_t info;

    info.type = WSF_BUF_ALLOC_FAILED;
    info.param.alloc.taskId = WSF_OS_GET_ACTIVE_HANDLER_ID();
    info.param.alloc.len = len;

    wsfBufDiagCback(&info);
  }
  else
  {
    WSF_TRACE_ALLOC2("WsfBufAlloc failed len:%u - task:%u", len, WSF_OS_GET_ACTIVE_HANDLER_ID());
  }
#else
  WSF_TRACE_ALLOC1("WsfBufAlloc failed len:%u", len);
#endif

#if WSF_BUF_ALLOC_FAIL_ASSERT == TRUE
  WSF_ASSERT(FALSE);
#endif

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Free a buffer.
 *
 *  \param  pBuf    Buffer to free.
 */
/*************************************************************************************************/
void WsfBufFree(void *pBuf)
{
  wsfBufPool_t  *pPool;
  wsfBufMem_t   *p = pBuf;

  WSF_CS_INIT(cs);

  /* Verify pointer is within range. */
#if WSF_BUF_FREE_CHECK_ASSERT == TRUE
  WSF_ASSERT(p >= ((wsfBufPool_t *) wsfBufMem)->pStart);
  WSF_ASSERT(p < (wsfBufMem_t *)(((uint8_t *) wsfBufMem) + wsfBufMemLen));
#endif

  /* Iterate over pools starting from last pool. */
  pPool = (wsfBufPool_t *) wsfBufMem + (wsfBufNumPools - 1);
  while (pPool >= (wsfBufPool_t *) wsfBufMem)
  {
    /* Check if the buffer memory is located inside this pool. */
    if (p >= pPool->pStart)
    {
      /* Enter critical section. */
      WSF_CS_ENTER(cs);

#if WSF_BUF_FREE_CHECK_ASSERT == TRUE
      WSF_ASSERT(p->free != WSF_BUF_FREE_NUM);
      p->free = WSF_BUF_FREE_NUM;
#endif
#if WSF_BUF_STATS == TRUE
      pPool->numAlloc--;
#endif

      /* Pool found; put buffer back in free list. */
      p->pNext = pPool->pFree;
      pPool->pFree = p;

      /* Decrement the number of allocated buffers */
      wsfOutstandingBufCount--;

      /* Exit critical section. */
      WSF_CS_EXIT(cs);

      WSF_TRACE_FREE2("WsfBufFree len:%u pBuf:%08x", pPool->desc.len, pBuf);

      return;
    }

    /* Next pool. */
    pPool--;
  }

  /* Should never get here. */
  WSF_ASSERT(FALSE);

  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Diagnostic function to get the buffer allocation statistics.
 *
 *  \return Buffer allocation statistics array.
 */
/*************************************************************************************************/
uint8_t *WsfBufGetAllocStats(void)
{
#if WSF_BUF_STATS_HIST == TRUE
  return wsfBufAllocCount;
#else
  return NULL;
#endif
}

/*************************************************************************************************/
/*!
 *  \brief  Diagnostic function to get the number of overflow times for each pool.
 *
 *  \return Overflow times statistics array
 */
/*************************************************************************************************/
uint8_t *WsfBufGetPoolOverFlowStats(void)
{
#if WSF_BUF_STATS_HIST == TRUE
  return wsfPoolOverFlowCount;
#else
  return NULL;
#endif
}

/*************************************************************************************************/
/*!
 *  \brief  Get number of pools.
 *
 *  \return Number of pools.
 */
/*************************************************************************************************/
uint8_t WsfBufGetNumPool(void)
{
  return wsfBufNumPools;
}

/*************************************************************************************************/
/*!
 *  \brief  Get statistics for each pool.
 *
 *  \param  pBuf    Buffer to store the statistics.
 *  \param  poolId  Pool ID.
 */
/*************************************************************************************************/
void WsfBufGetPoolStats(WsfBufPoolStat_t *pStat, uint8_t poolId)
{
  wsfBufPool_t  *pPool;

  if (poolId >= wsfBufNumPools)
  {
    pStat->bufSize = 0;
    return;
  }

  WSF_CS_INIT(cs);
  WSF_CS_ENTER(cs);

  pPool = (wsfBufPool_t *) wsfBufMem;

  pStat->bufSize  = pPool[poolId].desc.len;
  pStat->numBuf   = pPool[poolId].desc.num;
#if WSF_BUF_STATS == TRUE
  pStat->numAlloc = pPool[poolId].numAlloc;
  pStat->maxAlloc = pPool[poolId].maxAlloc;
  pStat->maxReqLen = pPool[poolId].maxReqLen;
#else
  pStat->numAlloc = 0;
  pStat->maxAlloc = 0;
  pStat->maxReqLen = 0;
#endif

  /* Exit critical section. */
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief  Called to register the buffer diagnostics callback function.
 *
 *  \param  pCallback   Pointer to the callback function.
 */
/*************************************************************************************************/
void WsfBufDiagRegister(WsfBufDiagCback_t callback)
{
#if WSF_OS_DIAG == TRUE
  wsfBufDiagCback = callback;
#else
  /* Unused parameter */
  (void)callback;
#endif
}

/*************************************************************************************************/
/*!
 *  \brief  Get the number of outstanding memory pool buffers
 *
 *  \return The number of outstanding buffers
 */
/*************************************************************************************************/
uint32_t WsfBufNumOutstanding(void)
{
  return wsfOutstandingBufCount;
}
