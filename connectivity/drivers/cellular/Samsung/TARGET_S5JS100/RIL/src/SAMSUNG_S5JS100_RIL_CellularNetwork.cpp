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
#include "SAMSUNG_S5JS100_RIL_CellularNetwork.h"
#include "RIL_CellularDevice.h"
#include "CellularContext.h"
#include "CellularLog.h"
#include "ScopedLock.h"

namespace mbed {

SAMSUNG_S5JS100_RIL_CellularNetwork::SAMSUNG_S5JS100_RIL_CellularNetwork(RIL_CellularDevice &device)
    : RIL_CellularNetwork(device), _edrx(-1)
{
}

SAMSUNG_S5JS100_RIL_CellularNetwork::~SAMSUNG_S5JS100_RIL_CellularNetwork()
{
}

nsapi_error_t SAMSUNG_S5JS100_RIL_CellularNetwork::set_access_technology_impl(RadioAccessTechnology rat)
{
    _rat = RAT_NB1;
    if (rat != RAT_NB1) {
        // only rat that is supported by this modem
        return NSAPI_ERROR_UNSUPPORTED;
    }

    return NSAPI_ERROR_OK;
}

nsapi_error_t SAMSUNG_S5JS100_RIL_CellularNetwork::set_receive_period(int mode, EDRXAccessTechnology act_type,
                                                                      uint8_t edrx_value)
{
    if (mode > 1) {
        tr_warning("Invalid eDRX mode: %d", mode);
        return NSAPI_ERROR_UNSUPPORTED;
    }
    if (act_type != EDRXEUTRAN_NB_S1_mode) {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    ScopedLock<rtos::Mutex> lock(_api_mutex);

    _int_data = mode;
    // eDRX enable/disable is set via RIL_REQUEST_PSM_ENABLED
    nsapi_error_t ret = _device.lock_and_send_request(RIL_REQUEST_PSM_ENABLED, &_int_data, 1,
                                                      callback(this, &SAMSUNG_S5JS100_RIL_CellularNetwork::request_set_psm_edrx_response));

    // we disabled eDRX or there was an error, no need to set any values
    if (!_int_data || ret != NSAPI_ERROR_OK) {
        return ret;
    }

    // try to set values only if PSM enable was successful. For now we use default value(0) as we don't
    // have PTW in API.
    if (ret == NSAPI_ERROR_OK) {
        unsigned int *data = new unsigned int[2];
        _edrx = edrx_value;
        data[0] = 0;
        data[1] = _edrx;
        ret = _device.lock_and_send_request(RIL_REQUEST_SET_EDRX, (void *)data, sizeof(unsigned int) * 2,
                                            callback(this, &SAMSUNG_S5JS100_RIL_CellularNetwork::request_set_psm_edrx_response));
        delete [] data;
    }

    return ret;
}

void SAMSUNG_S5JS100_RIL_CellularNetwork::request_set_psm_edrx_response(ril_token_t *token, RIL_Errno err, void *response,
                                                                        size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();
        token->response_error = (err == RIL_E_SUCCESS) ? NSAPI_ERROR_OK : NSAPI_ERROR_DEVICE_ERROR;
        token->cond_var->notify_one();
        token->cond_mutex->unlock();
    }
}

} /* namespace mbed */
