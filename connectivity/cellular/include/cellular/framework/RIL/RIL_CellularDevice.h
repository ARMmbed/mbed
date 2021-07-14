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
#ifndef RIL_CELLULARDEVICE_H_
#define RIL_CELLULARDEVICE_H_

#include "CellularDevice.h"
#include "CellularContext.h"
#include "RILAdaptation.h"
#include "Semaphore.h"

namespace mbed {

class RILAdaptation;
class RIL_CellularContext;
class RIL_CellularNetwork;
class RIL_CellularInformation;

class RIL_CellularDevice: public CellularDevice {
public:
    RIL_CellularDevice(RILAdaptation &ril);
    virtual ~RIL_CellularDevice();

    // Cellular properties. Defines what features/technologies are supported/used by the module
    enum CellularProperty {
        PROPERTY_C_EREG,            // 0 = not supported, 1 = supported. Does modem support this registration mode?
        PROPERTY_C_GREG,            // 0 = not supported, 1 = supported. Does modem support this registration mode?
        PROPERTY_C_REG,             // 0 = not supported, 1 = supported. Does modem support this registration mode?
        PROPERTY_IPV4_PDP_TYPE,     // 0 = not supported, 1 = supported. Does modem support IPV4?
        PROPERTY_IPV6_PDP_TYPE,     // 0 = not supported, 1 = supported. Does modem support IPV6?
        PROPERTY_IPV4V6_PDP_TYPE,   // 0 = not supported, 1 = supported. Does modem support dual stack IPV4V6?
        PROPERTY_NON_IP_PDP_TYPE,   // 0 = not supported, 1 = supported. Does modem support Non-IP?
        PROPERTY_MAX
    };

    intptr_t get_property(CellularProperty key);

    // from CellularDevice
    nsapi_error_t hard_power_on() override;
    nsapi_error_t hard_power_off() override;
    nsapi_error_t soft_power_on() override;
    nsapi_error_t soft_power_off() override;
    nsapi_error_t set_pin(const char *sim_pin) override;
    nsapi_error_t get_sim_state(SimState &state) override;
    CellularContext *create_context(const char *apn = NULL, bool cp_req = false, bool nonip_req = false) override;
    void delete_context(CellularContext *context) override;
    CellularNetwork *open_network() override;
#if MBED_CONF_CELLULAR_USE_SMS || defined(DOXYGEN_ONLY)
    CellularSMS *open_sms() override;
    void close_sms() override;
#endif
    CellularInformation *open_information() override;
    void close_network() override;
    void close_information() override;
    void set_timeout(int timeout) override;
    void modem_debug_on(bool on) override;
    nsapi_error_t init() override;
    nsapi_error_t is_ready() override;
    void set_ready_cb(Callback<void()> callback) override;
    nsapi_error_t set_power_save_mode(int periodic_time, int active_time = 0) override;
    ATHandler *get_at_handler() override;
    nsapi_error_t clear() override;
    CellularContext *get_context_list() const override;
    nsapi_error_t set_baud_rate(int baud_rate) override;
    nsapi_error_t send_at_command(char *data, size_t data_len);

    nsapi_error_t lock_and_send_request(int request_id, void *data, size_t data_len,
                                        Callback<void(ril_token_t *, RIL_Errno, void *, size_t)> callback);

    RILAdaptation &get_ril();

protected:
    virtual RIL_CellularContext *create_context_impl(const char *apn, bool cp_req = false, bool nonip_req = false);
    virtual RIL_CellularNetwork *open_network_impl();
    virtual RIL_CellularInformation *open_information_impl();
    virtual void unsolicited_response(int response_id, const void *data, size_t data_len);

    const intptr_t *_property_array;
    int _data;
    rtos::Mutex _api_mutex;
    RIL_CellularContext *_context_list;

private:
    RIL_CellularDevice();

    friend class RILAdaptation;
    void request_ack(ril_token_t *token);
    void request_timed_callback(RIL_TimedCallback callback, void *param, const struct timeval *relative_time);
    // helper methods
    void get_radiostatus(SimState *state = NULL, bool unsolicited_resp = false);
    void sim_status_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len);
    void common_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len);
    void check_registration_state();

    RILAdaptation &_ril;
    RIL_CellularNetwork *_network;
    RIL_CellularInformation *_information;

    bool _device_ready;
    Callback<void()> _device_ready_cb;
};

} /* namespace mbed */

#endif /* RIL_CELLULARDEVICE_H_ */
