/* Socket
 * Copyright (c) 2015 ARM Limited
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

#include "ns_list.h"
#include "netsocket/NetworkInterface.h"
#include "netsocket/NetworkStack.h"
#include "platform/Callback.h"
#include "platform/mbed_error.h"
#include <new>
#include <string.h>

// Default network-interface state
void NetworkInterface::set_as_default()
{

}

const char *NetworkInterface::get_mac_address()
{
    return 0;
}

nsapi_error_t NetworkInterface::set_mac_address(uint8_t *mac_addr, nsapi_size_t addr_len)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkInterface::get_ip_address(SocketAddress *)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkInterface::get_ipv6_link_local_address(SocketAddress *address)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkInterface::get_netmask(SocketAddress *)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkInterface::get_gateway(SocketAddress *)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

char *NetworkInterface::get_interface_name(char *interface_name)
{
    return 0;
}

nsapi_error_t NetworkInterface::set_network(const SocketAddress &ip_address, const SocketAddress &netmask, const SocketAddress &gateway)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkInterface::set_dhcp(bool dhcp)
{
    if (!dhcp) {
        return NSAPI_ERROR_UNSUPPORTED;
    } else {
        return NSAPI_ERROR_OK;
    }
}

nsapi_error_t NetworkInterface::gethostbyname(const char *name, SocketAddress *address, nsapi_version_t version, const char *interface_name)
{
    return get_stack()->gethostbyname(name, address, version, interface_name);
}

nsapi_value_or_error_t NetworkInterface::getaddrinfo(const char *hostname, SocketAddress *hints, SocketAddress **res, const char *interface_name)
{
    return get_stack()->getaddrinfo(hostname, hints, res, interface_name);
}

nsapi_value_or_error_t NetworkInterface::gethostbyname_async(const char *host, hostbyname_cb_t callback, nsapi_version_t version, const char *interface_name)
{
    return get_stack()->gethostbyname_async(host, callback, version, interface_name);
}

nsapi_value_or_error_t NetworkInterface::getaddrinfo_async(const char *hostname, SocketAddress *hints, hostbyname_cb_t callback, const char *interface_name)
{
    return get_stack()->getaddrinfo_async(hostname, hints, callback, interface_name);
}

nsapi_error_t NetworkInterface::gethostbyname_async_cancel(int id)
{
    return get_stack()->gethostbyname_async_cancel(id);
}

nsapi_error_t NetworkInterface::add_dns_server(const SocketAddress &address, const char *interface_name)
{
    return get_stack()->add_dns_server(address, interface_name);
}

nsapi_error_t NetworkInterface::get_dns_server(int index, SocketAddress *address, const char *interface_name)
{
    return get_stack()->get_dns_server(index, address, interface_name);
}

void NetworkInterface::attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    // Dummy, that needs to be overwritten when inherited, but cannot be removed
    // because suplied previously and can be referred from binaries.
}

typedef struct iface_eventlist_entry {
    NetworkInterface *iface;
    mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb;
    ns_list_link_t link;
} iface_eventlist_entry_t;

typedef NS_LIST_HEAD(iface_eventlist_entry_t, link) iface_eventlist_t;

static iface_eventlist_t *get_interface_event_list_head()
{
    static iface_eventlist_t NS_LIST_NAME_INIT(event_list);
    return &event_list;
}

static void call_all_event_listeners(NetworkInterface *iface, nsapi_event_t event, intptr_t val)
{
    iface_eventlist_t *event_list = get_interface_event_list_head();
    ns_list_foreach(iface_eventlist_entry_t, entry, event_list) {
        if (entry->iface == iface) {
            entry->status_cb(event, val);
        }
    }
}

nsapi_error_t NetworkInterface::internal_add_event_listener(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    iface_eventlist_t *event_list = get_interface_event_list_head();
#if MBED_CONF_PLATFORM_CALLBACK_COMPARABLE
    ns_list_foreach_safe(iface_eventlist_entry_t, entry, event_list) {
        if (entry->status_cb == status_cb && entry->iface == this) {
            return NSAPI_ERROR_OK;
        }
    }
#endif

    iface_eventlist_entry_t *entry = new (std::nothrow) iface_eventlist_entry_t;
    if (!entry) {
        return NSAPI_ERROR_NO_MEMORY;
    }

    entry->iface = this;
    entry->status_cb = status_cb;
    ns_list_add_to_end(event_list, entry);
    attach(mbed::callback(&call_all_event_listeners, this));
    return NSAPI_ERROR_OK;
}

#if MBED_CONF_NSAPI_ADD_EVENT_LISTENER_RETURN_CHANGE
nsapi_error_t NetworkInterface::add_event_listener(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    return internal_add_event_listener(status_cb);
}
#else
void NetworkInterface::add_event_listener(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    auto error = internal_add_event_listener(status_cb);
    switch (error) {
        case NSAPI_ERROR_OK:
            return;
        case NSAPI_ERROR_NO_MEMORY:
            MBED_ERROR(error, "Out of memory when adding an event listener");
        default:
            MBED_UNREACHABLE;
    }
}
#endif // MBED_CONF_NSAPI_ADD_EVENT_LISTENER_RETURN_CHANGE

#if MBED_CONF_PLATFORM_CALLBACK_COMPARABLE
void NetworkInterface::remove_event_listener(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    iface_eventlist_t *event_list = get_interface_event_list_head();
    ns_list_foreach_safe(iface_eventlist_entry_t, entry, event_list) {
        if (entry->status_cb == status_cb && entry->iface == this) {
            ns_list_remove(event_list, entry);
            delete entry;
        }
    }
}
#endif

NetworkInterface::~NetworkInterface()
{
    iface_eventlist_t *event_list = get_interface_event_list_head();
    ns_list_foreach_safe(iface_eventlist_entry_t, entry, event_list) {
        if (entry->iface == this) {
            ns_list_remove(event_list, entry);
            delete entry;
        }
    }
}

nsapi_connection_status_t NetworkInterface::get_connection_status() const
{
    return NSAPI_STATUS_ERROR_UNSUPPORTED;
}

nsapi_error_t NetworkInterface::set_blocking(bool blocking)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
