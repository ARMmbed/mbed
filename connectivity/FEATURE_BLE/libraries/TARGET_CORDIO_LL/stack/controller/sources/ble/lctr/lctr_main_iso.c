/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller connected isochronous stream main implementation file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_cis.h"
#include "lctr_int_bis.h"
#include "lctr_int_iso.h"
#include "hci_defs.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include "pal_codec.h"
#include <string.h>

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Codec event handlers (function pointers to decouple linker dependencies). */
lctrCodecHandlers_t lctrCodecHdlr;

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Process received ISO test data.
 *
 *  \param      pCisCtx     CIS context.
 *  \param      pDataBuf    Data buffer.
 *  \param      dataLen     Data length.
 */
/*************************************************************************************************/
static void lctrIsoProcessRxTestData(lctrCisCtx_t *pCisCtx, uint8_t *pDataBuf, uint8_t dataLen)
{
  uint16_t pldType = 0;
  uint64_t plTestCounter = pCisCtx->rxPktCounter;

  if (pCisCtx->rxPendInit && (dataLen >= LL_ISO_TEST_VAR_MIN_LEN))
  {
    memcpy(&pCisCtx->expectedPkt, pDataBuf, 4);
    pCisCtx->rxPendInit = FALSE;
    LL_TRACE_INFO1("lctrCisRxPostProcessing, Rx counter initialized to %d", pCisCtx->expectedPkt);
  }

  switch (pCisCtx->isoRxPldType)
  {
    case LL_ISO_PLD_TYPE_MAX_LEN:

      pldType = (pCisCtx->role == LL_ROLE_MASTER) ? pCisCtx->sduSizeSToM : pCisCtx->sduSizeMToS;
      break;

    case LL_ISO_PLD_TYPE_VAR_LEN:
      pldType = plTestCounter * 4;
      pldType = WSF_MIN(pCisCtx->localDataPdu.maxRxLen, pldType);
      break;

    case LL_ISO_PLD_TYPE_ZERO_LEN:
    default:
      break;
  }

  uint32_t pktNum = 0;
  if (dataLen >= LL_ISO_TEST_VAR_MIN_LEN)
  {
    memcpy(&pktNum, pDataBuf, 4);
  }

  switch (pCisCtx->isoRxPldType)
  {
    case LL_ISO_PLD_TYPE_MAX_LEN:
      if ((dataLen == pldType) &&
          (pktNum == pCisCtx->expectedPkt))
      {
        pCisCtx->expectedPkt++;
        pCisCtx->numRxSuccess++;

      }
      else
      {
        pCisCtx->expectedPkt = pktNum + 1;
        pCisCtx->numRxFailed++;

      }
      break;

    case LL_ISO_PLD_TYPE_VAR_LEN:
      if ((dataLen >= LL_ISO_TEST_VAR_MIN_LEN) &&
          (pktNum == pCisCtx->expectedPkt))
      {
        pCisCtx->numRxSuccess++;
        pCisCtx->expectedPkt++;
      }
      else
      {
        pCisCtx->expectedPkt = pktNum + 1;
        pCisCtx->numRxFailed++;
      }
      break;

    case LL_ISO_PLD_TYPE_ZERO_LEN:
      if (dataLen == 0)
      {
        pCisCtx->numRxSuccess++;
      }
      else
      {
        pCisCtx->numRxFailed++;
      }
      break;

    default:
      LL_TRACE_ERR1("Invalid value pldType=%u", pldType);
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Enable ISO Tx test.
 *
 *  \param      pCisCtx     CIS context.
 *  \param      pldType     Payload length type.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
static uint8_t lctrCisTxTest(lctrCisCtx_t *pCisCtx, uint8_t pldType)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);

  if (((pCisCtx->bnMToS == 0) && (pCisCtx->role == LL_ROLE_MASTER)) ||
      ((pCisCtx->bnSToM == 0) && (pCisCtx->role == LL_ROLE_SLAVE)))
  {
    LL_TRACE_WARN0("Transmit burst number must be greater than 0");
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }

  if (pldType > LL_ISO_PLD_TYPE_MAX_LEN)
  {
    LL_TRACE_WARN1("Unknown payload type=%u", pldType);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  pCisCtx->testTxPktCtr = 0;
#if (LL_ENABLE_TESTER)
  pCisCtx->testTxPktCtr = llTesterCb.isoTxTestNumInit;
#endif
  pCisCtx->testSduTs = pCigCtx->cigBod.dueUsec;
  pCisCtx->testPldType = pldType;
  pCisCtx->txTestEnabled = TRUE;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      CIS read ISO test counters.
 *
 *  \param      pCisCtx     CIS context.
 *  \param      pStats      Pointer to the statistics block.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
static uint8_t LctrCisReadTestCounters(lctrCisCtx_t *pCisCtx, LlIsoTestCtrs_t *pStats)
{

  if (!pCisCtx->rxTestEnabled &&
      !pCisCtx->txTestEnabled)
  {
    LL_TRACE_WARN0("Invalid ISO Test state, test mode must be enabled");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (pCisCtx->rxTestEnabled)
  {
    pStats->numSuccess = pCisCtx->numRxSuccess;
    pStats->numMissed = pCisCtx->numRxMissed;
    pStats->numFailed = pCisCtx->numRxFailed;
  }
  else /* if (pCisCtx->txTestEnabled) */
  {
    pStats->numSuccess = 0;
    pStats->numMissed = 0;
    pStats->numFailed = 0;
  }

  return LL_SUCCESS;
}

/**************************************************************************************************
  External Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Initialize codec resources.
 *
 *  Map codec handlers for codec operations. Function pointer abstraction is used to decouple
 *  linker dependencies when a codec is not required by the system.
 */
/*************************************************************************************************/
void LctrInitCodec(void)
{
  /* Add codec. */
  lctrCodecHdlr.start = PalCodecDataStartStream;
  lctrCodecHdlr.stop  = PalCodecDataStopStream;
  lctrCodecHdlr.inReq = PalCodecDataStreamIn;
  lctrCodecHdlr.out   = PalCodecDataStreamOut;
}

/*************************************************************************************************/
/*!
 *  \brief      Tx data completed task event handler.
 */
/*************************************************************************************************/
void lctrIsoTxCompletedHandler(void)
{
  WSF_CS_INIT(cs);

  uint8_t numHandles = 0;
  uint16_t handle[LL_MAX_CIS] = { 0 };
  uint16_t numSdu[LL_MAX_CIS] = { 0 };

  /* Cache buffer count within single CS. */
  WSF_CS_ENTER(cs);
  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if (pCisCtx->enabled && !pCisCtx->txTestEnabled)
    {
      if (pCisCtx->numTxComp && (pCisCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED))
      {
        numSdu[numHandles] = pCisCtx->numTxComp;
        handle[numHandles] = pCisCtx->cisHandle;
        pCisCtx->numTxComp = 0;
        numHandles++;
      }
      else if ((pCisCtx->framing == LL_ISO_PDU_TYPE_FRAMED) && pCisCtx->isoalTxCtx.compSdu)
      {
        numSdu[numHandles] = pCisCtx->isoalTxCtx.compSdu;
        handle[numHandles] = pCisCtx->cisHandle;
        pCisCtx->numTxComp = 0; /* numTxComp is still recorded in framed, so clear it here. */
        pCisCtx->isoalTxCtx.compSdu = 0;
        numHandles++;
      }
    }
  }

  WSF_CS_EXIT(cs);

  if (lmgrPersistCb.sendIsoCompCback && numHandles)
  {
    lmgrPersistCb.sendIsoCompCback(numHandles, handle, numSdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Rx data pending task event handler.
 */
/*************************************************************************************************/
void lctrCisRxPendingHandler(void)
{
  uint16_t cisHandle = 0;
  uint8_t *pRxBuf;
  lctrCisCtx_t *pCisCtx;

  /* Route and demux received Data PDUs. */
  while ((pRxBuf = lctrCisRxDeq(&cisHandle)) != NULL)
  {
    WSF_ASSERT(pRxBuf);

    pCisCtx = lctrFindCisByHandle(cisHandle);
    lctrIsoalRxCtx_t *pRxCtx = &pCisCtx->isoalRxCtx;

    if (!pCisCtx->enabled)
    {
      LL_TRACE_ERR1("!!! Data received on terminated cisHandle=%u", cisHandle);
      lctrCisRxPduFree(pRxBuf);
      continue;
    }

    /* Disassemble PDU. */
    lctrCisDataPduHdr_t rxHdr;
    lctrCisUnpackDataPduHdr(&rxHdr, pRxBuf);

    /* Decrypt PDU. */
    if (lctrPktDecryptHdlr)
    {
      if (lctrPktDecryptHdlr(&pCisCtx->bleData.chan.enc, pRxBuf) == FALSE)
      {
        LL_TRACE_ERR1("!!! MIC verification failed on connHandle=%u", cisHandle);
        LL_TRACE_ERR1("!!! MIC verification failed on cisEvtCounter=%u", pCisCtx->cisEvtCounter);
        lctrCisRxPduFree(pRxBuf);
        lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_TERM_MIC_FAILED);
        continue;
      }
      /* Increase packet counter after decryption. */
      lctrCisIncPacketCounterRx(pCisCtx);
    }

    lctrCisDataPduHdr_t cisDataHdr;
    lctrCisUnpackDataPduHdr(&cisDataHdr, pRxBuf);

    /* Demux PDU. */
    if (pCisCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED)
    {
      lctrIsoHdr_t isoHdr =
      {
        /* ISO header */
        .handle = cisHandle,
        .len    = pRxBuf[LCTR_ISO_DATA_PDU_LEN_OFFSET],
        /* .pb  = 0, */  /* assigned below */
        .tsFlag = FALSE,

        /* Data load */
        .ts     = 0,
        /* .pktSn = 0, */ /* assigned below */
        .sduLen = pRxBuf[LCTR_ISO_DATA_PDU_LEN_OFFSET],
        .ps     = LCTR_PS_VALID
      };

      switch (rxHdr.llid)
      {
        /* Received a end/complete PDU. */
        case LL_LLID_ISO_UNF_END_PDU:
          switch (pRxCtx->rxState)
          {
          case LL_ISO_SDU_STATE_NEW:
            isoHdr.pb = LCTR_PB_COMP;
            break;
          case LL_ISO_SDU_STATE_CONT:
            isoHdr.pb = LCTR_PB_LAST;
            pRxCtx->rxState = LL_ISO_SDU_STATE_NEW;
            break;
          default:
            lctrCisRxPduFree(pRxBuf);
            LL_TRACE_ERR2("!!! Invalid rxState; dropping Rx data PDU, connHandle=%u, rxState=%u", cisHandle, pRxCtx->rxState);
            pRxCtx->rxState = LL_ISO_SDU_STATE_NEW;
            continue;
          }
          break;

        /* Received a continuation/start PDU. */
        case LL_LLID_ISO_UNF_CONT_PDU:
          isoHdr.pb = (pRxCtx->rxState == LL_ISO_SDU_STATE_NEW) ? LCTR_PB_FIRST : LCTR_PB_CONT;
          pRxCtx->rxState = LL_ISO_SDU_STATE_CONT;
          break;

        /* Unknown LLID. */
        default:
          lctrCisRxPduFree(pRxBuf);
          LL_TRACE_ERR2("!!! Invalid LLID; dropping Rx data PDU, connHandle=%u llid=%u", cisHandle, rxHdr.llid);
          continue;
      }

      /* Pack isoHdr and queue PDU. */
      if (pRxBuf)
      {
        uint8_t * pSduBuf = pRxBuf - LCTR_CIS_DATA_PDU_START_OFFSET - HCI_ISO_DL_MAX_LEN;

        if ((isoHdr.pb == LCTR_PB_COMP) || (isoHdr.pb == LCTR_PB_FIRST))
        {
          isoHdr.tsFlag = TRUE;
          isoHdr.ts = PalBbGetCurrentTime();

          pRxCtx->packetSequence++;
        }
        isoHdr.pktSn = pRxCtx->packetSequence;

        /* LCTR_PB_COMP and LCTR_PB_FIRST will have their headers re-packed in lctrRecombineRxUnframedSdu(). */
        uint8_t headerOffset = lctrIsoPackHdr(pSduBuf, &isoHdr);

        /* Process received buffer for Rx testing purpose. */
        /* TODO: Optimize test packet dataflow. */
        if (pCisCtx->rxTestEnabled == TRUE)
        {
          lctrIsoProcessRxTestData(pCisCtx, pRxBuf + LL_DATA_HDR_LEN, rxHdr.len);
        }

        /* Move payload next to header. */
        if ((LCTR_CIS_DATA_PDU_START_OFFSET + LL_ISO_DATA_HDR_LEN) > HCI_ISO_HDR_LEN)
        {
          /* TODO optimize memory layout */
          memmove(pSduBuf + headerOffset , pRxBuf + LL_DATA_HDR_LEN, rxHdr.len);
        }

        /* Put onto pending queue until whole SDU is ready to be sent. */
        if (!lctrRecombineRxUnframedSdu(pRxCtx, pSduBuf))
        {
          break;
        }

        /* If it is time to enqueue the SDU, do so here. */
        uint8_t handlerId;
        while ((pSduBuf = WsfMsgDeq(&pRxCtx->data.unframed.pendSduQ, &handlerId)) != NULL)
        {
          /* Enqueue SDU for processing. */
          if (!lctrIsoRxConnEnq(&pCisCtx->dataPathOutCtx, pCisCtx->cisHandle,
                                pCisCtx->rxPktCounter - 1, pSduBuf))
          {
            /* The buffer was not freed, so free it now. */
            WsfMsgFree(pSduBuf);
          }
        }
      }
    }
    else /* LL_ISO_PDU_TYPE_FRAMED */
    {
      switch (rxHdr.llid)
      {
        case LL_LLID_ISO_FRA_PDU:
        {
          pCisCtx->dataPathOutCtx.cfg.hci.numRxPend += lctrAssembleRxFramedSdu(pRxCtx, &pCisCtx->dataPathOutCtx.cfg.hci.rxDataQ, pCisCtx->cisHandle, pRxBuf, cisDataHdr.len);

          /* Consume packets if needed */
          lctrIsoHdr_t isoHdr;
          /* ISO test mode. */
          if (pCisCtx->rxTestEnabled == TRUE)
          {
            while (pCisCtx->dataPathOutCtx.cfg.hci.numRxPend)
            {
              uint8_t *pTestRxBuf = lctrIsoRxConnDeq(&pCisCtx->dataPathOutCtx);
              lctrIsoUnpackHdr(&isoHdr, pTestRxBuf);
              lctrIsoProcessRxTestData(pCisCtx, pTestRxBuf+ HCI_ISO_HDR_LEN + HCI_ISO_DL_MAX_LEN, isoHdr.sduLen);
              WsfMsgFree(pTestRxBuf);
              LctrRxIsoComplete(1);
              pCisCtx->dataPathOutCtx.cfg.hci.numRxPend--;
            }
          }
          /* Codec output */
          else if (pCisCtx->dataPathOutCtx.id == LL_ISO_DATA_PATH_VS)
          {
            while (pCisCtx->dataPathOutCtx.cfg.hci.numRxPend)
            {
              uint8_t *pSduBuf = lctrIsoRxConnDeq(&pCisCtx->dataPathOutCtx);
              lctrIsoUnpackHdr(&isoHdr, pSduBuf);
              lctrCodecHdlr.out(pCisCtx->dataPathOutCtx.cfg.codec.streamId, isoHdr.pSdu, isoHdr.sduLen, isoHdr.ts);
              WsfMsgFree(pSduBuf);
              LctrRxIsoComplete(1);
            }
          }

          lctrCisRxPduFree(pRxBuf);
          break;
        }

        default:
          lctrCisRxPduFree(pRxBuf);
          LL_TRACE_ERR2("!!! Invalid LLID; dropping Rx data PDU, connHandle=%u llid=%u", cisHandle, rxHdr.llid);
          break;
      }
    }
  }

  /* Notify host of pending Rx data. */
  WSF_CS_INIT(cs);
  uint8_t numHandles = 0;
  uint16_t numSdu[LL_MAX_CIS] = { 0 };
  uint16_t handle[LL_MAX_CIS] = { 0 };

  /* Cache buffer count within single CS. */
  WSF_CS_ENTER(cs);
  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    pCisCtx = &pLctrCisTbl[i];

    if (pCisCtx->enabled &&
        (pCisCtx->dataPathOutCtx.id == LL_ISO_DATA_PATH_HCI) &&
        pCisCtx->dataPathOutCtx.cfg.hci.numRxPend)
    {
      handle[numHandles] = pCisCtx->cisHandle;
      numSdu[numHandles] = pCisCtx->dataPathOutCtx.cfg.hci.numRxPend;
      pCisCtx->dataPathOutCtx.cfg.hci.numRxPend = 0;
      numHandles++;
    }
  }
  WSF_CS_EXIT(cs);

  if (lmgrPersistCb.recvIsoPendCback && numHandles)
  {
    /* Notify host. */
    lmgrPersistCb.recvIsoPendCback(numHandles, handle, numSdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize ISO memory resources.
 *
 *  \param  pFreeMem    Pointer to free memory.
 *  \param  freeMemSize Size of pFreeMem.
 *
 *  \return Amount of free memory consumed.
 */
/*************************************************************************************************/
uint16_t LctrInitIsoMem(uint8_t *pFreeMem, uint32_t freeMemSize)
{
  uint8_t *pAvailMem = pFreeMem;

  if (((uint32_t)pAvailMem) & 3)
  {
    /* Align to next word. */
    pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
  }

  /*** Tx Memory ***/

  freeMemSize -= pAvailMem - pFreeMem;
  uint16_t memUsed = lctrIsoTxInitMem(pAvailMem, freeMemSize);
  if (memUsed == 0)
  {
    LL_TRACE_ERR2("LctrInitIsoMem: failed to allocate descriptors, need=%u available=%u", (pAvailMem - pFreeMem), freeMemSize);
    WSF_ASSERT(FALSE);
    return 0;
  }

  pAvailMem += memUsed;

  return (pAvailMem - pFreeMem);
}

/*************************************************************************************************/
/*!
 *  \brief  Transmit ISO data path.
 *
 *  \param  pIsoBuf     ISO buffer.
 */
/*************************************************************************************************/
void LctrTxIso(uint8_t *pIsoBuf)
{
  lctrIsoHdr_t isoHdr;

  /*** Disassemble ISO packet. ***/

  lctrIsoUnpackHdr(&isoHdr, pIsoBuf);

  if (isoHdr.sduLen > pLctrRtCfg->maxIsoSduLen)
  {
    LL_TRACE_ERR2("Invalid ISO header: invalid packet length, actLen=%u, maxLen=%u", isoHdr.sduLen, pLctrRtCfg->maxIsoSduLen);
    WsfMsgFree(pIsoBuf);
    if (lmgrPersistCb.sendCompCback)
    {
      lmgrPersistCb.sendCompCback(isoHdr.handle, 1);
    }
    return;
  }

  if (lmgrIsoCb.availTxBuf == 0)
  {
    LL_TRACE_ERR1("ISO Tx path flow controlled, handle=%u", isoHdr.handle);
    WsfMsgFree(pIsoBuf);
    if (lmgrPersistCb.sendCompCback)
    {
      lmgrPersistCb.sendCompCback(isoHdr.handle, 1);
    }
    return;
  }

  uint16_t expIsoLen = isoHdr.len - HCI_ISO_DL_MIN_LEN - ((isoHdr.tsFlag) ? HCI_ISO_TS_LEN : 0);
  if (isoHdr.sduLen != expIsoLen)
  {
    LL_TRACE_ERR2("Invalid ISO header: packet length mismatch, expSduLen=%u, actSduLen=%u", expIsoLen, isoHdr.sduLen);
    WsfMsgFree(pIsoBuf);
    if (lmgrPersistCb.sendCompCback)
    {
      lmgrPersistCb.sendCompCback(isoHdr.handle, 1);
    }
    return;
  }

  /*** Resolve ISO context. ***/

  lctrCisCtx_t *pCisCtx;
  lctrBisCtx_t *pBisCtx;

  if ((pCisCtx = lctrFindCisByHandle(isoHdr.handle)) != NULL)
  {
    if (!lctrCheckIsCisEstCis(pCisCtx->cisHandle))
    {
      LL_TRACE_ERR1("Invalid ISO handle: link not established cisHandle=%u; dropping packet", isoHdr.handle);
      if (lmgrPersistCb.sendCompCback)
      {
        lmgrPersistCb.sendCompCback(isoHdr.handle, 1);
      }
      WsfMsgFree(pIsoBuf);
      return;
    }

    if ((pCisCtx->localDataPdu.maxTxLen == 0) ||
        ((pCisCtx->bnMToS == 0) && (pCisCtx->role == LL_ROLE_MASTER)) ||
        ((pCisCtx->bnSToM == 0) && (pCisCtx->role == LL_ROLE_SLAVE)))
    {
      LL_TRACE_ERR1("Invalid CIS state: handle=%u does not accept transmissions; dropping packet", isoHdr.handle);
      WsfMsgFree(pIsoBuf);
      if (lmgrPersistCb.sendCompCback)
      {
        lmgrPersistCb.sendCompCback(isoHdr.handle, 1);
      }
      return;
    }

    if (pCisCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED)
    {
      lctrIsoSduTxDecAvailBuf();

      /*** Queue for transmit. ***/

      lctrCisTxDataPduQueue(pCisCtx, &isoHdr, pIsoBuf);
    }
    else /* LL_ISO_PDU_TYPE_FRAMED */
    {
      /* Queue up for transmission. */
      WsfMsgEnq(&pCisCtx->isoalTxCtx.pendingSduQ, 0, pIsoBuf);
      pCisCtx->isoalTxCtx.pendQueueSize++;

      /* PDU assembly handled in the EndOp. */
    }
  }
  else if ((pBisCtx = lctrFindBisByHandle(isoHdr.handle)) != NULL)
  {
    if (isoHdr.sduLen > pBisCtx->pBigCtx->maxSdu)
    {
      LL_TRACE_ERR2("Invalid ISO header: invalid packet length, actLen=%u, maxSdu=%u", isoHdr.sduLen, pBisCtx->pBigCtx->maxSdu);
      WsfMsgFree(pIsoBuf);
      if (lmgrPersistCb.sendCompCback)
      {
        lmgrPersistCb.sendCompCback(isoHdr.handle, 1);
      }
      return;
    }

    lctrIsoSduTxDecAvailBuf();

    /*** Queue for transmit. ***/

    if (pBisCtx->pBigCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED)
    {
      lctrBisTxIsoPduQueue(pBisCtx, &isoHdr, pIsoBuf);
    }
    else /* LL_ISO_PDU_TYPE_FRAMED */
    {
      /* Queue up for transmission. */
      WsfMsgEnq(&pBisCtx->roleData.slv.isoalTxCtx.pendingSduQ, 0, pIsoBuf);
      pBisCtx->roleData.slv.isoalTxCtx.pendQueueSize++;
    }
  }
  else
  {
    LL_TRACE_ERR1("Invalid ISO handle: unknown handle handle=%u; dropping packet", isoHdr.handle);
    WsfMsgFree(pIsoBuf);
    /* Do not inform host of the number of completed event. */
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Receive ISO data path.
 *
 *  \return Received data PDU buffer or NULL if queue empty or data consumed by a data path.
 */
/*************************************************************************************************/
uint8_t *LctrRxIso(void)
{
  uint8_t *pBuf;

  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if ((pCisCtx->enabled) && (pCisCtx->dataPathOutCtx.id ==  LL_ISO_DATA_PATH_HCI) &&
        ((pBuf = lctrIsoRxConnDeq(&pCisCtx->dataPathOutCtx)) != NULL))
    {
      return pBuf;
    }
  }

  for (unsigned int i = 0; i < pLctrRtCfg->maxBis; i++)
  {
    lctrBisCtx_t *pBisCtx = &pLctrBisTbl[i];

    if ((pBisCtx->enabled) &&
        (pBisCtx->pBigCtx->role == LL_ROLE_MASTER) &&
        ((pBuf = lctrBisRxIsoSduDeq(pBisCtx)) != NULL))
    {
      /* Postpone lctrIsoDataRxIncAvailBuf() until client consumes buffer, cf. LctrRxIsoComplete(). */
      return pBuf;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Indicate that received ISO data buffer has been deallocated
 *
 *  \param      numBufs     Number of completed packets.
 *
 *  Indicate that received ISO data buffer has been deallocated.
 */
/*************************************************************************************************/
void LctrRxIsoComplete(uint8_t numBufs)
{
  lctrIsoDataRxIncAvailBuf(numBufs);
}

/*************************************************************************************************/
/*!
 *  \brief      Used to identify and enable the isochronous data path between the host and the
 *              controller for each connected isochronous or broadcast isochronous stream.
 *
 *  \param      handle      BIS or CIS handle.
 *  \param      pPktSn      Packet sequence number.
 *  \param      pTs         Timestamp.
 *  \param      pTimeOffs   Time offset.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LctrReadIsoTxSync(uint16_t handle, uint16_t *pPktSn, uint32_t *pTs, uint32_t *pTimeOffs)
{
  uint8_t status = LL_SUCCESS;
  lctrCisCtx_t *pCisCtx;
  lctrBisCtx_t *pBisCtx;

  if ((pCisCtx = lctrFindCisByHandle(handle)) != NULL)
  {
    lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);

    *pPktSn = pCisCtx->cisEvtCounter;
    *pTs = pCigCtx->cigBod.dueUsec;
    *pTimeOffs = 0;
  }
  else if ((pBisCtx = lctrFindBisByHandle(handle)) != NULL)
  {
    *pPktSn = pBisCtx->pBigCtx->eventCounter * pBisCtx->pBigCtx->bn;
    *pTs = pBisCtx->pBigCtx->bod.dueUsec;
    *pTimeOffs = 0;
  }
  else
  {
    LL_TRACE_WARN1("LctrReadIsoTxSync: handle=%u not found", handle);
    status = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  return status;
}

/*************************************************************************************************/
/*!
 * \brief       Used to request the Controller to configure the data transport path in a given
 *              direction between the Controller and the Host.
 *
 * \param       pConfigDataPath  Parameters for configure data path.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LctrConfigureDataPath(LlIsoConfigDataPath_t *pConfigDataPath)
{
  switch (pConfigDataPath->dpId)
  {
  case LL_ISO_DATA_PATH_DISABLED:
  case LL_ISO_DATA_PATH_HCI:
    /* No action required. */
    break;
  case LL_ISO_DATA_PATH_VS:
    /* No action required. */
    break;
  default:
    LL_TRACE_WARN2("LlConfigureDataPath: unknown data path, dpDir=%u, dpId=%u", pConfigDataPath->dpDir, pConfigDataPath->dpId);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Used to identify and enable the isochronous data path between the host and the
 *              controller for each connected isochronous stream or broadcast isochronous stream.
 *
 *  \param      pSetupDataPath   Parameters for setup ISO data path.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LctrSetupIsoDataPath(LlIsoSetupDataPath_t *pSetupDataPath)
{
  lctrCisCtx_t *pCisCtx;
  lctrBisCtx_t *pBisCtx;
  lctrDpParams_t dpParam;

  if ((pCisCtx = lctrFindCisByHandle(pSetupDataPath->handle)) != NULL)
  {
    dpParam.handle = pCisCtx->cisHandle;
    dpParam.isoInt = (pCisCtx->role == LL_ROLE_SLAVE) ? pCisCtx->sduIntervalSToM : pCisCtx->sduIntervalMToS;
    dpParam.pktCtr = pCisCtx->txPktCounter + 1;
    dpParam.dpDir = pSetupDataPath->dpDir;

    switch (pSetupDataPath->dpDir)
    {
    case LL_ISO_DATA_DIR_INPUT:
      dpParam.pDataPathCtx = (lctrDataPathCtx_t *) (&pCisCtx->dataPathInCtx);
      break;
    case LL_ISO_DATA_DIR_OUTPUT:
      dpParam.pDataPathCtx = (lctrDataPathCtx_t *) (&pCisCtx->dataPathOutCtx);
      break;
    default:
      LL_TRACE_WARN2("LctrSetupIsoDataPath: handle=%u invalid direction dpDir=%u", pSetupDataPath->handle, pSetupDataPath->dpDir);
      return LL_ERROR_CODE_CMD_DISALLOWED;
    }
  }

  else if ((pBisCtx = lctrFindBisByHandle(pSetupDataPath->handle)) != NULL)
  {
    dpParam.handle = pBisCtx->handle;
    dpParam.isoInt = pBisCtx->pBigCtx->isoInterUsec;
    dpParam.pktCtr = (pBisCtx->pBigCtx->eventCounter + 1) * pBisCtx->pBigCtx->bn;
    dpParam.dpDir  = pSetupDataPath->dpDir;

    switch (pSetupDataPath->dpDir)
    {
    case LL_ISO_DATA_DIR_INPUT:
      if (pBisCtx->pBigCtx->role == LL_ROLE_SLAVE)
      {
        dpParam.pDataPathCtx = (lctrDataPathCtx_t *) (&pBisCtx->roleData.slv.dataPathInCtx);
      }
      else
      {
        LL_TRACE_WARN1("LctrSetupIsoDataPath: handle=%u invalid input direction for master", pSetupDataPath->handle);
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
      break;
    case LL_ISO_DATA_DIR_OUTPUT:
      if (pBisCtx->pBigCtx->role == LL_ROLE_MASTER)
      {
        dpParam.pDataPathCtx = (lctrDataPathCtx_t *) (&pBisCtx->roleData.mst.dataPathOutCtx);
      }
      else
      {
        LL_TRACE_WARN1("LctrSetupIsoDataPath: handle=%u invalid output direction for slave", pSetupDataPath->handle);
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
      break;
    default:
      LL_TRACE_WARN2("LctrSetupIsoDataPath: handle=%u invalid direction dpDir=%u", pSetupDataPath->handle, pSetupDataPath->dpDir);
      return LL_ERROR_CODE_CMD_DISALLOWED;
      break;
    }
  }

  else
  {
    LL_TRACE_WARN1("LctrSetupIsoDataPath: handle=%u not found", pSetupDataPath->handle);
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  return lctrIsoSetupDataPath(&dpParam, pSetupDataPath);
}

/*************************************************************************************************/
/*!
 *  \brief      Used to remove the isochronous data path associated with the
 *              connected isochronous stream or broadcast isochronous stream.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      dpDir       Direction of data path to remove.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LctrRemoveIsoDataPath(uint16_t handle, uint8_t dpDir)
{
  uint8_t status = LL_SUCCESS;
  lctrCisCtx_t *pCisCtx;
  lctrBisCtx_t *pBisCtx;
  lctrDpParams_t dpParam;

  if ((pCisCtx = lctrFindCisByHandle(handle)) != NULL)
  {
    /* Check to make sure parameters are valid. */
    if ((dpDir & ~(LL_ISO_DATA_PATH_INPUT_BIT | LL_ISO_DATA_PATH_OUTPUT_BIT)) ||
        (dpDir == 0))
    {
      LL_TRACE_WARN2("LctrRemoveIsoDataPath: handle=%u invalid direction dpDir=0x%08x", handle, dpDir);
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }

    /* Check for validity of parameters before operating on them. */
    if (dpDir & LL_ISO_DATA_PATH_INPUT_BIT)
    {
      if (pCisCtx->dataPathInCtx.id == LL_ISO_DATA_PATH_DISABLED)
      {
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
    }
    if (dpDir & LL_ISO_DATA_PATH_OUTPUT_BIT)
    {
      if (pCisCtx->dataPathOutCtx.id == LL_ISO_DATA_PATH_DISABLED)
      {
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
    }

    if (dpDir & LL_ISO_DATA_PATH_INPUT_BIT)
    {
      /* Stop input data path. */
      dpParam.handle = pCisCtx->cisHandle;
      dpParam.pDataPathCtx = (lctrDataPathCtx_t *)&pCisCtx->dataPathInCtx;
      /* Other parameters are unused in data path clearing. */
      lctrIsoInDataPathClear(&dpParam);

      pCisCtx->dataPathInCtx.id = LL_ISO_DATA_PATH_DISABLED;
    }
    if (dpDir & LL_ISO_DATA_PATH_OUTPUT_BIT)
    {
      /* Stop output data path. */
      dpParam.handle = pCisCtx->cisHandle;
      dpParam.pDataPathCtx = (lctrDataPathCtx_t *)&pCisCtx->dataPathOutCtx;
      /* Other parameters are unused in data path clearing. */
      lctrIsoOutDataPathClear(&dpParam);

      pCisCtx->dataPathOutCtx.id = LL_ISO_DATA_PATH_DISABLED;
    }
  }
  else if ((pBisCtx = lctrFindBisByHandle(handle)) != NULL)
  {
    if (dpDir & LL_ISO_DATA_PATH_INPUT_BIT)
    {
      if (pBisCtx->roleData.slv.dataPathInCtx.id == LL_ISO_DATA_PATH_DISABLED)
      {
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
    }
    if (dpDir & LL_ISO_DATA_PATH_OUTPUT_BIT)
    {
      if (pBisCtx->roleData.mst.dataPathOutCtx.id == LL_ISO_DATA_PATH_DISABLED)
      {
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
    }

    dpParam.handle = pBisCtx->handle;

    if (pBisCtx->pBigCtx->role == LL_ROLE_MASTER)
    {
      if (pBisCtx->roleData.mst.dataPathOutCtx.id == LL_ISO_DATA_PATH_DISABLED)
      {
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }

      dpParam.pDataPathCtx = (lctrDataPathCtx_t *)&pBisCtx->roleData.mst.dataPathOutCtx;
      /* Other parameters are unused in data path clearing. */
      lctrIsoOutDataPathClear(&dpParam);
      pBisCtx->roleData.mst.dataPathOutCtx.id = LL_ISO_DATA_PATH_DISABLED;
    }
    else
    {
      if (pBisCtx->roleData.slv.dataPathInCtx.id == LL_ISO_DATA_PATH_DISABLED)
      {
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }

      dpParam.pDataPathCtx = (lctrDataPathCtx_t *)&pBisCtx->roleData.slv.dataPathInCtx;
      /* Other parameters are unused in data path clearing. */
      lctrIsoInDataPathClear(&dpParam);
      pBisCtx->roleData.slv.dataPathInCtx.id = LL_ISO_DATA_PATH_DISABLED;
    }
  }
  else
  {
    LL_TRACE_WARN1("LctrRemoveIsoDataPath: handle=%u not found", handle);
    status = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      Enable ISO Tx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pldType     Payload length type.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LctrIsoTxTest(uint16_t handle, uint8_t pldType)
{
  uint8_t status;
  lctrCisCtx_t *pCisCtx;
  lctrBisCtx_t *pBisCtx;

  if ((pCisCtx = lctrFindCisByHandle(handle)) != NULL)
  {
    status = lctrCisTxTest(pCisCtx, pldType);
  }
  else if ((pBisCtx = lctrFindBisByHandle(handle)) != NULL)
  {
    status = lctrBisTxTest(pBisCtx, pldType);
  }
  else
  {
    LL_TRACE_WARN0("LctrIsoTxTest: handle=%u not found");
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      Enable ISO Rx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pldType     Payload length type.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LctrIsoRxTest(uint16_t handle, uint8_t pldType)
{
  uint8_t status;
  lctrCisCtx_t *pCisCtx;
  lctrBisCtx_t *pBisCtx;

  if ((pCisCtx = lctrFindCisByHandle(handle)) != NULL)
  {
    pCisCtx->rxPendInit = (pldType == LL_ISO_TEST_PL_LEN_ZERO) ? FALSE : TRUE;

    pCisCtx->rxTestEnabled = TRUE;
    pCisCtx->numRxSuccess = 0;
    pCisCtx->numRxMissed = 0;
    pCisCtx->numRxFailed = 0;
    pCisCtx->isoRxPldType = pldType;

    status = LL_SUCCESS;
  }
  else if ((pBisCtx = lctrFindBisByHandle(handle)) != NULL)
  {
    status = lctrBisRxTest(pBisCtx, pldType);
  }
  else
  {
    LL_TRACE_WARN0("LctrIsoRxTest: handle=%u not found");
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      ISO read test counter.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pStats      Pointer to the statistics block.
 *
 *  \return     Status code.
 */
/*************************************************************************************************/
uint8_t LctrIsoReadTestCounter(uint16_t handle, LlIsoTestCtrs_t *pStats)
{
  uint8_t status;
  lctrCisCtx_t *pCisCtx;
  lctrBisCtx_t *pBisCtx;

  if ((pCisCtx = lctrFindCisByHandle(handle)) != NULL)
  {
    status = LctrCisReadTestCounters(pCisCtx, pStats);
  }
  else if ((pBisCtx = lctrFindBisByHandle(handle)) != NULL)
  {
    status = LctrBisReadTestCounters(pBisCtx, pStats);
  }
  else
  {
    LL_TRACE_WARN1("LctrIsoReadTestCounter: handle=%u not found", handle);
    status = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      Terminate ISO Rx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pStats      Pointer to the statistics block.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LctrIsoTestEnd(uint16_t handle, LlIsoTestCtrs_t *pStats)
{
  uint8_t status;
  lctrCisCtx_t *pCisCtx;
  lctrBisCtx_t *pBisCtx;

  if ((pCisCtx = lctrFindCisByHandle(handle)) != NULL)
  {
    if (!pCisCtx->rxTestEnabled &&
        !pCisCtx->txTestEnabled)
    {
      LL_TRACE_WARN0("Invalid ISO Test state, test mode must be enabled");
      return LL_ERROR_CODE_CMD_DISALLOWED;
    }

    if (pCisCtx->rxTestEnabled)
    {
      status = LctrCisReadTestCounters(pCisCtx, pStats);
      pCisCtx->rxTestEnabled = FALSE;

      /* Counters will be reset on re-initialization of Rx test. */
    }
    if (pCisCtx->txTestEnabled)
    {
      pCisCtx->txTestEnabled = FALSE;
    }
 }
  else if ((pBisCtx = lctrFindBisByHandle(handle)) != NULL)
  {
    status = LctrBisReadTestCounters(pBisCtx, pStats);

    pBisCtx->test.term = TRUE;
  }
  else
  {
    LL_TRACE_WARN1("LctrIsoTestEnd: handle=%u not found", handle);
    status = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      Read ISO Link Quality action.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pStats      Storage for statistics.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LctrReadIsoLinkQual(uint16_t handle, LlIsoLinkQual_t *pStats)
{
  lctrCisCtx_t *pCisCtx;
  lctrBisCtx_t *pBisCtx;
  if ((pCisCtx = lctrFindCisByHandle(handle)) != NULL)
  {
    memcpy(pStats, &pCisCtx->isoLinkQualStats, sizeof(LlIsoLinkQual_t));
    return LL_SUCCESS;
  }
  else if ((pBisCtx = lctrFindBisByHandle(handle)) != NULL)
  {
    return LL_SUCCESS;
  }
  else
  {
    LL_TRACE_WARN1("LctrReadIsoLinkQual: handle=%u not found", handle);
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      ISO event complete enable.
 *
 *  \param      enable          Set to TRUE to enable, FALSE to disable.
 *
 *  Enable or disable reports about the scanners from which an advertiser receives scan requests.
 */
/*************************************************************************************************/
void LlIsoEventCompleteEnable(uint8_t enable)
{
  lmgrCb.sendIsoCmplEvt = enable;
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host ISO Event completion.
 *
 *  \param      handle          CIG or BIG Handle.
 *  \param      evtCtr          Event Counter.
 */
/*************************************************************************************************/
void lctrNotifyHostIsoEventComplete(uint8_t handle, uint32_t evtCtr)
{
  LlIsoEventCmplInd_t evt;

  evt.hdr.param = handle;
  evt.hdr.event = LL_ISO_EVT_CMPL_IND;
  evt.hdr.status = LL_SUCCESS;

  evt.handle = handle;
  evt.evtCtr = evtCtr;

  LL_TRACE_INFO1("### LlEvent ###  LL_ISO_EVT_CMPL_IND, handle=%u", handle);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Send Codec SDU data.
 *
 *  \param      handle          ISO Handle.
 */
/*************************************************************************************************/
void lctrIsoSendCodecSdu(uint16_t id, uint32_t pktCtr, uint32_t ts, uint8_t *pData, uint16_t actLen)
{
  /* Recover SDU buffer. */
  uint8_t *pSduBuf = pData - HCI_ISO_HDR_LEN - HCI_ISO_DL_MAX_LEN;

  lctrIsoHdr_t hdr =
  {
    .handle = id,
    .pb = LCTR_PB_COMP,
    .tsFlag = TRUE,
    .len = actLen,
    .ts = 0,
    .pktSn = pktCtr,
    .sduLen = actLen,
    .ps = LCTR_PS_VALID
  };

  lctrIsoPackHdr(pSduBuf, &hdr);
  LctrTxIso(pSduBuf);
}

/*************************************************************************************************/
/*!
 *  \brief      Setup ISO data path.
 *
 *  \param      pDpParam           Parameters to set up data path.
 *  \param      pSetupDataPath     Data path setup parameters.
 */
/*************************************************************************************************/
uint8_t lctrIsoSetupDataPath(lctrDpParams_t *pDpParam, LlIsoSetupDataPath_t *pSetupDataPath)
{
  uint8_t status = LL_SUCCESS;

  switch (pSetupDataPath->dpDir)
  {
    case LL_ISO_DATA_DIR_INPUT:
    {
      if (pSetupDataPath->dpId == pDpParam->pDataPathCtx->in.id)
      {
        /* No change. */
        return LL_SUCCESS;
      }

      lctrIsoInDataPathClear(pDpParam);
      pDpParam->pDataPathCtx->in.id = pSetupDataPath->dpId;
      status = lctrIsoInDataPathSetup(pDpParam, pSetupDataPath);
      break;
    }

    case LL_ISO_DATA_DIR_OUTPUT:
    {
      if (pSetupDataPath->dpId == pDpParam->pDataPathCtx->out.id)
      {
        /* No change. */
        return LL_SUCCESS;
      }

      lctrIsoOutDataPathClear(pDpParam);
      pDpParam->pDataPathCtx->out.id = pSetupDataPath->dpId;
      status = lctrIsoOutDataPathSetup(pDpParam, pSetupDataPath);

      break;
    }

    default:
      LL_TRACE_ERR1("Invalid value dpDir=%u", pSetupDataPath->dpDir);
      break;
  }

  return status;
}
