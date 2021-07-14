/* mbed Microcontroller Library
 * Copyright (c) 2017 ARM Limited
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

#ifndef LWIPSTACK_H_
#define LWIPSTACK_H_

#include "lwip/tcpip.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "lwip/api.h"
#include "netif/etharp.h"
#include "lwip/ethip6.h"
#include "netsocket/nsapi_types.h"
#include "netsocket/EMAC.h"
#include "netsocket/L3IP.h"
#include "netsocket/PPP.h"
#include "netsocket/OnboardNetworkStack.h"
#include "LWIPMemoryManager.h"

#if LWIP_IPV6
#include "lwip/ip6_addr.h"
#define IP_ADDR_VALID IP6_ADDR_VALID
#else
#define IP_ADDR_VALID 0
#endif

class LWIP : public OnboardNetworkStack, private mbed::NonCopyable<LWIP> {
public:
    using NetworkStack::get_ip_address;
    static LWIP &get_instance();

    class Interface final : public OnboardNetworkStack::Interface {
    public:
        /** Set IP address to LWIP stack
         *
         * This function can set both IPv4 or IPv6 address to LWIP stack.
         *
         * bringup() can only take one IP address and in dual stack case
         * another IP address can be set using this function.
         *
         * IPv4 or IPv6 address should be in format of https://tools.ietf.org/html/draft-main-ipaddr-text-rep-00.
         *
         * @param    ip         IP address to be used for the interface as IPv4 or IPv6 address string.
         *                      This parameter should not be NULL.
         * @param    netmask    Net mask to be used for the interface as IPv4 address string or NULL.
         *                      NULL value will set this value to 255.255.255.255.
         *                      This parameter will be ignored for IPv6 ip argument.
         * @param    gw         Gateway address to be used for the interface as IPv4 address string or NULL
         *                      NULL value will set this value to 0.0.0.0
         *                      This parameter will be ignored for IPv6 ip argument.
         * @param    ipv6_flag  Provide this flag for IPv6 state flag override. For example, you can set IP6_ADDR_PREFERRED.
         *                      Default value is IP6_ADDR_VALID. For IPv4, this value will be ignored.
         * @return              NSAPI_ERROR_OK on success, or error code
         */
        nsapi_error_t set_ip_address(const char *ip,
                                     const char *netmask,
                                     const char *gw,
                                     uint8_t ipv6_flag = IP_ADDR_VALID
                                    ) override;

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
        nsapi_error_t bringup(bool dhcp, const char *ip,
                              const char *netmask, const char *gw,
                              nsapi_ip_stack_t stack = DEFAULT_STACK,
                              bool blocking = true
                             ) override;

        /** Disconnect interface from the network
         *
         * After this call the network interface is inactive, to use it again user needs to call @a mbed_ipstack_bringup again.
         *
         * @return    NSAPI_ERROR_OK on success, or error code
         */
        nsapi_error_t bringdown() override;

        /** Register callback for status reporting
         *
         *  The specified status callback function will be called on status changes
         *  on the network. The parameters on the callback are the event type and
         *  event-type dependent reason parameter.
         *
         *  @param status_cb The callback for status changes
         */
        void attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb) override;

        /** Get the connection status
         *
         *  @return         The connection status according to ConnectionStatusType
         */
        nsapi_connection_status_t get_connection_status() const override;

        /** Return netif interface name
         *
         * @return       netif name  eg "en0"
        */
        char *get_interface_name(char *buf) override;

        /** Return MAC address of the network interface
         *
         * @return              MAC address as "V:W:X:Y:Z"
         */
        char *get_mac_address(char *buf, nsapi_size_t buflen) override;

        /** @copydoc OnboardNetworkStack::Interface::get_ip_address */
        nsapi_error_t get_ip_address(SocketAddress *address) override;

        /** Get the IPv6 link local address in SocketAddress representation
         *
         *  @address        SocketAddress representation of the link local IPv6 address
         *  @return         NSAPI_ERROR_OK on success, or error code
         */
        nsapi_error_t get_ipv6_link_local_address(SocketAddress *address) override;

        /** Copies netmask of the network interface to user supplied buffer
         *
         * @param    buf        buffer to which netmask will be copied as "W:X:Y:Z"
         * @param    buflen     size of supplied buffer
         * @return              Pointer to a buffer, or NULL if the buffer is too small
         */
        nsapi_error_t get_netmask(SocketAddress *address) override;

        /** Copies gateway address of the network interface to user supplied buffer
         *
         * @param    buf        buffer to which gateway address will be copied as "W:X:Y:Z"
         * @param    buflen     size of supplied buffer
         * @return              Pointer to a buffer, or NULL if the buffer is too small
         */
        nsapi_error_t get_gateway(SocketAddress *address) override;

    private:
        friend class LWIP;

        Interface();

        nsapi_error_t set_dhcp();
        static void netif_link_irq(struct netif *netif);
        static void netif_status_irq(struct netif *netif);
        static Interface *our_if_from_netif(struct netif *netif);
        static void delete_interface(OnboardNetworkStack::Interface **interface_out);
        NetworkInterface *network_if_from_netif_id(int id);
        int netif_id_from_network_if(NetworkInterface *userInterface);


#if LWIP_ETHERNET
        static err_t emac_low_level_output(struct netif *netif, struct pbuf *p);
        void emac_input(net_stack_mem_buf_t *buf);
        void emac_state_change(bool up);
#if LWIP_IGMP
        static err_t emac_igmp_mac_filter(struct netif *netif, const ip4_addr_t *group, enum netif_mac_filter_action action);
#endif
#if LWIP_IPV6_MLD
        static err_t emac_mld_mac_filter(struct netif *netif, const ip6_addr_t *group, enum netif_mac_filter_action action);
#endif

        static err_t emac_if_init(struct netif *netif);
#endif

#if LWIP_L3IP
#if LWIP_IPV4
        static err_t l3ip4_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr);
#endif
#if LWIP_IPV6
        static err_t l3ip6_output(struct netif *netif, struct pbuf *p, const ip6_addr_t *ipaddr);
#endif
        void l3ip_input(net_stack_mem_buf_t *buf);
        void l3ip_state_change(bool up);
#if LWIP_IGMP
        static err_t l3ip_multicast_ipv4_filter(struct netif *netif, const ip4_addr_t *group, enum netif_mac_filter_action action);
#endif
#if LWIP_IPV6_MLD
        static err_t l3ip_multicast_ipv6_filter(struct netif *netif, const ip6_addr_t *group, enum netif_mac_filter_action action);
#endif

        static err_t l3ip_if_init(struct netif *netif);
#endif

#if PPP_SUPPORT
#if PPP_IPV4_SUPPORT && LWIP_IPV4
        static err_t ppp4_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr);
#endif
#if PPP_IPV6_SUPPORT && LWIP_IPV6
        static err_t ppp6_output(struct netif *netif, struct pbuf *p, const ip6_addr_t *ipaddr);
#endif
        void ppp_input(net_stack_mem_buf_t *buf);
        void ppp_state_change(bool up);
        static err_t ppp_if_init(struct netif *netif);
#endif

        union {
#if LWIP_ETHERNET
            EMAC *emac; /**< HW specific emac implementation */
#endif
#if LWIP_L3IP
            L3IP *l3ip; /**<  L3IP implementation */
#endif
#if PPP_SUPPORT
            PPP *ppp; /**< PPP implementation */
#endif
            void *hw; /**< alternative implementation pointer - used for PPP */
        };

        NetworkInterface *user_network_interface;

        mbed_rtos_storage_semaphore_t remove_interface_sem;
        osSemaphoreId_t remove_interface;
        mbed_rtos_storage_semaphore_t linked_sem;
        osSemaphoreId_t linked;
        mbed_rtos_storage_semaphore_t unlinked_sem;
        osSemaphoreId_t unlinked;
        mbed_rtos_storage_semaphore_t has_any_addr_sem;
        osSemaphoreId_t has_any_addr;
#define HAS_ANY_ADDR 1
#if PREF_ADDR_TIMEOUT
        mbed_rtos_storage_semaphore_t has_pref_addr_sem;
        osSemaphoreId_t has_pref_addr;
#define HAS_PREF_ADDR 2
#endif
#if BOTH_ADDR_TIMEOUT
        mbed_rtos_storage_semaphore_t has_both_addr_sem;
        osSemaphoreId_t has_both_addr;
#define HAS_BOTH_ADDR 4
#endif
        char _interface_name[NSAPI_INTERFACE_NAME_MAX_SIZE];
        char has_addr_state;
        nsapi_connection_status_t connected;
        bool dhcp_started;
        bool dhcp_has_to_be_set;
        bool blocking;
        bool ppp_enabled;
        mbed::Callback<void(nsapi_event_t, intptr_t)> client_callback;
        struct netif netif;
        static Interface *list;
        Interface *next;
        LWIPMemoryManager *memory_manager;
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
    nsapi_error_t add_ethernet_interface(EMAC &emac, bool default_if, OnboardNetworkStack::Interface **interface_out, NetworkInterface *user_network_interface = NULL) override;

    /** Register a network interface with the IP stack
     *
     * Connects L3IP layer with the IP stack and initializes all the required infrastructure.
     * This function should be called only once for each available interface.
     *
     * @param      l3ip             L3IP HAL implementation for this network interface
     * @param      default_if       true if the interface should be treated as the default one
     * @param[out] interface_out    pointer to stack interface object controlling the L3IP
     * @return                      NSAPI_ERROR_OK on success, or error code
     */
    nsapi_error_t add_l3ip_interface(L3IP &l3ip, bool default_if, OnboardNetworkStack::Interface **interface_out) override;

    /** Register a PPP interface with the IP stack
     *
     * Connects PPP layer with the IP stack and initializes all the required infrastructure.
     * This function should be called only once for each available interface.
     *
     * This is an internal function that links ppp_lwip.cpp to mbed_ipstack_lwip.cpp,
     * once a driver starts it via the nsapi_ppp.h API.
     *
     * Ultimately the nsapi_ppp.h API will be deprecated, and there will be a
     * mbed_ipstack_add_ppp_interface() replacing nsapi_ppp_connect().
     *
     * @param      pcb              PPP implementation specific user data; will be passed to PPP callbacks
     * @param      default_if       true if the interface should be treated as the default one
     * @param      stack            Allow manual selection of IPv4 and/or IPv6
     * @param[out] interface_out    set to interface handle that must be passed to subsequent mbed_stack calls
     * @return                      NSAPI_ERROR_OK on success, or error code
     */
    nsapi_error_t add_ppp_interface(PPP &ppp, bool default_if, OnboardNetworkStack::Interface **interface_out) override;

    /** Remove a network interface from IP stack
     *
     * Removes layer 3 IP objects,network interface from stack list .
     * @param[out] interface_out    pointer to stack interface object controlling the EMAC
     * @return                      NSAPI_ERROR_OK on success, or error code
     */
    nsapi_error_t remove_ethernet_interface(OnboardNetworkStack::Interface **interface_out) override;

    /** Remove a network interface from IP stack
     *
     * Removes PPP objects,network interface from stack list, and shutdown device driver.
     * @param[out] interface_out    pointer to stack interface object controlling the PPP
     * @return                      NSAPI_ERROR_OK on success, or error code
     */
    nsapi_error_t remove_l3ip_interface(OnboardNetworkStack::Interface **interface_out) override;

    /** Remove a network interface from IP stack
     *
     * Removes PPP objects,network interface from stack list, and shutdown device driver.
     * @param[out] interface_out    pointer to stack interface object controlling the PPP
     * @return                      NSAPI_ERROR_OK on success, or error code
     */
    nsapi_error_t remove_ppp_interface(OnboardNetworkStack::Interface **interface_out) override;

    /** Get a domain name server from a list of servers to query
     *
     *  Returns a DNS server address for a index. If returns error no more
     *  DNS servers to read.
     *
     *  @param index    Index of the DNS server, starts from zero
     *  @param address  Destination for the host address
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t get_dns_server(int index, SocketAddress *address, const char *interface_name) override;

    /** Add a domain name server to list of servers to query
     *
     *  @param address  Destination for the host address
     *  @param interface_name  Network interface name
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    nsapi_error_t add_dns_server(const SocketAddress &address, const char *interface_name) override;

    /** @copydoc NetworkStack::get_ip_address */
    nsapi_error_t get_ip_address(SocketAddress *address) override;

    /** @copydoc NetworkStack::get_ip_address_if */
    nsapi_error_t get_ip_address_if(SocketAddress *address, const char *interface_name) override;

    /** Set the network interface as default one
      */
    void set_default_interface(OnboardNetworkStack::Interface *interface) override;

protected:
    LWIP();

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
    nsapi_error_t socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto) override;

    /** Close the socket
     *
     *  Closes any open connection and deallocates any memory associated
     *  with the socket.
     *
     *  @param handle   Socket handle
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t socket_close(nsapi_socket_t handle) override;

    /** Bind a specific address to a socket
     *
     *  Binding a socket specifies the address and port on which to recieve
     *  data. If the IP address is zeroed, only the port is bound.
     *
     *  @param handle   Socket handle
     *  @param address  Local address to bind
     *  @return         0 on success, negative error code on failure.
     */
    nsapi_error_t socket_bind(nsapi_socket_t handle, const SocketAddress &address) override;

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
    nsapi_error_t socket_listen(nsapi_socket_t handle, int backlog) override;

    /** Connects TCP socket to a remote host
     *
     *  Initiates a connection to a remote server specified by the
     *  indicated address.
     *
     *  @param handle   Socket handle
     *  @param address  The SocketAddress of the remote host
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t socket_connect(nsapi_socket_t handle, const SocketAddress &address) override;

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
    nsapi_error_t socket_accept(nsapi_socket_t server,
                                nsapi_socket_t *handle, SocketAddress *address = 0) override;

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
    nsapi_size_or_error_t socket_send(nsapi_socket_t handle,
                                      const void *data, nsapi_size_t size) override;

    /** Send a packet with ancillary data over a UDP socket
      *
      *  Sends data to the specified address. Returns the number of bytes
      *  sent from the buffer.
      *
      *  This call is non-blocking. If sendto would block,
      *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
      *
      *  @param handle        Socket handle
      *  @param address       The SocketAddress of the remote host
      *  @param data          Buffer of data to send to the host
      *  @param size          Size of the buffer in bytes
      *  @param control       Ancillary data storage
      *  @param control_size  Size of the Ancillary data in bytes
      *  @return              Number of sent bytes on success, negative error
      *                       code on failure
      */
    nsapi_size_or_error_t socket_sendmsg(nsapi_socket_t handle, const SocketAddress &address,
                                         const void *data, nsapi_size_t size,
                                         nsapi_msghdr_t *control, nsapi_size_t control_size) override;

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
    nsapi_size_or_error_t socket_recv(nsapi_socket_t handle,
                                      void *data, nsapi_size_t size) override;

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
    nsapi_size_or_error_t socket_sendto(nsapi_socket_t handle, const SocketAddress &address,
                                        const void *data, nsapi_size_t size) override;

    /** Receive a packet over a UDP socket
     *
     *  Receives data and stores the source address in address if address
     *  is not NULL. Returns the number of bytes received into the buffer.
     *
     *  This call is non-blocking. If recvfrom would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  It uses socket_recvmsg with  zero ancillary data.
     *  @param handle   Socket handle
     *  @param address  Destination for the source address or NULL
     *  @param buffer   Destination buffer for data received from the host
     *  @param size     Size of the buffer in bytes
     *  @return         Number of received bytes on success, negative error
     *                  code on failure
     */
    nsapi_size_or_error_t socket_recvfrom(nsapi_socket_t handle, SocketAddress *address,
                                          void *buffer, nsapi_size_t size) override;

    /** Receive a packet with ancillary data over a UDP socket
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
    *  @param control     Ancillary data storage
    *  @param control_size   Size of the Ancillary data in bytes
    *  @return         Number of received bytes on success, negative error
    *                  code on failure
    */
    nsapi_size_or_error_t socket_recvmsg(nsapi_socket_t handle, SocketAddress *address,
                                         void *data, nsapi_size_t size,
                                         nsapi_msghdr_t *control, nsapi_size_t control_size) override;

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
    void socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data) override;

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
    nsapi_error_t setsockopt(nsapi_socket_t handle, int level,
                             int optname, const void *optval, unsigned optlen) override;

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
    nsapi_error_t getsockopt(nsapi_socket_t handle, int level,
                             int optname, void *optval, unsigned *optlen) override;
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

    struct mbed_lwip_socket {
        bool in_use;

        struct netconn *conn;
        struct pbuf *buf;
        u16_t offset;

        void (*cb)(void *);
        void *data;

        // Track multicast addresses subscribed to by this socket
        nsapi_ip_mreq_t *multicast_memberships;
        uint32_t         multicast_memberships_count;
        uint32_t         multicast_memberships_registry;
    };

    struct lwip_callback {
        unsigned int delay;
        mbed::Callback<void()> callback;
    };

    static nsapi_error_t err_remap(err_t err);
    static bool is_local_addr(const ip_addr_t *ip_addr);
    static const ip_addr_t *get_ip_addr(bool any_addr, const struct netif *netif);
    static const ip_addr_t *get_ipv4_addr(const struct netif *netif);
    static const ip_addr_t *get_ipv6_addr(const struct netif *netif);
    static const ip_addr_t *get_ipv6_link_local_addr(const struct netif *netif);

    static void add_dns_addr(struct netif *lwip_netif, const char *interface_name);

    /* Static arena of sockets */
    struct mbed_lwip_socket arena[MEMP_NUM_NETCONN];
    void arena_init(void);
    struct mbed_lwip_socket *arena_alloc();
    void arena_dealloc(struct mbed_lwip_socket *s);

    static uint32_t next_registered_multicast_member(const struct mbed_lwip_socket *s, uint32_t index)
    {
        while (!(s->multicast_memberships_registry & (0x0001 << index))) {
            index++;
        }
        return index;
    }

    static uint32_t next_free_multicast_member(const struct mbed_lwip_socket *s, uint32_t index)
    {
        while ((s->multicast_memberships_registry & (0x0001 << index))) {
            index++;
        }
        return index;
    }

    static void set_multicast_member_registry_bit(struct mbed_lwip_socket *s, uint32_t index)
    {
        s->multicast_memberships_registry |= (0x0001 << index);
    }

    static void clear_multicast_member_registry_bit(struct mbed_lwip_socket *s, uint32_t index)
    {
        s->multicast_memberships_registry &= ~(0x0001 << index);
    }
    static int32_t find_multicast_member(const struct mbed_lwip_socket *s, const nsapi_ip_mreq_t *imr);

    static void socket_callback(struct netconn *nc, enum netconn_evt eh, u16_t len);

    static void tcpip_init_irq(void *handle);
    static void tcpip_thread_callback(void *ptr);

    char ip_address[40];
    Interface *default_interface;
    LWIPMemoryManager memory_manager;
    osThreadId tcpip_thread_id;
    rtos::Mutex adaptation;
    rtos::EventFlags _event_flag;
    static const int TCP_CLOSED_FLAG = 0x4u;
};

#endif /* LWIPSTACK_H_ */
