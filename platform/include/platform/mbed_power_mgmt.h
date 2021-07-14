/* mbed Microcontroller Library
 * Copyright (c) 2006-2019 ARM Limited
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
#ifndef MBED_POWER_MGMT_H
#define MBED_POWER_MGMT_H


#include "platform/mbed_toolchain.h"
#include "hal/ticker_api.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup platform-public-api */
/** @{*/

/**
 * \defgroup platform_power_mgmt Power management functions
 * @{
 */

/**
 * @defgroup hal_sleep_manager Sleep manager API
 * The sleep manager provides API to automatically select sleep mode.
 *
 * There are two sleep modes:
 * - sleep
 * - deepsleep
 *
 * Use locking/unlocking deepsleep for drivers that depend on features that
 * are not allowed (=disabled) during the deepsleep. For instance, high frequency
 * clocks.
 *
 * # Defined behavior
 * * The lock is a counter
 * * The lock can be locked up to USHRT_MAX - Verified by ::test_lock_eq_ushrt_max
 * * The lock has to be equally unlocked as locked - Verified by ::test_lock_eq_ushrt_max
 * * The function sleep_manager_lock_deep_sleep_internal() locks the automatic deep mode selection - Verified by ::test_lock_unlock
 * * The function sleep_manager_unlock_deep_sleep_internal() unlocks the automatic deep mode selection - Verified by ::test_lock_unlock
 * * The function sleep_manager_sleep_auto() chooses the sleep or deep sleep modes based on the lock - Verified by ::test_sleep_auto
 * * The function sleep_manager_lock_deep_sleep_internal() is IRQ and thread safe - Verified by ::sleep_manager_multithread_test and ::sleep_manager_irq_test
 * * The function sleep_manager_unlock_deep_sleep_internal() is IRQ and thread safe - Verified by ::sleep_manager_multithread_test and ::sleep_manager_irq_test
 * * The function sleep_manager_sleep_auto() is IRQ and thread safe
 *
 * Example:
 * @code
 *
 * void driver::handler()
 * {
 *     if (_sensor.get_event()) {
 *         // any event - we are finished, unlock the deepsleep
 *         sleep_manager_unlock_deep_sleep();
 *         _callback();
 *     }
 * }
 *
 * int driver::measure(event_t event, callback_t& callback)
 * {
 *      _callback = callback;
 *      sleep_manager_lock_deep_sleep();
 *      // start async transaction, we are waiting for an event
 *      return _sensor.start(event, callback);
 * }
 * @endcode
 * @{
 */

/**
 * @defgroup hal_sleep_manager_tests Sleep manager API tests
 * Tests to validate the proper implementation of the sleep manager
 *
 * To run the sleep manager hal tests use the command:
 *
 *     mbed test -t <toolchain> -m <target> -n tests-mbed_hal-sleep_manager*
 *
 */

#if defined(MBED_SLEEP_TRACING_ENABLED) || defined(MBED_SLEEP_STAT_ENABLED)

void sleep_tracker_lock(const char *const filename, int line);
void sleep_tracker_unlock(const char *const filename, int line);

#define sleep_manager_lock_deep_sleep()              \
    do                                               \
    {                                                \
        sleep_manager_lock_deep_sleep_internal();    \
        sleep_tracker_lock(MBED_FILENAME, __LINE__); \
    } while (0);

#define sleep_manager_unlock_deep_sleep()              \
    do                                                 \
    {                                                  \
        sleep_manager_unlock_deep_sleep_internal();    \
        sleep_tracker_unlock(MBED_FILENAME, __LINE__); \
    } while (0);

#else

#define sleep_manager_lock_deep_sleep() \
    sleep_manager_lock_deep_sleep_internal()

#define sleep_manager_unlock_deep_sleep() \
    sleep_manager_unlock_deep_sleep_internal()

#endif // MBED_SLEEP_TRACING_ENABLED

/** Lock the deep sleep mode
 *
 * This locks the automatic deep mode selection.
 * sleep_manager_sleep_auto() will ignore deepsleep mode if
 * this function is invoked at least once (the internal counter is non-zero)
 *
 * Use this locking mechanism for interrupt driven API that are
 * running in the background and deepsleep could affect their functionality
 *
 * The lock is a counter, can be locked up to USHRT_MAX
 * This function is IRQ and thread safe
 */
void sleep_manager_lock_deep_sleep_internal(void);

/** Unlock the deep sleep mode
 *
 * Use unlocking in pair with sleep_manager_lock_deep_sleep().
 *
 * The lock is a counter, should be equally unlocked as locked
 * This function is IRQ and thread safe
 */
void sleep_manager_unlock_deep_sleep_internal(void);

/** Get the status of deep sleep allowance for a target
 *
 * @return true if a target can go to deepsleep, false otherwise
 */
bool sleep_manager_can_deep_sleep(void);

/** Check if the target can deep sleep within a period of time
 *
 * This function in intended for use in testing. The amount
 * of time this functions waits for deeps sleep to be available
 * is currently 2ms. This may change in the future depending
 * on testing requirements.
 *
 * @return true if a target can go to deepsleep, false otherwise
 */
bool sleep_manager_can_deep_sleep_test_check(void);

/** Enter auto selected sleep mode. It chooses the sleep or deepsleep modes based
 *  on the deepsleep locking counter
 *
 * This function is IRQ and thread safe
 *
 * @note
 * If MBED_DEBUG is defined, only hal_sleep is allowed. This ensures the debugger
 * to be active for debug modes.
 *
 */
void sleep_manager_sleep_auto(void);

/** Send the microcontroller to sleep
 *
 * @note This function can be a noop if not implemented by the platform.
 * @note This function will be a noop in debug mode (debug build profile when MBED_DEBUG is defined).
 * @note This function will be a noop if the following conditions are met:
 *   - The RTOS is present
 *   - The processor turn off the Systick clock during sleep
 *   - The target does not implement tickless mode
 *
 * The processor is setup ready for sleep, and sent to sleep using __WFI(). In this mode, the
 * system clock to the core is stopped until a reset or an interrupt occurs. This eliminates
 * dynamic power used by the processor, memory systems and buses. The processor, peripheral and
 * memory state are maintained, and the peripherals continue to work and can generate interrupts.
 *
 * The processor can be woken up by any internal peripheral interrupt or external pin interrupt.
 *
 * @note
 *  The mbed interface semihosting is disconnected as part of going to sleep, and can not be restored.
 * Flash re-programming and the USB serial port will remain active, but the mbed program will no longer be
 * able to access the LocalFileSystem
 */
static inline void sleep(void)
{
#if DEVICE_SLEEP
#if (MBED_CONF_RTOS_PRESENT == 0) || (DEVICE_SYSTICK_CLK_OFF_DURING_SLEEP == 0) || defined(MBED_TICKLESS)
    sleep_manager_sleep_auto();
#endif /* (MBED_CONF_RTOS_PRESENT == 0) || (DEVICE_SYSTICK_CLK_OFF_DURING_SLEEP == 0) || defined(MBED_TICKLESS) */
#endif /* DEVICE_SLEEP */
}

/** Provides the time spent in sleep mode since boot.
 *
 *  @return  Time spent in sleep
 *  @note  Works only if platform supports LP ticker.
 */
us_timestamp_t mbed_time_sleep(void);

/** Provides the time spent in deep sleep mode since boot.
 *
 *  @return  Time spent in deep sleep
 *  @note  Works only if platform supports LP ticker.
 */
us_timestamp_t mbed_time_deepsleep(void);

/** Provides the time spent in idle mode since boot.
 *
 * @return  Idle thread time.
 * @note  Works only if platform supports LP ticker.
 */
us_timestamp_t mbed_time_idle(void);

/** Provides the time since the system is up i.e. boot.
 *
 * @return  System uptime.
 * @note  Works only if platform supports LP ticker.
 */
us_timestamp_t mbed_uptime(void);

/** @}*/

/** Resets the processor and most of the sub-system
 *
 * @note Does not affect the debug sub-system
 */
MBED_NORETURN static inline void system_reset(void)
{
    NVIC_SystemReset();
}

#ifdef __cplusplus
}
#endif

#endif

/** @}*/
/** @}*/
