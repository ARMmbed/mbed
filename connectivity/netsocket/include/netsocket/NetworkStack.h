
/* NetworkStack
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

#ifndef NETWORK_STACK_H
#define NETWORK_STACK_H

#include <type_traits>
#include "nsapi_types.h"
#include "netsocket/SocketAddress.h"
#include "netsocket/NetworkInterface.h"
#include "DNS.h"

/** @file NetworkStack.h NetworkStack class */
/** @addtogroup netsocket
 * @{ */

// Predeclared classes
class OnboardNetworkStack;

/** NetworkStack class
 *
 *  Common interface that is shared between hardware that
 *  can connect to a network over IP. By implementing the
 *  NetworkStack, a network stack can be used as a target
 *  for instantiating network sockets.
 */
class NetworkStack : public DNS {
public:
    virtual ~NetworkStack() = default;

    /** Get the local IP address
     *
     *  @param          address SocketAddress representation of the local IP address
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_UNSUPPORTED if this feature is not supported
     *  @retval         NSAPI_ERROR_PARAMETER if the provided pointer is invalid
     *  @retval         NSAPI_ERROR_NO_ADDRESS if the address cannot be obtained from stack
     */
    virtual nsapi_error_t get_ip_address(SocketAddress *address);

    /** Get the IPv6 link local address
     *
     *  @param          address SocketAddress representation of the link local IPv6 address
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_UNSUPPORTED if this feature is not supported
     *  @retval         NSAPI_ERROR_PARAMETER if the provided pointer is invalid
     */
    virtual nsapi_error_t get_ipv6_link_local_address(SocketAddress *address);

    /** Get the local IP address on interface name
     *
     *  @param          address SocketAddress representation of the link local IPv6 address
     *  @param          interface_name  Network interface_name
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_UNSUPPORTED if this feature is not supported
     *  @retval         NSAPI_ERROR_PARAMETER if the provided pointer is invalid
     *  @retval         NSAPI_ERROR_NO_ADDRESS if the address cannot be obtained from stack
     */
    virtual nsapi_error_t get_ip_address_if(SocketAddress *address, const char *interface_name);

    /** Translates a hostname to an IP address with specific version
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  @param host     Hostname to resolve
     *  @param address  Pointer to a SocketAddress to store the result.
     *  @param version  IP version of address to resolve, NSAPI_UNSPEC indicates
     *                  version is chosen by the stack (defaults to NSAPI_UNSPEC)
     *  @param interface_name  Network interface_name
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_PARAMETER if invalid (null) name is provided
     *  @retval         NSAPI_ERROR_DNS_FAILURE if DNS resolution fails
     *  @retval         int other negative errors, see @ref nsapi_dns_query
     */
    virtual nsapi_error_t gethostbyname(const char *host,
                                        SocketAddress *address, nsapi_version_t version = NSAPI_UNSPEC, const char *interface_name = NULL);

    /** Translate a hostname to the multiple IP addresses with specific version using network interface name.
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  @param hostname     Hostname to resolve.
     *  @param hints  Pointer to a SocketAddress with  query parameters.
     *  @param res    Pointer to a SocketAddress array  to store the result..
     *  @param interface_name  Network interface name
     *  @return         number of results on success, negative error code on failure.
     */
    virtual nsapi_value_or_error_t getaddrinfo(const char *hostname, SocketAddress *hints, SocketAddress **res, const char *interface_name = NULL);

    /** Hostname translation callback (asynchronous)
     *
     *  Callback will be called after DNS resolution completes or a failure occurs.
     *
     *  Callback should not take more than 10ms to execute, otherwise it might
     *  prevent underlying thread processing. A portable user of the callback
     *  should not make calls to network operations due to stack size limitations.
     *  The callback should not perform expensive operations such as socket recv/send
     *  calls or blocking operations.
     *
     *  @param result  Negative error code on failure or
     *                 value that represents the number of DNS records
     *  @param address On success, destination for the host SocketAddress
     */
    typedef mbed::Callback<void (nsapi_value_or_error_t result, SocketAddress *address)> hostbyname_cb_t;

    /** Translates a hostname to multiple IP addresses (asynchronous)
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  Call is non-blocking. Result of the DNS operation is returned by the callback.
     *  If this function returns failure, callback will not be called. In case result
     *  is success (IP address was found from DNS cache), callback will be called
     *  before function returns.
     *
     *  @param host     Hostname to resolve
     *  @param callback Callback that is called for result
     *  @param version  IP version of address to resolve, NSAPI_UNSPEC indicates
     *                  version is chosen by the stack (defaults to NSAPI_UNSPEC)
     *  @param interface_name  Network interface_name
     *  @return         0 on immediate success,
     *                  negative error code on immediate failure or
     *                  a positive unique id that represents the hostname translation operation
     *                  and can be passed to cancel
     */
    virtual nsapi_value_or_error_t gethostbyname_async(const char *host, hostbyname_cb_t callback, nsapi_version_t version = NSAPI_UNSPEC,
                                                       const char *interface_name = NULL);

    /** Translates a hostname to the multiple IP addresses (asynchronous)
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  The call is non-blocking. Result of the DNS operation is returned by the callback.
     *  If this function returns failure, callback will not be called. In case that
     *  IP addresses are found from DNS cache, callback will be called before function returns.
     *
     *  @param hostname     Hostname to resolve
     *  @param hints  Pointer to a SocketAddress with  query parameters.
     *  @param callback Callback that is called for result
     *  @param interface_name  Network interface_name
     *  @return         0 on immediate success,
     *                  negative error code on immediate failure or
     *                  a positive unique id that represents the hostname translation operation
     *                  and can be passed to cancel
     */
    virtual nsapi_value_or_error_t getaddrinfo_async(const char *hostname, SocketAddress *hints, hostbyname_cb_t callback, const char *interface_name = NULL);

    /** Cancels asynchronous hostname translation
     *
     *  When translation is cancelled, callback will not be called.
     *
     *  @param id       Unique id of the hostname translation operation
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t gethostbyname_async_cancel(int id);

    /** Add a domain name server to list of servers to query
     *
     *  @param address  Destination for the host address
     *  @param interface_name  Network interface name
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t add_dns_server(const SocketAddress &address, const char *interface_name = NULL);

    /** Get a domain name server from a list of servers to query
     *
     *  Returns a DNS server address for a index. If returns error no more
     *  DNS servers to read.
     *
     *  @param index    Index of the DNS server, starts from zero
     *  @param address  Destination for the host address
     *  @param interface_name  Network interface name
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t get_dns_server(int index, SocketAddress *address, const char *interface_name = NULL);

    /*  Set stack options
     *
     *  setstackopt allows an application to pass stack-specific options
     *  to the underlying stack using stack-specific level and option names,
     *  or to request generic options using levels from nsapi_stack_level_t.
     *
     *  For unsupported options, NSAPI_ERROR_UNSUPPORTED is returned
     *  and the stack is unmodified.
     *
     *  @param level    Stack-specific protocol level or nsapi_stack_level_t
     *  @param optname  Level-specific option name
     *  @param optval   Option value
     *  @param optlen   Length of the option value
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t setstackopt(int level, int optname, const void *optval, unsigned optlen);

    /*  Get stack options
     *
     *  getstackopt allows an application to retrieve stack-specific options
     *  to the underlying stack using stack-specific level and option names,
     *  or to request generic options using levels from nsapi_stack_level_t.
     *
     *  @param level    Stack-specific protocol level or nsapi_stack_level_t
     *  @param optname  Level-specific option name
     *  @param optval   Destination for option value
     *  @param optlen   Length of the option value
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t getstackopt(int level, int optname, void *optval, unsigned *optlen);

    /** Dynamic downcast to a OnboardNetworkStack */
    virtual OnboardNetworkStack *onboardNetworkStack()
    {
        return 0;
    }

protected:
    friend class InternetSocket;
    friend class InternetDatagramSocket;
    friend class TCPSocket;

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
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto) = 0;

    /** Close the socket
     *
     *  Closes any open connection and deallocates any memory associated
     *  with the socket.
     *
     *  @param handle   Socket handle
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t socket_close(nsapi_socket_t handle) = 0;

    /** Bind a specific address to a socket
     *
     *  Binding a socket specifies the address and port on which to receive
     *  data. If the IP address is zeroed, only the port is bound.
     *
     *  @param handle   Socket handle
     *  @param address  Local address to bind
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure.
     */
    virtual nsapi_error_t socket_bind(nsapi_socket_t handle, const SocketAddress &address) = 0;

    /** Listen for connections on a TCP socket
     *
     *  Marks the socket as a passive socket that can be used to accept
     *  incoming connections.
     *
     *  @param handle   Socket handle
     *  @param backlog  Number of pending connections that can be queued
     *                  simultaneously
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t socket_listen(nsapi_socket_t handle, int backlog) = 0;

    /** Connects TCP socket to a remote host
     *
     *  Initiates a connection to a remote server specified by the
     *  indicated address.
     *
     *  @param handle   Socket handle
     *  @param address  The SocketAddress of the remote host
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t socket_connect(nsapi_socket_t handle, const SocketAddress &address) = 0;

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
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t socket_accept(nsapi_socket_t server,
                                        nsapi_socket_t *handle, SocketAddress *address = 0) = 0;

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
    virtual nsapi_size_or_error_t socket_send(nsapi_socket_t handle,
                                              const void *data, nsapi_size_t size) = 0;

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
    virtual nsapi_size_or_error_t socket_recv(nsapi_socket_t handle,
                                              void *data, nsapi_size_t size) = 0;

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
    virtual nsapi_size_or_error_t socket_sendto(nsapi_socket_t handle, const SocketAddress &address,
                                                const void *data, nsapi_size_t size) = 0;

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
    virtual nsapi_size_or_error_t socket_recvfrom(nsapi_socket_t handle, SocketAddress *address,
                                                  void *buffer, nsapi_size_t size) = 0;

    /** Send a packet with ancillary data over a UDP socket
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
     *  @param control     Storage for ancillary data
     *  @param control_size   Size of  ancillary data
     *  @return         Number of sent bytes on success, negative error
     *                  code on failure
     */
    virtual nsapi_size_or_error_t socket_sendmsg(nsapi_socket_t handle, const SocketAddress &address,
                                                 const void *data, nsapi_size_t size,
                                                 nsapi_msghdr_t *control, nsapi_size_t control_size) = 0;

    /** Receive a packet with ancillary data over a UDP socket
     *
     *  Receives data and stores the source address in address if address
     *  is not NULL. Returns the number of bytes received into the buffer.
     *
     *  Additional information related to the message can be retrieved with
     *  the control data.
     *
     *  This call is non-blocking. If recvfrom would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param handle   Socket handle
     *  @param address  Destination for the source address or NULL
     *  @param buffer   Destination buffer for data received from the host
     *  @param size     Size of the buffer in bytes
     *  @param control     Storage for ancillary data
     *  @param control_size   Size of  ancillary data
     *  @return         Number of received bytes on success, negative error
     *                  code on failure
     */
    virtual nsapi_size_or_error_t socket_recvmsg(nsapi_socket_t handle, SocketAddress *address,
                                                 void *data, nsapi_size_t size,
                                                 nsapi_msghdr_t *control, nsapi_size_t control_size) = 0;

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
    virtual void socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data) = 0;

    /**  Set stack-specific socket options.
     *
     *  The setsockopt allow an application to pass stack-specific hints
     *  to the underlying stack. For unsupported options,
     *  NSAPI_ERROR_UNSUPPORTED is returned and the socket is unmodified.
     *
     *  @param handle   Socket handle.
     *  @param level    Stack-specific protocol level.
     *  @param optname  Stack-specific option identifier.
     *  @param optval   Option value.
     *  @param optlen   Length of the option value.
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure.
     */
    virtual nsapi_error_t setsockopt(nsapi_socket_t handle, int level,
                                     int optname, const void *optval, unsigned optlen);

    /**  Get stack-specific socket options.
     *
     *  The getstackopt allow an application to retrieve stack-specific hints
     *  from the underlying stack. For unsupported options,
     *  NSAPI_ERROR_UNSUPPORTED is returned and optval is unmodified.
     *
     *  @param handle   Socket handle.
     *  @param level    Stack-specific protocol level.
     *  @param optname  Stack-specific option identifier.
     *  @param optval   Destination for option value.
     *  @param optlen   Length of the option value.
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure.
     */
    virtual nsapi_error_t getsockopt(nsapi_socket_t handle, int level,
                                     int optname, void *optval, unsigned *optlen);

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
    virtual call_in_callback_cb_t get_call_in_callback();

    /** Call a callback after a delay
     *
     *  Call a callback from the network stack context after a delay. If function
     *  returns error callback will not be called.
     *
     *  @param delay    Delay in milliseconds
     *  @param func     Callback to be called
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t call_in(int delay, mbed::Callback<void()> func);
};

inline NetworkStack *_nsapi_create_stack(NetworkStack *stack, std::true_type /* convertible to NetworkStack */)
{
    return stack;
}

inline NetworkStack *_nsapi_create_stack(NetworkInterface *iface, std::false_type /* not convertible to NetworkStack */)
{
    return iface->get_stack();
}

/** Convert a raw nsapi_stack_t object into a C++ NetworkStack object
 *
 *  @param stack    Pointer to an object that can be converted to a stack
 *                  - A raw nsapi_stack_t object
 *                  - A pointer to a network stack
 *                  - A pointer to a network interface
 *  @return         Pointer to the underlying network stack
 */
NetworkStack *nsapi_create_stack(nsapi_stack_t *stack);

template <typename IF>
NetworkStack *nsapi_create_stack(IF *iface)
{
    return _nsapi_create_stack(iface, std::is_convertible<IF *, NetworkStack *>());
}


#endif

/** @} */
