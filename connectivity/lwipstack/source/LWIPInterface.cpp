/* Copyright (c) 2017 ARM Limited
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

#define __STDC_LIMIT_MACROS

#include "nsapi.h"
#include "mbed_interface.h"
#include "mbed_assert.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <new>
#include <stdint.h>

#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/tcpip.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "lwip/mld6.h"
#include "lwip/dns.h"
#include "lwip/udp.h"

#include "LWIPStack.h"
#include "lwip_tools.h"

LWIP::Interface *LWIP::Interface::list;

NetworkInterface *LWIP::Interface::network_if_from_netif_id(int id)
{
    for (Interface *interface = list; interface; interface = interface->next) {
        if (id == netif_get_index(&interface->netif)) {
            return interface->user_network_interface;
        }
    }
    return NULL;
}

int LWIP::Interface::netif_id_from_network_if(NetworkInterface *userInterface)
{
    if (userInterface != NULL) {
        for (Interface *interface = list; interface; interface = interface->next) {
            if (userInterface == interface->user_network_interface) {
                return netif_get_index(&interface->netif);
            }
        }
    }
    return 0;
}

LWIP::Interface *LWIP::Interface::our_if_from_netif(struct netif *netif)
{
    for (Interface *interface = list; interface; interface = interface->next) {
        if (netif == &interface->netif) {
            return interface;
        }
    }

    return NULL;
}

static void add_dns_addr_to_dns_list_index(const u8_t addr_type, const u8_t index, struct netif *netif)
{
#if LWIP_IPV6
    if (addr_type == IPADDR_TYPE_V6) {
        /* 2001:4860:4860::8888 google */
        ip_addr_t ipv6_dns_addr = IPADDR6_INIT(
                                      PP_HTONL(0x20014860UL),
                                      PP_HTONL(0x48600000UL),
                                      PP_HTONL(0x00000000UL),
                                      PP_HTONL(0x00008888UL));
        dns_setserver(index, &ipv6_dns_addr, netif);
    }
#endif
#if LWIP_IPV4
    if (addr_type == IPADDR_TYPE_V4) {
        /* 8.8.8.8 google */
        ip_addr_t ipv4_dns_addr = IPADDR4_INIT(0x08080808);
        dns_setserver(index, &ipv4_dns_addr, netif);
    }
#endif
}

static int get_ip_addr_type(const ip_addr_t *ip_addr)
{
#if LWIP_IPV6
    if (IP_IS_V6(ip_addr)) {
        return IPADDR_TYPE_V6;
    }
#endif
#if LWIP_IPV4
    if (IP_IS_V4(ip_addr)) {
        return IPADDR_TYPE_V4;
    }
#endif
#if LWIP_IPV6 && LWIP_IPV4
    return IPADDR_TYPE_ANY;
#endif
}

void LWIP::add_dns_addr(struct netif *lwip_netif, const char *interface_name)
{
    // Check for existing dns address
    for (char numdns = 0; numdns < DNS_MAX_SERVERS; numdns++) {
        const ip_addr_t *dns_ip_addr = dns_getserver(numdns, interface_name);
        if (!ip_addr_isany(dns_ip_addr)) {
            return;
        }
    }

    // Get preferred ip version
    const ip_addr_t *ip_addr = get_ip_addr(false, lwip_netif);
    u8_t addr_type = IPADDR_TYPE_ANY;

    // Add preferred ip version dns address to index 0
    if (ip_addr) {
        addr_type = get_ip_addr_type(ip_addr);
        add_dns_addr_to_dns_list_index(addr_type, 0, lwip_netif);
    }

#if LWIP_IPV4 && LWIP_IPV6
    if (!ip_addr) {
        // Get address for any ip version
        ip_addr = get_ip_addr(true, lwip_netif);
        if (!ip_addr) {
            return;
        }
        addr_type = get_ip_addr_type(ip_addr);
        // Add the dns address to index 0
        add_dns_addr_to_dns_list_index(addr_type, 0, lwip_netif);
    }

    if (addr_type == IPADDR_TYPE_V4) {
        // If ipv4 is preferred and ipv6 is available add ipv6 dns address to index 1
        ip_addr = get_ipv6_addr(lwip_netif);
    } else if (addr_type == IPADDR_TYPE_V6) {
        // If ipv6 is preferred and ipv4 is available add ipv4 dns address to index 1
        ip_addr = get_ipv4_addr(lwip_netif);
    } else {
        ip_addr = NULL;
    }

    if (ip_addr) {
        addr_type = get_ip_addr_type(ip_addr);
        add_dns_addr_to_dns_list_index(addr_type, 1, lwip_netif);
    }
#endif
}

nsapi_error_t LWIP::Interface::set_dhcp()
{
    netif_set_up(&netif);

#if LWIP_DHCP
    if (dhcp_has_to_be_set) {
        err_t err = dhcp_start(&netif);
        dhcp_has_to_be_set = false;
        if (err) {
            connected = NSAPI_STATUS_DISCONNECTED;
            if (client_callback) {
                client_callback(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, NSAPI_STATUS_DISCONNECTED);
            }
            return NSAPI_ERROR_DHCP_FAILURE;
        }
        dhcp_started = true;
    }
#endif

    return NSAPI_ERROR_OK;
}

void LWIP::Interface::netif_link_irq(struct netif *netif)
{
    LWIP::Interface *interface = our_if_from_netif(netif);
    nsapi_connection_status_t connectedStatusPrev = interface->connected;

    if (netif_is_link_up(&interface->netif) && interface->connected == NSAPI_STATUS_CONNECTING) {
        nsapi_error_t dhcp_status = interface->set_dhcp();

        if (interface->blocking && dhcp_status == NSAPI_ERROR_OK) {
            osSemaphoreRelease(interface->linked);
        } else if (dhcp_status != NSAPI_ERROR_OK) {
            netif_set_down(&interface->netif);
        }
    } else {
        osSemaphoreRelease(interface->unlinked);
        if (netif_is_up(&interface->netif)) {
            interface->connected = NSAPI_STATUS_CONNECTING;
        }
        netif_set_down(&interface->netif);
    }

    if (interface->client_callback && connectedStatusPrev != interface->connected
            && interface->connected != NSAPI_STATUS_GLOBAL_UP /* advertised by netif_status_irq */
            && interface->connected != NSAPI_STATUS_DISCONNECTED) { /* advertised by bring_down */
        interface->client_callback(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, interface->connected);
    }
}

void LWIP::Interface::netif_status_irq(struct netif *netif)
{
    LWIP::Interface *interface = our_if_from_netif(netif);
    nsapi_connection_status_t connectedStatusPrev = interface->connected;

    if (netif_is_up(&interface->netif) && netif_is_link_up(&interface->netif)) {
        bool dns_addr_has_to_be_added = false;
        if (!(interface->has_addr_state & HAS_ANY_ADDR) && LWIP::get_ip_addr(true, netif)) {
            if (interface->blocking) {
                osSemaphoreRelease(interface->has_any_addr);
            }
            interface->has_addr_state |= HAS_ANY_ADDR;
            dns_addr_has_to_be_added = true;
        }
#if PREF_ADDR_TIMEOUT
        if (!(interface->has_addr_state & HAS_PREF_ADDR) && LWIP::get_ip_addr(false, netif)) {
            if (interface->blocking) {
                osSemaphoreRelease(interface->has_pref_addr);
            }
            interface->has_addr_state |= HAS_PREF_ADDR;
            dns_addr_has_to_be_added = true;
        }
#endif
#if BOTH_ADDR_TIMEOUT
        if (!(interface->has_addr_state & HAS_BOTH_ADDR) && LWIP::get_ipv4_addr(netif) && LWIP::get_ipv6_addr(netif)) {
            if (interface->blocking) {
                osSemaphoreRelease(interface->has_both_addr);
            }
            interface->has_addr_state |= HAS_BOTH_ADDR;
            dns_addr_has_to_be_added = true;
        }
#endif
        if (dns_addr_has_to_be_added && !interface->blocking) {
            add_dns_addr(&interface->netif, interface->get_interface_name(interface->_interface_name));
        }

        if (interface->has_addr_state & HAS_ANY_ADDR) {
            interface->connected = NSAPI_STATUS_GLOBAL_UP;
#if LWIP_IPV6
            if (ip_addr_islinklocal(get_ipv6_addr(netif))) {
                interface->connected = NSAPI_STATUS_LOCAL_UP;
            }
#endif
        }

    } else if (!netif_is_up(&interface->netif) && netif_is_link_up(&interface->netif)) {
        interface->connected = NSAPI_STATUS_DISCONNECTED;
    }

    if (interface->client_callback && (connectedStatusPrev != interface->connected)
            && interface->connected != NSAPI_STATUS_DISCONNECTED) { /* advertised by bring_down */
        interface->client_callback(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, interface->connected);
    }
}

void LWIP::Interface::attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    client_callback = status_cb;
}

nsapi_connection_status_t LWIP::Interface::get_connection_status() const
{
    return connected;
}

#if LWIP_IPV6
static void mbed_lwip_clear_ipv6_addresses(struct netif *netif)
{
    for (u8_t i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        netif_ip6_addr_set_state(netif, i, IP6_ADDR_INVALID);
    }
}
#endif

char *LWIP::Interface::get_mac_address(char *buf, nsapi_size_t buflen)
{
    (void) snprintf(buf, buflen, "%02x:%02x:%02x:%02x:%02x:%02x",
                    netif.hwaddr[0], netif.hwaddr[1], netif.hwaddr[2],
                    netif.hwaddr[3], netif.hwaddr[4], netif.hwaddr[5]);
    return buf;
}

char *LWIP::Interface::get_interface_name(char *buf)
{
    sprintf(buf, "%c%c%d", netif.name[0], netif.name[1], netif.num);
    return buf;
}

nsapi_error_t LWIP::Interface::get_ipv6_link_local_address(SocketAddress *address)
{
#if LWIP_IPV6
    const ip_addr_t *addr = LWIP::get_ipv6_link_local_addr(&netif);
    nsapi_addr_t out;
    bool ret;

    if (!addr) {
        return NSAPI_ERROR_PARAMETER;
    }

    ret = convert_lwip_addr_to_mbed(&out, addr);
    if (ret != true) {
        return NSAPI_ERROR_PARAMETER;
    }

    address->set_addr(out);

    return NSAPI_ERROR_OK;
#else
    return NSAPI_ERROR_UNSUPPORTED;
#endif
}

nsapi_error_t LWIP::Interface::get_ip_address(SocketAddress *address)
{
    if (!address) {
        return NSAPI_ERROR_PARAMETER;
    }
    const ip_addr_t *addr = LWIP::get_ip_addr(true, &netif);
    if (!addr) {
        return NSAPI_ERROR_NO_ADDRESS;
    }
#if LWIP_IPV6
    if (IP_IS_V6(addr)) {
        char buf[NSAPI_IPv6_SIZE];
        address->set_ip_address(ip6addr_ntoa_r(ip_2_ip6(addr), buf, NSAPI_IPv6_SIZE));
        return NSAPI_ERROR_OK;
    }
#endif
#if LWIP_IPV4
    if (IP_IS_V4(addr)) {
        char buf[NSAPI_IPv4_SIZE];
        address->set_ip_address(ip4addr_ntoa_r(ip_2_ip4(addr), buf, NSAPI_IPv4_SIZE));
        return NSAPI_ERROR_OK;
    }
#endif
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t LWIP::Interface::get_netmask(SocketAddress *address)
{
    if (!address) {
        return NSAPI_ERROR_PARAMETER;
    }
#if LWIP_IPV4
    const ip4_addr_t *addr = netif_ip4_netmask(&netif);
    if (!ip4_addr_isany(addr)) {
        char buf[NSAPI_IPv4_SIZE];
        address->set_ip_address(ip4addr_ntoa_r(addr, buf, NSAPI_IPv4_SIZE));
        return NSAPI_ERROR_OK;
    } else {
        return NSAPI_ERROR_NO_ADDRESS;
    }
#else
    return NSAPI_ERROR_UNSUPPORTED;
#endif
}

nsapi_error_t LWIP::Interface::get_gateway(SocketAddress *address)
{
    if (!address) {
        return NSAPI_ERROR_PARAMETER;
    }
#if LWIP_IPV4
    const ip4_addr_t *addr = netif_ip4_gw(&netif);
    if (!ip4_addr_isany(addr)) {
        char buf[NSAPI_IPv4_SIZE];
        address->set_ip_address(ip4addr_ntoa_r(addr, buf, NSAPI_IPv4_SIZE));
        return NSAPI_ERROR_OK;
    } else {
        return NSAPI_ERROR_NO_ADDRESS;
    }
#else
    return NSAPI_ERROR_UNSUPPORTED;
#endif
}

LWIP::Interface::Interface() :
    hw(NULL), has_addr_state(0),
    connected(NSAPI_STATUS_DISCONNECTED),
    dhcp_started(false), dhcp_has_to_be_set(false), blocking(true), ppp_enabled(false)
{
    memset(&netif, 0, sizeof netif);

    osSemaphoreAttr_t attr;
    attr.name = NULL;
    attr.attr_bits = 0;

    attr.cb_mem = &linked_sem;
    attr.cb_size = sizeof linked_sem;
    linked = osSemaphoreNew(UINT16_MAX, 0, &attr);

    attr.cb_mem = &unlinked_sem;
    attr.cb_size = sizeof unlinked_sem;
    unlinked = osSemaphoreNew(UINT16_MAX, 0, &attr);

    attr.cb_mem = &has_any_addr_sem;
    attr.cb_size = sizeof has_any_addr_sem;
    has_any_addr = osSemaphoreNew(UINT16_MAX, 0, &attr);

    attr.cb_mem = &remove_interface_sem;
    attr.cb_size = sizeof remove_interface_sem;
    remove_interface = osSemaphoreNew(UINT16_MAX, 0, &attr);
#if PREF_ADDR_TIMEOUT
    attr.cb_mem = &has_pref_addr_sem;
    attr.cb_size = sizeof has_pref_addr_sem;
    has_pref_addr = osSemaphoreNew(UINT16_MAX, 0, &attr);
#endif
#if BOTH_ADDR_TIMEOUT
    attr.cb_mem = &has_both_addr_sem;
    attr.cb_size = sizeof has_both_addr_sem;
    has_both_addr = osSemaphoreNew(UINT16_MAX, 0, &attr);
#endif

    next = list;
    list = this;
}

nsapi_error_t LWIP::add_ethernet_interface(EMAC &emac, bool default_if, OnboardNetworkStack::Interface **interface_out, NetworkInterface *user_network_interface)
{
#if LWIP_ETHERNET
    Interface *interface = new (std::nothrow) Interface();
    if (!interface) {
        return NSAPI_ERROR_NO_MEMORY;
    }
    interface->emac = &emac;
    interface->memory_manager = &memory_manager;
    interface->ppp_enabled = false;

#if (MBED_MAC_ADDRESS_SUM != MBED_MAC_ADDR_INTERFACE)
    netif->interface.hwaddr[0] = MBED_MAC_ADDR_0;
    netif->interface.hwaddr[1] = MBED_MAC_ADDR_1;
    netif->interface.hwaddr[2] = MBED_MAC_ADDR_2;
    netif->interface.hwaddr[3] = MBED_MAC_ADDR_3;
    netif->interface.hwaddr[4] = MBED_MAC_ADDR_4;
    netif->interface.hwaddr[5] = MBED_MAC_ADDR_5;
#else
    mbed_mac_address((char *) interface->netif.hwaddr);
#endif

    interface->netif.hwaddr_len = 6;
    interface->user_network_interface = user_network_interface;
    if (!netif_add(&interface->netif,
#if LWIP_IPV4
                   0, 0, 0,
#endif
                   interface, &LWIP::Interface::emac_if_init, tcpip_input)) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    if (default_if) {
        netif_set_default(&interface->netif);
        default_interface = interface;
    }

    netif_set_link_callback(&interface->netif, &LWIP::Interface::netif_link_irq);
    netif_set_status_callback(&interface->netif, &LWIP::Interface::netif_status_irq);

    *interface_out = interface;

    /* Use mac address as additional seed to random number generator */
    uint64_t seed = interface->netif.hwaddr[0];
    for (uint8_t i = 1; i < 8; i++) {
        seed <<= 8;
        seed |= interface->netif.hwaddr[i % 6];
    }
    lwip_add_random_seed(seed);

    return NSAPI_ERROR_OK;
#else
    return NSAPI_ERROR_UNSUPPORTED;
#endif //LWIP_ETHERNET
}

void LWIP::Interface::delete_interface(OnboardNetworkStack::Interface **interface_out)
{
#if LWIP_ETHERNET
    if ((interface_out != NULL) && (*interface_out != NULL)) {
        LWIP::Interface *lwip = static_cast<Interface *>(*interface_out);
        LWIP::Interface *node = lwip->list;

        if (lwip->list != NULL) {
            if (lwip->list == lwip) {

                lwip->list = lwip->list->next;
                netif_remove(&node->netif);
                *interface_out = NULL;
                delete node;
            } else {
                while (node->next != NULL && node->next != lwip) {
                    node = node->next;
                }
                if (node->next != NULL && node->next == lwip) {
                    Interface *remove = node->next;
                    node->next = node->next->next;
                    netif_remove(&remove->netif);
                    *interface_out = NULL;
                    delete remove;
                }
            }
        }
        osSemaphoreRelease(lwip->remove_interface);
    }
#endif
}

nsapi_error_t LWIP::remove_ethernet_interface(OnboardNetworkStack::Interface **interface_out)
{
#if LWIP_ETHERNET
    LWIP::Interface *lwip = static_cast<Interface *>(*interface_out);
    tcpip_callback_with_block((tcpip_callback_fn)&LWIP::Interface::delete_interface, interface_out, 1);
    osSemaphoreAcquire(lwip->remove_interface, osWaitForever);
    return NSAPI_ERROR_OK;
#else
    return NSAPI_ERROR_UNSUPPORTED;
#endif //LWIP_ETHERNET
}

nsapi_error_t LWIP::add_l3ip_interface(L3IP &l3ip, bool default_if, OnboardNetworkStack::Interface **interface_out)
{
#if LWIP_L3IP
    Interface *interface = new (std::nothrow) Interface();
    if (!interface) {
        return NSAPI_ERROR_NO_MEMORY;
    }
    interface->l3ip = &l3ip;
    interface->memory_manager = &memory_manager;
    interface->ppp_enabled = false;



    // interface->netif.hwaddr_len = 0; should we set?

    if (!netif_add(&interface->netif,
#if LWIP_IPV4
                   0, 0, 0,
#endif
                   interface, &LWIP::Interface::l3ip_if_init, ip_input)) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    if (default_if) {
        netif_set_default(&interface->netif);
        default_interface = interface;
    }

    netif_set_link_callback(&interface->netif, &LWIP::Interface::netif_link_irq);
    netif_set_status_callback(&interface->netif, &LWIP::Interface::netif_status_irq);

    *interface_out = interface;


    //lwip_add_random_seed(seed); to do?

    return NSAPI_ERROR_OK;

#else
    return NSAPI_ERROR_UNSUPPORTED;

#endif //LWIP_L3IP
}

nsapi_error_t LWIP::remove_l3ip_interface(OnboardNetworkStack::Interface **interface_out)
{
#if LWIP_L3IP
    if ((interface_out != NULL) && (*interface_out != NULL)) {

        Interface *lwip = static_cast<Interface *>(*interface_out);
        Interface *node = lwip->list;

        if (lwip->list != NULL) {
            if (lwip->list == lwip) {
                lwip->list = lwip->list->next;
                netif_remove(&node->netif);
                delete node;
            } else {
                while (node->next != NULL && node->next != lwip) {
                    node = node->next;
                }
                if (node->next != NULL && node->next == lwip) {
                    Interface *remove = node->next;
                    node->next = node->next->next;
                    remove->l3ip->power_down();
                    netif_remove(&remove->netif);
                    delete remove;
                }
            }
        }
    }

    return NSAPI_ERROR_OK;
#else
    return NSAPI_ERROR_UNSUPPORTED;

#endif //LWIP_L3IP
}


nsapi_error_t LWIP::add_ppp_interface(PPP &ppp, bool default_if, OnboardNetworkStack::Interface **interface_out)
{
#if PPP_SUPPORT
    Interface *interface = new (std::nothrow) Interface();
    if (!interface) {
        return NSAPI_ERROR_NO_MEMORY;
    }
    interface->ppp = &ppp;
    interface->memory_manager = &memory_manager;
    interface->ppp_enabled = true;

    // interface->netif.hwaddr_len = 0; should we set?

    if (!netif_add(&interface->netif,
#if LWIP_IPV4
                   0, 0, 0,
#endif
                   interface, &LWIP::Interface::ppp_if_init, tcpip_input)) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    if (default_if) {
        netif_set_default(&interface->netif);
        default_interface = interface;
    }

    netif_set_link_callback(&interface->netif, &LWIP::Interface::netif_link_irq);
    netif_set_status_callback(&interface->netif, &LWIP::Interface::netif_status_irq);

    *interface_out = interface;

    //lwip_add_random_seed(seed); to do?

    return NSAPI_ERROR_OK;

#else
    return NSAPI_ERROR_UNSUPPORTED;

#endif //PPP_SUPPORT
}

nsapi_error_t LWIP::remove_ppp_interface(OnboardNetworkStack::Interface **interface_out)
{
#if PPP_SUPPORT
    if ((interface_out != NULL) && (*interface_out != NULL)) {

        Interface *lwip = static_cast<Interface *>(*interface_out);
        Interface *node = lwip->list;

        if (lwip->list != NULL) {
            if (lwip->list == lwip) {
                // Power down PPP service
                lwip->ppp->power_down();
                if (netif_is_link_up(&lwip->netif)) {
                    // Wait PPP service to report link down
                    osSemaphoreAcquire(lwip->unlinked, osWaitForever);
                }
                netif_remove(&node->netif);
                lwip->list = lwip->list->next;
                delete node;
            } else {
                while (node->next != NULL && node->next != lwip) {
                    node = node->next;
                }
                if (node->next != NULL && node->next == lwip) {
                    Interface *remove = node->next;
                    // Power down PPP service
                    remove->ppp->power_down();
                    if (netif_is_link_up(&lwip->netif)) {
                        // Wait PPP service to report link down
                        osSemaphoreAcquire(lwip->unlinked, osWaitForever);
                    }
                    netif_remove(&remove->netif);
                    node->next = node->next->next;
                    delete remove;
                }
            }
        }
    }

    return NSAPI_ERROR_OK;
#else
    return NSAPI_ERROR_UNSUPPORTED;

#endif //PPP_SUPPORT
}

void LWIP::set_default_interface(OnboardNetworkStack::Interface *interface)
{
    if (interface) {
        default_interface = static_cast<LWIP::Interface *>(interface);
        netif_set_default(&default_interface->netif);
    }
}

nsapi_error_t LWIP::Interface::bringup(bool dhcp, const char *ip, const char *netmask, const char *gw, const nsapi_ip_stack_t stack, bool block)
{
    // Check if we've already connected
    if (connected == NSAPI_STATUS_GLOBAL_UP) {
        return NSAPI_ERROR_IS_CONNECTED;
    } else if (connected == NSAPI_STATUS_CONNECTING) {
        return NSAPI_ERROR_BUSY;
    }

    connected = NSAPI_STATUS_CONNECTING;
    blocking = block;

#if LWIP_DHCP
    if (stack != IPV6_STACK && dhcp) {
        dhcp_has_to_be_set = true;
    }
#endif

#if LWIP_IPV6
    if (stack != IPV4_STACK) {
        if (netif.hwaddr_len == 6) {
            netif_create_ip6_linklocal_address(&netif, 1/*from MAC*/);
        }
#if LWIP_IPV6_MLD
        /*
         * For hardware/netifs that implement MAC filtering.
         * All-nodes link-local is handled by default, so we must let the hardware know
         * to allow multicast packets in.
         * Should set mld_mac_filter previously. */
        if (netif.mld_mac_filter != NULL) {
            ip6_addr_t ip6_allnodes_ll;
            ip6_addr_set_allnodes_linklocal(&ip6_allnodes_ll);
            netif.mld_mac_filter(&netif, &ip6_allnodes_ll, NETIF_ADD_MAC_FILTER);
        }
#endif /* LWIP_IPV6_MLD */

#if LWIP_IPV6_AUTOCONFIG
        /* IPv6 address autoconfiguration not enabled by default */
        netif.ip6_autoconfig_enabled = 1;
#endif /* LWIP_IPV6_AUTOCONFIG */
    } else {
        // Disable rourter solicitations
        netif.rs_count = 0;
    }
#endif /* LWIP_IPV6 */

    nsapi_error_t ret_add_ip = NSAPI_ERROR_OK;
    if (!dhcp && !ppp_enabled) {
        ret_add_ip = set_ip_address(ip, netmask, gw);
        if (ret_add_ip != NSAPI_ERROR_OK) {
            return ret_add_ip;
        }
    }

    if (client_callback) {
        client_callback(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, NSAPI_STATUS_CONNECTING);
    }


    if (!netif_is_link_up(&netif)) {
        if (blocking) {
            if (osSemaphoreAcquire(linked, LINK_TIMEOUT * 1000) != osOK) {
                return NSAPI_ERROR_NO_CONNECTION;
            }
        }
    } else {
        nsapi_error_t ret = set_dhcp();
        if (ret != NSAPI_ERROR_OK) {
            return ret;
        }
    }

    if (!blocking) {
        // Done enough - as addresses are acquired, there will be
        // connected callbacks.
        return NSAPI_ERROR_OK;
    }

    // If doesn't have address
    if (!LWIP::get_ip_addr(true, &netif)) {
        if (osSemaphoreAcquire(has_any_addr, DHCP_TIMEOUT * 1000) != osOK) {
            return NSAPI_ERROR_DHCP_FAILURE;
        }
    }

#if PREF_ADDR_TIMEOUT
    if (stack != IPV4_STACK && stack != IPV6_STACK) {
        // If address is not for preferred stack waits a while to see
        // if preferred stack address is acquired
        if (!LWIP::get_ip_addr(false, &netif)) {
            osSemaphoreAcquire(has_pref_addr, PREF_ADDR_TIMEOUT * 1000);
        }
    }
#endif
#if BOTH_ADDR_TIMEOUT
    if (stack != IPV4_STACK && stack != IPV6_STACK) {
        // If addresses for both stacks are not available waits a while to
        // see if address for both stacks are acquired
        if (!(LWIP::get_ipv4_addr(&netif) && LWIP::get_ipv6_addr(&netif))) {
            osSemaphoreAcquire(has_both_addr, BOTH_ADDR_TIMEOUT * 1000);
        }
    }
#endif

    add_dns_addr(&netif, get_interface_name(_interface_name));

    return NSAPI_ERROR_OK;
}

nsapi_error_t LWIP::Interface::set_ip_address(const char *ip,
                                              const char *netmask,
                                              const char *gw,
                                              uint8_t ipv6_flag)
{
    if (!ip) {
        return NSAPI_ERROR_PARAMETER;
    }

    int conv_ip = 1;

#if LWIP_IPV4
    ip4_addr_t ip_addr;
    ip4_addr_t netmask_addr;
    ip4_addr_t gw_addr;
    IP4_ADDR(&netmask_addr, 255, 255, 255, 255);
    ip4_addr_set_zero(&gw_addr);
    int conv_netmask = 1;
    int conv_gw = 1;

    conv_ip = inet_aton(ip, &ip_addr);
    if (netmask) {
        conv_netmask = inet_aton(netmask, &netmask_addr);
    }
    if (gw) {
        conv_gw = inet_aton(gw, &gw_addr);
    }
    if (conv_ip && conv_netmask && conv_gw) {
        netif_set_addr(&netif, &ip_addr, &netmask_addr, &gw_addr);
        return NSAPI_ERROR_OK;
    }
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
    ip6_addr_t ip_addr6;

    conv_ip = inet6_aton(ip, &ip_addr6);
    if (conv_ip) {
        s8_t chosen_idx;
        err_t ret = netif_add_ip6_address(&netif, &ip_addr6, &chosen_idx);
        // If failed here, consider increasing LWIP_IPV6_NUM_ADDRESSES.
        MBED_ASSERT(chosen_idx >= 0);
        if (!ret) {
            netif_ip6_addr_set_state(&netif, chosen_idx, ipv6_flag);
            return NSAPI_ERROR_OK;
        }
    }
#endif /* LWIP_IPV6 */

    return NSAPI_ERROR_PARAMETER;
}

nsapi_error_t LWIP::Interface::bringdown()
{
    // Check if we've connected
    if (connected == NSAPI_STATUS_DISCONNECTED) {
        return NSAPI_ERROR_NO_CONNECTION;
    }

#if LWIP_DHCP
    // Disconnect from the network
    if (dhcp_started) {
        dhcp_release(&netif);
        dhcp_stop(&netif);
        dhcp_started = false;
        dhcp_has_to_be_set = false;
    }
#endif

    netif_set_down(&netif);

#if LWIP_IPV6
    mbed_lwip_clear_ipv6_addresses(&netif);
#endif
#if LWIP_IPV4
    ip_addr_set_zero(&(netif.ip_addr));
    ip_addr_set_zero(&(netif.netmask));
    ip_addr_set_zero(&(netif.gw));
#endif

    osSemaphoreDelete(has_any_addr);
    osSemaphoreAttr_t attr;
    attr.name = NULL;
    attr.attr_bits = 0;
    attr.cb_mem = &has_any_addr_sem;
    attr.cb_size = sizeof has_any_addr_sem;
    has_any_addr = osSemaphoreNew(UINT16_MAX, 0, &attr);
#if PREF_ADDR_TIMEOUT
    osSemaphoreDelete(has_pref_addr);
    attr.cb_mem = &has_pref_addr_sem;
    attr.cb_size = sizeof has_pref_addr_sem;
    has_pref_addr = osSemaphoreNew(UINT16_MAX, 0, &attr);
#endif
#if BOTH_ADDR_TIMEOUT
    osSemaphoreDelete(has_both_addr);
    attr.cb_mem = &has_both_addr_sem;
    attr.cb_size = sizeof has_both_addr_sem;
    has_both_addr = osSemaphoreNew(UINT16_MAX, 0, &attr);
#endif
    has_addr_state = 0;

    connected = NSAPI_STATUS_DISCONNECTED;
    if (client_callback) {
        client_callback(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, connected);
    }
    return 0;
}
