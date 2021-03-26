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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "hal/trng_api.h"
#include "sss_driver_rng.h"
#include "mbed_error.h"

void trng_init(trng_t *obj)
{
    (void)obj;
}

void trng_free(trng_t *obj)
{
    (void)obj;
}

int trng_get_bytes(trng_t *obj, uint8_t *output, size_t length, size_t *output_length)
{
    (void)obj;

    stOCTET_STRING stRANDOM;
    unsigned int ret = 0;

    stRANDOM.pu08Data = (u08 *)output;
    //generate an approximation of 1
    ret = sss_generate_rawrandom(&stRANDOM, length);

    *output_length = (size_t)stRANDOM.u32DataByteLen;

    if (ret) {
        mbed_error_printf("Fail to get RNG value from SSS(0x%08x)\r\n", ret);
    }

    return ret;
}
