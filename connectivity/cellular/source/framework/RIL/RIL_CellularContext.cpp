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
#include "RIL_CellularContext.h"
#include "RIL_CellularDevice.h"
#include "RIL_CellularNetwork.h"
#include "RILAdaptation.h"
#include "LWIPStack.h"
#include "CellularLog.h"
#include "ScopedLock.h"

#define ril_itoa(v, s, d)    sprintf(s, "%d", v)

const char *local_subnet_mask = "255.255.255.255";

using namespace mbed_cellular_util;
using namespace mbed;

RIL_CellularContext::RIL_CellularContext(RIL_CellularDevice &device, const char *apn, bool cp_req, bool nonip_req) :
    _l3ip_driver(nullptr), _ifname(nullptr), _is_connected(false), _is_running(false), _current_op(OP_INVALID),
    _final_op(OP_CONNECT), _current_state(OP_INVALID), _stack_interface(nullptr),
    _connect_state(CS_INVALID), _set_registration_complete(false), _data_call_addresses(nullptr)
{
    tr_info("New RIL_CellularContext %s (%p)", apn ? apn : "", this);
    _stack = get_stack();
    _authentication_type = CellularContext::NOAUTH;
    _connect_status = NSAPI_STATUS_DISCONNECTED;
    _is_context_active = false;
    _is_context_activated = false;
    _apn = apn;
    _uname = nullptr;
    _pwd = nullptr;
    _status_cb = nullptr;
    _cid = -1;
    _new_context_set = false;
    _next = nullptr;
    memset(_retry_timeout_array, 0, sizeof(_retry_timeout_array));
    _retry_array_length = 0;
    _retry_count = 0;
    _is_blocking = true;
    _device = &device;
    _nw = nullptr;

    if (nonip_req) {
        _pdp_type = NON_IP_PDP_TYPE;
    } else {
        _pdp_type = IPV4V6_PDP_TYPE;
    }

    _mtu = 1500; // Default if modem does not give other
}

RIL_CellularContext::~RIL_CellularContext()
{
    tr_info("Delete CellularContext with apn: [%s] (%p)", _apn ? _apn : "", this);

    (void)disconnect();

    delete [] _ifname;

    if (_nw) {
        _device->close_network();
    }

    delete _data_call_addresses;
    _data_call_addresses = nullptr;
}

nsapi_error_t RIL_CellularContext::set_blocking(bool blocking)
{
    nsapi_error_t err = NSAPI_ERROR_OK;
    tr_info("RIL_CellularContext set blocking %d", blocking);
    _is_blocking = blocking;
    return err;
}

NetworkStack *RIL_CellularContext::get_stack()
{
    if (!_stack) {
        _stack = (NetworkStack *)&LWIP::get_instance();
    }
    return _stack;
}

nsapi_error_t RIL_CellularContext::get_ip_address(SocketAddress *address)
{
    if (_stack_interface) {
        return _stack_interface->get_ip_address(address);
    } else {
        return NSAPI_ERROR_NO_CONNECTION;
    }
}

char *RIL_CellularContext::get_interface_name(char *interface_name)
{
    if (_stack_interface) {
        return _stack_interface->get_interface_name(interface_name);
    } else if (_pdp_type == NON_IP_PDP_TYPE) {
        sprintf(interface_name, "ni%d", _cid);
        return interface_name;
    } else {
        return nullptr;
    }
}

void RIL_CellularContext::attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    _status_cb = status_cb;
}

nsapi_error_t RIL_CellularContext::connect()
{
    tr_info("CellularContext connect");
    ScopedLock<rtos::Mutex> lock(_api_mutex);

    if (_is_connected) {
        return NSAPI_ERROR_IS_CONNECTED;
    }

    if (_is_running) {
        return NSAPI_ERROR_BUSY;
    }

    _is_running = true;
    _final_op = OP_CONNECT;
    nsapi_error_t err = _device->set_sim_ready();
    _cb_data.error = check_operation(err, OP_SIM_READY);

    if (_is_blocking) {
        if (_cb_data.error == NSAPI_ERROR_OK || _cb_data.error == NSAPI_ERROR_ALREADY) {
            do_connect_with_retry();
        }
        _is_running = false;
    } else {
        if (_cb_data.error == NSAPI_ERROR_ALREADY) {
            // device is already attach state.
            int id = _device->get_queue()->call_in(0, this, &RIL_CellularContext::do_connect_with_retry);
            if (id == 0) {
                _is_running = false;
                _final_op = OP_INVALID;
                _current_op = OP_INVALID;
                return NSAPI_ERROR_NO_MEMORY;
            }
            return NSAPI_ERROR_OK;
        }
    }

    return _cb_data.error;
}

void RIL_CellularContext::do_connect_with_retry()
{
    CellularContext::do_connect_with_retry();
}

nsapi_error_t RIL_CellularContext::do_initial_attach()
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);

    const char *plmn = _device->get_plmn();
    tr_debug("RIL_CellularContext::do_initial_attach(), plmn: %s", (plmn ? plmn : "NULL"));
    _set_registration_complete = false;
    nsapi_error_t ret = NSAPI_ERROR_OK;

    rtos::Mutex *cond_mutex = new rtos::Mutex;
    rtos::ConditionVariable *cond_var = new rtos::ConditionVariable(*cond_mutex);

    cond_mutex->lock();

    RILAdaptation &ril = static_cast<RIL_CellularDevice *>(_device)->get_ril();

    ril_token_t *token1;
    if (plmn) {
        token1 = ril.send_request(RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL,
                                  (void *)plmn, strlen(plmn),
                                  callback(this, &RIL_CellularContext::request_set_initial_attach_apn_response),
                                  cond_mutex, cond_var);
    } else {
        token1 = ril.send_request(RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC,
                                  nullptr, 0,
                                  callback(this, &RIL_CellularContext::request_set_initial_attach_apn_response),
                                  cond_mutex, cond_var);
    }

    if (token1 == nullptr) {
        delete cond_var;
        delete cond_mutex;
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    // don't wait response to RIL_REQUEST_SET_NETWORK_SELECTION_XXX here as it comes after we got response to RIL_REQUEST_SET_INITIAL_ATTACH_APN
    // and device is actually registered.
    attach_req_t *data = new attach_req_t();

    data->pdp_type = pdp_type_to_string(_pdp_type);
    ril_itoa(_authentication_type, data->auth, 10);
    data->data[0] = _apn;
    data->data[1] = data->pdp_type;
    data->data[2] = data->auth;
    data->data[3] = _uname;
    data->data[4] = _pwd;
    data->data[5] = nullptr;

    tr_debug("Initial attach, apn: %s", data->data[0]);
    tr_debug("Initial attach, pdp_type: %s", data->data[1]);
    tr_debug("Initial attach, authentication type: %s", data-> data[2]);


    ril_token_t *token2 = ril.send_request(RIL_REQUEST_SET_INITIAL_ATTACH_APN,
                                           (void *)data, sizeof(char *) * 6,
                                           callback(this, &RIL_CellularContext::request_set_initial_attach_apn_response),
                                           cond_mutex, cond_var);

    if (token2) {
        bool time_out = cond_var->wait_for(RIL_CellularContext::DEVICE_TIMEOUT);
        if (time_out) {
            ret = NSAPI_ERROR_TIMEOUT;
            ril.cancel_request(token1);
            ril.cancel_request(token2);
            tr_warning("do_initial_attach timeout");
        }
    }

    delete data;

    if (ret == NSAPI_ERROR_OK) {
        ret = token1->response_error;
    }
    if (ret == NSAPI_ERROR_OK) {
        ret = token2->response_error;
    }

    delete token2;
    delete token1;

    delete cond_var;
    delete cond_mutex;

    return ret;
}

void RIL_CellularContext::request_set_initial_attach_apn_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();

        token->response_error = (err == RIL_E_SUCCESS) ? NSAPI_ERROR_OK : NSAPI_ERROR_DEVICE_ERROR;

        if (_set_registration_complete) {
            token->cond_var->notify_one();
        } else {
            _set_registration_complete = true;
        }

        token->cond_mutex->unlock();
    }
}

void RIL_CellularContext::get_signal_quality()
{
    _cb_data.error = _nw->get_signal_quality(_signal_quality.rssi, &_signal_quality.ber);
    _cb_data.data = &_signal_quality;

    if (_cb_data.error == NSAPI_ERROR_OK) {
        _device->cellular_callback((nsapi_event_t)CellularSignalQuality, (intptr_t)&_cb_data, this);
        if (_signal_quality.rssi == CellularNetwork::SignalQualityUnknown) {
            tr_info("RSSI unknown");
        } else {
            tr_info("RSSI %d dBm", _signal_quality.rssi);
        }
    }
}

void RIL_CellularContext::do_connect()
{
    nsapi_error_t ret = NSAPI_ERROR_OK;

    if (!_nw) {
        _nw = _device->open_network();
    }

    if (_connect_state < CS_INITIAL_ATTACH) {
        get_signal_quality(); // mimic state machine so AT and RIL api's look same to application. Same below.
        ret = do_initial_attach();
        if (ret != NSAPI_ERROR_OK) {
            _cb_data.error = ret;
            if (_retry_count == _retry_array_length - 1) {
                _is_running = false;
            }
            return;
        }
        _cb_data.error = NSAPI_ERROR_OK;
        _cb_data.status_data = CellularNetwork::RegisteredHomeNetwork;
        _connect_state = CS_INITIAL_ATTACH;
        _retry_count = 0; // kind of new state so start retry logic
        _device->cellular_callback((nsapi_event_t)CellularRegistrationStatusChanged, (intptr_t)&_cb_data, this);
        if (_final_op == OP_REGISTER) {
            _is_running = false;
            return;
        }
    }

    if (_connect_state < CS_ATTACH) {
        get_signal_quality();
        _connect_state = CS_ATTACH;
        _cb_data.status_data = CellularNetwork::Attached;
        _cb_data.error = NSAPI_ERROR_OK;
        _device->cellular_callback((nsapi_event_t)CellularAttachNetwork, (intptr_t)&_cb_data, this);
        if (_final_op == OP_ATTACH) {
            _is_running = false;
            return;
        }
    }

    if (_connect_state < CS_SETUP_DATA_CALL) {
        get_signal_quality();
        call_network_cb(NSAPI_STATUS_CONNECTING);

        connect_req_t *data = new connect_req_t();
        RIL_RadioTechnology rat = RADIO_TECH_LTE;
        rat = ((RIL_CellularNetwork *)_nw)->get_RIL_rat();
        int add_val = rat > 2 ? 2 : 0;
        ril_itoa(rat + add_val, data->rat, 10); // Radio technology to use: 0-CDMA, 1-GSM/UMTS, 2... for values above 2 this is RIL_RadioTechnology + 2.
        ril_itoa(RIL_DATA_PROFILE_DEFAULT, data->data_profile, 10);
        data->apn = _apn;
        data->uname = _uname;
        data->pwd = _pwd;
        ril_itoa(_authentication_type, data->auth, 10);
        data->pdp_type = pdp_type_to_string(_pdp_type);
        data->opt = nullptr;

        data->data[0] = data->rat;
        data->data[1] = data->data_profile;
        data->data[2] = data->apn;
        data->data[3] = data->uname;
        data->data[4] = data->pwd;
        data->data[5] = data->auth;
        data->data[6] = data->pdp_type;
        data->data[7] = data->opt;

        tr_debug("Setup datacall, rat: %s", data->rat);
        tr_debug("Setup datacall, data profile: %s", data->data_profile);
        tr_debug("Setup datacall, apn: %s", data->apn);
        tr_debug("Setup datacall, authentication: %s", data->auth);
        tr_debug("Setup datacall, pdp type: %s", data->pdp_type);

        ret = static_cast<RIL_CellularDevice *>(_device)->lock_and_send_request(RIL_REQUEST_SETUP_DATA_CALL, (void *)data, sizeof(char *) * 8,
                                                                                callback(this, &RIL_CellularContext::request_setup_data_call_response));
        delete data;
        _cb_data.error = ret;

        if (_cb_data.error) {
            if (_retry_count == _retry_array_length - 1) {
                _is_running = false;
            }
            return;
        }
        _cb_data.error = NSAPI_ERROR_OK;
        _connect_state = CS_SETUP_DATA_CALL;
        _retry_count = 0; // kind of new state so start retry logic

        if (ret == NSAPI_ERROR_OK) {
            create_interface_and_connect();
        }
    }
}

L3IP *RIL_CellularContext::get_L3IP_driver()
{
    return nullptr;
}

void RIL_CellularContext::delete_L3IP_driver()
{
}

nsapi_error_t RIL_CellularContext::add_dns_server_to_stack(const char *dns_server_address)
{
    nsapi_error_t ret = NSAPI_ERROR_OK;
    if (strlen(dns_server_address)) {
        SocketAddress dns(dns_server_address);
        char ifn[4]; // 2 characters, number and a null mark
        ret = add_dns_server(dns, get_interface_name(ifn));
        tr_debug("add_dns_server(%s) returned %d", dns_server_address, ret);
    }
    return ret;
}

void RIL_CellularContext::create_interface_and_connect()
{
    if (_pdp_type != NON_IP_PDP_TYPE) {
        MBED_ASSERT(_data_call_addresses);

        if (!_l3ip_driver) {
            _l3ip_driver = get_L3IP_driver();
        }

        if (_connect_state < CS_INTERFACE_ADDED) {
            _cb_data.error = static_cast<OnboardNetworkStack *>(_stack)->add_l3ip_interface(*_l3ip_driver, true, &_stack_interface);
            tr_debug("RIL_CellularContext::create_interface_and_connect: add_l3ip_interface = %d", _cb_data.error);
            if (_cb_data.error) {
                if (_retry_count == _retry_array_length - 1) {
                    _is_running = false;
                }
#if MBED_CONF_MBED_TRACE_ENABLE
                if (_cb_data.error == NSAPI_ERROR_UNSUPPORTED) {
                    tr_warning("Please set l3ip feature in mbed_app.json: \"lwip.l3ip-enabled\": true");
                }
#endif // MBED_CONF_MBED_TRACE_ENABLE
                return;
            }

            _stack_interface->attach(callback(this, &RIL_CellularContext::stack_status_cb));
            _connect_state = CS_INTERFACE_ADDED;
            _retry_count = 0; // kind of new state so start retry logic
        }

        const char *bringup_ip = _data_call_addresses->_local_addr_ipv4;
        const char *bringup_gw = _data_call_addresses->_gateway_addr_ipv4;

        if (strlen(_data_call_addresses->_local_addr_ipv4) && strlen(_data_call_addresses->_local_addr_ipv6)) {
            _stack_interface->set_ip_address(_data_call_addresses->_local_addr_ipv4,
                                             local_subnet_mask,
                                             _data_call_addresses->_gateway_addr_ipv4,
                                             IPV4_STACK);
        }

        if ((strlen(_data_call_addresses->_local_addr_ipv4) && strlen(_data_call_addresses->_local_addr_ipv6)) ||
                !strlen(_data_call_addresses->_local_addr_ipv4)) {
            bringup_ip = _data_call_addresses->_local_addr_ipv6;
            bringup_gw = _data_call_addresses->_gateway_addr_ipv6;
        }

        _cb_data.error = _stack_interface->bringup(false,
                                                   bringup_ip,
                                                   local_subnet_mask,
                                                   bringup_gw,
                                                   pdp_type_to_ip_stack(_pdp_type),
                                                   _is_blocking);

        // Take DNS servers into use if provided by the modem. Secondary must be added first
        // as this appends server to the beginning of the DNS servers list
        add_dns_server_to_stack(_data_call_addresses->_secondary_dns_addr_ipv4);
        add_dns_server_to_stack(_data_call_addresses->_primary_dns_addr_ipv4);
        add_dns_server_to_stack(_data_call_addresses->_secondary_dns_addr_ipv6);
        add_dns_server_to_stack(_data_call_addresses->_primary_dns_addr_ipv6);
    }

    if (_cb_data.error == NSAPI_ERROR_OK) {
        delete _data_call_addresses;
        _data_call_addresses = nullptr;

        _connect_state = CS_INVALID;
        _current_state = OP_CONNECT;
        _is_connected = true;
    }
    _is_running = false;
    tr_debug("RIL_CellularContext::create_interface_and_connect: bringup = %d, pdp_type = %s",
             _cb_data.error, pdp_type_to_string(_pdp_type));
}

nsapi_error_t RIL_CellularContext::close_stack_interface()
{
    nsapi_error_t ret = NSAPI_ERROR_NO_CONNECTION;
    if (_stack_interface) {
        ret = _stack_interface->bringdown();

        if (ret == NSAPI_ERROR_OK) {
            ret = static_cast<OnboardNetworkStack *>(_stack)->remove_l3ip_interface(&_stack_interface);

            if (ret == NSAPI_ERROR_OK) {
                _stack_interface = nullptr;

                delete_L3IP_driver();
                _l3ip_driver = nullptr;
            }
        }
    }
    return ret;
}

nsapi_error_t RIL_CellularContext::disconnect(bool request_deactivate_data_call)
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);

    if (_is_running) {
        return NSAPI_ERROR_BUSY;
    }

    if (!_is_connected) {
        return NSAPI_ERROR_NO_CONNECTION;
    }

    _is_running = true;
    nsapi_error_t ret = NSAPI_ERROR_OK;

    if (_pdp_type != NON_IP_PDP_TYPE) {
        ret = close_stack_interface();
    }

    if (ret == NSAPI_ERROR_OK && request_deactivate_data_call) {
        disconnect_ret_t *data = new disconnect_ret_t();
        ril_itoa(_cid, data->cid, 10);
        ril_itoa(0, data->reason, 10);

        data->data[0] = data->cid;
        data->data[1] = data->reason;

        tr_debug("Deactivate data call: cid = %s, reason = %s", data->cid, data->reason);
        ret = static_cast<RIL_CellularDevice *>(_device)->lock_and_send_request(RIL_REQUEST_DEACTIVATE_DATA_CALL, (void *)data, sizeof(char *) * 2,
                                                                                callback(this, &RIL_CellularContext::request_deactivate_data_call_response));
        delete data;
    }

    _connect_state = CS_INVALID;
    _current_state = OP_ATTACH; // we are still attached, we just deactivated the call or should we set as invalid?
    _final_op = OP_INVALID;
    _current_op = OP_INVALID;
    _is_running = false;
    _is_connected = false;
    _cid = -1;

    tr_info("Disconnect %s", ret ? "failed" : "success");

    return ret;
}

nsapi_error_t RIL_CellularContext::disconnect()
{
    return disconnect(true);
}

nsapi_error_t RIL_CellularContext::check_operation(nsapi_error_t err, ContextOperation op)
{
    _current_op = op;
    if (err == NSAPI_ERROR_IN_PROGRESS || err == NSAPI_ERROR_OK) {
        if (_is_blocking) {
            if (!_semaphore.try_acquire_for(get_timeout_for_operation(op))) { // cellular network searching may take several minutes
                tr_error("Operation timeout, no cellular connection");
                return NSAPI_ERROR_TIMEOUT;
            }
            return NSAPI_ERROR_OK;
        }
    }

    return err;
}

uint32_t RIL_CellularContext::get_timeout_for_operation(ContextOperation op) const
{
    uint32_t timeout = NETWORK_TIMEOUT; // default timeout is 30 minutes as registration and attach may take time
    if (op == OP_SIM_READY || op == OP_DEVICE_READY) {
        timeout = DEVICE_TIMEOUT; // use 5 minutes for device ready and sim
    }
    return timeout;
}

bool RIL_CellularContext::is_connected()
{
    return _is_connected;
}

void RIL_CellularContext::set_plmn(const char *plmn)
{
    _device->set_plmn(plmn);
}

void RIL_CellularContext::set_sim_pin(const char *sim_pin)
{
    _device->set_sim_pin(sim_pin);
}

nsapi_error_t RIL_CellularContext::connect(const char *sim_pin, const char *apn, const char *uname, const char *pwd)
{
    if (_is_running) {
        return NSAPI_ERROR_BUSY;
    }
    set_sim_pin(sim_pin);
    set_credentials(apn, uname, pwd);
    return connect();
}

void RIL_CellularContext::set_credentials(const char *apn, const char *uname, const char *pwd)
{
    _apn = apn;
    _uname = uname;
    _pwd = pwd;
}

nsapi_error_t RIL_CellularContext::get_netmask(SocketAddress *address)
{
    if (_stack_interface) {
        return _stack_interface->get_netmask(address);
    }
    return NSAPI_ERROR_NO_CONNECTION;
}

nsapi_error_t RIL_CellularContext::get_gateway(SocketAddress *address)
{
    if (_stack_interface) {
        return _stack_interface->get_gateway(address);
    }
    return NSAPI_ERROR_NO_CONNECTION;
}

nsapi_error_t RIL_CellularContext::get_pdpcontext_params(pdpContextList_t &params_list)
{
    // This will return only the current context params.
    pdpcontext_params_t *params = nullptr;
    params = params_list.add_new();

    if (_apn) {
        strncpy(params->apn, _apn, sizeof(params->apn));
    } else {
        strncpy(params->apn, "", sizeof(params->apn));
    }

    SocketAddress addr = 0;

    if (get_ip_address(&addr) == NSAPI_ERROR_OK) {
        strncpy(params->local_addr, addr.get_ip_address(), sizeof(params->local_addr));
    } else {
        strncpy(params->local_addr, "", sizeof(params->local_addr));
    }

    addr = 0;
    if (get_netmask(&addr) == NSAPI_ERROR_OK) {
        strncpy(params->local_subnet_mask, addr.get_ip_address(), sizeof(params->local_subnet_mask));
    } else {
        strncpy(params->local_subnet_mask, "", sizeof(params->local_subnet_mask));
    }

    addr = 0;
    if (get_gateway(&addr) == NSAPI_ERROR_OK) {
        strncpy(params->gateway_addr, addr.get_ip_address(), sizeof(params->gateway_addr));
    } else {
        strncpy(params->gateway_addr, "", sizeof(params->gateway_addr));
    }

    char ifn[4];
    SocketAddress dns1;
    SocketAddress dns2;
    get_dns_server(0, &dns1, get_interface_name(ifn));
    get_dns_server(1, &dns2, get_interface_name(ifn));
    if (dns1) {
        strncpy(params->dns_primary_addr, dns1.get_ip_address(), sizeof(params->dns_primary_addr));
    }
    if (dns2) {
        strncpy(params->dns_secondary_addr, dns2.get_ip_address(), sizeof(params->dns_secondary_addr));
    }

    params->p_cscf_prim_addr[0] = '\0';
    params->p_cscf_sec_addr[0] = '\0';
    params->cid = _cid;
    params->bearer_id = -1;
    params->im_signalling_flag = -1;
    params->lipa_indication = -1;
    params->ipv4_mtu = _mtu;
    params->wlan_offload = -1;
    params->local_addr_ind = -1;
    params->non_ip_mtu = _mtu;
    params->serving_plmn_rate_control_value = -1;

    return NSAPI_ERROR_OK;
}

nsapi_error_t RIL_CellularContext::get_rate_control(CellularContext::RateControlExceptionReports &reports,
                                                    CellularContext::RateControlUplinkTimeUnit &time_unit, int &uplink_rate)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularContext::get_apn_backoff_timer(int &backoff_timer)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularContext::set_device_ready()
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);

    if (_is_running) {
        return NSAPI_ERROR_BUSY;
    } else if (_current_state >= OP_DEVICE_READY) {
        return NSAPI_ERROR_ALREADY;
    }

    _is_running = true;
    _final_op = OP_DEVICE_READY;
    nsapi_error_t err = _device->set_device_ready();
    err =  check_operation(err, OP_DEVICE_READY);
    if (_is_blocking) {
        _is_running = false;
    }
    return err;
}

nsapi_error_t RIL_CellularContext::set_sim_ready()
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);

    if (_is_running) {
        return NSAPI_ERROR_BUSY;
    } else if (_current_state >= OP_SIM_READY) {
        return NSAPI_ERROR_ALREADY;
    }

    _is_running = true;
    _final_op = OP_SIM_READY;
    nsapi_error_t err = _device->set_sim_ready();
    err = check_operation(err, OP_SIM_READY);
    if (_is_blocking) {
        _is_running = false;
    }

    return err;
}

nsapi_error_t RIL_CellularContext::register_to_network()
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);

    if (_is_running) {
        return NSAPI_ERROR_BUSY;
    } else if (_current_state >= OP_REGISTER) {
        return NSAPI_ERROR_ALREADY;
    }

    _is_running = true;
    _final_op = OP_REGISTER;
    nsapi_error_t err = _device->set_sim_ready();
    err = check_operation(err, OP_SIM_READY);

    if (_is_blocking) {
        if (err == NSAPI_ERROR_ALREADY || err == NSAPI_ERROR_OK) {
            // device is already sim state. Continue to register state.
            RIL_CellularContext::do_connect_with_retry();
            err = _cb_data.error;
        }
        _is_running = false;
        _final_op = OP_INVALID;
        _current_op = OP_INVALID;
    } else  if (err == NSAPI_ERROR_ALREADY) {
        // device is already sim state. Continue to register state.
        int id = _device->get_queue()->call_in(0, this, &RIL_CellularContext::do_connect_with_retry);
        if (id == 0) {
            _is_running = false;
            _final_op = OP_INVALID;
            _current_op = OP_INVALID;
            return NSAPI_ERROR_NO_MEMORY;
        }
        return NSAPI_ERROR_OK;
    }
    return err;
}

nsapi_error_t RIL_CellularContext::attach_to_network()
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);

    if (_is_running) {
        return NSAPI_ERROR_BUSY;
    } else if (_current_state >= OP_ATTACH) {
        return NSAPI_ERROR_ALREADY;
    }

    _is_running = true;
    _final_op = OP_ATTACH;
    nsapi_error_t err = _device->set_sim_ready();
    err = check_operation(err, OP_SIM_READY);

    if (_is_blocking) {
        if (err == NSAPI_ERROR_ALREADY || err == NSAPI_ERROR_OK) {
            // device is already sim state. Continue to attach state.
            RIL_CellularContext::do_connect_with_retry();
            err = _cb_data.error;
        }
        _is_running = false;
        _final_op = OP_INVALID;
        _current_op = OP_INVALID;
    } else if (err == NSAPI_ERROR_ALREADY) {
        // device is already sim state. Continue to attach state.
        int id = _device->get_queue()->call_in(0, this, &RIL_CellularContext::do_connect_with_retry);
        if (id == 0) {
            _is_running = false;
            _final_op = OP_INVALID;
            _current_op = OP_INVALID;
            return NSAPI_ERROR_NO_MEMORY;
        }
        return NSAPI_ERROR_OK;
    }

    return err;
}

#if (DEVICE_SERIAL && DEVICE_INTERRUPTIN)
nsapi_error_t RIL_CellularContext::configure_hup(PinName /*dcd_pin*/, bool /*active_high*/)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
#endif

ControlPlane_netif *RIL_CellularContext::get_cp_netif()
{
    return nullptr;
}

void RIL_CellularContext::set_state(cell_callback_data_t *data, cellular_connection_status_t st)
{
    if (data->error == NSAPI_ERROR_OK) {
        if (st == CellularDeviceReady && _current_state < OP_DEVICE_READY) {
            _current_state = OP_DEVICE_READY;
        } else if (st == CellularSIMStatusChanged && data->status_data == CellularDevice::SimStateReady  &&
                   _current_state < OP_SIM_READY) {
            _current_state = OP_SIM_READY;
        } else if (st == CellularRegistrationStatusChanged && _current_state < OP_REGISTER &&
                   (data->status_data == CellularNetwork::RegisteredHomeNetwork ||
                    data->status_data == CellularNetwork::RegisteredRoaming ||
                    data->status_data == CellularNetwork::AlreadyRegistered)) {
            _current_state = OP_REGISTER;
        } else if (st == CellularAttachNetwork && data->status_data == CellularNetwork::Attached &&
                   _current_state < OP_ATTACH) {
            _current_state = OP_ATTACH;
        }
    }

    // set _is_running flag to false if async and op was finished
    if (!_is_blocking) {
        if (data->error == NSAPI_ERROR_OK) {
            if (st == CellularDeviceReady && _final_op == OP_DEVICE_READY) {
                _is_running = false;
            } else if (st == CellularSIMStatusChanged && data->status_data == CellularDevice::SimStateReady &&
                       _final_op == OP_SIM_READY) {
                _is_running = false;
            } else if (st == CellularRegistrationStatusChanged &&
                       (data->status_data == CellularNetwork::RegisteredHomeNetwork ||
                        data->status_data == CellularNetwork::RegisteredRoaming ||
                        data->status_data == CellularNetwork::AlreadyRegistered) && _final_op == OP_REGISTER) {
                _is_running = false;
            } else if (st == CellularAttachNetwork && data->status_data == CellularNetwork::Attached &&
                       _final_op == OP_ATTACH) {
                _is_running = false;
            }
        } else {
            _is_running = false;
        }
    }
}

void RIL_CellularContext::cellular_callback(nsapi_event_t ev, intptr_t ptr)
{
    if (ev >= NSAPI_EVENT_CELLULAR_STATUS_BASE && ev <= NSAPI_EVENT_CELLULAR_STATUS_END) {
        cell_callback_data_t *data = (cell_callback_data_t *)ptr;
        cellular_connection_status_t st = (cellular_connection_status_t)ev;
        _cb_data.error = data->error;
        tr_debug("CellularContext: event %d, err %d, data %d", ev, data->error, data->status_data);
        set_state(data, st);

        if (_is_blocking) {
            if (data->error != NSAPI_ERROR_OK) {
                // operation failed, release semaphore
                if (_current_op != OP_INVALID) {
                    _current_op = OP_INVALID;
                    _semaphore.release();
                }
            } else {
                if ((st == CellularDeviceReady && _current_op == OP_DEVICE_READY) ||
                        (st == CellularSIMStatusChanged && _current_op == OP_SIM_READY &&
                         data->status_data == CellularDevice::SimStateReady)) {
                    // target reached, release semaphore
                    _current_op = OP_INVALID;
                    _semaphore.release();
                } else if (st == CellularRegistrationStatusChanged && (data->status_data == CellularNetwork::RegisteredHomeNetwork ||
                                                                       data->status_data == CellularNetwork::RegisteredRoaming || data->status_data == CellularNetwork::AlreadyRegistered) && _current_op == OP_REGISTER) {
                    // target reached, release semaphore
                    _current_op = OP_INVALID;
                    _semaphore.release();
                } else if (st == CellularAttachNetwork && (_current_op == OP_ATTACH || _current_op == OP_CONNECT) &&
                           data->status_data == CellularNetwork::Attached) {
                    // target reached, release semaphore
                    _current_op = OP_INVALID;
                    _semaphore.release();
                }
            }
        } else {
            if (_current_op != OP_INVALID && st == CellularSIMStatusChanged && data->error == NSAPI_ERROR_OK &&
                    data->status_data == CellularDevice::SimStateReady && _final_op > OP_SIM_READY &&
                    _current_state < _final_op) {
                // forward to application
                if (_status_cb) {
                    _status_cb(ev, ptr);
                }
                // continue to register/attach/connect
                int id = _device->get_queue()->call_in(0, this, &RIL_CellularContext::do_connect_with_retry);
                if (id == 0) {
                    if (_status_cb) {
                        _cb_data.error = NSAPI_ERROR_NO_MEMORY;
                        _cb_data.final_try = true;
                        _cb_data.status_data = CellularNetwork::StatusNotAvailable;
                        _status_cb((nsapi_event_t)CellularRegistrationStatusChanged, (intptr_t)&_cb_data);
                    }
                }
                return;
            }
        }
    } else {
        tr_debug("CellularContext: event %d, ptr %d", ev, ptr);
    }

    // forward to application
    if (ev == NSAPI_EVENT_CONNECTION_STATUS_CHANGE) {
        // Route connection status events via call_network_cb as it filters duplicate events
        call_network_cb((nsapi_connection_status)ptr);
    } else if (_status_cb) {
        _status_cb(ev, ptr);
    }
}

const char *RIL_CellularContext::get_nonip_context_type_str()
{
    return nullptr;
}

void RIL_CellularContext::request_setup_data_call_response(ril_token_t *token, RIL_Errno err, void *response,
                                                           size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();
        if (err == RIL_E_SUCCESS && response && response_len) {
            RIL_Data_Call_Response_v11 *v11;
            // only one response in RIL_REQUEST_SETUP_DATA_CALL
            v11 = (RIL_Data_Call_Response_v11 *)response;
            _cb_data.error = NSAPI_ERROR_OK;
            if (v11->status == PDP_FAIL_NONE && v11->active == 2) {
                _cid = v11->cid;

                _pdp_type = string_to_pdp_type(v11->type);
                tr_debug("RIL_CellularContext::request_setup_data_call_response, cid: %d, pdp_type: %s", _cid, v11->type);

                if (v11->ifname) {
                    const uint32_t len = strlen(v11->ifname) + 1;
                    delete [] _ifname;
                    _ifname = new char[len];
                    memcpy(_ifname, v11->ifname, len);
                    tr_debug("RIL_CellularContext::request_setup_data_call_response, _ifname: %s", _ifname);
                } else {
                    _cb_data.error = NSAPI_ERROR_DEVICE_ERROR;
                }

                // Skip address fields in case of NonIP context
                if (_pdp_type != NON_IP_PDP_TYPE && !_cb_data.error) {
                    _cb_data.error = handle_data_call_setup_addresses(v11);
                } else {
                    tr_debug("Skipping address fields as not needed for NonIP context");
                }

                if (v11->mtu && !_cb_data.error) {
                    _mtu = v11->mtu;
                    tr_debug("RIL_CellularContext::request_setup_data_call_response, _mtu: %d", _mtu);
                }
            } else {
                tr_error("RIL_CellularContext::request_setup_data_call_response fail: status = %d, active = %d",
                         v11->status, v11->active);
                _cb_data.error = NSAPI_ERROR_DEVICE_ERROR;
            }
        } else {
            tr_error("RIL_CellularContext::request_setup_data_call_response fail: err = %d, response = %p, response_len = %d",
                     err, response, response_len);
            _cb_data.error = NSAPI_ERROR_DEVICE_ERROR;
        }

        token->response_error = _cb_data.error;
        token->cond_var->notify_one();
        token->cond_mutex->unlock();
    }
}

nsapi_error_t RIL_CellularContext::handle_data_call_setup_addresses(const RIL_Data_Call_Response_v11 *response)
{
    char temp[64];

    delete _data_call_addresses;
    _data_call_addresses = new data_call_addr_data_t;

    if (response->addresses) {
        mbed_cellular_util::separate_space_delimited_addresses(true, response->addresses,
                                                               _data_call_addresses->_local_addr_ipv6,
                                                               sizeof(_data_call_addresses->_local_addr_ipv6),
                                                               temp, sizeof(temp));
        mbed_cellular_util::separate_space_delimited_addresses(false, response->addresses,
                                                               _data_call_addresses->_local_addr_ipv4,
                                                               sizeof(_data_call_addresses->_local_addr_ipv4),
                                                               temp, sizeof(temp));

        tr_debug("request_setup_data_call_response, addresses: %s, IPv4: %s, IPv6: %s",
                 response->addresses, _data_call_addresses->_local_addr_ipv4, _data_call_addresses->_local_addr_ipv6);
    } else {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    if (response->dnses) {
        mbed_cellular_util::separate_space_delimited_addresses(true, response->dnses,
                                                               _data_call_addresses->_primary_dns_addr_ipv6,
                                                               sizeof(_data_call_addresses->_primary_dns_addr_ipv6),
                                                               _data_call_addresses->_secondary_dns_addr_ipv6,
                                                               sizeof(_data_call_addresses->_secondary_dns_addr_ipv6));

        mbed_cellular_util::separate_space_delimited_addresses(false, response->dnses,
                                                               _data_call_addresses->_primary_dns_addr_ipv4,
                                                               sizeof(_data_call_addresses->_primary_dns_addr_ipv4),
                                                               _data_call_addresses->_secondary_dns_addr_ipv4,
                                                               sizeof(_data_call_addresses->_secondary_dns_addr_ipv4));

        tr_debug("request_setup_data_call_response, DNSes: %s", response->dnses);
        tr_debug("request_setup_data_call_response, DNSes IPv4: %s, %s",
                 _data_call_addresses->_primary_dns_addr_ipv4, _data_call_addresses->_secondary_dns_addr_ipv4);
        tr_debug("request_setup_data_call_response, DNSes IPv6: %s, %s",
                 _data_call_addresses->_primary_dns_addr_ipv6, _data_call_addresses->_secondary_dns_addr_ipv6);
    }

    if (response->gateways) {
        mbed_cellular_util::separate_space_delimited_addresses(true, response->gateways,
                                                               _data_call_addresses->_gateway_addr_ipv6,
                                                               sizeof(_data_call_addresses->_gateway_addr_ipv6),
                                                               temp, sizeof(temp));
        mbed_cellular_util::separate_space_delimited_addresses(false, response->gateways,
                                                               _data_call_addresses->_gateway_addr_ipv4,
                                                               sizeof(_data_call_addresses->_gateway_addr_ipv4),
                                                               temp, sizeof(temp));

        tr_debug("request_setup_data_call_response, gateways: %s, IPv4: %s, IPv6: %s",
                 response->gateways, _data_call_addresses->_gateway_addr_ipv4, _data_call_addresses->_gateway_addr_ipv6);
    } else {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    return NSAPI_ERROR_OK;
}

RIL_CellularDevice::CellularProperty RIL_CellularContext::pdp_type_t_to_cellular_property(pdp_type_t pdp_type)
{
    RIL_CellularDevice::CellularProperty prop = RIL_CellularDevice::PROPERTY_IPV4_PDP_TYPE;
    if (pdp_type == IPV6_PDP_TYPE) {
        prop = RIL_CellularDevice::PROPERTY_IPV6_PDP_TYPE;
    } else if (pdp_type == IPV4V6_PDP_TYPE) {
        prop = RIL_CellularDevice::PROPERTY_IPV4V6_PDP_TYPE;
    } else if (pdp_type == NON_IP_PDP_TYPE) {
        prop = RIL_CellularDevice::PROPERTY_NON_IP_PDP_TYPE;
    }

    return prop;
}

nsapi_ip_stack_t RIL_CellularContext::pdp_type_to_ip_stack(pdp_type_t pdp_type)
{
    if (pdp_type >= NON_IP_PDP_TYPE) {
        MBED_ASSERT(false);
    }
    return (nsapi_ip_stack_t)pdp_type;
}

void RIL_CellularContext::call_network_cb(nsapi_connection_status_t status)
{
    if (_connect_status != status) {
        _connect_status = status;
        if (_status_cb) {
            _status_cb(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, _connect_status);
        }
        if (_nw && _connect_status == NSAPI_STATUS_DISCONNECTED) {
            tr_info("CellularContext disconnected with cid: %d and apn: %s", _cid, _apn);
            disconnect(false); // Clean up network interface objects if network dropped
        }
    }
}

void RIL_CellularContext::stack_status_cb(nsapi_event_t ev, intptr_t ptr)
{
    tr_debug("RIL_CellularContext::stack_status_cb: event %d, ptr %d", ev, ptr);

    if (ev == NSAPI_EVENT_CONNECTION_STATUS_CHANGE && ptr == NSAPI_STATUS_GLOBAL_UP) {
        _is_connected = true;
    } else {
        _is_connected = false;
    }

    // call device's callback, it will broadcast this to here (cellular_callback)
    _device->cellular_callback(ev, ptr, this);
}

nsapi_connection_status_t RIL_CellularContext::get_connection_status() const
{
    if (_stack_interface) {
        return _stack_interface->get_connection_status();
    } else {
        return _connect_status;
    }
}

void RIL_CellularContext::request_deactivate_data_call_response(ril_token_t *token, RIL_Errno err, void *response,
                                                                size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();
        token->response_error = (err == RIL_E_SUCCESS) ? NSAPI_ERROR_OK : NSAPI_ERROR_DEVICE_ERROR;
        token->cond_var->notify_one();
        token->cond_mutex->unlock();
    }
}

void RIL_CellularContext::unsolicited_response(int response_id, const void *data, size_t data_len)
{
    tr_debug("RIL_CellularContext::unsolicited_response: id = %d", response_id);

    if (response_id == RIL_UNSOL_DATA_CALL_LIST_CHANGED) {
        const int items = data_len / sizeof(RIL_Data_Call_Response_v11);
        MBED_ASSERT(data_len == items * sizeof(RIL_Data_Call_Response_v11));
        if (items) {
            MBED_ASSERT(data);
            const RIL_Data_Call_Response_v11 *resp_data = (const RIL_Data_Call_Response_v11 *)data;
            for (int i = 0; i < items; i++) {
                const RIL_Data_Call_Response_v11 &resp_item = resp_data[i];
                if (resp_item.cid == _cid) { // this context cid, continue processing
                    if (resp_item.active == 0 && resp_item.status != PDP_FAIL_NONE && resp_item.suggestedRetryTime == -1) {
                        // This context was active, clean up the network interface and send NSAPI_STATUS_DISCONNECTED to application
                        call_network_cb(NSAPI_STATUS_DISCONNECTED);
                    }
                    return;
                }
            }
        }
    }
}
