/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Generic baseband driver implementation file.
 *
 *  Copyright (c) 2016-2018 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2021 Packetcraft, Inc.
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

#include <string.h>
#include "bb_api.h"
#include "bb_int.h"
#include "pal_bb.h"

/**************************************************************************************************
  Globals
**************************************************************************************************/

BbCtrlBlk_t bbCb;                       /*!< BB control block. */
const BbRtCfg_t *pBbRtCfg = NULL;       /*!< Runtime configuration. */

/**************************************************************************************************
  Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Initialize runtime configuration.
 *
 *  \param      pCfg        Pointer to runtime configuration parameters (data must be static).
 *
 *  This function initializes the BB subsystem's runtime configuration.
 *
 *  \note       This routine must be called only once before any other initialization routine.
 */
/*************************************************************************************************/
void BbInitRunTimeCfg(const BbRtCfg_t *pCfg)
{
  WSF_ASSERT(pBbRtCfg == NULL);
  WSF_ASSERT(pCfg);

  WSF_ASSERT(pCfg->clkPpm >= 20);
  WSF_ASSERT(pCfg->rfSetupDelayUs > 0);
  WSF_ASSERT(pCfg->maxScanPeriodMs > 0);
  WSF_ASSERT(pCfg->schSetupDelayUs > 0);
  WSF_ASSERT(pCfg->BbTimerBoundaryUs > 0);

  pBbRtCfg = pCfg;
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize the BB.
 *
 *  Initialize baseband resources.
 */
/*************************************************************************************************/
void BbInit(void)
{
  WSF_ASSERT(pBbRtCfg);

  PalBbInit();

  memset(&bbCb, 0, sizeof(bbCb));
}

/*************************************************************************************************/
/*!
 *  \brief      Register operation completion handler.
 *
 *  \param      eventCback  Event callback.
 *
 *  Register operation completion handler.
 */
/*************************************************************************************************/
void BbRegister(BbBodCompCback_t eventCback)
{
  bbCb.bodCompCback = eventCback;
}

/*************************************************************************************************/
/*!
 *  \brief      Start BB processing of given protocol.
 *
 *  \param      protId  Protocol ID.
 */
/*************************************************************************************************/
static void bbProtStart(uint8_t protId)
{
  WSF_ASSERT(bbCb.prot[protId].startProtCback != NULL);

  /* Enable protocol-specific BB. */
  bbCb.prot[protId].startProtCback();

  /* Protocol now started. */
  bbCb.protStarted   = TRUE;
  bbCb.protIdStarted = protId;
  PalBbSetProtId(protId);
}

/*************************************************************************************************/
/*!
 *  \brief      Start BB processing of given protocol.
 *
 *  \param      protId  Protocol ID.
 *
 *  Enable BB and start processing the list of BODs.  This routine may be called several times, if
 *  a protocol layers has several simultaneously-enabled operations.  However, \ref BbStop() must
 *  be called an equal number of time to disable the baseband.
 */
/*************************************************************************************************/
void BbStart(uint8_t protId)
{
  WSF_ASSERT(protId < BB_PROT_NUM);

  if (!bbCb.protStarted)
  {
    /* Enable generic BB. */
    PalBbEnable();

    /* Enable protocol-specific BB. */
    bbProtStart(protId);
  }

  bbCb.prot[protId].startCnt++;
}

/*************************************************************************************************/
/*!
 *  \brief      Stop BB processing of given protocol.
 *
 *  \param      protId  Protocol ID.
 */
/*************************************************************************************************/
static void bbProtStop(uint8_t protId)
{
  WSF_ASSERT(bbCb.prot[protId].stopProtCback != NULL);

  /* No protocol started. */
  bbCb.protStarted = FALSE;

  /* Disable protocol-specific BB. */
  bbCb.prot[protId].stopProtCback();
}

/*************************************************************************************************/
/*!
 *  \brief      Stop BB processing of given protocol.
 *
 *  \param      protId  Protocol ID.
 *
 *  Disable BB processing of BODs.
 *
 *  \note       For any particular protocol, calls to \ref BbStart() and \ref BbStop() must be
 *              balanced to ensure that the hardware is disabled if and only if appropriate.
 */
/*************************************************************************************************/
void BbStop(uint8_t protId)
{
  WSF_ASSERT(protId < BB_PROT_NUM);
  WSF_ASSERT(bbCb.prot[protId].startCnt > 0);

  bbCb.prot[protId].startCnt--;

  if (bbCb.protStarted && (bbCb.protIdStarted == protId) && (bbCb.prot[protId].startCnt == 0))
  {
    /* Disable protocol-specific BB. */
    bbProtStop(protId);

    /* Disable generic BB. */
    PalBbDisable();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Execute BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *
 *  Execute the protocol specific BOD handler.
 */
/*************************************************************************************************/
void BbExecuteBod(BbOpDesc_t *pBod)
{
  WSF_ASSERT(pBod);

  WSF_ASSERT(pBod->protId < BB_PROT_NUM);
  /* TODO: Removed this assert as it spuriously seems to be taken. */
  /* WSF_ASSERT(bbCb.pOpInProgress == NULL); */
  bbCb.pOpInProgress = pBod;
  bbCb.termBod = FALSE;

  /* Enable generic BB. */
  if (!bbCb.protStarted)
  {
    PalBbEnable();
  }

  /* Switch protocols if necessary. */
  if (bbCb.protStarted && (bbCb.protIdStarted != pBod->protId))
  {
    /* Disable protocol-specific BB. */
    bbProtStop(bbCb.protIdStarted);   /* sets bbCb.protStarted = FALSE */
  }
  if (!bbCb.protStarted)
  {
    /* Enable protocol-specific BB. */
    bbProtStart(pBod->protId);
  }

  if (bbCb.prot[pBod->protId].execOpCback != NULL)
  {
    bbCb.prot[pBod->protId].execOpCback(pBod);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Cancel current executing BOD.
 */
/*************************************************************************************************/
void BbCancelBod(void)
{
  if (bbCb.pOpInProgress)
  {
    BbOpDesc_t * const pBod = bbCb.pOpInProgress;

    WSF_ASSERT(pBod->protId < BB_PROT_NUM);

    bbCb.pOpInProgress = NULL;
    if (bbCb.prot[pBod->protId].cancelOpCback != NULL)
    {
      bbCb.prot[pBod->protId].cancelOpCback(pBod);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get the currently-executing BOD.
 *
 *  \return     Currently-executing BOD.
 */
/*************************************************************************************************/
BbOpDesc_t *BbGetCurrentBod(void)
{
  return bbCb.pOpInProgress;
}

/*************************************************************************************************/
/*!
 *  \brief      Set termination flag of current executing BOD.
 *
 *  \note       This function is expected to be called during the execution context of the
 *              current executing BOD, typically in the related ISRs. In the end, termination
 *              flag will help to decide if BbTerminateBod() should be called.
 */
/*************************************************************************************************/
void BbSetBodTerminateFlag(void)
{
  if (bbCb.pOpInProgress)
  {
    bbCb.termBod = TRUE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get termination state of current executing BOD.
 *
 *  \return     TRUE if termination flag set, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t BbGetBodTerminateFlag(void)
{
  return bbCb.termBod;
}

/*************************************************************************************************/
/*!
 *  \brief      Terminate a BOD immediately.
 *
 *  \note       This function is expected to be called during the execution context of the
 *              current executing BOD, typically in the related ISRs.
 */
/*************************************************************************************************/
void BbTerminateBod(void)
{
  WSF_ASSERT(bbCb.bodCompCback);

  BbOpDesc_t * const pBod = bbCb.pOpInProgress;
  if (pBod &&
      (bbCb.prot[pBod->protId].lowPowerOpCback != NULL))
  {
    bbCb.prot[pBod->protId].lowPowerOpCback();
  }

  bbCb.pOpInProgress = NULL;
  bbCb.termBod = TRUE;
  bbCb.bodCompCback();
}

/*************************************************************************************************/
/*!
 *  \brief      Get BB clock accuracy.
 *
 *  \return     Clock accuracy in part per million.
 *
 *  Returns the current BB clock accuracy.
 */
/*************************************************************************************************/
uint16_t BbGetClockAccuracy(void)
{
  return pBbRtCfg->clkPpm;
}

/*************************************************************************************************/
/*!
 *  \brief      Get BB timer boundary before wraparound.
 *
 *  \return     Time boundary in microseconds.
 *
 */
/*************************************************************************************************/
uint32_t BbGetBbTimerBoundaryUs(void)
{
  return pBbRtCfg->BbTimerBoundaryUs;
}

/*************************************************************************************************/
/*!
 *  \brief      Get scheduler setup delay.
 *
 *  \return     Scheduler setup delay in microseconds.
 *
 *  Returns the scheduler setup delay.
 */
/*************************************************************************************************/
uint16_t BbGetSchSetupDelayUs(void)
{
  return pBbRtCfg->schSetupDelayUs;
}

/*************************************************************************************************/
/*!
 *  \brief      Adjust new time tick with wraparound.
 *
 *  \param      dueUsec    Time tick without wraparound in microseconds.

 *
 *  \return     Time tick with wraparound.
 *
 *  \note        dueUsec can only be at most +/-(BbTimerBoundaryUs/2) out of range.
 */
/*************************************************************************************************/
uint32_t BbAdjustTime(uint32_t dueUsec)
{
  /* If and only of dueUsec is outside the range of [0, BbTimerBoundaryUs], mapping adjustment is needed. */
  if (dueUsec > pBbRtCfg->BbTimerBoundaryUs)
  {
    /* dueUsec is in range [-(BbTimerBoundaryUs + 1)/2, 0). */
    if (dueUsec >= ~(pBbRtCfg->BbTimerBoundaryUs >> 1))
    {
      dueUsec += (pBbRtCfg->BbTimerBoundaryUs + 1);
    }
    /* dueUsec is in range [(BbTimerBoundaryUs + 1), (BbTimerBoundaryUs + 1) + (BbTimerBoundaryUs + 1) / 2). */
    else if (dueUsec <= (pBbRtCfg->BbTimerBoundaryUs + 1 + (pBbRtCfg->BbTimerBoundaryUs >> 1)))
    {
      dueUsec -= (pBbRtCfg->BbTimerBoundaryUs + 1);
    }
    else
    {
      /* It should never happen here. */
      WSF_ASSERT(FALSE);
    }
  }

  /* If dueUsec is  in range [0, BbTimerBoundaryUs], no need to adjust. */
  return dueUsec;
}

/*************************************************************************************************/
/*!
 *  \brief      Get delta between target and reference time. Only valid if target time is in the future.
 *
 *  \param      targetUsec    Target time in microseconds.
 *  \param      refUsec       Reference time in microseconds.
 *
 *  \return     Positive number in microseconds if target time is in the future.
 *              Zero if target time is in the past or same compared with reference time.
 *
 *  \note       Caller has to make sure target time and reference time are within SCH_MAX_SPAN.
 */
/*************************************************************************************************/
uint32_t BbGetTargetTimeDelta(uint32_t targetUsec, uint32_t refUsec)
{
  targetUsec = BbAdjustTime(targetUsec);
  refUsec = BbAdjustTime(refUsec);

  uint32_t delta = 0;

  if (targetUsec > refUsec)
  {
    /* Always bigger number minus smaller number. */
    if ((targetUsec - refUsec) < ((pBbRtCfg->BbTimerBoundaryUs >> 1) + 1))
    {
      /* Normal case. */
      delta = targetUsec - refUsec;
    }
    else
    {
      /* reference time must be wraparound and target time is in the past.*/
      delta = 0;
    }
  }
  else
  {
    /* Always bigger number minus smaller number. */
    if ((refUsec - targetUsec) < ((pBbRtCfg->BbTimerBoundaryUs >> 1) + 1))
    {
      /* target time is in the past. */
      delta = 0;
    }
    else
    {
      /* Target time must be wraparound. */
      delta = pBbRtCfg->BbTimerBoundaryUs - (refUsec - targetUsec) + 1;
    }
  }

  return delta;
}

/*************************************************************************************************/
/*!
 *  \brief      Returns the ID of the active protocol.
 *
 *  \return     Protocol operation in progress.
 */
/*************************************************************************************************/
uint8_t BbGetActiveProtocol(void)
{
  return bbCb.protIdStarted;
}

/*************************************************************************************************/
/*!
 *  \brief      Register protocol handlers.
 *
 *  \param      protId          Protocol ID.
 *  \param      execOpCback     Execute operation callback.
 *  \param      cancelOpCback   Cancel operation callback.
 *  \param      startProtCback  Start protocol callback.
 *  \param      stopProtCback   Stop protocol callback.
 */
/*************************************************************************************************/
void BbRegisterProt(PalBbProt_t protId, BbBodCback_t execOpCback, BbBodCback_t cancelOpCback,
                    BbProtCback_t startProtCback, BbProtCback_t stopProtCback)
{
  WSF_ASSERT(protId < BB_PROT_NUM);
  WSF_ASSERT(startProtCback != NULL);
  WSF_ASSERT(stopProtCback != NULL);

  bbCb.prot[protId].execOpCback    = execOpCback;
  bbCb.prot[protId].cancelOpCback  = cancelOpCback;
  bbCb.prot[protId].startProtCback = startProtCback;
  bbCb.prot[protId].stopProtCback  = stopProtCback;
}

/*************************************************************************************************/
/*!
 *  \brief      Register protocol handlers for low power.
 *
 *  \param      protId          Protocol ID.
 *  \param      lowPowerOpCback Low power operation callback.
 */
/*************************************************************************************************/
void BbRegisterProtLowPower(PalBbProt_t protId, BbLowPowerCback_t lowPowerOpCback)
{
  WSF_ASSERT(protId < BB_PROT_NUM);
  WSF_ASSERT(lowPowerOpCback != NULL);
  bbCb.prot[protId].lowPowerOpCback    = lowPowerOpCback;
}
