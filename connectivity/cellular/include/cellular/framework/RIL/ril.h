/*
 * Copyright (c) 2020, Arm Limited and affiliates.
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
#ifndef MBED_OS_FEATURES_CELLULAR_FRAMEWORK_RIL_RIL_H_
#define MBED_OS_FEATURES_CELLULAR_FRAMEWORK_RIL_RIL_H_

// Define RIL headers for different targets using different versions of ril.h header. Header also can
// contain target specific RIL request/responses which are not present in original ril.h by Google.

#if defined(CELLULAR_RIL_HEADER)
#include CELLULAR_RIL_HEADER
#else
#include "default_ril.h"
#endif

#endif /* MBED_OS_FEATURES_CELLULAR_FRAMEWORK_RIL_RIL_H_ */
