/* Copyright (c) 2019 ARM Limited
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

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>
#include "internal/mbed_rtos1_types.h"
#include "rtos/Kernel.h"

namespace rtos {
class Semaphore  {
public:
    Semaphore(int32_t count = 0);
    Semaphore(int32_t count, uint16_t max_count);
    void acquire();
    bool try_acquire();
    bool try_acquire_for(uint32_t millisec);
    bool try_acquire_for(Kernel::Clock::duration_u32 rel_time);
    bool try_acquire_until(uint64_t millisec);
    bool try_acquire_until(Kernel::Clock::time_point abs_time);
    osStatus release(void);
    ~Semaphore();
private:
    void constructor(int32_t count, uint16_t max_count);
};
}

#endif
