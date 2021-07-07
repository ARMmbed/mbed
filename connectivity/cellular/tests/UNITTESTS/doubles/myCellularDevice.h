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
#ifndef MY_CELLULARDEVICE_H_
#define MY_CELLULARDEVICE_H_

#include "AT_CellularDevice.h"
#include "AT_CellularNetwork.h"
#include "FileHandle_stub.h"
#include "ATHandler_stub.h"
#include "AT_CellularContext.h"
#include "gtest/gtest.h"
#include "drivers/BufferedSerial.h"

using namespace events;

namespace mbed {

class CellularSMS;
class CellularInformation;
class CellularContext;
class FileHandle;

class myCellularDevice : public AT_CellularDevice {
public:
    myCellularDevice(FileHandle *fh) : AT_CellularDevice(fh), _context_list(0), _network(0) {}
    virtual ~myCellularDevice()
    {
        delete _context_list;
        delete _network;
    }

    virtual nsapi_error_t set_pin(const char *sim_pin)
    {
        return NSAPI_ERROR_OK;
    }

    virtual nsapi_error_t get_sim_state(SimState &state)
    {
        return NSAPI_ERROR_OK;
    }


    virtual CellularContext *create_context(const char *apn = NULL, bool cp_req = false, bool nonip_req = false)
    {
        if (_context_list) {
            return _context_list;
        }
        EventQueue que;
        FileHandle_stub fh1;
        ATHandler at(&fh1, que, 0, ",");
        _context_list = new AT_CellularContext(at, this);
        return _context_list;
    }

    virtual void delete_context(CellularContext *context)
    {
        delete _context_list;
    }

    virtual CellularNetwork *open_network()
    {
        if (_network) {
            return _network;
        }
        EventQueue que;
        FileHandle_stub fh1;
        ATHandler at(&fh1, que, 0, ",");
        _network = new AT_CellularNetwork(at, *this);
        return _network;
    }

    virtual CellularSMS *open_sms()
    {
        return NULL;
    }

    virtual CellularInformation *open_information()
    {
        return NULL;
    }

    virtual void close_network()
    {
        delete _network;
    }

    virtual void close_sms() {}

    virtual void close_information() {}

    virtual void set_timeout(int timeout) {}

    virtual void modem_debug_on(bool on) {}

    virtual nsapi_error_t init()
    {
        return NSAPI_ERROR_OK;
    }

    virtual nsapi_error_t is_ready()
    {
        return NSAPI_ERROR_OK;
    }

    virtual nsapi_error_t hard_power_on()
    {
        return NSAPI_ERROR_OK;
    }

    virtual nsapi_error_t hard_power_off()
    {
        return NSAPI_ERROR_OK;
    }

    virtual nsapi_error_t soft_power_on()
    {
        return NSAPI_ERROR_OK;
    }

    virtual nsapi_error_t soft_power_off()
    {
        return NSAPI_ERROR_OK;
    }

    virtual void set_ready_cb(Callback<void()> callback)
    {
    }

    nsapi_error_t set_power_save_mode(int periodic_time, int active_time)
    {
        return NSAPI_ERROR_OK;
    }

    virtual CellularContext *get_context_list() const
    {
        return _context_list;
    }
    void cellular_callback(nsapi_event_t ev, intptr_t ptr)
    {
        CellularDevice::cellular_callback(ev, ptr);
    }
    virtual ATHandler *get_at_handler()
    {
        return NULL;
    }
    virtual nsapi_error_t release_at_handler(ATHandler *at_handler)
    {
        return NSAPI_ERROR_OK;
    }
    nsapi_error_t set_baud_rate(int baud_rate)
    {
        return NSAPI_ERROR_OK;
    }

    void verify_timeout_array(const uint16_t timeout[], int array_len)
    {
        if (array_len > CELLULAR_RETRY_ARRAY_SIZE) {
            FAIL();
        }
        uint16_t get_timeouts[CELLULAR_RETRY_ARRAY_SIZE];
        int get_timeouts_len = 0;

        get_retry_timeout_array(get_timeouts, get_timeouts_len);

        EXPECT_EQ(array_len, get_timeouts_len);

        for (int i = 0; i < array_len; i++) {
            EXPECT_EQ(timeout[i], get_timeouts[i]);
        }
    }



    AT_CellularNetwork *_network;
    AT_CellularContext *_context_list;
};

} // namespace
#endif /* MY_CELLULARDEVICE_H_ */
