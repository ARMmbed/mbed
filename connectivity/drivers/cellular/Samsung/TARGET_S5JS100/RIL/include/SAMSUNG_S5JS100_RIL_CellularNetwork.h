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
#ifndef SAMSUNG_S5JS100_RIL_CELLULARNETWORK_H_
#define SAMSUNG_S5JS100_RIL_CELLULARNETWORK_H_

#include "RIL_CellularNetwork.h"

namespace mbed {

class SAMSUNG_S5JS100_RIL_CellularNetwork: public RIL_CellularNetwork {
public:
    SAMSUNG_S5JS100_RIL_CellularNetwork(RIL_CellularDevice &device);
    virtual ~SAMSUNG_S5JS100_RIL_CellularNetwork();

    /** Differences to CellularNetwork::set_receive_period(...)
     *  1. mode can be only 0 = disable, 1 = enable eDRX
     *  2. Only mode which is supported by this modem is EDRXEUTRAN_NB_S1_mode
     *  3. edrx_value must follow the following table. Allowed values are
     *       TEDRX_5_12_SEC = 0x00,
     *       TEDRX_10_24_SEC = 0x01,
     *       TEDRX_20_48_SEC = 0x02,
     *       TEDRX_40_96_SEC = 0x03,
     *       TEDRX_61_44_SEC = 0x04,
     *       TEDRX_81_92_SEC = 0x05,
     *       TEDRX_102_4_SEC = 0x06,
     *       TEDRX_122_88_SEC = 0x07,
     *       TEDRX_143_36_SEC = 0x08,
     *       TEDRX_163_84_SEC = 0x09,
     *       TEDRX_327_68_SEC = 0x0A,
     *       TEDRX_655_36_SEC = 0x0B,
     *       TEDRX_1310_72_SEC = 0x0C,
     *       TEDRX_2621_44_SEC = 0x0D,
     *       TEDRX_5242_88_SEC = 0x0E,
     *       TEDRX_10485_76_SEC = 0x0F,
     */
    nsapi_error_t set_receive_period(int mode, EDRXAccessTechnology act_type, uint8_t edrx_value) override;

protected: // from RIL_CellularNetwork
    nsapi_error_t set_access_technology_impl(RadioAccessTechnology rat) override;
    void request_set_psm_edrx_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len);
private:
    int _edrx;
};

} /* namespace mbed */

#endif /* SAMSUNG_S5JS100_RIL_CELLULARNETWORK_H_ */
