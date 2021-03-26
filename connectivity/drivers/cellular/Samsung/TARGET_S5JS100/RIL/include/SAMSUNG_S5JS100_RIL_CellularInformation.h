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
#ifndef SAMSUNG_S5JS100_RIL_CELLULARINFORMATION_H_
#define SAMSUNG_S5JS100_RIL_CELLULARINFORMATION_H_

#include "RIL_CellularInformation.h"

namespace mbed {

class SAMSUNG_S5JS100_RIL_CellularInformation: public RIL_CellularInformation {
public:
    SAMSUNG_S5JS100_RIL_CellularInformation(RIL_CellularDevice &device);
    virtual ~SAMSUNG_S5JS100_RIL_CellularInformation();

    /** Additional return codes:
     *
     *  NSAPI_ERROR_PARAMETER       if buf_size is not big enough
     *  NSAPI_ERROR_WOULD_BLOCK     if unsolicited response has not arrived
     */
    nsapi_error_t get_iccid(char *buf, size_t buf_size) override;
protected:
    void unsolicited_response(int response_id, const void *data, size_t data_len) override;
private:
    char *_iccid;
};

} /* namespace mbed */

#endif // SAMSUNG_S5JS100_RIL_CELLULARINFORMATION_H_
