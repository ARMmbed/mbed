/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave connection ISR callbacks.
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

#include "lctr_int_conn.h"
#include "lmgr_api_conn.h"
#include "bb_ble_api.h"
#include "pal_bb.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_os.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Slave connection ISR control block. */
static union
{
  /* Added at top of structure for 32-bit alignment. */
  uint8_t emptyPdu[LL_EMPTY_PDU_LEN];
                                /*!< Empty PDU buffer. Used only by active operation. */
  uint32_t align32;             /*!< Not used, declared for alignment of emptyPdu. */
} lctrConnIsr;

/*************************************************************************************************/
/*!
 *  \brief      Setup next transmit data buffer.
 *
 *  \param      pCtx        Connection context.
 */
/*************************************************************************************************/
static inline void lctrBuildEmptyPdu(lctrConnCtx_t *pCtx)
{
  pCtx->txHdr.llid = LL_LLID_EMPTY_PDU;
  /* pCtx->txHdr.nesn = 0; */           /* FC bits already valid */
  /* pCtx->txHdr.sn   = 0; */           /* FC bits already valid */
  /* pCtx->txHdr.md   = 0; */           /* already set */
  pCtx->txHdr.len  = 0;

  lctrPackDataPduHdr(lctrConnIsr.emptyPdu, &pCtx->txHdr);
}

/*************************************************************************************************/
/*!
 *  \brief  Set flow control bits in PDU.
 *
 *  \param  pHdr        Unpacked PDU header.
 *  \param  pBuf        Packed packet buffer.
 */
/*************************************************************************************************/
static inline void lctrUpdateFlowCtrlBits(const lctrDataPduHdr_t *pHdr, uint8_t *pBuf)
{
  const uint8_t FC_BITMASK = 0x1C;

  pBuf[LCTR_DATA_PDU_FC_OFFSET] &= ~FC_BITMASK;

  pBuf[LCTR_DATA_PDU_FC_OFFSET] |= (pHdr->nesn & 1) << 2;
  pBuf[LCTR_DATA_PDU_FC_OFFSET] |= (pHdr->sn   & 1) << 3;
  pBuf[LCTR_DATA_PDU_FC_OFFSET] |= (pHdr->md   & 1) << 4;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the LLID of a Data PDU.
 *
 *  \param  pBuf        Data PDU packet buffer.
 *
 *  \return LLID of the Data PDU.
 */
/*************************************************************************************************/
static inline uint8_t lctrRecoverLlid(uint8_t *pBuf)
{
  return pBuf[LCTR_DATA_PDU_FC_OFFSET] & 0x03;
}

/*************************************************************************************************/
/*!
 *  \brief      Process Rx acknowledgment.
 *
 *  \param      pCtx        Connection context.
 *
 *  \return     Pointer to next Rx buffer.
 */
/*************************************************************************************************/
uint8_t *lctrProcessRxAck(lctrConnCtx_t *pCtx)
{
  uint8_t *pNextRxBuf = NULL;
  if (pCtx->rxHdr.llid == LL_LLID_VS_PDU)
  {
    if (pLctrVsHdlrs && pLctrVsHdlrs->rxPduAck)
    {
      if (pLctrVsHdlrs->rxPduAck(LCTR_GET_CONN_HANDLE(pCtx)))
      {
        return NULL;
      }
      if ((pNextRxBuf = lctrRxPduAlloc(pCtx->localDataPdu.maxRxLen)) == NULL)
      {
        WSF_ASSERT(FALSE);
        return NULL;                    /* flow control Rx */
      }
      return pNextRxBuf;
    }
    else
    {
      /* Labeling this packet a re-transmission effectively drops this packet. */
      return NULL;
    }
  }

  /* Acknowledgment of received PDU (new data PDU). */
  if (((pCtx->rxHdr.sn ^ pCtx->txHdr.nesn) & 1) == 0)           /* bits are same */
  {
    /* Accept packets up to the maximum length because peer may queue packets before length change. */
    if (pCtx->rxHdr.len > (pCtx->localDataPdu.maxRxLen + LL_DATA_MIC_LEN))
    {
      /* Invalid length value; ack PDU but drop it (don't process it). */
      pCtx->txHdr.nesn++;
      return NULL;
    }
    else if (pCtx->rxHdr.len)               /* zero length implies Empty PDU */
    {
      switch (pCtx->rxHdr.llid)
      {
        case LL_LLID_CONT_PDU:
        case LL_LLID_START_PDU:
        case LL_LLID_CTRL_PDU:
          if (!lmgrConnCb.availRxBuf)
          {
            return NULL;                    /* flow control Rx */
          }
          if ((pNextRxBuf = lctrRxPduAlloc(pCtx->localDataPdu.maxRxLen)) == NULL)
          {
            WSF_ASSERT(FALSE);
            return NULL;                    /* flow control Rx */
          }
          lmgrConnCb.availRxBuf--;
          break;
        default:
          /* Invalid LLID value; ack PDU but drop it (don't process it). */
          pCtx->txHdr.nesn++;
          return NULL;
      }

      lctrIncPacketCounterRx(pCtx);
    }

    pCtx->txHdr.nesn++;
    return pNextRxBuf;
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Last Data PDU acknowledged by peer.
 *
 *  \param      pCtx        Connection context.
 */
/*************************************************************************************************/
void lctrTxPduAck(lctrConnCtx_t *pCtx)
{
  pCtx->txHdr.len = 0;

  /* Remove last transmitted PDU. */
  if (lctrTxQueuePop(pCtx))
  {
    lctrIncPacketCounterTx(pCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Process Tx acknowledgment.
 *
 *  \param      pCtx        Connection context.
 *
 *  \return     TRUE if peer ACK last Tx PDU, FALSE if peer NACK'ed last PDU.
 *
 *  Free ARQ element on peer acknowledgment.
 */
/*************************************************************************************************/
bool_t lctrProcessTxAck(lctrConnCtx_t *pCtx)
{
  if (((pCtx->rxHdr.nesn ^ pCtx->txHdr.sn) & 1) == 1)           /* bits are different */
  {
    pCtx->txHdr.sn++;

    if (pCtx->txHdr.len)     /* last packet from ARQ queue; zero length implies empty PDU */
    {
      if ((pCtx->txHdr.llid == LL_LLID_CTRL_PDU) &&
          (pCtx->numTxPendCtrlPdu > 0))
      {
        pCtx->numTxPendCtrlPdu--;
      }

      /*** Peer ACK'ed a Data PDU ***/

      lctrTxPduAck(pCtx);
    }
    else
    {
      /*** Peer ACK'ed a Empty PDU ***/

      pCtx->emptyPduPend = FALSE;
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
 *  \param      pCtx        Connection context.
 *
 *  Cleanup from Tx acknowledgment processing.
 */
/*************************************************************************************************/
void lctrProcessTxAckCleanup(lctrConnCtx_t *pCtx)
{
  /* Complete buffer cleanup. */
  lctrTxQueuePopCleanup(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Setup for PDU transmission.
 *
 *  \param      pCtx        Connection context.
 *  \param      rxStatus    Status of last receive.
 *  \param      reqTx       Require Tx.
 *
 *  \return     Number of bytes transmitted, 0 if no packet transmitted.
 */
/*************************************************************************************************/
uint16_t lctrSetupForTx(lctrConnCtx_t *pCtx, uint8_t rxStatus, bool_t reqTx)
{
  uint16_t numTxBytes = 0;

  if ((rxStatus != BB_STATUS_SUCCESS) ||
      pCtx->rxHdr.md ||                 /* peer has more data */
      pCtx->txHdr.md ||                 /* peer is informed more data is pending */
      reqTx)
  {
    PalBbBleTxBufDesc_t bbDesc[3];
    uint8_t bbDescCnt;
    bool_t md;

    pCtx->txHdr.llid = ~LL_LLID_VS_PDU;                       /* reset last PDU LLID */

    /* Do not remove from ARQ until acknowledged by peer. */
    bbDescCnt = lctrTxQueuePeek(pCtx, &bbDesc[0], &md);
    if (!pCtx->emptyPduPend &&
        (bbDescCnt > 0))
    {
      /* Set flow control bits. */
      md = md || (lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_SLV_REQ_IMMED_ACK) && (pCtx->role == LL_ROLE_SLAVE)); /* try to get ACK from master for this Tx */;
      pCtx->txHdr.md = md;
      pCtx->txHdr.len = bbDesc[0].pBuf[LCTR_DATA_PDU_LEN_OFFSET];
      pCtx->txHdr.llid = lctrRecoverLlid(bbDesc[0].pBuf);

      if ((pCtx->txHdr.llid == LL_LLID_VS_PDU) &&
          (pLctrVsHdlrs && pLctrVsHdlrs->txPduFc))
      {
        pLctrVsHdlrs->txPduFc(LCTR_GET_CONN_HANDLE(pCtx), bbDesc[0].pBuf);
      }
      else
      {
        WSF_ASSERT(pCtx->txHdr.len);
        lctrUpdateFlowCtrlBits(&pCtx->txHdr, bbDesc[0].pBuf);
      }

#if (LL_ENABLE_TESTER)
      bbDesc[0].pBuf[0] ^= llTesterCb.pktLlId & 0x03;
#endif

      lctrSetBbPacketCounterTx(pCtx);
      BbBleTxData(&bbDesc[0], bbDescCnt);
      numTxBytes = LL_DATA_HDR_LEN + bbDesc[0].pBuf[LCTR_DATA_PDU_LEN_OFFSET];
    }
    else
    {
      /*** Send Empty PDU ***/

      bool_t sendAck = TRUE;

      if ((rxStatus == BB_STATUS_SUCCESS) &&
          (pCtx->rxHdr.llid == LL_LLID_VS_PDU))
      {
        if (pLctrVsHdlrs && pLctrVsHdlrs->txPduAck)
        {
          sendAck = pLctrVsHdlrs->txPduAck(LCTR_GET_CONN_HANDLE(pCtx));
        }
        /* else ignore illegal LLID */
      }

      if (sendAck)
      {
        /* Empty PDU with MD=1 implies previous transmitted empty PDU was NACK'ed. */
        pCtx->txHdr.md = (bbDescCnt > 0) ? TRUE : FALSE;
        /* Transmit empty PDU. */
        lctrBuildEmptyPdu(pCtx);

        PalBbBleTxBufDesc_t desc = {.pBuf = lctrConnIsr.emptyPdu, .len = sizeof(lctrConnIsr.emptyPdu)};
        BbBleTxData(&desc, 1);
        numTxBytes = desc.len;

        pCtx->emptyPduFirstAtt = !pCtx->emptyPduPend;
        pCtx->emptyPduPend     = TRUE;
      }
    }
  }
  /* else nothing to transmit */

  return numTxBytes;
}

/*************************************************************************************************/
/*!
 *  \brief      Rx post processing.
 *
 *  \param      pCtx        Connection context.
 *  \param      pRxBuf      Rx buffer.
 *  \param      pNextRxBuf  Next Rx buffer.
 *  \param      loadRxBuf   Rx buffer loading is required.
 */
/*************************************************************************************************/
void lctrRxPostProcessing(lctrConnCtx_t *pCtx, uint8_t *pRxBuf, uint8_t *pNextRxBuf, bool_t loadRxBuf)
{
  if (pNextRxBuf)       /* Another buffer ready to replace the received one. */
  {
    lctrRxEnq(pRxBuf, pCtx->eventCounter, LCTR_GET_CONN_HANDLE(pCtx));
    pRxBuf = pNextRxBuf;
  }

  /*** Reload receive buffer ***/

  if (loadRxBuf)
  {
    lctrSetBbPacketCounterRx(pCtx);
    BbBleRxData(pRxBuf, LCTR_DATA_PDU_LEN(pCtx->localDataPdu.maxRxLen));
  }
  else
  {
    if (pRxBuf)
    {
      lctrRxPduFree(pRxBuf);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check for maximum CE duration.
 *
 *  \param      pCtx        Connection context.
 *  \param      ceStartUsec CE start time in microseconds.
 *  \param      pendDurUsec Pending operation duration in microseconds.
 *
 *  \return     FALSE if within duration period, TRUE if exceeds period.
 */
/*************************************************************************************************/
bool_t lctrExceededMaxDur(lctrConnCtx_t *pCtx, uint32_t ceStartUsec, uint32_t pendDurUsec)
{
  BbOpDesc_t *pOp = &pCtx->connBod;

  if (lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_BYPASS_CE_GUARD))
  {
    return FALSE;
  }

  const uint32_t curTime = PalBbGetCurrentTime();
  const uint32_t setupDelayUsec = BbGetSchSetupDelayUs();

  uint32_t availCeUsec = LCTR_CONN_IND_US(pCtx->connInterval);

  if (pOp->pNext)
  {
    /* Limit CE duration to the edge of neighboring BOD. */
    availCeUsec = WSF_MIN(availCeUsec, BbGetTargetTimeDelta(pOp->pNext->dueUsec, ceStartUsec));
  }

  if ((BbGetTargetTimeDelta(curTime, ceStartUsec) + LL_BLE_TIFS_US + pendDurUsec + setupDelayUsec) > availCeUsec)
  {
    return TRUE;
  }

  return FALSE;
}
