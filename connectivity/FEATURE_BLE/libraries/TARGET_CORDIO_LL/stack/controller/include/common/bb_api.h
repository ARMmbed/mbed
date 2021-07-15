/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Baseband interface file.
 *
 *  Copyright (c) 2013-2018 ARM Ltd. All Rights Reserved.
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
 *
 *  \addtogroup BB_API              Baseband (BB) API
 *  \{
 *
 *  The baseband porting layer is a protocol independent BB + radio abstraction layer. It allows
 *  the simultaneous operation of protocol specific schedulers to transport packets across each
 *  medium via a single multi-protocol baseband. This interface describes operations for the
 *  following protocols:
 *
 *    - Bluetooth low energy: advertising and connections
 *    - ZigBee/802.15.4 (TBD)
 *    - BAN/802.15.6 (TBD)
 *
 *  \dot
 *  digraph
 *  {
 *    node [shape=record, width=1.0, height=0.5, fontname=Helvetica, fontsize=8];
 *
 *    ble    [label="BLE\nlink layer", style=filled, fillcolor=lightgray];
 *    zigbee [label="802.15.4\nMAC", style=filled, fillcolor=lightgray];
 *    ban    [label="802.15.6\nMAC", style=filled, fillcolor=lightgray];
 *    sch    [label="Multi-protocol\nscheduler", style=filled, fillcolor=lightgray];
 *    bb_drv [label="{ BB porting layer | BB specific driver }"];
 *    bb_hw  [label="BB hardware", style=filled, fillcolor=dimgray];
 *
 *    ble -> sch;
 *    ble -> bb_drv;
 *    zigbee -> sch;
 *    zigbee -> bb_drv;
 *    ban -> sch;
 *    ban -> bb_drv;
 *    sch -> bb_drv;
 *    bb_drv -> bb_hw [style=dashed];
 *  }
 *  \enddot
 *
 *  \defgroup BB_API_INIT           Initialization
 *  \defgroup BB_API_BOD            Baseband Operation Descriptors (BOD)
 *  \defgroup BB_API_CTRL           Control interface
 *  \defgroup BB_API_BLE            Bluetooth LE Protocol
 *
 *  \}
 */
/*************************************************************************************************/

#ifndef BB_API_H
#define BB_API_H

#include "wsf_types.h"
#include "cfg_mac.h"
#include "pal_bb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*! \addtogroup BB_API_BOD
 *  \{ */

/*! \} */    /* BB_API_BOD */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \addtogroup BB_API_INIT
 *  \{ */

/*! \brief      BB runtime configuration parameters. */
typedef struct
{
  /* Common */
  uint16_t  clkPpm;                 /*!< Clock accuracy in PPM. */
  uint8_t   rfSetupDelayUs;         /*!< RF setup delay in microseconds. */

  /* BLE */
  uint16_t  maxScanPeriodMs;        /*!< Maximum duration to scan in a scan interval. */

  /* Scheduler */
  uint16_t  schSetupDelayUs;        /*!< Operation setup delay in microseconds. */
  uint32_t  BbTimerBoundaryUs;      /*!< BB timer tick boundary translated in microseconds before wraparound. */
} BbRtCfg_t;

/*! \} */    /* BB_API_INIT */

/*! \addtogroup BB_API_BOD
 *  \{ */

/* Forward definition of BOD. */
struct BbOpDesc_tag;

/*! \brief      BOD completion event callback signature. */
typedef void (*BbBodCompCback_t)(void);

/*! \brief      BOD event callback signature. */
typedef void (*BbBodCback_t)(struct BbOpDesc_tag *pBod);

/*! \brief     Protocol event callback signature. */
typedef void (*BbProtCback_t)(void);

/*! \brief     Low power callback. */
typedef void (*BbLowPowerCback_t)(void);

/*! \brief      BOD rescheduling policy (listed in priority order). */
typedef enum
{
  BB_RESCH_FIXED_PREFERRED,         /*!< BOD is fixed (rescheduling has limited recoverable consequences). */
  BB_RESCH_FIXED,                   /*!< BOD is fixed (rescheduling has recoverable consequences). */
  BB_RESCH_MOVEABLE_PREFERRED,      /*!< BOD is movable (rescheduling has minor consequences). */
  BB_RESCH_MOVEABLE,                /*!< BOD is movable (rescheduling has no consequences). */
  BB_RESCH_BACKGROUND               /*!< BOD is single background task. */
} BbReschPol_t;

/*! \} */    /* BB_API_BOD */

/*! \addtogroup BB_API_BOD
 *  \{ */

/* Forward protocol data definitions. */
struct BbBleData_tag;
struct Bb154Data_tag;

/*! \brief      Baseband operation descriptor (BOD). */
typedef struct BbOpDesc_tag
{
  struct BbOpDesc_tag *pPrev;       /*!< Previous BOD. */
  struct BbOpDesc_tag *pNext;       /*!< Next BOD. */

  uint32_t      dueUsec;            /*!< Absolute clock due time in microseconds. */
  uint32_t      minDurUsec;         /*!< Minimum required duration in microseconds. */
  uint32_t      maxDurUsec;         /*!< Maximum required duration in microseconds. */

  BbReschPol_t  reschPolicy:8;      /*!< Rescheduling policy. */

  PalBbProt_t   protId:8;           /*!< Protocol type. */

  bool_t        recoverable;        /*!< Indicates if the BOD can recover if the dueUsec time is in the past. */

  BbBodCback_t  endCback;           /*!< End of BOD callback (when BOD ends). */
  BbBodCback_t  abortCback;         /*!< Abort BOD callback (when BOD is removed before beginning). */

  void          *pCtx;              /*!< Client defined context. */
  uint16_t      *pDataLen;          /*!< Pointer to client data length. */

  union
  {
    struct BbBleData_tag *pBle;     /*!< BLE operation data. */
    struct Bb154Data_tag *p154;     /*!< 802.15.4 operation data. */
  } prot;                           /*!< Protocol specific data. */
} BbOpDesc_t;

/*! \} */    /* BB_API_BOD */

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*! \addtogroup BB_API_INIT
 *  \{ */

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
void BbInitRunTimeCfg(const BbRtCfg_t *pCfg);

/*************************************************************************************************/
/*!
 *  \brief      Initialize the BB.
 *
 *  Initialize baseband resources.
 */
/*************************************************************************************************/
void BbInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Register operation completion handler.
 *
 *  \param      eventCback  Event callback.
 *
 *  Register operation completion handler.
 */
/*************************************************************************************************/
void BbRegister(BbBodCompCback_t eventCback);

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
                    BbProtCback_t startProtCback, BbProtCback_t stopProtCback);

/*************************************************************************************************/
/*!
 *  \brief      Register protocol handlers for low power.
 *
 *  \param      protId          Protocol ID.
 *  \param      lowPowerOpCback Low power operation callback.
 */
/*************************************************************************************************/
void BbRegisterProtLowPower(PalBbProt_t protId, BbLowPowerCback_t lowPowerOpCback);

/*! \} */    /* BB_API_INIT */

/*! \addtogroup BB_API_CTRL
 *  \{ */

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
void BbStart(PalBbProt_t protId);

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
void BbStop(PalBbProt_t protId);

/*************************************************************************************************/
/*!
 *  \brief      Execute BOD.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *
 *  Execute the protocol specific BOD handler.
 */
/*************************************************************************************************/
void BbExecuteBod(BbOpDesc_t *pBod);

/*************************************************************************************************/
/*!
 *  \brief      Cancel current executing BOD.
 */
/*************************************************************************************************/
void BbCancelBod(void);

/*************************************************************************************************/
/*!
 *  \brief      Get the currently-executing BOD.
 *
 *  \return     Currently-executing BOD.
 */
/*************************************************************************************************/
BbOpDesc_t *BbGetCurrentBod(void);

/*************************************************************************************************/
/*!
 *  \brief      Set termination flag of current executing BOD.
 *
 *  \note       This function is expected to be called during the execution context of the
 *              current executing BOD, typically in the related ISRs. In the end, termination
 *              flag will help to decide if BbTerminateBod() should be called.
 */
/*************************************************************************************************/
void BbSetBodTerminateFlag(void);

/*************************************************************************************************/
/*!
 *  \brief      Get termination state of current executing BOD.
 *
 *  \return     TRUE if termination flag set, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t BbGetBodTerminateFlag(void);

/*************************************************************************************************/
/*!
 *  \brief      Terminate a BOD immediately.
 *
 *  \note       This function is expected to be called during the execution context of the
 *              current executing BOD, typically in the related ISRs.
 */
/*************************************************************************************************/
void BbTerminateBod(void);

/*************************************************************************************************/
/*!
 *  \brief      Get BB clock accuracy.
 *
 *  \return     Clock accuracy in part per million.
 *
 *  Returns the current BB clock accuracy.
 */
/*************************************************************************************************/
uint16_t BbGetClockAccuracy(void);

/*************************************************************************************************/
/*!
 *  \brief      Get BB timer boundary before wraparound.
 *
 *  \return     Time boundary in microseconds.
 *
 */
/*************************************************************************************************/
uint32_t BbGetBbTimerBoundaryUs(void);

/*************************************************************************************************/
/*!
 *  \brief      Get scheduler setup delay.
 *
 *  \return     Scheduler setup delay in microseconds.
 *
 *  Returns the scheduler setup delay.
 */
/*************************************************************************************************/
uint16_t BbGetSchSetupDelayUs(void);

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
uint32_t BbAdjustTime(uint32_t dueUsec);

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
uint32_t BbGetTargetTimeDelta(uint32_t targetUsec, uint32_t refUsec);

/*************************************************************************************************/
/*!
 *  \brief      Returns the ID of the active protocol.
 *
 *  \return     Protocol operation in progress.
 */
/*************************************************************************************************/
uint8_t BbGetActiveProtocol(void);

/*! \} */    /* BB_API_CTRL */

#ifdef __cplusplus
};
#endif

#endif /* BB_API_H */
