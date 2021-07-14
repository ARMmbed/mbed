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
#include "RIL_CellularInformation.h"
#include "RIL_CellularDevice.h"
#include "RIL_CellularContext.h"
#include "CellularLog.h"
#include "ScopedLock.h"

namespace mbed {

RIL_CellularInformation::RIL_CellularInformation(RIL_CellularDevice &device) : _type(IMEI), _buf(nullptr), _device(device)
{
}

RIL_CellularInformation::~RIL_CellularInformation()
{
    delete [] _buf;
    _buf = nullptr;
}

nsapi_error_t RIL_CellularInformation::get_manufacturer(char *buf, size_t buf_size)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularInformation::get_model(char *buf, size_t buf_size)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularInformation::get_iccid(char *buf, size_t buf_size)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t RIL_CellularInformation::get_revision(char *buf, size_t buf_size)
{
    return get_info(RIL_REQUEST_BASEBAND_VERSION, buf, buf_size);
}

nsapi_error_t RIL_CellularInformation::get_imsi(char *buf, size_t buf_size)
{
    return get_info(RIL_REQUEST_GET_IMSI, buf, buf_size);
}

nsapi_error_t RIL_CellularInformation::get_info(int request_id, char *buf, size_t buf_size)
{
    if (buf == nullptr || buf_size == 0) {
        return NSAPI_ERROR_PARAMETER;
    }

    ScopedLock<rtos::Mutex> lock(_api_mutex);
    nsapi_error_t ret = _device.lock_and_send_request(request_id, buf, buf_size,
                                                      callback(this, &RIL_CellularInformation::info_request_response));

    if (ret == NSAPI_ERROR_OK) {
        memcpy(buf, _buf, strlen(_buf) + 1);
        delete [] _buf;
        _buf = nullptr;
    }

    return ret;
}

nsapi_error_t RIL_CellularInformation::get_serial_number(char *buf, size_t buf_size, SerialNumberType type)
{
    // currently not supported in RILD
    if (type == SN || type == SVN) {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    if (buf == nullptr || buf_size == 0) {
        return NSAPI_ERROR_PARAMETER;
    }

    ScopedLock<rtos::Mutex> lock(_api_mutex);
    _type = type;

    nsapi_error_t ret = _device.lock_and_send_request(RIL_REQUEST_DEVICE_IDENTITY, buf, buf_size,
                                                      callback(this, &RIL_CellularInformation::request_device_identity_response));

    if (ret == NSAPI_ERROR_OK) {
        memcpy(buf, _buf, strlen(_buf) + 1);
        delete [] _buf;
        _buf = nullptr;
    }

    return ret;
}

void RIL_CellularInformation::request_device_identity_response(ril_token_t *token, RIL_Errno err, void *response,
                                                               size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();

        const char **resp = (const char **)response;
        if (err == RIL_E_SUCCESS && response && response_len) {
            _buf = new char[strlen(resp[_type - 1]) + 1];
            memcpy(_buf, resp[_type - 1], strlen(resp[_type - 1]) + 1);
            token->response_error = NSAPI_ERROR_OK;
        } else {
            token->response_error = NSAPI_ERROR_DEVICE_ERROR;
        }

        token->cond_var->notify_one();
        token->cond_mutex->unlock();
    }
}

void RIL_CellularInformation::info_request_response(ril_token_t *token, RIL_Errno err, void *response, size_t response_len)
{
    if (err != RIL_E_CANCELLED) {
        token->cond_mutex->lock();

        // RIL_REQUEST_BASEBAND_VERSION and RIL_REQUEST_GET_IMSI
        if (err == RIL_E_SUCCESS && response && response_len) {
            token->response_error = NSAPI_ERROR_OK;
            _buf = new char[response_len];
            memcpy(_buf, (const char *)response, response_len);
        } else {
            token->response_error = NSAPI_ERROR_DEVICE_ERROR;
        }

        token->cond_var->notify_one();
        token->cond_mutex->unlock();
    }
}

void RIL_CellularInformation::unsolicited_response(int response_id, const void *data, size_t data_len)
{
}

} /* namespace mbed */
