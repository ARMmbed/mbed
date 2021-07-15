/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller connected isochronous stream ISR callbacks.
 *
 *  Copyright (c) 2013-2019 ARM Ltd. All Rights Reserved.
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

#include "lctr_int_iso.h"
#include "lctr_int_cis.h"
#include "bb_ble_api.h"
#include "pal_bb.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_os.h"
#include "util/bstream.h"
#include <string.h>

#if (LL_ENABLE_TESTER)
#include "pal_bb_ble_tester.h"
#endif

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      CIS ISR control block. */
static union
{
  /* Added at top of structure for 32-bit alignment. */
  uint8_t emptyPdu[LL_EMPTY_PDU_LEN];
                                /*!< Empty PDU buffer. Used only by active operation. */
  uint32_t align32;             /*!< Not used, declared for alignment of emptyPdu. */
} lctrCisIsr;

/*************************************************************************************************/
/*!
 *  \brief      Build empty CIS PDU.
 *
 *  \param      pCisCtx     CIS context.
 */
/*************************************************************************************************/
static inline void lctrCisBuildEmptyPdu(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->txHdr.llid = LL_LLID_ISO_EMPTY_PDU;
  /* pCisCtx->txHdr.nesn = 0; */           /* FC bits already valid */
  /* pCisCtx->txHdr.sn   = 0; */           /* FC bits already valid */
  /* pCisCtx->txHdr.cie  = 0; */           /* already set */
  pCisCtx->txHdr.len  = 0;

  lctrCisPackDataPduHdr(lctrCisIsr.emptyPdu, &pCisCtx->txHdr);
}

/*************************************************************************************************/
/*!
 *  \brief      Build NULL CIS PDU.
 *
 *  \param      pCisCtx     CIS context.
 */
/*************************************************************************************************/
static inline void lctrCisBuildNullPdu(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->txHdr.llid = LL_LLID_ISO_UNF_END_PDU;
  /* pCisCtx->txHdr.nesn = 0; */           /* FC bits already valid */
  pCisCtx->txHdr.sn   = 0;
  /* pCisCtx->txHdr.cie  = 0; */           /* already set */
  pCisCtx->txHdr.np  = 1;
  pCisCtx->txHdr.len  = 0;

  lctrCisPackDataPduHdr(lctrCisIsr.emptyPdu, &pCisCtx->txHdr);
}

/*************************************************************************************************/
/*!
 *  \brief  Set flow control bits in PDU.
 *
 *  \param  pHdr        Unpacked PDU header.
 *  \param  pBuf        Packed packet buffer.
 */
/*************************************************************************************************/
static inline void lctrCisUpdateFlowCtrlBits(const lctrCisDataPduHdr_t *pHdr, uint8_t *pBuf)
{
  const uint8_t FC_BITMASK = 0xC;

  pBuf[LCTR_ISO_DATA_PDU_FC_OFFSET] &= ~FC_BITMASK;

  pBuf[LCTR_ISO_DATA_PDU_FC_OFFSET] |= (pHdr->nesn & 1) << 2;
  pBuf[LCTR_ISO_DATA_PDU_FC_OFFSET] |= (pHdr->sn   & 1) << 3;
  pBuf[LCTR_ISO_DATA_PDU_FC_OFFSET] |= (pHdr->cie  & 1) << 4;
  pBuf[LCTR_ISO_DATA_PDU_FC_OFFSET] |= (pHdr->np   & 1) << 6;
}

/*************************************************************************************************/
/*!
 *  \brief      Process Rx acknowledgment.
 *
 *  \param      pCisCtx       CIS context.
 *
 *  \return     TRUE if peer ACK last Tx PDU, FALSE if peer NACK'ed last PDU.
 */
/*************************************************************************************************/
bool_t lctrCisProcessRxAck(lctrCisCtx_t *pCisCtx)
{
  bool_t result = FALSE;

  /* Nulls must not be ACKed. */
  if (pCisCtx->rxHdr.np == 1)
  {
    return result;
  }

  /* Acknowledgment of received PDU (new data PDU). */
  if (((pCisCtx->rxHdr.sn ^ pCisCtx->txHdr.nesn) & 1) == 0)           /* bits are same */
  {
    /* Accept packets up to the maximum length because peer may queue packets before length change. */
    if (pCisCtx->rxHdr.len > (pCisCtx->localDataPdu.maxRxLen + LL_DATA_MIC_LEN))
    {
      /* Invalid length value; ack PDU but drop it (don't process it). */
      lctrCisIncPacketCounterRx(pCisCtx);
      pCisCtx->txHdr.nesn++;
      return result;
    }

    if ((pCisCtx->rxHdr.len) ||
        ((pCisCtx->rxHdr.len == 0) && (pCisCtx->rxHdr.llid == LL_LLID_ISO_UNF_END_PDU)))
    {
      if (lctrSetDecryptPktCountHdlr)
      {
        lctrCisIncPacketCounterRx(pCisCtx);
      }
      result = TRUE;
    }
    else /* Empty PDU. */
    {
      lctrCisIncPacketCounterRx(pCisCtx);
    }
    /* Invalid LLID value; ack PDU but drop it (don't process it). */
    /* length of 0 and LLID of LL_LLID_EMPTY_PDU implies padding/empty PDU. */

    pCisCtx->txHdr.nesn++;
    if (pCisCtx->rxFtParamList.pHead != NULL)
    {
      pCisCtx->rxFtParamList.pHead->ftParam.pduRcved = TRUE;
    }
    return result;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Last Data PDU acknowledged by peer.
 *
 *  \param      pCisCtx        Connection context.
 */
/*************************************************************************************************/
void lctrCisTxPduAck(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->txHdr.len = 0;
  pCisCtx->txBufPendAck = FALSE;

  /* Remove last transmitted PDU. */
  if (lctrCisTxQueuePop(pCisCtx))
  {
    pCisCtx->txDataCounter++;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Process Tx acknowledgment.
 *
 *  \param      pCisCtx        Connection context.
 *
 *  \return     TRUE if peer ACK last Tx PDU, FALSE if peer NACK'ed last PDU.
 *
 *  Free ARQ element on peer acknowledgment.
 */
/*************************************************************************************************/
bool_t lctrCisProcessTxAck(lctrCisCtx_t *pCisCtx)
{
  if (((pCisCtx->rxHdr.nesn ^ pCisCtx->dataSn) & 1) == 1)           /* bits are different */
  {
    pCisCtx->dataSn++;

    if (pCisCtx->txBufPendAck)
    {
      /*** Peer ACK'ed a data PDU ***/

      lctrCisTxPduAck(pCisCtx);
    }
    else
    {
      /*** Peer ACK'ed an Empty PDU ***/
      pCisCtx->txDataCounter++;
    }

    if (pCisCtx->txFtParamList.pHead != NULL)
    {
      pCisCtx->txFtParamList.pHead->ftParam.pduAcked = TRUE;
    }

    return TRUE;
  }
  /* Peer NACK'ed PDU. */
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Post-process Tx acknowledgment.
 *
 *  \param      pCisCtx        Connection context.
 *
 *  Cleanup from Tx acknowledgment processing.
 */
/*************************************************************************************************/
void lctrCisProcessTxAckCleanup(lctrCisCtx_t *pCisCtx)
{
  /* Complete buffer cleanup. */
  if (pCisCtx->txPduIsAcked || pCisCtx->pduFlushed)
  {
    lctrCisTxQueuePopCleanup(pCisCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Setup for CIS data PDU transmission.
 *
 *  \param      pCigCtx     Connection context.
 *  \param      rxStatus    Status of last receive.
 *  \param      reqTx       Require Tx.
 *
 *  \return     Number of bytes transmitted, 0 if no packet transmitted.
 */
/*************************************************************************************************/
uint16_t lctrCisSetupForTx(lctrCigCtx_t *pCigCtx, uint8_t rxStatus, bool_t reqTx)
{
  uint16_t numTxBytes = 0;
  lctrCisCtx_t *pCisCtx = pCigCtx->pCisCtx;

#if (LL_ENABLE_TESTER)
  /* Invalidate access address if trigger is set. */
  if (llTesterCb.isoAccAddrSeTrigMask &&
      (llTesterCb.isoAccAddrSeTrigMask & (1 << pCisCtx->subEvtCounter)) &&
      !llTesterCb.isoAccAddrInvForRx)
  {
    PalBbTesterInvalidateNextAccAddr(FALSE);

    if (llTesterCb.isoAccAddrInvNumTrig)
    {
      llTesterCb.isoAccAddrInvNumTrig--;
      if (llTesterCb.isoAccAddrInvNumTrig == 0)
      {
        llTesterCb.isoAccAddrSeTrigMask = 0;
      }
    }
  }
#endif

  /*** Setup for transmit ***/

  /* pCisCtx->txHdr.cie  = 0; CIE bit is initialized in the lctrMstCisInitIsr or lctrSlvCisInitIsr and shall be set once for the event. */
  pCisCtx->txHdr.np   = 0;
  pCisCtx->txHdr.len  = 0;
  pCisCtx->txHdr.llid = LL_LLID_ISO_UNF_END_PDU;
  pCisCtx->txHdr.sn = pCisCtx->dataSn;

  if ((rxStatus != BB_STATUS_SUCCESS) ||
      reqTx)
  {
    PalBbBleTxBufDesc_t bbDesc[3];
    uint8_t bbDescCnt;

    if (pCisCtx->isTxDone == TRUE)
    {
      /*** Send NULL PDU ***/

      lctrCisBuildNullPdu(pCisCtx);

      PalBbBleTxBufDesc_t desc = {.pBuf = lctrCisIsr.emptyPdu, .len = sizeof(lctrCisIsr.emptyPdu)};
      BbBleCisTxData(&desc, 1);
      numTxBytes = desc.len;

      return numTxBytes;
    }

    /* Do not remove from ARQ until acknowledged by peer. */
    bbDescCnt = lctrCisTxQueuePeek(pCisCtx, &bbDesc[0]);
    lctrFtParam_t *pFtParam = &pCisCtx->txFtParamList.pHead->ftParam;

    if (bbDescCnt > 0)
    {
      /* Set flow control bits. */
      pCisCtx->txHdr.len = bbDesc[0].pBuf[LCTR_ISO_DATA_PDU_LEN_OFFSET];
      pFtParam->pduType[pFtParam->pduCounter] = LCTR_CIS_PDU_NON_EMPTY;
      pCisCtx->txBufPendAck = TRUE;
      lctrCisUpdateFlowCtrlBits(&pCisCtx->txHdr, bbDesc[0].pBuf);

#if (LL_ENABLE_TESTER)
      bbDesc[0].pBuf[0] ^= llTesterCb.pktLlId & 0x03;
#endif

      lctrSetBbCisPacketCounterTx(pCisCtx);
      BbBleCisTxData(&bbDesc[0], bbDescCnt);
      numTxBytes = LL_DATA_HDR_LEN + bbDesc[0].pBuf[LCTR_ISO_DATA_PDU_LEN_OFFSET];

      if (pCisCtx->subEvtCounter == pCisCtx->nse)
      {
        pCisCtx->isoLinkQualStats.txLastSubEventPkt++;
      }
    }
    else
    {
      /*** Send Empty PDU ***/

      lctrCisBuildEmptyPdu(pCisCtx);
      if (pFtParam != NULL)
      {
        pFtParam->pduType[pFtParam->pduCounter] = LCTR_CIS_PDU_EMPTY;
      }

      PalBbBleTxBufDesc_t desc = {.pBuf = lctrCisIsr.emptyPdu, .len = sizeof(lctrCisIsr.emptyPdu)};
      BbBleCisTxData(&desc, 1);
      numTxBytes = desc.len;
    }
  }
  /* else nothing to transmit */

  return numTxBytes;
}

/*************************************************************************************************/
/*!
 *  \brief      Rx post processing.
 *
 *  \param      pCisCtx     CIS context.
 *  \param      pRxBuf      Rx buffer.
 */
/*************************************************************************************************/
void lctrCisRxPostProcessing(lctrCisCtx_t *pCisCtx, uint8_t *pRxBuf)
{
  if (!pCisCtx->validRx && (pRxBuf != NULL)) /* Another buffer ready to replace the received one. */
  {
    lctrCisRxPduFree(pRxBuf);
    pRxBuf = NULL;
  }

  /* Notify the task */
  lctrCisRxEnq(pRxBuf, pCisCtx->cisEvtCounter, pCisCtx->cisHandle);
}
