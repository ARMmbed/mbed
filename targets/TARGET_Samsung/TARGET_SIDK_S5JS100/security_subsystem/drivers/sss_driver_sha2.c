/****************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

/*************** Include Files ********************************************/
#include "sss_driver_error.h"
#include "sss_driver_sha2.h"

/*************** Assertions ***********************************************/

/*************** Definitions / Macros *************************************/

/*************** New Data Types (Basic Data Types)  ***********************/

/*************** New Data Types *******************************************/

/*************** Constants ************************************************/

/*************** Variable declarations ************************************/

/*************** Functions  ***********************************************/


int sss_SHA2_256(
    const stOCTET_STRING *pstMessage,
    stOCTET_STRING *pstDigest)
{

    int ret = FAIL;

    stOCTET_STRING stHASH_Input;
    unsigned int object_id;
    unsigned int block_byte_len;

    //! step 0 : clear Mailbox
    ret = mb_system_clear(CLEAR_TYPE_MAILBOX);

    if (ret != SSSR_SUCCESS) {
        return ret;
    }

    //! assign hash_byte_len to compare returned result from sss_fw after hash operation
    object_id = OID_SHA2_256;
    block_byte_len = 64;

    //! step 1 : set message length parameter to SSS
    ret = mb_hash_init(pstMessage, object_id);

    if (ret != SSSR_SUCCESS) {
        return ret;
    }

    //! step 2 : set message block to SSS
    stHASH_Input.pu08Data = pstMessage->pu08Data;
    stHASH_Input.u32DataByteLen = pstMessage->u32DataByteLen;

    ret = mb_hash_update(&stHASH_Input, block_byte_len);

    if (ret != SSSR_SUCCESS) {
        return ret;
    }

    //! step 3 : get hash result from SSS
    ret = mb_hash_final(&stHASH_Input, pstDigest);

    return ret;
}



int sss_SHA2_384(
    const stOCTET_STRING *pstMessage,
    stOCTET_STRING *pstDigest)
{

    int ret = FAIL;

    stOCTET_STRING stHASH_Input;
    unsigned int object_id;
    unsigned int block_byte_len;

    //! step 0 : clear Mailbox
    ret = mb_system_clear(CLEAR_TYPE_MAILBOX);

    if (ret != SSSR_SUCCESS) {
        return ret;
    }

    //! assign hash_byte_len to compare returned result from sss_fw after hash operation
    object_id = OID_SHA2_384;
    block_byte_len = 128;

    //! step 1 : set message length parameter to SSS
    ret = mb_hash_init(pstMessage, object_id);

    if (ret != SSSR_SUCCESS) {
        return ret;
    }

    //! step 2 : set message block to SSS
    stHASH_Input.pu08Data = pstMessage->pu08Data;
    stHASH_Input.u32DataByteLen = pstMessage->u32DataByteLen;

    ret = mb_hash_update(&stHASH_Input, block_byte_len);

    if (ret != SSSR_SUCCESS) {
        return ret;
    }

    //! step 3 : get hash result from SSS
    ret = mb_hash_final(&stHASH_Input, pstDigest);

    return ret;
}



int sss_SHA2_512(
    const stOCTET_STRING *pstMessage,
    stOCTET_STRING *pstDigest)
{

    int ret = FAIL;

    stOCTET_STRING stHASH_Input;
    unsigned int object_id;
    unsigned int block_byte_len;

    //! step 0 : clear Mailbox
    ret = mb_system_clear(CLEAR_TYPE_MAILBOX);

    if (ret != SSSR_SUCCESS) {
        return ret;
    }

    //! assign hash_byte_len to compare returned result from sss_fw after hash operation
    object_id = OID_SHA2_512;
    block_byte_len = 128;

    //! step 1 : set message length parameter to SSS
    ret = mb_hash_init(pstMessage, object_id);

    if (ret != SSSR_SUCCESS) {
        return ret;
    }

    //! step 2 : set message block to SSS
    stHASH_Input.pu08Data = pstMessage->pu08Data;
    stHASH_Input.u32DataByteLen = pstMessage->u32DataByteLen;

    ret = mb_hash_update(&stHASH_Input, block_byte_len);

    if (ret != SSSR_SUCCESS) {
        return ret;
    }

    //! step 3 : get hash result from SSS
    ret = mb_hash_final(&stHASH_Input, pstDigest);

    return ret;
}


