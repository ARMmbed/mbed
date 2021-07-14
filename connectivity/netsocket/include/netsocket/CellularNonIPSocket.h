/* CellularNonIPSocket
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

#ifndef CELLULARNONIPSOCKET_H
#define CELLULARNONIPSOCKET_H

#include "netsocket/Socket.h"
#include "rtos/Mutex.h"
#include "rtos/EventFlags.h"
#include "Callback.h"
#include "mbed_atomic.h"
#include "mbed_toolchain.h"
#include "ControlPlane_netif.h"
#include "CellularContext.h"

namespace mbed {

/** \addtogroup netsocket */
/** @{*/

/** Socket implementation for cellular Non-IP data delivery(NIDD).
 *  Relies on Control Plane CIoT EPS optimization feature,
 *  implemented in ControlPlane_netif class.
 */
class CellularNonIPSocket : public Socket {
public:
    /** Destroy the socket.
    *
    *  @note Closes socket if it's still open.
    */
    ~CellularNonIPSocket() override;

    /** Creates a socket.
    */
    CellularNonIPSocket();

    /** Opens a socket on the given cellular context.
    *
    *  @param cellular_context     Cellular PDP context over which this socket is sending and
    *                              receiving data. The context has support for providing
    *                              a control plane interface for data delivery.
    *  @return                     NSAPI_ERROR_OK on success
    *                              NSAPI_ERROR_PARAMETER otherwise
    */
    nsapi_error_t open(mbed::CellularContext *cellular_context);

    /** Opens a socket that will use the given control plane interface for data delivery.
    *   Attaches the event as callback to the control plane interface.
    *
    *  @param cp_netif     Control plane interface for data delivery.
    *  @return             NSAPI_ERROR_OK on success
    *                      NSAPI_ERROR_PARAMETER otherwise
    *
    */
    nsapi_error_t open(mbed::ControlPlane_netif *cp_netif);

    /** Closes socket
    *
    *  @return         NSAPI_ERROR_OK on success
    *                  NSAPI_ERROR_NO_SOCKET otherwise
    */

    nsapi_error_t close() override;

    /** Send data over a control plane cellular context.
    *
    *  By default, send blocks until all data is sent. If socket is set to
    *  nonblocking or times out, a partial amount can be written.
    *  NSAPI_ERROR_WOULD_BLOCK is returned if no data was written.
    *
    *  @param data     Buffer of data to be sent.
    *  @param size     Size of the buffer in bytes.
    *  @return         Number of sent bytes on success, negative error
    *                  code on failure.
    */
    nsapi_size_or_error_t send(const void *data, nsapi_size_t size) override;

    /** Receive data from a socket.
    *
    *  By default, recv blocks until some data is received. If socket is set to
    *  nonblocking or times out, NSAPI_ERROR_WOULD_BLOCK can be returned to
    *  indicate no data.
    *
    *  @param data     Pointer to buffer for received data.
    *  @param size     Size of the buffer in bytes.
    *  @return         Number of received bytes on success, negative error
    *                  code on failure.
    */
    nsapi_size_or_error_t recv(void *data, nsapi_size_t size) override;

    /** @copydoc Socket::set_blocking
     */
    void set_blocking(bool blocking) override;

    /** @copydoc Socket::set_timeout
     */
    void set_timeout(int timeout) override;

    /** @copydoc Socket::sigio
     */
    void sigio(mbed::Callback<void()> func) override;

    /// NOT APPLICABLE
    nsapi_error_t connect(const SocketAddress &address) override;
    /// NOT APPLICABLE
    Socket *accept(nsapi_error_t *error = NULL) override;
    /// NOT APPLICABLE
    nsapi_error_t listen(int backlog = 1) override;
    /// NOT APPLICABLE
    nsapi_error_t setsockopt(int level, int optname, const void *optval, unsigned optlen) override;
    /// NOT APPLICABLE
    nsapi_error_t getsockopt(int level, int optname, void *optval, unsigned *optlen) override;
    /// NOT APPLICABLE
    nsapi_error_t getpeername(SocketAddress *address) override;
    /// NOT APPLICABLE
    nsapi_size_or_error_t sendto(const SocketAddress &address,
                                 const void *data, nsapi_size_t size) override;
    /// NOT APPLICABLE
    nsapi_size_or_error_t recvfrom(SocketAddress *address,
                                   void *data, nsapi_size_t size) override;
    /// NOT APPLICABLE
    nsapi_size_or_error_t sendmsg(const SocketAddress &address,
                                  const void *data, nsapi_size_t size,
                                  nsapi_msghdr_t *control, nsapi_size_t control_size) override;
    /// NOT APPLICABLE
    nsapi_size_or_error_t recvmsg(SocketAddress *address,
                                  void *data, nsapi_size_t size,
                                  nsapi_msghdr_t *control, nsapi_size_t control_size) override;
    /// NOT APPLICABLE
    nsapi_error_t bind(const SocketAddress &address) override;

protected:
    void event();

    uint32_t _timeout = osWaitForever;
    mbed::Callback<void()> _event;
    mbed::Callback<void()> _callback;
    rtos::EventFlags _event_flag;
    rtos::Mutex _lock;
    uint8_t _readers = 0;
    uint8_t _writers = 0;
    core_util_atomic_flag _pending = CORE_UTIL_ATOMIC_FLAG_INIT;

    // Event flags
    static const int READ_FLAG     = 0x1u;
    static const int WRITE_FLAG    = 0x2u;
    static const int FINISHED_FLAG = 0x3u;

    static ControlPlane_netif *_cp_netif; // there can be only one Non-IP socket
    bool _opened = false;
};

/** @}*/
} // namespace mbed

#endif // CELLULARNONIPSOCKET_H
