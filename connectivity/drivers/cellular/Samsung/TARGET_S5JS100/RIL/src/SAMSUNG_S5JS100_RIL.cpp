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
#include "SAMSUNG_S5JS100_RIL.h"
#include "SAMSUNG_S5JS100_RIL_CellularContext.h"
#include "SAMSUNG_S5JS100_RIL_CellularNetwork.h"
#include "SAMSUNG_S5JS100_RIL_CellularInformation.h"
#include "SAMSUNG_S5JS100_RIL_IF.h"
#include "CellularLog.h"
#include "ScopedLock.h"

using namespace mbed;

static const intptr_t cellular_properties[RIL_CellularDevice::PROPERTY_MAX] = {
    1,  // C_EREG
    0,  // C_GREG
    0,  // C_REG
    1,  // PROPERTY_IPV4_STACK
    1,  // PROPERTY_IPV6_STACK
    1,  // PROPERTY_IPV4V6_STACK
    1,  // PROPERTY_NON_IP_PDP_TYPE
};

SAMSUNG_S5JS100_RIL::SAMSUNG_S5JS100_RIL(RILAdaptation &ril)
    : RIL_CellularDevice(ril),
      _periodic_time(0),
      _active_time(0)
{
    _property_array = cellular_properties;
    // open information here so we can save unsolicited iccd response
    open_information();
}

SAMSUNG_S5JS100_RIL::~SAMSUNG_S5JS100_RIL()
{
}

RIL_CellularContext *SAMSUNG_S5JS100_RIL::create_context_impl(const char *apn, bool cp_req, bool nonip_req)
{
    return new SAMSUNG_S5JS100_RIL_CellularContext(*this, apn, cp_req, nonip_req);
}

RIL_CellularNetwork *SAMSUNG_S5JS100_RIL::open_network_impl()
{
    return new SAMSUNG_S5JS100_RIL_CellularNetwork(*this);
}

RIL_CellularInformation *SAMSUNG_S5JS100_RIL::open_information_impl()
{
    return new SAMSUNG_S5JS100_RIL_CellularInformation(*this);
}

nsapi_error_t SAMSUNG_S5JS100_RIL::send_at_command(char *data, size_t data_len)
{
    if (data == nullptr || data_len <= 0) {
        return NSAPI_ERROR_PARAMETER;
    }
    ScopedLock<rtos::Mutex> lock(_api_mutex);
    nsapi_error_t ret = lock_and_send_request(RIL_REQUEST_SET_FORWARDING_AT_COMMAND, data, data_len,
                                              callback(this, &SAMSUNG_S5JS100_RIL::common_extension_response));

    return ret;
}

nsapi_error_t SAMSUNG_S5JS100_RIL::set_power_save_mode(int periodic_time, int active_time)
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);

    if (periodic_time == 0 && active_time == 0) {
        _data = 0;
    } else {
        _data = 1;
    }

    nsapi_error_t ret = lock_and_send_request(RIL_REQUEST_PSM_ENABLED, &_data, 1, callback(this, &SAMSUNG_S5JS100_RIL::common_extension_response));

    // we disabled PSM, no need to set any values
    if (!_data) {
        return ret;
    }

    // try to set values only if PSM enable was successful.
    if (ret == NSAPI_ERROR_OK) {
        unsigned int *data = new unsigned int[2];
        _periodic_time = periodic_time;
        _active_time = active_time;
        data[0] = _periodic_time;
        data[1] = _active_time;
        ret = lock_and_send_request(RIL_REQUEST_PSM_TIMER, (void *)data, sizeof(unsigned int) * 2,
                                    callback(this, &SAMSUNG_S5JS100_RIL::common_extension_response));
        delete [] data;
    }

    return ret;
}

void SAMSUNG_S5JS100_RIL::pms_timer_changed(const void *data, size_t data_len)
{
    const int *dt = (const int *)data;
    if (data && data_len > 0) {
        int status = dt[0];
        tr_debug("PSM timer was %s", status ? "accepted" : "rejected");
        if (status) {
            // accepted, check the values
            tr_debug("PSM T3412 timer: %d, T3324 timer: %d", dt[1], dt[2]);
        }
    }
    // TODO: send notification to application. need to specify new cellular_event_status
}

void SAMSUNG_S5JS100_RIL::pms_status_changed(const void *data, size_t data_len)
{
}

void SAMSUNG_S5JS100_RIL::at_response(const void *data, size_t data_len)
{
    cell_callback_data_t dt;
    dt.data = data;
    dt.status_data = data_len;
    if (_status_cb) {
        _status_cb((nsapi_event_t)CellularRILATResponse, (intptr_t)&dt);
    }
}

void SAMSUNG_S5JS100_RIL::handle_ril_restart()
{
    tr_debug("Shutting down RIL interface...");

    // Disconnect all contexts
    RIL_CellularContext *curr = _context_list;
    while (curr) {
        curr->disconnect(false);
        curr = (RIL_CellularContext *)curr->_next;
    }

    // Close RIL interface. Next connect() will open it again.
    hard_power_off();

    tr_debug("Shutting down RIL interface... Done");
}

void SAMSUNG_S5JS100_RIL::unsolicited_response(int response_id, const void *data, size_t data_len)
{
    switch (response_id) {
        case RIL_UNSOL_PSM_TIMER_CHANGED:
            pms_timer_changed(data, data_len);
            break;
        case RIL_UNSOL_PSM_STATUS_CHANGED:
            pms_status_changed(data, data_len);
            break;
        case RIL_UNSOL_FORWARDING_AT_COMMAND_IND:
            at_response(data, data_len);
            break;
        case RIL_UNSOL_RIL_RESTART_REQUIRED:
            handle_ril_restart();
            break;
        default:
            break;
    }

    RIL_CellularDevice::unsolicited_response(response_id, data, data_len);
}

void SAMSUNG_S5JS100_RIL::common_extension_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();
        token->response_error = (err == RIL_E_SUCCESS) ? NSAPI_ERROR_OK : NSAPI_ERROR_DEVICE_ERROR;
        token->cond_var->notify_one();
        token->cond_mutex->unlock();
    }
}
