/*
 * Copyright (c) 2018, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#if defined(MBED_CONF_RTOS_PRESENT)

#include "unity.h"

#include "EMACMemoryManager.h"
#include "emac_TestNetworkStack.h"
#include "emac_initialize.h"

EmacTestNetworkStack::EmacTestNetworkStack()
    : m_interface(NULL)
{

}

nsapi_error_t EmacTestNetworkStack::gethostbyname(const char *host, SocketAddress *address, nsapi_version_t version)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t EmacTestNetworkStack::add_dns_server(const SocketAddress &address)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t EmacTestNetworkStack::call_in(int delay, mbed::Callback<void()> func)
{
    // Implemented as empty to save memory
    return NSAPI_ERROR_DEVICE_ERROR;
}

EmacTestNetworkStack::call_in_callback_cb_t EmacTestNetworkStack::get_call_in_callback()
{
    call_in_callback_cb_t cb(this, &EmacTestNetworkStack::call_in);
    return cb;
}

nsapi_error_t EmacTestNetworkStack::socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t EmacTestNetworkStack::socket_close(nsapi_socket_t handle)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t EmacTestNetworkStack::socket_bind(nsapi_socket_t handle, const SocketAddress &address)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t EmacTestNetworkStack::socket_listen(nsapi_socket_t handle, int backlog)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t EmacTestNetworkStack::socket_connect(nsapi_socket_t handle, const SocketAddress &address)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t EmacTestNetworkStack::socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address)
{
    return NSAPI_ERROR_OK;
}

nsapi_size_or_error_t EmacTestNetworkStack::socket_send(nsapi_socket_t handle, const void *data, nsapi_size_t size)
{
    return NSAPI_ERROR_OK;
}

nsapi_size_or_error_t EmacTestNetworkStack::socket_recv(nsapi_socket_t handle, void *data, nsapi_size_t size)
{
    return NSAPI_ERROR_OK;
}

nsapi_size_or_error_t EmacTestNetworkStack::socket_sendto(nsapi_socket_t handle, const SocketAddress &address, const void *data, nsapi_size_t size)
{
    return size;
}

nsapi_size_or_error_t EmacTestNetworkStack::socket_recvfrom(nsapi_socket_t handle, SocketAddress *address, void *data, nsapi_size_t size)
{
    return NSAPI_ERROR_DEVICE_ERROR;
}

nsapi_error_t EmacTestNetworkStack::setsockopt(nsapi_socket_t handle, int level, int optname, const void *optval, unsigned optlen)
{
    return NSAPI_ERROR_OK;
}

nsapi_error_t EmacTestNetworkStack::getsockopt(nsapi_socket_t handle, int level, int optname, void *optval, unsigned *optlen)
{
    return NSAPI_ERROR_OK;
}

void EmacTestNetworkStack::socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data)
{

}

nsapi_error_t EmacTestNetworkStack::add_ethernet_interface(EMAC &emac, bool default_if, OnboardNetworkStack::Interface **interface_out, NetworkInterface *user_network_interface)
{
    // Test network stack supports only one interface
    TEST_ASSERT_MESSAGE(!m_interface, "Only one interface supported!");

    m_interface = &EmacTestNetworkStack::Interface::get_instance();
    TEST_ASSERT_MESSAGE(m_interface, "Invalid interface!");

    m_interface->m_emac = &emac;

    EmacTestMemoryManager *memory_manager = &EmacTestMemoryManager::get_instance();
    emac.set_memory_manager(*memory_manager);

    *interface_out = m_interface;

    return NSAPI_ERROR_OK;
}

EmacTestNetworkStack::Interface::Interface()
    : m_emac(NULL)
{

}


void EmacTestNetworkStack::Interface::attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{

}

nsapi_connection_status_t EmacTestNetworkStack::Interface::get_connection_status() const
{
    return NSAPI_STATUS_ERROR_UNSUPPORTED;
}

char *EmacTestNetworkStack::Interface::get_mac_address(char *buf, nsapi_size_t buflen)
{
    return NULL;
}

nsapi_error_t EmacTestNetworkStack::Interface::set_mac_address(uint8_t *buf, nsapi_size_t buflen)
{
    return NSAPI_STATUS_ERROR_UNSUPPORTED;
}

nsapi_error_t EmacTestNetworkStack::Interface::get_ip_address(SocketAddress *address)
{
    return NSAPI_ERROR_OK;
}

char *EmacTestNetworkStack::Interface::get_ip_address(char *buf, nsapi_size_t buflen)
{
    return NULL;
}

nsapi_error_t EmacTestNetworkStack::Interface::get_netmask(SocketAddress *address)
{
    return NSAPI_ERROR_OK;
}

char *EmacTestNetworkStack::Interface::get_netmask(char *buf, nsapi_size_t buflen)
{
    return NULL;
}

nsapi_error_t EmacTestNetworkStack::Interface::get_gateway(SocketAddress *address)
{
    return NSAPI_ERROR_OK;
}

char *EmacTestNetworkStack::Interface::get_gateway(char *buf, nsapi_size_t buflen)
{
    return NULL;
}

nsapi_error_t EmacTestNetworkStack::Interface::bringup(bool dhcp, const char *ip, const char *netmask, const char *gw, const nsapi_ip_stack_t stack, bool blocking)
{
    if (!emac_if_init(m_emac)) {
        TEST_ASSERT_MESSAGE(0, "emac initialization failed!");
    }

    return NSAPI_ERROR_OK;
}

nsapi_error_t EmacTestNetworkStack::Interface::bringdown()
{
    return NSAPI_ERROR_OK;
}

EmacTestNetworkStack::Interface &EmacTestNetworkStack::Interface::get_instance()
{
    static EmacTestNetworkStack::Interface test_interface;
    return test_interface;
}

EmacTestNetworkStack &EmacTestNetworkStack::get_instance()
{
    static EmacTestNetworkStack test_stack;
    return test_stack;
}

#define TEST 0x33254234
#if MBED_CONF_NSAPI_DEFAULT_STACK == TEST
#undef TEST
OnboardNetworkStack &OnboardNetworkStack::get_default_instance()
{
    return EmacTestNetworkStack::get_instance();
}
#endif // defined(MBED_CONF_RTOS_PRESENT)
#endif
