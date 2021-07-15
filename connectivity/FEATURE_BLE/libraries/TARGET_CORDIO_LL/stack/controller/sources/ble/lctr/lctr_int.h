/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Internal link layer controller interface file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#ifndef LCTR_INT_H
#define LCTR_INT_H

#include "lctr_api.h"
#include "lmgr_api.h"
#include "ll_defs.h"
#include "ll_math.h"
#include "util/crc32.h"
#include "pal_bb.h"

#if (LL_ENABLE_TESTER)
#include "ll_tester_api.h"
#include "lctr_int_tester.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Minimum delay between connect indication and data channel in CONNECT_IND units. */
#define LCTR_DATA_CHAN_DLY                           1       /* 1.25 ms */

/*! \brief      Minimum delay between auxiliary connect request and data channel using uncoded PHY in CONNECT_REQ units. */
#define LCTR_DATA_CHAN_DLY_AUX_UNCODED               2       /* 2.5 ms */

/*! \brief      Minimum delay between auxiliary connect request and data channel using coded PHY in CONNECT_REQ units. */
#define LCTR_DATA_CHAN_DLY_AUX_CODED                 3       /* 3.75 ms */

/*! \brief      Convert connect indication ticks to scheduler ticks. */
#define LCTR_CONN_IND_TICKS(x)                       ((x) * (1250 / LL_BLE_US_PER_TICK))

/*! \brief      Convert connect indication ticks to milliseconds (no divide, rounds up). */
#define LCTR_CONN_IND_MS(x)                          ((x) + ((x) >> 2) + (((x) & 3) ? 1 : 0))

/*! \brief      Convert connect indication ticks to microseconds. */
#define LCTR_CONN_IND_US(x)                          ((x) * 1250)

/*! \brief      Convert connect indication timeout ticks to milliseconds. */
#define LCTR_CONN_IND_TO_MS(x)                       ((x) * 10)

/*! \brief      Convert microseconds to connection indication ticks. */
#define LCTR_US_TO_CONN_IND(x)                       LL_MATH_DIV_1250(x)

/*! \brief      Convert BLE protocol ticks to microseconds. */
#define LCTR_BLE_TO_US(x)                            ((x) * LL_BLE_US_PER_TICK)

/*! \brief      Convert periodic interval milliseconds to microseconds. */
#define LCTR_PER_INTER_TO_US(x)                      ((x) * 1250)

/*! \brief      Convert periodic interval microseconds to milliseconds. */
#define LCTR_PER_INTER_TO_MS(x)                      LL_MATH_DIV_1250(x)

/*! \brief      Convert periodic sync timeout unit to milliseconds. */
#define LCTR_PER_SYNC_TIMEOUT_TO_MS(x)               ((x) * 10)

/*! \brief      Convert isochronous interval to microseconds. */
#define LCTR_ISO_INT_TO_US(x)                        ((x) * 1250)

/*! \brief      Fast termination supervision multiplier. */
#define LCTR_FAST_TERM_CNT                           6

/*! \brief      Duration of a advertising packet in microseconds. */
#define LCTR_ADV_PKT_1M_US(len)                      ((LL_PREAMBLE_LEN_1M + LL_AA_LEN + LL_ADV_HDR_LEN + len + LL_CRC_LEN) << 3)

/*! \brief      Duration of a connection indication packet in microseconds. */
#define LCTR_CONN_IND_PKT_1M_US                      LCTR_ADV_PKT_1M_US(LL_CONN_IND_PDU_LEN)

/*! \brief      Extra area in ADVB buffer. */
#define LCTR_ADVB_BUF_EXTRA_SIZE                     6

/*! \brief      Size for ADVB buffer allocation. */
#define LCTR_ADVB_BUF_SIZE                           (WSF_MAX(BB_FIXED_ADVB_PKT_LEN, LL_ADVB_MAX_LEN) + LCTR_ADVB_BUF_EXTRA_SIZE)

/*! \brief      RSSI offset of extra data in ADVB buffer. */
#define LCTR_ADVB_BUF_OFFSET_RSSI                    ((LCTR_ADVB_BUF_SIZE - LCTR_ADVB_BUF_EXTRA_SIZE) + 0)

/*! \brief      RPA offset of extra data in ADVB buffer. */
#define LCTR_ADVB_BUF_OFFSET_RX_RPA                  ((LCTR_ADVB_BUF_SIZE - LCTR_ADVB_BUF_EXTRA_SIZE) + 1)

/*! \brief      CRC offset of extra data in ADVB buffer. */
#define LCTR_ADVB_BUF_OFFSET_CRC                     ((LCTR_ADVB_BUF_SIZE - LCTR_ADVB_BUF_EXTRA_SIZE) + 2)

/*! \brief      LCTR Maximum span of scheduler elements. */
#define LCTR_SCH_MAX_SPAN                            ((BbGetBbTimerBoundaryUs() >> 1) + 1)

/*! \brief      LCTR Maximum value for sleep clock accuracy. */
#define LCTR_MAX_SCA                                 7

/*! \brief      Change supervision timeout value to us. */
#define LCTR_SUP_TIMEOUT_VAL_TO_US(x)                (x * 10000)

/*! \brief      Array size of running average arrays, derived from LL_PC_TBL_LEN. */
#define LL_PC_TBL_LEN                                (1 << LL_PC_TBL_POW)

/*! \brief      Calculate microsecond format from channel reporting parameter (200 millisecond ticks). */
#define LCTR_CH_RPT_IND_US(x)                        ((((uint64_t) x) * 200) * 1000)

/*! \brief      Calculate microsecond format from channel reporting parameter (200 millisecond ticks). */
#define LCTR_CH_RPT_IND_US(x)    ((((uint64_t) x) * 200) * 1000)

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Call signature of a reset handler. */
typedef void (*LctrResetHdlr_t)(void);

/*! \brief      Call signature of a message dispatch handler. */
typedef void (*LctrMsgDisp_t)(lctrMsgHdr_t *pMsg);

/*! \brief      Call signature of a message dispatch handler. */
typedef void (*LctrEvtHdlr_t)(void);

/*! \brief      Reservation manager callback signature. */
typedef void (*LctrRmCback_t)(uint32_t rsvnOffs[], uint32_t refTime);

/*! \brief      Channel class update handler call signature. */
typedef uint8_t (*lctrChClassHdlr_t)(uint64_t chanMap);

/*! \brief      RSSI running average data type. */
typedef struct
{
  int8_t averageRssi[LL_PC_TBL_LEN];
                                    /*!< Running average RSSI. */
  uint8_t rssiIdx;                  /*!< Running average RSSI index. */
  uint8_t avgCount;                 /*!< Count of average entries since last reset. */
} lctrRssiRunAvg_t;

/**************************************************************************************************
  Globals
**************************************************************************************************/

extern LctrResetHdlr_t lctrResetHdlrTbl[LCTR_DISP_TOTAL];
extern LctrMsgDisp_t lctrMsgDispTbl[LCTR_DISP_TOTAL];
extern LctrEvtHdlr_t lctrEventHdlrTbl[LCTR_EVENT_TOTAL];
extern lctrMsgHdr_t *pLctrMsg;
extern const uint16_t scaPpmTbl[];
extern bool_t lctrResetEnabled;

/**************************************************************************************************
  Functions
**************************************************************************************************/

/* Initialization. */
void lctrRegisterChClassHandler(lctrChClassHdlr_t cback);

/* Helper routines. */
uint32_t lctrComputeAccessAddr(void);
uint32_t lctrComputeSeedAccessAddr(void);
uint8_t lctrComputeHopInc(void);
uint8_t lctrPeriodicSelectNextChannel(lmgrChanParam_t *pChanParam, uint16_t eventCounter);
void lctrPeriodicBuildRemapTable(lmgrChanParam_t *pChanParam);
uint16_t lctrCalcTotalAccuracy(uint8_t mstScaIdx);
uint32_t lctrComputeCrcInit(void);
uint32_t lctrCalcWindowWideningUsec(uint32_t unsyncTimeUsec, uint32_t caPpm);

/* RSSI Averaging. */
int8_t lctrRssiGetAverage(lctrRssiRunAvg_t *pAvg);
void lctrRssiAddAveragePoint(lctrRssiRunAvg_t *pAvg, int8_t value);

/* Host events */
void lctrNotifyHostHwErrInd(uint8_t code);
void lctrNotifyHostConnectInd(uint16_t handle, uint8_t role, lctrConnInd_t *pConnInd,
                              uint8_t peerIdAddrType, uint64_t peerIdAddr, uint64_t peerRpa,
                              uint64_t localRpa, uint8_t status, uint8_t usedChSel);

/* State machine */
void lctrMstScanExecuteSm(uint8_t event);
void lctrMstInitExecuteSm(uint8_t event);
void lctrSlvAdvExecuteSm(uint8_t event);

/*************************************************************************************************/
/*!
 *  \brief  Calculate DID.
 *
 *  \param  pBuf    Data buffer.
 *  \param  len     Length of data buffer.
 *
 *  \return DID value.
 */
/*************************************************************************************************/
static inline uint16_t lctrCalcDID(const uint8_t *pBuf, uint16_t len)
{
  return CalcCrc32(LlMathRandNum(), len, pBuf);
}

/*************************************************************************************************/
/*!
 *  \brief  Convert PHYS bit to PHY.
 *
 *  \param  physBit     PHYS bit.
 *
 *  \return PHY.
 */
/*************************************************************************************************/
static inline PalBbPhy_t lctrPhysBitToPhy(uint8_t physBit)
{
  switch (physBit)
  {
    default:
    case LL_PHYS_LE_1M_BIT:
      return BB_PHY_BLE_1M;
    case LL_PHYS_LE_2M_BIT:
      return BB_PHY_BLE_2M;
    case LL_PHYS_LE_CODED_BIT:
      return BB_PHY_BLE_CODED;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Convert PHY to PHYS bit.
 *
 *  \param  phy         PHY.
 *
 *  \return PHYS bit.
 */
/*************************************************************************************************/
static inline uint8_t lctrPhyToPhysBit(uint8_t phy)
{
  switch (phy)
  {
    default:
    case BB_PHY_BLE_1M:
      return LL_PHYS_LE_1M_BIT;
    case BB_PHY_BLE_2M:
      return LL_PHYS_LE_2M_BIT;
    case BB_PHY_BLE_CODED:
      return LL_PHYS_LE_CODED_BIT;
  }
}

#ifdef __cplusplus
};
#endif

#endif /* LCTR_INT_H */
