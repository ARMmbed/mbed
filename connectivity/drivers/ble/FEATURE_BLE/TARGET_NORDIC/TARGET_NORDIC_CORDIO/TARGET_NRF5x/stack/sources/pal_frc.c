/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  FRC implementation.
 *
 *  Low power Free Running Clock. A monotonically increasing 32-bit free running counter which
 *  can count even when the system is in low power states.
 *
 *  Copyright (c) 2019-2020 Packetcraft, Inc.
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*************************************************************************************************/

#include "pal_frc.h"
#include "pal_led.h"
#include "pal_sys.h"

#include "nrfx_rtc.h"
#include "nrfx_clock.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* \brief   Ticks per seconds. */
#define PAL_FRC_TICKS_PER_SEC   NRFX_RTC_DEFAULT_CONFIG_FREQUENCY

/* \brief   Maximum FRC time. */
#define PAL_FRC_MAX_TIME        0x00FFFFFF

/* \brief   RTC channel used for wakeup. */
#define PAL_FRC_WAKEUP_CHAN     NRFX_RTC_INT_COMPARE0

/* \brief   Maximum allowed timer value in milliseconds (511,999 ms). */
#define PAL_FRC_MAX_WAKEUP_MS   (PAL_FRC_MAX_TIME / PAL_FRC_TICKS_PER_SEC * 1000)

/* \brief   Maximum allowed timer value in microseconds (511,999,969 us). */
#define PAL_FRC_MAX_WAKEUP_US   (PAL_FRC_MAX_TIME / PAL_FRC_TICKS_PER_SEC * 1000000)

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! \brief  RTC control block. */
#if NRFX_RTC1_ENABLED
static const nrfx_rtc_t palFrcRtcInst = NRFX_RTC_INSTANCE(1);
#elif NRFX_RTC2_ENABLED
static const nrfx_rtc_t palFrcRtcInst = NRFX_RTC_INSTANCE(2);
#endif

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Set wakeup timer.
 */
/*************************************************************************************************/
void palFrcSetWakeup(uint32_t nextWakeMs)
{
  PAL_SYS_ASSERT(nextWakeMs < PAL_FRC_MAX_WAKEUP_MS);

  uint32_t nextWake = NRFX_RTC_US_TO_TICKS(nextWakeMs * UINT64_C(1000), UINT64_C(PAL_FRC_TICKS_PER_SEC));
  uint32_t wakeTime = nrfx_rtc_counter_get(&palFrcRtcInst) + nextWake;

  nrfx_rtc_cc_set(&palFrcRtcInst, PAL_FRC_WAKEUP_CHAN, wakeTime, true);
}

/*************************************************************************************************/
/*!
 *  \brief  Clear wakeup timer.
 */
/*************************************************************************************************/
void palFrcClearWakeup(void)
{
  nrfx_rtc_cc_disable(&palFrcRtcInst, PAL_FRC_WAKEUP_CHAN);
}

/*************************************************************************************************/
/*!
 *  \brief  Clock event handler.
 */
/*************************************************************************************************/
static void palFrcWakeupClockEvent(nrfx_clock_evt_type_t event)
{
  /* No action require; ISR used to wakeup CPU. */
}

/*************************************************************************************************/
/*!
 *  \brief  RTC event handler.
 */
/*************************************************************************************************/
static void palFrcWakeupRtcEvent(nrfx_rtc_int_type_t int_type)
{
  switch (int_type)
  {
  case PAL_FRC_WAKEUP_CHAN:
    /* No action require; ISR used to wakeup CPU. */
    break;
  default:
    break;
  }
}

/**************************************************************************************************
  Public Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  FRC initialization.
 */
/*************************************************************************************************/

void PalFrcInit(void)
{
  nrfx_err_t err;
  (void)err;

  err = nrfx_clock_init(palFrcWakeupClockEvent);
  PAL_SYS_ASSERT(err == NRFX_SUCCESS);
  nrfx_clock_enable();
  nrfx_clock_lfclk_start();
  while (!nrfx_clock_lfclk_is_running()) { };

  nrfx_rtc_config_t cfg = NRFX_RTC_DEFAULT_CONFIG;
  err = nrfx_rtc_init(&palFrcRtcInst, &cfg, palFrcWakeupRtcEvent);
  PAL_SYS_ASSERT(err == NRFX_SUCCESS);
  nrfx_rtc_enable(&palFrcRtcInst);
  nrfx_rtc_counter_clear(&palFrcRtcInst);
  nrfx_rtc_tick_enable(&palFrcRtcInst, false);
}

/*************************************************************************************************/
/*!
 *  \brief  Get current time.
 *
 *  \return FRC time.
 *
 *  \note   Caution must be taken when performing calculations with FRC time. FRC value bit width
 *          and resolution may vary.
 */
/*************************************************************************************************/
uint32_t PalFrcGetCurrentTime(void)
{
  return nrfx_rtc_counter_get(&palFrcRtcInst);
}

/*************************************************************************************************/
/*!
 *  \brief  Convert time in milliseconds to FRC ticks.
 *
 *  \param  deltaMs     Time in milliseconds.
 *
 *  \return Conversation of \a deltaMs to FRC ticks.
 */
/*************************************************************************************************/
uint32_t PalFrcMsToTicks(uint32_t timeMs)
{
  return timeMs * PAL_FRC_TICKS_PER_SEC / 1000;
}

/*************************************************************************************************/
/*!
 *  \brief  Compute delta time in milliseconds.
 *
 *  \param  endTime     FRC end time.
 *  \param  startTime   FRC start time.
 *
 *  \return Delta time in milliseconds.
 */
/*************************************************************************************************/
uint32_t PalFrcDeltaMs(uint32_t endTime, uint32_t startTime)
{
  uint32_t deltaTime = (endTime - startTime) & PAL_FRC_MAX_TIME;
  return deltaTime * UINT64_C(1000) / PAL_FRC_TICKS_PER_SEC;
}

/*************************************************************************************************/
/*!
 *  \brief  Compute delta time in microseconds.
 *
 *  \param  endTime     FRC end time.
 *  \param  startTime   FRC start time.
 *
 *  \return Delta time in microseconds.
 */
/*************************************************************************************************/
uint32_t PalFrcDeltaUs(uint32_t endTime, uint32_t startTime)
{
  uint32_t deltaTime = (endTime - startTime) & PAL_FRC_MAX_TIME;
  return deltaTime * UINT64_C(1000000) / PAL_FRC_TICKS_PER_SEC;
}
