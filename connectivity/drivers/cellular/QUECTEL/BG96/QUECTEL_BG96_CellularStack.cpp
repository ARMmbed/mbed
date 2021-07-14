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

#include <stdio.h>
#include <string.h>
#include "QUECTEL/BG96/QUECTEL_BG96_CellularStack.h"
#include "CellularLog.h"
#include "netsocket/TLSSocket.h"
#include "CellularUtil.h"

// Ref: Quectel_BG96_SSL_AT_Commands_Manual, ch 2.1.1 AT+QSSLCFG
static const int BG96_SUPPORTED_SSL_VERSION     = 4; // All
static const char BG96_SUPPORTED_CIPHER_SUITE[] = "0xFFFF"; // Support all

// TODO: At the moment we support only one active SSL context
//       Later can be expanded to support multiple contexts. Modem supports IDs 0-5.
static const int sslctxID = 0;

static const int BG96_ASYNC_DNS_QUERY_ID = 1; // BG96 driver only supports one request, so using id 1

using namespace mbed;
using namespace std::chrono_literals;

QUECTEL_BG96_CellularStack::QUECTEL_BG96_CellularStack(ATHandler &atHandler, int cid, nsapi_ip_stack_t stack_type, AT_CellularDevice &device) :
    AT_CellularStack(atHandler, cid, stack_type, device)
#ifdef MBED_CONF_CELLULAR_OFFLOAD_DNS_QUERIES
#if (MBED_CONF_CELLULAR_OFFLOAD_DNS_QUERIES != 1)
#error Define cellular.offload-dns-queries to null or 1.
#endif
    , _dns_callback(NULL), _dns_version(NSAPI_UNSPEC)
#endif
    , _tls_sec_level(0)
{
    _at.set_urc_handler("+QIURC: \"recv", mbed::Callback<void()>(this, &QUECTEL_BG96_CellularStack::urc_qiurc_recv));
    _at.set_urc_handler("+QIURC: \"close", mbed::Callback<void()>(this, &QUECTEL_BG96_CellularStack::urc_qiurc_closed));
#ifdef MBED_CONF_CELLULAR_OFFLOAD_DNS_QUERIES
    _at.set_urc_handler("+QIURC: \"dnsgip\",", mbed::Callback<void()>(this, &QUECTEL_BG96_CellularStack::urc_qiurc_dnsgip));
#endif

    _at.set_urc_handler("+QSSLURC: \"recv", mbed::Callback<void()>(this, &QUECTEL_BG96_CellularStack::urc_qiurc_recv));
    _at.set_urc_handler("+QSSLURC: \"close", mbed::Callback<void()>(this, &QUECTEL_BG96_CellularStack::urc_qiurc_closed));

    // TODO: this needs to be handled properly, but now making just a quick hack
    // Close all SSL sockets if open. This can happen for example if application processor
    // was reset but modem not. Old sockets are still up and running and it prevents
    // new SSL configurations and creating new sockets.
    for (int i = 0; i < 12; i++) {
        _at.clear_error();
        tr_debug("Closing SSL socket %d...", i);
        _at.at_cmd_discard("+QSSLCLOSE", "=", "%d", i);
    }
    _at.clear_error();
}

QUECTEL_BG96_CellularStack::~QUECTEL_BG96_CellularStack()
{
}

nsapi_error_t QUECTEL_BG96_CellularStack::socket_listen(nsapi_socket_t handle, int backlog)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t QUECTEL_BG96_CellularStack::socket_accept(void *server, void **socket, SocketAddress *addr)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t QUECTEL_BG96_CellularStack::socket_connect(nsapi_socket_t handle, const SocketAddress &address)
{
    CellularSocket *socket = (CellularSocket *)handle;

    int modem_connect_id = -1;
    int err = NSAPI_ERROR_NO_CONNECTION;

    int request_connect_id = find_socket_index(socket);
    // assert here as its a programming error if the socket container doesn't contain
    // specified handle
    MBED_ASSERT(request_connect_id != -1);

    _at.lock();
    if (socket->proto == NSAPI_TCP) {
        if (socket->tls_socket) {
            if (_tls_sec_level == 0) {
                _at.unlock();
                return NSAPI_ERROR_AUTH_FAILURE;
            }

            _at.at_cmd_discard("+QSSLOPEN", "=", "%d%d%d%s%d%d", _cid, sslctxID, request_connect_id,
                               address.get_ip_address(), address.get_port(), 0);
            handle_open_socket_response(modem_connect_id, err, true);

            if ((_at.get_last_error() == NSAPI_ERROR_OK) && err) {
                if (err == BG96_SOCKET_BIND_FAIL) {
                    socket->id = -1;
                    _at.unlock();
                    return NSAPI_ERROR_PARAMETER;
                }
                socket_close_impl(modem_connect_id);

                _at.at_cmd_discard("+QSSLOPEN", "=", "%d%d%d%s%d%d", _cid, sslctxID, request_connect_id,
                                   address.get_ip_address(), address.get_port(), 0);
                handle_open_socket_response(modem_connect_id, err, true);
            }
        } else {
            char ipdot[NSAPI_IP_SIZE];
            ip2dot(address, ipdot);
            _at.at_cmd_discard("+QIOPEN", "=", "%d%d%s%s%d%d%d", _cid, request_connect_id, "TCP",
                               ipdot, address.get_port(), socket->localAddress.get_port(), 0);

            handle_open_socket_response(modem_connect_id, err, false);

            if ((_at.get_last_error() == NSAPI_ERROR_OK) && err) {
                if (err == BG96_SOCKET_BIND_FAIL) {
                    socket->id = -1;
                    _at.unlock();
                    return NSAPI_ERROR_PARAMETER;
                }
                socket_close_impl(modem_connect_id);

                _at.at_cmd_discard("+QIOPEN", "=", "%d%d%s%s%d%d%d", _cid, request_connect_id, "TCP",
                                   ipdot, address.get_port(), socket->localAddress.get_port(), 0);

                handle_open_socket_response(modem_connect_id, err, false);
            }
        }
    }

    // If opened successfully BUT not requested one, close it
    if (!err && (modem_connect_id != request_connect_id)) {
        _at.at_cmd_discard("+QICLOSE", "=", "%d", modem_connect_id);
    }

    nsapi_error_t ret_val = _at.get_last_error();
    _at.unlock();

    if ((!err) && (ret_val == NSAPI_ERROR_OK) && (modem_connect_id == request_connect_id)) {
        socket->id = request_connect_id;
        socket->remoteAddress = address;
        socket->connected = true;
        return NSAPI_ERROR_OK;
    }

    return err;
}

nsapi_error_t QUECTEL_BG96_CellularStack::get_ip_address(SocketAddress *address)
{
    if (!address) {
        return NSAPI_ERROR_PARAMETER;
    }
    _at.lock();

    _at.cmd_start_stop("+QIACT", "?");
    _at.resp_start("+QIACT:");

    int32_t context_type = 0;

    if (_at.info_resp()) {
        _at.skip_param(); // ID
        _at.skip_param(); // State

        context_type = _at.read_int();
        if (context_type == 1) {
            _stack_type = IPV4_STACK;
        } else if (context_type == 2) {
            _stack_type = IPV6_STACK;
        }

        if (_at.read_string(_ip, PDP_IPV6_SIZE) != -1) {
            mbed_cellular_util::convert_ipv6(_ip);
            address->set_ip_address(_ip);
        }
    }
    _at.resp_stop();
    _at.unlock();

    return (context_type > 0) ? NSAPI_ERROR_OK : NSAPI_ERROR_NO_ADDRESS;
}

void QUECTEL_BG96_CellularStack::urc_qiurc_recv()
{
    urc_qiurc(URC_RECV);
}

void QUECTEL_BG96_CellularStack::urc_qiurc_closed()
{
    urc_qiurc(URC_CLOSED);
}

#ifdef MBED_CONF_CELLULAR_OFFLOAD_DNS_QUERIES
bool QUECTEL_BG96_CellularStack::read_dnsgip(SocketAddress &address, nsapi_version_t _dns_version)
{
    if (_at.read_int() == 0) {
        int count = _at.read_int();
        _at.skip_param();
        for (; count > 0; count--) {
            _at.resp_start("+QIURC: \"dnsgip\",");
            char ipAddress[NSAPI_IP_SIZE];
            _at.read_string(ipAddress, sizeof(ipAddress));
            if (address.set_ip_address(ipAddress)) {
                if (_dns_version == NSAPI_UNSPEC || _dns_version == address.get_ip_version()) {
                    return true;
                }
            }
        }
    }
    return false;
}

void QUECTEL_BG96_CellularStack::urc_qiurc_dnsgip()
{
    if (!_dns_callback) {
        return;
    }
    SocketAddress address;
    if (read_dnsgip(address, _dns_version)) {
        _dns_callback(1, &address);
    } else {
        _dns_callback(NSAPI_ERROR_DNS_FAILURE, NULL);
    }
    _dns_callback = NULL;
}
#endif

void QUECTEL_BG96_CellularStack::urc_qiurc(urc_type_t urc_type)
{
    _at.lock();
    _at.skip_param();
    const int sock_id = _at.read_int();
    const nsapi_error_t err = _at.unlock_return_error();

    if (err != NSAPI_ERROR_OK) {
        return;
    }

    CellularSocket *sock = find_socket(sock_id);
    if (sock) {
        if (urc_type == URC_CLOSED) {
            tr_info("Socket closed %d", sock_id);
            sock->closed = true;
        }
        if (sock->_cb) {
            sock->_cb(sock->_data);
        }
    }
}

nsapi_error_t QUECTEL_BG96_CellularStack::socket_close_impl(int sock_id)
{
    _at.set_at_timeout(BG96_CLOSE_SOCKET_TIMEOUT);
    nsapi_error_t err;
    CellularSocket *socket = find_socket(sock_id);
    if (socket && socket->tls_socket) {
        err = _at.at_cmd_discard("+QSSLCLOSE", "=", "%d", sock_id);
        if (err == NSAPI_ERROR_OK) {
            // Disable TLSSocket settings to prevent reuse on next socket without setting the values
            _tls_sec_level = 0;
            err = _at.at_cmd_discard("+QSSLCFG", "=\"seclevel\",", "%d%d", sslctxID, _tls_sec_level);
        }
    } else {
        err = _at.at_cmd_discard("+QICLOSE", "=", "%d", sock_id);
    }
    _at.restore_at_timeout();

    return err;
}

void QUECTEL_BG96_CellularStack::handle_open_socket_response(int &modem_connect_id, int &err, bool tlssocket)
{
    // OK
    // QIOPEN -> should be handled as URC?
    _at.set_at_timeout(BG96_CREATE_SOCKET_TIMEOUT);

    if (tlssocket) {
        _at.resp_start("+QSSLOPEN:");
    } else {
        _at.resp_start("+QIOPEN:");
    }

    _at.restore_at_timeout();
    modem_connect_id = _at.read_int();
    err = _at.read_int();
    if (tlssocket && err != 0) {
        err = NSAPI_ERROR_AUTH_FAILURE;
    }
}

nsapi_error_t QUECTEL_BG96_CellularStack::create_socket_impl(CellularSocket *socket)
{
    int modem_connect_id = -1;
    int remote_port = 0;
    int err = -1;

    int request_connect_id = find_socket_index(socket);
    // assert here as its a programming error if the socket container doesn't contain
    // specified handle
    MBED_ASSERT(request_connect_id != -1);

    if (socket->proto == NSAPI_UDP && !socket->connected) {
        _at.at_cmd_discard("+QIOPEN", "=", "%d%d%s%s%d%d%d", _cid, request_connect_id, "UDP SERVICE",
                           (_ip_ver_sendto == NSAPI_IPv4) ? "127.0.0.1" : "0:0:0:0:0:0:0:1",
                           remote_port, socket->localAddress.get_port(), 0);

        handle_open_socket_response(modem_connect_id, err, false);

        if ((_at.get_last_error() == NSAPI_ERROR_OK) && err) {
            if (err == BG96_SOCKET_BIND_FAIL) {
                socket->id = -1;
                return NSAPI_ERROR_PARAMETER;
            }
            socket_close_impl(modem_connect_id);

            _at.at_cmd_discard("+QIOPEN", "=", "%d%d%s%s%d%d%d", _cid, request_connect_id, "UDP SERVICE",
                               (_ip_ver_sendto == NSAPI_IPv4) ? "127.0.0.1" : "0:0:0:0:0:0:0:1",
                               remote_port, socket->localAddress.get_port(), 0);

            handle_open_socket_response(modem_connect_id, err, false);
        }
    } else if (socket->proto == NSAPI_UDP && socket->connected) {
        char ipdot[NSAPI_IP_SIZE];
        ip2dot(socket->remoteAddress, ipdot);
        _at.at_cmd_discard("+QIOPEN", "=", "%d%d%s%s%d", _cid, request_connect_id, "UDP",
                           ipdot, socket->remoteAddress.get_port());

        handle_open_socket_response(modem_connect_id, err, false);

        if ((_at.get_last_error() == NSAPI_ERROR_OK) && err) {
            if (err == BG96_SOCKET_BIND_FAIL) {
                socket->id = -1;
                return NSAPI_ERROR_PARAMETER;
            }
            socket_close_impl(modem_connect_id);

            _at.at_cmd_discard("+QIOPEN", "=", "%d%d%s%s%d", _cid, request_connect_id, "UDP",
                               ipdot, socket->remoteAddress.get_port());

            handle_open_socket_response(modem_connect_id, err, false);
        }
    }

    // If opened successfully BUT not requested one, close it
    if (!err && (modem_connect_id != request_connect_id)) {
        socket_close_impl(modem_connect_id);
    }

    nsapi_error_t ret_val = _at.get_last_error();

    if (ret_val == NSAPI_ERROR_OK && (modem_connect_id == request_connect_id)) {
        socket->id = request_connect_id;
    }

    return ret_val;
}

nsapi_size_or_error_t QUECTEL_BG96_CellularStack::socket_sendto_impl(CellularSocket *socket, const SocketAddress &address,
                                                                     const void *data, nsapi_size_t size)
{
    if (_ip_ver_sendto != address.get_ip_version()) {
        _ip_ver_sendto =  address.get_ip_version();
        socket_close_impl(socket->id);
        create_socket_impl(socket);
    }

    int sent_len = 0;
    int sent_len_before = 0;
    int sent_len_after = 0;

    if (socket->tls_socket) {
        sent_len_after = size;
    } else {
        // Get the sent count before sending
        _at.at_cmd_int("+QISEND", "=", sent_len_before, "%d%d", socket->id, 0);
    }

    // Send
    if (socket->proto == NSAPI_UDP) {
        if (size > BG96_MAX_SEND_SIZE) {
            return NSAPI_ERROR_PARAMETER;
        }
        char ipdot[NSAPI_IP_SIZE];
        ip2dot(address, ipdot);
        _at.cmd_start_stop("+QISEND", "=", "%d%d%s%d", socket->id, size,
                           ipdot, address.get_port());
        _at.resp_start(">");
        _at.write_bytes((uint8_t *)data, size);
        _at.resp_start();
        _at.set_stop_tag("\r\n");
        // Possible responses are SEND OK, SEND FAIL or ERROR.
        char response[16];
        response[0] = '\0';
        _at.read_string(response, sizeof(response));
        _at.resp_stop();
        if (strcmp(response, "SEND OK") != 0) {
            return NSAPI_ERROR_DEVICE_ERROR;
        }
    } else {
        const char *buf = (const char *)data;
        nsapi_size_t blk = BG96_MAX_SEND_SIZE;
        nsapi_size_t count = size;

        while (count > 0) {
            if (count < blk) {
                blk = count;
            }

            if (socket->tls_socket) {
                _at.cmd_start_stop("+QSSLSEND", "=", "%d%d", socket->id, blk);
            }

            else {
                _at.cmd_start_stop("+QISEND", "=", "%d%d", socket->id, blk);
            }

            _at.resp_start(">");
            _at.write_bytes((uint8_t *)buf, blk);
            _at.resp_start();
            _at.set_stop_tag("\r\n");
            // Possible responses are SEND OK, SEND FAIL or ERROR.
            char response[16];
            response[0] = '\0';
            _at.read_string(response, sizeof(response));
            _at.resp_stop();
            if (strcmp(response, "SEND OK") != 0) {
                return NSAPI_ERROR_DEVICE_ERROR;
            }

            buf += blk;
            count -= blk;
        }
    }

    // Get the sent count after sending
    nsapi_size_or_error_t err = NSAPI_ERROR_OK;

    if (!socket->tls_socket) {
        err = _at.at_cmd_int("+QISEND", "=", sent_len_after, "%d%d", socket->id, 0);
    }

    if (err == NSAPI_ERROR_OK) {
        sent_len = sent_len_after - sent_len_before;
        return sent_len;
    }

    return err;
}

nsapi_size_or_error_t QUECTEL_BG96_CellularStack::socket_recvfrom_impl(CellularSocket *socket, SocketAddress *address,
                                                                       void *buffer, nsapi_size_t size)
{
    nsapi_size_or_error_t recv_len = 0;
    int port = -1;
    char ip_address[NSAPI_IP_SIZE + 1];

    if (socket->proto == NSAPI_TCP) {
        // do not read more than max size
        size = size > BG96_MAX_RECV_SIZE ? BG96_MAX_RECV_SIZE : size;
        if (socket->tls_socket) {
            _at.cmd_start_stop("+QSSLRECV", "=", "%d%d", socket->id, size);
        } else {
            _at.cmd_start_stop("+QIRD", "=", "%d%d", socket->id, size);
        }
    } else {
        _at.cmd_start_stop("+QIRD", "=", "%d", socket->id);
    }

    if (socket->tls_socket) {
        _at.resp_start("+QSSLRECV:");
    } else {
        _at.resp_start("+QIRD:");
    }

    recv_len = _at.read_int();
    if (recv_len > 0) {
        // UDP has remote_IP and remote_port parameters
        if (socket->proto == NSAPI_UDP) {
            _at.read_string(ip_address, sizeof(ip_address));
            port = _at.read_int();
        }
        // do not read more than buffer size
        recv_len = recv_len > (nsapi_size_or_error_t)size ? size : recv_len;
        _at.read_bytes((uint8_t *)buffer, recv_len);
    }
    _at.resp_stop();

    // We block only if 0 recv length really means no data.
    // If 0 is followed by ip address and port can be an UDP 0 length packet
    if (!recv_len && port < 0) {
        return NSAPI_ERROR_WOULD_BLOCK;
    }

    if (address) {
        address->set_ip_address(ip_address);
        address->set_port(port);
    }

    return recv_len;
}

#ifdef MBED_CONF_CELLULAR_OFFLOAD_DNS_QUERIES
nsapi_error_t QUECTEL_BG96_CellularStack::gethostbyname(const char *host, SocketAddress *address,
                                                        nsapi_version_t version, const char *interface_name)
{
    (void) interface_name;
    MBED_ASSERT(host);
    MBED_ASSERT(address);

    _at.lock();

    if (_dns_callback) {
        _at.unlock();
        return NSAPI_ERROR_BUSY;
    }

    if (!address->set_ip_address(host)) {
        _at.set_at_timeout(1min); // from BG96_TCP/IP_AT_Commands_Manual_V1.0
        _at.at_cmd_discard("+QIDNSGIP", "=", "%d%s", _cid, host);
        _at.resp_start("+QIURC: \"dnsgip\",");
        _at.restore_at_timeout();
        if (!read_dnsgip(*address, version)) {
            _at.unlock();
            return NSAPI_ERROR_DNS_FAILURE;
        }
    }

    return _at.unlock_return_error();
}

nsapi_value_or_error_t QUECTEL_BG96_CellularStack::gethostbyname_async(const char *host, hostbyname_cb_t callback,
                                                                       nsapi_version_t version, const char *interface_name)
{
    (void) interface_name;
    MBED_ASSERT(host);
    MBED_ASSERT(callback);

    _at.lock();

    if (_dns_callback) {
        _at.unlock();
        return NSAPI_ERROR_BUSY;
    }

    _at.at_cmd_discard("+QIDNSGIP", "=", "%d%s", _cid, host);
    if (!_at.get_last_error()) {
        _dns_callback = callback;
        _dns_version = version;
    }

    return _at.unlock_return_error() ? NSAPI_ERROR_DNS_FAILURE : BG96_ASYNC_DNS_QUERY_ID;
}

nsapi_error_t QUECTEL_BG96_CellularStack::gethostbyname_async_cancel(int id)
{
    _at.lock();
    _dns_callback = NULL;
    _at.unlock();
    return NSAPI_ERROR_OK;
}
#endif

void QUECTEL_BG96_CellularStack::ip2dot(const SocketAddress &ip, char *dot)
{
    if (ip.get_ip_version() == NSAPI_IPv6) {
        const uint8_t *bytes = (uint8_t *)ip.get_ip_bytes();
        for (int i = 0; i < NSAPI_IPv6_BYTES; i += 2) {
            if (i != 0) {
                *dot++ = ':';
            }
            dot += sprintf(dot, "%x", (*(bytes + i) << 8 | *(bytes + i + 1)));
        }
    } else if (ip.get_ip_version() == NSAPI_IPv4) {
        strcpy(dot, ip.get_ip_address());
    } else {
        *dot = '\0';
    }
}

#if defined(MBED_CONF_NSAPI_OFFLOAD_TLSSOCKET) && (MBED_CONF_NSAPI_OFFLOAD_TLSSOCKET)

nsapi_error_t QUECTEL_BG96_CellularStack::set_to_modem_impl(const char *filename, const char *config, const char *data, size_t size)
{
    // Delete old file from the modem.
    _at.at_cmd_discard("+QFDEL", "=", "%s", filename);
    _at.clear_error(); // Ignore error if file didn't exist

    // Upload new file to modem
    _at.cmd_start_stop("+QFUPL", "=", "%s%d", filename, size);
    _at.resp_start("CONNECT");
    _at.write_bytes((uint8_t *)data, size);
    _at.resp_start("+QFUPL:");
    size_t upload_size = _at.read_int();
    _at.resp_stop();
    if (upload_size != size) {
        tr_error("Upload error! orig = %d, uploaded = %d", size, upload_size);
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    // Configure into use
    _at.at_cmd_discard("+QSSLCFG", "=", "%s%d%s", config, sslctxID, filename);

    return _at.get_last_error();
}

nsapi_error_t QUECTEL_BG96_CellularStack::setsockopt(nsapi_socket_t handle, int level,
                                                     int optname, const void *optval, unsigned optlen)
{
    CellularSocket *socket = (CellularSocket *)handle;
    nsapi_error_t ret = NSAPI_ERROR_OK;

    if (level == NSAPI_TLSSOCKET_LEVEL) {
        if (optval) {
            _at.lock();
            switch (optname) {
                case NSAPI_TLSSOCKET_ENABLE: {
                    MBED_ASSERT(optlen == sizeof(bool));
                    bool *enabled = (bool *)optval;
                    if (socket->proto == NSAPI_TCP) {
                        socket->tls_socket = enabled;

                        if (enabled) {
                            _at.at_cmd_discard("+QSSLCFG", "=\"seclevel\",", "%d%d", sslctxID, _tls_sec_level);

                            _at.at_cmd_discard("+QSSLCFG", "=\"sslversion\",", "%d%d", sslctxID, BG96_SUPPORTED_SSL_VERSION);

                            _at.cmd_start("AT+QSSLCFG=\"ciphersuite\",");
                            _at.write_int(sslctxID);
                            _at.write_string(BG96_SUPPORTED_CIPHER_SUITE, false);
                            _at.cmd_stop_read_resp();

                            ret = _at.get_last_error();
                        }
                    } else {
                        tr_error("Trying to set non-TCPSocket as TLSSocket");
                        ret = NSAPI_ERROR_PARAMETER;
                    }
                }
                break;

                case NSAPI_TLSSOCKET_SET_HOSTNAME: {
                    const char *hostname = (const char *)optval;
                    _at.at_cmd_discard("+QSSLCFG", "=\"checkhost\",", "%d%s", sslctxID, hostname);
                    ret = _at.get_last_error();
                }
                break;

                case NSAPI_TLSSOCKET_SET_CACERT: {
                    const char *cacert = (const char *)optval;
                    ret = set_to_modem_impl("cacert.pem", "cacert", cacert, optlen);

                    // Set sec level to "Manage server authentication" if only cacert is in use
                    if (ret == NSAPI_ERROR_OK && _tls_sec_level == 0) {
                        _tls_sec_level = 1;
                    }
                }
                break;

                case NSAPI_TLSSOCKET_SET_CLCERT: {
                    const char *clcert = (const char *)optval;
                    ret = set_to_modem_impl("clcert.pem", "clientcert", clcert, optlen);

                    // Set sec level to "Manage server and client authentication if requested by the remote server"
                    if (ret == NSAPI_ERROR_OK) {
                        _tls_sec_level = 2;
                    }
                }
                break;

                case NSAPI_TLSSOCKET_SET_CLKEY: {
                    const char *clkey = (const char *)optval;
                    ret = set_to_modem_impl("client.key", "clientkey", clkey, optlen);

                    // Set sec level to "Manage server and client authentication if requested by the remote server"
                    if (ret == NSAPI_ERROR_OK) {
                        _tls_sec_level = 2;
                    }
                }
                break;

                default:
                    tr_error("Unsupported sockopt (%d)", optname);
                    ret = NSAPI_ERROR_UNSUPPORTED;
            }
            _at.unlock();
        } else {
            tr_error("No optval!");
            ret = NSAPI_ERROR_PARAMETER;
        }
    } else {
        tr_warning("Unsupported level (%d)", level);
        ret = NSAPI_ERROR_UNSUPPORTED;
    }

    return ret;
}
#endif
