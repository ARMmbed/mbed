/* nsapi_dns.cpp
 * Original work Copyright (c) 2013 Henry Leinen (henry[dot]leinen [at] online [dot] de)
 * Modified work Copyright (c) 2015 ARM Limited
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

/* Declare __STDC_LIMIT_MACROS so stdint.h defines INT32_MAX when using C++ */
#define __STDC_LIMIT_MACROS

#include "netsocket/nsapi_dns.h"
#include "netsocket/UDPSocket.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "events/mbed_shared_queues.h"
#include "events/EventQueue.h"
#include "netsocket/OnboardNetworkStack.h"
#include "rtos/Kernel.h"
#include "PlatformMutex.h"
#include "SingletonPtr.h"

using namespace std::chrono;
using rtos::Kernel::Clock;

#define CLASS_IN 1

#define RR_A 1
#define RR_AAAA 28

// DNS options
#define DNS_BUFFER_SIZE 512
#define DNS_SERVERS_SIZE 5
#define DNS_RESPONSE_MIN_SIZE 12
#define DNS_STACK_SERVERS_NUM 5
#define DNS_QUERY_QUEUE_SIZE 5
#define DNS_HOST_NAME_MAX_LEN 255
#define DNS_TIMER_TIMEOUT 100
#if !defined(MIN)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

struct DNS_CACHE {
    nsapi_addr_t *address;
    char *host;
    Clock::time_point expires;      /*!< time to live in milliseconds */
    Clock::time_point accessed;     /*!< last accessed */
    uint8_t count;                  /*!< number of IP addresses */
};

struct SOCKET_CB_DATA {
    call_in_callback_cb_t call_in_cb;
    NetworkStack *stack;
};

enum dns_state {
    DNS_CREATED,           /*!< created, not yet making query to network */
    DNS_INITIATED,         /*!< making query to network */
    DNS_CANCELLED          /*!< cancelled, callback will not be called */
};

struct DNS_QUERY {
    intptr_t unique_id;
    nsapi_error_t status;
    NetworkStack *stack;
    char *host;
    const char *interface_name;
    NetworkStack::hostbyname_cb_t callback;
    call_in_callback_cb_t call_in_cb;
    nsapi_size_t addr_count;
    nsapi_version_t version;
    UDPSocket *socket;
    SOCKET_CB_DATA *socket_cb_data;
    nsapi_addr_t *addrs;
    duration<uint32_t> ttl;
    uint32_t total_timeout;
    uint32_t socket_timeout;
    uint16_t dns_message_id;
    uint8_t dns_server;
    uint8_t retries;
    uint8_t total_attempts;
    uint8_t send_success;
    uint8_t count;
    dns_state state;
};

static void nsapi_dns_cache_add(const char *host, nsapi_addr_t *address, duration<uint32_t> ttl, uint8_t count);
static nsapi_size_or_error_t nsapi_dns_cache_find(const char *host, nsapi_version_t version, nsapi_addr_t *address);
static void nsapi_dns_cache_reset();

static nsapi_error_t nsapi_dns_get_server_addr(NetworkStack *stack, uint8_t *index, uint8_t *total_attempts, uint8_t *send_success, SocketAddress *dns_addr, const char *interface_name);

static void nsapi_dns_query_async_create(void *ptr);
static nsapi_error_t nsapi_dns_query_async_delete(intptr_t unique_id);
static void nsapi_dns_query_async_send(void *ptr);
static void nsapi_dns_query_async_timeout(void);
static void nsapi_dns_query_async_resp(DNS_QUERY *query, nsapi_error_t status, SocketAddress *address);
static void nsapi_dns_query_async_socket_callback(void *ptr);
static void nsapi_dns_query_async_socket_callback_handle(NetworkStack *stack);
static void nsapi_dns_query_async_response(void *ptr);
static void nsapi_dns_query_async_initiate_next(void);

// *INDENT-OFF*
static nsapi_addr_t dns_servers[DNS_SERVERS_SIZE] = {
    {NSAPI_IPv4, {8, 8, 8, 8}},                             // Google
    {NSAPI_IPv6, {0x20,0x01, 0x48,0x60, 0x48,0x60, 0,0,     // Google
                  0,0, 0,0, 0,0, 0x88,0x88}},
    {NSAPI_IPv4, {209, 244, 0, 3}},                         // Level 3
    {NSAPI_IPv4, {84, 200, 69, 80}},                        // DNS.WATCH
    {NSAPI_IPv6, {0x20,0x01, 0x16,0x08, 0,0x10, 0,0x25,     // DNS.WATCH
                  0,0, 0,0, 0x1c,0x04, 0xb1,0x2f}},
};
// *INDENT-ON*

#if (MBED_CONF_NSAPI_DNS_CACHE_SIZE > 0)
static DNS_CACHE *dns_cache[MBED_CONF_NSAPI_DNS_CACHE_SIZE];
// Protects cache shared between blocking and asynchronous calls
static SingletonPtr<PlatformMutex> dns_cache_mutex;
#endif

static uint16_t dns_message_id = 1;
static intptr_t dns_unique_id = 1;
static DNS_QUERY *dns_query_queue[DNS_QUERY_QUEUE_SIZE];
// Protects from several threads running asynchronous DNS
static SingletonPtr<PlatformMutex> dns_mutex;
static SingletonPtr<call_in_callback_cb_t> dns_call_in;
static bool dns_timer_running = false;

// DNS server configuration
extern "C" nsapi_error_t nsapi_dns_add_server(nsapi_addr_t addr, const char *interface_name)
{
    // check if addr was already added
    for (int i = 0; i < DNS_SERVERS_SIZE; i++) {
        if (memcmp(&addr, &dns_servers[i], sizeof(nsapi_addr_t)) == 0) {
            return NSAPI_ERROR_OK;
        }
    }

    memmove(&dns_servers[1], &dns_servers[0],
            (DNS_SERVERS_SIZE - 1)*sizeof(nsapi_addr_t));

    dns_servers[0] = addr;
    return NSAPI_ERROR_OK;
}


// DNS packet parsing
static void dns_append_byte(uint8_t **p, uint8_t byte)
{
    *(*p)++ = byte;
}

static void dns_append_word(uint8_t **p, uint16_t word)
{

    dns_append_byte(p, 0xff & (word >> 8));
    dns_append_byte(p, 0xff & (word >> 0));
}

static void dns_append_name(uint8_t **p, const char *name, uint8_t len)
{
    dns_append_byte(p, len);
    memcpy(*p, name, len);
    *p += len;
}

static uint8_t dns_scan_byte(const uint8_t **p)
{
    return *(*p)++;
}

static uint16_t dns_scan_word(const uint8_t **p)
{
    uint16_t a = dns_scan_byte(p);
    uint16_t b = dns_scan_byte(p);
    return (a << 8) | b;
}

static uint32_t dns_scan_word32(const uint8_t **p)
{
    uint32_t value = dns_scan_byte(p) << 24;
    value |= dns_scan_byte(p) << 16;
    value |= dns_scan_byte(p) << 8;
    value |= dns_scan_byte(p);

    return value;
}

static int dns_append_question(uint8_t *ptr, uint16_t id, const char *host, nsapi_version_t version)
{
    uint8_t *s_ptr = ptr;
    uint8_t **p = &ptr;

    // fill the header
    dns_append_word(p, id);     // id      = 1
    dns_append_word(p, 0x0100); // flags   = recursion required
    dns_append_word(p, 1);      // qdcount = 1
    dns_append_word(p, 0);      // ancount = 0
    dns_append_word(p, 0);      // nscount = 0
    dns_append_word(p, 0);      // arcount = 0

    // fill out the question names
    while (host[0]) {
        size_t label_len = strcspn(host, ".");
        dns_append_name(p, host, label_len);
        host += label_len + (host[label_len] == '.');
    }

    dns_append_byte(p, 0);

    // fill out question footer
    if (version != NSAPI_IPv6) {
        dns_append_word(p, RR_A);       // qtype  = ipv4
    } else {
        dns_append_word(p, RR_AAAA);    // qtype  = ipv6
    }
    dns_append_word(p, CLASS_IN);

    return *p - s_ptr;
}

static int dns_scan_response(const uint8_t *ptr, uint16_t exp_id, duration<uint32_t> *ttl, nsapi_addr_t *addr, unsigned addr_count)
{
    const uint8_t **p = &ptr;

    // scan header
    uint16_t id    = dns_scan_word(p);
    uint16_t flags = dns_scan_word(p);
    bool    qr     = 0x1 & (flags >> 15);
    uint8_t opcode = 0xf & (flags >> 11);
    uint8_t rcode  = 0xf & (flags >>  0);

    uint16_t qdcount = dns_scan_word(p); // qdcount
    uint16_t ancount = dns_scan_word(p); // ancount
    dns_scan_word(p);                    // nscount
    dns_scan_word(p);                    // arcount

    // verify header is response to query
    if (!(id == exp_id && qr && opcode == 0)) {
        return -1;
    }

    if (rcode != 0) {
        return 0;
    }

    // skip questions
    for (int i = 0; i < qdcount; i++) {
        while (true) {
            uint8_t len = dns_scan_byte(p);
            if (len == 0) {
                break;
            }

            *p += len;
        }

        dns_scan_word(p); // qtype
        dns_scan_word(p); // qclass
    }

    // scan each response
    unsigned count = 0;

    for (int i = 0; i < ancount && count < addr_count; i++) {
        while (true) {
            uint8_t len = dns_scan_byte(p);
            if (len == 0) {
                break;
            } else if (len & 0xc0) { // this is link
                dns_scan_byte(p);
                break;
            }

            *p += len;
        }

        uint16_t rtype    = dns_scan_word(p);    // rtype
        uint16_t rclass   = dns_scan_word(p);    // rclass
        uint32_t ttl_val  = dns_scan_word32(p);  // ttl
        uint16_t rdlength = dns_scan_word(p);    // rdlength

        if (i == 0) {
            // Is interested only on first address that is stored to cache
            if (ttl_val > INT32_MAX) {
                ttl_val = INT32_MAX;
            }
            *ttl = duration<uint32_t>(ttl_val);
        }

        if (rtype == RR_A && rclass == CLASS_IN && rdlength == NSAPI_IPv4_BYTES) {
            // accept A record
            addr->version = NSAPI_IPv4;
            for (int i = 0; i < NSAPI_IPv4_BYTES; i++) {
                addr->bytes[i] = dns_scan_byte(p);
            }

            addr += 1;
            count += 1;
        } else if (rtype == RR_AAAA && rclass == CLASS_IN && rdlength == NSAPI_IPv6_BYTES) {
            // accept AAAA record
            addr->version = NSAPI_IPv6;
            for (int i = 0; i < NSAPI_IPv6_BYTES; i++) {
                addr->bytes[i] = dns_scan_byte(p);
            }

            addr += 1;
            count += 1;
        } else {
            // skip unrecognized records
            *p += rdlength;
        }
    }

    return count;
}

static void nsapi_dns_cache_add(const char *host, nsapi_addr_t *address, duration<uint32_t> ttl, uint8_t count)
{
#if (MBED_CONF_NSAPI_DNS_CACHE_SIZE > 0)
    // RFC 1034: if TTL is zero, entry is not added to cache
    if (ttl == ttl.zero()) {
        return;
    }

    // Checks if already cached
    if (nsapi_dns_cache_find(host, address->version, NULL) == NSAPI_ERROR_OK) {
        return;
    }

    dns_cache_mutex->lock();

    int index = -1;
    Clock::time_point accessed = Clock::time_point::max();

    // Finds free or last accessed entry
    for (int i = 0; i < MBED_CONF_NSAPI_DNS_CACHE_SIZE; i++) {
        if (!dns_cache[i]) {
            index = i;
            break;
        } else if (dns_cache[i]->accessed <= accessed) {
            accessed = dns_cache[i]->accessed;
            index = i;
        }
    }

    if (index < 0) {
        dns_cache_mutex->unlock();
        return;
    }

    // Allocates in case entry is free, otherwise reuses
    if (!dns_cache[index]) {
        dns_cache[index] = new (std::nothrow) DNS_CACHE;
    } else {
        delete dns_cache[index]->host;
        delete dns_cache[index]->address;
    }

    if (dns_cache[index]) {
        dns_cache[index]->address = new (std::nothrow) nsapi_addr_t[count];
        for (int i = 0; i < count; i++) {
            dns_cache[index]->address[i] = address[i];
        }
        dns_cache[index]->count = count;
        dns_cache[index]->host = new (std::nothrow) char[strlen(host) + 1];
        strcpy(dns_cache[index]->host, host);
        auto now = Clock::now();
        dns_cache[index]->expires = now + ttl;
        dns_cache[index]->accessed = now;
    }

    dns_cache_mutex->unlock();
#endif
}

static nsapi_size_or_error_t nsapi_dns_cache_find(const char *host, nsapi_version_t version, nsapi_addr_t *address)
{
    nsapi_error_t ret_val = NSAPI_ERROR_NO_ADDRESS;

#if (MBED_CONF_NSAPI_DNS_CACHE_SIZE > 0)
    dns_cache_mutex->lock();

    for (int i = 0; i < MBED_CONF_NSAPI_DNS_CACHE_SIZE; i++) {
        if (dns_cache[i]) {
            auto now = Clock::now();
            // Checks all entries for expired entries
            if (now > dns_cache[i]->expires) {
                delete dns_cache[i]->host;
                delete dns_cache[i]->address;
                delete dns_cache[i];
                dns_cache[i] = NULL;
            } else if ((version == NSAPI_UNSPEC || version == dns_cache[i]->address[0].version) && //only first IP address version check, others have the same version
                       strcmp(dns_cache[i]->host, host) == 0) {
                if (address) {
                    ret_val = 0;
                    for (int count = 0; count < dns_cache[i]->count; count++) {
                        address[count] = dns_cache[i]->address[count];
                        ret_val++;
                    }
                }
                dns_cache[i]->accessed = now;

            }
        }
    }

    dns_cache_mutex->unlock();
#endif

    return ret_val;
}

static void nsapi_dns_cache_reset()
{
#if (MBED_CONF_NSAPI_DNS_CACHE_SIZE > 0)
    dns_cache_mutex->lock();
    for (int i = 0; i < MBED_CONF_NSAPI_DNS_CACHE_SIZE; i++) {
        if (dns_cache[i]) {
            delete[] dns_cache[i]->host;
            dns_cache[i]->host = NULL;
            delete[] dns_cache[i]->address;
            delete dns_cache[i];
            dns_cache[i] = NULL;
        }
    }
    dns_cache_mutex->unlock();
#endif
}

static nsapi_error_t nsapi_dns_get_server_addr(NetworkStack *stack, uint8_t *index, uint8_t *total_attempts, uint8_t *send_success, SocketAddress *dns_addr, const char *interface_name)
{
    bool dns_addr_set = false;

    if (*total_attempts == 0) {
        return NSAPI_ERROR_NO_ADDRESS;
    }

    if (*index >= DNS_SERVERS_SIZE + DNS_STACK_SERVERS_NUM) {
        // If there are total attempts left and send to has been successful at least once on this round
        if (*total_attempts && *send_success) {
            *index = 0;
            *send_success = 0;
        } else {
            return NSAPI_ERROR_NO_ADDRESS;
        }
    }

    if (*index < DNS_STACK_SERVERS_NUM) {
        nsapi_error_t ret = stack->get_dns_server(*index, dns_addr, interface_name);
        if (ret < 0) {
            *index = DNS_STACK_SERVERS_NUM;
        } else {
            dns_addr_set = true;
        }
    }

    if (!dns_addr_set) {
        dns_addr->set_addr(dns_servers[*index - DNS_STACK_SERVERS_NUM]);
    }

    dns_addr->set_port(53);

    return NSAPI_ERROR_OK;
}

// core query function
static nsapi_size_or_error_t nsapi_dns_query_multiple(NetworkStack *stack, const char *host,
                                                      nsapi_addr_t *addr, unsigned addr_count,  const char *interface_name, nsapi_version_t version)
{
    // check for valid host name
    int host_len = host ? strlen(host) : 0;
    if (host_len > DNS_HOST_NAME_MAX_LEN || host_len == 0) {
        return NSAPI_ERROR_PARAMETER;
    }

    // check cache
    nsapi_addr *tmp = new (std::nothrow) nsapi_addr_t [MBED_CONF_NSAPI_DNS_ADDRESSES_LIMIT];
    int cached = nsapi_dns_cache_find(host, version, tmp);
    if (cached > 0) {
        unsigned int us_cached = cached;
        for (unsigned int i = 0;  i < MIN(us_cached, addr_count); i++) {
            addr[i] = tmp[i];
        }
        delete [] tmp;
        return MIN(us_cached, addr_count);
    }
    delete [] tmp;
    // create a udp socket
    UDPSocket socket;
    int err = socket.open(stack);
    if (err) {
        return err;
    }

    socket.set_timeout(MBED_CONF_NSAPI_DNS_RESPONSE_WAIT_TIME);

    if (interface_name != NULL) {
        socket.setsockopt(NSAPI_SOCKET, NSAPI_BIND_TO_DEVICE, interface_name, NSAPI_INTERFACE_NAME_MAX_SIZE);
    }
    // create network packet
    uint8_t *const packet = (uint8_t *)malloc(DNS_BUFFER_SIZE);
    if (!packet) {
        return NSAPI_ERROR_NO_MEMORY;
    }

    nsapi_size_or_error_t result = NSAPI_ERROR_DNS_FAILURE;

    uint8_t retries = MBED_CONF_NSAPI_DNS_RETRIES;
    uint8_t index = 0;
    uint8_t total_attempts = MBED_CONF_NSAPI_DNS_TOTAL_ATTEMPTS;
    uint8_t send_success = 0;

    // check against each dns server
    while (true) {
        SocketAddress dns_addr;
        err = nsapi_dns_get_server_addr(stack, &index, &total_attempts, &send_success, &dns_addr, interface_name);
        if (err != NSAPI_ERROR_OK) {
            break;
        }

        if (version != NSAPI_UNSPEC && (dns_addr.get_ip_version() != version)) {
            retries = MBED_CONF_NSAPI_DNS_RETRIES;
            index++;
            continue;
        }
        // send the question
        int len = dns_append_question(packet, 1, host, dns_addr.get_ip_version());

        err = socket.sendto(dns_addr, packet, len);
        // send may fail for various reasons, including wrong address type - move on
        if (err < 0) {
            // goes to next dns server
            retries = MBED_CONF_NSAPI_DNS_RETRIES;
            index++;
            continue;
        }

        send_success++;

        if (total_attempts) {
            total_attempts--;
        }

        // recv the response
        err = socket.recvfrom(NULL, packet, DNS_BUFFER_SIZE);
        if (err == NSAPI_ERROR_WOULD_BLOCK) {
            if (retries) {
                // retries
                retries--;
            } else {
                // goes to next dns server
                retries = MBED_CONF_NSAPI_DNS_RETRIES;
                index++;
            }
            continue;
        } else if (err < 0) {
            result = err;
            break;
        }

        const uint8_t *response = packet;
        duration<uint32_t> ttl;
        int resp = dns_scan_response(response, 1, &ttl, addr, addr_count);
        if (resp > 0) {
            nsapi_dns_cache_add(host, addr, ttl, resp);
            result = resp;
        } else if (resp < 0) {
            continue;
        }

        /* The DNS response is final, no need to check other servers */
        break;
    }

    // clean up packet
    free(packet);

    // clean up udp
    err = socket.close();
    if (err) {
        return err;
    }

    // return result
    return result;
}

// convenience functions for other forms of queries
extern "C" nsapi_size_or_error_t nsapi_dns_query_multiple(nsapi_stack_t *stack, const char *host,
                                                          nsapi_addr_t *addr, nsapi_size_t addr_count, const char *interface_name, nsapi_version_t version)
{
    NetworkStack *nstack = nsapi_create_stack(stack);
    return nsapi_dns_query_multiple(nstack, host, addr, addr_count, interface_name, version);
}

nsapi_size_or_error_t nsapi_dns_query_multiple(NetworkStack *stack, const char *host,
                                               SocketAddress *addresses, nsapi_size_t addr_count, const char *interface_name, nsapi_version_t version)
{
    nsapi_addr_t *addrs = new (std::nothrow) nsapi_addr_t[addr_count];

    if (!addrs) {
        return NSAPI_ERROR_NO_MEMORY;
    }

    nsapi_size_or_error_t result = nsapi_dns_query_multiple(stack, host, addrs, addr_count, interface_name, version);

    if (result > 0) {
        for (int i = 0; i < result; i++) {
            addresses[i].set_addr(addrs[i]);
        }
    }

    delete[] addrs;
    return result;
}

extern "C" nsapi_error_t nsapi_dns_query(nsapi_stack_t *stack, const char *host,
                                         nsapi_addr_t *addr, nsapi_version_t version)
{
    NetworkStack *nstack = nsapi_create_stack(stack);
    nsapi_size_or_error_t result = nsapi_dns_query_multiple(nstack, host, addr, 1, NULL, version);
    return (nsapi_error_t)((result > 0) ? 0 : result);
}

nsapi_error_t nsapi_dns_query(NetworkStack *stack, const char *host,
                              SocketAddress *address, nsapi_version_t version)
{
    nsapi_addr_t addr;
    nsapi_size_or_error_t result = nsapi_dns_query_multiple(stack, host, &addr, 1, NULL, version);
    address->set_addr(addr);
    return (nsapi_error_t)((result > 0) ? 0 : result);
}

nsapi_error_t nsapi_dns_query(NetworkStack *stack, const char *host,
                              SocketAddress *address, const char *interface_name, nsapi_version_t version)
{
    nsapi_addr_t addr;
    nsapi_size_or_error_t result = nsapi_dns_query_multiple(stack, host, &addr, 1, interface_name, version);
    address->set_addr(addr);
    return (nsapi_error_t)((result > 0) ? 0 : result);
}

nsapi_value_or_error_t nsapi_dns_query_async(NetworkStack *stack, const char *host,
                                             NetworkStack::hostbyname_cb_t callback, call_in_callback_cb_t call_in_cb,
                                             nsapi_version_t version)
{
    return nsapi_dns_query_multiple_async(stack, host, callback, 0, call_in_cb, NULL, version);
}

nsapi_value_or_error_t nsapi_dns_query_async(NetworkStack *stack, const char *host,
                                             NetworkStack::hostbyname_cb_t callback, call_in_callback_cb_t call_in_cb,
                                             const char *interface_name, nsapi_version_t version)
{
    return nsapi_dns_query_multiple_async(stack, host, callback, 0, call_in_cb, interface_name, version);
}

void nsapi_dns_call_in_set(call_in_callback_cb_t callback)
{
    *dns_call_in.get() = callback;
}

void nsapi_dns_reset()
{
    nsapi_dns_cache_reset();
    dns_message_id = 1;
    dns_unique_id = 1;
}

nsapi_error_t nsapi_dns_call_in(call_in_callback_cb_t cb, int delay, mbed::Callback<void()> func)
{
    if (*dns_call_in.get()) {
        dns_call_in->call(delay, func);
    } else {
        return cb(delay, func);
    }
    return NSAPI_ERROR_OK;
}

nsapi_value_or_error_t nsapi_dns_query_multiple_async(NetworkStack *stack, const char *host,
                                                      NetworkStack::hostbyname_cb_t callback, nsapi_size_t addr_count,
                                                      call_in_callback_cb_t call_in_cb, const char *interface_name, nsapi_version_t version)
{
    dns_mutex->lock();

    if (!stack) {
        return NSAPI_ERROR_PARAMETER;
    }

    // check for valid host name
    int host_len = host ? strlen(host) : 0;
    if (host_len > DNS_HOST_NAME_MAX_LEN || host_len == 0) {
        dns_mutex->unlock();
        return NSAPI_ERROR_PARAMETER;
    }

    nsapi_addr *address = new (std::nothrow) nsapi_addr_t [MBED_CONF_NSAPI_DNS_ADDRESSES_LIMIT];
    int cached = nsapi_dns_cache_find(host, version, address);
    if (!addr_count) {
        if (cached > 0) {
            SocketAddress addr(*address);
            dns_mutex->unlock();
            callback(1, &addr);
            delete[] address;
            return NSAPI_ERROR_OK;
        }

    } else {

        if (cached > 0) {
            SocketAddress *addr  = new (std::nothrow) SocketAddress [cached];
            for (int i = 0;  i < cached; i++) {
                addr[i].set_addr(address[i]);
            }
            dns_mutex->unlock();
            callback(cached, addr);
            delete[] address;
            delete[] addr;
            return cached;
        }
    }
    delete[] address;
    int index = -1;

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (!dns_query_queue[i]) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        dns_mutex->unlock();
        return NSAPI_ERROR_NO_MEMORY;
    }

    DNS_QUERY *query = new (std::nothrow) DNS_QUERY;

    if (!query) {
        dns_mutex->unlock();
        return NSAPI_ERROR_NO_MEMORY;
    }

    query->host = new (std::nothrow) char[host_len + 1];
    if (!query->host) {
        delete query;
        dns_mutex->unlock();
        return NSAPI_ERROR_NO_MEMORY;
    }
    strcpy(query->host, host);
    query->status = NSAPI_ERROR_TIMEOUT;
    query->callback = callback;
    query->call_in_cb = call_in_cb;
    query->stack = stack;
    query->addr_count = addr_count;
    query->version = version;
    query->socket = NULL;
    query->socket_cb_data = NULL;
    query->addrs = NULL;
    query->dns_server = 0;
    query->retries = MBED_CONF_NSAPI_DNS_RETRIES + 1;
    query->total_attempts =  MBED_CONF_NSAPI_DNS_TOTAL_ATTEMPTS;
    query->send_success = 0;
    query->dns_message_id = 0;
    query->socket_timeout = 0;
    query->total_timeout = MBED_CONF_NSAPI_DNS_TOTAL_ATTEMPTS * MBED_CONF_NSAPI_DNS_RESPONSE_WAIT_TIME + 500;
    query->count = 0;
    query->state = DNS_CREATED;
    query->interface_name = interface_name;
    query->unique_id = dns_unique_id++;
    if (query->unique_id > 0x7FFF) {
        query->unique_id = 1;
    }

    int ongoing_queries = 0;

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i]) {
            if (!query->socket && dns_query_queue[i]->socket && dns_query_queue[i]->stack == query->stack) {
                query->socket = dns_query_queue[i]->socket;
                query->socket_cb_data = dns_query_queue[i]->socket_cb_data;
            }
            ongoing_queries++;
        }
    }

    dns_query_queue[index] = query;

    // Add some overhead based on number of ongoing queries
    query->total_timeout += ongoing_queries * 500;

    if (!dns_timer_running) {
        if (nsapi_dns_call_in(query->call_in_cb, DNS_TIMER_TIMEOUT, mbed::callback(nsapi_dns_query_async_timeout)) != NSAPI_ERROR_OK) {
            delete[] query->host;
            delete query;
            dns_query_queue[index] = NULL;
            dns_mutex->unlock();
            return NSAPI_ERROR_NO_MEMORY;
        }
        dns_timer_running = true;
    }

    // Initiates query
    nsapi_dns_query_async_initiate_next();

    dns_mutex->unlock();

    return query->unique_id;
}

static void nsapi_dns_query_async_initiate_next(void)
{
    intptr_t id = INTPTR_MAX;
    DNS_QUERY *query = NULL;

    // Trigger next query to start, find one that has been on queue longest
    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i]) {
            if (dns_query_queue[i]->state == DNS_CREATED) {
                if (dns_query_queue[i]->unique_id <= id) {
                    query = dns_query_queue[i];
                    id = dns_query_queue[i]->unique_id;
                }
                // If some query is already ongoing do not trigger
            } else if (dns_query_queue[i]->state == DNS_INITIATED) {
                query = NULL;
                break;
            }
        }
    }

    if (query) {
        query->state = DNS_INITIATED;
        nsapi_dns_call_in(query->call_in_cb, 0, mbed::callback(nsapi_dns_query_async_create, reinterpret_cast<void *>(query->unique_id)));
    }
}

static void nsapi_dns_query_async_timeout(void)
{
    dns_mutex->lock();

    DNS_QUERY *query = NULL;

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i]) {
            if (dns_query_queue[i]->state == DNS_CANCELLED) {
                // Delete cancelled
                nsapi_dns_query_async_delete(dns_query_queue[i]->unique_id);
                nsapi_dns_query_async_initiate_next();
                continue;
            }

            if (dns_query_queue[i]->total_timeout > DNS_TIMER_TIMEOUT) {
                dns_query_queue[i]->total_timeout -= DNS_TIMER_TIMEOUT;
            } else {
                // If does not already have response, fails
                if (dns_query_queue[i]->status == NSAPI_ERROR_TIMEOUT) {
                    dns_query_queue[i]->socket_timeout = 0;
                    nsapi_dns_call_in(dns_query_queue[i]->call_in_cb, 0, mbed::callback(nsapi_dns_query_async_response, reinterpret_cast<void *>(dns_query_queue[i]->unique_id)));
                }
            }

            if (dns_query_queue[i]->socket_timeout > 0) {
                if (dns_query_queue[i]->socket_timeout > DNS_TIMER_TIMEOUT) {
                    dns_query_queue[i]->socket_timeout -= DNS_TIMER_TIMEOUT;
                } else {
                    // Retries
                    dns_query_queue[i]->socket_timeout = 0;
                    nsapi_dns_call_in(dns_query_queue[i]->call_in_cb, 0,
                                      mbed::callback(nsapi_dns_query_async_send, reinterpret_cast<void *>(dns_query_queue[i]->unique_id)));
                }
            }

            if (!query) {
                query = dns_query_queue[i];
            }
        }
    }

    // Starts timer again
    if (query) {
        nsapi_dns_call_in(query->call_in_cb, DNS_TIMER_TIMEOUT, mbed::callback(nsapi_dns_query_async_timeout));
    } else {
        dns_timer_running = false;
    }

    dns_mutex->unlock();
}

nsapi_error_t nsapi_dns_query_async_cancel(nsapi_size_or_error_t id)
{
    dns_mutex->lock();

    DNS_QUERY *query = NULL;

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i] && dns_query_queue[i]->unique_id == id) {
            query = dns_query_queue[i];
            break;
        }
    }

    if (!query || query->state == DNS_CANCELLED) {
        dns_mutex->unlock();
        return NSAPI_ERROR_PARAMETER;
    }

    // Mark the query as cancelled, deleted by timer handler
    query->state = DNS_CANCELLED;
    // Do not call callback
    query->callback = 0;

    dns_mutex->unlock();

    return NSAPI_ERROR_OK;
}

static void nsapi_dns_query_async_create(void *ptr)
{
    dns_mutex->lock();

    intptr_t unique_id = reinterpret_cast<intptr_t>(ptr);

    DNS_QUERY *query = NULL;

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i] && dns_query_queue[i]->unique_id == unique_id) {
            query = dns_query_queue[i];
            break;
        }
    }

    if (!query || query->state == DNS_CANCELLED) {
        // Cancel has been called
        dns_mutex->unlock();
        return;
    }

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i] && dns_query_queue[i] != query) {
            if (!query->socket && dns_query_queue[i]->socket && dns_query_queue[i]->stack == query->stack) {
                query->socket = dns_query_queue[i]->socket;
                query->socket_cb_data = dns_query_queue[i]->socket_cb_data;
            }
        }
    }

    UDPSocket *socket;

    if (query->socket) {
        socket = query->socket;
    } else {
        socket = new (std::nothrow) UDPSocket;
        if (!socket) {
            nsapi_dns_query_async_resp(query, NSAPI_ERROR_NO_MEMORY, NULL);
            return;
        }

        int err = socket->open(query->stack);

        if (err) {
            delete socket;
            nsapi_dns_query_async_resp(query, err, NULL);
            return;
        }

        socket->set_timeout(0);
        if (query->interface_name != NULL) {
            socket->setsockopt(NSAPI_SOCKET, NSAPI_BIND_TO_DEVICE, query->interface_name, NSAPI_INTERFACE_NAME_MAX_SIZE);
        }

        if (!query->socket_cb_data) {
            query->socket_cb_data = new (std::nothrow) SOCKET_CB_DATA;
            if (!query->socket_cb_data) {
                delete socket;
                nsapi_dns_query_async_resp(query, NSAPI_ERROR_NO_MEMORY, NULL);
                return;
            }
        }
        query->socket_cb_data->call_in_cb = query->call_in_cb;
        query->socket_cb_data->stack = query->stack;
        socket->sigio(mbed::callback(nsapi_dns_query_async_socket_callback, query->socket_cb_data));

        query->socket = socket;
    }

    dns_mutex->unlock();

    nsapi_dns_query_async_send(reinterpret_cast<void *>(query->unique_id));

}

static nsapi_error_t nsapi_dns_query_async_delete(intptr_t unique_id)
{
    int index = -1;
    DNS_QUERY *query = NULL;

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i] && dns_query_queue[i]->unique_id == unique_id) {
            query = dns_query_queue[i];
            index = i;
            break;
        }
    }

    if (!query) {
        return NSAPI_ERROR_PARAMETER;
    }

    bool close_socket = true;

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i] && dns_query_queue[i] != query && dns_query_queue[i]->socket &&
                dns_query_queue[i]->stack == query->stack) {
            close_socket = false;
        }
    }

    if (close_socket && query->socket) {
        query->socket->close();
        delete query->socket;
        delete query->socket_cb_data;
    }

    if (query->addrs) {
        delete[] query->addrs;
    }

    delete[] query->host;
    delete query;
    dns_query_queue[index] = NULL;

    return NSAPI_ERROR_OK;
}

static void nsapi_dns_query_async_resp(DNS_QUERY *query, nsapi_error_t status, SocketAddress *address)
{
    NetworkStack::hostbyname_cb_t callback = query->callback;
    nsapi_dns_query_async_delete(query->unique_id);
    nsapi_dns_query_async_initiate_next();

    dns_mutex->unlock();

    if (callback) {
        callback(status, address);
    }
}

static void nsapi_dns_query_async_send(void *ptr)
{
    dns_mutex->lock();

    intptr_t unique_id = reinterpret_cast<intptr_t>(ptr);

    DNS_QUERY *query = NULL;

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i] && dns_query_queue[i]->unique_id == unique_id) {
            query = dns_query_queue[i];
            break;
        }
    }

    if (!query || query->state != DNS_INITIATED) {
        // Cancel has been called
        dns_mutex->unlock();
        return;
    }

    if (query->retries) {
        query->retries--;
    } else {
        query->dns_server++;
        query->retries = MBED_CONF_NSAPI_DNS_RETRIES;
    }

    query->dns_message_id = dns_message_id++;
    if (dns_message_id == 0) {
        dns_message_id = 1;
    }

    // create network packet
    uint8_t *packet = (uint8_t *)malloc(DNS_BUFFER_SIZE);
    if (!packet) {
        nsapi_dns_query_async_resp(query, NSAPI_ERROR_NO_MEMORY, NULL);
        return;
    }

    while (true) {
        SocketAddress dns_addr;
        nsapi_size_or_error_t err = nsapi_dns_get_server_addr(query->stack, &(query->dns_server), &(query->total_attempts), &(query->send_success), &dns_addr, query->interface_name);
        if (err != NSAPI_ERROR_OK) {
            nsapi_dns_query_async_resp(query, NSAPI_ERROR_TIMEOUT, NULL);
            free(packet);
            return;
        }

        if (query->version != NSAPI_UNSPEC && dns_addr.get_ip_version() != query->version) {
            query->dns_server++;
            continue;
        }
        // send the question
        int len = dns_append_question(packet, query->dns_message_id, query->host, dns_addr.get_ip_version());

        err = query->socket->sendto(dns_addr, packet, len);

        if (err < 0) {
            if (err == NSAPI_ERROR_WOULD_BLOCK) {
                nsapi_dns_call_in(query->call_in_cb, DNS_TIMER_TIMEOUT, mbed::callback(nsapi_dns_query_async_send, ptr));
                free(packet);
                dns_mutex->unlock();
                return; // Timeout handler will retry the connection if possible
            } else {
                query->dns_server++;
            }
        } else {
            break;
        }
    }

    query->send_success++;

    if (query->total_attempts) {
        query->total_attempts--;
    }

    free(packet);

    query->socket_timeout = MBED_CONF_NSAPI_DNS_RESPONSE_WAIT_TIME;

    dns_mutex->unlock();
}

static void nsapi_dns_query_async_socket_callback(void *ptr)
{
    SOCKET_CB_DATA *cb_data = static_cast<SOCKET_CB_DATA *>(ptr);

    if (cb_data) {
        nsapi_dns_call_in(cb_data->call_in_cb, 0, mbed::callback(nsapi_dns_query_async_socket_callback_handle, cb_data->stack));
    }
}

static void nsapi_dns_query_async_socket_callback_handle(NetworkStack *stack)
{
    UDPSocket *socket = NULL;

    dns_mutex->lock();

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i] && dns_query_queue[i]->stack == stack) {
            socket = dns_query_queue[i]->socket;
            break;
        }
    }

    if (socket) {
        // create network packet
        uint8_t *packet = (uint8_t *)malloc(DNS_BUFFER_SIZE);
        if (!packet) {
            dns_mutex->unlock();
            return;
        }

        while (true) {
            // recv the response
            nsapi_size_or_error_t size = socket->recvfrom(NULL, packet, DNS_BUFFER_SIZE);

            if (size < DNS_RESPONSE_MIN_SIZE) {
                break;
            }

            // gets id from response to associate with correct query
            uint16_t id = (packet[0] << 8) | packet[1];

            DNS_QUERY *query = NULL;

            for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
                if (dns_query_queue[i] && dns_query_queue[i]->dns_message_id == id) {
                    query = dns_query_queue[i];
                    break;
                }
            }

            if (!query || query->state != DNS_INITIATED) {
                continue;
            }

            int requested_count = 1;
            if (query->addr_count > 1) {
                requested_count = query->addr_count;
            }

            query->addrs = new (std::nothrow) nsapi_addr_t[requested_count];

            int resp = dns_scan_response(packet, id, &(query->ttl), query->addrs, requested_count);

            // Ignore invalid responses
            if (resp < 0) {
                delete[] query->addrs;
                query->addrs = 0;
            } else {
                query->count = resp;
                query->status = NSAPI_ERROR_DNS_FAILURE; // Used in case failure, otherwise ok
                query->socket_timeout = 0;
                nsapi_dns_call_in(query->call_in_cb, 0, mbed::callback(nsapi_dns_query_async_response, reinterpret_cast<void *>(query->unique_id)));
            }
        }

        free(packet);
    }

    dns_mutex->unlock();
}

static void nsapi_dns_query_async_response(void *ptr)
{
    dns_mutex->lock();

    intptr_t unique_id = reinterpret_cast<intptr_t>(ptr);

    DNS_QUERY *query = NULL;

    for (int i = 0; i < DNS_QUERY_QUEUE_SIZE; i++) {
        if (dns_query_queue[i] && dns_query_queue[i]->unique_id == unique_id) {
            query = dns_query_queue[i];
            break;
        }
    }

    if (query && query->state == DNS_INITIATED) {
        SocketAddress *addresses = NULL;
        nsapi_error_t status = query->status;

        if (query->count > 0) {
            addresses = new (std::nothrow) SocketAddress[query->count];

            for (int i = 0; i < query->count; i++) {
                addresses[i].set_addr(query->addrs[i]);
            }

            // Adds address to cache
            nsapi_dns_cache_add(query->host, &(query->addrs[0]), query->ttl, query->count);
            status = query->count;
        }

        nsapi_dns_query_async_resp(query, status, addresses);
        delete[] addresses;
    } else {
        dns_mutex->unlock();
    }
}
