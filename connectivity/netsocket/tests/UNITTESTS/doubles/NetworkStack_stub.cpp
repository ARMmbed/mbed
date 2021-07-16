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

#include "netsocket/NetworkStack.h"
#include "netsocket/nsapi_dns.h"
#include "stddef.h"
#include <new>

// Default NetworkStack operations
nsapi_error_t NetworkStack::gethostbyname(const char *name, SocketAddress *address, nsapi_version_t version, const char *interface_name)
{
    return NSAPI_ERROR_OK;
}

nsapi_value_or_error_t NetworkStack::getaddrinfo(const char *hostname, SocketAddress *hints, SocketAddress **res, const char *interface_name)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t NetworkStack::add_dns_server(const SocketAddress &address, const char *interface_name)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t NetworkStack::get_dns_server(int index, SocketAddress *address, const char *interface_name)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkStack::setstackopt(int level, int optname, const void *optval, unsigned optlen)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkStack::getstackopt(int level, int optname, void *optval, unsigned *optlen)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkStack::setsockopt(void *handle, int level, int optname, const void *optval, unsigned optlen)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkStack::getsockopt(void *handle, int level, int optname, void *optval, unsigned *optlen)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

// Conversion function for network stacks
NetworkStack *nsapi_create_stack(nsapi_stack_t *stack)
{
    return nullptr;
}

nsapi_value_or_error_t NetworkStack::gethostbyname_async(const char *host, hostbyname_cb_t callback, nsapi_version_t version,
                                                         const char *interface_name)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_value_or_error_t NetworkStack::getaddrinfo_async(const char *hostname, SocketAddress *hints, hostbyname_cb_t callback, const char *interface_name)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkStack::gethostbyname_async_cancel(int id)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

call_in_callback_cb_t NetworkStack::get_call_in_callback()
{
    return NULL;
}

nsapi_error_t NetworkStack::call_in(int delay, mbed::Callback<void()> func)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkStack::get_ip_address(SocketAddress *address)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkStack::get_ipv6_link_local_address(SocketAddress *address)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkStack::get_ip_address_if(SocketAddress *address, const char *interface_name)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
