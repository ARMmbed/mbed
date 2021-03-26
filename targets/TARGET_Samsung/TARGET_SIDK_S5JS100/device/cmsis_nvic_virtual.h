/*
 * Copyright (c) 2020 Arm Limited
 *
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
/* @file  : cmsis_nvic_virtual.h
 * @brief : NVIC functions list
 * @date  : December 2019
 * @note  : List of NVIC macro and customize TFM interrupt
 */


#include "cmsis.h"

#ifndef NVIC_VIRTUAL_H
#define NVIC_VIRTUAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "s5js100_type.h"

/* NVIC functions */
#define NVIC_SetPriorityGrouping    __NVIC_SetPriorityGrouping
#define NVIC_GetPriorityGrouping    __NVIC_GetPriorityGrouping
#define NVIC_EnableIRQ              __NVIC_EnableIRQ
#define NVIC_GetEnableIRQ           __NVIC_GetEnableIRQ
#define NVIC_DisableIRQ             __NVIC_DisableIRQ
#define NVIC_GetPendingIRQ          __NVIC_GetPendingIRQ
#define NVIC_SetPendingIRQ          __NVIC_SetPendingIRQ
#define NVIC_ClearPendingIRQ        __NVIC_ClearPendingIRQ
#define NVIC_GetActive              __NVIC_GetActive
#define NVIC_SetPriority            __NVIC_SetPriority
#define NVIC_GetPriority            __NVIC_GetPriority
#define NVIC_SystemReset            __NVIC_TFMSystemReset


/**
 * \brief Overriding the default CMSIS system reset implementation by calling
 *        secure TFM service.
 *
 */
void disable_counter(void);
void up_systemreset(void);
void pmu_test_auto(int sleep_time_msec);
__NO_RETURN __STATIC_INLINE void __NVIC_TFMSystemReset(void)
{
    up_systemreset();
//  disable_counter();
//  pmu_test_auto(3000);
}

#ifdef __cplusplus
}
#endif

#endif
