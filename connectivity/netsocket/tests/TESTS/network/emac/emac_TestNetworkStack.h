/*
 * Copyright (c) 2018, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EMAC_TEST_NETWORK_STACK_H
#define EMAC_TEST_NETWORK_STACK_H

#include "netsocket/nsapi_types.h"
#include "netsocket/EMAC.h"
#include "netsocket/OnboardNetworkStack.h"

#include "emac_TestMemoryManager.h"

class EmacTestNetworkStack : public OnboardNetworkStack, private mbed::NonCopyable<EmacTestNetworkStack> {
public:

    static EmacTestNetworkStack &get_instance();

    EmacTestNetworkStack();
    virtual ~EmacTestNetworkStack() {}

    class Interface : public OnboardNetworkStack::Interface {
    public:

        static Interface &get_instance();

        /** Connect the interface to the network
         *
         * Sets up a connection on specified network interface, using DHCP or provided network details. If the @a dhcp is set to
         * true all the remaining parameters are ignored.
         *
         * @param    dhcp       true if the network details should be acquired using DHCP
         * @param    ip         IP address to be used for the interface as "W:X:Y:Z" or NULL
         * @param    netmask    Net mask to be used for the interface as "W:X:Y:Z" or NULL
         * @param    gw         Gateway address to be used for the interface as "W:X:Y:Z" or NULL
         * @param    stack      Allow manual selection of IPv4 and/or IPv6.
         * @param    blocking   Specify whether bringup blocks for connection completion.
         * @return              NSAPI_ERROR_OK on success, or error code
         */
        virtual nsapi_error_t bringup(bool dhcp, const char *ip,
                                      const char *netmask, const char *gw,
                                      nsapi_ip_stack_t stack = DEFAULT_STACK,
                                      bool blocking = true
                                     );

        /** Disconnect interface from the network
         *
         * After this call the network interface is inactive, to use it again user needs to call @a mbed_ipstack_bringup again.
         *
         * @return    NSAPI_ERROR_OK on success, or error code
         */
        virtual nsapi_error_t bringdown();

        /** Register callback for status reporting
         *
         *  The specified status callback function will be called on status changes
         *  on the network. The parameters on the callback are the event type and
         *  event-type dependent reason parameter.
         *
         *  @param status_cb The callback for status changes
         */
        virtual void attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb);

        /** Get the connection status
         *
         *  @return         The connection status according to ConnectionStatusType
         */
        virtual nsapi_connection_status_t get_connection_status() const;

        /** Return MAC address of the network interface
         *
         * @return              MAC address as "V:W:X:Y:Z"
         */
        virtual char *get_mac_address(char *buf, nsapi_size_t buflen);

        /** Set MAC address on the network interface
         *
         *  @param          mac_addr Buffer containing the MAC address in hexadecimal format.
         *  @param          addr_len Length of provided buffer in bytes (6 or 8)
         *  @retval         NSAPI_ERROR_OK on success
         *  @retval         NSAPI_ERROR_UNSUPPORTED if this feature is not supported
         *  @retval         NSAPI_ERROR_PARAMETER if address is not valid
         *  @retval         NSAPI_ERROR_BUSY if address can't be set.
         */
        virtual nsapi_error_t set_mac_address(uint8_t *mac_addr, nsapi_size_t addr_len);

        /** Copies IP address of the network interface to user supplied buffer
         *
         * @param    buf        buffer to which IP address will be copied as "W:X:Y:Z"
         * @param    buflen     size of supplied buffer
         * @return              Pointer to a buffer, or NULL if the buffer is too small
         */
        virtual nsapi_error_t get_ip_address(SocketAddress *address);

        MBED_DEPRECATED_SINCE("mbed-os-5.15", "String-based APIs are deprecated")
        virtual char *get_ip_address(char *buf, nsapi_size_t buflen);

        /** Copies netmask of the network interface to user supplied buffer
         *
         * @param    buf        buffer to which netmask will be copied as "W:X:Y:Z"
         * @param    buflen     size of supplied buffer
         * @return              Pointer to a buffer, or NULL if the buffer is too small
         */
        virtual nsapi_error_t get_netmask(SocketAddress *address);

        MBED_DEPRECATED_SINCE("mbed-os-5.15", "String-based APIs are deprecated")
        virtual char *get_netmask(char *buf, nsapi_size_t buflen);

        /** Copies gateway address of the network interface to user supplied buffer
         *
         * @param    buf        buffer to which gateway address will be copied as "W:X:Y:Z"
         * @param    buflen     size of supplied buffer
         * @return              Pointer to a buffer, or NULL if the buffer is too small
         */
        virtual nsapi_error_t get_gateway(SocketAddress *address);

        MBED_DEPRECATED_SINCE("mbed-os-5.15", "String-based APIs are deprecated")
        virtual char *get_gateway(char *buf, nsapi_size_t buflen);

    private:
        friend EmacTestNetworkStack;

        Interface();

        EMAC *m_emac;
    };

    /** Register a network interface with the IP stack
     *
     * Connects EMAC layer with the IP stack and initializes all the required infrastructure.
     * This function should be called only once for each available interface.
     *
     * @param      emac             EMAC HAL implementation for this network interface
     * @param      default_if       true if the interface should be treated as the default one
     * @param[out] interface_out    pointer to stack interface object controlling the EMAC
     * @return                      NSAPI_ERROR_OK on success, or error code
     */
    virtual nsapi_error_t add_ethernet_interface(EMAC &emac, bool default_if, OnboardNetworkStack::Interface **interface_out, NetworkInterface *user_network_interface);

    /** Translates a hostname to an IP address with specific version
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  @param host     Hostname to resolve
     *  @param address  Destination for the host SocketAddress
     *  @param version  IP version of address to resolve, NSAPI_UNSPEC indicates
     *                  version is chosen by the stack (defaults to NSAPI_UNSPEC)
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t gethostbyname(const char *host,
                                        SocketAddress *address, nsapi_version_t version = NSAPI_UNSPEC);

    /** Add a domain name server to list of servers to query
     *
     *  @param address  Destination for the host address
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t add_dns_server(const SocketAddress &address);

protected:

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
    virtual nsapi_error_t socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto);

    /** Close the socket
     *
     *  Closes any open connection and deallocates any memory associated
     *  with the socket.
     *
     *  @param handle   Socket handle
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t socket_close(nsapi_socket_t handle);

    /** Bind a specific address to a socket
     *
     *  Binding a socket specifies the address and port on which to receive
     *  data. If the IP address is zeroed, only the port is bound.
     *
     *  @param handle   Socket handle
     *  @param address  Local address to bind
     *  @return         0 on success, negative error code on failure.
     */
    virtual nsapi_error_t socket_bind(nsapi_socket_t handle, const SocketAddress &address);

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
    virtual nsapi_error_t socket_listen(nsapi_socket_t handle, int backlog);

    /** Connects TCP socket to a remote host
     *
     *  Initiates a connection to a remote server specified by the
     *  indicated address.
     *
     *  @param handle   Socket handle
     *  @param address  The SocketAddress of the remote host
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t socket_connect(nsapi_socket_t handle, const SocketAddress &address);

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
    virtual nsapi_error_t socket_accept(nsapi_socket_t server,
                                        nsapi_socket_t *handle, SocketAddress *address = 0);

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
                                              const void *data, nsapi_size_t size);

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
                                              void *data, nsapi_size_t size);

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
                                                const void *data, nsapi_size_t size);

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
                                                  void *buffer, nsapi_size_t size);

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
    virtual void socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data);

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
    virtual nsapi_error_t setsockopt(nsapi_socket_t handle, int level,
                                     int optname, const void *optval, unsigned optlen);

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
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t call_in(int delay, mbed::Callback<void()> func);

    Interface *m_interface;
};

#endif /* EMAC_TEST_NETWORK_STACK_H */
