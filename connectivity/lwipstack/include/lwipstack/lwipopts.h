/* Copyright (C) 2012 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Workaround for Linux timeval
#if defined (TOOLCHAIN_GCC)
#define LWIP_TIMEVAL_PRIVATE 0
#include <sys/time.h>
#endif
#include "nsapi_types.h"
#include "mbed_retarget.h"

// Operating System
#define NO_SYS                      0

#if !MBED_CONF_LWIP_IPV4_ENABLED && !MBED_CONF_LWIP_IPV6_ENABLED
#error "Either IPv4 or IPv6 must be enabled."
#endif

#define LWIP_IPV4                   MBED_CONF_LWIP_IPV4_ENABLED

#define LWIP_IPV6                   MBED_CONF_LWIP_IPV6_ENABLED

#define LWIP_PROVIDE_ERRNO 0

// On dual stack configuration how long to wait for both or preferred stack
// addresses before completing bring up.
#if LWIP_IPV4 && LWIP_IPV6
#if MBED_CONF_LWIP_ADDR_TIMEOUT_MODE
#define BOTH_ADDR_TIMEOUT           MBED_CONF_LWIP_ADDR_TIMEOUT
#define PREF_ADDR_TIMEOUT           0
#else
#define PREF_ADDR_TIMEOUT           MBED_CONF_LWIP_ADDR_TIMEOUT
#define BOTH_ADDR_TIMEOUT           0
#endif
#else
#define PREF_ADDR_TIMEOUT           0
#define BOTH_ADDR_TIMEOUT           0
#endif


#define DHCP_TIMEOUT                MBED_CONF_LWIP_DHCP_TIMEOUT

#define LINK_TIMEOUT                60

#define PREF_IPV4                   1
#define PREF_IPV6                   2

#if MBED_CONF_LWIP_IP_VER_PREF == 6
#define IP_VERSION_PREF             PREF_IPV6
#elif MBED_CONF_LWIP_IP_VER_PREF == 4
#define IP_VERSION_PREF             PREF_IPV4
#else
#error "Either IPv4 or IPv6 must be preferred."
#endif

#undef  LWIP_DEBUG
#if MBED_CONF_LWIP_DEBUG_ENABLED
#define LWIP_DEBUG                  1
#endif

#if NO_SYS == 0
#include "cmsis_os2.h"

#define SYS_LIGHTWEIGHT_PROT        1

#define LWIP_RAW                    MBED_CONF_LWIP_RAW_SOCKET_ENABLED

#define MEMP_NUM_TCPIP_MSG_INPKT    MBED_CONF_LWIP_MEMP_NUM_TCPIP_MSG_INPKT

// Thread stacks use 8-byte alignment
#define LWIP_ALIGN_UP(pos, align) ((pos) % (align) ? (pos) +  ((align) - (pos) % (align)) : (pos))

#ifdef LWIP_DEBUG
// For LWIP debug, double the stack
#define TCPIP_THREAD_STACKSIZE      LWIP_ALIGN_UP(MBED_CONF_LWIP_TCPIP_THREAD_STACKSIZE*2, 8)
#elif MBED_DEBUG
// When debug is enabled on the build increase stack 25 percent
#define TCPIP_THREAD_STACKSIZE      LWIP_ALIGN_UP(MBED_CONF_LWIP_TCPIP_THREAD_STACKSIZE + MBED_CONF_LWIP_TCPIP_THREAD_STACKSIZE / 4, 8)
#else
#define TCPIP_THREAD_STACKSIZE      LWIP_ALIGN_UP(MBED_CONF_LWIP_TCPIP_THREAD_STACKSIZE, 8)
#endif

// Thread priority (osPriorityNormal by default)
#define TCPIP_THREAD_PRIO           (MBED_CONF_LWIP_TCPIP_THREAD_PRIORITY)

#ifdef LWIP_DEBUG
#define DEFAULT_THREAD_STACKSIZE    LWIP_ALIGN_UP(MBED_CONF_LWIP_DEFAULT_THREAD_STACKSIZE*2, 8)
#else
#define DEFAULT_THREAD_STACKSIZE    LWIP_ALIGN_UP(MBED_CONF_LWIP_DEFAULT_THREAD_STACKSIZE, 8)
#endif

#define MEMP_NUM_SYS_TIMEOUT        16

#define sys_msleep(ms) sys_msleep(ms)

#endif

// 32-bit alignment
#define MEM_ALIGNMENT               4

#define LWIP_RAM_HEAP_POINTER       lwip_ram_heap

// Number of simultaneously queued TCP segments.
#define MEMP_NUM_TCP_SEG            MBED_CONF_LWIP_MEMP_NUM_TCP_SEG

// TCP Maximum segment size.
#define TCP_MSS                     MBED_CONF_LWIP_TCP_MSS

// TCP sender buffer space (bytes).
#define TCP_SND_BUF                 MBED_CONF_LWIP_TCP_SND_BUF

// TCP sender buffer space (bytes).
#define TCP_WND                     MBED_CONF_LWIP_TCP_WND

#define TCP_MAXRTX                  MBED_CONF_LWIP_TCP_MAXRTX

#define TCP_SYNMAXRTX               MBED_CONF_LWIP_TCP_SYNMAXRTX

// Number of pool pbufs.
// Each requires 684 bytes of RAM (if MSS=536 and PBUF_POOL_BUFSIZE defaulting to be based on MSS)
#define PBUF_POOL_SIZE              MBED_CONF_LWIP_PBUF_POOL_SIZE

#ifdef MBED_CONF_LWIP_PBUF_POOL_BUFSIZE
#undef PBUF_POOL_BUFSIZE
#define PBUF_POOL_BUFSIZE           LWIP_MEM_ALIGN_SIZE(MBED_CONF_LWIP_PBUF_POOL_BUFSIZE)
#else
#ifndef PBUF_POOL_BUFSIZE
#if LWIP_IPV6
#define PBUF_POOL_BUFSIZE               LWIP_MEM_ALIGN_SIZE(TCP_MSS+20+40+PBUF_LINK_ENCAPSULATION_HLEN+PBUF_LINK_HLEN)
#elif LWIP_IPV4
#define PBUF_POOL_BUFSIZE               LWIP_MEM_ALIGN_SIZE(TCP_MSS+20+20+PBUF_LINK_ENCAPSULATION_HLEN+PBUF_LINK_HLEN)
#endif
#endif
#endif

#define MEM_SIZE                    MBED_CONF_LWIP_MEM_SIZE

// One tcp_pcb_listen is needed for each TCP server.
// Each requires 72 bytes of RAM.
#define MEMP_NUM_TCP_PCB_LISTEN     MBED_CONF_LWIP_TCP_SERVER_MAX

// One is tcp_pcb needed for each TCPSocket.
// Each requires 196 bytes of RAM.
#define MEMP_NUM_TCP_PCB            MBED_CONF_LWIP_TCP_SOCKET_MAX

// One udp_pcb is needed for each UDPSocket.
// Each requires 84 bytes of RAM (total rounded to multiple of 512).
#define MEMP_NUM_UDP_PCB            MBED_CONF_LWIP_UDP_SOCKET_MAX

// Number of non-pool pbufs.
// Each requires 92 bytes of RAM.
#define MEMP_NUM_PBUF               MBED_CONF_LWIP_NUM_PBUF

// Each netbuf requires 64 bytes of RAM.
#define MEMP_NUM_NETBUF             MBED_CONF_LWIP_NUM_NETBUF

// One netconn is needed for each UDPSocket or TCPSocket.
// Each requires 236 bytes of RAM (total rounded to multiple of 512).
#define MEMP_NUM_NETCONN            MBED_CONF_LWIP_SOCKET_MAX

#if MBED_CONF_LWIP_TCP_ENABLED
#define LWIP_TCP                    1
#define TCP_OVERSIZE                0
#define LWIP_TCP_KEEPALIVE          1

#define TCP_CLOSE_TIMEOUT            MBED_CONF_LWIP_TCP_CLOSE_TIMEOUT

#else
#define LWIP_TCP                    0
#endif

#define LWIP_DNS                    1
// Only DNS address storage is enabled
#define LWIP_FULL_DNS               0
#define LWIP_SOCKET                 0

#define SO_REUSE                    1

// Support Multicast
#include "stdlib.h"
#define LWIP_IGMP                   LWIP_IPV4
#define LWIP_RAND()                 lwip_get_random()

#define LWIP_COMPAT_SOCKETS         0
#define LWIP_POSIX_SOCKETS_IO_NAMES 0

#define LWIP_BROADCAST_PING         1

// Fragmentation on, as per IPv4 default
#define LWIP_IPV6_FRAG              LWIP_IPV6

// Queuing, default is  "disabled", as per IPv4 default (so actually queues 1)
#define LWIP_ND6_QUEUEING           MBED_CONF_ND6_QUEUEING

// Debug Options
#define NETIF_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                  LWIP_DBG_OFF
#define API_LIB_DEBUG               LWIP_DBG_OFF
#define API_MSG_DEBUG               LWIP_DBG_OFF
#define SOCKETS_DEBUG               LWIP_DBG_OFF
#define ICMP_DEBUG                  LWIP_DBG_OFF
#define IGMP_DEBUG                  LWIP_DBG_OFF
#define INET_DEBUG                  LWIP_DBG_OFF
#define IP_DEBUG                    LWIP_DBG_OFF
#define IP_REASS_DEBUG              LWIP_DBG_OFF
#define RAW_DEBUG                   LWIP_DBG_OFF
#define MEM_DEBUG                   LWIP_DBG_OFF
#define MEMP_DEBUG                  LWIP_DBG_OFF
#define SYS_DEBUG                   LWIP_DBG_OFF
#define TIMERS_DEBUG                LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF
#define TCP_INPUT_DEBUG             LWIP_DBG_OFF
#define TCP_FR_DEBUG                LWIP_DBG_OFF
#define TCP_RTO_DEBUG               LWIP_DBG_OFF
#define TCP_CWND_DEBUG              LWIP_DBG_OFF
#define TCP_WND_DEBUG               LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG            LWIP_DBG_OFF
#define TCP_RST_DEBUG               LWIP_DBG_OFF
#define TCP_QLEN_DEBUG              LWIP_DBG_OFF
#define UDP_DEBUG                   LWIP_DBG_OFF
#define TCPIP_DEBUG                 LWIP_DBG_OFF
#define SLIP_DEBUG                  LWIP_DBG_OFF
#define DHCP_DEBUG                  LWIP_DBG_OFF
#define AUTOIP_DEBUG                LWIP_DBG_OFF
#define DNS_DEBUG                   LWIP_DBG_OFF
#define IP6_DEBUG                   LWIP_DBG_OFF

#define ETHARP_DEBUG                LWIP_DBG_OFF
#define UDP_LPC_EMAC                LWIP_DBG_OFF

#ifdef LWIP_DEBUG
#define MEMP_OVERFLOW_CHECK         1
#define MEMP_SANITY_CHECK           1
#define LWIP_DBG_TYPES_ON           LWIP_DBG_ON
#define LWIP_DBG_MIN_LEVEL          LWIP_DBG_LEVEL_ALL
#else
#define LWIP_NOASSERT               1
#define LWIP_STATS                  0
#endif

#define TRACE_TO_ASCII_HEX_DUMP     0

#define LWIP_PLATFORM_BYTESWAP      1

#define LWIP_RANDOMIZE_INITIAL_LOCAL_PORTS 1

// Interface type configuration

#if MBED_CONF_LWIP_ETHERNET_ENABLED
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_DHCP                   LWIP_IPV4
#else
#define LWIP_ARP                    0
#define LWIP_ETHERNET               0
#endif // MBED_CONF_LWIP_ETHERNET_ENABLED

#if MBED_CONF_LWIP_L3IP_ENABLED
#define LWIP_L3IP                   1
#else
#define LWIP_L3IP                   0
#endif

//Maximum size of network interface name
#define INTERFACE_NAME_MAX_SIZE NSAPI_INTERFACE_NAME_MAX_SIZE
// Note generic macro name used rather than MBED_CONF_LWIP_PPP_ENABLED
// to allow users like PPPCellularInterface to detect that nsapi_ppp.h is available.

// Enable PPP for now either from lwIP PPP configuration (obsolete) or from PPP service configuration
#if MBED_CONF_PPP_ENABLED || MBED_CONF_LWIP_PPP_ENABLED

#define PPP_SUPPORT                      1

#if MBED_CONF_PPP_IPV4_ENABLED || MBED_CONF_LWIP_IPV4_ENABLED
#define LWIP 0x11991199
#if (MBED_CONF_NSAPI_DEFAULT_STACK == LWIP) && !MBED_CONF_LWIP_IPV4_ENABLED
#error LWIP: IPv4 PPP enabled but not IPv4
#endif
#undef LWIP
#define PPP_IPV4_SUPPORT                 1
#endif

#if MBED_CONF_PPP_IPV6_ENABLED || MBED_CONF_LWIP_IPV6_ENABLED
#define LWIP 0x11991199
#if (MBED_CONF_NSAPI_DEFAULT_STACK == LWIP) && !MBED_CONF_LWIP_IPV6_ENABLED
#error LWIP: IPv6 PPP enabled but not IPv6
#endif
#undef LWIP
#define PPP_IPV6_SUPPORT                 1
// Later to be dynamic for use for multiple interfaces
#define LWIP_IPV6_DUP_DETECT_ATTEMPTS    0
#endif

#endif

// FIXME: Add compile time configuration and define conditionaly
#define LWIP_NETBUF_RECVINFO            MBED_CONF_LWIP_NETBUF_RECVINFO_ENABLED

// Make sure we default these to off, so
// LWIP doesn't default to on
#ifndef LWIP_ARP
#define LWIP_ARP                    0
#endif

// Checksum-on-copy disabled due to https://savannah.nongnu.org/bugs/?50914
#define LWIP_CHECKSUM_ON_COPY       0

#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1

#define DNS_TABLE_SIZE                  2
#define DNS_MAX_NAME_LENGTH             128

#include "lwip_random.h"
#include "lwip_tcp_isn.h"
#define LWIP_HOOK_TCP_ISN lwip_hook_tcp_isn
#ifdef MBEDTLS_MD5_C
#define LWIP_USE_EXTERNAL_MBEDTLS       1
#else
#define LWIP_USE_EXTERNAL_MBEDTLS       0
#endif

#define LWIP_ND6_RDNSS_MAX_DNS_SERVERS  MBED_CONF_LWIP_ND6_RDNSS_MAX_DNS_SERVERS

#endif /* LWIPOPTS_H_ */
