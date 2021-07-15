/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer (LL) DTM interface implementation file.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
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

#include "ll_api.h"
#include "ll_defs.h"
#include "lctr_api.h"
#include "lmgr_api_adv_slave.h"
#include "sch_api.h"
#include "bb_api.h"
#include "bb_ble_api.h"
#include "bb_ble_api_op.h"
#include "pal_bb_ble.h"
#include "wsf_buf.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include <string.h>

/* Access internal definitions. */
#include "../lctr/lctr_int.h"

#if (LL_ENABLE_TESTER)
#include "ll_tester_api.h"
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Maximum DTM payload length. */
#define LL_DTM_MAX_PLD_LEN      (BB_DATA_PLD_MAX_LEN + BB_DATA_PDU_TAILROOM)

/*! \brief      Capable maximum DTM PDU length. */
#define LL_DTM_MAX_PDU_LEN      (LL_DTM_HDR_LEN + LL_DTM_MAX_PLD_LEN)

/*! \brief      Test states. */
enum
{
  LL_TEST_STATE_IDLE  = 0,    /*!< Disabled state */
  LL_TEST_STATE_TX    = 1,    /*!< Tx enabled state */
  LL_TEST_STATE_RX    = 2,    /*!< Rx enabled state */
  LL_TEST_STATE_TERM  = 3,    /*!< Terminating state */
  LL_TEST_STATE_RESET = 4     /*!< Reset state. */
};

/*! \brief      Test task messages for LCTR_DISP_TEST dispatcher. */
enum
{
  /* Broadcast events */
  LL_TEST_MSG_RESET           = LCTR_MSG_RESET,
  /* Test events */
  LL_TEST_MSG_TERMINATE,      /* Test BOD terminated event. */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Test mode control block. */
static struct
{
  uint8_t        state;         /*!< Test mode enable state. */
  uint16_t       numPkt;        /*!< Number of packet operations for auto-termination of tests. */
  LlTestReport_t rpt;           /*!< Test report. */

  struct
  {
    uint8_t  pduLen;            /*!< Transmit PDU buffer length. */
    uint8_t  pktType;           /*!< Transmit packet type. */
    uint8_t  chanIdx;           /*!< Transmit channel. */
    uint8_t  phy;               /*!< Transmit PHY. */
    uint16_t phyOptions;        /*!< Transmit PHY options. */
    uint32_t pktInterUsec;      /*!< Transmit packet interval. */
    uint32_t errPattern;        /*!< Error pattern. */
    uint32_t errBit;            /*!< Error bit. */
  } tx;                         /*!< Transmit parameters. */
} llTestCb;

/*************************************************************************************************/
/*!
 *  \brief      Calculate packet interval.
 *
 *  \param      len         Packet length.
 *  \param      phy         PHY.
 *  \param      phyOptions  PHY options.
 *
 *  \return     Packet length in microseconds.
 */
/*************************************************************************************************/
static uint32_t llCalcPacketInterval(uint8_t len, uint8_t phy, uint8_t phyOptions)
{
  unsigned int totalTime;

  switch (phy)
  {
    case BB_PHY_BLE_CODED:
      totalTime = (LL_PREAMBLE_LEN_CODED_BITS + (LL_AA_LEN * 8) + LL_CI_LEN_BITS + LL_TERM1_LEN_BITS) * LL_BLE_US_PER_BIT_CODED_S8;
      switch (phyOptions)
      {
        case BB_PHY_OPTIONS_BLE_S2:
          totalTime += ((LL_DTM_HDR_LEN + len + LL_CRC_LEN) * LL_BLE_US_PER_BYTE_CODED_S2) + (LL_TERM2_LEN_BITS * LL_BLE_US_PER_BIT_CODED_S2);
          break;
        default:
        case BB_PHY_OPTIONS_BLE_S8:
          totalTime += ((LL_DTM_HDR_LEN + len + LL_CRC_LEN) * LL_BLE_US_PER_BYTE_CODED_S8) + (LL_TERM2_LEN_BITS * LL_BLE_US_PER_BIT_CODED_S8);
          break;
      }
      break;
    case BB_PHY_BLE_2M:
      totalTime = (LL_PREAMBLE_LEN_2M + LL_AA_LEN + LL_DTM_HDR_LEN + len + LL_CRC_LEN) * LL_BLE_US_PER_BYTE_2M;
      break;
    case BB_PHY_BLE_1M:
    default:
      totalTime = (LL_PREAMBLE_LEN_1M + LL_AA_LEN + LL_DTM_HDR_LEN + len + LL_CRC_LEN) * LL_BLE_US_PER_BYTE_1M;
      break;
  }

  /* ceil((t) / 625) * 625 */
  return LL_MATH_DIV_625(totalTime + 624) * 625;
}

/*************************************************************************************************/
/*!
 *  \brief      Convert channel number to channel index.
 *
 *  \param      rfChan      RF channel number.
 *
 *  \return     Channel index.
 */
/*************************************************************************************************/
uint8_t llConvertRfChanToChanIdx(uint8_t rfChan)
{
  if (rfChan == 0)
  {
    return 37;
  }
  else if (rfChan <= 11)
  {
    return rfChan - 1;
  }
  else if (rfChan == 12)
  {
    return 38;
  }
  else if (rfChan <= 38)
  {
    return rfChan - 2;
  }
  else /* if (rfChan == 39) */
  {
    return 39;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Fill buffer with random payload values.
 *
 *  \param      pBuf        Buffer to fill.
 *  \param      len         Number of bytes to fill.
 *
 *  Fill payload with random numbers.
 */
/*************************************************************************************************/
static inline void llPrbs9Payload(uint8_t *pBuf, uint8_t len)
{
  const uint32_t lfsrInit = 0x1FF;
  const uint32_t bitsPerByte = 8;

  unsigned int bit;
  unsigned int bitNum;
  uint32_t lfsr = lfsrInit;

  while (len-- > 0)
  {
    /* Empty buffer (initial value contains first byte). */
    *(pBuf++) = (uint8_t)lfsr;

    /* Calculate next 8 bits. */
    bitNum = bitsPerByte;
    while (bitNum-- > 0)
    {
      /* Cycle PRBS9 sequence; taps are 5th (bit 4) and 9th (bit 0) position. */
      bit  = ((lfsr >> 4) ^ (lfsr >> 0)) & 1;
      lfsr =  (lfsr >> 1) | (bit << 8);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Build transmit buffer.
 *
 *  \param      len         Length of test data.
 *  \param      pktType     Test packet payload type.
 *  \param      pBuf        Tx buffer.
 *
 *  Build DTM packet header and payload buffer.
 */
/*************************************************************************************************/
static void llBuildTxPkt(uint8_t len, uint8_t pktType, uint8_t *pBuf)
{
  /* Fill header. */
  *pBuf++ = pktType;
  *pBuf++ = len;

  /* Fill payload. */
  switch (pktType)
  {
    case LL_TEST_PKT_TYPE_0F:
      memset(pBuf, 0x0F, len);
      break;
    case LL_TEST_PKT_TYPE_55:
      memset(pBuf, 0x55, len);
      break;
    case LL_TEST_PKT_TYPE_FF:
      memset(pBuf, 0xFF, len);
      break;
    case LL_TEST_PKT_TYPE_00:
      memset(pBuf, 0x00, len);
      break;
    case LL_TEST_PKT_TYPE_F0:
      memset(pBuf, 0xF0, len);
      break;
    case LL_TEST_PKT_TYPE_AA:
      memset(pBuf, 0xAA, len);
      break;
    case LL_TEST_PKT_TYPE_PRBS9:
      llPrbs9Payload(pBuf, len);
      break;
    case LL_TEST_PKT_TYPE_PRBS15:
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Tx operation end callback.
 *
 *  \param  pOp     Tx operation descriptor.
 */
/*************************************************************************************************/
static void llTestTxOpEndCback(BbOpDesc_t *pOp)
{
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleTestTx_t * const pTx = &pBle->op.testTx;

  llTestCb.rpt.numTx++;

  if ((llTestCb.numPkt > 0) && (llTestCb.numPkt <= llTestCb.rpt.numTx))
  {
    /* Auto terminate. */
    LlEndTest(NULL);
  }

  if (llTestCb.state == LL_TEST_STATE_TX)
  {
    /* Check for changes in operating parameters. */
    if ((llTestCb.tx.chanIdx    != pBle->chan.chanIdx) ||
        (llTestCb.tx.pktType    != pTx->pTxBuf[0]) ||
        (llTestCb.tx.pduLen     != pTx->pTxBuf[1]) ||
        (llTestCb.tx.phy        != pBle->chan.txPhy) ||
        (llTestCb.tx.phyOptions != pBle->chan.initTxPhyOptions))
    {
      /* Update operational parameters. */
      pBle->chan.chanIdx          = llTestCb.tx.chanIdx;
      pBle->chan.rxPhy            = llTestCb.tx.phy;
      pBle->chan.txPhy            = llTestCb.tx.phy;
      pBle->chan.initTxPhyOptions = llTestCb.tx.phyOptions;
      pBle->chan.tifsTxPhyOptions = 0; /* Field not used. */
      pTx->txLen                  = LL_DTM_HDR_LEN + llTestCb.tx.pduLen;
      pTx->pktInterUsec           = llCalcPacketInterval(llTestCb.tx.pduLen, pBle->chan.txPhy, pBle->chan.initTxPhyOptions);

      /* Build packet buffer. */
      llBuildTxPkt(llTestCb.tx.pduLen, llTestCb.tx.pktType, pTx->pTxBuf);
    }

    /* Reschedule transmit. */
    SchInsertNextAvailable(pOp);
  }
  else
  {
    WsfBufFree(pTx->pTxBuf);
    WsfBufFree(pBle);
    WsfBufFree(pOp);

    BbStop(BB_PROT_BLE_DTM);

    if (llTestCb.state == LL_TEST_STATE_RESET)
    {
      lctrMsgHdr_t *pMsg;

      /* Send SM an test termination event. */
      if ((pMsg = (lctrMsgHdr_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        /* pMsg->handle = 0; */           /* not used */
        pMsg->dispId = LCTR_DISP_TEST;
        pMsg->event  = LL_TEST_MSG_TERMINATE;

        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
      }
    }
    else
    {
      /* Terminate immediately. */
      llTestCb.state = LL_TEST_STATE_IDLE;
      lmgrCb.testEnabled = FALSE;
      LmgrDecResetRefCount();
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Complete a receive.
 *
 *  \param      pOp     Operation context.
 *  \param      status  Receive status.
 *
 *  \return     TRUE if next receive should be set up.
 */
/*************************************************************************************************/
static bool_t llTestTxComplete(BbOpDesc_t *pOp, uint8_t status)
{
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleTestTx_t * const pTx = &pBle->op.testTx;

  llTestCb.rpt.numTx++;

  /* All of the requested packets have been sent. */
  if ((llTestCb.numPkt > 0) && (llTestCb.numPkt <= llTestCb.rpt.numTx))
  {
    return FALSE;
  }

  if (status != BB_STATUS_SUCCESS)
  {
    LL_TRACE_ERR2("Terminating Tx test mode due to failure, status=%u, numTx=%u", status, llTestCb.rpt.numTx);
    return FALSE;
  }

  if (llTestCb.state == LL_TEST_STATE_TX)
  {
    /* Operating parameters have changed. */
    if ((llTestCb.tx.chanIdx    != pBle->chan.chanIdx) ||
        (llTestCb.tx.pktType    != pTx->pTxBuf[0]) ||
        (llTestCb.tx.pduLen     != pTx->pTxBuf[1]) ||
        (llTestCb.tx.phy        != pBle->chan.txPhy))
    {
      return FALSE;
    }

    /* Update CRC for PER testing. */
    bool_t err = ((llTestCb.tx.errPattern & (1 << llTestCb.tx.errBit)) != 0) ? FALSE : TRUE;
    llTestCb.tx.errBit = (llTestCb.tx.errBit + 1) & (32 - 1);
    pBle->chan.crcInit = err ? (LL_DTM_CRC_INIT ^ 0xFFFFFF) : LL_DTM_CRC_INIT;
#if (LL_ENABLE_TESTER)
    pBle->chan.crcInitTx = pBle->chan.crcInit ^ llTesterCb.dataCrcInitTx;
#endif
  }
  else
  {
    /* DTM has ended. */
    return FALSE;
  }

  /* Continue transmitting next packet. */
  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Enter enhanced transmit test mode.
 *
 *  \param      rfChan      RF channel number, i.e. "rfChan = (F - 2402) / 2)".
 *  \param      len         Length of test data.
 *  \param      pktType     Test packet payload type.
 *  \param      phy         Transmitter PHY.
 *  \param      numPkt      Auto terminate after number of packets, 0 for infinite.
 *
 *  \return     Status error code.
 *
 *  Start the transmit test mode on the given channel.
 */
/*************************************************************************************************/
uint8_t LlEnhancedTxTest(uint8_t rfChan, uint8_t len, uint8_t pktType, uint8_t phy, uint16_t numPkt)
{
  WSF_CS_INIT(cs);

  LL_TRACE_INFO3("### LlApi ###  LlTxTest, rfChan=%u, len=%u, pktType=%u", rfChan, len, pktType);

  if ((LL_API_PARAM_CHECK == 1) &&
      (!((llTestCb.state == LL_TEST_STATE_IDLE) || (llTestCb.state == LL_TEST_STATE_TX)) ||
       (lmgrCb.numConnEnabled > 0) ||
       lmgrCb.advEnabled ||
       lmgrCb.numExtAdvEnabled ||
       lmgrCb.numScanEnabled ||
       lmgrCb.numInitEnabled))
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  /* Extra parameter checks for dynamic changes. */
  if ((LL_API_PARAM_CHECK == 1) &&
      (llTestCb.state == LL_TEST_STATE_TX))
  {
    /* Do not allow transition from CBS to packet mode dynamically or vice-versa. */
    if (((pktType == LL_TEST_PKT_TYPE_PRBS15) && (llTestCb.tx.pktType != LL_TEST_PKT_TYPE_PRBS15)) ||
        ((pktType != LL_TEST_PKT_TYPE_PRBS15) && (llTestCb.tx.pktType == LL_TEST_PKT_TYPE_PRBS15)))
    {
      return LL_ERROR_CODE_CMD_DISALLOWED;
    }
  }

  if ((LL_API_PARAM_CHECK == 1) &&
     ((rfChan > LL_DTM_MAX_CHAN_IDX) ||
      /* (len > LL_DTM_PDU_ABS_MAX_LEN) || */   /* always valid since len is uint8_t. */
      (pktType > LL_TEST_PKT_TYPE_AA) ||
      ((phy < LL_TEST_PHY_LE_1M) || (phy > LL_TEST_PHY_LE_CODED_S2))))
  {
    return LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE;
  }

  /* Check whether PHY is supported. */
  switch (phy)
  {
    case LL_TEST_PHY_LE_2M:
      if ((lmgrCb.features & LL_FEAT_LE_2M_PHY) == 0)
      {
        return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
      }
      break;
    case LL_TEST_PHY_LE_CODED_S2:
    case LL_TEST_PHY_LE_CODED_S8:
      if ((lmgrCb.features & LL_FEAT_LE_CODED_PHY) == 0)
      {
        return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
      }
      break;
  }

  if (llTestCb.state == LL_TEST_STATE_IDLE)
  {
    /* Init test state. */
    memset(&llTestCb.rpt, 0, sizeof(llTestCb.rpt));
  }

  /* Handle non-packet test mode. */
  if (pktType == LL_TEST_PKT_TYPE_PRBS15)
  {
    llTestCb.state = LL_TEST_STATE_TX;
    llTestCb.tx.pktType = pktType;

    BbStart(BB_PROT_PRBS15);

    lmgrCb.testEnabled = TRUE;
    LmgrIncResetRefCount();

    return LL_SUCCESS;
  }

  BbOpDesc_t *pOp;
  BbBleData_t *pBle;

  /* Restrict length to maximum. */
  len = WSF_MIN(len, WSF_MAX(LL_ADVB_MAX_LEN - LL_ADV_HDR_LEN, LL_DTM_MAX_PLD_LEN));

  /* Modify operational parameters. */
  WSF_CS_ENTER(cs);
  llTestCb.tx.pduLen  = WSF_MIN(len, WSF_MAX(LL_ADVB_MAX_LEN - LL_ADV_HDR_LEN, LL_DTM_MAX_PLD_LEN));
  llTestCb.tx.pktType = pktType;
  llTestCb.tx.chanIdx = llConvertRfChanToChanIdx(rfChan);
  switch (phy)
  {
    case LL_TEST_PHY_LE_1M:
      llTestCb.tx.phy        = BB_PHY_BLE_1M;
      llTestCb.tx.phyOptions = BB_PHY_OPTIONS_DEFAULT;
      break;
    case LL_TEST_PHY_LE_2M:
      llTestCb.tx.phy        = BB_PHY_BLE_2M;
      llTestCb.tx.phyOptions = BB_PHY_OPTIONS_DEFAULT;
      break;
    case LL_TEST_PHY_LE_CODED_S2:
      llTestCb.tx.phy        = BB_PHY_BLE_CODED;
      llTestCb.tx.phyOptions = BB_PHY_OPTIONS_BLE_S2;
      break;
    case LL_TEST_PHY_LE_CODED_S8:
      llTestCb.tx.phy        = BB_PHY_BLE_CODED;
      llTestCb.tx.phyOptions = BB_PHY_OPTIONS_BLE_S8;
      break;
  }

  llTestCb.tx.errPattern = 0xFFFFFFFF;
  llTestCb.tx.errBit     = 0;
  WSF_CS_EXIT(cs);

  if (llTestCb.state == LL_TEST_STATE_TX)
  {
    return LL_SUCCESS;
  }

  if ((pOp = WsfBufAlloc(sizeof(BbOpDesc_t))) == NULL)
  {
    return LL_ERROR_CODE_UNSPECIFIED_ERROR;
  }

  if ((pBle = WsfBufAlloc(sizeof(BbBleData_t))) == NULL)
  {
    WsfBufFree(pOp);
    return LL_ERROR_CODE_UNSPECIFIED_ERROR;
  }

  BbBleTestTx_t * const pTx = &pBle->op.testTx;

  memset(pOp, 0, sizeof(*pOp));
  memset(pBle, 0, sizeof(*pBle));
  pOp->prot.pBle = pBle;

  /*** General Setup ***/

  llTestCb.numPkt = numPkt;
  pOp->protId = BB_PROT_BLE_DTM;
  pOp->endCback = llTestTxOpEndCback;

  /*** BLE General Setup ***/

  pBle->chan.opType           = BB_BLE_OP_TEST_TX;
  pBle->chan.chanIdx          = llTestCb.tx.chanIdx;
  pBle->chan.accAddr          = LL_DTM_SYNC_WORD;
  pBle->chan.crcInit          = LL_DTM_CRC_INIT;
  pBle->chan.txPower          = lmgrCb.advTxPwr;
  pBle->chan.rxPhy            = llTestCb.tx.phy;
  pBle->chan.initTxPhyOptions = llTestCb.tx.phyOptions;
  /* pBle->chan.tifsTxPhyOptions = 0; */ /* Field not used. */
  pBle->chan.txPhy            = pBle->chan.rxPhy;

#if (LL_ENABLE_TESTER)
  pBle->chan.accAddrTx = LL_DTM_SYNC_WORD ^ llTesterCb.dataAccessAddrTx;
  pBle->chan.crcInitTx = LL_DTM_CRC_INIT  ^ llTesterCb.dataCrcInitTx;
#endif

  /*** BLE Tx Test Setup ***/

  /* Longest packet for dynamic test changes. */
  pTx->txLen        = WSF_MAX(LL_DTM_MAX_PDU_LEN, LL_ADVB_MAX_LEN);
  pTx->testCback    = llTestTxComplete;
  pTx->pktInterUsec = llCalcPacketInterval(llTestCb.tx.pduLen, pBle->chan.txPhy, pBle->chan.initTxPhyOptions);

  if ((pTx->pTxBuf = WsfBufAlloc(pTx->txLen)) == NULL)
  {
    WsfBufFree(pBle);
    WsfBufFree(pOp);
    return LL_ERROR_CODE_UNSPECIFIED_ERROR;
  }

  llBuildTxPkt(len, pktType, pTx->pTxBuf);
  llTestCb.tx.pktInterUsec = pTx->pktInterUsec;

  /*** Commit operation ***/

  llTestCb.state = LL_TEST_STATE_TX;
  lmgrCb.testEnabled = TRUE;
  LmgrIncResetRefCount();
  BbStart(BB_PROT_BLE_DTM);
  SchInsertNextAvailable(pOp);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Enter transmit test mode.
 *
 *  \param      rfChan      RF channel number, i.e. "rfChan = (F - 2402) / 2)".
 *  \param      len         Length of test data.
 *  \param      pktType     Test packet payload type.
 *  \param      numPkt      Auto terminate after number of packets, 0 for infinite.
 *
 *  \return     Status error code.
 *
 *  Start the transmit test mode on the given channel.
 */
/*************************************************************************************************/
uint8_t LlTxTest(uint8_t rfChan, uint8_t len, uint8_t pktType, uint16_t numPkt)
{
  return LlEnhancedTxTest(rfChan, len, pktType, LL_TEST_PHY_LE_1M, numPkt);
}

/*************************************************************************************************/
/*!
 *  \brief  Rx operation completion callback.
 *
 *  \param  pOp     Rx operation descriptor.
 */
/*************************************************************************************************/
static void llTestRxOpEndCback(BbOpDesc_t *pOp)
{
  BbBleData_t * const pBle = pOp->prot.pBle;
  BbBleTestRx_t * const pRx = &pBle->op.testRx;

  if (llTestCb.numPkt > 0)
  {
    uint32_t attempts = llTestCb.rpt.numRxSuccess +
                        llTestCb.rpt.numRxCrcError +
                        llTestCb.rpt.numRxTimeout;

    if (llTestCb.numPkt <= attempts)
    {
      /* Auto terminate. */
      LlEndTest(NULL);
    }
  }

  if (llTestCb.state == LL_TEST_STATE_RX)
  {
    /* Reschedule receive. */
    SchInsertNextAvailable(pOp);
  }
  else
  {
    WsfBufFree(pRx->pRxBuf);
    WsfBufFree(pBle);
    WsfBufFree(pOp);

    BbStop(BB_PROT_BLE_DTM);

    if (llTestCb.state == LL_TEST_STATE_RESET)
    {
      lctrMsgHdr_t *pMsg;

      /* Send SM an test termination event. */
      if ((pMsg = (lctrMsgHdr_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        /* pMsg->handle = 0; */           /* not used */
        pMsg->dispId = LCTR_DISP_TEST;
        pMsg->event  = LL_TEST_MSG_TERMINATE;

        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
      }
    }
    else
    {
      /* Terminate immediately. */
      llTestCb.state = LL_TEST_STATE_IDLE;
      lmgrCb.testEnabled = FALSE;
      LmgrDecResetRefCount();
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Complete a receive.
 *
 *  \param      pBod    Pointer to the BOD to execute.
 *  \param      status  Receive status.
 *
 *  \return     TRUE if next receive should be set up.
 */
/*************************************************************************************************/
static bool_t llTestRxComplete(BbOpDesc_t *pBod, uint8_t status)
{
  switch (status)
  {
    case BB_STATUS_SUCCESS:
      llTestCb.rpt.numRxSuccess++;
      break;
    case BB_STATUS_CRC_FAILED:
      llTestCb.rpt.numRxCrcError++;
      break;
    case BB_STATUS_RX_TIMEOUT:
    default:
      llTestCb.rpt.numRxTimeout++;
      break;
  }

  /* All of the requested packets have been received. */
  if (llTestCb.numPkt > 0)
  {
    uint32_t attempts = llTestCb.rpt.numRxSuccess +
                        llTestCb.rpt.numRxCrcError +
                        llTestCb.rpt.numRxTimeout;

    if (llTestCb.numPkt <= attempts)
    {
      return FALSE;
    }
  }

  /* DTM has ended. */
  if (llTestCb.state != LL_TEST_STATE_RX)
  {
    return FALSE;
  }

  /* Continue receiving next packet. */
  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Enter enhanced receive test mode.
 *
 *  \param      rfChan      RF channel number, i.e. "rfChan = (F - 2402) / 2)".
 *  \param      phy         Receiver PHY.
 *  \param      modIdx      Modulation index.
 *  \param      numPkt      Auto terminate after number of successful packets, 0 for infinite.
 *
 *  \return     Status error code.
 *
 *  Start the receive test mode on the given channel.
 */
/*************************************************************************************************/
uint8_t LlEnhancedRxTest(uint8_t rfChan, uint8_t phy, uint8_t modIdx, uint16_t numPkt)
{
  LL_TRACE_INFO1("### LlApi ###  LlRxTest, rfChan=%u", rfChan);

  if ((LL_API_PARAM_CHECK == 1) &&
      ((llTestCb.state != LL_TEST_STATE_IDLE) ||
      (lmgrCb.numConnEnabled > 0) ||
      lmgrCb.advEnabled ||
      lmgrCb.numExtAdvEnabled ||
      lmgrCb.numScanEnabled ||
      lmgrCb.numInitEnabled))
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
     ((rfChan > LL_DTM_MAX_CHAN_IDX) ||
      ((phy < LL_TEST_PHY_LE_1M) || (phy > LL_TEST_PHY_LE_CODED)) ||
      (modIdx > LL_TEST_MOD_IDX_STABLE)))
  {
    return LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE;
  }

  /* Check whether PHY is supported. */
  switch (phy)
  {
    case LL_TEST_PHY_LE_2M:
      if ((lmgrCb.features & LL_FEAT_LE_2M_PHY) == 0)
      {
        return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
      }
      break;
    case LL_TEST_PHY_LE_CODED:
      if ((lmgrCb.features & LL_FEAT_LE_CODED_PHY) == 0)
      {
        return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
      }
      break;
  }

  BbOpDesc_t *pOp;
  BbBleData_t *pBle;

  if ((pOp = WsfBufAlloc(sizeof(BbOpDesc_t))) == NULL)
  {
    return LL_ERROR_CODE_UNSPECIFIED_ERROR;
  }

  if ((pBle = WsfBufAlloc(sizeof(BbBleData_t))) == NULL)
  {
    WsfBufFree(pOp);
    return LL_ERROR_CODE_UNSPECIFIED_ERROR;
  }

  BbBleTestRx_t * const pRx = &pBle->op.testRx;

  memset(pOp, 0, sizeof(*pOp));
  memset(pBle, 0, sizeof(*pBle));
  pOp->prot.pBle = pBle;

  /*** General Setup ***/

  llTestCb.numPkt = numPkt;
  pOp->protId = BB_PROT_BLE_DTM;
  pOp->endCback = llTestRxOpEndCback;

  /*** BLE General Setup ***/

  pBle->chan.opType  = BB_BLE_OP_TEST_RX;
  pBle->chan.chanIdx = llConvertRfChanToChanIdx(rfChan);
  pBle->chan.accAddr = LL_DTM_SYNC_WORD;
  pBle->chan.crcInit = LL_DTM_CRC_INIT;
  /* pBle->txPwrLevel = 0; */           /* value ignored */
  switch (phy)
  {
    case LL_TEST_PHY_LE_1M:
      pBle->chan.rxPhy = BB_PHY_BLE_1M;
      break;
    case LL_TEST_PHY_LE_2M:
      pBle->chan.rxPhy = BB_PHY_BLE_2M;
      break;
    case LL_TEST_PHY_LE_CODED:
      pBle->chan.rxPhy = BB_PHY_BLE_CODED;
      break;
  }
  pBle->chan.txPhy = pBle->chan.rxPhy;

#if (LL_ENABLE_TESTER)
  pBle->chan.accAddrRx = LL_DTM_SYNC_WORD ^ llTesterCb.dataAccessAddrRx;
  pBle->chan.crcInitRx = LL_DTM_CRC_INIT  ^ llTesterCb.dataCrcInitRx;
#endif

  /*** BLE Rx Test Setup ***/

  pRx->rxSyncDelayUsec = pLctrRtCfg->dtmRxSyncMs * 1000;
  pRx->rxLen = WSF_MAX(LL_DTM_MAX_PDU_LEN, LL_ADVB_MAX_LEN);
  pRx->testCback = llTestRxComplete;

  uint16_t allocLen = WSF_MAX(pRx->rxLen, BB_FIXED_DATA_PKT_LEN);
  if ((pRx->pRxBuf = WsfBufAlloc(allocLen)) == NULL)
  {
    WsfBufFree(pBle);
    WsfBufFree(pOp);
    return LL_ERROR_CODE_UNSPECIFIED_ERROR;
  }

  /*** Commit operation ***/

  llTestCb.state = LL_TEST_STATE_RX;
  lmgrCb.testEnabled = TRUE;
  LmgrIncResetRefCount();
  memset(&llTestCb.rpt, 0, sizeof(llTestCb.rpt));   /* clear report */
  BbStart(BB_PROT_BLE_DTM);
  SchInsertNextAvailable(pOp);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Enter receive test mode.
 *
 *  \param      rfChan      RF channel number, i.e. "rfChan = (F - 2402) / 2)".
 *  \param      numPkt      Auto terminate after number of successful packets, 0 for infinite.
 *
 *  \return     Status error code.
 *
 *  Start the receive test mode on the given channel.
 */
/*************************************************************************************************/
uint8_t LlRxTest(uint8_t rfChan, uint16_t numPkt)
{
  return LlEnhancedRxTest(rfChan, LL_TEST_PHY_LE_1M, LL_TEST_MOD_IDX_STANDARD, numPkt);
}

/*************************************************************************************************/
/*!
 *  \brief      End test mode.
 *
 *  \param      pRpt        Report return buffer.
 *
 *  \return     Status error code.
 *
 *  End test mode and return the report.
 *
 *  \note       Receive tests must wait for the duration of the synchronization timeout before
 *              another test can be issued.
 */
/*************************************************************************************************/
uint8_t LlEndTest(LlTestReport_t *pRpt)
{
  LL_TRACE_INFO0("### LlApi ###  LlEndTest");

  if ((llTestCb.state == LL_TEST_STATE_TX) &&
      (llTestCb.tx.pktType == LL_TEST_PKT_TYPE_PRBS15))
  {
    BbStop(BB_PROT_PRBS15);
    llTestCb.state = LL_TEST_STATE_IDLE;
    lmgrCb.testEnabled = FALSE;
    LmgrDecResetRefCount();

    return LL_SUCCESS;
  }

  if (llTestCb.state == LL_TEST_STATE_TX)
  {
    /* Signal termination. */
    llTestCb.state = LL_TEST_STATE_TERM;
  }
  else if (llTestCb.state == LL_TEST_STATE_RX)
  {
    /* Signal termination. */
    BbCancelBod();
    llTestCb.state = LL_TEST_STATE_TERM;
  }
  else
  {
    LL_TRACE_WARN1("Test not running, state=%u", llTestCb.state);
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (pRpt)
  {
    *pRpt = llTestCb.rpt;
  }

  LL_TRACE_INFO1("Test completed, numTx=%u", llTestCb.rpt.numTx);
  LL_TRACE_INFO1("                numRxSuccess=%u", llTestCb.rpt.numRxSuccess);
  LL_TRACE_INFO1("                numRxCrcError=%u", llTestCb.rpt.numRxCrcError);
  LL_TRACE_INFO1("                numRxTimeout=%u", llTestCb.rpt.numRxTimeout);

  memset(&llTestCb.rpt, 0, sizeof(llTestCb.rpt));   /* clear report */

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set pattern of errors for Tx test mode.
 *
 *  \param      pattern   Error pattern (1s = no error; 0s = CRC failure).
 *
 *  \return     Status error code.
 *
 *  Set pattern of errors for Tx test mode.
 *
 *  \note       The error pattern must be set after the Tx test is started.
 */
/*************************************************************************************************/
uint8_t LlSetTxTestErrorPattern(uint32_t pattern)
{
  llTestCb.tx.errPattern = pattern;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Test reset handler.
 */
/*************************************************************************************************/
static void llTestResetHandler(void)
{
  memset(&llTestCb, 0, sizeof(llTestCb));
  llTestCb.tx.errPattern = 0xFFFFFFFF;
}

/*************************************************************************************************/
/*!
 *  \brief      Test message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void llTestDisp(lctrMsgHdr_t *pMsg)
{
  switch (pMsg->event)
  {
    case LL_TEST_MSG_RESET:
      if (lmgrCb.testEnabled)
      {
        LlEndTest(NULL);
        if (llTestCb.state == LL_TEST_STATE_TERM)
        {
          /* Terminate will send confirm. */
          llTestCb.state = LL_TEST_STATE_RESET;
        }
      }
      break;

    case LL_TEST_MSG_TERMINATE:
      llTestCb.state = LL_TEST_STATE_IDLE;
      lmgrCb.testEnabled = FALSE;
      LmgrDecResetRefCount();
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for test.
 */
/*************************************************************************************************/
void LlTestInit(void)
{
  llTestResetHandler();

  /* Add test reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_TEST] = llTestResetHandler;

  /* Add test message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_TEST] = (LctrMsgDisp_t)llTestDisp;

  BbBleTestInit();
}
