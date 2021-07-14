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

#include "netsocket/NetworkStack.h"
#include "netsocket/nsapi_dns.h"
#include "stddef.h"
#include <new>
#include "events/EventQueue.h"
#include "events/mbed_shared_queues.h"
#include "platform/mbed_error.h"

// Default NetworkStack operations

using namespace std::chrono;

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

nsapi_error_t NetworkStack::gethostbyname(const char *name, SocketAddress *address, nsapi_version_t version, const char *interface_name)
{
    if (name[0] == '\0') {
        return NSAPI_ERROR_PARAMETER;
    }

    // check for simple ip addresses
    if (address->set_ip_address(name)) {
        if (version != NSAPI_UNSPEC && address->get_ip_version() != version) {
            return NSAPI_ERROR_DNS_FAILURE;
        }

        return NSAPI_ERROR_OK;
    }

    // if the version is unspecified, try to guess the version from the
    // ip address of the underlying stack
    if (version == NSAPI_UNSPEC) {
        SocketAddress testaddress;
        if (this->get_ip_address(&testaddress) == NSAPI_ERROR_OK) {
            version = testaddress.get_ip_version();
        }
    }

    return nsapi_dns_query(this, name, address, interface_name, version);
}

nsapi_value_or_error_t NetworkStack::getaddrinfo(const char *hostname, SocketAddress *hints, SocketAddress **res, const char *interface_name)
{
    int i;
    if (hostname[0] == '\0') {
        return NSAPI_ERROR_PARAMETER;
    }
    nsapi_version_t version = hints->get_ip_version();
    // if the version is unspecified, try to guess the version from the
    // ip address of the underlying stack
    if (version == NSAPI_UNSPEC) {
        SocketAddress testaddress;
        if (this->get_ip_address(&testaddress) == NSAPI_ERROR_OK) {
            version = testaddress.get_ip_version();
        }
    }

    SocketAddress *temp = new (std::nothrow) SocketAddress [MBED_CONF_NSAPI_DNS_ADDRESSES_LIMIT];

    if (!temp) {
        return NSAPI_ERROR_NO_MEMORY;
    }

    int adr_cnt = nsapi_dns_query_multiple(this, hostname, temp, MBED_CONF_NSAPI_DNS_ADDRESSES_LIMIT, interface_name, version);

    if (adr_cnt > 0) {
        *res = new (std::nothrow) SocketAddress [adr_cnt];
        for (i = 0;  i < adr_cnt; i++) {
            (*res)[i] = temp[i];
        }
    }
    delete[] temp;
    return adr_cnt;
}

nsapi_value_or_error_t NetworkStack::gethostbyname_async(const char *name, hostbyname_cb_t callback, nsapi_version_t version, const char *interface_name)
{
    SocketAddress address;

    if (name[0] == '\0') {
        return NSAPI_ERROR_PARAMETER;
    }

    // check for simple ip addresses
    if (address.set_ip_address(name)) {
        if (version != NSAPI_UNSPEC && address.get_ip_version() != version) {
            return NSAPI_ERROR_DNS_FAILURE;
        }

        callback(1, &address);
        return NSAPI_ERROR_OK;
    }

    // if the version is unspecified, try to guess the version from the
    // ip address of the underlying stack
    if (version == NSAPI_UNSPEC) {
        SocketAddress testaddress;
        if (this->get_ip_address(&testaddress) == NSAPI_ERROR_OK) {
            version = testaddress.get_ip_version();
        }
    }

    call_in_callback_cb_t call_in_cb = get_call_in_callback();

    return nsapi_dns_query_async(this, name, callback, call_in_cb, interface_name, version);
}

nsapi_value_or_error_t NetworkStack::getaddrinfo_async(const char *hostname, SocketAddress *hints, hostbyname_cb_t callback, const char *interface_name)
{
    SocketAddress address;

    if (hostname[0] == '\0') {
        return NSAPI_ERROR_PARAMETER;
    }
    nsapi_version_t version = hints->get_ip_version();
    // if the version is unspecified, try to guess the version from the
    // ip address of the underlying stack
    if (version == NSAPI_UNSPEC) {
        SocketAddress testaddress;
        if (this->get_ip_address(&testaddress) == NSAPI_ERROR_OK) {
            version = testaddress.get_ip_version();
        }
    }

    call_in_callback_cb_t call_in_cb = get_call_in_callback();

    return nsapi_dns_query_multiple_async(this, hostname, callback, MBED_CONF_NSAPI_DNS_ADDRESSES_LIMIT, call_in_cb, interface_name, version);
}
nsapi_error_t NetworkStack::gethostbyname_async_cancel(int id)
{
    return nsapi_dns_query_async_cancel(id);
}

nsapi_error_t NetworkStack::add_dns_server(const SocketAddress &address, const char *interface_name)
{
    return nsapi_dns_add_server(address, interface_name);
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

nsapi_error_t NetworkStack::call_in(int delay, mbed::Callback<void()> func)
{
    static events::EventQueue *event_queue = mbed::mbed_event_queue();

    if (!event_queue) {
        goto NO_MEM;
    }

    if (delay > 0) {
        if (event_queue->call_in(milliseconds(delay), func) == 0) {
            goto NO_MEM;
        }
    } else {
        if (event_queue->call(func) == 0) {
            goto NO_MEM;
        }
    }

    return NSAPI_ERROR_OK;

NO_MEM:
    MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_DRIVER, MBED_ERROR_CODE_ENOMEM), \
               "NetworkStack::call_in() unable to add event to queue. Increase \"events.shared-eventsize\"\n");
}

call_in_callback_cb_t NetworkStack::get_call_in_callback()
{
    call_in_callback_cb_t cb(this, &NetworkStack::call_in);
    return cb;
}

// NetworkStackWrapper class for encapsulating the raw nsapi_stack structure
class NetworkStackWrapper : public NetworkStack {
private:
    inline nsapi_stack_t *_stack()
    {
        return reinterpret_cast<nsapi_stack_t *>(
                   reinterpret_cast<uint8_t *>(this)
                   - offsetof(nsapi_stack_t, _stack_buffer));
    }

    inline const nsapi_stack_api_t *_stack_api()
    {
        return _stack()->stack_api;
    }

public:
    nsapi_error_t get_ip_address(SocketAddress *address) override
    {
        if (!_stack_api()->get_ip_address) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        *address = SocketAddress(_stack_api()->get_ip_address(_stack()));

        return *address ? NSAPI_ERROR_OK : NSAPI_ERROR_NO_ADDRESS;
    }

    nsapi_error_t gethostbyname(const char *name, SocketAddress *address, nsapi_version_t version, const char *interface_name) override
    {
        if (!_stack_api()->gethostbyname) {
            return NetworkStack::gethostbyname(name, address, version, interface_name);
        }

        nsapi_addr_t addr = {NSAPI_UNSPEC, 0};
        nsapi_error_t err = _stack_api()->gethostbyname(_stack(), name, &addr, version);
        address->set_addr(addr);
        return err;
    }

    nsapi_error_t add_dns_server(const SocketAddress &address, const char *interface_name) override
    {
        if (!_stack_api()->add_dns_server) {
            return NetworkStack::add_dns_server(address, interface_name);
        }

        return _stack_api()->add_dns_server(_stack(), address.get_addr());
    }

    nsapi_error_t setstackopt(int level, int optname, const void *optval, unsigned optlen) override
    {
        if (!_stack_api()->setstackopt) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->setstackopt(_stack(), level, optname, optval, optlen);
    }

    nsapi_error_t getstackopt(int level, int optname, void *optval, unsigned *optlen) override
    {
        if (!_stack_api()->getstackopt) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->getstackopt(_stack(), level, optname, optval, optlen);
    }

protected:
    nsapi_error_t socket_open(nsapi_socket_t *socket, nsapi_protocol_t proto) override
    {
        if (!_stack_api()->socket_open) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->socket_open(_stack(), socket, proto);
    }

    nsapi_error_t socket_close(nsapi_socket_t socket) override
    {
        if (!_stack_api()->socket_close) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->socket_close(_stack(), socket);
    }

    nsapi_error_t socket_bind(nsapi_socket_t socket, const SocketAddress &address) override
    {
        if (!_stack_api()->socket_bind) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->socket_bind(_stack(), socket, address.get_addr(), address.get_port());
    }

    nsapi_error_t socket_listen(nsapi_socket_t socket, int backlog) override
    {
        if (!_stack_api()->socket_listen) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->socket_listen(_stack(), socket, backlog);
    }

    nsapi_error_t socket_connect(nsapi_socket_t socket, const SocketAddress &address) override
    {
        if (!_stack_api()->socket_connect) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->socket_connect(_stack(), socket, address.get_addr(), address.get_port());
    }

    nsapi_error_t socket_accept(nsapi_socket_t server, nsapi_socket_t *socket, SocketAddress *address) override
    {
        if (!_stack_api()->socket_accept) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        nsapi_addr_t addr = {NSAPI_IPv4, 0};
        uint16_t port = 0;

        nsapi_error_t err = _stack_api()->socket_accept(_stack(), server, socket, &addr, &port);

        if (address) {
            address->set_addr(addr);
            address->set_port(port);
        }

        return err;
    }

    nsapi_size_or_error_t socket_send(nsapi_socket_t socket, const void *data, nsapi_size_t size) override
    {
        if (!_stack_api()->socket_send) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->socket_send(_stack(), socket, data, size);
    }

    nsapi_size_or_error_t socket_recv(nsapi_socket_t socket, void *data, nsapi_size_t size) override
    {
        if (!_stack_api()->socket_recv) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->socket_recv(_stack(), socket, data, size);
    }

    nsapi_size_or_error_t socket_sendto(nsapi_socket_t socket, const SocketAddress &address, const void *data, nsapi_size_t size) override
    {
        if (!_stack_api()->socket_sendto) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->socket_sendto(_stack(), socket, address.get_addr(), address.get_port(), data, size);
    }

    nsapi_size_or_error_t socket_recvfrom(nsapi_socket_t socket, SocketAddress *address, void *data, nsapi_size_t size) override
    {
        if (!_stack_api()->socket_recvfrom) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        nsapi_addr_t addr = {NSAPI_IPv4, 0};
        uint16_t port = 0;

        nsapi_size_or_error_t err = _stack_api()->socket_recvfrom(_stack(), socket, &addr, &port, data, size);

        if (address) {
            address->set_addr(addr);
            address->set_port(port);
        }

        return err;
    }

    nsapi_size_or_error_t socket_sendmsg(nsapi_socket_t socket, const SocketAddress &address, const void *data, nsapi_size_t size, nsapi_msghdr_t *control, nsapi_size_t control_size) override
    {
        if (control != NULL) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        if (!_stack_api()->socket_sendmsg) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->socket_sendmsg(_stack(), socket, address.get_addr(), address.get_port(), data, size, control, control_size);
    }

    nsapi_size_or_error_t socket_recvmsg(nsapi_socket_t socket, SocketAddress *address, void *data, nsapi_size_t size, nsapi_msghdr_t *control, nsapi_size_t control_size) override
    {
        if (control != NULL) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        if (!_stack_api()->socket_recvmsg) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        nsapi_addr_t addr = {NSAPI_IPv4, 0};
        uint16_t port = 0;

        nsapi_size_or_error_t err = _stack_api()->socket_recvmsg(_stack(), socket, &addr, &port, data, size, control, control_size);

        if (address) {
            address->set_addr(addr);
            address->set_port(port);
        }

        return err;
    }



    void socket_attach(nsapi_socket_t socket, void (*callback)(void *), void *data) override
    {
        if (!_stack_api()->socket_attach) {
            return;
        }

        return _stack_api()->socket_attach(_stack(), socket, callback, data);
    }

    nsapi_error_t setsockopt(nsapi_socket_t socket, int level, int optname, const void *optval, unsigned optlen) override
    {
        if (!_stack_api()->setsockopt) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->setsockopt(_stack(), socket, level, optname, optval, optlen);
    }

    nsapi_error_t getsockopt(nsapi_socket_t socket, int level, int optname, void *optval, unsigned *optlen) override
    {
        if (!_stack_api()->getsockopt) {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        return _stack_api()->getsockopt(_stack(), socket, level, optname, optval, optlen);
    }
};


// Conversion function for network stacks
NetworkStack *nsapi_create_stack(nsapi_stack_t *stack)
{
    static_assert(sizeof stack->_stack_buffer >= sizeof(NetworkStackWrapper),
                  "The nsapi_stack_t stack buffer must fit a NetworkStackWrapper");
    return new (stack->_stack_buffer) NetworkStackWrapper;
}
