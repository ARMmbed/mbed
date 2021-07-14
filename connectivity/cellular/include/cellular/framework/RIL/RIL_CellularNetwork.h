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
#ifndef RIL_CELLULARNETWORK_H_
#define RIL_CELLULARNETWORK_H_

#include "CellularNetwork.h"
#include "RILAdaptation.h"

namespace mbed {

class RIL_CellularDevice;

class RIL_CellularNetwork: public CellularNetwork {
public:
    RIL_CellularNetwork(RIL_CellularDevice &device);
    virtual ~RIL_CellularNetwork();

    /** Getter method for RIL_RadioTechnology defined in ril.h
     *
     *  @return enum RIL_RadioTechnology
     */
    RIL_RadioTechnology get_RIL_rat() const;

protected:
    friend class RIL_CellularDevice;
    void unsolicited_response(int response_id, const void *data, size_t data_len);

    /** Sets access technology to be scanned. Modem specific implementation.
     *
     *  @param rat  Radio Access technology
     *
     *  @return     zero on success
     */
    virtual nsapi_error_t set_access_technology_impl(RadioAccessTechnology rat);


public: // CellularNetwork
    nsapi_error_t set_registration(const char *plmn = 0) override;
    nsapi_error_t get_network_registering_mode(NWRegisteringMode &mode) override;
    nsapi_error_t set_attach() override;
    nsapi_error_t get_attach(AttachStatus &status) override;
    nsapi_error_t detach() override;
    void attach(Callback<void(nsapi_event_t, intptr_t)> status_cb) override;
    nsapi_error_t set_access_technology(RadioAccessTechnology rat) override;
    nsapi_error_t scan_plmn(operList_t &operators, int &ops_count) override;
    nsapi_error_t set_ciot_optimization_config(CIoT_Supported_Opt supported_opt,
                                               CIoT_Preferred_UE_Opt preferred_opt,
                                               Callback<void(CIoT_Supported_Opt)> network_support_cb) override;
    nsapi_error_t get_ciot_ue_optimization_config(CIoT_Supported_Opt &supported_opt,
                                                  CIoT_Preferred_UE_Opt &preferred_opt) override;
    nsapi_error_t get_ciot_network_optimization_config(CIoT_Supported_Opt &supported_network_opt) override;
    nsapi_error_t get_signal_quality(int &rssi, int *ber = NULL) override;
    int get_3gpp_error() override;
    nsapi_error_t get_operator_params(int &format, operator_t &operator_params) override;
    nsapi_error_t set_registration_urc(RegistrationType type, bool on) override;
    nsapi_error_t get_operator_names(operator_names_list &op_names) override;
    bool is_active_context(int *number_of_active_contexts = NULL, int cid = -1) override;
    nsapi_error_t get_registration_params(registration_params_t &reg_params) override;
    nsapi_error_t get_registration_params(RegistrationType type, registration_params_t &reg_params) override;
    nsapi_error_t set_receive_period(int mode, EDRXAccessTechnology act_type, uint8_t edrx_value) override;

private:
    void data_registration_state_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len);
    void signal_strength_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len);
    void network_registering_mode_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len);

    CellularNetwork::RadioAccessTechnology RILrat_to_rat(RIL_RadioTechnology tech);

protected:
    RadioAccessTechnology _rat;
    int _int_data;
    rtos::Mutex _api_mutex;
    RIL_CellularDevice &_device;

private:
    registration_params_t _reg_params;
    int _rssi;
    int _ber;
    bool _active_context;
    Callback<void(nsapi_event_t, intptr_t)> _connection_status_cb;
};

} /* namespace mbed */

#endif /* RIL_CELLULARNETWORK_H_ */
