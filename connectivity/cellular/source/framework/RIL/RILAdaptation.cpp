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
#include "RILAdaptation.h"
#include "RIL_CellularDevice.h"
#include "CellularLog.h"

namespace mbed {

static RILAdaptation *handle = nullptr;

RILAdaptation::RILAdaptation()
    : _device(nullptr),
      _radio_funcs(nullptr),
      _ril_token_id(0)
{
    MBED_ASSERT(!handle);
    handle = this;
}

RILAdaptation::~RILAdaptation()
{
    handle = nullptr;
}

void RILAdaptation::set_device(RIL_CellularDevice *device)
{
    _device = device;
}

/* Just a dummy implementation for non-RIL builds */
extern "C"
MBED_WEAK const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
    return nullptr;
}

nsapi_error_t RILAdaptation::init_ril()
{
    if (!_radio_funcs) {
        static const struct RIL_Env env = {
            RILAdaptation::request_complete,
            RILAdaptation::unsolicited_response,
            RILAdaptation::request_timed_callback,
            RILAdaptation::request_ack
        };

        _radio_funcs = RIL_Init(&env, 0, nullptr);

        if (!_radio_funcs) {
            tr_warning("RILAdaptation::init_ril(failed)");
            return NSAPI_ERROR_DEVICE_ERROR;
        }

        _ril_token_id = 0;
    }
    return NSAPI_ERROR_OK;
}

nsapi_error_t RILAdaptation::deinit_ril()
{
    _radio_funcs = nullptr;
    return NSAPI_ERROR_OK;
}

RIL_CellularDevice *RILAdaptation::get_device() const
{
    return _device;
}

// request methods for ril requests
ril_token_t *RILAdaptation::send_request(int request, void *data, size_t datalen, Callback<void(ril_token_t *, RIL_Errno, void *, size_t)> callback,
                                         rtos::Mutex *cond_mutex, rtos::ConditionVariable *cond_var)
{
    if (!_radio_funcs) {
        tr_warning("RILAdaptation::send_request, Radio funcs not available");
        return nullptr;
    }

    RIL_RadioState radio_state = _radio_funcs->onStateRequest();
    if (radio_state == RADIO_STATE_UNAVAILABLE) {
        tr_warning("RILAdaptation::send_request() Radio state was RADIO_STATE_UNAVAILABLE, return NULL");
        return nullptr;
    }

    _ril_token_id++;

    ril_token_t *token = new ril_token_t();
    token->token_id = _ril_token_id;
    token->request_id = request;
    token->cb = callback;
    token->cond_mutex = cond_mutex;
    token->cond_var = cond_var;

    tr_debug("RILAdaptation::send_request, request_id: %s (%d), token_id: %d",
             get_ril_name(token->request_id), token->request_id, token->token_id);

    _radio_funcs->onRequest(request, data, datalen, token);

    return token;
}

RIL_RadioState RILAdaptation::get_radio_state()
{
    if (_radio_funcs) {
        return _radio_funcs->onStateRequest();
    } else {
        tr_warning("RILAdaptation::get_radio_state, Radio funcs not available");
        return RADIO_STATE_UNAVAILABLE;
    }
}

const char *RILAdaptation::get_version(void)
{
    if (_radio_funcs) {
        return _radio_funcs->getVersion();
    } else {
        tr_warning("RILAdaptation::get_version, Radio funcs not available");
        return nullptr;
    }
}

void RILAdaptation::cancel_request(ril_token_t *token)
{
    if (_radio_funcs) {
        _radio_funcs->onCancel(token);
    } else {
        tr_warning("RILAdaptation::cancel_request, Radio funcs not available");
    }
}

RILAdaptation *RILAdaptation::get_instance()
{
    return handle;
}

// callback methods from ril
void RILAdaptation::request_ack(RIL_Token t)
{
    ril_token_t *token = (ril_token_t *)t;
    (void)token; // Silence compiler warning if traces are disabled
    tr_debug("RILAdaptation::request_ack, token_id: %d", token->token_id);
    RILAdaptation::get_instance()->get_device()->request_ack(token);
}

void RILAdaptation::request_timed_callback(RIL_TimedCallback callback, void *param,
                                           const struct timeval *relative_time)
{
    tr_debug("RILAdaptation::request_timed_callback");
    RILAdaptation::get_instance()->get_device()->request_timed_callback(callback, param, relative_time);
}

void RILAdaptation::unsolicited_response(int response_id, const void *data, size_t data_len)
{
    tr_debug("RILAdaptation::unsolicited_response, response_id: %s (%d)",
             RILAdaptation::get_instance()->get_ril_name(response_id), response_id);
    RILAdaptation::get_instance()->get_device()->unsolicited_response(response_id, data, data_len);
}

void RILAdaptation::request_complete(RIL_Token t, RIL_Errno e, void *response, size_t response_len)
{
    ril_token_t *token = (ril_token_t *)t;
    tr_debug("RILAdaptation::request_complete, request_id: %s (%d), token_id: %d error: %d",
             RILAdaptation::get_instance()->get_ril_name(token->request_id), token->request_id, token->token_id, e);

    token->cb(token, e, response, response_len);
}

#if MBED_CONF_MBED_TRACE_ENABLE
#define RIL_DEFINE(req) case req: ret = #req; break;
const char *RILAdaptation::get_ril_name(int request)
{
    const char *ret;
    switch (request) {
            // Requests
            RIL_DEFINE(RIL_REQUEST_GET_IMSI);
            RIL_DEFINE(RIL_REQUEST_DATA_REGISTRATION_STATE);
            RIL_DEFINE(RIL_REQUEST_SIGNAL_STRENGTH);
            RIL_DEFINE(RIL_REQUEST_RADIO_POWER);
            RIL_DEFINE(RIL_REQUEST_SETUP_DATA_CALL);
            RIL_DEFINE(RIL_REQUEST_DEACTIVATE_DATA_CALL);
            RIL_DEFINE(RIL_REQUEST_DATA_CALL_LIST);
            RIL_DEFINE(RIL_REQUEST_DEVICE_IDENTITY);
            RIL_DEFINE(RIL_REQUEST_SET_INITIAL_ATTACH_APN);
            RIL_DEFINE(RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL);
            RIL_DEFINE(RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC);
            RIL_DEFINE(RIL_REQUEST_BASEBAND_VERSION);
            RIL_DEFINE(RIL_REQUEST_GET_SIM_STATUS);
            RIL_DEFINE(RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE);

            // Unsolicited responses
            RIL_DEFINE(RIL_UNSOL_NITZ_TIME_RECEIVED);
            RIL_DEFINE(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED);
            RIL_DEFINE(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED);
            RIL_DEFINE(RIL_UNSOL_SIGNAL_STRENGTH);
            RIL_DEFINE(RIL_UNSOL_DATA_CALL_LIST_CHANGED);
            RIL_DEFINE(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED);

        default:
            ret = "UNKNOWN";
    }

    return ret;
}
#endif

} /* namespace mbed */
