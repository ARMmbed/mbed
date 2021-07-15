/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Main link layer controller implementation file.
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

#include "lctr_int.h"
#include "lmgr_api.h"
#include "sch_api_ble.h"
#include "bb_ble_api_reslist.h"
#include "bb_ble_api_whitelist.h"
#include "wsf_assert.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Constants
**************************************************************************************************/

#define LCTR_NUM_CH_CLASS_HANDLERS      4       /*!< Number of channel class handlers. */

/**************************************************************************************************
  Globals
**************************************************************************************************/

/*! \brief      Link layer controller reset handler lookup table. */
LctrResetHdlr_t lctrResetHdlrTbl[LCTR_DISP_TOTAL];

/*! \brief      Link layer controller message dispatcher lookup table. */
LctrMsgDisp_t   lctrMsgDispTbl[LCTR_DISP_TOTAL];

/*! \brief      Link layer task event handler lookup table. */
LctrEvtHdlr_t   lctrEventHdlrTbl[LCTR_EVENT_TOTAL];

/*! \brief      Event message which triggered the current execution context. */
lctrMsgHdr_t    *pLctrMsg;

/*! \brief      Channel class update handlers. */
static lctrChClassHdlr_t lctrChClassHandlerTbl[LCTR_NUM_CH_CLASS_HANDLERS];

/*! \brief      Channel class update handler count. */
static uint8_t lctrChClassHandlerCnt;

/*! \brief      Reset enabled. */
bool_t   lctrResetEnabled = FALSE;

/*************************************************************************************************/
/*!
 *  \brief      Notify host reset confirm.
 */
/*************************************************************************************************/
static void lctrNotifyHostResetCnf(void)
{
  wsfMsgHdr_t evt =
  {
    .event  = LL_RESET_CNF,
    .status = LL_SUCCESS
  };

  LL_TRACE_INFO0("### LlEvent ###  LL_RESET_CNF, status=LL_SUCCESS");

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Set the supporting state bitmask.
 */
/*************************************************************************************************/
void LctrSetSupStates(void)
{
  const bool_t supAdv  = lctrMsgDispTbl[LCTR_DISP_ADV]  ? TRUE : FALSE;
  const bool_t supScan = lctrMsgDispTbl[LCTR_DISP_SCAN] ? TRUE : FALSE;
  const bool_t supInit = lctrMsgDispTbl[LCTR_DISP_INIT] ? TRUE : FALSE;
  const bool_t supConn = lctrMsgDispTbl[LCTR_DISP_CONN] ? TRUE : FALSE;

  lmgrPersistCb.supStates = 0;

  /* Non-connectable advertising is supported. */
  if (supAdv)
  {
    lmgrPersistCb.supStates |= LL_SUP_STATE_NON_CONN_ADV |
                               LL_SUP_STATE_SCAN_ADV;
  }

  /* Non-connectable advertising while scanning is supported. */
  if (supAdv && supScan)
  {
    lmgrPersistCb.supStates |= LL_SUP_STATE_NON_CONN_ADV_AND_PASS_SCAN |
                               LL_SUP_STATE_SCAN_ADV_AND_PASS_SCAN |
                               LL_SUP_STATE_NON_CONN_ADV_AND_ACT_SCAN |
                               LL_SUP_STATE_SCAN_ADV_AND_ACT_SCAN |
                               LL_SUP_STATE_LO_DUTY_DIR_ADV_AND_PASS_SCAN |
                               LL_SUP_STATE_LO_DUTY_DIR_ADV_AND_ACT_SCAN;
  }

  /* Connectable advertising and slave connection are supported. */
  if (supAdv && supConn)
  {
    lmgrPersistCb.supStates |= LL_SUP_STATE_CONN_ADV |
                               LL_SUP_STATE_HI_DUTY_DIR_ADV |
                               LL_SUP_STATE_LO_DUTY_DIR_ADV |
                               LL_SUP_STATE_CONN_SLV;
    if (pLctrRtCfg->maxConn > 1)
    {
      lmgrPersistCb.supStates |= LL_SUP_STATE_CONN_ADV_SLV |
                                 LL_SUP_STATE_HI_DUTY_DIR_ADV_SLV |
                                 LL_SUP_STATE_LO_DUTY_DIR_ADV_SLV;
    }
  }

  /* Scanning is supported. */
  if (supScan)
  {
    lmgrPersistCb.supStates |= LL_SUP_STATE_PASS_SCAN |
                               LL_SUP_STATE_ACT_SCAN;
  }

  /* Initiating and scanning while connected are supported. */
  if (supScan && supConn)
  {
    lmgrPersistCb.supStates |= LL_SUP_STATE_INIT |
                               LL_SUP_STATE_PASS_SCAN_MST |
                               LL_SUP_STATE_ACT_SCAN_MST |
                               LL_SUP_STATE_PASS_SCAN_SLV |
                               LL_SUP_STATE_ACT_SCAN_SLV;

    if (pLctrRtCfg->maxConn > 1)
    {
      lmgrPersistCb.supStates |= LL_SUP_STATE_INIT_MST;
    }

    if (supInit)
    {
      lmgrPersistCb.supStates |= LL_SUP_STATE_PASS_SCAN_AND_INIT |
                                 LL_SUP_STATE_ACT_SCAN_AND_INIT;
    }
  }

  /* Connectable advertising while scanning and advertising while initiating and advertising while connected are supported. */
  if (supAdv && supScan && supConn)
  {
    lmgrPersistCb.supStates |= LL_SUP_STATE_CONN_ADV_AND_PASS_SCAN |
                               LL_SUP_STATE_HI_DUTY_DIR_ADV_AND_PASS_SCAN |
                               LL_SUP_STATE_CONN_ADV_AND_ACT_SCAN |
                               LL_SUP_STATE_HI_DUTY_DIR_ADV_ACT_SCAN |
                               LL_SUP_STATE_NON_CONN_ADV_AND_INIT |
                               LL_SUP_STATE_SCAN_ADV_AND_INIT |
                               LL_SUP_STATE_NON_CONN_ADV_MST |
                               LL_SUP_STATE_SCAN_ADV_MST |
                               LL_SUP_STATE_NON_CONN_ADV_SLV |
                               LL_SUP_STATE_SCAN_ADV_SLV;

    if (pLctrRtCfg->maxConn > 1)
    {
      lmgrPersistCb.supStates |= LL_SUP_STATE_CONN_ADV_AND_INIT |
                                 LL_SUP_STATE_HI_DUTY_DIR_ADV_AND_INIT |
                                 LL_SUP_STATE_LO_DUTY_DIR_ADV_AND_INIT |
                                 LL_SUP_STATE_CONN_ADV_MST |
                                 LL_SUP_STATE_HI_DUTY_DIR_ADV_MST |
                                 LL_SUP_STATE_LO_DUTY_DIR_ADV_MST |
                                 LL_SUP_STATE_INIT_SLV;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Link layer controller message dispatch handler.
 *
 *  \param  pMsg        WSF message.
 */
/*************************************************************************************************/
void LctrMsgDispatcher(lctrMsgHdr_t *pMsg)
{
  uint8_t dispId = pMsg->dispId;

  pLctrMsg = pMsg;

  if (dispId != LCTR_DISP_BCST)
  {
    /*** Unicast message ***/

    WSF_ASSERT(dispId < LCTR_DISP_TOTAL);

    if (lctrMsgDispTbl[dispId])
    {
      lctrMsgDispTbl[dispId](pMsg);
    }
  }
  else
  {
    /*** Broadcast message ***/

    if (pMsg->event == LCTR_MSG_RESET)
    {
      lctrResetEnabled = TRUE;
    }

    for (dispId = LCTR_DISP_FIRST_SM; dispId <= LCTR_DISP_LAST_SM; dispId++)
    {
      if (lctrMsgDispTbl[dispId])
      {
        lctrMsgDispTbl[dispId](pMsg);
      }
    }
  }

  if (lctrResetEnabled && (lmgrCb.resetDelayRefCnt == 0))
  {
    /* Reset common/shared subsystems. */
    BbBleWhiteListClear();
    BbBleResListClear();
    LmgrSetDefaults();
    #if (LL_ENABLE_TESTER)
      LlTesterInit();
    #endif

    for (dispId = LCTR_DISP_FIRST_SM; dispId <= LCTR_DISP_LAST_SM; dispId++)
    {
      if (lctrResetHdlrTbl[dispId])
      {
        lctrResetHdlrTbl[dispId]();
      }
    }

    lctrResetEnabled = FALSE;
    lctrNotifyHostResetCnf();
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Link layer controller task event handler.
 *
 *  \param  event       Event id.
 */
/*************************************************************************************************/
void LctrEventHandler(uint8_t event)
{
  WSF_ASSERT(event < LCTR_EVENT_TOTAL);

  if (lctrEventHdlrTbl[event])
  {
    lctrEventHdlrTbl[event]();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host HW error indication.
 *
 *  \param      code        Error code.
 */
/*************************************************************************************************/
void lctrNotifyHostHwErrInd(uint8_t code)
{
  LlHwErrorInd_t evt =
  {
    .hdr =
    {
      .param        = 0,
      .event        = LL_ERROR_IND,
      .status       = code
    },

    .code           = code
  };

  LL_TRACE_INFO1("### LlEvent ###  LL_ERROR_IND, status=%u", code);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief  Compute the window widening delay in microseconds.
 *
 *  \param  unsyncTimeUsec  Unsynchronized time in microseconds.
 *  \param  caPpm           Total clock accuracy.
 *
 *  \return Window widening delay in microseconds.
 */
/*************************************************************************************************/
uint32_t lctrCalcWindowWideningUsec(uint32_t unsyncTimeUsec, uint32_t caPpm)
{
  if (lmgrGetOpFlag(LL_OP_MODE_FLAG_ENA_WW))
  {
    /* Largest unsynchronized time is 1,996 seconds (interval=4s and latency=499) and
     * largest total accuracy is 1000 ppm. */
    /* coverity[overflow_before_widen] */
    uint64_t wwDlyUsec = LL_MATH_DIV_10E6(((uint64_t)unsyncTimeUsec * caPpm) +
                                          999999);     /* round up */

    /* Reduce to 32-bits and always round up to a sleep clock tick. */
    return wwDlyUsec + pLctrRtCfg->ceJitterUsec + LL_WW_RX_DEVIATION_USEC;
  }
  else
  {
    return 0;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Compute new access address.
 *
 *  \return New access address value.
 */
/*************************************************************************************************/
uint32_t lctrComputeAccessAddr(void)
{
  uint32_t accessAddr = LlMathRandNum();

  /*
   * The following code enforces a pattern to make sure the address meets all requirements
   * (including requirements for the LE coded PHY).  The pattern is
   *
   *   0byyyyyy1x 0xxxx1x0 xxxx1x0x xx11x0x1
   *
   * with 2^5 choices for the upper six bits.  This provides 2^5 * 2^16 = 2097152 variations.
   */

  /* Patterns for upper six bits.  The lower row contains complemented values of the upper row. */
  static const uint8_t upperSixBits[] =
  {
    /* 000010 000100 000101 000110 001000 001100 001101 001110 010000 010001 010011 010111 010110 011000 011100 011110 */
       0x08,  0x10,  0x14,  0x18,  0x20,  0x30,  0x34,  0x38,  0x40,  0x44,  0x4C,  0x5C,  0x58,  0x60,  0x70,  0x78,
    /* 111101 111011 111010 111001 110111 110011 110010 110001 101111 101110 101100 101000 101001 100111 100011 100001 */
       0xF4,  0xEC,  0xE8,  0xE4,  0xDC,  0xCC,  0xC8,  0xC4,  0xBC,  0xB8,  0xB0,  0xA0,  0xA4,  0x9C,  0x8C,  0x84
  };

  /* Set the upper six bits. */
  accessAddr  = (accessAddr & ~0xFC000000) | (upperSixBits[accessAddr >> 27] << 24);

  /* Set   ones  with the mask 0b00000010 00000100 00001000 00110001 */
  accessAddr |=  0x02040831;

  /* Clear zeros with the mask 0b00000000 10000001 00000010 00000100 */
  accessAddr &= ~0x00810204;

  return accessAddr;
}

/*************************************************************************************************/
/*!
 *  \brief  Compute new seed access address.
 *
 *  \return New seed access address value.
 */
/*************************************************************************************************/
uint32_t lctrComputeSeedAccessAddr(void)
{
  uint32_t accessAddr = LlMathRandNum();

  /*
   * The following code enforces a pattern to make sure the address meets all requirements.
   * The pattern is either the first one or the second one
   *
   *   0bxxxxxx0x 1yyyyyyy yxxxxxxx xxxxxxxx
   *
   * with 2^5 choices for the middle 8 bits(indicated by y).  This provides 2^5 * 2^ (21 - 4(z)) = 2 ^ 22 = 4194304 variations.
   * Z depends LL_MAX_BIS, 6 BISs require 4 bits or 16 combination to assure at least two bits are different.
   */

  /* Patterns for middle 8 bits with 32 combinations. The upper one follows the 1yy0yy10 and lower one follows the 0yy1yy01 */
  static const uint8_t upperSixBits[] =
  {
    /* 10000010 10000110  10001010  10001110  10100010  10100110  10101010  10101110  11000010  11000110  11001010  11001110  11100010  11100110  11101010  11101110 */
       0x82,    0x86,     0x8A,     0x8E,     0xA2,     0xA6,     0xAA,     0xAE,     0xC2,     0xC6,     0xCA,     0xCE,     0xE2,     0xE6,     0xEA,     0xEE,
    /* 00010001 00010101  00011001  00011101  00110001  00110101  00111001  00111101  01010001  01010101  01011001  01011101  01110001  01110101  01111001  01111101 */
       0x11,    0x15,     0x19,     0x1D,     0x31,     0x35,     0x39,     0x3D,     0x51,     0x55,     0x59,     0x5D,     0x71,     0x75,     0x79,     0x7D,
  };

  /* Set the middle 8 bits. */
  accessAddr  = (accessAddr & ~0x007F8000) | (upperSixBits[accessAddr >> 27] << 15);

  /* Set ones with the mask 0b00000000 10000000 00000000 00000000 */
  accessAddr |=  0x00800000;

  /* Clear zeros with the mask 0b00000010 00000000 00000000 00000000 */
  accessAddr &= ~0x02000000;

  /* TODO add code to make sure two BIG seedAccessAddress in the same device shall differ in at least two bits. */

  return accessAddr;
}

/*************************************************************************************************/
/*!
 *  \brief      Register channel class handler
 *
 *  \param      cback        Callback function to register.
 */
/*************************************************************************************************/
void lctrRegisterChClassHandler(lctrChClassHdlr_t cback)
{
  /* Ensure registration does not exceed limits. */
  WSF_ASSERT(lctrChClassHandlerCnt < LCTR_NUM_CH_CLASS_HANDLERS);

  /* If callback is already registered, do nothing. */
  for (unsigned int i = 0; i < lctrChClassHandlerCnt; i++)
  {
    if (lctrChClassHandlerTbl[0] == cback)
    {
      return;
    }
  }

  lctrChClassHandlerTbl[lctrChClassHandlerCnt++] = cback;
}

/*************************************************************************************************/
/*!
 *  \brief      Set channel class.
 *
 *  \param      chanMap        Channel map (0=bad, 1=usable).
 *
 *  \return     Status error code.
 *
 *  Set the channel class. At least 2 bits must be set.
 */
/*************************************************************************************************/
uint8_t LctrSetChannelClass(uint64_t chanMap)
{
  uint8_t result = LL_SUCCESS;

  lmgrCb.chanClass = chanMap;

  for (unsigned int i = 0; i < lctrChClassHandlerCnt; i++)
  {
    WSF_ASSERT(lctrChClassHandlerTbl[i]);

    /* Return first error. */
    if (result == LL_SUCCESS)
    {
      result = lctrChClassHandlerTbl[i](chanMap);
    }
    else
    {
      lctrChClassHandlerTbl[i](chanMap);
    }
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief  Get average RSSI of an averaging unit.
 *
 *  \param  pAvg   Average block
 *
 *  \return Average block average.
 */
/*************************************************************************************************/
int8_t lctrRssiGetAverage(lctrRssiRunAvg_t *pAvg)
{
   WSF_ASSERT(pAvg->avgCount >= LL_PC_TBL_LEN);

   int32_t total = 0;
   for (int i = 0; i < LL_PC_TBL_LEN; i++)
   {
     total += (int32_t) pAvg->averageRssi[i];
   }

   return (int8_t) (total >> LL_PC_TBL_POW);
}

/*************************************************************************************************/
/*!
 *  \brief  Add a value to the average block.
 *
 *  \param  pAvg   Average block
 *  \param  value  Value to add
 *
 *  \note   oldest value will be overwritten.
 */
/*************************************************************************************************/
void lctrRssiAddAveragePoint(lctrRssiRunAvg_t *pAvg, int8_t value)
{
  pAvg->averageRssi[pAvg->rssiIdx] = value;
  pAvg->rssiIdx = ((pAvg->rssiIdx + 1) == LL_PC_TBL_LEN) ? 0 : (pAvg->rssiIdx + 1);
  pAvg->avgCount++;
}
