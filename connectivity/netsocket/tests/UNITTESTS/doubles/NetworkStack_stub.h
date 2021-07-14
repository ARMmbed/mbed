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

#ifndef NETWORKSTACKSTUB_H
#define NETWORKSTACKSTUB_H

#include "netsocket/NetworkStack.h"
#include <list>

/*
 * Note: If you want to:
 * - control and/or set expectations for the data returned/sent from/to the stack
 * - set expectations on the calls to NetworkStack
 * See OnboardNetworkStack_mock.h and its OnboardNetworkStackMock class.
 */

class NetworkStackstub : public NetworkStack {
public:
    std::list<nsapi_error_t> return_values;
    nsapi_error_t return_value;
    SocketAddress return_socketAddress;

    NetworkStackstub() :
        return_value(0),
        return_socketAddress()
    {
    }

    virtual nsapi_error_t get_ip_address(SocketAddress *address)
    {
        address->set_ip_address("127.0.0.1");
        return NSAPI_ERROR_OK;
    }
    virtual nsapi_error_t gethostbyname(const char *host,
                                        SocketAddress *address, nsapi_version_t version, const char *interface_name)
    {
        return return_value;
    }
    virtual nsapi_value_or_error_t getaddrinfo(const char *hostname, SocketAddress *hints, SocketAddress **res, const char *interface_name)
    {
        return return_value;
    }
    virtual nsapi_value_or_error_t gethostbyname_async(const char *host, hostbyname_cb_t callback, nsapi_version_t version,
                                                       const char *interface_name)
    {
        return return_value;
    }
    virtual nsapi_value_or_error_t getaddrinfo_async(const char *hostname, SocketAddress *hints, hostbyname_cb_t callback, const char *interface_name)
    {
        return return_value;
    }
    virtual nsapi_error_t gethostbyname_async_cancel(int id)
    {
        return return_value;
    }

protected:
    virtual nsapi_error_t socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto)
    {
        if (return_value == NSAPI_ERROR_OK && return_values.front() == NSAPI_ERROR_OK) {
            // Make sure a non-NULL value is returned if error is not expected
            *handle = reinterpret_cast<nsapi_socket_t *>(1234);
        } else {
            *handle = NULL;
        }
        return return_value;
    };
    virtual nsapi_error_t socket_close(nsapi_socket_t handle)
    {
        return return_value;
    };
    virtual nsapi_error_t socket_bind(nsapi_socket_t handle, const SocketAddress &address)
    {
        return return_value;
    };
    virtual nsapi_error_t socket_listen(nsapi_socket_t handle, int backlog)
    {
        return return_value;
    };
    virtual nsapi_error_t socket_connect(nsapi_socket_t handle, const SocketAddress &address)
    {
        if (!return_values.empty()) {
            nsapi_error_t ret = return_values.front();
            return_values.pop_front();
            return ret;
        }
        return return_value;
    };
    virtual nsapi_error_t socket_accept(nsapi_socket_t server,
                                        nsapi_socket_t *handle, SocketAddress *address = 0)
    {
        if (return_value == NSAPI_ERROR_OK && return_values.front() == NSAPI_ERROR_OK) {
            // Make sure a non-NULL value is returned if error is not expected
            *handle = reinterpret_cast<nsapi_socket_t *>(1234);
        } else {
            *handle = NULL;
        }
        return return_value;
    };
    virtual nsapi_size_or_error_t socket_send(nsapi_socket_t handle,
                                              const void *data, nsapi_size_t size)
    {
        if (!return_values.empty()) {
            nsapi_error_t ret = return_values.front();
            return_values.pop_front();
            return ret;
        }
        return return_value;
    };
    virtual nsapi_size_or_error_t socket_recv(nsapi_socket_t handle,
                                              void *data, nsapi_size_t size)
    {
        if (!return_values.empty()) {
            nsapi_error_t ret = return_values.front();
            return_values.pop_front();
            return ret;
        }
        return return_value;
    };
    virtual nsapi_size_or_error_t socket_sendto(nsapi_socket_t handle, const SocketAddress &address,
                                                const void *data, nsapi_size_t size)
    {
        return return_value;
    };
    virtual nsapi_size_or_error_t socket_sendmsg(nsapi_socket_t handle, const SocketAddress &address,
                                                 const void *data, nsapi_size_t size,
                                                 nsapi_msghdr_t *control, nsapi_size_t control_size)
    {
        return return_value;
    };
    virtual nsapi_size_or_error_t socket_recvfrom(nsapi_socket_t handle, SocketAddress *address,
                                                  void *buffer, nsapi_size_t size)
    {
        if (return_socketAddress != SocketAddress()) {
            *address = return_socketAddress;
        }
        if (!return_values.empty()) {
            nsapi_error_t ret = return_values.front();
            return_values.pop_front();
            return ret;
        }
        return return_value;
    };
    virtual nsapi_size_or_error_t socket_recvmsg(nsapi_socket_t handle, SocketAddress *address,
                                                 void *data, nsapi_size_t size,
                                                 nsapi_msghdr_t *control, nsapi_size_t control_size)
    {
        return return_value;
    };
    virtual void socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data) {};

private:
    virtual nsapi_error_t call_in(int delay, mbed::Callback<void()> func)
    {
        return return_value;
    }
};

#endif // NETWORKSTACKSTUB_H
