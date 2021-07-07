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

#ifndef __RTX_OS__
#define __RTX_OS__

#include "inttypes.h"

typedef struct osRtxSemaphore_s {
    uint8_t                          id;  ///< Object Identifier
    uint8_t                       state;  ///< Object State
    uint8_t                       flags;  ///< Object Flags
    uint8_t                    reserved;
    const char                    *name;  ///< Object Name
    uint16_t                     tokens;  ///< Current number of tokens
    uint16_t                 max_tokens;  ///< Maximum number of tokens
} osRtxSemaphore_t;

typedef struct osRtxThread_s {
    uint8_t                          id;  ///< Object Identifier
    uint8_t                       state;  ///< Object State
    uint8_t                       flags;  ///< Object Flags
    uint8_t                        attr;  ///< Object Attributes
    const char                    *name;  ///< Object Name
    struct osRtxThread_s   *thread_next;  ///< Link pointer to next Thread in Object list
    struct osRtxThread_s   *thread_prev;  ///< Link pointer to previous Thread in Object list
    struct osRtxThread_s    *delay_next;  ///< Link pointer to next Thread in Delay list
    struct osRtxThread_s    *delay_prev;  ///< Link pointer to previous Thread in Delay list
    struct osRtxThread_s   *thread_join;  ///< Thread waiting to Join
    uint32_t                      delay;  ///< Delay Time
    int8_t                     priority;  ///< Thread Priority
    int8_t                priority_base;  ///< Base Priority
    uint8_t                 stack_frame;  ///< Stack Frame (EXC_RETURN[7..0])
    uint8_t               flags_options;  ///< Thread/Event Flags Options
    uint32_t                 wait_flags;  ///< Waiting Thread/Event Flags
    uint32_t               thread_flags;  ///< Thread Flags
    struct osRtxMutex_s     *mutex_list;  ///< Link pointer to list of owned Mutexes
    void                     *stack_mem;  ///< Stack Memory
    uint32_t                 stack_size;  ///< Stack Size
    uint32_t                         sp;  ///< Current Stack Pointer
    uint32_t                thread_addr;  ///< Thread entry address
    uint32_t                  tz_memory;  ///< TrustZone Memory Identifier
    void                       *context;  ///< Context for OsEventObserver objects
} osRtxThread_t;

typedef struct {
    uint8_t                          id;  ///< Object Identifier
    uint8_t                       state;  ///< Object State
    uint8_t                       flags;  ///< Object Flags
    uint8_t                    reserved;
    const char                    *name;  ///< Object Name
    osRtxThread_t          *thread_list;  ///< Waiting Threads List
    uint32_t                event_flags;  ///< Event Flags
} osRtxEventFlags_t;

#endif
