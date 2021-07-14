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

#include "netsocket/InternetDatagramSocket.h"
#include "Timer.h"
#include "mbed_assert.h"

nsapi_error_t InternetDatagramSocket::connect(const SocketAddress &address)
{
    _remote_peer = address;
    _socket_stats.stats_update_peer(this, _remote_peer);
    _socket_stats.stats_update_socket_state(this, SOCK_CONNECTED);
    return NSAPI_ERROR_OK;
}

nsapi_size_or_error_t InternetDatagramSocket::sendmsg(const SocketAddress &address, const void *data, nsapi_size_t size, nsapi_msghdr_t *control, nsapi_size_t control_size)
{
    _lock.lock();
    nsapi_size_or_error_t ret;

    _writers++;
    if (_socket) {
        _socket_stats.stats_update_socket_state(this, SOCK_OPEN);
        _socket_stats.stats_update_peer(this, address);
    }
    while (true) {
        if (!_socket) {
            ret = NSAPI_ERROR_NO_SOCKET;
            break;
        }

        core_util_atomic_flag_clear(&_pending);
        nsapi_size_or_error_t sent = _stack->socket_sendmsg(_socket, address, data, size, control, control_size);
        if ((0 == _timeout) || (NSAPI_ERROR_WOULD_BLOCK != sent)) {
            _socket_stats.stats_update_sent_bytes(this, sent);
            ret = sent;
            break;
        } else {
            uint32_t flag;

            // Release lock before blocking so other threads
            // accessing this object aren't blocked
            _lock.unlock();
            flag = _event_flag.wait_any(WRITE_FLAG, _timeout);
            _lock.lock();

            if (flag & osFlagsError) {
                // Timeout break
                ret = NSAPI_ERROR_WOULD_BLOCK;
                break;
            }
        }
    }

    _writers--;
    if (!_socket || !_writers) {
        _event_flag.set(FINISHED_FLAG);
    }
    _lock.unlock();
    return ret;
}

nsapi_size_or_error_t InternetDatagramSocket::sendto(const SocketAddress &address, const void *data, nsapi_size_t size)
{

    return sendmsg(address, data, size, NULL, 0);
}

nsapi_size_or_error_t InternetDatagramSocket::send(const void *data, nsapi_size_t size)
{
    if (!_remote_peer) {
        return NSAPI_ERROR_NO_ADDRESS;
    }
    return sendto(_remote_peer, data, size);
}


nsapi_size_or_error_t InternetDatagramSocket::recvmsg(SocketAddress *address, void *buffer, nsapi_size_t size, nsapi_msghdr_t *control, nsapi_size_t control_size)
{
    _lock.lock();
    nsapi_size_or_error_t ret;
    SocketAddress ignored;

    if (!address) {
        address = &ignored;
    }

    _readers++;

    if (_socket) {
        _socket_stats.stats_update_socket_state(this, SOCK_OPEN);
    }
    while (true) {
        if (!_socket) {
            ret = NSAPI_ERROR_NO_SOCKET;
            break;
        }

        core_util_atomic_flag_clear(&_pending);
        nsapi_size_or_error_t recv = _stack->socket_recvmsg(_socket, address, buffer, size, control, control_size);

        // Filter incomming packets using connected peer address
        if (recv >= 0 && _remote_peer && _remote_peer != *address) {
            continue;
        }

        _socket_stats.stats_update_peer(this, _remote_peer);
        // Non-blocking sockets always return. Blocking only returns when success or errors other than WOULD_BLOCK
        if ((0 == _timeout) || (NSAPI_ERROR_WOULD_BLOCK != recv)) {
            ret = recv;
            _socket_stats.stats_update_recv_bytes(this, recv);
            break;
        } else {
            uint32_t flag;

            // Release lock before blocking so other threads
            // accessing this object aren't blocked
            _lock.unlock();
            flag = _event_flag.wait_any(READ_FLAG, _timeout);
            _lock.lock();

            if (flag & osFlagsError) {
                // Timeout break
                ret = NSAPI_ERROR_WOULD_BLOCK;
                break;
            }
        }
    }

    _readers--;
    if (!_socket || !_readers) {
        _event_flag.set(FINISHED_FLAG);
    }

    _lock.unlock();
    return ret;
}

nsapi_size_or_error_t InternetDatagramSocket::recvfrom(SocketAddress *address, void *buffer, nsapi_size_t size)
{
    return recvmsg(address, buffer, size, NULL, 0);
}

nsapi_size_or_error_t InternetDatagramSocket::recv(void *buffer, nsapi_size_t size)
{
    return recvfrom(NULL, buffer, size);
}

Socket *InternetDatagramSocket::accept(nsapi_error_t *error)
{
    if (error) {
        *error = NSAPI_ERROR_UNSUPPORTED;
    }
    return NULL;
}

nsapi_error_t InternetDatagramSocket::listen(int)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
