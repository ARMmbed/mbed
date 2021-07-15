/*************************************************************************************************/
/*!
 *  \file   wsf_buf.h
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
#ifndef WSF_BUF_H
#define WSF_BUF_H

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup WSF_BUF_API
 *  \{ */

/**************************************************************************************************
  Configuration
**************************************************************************************************/

/*! \brief Check if trying to free a buffer that is already free */
#ifndef WSF_BUF_FREE_CHECK_ASSERT
#define WSF_BUF_FREE_CHECK_ASSERT TRUE
#endif

/*! \brief Assert on best-fit buffer allocation failure */
#ifndef WSF_BUF_ALLOC_BEST_FIT_FAIL_ASSERT
#define WSF_BUF_ALLOC_BEST_FIT_FAIL_ASSERT FALSE
#endif

/*! \brief Assert on buffer allocation failure */
#ifndef WSF_BUF_ALLOC_FAIL_ASSERT
#define WSF_BUF_ALLOC_FAIL_ASSERT TRUE
#endif

/*! \brief Buffer histogram stats */
#ifndef WSF_BUF_STATS_HIST
#define WSF_BUF_STATS_HIST FALSE
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief Length of the buffer statistics array */
#define WSF_BUF_STATS_MAX_LEN       128

/*! \brief Max number of pools can allocate */
#define WSF_BUF_STATS_MAX_POOL      32

/*! \brief Failure Codes */
#define WSF_BUF_ALLOC_FAILED        1

#ifndef WSF_BUF_STATS
/*! \brief Enable buffer allocation statistics. */
#define WSF_BUF_STATS FALSE
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief Buffer pool descriptor structure */
typedef struct
{
  uint16_t   len;                  /*!< \brief Length of buffers in pool */
  uint8_t    num;                  /*!< \brief Number of buffers in pool */
} wsfBufPoolDesc_t;

/*! \brief Pool statistics */
typedef struct
{
  uint16_t   bufSize;              /*!< \brief Pool buffer size. */
  uint8_t    numBuf;               /*!< \brief Total number of buffers. */
  uint8_t    numAlloc;             /*!< \brief Number of outstanding allocations. */
  uint8_t    maxAlloc;             /*!< \brief High allocation watermark. */
  uint16_t   maxReqLen;            /*!< \brief Maximum requested buffer length. */
} WsfBufPoolStat_t;

/*! \brief WSF buffer diagnostics - buffer allocation failure */
typedef struct
{
  uint8_t    taskId;               /*!< \brief Task handler ID where failure occured */
  uint16_t   len;                  /*!< \brief Length of buffer being allocated */
} wsfBufDiagAllocFail_t;

/*! \brief WSF buffer diagnostics message */
typedef struct
{
  union
  {
    wsfBufDiagAllocFail_t alloc;   /*!< \brief Buffer allocation failure */
  } param;                         /*!< \brief Union of diagnostic data types. */

  uint8_t type;                    /*!< \brief Type of error */
} WsfBufDiag_t;

/**************************************************************************************************
  Callback Function Datatypes
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Callback providing WSF buffer diagnostic messages.
 *
 *  \param  pInfo     Diagnostics message.
 */
/*************************************************************************************************/
typedef void (*WsfBufDiagCback_t)(WsfBufDiag_t *pInfo);

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

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
uint32_t WsfBufCalcSize(uint8_t numPools, wsfBufPoolDesc_t *pDesc);

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
uint32_t WsfBufInit(uint8_t numPools, wsfBufPoolDesc_t *pDesc);

/*************************************************************************************************/
/*!
 *  \brief  Allocate a buffer.
 *
 *  \param  len     Length of buffer to allocate.
 *
 *  \return Pointer to allocated buffer or NULL if allocation fails.
 */
/*************************************************************************************************/
void *WsfBufAlloc(uint16_t len);

/*************************************************************************************************/
/*!
 *  \brief  Free a buffer.
 *
 *  \param  pBuf    Buffer to free.
 */
/*************************************************************************************************/
void WsfBufFree(void *pBuf);

/*************************************************************************************************/
/*!
 *  \brief  Diagnostic function to get the buffer allocation statistics.
 *
 *  \return Buffer allocation statistics array.
 */
/*************************************************************************************************/
uint8_t *WsfBufGetAllocStats(void);

/*************************************************************************************************/
/*!
 *  \brief  Diagnostic function to get the number of overflow times for each pool.
 *
 *  \return Overflow times statistics array
 */
/*************************************************************************************************/
uint8_t *WsfBufGetPoolOverFlowStats(void);

/*************************************************************************************************/
/*!
 *  \brief  Get number of pools.
 *
 *  \return Number of pools.
 */
/*************************************************************************************************/
uint8_t WsfBufGetNumPool(void);

/*************************************************************************************************/
/*!
 *  \brief  Get statistics for each pool.
 *
 *  \param  pStat   Buffer to store the statistics.
 *  \param  numPool Number of pool elements.
 *
 *  \return Pool statistics.
 */
/*************************************************************************************************/
void WsfBufGetPoolStats(WsfBufPoolStat_t *pStat, uint8_t numPool);

/*************************************************************************************************/
/*!
 *  \brief  Called to register the buffer diagnostics callback function.
 *
 *  \param  callback   Pointer to the callback function.
 */
/*************************************************************************************************/
void WsfBufDiagRegister(WsfBufDiagCback_t callback);

/*************************************************************************************************/
/*!
 *  \brief  Get the number of outstanding memory pool buffers
 *
 *  \return The number of outstanding buffers
 */
/*************************************************************************************************/
uint32_t WsfBufNumOutstanding(void);

/*! \} */    /* WSF_BUF_API */

#ifdef __cplusplus
};
#endif

#endif /* WSF_BUF_H */
