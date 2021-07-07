/*
 * Copyright (c) , Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __CMSIS_OS2_H__
#define __CMSIS_OS2_H__

#ifdef  __cplusplus
extern "C"
{
#endif

#include <inttypes.h>

//If conflicts, then remove these, copied from cmsis_os.h
typedef int32_t                  osStatus;


//These are from cmsis_os2.h
typedef void *osSemaphoreId_t;

typedef struct {
    const char                   *name;   ///< name of the semaphore
    uint32_t                 attr_bits;   ///< attribute bits
    void                      *cb_mem;    ///< memory for control block
    uint32_t                   cb_size;   ///< size of provided memory for control block
} osSemaphoreAttr_t;

/// Status code values returned by CMSIS-RTOS functions.
typedef enum {
    osOK                      =  0,         ///< Operation completed successfully.
    osError                   = -1,         ///< Unspecified RTOS error: run-time error but no other error message fits.
    osErrorTimeout            = -2,         ///< Operation not completed within the timeout period.
    osErrorResource           = -3,         ///< Resource not available.
    osErrorParameter          = -4,         ///< Parameter error.
    osErrorNoMemory           = -5,         ///< System is out of memory: it was impossible to allocate or reserve memory for the operation.
    osErrorISR                = -6,         ///< Not allowed in ISR context: the function cannot be called from interrupt service routines.
    osStatusReserved          = 0x7FFFFFFF  ///< Prevents enum down-size compiler optimization.
} osStatus_t;

//Thread
typedef enum {
    osPriorityNormal        = 24       ///< Priority: normal
} osPriority_t;

typedef void *osThreadId_t;

typedef void *osMutexId_t;

typedef void *osEventFlagsId_t;

/// Attributes structure for thread.
typedef struct {
    int unused;
} osThreadAttr_t;

#define osWaitForever         0xFFFFFFFFU ///< Wait forever timeout value.

// Flags options (\ref osThreadFlagsWait and \ref osEventFlagsWait).
#define osFlagsWaitAny        0x00000000U ///< Wait for any flag (default).
#define osFlagsWaitAll        0x00000001U ///< Wait for all flags.
#define osFlagsNoClear        0x00000002U ///< Do not clear flags which have been specified to wait for.

// Flags errors (returned by osThreadFlagsXxxx and osEventFlagsXxxx).
#define osFlagsError          0x80000000U ///< Error indicator.
#define osFlagsErrorUnknown   0xFFFFFFFFU ///< osError (-1).
#define osFlagsErrorTimeout   0xFFFFFFFEU ///< osErrorTimeout (-2).
#define osFlagsErrorResource  0xFFFFFFFDU ///< osErrorResource (-3).
#define osFlagsErrorParameter 0xFFFFFFFCU ///< osErrorParameter (-4).
#define osFlagsErrorISR       0xFFFFFFFAU ///< osErrorISR (-6).

// Thread attributes (attr_bits in \ref osThreadAttr_t).
#define osThreadDetached      0x00000000U ///< Thread created in detached mode (default)
#define osThreadJoinable      0x00000001U ///< Thread created in joinable mode

// Mutex attributes (attr_bits in \ref osMutexAttr_t).
#define osMutexRecursive      0x00000001U ///< Recursive mutex.
#define osMutexPrioInherit    0x00000002U ///< Priority inherit protocol.
#define osMutexRobust         0x00000008U ///< Robust mutex.

/// Acquire a Mutex or timeout if it is locked.
/// \param[in]     mutex_id      mutex ID obtained by \ref osMutexNew.
/// \param[in]     timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
/// \return status code that indicates the execution status of the function.
osStatus_t osMutexAcquire(osMutexId_t mutex_id, uint32_t timeout);

/// Release a Mutex that was acquired by \ref osMutexAcquire.
/// \param[in]     mutex_id      mutex ID obtained by \ref osMutexNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osMutexRelease(osMutexId_t mutex_id);

#ifdef  __cplusplus
}
#endif

#endif
