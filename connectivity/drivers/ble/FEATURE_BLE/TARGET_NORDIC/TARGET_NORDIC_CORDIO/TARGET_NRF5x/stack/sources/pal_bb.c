/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Baseband driver port file.
 *
 *  Copyright (c) 2016-2018 Arm Ltd. All Rights Reserved.
 *  ARM confidential and proprietary.
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

#include "pal_types.h"
#include "pal_bb.h"
#include "pal_bb.h"
#include "pal_led.h"

#include "nrf.h"
#include "nrf_timer.h"

#include <string.h>

#include "mbed_nrf5x_adaptation.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! \brief  Driver enable count. */
static uint8_t palBbEnableCnt;

/*! \brief  Protocol ID. */
static uint8_t palBbProtId = BB_PROT_BLE;

/*! \brief  Table of timer IRQ callbacks. */
static bbDrvIrqCback_t palBbTimerIrqCbackTbl[BB_PROT_NUM];

/*! \brief  Table of radio IRQ callbacks. */
static bbDrvIrqCback_t palBbRadioIrqCbackTbl[BB_PROT_NUM];

/**************************************************************************************************
  Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Initialize the baseband driver.
 *
 *  One-time initialization of baseband resources. This routine can be used to setup baseband
 *  resources, load RF trim parameters and execute RF calibrations.
 *
 *  This routine should block until the BB hardware is completely initialized.
 */
/*************************************************************************************************/
void PalBbInit(void)
{
  palBbEnableCnt = 0;

  /* Cycle radio peripheral power to guarantee known radio state. */
  NRF_RADIO->POWER = 0;
  NRF_RADIO->POWER = 1;

  memset(palBbTimerIrqCbackTbl, 0, sizeof(palBbTimerIrqCbackTbl));
  memset(palBbRadioIrqCbackTbl, 0, sizeof(palBbRadioIrqCbackTbl));
}

/*************************************************************************************************/
/*!
 *  \brief      Enable the BB hardware.
 *
 *  This routine brings the BB hardware out of low power (enable power and clocks) just before a
 *  first BB operation is executed.
 */
/*************************************************************************************************/
void PalBbEnable(void)
{
  palBbEnableCnt++;
}

/*************************************************************************************************/
/*!
 *  \brief      Disable the BB hardware.
 *
 *  This routine signals the BB hardware to go into low power (disable power and clocks) after all
 *  BB operations have been disabled.
 */
/*************************************************************************************************/
void PalBbDisable(void)
{
  if (palBbEnableCnt)
  {
    palBbEnableCnt--;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Load BB timing configuration.
 *
 *  \param      pCfg                Return configuration values.
 */
/*************************************************************************************************/
void PalBbLoadCfg(PalBbCfg_t *pCfg)
{
  pCfg->clkPpm = 20;
  pCfg->rfSetupDelayUsec = BB_RF_SETUP_DELAY_US;
  pCfg->maxScanPeriodMsec = BB_MAX_SCAN_PERIOD_MS;
  pCfg->schSetupDelayUsec = BB_SCH_SETUP_DELAY_US;
#if (BB_CLK_RATE_HZ == 32768)
  pCfg->BbTimerBoundaryUsec = BB_RTC_MAX_VALUE_US;
#elif (BB_CLK_RATE_HZ == 8000000)
  pCfg->BbTimerBoundaryUsec = BB_TIMER_8MHZ_MAX_VALUE_US;
#elif (BB_CLK_RATE_HZ == 1000000)
  pCfg->BbTimerBoundaryUsec = BB_TIMER_1MHZ_MAX_VALUE_US;
#else
  #error "Unsupported platform."
#endif
}

/*************************************************************************************************/
/*!
 *  \brief      Get the current BB clock value in microseconds.
 *
 *  \return     Current BB clock value, units are microseconds.
 *
 *  This routine reads the current value from the BB clock and returns its value.
 */
/*************************************************************************************************/
uint32_t PalBbGetCurrentTime(void)
{
  if (palBbEnableCnt > 0)
  {
    if (USE_RTC_BB_CLK)
    {
      /* return the RTC counter value */
      return BB_TICKS_TO_US(PAL_BB_RTC->COUNTER);
    }
    else
    {
      /* Capture current TIMER0 count to capture register 3 */
      nrf_timer_task_trigger(PAL_BB_TIMER, NRF_TIMER_TASK_CAPTURE3);

      /* Read and return the captured count value from capture register 3 */
      return BB_TICKS_TO_US(nrf_timer_cc_read(PAL_BB_TIMER, NRF_TIMER_CC_CHANNEL3));
    }
  }
  return 0;
}

/*************************************************************************************************/
/*!
 *  \brief      Called to register a protocol's Radio and Timer IRQ callback functions.
 *
 *  \param      protId      Protocol ID.
 *  \param      timerCback  Timer IRQ callback.
 *  \param      radioCback  Timer IRQ callback.
 */
/*************************************************************************************************/
void PalBbRegisterProtIrq(uint8_t protId, bbDrvIrqCback_t timerCback, bbDrvIrqCback_t radioCback)
{
  palBbRadioIrqCbackTbl[protId] = radioCback;
  palBbTimerIrqCbackTbl[protId] = timerCback;
}

/*************************************************************************************************/
/*!
 *  \brief      Set protocol ID.
 *
 *  \param      protId      Protocol ID.
 */
/*************************************************************************************************/
void PalBbSetProtId(uint8_t protId)
{
  palBbProtId = protId;
}

/*************************************************************************************************/
/*!
 *  \brief      Combined BLE and 154 radio interrupt handler.
 */
/*************************************************************************************************/
void RADIO_IRQHandler(void)
{
  if (palBbRadioIrqCbackTbl[palBbProtId])
  {
    palBbRadioIrqCbackTbl[palBbProtId]();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Combined BLE and 154 timer interrupt handler.
 */
/*************************************************************************************************/
void PAL_BB_IRQ_HANDLER(void)
{
  if (palBbTimerIrqCbackTbl[palBbProtId])
  {
    palBbTimerIrqCbackTbl[palBbProtId]();
  }
}
