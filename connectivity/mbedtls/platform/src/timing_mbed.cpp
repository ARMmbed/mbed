/*
 *  timing.cpp
 *
 *  Copyright The Mbed TLS Contributors
 *  Copyright (C) 2021, Arm Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_TIMING_ALT)

#include "mbedtls/timing.h"
#include "drivers/Timeout.h"
#include "drivers/LowPowerTimeout.h"
#include "drivers/Timer.h"
#include "drivers/LowPowerTimer.h"
#include <chrono>

extern "C" {
    volatile int mbedtls_timing_alarmed = 0;
};

static void handle_alarm(void)
{
    mbedtls_timing_alarmed = 1;
}

extern "C" void mbedtls_set_alarm(int seconds)
{
#if DEVICE_LPTICKER
    static mbed::LowPowerTimeout t;
#elif DEVICE_USTICKER
    static mbed::Timeout t;
#else
#error "MBEDTLS_TIMING_C requires either LPTICKER or USTICKER"
#endif

    mbedtls_timing_alarmed = 0;

    t.attach(handle_alarm, std::chrono::seconds(seconds));
}

// The static Mbed timer here is initialized once only.
// Mbed TLS can have multiple timers (mbedtls_timing_hr_time) derived
// from the Mbed timer.
#if DEVICE_LPTICKER
static mbed::LowPowerTimer timer;
#elif DEVICE_USTICKER
static mbed::Timer timer;
#else
#error "MBEDTLS_TIMING_C requires either LPTICKER or USTICKER"
#endif
static int timer_init = 0;

#if !defined(HAVE_HARDCLOCK)
#define HAVE_HARDCLOCK

extern "C" unsigned long mbedtls_timing_hardclock(void)
{
    if (timer_init == 0) {
        timer.reset();
        timer.start();
        timer_init = 1;
    }

    return timer.elapsed_time().count();
}
#endif /* !HAVE_HARDCLOCK */

extern "C" unsigned long mbedtls_timing_get_timer(struct mbedtls_timing_hr_time *val, int reset)
{
    if (timer_init == 0) {
        timer.reset();
        timer.start();
        timer_init = 1;
    }

    if (reset) {
        val->start = std::chrono::duration_cast<std::chrono::milliseconds>(timer.elapsed_time()).count();
        return 0;
    } else {
        return std::chrono::duration_cast<std::chrono::milliseconds>(timer.elapsed_time()).count() - val->start;
    }
}

/**
 * Note: The following implementations come from the default timing.c
 * from Mbed TLS. They are disabled in timing.c when MBEDTLS_TIMING_ALT
 * is defined, but the implementation is nonetheless applicable to
 * Mbed OS, so we copy them over.
 */

extern "C" void mbedtls_timing_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms)
{
    mbedtls_timing_delay_context *ctx = (mbedtls_timing_delay_context *) data;

    ctx->int_ms = int_ms;
    ctx->fin_ms = fin_ms;

    if (fin_ms != 0) {
        (void) mbedtls_timing_get_timer(&ctx->timer, 1);
    }
}

extern "C" int mbedtls_timing_get_delay(void *data)
{
    mbedtls_timing_delay_context *ctx = (mbedtls_timing_delay_context *) data;
    unsigned long elapsed_ms;

    if (ctx->fin_ms == 0) {
        return -1;
    }

    elapsed_ms = mbedtls_timing_get_timer(&ctx->timer, 0);

    if (elapsed_ms >= ctx->fin_ms) {
        return 2;
    }

    if (elapsed_ms >= ctx->int_ms) {
        return 1;
    }

    return 0;
}

#endif // MBEDTLS_TIMING_ALT
