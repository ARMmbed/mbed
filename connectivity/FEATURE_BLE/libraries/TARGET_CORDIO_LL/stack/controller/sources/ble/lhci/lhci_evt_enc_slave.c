/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  LL HCI event module implementation file.
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
#include "ll_api.h"
#include "wsf_msg.h"
#include "util/bstream.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief  Pack start encryption event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        Encryption change indication data.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackEncChangeEvt(uint8_t *pBuf, const LlEncChangeInd_t *pEvt)
{
  const uint8_t len = HCI_LEN_ENC_CHANGE;

  UINT8_TO_BSTREAM (pBuf, pEvt->status);
  UINT16_TO_BSTREAM(pBuf, pEvt->handle);
  UINT8_TO_BSTREAM (pBuf, pEvt->enabled);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack start encryption event packet [v2].
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        Encryption change indication data.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackEncChangeV2Evt(uint8_t *pBuf, const LlEncChangeInd_t *pEvt)
{
  const uint8_t len = HCI_LEN_ENC_CHANGE;

  UINT8_TO_BSTREAM (pBuf, pEvt->status);
  UINT16_TO_BSTREAM(pBuf, pEvt->handle);
  UINT8_TO_BSTREAM (pBuf, pEvt->enabled);
  UINT8_TO_BSTREAM (pBuf, pEvt->encKeySize);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack key refresh complete event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        Key refresh indication data.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackEncKeyRefreshEvt(uint8_t *pBuf, const LlEncKeyRefreshInd_t *pEvt)
{
  const uint8_t len = HCI_LEN_ENC_KEY_REFRESH_CMPL;

  UINT8_TO_BSTREAM (pBuf, pEvt->status);
  UINT16_TO_BSTREAM(pBuf, pEvt->handle);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a connection update complete event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        Connection update indication data.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackLtkReqEvt(uint8_t *pBuf, const LlLtkReqInd_t *pEvt)
{
  const uint8_t len = HCI_LEN_LE_LTK_REQ;

  UINT8_TO_BSTREAM (pBuf, HCI_LE_LTK_REQ_EVT);
  UINT16_TO_BSTREAM(pBuf, pEvt->handle);
  memcpy(pBuf, pEvt->randNum, sizeof(pEvt->randNum));
  pBuf += sizeof(pEvt->randNum);
  UINT16_TO_BSTREAM(pBuf, pEvt->encDiversifier);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a authenticated payload timeout indication event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        Authenticated payload timeout indication data.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackAuthPayloadTimeoutEvt(uint8_t *pBuf, const LlAuthPayloadTimeoutInd_t *pEvt)
{
  const uint8_t len = HCI_LEN_AUTH_PAYLOAD_TIMEOUT;

  UINT16_TO_BSTREAM(pBuf, pEvt->handle);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  LL slave data event handler.
 *
 *  \param  pEvt    Buffer containing LL event.
 *
 *  \return TRUE if event handled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lhciSlvEncEncodeEvtPkt(LlEvt_t *pEvt)
{
  uint8_t *pEvtBuf = NULL;

  switch (pEvt->hdr.event)
  {
    case LL_ENC_CHANGE_IND:
      if (lhciCb.evtMskPg2 & ((uint64_t)(HCI_EVT_MASK_ENC_CHANGE_V2) << LHCI_BYTE_TO_BITS(3)))
      {
        if ((pEvtBuf = lhciAllocEvt(HCI_ENC_CHANGE_V2_EVT, HCI_LEN_ENC_CHANGE_V2)) != NULL)
        {
          pEvt->encChangeInd.encKeySize = 0x01; /* Unused value. */
          lhciPackEncChangeV2Evt(pEvtBuf, &pEvt->encChangeInd);
        }
      }
      else if (lhciCb.evtMsk & ((uint64_t)(HCI_EVT_MASK_ENC_CHANGE) << LHCI_BYTE_TO_BITS(0)))
      {
        if ((pEvtBuf = lhciAllocEvt(HCI_ENC_CHANGE_EVT, HCI_LEN_ENC_CHANGE)) != NULL)
        {
          lhciPackEncChangeEvt(pEvtBuf, &pEvt->encChangeInd);
        }
      }
      break;
    case LL_ENC_KEY_REFRESH_IND:
      if (lhciCb.evtMsk & ((uint64_t)(HCI_EVT_MASK_ENC_KEY_REFRESH_CMPL) << LHCI_BYTE_TO_BITS(5)))
      {
        if ((pEvtBuf = lhciAllocEvt(HCI_ENC_KEY_REFRESH_CMPL_EVT, HCI_LEN_ENC_KEY_REFRESH_CMPL)) != NULL)
        {
          lhciPackEncKeyRefreshEvt(pEvtBuf, &pEvt->keyRefreshInd);
        }
      }
      break;
    case LL_LTK_REQ_IND:
      if ((lhciCb.leEvtMsk & ((uint64_t)(HCI_EVT_MASK_LE_LTK_REQ_EVT) << LHCI_BYTE_TO_BITS(0))) &&
         (lhciCb.evtMsk & ((uint64_t)(HCI_EVT_MASK_LE_META) << LHCI_BYTE_TO_BITS(7))))
      {
        if ((pEvtBuf = lhciAllocEvt(HCI_LE_META_EVT, HCI_LEN_LE_LTK_REQ)) != NULL)
        {
          lhciPackLtkReqEvt(pEvtBuf, &pEvt->ltkReqInd);
        }
      }
      /* If the event could not be sent, process it as a negative reply. */
      if (!pEvtBuf)
      {
        LlLtkReqNegReply(pEvt->ltkReqInd.handle);
      }
      break;
    case LL_AUTH_PAYLOAD_TIMEOUT_IND:
      if (lhciCb.evtMskPg2 & ((uint64_t)(HCI_EVT_MASK_AUTH_PAYLOAD_TIMEOUT) << LHCI_BYTE_TO_BITS(2)))
      {
        if ((pEvtBuf = lhciAllocEvt(HCI_AUTH_PAYLOAD_TIMEOUT_EVT, HCI_LEN_AUTH_PAYLOAD_TIMEOUT)) != NULL)
        {
          lhciPackAuthPayloadTimeoutEvt(pEvtBuf, &pEvt->authPayloadTimeoutInd);
        }
      }
      break;

    case LL_LTK_REQ_NEG_REPLY_CNF:
    case LL_LTK_REQ_REPLY_CNF:
    default:
      break;
  }

  if (pEvtBuf)
  {
    lhciSendEvt(pEvtBuf);
    return TRUE;
  }

  return FALSE;
}
