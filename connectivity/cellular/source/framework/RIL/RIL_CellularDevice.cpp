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
#include "RIL_CellularDevice.h"
#include "RILAdaptation.h"
#include "RIL_CellularContext.h"
#include "RIL_CellularNetwork.h"
#include "RIL_CellularInformation.h"
#include "CellularLog.h"
#include "ScopedLock.h"

namespace mbed {

RIL_CellularDevice::RIL_CellularDevice(RILAdaptation &ril)
    : CellularDevice(),
      _property_array(nullptr),
      _data(0),
      _context_list(nullptr),
      _ril(ril),
      _network(nullptr),
      _information(nullptr),
      _device_ready(false),
      _device_ready_cb()
{
    _ril.set_device(this);
}

RIL_CellularDevice::~RIL_CellularDevice()
{
    delete _network;
    delete _information;

    RIL_CellularContext *next;
    while (_context_list) {
        next = (RIL_CellularContext *)_context_list->_next;
        delete _context_list;
        _context_list = next;
    }

    _ril.set_device(nullptr);
}

intptr_t RIL_CellularDevice::get_property(CellularProperty key)
{
    return _property_array[key];
}

nsapi_error_t RIL_CellularDevice::hard_power_on()
{
    return _ril.init_ril();
}

nsapi_error_t RIL_CellularDevice::hard_power_off()
{
    return _ril.deinit_ril();
}

nsapi_error_t RIL_CellularDevice::soft_power_on()
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);
    _data = 1;
    nsapi_error_t ret = lock_and_send_request(RIL_REQUEST_RADIO_POWER, &_data, 1, callback(this, &RIL_CellularDevice::common_response));
    return ret;
}

nsapi_error_t RIL_CellularDevice::soft_power_off()
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);
    _data = 0;
    nsapi_error_t ret = lock_and_send_request(RIL_REQUEST_RADIO_POWER, &_data, 1, callback(this, &RIL_CellularDevice::common_response));
    return ret;
}

nsapi_error_t RIL_CellularDevice::set_pin(const char *sim_pin)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t RIL_CellularDevice::get_sim_state(SimState &state)
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);
    nsapi_error_t ret = lock_and_send_request(RIL_REQUEST_GET_SIM_STATUS, nullptr, 0, callback(this, &RIL_CellularDevice::sim_status_response));

    if (ret == NSAPI_ERROR_OK) {
        get_radiostatus(&state);
    }

    return ret;
}

CellularContext *RIL_CellularDevice::create_context(const char *apn, bool cp_req, bool nonip_req)
{
    RIL_CellularContext *ctx = create_context_impl(apn, cp_req, nonip_req);

    if (_context_list == nullptr) {
        _context_list = ctx;
        return ctx;
    }

    RIL_CellularContext *curr = _context_list;
    while (curr->_next) {
        curr = (RIL_CellularContext *)curr->_next;
    }

    curr->_next = ctx;
    return ctx;
}

RIL_CellularContext *RIL_CellularDevice::create_context_impl(const char *apn, bool cp_req, bool nonip_req)
{
    return new RIL_CellularContext(*this, apn, cp_req, nonip_req);
}

void RIL_CellularDevice::delete_context(CellularContext *context)
{
    RIL_CellularContext *curr = _context_list;
    RIL_CellularContext *prev = nullptr;
    while (curr) {
        if (curr == context) {
            // delete context first as RIL_CellularContext::~RIL_CellularContext() will call disconnect which makes
            // RIL request and ril responses are forwarded only to context which started it.
            curr = (RIL_CellularContext *)curr->_next;
            delete (RIL_CellularContext *)context;
            if (prev == nullptr) {
                _context_list = curr;
            } else {
                prev->_next = curr;
            }
            break;
        }
        prev = curr;
        curr = (RIL_CellularContext *)curr->_next;
    }
}

nsapi_error_t RIL_CellularDevice::clear()
{
    return NSAPI_ERROR_OK;
}

CellularContext *RIL_CellularDevice::get_context_list() const
{
    return _context_list;
}

nsapi_error_t RIL_CellularDevice::set_baud_rate(int baud_rate)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularDevice::send_at_command(char *data, size_t data_len)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

CellularNetwork *RIL_CellularDevice::open_network()
{
    if (!_network) {
        _network = open_network_impl();
    }
    _network_ref_count++;
    return _network;
}

RIL_CellularNetwork *RIL_CellularDevice::open_network_impl()
{
    return new RIL_CellularNetwork(*this);
}

#if MBED_CONF_CELLULAR_USE_SMS || defined(DOXYGEN_ONLY)
CellularSMS *RIL_CellularDevice::open_sms()
{
    return nullptr;
}

void RIL_CellularDevice::close_sms()
{
}
#endif

CellularInformation *RIL_CellularDevice::open_information()
{
    if (!_information) {
        _information = open_information_impl();
    }
    _info_ref_count++;
    return _information;
}

RIL_CellularInformation *RIL_CellularDevice::open_information_impl()
{
    return new RIL_CellularInformation(*this);
}

void RIL_CellularDevice::close_network()
{
    if (_network) {
        _network_ref_count--;
        if (_network_ref_count == 0) {
            delete _network;
            _network = nullptr;
        }
    }
}

void RIL_CellularDevice::close_information()
{
    if (_information) {
        _info_ref_count--;
        if (_info_ref_count == 0) {
            delete _information;
            _information = nullptr;
        }
    }
}

void RIL_CellularDevice::set_timeout(int /*timeout*/)
{
}

void RIL_CellularDevice::modem_debug_on(bool /*on*/)
{
}

nsapi_error_t RIL_CellularDevice::init()
{
    open_network();// ONLY WAY to fail if new fails and then mbed-os crashes
    return NSAPI_ERROR_OK;
}

nsapi_error_t RIL_CellularDevice::is_ready()
{
    get_radiostatus();
    return _device_ready ? NSAPI_ERROR_OK : NSAPI_ERROR_DEVICE_ERROR;
}

void RIL_CellularDevice::set_ready_cb(Callback<void()> callback)
{
    _device_ready_cb = callback;
}

nsapi_error_t RIL_CellularDevice::set_power_save_mode(int periodic_time, int active_time)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

ATHandler *RIL_CellularDevice::get_at_handler()
{
    return nullptr;
}

void RIL_CellularDevice::get_radiostatus(SimState *state, bool unsolicited_resp)
{
    _device_ready = false;
    if (state) {
        *state = SimStateUnknown;
    }
    // status changed, we need to query it, sync method
    RIL_RadioState radio_state = _ril.get_radio_state();
    tr_debug("RIL_CellularDevice: Radio state: %d", radio_state);

    switch (radio_state) {
        case RADIO_STATE_OFF:                   /* Radio explictly powered off (eg CFUN=0) */
        case RADIO_STATE_UNAVAILABLE:           /* Radio unavailable (eg, resetting or not booted) States 2-9 below are deprecated. Just leaving them here for backward compatibility. */
        case RADIO_STATE_SIM_NOT_READY:         /* Radio is on, but the SIM interface is not ready */
        case RADIO_STATE_RUIM_NOT_READY:        /* Radio is on, but the RUIM interface is not ready */
        case RADIO_STATE_NV_NOT_READY:          /* Radio is on, but the NV interface is not available */
            tr_warn("SIM state unknown");
            break;
        case RADIO_STATE_RUIM_LOCKED_OR_ABSENT: /* RUIM PIN locked, PUK required, network personalization locked, or RUIM absent */
        case RADIO_STATE_SIM_LOCKED_OR_ABSENT:  /* SIM PIN locked, PUK required, network personalization locked, or SIM absent */
            if (state) {
                *state = SimStatePinNeeded;
            }
            tr_warn("SIM pin needed");
            _device_ready = true;
            break;
        case RADIO_STATE_NV_READY:              /* Radio is on and the NV interface is available */
        case RADIO_STATE_RUIM_READY:            /* Radio is on and the RUIM interface is available */
        case RADIO_STATE_SIM_READY:             /* Radio is on and SIM interface is available */
        case RADIO_STATE_ON:
            if (state) {
                *state = SimStateReady;
                tr_info("SIM is ready");
            }
            _device_ready = true;
            break;
        default:
            MBED_ASSERT(0);
            break;
    }

    if (unsolicited_resp && _device_ready && _device_ready_cb) {
        // call for faster start-up
        _device_ready_cb();
    }
}

// callback methods from ril/RILAdaptation
void RIL_CellularDevice::request_ack(ril_token_t */*token*t*/)
{
}

void RIL_CellularDevice::request_timed_callback(RIL_TimedCallback /*callback*/, void */*param*/,
                                                const struct timeval */*relative_time*/)
{
}

void RIL_CellularDevice::sim_status_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();

        if (err == RIL_E_SUCCESS && response && response_len) {
            const RIL_CardStatus_v6 *v6 = (const RIL_CardStatus_v6 *)response;
            RIL_CardState card_state = v6->card_state;
            tr_debug("get_sim_status_complete, %s", (card_state == RIL_CARDSTATE_PRESENT) ? "SIM Card present" : "SIM error");
            if (card_state != RIL_CARDSTATE_PRESENT) {
                token->response_error = NSAPI_ERROR_DEVICE_ERROR;
            } else {
                token->response_error = NSAPI_ERROR_OK;
            }
        } else {
            token->response_error = NSAPI_ERROR_DEVICE_ERROR;
        }

        token->cond_var->notify_one();
        token->cond_mutex->unlock();
    }
}

void RIL_CellularDevice::common_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();
        token->response_error = (err == RIL_E_SUCCESS) ? NSAPI_ERROR_OK : NSAPI_ERROR_DEVICE_ERROR;
        token->cond_var->notify_one();
        token->cond_mutex->unlock();
    }
}

void RIL_CellularDevice::check_registration_state()
{
    // network opened in init() so safe to use here
    CellularNetwork::registration_params_t reg_params;
    nsapi_error_t err = _network->get_registration_params(CellularNetwork::C_EREG, reg_params);
    if (err == NSAPI_ERROR_OK) {
        tr_debug("Reg state after VOICE_NETWORK_STATE_CHANGED, _status: %d, access tech: %d, cell id: %d, lac: %d, active time: %d, periodic tau: %d", reg_params._status, reg_params._act, reg_params._cell_id, reg_params._lac, reg_params._active_time, reg_params._periodic_tau);
        if (reg_params._status != CellularNetwork::RegisteredHomeNetwork && reg_params._status != CellularNetwork::RegisteredRoaming) {
            cell_callback_data_t data;
            data.error = NSAPI_ERROR_OK;
            data.status_data = reg_params._status;
            if (_status_cb) {
                _status_cb((nsapi_event_t)CellularRegistrationStatusChanged, (intptr_t)&data);
            }

            RIL_CellularContext *curr = _context_list;
            RIL_CellularContext *next;
            while (curr) {
                curr->cellular_callback((nsapi_event_t)CellularRegistrationStatusChanged, (intptr_t)&data);
                next = (RIL_CellularContext *)curr->_next;
                curr = next;
            }
        }
    } else {
        tr_warning("Reg state after VOICE_NETWORK_STATE_CHANGED request failed with %d", err);
    }
}

void RIL_CellularDevice::unsolicited_response(int response_id, const void *data, size_t data_len)
{
    //RIL_UNSOL_CELL_INFO_LIST if we need to forward cell info changes
    switch (response_id) {
        case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED: // comes usually when device is registers so used here to make registration phase faster
            get_radiostatus(nullptr, true);
            break;
        case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED:
            _queue.call_in(0, this, &RIL_CellularDevice::check_registration_state);
            break;
        default:
            break;
    }

    if (_network) {
        _network->unsolicited_response(response_id, data, data_len);
    }

    if (_information) {
        _information->unsolicited_response(response_id, data, data_len);
    }

    RIL_CellularContext *curr = _context_list;
    RIL_CellularContext *next;
    while (curr) {
        curr->unsolicited_response(response_id, data, data_len);
        next = (RIL_CellularContext *)curr->_next;
        curr = next;
    }
}

nsapi_error_t RIL_CellularDevice::lock_and_send_request(int request_id, void *data, size_t data_len,
                                                        Callback<void(ril_token_t *, RIL_Errno, void *, size_t)> callback)
{
    rtos::Mutex *cond_mutex = new rtos::Mutex;
    rtos::ConditionVariable *cond_var = new rtos::ConditionVariable(*cond_mutex);

    cond_mutex->lock();

    nsapi_error_t ret = NSAPI_ERROR_OK;

    // If token is created, the ownership of cond_mutex and cond_var is moved to token
    ril_token_t *token = _ril.send_request(request_id, data, data_len, callback, cond_mutex, cond_var);
    if (token) {
        bool time_out = token->cond_var->wait_for(RIL_CellularContext::DEVICE_TIMEOUT);
        if (time_out) {
            ret = NSAPI_ERROR_TIMEOUT;
            _ril.cancel_request(token);
            tr_warning("%s timeout for request %s", __FUNCTION__, _ril.get_ril_name(request_id));
        } else {
            ret = token->response_error;
        }
        delete token;
    } else {
        ret = NSAPI_ERROR_DEVICE_ERROR;
    }

    delete cond_var;
    delete cond_mutex;

    return ret;
}

RILAdaptation &RIL_CellularDevice::get_ril()
{
    return _ril;
}

} /* namespace mbed */
