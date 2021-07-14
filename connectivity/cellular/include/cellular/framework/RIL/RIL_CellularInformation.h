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
#ifndef RIL_CELLULARINFORMATION_H_
#define RIL_CELLULARINFORMATION_H_

#include "CellularInformation.h"
#include "RILAdaptation.h"

namespace mbed {

class RIL_CellularDevice;

class RIL_CellularInformation: public CellularInformation {
public:
    RIL_CellularInformation(RIL_CellularDevice &device);
    virtual ~RIL_CellularInformation();

public: // from CellularInformation
    nsapi_error_t get_manufacturer(char *buf, size_t buf_size) override;
    nsapi_error_t get_model(char *buf, size_t buf_size) override;
    nsapi_error_t get_revision(char *buf, size_t buf_size) override;
    nsapi_error_t get_serial_number(char *buf, size_t buf_size, SerialNumberType type) override;
    nsapi_error_t get_imsi(char *buf, size_t buf_size) override;
    nsapi_error_t get_iccid(char *buf, size_t buf_size) override;

protected:
    friend class RIL_CellularDevice;
    virtual void unsolicited_response(int response_id, const void *data, size_t data_len);

private:
    nsapi_error_t get_info(int request_id, char *buf, size_t buf_size);
    void request_device_identity_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len);
    void info_request_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len);

private:
    SerialNumberType _type;
    char *_buf;
    RIL_CellularDevice &_device;
    rtos::Mutex _api_mutex;
};

} /* namespace mbed */

#endif // RIL_CELLULARINFORMATION_H_
