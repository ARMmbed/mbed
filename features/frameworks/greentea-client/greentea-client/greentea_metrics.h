/*
 * mbed Microcontroller Library
 * Copyright (c) 2006-2019 ARM Limited
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
#include "platform/mbed_toolchain.h"

/** \addtogroup frameworks */
/** @{*/
#ifndef GREENTEA_METRICS_H
#define GREENTEA_METRICS_H

/**
 *  Setup platform specific metrics
 */
MBED_DEPRECATED_SINCE("mbed-os-6.13", "Greentea metrics API are deprecated")
void greentea_metrics_setup(void);

/**
 *  Report and cleanup platform specifc metrics
 */
MBED_DEPRECATED_SINCE("mbed-os-6.13", "Greentea metrics API are deprecated")
void greentea_metrics_report(void);

#endif

/** @}*/
