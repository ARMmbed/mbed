
/** \addtogroup netsocket */
/** @{*/
/* nsapi.h - The network socket API
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

#ifndef NSAPI_TYPES_H
#define NSAPI_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * The Type of Service provides an indication of the abstract
 * parameters of the quality of service desired.  These parameters are
 * to be used to guide the selection of the actual service parameters
 * when transmitting a datagram through a particular network.  Several
 * networks offer service precedence, which somehow treats high
 * precedence traffic as more important than other traffic (generally
 * by accepting only traffic above a certain precedence at time of high
 * load).  The major choice is a three way tradeoff between low-delay,
 * high-reliability, and high-throughput.
 * The use of the Delay, Throughput, and Reliability indications may
 * increase the cost (in some sense) of the service.  In many networks
 * better performance for one of these parameters is coupled with worse
 * performance on another.  Except for very unusual cases at most two
 * of these three indications should be set.
 */
#define NSAPI_IPTOS_TOS_MASK          0x1E
#define NSAPI_IPTOS_TOS(tos)          ((tos) & IPTOS_TOS_MASK)
#define NSAPI_IPTOS_LOWDELAY          0x10
#define NSAPI_IPTOS_THROUGHPUT        0x08
#define NSAPI_IPTOS_RELIABILITY       0x04
#define NSAPI_IPTOS_LOWCOST           0x02
#define NSAPI_IPTOS_MINCOST           IPTOS_LOWCOST

/*
 * The Network Control precedence designation is intended to be used
 * within a network only.  The actual use and control of that
 * designation is up to each network. The Internetwork Control
 * designation is intended for use by gateway control originators only.
 * If the actual use of these precedence designations is of concern to
 * a particular network, it is the responsibility of that network to
 * control the access to, and use of, those precedence designations.
 */
#define NSAPI_IPTOS_PREC_MASK                 0xe0
#define NSAPI_IPTOS_PREC(tos)                ((tos) & IPTOS_PREC_MASK)
#define NSAPI_IPTOS_PREC_NETCONTROL           0xe0
#define NSAPI_IPTOS_PREC_INTERNETCONTROL      0xc0
#define NSAPI_IPTOS_PREC_CRITIC_ECP           0xa0
#define NSAPI_IPTOS_PREC_FLASHOVERRIDE        0x80
#define NSAPI_IPTOS_PREC_FLASH                0x60
#define NSAPI_IPTOS_PREC_IMMEDIATE            0x40
#define NSAPI_IPTOS_PREC_PRIORITY             0x20
#define NSAPI_IPTOS_PREC_ROUTINE              0x00

/** Enum of standardized error codes
 *
 *  Valid error codes have negative values and may
 *  be returned by any network operation.
 *
 *  @enum nsapi_error
 */
enum nsapi_error {
    NSAPI_ERROR_OK                  =  0,        /*!< no error */
    NSAPI_ERROR_WOULD_BLOCK         = -3001,     /*!< no data is not available but call is non-blocking */
    NSAPI_ERROR_UNSUPPORTED         = -3002,     /*!< unsupported functionality */
    NSAPI_ERROR_PARAMETER           = -3003,     /*!< invalid configuration */
    NSAPI_ERROR_NO_CONNECTION       = -3004,     /*!< not connected to a network */
    NSAPI_ERROR_NO_SOCKET           = -3005,     /*!< socket not available for use */
    NSAPI_ERROR_NO_ADDRESS          = -3006,     /*!< IP address is not known */
    NSAPI_ERROR_NO_MEMORY           = -3007,     /*!< memory resource not available */
    NSAPI_ERROR_NO_SSID             = -3008,     /*!< ssid not found */
    NSAPI_ERROR_DNS_FAILURE         = -3009,     /*!< DNS failed to complete successfully */
    NSAPI_ERROR_DHCP_FAILURE        = -3010,     /*!< DHCP failed to complete successfully */
    NSAPI_ERROR_AUTH_FAILURE        = -3011,     /*!< connection to access point failed */
    NSAPI_ERROR_DEVICE_ERROR        = -3012,     /*!< failure interfacing with the network processor */
    NSAPI_ERROR_IN_PROGRESS         = -3013,     /*!< operation (eg connect) in progress */
    NSAPI_ERROR_ALREADY             = -3014,     /*!< operation (eg connect) already in progress */
    NSAPI_ERROR_IS_CONNECTED        = -3015,     /*!< socket is already connected */
    NSAPI_ERROR_CONNECTION_LOST     = -3016,     /*!< connection lost */
    NSAPI_ERROR_CONNECTION_TIMEOUT  = -3017,     /*!< connection timed out */
    NSAPI_ERROR_ADDRESS_IN_USE      = -3018,     /*!< Address already in use */
    NSAPI_ERROR_TIMEOUT             = -3019,     /*!< operation timed out */
    NSAPI_ERROR_BUSY                = -3020,     /*!< device is busy and cannot accept new operation */
};


/** Enum of connection status types
 *
 *  Valid error codes have negative values.
 *
 *  @enum nsapi_connection_status
 */
typedef enum nsapi_connection_status {
    NSAPI_STATUS_LOCAL_UP           = 0,        /*!< local IP address set */
    NSAPI_STATUS_GLOBAL_UP          = 1,        /*!< global IP address set */
    NSAPI_STATUS_DISCONNECTED       = 2,        /*!< no connection to network */
    NSAPI_STATUS_CONNECTING         = 3,        /*!< connecting to network */
    NSAPI_STATUS_ERROR_UNSUPPORTED  = NSAPI_ERROR_UNSUPPORTED
} nsapi_connection_status_t;


/** Enum of event types
 *
 *  Event callbacks are accompanied with an event-dependent parameter passed as an intptr_t.
 *
 *  @enum nsapi_event
 */
typedef enum nsapi_event {
    NSAPI_EVENT_CONNECTION_STATUS_CHANGE = 0, /*!< network connection status has changed, the parameter = new status (nsapi_connection_status_t) */
    NSAPI_EVENT_CELLULAR_STATUS_BASE     = 0x1000,  /*!< Cellular modem status has changed, See the enum values from enum cellular_connection_status_t in /connectivity/cellular/framework/common/CellularCommon.h */
    NSAPI_EVENT_CELLULAR_STATUS_END      = 0x1FFF   /*!< cellular modem status has changed, See the enum values from enum cellular_connection_status_t in /connectivity/cellular/framework/common/CellularCommon.h */
} nsapi_event_t;


/** Type used to represent error codes
 *
 *  This is a separate type from enum nsapi_error to avoid breaking
 *  compatibility in type-sensitive overloads
 */
typedef signed int nsapi_error_t;

/** Type used to represent the size of data passed through sockets
 */
typedef unsigned int nsapi_size_t;

/** Type used to represent either a size or error passed through sockets
 *
 *  A valid nsapi_size_or_error_t is either a non-negative size or a
 *  negative error code from the nsapi_error_t
 */
typedef signed int nsapi_size_or_error_t;

/** Type used to represent either a value or error
 *
 *  A valid nsapi_value_or_error_t is either a non-negative value or a
 *  negative error code from the nsapi_error_t
 */
typedef signed int nsapi_value_or_error_t;

/** Enum of encryption types
 *
 *  The security type specifies a particular security to use when
 *  connected to a WiFi network
 */
typedef enum nsapi_security {
    NSAPI_SECURITY_NONE         = 0x0,      /*!< open access point */
    NSAPI_SECURITY_WEP          = 0x1,      /*!< phrase conforms to WEP */
    NSAPI_SECURITY_WPA          = 0x2,      /*!< phrase conforms to WPA */
    NSAPI_SECURITY_WPA2         = 0x3,      /*!< phrase conforms to WPA2 */
    NSAPI_SECURITY_WPA_WPA2     = 0x4,      /*!< phrase conforms to WPA/WPA2 */
    NSAPI_SECURITY_PAP          = 0x5,      /*!< phrase conforms to PPP authentication context */
    NSAPI_SECURITY_CHAP         = 0x6,      /*!< phrase conforms to PPP authentication context */
    NSAPI_SECURITY_EAP_TLS      = 0x7,      /*!< phrase conforms to EAP-TLS */
    NSAPI_SECURITY_PEAP         = 0x8,      /*!< phrase conforms to PEAP */
    NSAPI_SECURITY_WPA2_ENT     = 0x9,      /*!< phrase conforms to WPA2-AES and WPA-TKIP with enterprise security */
    NSAPI_SECURITY_WPA3         = 0xA,      /*!< phrase conforms to WPA3 */
    NSAPI_SECURITY_WPA3_WPA2    = 0xB,      /*!< phrase conforms to WPA3_WPA2 */
    NSAPI_SECURITY_UNKNOWN      = 0xFF,     /*!< unknown/unsupported security in scan results */
} nsapi_security_t;

/** Size of  2 char network interface name from driver
 */
#define NSAPI_INTERFACE_PREFIX_SIZE 2

/** Maximum size of network interface name
 */
#define NSAPI_INTERFACE_NAME_MAX_SIZE 6

/** Maximum size of IP address representation
 */
#define NSAPI_IP_SIZE NSAPI_IPv6_SIZE

/** Maximum number of bytes for IP address
 */
#define NSAPI_IP_BYTES NSAPI_IPv6_BYTES

/** Maximum size of MAC address representation
 */
#define NSAPI_MAC_SIZE 18

/** Maximum number of bytes for MAC address
 */
#define NSAPI_MAC_BYTES 6

/** Size of IPv4 representation
 */
#define NSAPI_IPv4_SIZE 16

/** Number of bytes in IPv4 address
 */
#define NSAPI_IPv4_BYTES 4

/** Size of IPv6 representation
 */
#define NSAPI_IPv6_SIZE 40

/** Number of bytes in IPv6 address
 */
#define NSAPI_IPv6_BYTES 16

/** Enum of IP address versions
 *
 *  The IP version specifies the type of an IP address.
 *
 *  @enum nsapi_version
 */
typedef enum nsapi_version {
    NSAPI_UNSPEC,   /*!< Address is unspecified */
    NSAPI_IPv4,     /*!< Address is IPv4 */
    NSAPI_IPv6,     /*!< Address is IPv6 */
} nsapi_version_t;

/** IP address structure for passing IP addresses by value
 */
typedef struct nsapi_addr {
    /** IP version
     *  - NSAPI_IPv4
     *  - NSAPI_IPv6
     *  - NSAPI_UNSPEC
     */
    nsapi_version_t version;

    /** IP address
     *  The raw bytes of the IP address stored in big-endian format
     */
    uint8_t bytes[NSAPI_IP_BYTES];
} nsapi_addr_t;


/** Opaque handle for network sockets
 */
typedef void *nsapi_socket_t;


/** Enum of socket protocols
 *
 *  The socket protocol specifies a particular protocol to
 *  be used with a newly created socket.
 *
 *  @enum nsapi_protocol
 */
typedef enum nsapi_protocol {
    NSAPI_TCP, /*!< Socket is of TCP type */
    NSAPI_UDP, /*!< Socket is of UDP type */
    NSAPI_ICMP, /*!< Socket is of ICMP type */
} nsapi_protocol_t;

/** Enum of standardized stack option levels
 *  for use with NetworkStack::setstackopt and getstackopt.
 *
 *  @enum nsapi_stack_level
 */
typedef enum nsapi_stack_level {
    NSAPI_STACK     = 5000, /*!< Stack option level - see nsapi_stack_option_t for options */
} nsapi_stack_level_t;

/** Enum of standardized stack option names for level NSAPI_STACK
 *  of NetworkStack::setstackopt and getstackopt.
 *
 *  These options may not be supported on all stacks, in which
 *  case NSAPI_ERROR_UNSUPPORTED may be returned.
 *
 *  @enum nsapi_stack_option
 */
typedef enum nsapi_stack_option {
    NSAPI_IPV4_MRU, /*!< Sets/gets size of largest IPv4 fragmented datagram to reassemble */
    NSAPI_IPV6_MRU, /*!< Sets/gets size of largest IPv6 fragmented datagram to reassemble */
} nsapi_stack_option_t;

/** Enum of standardized socket option levels
 *  for use with Socket::setsockopt and getsockopt.
 *
 *  @enum nsapi_socket_level
 */
typedef enum nsapi_socket_level {
    NSAPI_SOCKET    = 7000, /*!< Socket option level - see nsapi_socket_option_t for options */
} nsapi_socket_level_t;

/** Enum of standardized socket option names for level NSAPI_SOCKET
 *  of Socket::setsockopt and getsockopt.
 *
 *  These options may not be supported on all stacks, in which
 *  case NSAPI_ERROR_UNSUPPORTED may be returned.
 *
 *  @enum nsapi_socket_option
 */
typedef enum nsapi_socket_option {
    NSAPI_REUSEADDR,         /*!< Allow bind to reuse local addresses */
    NSAPI_KEEPALIVE,         /*!< Enables sending of keepalive messages */
    NSAPI_KEEPIDLE,          /*!< Sets timeout value to initiate keepalive */
    NSAPI_KEEPINTVL,         /*!< Sets timeout value for keepalive */
    NSAPI_LINGER,            /*!< Keeps close from returning until queues empty */
    NSAPI_SNDBUF,            /*!< Sets send buffer size */
    NSAPI_RCVBUF,            /*!< Sets recv buffer size */
    NSAPI_ADD_MEMBERSHIP,    /*!< Add membership to multicast address */
    NSAPI_DROP_MEMBERSHIP,   /*!< Drop membership to multicast address */
    NSAPI_BIND_TO_DEVICE,    /*!< Bind socket network interface name*/
    NSAPI_LATENCY,           /*!< Read estimated latency to destination */
    NSAPI_STAGGER,           /*!< Read estimated stagger value to destination */
    NSAPI_IPTOS,             /*!< Set IP type of service to set specific precedence */
    NSAPI_BROADCAST          /*!< Set broadcast flag for UDP socket */
} nsapi_socket_option_t;

typedef enum nsapi_tlssocket_level {
    NSAPI_TLSSOCKET_LEVEL   = 7099, /*!< TLSSocket option level - see nsapi_tlssocket_option_t for options*/
} nsapi_tlssocket_level_t;

typedef enum nsapi_tlssocket_option {
    NSAPI_TLSSOCKET_SET_HOSTNAME,   /*!< Set host name */
    NSAPI_TLSSOCKET_SET_CACERT,     /*!< Set server CA certificate */
    NSAPI_TLSSOCKET_SET_CLCERT,     /*!< Set client certificate */
    NSAPI_TLSSOCKET_SET_CLKEY,      /*!< Set client key */
    NSAPI_TLSSOCKET_ENABLE          /*!< Enable TLSSocket */
} nsapi_tlssocket_option_t;

/** Supported IP protocol versions of IP stack
 *
 *  @enum nsapi_ip_stack
 */
typedef enum nsapi_ip_stack {
    DEFAULT_STACK = 0,
    IPV4_STACK,
    IPV6_STACK,
    IPV4V6_STACK
} nsapi_ip_stack_t;

/* Backwards compatibility - previously didn't distinguish stack and socket options */
typedef nsapi_socket_level_t nsapi_level_t;
typedef nsapi_socket_option_t nsapi_option_t;

/** nsapi_wifi_ap structure
 *
 *  Structure representing a WiFi Access Point
 */
typedef struct nsapi_wifi_ap {
    char ssid[33]; /* 32 is what 802.11 defines as longest possible name; +1 for the \0 */
    uint8_t bssid[6];
    nsapi_security_t security;
    int8_t rssi;
    uint8_t channel;
} nsapi_wifi_ap_t;


/** nsapi_stack structure
 *
 *  Stack structure representing a specific instance of a stack.
 */
typedef struct nsapi_stack {
    /** Network stack operation table
     *
     *  Provides access to the underlying api of the stack. This is not
     *  flattened into the nsapi_stack to allow allocation in read-only
     *  memory.
     */
    const struct nsapi_stack_api *stack_api;

    /** Opaque handle for network stacks
     */
    void *stack;

    // Internal nsapi buffer
    unsigned _stack_buffer[16];
} nsapi_stack_t;

/** nsapi_ip_mreq structure
 */
typedef struct nsapi_ip_mreq {
    nsapi_addr_t imr_multiaddr; /* IP multicast address of group */
    nsapi_addr_t imr_interface; /* local IP address of interface */
} nsapi_ip_mreq_t;

/** nsapi_latency_req structure
 */
typedef struct nsapi_latency_req {
    uint8_t addr[16];   /* [IN] Destination address to estimate latency */
    uint32_t latency;   /* [OUT] Latency value */
} nsapi_latency_req_t;

/** nsapi_stagger_req structure
 */
typedef struct nsapi_stagger_req {
    uint8_t addr[16];       /* [IN] Destination address to estimate stagger */
    uint16_t data_amount;   /* [IN] Amount of data to be sent in kilobytes */
    uint16_t stagger_min;   /* [OUT] Minimum stagger value in seconds */
    uint16_t stagger_max;   /* [OUT] Maximum stagger value in seconds */
    uint16_t stagger_rand;  /* [OUT] Randomized stagger value in seconds */
} nsapi_stagger_req_t;

/** nsapi_stack_api structure
 *
 *  Common api structure for network stack operations. A network stack
 *  can provide a nsapi_stack_api structure filled out with the
 *  appropriate implementation.
 *
 *  Unsupported operations can be left as null pointers.
 */
typedef struct nsapi_stack_api {
    /** Get the local IP address
     *
     *  @param stack    Stack handle
     *  @return         Local IP Address or null address if not connected
     */
    nsapi_addr_t (*get_ip_address)(nsapi_stack_t *stack);

    /** Translates a hostname to an IP address
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  @param stack    Stack handle
     *  @param addr     Destination for the host IP address
     *  @param host     Hostname to resolve
     *  @param version  Address family
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*gethostbyname)(nsapi_stack_t *stack, const char *host, nsapi_addr_t *addr, nsapi_version_t version);

    /** Add a domain name server to list of servers to query
     *
     *  @param addr     Destination for the host address
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*add_dns_server)(nsapi_stack_t *stack, nsapi_addr_t addr);

    /** Set stack-specific stack options
     *
     *  The setstackopt allow an application to pass stack-specific hints
     *  to the underlying stack. For unsupported options,
     *  NSAPI_ERROR_UNSUPPORTED is returned and the stack is unmodified.
     *
     *  @param stack    Stack handle
     *  @param level    Stack-specific protocol level
     *  @param optname  Stack-specific option identifier
     *  @param optval   Option value
     *  @param optlen   Length of the option value
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*setstackopt)(nsapi_stack_t *stack, int level,
                                 int optname, const void *optval, unsigned optlen);

    /** Get stack-specific stack options
     *
     *  The getstackopt allow an application to retrieve stack-specific hints
     *  from the underlying stack. For unsupported options,
     *  NSAPI_ERROR_UNSUPPORTED is returned and optval is unmodified.
     *
     *  @param stack    Stack handle
     *  @param level    Stack-specific protocol level
     *  @param optname  Stack-specific option identifier
     *  @param optval   Destination for option value
     *  @param optlen   Length of the option value
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*getstackopt)(nsapi_stack_t *stack, int level,
                                 int optname, void *optval, unsigned *optlen);

    /** Opens a socket
     *
     *  Creates a network socket and stores it in the specified handle.
     *  The handle must be passed to following calls on the socket.
     *
     *  A stack may have a finite number of sockets, in this case
     *  NSAPI_ERROR_NO_SOCKET is returned if no socket is available.
     *
     *  @param stack    Stack context
     *  @param socket   Destination for the handle to a newly created socket
     *  @param proto    Protocol of socket to open, NSAPI_TCP or NSAPI_UDP
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*socket_open)(nsapi_stack_t *stack, nsapi_socket_t *socket,
                                 nsapi_protocol_t proto);

    /** Close the socket
     *
     *  Closes any open connection and deallocates any memory associated
     *  with the socket.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*socket_close)(nsapi_stack_t *stack, nsapi_socket_t socket);

    /** Bind a specific address to a socket
     *
     *  Binding a socket specifies the address and port on which to receive
     *  data. If the IP address is zeroed, only the port is bound.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @param addr     Local address to bind, may be null
     *  @param port     Local port to bind
     *  @return         0 on success, negative error code on failure.
     */
    nsapi_error_t (*socket_bind)(nsapi_stack_t *stack, nsapi_socket_t socket,
                                 nsapi_addr_t addr, uint16_t port);

    /** Listen for connections on a TCP socket
     *
     *  Marks the socket as a passive socket that can be used to accept
     *  incoming connections.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @param backlog  Number of pending connections that can be queued
     *                  simultaneously
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*socket_listen)(nsapi_stack_t *stack, nsapi_socket_t socket, int backlog);

    /** Connects TCP socket to a remote host
     *
     *  Initiates a connection to a remote server specified by the
     *  indicated address.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @param addr     The address of the remote host
     *  @param port     The port of the remote host
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*socket_connect)(nsapi_stack_t *stack, nsapi_socket_t socket,
                                    nsapi_addr_t addr, uint16_t port);

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
     *  @param stack    Stack handle
     *  @param server   Socket handle to server to accept from
     *  @param socket   Destination for a handle to the newly created socket
     *  @param addr     Destination for the address of the remote host
     *  @param port     Destination for the port of the remote host
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*socket_accept)(nsapi_stack_t *stack, nsapi_socket_t server,
                                   nsapi_socket_t *socket, nsapi_addr_t *addr, uint16_t *port);

    /** Send data over a TCP socket
     *
     *  The socket must be connected to a remote host. Returns the number of
     *  bytes sent from the buffer.
     *
     *  This call is non-blocking. If send would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @param data     Buffer of data to send to the host
     *  @param size     Size of the buffer in bytes
     *  @return         Number of sent bytes on success, negative error
     *                  code on failure
     */
    nsapi_size_or_error_t (*socket_send)(nsapi_stack_t *stack, nsapi_socket_t socket,
                                         const void *data, nsapi_size_t size);

    /** Receive data over a TCP socket
     *
     *  The socket must be connected to a remote host. Returns the number of
     *  bytes received into the buffer.
     *
     *  This call is non-blocking. If recv would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @param data     Destination buffer for data received from the host
     *  @param size     Size of the buffer in bytes
     *  @return         Number of received bytes on success, negative error
     *                  code on failure
     */
    nsapi_size_or_error_t (*socket_recv)(nsapi_stack_t *stack, nsapi_socket_t socket,
                                         void *data, nsapi_size_t size);

    /** Send a packet over a UDP socket
     *
     *  Sends data to the specified address. Returns the number of bytes
     *  sent from the buffer.
     *
     *  This call is non-blocking. If sendto would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @param addr     The address of the remote host
     *  @param port     The port of the remote host
     *  @param data     Buffer of data to send to the host
     *  @param size     Size of the buffer in bytes
     *  @return         Number of sent bytes on success, negative error
     *                  code on failure
     */
    nsapi_size_or_error_t (*socket_sendto)(nsapi_stack_t *stack, nsapi_socket_t socket,
                                           nsapi_addr_t addr, uint16_t port, const void *data, nsapi_size_t size);

    /** Receive a packet over a UDP socket
     *
     *  Receives data and stores the source address in address if address
     *  is not NULL. Returns the number of bytes received into the buffer.
     *
     *  This call is non-blocking. If recvfrom would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @param addr     Destination for the address of the remote host
     *  @param port     Destination for the port of the remote host
     *  @param data     Destination buffer for data received from the host
     *  @param size     Size of the buffer in bytes
     *  @return         Number of received bytes on success, negative error
     *                  code on failure
     */
    nsapi_size_or_error_t (*socket_recvfrom)(nsapi_stack_t *stack, nsapi_socket_t socket,
                                             nsapi_addr_t *addr, uint16_t *port, void *buffer, nsapi_size_t size);

    /** Register a callback on state change of the socket
     *
     *  The specified callback will be called on state changes such as when
     *  the socket can recv/send/accept successfully and on when an error
     *  occurs. The callback may also be called spuriously without reason.
     *
     *  The callback may be called in an interrupt context and should not
     *  perform expensive operations such as recv/send calls.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @param callback Function to call on state change
     *  @param data     Argument to pass to callback
     */
    void (*socket_attach)(nsapi_stack_t *stack, nsapi_socket_t socket,
                          void (*callback)(void *), void *data);

    /** Set stack-specific socket options
     *
     *  The setsockopt allow an application to pass stack-specific hints
     *  to the underlying stack. For unsupported options,
     *  NSAPI_ERROR_UNSUPPORTED is returned and the socket is unmodified.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @param level    Stack-specific protocol level
     *  @param optname  Stack-specific option identifier
     *  @param optval   Option value
     *  @param optlen   Length of the option value
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*setsockopt)(nsapi_stack_t *stack, nsapi_socket_t socket, int level,
                                int optname, const void *optval, unsigned optlen);

    /** Get stack-specific socket options
     *
     *  The getstackopt allow an application to retrieve stack-specific hints
     *  from the underlying stack. For unsupported options,
     *  NSAPI_ERROR_UNSUPPORTED is returned and optval is unmodified.
     *
     *  @param stack    Stack handle
     *  @param socket   Socket handle
     *  @param level    Stack-specific protocol level
     *  @param optname  Stack-specific option identifier
     *  @param optval   Destination for option value
     *  @param optlen   Length of the option value
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t (*getsockopt)(nsapi_stack_t *stack, nsapi_socket_t socket, int level,
                                int optname, void *optval, unsigned *optlen);
} nsapi_stack_api_t;


#ifdef __cplusplus
}
#endif

#endif

/** @}*/
