/** @file init.cpp
 *
 * mbed Microcontroller Library
 * Copyright (c) 2006-2016 ARM Limited
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
 *
 * Test cases to test initialization/uninitialization code.
 */
#if defined __MBED__ && ! defined TOOLCHAIN_GCC_ARM


#include <stdio.h>
int main()
{
    printf("Not implemented for ARM toolchain\n");
}


#else


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef TARGET_LIKE_FRDM_K64F_GCC
#include <mbed-drivers/mbed.h>
#endif

#include <flash-abstraction/Driver_Common.h>
#include "cfstore_config.h"
#include "../../../source/cfstore_test.h"
#include "../../../source/cfstore_debug.h"
#include <configuration-store/configuration_store.h>
#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"
#ifdef YOTTA_CFG_CONFIG_UVISOR
#include "uvisor-lib/uvisor-lib.h"
#endif /* YOTTA_CFG_CONFIG_UVISOR */


using namespace utest::v1;

static char cfstore_init_utest_msg_g[CFSTORE_UTEST_MSG_BUF_SIZE];

typedef struct cfstore_init_ctx_t
{
    ARM_CFSTORE_CAPABILITIES caps;
} cfstore_init_ctx_t;

static cfstore_init_ctx_t cfstore_init_ctx_g;
extern ARM_CFSTORE_DRIVER cfstore_driver;
ARM_CFSTORE_DRIVER *cfstore_drv = &cfstore_driver;


/* report whether built/configured for flash sync or async mode */
static control_t cfstore_init_test_00(const size_t call_count)
{
    (void) call_count;
    CFSTORE_LOG("INITIALIZING: caps.asynchronous_ops=%lu\n", cfstore_driver.GetCapabilities().asynchronous_ops);
    return CaseNext;
}

static void cfstore_init_test_01(cfstore_init_ctx_t* ctx)
{
    int32_t ret;

    (void) ctx;
	CFSTORE_LOG("INITIALIZING%s", "\r\n");
	ret = cfstore_drv->Initialize(NULL, NULL);
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_init_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: Initialize() should return ret >= 0 for async/synch modes(ret=%ld)\r\n", __func__, ret);
	TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_init_utest_msg_g);

	CFSTORE_LOG("FLUSHING1%s", "\r\n");
	ret = cfstore_drv->Flush();
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_init_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: Flush() failed (ret=%ld)\r\n", __func__, ret);
	TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_init_utest_msg_g);

	CFSTORE_LOG("UNINITIALIZING%s", "\r\n");
	ret = cfstore_drv->Uninitialize();
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_init_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: Uninitialize() should return ret >= 0 for synch mode(ret=%ld)\r\n", __func__, ret);
	TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_init_utest_msg_g);

	CFSTORE_LOG("***************%s", "\r\n");
	CFSTORE_LOG("*** SUCCESS ***%s", "\r\n");
	CFSTORE_LOG("***************%s", "\r\n");
	return;
}

static control_t cfstore_init_app_start(const size_t call_count)
{
    cfstore_init_ctx_t* ctx = &cfstore_init_ctx_g;

    (void) call_count;

    /* initialise the context */
    memset(ctx, 0, sizeof(cfstore_init_ctx_t));
    ctx->caps = cfstore_drv->GetCapabilities();
    CFSTORE_LOG("%s:INITIALIZING: caps.asynchronous_ops=%lu\n", __func__, ctx->caps.asynchronous_ops);
    if(ctx->caps.asynchronous_ops == true){
    	/* This is a sync mode only test. If this test is not built for sync mode, then skip testing return true
    	 * This means the test will conveniently pass when run in CI as part of async mode testing */
        CFSTORE_LOG("*** Skipping test as binary built for flash journal async mode, and this test is sync-only%s", "\n");
        return CaseNext;
    }
    cfstore_init_test_01(ctx);
    return CaseNext;
}

#ifndef YOTTA_CONFIGURATION_STORE_INIT_VERSION_STRING


/* when built as Configuration-Store example, include greentea support otherwise omit */

utest::v1::status_t greentea_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(100, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
           /*          1         2         3         4         5         6        7  */
           /* 1234567890123456789012345678901234567890123456789012345678901234567890 */
        Case("INIT_test_00", cfstore_init_test_00),
        Case("INIT_test_01_start", cfstore_init_app_start),
};


/* Declare your test specification with a custom setup handler */
Specification specification(greentea_setup, cases);

#if defined CFSTORE_CONFIG_MBED_OS_VERSION && CFSTORE_CONFIG_MBED_OS_VERSION == 3
/* mbedosV3*/
void app_start(int argc __unused, char** argv __unused)
{
    /* Run the test specification */
    Harness::run(specification);
}
#endif /* CFSTORE_CONFIG_MBED_OS_VERSION == 3 */

#if defined CFSTORE_CONFIG_MBED_OS_VERSION && CFSTORE_CONFIG_MBED_OS_VERSION == 4
/* mbedosV3++*/
int main()
{
    return !Harness::run(specification);
}
#endif /* CFSTORE_CONFIG_MBED_OS_VERSION == 4 */


#else   // YOTTA_CONFIGURATION_STORE_INIT_VERSION_STRING


// stand alone Configuration-Store-Example
void app_start(int argc __unused, char** argv __unused)
{
    cfstore_init_app_start(0);
}


#endif // YOTTA_CONFIGURATION_STORE_INIT_VERSION_STRING


#endif // __MBED__ && ! defined TOOLCHAIN_GCC_ARM
