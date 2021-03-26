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
#include "SAMSUNG_S5JS100_RILAdaptation.h"
#include "CellularLog.h"

extern "C" {
    void RIL_Deinit(void);
}

namespace mbed {

SAMSUNG_S5JS100_RILAdaptation::SAMSUNG_S5JS100_RILAdaptation()
{
}

SAMSUNG_S5JS100_RILAdaptation::~SAMSUNG_S5JS100_RILAdaptation()
{
    deinit_ril();
}

nsapi_error_t SAMSUNG_S5JS100_RILAdaptation::deinit_ril()
{
    RIL_Deinit();
    return RILAdaptation::deinit_ril();
}

#if MBED_CONF_MBED_TRACE_ENABLE
#define RIL_DEFINE(req) case req: ret = #req; break;
const char *SAMSUNG_S5JS100_RILAdaptation::get_ril_name(int request)
{
    const char *ret;
    switch (request) {
            // Extensions
            RIL_DEFINE(RIL_REQUEST_SET_EDRX);
            RIL_DEFINE(RIL_REQUEST_SET_FORWARDING_AT_COMMAND);
            RIL_DEFINE(RIL_REQUEST_PSM_ENABLED);
            RIL_DEFINE(RIL_REQUEST_PSM_TIMER);

            // Unsolicited responses (extensions)
            RIL_DEFINE(RIL_UNSOL_FORWARDING_AT_COMMAND_IND);
            RIL_DEFINE(RIL_UNSOL_PSM_TIMER_CHANGED);
            RIL_DEFINE(RIL_UNSOL_PSM_STATUS_CHANGED);
            RIL_DEFINE(RIL_UNSOL_ICCID_INFO);
            RIL_DEFINE(RIL_UNSOL_RIL_RESTART_REQUIRED);

        default:
            ret = RILAdaptation::get_ril_name(request);
    }

    return ret;
}
#endif

} /* namespace mbed */
