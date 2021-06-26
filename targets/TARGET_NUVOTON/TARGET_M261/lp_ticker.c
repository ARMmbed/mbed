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

#include "lp_ticker_api.h"

#if DEVICE_LPTICKER

#include "sleep_api.h"
#include "mbed_assert.h"
#include "nu_modutil.h"
#include "nu_timer.h"
#include "nu_miscutil.h"

/* Micro seconds per second */
#define NU_US_PER_SEC               1000000
/* Timer clock per lp_ticker tick */
#define NU_TMRCLK_PER_TICK          1
/* Timer clock per second */
#if MBED_CONF_TARGET_LXT_PRESENT
#define NU_TMRCLK_PER_SEC           (__LXT)
#else
#define NU_TMRCLK_PER_SEC           (__LIRC)
#endif
/* Timer max counter bit size */
#define NU_TMR_MAXCNT_BITSIZE       24
/* Timer max counter */
#define NU_TMR_MAXCNT               ((1 << NU_TMR_MAXCNT_BITSIZE) - 1)


static void tmr1_vec(void);

/* NOTE: To wake the system from power down mode, timer clock source must be ether LXT or LIRC. */
#if MBED_CONF_TARGET_LXT_PRESENT
static const struct nu_modinit_s timer1_modinit = {TIMER_1, TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_LXT, 0, TMR1_RST, TMR1_IRQn, (void *) tmr1_vec};
#else
static const struct nu_modinit_s timer1_modinit = {TIMER_1, TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_LIRC, 0, TMR1_RST, TMR1_IRQn, (void *) tmr1_vec};
#endif

#define TIMER_MODINIT       timer1_modinit

/* Timer interrupt enable/disable
 * 
 * Because Timer interrupt enable/disable (TIMER_EnableInt/TIMER_DisableInt) needs wait for lp_ticker,
 * we call NVIC_DisableIRQ/NVIC_EnableIRQ instead.
 */

/* Track ticker status */
static volatile uint16_t ticker_inited = 0;

#define TMR_CMP_MIN         2
#define TMR_CMP_MAX         0xFFFFFFu

/* Synchronization issue with LXT/LIRC-clocked Timer
 *
 * PCLK                 : typical HCLK/2
 * ECLK (engine clock)  : LXT/LIRC for Timer used to implement lp_ticker
 *
 * When system clock is higher than Timer clock (LXT/LIRC), we need to add delay for ECLK
 * domain to take effect:
 * 1. Write                 : typical 1PCLK + 2ECLK
 *    Read-check doesn't work because it just checks PCLK domain and doesn't check into
 *    ECLK domain.
 * 2. Clear interrupt flag  : typical 2PCLK
 *    It is very rare that we would meet dummy interrupt and get stuck in ISR until
 *    'clear interrupt flag' takes effect. The issue is ignorable because the pending
 *    time is very short (at most 1 dummy interrupt). We won't take special handling for it.
 */

void lp_ticker_init(void)
{
    if (ticker_inited) {
        /* By HAL spec, ticker_init allows the ticker to keep counting and disables the
         * ticker interrupt. */
        lp_ticker_disable_interrupt();
        return;
    }
    ticker_inited = 1;

    /* Select IP clock source
     *
     * NOTE: We must call secure version (from non-secure domain) because SYS/CLK regions are secure.
     */
    CLK_SetModuleClock(TIMER_MODINIT.clkidx, TIMER_MODINIT.clksrc, TIMER_MODINIT.clkdiv);

    /* Enable IP clock
     *
     * NOTE: We must call secure version (from non-secure domain) because SYS/CLK regions are secure.
     */
    CLK_EnableModuleClock(TIMER_MODINIT.clkidx);

    /* Reset module
     *
     * NOTE: We must call secure version (from non-secure domain) because SYS/CLK regions are secure.
     */
    SYS_ResetModule(TIMER_MODINIT.rsetidx);

    TIMER_T *timer_base = (TIMER_T *) NU_MODBASE(TIMER_MODINIT.modname);

    // Configure clock
    uint32_t clk_timer = TIMER_GetModuleClock(timer_base);
    uint32_t prescale_timer = clk_timer / NU_TMRCLK_PER_SEC - 1;
    MBED_ASSERT((prescale_timer != (uint32_t) -1) && prescale_timer <= 127);
    MBED_ASSERT((clk_timer % NU_TMRCLK_PER_SEC) == 0);
    uint32_t cmp_timer = TMR_CMP_MAX;
    MBED_ASSERT(cmp_timer >= TMR_CMP_MIN && cmp_timer <= TMR_CMP_MAX);
    // Continuous mode
    // NOTE: TIMER_CTL_CNTDATEN_Msk exists in NUC472, but not in M451/M480/M2351. In M451/M480/M2351, TIMER_CNT is updated continuously by default.
    timer_base->CTL = TIMER_CONTINUOUS_MODE | prescale_timer/* | TIMER_CTL_CNTDATEN_Msk*/;
    nu_busy_wait_us((NU_US_PER_SEC / NU_TMRCLK_PER_SEC) * 3);

    timer_base->CMP = cmp_timer;
    nu_busy_wait_us((NU_US_PER_SEC / NU_TMRCLK_PER_SEC) * 3);

    // Set vector
    NVIC_SetVector(TIMER_MODINIT.irq_n, (uint32_t) TIMER_MODINIT.var);

    NVIC_DisableIRQ(TIMER_MODINIT.irq_n);

    TIMER_EnableInt(timer_base);
    nu_busy_wait_us((NU_US_PER_SEC / NU_TMRCLK_PER_SEC) * 3);

    TIMER_EnableWakeup(timer_base);
    nu_busy_wait_us((NU_US_PER_SEC / NU_TMRCLK_PER_SEC) * 3);

    TIMER_Start(timer_base);
    nu_busy_wait_us((NU_US_PER_SEC / NU_TMRCLK_PER_SEC) * 3);

    /* Wait for timer to start counting and raise active flag */
    while(! (timer_base->CTL & TIMER_CTL_ACTSTS_Msk));
}

void lp_ticker_free(void)
{
    /* Disable interrupt */
    NVIC_DisableIRQ(TIMER_MODINIT.irq_n);

    // Disable IP clock
    CLK_DisableModuleClock(TIMER_MODINIT.clkidx);

    ticker_inited = 0;
}

timestamp_t lp_ticker_read()
{
    if (! ticker_inited) {
        lp_ticker_init();
    }

    TIMER_T *timer_base = (TIMER_T *) NU_MODBASE(TIMER_MODINIT.modname);

    return  (TIMER_GetCounter(timer_base) / NU_TMRCLK_PER_TICK);
}

void lp_ticker_set_interrupt(timestamp_t timestamp)
{
    /* Clear any previously pending interrupts */
    lp_ticker_clear_interrupt();
    NVIC_ClearPendingIRQ(TIMER_MODINIT.irq_n);

    /* In continuous mode, counter will be reset to zero with the following sequence: 
     * 1. Stop counting
     * 2. Configure new CMP value
     * 3. Restart counting
     *
     * This behavior is not what we want. To fix it, we could configure new CMP value
     * without stopping counting first.
     */
    TIMER_T *timer_base = (TIMER_T *) NU_MODBASE(TIMER_MODINIT.modname);

    /* NOTE: Because H/W timer requests min compare value, our implementation would have alarm delay of 
     *       (TMR_CMP_MIN - interval_clk) clocks when interval_clk is between [1, TMR_CMP_MIN). */
    uint32_t cmp_timer = timestamp * NU_TMRCLK_PER_TICK;
    cmp_timer = NU_CLAMP(cmp_timer, TMR_CMP_MIN, TMR_CMP_MAX);

    /* NOTE: Rely on LPTICKER_DELAY_TICKS to be non-blocking. */
    timer_base->CMP = cmp_timer;

    /* We can call ticker_irq_handler now. */
    NVIC_EnableIRQ(TIMER_MODINIT.irq_n);
}

void lp_ticker_disable_interrupt(void)
{
    /* We cannot call ticker_irq_handler now. */
    NVIC_DisableIRQ(TIMER_MODINIT.irq_n);
}

void lp_ticker_clear_interrupt(void)
{
    /* To avoid sync issue, we clear TIF/TWKF simultaneously rather than call separate 
     * driver API:
     *
     * TIMER_ClearIntFlag((TIMER_T *) NU_MODBASE(TIMER_MODINIT.modname));
     * TIMER_ClearWakeupFlag((TIMER_T *) NU_MODBASE(TIMER_MODINIT.modname));
     */
    TIMER_T *timer_base = (TIMER_T *) NU_MODBASE(TIMER_MODINIT.modname);
    timer_base->INTSTS = TIMER_INTSTS_TIF_Msk | TIMER_INTSTS_TWKF_Msk;
}

void lp_ticker_fire_interrupt(void)
{
    // NOTE: This event was in the past. Set the interrupt as pending, but don't process it here.
    //       This prevents a recursive loop under heavy load which can lead to a stack overflow.
    NVIC_SetPendingIRQ(TIMER_MODINIT.irq_n);

    /* We can call ticker_irq_handler now. */
    NVIC_EnableIRQ(TIMER_MODINIT.irq_n);
}

const ticker_info_t* lp_ticker_get_info()
{
    static const ticker_info_t info = {
        NU_TMRCLK_PER_SEC / NU_TMRCLK_PER_TICK,
        NU_TMR_MAXCNT_BITSIZE
    };
    return &info;
}


static void tmr1_vec(void)
{
    lp_ticker_clear_interrupt();

    // NOTE: lp_ticker_set_interrupt() may get called in lp_ticker_irq_handler();
    lp_ticker_irq_handler();
}

#endif
