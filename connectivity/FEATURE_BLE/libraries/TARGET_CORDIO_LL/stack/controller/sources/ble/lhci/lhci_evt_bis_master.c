/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  LL HCI event module implementation file.
 *
 *  Copyright (c) 2019 Arm Ltd. All Rights Reserved.
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
#include "lctr_api.h"
#include "ll_api.h"
#include "util/bstream.h"

/**************************************************************************************************
  External Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Pack a BIG Terminate Sync command complete packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        BIG terminate event data.
 */
/*************************************************************************************************/
static void lhciPackCmdCompleteBigTerminateSync(uint8_t *pBuf, const LlBigTermSyncCnf_t *pEvt)
{
  UINT8_TO_BSTREAM(pBuf, pEvt->status);
  UINT8_TO_BSTREAM(pBuf, pEvt->bigHandle);
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a BIG create BIG Sync Established event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        BIG Sync Established event data.
 */
/*************************************************************************************************/
static void lhciPackBigSyncEstEvt(uint8_t *pBuf, const LlBigSyncEstInd_t *pEvt)
{
  UINT8_TO_BSTREAM (pBuf, HCI_LE_BIG_SYNC_EST_EVT);
  UINT8_TO_BSTREAM (pBuf, pEvt->status);
  UINT8_TO_BSTREAM (pBuf, pEvt->bigHandle);
  UINT24_TO_BSTREAM(pBuf, pEvt->transLatUsec);
  UINT8_TO_BSTREAM (pBuf, pEvt->nse);
  UINT8_TO_BSTREAM (pBuf, pEvt->bn);
  UINT8_TO_BSTREAM (pBuf, pEvt->pto);
  UINT8_TO_BSTREAM (pBuf, pEvt->irc);
  UINT16_TO_BSTREAM(pBuf, pEvt->maxPdu);
  UINT16_TO_BSTREAM(pBuf, pEvt->isoInterval);
  UINT8_TO_BSTREAM (pBuf, pEvt->numBis);
  for (unsigned int i = 0; i < pEvt->numBis; i++)
  {
    UINT16_TO_BSTREAM(pBuf, pEvt->bisHandle[i]);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a BIG Sync Lost event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        Terminate BIG Complete event data.
 */
/*************************************************************************************************/
static void lhciPackBigSyncLostEvt(uint8_t *pBuf, const LlBigSyncLostInd_t *pEvt)
{
  UINT8_TO_BSTREAM(pBuf, HCI_LE_BIG_SYNC_LOST_EVT);
  UINT8_TO_BSTREAM(pBuf, pEvt->bigHandle);
  UINT8_TO_BSTREAM(pBuf, pEvt->reason);
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a BIG Sync Lost event packet.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pEvt        Terminate BIG Complete event data.
 */
/*************************************************************************************************/
static void lhciPackBigBigInfoAdvReportEvt(uint8_t *pBuf, const LlBigInfoAdvRptInd_t *pEvt)
{
  UINT8_TO_BSTREAM (pBuf, HCI_LE_BIG_INFO_ADV_REPORT_EVT);
  UINT16_TO_BSTREAM(pBuf, pEvt->syncHandle);
  UINT8_TO_BSTREAM (pBuf, pEvt->numBis);
  UINT8_TO_BSTREAM (pBuf, pEvt->nse);
  UINT16_TO_BSTREAM(pBuf, pEvt->isoInterv);
  UINT8_TO_BSTREAM (pBuf, pEvt->bn);
  UINT8_TO_BSTREAM (pBuf, pEvt->pto);
  UINT8_TO_BSTREAM (pBuf, pEvt->irc);
  UINT16_TO_BSTREAM(pBuf, pEvt->maxPdu);
  UINT24_TO_BSTREAM(pBuf, pEvt->sduInterv);
  UINT16_TO_BSTREAM(pBuf, pEvt->maxSdu);
  UINT8_TO_BSTREAM (pBuf, pEvt->phy);
  UINT8_TO_BSTREAM (pBuf, pEvt->framing);
  UINT8_TO_BSTREAM (pBuf, pEvt->encrypt);
}

/*************************************************************************************************/
/*!
 *  \brief  LL BIS master event handler.
 *
 *  \param  pEvt    Buffer containing LL event.
 *
 *  \return TRUE if event handled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lhciMstBisEncodeEvtPkt(LlEvt_t *pEvt)
{
  uint8_t *pEvtBuf = NULL;

  switch (pEvt->hdr.event)
  {
    case LL_BIG_TERM_SYNC_CNF:
      if ((pEvtBuf = lhciAllocEvt(HCI_CMD_CMPL_EVT, HCI_LEN_CMD_CMPL + LHCI_LEN_LE_BIG_TERMINATE_SYNC)) != NULL)
      {
        uint8_t *pBuf = pEvtBuf;
        pBuf += LhciPackCmdCompleteEvt(pBuf, HCI_OPCODE_LE_BIG_TERMINATE_SYNC);
        lhciPackCmdCompleteBigTerminateSync(pBuf, &pEvt->bigTermSyncCnf);
      }
      break;

    case LL_BIG_SYNC_EST_IND:
      if ((lhciCb.leEvtMsk & ((uint64_t)(HCI_EVT_MASK_LE_BIG_SYNC_EST_EVT) << LHCI_BYTE_TO_BITS(3))) &&
         (lhciCb.evtMsk & ((uint64_t)(HCI_EVT_MASK_LE_META) << LHCI_BYTE_TO_BITS(7))))
      {
        if ((pEvtBuf = lhciAllocEvt(HCI_LE_META_EVT, HCI_LEN_LE_BIG_SYNC_EST(pEvt->bigSyncEstInd.numBis))) != NULL)
        {
          lhciPackBigSyncEstEvt(pEvtBuf, &pEvt->bigSyncEstInd);
        }
      }
      break;

    case LL_BIG_SYNC_LOST_IND:
      if ((lhciCb.leEvtMsk & ((uint64_t)(HCI_EVT_MASK_LE_BIG_SYNC_LOST_EVT) << LHCI_BYTE_TO_BITS(3))) &&
         (lhciCb.evtMsk & ((uint64_t)(HCI_EVT_MASK_LE_META) << LHCI_BYTE_TO_BITS(7))))
      {
        if ((pEvtBuf = lhciAllocEvt(HCI_LE_META_EVT, HCI_LEN_LE_BIG_SYNC_LOST)) != NULL)
        {
          lhciPackBigSyncLostEvt(pEvtBuf, &pEvt->bigSyncLostInd);
        }
      }
      break;

    case LL_BIG_INFO_ADV_REPORT_IND:
    {
      bool_t evtSent = FALSE;

      if ((lhciCb.leEvtMsk & ((uint64_t)(HCI_EVT_MASK_LE_BIG_INFO_ADV_RPT_EVT) << LHCI_BYTE_TO_BITS(4))) &&
         (lhciCb.evtMsk & ((uint64_t)(HCI_EVT_MASK_LE_META) << LHCI_BYTE_TO_BITS(7))))
      {
        if ((lhciCb.numAdvReport < pLctrRtCfg->maxAdvReports) &&
            ((pEvtBuf = lhciAllocEvt(HCI_LE_META_EVT, HCI_LEN_LE_BIG_INFO_ADV_REPORT)) != NULL))
        {
          lhciPackBigBigInfoAdvReportEvt(pEvtBuf, &pEvt->bigInfoInd);
          lhciCb.numAdvReport++;
          evtSent = TRUE;
        }
      }
      if (!evtSent)
      {
        /* Handle event here even if event cannot be sent. */
        return TRUE;
      }
      break;
    }
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
