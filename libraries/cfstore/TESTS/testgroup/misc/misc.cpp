/*
 * @file misc.cpp
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
 * Test cases for miscellaneious API drv->Xxx() functions.
 */
#if defined __MBED__ && ! defined TOOLCHAIN_GCC_ARM


#include <stdio.h>
int main()
{
    printf("Not implemented for ARM toolchain\n");
}


#else


#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#ifdef TARGET_LIKE_FRDM_K64F_GCC
#include <mbed-drivers/mbed.h>
#endif

#include "cfstore_config.h"
// todo: on morpheus: if the above doesnt need the path to the source subdir then why do the following?
// maybe this is specific to the morpheus build system?
#include "../../../source/cfstore_test.h"
#include "../../../source/cfstore_debug.h"
#include <flash-abstraction/Driver_Common.h>
#include "cfstore_config.h"
#include <configuration-store/configuration_store.h>
#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"
#include "../../../source/cfstore_utest.h"
#ifdef YOTTA_CFG_CFSTORE_UVISOR
#include "uvisor-lib/uvisor-lib.h"
#include "cfstore_uvisor.h"
#endif /* YOTTA_CFG_CFSTORE_UVISOR */

using namespace utest::v1;

static char cfstore_misc_utest_msg_g[CFSTORE_UTEST_MSG_BUF_SIZE];

#ifdef YOTTA_CFG_CFSTORE_UVISOR
/* Create the main box ACL list for the application.
 * The main ACL gets inherited by all the other boxes
 */
CFSTORE_UVISOR_MAIN_ACL(cfstore_acl_uvisor_box_add_del_g);

/* Enable uVisor. */
UVISOR_SET_MODE_ACL(UVISOR_ENABLED, cfstore_acl_uvisor_box_add_del_g);
#endif /* YOTTA_CFG_CFSTORE_UVISOR */



/* report whether built/configured for flash sync or async mode */
static control_t cfstore_misc_test_00(const size_t call_count)
{
    (void) call_count;
    CFSTORE_LOG("INITIALIZING: caps.asynchronous_ops=%lu\n", cfstore_driver.GetCapabilities().asynchronous_ops);
    return CaseNext;
}


/** @brief  basic PowerControl() test
 *
 * @return  status code
 *          ARM_DRIVER_OK, the test passed successfully
 *          ret < ARM_DRIVER_OK, the test failed with the return code
 *          supplying more details
 */
static control_t cfstore_misc_test_00_start(const size_t call_count)
{
    int32_t ret = ARM_DRIVER_ERROR;
    ARM_CFSTORE_DRIVER* drv = &cfstore_driver;
    ARM_POWER_STATE state = ARM_POWER_OFF;

    CFSTORE_FENTRYLOG("%s:entered\n", __func__);
    (void) call_count;

    /* try setting the power control state before initialised, which should fail */
    ret = drv->PowerControl(state);
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Eror: PowerControl() call should have failed as CFSTORE not initialised, but the call succeeded\r\n", __func__);
    TEST_ASSERT_MESSAGE(ret < ARM_DRIVER_OK, cfstore_misc_utest_msg_g);

    ret = drv->Initialize(cfstore_utest_default_callback, NULL);
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: failed to initialize CFSTORE (ret=%" PRId32 ")\n", __func__, ret);
    TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);
    return CaseTimeout(CFSTORE_UTEST_DEFAULT_TIMEOUT_MS);
}

static control_t cfstore_misc_test_00_end(const size_t call_count)
{
    int32_t ret = ARM_DRIVER_ERROR;
    ARM_CFSTORE_DRIVER* drv = &cfstore_driver;
    ARM_POWER_STATE state = ARM_POWER_OFF;

    CFSTORE_FENTRYLOG("%s:entered\n", __func__);
    (void) call_count;

    while(state <= ARM_POWER_FULL)
    {
        ret = drv->PowerControl(state);
        CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: PowerControl() call failed\r\n", __func__);
        TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);
        state = (ARM_POWER_STATE)((uint32_t) state + 1);
    }
    /*try invalid value which should fail*/
    ret = drv->PowerControl((ARM_POWER_STATE ) 0xffffffff);
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:ERror: PowerControl() did not fail with bad value 0xffffffff (not as expected)\r\n", __func__);
    TEST_ASSERT_MESSAGE(ret < ARM_DRIVER_OK, cfstore_misc_utest_msg_g);

    ret = drv->Uninitialize();
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: Uninitialize() call failed.\n", __func__);
    TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);
    return CaseNext;
}


/** @brief  basic GetVersion() test
 *
 * @return  status code
 *          ARM_DRIVER_OK, the test passed successfully
 *          ret < ARM_DRIVER_OK, the test failed with the return code
 *          supplying more details
 */
static control_t cfstore_misc_test_01(const size_t call_count)
{
    ARM_CFSTORE_DRIVER* drv = &cfstore_driver;
    ARM_DRIVER_VERSION version;

    CFSTORE_FENTRYLOG("%s:entered\n", __func__);
    (void) call_count;
    memset(&version, 0, sizeof(version));

    version = drv->GetVersion();
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:GetVersion() failed to return correct API version.\r\n", __func__);
    TEST_ASSERT_MESSAGE(version.api == ARM_CFSTORE_API_VERSION, cfstore_misc_utest_msg_g);

    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:GetVersion() to return correct driver version.\r\n", __func__);
    TEST_ASSERT_MESSAGE(version.drv == ARM_CFSTORE_DRV_VERSION, cfstore_misc_utest_msg_g);
    return CaseNext;
}


/** @brief  basic GetCapabilities() test
 *
 * @return  status code
 *          ARM_DRIVER_OK, the test passed successfully
 *          ret < ARM_DRIVER_OK, the test failed with the return code
 *          supplying more details
 */
static control_t cfstore_misc_test_02(const size_t call_count)
{
    ARM_CFSTORE_DRIVER* drv = &cfstore_driver;
    ARM_CFSTORE_CAPABILITIES caps;

    CFSTORE_FENTRYLOG("%s:entered\n", __func__);
    (void) call_count;
    memset(&caps, 0, sizeof(caps));
    caps = drv->GetCapabilities();
    //CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Capabilities() failed to return asynchronous_ops == false.\r\n", __func__);
    //TEST_ASSERT_MESSAGE(caps.asynchronous_ops == false, cfstore_misc_utest_msg_g);

#ifdef YOTTA_CFG_CONFIG_HARDWARE_MTD_ASYNC_OPS
    /* sync mode */
    printf("%s:sync mode: caps.asynchronous_ops =%" PRIu32 "\n", __func__, caps.asynchronous_ops);
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: GetCapabilities() reported caps.asynchronous_ops != false but system built for sync operation.\r\n", __func__);
    TEST_ASSERT_MESSAGE(caps.asynchronous_ops == false, cfstore_misc_utest_msg_g);
#else
    /* async mode */
    printf("%s:async mode: caps.asynchronous_ops =%" PRIu32 "\n", __func__, caps.asynchronous_ops);
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: GetCapabilities() reported caps.asynchronous_ops != true but system built for async operation.\r\n", __func__);
    TEST_ASSERT_MESSAGE(caps.asynchronous_ops == true, cfstore_misc_utest_msg_g);
#endif


    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Capabilities() failed to return uvisor_support_enabled == false.\r\n", __func__);
    TEST_ASSERT_MESSAGE(caps.uvisor_support_enabled == false, cfstore_misc_utest_msg_g);
    return CaseNext;
}


/* KV data for test_03 */
static cfstore_kv_data_t cfstore_misc_test_03_kv_data[] = {
        /*          1         2         3         4         5         6        7  */
        /* 1234567890123456789012345678901234567890123456789012345678901234567890 */
        { "The.principles.of.least.action.in.quantum.mechanics", "DoctoralThesis"},
        { "Space.Time.Approach.to.Quantum.Electrodynamic", " PhysicalReview766)"},
        { "An.Operator.Calculus.Having.Applications.in.Quantum.Electrodynamics", "PhysicalReview84)"},
        { NULL, NULL},
};


/** @brief  basic GetKeyName() test
 *
 * @return  status code
 *          ARM_DRIVER_OK, the test passed successfully
 *          ret < ARM_DRIVER_OK, the test failed with the return code
 *          supplying more details
 */
static control_t cfstore_misc_test_03_end(const size_t call_count)
{
    uint8_t key_name_len = 0;
    char key_name_buf[CFSTORE_KEY_NAME_MAX_LENGTH+1];
    int32_t ret = ARM_DRIVER_ERROR;
    ARM_CFSTORE_DRIVER* drv = &cfstore_driver;
    cfstore_kv_data_t* node = NULL;
    ARM_CFSTORE_HANDLE_INIT(hkey);
    ARM_CFSTORE_FMODE flags;

    CFSTORE_FENTRYLOG("%s:entered\r\n", __func__);
    (void) call_count;
    memset(key_name_buf, 0, CFSTORE_KEY_NAME_MAX_LENGTH+1);
    /* dont set any flags to get default settings */
    memset(&flags, 0, sizeof(flags));

    ret = cfstore_test_create_table(cfstore_misc_test_03_kv_data);
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: unable to create keys from table.\r\n", __func__);
    TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);

    node = cfstore_misc_test_03_kv_data;
    while(node->key_name != NULL)
    {
        CFSTORE_DBGLOG("%s:About to open KV (key_name=\"%s\", value=\"%s\")\r\n", __func__, node->key_name, node->value);
        ret = drv->Open(node->key_name, flags, hkey);
        CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Failed to open node (key_name=\"%s\", value=\"%s\")(ret=%" PRId32 ")\r\n", __func__, node->key_name, node->value, ret);
        TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);

        key_name_len = CFSTORE_KEY_NAME_MAX_LENGTH+1;
        memset(key_name_buf, 0, key_name_len);
        drv->GetKeyName(hkey, key_name_buf, &key_name_len);
        CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: failed to GetKeyName() (key_name (expected)=\"%s\", key_name (returned)=\"%s\", value=\"%s\"), len return=%d, len expected=%d\r\n", __func__, node->key_name, key_name_buf, node->value, (int) key_name_len, (int) strlen(node->key_name));
        TEST_ASSERT_MESSAGE(key_name_len == strlen(node->key_name)+1, cfstore_misc_utest_msg_g);

        CFSTORE_LOG("GetKeyName() successfully reported key_name (key_name=\"%s\")\r\n", key_name_buf);
        ret = drv->Close(hkey);
        CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Failed to close key (key_name=\"%s\", value=\"%s\")\r\n", __func__, node->key_name, node->value);
        TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);
        node++;
    }

    cfstore_test_delete_all();
    ret = drv->Uninitialize();
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: Uninitialize() call failed.\n", __func__);
    TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);
    return CaseNext;
}


/** @brief  basic GetValueLen() test
 *
 * @return  status code
 *          ARM_DRIVER_OK, the test passed successfully
 *          ret < ARM_DRIVER_OK, the test failed with the return code
 *          supplying more details
 */
static control_t cfstore_misc_test_04_end(const size_t call_count)
{
    int32_t ret = ARM_DRIVER_ERROR;
    ARM_CFSTORE_SIZE len = 0;
    ARM_CFSTORE_DRIVER* drv = &cfstore_driver;
    cfstore_kv_data_t* node = NULL;
    ARM_CFSTORE_HANDLE_INIT(hkey);
    ARM_CFSTORE_FMODE flags;

    CFSTORE_FENTRYLOG("%s:entered\n", __func__);
    (void) call_count;
    /* dont set any flags to get default settings */
    memset(&flags, 0, sizeof(flags));

    ret = cfstore_test_create_table(cfstore_misc_test_03_kv_data);
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: unable to create keys from table.\r\n", __func__);
    TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);

    node = cfstore_misc_test_03_kv_data;
    while(node->key_name != NULL)
    {
        CFSTORE_DBGLOG("%s:About to open KV (key_name=\"%s\", value=\"%s\")\r\n", __func__, node->key_name, node->value);
        ret = drv->Open(node->key_name, flags, hkey);
        CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Failed to open node (key_name=\"%s\", value=\"%s\")(ret=%d)\r\n", __func__, node->key_name, node->value, (int) ret);
        TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);

        drv->GetValueLen(hkey, &len);
        CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Errro: GetValueLen() retrieve incorrect length of value blob(expected=%d, returned=%d)\r\n", __func__, (int) strlen(node->value), (int) len);
        TEST_ASSERT_MESSAGE(len == strlen(node->value), cfstore_misc_utest_msg_g);
        CFSTORE_LOG("GetValueLen() successfully reported correct value blob length%s", "\r\n");

        ret = drv->Close(hkey);
        CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Failed to close key (key_name=\"%s\", value=\"%s\")\r\n", __func__, node->key_name, node->value);
        TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);
        node++;
    }
    cfstore_test_delete_all();
    ret = drv->Uninitialize();
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: Uninitialize() call failed.\n", __func__);
    TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);
    return CaseNext;
}


/** @brief  basic GetStatus() test
 *
 * @return  status code
 *          ARM_DRIVER_OK, the test passed successfully
 *          ret < ARM_DRIVER_OK, the test failed with the return code
 *          supplying more details
 */
static control_t cfstore_misc_test_05_start(const size_t call_count)
{
    int32_t ret = ARM_DRIVER_ERROR;
    ARM_CFSTORE_STATUS status;
    ARM_CFSTORE_DRIVER* drv = &cfstore_driver;

    CFSTORE_FENTRYLOG("%s:entered\n", __func__);
    (void) call_count;

    status = drv->GetStatus();
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: GetStatus() before initialisation should have reported error, but reported no error.\r\n", __func__);
    TEST_ASSERT_MESSAGE(status.error == true, cfstore_misc_utest_msg_g);

    ret = drv->Initialize(cfstore_utest_default_callback, NULL);
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: failed to initialize CFSTORE (ret=%" PRId32 ")\n", __func__, ret);
    TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);
    return CaseTimeout(CFSTORE_UTEST_DEFAULT_TIMEOUT_MS);
}

static control_t cfstore_misc_test_05_end(const size_t call_count)
{
    int32_t ret = ARM_DRIVER_ERROR;
    ARM_CFSTORE_DRIVER* drv = &cfstore_driver;
    ARM_CFSTORE_STATUS status;

    CFSTORE_FENTRYLOG("%s:entered\n", __func__);
    (void) call_count;

    status = drv->GetStatus();
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: GetStatus() but reported error.\r\n", __func__);
    TEST_ASSERT_MESSAGE(status.error == false, cfstore_misc_utest_msg_g);

    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: GetStatus() reported operation in progress.\r\n", __func__);
    TEST_ASSERT_MESSAGE(status.in_progress == false, cfstore_misc_utest_msg_g);

    ret = drv->Uninitialize();
    CFSTORE_TEST_UTEST_MESSAGE(cfstore_misc_utest_msg_g, CFSTORE_UTEST_MSG_BUF_SIZE, "%s:Error: Uninitialize() call failed.\n", __func__);
    TEST_ASSERT_MESSAGE(ret >= ARM_DRIVER_OK, cfstore_misc_utest_msg_g);
    return CaseNext;
}


utest::v1::status_t greentea_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(200, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
           /*          1         2         3         4         5         6        7  */
           /* 1234567890123456789012345678901234567890123456789012345678901234567890 */
        Case("MISC_test_00", cfstore_misc_test_00),
        Case("MISC_test_00_start", cfstore_misc_test_00_start),
        Case("MISC_test_00_end", cfstore_misc_test_00_end),
        Case("MISC_test_01", cfstore_misc_test_01),
        Case("MISC_test_02", cfstore_misc_test_02),
        Case("MISC_test_03_start", cfstore_utest_default_start),
        Case("MISC_test_03_end", cfstore_misc_test_03_end),
        Case("MISC_test_04_start", cfstore_utest_default_start),
        Case("MISC_test_04_end", cfstore_misc_test_04_end),
        Case("MISC_test_05_start", cfstore_misc_test_05_start),
        Case("MISC_test_05_end", cfstore_misc_test_05_end),
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


#endif // __MBED__ && ! defined TOOLCHAIN_GCC_ARM
