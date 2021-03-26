/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
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

#if MBED_CONF_NSAPI_PRESENT

#include "ONBOARD_SAMSUNG_S5JS100_RIL.h"
#include "SAMSUNG_S5JS100_RILAdaptation.h"
#include "CellularLog.h"

using namespace mbed;

ONBOARD_SAMSUNG_S5JS100_RIL::ONBOARD_SAMSUNG_S5JS100_RIL(SAMSUNG_S5JS100_RILAdaptation &ril)
    : SAMSUNG_S5JS100_RIL(ril)
{
}

CellularDevice *CellularDevice::get_target_default_instance()
{
    static SAMSUNG_S5JS100_RILAdaptation ril;
    static ONBOARD_SAMSUNG_S5JS100_RIL device(ril);
    return &device;
}

#endif // MBED_CONF_NSAPI_PRESENT
