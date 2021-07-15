/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave BIG ISR callbacks.
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

#include "lctr_int_bis_slave.h"
#include "lctr_int_bis.h"
#include "lctr_int_iso.h"
#include "ll_defs.h"
#include "wsf_trace.h"

#if (LL_ENABLE_TESTER)
#include "pal_bb_ble_tester.h"
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief  Write CSSN value. */
#define LCTR_BIS_WR_CSSN(v, cssn)       (v) = ((v) & 0xE3) | (((cssn) & 0x7) << 2)

/*! \brief  Set CSTF flag. */
#define LCTR_BIS_SET_CSTF(v)            (v) |= (1 << 5)

/*! \brief  Clear CSTF flag. */
#define LCTR_BIS_CLR_CSTF(v)            (v) &= ~(1 << 5)

/**************************************************************************************************
  Globals
**************************************************************************************************/

/*! \brief      BIS slave ISR control block. */
static struct
{
  lctrSeCtx_t se;           /*!< Subevent context. */
  uint8_t csFlags;          /*!< CSSN and CSTF flags. */
  uint8_t emptyPdu[LL_EMPTY_PDU_LEN];
                            /*!< Empty PDU buffer. */
  uint8_t numTxFrags[LL_MAX_BIS];
                            /*!< Number of fragments transmitted. */
  uint32_t nextSeOffs;      /*!< Next subevent time. */
  PalBbBleChan_t *pNextChan;/*!< Next subevent channel. */
  uint8_t *pCtrlBuf;        /*!< Transmit BIS Control PDU buffer. */
} lctrSlvBisIsr;

/*! \brief      Transmit descriptor. */
PalBbBleTxBufDesc_t lctrSlvBisTxDesc[LL_MAX_BIS][LL_MAX_BN][3];  /* Place outside of lctrSlvBisIsr to reduce memclr(). */

/*! \brief      Encrypted BIS Control PDU buffer. */
static uint8_t lctrEncCtrlBuf[32];   /* Place outside of lctrSlvBisIsr to reduce memclr(). */

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Setup BIS Data PDU for an BIS ISO Event.
 *
 *  \param  pBigCtx     BIG context.
 *  \param  bisIdx      BIS index.
 *  \param  burstIdx    Burst index.
 */
/*************************************************************************************************/
static void lctrSlvBisSetupTxData(lctrBigCtx_t *pBigCtx, uint8_t bisIdx, uint8_t burstIdx)
{
  if (lctrBisTxQueuePeek(pBigCtx->pBisCtx[bisIdx], burstIdx, &lctrSlvBisTxDesc[bisIdx][burstIdx][0]) > 0)
  {
    lctrSlvBisIsr.numTxFrags[bisIdx]++;

    /* Update Control PDU flags. */
    LCTR_BIS_WR_CSSN(lctrSlvBisTxDesc[bisIdx][burstIdx][0].pBuf[LCTR_ISO_DATA_PDU_FC_OFFSET], pBigCtx->bcp.cssn);
    if (lctrSlvBisIsr.pCtrlBuf)
    {
      LCTR_BIS_SET_CSTF(lctrSlvBisTxDesc[bisIdx][burstIdx][0].pBuf[LCTR_ISO_DATA_PDU_FC_OFFSET]);
    }
    else
    {
      LCTR_BIS_CLR_CSTF(lctrSlvBisTxDesc[bisIdx][burstIdx][0].pBuf[LCTR_ISO_DATA_PDU_FC_OFFSET]);
    }
  }
  else
  {
    /* BIS Empty PDU. */
    lctrSlvBisTxDesc[bisIdx][burstIdx][0].pBuf = lctrSlvBisIsr.emptyPdu;
    lctrSlvBisTxDesc[bisIdx][burstIdx][0].len = sizeof(lctrSlvBisIsr.emptyPdu);
    lctrSlvBisTxDesc[bisIdx][burstIdx][1].pBuf = NULL;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  BIG Control PDU decrypt.
 *
 *  \param  pBigCtx     BIG context.
 */
/*************************************************************************************************/
static void lctrMstBigControlEncrypt(lctrBigCtx_t *pBigCtx)
{
  uint64_t pktCtr = pBigCtx->eventCounter * pBigCtx->bn;
  pBigCtx->ctrChan.enc.pTxPktCounter = &pktCtr;

  if (pBigCtx->encrypt && lctrPktEncryptHdlr)
  {
    uint16_t len = lctrSlvBisIsr.pCtrlBuf[LCTR_ISO_DATA_PDU_LEN_OFFSET];

    /* Ensure encryption shadow buffer can hold the contents of the data. */
    WSF_ASSERT((size_t)(LL_DATA_HDR_LEN + len + LL_DATA_MIC_LEN) <= sizeof(lctrEncCtrlBuf));

    memcpy(lctrEncCtrlBuf, lctrSlvBisIsr.pCtrlBuf, LL_DATA_HDR_LEN + len);
    uint8_t *pPld = lctrEncCtrlBuf + LL_DATA_HDR_LEN;
    uint8_t *pTrl = pPld + len;

    if (lctrPktEncryptHdlr(&pBigCtx->ctrChan.enc, lctrEncCtrlBuf, pPld, pTrl))
    {
      #if (LL_ENABLE_TESTER)
        pTrl[0] ^= (llTesterCb.pktMic >>  0) & 0xFF;
        pTrl[1] ^= (llTesterCb.pktMic >>  8) & 0xFF;
        pTrl[2] ^= (llTesterCb.pktMic >> 16) & 0xFF;
        pTrl[3] ^= (llTesterCb.pktMic >> 24) & 0xFF;
      #endif
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Transmit a BIS Control PDU.
 *
 *  \param  pBigCtx     BIG context.
 */
/*************************************************************************************************/
static void lctrSlvBisTxControl(lctrBigCtx_t *pBigCtx)
{
  PalBbBleTxBufDesc_t desc;

  if (pBigCtx->encrypt && lctrPktEncryptHdlr)
  {
    desc.pBuf = lctrEncCtrlBuf;
  }
  else
  {
    desc.pBuf = lctrSlvBisIsr.pCtrlBuf;
  }
  desc.len = LL_DATA_HDR_LEN + desc.pBuf[LCTR_ISO_DATA_PDU_LEN_OFFSET];

  /* Update Control PDU flags. */
  LCTR_BIS_WR_CSSN(desc.pBuf[LCTR_ISO_DATA_PDU_FC_OFFSET], pBigCtx->bcp.cssn);
  LCTR_BIS_CLR_CSTF(desc.pBuf[LCTR_ISO_DATA_PDU_FC_OFFSET]);

  /* Set the control packet counter for inline encryption. */
  if (pBigCtx->encrypt && lctrSetEncryptPktCountHdlr)
  {
    uint64_t pktCtr = pBigCtx->eventCounter * pBigCtx->bn;
    lctrSetEncryptPktCountHdlr(pktCtr);
  }

  BbBleBisTxData(&desc, 1, 0, NULL);
}

/*************************************************************************************************/
/*!
 *  \brief  Transmit a BIS Data PDU.
 *
 *  \param  pBigCtx     BIG context.
 */
/*************************************************************************************************/
static void lctrSlvBisTxData(lctrBigCtx_t *pBigCtx)
{
  lctrBisCtx_t * const pBisCtx = pBigCtx->pBisCtx[lctrSlvBisIsr.se.bisEvtIdx];
  uint8_t descCnt;
  PalBbBleTxBufDesc_t *pTxDesc;

  /* Get BIS Data PDU. */

  PalBbBleTxBufDesc_t ptoTxDesc;
  if (lctrSlvBisIsr.se.ptIdx == 0)
  {
    pTxDesc = &lctrSlvBisTxDesc[lctrSlvBisIsr.se.bisEvtIdx][lctrSlvBisIsr.se.burstIdx][0];

    if (pTxDesc[1].pBuf)
    {
      if (pTxDesc[2].pBuf)
      {
        descCnt = 3;
      }
      else
      {
        descCnt = 2;
      }
    }
    else
    {
      descCnt = 1;
    }
  }
  else
  {
    ptoTxDesc.pBuf = lctrSlvBisIsr.emptyPdu;
    ptoTxDesc.len = sizeof(lctrSlvBisIsr.emptyPdu);

    pTxDesc = &ptoTxDesc;
    descCnt = 1;
  }

  /* Commit transmission. */

#if (LL_ENABLE_TESTER)
  /* Check subevent trigger for access address invalidation. */
  if ((llTesterCb.isoAccAddrSeTrigMask & (1 << lctrSlvBisIsr.se.burstIdx)) &&
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

  /* set the inline encryption packet counter to the current bisPayloadCounter */
  if (pBigCtx->encrypt && lctrSetEncryptPktCountHdlr)
  {
    uint64_t pktCtr = (pBigCtx->eventCounter * pBigCtx->bn) + lctrSlvBisIsr.se.burstIdx;
    lctrSetEncryptPktCountHdlr(pktCtr);
  }

  BbBleBisTxData(pTxDesc, descCnt,
                 pBigCtx->bod.dueUsec + lctrSlvBisIsr.nextSeOffs,
                 lctrSlvBisIsr.pNextChan);

  /* Post-commit calculation. */

  /* Now that channel data is set in BB, compute next channel information. */
  pBisCtx->chan.chanIdx = LmgrSelectNextSubEvtChannel(&pBisCtx->chSelInfo);
}

/*************************************************************************************************/
/*!
 *  \brief  BIS Tx Test payload generator.
 *
 *  \param  pBigCtx     BIG context.
 */
/*************************************************************************************************/
static void lctrSlvBisTxTestPayloadHandler(lctrBigCtx_t *pBigCtx)
{
  for (unsigned int i = 0; i < pBigCtx->numBis; i++)
  {
    lctrBisCtx_t * const pBisCtx = pBigCtx->pBisCtx[i];

    if (pBisCtx->test.enabled)
    {
      if (!pBisCtx->test.term)
      {
        uint8_t *pIsoBuf;
        uint32_t pldCtr;

        if (pBigCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED)
        {
          for (unsigned int j = 0; j < pBigCtx->bn; j++)
          {
            pldCtr = (pBigCtx->eventCounter * pBigCtx->bn) + j;
            if ((pIsoBuf = lctrGenerateIsoTestData(pBisCtx->handle, pBisCtx->test.pldType,
                                                   pBigCtx->maxSdu, pldCtr)) != NULL)
            {
              LctrTxIso(pIsoBuf);
            }
            else
            {
              /* Out of memory; gracefully continue execution. */
              return;
            }
          }
        }

        else /* LL_ISO_PDU_TYPE_FRAMED */
        {
          pldCtr = pBisCtx->test.util.framed.payloadCtr++;
          if ((pIsoBuf = lctrGenerateIsoTestData(pBisCtx->handle, pBisCtx->test.pldType,
                                                 pBigCtx->maxSdu, pldCtr)) != NULL)
          {
            LctrTxIso(pIsoBuf);
          }
          else
          {
            /* Out of memory; gracefully continue execution. */
            return;
          }
        }
      }
      else
      {
        /* Terminate ISO Test mode operation. */
        pBisCtx->test.enabled = FALSE;
        pBisCtx->test.term = FALSE;
        pBisCtx->test.util.framed.payloadCtr = 0;
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check for pending SDU fragments.
 *
 *  \param  pBigCtx     BIG context.
 *
 *  SDU fragments may be pending waiting on a missed SDU event. Call the ISOAL PDU assembler to
 *  obtain any pending data.
 */
/*************************************************************************************************/
static void lctrSlvCheckPendingSdu(lctrBigCtx_t *pBigCtx)
{
  for (unsigned int i = 0; i < pBigCtx->numBis; i++)
  {
    lctrBisCtx_t * const pBisCtx = pBigCtx->pBisCtx[i];

    uint8_t *pDataBuf;

    if (pBisCtx->roleData.slv.isoalTxCtx.pendQueueSize == 0)
    {
      /* No pending SDUs. */
      continue;
    }

    if ((pDataBuf = lctrTxIsoDataPduAlloc()) != NULL)
    {
      lctrIsoHdr_t isoHdr =
      {
        .handle = pBisCtx->handle,
        .tsFlag = FALSE
      };

      if ((isoHdr.sduLen = lctrAssembleTxFramedPdu(&pBisCtx->roleData.slv.isoalTxCtx, pDataBuf, pBisCtx->pBigCtx->maxPdu)) > 0)
      {
        lctrBisTxIsoPduQueue(pBisCtx, &isoHdr, pDataBuf);
      }
      else
      {
        /* Release unused buffer. */
        WsfMsgFree(pDataBuf);
      }
    }
    else
    {
      LL_TRACE_WARN0("Tx path flow controlled; flush pending SDU queue");

      uint8_t *pSduBuf;
      wsfHandlerId_t temp;

      while ((pSduBuf = WsfMsgDeq(&pBisCtx->roleData.slv.isoalTxCtx.pendingSduQ, &temp)) != NULL)
      {
        WsfMsgFree(pSduBuf);
        lctrIsoSduTxIncAvailBuf();
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  BIG Control Procedure handler.
 *
 *  \param  pBigCtx     BIG context.
 */
/*************************************************************************************************/
static void lctrSlvBigControlProcedureHandler(lctrBigCtx_t *pBigCtx)
{
  if (pBigCtx->bcp.actMsk)
  {
    uint16_t inst = (pBigCtx->eventCounter + 1) & 0xFFFF;

    if ((pBigCtx->bcp.actMsk & (1 << LL_BIG_OPCODE_CHAN_MAP_IND)) &&
        (inst == pBigCtx->bcp.chanMapUpd.inst))
    {
      pBigCtx->bcp.actMsk &= ~(1 << LL_BIG_OPCODE_CHAN_MAP_IND);

      lctrRemapBigChannels(pBigCtx, pBigCtx->bcp.chanMapUpd.chanMap);

      LL_TRACE_INFO2("Updated channel map: bigHandle=%u inst=%u", pBigCtx->handle, inst);

      if (pBigCtx->roleData.slv.pAdvSet)
      {
        /* Re-enable BIG Info transmissions. */
        LctrAcadBigInfo_t *pBigInfo = &pBigCtx->roleData.slv.pAdvSet->acadParams[LCTR_ACAD_ID_BIG_INFO].bigInfo;
        pBigInfo->chanMap = pBigCtx->bcp.chanMapUpd.chanMap;
        pBigInfo->hdr.state = LCTR_ACAD_STATE_ENABLED;
      }
    }

    /* Resume pending BIG Control Procedure. */
    if (pBigCtx->bcp.actMsk == 0)
    {
      if (pBigCtx->bcp.pendMsk & (1 << LL_BIG_OPCODE_CHAN_MAP_IND))
      {
        pBigCtx->bcp.pendMsk &= ~(1 << LL_BIG_OPCODE_CHAN_MAP_IND);
        lctrSlvBigSendMsg(pBigCtx, LCTR_SLV_BIG_MSG_CH_MAP_UPD);
      }
      else if (pBigCtx->bcp.pendMsk & (1 << LL_BIG_OPCODE_BIG_TERM_IND))
      {
        pBigCtx->bcp.pendMsk &= ~(1 << LL_BIG_OPCODE_BIG_TERM_IND);
        lctrSlvBigSendMsg(pBigCtx, LCTR_SLV_BIG_MSG_TERMINATE_BIG);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  BIS Tx Data PDU complete handler.
 *
 *  \param  pBigCtx     BIG context.
 */
/*************************************************************************************************/
static void lctrSlvBisTxDataPduHandler(lctrBigCtx_t *pBigCtx)
{
  /*** Tx completion ***/

  for (unsigned int i = 0; i < pBigCtx->numBis; i++)
  {
    lctrBisCtx_t * const pBisCtx = pBigCtx->pBisCtx[i];

    if (lctrSlvBisIsr.numTxFrags[i])
    {
      lctrBisTxQueuePopCleanup(pBisCtx, lctrSlvBisIsr.numTxFrags[i]);
    }
  }

  lctrNotifyIsoTxComplete(pBigCtx);
}

/**************************************************************************************************
  External Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Complete a transmitted BIS data buffer using sequential packing.
 *
 *  \param  pOp         Operation context.
 *  \param  status      Transmit status.
 */
/*************************************************************************************************/
void lctrSlvBisTxCompletionSequential(BbOpDesc_t *pOp, uint8_t status)
{
  lctrBigCtx_t * const pBigCtx = pOp->pCtx;

  if (status != BB_STATUS_SUCCESS)
  {
    LL_TRACE_WARN2("!!! Failed BIG transmission handle=%u ec[15:0]=%u", pBigCtx->handle, pBigCtx->eventCounter);

    BbSetBodTerminateFlag();
    return;
  }

  /* BIG Event limit (beyond BIS Control SubEvent). */

  if (lctrSlvBisIsr.se.bisEvtIdx >= pBigCtx->numBis)
  {
    BbSetBodTerminateFlag();
    return;
  }

  size_t numSePkts = (pBigCtx->bn * pBigCtx->irc) + lctrSlvBisIsr.se.ptIdx;

  if (!lctrBisCalcNextIdxSequential(pBigCtx, &lctrSlvBisIsr.se, numSePkts))
  {
    if (lctrSlvBisIsr.pCtrlBuf)
    {
      lctrSlvBisTxControl(pBigCtx);
    }
    else
    {
      BbSetBodTerminateFlag();
    }
    return;
  }

  /* Compute next channel. */
  lctrSeCtx_t nextSe = lctrSlvBisIsr.se;
  if (lctrBisCalcNextIdxSequential(pBigCtx, &nextSe, numSePkts))
  {
    lctrSlvBisIsr.pNextChan = &pBigCtx->pBisCtx[nextSe.bisEvtIdx]->chan;
  }
  else
  {
    if (lctrSlvBisIsr.pCtrlBuf == NULL)
    {
      lctrSlvBisIsr.pNextChan = NULL;
    }
    else
    {
      lctrSlvBisIsr.pNextChan = &pBigCtx->ctrChan;
    }
  }

  /* Compute next packet time. */
  lctrSlvBisIsr.nextSeOffs += pBigCtx->subInterUsec;

  lctrSlvBisTxData(pBigCtx);
}

/*************************************************************************************************/
/*!
 *  \brief  Complete a transmitted BIS data buffer using interleaved packing.
 *
 *  \param  pOp         Operation context.
 *  \param  status      Transmit status.
 */
/*************************************************************************************************/
void lctrSlvBisTxCompletionInterleaved(BbOpDesc_t *pOp, uint8_t status)
{
  lctrBigCtx_t * const pBigCtx = pOp->pCtx;

  if (status != BB_STATUS_SUCCESS)
  {
    LL_TRACE_WARN2("!!! Failed BIG transmission handle=%u ec[15:0]=%u", pBigCtx->handle, pBigCtx->eventCounter);

    BbSetBodTerminateFlag();
    return;
  }

  size_t numSePkts = (pBigCtx->bn * pBigCtx->irc) + lctrSlvBisIsr.se.ptIdx;

  /* BIG Event limit (beyond BIS Control SubEvent). */

  if (numSePkts > pBigCtx->nse)
  {
    BbSetBodTerminateFlag();
    return;
  }

  /* BIS loop. */

  if (!lctrBisCalcNextIdxInterleaved(pBigCtx, &lctrSlvBisIsr.se, numSePkts))
  {
    if (lctrSlvBisIsr.pCtrlBuf)
    {
      lctrSlvBisTxControl(pBigCtx);
    }
    else
    {
      BbSetBodTerminateFlag();
    }

    return;
  }

  /* Compute next channel. */
  lctrSeCtx_t nextSe = lctrSlvBisIsr.se;
  if (lctrBisCalcNextIdxInterleaved(pBigCtx, &nextSe, numSePkts))
  {
    lctrSlvBisIsr.pNextChan = &pBigCtx->pBisCtx[nextSe.bisEvtIdx]->chan;
  }
  else
  {
    if (lctrSlvBisIsr.pCtrlBuf == NULL)
    {
      lctrSlvBisIsr.pNextChan = NULL;
    }
    else
    {
      lctrSlvBisIsr.pNextChan = &pBigCtx->ctrChan;
    }
  }

  /* Compute next packet time. */
  lctrSlvBisIsr.nextSeOffs += pBigCtx->bisSpaceUsec;

  lctrSlvBisTxData(pBigCtx);
}

/*************************************************************************************************/
/*!
 *  \brief  Begin a BIG operation.
 *
 *  \param  pOp     Begin operation.
 */
/*************************************************************************************************/
void lctrSlvBigBeginOp(BbOpDesc_t *pOp)
{
  lctrBigCtx_t * const pBigCtx = pOp->pCtx;

  memset(&lctrSlvBisIsr, 0, sizeof(lctrSlvBisIsr));

  switch (pBigCtx->packing)
  {
    case LL_PACKING_INTERLEAVED:
      if (pBigCtx->numBis > 1)
      {
        lctrSlvBisIsr.pNextChan = &pBigCtx->pBisCtx[1]->chan;
      }
      else if (pBigCtx->nse > 1)
      {
        lctrSlvBisIsr.pNextChan = &pBigCtx->pBisCtx[0]->chan;
      }
      else
      {
        lctrSlvBisIsr.pNextChan = &pBigCtx->ctrChan;
      }

      lctrSlvBisIsr.nextSeOffs = pBigCtx->bisSpaceUsec;
      break;

    case LL_PACKING_SEQUENTIAL:
    default:
      if (pBigCtx->nse > 1)
      {
        lctrSlvBisIsr.pNextChan = &pBigCtx->pBisCtx[0]->chan;
      }
      else if (pBigCtx->numBis > 1)
      {
        lctrSlvBisIsr.pNextChan = &pBigCtx->pBisCtx[1]->chan;
      }
      else
      {
        lctrSlvBisIsr.pNextChan = &pBigCtx->ctrChan;
      }

      lctrSlvBisIsr.nextSeOffs = pBigCtx->subInterUsec;
      break;
  }

  /* Setup Control PDU */
  lctrSlvBisIsr.pCtrlBuf = lctrBigTxCtrlQueuePeek(pBigCtx);

  /* Pre-pack BIS Empty PDU. */
  lctrBisDataPduHdr_t hdr =
  {
    .llid = LL_LLID_ISO_EMPTY_PDU,
    .cssn = pBigCtx->bcp.cssn,
    .cstf = lctrSlvBisIsr.pCtrlBuf != NULL,
    .len = 0
  };
  uint8_t *pEmptyPdu = lctrSlvBisIsr.emptyPdu;
  lctrBisPackDataPduHdr(pEmptyPdu, &hdr);

  /* Commit first Data PDU. */
  lctrSlvBisSetupTxData(pBigCtx, 0, 0);
  lctrSlvBisTxData(pBigCtx);

  /* Now that the first Data PDU is committed, setup remaining Data PDUs. */
  for (unsigned int i = 0; i < pBigCtx->numBis; i++)
  {
    for (unsigned int j = 0; j < pBigCtx->bn; j++)
    {
      if (!((i == 0) && (j == 0)))  /* skip first Data PDU */
      {
        lctrSlvBisSetupTxData(pBigCtx, i, j);
      }
    }
  }

  /* Post setup processing. */

  if (!pBigCtx->roleData.slv.notifyHostEst)
  {
    pBigCtx->roleData.slv.notifyHostEst = TRUE;
    lctrNotifyHostCreateBigComplete(pBigCtx, LL_SUCCESS);
  }

  if (lctrSlvBisIsr.pCtrlBuf)
  {
    lctrMstBigControlEncrypt(pBigCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  End a BIS operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrSlvBigEndOp(BbOpDesc_t *pOp)
{
  lctrBigCtx_t * const pBigCtx = pOp->pCtx;

  /* Setup for next BIG Event. */

  while (TRUE)
  {
    /* Complete BIS Data PDU processing. */
    lctrSlvBisTxDataPduHandler(pBigCtx);
    /* TODO Incoming data payloadCounter should match scheduled eventCounter */

    /* BIS Control PDU accounting. */
    lctrBigTxCtrlQueuePop(pBigCtx);

    if (pBigCtx->state != LCTR_SLV_BIG_STATE_ENABLED)
    {
      if (pBigCtx->bcp.actMsk & (1 << LL_BIG_OPCODE_BIG_TERM_IND))
      {
        /* Ensure delivery of BIG_TERMINATE_IND is complete. */
        if (WsfQueueEmpty(&pBigCtx->roleData.slv.txCtrlQ))
        {
          lctrSlvBigSendMsg(pBigCtx, LCTR_SLV_BIG_MSG_TERMINATED);
          return;
        }
      }
      else /* Reset */
      {
        lctrSlvBigSendMsg(pBigCtx, LCTR_SLV_BIG_MSG_TERMINATED);
        return;
      }
    }

    /* Advance to next interval. */
    pBigCtx->eventCounter += 1;
    pOp->dueUsec += pBigCtx->isoInterUsec;

    /* Select next BIS Event channels. */
    lctrSelectBigChannels(pBigCtx);
    lctrSlvBigControlProcedureHandler(pBigCtx);

    if (SchInsertAtDueTime(pOp, NULL))
    {
      break;
    }

    LL_TRACE_WARN2("!!! BIG schedule conflict handle=%u, ec[15:0]=%u", pBigCtx->handle, pBigCtx->eventCounter);
  }

  /* SDU input. */

  lctrSlvBisTxTestPayloadHandler(pBigCtx);

  for (unsigned int i = 0; i < pBigCtx->numBis; i++)
  {
    lctrBisCtx_t * const pBisCtx = pBigCtx->pBisCtx[i];

    if (pBisCtx->roleData.slv.dataPathInCtx.id == LL_ISO_DATA_PATH_VS)
    {
      uint8_t *pSduBuf;
      if ((pSduBuf = WsfMsgAlloc(pLctrRtCfg->maxIsoSduLen + HCI_ISO_DL_MAX_LEN)) != NULL)
      {
        WSF_ASSERT(lctrCodecHdlr.inReq);
        lctrCodecHdlr.inReq(pBisCtx->handle, pSduBuf + HCI_ISO_HDR_LEN + HCI_ISO_DL_MAX_LEN, pBigCtx->maxSdu);
      }
      else
      {
        LL_TRACE_WARN1("!!! Out of memory; dropping input SDU, handle=%u", pBisCtx->handle);
      }
    }
  }

  /* Update BIG Info. */

  lctrAdvSet_t * const pAdvSet = pBigCtx->roleData.slv.pAdvSet;
  if (pAdvSet)
  {
    lctrAcadParam_t * const pAcad = &pAdvSet->acadParams[LCTR_ACAD_ID_BIG_INFO];
    pAcad->bigInfo.bigAnchorPoint = pOp->dueUsec;
    pAcad->bigInfo.bisPldCtr = pBigCtx->eventCounter * pBigCtx->bn;
  }

  /* SDU queue maintenance. */

  switch (pBigCtx->framing)
  {
    case LL_ISO_PDU_TYPE_FRAMED:
      lctrSlvCheckPendingSdu(pBigCtx);
      break;
    default:
      break;
  }

  /* Notifications. */

  if (lmgrCb.sendIsoCmplEvt)
  {
    lctrNotifyHostIsoEventComplete(pBigCtx->handle, (uint32_t) pBigCtx->eventCounter);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Abort a BIS operation.
 *
 *  \param  pOp     Completed operation.
 */
/*************************************************************************************************/
void lctrSlvBigAbortOp(BbOpDesc_t *pOp)
{
  WSF_ASSERT(pOp->protId == BB_PROT_BLE);
  WSF_ASSERT(pOp->prot.pBle->chan.opType == BB_BLE_OP_SLV_BIS_EVENT);

  LL_TRACE_WARN1("!!! BIG Broadcaster BOD aborted, eventCounter=%u", ((lctrBigCtx_t *)pOp->pCtx)->eventCounter);

  /* Clear state since abort is called without lctrMstBigBeginOp() initializing state. */
  memset(&lctrSlvBisIsr, 0, sizeof(lctrSlvBisIsr));

  lctrSlvBigEndOp(pOp);
}
