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
#include "RIL_CellularNetwork.h"
#include "CellularUtil.h"
#include "CellularLog.h"
#include "CellularCommon.h"
#include "RIL_CellularContext.h"
#include "RIL_CellularDevice.h"
#include "mbed_wait_api.h"
#include "ScopedLock.h"

using namespace mbed_cellular_util;
using namespace mbed;

#define TWO_BYTES_HEX 4
#define FOUR_BYTES_HEX 8

RIL_CellularNetwork::RIL_CellularNetwork(RIL_CellularDevice &device)
    : _rat(RAT_UNKNOWN), _int_data(-1), _device(device),
      _rssi(-1), _ber(-1), _active_context(false), _connection_status_cb()
{
}

RIL_CellularNetwork::~RIL_CellularNetwork()
{
}

RIL_RadioTechnology RIL_CellularNetwork::get_RIL_rat() const
{
    RIL_RadioTechnology ril_rat = RADIO_TECH_LTE;

    switch (_rat) {
        case RAT_GSM:
        case RAT_GSM_COMPACT:
            ril_rat = RADIO_TECH_GSM;
            break;
        case RAT_UTRAN:
            ril_rat = RADIO_TECH_UMTS;
            break;
        case RAT_EGPRS:
            ril_rat = RADIO_TECH_EDGE;
            break;
        case RAT_HSDPA:
            ril_rat = RADIO_TECH_HSDPA;
            break;
        case RAT_HSUPA:
            ril_rat = RADIO_TECH_HSUPA;
            break;
        case RAT_HSDPA_HSUPA:
            ril_rat = RADIO_TECH_HSPA;
            break;
        case RAT_E_UTRAN:
        case RAT_CATM1:
        case RAT_NB1:
            ril_rat = RADIO_TECH_LTE;
            break;
        case RAT_UNKNOWN:
        case RAT_MAX:
        default:
            break;
    }

    return ril_rat;
}

nsapi_error_t RIL_CellularNetwork::set_registration(const char *plmn)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t RIL_CellularNetwork::get_network_registering_mode(NWRegisteringMode &mode)
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);

    nsapi_error_t ret = _device.lock_and_send_request(RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE, nullptr, 0,
                                                      callback(this, &RIL_CellularNetwork::network_registering_mode_response));

    if (ret == NSAPI_ERROR_OK) {
        MBED_ASSERT(_int_data == 0 || _int_data == 1);
        mode = (NWRegisteringMode)_int_data;
    }

    return ret;
}

nsapi_error_t RIL_CellularNetwork::set_attach()
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t RIL_CellularNetwork::get_attach(AttachStatus &status)
{
    status = CellularNetwork::Detached;
    return NSAPI_ERROR_OK;
}

nsapi_error_t RIL_CellularNetwork::detach()
{
    return NSAPI_ERROR_UNSUPPORTED;
}

void RIL_CellularNetwork::attach(Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    _connection_status_cb = status_cb;
}

nsapi_error_t RIL_CellularNetwork::set_access_technology_impl(RadioAccessTechnology rat)
{
    _rat = rat;
    return NSAPI_ERROR_OK;
}

nsapi_error_t RIL_CellularNetwork::set_access_technology(RadioAccessTechnology rat)
{
    return set_access_technology_impl(rat);
}

nsapi_error_t RIL_CellularNetwork::scan_plmn(operList_t &operators, int &ops_count)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularNetwork::set_ciot_optimization_config(CIoT_Supported_Opt supported_opt,
                                                                CIoT_Preferred_UE_Opt preferred_opt,
                                                                Callback<void(CIoT_Supported_Opt)> network_support_cb)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularNetwork::get_ciot_ue_optimization_config(CIoT_Supported_Opt &supported_opt,
                                                                   CIoT_Preferred_UE_Opt &preferred_opt)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularNetwork::get_ciot_network_optimization_config(CIoT_Supported_Opt &supported_network_opt)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularNetwork::get_signal_quality(int &rssi, int *ber)
{
    ScopedLock<rtos::Mutex> lock(_api_mutex);
    _rssi = -1;
    _ber = -1;
    nsapi_error_t ret = _device.lock_and_send_request(RIL_REQUEST_SIGNAL_STRENGTH, nullptr, 0,
                                                      callback(this, &RIL_CellularNetwork::signal_strength_response));

    if (ret == NSAPI_ERROR_OK) {
        rssi = _rssi;
        *ber = _ber;
    }

    return ret;
}

int RIL_CellularNetwork::get_3gpp_error()
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularNetwork::get_operator_params(int &format, operator_t &operator_params)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularNetwork::set_registration_urc(RegistrationType type, bool on)
{
    if (!_device.get_property((RIL_CellularDevice::CellularProperty)type)) {
        return NSAPI_ERROR_UNSUPPORTED;
    }
    return NSAPI_ERROR_OK;
}

nsapi_error_t RIL_CellularNetwork::get_operator_names(operator_names_list &op_names)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

bool RIL_CellularNetwork::is_active_context(int *number_of_active_contexts, int cid)
{
    return false;
}

nsapi_error_t RIL_CellularNetwork::get_registration_params(registration_params_t &reg_params)
{
    reg_params = _reg_params;
    return NSAPI_ERROR_OK;
}

nsapi_error_t RIL_CellularNetwork::get_registration_params(RegistrationType type, registration_params_t &reg_params)
{
    if (!_device.get_property((RIL_CellularDevice::CellularProperty)type)) {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    ScopedLock<rtos::Mutex> lock(_api_mutex);
    nsapi_error_t ret = _device.lock_and_send_request(RIL_REQUEST_DATA_REGISTRATION_STATE, nullptr, 0,
                                                      callback(this, &RIL_CellularNetwork::data_registration_state_response));

    if (ret == NSAPI_ERROR_OK) {
        reg_params = _reg_params;
    }

    return ret;
}

nsapi_error_t RIL_CellularNetwork::set_receive_period(int mode, EDRXAccessTechnology act_type, uint8_t edrx_value)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

CellularNetwork::RadioAccessTechnology RIL_CellularNetwork::RILrat_to_rat(RIL_RadioTechnology tech)
{
    RadioAccessTechnology rat = RAT_NB1;
    switch (tech) {
        case RADIO_TECH_UNKNOWN:
            rat = RAT_UNKNOWN;
            break;
        case RADIO_TECH_GPRS:
        case RADIO_TECH_EDGE:
            rat = RAT_EGPRS;
            break;
        case RADIO_TECH_UMTS:
            rat = RAT_UTRAN;
            break;
        case RADIO_TECH_HSDPA:
            rat = RAT_HSDPA;
            break;
        case RADIO_TECH_HSUPA:
            rat = RAT_HSUPA;
            break;
        case RADIO_TECH_HSPA:
            rat = RAT_HSDPA_HSUPA;
            break;
        case RADIO_TECH_LTE:
            rat = RAT_NB1; // or  RAT_E_UTRAN, RAT_CATM1 ?
            break;

        case RADIO_TECH_IS95A:
        case RADIO_TECH_IS95B:
        case RADIO_TECH_1xRTT:
        case RADIO_TECH_EVDO_0:
        case RADIO_TECH_EVDO_A:
        case RADIO_TECH_EVDO_B:
        case RADIO_TECH_EHRPD:
        case RADIO_TECH_HSPAP:
        case RADIO_TECH_GSM:
        case RADIO_TECH_TD_SCDMA:
        case RADIO_TECH_IWLAN:
        default:
            break;
    }

    return rat;
}

void RIL_CellularNetwork::data_registration_state_response(ril_token_t *token, RIL_Errno err, void *response,
                                                           size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();
        if (err == RIL_E_SUCCESS && response && response_len > 0) {
            const char **resp = (const char **)response;
            int tmp = atoi(resp[0]); // ((const char **)response)[0] is registration state 0-5 from TS 27.007 10.1.20 AT+CGREG
            MBED_ASSERT(tmp >= 0 && tmp < 6);
            _reg_params._status = (CellularNetwork::RegistrationStatus)tmp;
            tr_info("_reg_params._status: %d", _reg_params._status);

            if (resp[1]) {
                _reg_params._lac = hex_str_to_int(resp[1], TWO_BYTES_HEX);
            }

            tmp = atoi(resp[3]); // indicates the available data radio technology, valid values as defined by RIL_RadioTechnology.
            MBED_ASSERT(RADIO_TECH_UNKNOWN <= tmp && tmp <= RADIO_TECH_IWLAN);
            _reg_params._act  = RILrat_to_rat((RIL_RadioTechnology)tmp);
            tr_info("_reg_params._act: %d", _reg_params._act);

#if MBED_CONF_MBED_TRACE_ENABLE
            if (_reg_params._status == CellularNetwork::RegistrationDenied) {
                /*((const char **)response)[4] if registration state is 3 (Registration denied) this is an enumerated reason why
                                    registration was denied.  See 3GPP TS 24.008, Annex G.6 "Additonal cause codes for GMM".
                                       7 == GPRS services not allowed
                                       8 == GPRS services and non-GPRS services not allowed
                                       9 == MS identity cannot be derived by the network
                                       10 == Implicitly detached
                                       14 == GPRS services not allowed in this PLMN
                                       16 == MSC temporarily not reachable
                                       40 == No PDP context activated */
                tmp = atoi(resp[4]);
                const char *reason = nullptr;
                switch (tmp) {
                    case 7:
                        reason = "GPRS services not allowed";
                        break;
                    case 8:
                        reason = "GPRS services and non-GPRS services not allowed";
                        break;
                    case 9:
                        reason = "MS identity cannot be derived by the network";
                        break;
                    case 10:
                        reason = "Implicitly detached";
                        break;
                    case 14:
                        reason = "GPRS services not allowed in this PLMN";
                        break;
                    case 16:
                        reason = "MSC temporarily not reachable";
                        break;
                    case 40:
                        reason = "No PDP context activated";
                        break;
                    default:
                        reason = "Unknown reason";
                        break;

                }
                tr_info("Registration was denied because of: '%s'", reason);
            }
#endif
            if (resp[7]) {
                _reg_params._cell_id = hex_str_to_int(resp[7], FOUR_BYTES_HEX);
            } else if (resp[8]) {
                _reg_params._cell_id = hex_str_to_int(resp[8], FOUR_BYTES_HEX);
            }
            token->response_error = NSAPI_ERROR_OK;
        } else {
            token->response_error = NSAPI_ERROR_DEVICE_ERROR;
        }

        token->cond_var->notify_one();
        token->cond_mutex->unlock();
    }
}

void RIL_CellularNetwork::network_registering_mode_response(ril_token_t *token, RIL_Errno err, void *response,
                                                            size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();

        if (err == RIL_E_SUCCESS) {
            token->response_error = NSAPI_ERROR_OK;
            _int_data = ((const int *)response)[0];
        } else {
            token->response_error = NSAPI_ERROR_DEVICE_ERROR;
        }

        token->cond_var->notify_one();
        token->cond_mutex->unlock();
    }
}

void RIL_CellularNetwork::signal_strength_response(ril_token_t *token, RIL_Errno err, void *response,
                                                   size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        if (token) {
            token->cond_mutex->lock();
        }

        nsapi_error_t error = NSAPI_ERROR_OK;

        if (err == RIL_E_SUCCESS && response && sizeof(RIL_SignalStrength_v10) == response_len) {
            RIL_SignalStrength_v10 *resp = (RIL_SignalStrength_v10 *)response;
            tr_debug("RIL_CellularNetwork::signal_strength_response, rat: %d", _rat);
            int rssi = -1;

            if (_rat >= RAT_E_UTRAN) {
                // LTE
                RIL_LTE_SignalStrength_v8 lte_signal = resp->LTE_SignalStrength;
                rssi = lte_signal.signalStrength;
            } else {
                // GSM
                RIL_GW_SignalStrength gsm_signal = resp->GW_SignalStrength;
                rssi = gsm_signal.signalStrength;
                if (gsm_signal.bitErrorRate < 0) {
                    error = NSAPI_ERROR_DEVICE_ERROR;
                } else {
                    _ber = gsm_signal.bitErrorRate;
                }
            }

            if (error == NSAPI_ERROR_OK) {
                if (rssi < 0) {
                    error = NSAPI_ERROR_DEVICE_ERROR;
                } else {
                    if (rssi == 99) {
                        _rssi = 0;
                    } else {
                        _rssi = -113 + 2 * rssi;
                    }
                }
            }
        } else {
            error = NSAPI_ERROR_DEVICE_ERROR;
        }

        if (token) {
            token->response_error = error;
            token->cond_var->notify_one();
            token->cond_mutex->unlock();
        }
    }
}

void RIL_CellularNetwork::unsolicited_response(int response_id, const void *data, size_t data_len)
{
    switch (response_id) {
        case RIL_UNSOL_SIGNAL_STRENGTH:
            signal_strength_response(nullptr, RIL_E_SUCCESS, (void *)data, data_len);
            break;
        default:
            break;
    }
}

