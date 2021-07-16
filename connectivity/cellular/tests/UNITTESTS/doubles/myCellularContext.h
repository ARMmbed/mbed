/*
 * Copyright (c) 2018, Arm Limited and affiliates.
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

#include "CellularContext.h"
#include "ControlPlane_netif_stub.h"

namespace mbed {

class myCellularContext : public CellularContext {
public:
    std::list<nsapi_error_t> return_values;
    nsapi_error_t return_value;
    ControlPlane_netif_stub *my_cp_netif;

    myCellularContext()
    {
        return_value = 0;
        my_cp_netif = NULL;
    }
    ~myCellularContext()
    {
        if (my_cp_netif) {
            delete my_cp_netif;
            my_cp_netif = NULL;
        }
    }

    nsapi_error_t configure_hup(PinName dcd_pin, bool active_high)
    {
        return NSAPI_ERROR_OK;
    };

    void enable_hup(bool enable)
    {
    };

    nsapi_error_t connect()
    {
        return NSAPI_ERROR_OK;
    };

    nsapi_error_t set_device_ready()
    {
        return NSAPI_ERROR_OK;
    };

    nsapi_error_t set_sim_ready()
    {
        return NSAPI_ERROR_OK;
    };

    nsapi_error_t register_to_network()
    {
        return NSAPI_ERROR_OK;
    };

    nsapi_error_t attach_to_network()
    {
        return NSAPI_ERROR_OK;
    };

    nsapi_error_t check_operation(nsapi_error_t err, ContextOperation op)
    {
        return NSAPI_ERROR_OK;
    };

    uint32_t get_timeout_for_operation(ContextOperation op) const
    {
        uint32_t timeout = 10 * 60 * 1000; // default timeout is 10 minutes as registration and attach may take time
        return timeout;
    };

    bool is_connected()
    {
        return true;
    };

    NetworkStack *get_stack()
    {
        return NULL;
    };

    nsapi_error_t get_ip_address(SocketAddress *address)
    {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    const char *get_ip_address()
    {
        return NULL;
    };

    void attach(Callback<void(nsapi_event_t, intptr_t)> status_cb)
    {
    };

    nsapi_error_t set_blocking(bool blocking)
    {
        return NSAPI_ERROR_OK;
    };

    void set_plmn(const char *plmn)
    {
    };

    void set_sim_pin(const char *sim_pin)
    {
    };

    nsapi_error_t connect(const char *sim_pin, const char *apn, const char *uname,
                          const char *pwd)
    {
        return NSAPI_ERROR_OK;
    };

    void set_credentials(const char *apn, const char *uname, const char *pwd)
    {

    };

    bool get_context()
    {
        return true;
    };

    const char* get_nonip_context_type_str() {
        return "Non-IP";
    }

    bool set_new_context(int cid)
    {
        return true;
    };

    nsapi_error_t do_activate_context()
    {
        return NSAPI_ERROR_OK;
    };

    void do_connect()
    {
    };

    nsapi_error_t disconnect()
    {
        return NSAPI_ERROR_OK;
    };

    nsapi_error_t get_apn_backoff_timer(int &backoff_timer)
    {
        return NSAPI_ERROR_OK;
    };

    nsapi_error_t get_rate_control(
        CellularContext::RateControlExceptionReports &reports,
        CellularContext::RateControlUplinkTimeUnit &timeUnit, int &uplinkRate)
    {
        return NSAPI_ERROR_OK;
    };

    nsapi_error_t get_pdpcontext_params(pdpContextList_t &params_list)
    {
        return NSAPI_ERROR_OK;
    };

    // Called by CellularDevice for network and cellular device changes
    void cellular_callback(nsapi_event_t ev, intptr_t ptr)
    {
    };

    ControlPlane_netif_stub *get_cp_netif()
    {
        if (!my_cp_netif) {
            my_cp_netif = new ControlPlane_netif_stub();
        }
        return my_cp_netif;
    };

    nsapi_error_t activate_non_ip_context()
    {
        return NSAPI_ERROR_OK;
    };

    nsapi_error_t setup_control_plane_opt()
    {
        return NSAPI_ERROR_OK;
    };

    void deactivate_ip_context()
    {
    };

    void deactivate_non_ip_context()
    {
    };

    void set_disconnect()
    {
    };

    void set_cid(int cid)
    {
    };

    void do_connect_with_retry()
    {
    };

};

}
