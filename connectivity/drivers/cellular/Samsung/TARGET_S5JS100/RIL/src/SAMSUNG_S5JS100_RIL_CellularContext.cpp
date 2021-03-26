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
#include "SAMSUNG_S5JS100_RIL_CellularContext.h"
#include "SAMSUNG_S5JS100_L3IP_Driver.h"

namespace mbed {

SAMSUNG_S5JS100_RIL_CellularContext::SAMSUNG_S5JS100_RIL_CellularContext(RIL_CellularDevice &device, const char *apn,
                                                                         bool cp_req, bool nonip_req)
    : RIL_CellularContext(device, apn, cp_req, nonip_req),
      _cp_netif(nullptr)
{
}

SAMSUNG_S5JS100_RIL_CellularContext::~SAMSUNG_S5JS100_RIL_CellularContext()
{
    delete _cp_netif;
    _cp_netif = nullptr;

    delete_L3IP_driver();
}

ControlPlane_netif *SAMSUNG_S5JS100_RIL_CellularContext::get_cp_netif()
{
    if (!_cp_netif) {
        _cp_netif = new SAMSUNG_S5JS100_RIL_ControlPlane_netif(_ifname);
    }
    return _cp_netif;
}

L3IP *SAMSUNG_S5JS100_RIL_CellularContext::get_L3IP_driver()
{
    return new SAMSUNG_S5JS100_L3IP_Driver(_ifname, _mtu);
}

void SAMSUNG_S5JS100_RIL_CellularContext::delete_L3IP_driver()
{
    delete (SAMSUNG_S5JS100_L3IP_Driver *)_l3ip_driver;
    _l3ip_driver = nullptr;
}

} /* namespace mbed */
