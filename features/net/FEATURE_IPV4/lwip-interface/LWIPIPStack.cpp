/* LWIPIPStackInterface
 * Copyright (c) 2015 - 2016 ARM Limited
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

#include "platform.h"

#if DEVICE_EMAC

#include "IPStackInterface.h"
#include "lwip_stack.h"

/** LWIP specific implementation of IPStackInterface */

void IPStackInterface::bringup(emac_interface_t *emac)
{
    lwip_bringup(emac);
}

void IPStackInterface::bringdown()
{
    lwip_bringdown();
}

const char * IPStackInterface::get_mac_address()
{
    return lwip_get_mac_address();
}

const char * IPStackInterface::get_ip_address()
{
    return lwip_get_ip_address();
}

#endif /* DEVICE_EMAC */
