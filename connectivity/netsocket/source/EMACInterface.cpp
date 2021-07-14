/* LWIP implementation of NetworkInterfaceAPI
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

#include "netsocket/EMACInterface.h"
#include "mbed-trace/mbed_trace.h"

using namespace mbed;

#define TRACE_GROUP "EMACi"

/* Interface implementation */
EMACInterface::EMACInterface(EMAC &emac, OnboardNetworkStack &stack) :
    _emac(emac),
    _stack(stack)
{
}

nsapi_error_t EMACInterface::set_network(const SocketAddress &ip_address, const SocketAddress &netmask, const SocketAddress &gateway)
{
    _dhcp = false;

    strncpy(_ip_address, ip_address.get_ip_address() ? ip_address.get_ip_address() : "", sizeof(_ip_address));
    _ip_address[sizeof(_ip_address) - 1] = '\0';
    strncpy(_netmask, netmask.get_ip_address() ? netmask.get_ip_address() : "", sizeof(_netmask));
    _netmask[sizeof(_netmask) - 1] = '\0';
    strncpy(_gateway, gateway.get_ip_address() ? gateway.get_ip_address() : "", sizeof(_gateway));
    _gateway[sizeof(_gateway) - 1] = '\0';

    return NSAPI_ERROR_OK;
}

nsapi_error_t EMACInterface::set_dhcp(bool dhcp)
{
    _dhcp = dhcp;
    return NSAPI_ERROR_OK;
}

nsapi_error_t EMACInterface::connect()
{
    if (!_interface) {
        nsapi_error_t err = NSAPI_ERROR_UNSUPPORTED;

        if (_hw_mac_addr_set) {
            err = _stack.add_ethernet_interface(_emac, true, &_interface, _hw_mac_addr, this);
            if (err == NSAPI_ERROR_UNSUPPORTED) {
                tr_error("Failed to set user MAC address");
            }
        }

        if (err == NSAPI_ERROR_UNSUPPORTED) {
            err = _stack.add_ethernet_interface(_emac, true, &_interface, this);
        }

        if (err != NSAPI_ERROR_OK) {
            _interface = NULL;
            return err;
        }
        _interface->attach(_connection_status_cb);
    }

    return _interface->bringup(_dhcp,
                               _ip_address[0] ? _ip_address : 0,
                               _netmask[0] ? _netmask : 0,
                               _gateway[0] ? _gateway : 0,
                               DEFAULT_STACK,
                               _blocking);
}

nsapi_error_t EMACInterface::disconnect()
{
    if (_interface) {
        return _interface->bringdown();
    }
    return NSAPI_ERROR_NO_CONNECTION;
}

const char *EMACInterface::get_mac_address()
{
    if (_interface && _interface->get_mac_address(_mac_address, sizeof(_mac_address))) {
        return _mac_address;
    }
    return nullptr;
}

nsapi_error_t EMACInterface::set_mac_address(uint8_t *mac_addr, nsapi_size_t addr_len)
{
    if (!mac_addr || addr_len != NSAPI_MAC_BYTES) {
        return NSAPI_ERROR_PARAMETER;
    }

    if (_interface) {
        // can't set MAC address once initialized
        return NSAPI_ERROR_BUSY;
    }

    memcpy(_hw_mac_addr, mac_addr, addr_len);
    _hw_mac_addr_set = true;

    return NSAPI_ERROR_OK;
}

nsapi_error_t EMACInterface::get_ip_address(SocketAddress *address)
{
    if (_interface && _interface->get_ip_address(address) == NSAPI_ERROR_OK) {
        strncpy(_ip_address, address->get_ip_address(), sizeof(_ip_address));
        return NSAPI_ERROR_OK;
    }

    return NSAPI_ERROR_NO_CONNECTION;
}

nsapi_error_t EMACInterface::get_ipv6_link_local_address(SocketAddress *address)
{
    if (_interface) {
        return _interface->get_ipv6_link_local_address(address);
    }

    return NSAPI_ERROR_NO_CONNECTION;
}

nsapi_error_t EMACInterface::get_netmask(SocketAddress *address)
{
    if (_interface && _interface->get_netmask(address) == NSAPI_ERROR_OK) {
        strncpy(_netmask, address->get_ip_address(), sizeof(_netmask));
        return NSAPI_ERROR_OK;
    }

    return NSAPI_ERROR_NO_CONNECTION;
}

nsapi_error_t EMACInterface::get_gateway(SocketAddress *address)
{
    if (_interface && _interface->get_gateway(address) == NSAPI_ERROR_OK) {
        strncpy(_gateway, address->get_ip_address(), sizeof(_gateway));
        return NSAPI_ERROR_OK;
    }

    return NSAPI_ERROR_NO_CONNECTION;
}

char *EMACInterface::get_interface_name(char *interface_name)
{
    if (_interface) {
        return _interface->get_interface_name(interface_name);
    }

    return NULL;
}

void EMACInterface::set_as_default()
{
    if (_interface) {
        _stack.set_default_interface(_interface);
    }
}

NetworkStack *EMACInterface::get_stack()
{
    return &_stack;
}

void EMACInterface::attach(
    mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    _connection_status_cb = status_cb;
    if (_interface) {
        _interface->attach(status_cb);
    }
}

nsapi_connection_status_t EMACInterface::get_connection_status() const
{
    if (_interface) {
        return _interface->get_connection_status();
    } else {
        return NSAPI_STATUS_DISCONNECTED;
    }
}

nsapi_error_t EMACInterface::set_blocking(bool blocking)
{
    _blocking = blocking;
    return NSAPI_ERROR_OK;
}
