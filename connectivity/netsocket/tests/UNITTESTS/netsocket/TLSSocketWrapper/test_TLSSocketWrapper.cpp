/*
 * Copyright (c) 2018, Arm Limited and affiliates
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

#include "gtest/gtest.h"
#include "netsocket/TCPSocket.h"
#include "netsocket/TLSSocketWrapper.h"
#include "NetworkStack_stub.h"
#include "mbedtls_stub.h"
#include <cstring> //memset

#include "mbed_error.h"
mbed_error_status_t mbed_error(mbed_error_status_t error_status, const char *error_msg, unsigned int error_value, const char *filename, int line_number)
{
    return 0;
}

// Control the rtos EventFlags stub. See EventFlags_stub.cpp
extern std::list<uint32_t> eventFlagsStubNextRetval;

class TestTLSSocketWrapper : public testing::Test {
public:
    unsigned int dataSize = 10;
    char dataBuf[10];
protected:
    TCPSocket *transport;
    TLSSocketWrapper *wrapper;
    NetworkStackstub stack;

    virtual void SetUp()
    {
        memset(mbedtls_stub.retArray, 0, sizeof(mbedtls_stub.retArray));
        mbedtls_stub.useCounter = false;
        mbedtls_stub.counter = 0;
        mbedtls_stub.crt_expected_int = 0;
        mbedtls_stub.expected_int = 0;
        transport = new TCPSocket();
        wrapper = new TLSSocketWrapper(transport);
    }

    virtual void TearDown()
    {
        stack.return_values.clear();
        eventFlagsStubNextRetval.clear();
        delete wrapper;
        delete transport;
    }

    char *cert = "-----BEGIN CERTIFICATE-----\
                MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/\
                MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\
                DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow\
                SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT\
                GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC\
                AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF\
                q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8\
                SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0\
                Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA\
                a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj\
                /PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T\
                AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG\
                CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv\
                bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k\
                c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw\
                VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC\
                ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz\
                MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu\
                Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF\
                AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo\
                uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/\
                wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu\
                X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG\
                PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6\
                KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==\
                -----END CERTIFICATE-----";
};

static bool callback_is_called;
static void my_callback()
{
    callback_is_called = true;
}

TEST_F(TestTLSSocketWrapper, constructor)
{
    EXPECT_TRUE(wrapper);
}

TEST_F(TestTLSSocketWrapper, constructor_hostname)
{
    TLSSocketWrapper *wrapper2 = new TLSSocketWrapper(transport, "localhost");
    delete wrapper2;
}

TEST_F(TestTLSSocketWrapper, no_socket)
{
    TLSSocketWrapper *wrapperTmp = new TLSSocketWrapper(NULL);
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapperTmp->connect(a), NSAPI_ERROR_NO_SOCKET);
    EXPECT_EQ(wrapperTmp->bind(a), NSAPI_ERROR_NO_SOCKET);
    EXPECT_EQ(wrapperTmp->setsockopt(0, 0, 0, 0), NSAPI_ERROR_NO_SOCKET);
    EXPECT_EQ(wrapperTmp->getsockopt(0, 0, 0, 0), NSAPI_ERROR_NO_SOCKET);
    EXPECT_EQ(wrapperTmp->send(dataBuf, dataSize), NSAPI_ERROR_NO_SOCKET);
    EXPECT_EQ(wrapperTmp->recv(dataBuf, dataSize), NSAPI_ERROR_NO_SOCKET);
    EXPECT_EQ(wrapperTmp->close(), NSAPI_ERROR_NO_SOCKET);
    delete wrapperTmp;
}

/* connect */

TEST_F(TestTLSSocketWrapper, connect)
{
    transport->open(&stack);
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_IS_CONNECTED);
}

/* connect: TCP-related errors */

TEST_F(TestTLSSocketWrapper, connect_no_open)
{
    stack.return_value = NSAPI_ERROR_OK;
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_NO_SOCKET);
}

TEST_F(TestTLSSocketWrapper, connect_error_in_progress_no_timeout)
{
    transport->open(&stack);
    stack.return_value = NSAPI_ERROR_IN_PROGRESS;
    const SocketAddress a("127.0.0.1", 1024);
    eventFlagsStubNextRetval.push_back(osFlagsError); // Break the wait loop
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_IN_PROGRESS);
}

TEST_F(TestTLSSocketWrapper, connect_with_timeout)
{
    transport->open(&stack);
    stack.return_value = NSAPI_ERROR_IN_PROGRESS;
    const SocketAddress a("127.0.0.1", 1024);
    eventFlagsStubNextRetval.push_back(osFlagsError); // Break the wait loop
    wrapper->set_blocking(true);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_IN_PROGRESS);
}

TEST_F(TestTLSSocketWrapper, connect_error_is_connected)
{
    transport->open(&stack);
    const SocketAddress a("127.0.0.1", 1024);
    wrapper->set_timeout(1);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[0] = 1; //  mbedtls_ctr_drbg_seed error
}

/* connect: SSL-related errors */

TEST_F(TestTLSSocketWrapper, connect_fail_ctr_drbg_seed)
{
    transport->open(&stack);
    mbedtls_stub.crt_expected_int = 1; // mbedtls_ctr_drbg_seed error
    stack.return_value = NSAPI_ERROR_OK;
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_AUTH_FAILURE);
    mbedtls_stub.crt_expected_int = 0;
}

TEST_F(TestTLSSocketWrapper, connect_fail_ssl_setup)
{
    transport->open(&stack);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[0] = 0; // mbedtls_ssl_config_defaults ok
    mbedtls_stub.retArray[1] = 2; // mbedtls_ssl_setup           error
    stack.return_value = NSAPI_ERROR_OK;
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_AUTH_FAILURE);
}

TEST_F(TestTLSSocketWrapper, connect_handshake_fail_ssl_handshake)
{
    transport->open(&stack);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[2] = -1; // mbedtls_ssl_handshake error
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_AUTH_FAILURE);
}

TEST_F(TestTLSSocketWrapper, connect_handshake_fail_ssl_handshake_in_progress)
{
    transport->open(&stack);
    mbedtls_stub.useCounter = true;
    wrapper->set_timeout(1);
    eventFlagsStubNextRetval.push_back(osFlagsError); // Break the wait loop
    mbedtls_stub.retArray[2] = MBEDTLS_ERR_SSL_WANT_READ; // mbedtls_ssl_handshake error
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_IN_PROGRESS);

    // Check that send will fail in this situation.
    mbedtls_stub.retArray[3] = MBEDTLS_ERR_SSL_WANT_READ; // mbedtls_ssl_handshake error
    eventFlagsStubNextRetval.push_back(osFlagsError); // Break the wait loop
    EXPECT_EQ(wrapper->send(dataBuf, dataSize), NSAPI_ERROR_WOULD_BLOCK);

    // Check that recv will fail in this situation.
    mbedtls_stub.retArray[4] = MBEDTLS_ERR_SSL_WANT_READ; // mbedtls_ssl_handshake error
    eventFlagsStubNextRetval.push_back(osFlagsError); // Break the wait loop
    EXPECT_EQ(wrapper->recv(dataBuf, dataSize), NSAPI_ERROR_WOULD_BLOCK);
}

TEST_F(TestTLSSocketWrapper, connect_handshake_fail_ssl_get_verify_result)
{
    transport->open(&stack);
    mbedtls_stub.uint32_value = 1; // mbedtls_ssl_get_verify_result error
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
}

/* send */

TEST_F(TestTLSSocketWrapper, send_no_open)
{
    stack.return_value = NSAPI_ERROR_OK;
    EXPECT_EQ(wrapper->send(dataBuf, dataSize), NSAPI_ERROR_NO_CONNECTION);
}

TEST_F(TestTLSSocketWrapper, send_in_one_chunk)
{
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[3] = dataSize; // mbedtls_ssl_write
    transport->open(&stack);
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->send(dataBuf, dataSize), dataSize);
}

TEST_F(TestTLSSocketWrapper, send_in_two_chunks)
{
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[3] = dataSize; // mbedtls_ssl_write
    transport->open(&stack);
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->send(dataBuf, dataSize), dataSize);
}

TEST_F(TestTLSSocketWrapper, send_error_would_block)
{
    transport->open(&stack);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[3] = MBEDTLS_ERR_SSL_WANT_WRITE; // mbedtls_ssl_write
    eventFlagsStubNextRetval.push_back(osFlagsError); // Break the wait loop
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->send(dataBuf, dataSize), NSAPI_ERROR_WOULD_BLOCK);
}

TEST_F(TestTLSSocketWrapper, send_device_error)
{
    transport->open(&stack);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[3] = MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE; // mbedtls_ssl_write
    eventFlagsStubNextRetval.push_back(osFlagsError); // Break the wait loop
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->send(dataBuf, dataSize), NSAPI_ERROR_DEVICE_ERROR);
}

TEST_F(TestTLSSocketWrapper, send_to)
{
    transport->open(&stack);
    mbedtls_stub.useCounter = true;

    mbedtls_stub.retArray[3] = dataSize; // mbedtls_ssl_write
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->sendto(a, dataBuf, dataSize), dataSize);
}

/* recv */

TEST_F(TestTLSSocketWrapper, recv_no_open)
{
    stack.return_value = NSAPI_ERROR_OK;
    EXPECT_EQ(wrapper->recv(dataBuf, dataSize), NSAPI_ERROR_NO_CONNECTION);
}

TEST_F(TestTLSSocketWrapper, recv_all_data)
{
    transport->open(&stack);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[3] = dataSize; // mbedtls_ssl_read
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->recv(dataBuf, dataSize), dataSize);
}

TEST_F(TestTLSSocketWrapper, recv_less_than_expected)
{
    transport->open(&stack);
    unsigned int lessThanDataSize = dataSize - 1;
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[3] = lessThanDataSize; // mbedtls_ssl_read
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->recv(dataBuf, dataSize), lessThanDataSize);
}

TEST_F(TestTLSSocketWrapper, recv_would_block)
{
    transport->open(&stack);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[3] = MBEDTLS_ERR_SSL_WANT_WRITE; // mbedtls_ssl_read
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    eventFlagsStubNextRetval.push_back(osFlagsError); // Break the wait loop
    EXPECT_EQ(wrapper->recv(dataBuf, dataSize), NSAPI_ERROR_WOULD_BLOCK);
}

TEST_F(TestTLSSocketWrapper, recv_device_error)
{
    transport->open(&stack);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[3] = MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE; // mbedtls_ssl_read
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    eventFlagsStubNextRetval.push_back(osFlagsError); // Break the wait loop
    EXPECT_EQ(wrapper->recv(dataBuf, dataSize), NSAPI_ERROR_DEVICE_ERROR);
}

TEST_F(TestTLSSocketWrapper, recv_peer_clode_notify)
{
    transport->open(&stack);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[3] = MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY; // mbedtls_ssl_read
    const SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    eventFlagsStubNextRetval.push_back(osFlagsError); // Break the wait loop
    EXPECT_EQ(wrapper->recv(dataBuf, dataSize), 0);
}

TEST_F(TestTLSSocketWrapper, recv_from_no_socket)
{
    SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->recvfrom(&a, dataBuf, dataSize), NSAPI_ERROR_NO_CONNECTION);
}

TEST_F(TestTLSSocketWrapper, recv_from)
{
    SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(transport->open(&stack), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    SocketAddress b;
    EXPECT_EQ(wrapper->recvfrom(&b, dataBuf, dataSize), NSAPI_ERROR_OK);
    EXPECT_EQ(a, b);
}

TEST_F(TestTLSSocketWrapper, recv_from_null)
{
    SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(transport->open(&stack), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->connect(a), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->recvfrom(NULL, dataBuf, dataSize), NSAPI_ERROR_OK);
}

/* set_root_ca_cert */

TEST_F(TestTLSSocketWrapper, set_root_ca_cert)
{
    EXPECT_EQ(wrapper->get_ca_chain(), static_cast<mbedtls_x509_crt *>(NULL));
    EXPECT_EQ(transport->open(&stack), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->set_root_ca_cert(cert, strlen(cert)), NSAPI_ERROR_OK);
    EXPECT_NE(wrapper->get_ca_chain(), static_cast<mbedtls_x509_crt *>(NULL));
}

TEST_F(TestTLSSocketWrapper, set_root_ca_cert_nolen)
{
    EXPECT_EQ(transport->open(&stack), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->set_root_ca_cert(cert), NSAPI_ERROR_OK);
}

TEST_F(TestTLSSocketWrapper, set_root_ca_cert_invalid)
{
    EXPECT_EQ(transport->open(&stack), NSAPI_ERROR_OK);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[0] = 1; // mbedtls_x509_crt_parse error
    EXPECT_EQ(wrapper->set_root_ca_cert(cert, strlen(cert)), NSAPI_ERROR_PARAMETER);
}

TEST_F(TestTLSSocketWrapper, set_root_ca_cert_path)
{
    EXPECT_EQ(transport->open(&stack), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->set_root_ca_cert_path("/"), NSAPI_ERROR_OK);
}

TEST_F(TestTLSSocketWrapper, set_client_cert_key)
{
    EXPECT_EQ(wrapper->get_own_cert(), static_cast<mbedtls_x509_crt *>(NULL));
    EXPECT_EQ(transport->open(&stack), NSAPI_ERROR_OK);
    EXPECT_EQ(wrapper->set_client_cert_key(cert, cert), NSAPI_ERROR_OK);
    EXPECT_NE(wrapper->get_own_cert(), static_cast<mbedtls_x509_crt *>(NULL));
}

TEST_F(TestTLSSocketWrapper, set_client_cert_key_invalid)
{
    EXPECT_EQ(transport->open(&stack), NSAPI_ERROR_OK);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[0] = 1; // mbedtls_x509_crt_parse error
    EXPECT_EQ(wrapper->set_client_cert_key(cert, cert), NSAPI_ERROR_PARAMETER);
}

TEST_F(TestTLSSocketWrapper, set_client_cert_key_invalid_pem)
{
    EXPECT_EQ(transport->open(&stack), NSAPI_ERROR_OK);
    mbedtls_stub.useCounter = true;
    mbedtls_stub.retArray[0] = 0; // mbedtls_x509_crt_parse ok
    mbedtls_stub.retArray[1] = 1; // mbedtls_pk_parse_key error
    EXPECT_EQ(wrapper->set_client_cert_key(cert, cert), NSAPI_ERROR_PARAMETER);
}

TEST_F(TestTLSSocketWrapper, bind)
{
    transport->open(&stack);
    SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->bind(a), NSAPI_ERROR_OK);
}

TEST_F(TestTLSSocketWrapper, bind_no_open)
{
    SocketAddress a("127.0.0.1", 1024);
    EXPECT_EQ(wrapper->bind(a), NSAPI_ERROR_NO_SOCKET);
}

TEST_F(TestTLSSocketWrapper, sigio)
{
    transport->open(&stack);
    callback_is_called = false;
    wrapper->sigio(mbed::callback(my_callback));
    SocketAddress a("127.0.0.1", 1024);
    wrapper->close(); //trigger events.
    EXPECT_EQ(callback_is_called, true);
}

TEST_F(TestTLSSocketWrapper, setsockopt)
{
    transport->open(&stack);
    EXPECT_EQ(wrapper->setsockopt(0, 0, 0, 0), NSAPI_ERROR_UNSUPPORTED);
}

TEST_F(TestTLSSocketWrapper, getsockopt_no_stack)
{
    EXPECT_EQ(wrapper->getsockopt(0, 0, 0, 0), NSAPI_ERROR_NO_SOCKET);
}

TEST_F(TestTLSSocketWrapper, getsockopt)
{
    transport->open(&stack);
    EXPECT_EQ(wrapper->getsockopt(0, 0, 0, 0), NSAPI_ERROR_UNSUPPORTED);
}

/* unsupported */

TEST_F(TestTLSSocketWrapper, listen_unsupported)
{
    EXPECT_EQ(wrapper->listen(1), NSAPI_ERROR_UNSUPPORTED);
}

TEST_F(TestTLSSocketWrapper, accept_unsupported)
{
    nsapi_error_t error;
    EXPECT_EQ(wrapper->accept(&error), static_cast<TCPSocket *>(NULL));
    EXPECT_EQ(error, NSAPI_ERROR_UNSUPPORTED);
}
