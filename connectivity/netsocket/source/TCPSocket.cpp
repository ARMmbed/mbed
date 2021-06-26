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

#include "netsocket/TCPSocket.h"
#include <new>
#include "Timer.h"
#include "mbed_assert.h"

TCPSocket::TCPSocket()
{
    _socket_stats.stats_update_proto(this, NSAPI_TCP);
}

TCPSocket::TCPSocket(TCPSocket *parent, nsapi_socket_t socket, SocketAddress address) : TCPSocket()
{
    _socket = socket;
    _stack = parent->_stack;
    _factory_allocated = true;
    _remote_peer = address;
    _event = mbed::Callback<void()>(this, &TCPSocket::event);
    _stack->socket_attach(socket, _event.thunk, &_event);
}

nsapi_protocol_t TCPSocket::get_proto()
{
    return NSAPI_TCP;
}

nsapi_error_t TCPSocket::connect(const SocketAddress &address)
{
    _lock.lock();
    nsapi_error_t ret;

    // If this assert is hit then there are two threads
    // performing a send at the same time which is undefined
    // behavior
    MBED_ASSERT(_writers == 0);
    _writers++;

    bool blocking_connect_in_progress = false;

    while (true) {
        if (!_socket) {
            ret = NSAPI_ERROR_NO_SOCKET;
            break;
        }

        core_util_atomic_flag_clear(&_pending);
        ret = _stack->socket_connect(_socket, address);
        if ((_timeout == 0) || !(ret == NSAPI_ERROR_IN_PROGRESS || ret == NSAPI_ERROR_ALREADY)) {
            _socket_stats.stats_update_socket_state(this, SOCK_CONNECTED);
            break;
        } else {
            blocking_connect_in_progress = true;

            uint32_t flag;

            // Release lock before blocking so other threads
            // accessing this object aren't blocked
            _lock.unlock();
            flag = _event_flag.wait_any(WRITE_FLAG, _timeout);
            _lock.lock();
            if (flag & osFlagsError) {
                // Timeout break
                break;
            }
        }
    }

    _writers--;
    if (!_socket) {
        _event_flag.set(FINISHED_FLAG);
    }

    /* Non-blocking connect gives "EISCONN" once done - convert to OK for blocking mode if we became connected during this call */
    if (ret == NSAPI_ERROR_IS_CONNECTED && blocking_connect_in_progress) {
        _socket_stats.stats_update_socket_state(this, SOCK_CONNECTED);
        ret = NSAPI_ERROR_OK;
    }

    if (ret == NSAPI_ERROR_OK || ret == NSAPI_ERROR_IN_PROGRESS) {
        _remote_peer = address;
        _socket_stats.stats_update_peer(this, _remote_peer);
    }

    _lock.unlock();
    return ret;
}

nsapi_size_or_error_t TCPSocket::send(const void *data, nsapi_size_t size)
{
    _lock.lock();
    const uint8_t *data_ptr = static_cast<const uint8_t *>(data);
    nsapi_size_or_error_t ret;
    nsapi_size_t written = 0;

    // If this assert is hit then there are two threads
    // performing a send at the same time which is undefined
    // behavior
    MBED_ASSERT(_writers == 0);
    _writers++;

    // Unlike recv, we should write the whole thing if blocking. POSIX only
    // allows partial as a side-effect of signal handling; it normally tries to
    // write everything if blocking. Without signals we can always write all.
    while (true) {
        if (!_socket) {
            ret = NSAPI_ERROR_NO_SOCKET;
            break;
        }

        core_util_atomic_flag_clear(&_pending);
        ret = _stack->socket_send(_socket, data_ptr + written, size - written);
        if (ret >= 0) {
            written += ret;
            if (written >= size) {
                break;
            }
        }
        if (_timeout == 0) {
            break;
        } else if (ret == NSAPI_ERROR_WOULD_BLOCK) {
            uint32_t flag;

            // Release lock before blocking so other threads
            // accessing this object aren't blocked
            _lock.unlock();
            flag = _event_flag.wait_any(WRITE_FLAG, _timeout);
            _lock.lock();

            if (flag & osFlagsError) {
                // Timeout break
                break;
            }
        } else if (ret < 0) {
            break;
        }
    }

    _writers--;
    if (!_socket) {
        _event_flag.set(FINISHED_FLAG);
    }

    _lock.unlock();
    if (ret <= 0 && ret != NSAPI_ERROR_WOULD_BLOCK) {
        return ret;
    } else if (written == 0) {
        return NSAPI_ERROR_WOULD_BLOCK;
    } else {
        _socket_stats.stats_update_sent_bytes(this, written);
        return written;
    }
}

nsapi_size_or_error_t TCPSocket::sendto(const SocketAddress &address, const void *data, nsapi_size_t size)
{
    (void)address;
    return send(data, size);
}

nsapi_size_or_error_t TCPSocket::recv(void *data, nsapi_size_t size)
{
    _lock.lock();
    nsapi_size_or_error_t ret;

    // If this assert is hit then there are two threads
    // performing a recv at the same time which is undefined
    // behavior
    MBED_ASSERT(_readers == 0);
    _readers++;

    while (true) {
        if (!_socket) {
            ret = NSAPI_ERROR_NO_SOCKET;
            break;
        }

        core_util_atomic_flag_clear(&_pending);
        ret = _stack->socket_recv(_socket, data, size);
        if ((_timeout == 0) || (ret != NSAPI_ERROR_WOULD_BLOCK)) {
            _socket_stats.stats_update_recv_bytes(this, ret);
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
    if (!_socket) {
        _event_flag.set(FINISHED_FLAG);
    }

    _lock.unlock();
    return ret;
}

nsapi_size_or_error_t TCPSocket::recvfrom(SocketAddress *address, void *data, nsapi_size_t size)
{
    if (address) {
        *address = _remote_peer;
    }
    return recv(data, size);
}

nsapi_error_t TCPSocket::listen(int backlog)
{
    _lock.lock();
    nsapi_error_t ret;

    if (!_socket) {
        ret = NSAPI_ERROR_NO_SOCKET;
    } else {
        ret = _stack->socket_listen(_socket, backlog);
        if (NSAPI_ERROR_OK == ret) {
            _socket_stats.stats_update_socket_state(this, SOCK_LISTEN);
        }
    }

    _lock.unlock();
    return ret;
}

TCPSocket *TCPSocket::accept(nsapi_error_t *error)
{
    _lock.lock();
    TCPSocket *connection = NULL;
    nsapi_error_t ret;

    _readers++;

    while (true) {
        if (!_socket) {
            ret = NSAPI_ERROR_NO_SOCKET;
            break;
        }

        core_util_atomic_flag_clear(&_pending);
        void *socket;
        SocketAddress address;
        ret = _stack->socket_accept(_socket, &socket, &address);

        if (0 == ret) {
            connection = new (std::nothrow) TCPSocket(this, socket, address);
            if (!connection) {
                ret = NSAPI_ERROR_NO_MEMORY;
                break;
            }

            _socket_stats.stats_update_peer(connection, address);
            _socket_stats.stats_update_socket_state(connection, SOCK_CONNECTED);
            break;
        } else if ((_timeout == 0) || (ret != NSAPI_ERROR_WOULD_BLOCK)) {
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
    if (!_socket) {
        _event_flag.set(FINISHED_FLAG);
    }
    _lock.unlock();
    if (error) {
        *error = ret;
    }
    return connection;
}
