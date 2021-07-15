/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Timer driver
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

/*
 * Notes:
 *
 *    This is timer driver used for scheduler and other low power related tasks.
 *    It has dependency on PAL API, and some scheduler and BB API due to the complexity explained below.
 *    This is not ideal and should be fixed later.
 *
 *    If SCH_TIMER_REQUIRED == FALSE:
 *    No hardware timer is needed for scheduler.
 *
 *    If SCH_TIMER_REQUIRED == TRUE and BB_CLK_RATE_HZ == 32768:
 *    RTC1 timer(channel 3) is in use for scheduler.
 *
 *    If SCH_TIMER_REQUIRED == TRUE and BB_CLK_RATE_HZ == 1MHz or 8MHz:
 *    Timer2 is in used for scheduler.
 *
 *    RTC1 timer(Channel 0) is required for wsf software timer no matter what options are chosen.
 *
 *    RTC1 timer(Channel 1 and 2) are required as long as BB_CLK_RATE_HZ == 32768.
 *
 *    NRF timer1's compare/capture registers are assigned specific uses:
 *      CC[0] - Compare value for timer expiry interrupt.
 *      CC[1] - manual capture of current time.
 *
 *    NRF RTC1's compare/capture registers are assigned specific uses:
 *      CC[0] - Compare value for wsf software timer interrupt.
 *      CC[1] - Compare value for NRF timer0 start task.
 *      CC[2] - Compare value for NRF HFCLK start task.
 *      CC[3] - Compare value for scheduler load interrupt.
 *
 */

#include "pal_timer.h"
#include "pal_rtc.h"
#include "sch_api.h"
#include "bb_api.h"

#include "nrf.h"
#include "nrf_gpio.h"

#include "mbed_nrf5x_adaptation.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Ticks and microseconds conversion. */
#if BB_CLK_RATE_HZ == 32768
  #define PAL_TIMER_US_TO_TICKS(us)         ((uint32_t)(((uint64_t)(us) * (uint64_t)(70368745)) >> 31))  /* calculated value may be one tick low */
  #define PAL_TIMER_TICKS_TO_US(ticks)      ((uint32_t)(((uint64_t)(ticks) * 15625) >> 9))
#else
  #define PAL_TIMER_US_TO_TICKS(us)         (us)
  #define PAL_TIMER_TICKS_TO_US(ticks)      (ticks)
#endif

/*! \brief  23 RTC ticks time(700us) for xtal start up befor scheduler load. */
#define PAL_HFCLK_OSC_SETTLE_TICKS  (23)

#ifdef DEBUG

/*! \brief      Parameter and state check. */
#define PAL_TIMER_CHECK(expr)         { if (!(expr)) { palTimerCb.state = PAL_TIMER_STATE_ERROR; while(1); } }

#else

/*! \brief      Parameter and state check (disabled). */
#define PAL_TIMER_CHECK(expr)

#endif

/* Scheduler timer resolution is fixed at 1us per tick. */
#define PAL_TIMER_1MHZ_PRESCALER       4

#ifdef DEBUG
#define PAL_TIMER_DEBUG_0_PIN          35   /* P1.03 */
#endif

/* Predefined channel ID for timer1 and rtc1. */
#define TIMER_CHANNEL_START_BB         0
#define TIMER_CHANNEL_READ_TICK        1

#define RTC_CHANNEL_START_HFCLK        2
#define RTC_CHANNEL_START_BB           3

/* for nRF52, stall until write buffer empties */
#define WAIT_FOR_WR_BUF_EMPTY(r)    ((void)r)

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Scheduler timer driver control block. */
static struct
{
  PalTimerState_t    state;            /*!< State. */
  uint32_t           compareVal;       /*!<  Absolute compare value for timer expiry interrupt. */
} palTimerCb;

PalTimerCompCback_t palTimerExpCback;   /*!< Timer expiry call back function. */

/* Nordic specific internal functions. */
extern void PalRtcIrqRegister(uint8_t channelId, palRtcIrqCback_t cback);
extern void PalRtcClearCompareEvents(uint8_t channelId);

#if BB_CLK_RATE_HZ == 32768
/*************************************************************************************************/
/*!
 *  \brief      scheduler callback handler from RTC interrupt.
 */
/*************************************************************************************************/
static void palTimerRtcIrqHandler(void)
{
  #ifdef DEBUG
    nrf_gpio_pin_set(PAL_TIMER_DEBUG_0_PIN);
  #endif

  PAL_TIMER_CHECK(palTimerCb.state == PAL_TIMER_STATE_BUSY);

  palTimerCb.state = PAL_TIMER_STATE_READY;

  if (palTimerExpCback)
  {
    palTimerExpCback();
  }

  #ifdef DEBUG
    nrf_gpio_pin_clear(PAL_TIMER_DEBUG_0_PIN);
  #endif
}
#endif

#if SCH_TIMER_REQUIRED
/*************************************************************************************************/
/*!
 *  \brief      Get the current tick of the scheduler timer.
 *
 *  \return     Current tick of the scheduler timer.
 */
/*************************************************************************************************/
static uint32_t palTimerGetCurrentTime(void)
{
  /* Only valid for initialized scheduler timer. */
  if (palTimerCb.state)
  {
    #if BB_CLK_RATE_HZ == 32768
      /* Read and return the captured count value from capture register 1 */
      return PalRtcCounterGet();
    #else
      /* Capture current TIMER2 count to capture register 1 */
      PAL_BB_TIMER->TASKS_CAPTURE[TIMER_CHANNEL_READ_TICK] = 1;
      /* Read and return the captured count value from capture register 1 */
      return PAL_BB_TIMER->CC[TIMER_CHANNEL_READ_TICK];
    #endif
  }
  return 0;
}
#endif

/*************************************************************************************************/
/*!
 *  \brief      Initialize the scheduler timer.
 *
 *  \param      expCback   Timer expire call back function.
 */
/*************************************************************************************************/
void PalTimerInit(PalTimerCompCback_t expCback)
{
  #ifdef DEBUG
    nrf_gpio_cfg_output(PAL_TIMER_DEBUG_0_PIN);
  #endif
  PAL_TIMER_CHECK(palTimerCb.state == PAL_TIMER_STATE_UNINIT);
  PAL_TIMER_CHECK(expCback != NULL);

  #if SCH_TIMER_REQUIRED == TRUE
    #if BB_CLK_RATE_HZ == 32768
      PalRtcIrqRegister(RTC_CHANNEL_START_BB, palTimerRtcIrqHandler);
    #else
      /* Give scheduler timer the highest priority. */
      NVIC_SetPriority(PAL_BB_IRQn, 0);  /* highest priority */
      NVIC_DisableIRQ(PAL_BB_IRQn);

      /* stop timer if it was somehow running (timer must be stopped for configuration) */
      PAL_BB_TIMER->TASKS_STOP  = 1;

      /* clear timer to zero count */
      PAL_BB_TIMER->TASKS_CLEAR = 1;

      /* configure timer */
      PAL_BB_TIMER->MODE      = TIMER_MODE_MODE_Timer;
      PAL_BB_TIMER->BITMODE   = TIMER_BITMODE_BITMODE_32Bit;
      PAL_BB_TIMER->PRESCALER = PAL_TIMER_1MHZ_PRESCALER;  /* f = 16MHz / (2 ^ TIMER_PRESCALER) */

      /* timer1 is a free running clock. */
      PAL_BB_TIMER->TASKS_START = 1;

      /* Clear out and enable timer1 interrupt at system level. */
      PAL_BB_TIMER->INTENCLR = 0xFFFFFFFF;
      PAL_BB_TIMER->EVENTS_COMPARE[TIMER_CHANNEL_START_BB] = 0;
      NVIC_ClearPendingIRQ(PAL_BB_IRQn);
      NVIC_EnableIRQ(PAL_BB_IRQn);
    #endif
  #endif

  palTimerCb.compareVal = 0;
  palTimerExpCback = expCback;
  palTimerCb.state = PAL_TIMER_STATE_READY;
}

/*************************************************************************************************/
/*!
 *  \brief      De-initialize the scheduler timer.
 */
/*************************************************************************************************/
void PalTimerDeInit(void)
{
  #if SCH_TIMER_REQUIRED == TRUE
    #if BB_CLK_RATE_HZ != 32768
      NVIC_DisableIRQ(PAL_BB_IRQn);

      /* stop timer */
      PAL_BB_TIMER->TASKS_STOP  = 1;
      PAL_BB_TIMER->TASKS_SHUTDOWN = 1;
    #endif
  #endif

  palTimerCb.state = PAL_TIMER_STATE_UNINIT;
}

/*************************************************************************************************/
/*!
 *  \brief      Return scheduler timer state.
 *
 *  \return     state.
 */
/*************************************************************************************************/
PalTimerState_t PalTimerGetState(void)
{
  return palTimerCb.state;
}

/*************************************************************************************************/
/*!
 *  \brief      Get time to the next timer expiration.
 *
 *  \return     Time to next expiration in microseconds; 0 if no timer is set..
 */
/*************************************************************************************************/
uint32_t PalTimerGetTimeToNextExpiration(void)
{
  if (palTimerCb.state == PAL_TIMER_STATE_BUSY)
  {
    uint32_t currentTime = PalBbGetCurrentTime();
    if (currentTime < palTimerCb.compareVal)
    {
      /* Return the number of usecs to the next timer expiration. */
      return palTimerCb.compareVal - PalBbGetCurrentTime();
    }
    else
    {
      /* Next timer expiration is in the past. */
      return 1;
    }
  }
  else
  {
    /* No timer is active. */
    return 0;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Start the scheduler timer.
 *
 *  \param      expTimeUsec      Absolute expiration time.
 */
/*************************************************************************************************/
void PalTimerStart(uint32_t expTimeUsec)
{
  PAL_TIMER_CHECK(palTimerCb.state == PAL_TIMER_STATE_READY);
  PAL_TIMER_CHECK(expTimeUsec != 0);

  #if SCH_TIMER_REQUIRED == TRUE
    uint32_t startTimeTick = palTimerGetCurrentTime() + PAL_TIMER_US_TO_TICKS(expTimeUsec);

    #if BB_CLK_RATE_HZ == 32768
	    /* Set compare value to start BB. */
	    PalRtcCompareSet(RTC_CHANNEL_START_BB, startTimeTick);

	    /* Enable RTC interrupt source to start BB.  */
	    PalRtcEnableCompareIrq(RTC_CHANNEL_START_BB);

	    /* Set compare value to start HFCLK. */
	    uint32_t startHFCLKTick = (startTimeTick - PAL_HFCLK_OSC_SETTLE_TICKS) & PAL_MAX_RTC_COUNTER_VAL;

	    PalRtcClearCompareEvents(RTC_CHANNEL_START_HFCLK);
	    PalRtcCompareSet(RTC_CHANNEL_START_HFCLK, startHFCLKTick);
 	  #else
		  /* Clear pending events. */
	   	PAL_BB_TIMER->EVENTS_COMPARE[PAL_BB_IRQ_EXPIRY_CHANNEL] = 0;
    	WAIT_FOR_WR_BUF_EMPTY(PAL_BB_TIMER->EVENTS_COMPARE[PAL_BB_IRQ_EXPIRY_CHANNEL]);

      /* Set compare value. */
    	PAL_BB_TIMER->CC[PAL_BB_IRQ_EXPIRY_CHANNEL] = startTimeTick;

      /* Enable timer1 interrupt source for CC[PAL_BB_IRQ_EXPIRY_CHANNEL].  */
   	 	PAL_BB_TIMER->INTENSET = PAL_BB_TIMER_EXPIRY_INTENSET_MASK;
    #endif // BB_CLK_RATE_HZ

	  palTimerCb.compareVal = startTimeTick;
  	palTimerCb.state = PAL_TIMER_STATE_BUSY;
  #else
    (void)expTimeUsec;
    if (BbGetCurrentBod() == NULL && palTimerExpCback)
    {
      palTimerExpCback();
    }
  #endif // SCH_TIMER_REQUIRED
}

/*************************************************************************************************/
/*!
 *  \brief      Stop the scheduler timer.
 */
/*************************************************************************************************/
void PalTimerStop()
{
  #if SCH_TIMER_REQUIRED == TRUE
    #if BB_CLK_RATE_HZ == 32768
      /* Disable this interrupt */
      PalRtcDisableCompareIrq(RTC_CHANNEL_START_BB);
    #else
      /* Disable this interrupt */
      PAL_BB_TIMER->INTENCLR = PAL_BB_TIMER_EXPIRY_INTENCLR_MASK;
    #endif

    palTimerCb.state = PAL_TIMER_STATE_READY;
  #else
    BbCancelBod();
  #endif

  palTimerCb.state = PAL_TIMER_STATE_READY;
}
