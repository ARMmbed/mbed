/*
 * Copyright (c) 2020, Arduino SA, All Rights Reserved
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

#include "mbed.h"
#include "TLSSocket.h"
#include "greentea-client/test_env.h"
#include "unity/unity.h"
#include "utest.h"
#include "tls_tests.h"
#include "HeapBlockDevice.h"
#include "LittleFileSystem.h"

using namespace utest::v1;

void TLSSOCKET_CERT_IN_FILESYSTEM()
{
    SKIP_IF_TCP_UNSUPPORTED();

    HeapBlockDevice bd(1024 * 10);
    LittleFileSystem fs("fs");
    TEST_ASSERT_EQUAL(0, fs.format(&bd));
    TEST_ASSERT_EQUAL(0, fs.mount(&bd));

    FILE *fp = fopen("/fs/certs.pem", "wb");
    int ret = fwrite(tls_global::cert, strlen(tls_global::cert), 1, fp);
    fclose(fp);

    TLSSocket sock;
    TEST_ASSERT_EQUAL(NSAPI_ERROR_OK, sock.open(NetworkInterface::get_default_instance()));
    TEST_ASSERT_EQUAL(NSAPI_ERROR_OK, sock.set_root_ca_cert_path("/fs"));

    SocketAddress a;
    TEST_ASSERT_EQUAL(NSAPI_ERROR_OK, NetworkInterface::get_default_instance()->gethostbyname(ECHO_SERVER_ADDR, &a));
    a.set_port(ECHO_SERVER_PORT_TLS);
    TEST_ASSERT_EQUAL(NSAPI_ERROR_OK, sock.connect(a));
}

