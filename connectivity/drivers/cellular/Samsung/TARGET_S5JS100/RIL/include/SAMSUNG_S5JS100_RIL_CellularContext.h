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
#ifndef SAMSUNG_S5JS100_RIL_CELLULARCONTEXT_H_
#define SAMSUNG_S5JS100_RIL_CELLULARCONTEXT_H_

#include "RIL_CellularContext.h"
#include "SAMSUNG_S5JS100_RIL_ControlPlane_netif.h"

namespace mbed {

class SAMSUNG_S5JS100_RIL_CellularContext: public RIL_CellularContext {
public:
    SAMSUNG_S5JS100_RIL_CellularContext(RIL_CellularDevice &device, const char *apn = 0, bool cp_req = false,
                                        bool nonip_req = false);
    virtual ~SAMSUNG_S5JS100_RIL_CellularContext();

    ControlPlane_netif *get_cp_netif() override;

protected: // from RIL_CellularContext
    L3IP *get_L3IP_driver() override;
    void delete_L3IP_driver() override;

    SAMSUNG_S5JS100_RIL_ControlPlane_netif *_cp_netif;
};

} /* namespace mbed */

#endif /* SAMSUNG_S5JS100_RIL_CELLULARCONTEXT_H_ */
