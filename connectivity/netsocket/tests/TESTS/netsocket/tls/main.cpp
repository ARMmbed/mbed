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

#if !defined(MBED_CONF_RTOS_PRESENT)
#error [NOT_SUPPORTED] tls test cases require a RTOS to run
#else
#define WIFI 2
#if !defined(MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE) || \
    (MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == WIFI && !defined(MBED_CONF_NSAPI_DEFAULT_WIFI_SSID))
#error [NOT_SUPPORTED] No network configuration found for this target.
#else

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity/unity.h"
#include "utest.h"
#include "utest/utest_stack_trace.h"
#include "tls_tests.h"
#include "cert.h"
#ifdef MBED_CONF_APP_BAUD_RATE
#include "CellularDevice.h"
#endif
#include "ip6string.h"

#ifndef ECHO_SERVER_ADDR
#error [NOT_SUPPORTED] Requires parameters for echo server
#else

#if defined(MBEDTLS_SSL_CLI_C) || defined(DOXYGEN_ONLY)

using namespace utest::v1;

namespace {
Timer tc_bucket; // Timer to limit a test cases run time
}

#if MBED_CONF_NSAPI_SOCKET_STATS_ENABLED
mbed_stats_socket_t tls_stats[MBED_CONF_NSAPI_SOCKET_STATS_MAX_COUNT];
#endif

char tls_global::rx_buffer[RX_BUFF_SIZE];
char tls_global::tx_buffer[TX_BUFF_SIZE];

void drop_bad_packets(TLSSocket &sock, int orig_timeout)
{
    nsapi_error_t err;
    sock.set_timeout(0);
    while (true) {
        err = sock.recv(NULL, 0);
        if (err == NSAPI_ERROR_WOULD_BLOCK) {
            break;
        }
    }
    sock.set_timeout(orig_timeout);
}

static void _ifup()
{
    NetworkInterface *net = NetworkInterface::get_default_instance();
    nsapi_error_t err = net->connect();
    TEST_ASSERT_EQUAL(NSAPI_ERROR_OK, err);
    SocketAddress address;
    net->get_ip_address(&address);

#define MESH 3
#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == MESH
    printf("Waiting for GLOBAL_UP\n");
    while (net->get_connection_status() != NSAPI_STATUS_GLOBAL_UP) {
        ThisThread::sleep_for(500);
    }
#endif
    printf("MBED: TLSClient address is '%s'\n", address ? address.get_ip_address() : "null");
}

static void _ifdown()
{
    NetworkInterface::get_default_instance()->disconnect();
    tr_info("MBED: ifdown\n");
}

nsapi_error_t tlssocket_connect_to_srv(TLSSocket &sock, uint16_t port)
{
    SocketAddress tls_addr;

    NetworkInterface::get_default_instance()->gethostbyname(ECHO_SERVER_ADDR, &tls_addr);
    tls_addr.set_port(port);

    tr_info("MBED: Server '%s', port %d\n", tls_addr.get_ip_address(), tls_addr.get_port());

    nsapi_error_t err = sock.open(NetworkInterface::get_default_instance());
    if (err != NSAPI_ERROR_OK) {
        tr_error("Error from sock.open: %d\n", err);
        return err;
    }

    err = sock.set_root_ca_cert(tls_global::cert);
    if (err != NSAPI_ERROR_OK) {
        tr_error("Error from sock.set_root_ca_cert: %d\n", err);
        return err;
    }

    err = sock.connect(tls_addr);
    if (err != NSAPI_ERROR_OK) {
        tr_error("Error from sock.connect: %d\n", err);
        return err;
    }

    return NSAPI_ERROR_OK;
}

nsapi_error_t tlssocket_connect_to_echo_srv(TLSSocket &sock)
{
    return tlssocket_connect_to_srv(sock, ECHO_SERVER_PORT_TLS);
}

nsapi_error_t tlssocket_connect_to_discard_srv(TLSSocket &sock)
{
    return tlssocket_connect_to_srv(sock, ECHO_SERVER_DISCARD_PORT_TLS);
}

bool is_tcp_supported()
{
    static bool supported;
    static bool tested = false;
    if (!tested) {
        TCPSocket socket;
        supported = socket.open(NetworkInterface::get_default_instance()) == NSAPI_ERROR_OK;
    }
    return supported;
}

void fill_tx_buffer_ascii(char *buff, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        buff[i] = (rand() % 43) + '0';
    }
}

int split2half_rmng_tls_test_time()
{
    return (tls_global::TESTS_TIMEOUT - tc_bucket.read()) / 2;
}

#if MBED_CONF_NSAPI_SOCKET_STATS_ENABLED
int fetch_stats()
{
    return SocketStats::mbed_stats_socket_get_each(&tls_stats[0], MBED_CONF_NSAPI_SOCKET_STATS_MAX_COUNT);
}
#endif

// Test setup
utest::v1::status_t greentea_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(tls_global::TESTS_TIMEOUT, "default_auto");
    _ifup();

#ifdef MBED_CONF_APP_BAUD_RATE
    CellularDevice::get_default_instance()->set_baud_rate(MBED_CONF_APP_BAUD_RATE);
#endif

    tc_bucket.start();
    return greentea_test_setup_handler(number_of_cases);
}

void greentea_teardown(const size_t passed, const size_t failed, const failure_t failure)
{
    tc_bucket.stop();
    _ifdown();
    return greentea_test_teardown_handler(passed, failed, failure);
}

utest::v1::status_t greentea_case_setup_handler_tls(const Case *const source, const size_t index_of_case)
{
#if MBED_CONF_NSAPI_SOCKET_STATS_ENABLED
    int count = fetch_stats();
    for (int j = 0; j < count; j++) {
        TEST_ASSERT_EQUAL(SOCK_CLOSED,  tls_stats[j].state);
    }
#endif
    return greentea_case_setup_handler(source, index_of_case);
}

utest::v1::status_t greentea_case_teardown_handler_tls(const Case *const source, const size_t passed, const size_t failed, const failure_t failure)
{
#if MBED_CONF_NSAPI_SOCKET_STATS_ENABLED
    int count = fetch_stats();
    for (int j = 0; j < count; j++) {
        TEST_ASSERT_EQUAL(SOCK_CLOSED,  tls_stats[j].state);
    }
#endif
    return greentea_case_teardown_handler(source, passed, failed, failure);
}

static void test_failure_handler(const failure_t failure)
{
    UTEST_LOG_FUNCTION();
    if (failure.location == LOCATION_TEST_SETUP || failure.location == LOCATION_TEST_TEARDOWN) {
        verbose_test_failure_handler(failure);
        GREENTEA_TESTSUITE_RESULT(false);
        while (1) ;
    }
}


Case cases[] = {
    Case("TLSSOCKET_ECHOTEST", TLSSOCKET_ECHOTEST),
    Case("TLSSOCKET_ECHOTEST_NONBLOCK", TLSSOCKET_ECHOTEST_NONBLOCK),
    Case("TLSSOCKET_CONNECT_INVALID", TLSSOCKET_CONNECT_INVALID),
    Case("TLSSOCKET_ECHOTEST_BURST", TLSSOCKET_ECHOTEST_BURST),
    Case("TLSSOCKET_ECHOTEST_BURST_NONBLOCK", TLSSOCKET_ECHOTEST_BURST_NONBLOCK),
    Case("TLSSOCKET_RECV_TIMEOUT", TLSSOCKET_RECV_TIMEOUT),
    Case("TLSSOCKET_ENDPOINT_CLOSE", TLSSOCKET_ENDPOINT_CLOSE),
    Case("TLSSOCKET_HANDSHAKE_INVALID", TLSSOCKET_HANDSHAKE_INVALID),
    Case("TLSSOCKET_OPEN_TWICE", TLSSOCKET_OPEN_TWICE),
    Case("TLSSOCKET_OPEN_LIMIT", TLSSOCKET_OPEN_LIMIT),
    Case("TLSSOCKET_OPEN_DESTRUCT", TLSSOCKET_OPEN_DESTRUCT),
    Case("TLSSOCKET_SEND_UNCONNECTED", TLSSOCKET_SEND_UNCONNECTED),
    Case("TLSSOCKET_SEND_CLOSED", TLSSOCKET_SEND_CLOSED),
    Case("TLSSOCKET_SEND_REPEAT", TLSSOCKET_SEND_REPEAT),
    Case("TLSSOCKET_SEND_TIMEOUT", TLSSOCKET_SEND_TIMEOUT),
    Case("TLSSOCKET_NO_CERT", TLSSOCKET_NO_CERT),
#if defined(MBEDTLS_SSL_CLI_C) && defined(MBEDTLS_FS_IO)
    Case("TLSSOCKET_CERT_IN_FILESYSTEM", TLSSOCKET_CERT_IN_FILESYSTEM),
#endif
//    Temporarily removing this test, as TLS library consumes too much memory
//    and we see frequent memory allocation failures on architectures with less
//    RAM such as DISCO_L475VG_IOT1A and NUCLEO_F207ZG (both have 128 kB RAM)
//    This test also fails for IAR, due to wrong heap configuration in the linker
//    script - see https://github.com/ARMmbed/mbed-os/issues/8306
//    Case("TLSSOCKET_SIMULTANEOUS", TLSSOCKET_SIMULTANEOUS)
};

const handlers_t tls_test_case_handlers = {
    default_greentea_test_setup_handler,
    greentea_test_teardown_handler,
    test_failure_handler,
    greentea_case_setup_handler_tls,
    greentea_case_teardown_handler_tls,
    greentea_case_failure_continue_handler
};

Specification specification(greentea_setup, cases, greentea_teardown, tls_test_case_handlers);

int retval;
void run_test(void)
{
    retval = !Harness::run(specification);
}

static unsigned char stack_mem[8192];
int main()
{
    Thread *th = new Thread(osPriorityNormal, 8192, stack_mem, "tls_gt_thread");
    th->start(callback(run_test));
    th->join();
    return retval;
}

#else
#error [NOT_SUPPORTED] This device does not support SSL library
#endif // defined(MBEDTLS_SSL_CLI_C) || defined(DOXYGEN_ONLY)

#endif // ECHO_SERVER_ADDR
#endif // !defined(MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE) || (MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == WIFI && !defined(MBED_CONF_NSAPI_DEFAULT_WIFI_SSID))
#endif // !defined(MBED_CONF_RTOS_PRESENT)
