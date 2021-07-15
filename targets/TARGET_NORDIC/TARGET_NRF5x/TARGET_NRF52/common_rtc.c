/*
 * Copyright (c) 2013 Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list
 *      of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form, except as embedded into a Nordic Semiconductor ASA
 *      integrated circuit in a product or a software update for such product, must reproduce
 *      the above copyright notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of Nordic Semiconductor ASA nor the names of its contributors may be
 *      used to endorse or promote products derived from this software without specific prior
 *      written permission.
 *
 *   4. This software, with or without modification, must only be used with a
 *      Nordic Semiconductor ASA integrated circuit.
 *
 *   5. Any software provided in binary or object form under this license must not be reverse
 *      engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "us_ticker_api.h"
#include "common_rtc.h"
#include "app_util.h"
#include "lp_ticker_api.h"
#include "mbed_critical.h"

#if defined(NRF52_PAN_20)
/* Macro for testing if the SoftDevice is active, regardless of whether the
 * application is build with the SoftDevice or not.
 */
#if defined(SOFTDEVICE_PRESENT)
#include "nrf_sdh.h"
#define NRF_HAL_SD_IS_ENABLED() nrf_sdh_is_enabled()
#else
#define NRF_HAL_SD_IS_ENABLED() 0
#endif
#endif


//------------------------------------------------------------------------------
// Common stuff used also by lp_ticker and rtc_api (see "common_rtc.h").
//
#include "app_util_platform.h"

bool              m_common_rtc_enabled = false;
uint32_t volatile m_common_rtc_overflows = 0;
bool     volatile lp_ticker_interrupt_fire = false;

__STATIC_INLINE void rtc_ovf_event_check(void)
{
    if (nrf_rtc_event_pending(COMMON_RTC_INSTANCE, NRF_RTC_EVENT_OVERFLOW)) {
        nrf_rtc_event_clear(COMMON_RTC_INSTANCE, NRF_RTC_EVENT_OVERFLOW);
        /* Don't disable this event. It shall occur periodically.
         * It is needed for RTC. */

        ++m_common_rtc_overflows;
    }
}

#if defined(TARGET_MCU_NRF51822)
void common_rtc_irq_handler(void)
#else
void COMMON_RTC_IRQ_HANDLER(void)
#endif
{
    rtc_ovf_event_check();

#if DEVICE_LPTICKER
    if (nrf_rtc_event_pending(COMMON_RTC_INSTANCE, LP_TICKER_EVENT) ||
        lp_ticker_interrupt_fire) {

        if (lp_ticker_interrupt_fire) {
            lp_ticker_interrupt_fire = false;
        }

        lp_ticker_irq_handler();
    }
#endif
}

/* Function for fix errata 20: RTC Register values are invalid. */
__STATIC_INLINE void errata_20(void)
{
#if defined(NRF52_PAN_20)
    if (!NRF_HAL_SD_IS_ENABLED())
    {
        NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
        NRF_CLOCK->TASKS_LFCLKSTART    = 1;

        while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
        {
        }
    }
    COMMON_RTC_INSTANCE->TASKS_STOP = 0;
#endif
}

void COMMON_RTC_IRQ_HANDLER(void);

void common_rtc_init(void)
{
    if (m_common_rtc_enabled) {
#if DEVICE_LPTICKER
        nrf_rtc_event_clear(COMMON_RTC_INSTANCE, LP_TICKER_EVENT);
        nrf_rtc_int_disable(COMMON_RTC_INSTANCE, LP_TICKER_INT_MASK);
#endif
        return;
    }

    errata_20();

    nrf_rtc_task_trigger(COMMON_RTC_INSTANCE, NRF_RTC_TASK_STOP);

    NVIC_SetVector(COMMON_RTC_IRQn, (uint32_t)COMMON_RTC_IRQ_HANDLER);

    /* RTC is driven by the low frequency (32.768 kHz) clock, a proper request
     * must be made to have it running.
     * Currently this clock is started in 'SystemInit' (see "system_nrf51.c"
     * or "system_nrf52.c", respectively).
     */

    nrf_rtc_prescaler_set(COMMON_RTC_INSTANCE, 0);

    /* Clear all RTC events. */
#if defined(TARGET_MCU_NRF51822)
    nrf_rtc_event_clear(COMMON_RTC_INSTANCE, OS_TICK_EVENT);
#endif
#if DEVICE_LPTICKER
    nrf_rtc_event_clear(COMMON_RTC_INSTANCE, LP_TICKER_EVENT);
#endif
    nrf_rtc_event_clear(COMMON_RTC_INSTANCE, NRF_RTC_EVENT_OVERFLOW);

    /* Disable all RTC events (According to NRF_51 Reference Manual
     * RTC events can not be used to control RTC interrupts).
     * IRQ signal to NVIC is provided if interrupt is enabled.
     */

    nrf_rtc_event_disable(COMMON_RTC_INSTANCE, NRF_RTC_INT_OVERFLOW_MASK
#if defined(TARGET_MCU_NRF51822)
        | OS_TICK_INT_MASK
#endif
#if DEVICE_LPTICKER
        | LP_TICKER_INT_MASK
#endif
     );

    /* This interrupt is enabled permanently, since overflow indications are needed
     * continuously.
     */
    nrf_rtc_int_enable(COMMON_RTC_INSTANCE, NRF_RTC_INT_OVERFLOW_MASK);

    /* Disable LP ticker interrupt for now. */
#if DEVICE_LPTICKER
    nrf_rtc_int_disable(COMMON_RTC_INSTANCE, LP_TICKER_INT_MASK);
#endif

    NRFX_IRQ_PRIORITY_SET(nrfx_get_irq_number(COMMON_RTC_INSTANCE), APP_IRQ_PRIORITY_HIGH);
    NRFX_IRQ_ENABLE(nrfx_get_irq_number(COMMON_RTC_INSTANCE));

    nrf_rtc_task_trigger(COMMON_RTC_INSTANCE, NRF_RTC_TASK_START);

    m_common_rtc_enabled = true;
}

void common_rtc_free()
{
    nrf_rtc_task_trigger(COMMON_RTC_INSTANCE, NRF_RTC_TASK_STOP);
    nrf_rtc_int_disable(COMMON_RTC_INSTANCE, LP_TICKER_INT_MASK);
    NVIC_DisableIRQ(nrfx_get_irq_number(COMMON_RTC_INSTANCE));

    m_common_rtc_enabled = false;
}

void common_rtc_set_interrupt(uint32_t ticks_count, uint32_t cc_channel,
                              uint32_t int_mask)
{
    /* Set ticks value when interrupt should be fired.
     * Interrupt scheduling is performed in upper layers. */

    core_util_critical_section_enter();

    /* Wrap ticks_count before comparisons. */
    ticks_count = RTC_WRAP(ticks_count);

    /* COMPARE occurs when a CC register is N and the COUNTER value transitions from N-1 to N.
     * If the COUNTER is N, writing N+2 to a CC register is guaranteed to trigger a
     * COMPARE event at N+2.
     */
    const uint32_t now = nrf_rtc_counter_get(COMMON_RTC_INSTANCE);

    if (now == ticks_count ||
        RTC_WRAP(now + 1) == ticks_count) {
        ticks_count = RTC_WRAP(ticks_count + 2);
    }

    nrf_rtc_cc_set(COMMON_RTC_INSTANCE, cc_channel, ticks_count);

    if (!nrf_rtc_int_is_enabled(COMMON_RTC_INSTANCE, int_mask)) {
        nrf_rtc_event_clear(COMMON_RTC_INSTANCE, LP_TICKER_EVENT);
        nrf_rtc_int_enable(COMMON_RTC_INSTANCE, int_mask);
    }

    core_util_critical_section_exit();
}

/* Since there is no SysTick on NRF51, the RTC1 channel 0 is used as an
 * alternative source of RTOS ticks.
 */
#if defined(TARGET_MCU_NRF51822)

#include "mbed_toolchain.h"

void COMMON_RTC_IRQ_HANDLER(void)
{
    if(!nrf_rtc_event_pending(COMMON_RTC_INSTANCE, OS_TICK_EVENT)) {
        common_rtc_irq_handler();
    }
}

IRQn_Type mbed_get_m0_tick_irqn()
{
    return SWI3_IRQn;
}


#endif // defined(TARGET_MCU_NRF51822)
