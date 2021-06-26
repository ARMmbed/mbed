/* 
 * Copyright (c) 2019-2020 Nuvoton Technology Corporation
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

#if DEVICE_TRNG

#include "cmsis.h"
#include <limits.h>
#include "crypto-misc.h"
#include "hal/trng_api.h"
#include "platform/mbed_toolchain.h"
#include "platform/mbed_critical.h"
#include "platform/mbed_error.h"
#include "nu_modutil.h"

/* Module init definition: modname, clkidx, clksrc, clkdiv, rstidx, irqnum, misc */
static const struct nu_modinit_s trng_modinit = {TRNG_0, TRNG_MODULE, 0, 0, TRNG_RST, TRNG_IRQn, NULL};

/* TRNG init counter. TRNG is kept active as it is non-zero. */
static uint16_t trng_init_counter = 0U;

void trng_init(MBED_UNUSED trng_t *obj)
{
    core_util_critical_section_enter();
    if (trng_init_counter == USHRT_MAX) {
        core_util_critical_section_exit();
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_HAL, MBED_ERROR_CODE_OVERFLOW), \
                   "TRNG initialization counter would overflow");
    }
    ++ trng_init_counter;
    if (trng_init_counter == 1) {
        /* Enable IP clock */
        CLK_EnableModuleClock(trng_modinit.clkidx);

        /* Reset IP */
        SYS_ResetModule(trng_modinit.rsetidx);

#if MBED_CONF_TARGET_LXT_PRESENT
        /* 32K clock from (external) LXT */
#else
        /* 32K clock from LIRC32 */

        /* Unlock protected registers */
        SYS_UnlockReg();

        /* To access RTC registers, clock must be enabled first. */
        CLK_EnableModuleClock(RTC_MODULE);

        /* Enable 32K clock from LIRC32 */
        RTC->LXTCTL |= (RTC_LXTCTL_C32KS_Msk | RTC_LXTCTL_LIRC32KEN_Msk);
        CLK_WaitClockReady(CLK_STATUS_LIRC32STB_Msk | CLK_STATUS_LXTSTB_Msk);

        /* Lock protected registers */
        SYS_LockReg();
#endif

        TRNG_T *trng_base = (TRNG_T *) NU_MODBASE(trng_modinit.modname);

        trng_base->ACT |= TRNG_ACT_ACT_Msk;
        while (!(trng_base->CTL & TRNG_CTL_READY_Msk));
    }
    core_util_critical_section_exit();
}

void trng_free(MBED_UNUSED trng_t *obj)
{
    core_util_critical_section_enter();
    if (trng_init_counter == 0) {
        core_util_critical_section_exit();
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_HAL, MBED_ERROR_CODE_UNDERFLOW), \
                   "TRNG initialization counter would underflow");
    }
    -- trng_init_counter;
    if (trng_init_counter == 0) {
        TRNG_T *trng_base = (TRNG_T *) NU_MODBASE(trng_modinit.modname);

        trng_base->ACT &= ~TRNG_ACT_ACT_Msk;

        /* Disable IP clock */
        CLK_DisableModuleClock(trng_modinit.clkidx);
    }
    core_util_critical_section_exit();
}

int trng_get_bytes(MBED_UNUSED trng_t *obj, uint8_t *output, size_t length, size_t *output_length)
{
    /* Check augument validity */
    if (!output && length) {
        return -1;
    }

    uint8_t *output_ind = output;
    uint8_t *output_end = output + length;

    TRNG_T *trng_base = (TRNG_T *) NU_MODBASE(trng_modinit.modname);

    for (; output_ind != output_end; output_ind ++) {
        trng_base->CTL |= TRNG_CTL_TRNGEN_Msk;
        while (!(trng_base->CTL & TRNG_CTL_DVIF_Msk));
        *output_ind = trng_base->DATA & 0xff;
    }

    if (output_length) {
        *output_length = length;
    }

    return 0;
}

#endif
