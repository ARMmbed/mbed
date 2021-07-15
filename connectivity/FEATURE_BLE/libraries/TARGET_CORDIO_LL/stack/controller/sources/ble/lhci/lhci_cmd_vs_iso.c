/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Arm Ltd. vendor specific HCI command module implementation file.
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
#include "bb_ble_api.h"
#include "ll_api.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/*! \brief      Control block for ISO generation. */
lhciIsoCb_t lhciIsoCb;

/*************************************************************************************************/
/*!
 *  \brief  Decode Arm Ltd. connection vendor specific HCI command packet.
 *
 *  \param  pHdr    Unpacked command header.
 *  \param  pBuf    Packed HCI packet buffer.
 *
 *  Command processing for vendor specific commands.
 *
 *  \return TRUE if opcode handled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lhciIsoVsStdDecodeCmdPkt(LhciHdr_t *pHdr, uint8_t *pBuf)
{
  uint8_t status = HCI_SUCCESS;
  uint8_t evtParamLen = 1;      /* default is status field only */

  /* Decode and consume command packet. */
  switch (pHdr->opCode)
  {
    /* --- ISO --- */

    case LHCI_OPCODE_VS_GET_ISO_TEST_REPORT:
      evtParamLen += sizeof(uint32_t) * 4;
      break;
    case LHCI_OPCODE_VS_ENA_ISO_SINK:
      lhciIsoCb.recvIsoSink = pBuf[0];
      break;
    case LHCI_OPCODE_VS_ENA_AUTO_GEN_ISO:
      BSTREAM_TO_UINT16(lhciIsoCb.genPktLen, pBuf);
      break;
    case LHCI_OPCODE_VS_GENERATE_ISO:
    {
      uint16_t handle;
      uint16_t pktLen;
      uint8_t numPkts;

      BSTREAM_TO_UINT16(handle, pBuf);
      BSTREAM_TO_UINT16(pktLen, pBuf);
      numPkts = *pBuf++;

      lhciGenerateIso(handle, pktLen, numPkts);
      break;
    }
    case LHCI_OPCODE_VS_GET_CIS_STATS:
    case LHCI_OPCODE_VS_GET_BIS_STATS:
      evtParamLen += sizeof(BbBleDataPktStats_t);
      break;

    /* --- default --- */

    default:
      return FALSE;       /* exit dispatcher routine */
  }

  uint8_t *pEvtBuf;

  /* Encode and send command complete event packet. */
  if ((pEvtBuf = lhciAllocCmdCmplEvt(evtParamLen, pHdr->opCode)) != NULL)
  {
    pBuf  = pEvtBuf;
    pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);

    switch (pHdr->opCode)
    {
      case LHCI_OPCODE_VS_GET_ISO_TEST_REPORT:
        UINT32_TO_BSTREAM(pBuf, lhciIsoCb.recvIsoPktCnt);
        UINT32_TO_BSTREAM(pBuf, lhciIsoCb.recvIsoOctetCnt);
        UINT32_TO_BSTREAM(pBuf, lhciIsoCb.genPktCnt);
        UINT32_TO_BSTREAM(pBuf, lhciIsoCb.genOctetCnt);
        break;
      case LHCI_OPCODE_VS_GET_CIS_STATS:
      {
        BbBleDataPktStats_t stats;
        BbBleGetCisStats(&stats);
        memcpy(pBuf, (uint8_t *)&stats, sizeof(stats));
        break;
      }
      case LHCI_OPCODE_VS_GET_BIS_STATS:
      {
        BbBleDataPktStats_t stats;
        BbBleGetBisStats(&stats);
        memcpy(pBuf, (uint8_t *)&stats, sizeof(stats));
        break;
      }

      /* --- default --- */

      default:
        break;
    }

    lhciSendCmdCmplEvt(pEvtBuf);
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Generate ISO packets.
 *
 *  \param  handle      Connection handle.
 *  \param  pktLen      Packet length.
 *  \param  numPkts     Number of packets.
 */
/*************************************************************************************************/
void lhciGenerateIso(uint16_t handle, uint16_t pktLen, uint8_t numPkts)
{
  if (lhciIsoCb.genEnaFlag)
  {
    /* Prevent re-entrance. */
    return;
  }

  lhciIsoCb.genEnaFlag = TRUE;

  while (numPkts--)
  {
    uint8_t *pIsoBuf;
    if ((pIsoBuf = (uint8_t*)WsfMsgAlloc(LL_ISO_PDU_MAX_LEN)) == NULL)
    {
      break;
    }

    uint8_t *pBuf = pIsoBuf;
    UINT16_TO_BSTREAM(pBuf, handle & 0xFFF);   /* BTS=0 and BF=0 */
    UINT16_TO_BSTREAM(pBuf, pktLen);
    memset(pBuf, lhciIsoCb.genPldCnt++, pktLen);

    LlSendIsoData(pIsoBuf);
  }

  lhciIsoCb.genEnaFlag = FALSE;
}
