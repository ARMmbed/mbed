/* LWIP implementation of NSAPI NetworkStack
 * Copyright (c) 2017 ARM Limited
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
#include "nsapi.h"
#include "mbed_interface.h"
#include "mbed_assert.h"
#include "Semaphore.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/tcpip.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "lwip/mld6.h"
#include "lwip/igmp.h"
#include "lwip/dns.h"
#include "lwip/udp.h"
#include "lwip/raw.h"
#include "lwip/netif.h"
#include "lwip/lwip_errno.h"
#include "lwip-sys/arch/sys_arch.h"

#include "LWIPStack.h"
#include "lwip_tools.h"

#ifndef LWIP_SOCKET_MAX_MEMBERSHIPS
#define LWIP_SOCKET_MAX_MEMBERSHIPS 4
#endif

void LWIP::socket_callback(struct netconn *nc, enum netconn_evt eh, u16_t len)
{
    // Filter send minus events
    if (eh == NETCONN_EVT_SENDMINUS && nc->state == NETCONN_WRITE) {
        return;
    }

    LWIP &lwip = LWIP::get_instance();
    lwip.adaptation.lock();

    if (eh == NETCONN_EVT_RCVPLUS && nc->state == NETCONN_NONE) {
        lwip._event_flag.set(TCP_CLOSED_FLAG);
    }

    for (int i = 0; i < MEMP_NUM_NETCONN; i++) {
        if (lwip.arena[i].in_use
                && lwip.arena[i].conn == nc
                && lwip.arena[i].cb) {
            lwip.arena[i].cb(lwip.arena[i].data);
        }
    }

    lwip.adaptation.unlock();
}

void LWIP::tcpip_init_irq(void *eh)
{
    static_cast<rtos::Semaphore *>(eh)->release();
    sys_tcpip_thread_set();
}

/* LWIP network stack implementation */
LWIP::LWIP()
{
    default_interface = NULL;
    tcpip_thread_id = NULL;

    // Seed lwip random
    lwip_seed_random();

    // Initialise TCP sequence number
    uint32_t tcp_isn_secret[4];
    for (int i = 0; i < 4; i++) {
        tcp_isn_secret[i] = LWIP_RAND();
    }
    lwip_init_tcp_isn(0, (u8_t *) &tcp_isn_secret);

    rtos::Semaphore tcpip_inited;

    tcpip_init(&LWIP::tcpip_init_irq, &tcpip_inited);
    tcpip_inited.acquire();

    // Zero out socket set
    arena_init();
}

nsapi_error_t LWIP::get_dns_server(int index, SocketAddress *address, const char *interface_name)
{
    int dns_entries = 0;

    for (int i = 0; i < DNS_MAX_SERVERS; i++) {
        const ip_addr_t *ip_addr = dns_getserver(i, interface_name);
        if (!ip_addr_isany(ip_addr)) {
            if (index == dns_entries) {
                nsapi_addr_t addr;
                convert_lwip_addr_to_mbed(&addr, ip_addr);
                address->set_addr(addr);
                return NSAPI_ERROR_OK;
            }
            dns_entries++;
        }
    }
    return NSAPI_ERROR_NO_ADDRESS;
}

nsapi_error_t LWIP::add_dns_server(const SocketAddress &address, const char *interface_name)
{
    int index;
    nsapi_addr_t addr = address.get_addr();
    const ip_addr_t *ip_addr_move;
    ip_addr_t ip_addr;

    if (!convert_mbed_addr_to_lwip(&ip_addr, &addr)) {
        return NSAPI_ERROR_PARAMETER;
    }

    if (ip_addr_isany(&ip_addr)) {
        return NSAPI_ERROR_NO_ADDRESS;
    }

    if (interface_name == NULL) {
        for (index = DNS_MAX_SERVERS - 1; index > 0; index--) {
            ip_addr_move = dns_getserver(index - 1, NULL);
            if (!ip_addr_isany(ip_addr_move)) {
                dns_setserver(index, ip_addr_move, NULL);
            }
        }
        dns_setserver(0, &ip_addr, NULL);
    } else {
        for (index = DNS_MAX_SERVERS - 1; index > 0; index--) {
            ip_addr_move = dns_get_interface_server(index - 1, interface_name);
            if (!ip_addr_isany(ip_addr_move)) {
                dns_add_interface_server(index, interface_name, ip_addr_move);
            }
        }
        dns_add_interface_server(0, interface_name, &ip_addr);
    }
    return NSAPI_ERROR_OK;
}

void LWIP::tcpip_thread_callback(void *ptr)
{
    lwip_callback *cb = static_cast<lwip_callback *>(ptr);

    if (cb->delay) {
        sys_timeout(cb->delay, LWIP::tcpip_thread_callback, ptr);
        cb->delay = 0;
    } else {
        cb->callback();
        delete cb;
    }
}

nsapi_error_t LWIP::call_in(int delay, mbed::Callback<void()> func)
{
    lwip_callback *cb = new (std::nothrow) lwip_callback;
    if (!cb) {
        return NSAPI_ERROR_NO_MEMORY;
    }

    cb->delay = delay;
    cb->callback = func;

    if (tcpip_callback_with_block(LWIP::tcpip_thread_callback, cb, 1) != ERR_OK) {
        return NSAPI_ERROR_NO_MEMORY;
    }

    return NSAPI_ERROR_OK;
}

LWIP::call_in_callback_cb_t LWIP::get_call_in_callback()
{
    call_in_callback_cb_t cb(this, &LWIP::call_in);
    return cb;
}

nsapi_error_t LWIP::get_ip_address(SocketAddress *address)
{
    if (!default_interface) {
        return NSAPI_ERROR_NO_ADDRESS;
    }

    return get_ip_address_if(address, nullptr);
}

nsapi_error_t LWIP::get_ip_address_if(SocketAddress *address, const char *interface_name)
{
    if (!address) {
        return NSAPI_ERROR_PARAMETER;
    }

    const ip_addr_t *addr;

    if (interface_name == NULL) {
        addr = get_ip_addr(true, &default_interface->netif);
    } else {
        addr = get_ip_addr(true, netif_find(interface_name));
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

nsapi_error_t LWIP::socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto)
{
    // check if network is connected
//    if (lwip_connected == NSAPI_STATUS_DISCONNECTED) {
//        return NSAPI_ERROR_NO_CONNECTION;
//    }

    // allocate a socket
    struct mbed_lwip_socket *s = arena_alloc();
    if (!s) {
        return NSAPI_ERROR_NO_SOCKET;
    }

    enum netconn_type netconntype;
    if (proto == NSAPI_TCP) {
        netconntype = NETCONN_TCP;
    } else if (proto == NSAPI_UDP) {
        netconntype = NETCONN_UDP;
    } else if (proto == NSAPI_ICMP) {
        netconntype = NETCONN_RAW;
    } else {
        return NSAPI_ERROR_UNSUPPORTED;
    }

#if LWIP_IPV6
    // Enable IPv6 (or dual-stack)
    netconntype = (enum netconn_type)(netconntype | NETCONN_TYPE_IPV6);
#endif

    if (proto == NSAPI_ICMP) {
        s->conn = netconn_new_with_proto_and_callback(NETCONN_RAW,
                                                      (u8_t)IP_PROTO_ICMP, &LWIP::socket_callback);
    } else {
        s->conn = netconn_new_with_callback(netconntype, &LWIP::socket_callback);
    }

    if (!s->conn) {
        arena_dealloc(s);
        return NSAPI_ERROR_NO_SOCKET;
    }

    netconn_set_nonblocking(s->conn, true);
    *(struct mbed_lwip_socket **)handle = s;
    return 0;
}

nsapi_error_t LWIP::socket_close(nsapi_socket_t handle)
{
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)handle;
#if LWIP_TCP
    /* Check if TCP FSM is in ESTABLISHED state.
     * Then give extra time for connection close handshaking  until TIME_WAIT state.
     * The purpose is to prevent eth/wifi driver stop and  FIN ACK corrupt.
     * This may happend if network interface disconnect follows immediately after socket_close.*/
    if (NETCONNTYPE_GROUP(s->conn->type) == NETCONN_TCP && s->conn->pcb.tcp->state == ESTABLISHED) {
        _event_flag.clear(TCP_CLOSED_FLAG);
        netconn_shutdown(s->conn, false, true);
        _event_flag.wait_any(TCP_CLOSED_FLAG, TCP_CLOSE_TIMEOUT);
    }
#endif
    if (s->buf) {
        pbuf_free(s->buf);
    }
    err_t err = netconn_delete(s->conn);
    arena_dealloc(s);
    return err_remap(err);
}

nsapi_error_t LWIP::socket_bind(nsapi_socket_t handle, const SocketAddress &address)
{
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)handle;
    ip_addr_t ip_addr;

    if (
#if LWIP_TCP
        (NETCONNTYPE_GROUP(s->conn->type) == NETCONN_TCP && s->conn->pcb.tcp->local_port != 0) ||
#endif
        (NETCONNTYPE_GROUP(s->conn->type) == NETCONN_UDP && s->conn->pcb.udp->local_port != 0)) {
        return NSAPI_ERROR_PARAMETER;
    }

    nsapi_addr_t addr = address.get_addr();
    if (!convert_mbed_addr_to_lwip(&ip_addr, &addr)) {
        return NSAPI_ERROR_PARAMETER;
    }

    if (!ip_addr_isany_val(ip_addr) && !is_local_addr(&ip_addr)) {
        return NSAPI_ERROR_PARAMETER;
    }

    err_t err = netconn_bind(s->conn, &ip_addr, address.get_port());
    return err_remap(err);
}

nsapi_error_t LWIP::socket_listen(nsapi_socket_t handle, int backlog)
{
#if LWIP_TCP
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)handle;

    if (s->conn->pcb.tcp->local_port == 0) {
        return NSAPI_ERROR_PARAMETER;
    }

    err_t err = netconn_listen_with_backlog(s->conn, backlog);
    return err_remap(err);
#else
    return NSAPI_ERROR_UNSUPPORTED;
#endif
}

nsapi_error_t LWIP::socket_connect(nsapi_socket_t handle, const SocketAddress &address)
{
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)handle;
    ip_addr_t ip_addr;

    nsapi_addr_t addr = address.get_addr();
    if (!convert_mbed_addr_to_lwip(&ip_addr, &addr)) {
        return NSAPI_ERROR_PARAMETER;
    }

    netconn_set_nonblocking(s->conn, false);
    err_t err = netconn_connect(s->conn, &ip_addr, address.get_port());
    netconn_set_nonblocking(s->conn, true);

    return err_remap(err);
}

nsapi_error_t LWIP::socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address)
{
#if LWIP_TCP
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)server;
    struct mbed_lwip_socket *ns = arena_alloc();
    if (!ns) {
        return NSAPI_ERROR_NO_SOCKET;
    }

    if (s->conn->pcb.tcp->state != LISTEN) {
        return NSAPI_ERROR_PARAMETER;
    }

    err_t err = netconn_accept(s->conn, &ns->conn);
    if (err != ERR_OK) {
        arena_dealloc(ns);
        return err_remap(err);
    }

    netconn_set_nonblocking(ns->conn, true);
    *(struct mbed_lwip_socket **)handle = ns;

    ip_addr_t peer_addr;
    nsapi_addr_t addr;
    u16_t port;
    (void) netconn_peer(ns->conn, &peer_addr, &port);
    convert_lwip_addr_to_mbed(&addr, &peer_addr);

    if (address) {
        address->set_addr(addr);
        address->set_port(port);
    }

    return 0;
#else
    return NSAPI_ERROR_UNSUPPORTED;
#endif
}

nsapi_size_or_error_t LWIP::socket_send(nsapi_socket_t handle, const void *data, nsapi_size_t size)
{
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)handle;
    size_t bytes_written = 0;

    err_t err = netconn_write_partly(s->conn, data, size, NETCONN_COPY, &bytes_written);
    if (err != ERR_OK) {
        return err_remap(err);
    }

    return (nsapi_size_or_error_t)bytes_written;
}

nsapi_size_or_error_t LWIP::socket_recv(nsapi_socket_t handle, void *data, nsapi_size_t size)
{
#if LWIP_TCP
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)handle;

    if (!s->buf) {
        err_t err = netconn_recv_tcp_pbuf(s->conn, &s->buf);
        s->offset = 0;

        if (err != ERR_OK) {
            return err_remap(err);
        }
    }

    u16_t recv = pbuf_copy_partial(s->buf, data, (u16_t)size, s->offset);
    s->offset += recv;

    if (s->offset >= s->buf->tot_len) {
        pbuf_free(s->buf);
        s->buf = 0;
    }

    return recv;
#else
    return NSAPI_ERROR_UNSUPPORTED;
#endif
}

nsapi_size_or_error_t LWIP::socket_sendto(nsapi_socket_t handle, const SocketAddress &address, const void *data, nsapi_size_t size)
{
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)handle;
    ip_addr_t ip_addr;

    nsapi_addr_t addr = address.get_addr();
    if (!convert_mbed_addr_to_lwip(&ip_addr, &addr)) {
        return NSAPI_ERROR_PARAMETER;
    }
    struct netif *netif_ = netif_get_by_index(s->conn->pcb.ip->netif_idx);
    if (!netif_) {
        netif_ = &default_interface->netif;
    }
    if (netif_) {
        if ((addr.version == NSAPI_IPv4 && !get_ipv4_addr(netif_)) ||
                (addr.version == NSAPI_IPv6 && !get_ipv6_addr(netif_) && !get_ipv6_link_local_addr(netif_))) {
            return NSAPI_ERROR_PARAMETER;
        }
    }
    struct netbuf *buf = netbuf_new();

    err_t err = netbuf_ref(buf, data, (u16_t)size);
    if (err != ERR_OK) {
        netbuf_free(buf);
        return err_remap(err);
    }

    err = netconn_sendto(s->conn, buf, &ip_addr, address.get_port());
    netbuf_delete(buf);
    if (err != ERR_OK) {
        return err_remap(err);
    }

    return size;
}

nsapi_size_or_error_t LWIP::socket_recvfrom(nsapi_socket_t handle, SocketAddress *address, void *data, nsapi_size_t size)
{
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)handle;
    struct netbuf *buf;

    err_t err = netconn_recv(s->conn, &buf);
    if (err != ERR_OK) {
        return err_remap(err);
    }

    if (address) {
        nsapi_addr_t addr;
        convert_lwip_addr_to_mbed(&addr, netbuf_fromaddr(buf));
        address->set_addr(addr);
        address->set_port(netbuf_fromport(buf));
    }

    u16_t recv = netbuf_copy(buf, data, (u16_t)size);
    netbuf_delete(buf);

    return recv;
}

int32_t LWIP::find_multicast_member(const struct mbed_lwip_socket *s, const nsapi_ip_mreq_t *imr)
{
    uint32_t count = 0;
    uint32_t index = 0;
    // Set upper limit on while loop, should break out when the membership pair is found
    while (count < s->multicast_memberships_count) {
        index = next_registered_multicast_member(s, index);

        if (memcmp(&s->multicast_memberships[index].imr_multiaddr, &imr->imr_multiaddr, sizeof(nsapi_addr_t)) == 0 &&
                memcmp(&s->multicast_memberships[index].imr_interface, &imr->imr_interface, sizeof(nsapi_addr_t)) == 0) {
            return index;
        }
        count++;
        index++;
    }

    return -1;
}

nsapi_error_t LWIP::setsockopt(nsapi_socket_t handle, int level, int optname, const void *optval, unsigned optlen)
{
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)handle;

    switch (optname) {
        case NSAPI_BIND_TO_DEVICE:
            if (optlen > NSAPI_INTERFACE_NAME_MAX_SIZE) {
                return NSAPI_ERROR_UNSUPPORTED;
            }

            netconn_bind_if(s->conn,  netif_name_to_index((const char *)optval));

            return 0;
#if LWIP_TCP
        case NSAPI_KEEPALIVE:
            if (optlen != sizeof(int) || NETCONNTYPE_GROUP(s->conn->type) != NETCONN_TCP) {
                return NSAPI_ERROR_UNSUPPORTED;
            }

            s->conn->pcb.tcp->so_options |= SOF_KEEPALIVE;
            return 0;

        case NSAPI_KEEPIDLE:
            if (optlen != sizeof(int) || NETCONNTYPE_GROUP(s->conn->type) != NETCONN_TCP) {
                return NSAPI_ERROR_UNSUPPORTED;
            }

            s->conn->pcb.tcp->keep_idle = *(int *)optval;
            return 0;

        case NSAPI_KEEPINTVL:
            if (optlen != sizeof(int) || NETCONNTYPE_GROUP(s->conn->type) != NETCONN_TCP) {
                return NSAPI_ERROR_UNSUPPORTED;
            }

            s->conn->pcb.tcp->keep_intvl = *(int *)optval;
            return 0;
#endif

        case NSAPI_REUSEADDR:
            if (optlen != sizeof(int)) {
                return NSAPI_ERROR_UNSUPPORTED;
            }

            if (*(int *)optval) {
                ip_set_option(s->conn->pcb.ip, SOF_REUSEADDR);
            } else {
                ip_reset_option(s->conn->pcb.ip, SOF_REUSEADDR);
            }
            return 0;

        case NSAPI_BROADCAST:
            if (NETCONNTYPE_GROUP(s->conn->type) != NETCONN_UDP) {
                return NSAPI_ERROR_UNSUPPORTED;
            }

            if (optlen != sizeof(int)) {
                return NSAPI_ERROR_UNSUPPORTED;
            }

            if (*(const int *)optval) {
                ip_set_option(s->conn->pcb.ip, SOF_BROADCAST);
            } else {
                ip_reset_option(s->conn->pcb.ip, SOF_BROADCAST);
            }
            return 0;

        case NSAPI_ADD_MEMBERSHIP:
        case NSAPI_DROP_MEMBERSHIP: {
            if (optlen != sizeof(nsapi_ip_mreq_t)) {
                return NSAPI_ERROR_PARAMETER;
            }
            err_t igmp_err;
            const nsapi_ip_mreq_t *imr = static_cast<const nsapi_ip_mreq_t *>(optval);

            /* Check interface address type matches group, or is unspecified */
            if (imr->imr_interface.version != NSAPI_UNSPEC && imr->imr_interface.version != imr->imr_multiaddr.version) {
                return NSAPI_ERROR_PARAMETER;
            }

            ip_addr_t if_addr;
            ip_addr_t multi_addr;

            /* Convert the group address */
            if (!convert_mbed_addr_to_lwip(&multi_addr, &imr->imr_multiaddr)) {
                return NSAPI_ERROR_PARAMETER;
            }

            if (imr->imr_interface.version != NSAPI_UNSPEC) {
                /* Convert the interface address */
                if (!convert_mbed_addr_to_lwip(&if_addr, &imr->imr_interface)) {
                    return NSAPI_ERROR_PARAMETER;
                }
            } else {
                /* Set interface address to "any", matching the group address type */
                ip_addr_set_any(IP_IS_V6(&multi_addr), &if_addr);
            }

            igmp_err = ERR_USE; // Maps to NSAPI_ERROR_UNSUPPORTED
            int32_t member_pair_index = find_multicast_member(s, imr);

            if (optname == NSAPI_ADD_MEMBERSHIP) {
                if (!s->multicast_memberships) {
                    // First multicast join on this socket, allocate space for membership tracking
                    s->multicast_memberships = (nsapi_ip_mreq_t *)malloc(sizeof(nsapi_ip_mreq_t) * LWIP_SOCKET_MAX_MEMBERSHIPS);
                    if (!s->multicast_memberships) {
                        return NSAPI_ERROR_NO_MEMORY;
                    }
                } else if (s->multicast_memberships_count == LWIP_SOCKET_MAX_MEMBERSHIPS) {
                    return NSAPI_ERROR_NO_MEMORY;
                }

                if (member_pair_index != -1) {
                    return NSAPI_ERROR_ADDRESS_IN_USE;
                }

                member_pair_index = next_free_multicast_member(s, 0);

                adaptation.lock();

#if LWIP_IPV4
                if (IP_IS_V4(&if_addr)) {
                    igmp_err = igmp_joingroup(ip_2_ip4(&if_addr), ip_2_ip4(&multi_addr));
                }
#endif
#if LWIP_IPV6
                if (IP_IS_V6(&if_addr)) {
                    igmp_err = mld6_joingroup(ip_2_ip6(&if_addr), ip_2_ip6(&multi_addr));
                }
#endif

                adaptation.unlock();

                if (igmp_err == ERR_OK) {
                    set_multicast_member_registry_bit(s, member_pair_index);
                    s->multicast_memberships[member_pair_index] = *imr;
                    s->multicast_memberships_count++;
                }
            } else {
                if (member_pair_index == -1) {
                    return NSAPI_ERROR_NO_ADDRESS;
                }

                clear_multicast_member_registry_bit(s, member_pair_index);
                s->multicast_memberships_count--;

                adaptation.lock();

#if LWIP_IPV4
                if (IP_IS_V4(&if_addr)) {
                    igmp_err = igmp_leavegroup(ip_2_ip4(&if_addr), ip_2_ip4(&multi_addr));
                }
#endif
#if LWIP_IPV6
                if (IP_IS_V6(&if_addr)) {
                    igmp_err = mld6_leavegroup(ip_2_ip6(&if_addr), ip_2_ip6(&multi_addr));
                }
#endif

                adaptation.unlock();
            }

            return err_remap(igmp_err);
        }

        case NSAPI_IPTOS:
            if (optlen != sizeof(u8_t)) {
                return NSAPI_ERROR_UNSUPPORTED;
            }
            s->conn->pcb.ip->tos = (u8_t)(*(const int *)optval);
            return 0;
        default:
            return NSAPI_ERROR_UNSUPPORTED;
    }
}

nsapi_error_t LWIP::getsockopt(nsapi_socket_t handle, int level, int optname, void *optval, unsigned *optlen)
{
    return NSAPI_ERROR_UNSUPPORTED;
}


void LWIP::socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data)
{
    struct mbed_lwip_socket *s = (struct mbed_lwip_socket *)handle;

    s->cb = callback;
    s->data = data;
}

LWIP &LWIP::get_instance()
{
    static LWIP lwip;
    return lwip;
}

// This works as long as it's not ever set to something which corresponds to
// a macro defined as a non-integer. Eg `#define Nanostack "Foo"`
#define LWIP 0x11991199
#if MBED_CONF_NSAPI_DEFAULT_STACK == LWIP
#undef LWIP
OnboardNetworkStack &OnboardNetworkStack::get_default_instance()
{
    return LWIP::get_instance();
}
#endif
