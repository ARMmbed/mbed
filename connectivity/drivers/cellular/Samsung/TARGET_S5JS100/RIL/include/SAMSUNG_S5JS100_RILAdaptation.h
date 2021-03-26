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
#ifndef SAMSUNG_S5JS100_RIL_ADAPTATION_H_
#define SAMSUNG_S5JS100_RIL_ADAPTATION_H_

#include "RILAdaptation.h"

namespace mbed {

class SAMSUNG_S5JS100_RILAdaptation : public RILAdaptation {
public:
    SAMSUNG_S5JS100_RILAdaptation();
    virtual ~SAMSUNG_S5JS100_RILAdaptation();

    nsapi_error_t deinit_ril() override;

#if MBED_CONF_MBED_TRACE_ENABLE
    const char *get_ril_name(int request) override;
#endif
};

} /* namespace mbed */

#endif /* SAMSUNG_S5JS100_RIL_ADAPTATION_H_ */
