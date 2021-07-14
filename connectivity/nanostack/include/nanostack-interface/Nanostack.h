/*
 * Copyright (c) 2016-2017, Arm Limited and affiliates.
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

#ifndef NANOSTACK_H_
#define NANOSTACK_H_

#include "OnboardNetworkStack.h"
#include "NanostackMemoryManager.h"
#include "MeshInterface.h"
#include "mesh_interface_types.h"
#include "eventOS_event.h"

struct ns_address;

class Nanostack : public OnboardNetworkStack, private mbed::NonCopyable<Nanostack> {
public:
    static Nanostack &get_instance();

    // Our Nanostack::Interface etc are defined by mbed_mesh_api
    class Interface;
    class EthernetInterface;
    class MeshInterface;
    class LoWPANNDInterface;
    class ThreadInterface;
    class WisunInterface;
    class PPPInterface;

    /* Implement OnboardNetworkStack method */
    nsapi_error_t add_ethernet_interface(EMAC &emac, bool default_if, OnboardNetworkStack::Interface **interface_out, NetworkInterface *user_network_interface = NULL) override;
    nsapi_error_t add_ethernet_interface(EMAC &emac, bool default_if, OnboardNetworkStack::Interface **interface_out, const uint8_t *mac_addr, NetworkInterface *user_network_interface = NULL) override;

    /* Local variant with stronger typing and manual address specification */
    nsapi_error_t add_ethernet_interface(EMAC &emac, bool default_if, Nanostack::EthernetInterface **interface_out, const uint8_t *mac_addr = NULL, NetworkInterface *user_network_interface = NULL);

    nsapi_error_t add_ppp_interface(PPP &ppp, bool default_if, OnboardNetworkStack::Interface **interface_out) override;

    /* Local variant with stronger typing and manual address specification */
    nsapi_error_t add_ppp_interface(PPP &ppp, bool default_if, Nanostack::PPPInterface **interface_out);

    nsapi_error_t remove_ppp_interface(OnboardNetworkStack::Interface **interface_out) override;

protected:

    Nanostack();

    /** @copydoc NetworkStack::get_ip_address */
    nsapi_error_t get_ip_address(SocketAddress *sockAddr) override;

    /** Translate a hostname to an IP address with specific version using network interface name.
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  Method first checks Nanostack DNS query result cache. If match is found, then the result is returned immediately.
     *  Otherwise method calls DNS resolver to find a match.
     *
     *  @param host     Hostname to resolve.
     *  @param address  Pointer to a SocketAddress to store the result.
     *  @param version  IP version of address to resolve, NSAPI_UNSPEC indicates
     *                  version is chosen by the stack (defaults to NSAPI_UNSPEC).
     *  @param interface_name  Network interface name
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure.
     */
    virtual nsapi_error_t gethostbyname(const char *name, SocketAddress *address, nsapi_version_t version, const char *interface_name) override;

    /** Translate a hostname to an IP address (asynchronous) using network interface name.
     *
     *  The hostname may be either a domain name or a dotted IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  Method first checks Nanostack DNS query result cache. If match is found, then the result is returned immediately.
     *
     *  Call is non-blocking. Result of the DNS operation is returned by the callback.
     *  If this function returns failure, callback will not be called. In case result
     *  is success (IP address was found from DNS cache), callback will be called
     *  before function returns.
     *
     *  @param host     Hostname to resolve.
     *  @param callback Callback that is called for result.
     *  @param version  IP version of address to resolve, NSAPI_UNSPEC indicates
     *                  version is chosen by the stack (defaults to NSAPI_UNSPEC).
     *  @param interface_name  Network interface name
     *  @return         0 on immediate success,
     *                  negative error code on immediate failure or
     *                  a positive unique id that represents the hostname translation operation
     *                  and can be passed to cancel.
     */
    virtual nsapi_value_or_error_t gethostbyname_async(const char *name, hostbyname_cb_t callback, nsapi_version_t version, const char *interface_name) override;

    /** Get a domain name server from a list of servers to query
     *
     *  Returns a DNS server address for a index. DNS servers are queried from Nanostack DNS cache.
     *  If returns error no more DNS servers to read.
     *
     *  @param index    Index of the DNS server, starts from zero
     *  @param address  Destination for the host address
     *  @param interface_name Network interface name
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t get_dns_server(int index, SocketAddress *address, const char *interface_name) override;

    /** Opens a socket
     *
     *  Creates a network socket and stores it in the specified handle.
     *  The handle must be passed to following calls on the socket.
     *
     *  A stack may have a finite number of sockets, in this case
     *  NSAPI_ERROR_NO_SOCKET is returned if no socket is available.
     *
     *  @param handle   Destination for the handle to a newly created socket
     *  @param proto    Protocol of socket to open, NSAPI_TCP or NSAPI_UDP
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t socket_open(void **handle, nsapi_protocol_t proto) override;

    /** Close the socket
     *
     *  Closes any open connection and deallocates any memory associated
     *  with the socket.
     *
     *  @param handle   Socket handle
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t socket_close(void *handle) override;

    /** Bind a specific address to a socket
     *
     *  Binding a socket specifies the address and port on which to recieve
     *  data. If the IP address is zeroed, only the port is bound.
     *
     *  @param handle   Socket handle
     *  @param address  Local address to bind
     *  @return         0 on success, negative error code on failure.
     */
    nsapi_error_t socket_bind(void *handle, const SocketAddress &address) override;

    /** Listen for connections on a TCP socket
     *
     *  Marks the socket as a passive socket that can be used to accept
     *  incoming connections.
     *
     *  @param handle   Socket handle
     *  @param backlog  Number of pending connections that can be queued
     *                  simultaneously
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t socket_listen(void *handle, int backlog) override;

    /** Connects TCP socket to a remote host
     *
     *  Initiates a connection to a remote server specified by the
     *  indicated address.
     *
     *  @param handle   Socket handle
     *  @param address  The SocketAddress of the remote host
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t socket_connect(void *handle, const SocketAddress &address) override;

    /** Accepts a connection on a TCP socket
     *
     *  The server socket must be bound and set to listen for connections.
     *  On a new connection, creates a network socket and stores it in the
     *  specified handle. The handle must be passed to following calls on
     *  the socket.
     *
     *  A stack may have a finite number of sockets, in this case
     *  NSAPI_ERROR_NO_SOCKET is returned if no socket is available.
     *
     *  This call is non-blocking. If accept would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param server   Socket handle to server to accept from
     *  @param handle   Destination for a handle to the newly created socket
     *  @param address  Destination for the remote address or NULL
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t socket_accept(void *handle, void **server, SocketAddress *address) override;

    /** Send data over a TCP socket
     *
     *  The socket must be connected to a remote host. Returns the number of
     *  bytes sent from the buffer.
     *
     *  This call is non-blocking. If send would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param handle   Socket handle
     *  @param data     Buffer of data to send to the host
     *  @param size     Size of the buffer in bytes
     *  @return         Number of sent bytes on success, negative error
     *                  code on failure
     */
    nsapi_size_or_error_t socket_send(void *handle, const void *data, nsapi_size_t size) override;

    /** Receive data over a TCP socket
     *
     *  The socket must be connected to a remote host. Returns the number of
     *  bytes received into the buffer.
     *
     *  This call is non-blocking. If recv would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param handle   Socket handle
     *  @param data     Destination buffer for data received from the host
     *  @param size     Size of the buffer in bytes
     *  @return         Number of received bytes on success, negative error
     *                  code on failure
     */
    nsapi_size_or_error_t socket_recv(void *handle, void *data, nsapi_size_t size) override;

    /** Send a packet over a UDP socket
     *
     *  Sends data to the specified address. Returns the number of bytes
     *  sent from the buffer.
     *
     *  This call is non-blocking. If sendto would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param handle   Socket handle
     *  @param address  The SocketAddress of the remote host
     *  @param data     Buffer of data to send to the host
     *  @param size     Size of the buffer in bytes
     *  @return         Number of sent bytes on success, negative error
     *                  code on failure
     */
    nsapi_size_or_error_t socket_sendto(void *handle, const SocketAddress &address, const void *data, nsapi_size_t size) override;

    /** Receive a packet over a UDP socket
     *
     *  Receives data and stores the source address in address if address
     *  is not NULL. Returns the number of bytes received into the buffer.
     *
     *  This call is non-blocking. If recvfrom would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param handle   Socket handle
     *  @param address  Destination for the source address or NULL
     *  @param buffer   Destination buffer for data received from the host
     *  @param size     Size of the buffer in bytes
     *  @return         Number of received bytes on success, negative error
     *                  code on failure
     */
    nsapi_size_or_error_t socket_recvfrom(void *handle, SocketAddress *address, void *buffer, nsapi_size_t size) override;

    /** Register a callback on state change of the socket
     *
     *  The specified callback will be called on state changes such as when
     *  the socket can recv/send/accept successfully and on when an error
     *  occurs. The callback may also be called spuriously without reason.
     *
     *  The callback may be called in an interrupt context and should not
     *  perform expensive operations such as recv/send calls.
     *
     *  @param handle   Socket handle
     *  @param callback Function to call on state change
     *  @param data     Argument to pass to callback
     */
    void socket_attach(void *handle, void (*callback)(void *), void *data) override;

    /*  Set stack-specific socket options
     *
     *  The setsockopt allow an application to pass stack-specific hints
     *  to the underlying stack. For unsupported options,
     *  NSAPI_ERROR_UNSUPPORTED is returned and the socket is unmodified.
     *
     *  @param handle   Socket handle
     *  @param level    Stack-specific protocol level
     *  @param optname  Stack-specific option identifier
     *  @param optval   Option value
     *  @param optlen   Length of the option value
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t setsockopt(void *handle, int level, int optname, const void *optval, unsigned optlen) override;

    /*  Get stack-specific socket options
     *
     *  The getstackopt allow an application to retrieve stack-specific hints
     *  from the underlying stack. For unsupported options,
     *  NSAPI_ERROR_UNSUPPORTED is returned and optval is unmodified.
     *
     *  @param handle   Socket handle
     *  @param level    Stack-specific protocol level
     *  @param optname  Stack-specific option identifier
     *  @param optval   Destination for option value
     *  @param optlen   Length of the option value
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t getsockopt(void *handle, int level, int optname, void *optval, unsigned *optlen) override;

    // FIXME: Implement
    nsapi_size_or_error_t socket_sendmsg(nsapi_socket_t handle, const SocketAddress &address,
                                         const void *data, nsapi_size_t size,
                                         nsapi_msghdr_t *control, nsapi_size_t control_size) override
    {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    nsapi_size_or_error_t socket_recvmsg(nsapi_socket_t handle, SocketAddress *address,
                                         void *data, nsapi_size_t size,
                                         nsapi_msghdr_t *control, nsapi_size_t control_size) override
    {
        return NSAPI_ERROR_UNSUPPORTED;
    }

private:

    /** Call in callback
      *
      *  Callback is used to call the call in method of the network stack.
      */
    typedef mbed::Callback<nsapi_error_t (int delay_ms, mbed::Callback<void()> user_cb)> call_in_callback_cb_t;

    /** Get a call in callback
     *
     *  Get a call in callback from the network stack context.
     *
     *  Callback should not take more than 10ms to execute, otherwise it might
     *  prevent underlying thread processing. A portable user of the callback
     *  should not make calls to network operations due to stack size limitations.
     *  The callback should not perform expensive operations such as socket recv/send
     *  calls or blocking operations.
     *
     *  @return         Call in callback
     */
    call_in_callback_cb_t get_call_in_callback() override;

    /** Call a callback after a delay
     *
     *  Call a callback from the network stack context after a delay. If function
     *  returns error callback will not be called.
     *
     *  @param delay    Delay in milliseconds
     *  @param func     Callback to be called
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t call_in(int delay, mbed::Callback<void()> func) override;

    struct nanostack_callback {
        mbed::Callback<void()> callback;
    };

    nsapi_size_or_error_t do_sendto(void *handle, const struct ns_address *address, const void *data, nsapi_size_t size);
    static void call_event_tasklet_main(arm_event_s *event);
    char text_ip_address[40];
    NanostackMemoryManager memory_manager;
    int8_t call_event_tasklet;
};

nsapi_error_t map_mesh_error(mesh_error_t err);

#endif /* NANOSTACK_H_ */
