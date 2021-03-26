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
#include "SAMSUNG_S5JS100_RIL_CellularInformation.h"

namespace mbed {

SAMSUNG_S5JS100_RIL_CellularInformation::SAMSUNG_S5JS100_RIL_CellularInformation(RIL_CellularDevice &device) :
    RIL_CellularInformation(device), _iccid(0)
{
}

SAMSUNG_S5JS100_RIL_CellularInformation::~SAMSUNG_S5JS100_RIL_CellularInformation()
{
    if (_iccid) {
        delete [] _iccid;
        _iccid = nullptr;
    }
}

nsapi_error_t SAMSUNG_S5JS100_RIL_CellularInformation::get_iccid(char *buf, size_t buf_size)
{
    if (_iccid) {
        size_t iccid_len = strlen(_iccid) + 1;
        if (buf_size < iccid_len) {
            return NSAPI_ERROR_PARAMETER;
        } else {
            memcpy(buf, _iccid, iccid_len);
            return NSAPI_ERROR_OK;
        }
    } else {
        return NSAPI_ERROR_WOULD_BLOCK;
    }
}

void SAMSUNG_S5JS100_RIL_CellularInformation::unsolicited_response(int response_id, const void *data, size_t data_len)
{
    if (response_id == RIL_UNSOL_ICCID_INFO) {
        if (data_len > 0 && data) {
            delete [] _iccid;
            _iccid = new char[data_len + 1];
            memcpy(_iccid, (const char *)data, data_len + 1);
        }
    }

    RIL_CellularInformation::unsolicited_response(response_id, data, data_len);
}

} /* namespace mbed */
