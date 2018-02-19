/*
 * Copyright (c) 2017, Arm Limited and affiliates.
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

#ifndef TELIT_HE910_CELLULAR_NETWORK_H_
#define TELIT_HE910_CELLULAR_NETWORK_H_

#include "AT_CellularNetwork.h"

namespace mbed {

class TELIT_HE910_CellularNetwork : public AT_CellularNetwork
{
public:
    TELIT_HE910_CellularNetwork(ATHandler &atHandler);
    virtual ~TELIT_HE910_CellularNetwork();

protected:

    virtual bool get_modem_stack_type(nsapi_ip_stack_t requested_stack);

    /**
     * Check if modem supports given registration type.
     *
     * @param rat enum RegistrationType
     * @return true if given registration type is supported by modem
     */
    virtual bool has_registration(RegistrationType rat);

    /**
     * Sets access technology to be scanned.
     *
     * @param opRat Access technology
     *
     * @return zero on success
     */
    virtual nsapi_error_t set_access_technology_impl(operator_t::RadioAccessTechnology opRat);
};
} // namespace mbed
#endif // TELIT_HE910_CELLULAR_NETWORK_H_
