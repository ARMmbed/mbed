/*
 * Copyright (c) 2017, Arm Limited and affiliates.
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
#ifndef FEATURES_CELLULAR_UNITTESTS_TARGET_H_RANDLIB_H_
#define FEATURES_CELLULAR_UNITTESTS_TARGET_H_RANDLIB_H_

#include <stdint.h>

extern void randLIB_seed_random(void);

uint16_t randLIB_get_random_in_range(uint16_t min, uint16_t max);


#endif /* FEATURES_CELLULAR_UNITTESTS_TARGET_H_RANDLIB_H_ */
