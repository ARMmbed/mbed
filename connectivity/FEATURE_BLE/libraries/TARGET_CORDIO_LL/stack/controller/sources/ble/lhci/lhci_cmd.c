/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI command module implementation file.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
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

#include "lhci_int.h"
#include "hci_defs.h"
#include "pal_bb_ble.h"
#include "ll_api.h"
#include "ll_defs.h"
#include "ll_math.h"
#include "wsf_msg.h"
#include "util/bstream.h"
#include "lmgr_api.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief  Unpack a set event mask command packet (either general and LE event masks).
 *
 *  \param  pEvtMsk     Event mask.
 *  \param  pBuf        Packed packet buffer.
 *
 *  \return Payload length.
 */
/*************************************************************************************************/
static uint8_t lhciUnpackSetEventMaskCmd(uint64_t *pEvtMsk, const uint8_t *pBuf)
{
  const uint8_t len = sizeof(uint64_t);

  BSTREAM_TO_UINT64(*pEvtMsk, pBuf);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a read buffer size event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  status      Completion status.
 *  \param  pktLen      Data packet length.
 *  \param  numPkts     Number of controller data packet buffers.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackReadBufSizeEvt(uint8_t *pBuf, uint8_t status, uint16_t pktLen, uint8_t numPkts)
{
  const uint8_t len = LHCI_LEN_LE_READ_BUF_SIZE_EVT;

  UINT8_TO_BSTREAM (pBuf, status);
  UINT16_TO_BSTREAM(pBuf, pktLen);
  UINT8_TO_BSTREAM (pBuf, numPkts);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a local version info event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  status      Completion status.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackLocalVersionInfo(uint8_t *pBuf, uint8_t status)
{
  const uint8_t len = LHCI_LEN_READ_LOCAL_VER_EVT;

  uint16_t compId;
  uint8_t btVer;
  uint16_t implRev;

  LlGetVersion(&compId, &btVer, &implRev);

  UINT8_TO_BSTREAM (pBuf, status);
  UINT8_TO_BSTREAM (pBuf, btVer);           /* HCI_Version */
  UINT16_TO_BSTREAM(pBuf, implRev);         /* HCI_Revision */
  UINT8_TO_BSTREAM (pBuf, btVer);           /* LMP_Version */
  UINT16_TO_BSTREAM(pBuf, compId);          /* Manufacturer_Name */
  UINT16_TO_BSTREAM(pBuf, implRev);         /* LMP_Subversion */

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a local supported commands event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  status      Completion status.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackLocalSupCmds(uint8_t *pBuf, uint8_t status)
{
  const uint8_t len = LHCI_LEN_READ_LOCAL_SUP_CMDS_EVT;

  memset(pBuf, 0, len);

  UINT8_TO_BSTREAM (pBuf, status);
  memcpy(pBuf, lhciPersistCb.supCmds, HCI_SUP_CMD_LEN);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a local supported features event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  status      Completion status.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackLocalSupFeat(uint8_t *pBuf, uint8_t status)
{
  const uint8_t len = LHCI_LEN_LE_READ_LOCAL_SUP_FEAT_EVT;

  UINT8_TO_BSTREAM (pBuf, status);
  UINT64_TO_BSTREAM(pBuf, LHCI_LOCAL_SUP_FEAT_VAL);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Build and send command status event packet.
 *
 *  \param  pCmdHdr     Command HCI header.
 *  \param  status      Status value.
 */
/*************************************************************************************************/
void lhciSendCmdStatusEvt(LhciHdr_t *pCmdHdr, uint8_t status)
{
  uint8_t *pEvtBuf;

  if ((pEvtBuf = lhciAllocEvt(HCI_CMD_STATUS_EVT, HCI_LEN_CMD_STATUS)) == NULL)
  {
    return;
  }

  lhciPackCmdStatusEvt(pEvtBuf, status, pCmdHdr->opCode);
  lhciSendEvt(pEvtBuf);
}

/*************************************************************************************************/
/*!
 *  \brief  Build and send a command complete event packet.
 *
 *  \param  pCmdHdr     Command HCI header.
 *  \param  status      Status value.
 *  \param  paramLen    Parameter length of the command status event.
 */
/*************************************************************************************************/
static void lhciCommonSendCmdCmplEvt(LhciHdr_t *pCmdHdr, uint8_t status, uint8_t paramLen)
{
  uint8_t *pBuf;
  uint8_t *pEvtBuf;

  if ((pEvtBuf = lhciAllocCmdCmplEvt(paramLen, pCmdHdr->opCode)) == NULL)
  {
    return;
  }
  pBuf = pEvtBuf;

  switch (pCmdHdr->opCode)
  {
    /* --- command completion with status only parameter --- */

    case HCI_OPCODE_SET_EVENT_MASK:
    case HCI_OPCODE_SET_EVENT_MASK_PAGE2:
    case HCI_OPCODE_LE_SET_EVENT_MASK:
    case HCI_OPCODE_LE_SET_RAND_ADDR:
    case HCI_OPCODE_LE_CLEAR_WHITE_LIST:
    case HCI_OPCODE_LE_ADD_DEV_WHITE_LIST:
    case HCI_OPCODE_LE_REMOVE_DEV_WHITE_LIST:
    case HCI_OPCODE_LE_WRITE_RF_PATH_COMP:
    case HCI_OPCODE_NOP:
    case HCI_OPCODE_LE_MODIFY_SLEEP_CLK_ACC:
    case HCI_OPCODE_LE_SET_HOST_FEATURE:
    case HCI_OPCODE_WRITE_AFH_CHAN_ASSESSMENT_MODE:
      lhciPackCmdCompleteEvtStatus(pBuf, status);
      break;

    /* --- status --- */

    case HCI_OPCODE_LE_RAND:
    {
      const uint8_t size = sizeof(uint64_t) / sizeof(uint8_t);

      pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);
      PalCryptoGenerateRandomNumber(pBuf, size);
      /* pBuf += size; */
      break;
    }
    /* --- device management --- */

    case HCI_OPCODE_LE_READ_WHITE_LIST_SIZE:
      pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);
      *pBuf = LlGetWhitelistSize();
      break;
    case HCI_OPCODE_READ_LOCAL_VER_INFO:
      lhciPackLocalVersionInfo(pBuf, status);
      break;
    case HCI_OPCODE_READ_LOCAL_SUP_CMDS:
      lhciPackLocalSupCmds(pBuf, status);
      break;
    case HCI_OPCODE_READ_LOCAL_SUP_FEAT:
      lhciPackLocalSupFeat(pBuf, status);
      break;
    case HCI_OPCODE_READ_BD_ADDR:
      pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);
      LlGetBdAddr(pBuf);
      break;
    case HCI_OPCODE_LE_READ_BUF_SIZE:
      lhciPackReadBufSizeEvt(pBuf, status, LlGetAclMaxSize(), LlGetAclTxBufs());
      break;
    case HCI_OPCODE_LE_READ_LOCAL_SUP_FEAT:
      pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);
      LlGetFeatures(pBuf);
      break;
    case HCI_OPCODE_LE_READ_SUP_STATES:
      pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);
      LlGetSupStates(pBuf);
      break;
    case HCI_OPCODE_LE_READ_TX_POWER:
      pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);
      LlReadSupTxPower((int8_t *)&pBuf[0], (int8_t *)&pBuf[1]);
      break;
    case HCI_OPCODE_LE_READ_RF_PATH_COMP:
    {
      int16_t txPathComp;
      int16_t rxPathComp;
      LlReadRfPathComp(&txPathComp, &rxPathComp);
      pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);
      UINT16_TO_BSTREAM(pBuf, txPathComp);
      UINT16_TO_BSTREAM(pBuf, rxPathComp);
      break;
    }
    case HCI_OPCODE_READ_AFH_CHAN_ASSESMENT_MODE:
    {
      pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);
      UINT8_TO_BSTREAM(pBuf, lhciCb.chanAssesmentMode);
      break;
    }

    /* --- test --- */

    case HCI_OPCODE_LE_RECEIVER_TEST:
      lhciPackCmdCompleteEvtStatus(pBuf, status);
      break;
    case HCI_OPCODE_LE_TRANSMITTER_TEST:
      lhciPackCmdCompleteEvtStatus(pBuf, status);
      break;
    case HCI_OPCODE_LE_TEST_END:
    {
      LlTestReport_t rpt = {0};
      status = LlEndTest(&rpt);
      pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);
      UINT16_TO_BSTREAM(pBuf, rpt.numRxSuccess);
      break;
    }

    /* --- default --- */

    default:
      break;
  }

  lhciSendCmdCmplEvt(pEvtBuf);
}

/*************************************************************************************************/
/*!
 *  \brief  Decode an HCI command packet.
 *
 *  \param  pHdr    Decoded packet header.
 *  \param  pBuf    Packed HCI packet buffer.
 *
 *  Command processing is organized by urgency then by frequency of command calls during normal
 *  operation. The following grouping is used:
 *      - status
 *      - local device management
 *      - test
 *      - default/unused
 *
 *  \return TRUE if opcode handled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lhciCommonDecodeCmdPkt(LhciHdr_t *pHdr, uint8_t *pBuf)
{
#if LHCI_ENABLE_VS
  if (lhciCommonVsStdDecodeCmdPkt(pHdr, pBuf))
  {
    return TRUE;
  }
#endif

  uint8_t status = HCI_SUCCESS;
  uint8_t paramLen = 0;

  switch (pHdr->opCode)
  {
    /* --- status --- */

    case HCI_OPCODE_LE_RAND:
      paramLen = LHCI_LEN_LE_RAND_EVT;
      break;

    /* --- local device management --- */

    case HCI_OPCODE_LE_SET_RAND_ADDR:
      status = LlSetRandAddr(pBuf);
      paramLen = LHCI_LEN_LE_SET_RAND_ADDR_EVT;
      break;
    case HCI_OPCODE_LE_READ_WHITE_LIST_SIZE:
      paramLen = LHCI_LEN_LE_READ_WL_SIZE_EVT;
      break;
    case HCI_OPCODE_LE_CLEAR_WHITE_LIST:
      status = LlClearWhitelist();
      paramLen = LHCI_LEN_LE_CLEAR_WHITE_LIST_EVT;
      break;
    case HCI_OPCODE_LE_ADD_DEV_WHITE_LIST:
      status = LlAddDeviceToWhitelist(*pBuf, pBuf + 1);
      paramLen = LHCI_LEN_LE_ADD_DEV_WHITE_LIST_EVT;
      break;
    case HCI_OPCODE_LE_REMOVE_DEV_WHITE_LIST:
      status = LlRemoveDeviceFromWhitelist(*pBuf, pBuf + 1);
      paramLen = LHCI_LEN_LE_REMOVE_DEV_WHITE_LIST_EVT;
      break;
    case HCI_OPCODE_SET_EVENT_MASK:
      lhciUnpackSetEventMaskCmd(&lhciCb.evtMsk, pBuf);
      paramLen = LHCI_LEN_SET_EVENT_MASK_EVT;
      break;
    case HCI_OPCODE_SET_EVENT_MASK_PAGE2:
      lhciUnpackSetEventMaskCmd(&lhciCb.evtMskPg2, pBuf);
      paramLen = LHCI_LEN_SET_EVENT_MASK_EVT;
      break;
    case HCI_OPCODE_READ_LOCAL_VER_INFO:
      paramLen = LHCI_LEN_READ_LOCAL_VER_EVT;
      break;
    case HCI_OPCODE_READ_LOCAL_SUP_CMDS:
      paramLen = LHCI_LEN_READ_LOCAL_SUP_CMDS_EVT;
      break;
    case HCI_OPCODE_READ_LOCAL_SUP_FEAT:
      paramLen = LHCI_LEN_READ_LOCAL_SUP_FEAT_EVT;
      break;
    case HCI_OPCODE_READ_BD_ADDR:
      paramLen = LHCI_LEN_READ_BD_ADDR_EVT;
      break;
    case HCI_OPCODE_LE_SET_EVENT_MASK:
      lhciUnpackSetEventMaskCmd(&lhciCb.leEvtMsk, pBuf);
      paramLen = LHCI_LEN_LE_SET_EVENT_MASK_EVT;
      break;
    case HCI_OPCODE_LE_READ_BUF_SIZE:
      paramLen = LHCI_LEN_LE_READ_BUF_SIZE_EVT;
      break;
    case HCI_OPCODE_LE_READ_LOCAL_SUP_FEAT:
      paramLen = LHCI_LEN_LE_READ_LOCAL_SUP_FEAT_EVT;
      break;
    case HCI_OPCODE_LE_READ_SUP_STATES:
      paramLen = LHCI_LEN_LE_READ_SUP_STATES_EVT;
      break;
    case HCI_OPCODE_LE_READ_TX_POWER:
      paramLen = LHCI_LEN_LE_READ_SUP_TX_POWER;
      break;
    case HCI_OPCODE_LE_SET_HOST_FEATURE:
      status = LlSetHostFeatures(*pBuf, *(pBuf + 1));
      paramLen = LHCI_LEN_LE_SET_HOST_FEATURE;
      break;
    case HCI_OPCODE_LE_WRITE_RF_PATH_COMP:
    {
      int16_t txPathComp;
      int16_t rxPathComp;
      BSTREAM_TO_UINT16(txPathComp, pBuf);
      BSTREAM_TO_UINT16(rxPathComp, pBuf);
      status = LlWriteRfPathComp(txPathComp, rxPathComp);
      paramLen = LHCI_LEN_LE_WRITE_RF_PATH_COMP;
      break;
    }
    case HCI_OPCODE_LE_READ_RF_PATH_COMP:
    {
      paramLen = LHCI_LEN_LE_READ_RF_PATH_COMP;
      break;
    }
    case HCI_OPCODE_RESET:
      LlReset();

      /* No events during reset. */
      lhciCb.evtMsk = 0;
      lhciCb.evtMskPg2 = 0;
      lhciCb.leEvtMsk = 0;

      return TRUE;       /* LL event handler sends command completion */
    case HCI_OPCODE_NOP:
      paramLen = 1;
      break;
    case HCI_OPCODE_READ_AFH_CHAN_ASSESMENT_MODE:
    {
      paramLen = LHCI_LEN_READ_AFH_CHAN_ASSESMENT_MODE;
      break;
    }
    case HCI_OPCODE_WRITE_AFH_CHAN_ASSESSMENT_MODE:
    {
      BSTREAM_TO_UINT8(lhciCb.chanAssesmentMode, pBuf);
      paramLen = LHCI_LEN_WRITE_AFH_CHAN_ASSESMENT_MODE;
      break;
    }

    /* --- test --- */

    case HCI_OPCODE_LE_RECEIVER_TEST:
      status = LlRxTest(pBuf[0], 0);
      paramLen = LHCI_LEN_LE_RECEIVER_TEST_EVT;
      break;
    case HCI_OPCODE_LE_TRANSMITTER_TEST:
      status = LlTxTest(pBuf[0], pBuf[1], pBuf[2], 0);
      paramLen = LHCI_LEN_LE_TRANSMITTER_TEST_EVT;
      break;
    case HCI_OPCODE_LE_TEST_END:
      paramLen = LHCI_LEN_LE_TEST_END_EVT;
      break;
    case HCI_OPCODE_LE_MODIFY_SLEEP_CLK_ACC:
      paramLen = LHCI_LEN_LE_MODIFY_SCA_EVT;
      status = LlModifySleepClockAccuracy(pBuf[0]);
      break;

    /* --- default --- */

    default:
      return FALSE;       /* exit dispatcher routine */
  }

  lhciCommonSendCmdCmplEvt(pHdr, status, paramLen);

  return TRUE;
}
