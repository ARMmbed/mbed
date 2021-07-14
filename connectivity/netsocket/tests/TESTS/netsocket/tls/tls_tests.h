/*
 * Copyright (c) 2019, ARM Limited, All Rights Reserved
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

#ifndef TLS_TESTS_H
#define TLS_TESTS_H

#include "../test_params.h"
#include "TLSSocket.h"
#include "mbed_trace.h"

#define TRACE_GROUP "GRNT"

#if defined(MBEDTLS_SSL_CLI_C) || defined(DOXYGEN_ONLY)

NetworkInterface *get_interface();
void drop_bad_packets(TLSSocket &sock, int orig_timeout);
void fill_tx_buffer_ascii(char *buff, size_t len);
nsapi_error_t tlssocket_connect_to_echo_srv(TLSSocket &sock);
nsapi_error_t tlssocket_connect_to_discard_srv(TLSSocket &sock);
bool is_tcp_supported();

#define SKIP_IF_TCP_UNSUPPORTED() \
    if (!is_tcp_supported()) { \
        TEST_SKIP_MESSAGE("TCP not supported"); \
    }

#if MBED_CONF_NSAPI_SOCKET_STATS_ENABLED
extern mbed_stats_socket_t tls_stats[MBED_CONF_NSAPI_SOCKET_STATS_MAX_COUNT];
int fetch_stats(void);
#endif

/**
 * Single testcase might take only half of the remaining execution time
 */
int split2half_rmng_tls_test_time(); // [s]

namespace tls_global {
#ifdef MBED_GREENTEA_TEST_TLSSOCKET_TIMEOUT_S
static const int TESTS_TIMEOUT = MBED_GREENTEA_TEST_TLSSOCKET_TIMEOUT_S;
#else
#define MESH 3
#define WISUN 0x2345
#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == MESH && MBED_CONF_NSAPI_DEFAULT_MESH_TYPE == WISUN
static const int TESTS_TIMEOUT = (25 * 60);
#else
static const int TESTS_TIMEOUT = (10 * 60);
#endif
#endif

static const int TLS_OS_STACK_SIZE = 2048;

static const int RX_BUFF_SIZE = 1220;
static const int TX_BUFF_SIZE = 1220;

extern char rx_buffer[RX_BUFF_SIZE];
extern char tx_buffer[TX_BUFF_SIZE];
extern const char *cert;
}

/*
 * Test cases
 */
void TLSSOCKET_CONNECT_INVALID();
void TLSSOCKET_HANDSHAKE_INVALID();
void TLSSOCKET_ECHOTEST();
void TLSSOCKET_ECHOTEST_NONBLOCK();
void TLSSOCKET_ECHOTEST_BURST();
void TLSSOCKET_ECHOTEST_BURST_NONBLOCK();
void TLSSOCKET_ENDPOINT_CLOSE();
void TLSSOCKET_RECV_TIMEOUT();
void TLSSOCKET_OPEN_TWICE();
void TLSSOCKET_OPEN_LIMIT();
void TLSSOCKET_OPEN_DESTRUCT();
void TLSSOCKET_SEND_UNCONNECTED();
void TLSSOCKET_SEND_CLOSED();
void TLSSOCKET_SEND_REPEAT();
void TLSSOCKET_NO_CERT();
void TLSSOCKET_CERT_IN_FILESYSTEM();
void TLSSOCKET_SIMULTANEOUS();
void TLSSOCKET_SEND_TIMEOUT();

#endif // defined(MBEDTLS_SSL_CLI_C) || defined(DOXYGEN_ONLY)

#endif //TLS_TESTS_H
