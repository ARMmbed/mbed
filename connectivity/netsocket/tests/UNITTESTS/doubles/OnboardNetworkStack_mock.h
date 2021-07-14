/*
 * Copyright (c) 2019, Arm Limited and affiliates.
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

#ifndef UNITTESTS_STUBS_ONBOARDNETWORKSTACK_MOCK_H_
#define UNITTESTS_STUBS_ONBOARDNETWORKSTACK_MOCK_H_

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "netsocket/OnboardNetworkStack.h"

/*
 * Note: If all you need is a simple stub with basic return value control
 * consider using NetworkStack_stub.h and its NetworkStackstub class.
 */

class OnboardNetworkStackMock : public OnboardNetworkStack {
public:
    MOCK_METHOD3(gethostbyname, nsapi_error_t(const char *host, SocketAddress *address, nsapi_version_t version));
    MOCK_METHOD1(add_dns_server, nsapi_error_t(const SocketAddress &address));
    MOCK_METHOD3(get_dns_server, nsapi_error_t(int index, SocketAddress *address, const char *interface_name));
    MOCK_METHOD2(call_in, nsapi_error_t(int delay, mbed::Callback<void()> func));
    MOCK_METHOD2(socket_open, nsapi_error_t(nsapi_socket_t *handle, nsapi_protocol_t proto));
    MOCK_METHOD1(socket_close, nsapi_error_t(nsapi_socket_t handle));
    MOCK_METHOD2(socket_bind, nsapi_error_t(nsapi_socket_t handle, const SocketAddress &address));
    MOCK_METHOD2(socket_listen, nsapi_error_t(nsapi_socket_t handle, int backlog));
    MOCK_METHOD2(socket_connect, nsapi_error_t(nsapi_socket_t handle, const SocketAddress &address));
    MOCK_METHOD3(socket_accept, nsapi_error_t(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address));
    MOCK_METHOD3(socket_send, nsapi_error_t(nsapi_socket_t handle, const void *data, nsapi_size_t size));
    MOCK_METHOD3(socket_recv, nsapi_error_t(nsapi_socket_t handle, void *data, nsapi_size_t size));
    MOCK_METHOD4(socket_sendto, nsapi_error_t(nsapi_socket_t handle, const SocketAddress &address, const void *data, nsapi_size_t size));
    MOCK_METHOD4(socket_recvfrom, nsapi_error_t(nsapi_socket_t handle, SocketAddress *address, void *data, nsapi_size_t size));
    MOCK_METHOD5(setsockopt, nsapi_error_t(nsapi_socket_t handle, int level, int optname, const void *optval, unsigned optlen));
    MOCK_METHOD5(getsockopt, nsapi_error_t(nsapi_socket_t handle, int level, int optname, const void *optval, unsigned *optlen));
    MOCK_METHOD6(socket_sendmsg, nsapi_error_t(nsapi_socket_t handle, SocketAddress *address, void *data, nsapi_size_t size, nsapi_msghdr_t *control, nsapi_size_t control_size));
    MOCK_METHOD6(socket_recvmsg, nsapi_error_t(nsapi_socket_t handle, SocketAddress *address, void *data, nsapi_size_t size, nsapi_msghdr_t *control, nsapi_size_t control_size));
//    MOCK_METHOD3(socket_attach, void(nsapi_socket_t handle, void (*callback)(void *), void *data));
    MOCK_METHOD3(add_ethernet_interface, nsapi_error_t(EMAC &emac, bool default_if, OnboardNetworkStack::Interface **interface_out));
    MOCK_METHOD3(add_ppp_interface, nsapi_error_t(PPP &ppp, bool default_if, OnboardNetworkStack::Interface **interface_out));
    MOCK_METHOD1(set_default_interface, void (OnboardNetworkStack::Interface *interface));
    void *socket_cb;
    // No need to mock socket_attach really.
    void socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data)
    {
        socket_cb = data;
    };


    static OnboardNetworkStackMock &get_instance()
    {
        static OnboardNetworkStackMock stackMock1;
        return stackMock1;
    }

    class InterfaceMock : public OnboardNetworkStack::Interface {
    public:

        static InterfaceMock &get_instance()
        {
            static InterfaceMock test_interface;
            return test_interface;
        }
        MOCK_METHOD6(bringup, nsapi_error_t(bool dhcp, const char *ip,
                                            const char *netmask, const char *gw,
                                            nsapi_ip_stack_t stack,
                                            bool blocking
                                           ));
        MOCK_METHOD0(bringdown, nsapi_error_t());
        MOCK_METHOD1(attach, void(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb));
        MOCK_CONST_METHOD0(get_connection_status, nsapi_connection_status_t());
        MOCK_METHOD1(get_interface_name, char *(char *buf));
        MOCK_METHOD2(get_mac_address, char *(char *buf, nsapi_size_t buflen));
        MOCK_METHOD1(get_ip_address, nsapi_error_t (SocketAddress *address));
        MOCK_METHOD1(get_ipv6_link_local_address, nsapi_error_t(SocketAddress *address));
        MOCK_METHOD2(get_netmask, char *(char *buf, nsapi_size_t buflen));
        MOCK_METHOD1(get_netmask, nsapi_error_t (SocketAddress *address));
        MOCK_METHOD2(get_gateway, char *(char *buf, nsapi_size_t buflen));
        MOCK_METHOD1(get_gateway, nsapi_error_t (SocketAddress *address));
    };
};


#endif /* UNITTESTS_STUBS_ONBOARDNETWORKSTACK_MOCK_H_ */
