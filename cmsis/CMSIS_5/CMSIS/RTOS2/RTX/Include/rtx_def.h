/*
 * Copyright (c) 2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * -----------------------------------------------------------------------------
 *
 * Project:     CMSIS-RTOS RTX
 * Title:       RTX derived definitions
 *
 * -----------------------------------------------------------------------------
 */

#ifndef RTX_DEF_H_
#define RTX_DEF_H_

#ifdef   _RTE_
#include "RTE_Components.h"
#endif
#include "RTX_Config.h"

#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
  #define RTX_OBJ_MEM_USAGE
#endif

#if (defined(OS_STACK_CHECK) && (OS_STACK_CHECK != 0))
  #define RTX_STACK_CHECK
#endif

#ifdef  RTE_CMSIS_RTOS2_RTX5_ARMV8M_NS
  #define DOMAIN_NS             1
#endif

#endif  // RTX_DEF_H_
