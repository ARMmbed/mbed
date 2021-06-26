/* mbed Microcontroller Library
 * Copyright (c) 2017-2018 Nuvoton
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

#include "watchdog_api.h"

#if DEVICE_WATCHDOG

#include "cmsis.h"

/* Watchdog clock per second */
#if MBED_CONF_TARGET_LXT_PRESENT
#define NU_WDTCLK_PER_SEC           (__LXT)
#define NU_WDTCLK_PER_SEC_MAX       (__LXT)
#define NU_WDTCLK_PER_SEC_MIN       (__LXT)
#else
#define NU_WDTCLK_PER_SEC           (__LIRC)
#define NU_WDTCLK_PER_SEC_MAX       ((uint32_t) ((__LIRC) * 1.5f))
#define NU_WDTCLK_PER_SEC_MIN       ((uint32_t) ((__LIRC) * 0.5f))
#endif

/* Convert watchdog clock to nearest ms */
#define NU_WDTCLK2MS(WDTCLK)        (((WDTCLK) * 1000 + ((NU_WDTCLK_PER_SEC) / 2)) / (NU_WDTCLK_PER_SEC))

/* Convert ms to nearest watchdog clock */
#define NU_MS2WDTCLK(MS)            (((MS) * (NU_WDTCLK_PER_SEC) + 500) / 1000)

/* List of hardware-supported watchdog timeout in clocks */
#define NU_WDT_16CLK                    16
#define NU_WDT_64CLK                    64
#define NU_WDT_256CLK                   256
#define NU_WDT_1024CLK                  1024
#define NU_WDT_4096CLK                  4096
#define NU_WDT_16384CLK                 16384
#define NU_WDT_65536CLK                 65536
#define NU_WDT_262144CLK                262144

/* Watchdog reset delay
 *
 * 1. Cannot be too small. This is to avoid premature WDT reset in pieces of timeout cascading.
 * 2. Cannot be too large. This is to pass Greentea reset_reason/watchdog_reset tests, which have e.g. 50~100 reset delay tolerance.
 */
#define NU_WDT_RESET_DELAY_RSTDSEL      WDT_RESET_DELAY_130CLK

/* Support watchdog timeout values beyond H/W
 *
 * Watchdog Timer H/W just supports timeout values of 2^4, 2^6, ..., 2^18 clocks.
 * To extend the support range to 1 and UINT32_MAX, we cascade multiple small timeouts to
 * reach one large timeout specified in hal_watchdog_init.
 */

/* Track if WDT H/W has been initialized */
static bool wdt_hw_inited = 0;
/* Hold initially-configured timeout in hal_watchdog_init */
static uint32_t wdt_timeout_reload_ms = 0;
/* Track remaining timeout for cascading */
static uint32_t wdt_timeout_rmn_clk = 0;

static void watchdog_setup_cascade_timeout(void);
static void WDT_IRQHandler(void);

watchdog_status_t hal_watchdog_init(const watchdog_config_t *config)
{
    /* Check validity of arguments */
    if (! config || ! config->timeout_ms) {
        return WATCHDOG_STATUS_INVALID_ARGUMENT;
    }

    wdt_timeout_reload_ms = config->timeout_ms;
    wdt_timeout_rmn_clk = NU_MS2WDTCLK(wdt_timeout_reload_ms);
    
    if (! wdt_hw_inited) {
        wdt_hw_inited = 1;

        SYS_UnlockReg();

        /* Enable IP module clock */
        CLK_EnableModuleClock(WDT_MODULE);

        /* Select IP clock source */
#if MBED_CONF_TARGET_LXT_PRESENT
        CLK_SetModuleClock(WDT_MODULE, CLK_CLKSEL1_WDTSEL_LXT, 0);
#else
        CLK_SetModuleClock(WDT_MODULE, CLK_CLKSEL1_WDTSEL_LIRC, 0);
#endif

        SYS_LockReg();

        /* Set up IP interrupt */
        NVIC_SetVector(WDT_IRQn, (uint32_t) WDT_IRQHandler);
        NVIC_EnableIRQ(WDT_IRQn);
    }

    watchdog_setup_cascade_timeout();

    return WATCHDOG_STATUS_OK;
}

void hal_watchdog_kick(void)
{
    /* If a watchdog is not running, this function does nothing */
    if (!(WDT->CTL & WDT_CTL_WDTEN_Msk)) {
        return;
    }

    wdt_timeout_rmn_clk = NU_MS2WDTCLK(wdt_timeout_reload_ms);
    watchdog_setup_cascade_timeout();
}

watchdog_status_t hal_watchdog_stop(void)
{
    SYS_UnlockReg();

    /* Clear all flags & Disable interrupt & Disable WDT */
    WDT->CTL = (WDT->CTL & ~(WDT_CTL_WDTEN_Msk | WDT_CTL_INTEN_Msk)) | (WDT_CTL_WKF_Msk | WDT_CTL_IF_Msk | WDT_CTL_RSTF_Msk);        

    SYS_LockReg();

    return WATCHDOG_STATUS_OK;
}

uint32_t hal_watchdog_get_reload_value(void)
{
    return wdt_timeout_reload_ms;
}

watchdog_features_t hal_watchdog_get_platform_features(void)
{
    watchdog_features_t wdt_feat;

    /* We can support timeout values between 1 and UINT32_MAX by cascading. */
    wdt_feat.max_timeout = UINT32_MAX;
    /* Support re-configuring watchdog timer */
    wdt_feat.update_config = 1;
    /* Support stopping watchdog timer */
    wdt_feat.disable_watchdog = 1;
    /* Typical frequency of not calibrated watchdog clock in Hz */
    wdt_feat.clock_typical_frequency = NU_WDTCLK_PER_SEC;
    /* Maximum frequency of not calibrated watchdog clock in Hz */
    wdt_feat.clock_max_frequency = NU_WDTCLK_PER_SEC_MAX;

    return wdt_feat;
}

static void watchdog_setup_cascade_timeout(void)
{
    uint32_t wdt_timeout_clk_toutsel;

    if (wdt_timeout_rmn_clk >= NU_WDT_262144CLK) {
        wdt_timeout_rmn_clk -= NU_WDT_262144CLK;
        wdt_timeout_clk_toutsel = WDT_TIMEOUT_2POW18;
    } else if (wdt_timeout_rmn_clk >= NU_WDT_65536CLK) {
        wdt_timeout_rmn_clk -= NU_WDT_65536CLK;
        wdt_timeout_clk_toutsel = WDT_TIMEOUT_2POW16;
    } else if (wdt_timeout_rmn_clk >= NU_WDT_16384CLK) {
        wdt_timeout_rmn_clk -= NU_WDT_16384CLK;
        wdt_timeout_clk_toutsel = WDT_TIMEOUT_2POW14;
    } else if (wdt_timeout_rmn_clk >= NU_WDT_4096CLK) {
        wdt_timeout_rmn_clk -= NU_WDT_4096CLK;
        wdt_timeout_clk_toutsel = WDT_TIMEOUT_2POW12;
    } else if (wdt_timeout_rmn_clk >= NU_WDT_1024CLK) {
        wdt_timeout_rmn_clk -= NU_WDT_1024CLK;
        wdt_timeout_clk_toutsel = WDT_TIMEOUT_2POW10;
    } else if (wdt_timeout_rmn_clk >= NU_WDT_256CLK) {
        wdt_timeout_rmn_clk -= NU_WDT_256CLK;
        wdt_timeout_clk_toutsel = WDT_TIMEOUT_2POW8;
    } else if (wdt_timeout_rmn_clk >= NU_WDT_64CLK) {
        wdt_timeout_rmn_clk -= NU_WDT_64CLK;
        wdt_timeout_clk_toutsel = WDT_TIMEOUT_2POW6;
    } else if (wdt_timeout_rmn_clk >= NU_WDT_16CLK) {
        wdt_timeout_rmn_clk -= NU_WDT_16CLK;
        wdt_timeout_clk_toutsel = WDT_TIMEOUT_2POW4;
    } else if (wdt_timeout_rmn_clk) {
        wdt_timeout_rmn_clk = 0;
        wdt_timeout_clk_toutsel = WDT_TIMEOUT_2POW4;
    } else {
        /* WDT has timed-out and will restart system soon. We just disable interrupt to escape 
         * getting stuck in WDT ISR. */
        SYS_UnlockReg();

        /* Clear all flags & Disable interrupt */
        WDT->CTL = (WDT->CTL & ~WDT_CTL_INTEN_Msk) | (WDT_CTL_WKF_Msk | WDT_CTL_IF_Msk | WDT_CTL_RSTF_Msk);        

        SYS_LockReg();
        return;
    }

    SYS_UnlockReg();

    /* Configure reset delay on timeout */
    WDT->ALTCTL = NU_WDT_RESET_DELAY_RSTDSEL;               

    /* Configure another piece of cascaded WDT timeout */
    WDT->CTL = wdt_timeout_clk_toutsel |                    // Timeout interval
        WDT_CTL_WDTEN_Msk |                                 // Enable watchdog timer
        WDT_CTL_INTEN_Msk |                                 // Enable interrupt
        WDT_CTL_WKF_Msk |                                   // Clear wake-up flag
        WDT_CTL_WKEN_Msk |                                  // Enable wake-up on timeout
        WDT_CTL_IF_Msk |                                    // Clear interrupt flag
        WDT_CTL_RSTF_Msk |                                  // Clear reset flag
        WDT_CTL_RSTEN_Msk |                                 // Enable reset always to address cascaded timeout failure in interrupt disabled scenario e.g. Hard Fault
        WDT_CTL_RSTCNT_Msk;                                 // Reset watchdog timer

    SYS_LockReg();
}

void WDT_IRQHandler(void)
{
    /* Check WDT interrupt flag */
    if (WDT_GET_TIMEOUT_INT_FLAG()) {
        /* Continue another piece of cascaded WDT timeout */
        watchdog_setup_cascade_timeout();
    } else {
        /* Clear all flags */
        WDT->CTL |= (WDT_CTL_WKF_Msk | WDT_CTL_IF_Msk | WDT_CTL_RSTF_Msk);
    }
}

#endif
