/*
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

/** @file TCPSocket.h TCPSocket class */
/** \addtogroup netsocket
 * @{*/

#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include "netsocket/InternetSocket.h"
#include "netsocket/NetworkStack.h"
#include "netsocket/NetworkInterface.h"
#include "rtos/EventFlags.h"


/** TCP socket connection
 */
class TCPSocket : public InternetSocket {
public:
    /** Create an uninitialized socket
     *
     *  Must call open to initialize the socket on a network stack.
     */
    TCPSocket();

    /** Override multicast functions to return error for TCP
     *
     */
    int join_multicast_group(const SocketAddress &address)
    {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    /** Connects TCP socket to a remote host
     *
     *  Initiates a connection to a remote server specified by the
     *  indicated address.
     *
     *  @param address  The SocketAddress of the remote host
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_IN_PROGRESS if the operation is ongoing
     *  @retval         NSAPI_ERROR_NO_SOCKET if the socket has not been allocated
     *  @retval         NSAPI_ERROR_DNS_FAILURE if the DNS address of host could not be resolved
     *  @retval         NSAPI_ERROR_IS_CONNECTED if the connection is already established
     *  @retval         int Other negative error codes for stack-related failures.
     *                  See NetworkStack::socket_connect().
     */
    nsapi_error_t connect(const SocketAddress &address) override;

    /** Send data over a TCP socket
     *
     *  The socket must be connected to a remote host. Returns the number of
     *  bytes sent from the buffer.
     *
     *  By default, send blocks until all data is sent. If socket is set to
     *  non-blocking or times out, a partial amount can be written.
     *  NSAPI_ERROR_WOULD_BLOCK is returned if no data was written.
     *
     *  @param data     Buffer of data to send to the host
     *  @param size     Size of the buffer in bytes
     *  @retval         int Number of sent bytes on success
     *  @retval         NSAPI_ERROR_NO_SOCKET in case socket was not created correctly
     *  @retval         NSAPI_ERROR_WOULD_BLOCK in case non-blocking mode is enabled
     *                  and send cannot be performed immediately
     *  @retval         int Other negative error codes for stack-related failures.
     *                  See @ref NetworkStack::socket_send.
     */
    nsapi_size_or_error_t send(const void *data, nsapi_size_t size) override;

    /** Receive data over a TCP socket
     *
     *  The socket must be connected to a remote host. Returns the number of
     *  bytes received into the buffer.
     *
     *  By default, recv blocks until some data is received. If socket is set to
     *  non-blocking or times out, NSAPI_ERROR_WOULD_BLOCK can be returned to
     *  indicate no data.
     *
     *  @param data     Destination buffer for data received from the host
     *  @param size     Size of the buffer in bytes
     *  @retval         int Number of received bytes on success
     *  @retval         NSAPI_ERROR_NO_SOCKET in case socket was not created correctly
     *  @retval         NSAPI_ERROR_WOULD_BLOCK in case non-blocking mode is enabled
     *                  and send cannot be performed immediately
     *  @retval         int Other negative error codes for stack-related failures.
     *                  See @ref NetworkStack::socket_recv.
     */
    nsapi_size_or_error_t recv(void *data, nsapi_size_t size) override;

    /** Send data on a socket.
     *
     * TCP socket is connection oriented protocol, so address is ignored.
     *
     * By default, sendto blocks until data is sent. If socket is set to
     * non-blocking or times out, NSAPI_ERROR_WOULD_BLOCK is returned
     * immediately.
     *
     *  @param address  Remote address
     *  @param data     Buffer of data to send to the host
     *  @param size     Size of the buffer in bytes
     *  @retval         int Number of sent bytes on success
     *  @retval         NSAPI_ERROR_NO_SOCKET in case socket was not created correctly
     *  @retval         NSAPI_ERROR_WOULD_BLOCK in case non-blocking mode is enabled
     *                  and send cannot be performed immediately
     *  @retval         int Other negative error codes for stack-related failures.
     *                  See @ref NetworkStack::socket_send.
     */
    nsapi_size_or_error_t sendto(const SocketAddress &address,
                                 const void *data, nsapi_size_t size) override;

    /** Receive a data from a socket
     *
     *  Receives a data and stores the source address in address if address
     *  is not NULL. Returns the number of bytes written into the buffer.
     *
     *  By default, recvfrom blocks until a data is received. If socket is set to
     *  non-blocking or times out with no datagram, NSAPI_ERROR_WOULD_BLOCK
     *  is returned.
     *
     *  @param address  Destination for the source address or NULL
     *  @param data     Destination buffer for datagram received from the host
     *  @param size     Size of the buffer in bytes
     *  @retval         int Number of received bytes on success
     *  @retval         NSAPI_ERROR_NO_SOCKET in case socket was not created correctly
     *  @retval         NSAPI_ERROR_WOULD_BLOCK in case non-blocking mode is enabled
     *                  and send cannot be performed immediately
     *  @retval         int Other negative error codes for stack-related failures.
     *                  See @ref NetworkStack::socket_recv.
     */
    nsapi_size_or_error_t recvfrom(SocketAddress *address,
                                   void *data, nsapi_size_t size) override;

    /** Send data on a packet with ancillary datasocket.
    *
    * TCP socket is connection oriented protocol, so address is ignored.
    *
    * By default, sendmsg blocks until data is sent. If socket is set to
    * non-blocking or times out, NSAPI_ERROR_WOULD_BLOCK is returned
    * immediately.
    *
    *  @param address  Remote address
    *  @param data     Buffer of data to send to the host
    *  @param size     Size of the buffer in bytes
    *  @retval         int Number of sent bytes on success
    *  @retval         NSAPI_ERROR_NO_SOCKET in case socket was not created correctly
    *  @retval         NSAPI_ERROR_WOULD_BLOCK in case non-blocking mode is enabled
    *                  and send cannot be performed immediately
    *  @retval         int Other negative error codes for stack-related failures.
    *                  See @ref NetworkStack::socket_send.
    */
    nsapi_size_or_error_t sendmsg(const SocketAddress &address,
                                  const void *data, nsapi_size_t size,
                                  nsapi_msghdr_t *control, nsapi_size_t control_size) override;

    /** Receive a packet with ancillary data from a socket
     *
     *  Receives a data and stores the source address in address if address
     *  is not NULL. Returns the number of bytes written into the buffer.
     *
     *  By default, recvmsg blocks until a data is received. If socket is set to
     *  non-blocking or times out with no datagram, NSAPI_ERROR_WOULD_BLOCK
     *  is returned.
     *
     *  @param address  Destination for the source address or NULL
     *  @param data     Destination buffer for datagram received from the host
     *  @param size     Size of the buffer in bytes
     *  @control        Pointer to the control buffer
     *  @control_size     Size of the control buffer in bytes
     *  @retval         int Number of received bytes on success
     *  @retval         NSAPI_ERROR_NO_SOCKET in case socket was not created correctly
     *  @retval         NSAPI_ERROR_WOULD_BLOCK in case non-blocking mode is enabled
     *                  and send cannot be performed immediately
     *  @retval         int Other negative error codes for stack-related failures.
     *                  See @ref NetworkStack::socket_recv.
     */
    nsapi_size_or_error_t recvmsg(SocketAddress *address,
                                  void *data, nsapi_size_t size,
                                  nsapi_msghdr_t *control, nsapi_size_t control_size) override;

    /** Accepts a connection on a socket.
     *
     *  The server socket must be bound and set to listen for connections.
     *  On a new connection, returns connected network socket which user is expected to call close()
     *  and that deallocates the resources. Referencing a returned pointer after a close()
     *  call is not allowed and leads to undefined behavior.
     *
     *  By default, accept blocks until incoming connection occurs. If socket is set to
     *  non-blocking or times out, error is set to NSAPI_ERROR_WOULD_BLOCK.
     *
     *  @param error      pointer to storage of the error value or NULL:
     *                    NSAPI_ERROR_OK on success
     *                    NSAPI_ERROR_WOULD_BLOCK if socket is set to non-blocking and would block
     *                    NSAPI_ERROR_NO_SOCKET if the socket was not open
     *  @return           pointer to a socket
     */
    TCPSocket *accept(nsapi_error_t *error = NULL) override;

    /** Listen for incoming connections.
     *
     *  Marks the socket as a passive socket that can be used to accept
     *  incoming connections.
     *
     *  @param backlog  Number of pending connections that can be queued
     *                  simultaneously, defaults to 1
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_NO_SOCKET in case socket was not created correctly
     *  @retval         int Other negative error codes for stack-related failures.
     *                  See @ref NetworkStack::socket_listen.
     */
    nsapi_error_t listen(int backlog = 1) override;

protected:
    nsapi_protocol_t get_proto() override;

private:
    /** Create a socket out of a given socket
     *
     *  To be used within accept() function. Close() will clean this up.
     */
    TCPSocket(TCPSocket *parent, nsapi_socket_t socket, SocketAddress address);
};


#endif

/** @}*/
