/*
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include "cmsis_os.h"
#include "platform/Callback.h"

#define OS_STACK_SIZE 0

namespace rtos {

class Thread {
public:

    Thread(osPriority priority = osPriorityNormal,
           uint32_t stack_size = OS_STACK_SIZE,
           unsigned char *stack_mem = nullptr, const char *name = nullptr)
    {
    }

    Thread(uint32_t tz_module, osPriority priority = osPriorityNormal,
           uint32_t stack_size = OS_STACK_SIZE,
           unsigned char *stack_mem = nullptr, const char *name = nullptr)
    {
    }

    osStatus start(mbed::Callback<void()> task);

    osStatus join() {return 0;};
    osStatus terminate();
    osStatus set_priority(osPriority priority){return 0;};
    osPriority get_priority() const{return osPriorityNormal;};
    uint32_t flags_set(uint32_t flags){return 0;};

    /** State of the Thread */
    enum State {
        Inactive,           /**< NOT USED */
        Ready,              /**< Ready to run */
        Running,            /**< Running */
        WaitingDelay,       /**< Waiting for a delay to occur */
        WaitingJoin,        /**< Waiting for thread to join. Only happens when using RTX directly. */
        WaitingThreadFlag,  /**< Waiting for a thread flag to be set */
        WaitingEventFlag,   /**< Waiting for a event flag to be set */
        WaitingMutex,       /**< Waiting for a mutex event to occur */
        WaitingSemaphore,   /**< Waiting for a semaphore event to occur */
        WaitingMemoryPool,  /**< Waiting for a memory pool */
        WaitingMessageGet,  /**< Waiting for message to arrive */
        WaitingMessagePut,  /**< Waiting for message to be send */
        WaitingInterval,    /**< NOT USED */
        WaitingOr,          /**< NOT USED */
        WaitingAnd,         /**< NOT USED */
        WaitingMailbox,     /**< NOT USED (Mail is implemented as MemoryPool and Queue) */

        /* Not in sync with RTX below here */
        Deleted,            /**< The task has been deleted or not started */
    };

    State get_state() const {
      return Ready;
    };
    uint32_t stack_size() const {
      return 0;
    };
    uint32_t free_stack() const {
      return 0;
    };
    uint32_t used_stack() const {
      return 0;
    };
    uint32_t max_stack() const {
      return 0;
    };
    const char *get_name() const {
      return "";
    };
    osThreadId_t get_id() const {
      return 0;
    };
    virtual ~Thread();
private:
    // Required to share definitions without
    // delegated constructors
    void constructor(osPriority priority = osPriorityNormal,
                     uint32_t stack_size = OS_STACK_SIZE,
                     unsigned char *stack_mem = nullptr,
                     const char *name = nullptr);
};
}
#endif
