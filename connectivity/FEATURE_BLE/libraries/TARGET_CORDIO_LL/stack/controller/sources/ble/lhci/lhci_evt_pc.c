/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  LL HCI event module implementation file.
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
#include "chci_api.h"
#include "hci_defs.h"
#include "wsf_msg.h"
#include "util/bstream.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief  Power report event handler.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        Power report indication data.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackPowerReportEvt(uint8_t *pBuf, const LlPowerReportInd_t *pEvt)
{
  const uint8_t len = HCI_LEN_LE_POWER_REPORT;

  UINT8_TO_BSTREAM (pBuf, HCI_LE_POWER_REPORT_EVT);
  UINT8_TO_BSTREAM (pBuf, pEvt->status)
  UINT16_TO_BSTREAM(pBuf, pEvt->connHandle);
  UINT8_TO_BSTREAM (pBuf, pEvt->reason);
  UINT8_TO_BSTREAM (pBuf, pEvt->phy);
  UINT8_TO_BSTREAM (pBuf, pEvt->txPower);
  UINT8_TO_BSTREAM (pBuf, pEvt->txPowerLimits);
  UINT8_TO_BSTREAM (pBuf, pEvt->delta);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Path loss event handler.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        Path loss event data.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackPathLossEvt(uint8_t *pBuf, const LlPathLossThresholdInd_t *pEvt)
{
  const uint8_t len = HCI_LEN_LE_PATH_LOSS_ZONE;

  UINT8_TO_BSTREAM (pBuf, HCI_LE_PATH_LOSS_REPORT_EVT);
  UINT16_TO_BSTREAM(pBuf, pEvt->connHandle);
  UINT8_TO_BSTREAM (pBuf, pEvt->curPathLoss);
  UINT8_TO_BSTREAM (pBuf, pEvt->zoneEntered);

  return len;
}
/*************************************************************************************************/
/*!
 *  \brief  LL connection event handler.
 *
 *  \param  pEvt    Buffer containing LL event.
 *
 *  \return TRUE if event handled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lhciPclEncodeEvtPkt(LlEvt_t *pEvt)
{
  uint8_t *pEvtBuf = NULL;

  switch (pEvt->hdr.event)
  {
    case LL_TX_POWER_REPORTING_IND:
      if ((lhciCb.leEvtMsk & ((uint64_t)(HCI_EVT_MASK_LE_TX_POWER_REPORT_EVT) << LHCI_BYTE_TO_BITS(4))) &&
           (lhciCb.evtMsk & ((uint64_t)(HCI_EVT_MASK_LE_META) << LHCI_BYTE_TO_BITS(7))))
      {
        if ((pEvtBuf = lhciAllocEvt(HCI_LE_META_EVT, HCI_LEN_LE_POWER_REPORT)) != NULL)
        {
          lhciPackPowerReportEvt(pEvtBuf, &pEvt->powerRptInd);
        }
      }
      break;
    case LL_PATH_LOSS_REPORTING_IND:
      if ((lhciCb.leEvtMsk & ((uint64_t)(HCI_EVT_MASK_LE_PATH_LOSS_REPORT_EVT) << LHCI_BYTE_TO_BITS(3))) &&
           (lhciCb.evtMsk & ((uint64_t)(HCI_EVT_MASK_LE_META) << LHCI_BYTE_TO_BITS(7))))
      {
        if ((pEvtBuf = lhciAllocEvt(HCI_LE_META_EVT, HCI_LEN_LE_PATH_LOSS_ZONE)) != NULL)
        {
          lhciPackPathLossEvt(pEvtBuf, &pEvt->pathLossInd);
        }
      }
      break;

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
