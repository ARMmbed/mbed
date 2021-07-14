/* CellularNonIPSocket
#include <CellularNonIPSocket.h>
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

#if MBED_CONF_CELLULAR_PRESENT

#include "platform/Callback.h"
#include "netsocket/CellularNonIPSocket.h"
#include <stdio.h>

using namespace mbed;

ControlPlane_netif *CellularNonIPSocket::_cp_netif;

CellularNonIPSocket::CellularNonIPSocket() = default;

nsapi_error_t CellularNonIPSocket::open(CellularContext *cellular_context)
{
    if (cellular_context == nullptr) {
        return NSAPI_ERROR_PARAMETER;
    }

    return open(cellular_context->get_cp_netif());
}

CellularNonIPSocket::~CellularNonIPSocket()
{
    close();
}

nsapi_error_t CellularNonIPSocket::open(ControlPlane_netif *cp_netif)
{
    _lock.lock();

    if (cp_netif == nullptr || _opened) {
        _lock.unlock();
        return NSAPI_ERROR_PARAMETER;
    }

    if (_cp_netif) {
        _lock.unlock();
        return NSAPI_ERROR_NO_SOCKET;
    }
    _cp_netif = cp_netif;

    _event = callback(this, &CellularNonIPSocket::event);
    _cp_netif->attach(Callback<void()>::thunk, &_event);
    _opened = true;

    _lock.unlock();
    return NSAPI_ERROR_OK;
}

nsapi_error_t CellularNonIPSocket::close()
{
    _lock.lock();

    nsapi_error_t ret = NSAPI_ERROR_OK;
    if (!_opened)  {
        return NSAPI_ERROR_NO_SOCKET;
    }

    // Just in case - tell the stack not to callback any more, then remove this socket.
    _cp_netif->attach(nullptr, nullptr);
    _opened = false;
    _cp_netif = nullptr; // Invalidate the cp_netif pointer - otherwise open() fails.

    // Wakeup anything in a blocking operation
    // on this socket
    event();

    // Wait until all readers and writers are gone
    while (_readers || _writers) {
        _lock.unlock();
        _event_flag.wait_any(FINISHED_FLAG, osWaitForever);
        _lock.lock();
    }

    _lock.unlock();

    return ret;
}

nsapi_size_or_error_t CellularNonIPSocket::send(const void *data, nsapi_size_t size)
{
    if (!data) {
        return NSAPI_ERROR_PARAMETER;
    }
    _lock.lock();
    nsapi_size_or_error_t ret;

    _writers++;

    while (true) {
        if (!_opened) {
            ret = NSAPI_ERROR_NO_SOCKET;
            break;
        }

        core_util_atomic_flag_clear(&_pending);
        nsapi_size_or_error_t sent = _cp_netif->send(data, size);
        if ((0 == _timeout) || (NSAPI_ERROR_WOULD_BLOCK != sent)) {
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
    if (!_opened || !_writers) {
        _event_flag.set(FINISHED_FLAG);
    }
    _lock.unlock();
    return ret;
}

nsapi_size_or_error_t CellularNonIPSocket::recv(void *buffer, nsapi_size_t size)
{
    _lock.lock();
    nsapi_size_or_error_t ret = NSAPI_ERROR_NO_SOCKET;

    _readers++;

    while (true) {
        if (!_opened) {
            break;
        }

        core_util_atomic_flag_clear(&_pending);
        nsapi_size_or_error_t recv = _cp_netif->recv(buffer, size);

        // Non-blocking sockets always return. Blocking only returns when success or errors other than WOULD_BLOCK
        if ((0 == _timeout) || (NSAPI_ERROR_WOULD_BLOCK != recv)) {
            ret = recv;
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
                // Poll once more for a possibly missed data received indication
                ret = _cp_netif->recv(buffer, size);
                break;
            }
        }
    }

    _readers--;
    if (!_opened || !_readers) {
        _event_flag.set(FINISHED_FLAG);
    }

    _lock.unlock();
    return ret;
}

void CellularNonIPSocket::set_blocking(bool blocking)
{
    set_timeout(blocking ? -1 : 0);
}

void CellularNonIPSocket::set_timeout(int timeout)
{
    _lock.lock();

    if (timeout >= 0) {
        _timeout = (uint32_t)timeout;
    } else {
        _timeout = osWaitForever;
    }

    _lock.unlock();
}

void CellularNonIPSocket::event()
{
    _event_flag.set(READ_FLAG | WRITE_FLAG);

    if (_callback && !core_util_atomic_flag_test_and_set(&_pending)) {
        _callback();
    }
}

void CellularNonIPSocket::sigio(Callback<void()> callback)
{
    _lock.lock();
    _callback = callback;
    _lock.unlock();
}

nsapi_error_t CellularNonIPSocket::connect(const SocketAddress &address)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

Socket *CellularNonIPSocket::accept(nsapi_error_t *error)
{
    if (error) {
        *error = NSAPI_ERROR_UNSUPPORTED;
    }
    return nullptr;
}

nsapi_error_t CellularNonIPSocket::listen(int backlog)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_size_or_error_t CellularNonIPSocket::sendto(const SocketAddress &address,
                                                  const void *data, nsapi_size_t size)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
nsapi_size_or_error_t CellularNonIPSocket::sendmsg(const SocketAddress &address,
                                                   const void *data, nsapi_size_t size,
                                                   nsapi_msghdr_t *control, nsapi_size_t control_size)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_size_or_error_t CellularNonIPSocket::recvfrom(SocketAddress *address,
                                                    void *data, nsapi_size_t size)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_size_or_error_t CellularNonIPSocket::recvmsg(SocketAddress *address,
                                                   void *data, nsapi_size_t size,
                                                   nsapi_msghdr_t *control, nsapi_size_t control_size)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t CellularNonIPSocket::setsockopt(int level, int optname, const void *optval, unsigned optlen)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t CellularNonIPSocket::getsockopt(int level, int optname, void *optval, unsigned *optlen)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t  CellularNonIPSocket::getpeername(SocketAddress *address)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t CellularNonIPSocket::bind(const SocketAddress &address)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

#endif
