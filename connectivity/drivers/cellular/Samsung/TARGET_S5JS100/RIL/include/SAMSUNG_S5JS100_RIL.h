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
#ifndef SAMSUNG_S5JS100_RIL_H_
#define SAMSUNG_S5JS100_RIL_H_

#include "RIL_CellularDevice.h"

namespace mbed {

class SAMSUNG_S5JS100_RIL: public RIL_CellularDevice {
public:
    SAMSUNG_S5JS100_RIL(RILAdaptation &ril);
    virtual ~SAMSUNG_S5JS100_RIL();

protected:
    RIL_CellularContext *create_context_impl(const char *apn, bool cp_req = false, bool nonip_req = false) override;
    RIL_CellularNetwork *open_network_impl() override;
    RIL_CellularInformation *open_information_impl() override;
    nsapi_error_t set_power_save_mode(int periodic_time, int active_time = 0) override;
    nsapi_error_t send_at_command(char *data, size_t data_len);

private:
    SAMSUNG_S5JS100_RIL();

    friend class RILAdaptation;
    void unsolicited_response(int response_id, const void *data, size_t data_len) override;
    void common_extension_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len);
    void pms_status_changed(const void *data, size_t data_len);
    void pms_timer_changed(const void *data, size_t data_len);
    void at_response(const void *data, size_t data_len);
    void handle_ril_restart();

private:
    unsigned int _periodic_time;
    unsigned int _active_time;
};

} /* namespace mbed */

#endif /* SAMSUNG_S5JS100_RIL_H_ */
