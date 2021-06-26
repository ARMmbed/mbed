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

#ifndef QUECTEL_BG96_CELLULARSTACK_H_
#define QUECTEL_BG96_CELLULARSTACK_H_

#include "AT_CellularStack.h"

namespace mbed {

#define BG96_CREATE_SOCKET_TIMEOUT 150s
#define BG96_CLOSE_SOCKET_TIMEOUT 20s // TCP socket max timeout is >10sec
#define BG96_MAX_RECV_SIZE 1500
#define BG96_MAX_SEND_SIZE 1460
#define BG96_SOCKET_BIND_FAIL 556

typedef enum {
    URC_RECV,
    URC_CLOSED,
} urc_type_t;

class QUECTEL_BG96_CellularStack : public AT_CellularStack {
public:
    QUECTEL_BG96_CellularStack(ATHandler &atHandler, int cid, nsapi_ip_stack_t stack_type, AT_CellularDevice &device);
    virtual ~QUECTEL_BG96_CellularStack();

protected: // NetworkStack

    virtual nsapi_error_t socket_listen(nsapi_socket_t handle, int backlog);

    virtual nsapi_error_t socket_accept(nsapi_socket_t server,
                                        nsapi_socket_t *handle, SocketAddress *address = 0);

    virtual nsapi_error_t socket_connect(nsapi_socket_t handle, const SocketAddress &address);

    virtual nsapi_error_t get_ip_address(SocketAddress *address);

#ifdef MBED_CONF_CELLULAR_OFFLOAD_DNS_QUERIES
    virtual nsapi_error_t gethostbyname(const char *host, SocketAddress *address, nsapi_version_t version, const char *interface_name);
    virtual nsapi_value_or_error_t gethostbyname_async(const char *host, hostbyname_cb_t callback, nsapi_version_t version = NSAPI_UNSPEC,
                                                       const char *interface_name = NULL);
    virtual nsapi_error_t gethostbyname_async_cancel(int id);
#endif

#if defined(MBED_CONF_NSAPI_OFFLOAD_TLSSOCKET) && (MBED_CONF_NSAPI_OFFLOAD_TLSSOCKET)
    virtual nsapi_error_t setsockopt(nsapi_socket_t handle, int level,
                                     int optname, const void *optval, unsigned optlen);
#endif

protected: // AT_CellularStack

    virtual nsapi_error_t socket_close_impl(int sock_id);

    virtual nsapi_error_t create_socket_impl(CellularSocket *socket);

    virtual nsapi_size_or_error_t socket_sendto_impl(CellularSocket *socket, const SocketAddress &address,
                                                     const void *data, nsapi_size_t size);

    virtual nsapi_size_or_error_t socket_recvfrom_impl(CellularSocket *socket, SocketAddress *address,
                                                       void *buffer, nsapi_size_t size);

private:
    // URC handler
    void urc_qiurc(urc_type_t urc_type);
    // URC handler for socket data being received
    void urc_qiurc_recv();
    // URC handler for socket being closed
    void urc_qiurc_closed();

    void handle_open_socket_response(int &modem_connect_id, int &err, bool tlssocket);

#if defined(MBED_CONF_NSAPI_OFFLOAD_TLSSOCKET) && (MBED_CONF_NSAPI_OFFLOAD_TLSSOCKET)
    nsapi_error_t set_to_modem_impl(const char *filename, const char *config, const char *data, size_t size);
#endif

#ifdef MBED_CONF_CELLULAR_OFFLOAD_DNS_QUERIES
    // URC handler for DNS query
    void urc_qiurc_dnsgip();
    // read DNS query result
    bool read_dnsgip(SocketAddress &address, nsapi_version_t _dns_version);
    hostbyname_cb_t _dns_callback;
    nsapi_version_t _dns_version;
#endif

    uint8_t _tls_sec_level;

    /** Convert IP address to dotted string representation
     *
     *  BG96 requires consecutive zeros so can't use get_ip_address or ip6tos directly.
     *
     *  @param ip address
     *  @param dot buffer with size NSAPI_IPv6, where address is written zero terminated
     */
    void ip2dot(const SocketAddress &ip, char *dot);
};
} // namespace mbed
#endif /* QUECTEL_BG96_CELLULARSTACK_H_ */
