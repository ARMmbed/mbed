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

/** Resets the processor and most of the sub-system
 *
 * @note Does not affect the debug sub-system
 */
#ifndef MBED_POWER_MGMT_H
#define MBED_POWER_MGMT_H

extern void mock_system_reset();

static inline void system_reset(void)
{
    mock_system_reset();
}

void sleep_manager_unlock_deep_sleep(void);

void sleep_manager_lock_deep_sleep(void);

#endif

