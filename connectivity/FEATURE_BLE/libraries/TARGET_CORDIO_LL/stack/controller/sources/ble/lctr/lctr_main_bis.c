/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller broadcast isochronous main implementation file.
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

#include "lctr_int_bis.h"
#include "lctr_int_iso.h"
#include "lctr_int_bis_master.h"
#include "wsf_assert.h"
#include "wsf_buf.h"
#include "wsf_math.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief  Queue header. */
#define LCTR_Q_ELEM_LEN  4

/**************************************************************************************************
  Globals
**************************************************************************************************/

/*! \brief      BIS context table. */
lctrBisCtx_t *pLctrBisTbl;

/*! \brief      BIG context table. */
lctrBigCtx_t *pLctrBigTbl;

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Assemble BIS Data PDU.
 *
 *  \param  pBuf        Buffer to pack the Data PDU header.
 *  \param  pduLen      PDU length.
 *  \param  llid        LLID of PDU.
 *  \param  pSegHdr     Segmentation header or NULL if not required.
 *
 *  \return Length of header.
 */
/*************************************************************************************************/
static uint8_t lctrAssembleBisDataPdu(uint8_t *pBuf, uint16_t pduLen, LlIsoLlid_t llid, lctrIsoSegHdr_t *pSegHdr)
{
  uint8_t hdrLen;

  /* All additional fields must be zero'ed since flow control bits will be or'ed in at transmit. */
  lctrBisDataPduHdr_t dataHdr =
  {
    .llid = llid,
    .cssn = 0,      /* Completed in ISR. */
    .cstf = 0,      /* Completed in ISR. */
    .len = pduLen
  };

  hdrLen = lctrBisPackDataPduHdr(pBuf, &dataHdr);

  if (pSegHdr)
  {
    hdrLen += lctrIsoPackSegHdr(pBuf, pSegHdr);
  }

  return hdrLen;
}

/*************************************************************************************************/
/*!
 *  \brief  Fragment an ISO SDU into a Data PDU descriptor.
 *
 *  \param  pBisCtx     BIS context.
 *  \param  pIsoHdr     ISO SDU header.
 *  \param  pIsoBuf     ISO SDU Buffer.
 *  \param  pDesc       Fragmentation descriptor.
 */
/*************************************************************************************************/
static void lctrBisFragmentIsoSdu(lctrBisCtx_t *pBisCtx, lctrIsoHdr_t *pIsoHdr, uint8_t *pIsoBuf, lctrIsoTxBufDesc_t *pDesc)
{
  lctrBigCtx_t *pBigCtx = pBisCtx->pBigCtx;

  pDesc->isoLen  = pIsoHdr->sduLen;
  pDesc->fragLen = pBisCtx->pBigCtx->maxPdu;
  pDesc->fragCnt = 0;
  pDesc->pIsoSdu = pIsoBuf;
  pDesc->pPduBuf = pIsoBuf + (HCI_ISO_HDR_LEN + HCI_ISO_DL_MIN_LEN) + ((pIsoHdr->tsFlag) ? HCI_ISO_TS_LEN : 0);

  uint8_t *pSduBuf = pDesc->pPduBuf;
  uint16_t fragOffset = 0;

  if (pDesc->isoLen > 0)
  {
    while (fragOffset < pDesc->isoLen)
    {
      const uint16_t dataRem = pDesc->isoLen - fragOffset;
      const uint16_t fragSize = WSF_MIN(dataRem, pDesc->fragLen);
      fragOffset += fragSize;

      LlIsoLlid_t llid;

      if (pBigCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED)
      {
        llid = (fragOffset == pDesc->isoLen) ? LL_LLID_ISO_UNF_END_PDU : LL_LLID_ISO_UNF_CONT_PDU;
      }
      else
      {
        llid = LL_LLID_ISO_FRA_PDU;
      }

      pDesc->frag[pDesc->fragCnt].hdrLen = lctrAssembleBisDataPdu(pDesc->frag[pDesc->fragCnt].hdr, fragSize, llid, NULL);

      uint64_t pktCtr = (pBisCtx->pBigCtx->eventCounter * pBisCtx->pBigCtx->bn) + pDesc->fragCnt;
      pBisCtx->chan.enc.pTxPktCounter = &pktCtr;

      if (lctrPktEncryptHdlr &&
          lctrPktEncryptHdlr(&pBisCtx->chan.enc,
                             pDesc->frag[pDesc->fragCnt].hdr,
                             pSduBuf,
                             pDesc->frag[pDesc->fragCnt].trl))
      {
        pDesc->frag[pDesc->fragCnt].trlLen = LL_DATA_MIC_LEN;

        #if (LL_ENABLE_TESTER)
          pDesc->frag[pDesc->fragCnt].trl[0] ^= (llTesterCb.pktMic >>  0) & 0xFF;
          pDesc->frag[pDesc->fragCnt].trl[1] ^= (llTesterCb.pktMic >>  8) & 0xFF;
          pDesc->frag[pDesc->fragCnt].trl[2] ^= (llTesterCb.pktMic >> 16) & 0xFF;
          pDesc->frag[pDesc->fragCnt].trl[3] ^= (llTesterCb.pktMic >> 24) & 0xFF;
        #endif
      }
      else
      {
        pDesc->frag[pDesc->fragCnt].trlLen = 0;
      }

      pSduBuf += fragSize;
      pDesc->fragCnt++;
    }
  }
  else
  {
    /* Zero length SDU. */

    pDesc->frag[0].hdrLen = lctrAssembleBisDataPdu(pDesc->frag[0].hdr, 0, LL_LLID_ISO_UNF_END_PDU, NULL);
    pDesc->frag[0].trlLen = 0;

    pDesc->fragCnt = 1;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Allocate a BIG context.
 *
 *  \param      bigHandle   BIG handle.
 *
 *  \return     BIG context or NULL if at capacity.
 */
/*************************************************************************************************/
lctrBigCtx_t *lctrAllocBigCtx(uint8_t bigHandle)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxBig; i++)
  {
    lctrBigCtx_t *pBigCtx = &pLctrBigTbl[i];

    if (!pBigCtx->enabled)
    {
      memset(pBigCtx, 0, sizeof(lctrBigCtx_t));

      pBigCtx->enabled = TRUE;
      pBigCtx->handle = bigHandle;

      return pBigCtx;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Free a  BIG context.
 *
 *  \param      pBigCtx       BIG context to free.
 */
/*************************************************************************************************/
void lctrFreeBigCtx(lctrBigCtx_t *pBigCtx)
{
  WSF_ASSERT(pBigCtx->enabled);

  pBigCtx->enabled = FALSE;

  /* Free all BIS context associated with the BIG. */
  for (unsigned int i = 0; i < pBigCtx->numBis; i++)
  {
    lctrBisCtx_t *pBisCtx = pBigCtx->pBisCtx[i];

    if (pBisCtx == NULL)
    {
      /* Filtered BIS. */
      continue;
    }

    switch (pBigCtx->role)
    {
      case LL_ROLE_SLAVE:
        LctrRemoveIsoDataPath(pBisCtx->handle, LL_ISO_DATA_PATH_INPUT_BIT);
        break;
      case LL_ROLE_MASTER:
        LctrRemoveIsoDataPath(pBisCtx->handle, LL_ISO_DATA_PATH_OUTPUT_BIT);
        break;
      default:
        break;
    }

    lctrFreeBisCtx(pBisCtx);
  }

  /* Flush BIG Control PDUs. */
  switch (pBigCtx->role)
  {
    case LL_ROLE_SLAVE:
      while (!WsfQueueEmpty(&pBigCtx->roleData.slv.txCtrlQ))
      {
        lctrBigTxCtrlQueuePopCleanup(pBigCtx);
      }
      break;
    case LL_ROLE_MASTER:
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check if the periodic advertising handle is associated with another BIG.
 *
 *  \param      advHandle       Periodic advertising handle.
 *
 *  \return     TRUE if periodic advertising handle is associated with another BIG, otherwise FALSE.
 */
/*************************************************************************************************/
uint8_t lctrBigIsPerAdvUsed(uint8_t advHandle)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxBig; i++)
  {
    lctrBigCtx_t *pBigCtx = &pLctrBigTbl[i];

    if ((pBigCtx->enabled) &&
        (pBigCtx->role == LL_ROLE_SLAVE) &&
        (pBigCtx->roleData.slv.pAdvSet) &&
        (pBigCtx->roleData.slv.pAdvSet->handle == advHandle))
    {
      return TRUE;
    }
  }
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Find a BIG by BIG handle.
 *
 *  \param      bigHandle   BIG handle.
 *
 *  \return     BIG context or NULL if not found.
 */
/*************************************************************************************************/
lctrBigCtx_t *lctrFindBigByHandle(uint8_t bigHandle)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxBig; i++)
  {
    lctrBigCtx_t *pBigCtx = &pLctrBigTbl[i];

    if ((pBigCtx->enabled) &&
        (pBigCtx->handle == bigHandle))
    {
      return pBigCtx;
    }
  }
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Find a BIG by SyncHandle.
 *
 *  \param      syncHandle   BIG sync handle.
 *
 *  \return     BIG context or NULL if not found.
 */
/*************************************************************************************************/
lctrBigCtx_t *lctrFindBigBySyncHandle(uint16_t syncHandle)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxBig; i++)
  {
    lctrBigCtx_t *pBigCtx = &pLctrBigTbl[i];

    if ((pBigCtx->enabled) &&
        (pBigCtx->roleData.mst.pPerScanCtx == LCTR_GET_PER_SCAN_CTX(syncHandle)))
    {
      return pBigCtx;
    }
  }
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Check is current BIG synchronization in progress.
 *
 *  \return     TRUE if syncrhonizing in progres, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrIsBigSynchronizing(void)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxBig; i++)
  {
    lctrBigCtx_t *pBigCtx = &pLctrBigTbl[i];

    if ((pBigCtx->enabled) &&
        (pBigCtx->state == LCTR_MST_BIG_STATE_SYNCING))
    {
      return TRUE;
    }
  }
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Allocate a BIS context.
 *
 *  \param      pBigCtx   Pointer to the BIG context.
 *
 *  \return     BIS context or NULL if at capacity.
 */
/*************************************************************************************************/
lctrBisCtx_t *lctrAllocBisCtx(lctrBigCtx_t *pBigCtx)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxBis; i++)
  {
    lctrBisCtx_t *pBisCtx = &pLctrBisTbl[i];

    if (!pBisCtx->enabled)
    {
      memset(pBisCtx, 0, sizeof(lctrBisCtx_t));

      /* State and context. */
      pBisCtx->enabled = TRUE;
      pBisCtx->handle = LCTR_FIRST_BIS_HANDLE + i;
      pBisCtx->pBigCtx = pBigCtx;
      pBisCtx->bisNum = pBigCtx->numBis + 1;

      if (pBigCtx->role == LL_ROLE_MASTER)
      {
        pBisCtx->roleData.mst.dataPathOutCtx.id = LL_ISO_DATA_PATH_DISABLED;
      }
      else
      {
        pBisCtx->roleData.slv.dataPathInCtx.id = LL_ISO_DATA_PATH_DISABLED;
      }

      /* Parent context. */
      pBigCtx->pBisCtx[pBigCtx->numBis++] = pBisCtx;

      return pBisCtx;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Free a  BIS context.
 *
 *  \param      pBisCtx       BIS context to free.
 */
/*************************************************************************************************/
void lctrFreeBisCtx(lctrBisCtx_t *pBisCtx)
{
  WSF_ASSERT(pBisCtx->enabled);
  pBisCtx->enabled = FALSE;

  uint8_t *pPdu;

  /* Flush remaining packets. */
  switch (pBisCtx->pBigCtx->role)
  {
    case LL_ROLE_MASTER:
      while ((pPdu = lctrBisDequeueRxDataPduTop(pBisCtx)) != NULL)
      {
        lctrBisRxIsoDataPduFree(pPdu);
      }

      uint8_t *pIsoSdu;
      while ((pIsoSdu = lctrBisRxIsoSduDeq(pBisCtx)) != NULL)
      {
        WsfMsgFree(pIsoSdu);
        lctrIsoDataRxIncAvailBuf(1);
      }

      lctrIsoalRxDataPathClear(&pBisCtx->roleData.mst.isoalRxCtx, pBisCtx->pBigCtx->framing);
      break;
    case LL_ROLE_SLAVE:
      while (!WsfQueueEmpty(&pBisCtx->roleData.slv.txDataQ))
      {
        lctrBisTxQueuePopCleanup(pBisCtx, 1);
      }

      /* Free any IsoalTxPendQ */
      void *pIsoBuf;
      uint8_t handlerId;
      while ((pIsoBuf = WsfMsgDeq(&pBisCtx->roleData.slv.isoalTxCtx.pendingSduQ, &handlerId)) != NULL)
      {
        WsfMsgFree(pIsoBuf);
      }
      break;
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Find a BIS by handle.
 *
 *  \param      bisHandle      BIS handle.
 *
 *  \return     BIS context or NULL if not found.
 */
/*************************************************************************************************/
lctrBisCtx_t *lctrFindBisByHandle(uint16_t bisHandle)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxBis; i++)
  {
    lctrBisCtx_t *pBisCtx = &pLctrBisTbl[i];

    if ((pBisCtx->enabled) &&
        (pBisCtx->handle == bisHandle))
    {
      return pBisCtx;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of available BIS contexts.
 *
 *  \return     Number of available BIS contexts.
 */
/*************************************************************************************************/
uint8_t lctrGetNumAvailBisCtx(void)
{
  uint8_t count = 0;

  for (unsigned int i = 0; i < pLctrRtCfg->maxBis; i++)
  {
    lctrBisCtx_t *pBisCtx = &pLctrBisTbl[i];

    if (pBisCtx->enabled == FALSE)
    {
      count++;
    }
  }

  return count;
}

/*************************************************************************************************/
/*!
 *  \brief  Compute new access address.
 *
 *  \param      seedAccAddr       Seed access address.
 *  \param      bisNum            BIS number.
 *
 *  \return New seed access address value.
 */
/*************************************************************************************************/
uint32_t lctrComputeBisAccessAddr(uint32_t seedAccAddr, uint8_t bisNum)
{
  uint32_t accAddr = seedAccAddr;
  uint16_t div;
  uint32_t dw = 0;
  uint8_t d0, d1, d2, d3, d4, d5, d6;

  d0 = d1 = d2 = d3 = d4 = d5 = d6 = 0;

  /* Diversifier = ((35 * n) + 42) MOD 128 */
  const uint16_t term = (35 * bisNum) + 42;
  div = term - ((term >> 7) * 128);

  d0 = div & 1;
  d1 = (div >> 1) & 1;
  d2 = (div >> 2) & 1;
  d3 = (div >> 3) & 1;
  d4 = (div >> 4) & 1;
  d5 = (div >> 5) & 1;
  d6 = (div >> 6) & 1;

  /* DW = D0D0D0D0D0D0D1D6_D10D5D40D3D20_00000000_00000000b  Note the digit after the letter is the index of the D. */
  dw = ((d0 << 31) | (d0 << 30) | (d0 << 29) | (d0 << 28) | (d0 << 27)  | (d0 << 26) | (d1 << 25) | (d6 << 24) | \
        (d1 << 23) | (d5 << 21) | (d4 << 20) | (d3 << 18) | (d2 << 17));

  /* SAA bit-wise XORed with DW */
  accAddr ^= dw;

  return accAddr;
}

/*************************************************************************************************/
/*!
 *  \brief      Set up a new BIS.
 *
 *  \param      pBisCtx         BIS context.
 *  \param      seedAccAddr     Seed access address.
 *  \param      baseCrcInit     Base CRC Init.
 *  \param      chMap           Channel map.
 *  \param      phy             PHY.
 */
/*************************************************************************************************/
void lctrSetupBisContext(lctrBisCtx_t *pBisCtx, uint32_t seedAccAddr, uint16_t baseCrcInit, uint64_t chMap, LlPhy_t phy)
{
  pBisCtx->chan.opType = (pBisCtx->pBigCtx->role == LL_ROLE_SLAVE) ? BB_BLE_OP_SLV_BIS_EVENT : BB_BLE_OP_MST_BIS_EVENT;

  uint32_t accAddr = lctrComputeBisAccessAddr(seedAccAddr, pBisCtx->bisNum);

  pBisCtx->chSelInfo.chanMask = chMap;
  pBisCtx->chSelInfo.usedChSel = LL_CH_SEL_2;
  pBisCtx->chSelInfo.chIdentifier = (accAddr >> 16) ^ (accAddr >> 0);
  LmgrBuildRemapTable(&pBisCtx->chSelInfo);

  pBisCtx->chan.accAddr = accAddr;
  pBisCtx->chan.crcInit = (baseCrcInit << 8) | pBisCtx->bisNum;
  pBisCtx->chan.txPower = lmgrCb.advTxPwr;
  pBisCtx->chan.txPhy = pBisCtx->chan.rxPhy = phy;
  pBisCtx->chan.initTxPhyOptions = BB_PHY_OPTIONS_DEFAULT;
  pBisCtx->chan.tifsTxPhyOptions = BB_PHY_OPTIONS_DEFAULT;
  /* pBisCtx->chan.peerTxStableModIdx = FALSE; */

#if (LL_ENABLE_TESTER)
  pBisCtx->chan.accAddrRx = pBisCtx->chan.accAddr ^ llTesterCb.dataAccessAddrRx;
  pBisCtx->chan.accAddrTx = pBisCtx->chan.accAddr ^ llTesterCb.dataAccessAddrTx;
  pBisCtx->chan.crcInitRx = pBisCtx->chan.crcInit ^ llTesterCb.dataCrcInitRx;
  pBisCtx->chan.crcInitTx = pBisCtx->chan.crcInit ^ llTesterCb.dataCrcInitTx;
#endif

  if (pBisCtx->pBigCtx->encrypt)
  {
    pBisCtx->chan.enc.enaEncrypt = TRUE;
    pBisCtx->chan.enc.enaDecrypt = TRUE;
    pBisCtx->chan.enc.enaAuth = TRUE;
    pBisCtx->chan.enc.nonceMode = PAL_BB_NONCE_MODE_EXT64_CNTR;

    memcpy(pBisCtx->chan.enc.iv, pBisCtx->pBigCtx->giv, LL_IV_LEN);
    pBisCtx->chan.enc.iv[0] ^= accAddr >>  0;
    pBisCtx->chan.enc.iv[1] ^= accAddr >>  8;
    pBisCtx->chan.enc.iv[2] ^= accAddr >> 16;
    pBisCtx->chan.enc.iv[3] ^= accAddr >> 24;

    memcpy(pBisCtx->chan.enc.sk, pBisCtx->pBigCtx->bleData.chan.enc.sk, PAL_CRYPTO_LL_KEY_LEN);

    /* The directionBit shall be set to 1 for Broadcast Isochronous PDUs. */
    pBisCtx->chan.enc.dir = 1;
    pBisCtx->chan.enc.type = PAL_BB_TYPE_BIS;

    if (lctrInitCipherBlkHdlr)
    {
      lctrInitCipherBlkHdlr(&pBisCtx->chan.enc, pBisCtx->handle, 1);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Select all BIS channels and BIG Control channels.
 *
 *  \param  pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrSelectBigChannels(lctrBigCtx_t *pBigCtx)
{
  /* Select BIS channels. */
  for (unsigned int i = 0; i < pBigCtx->numBis; i++)
  {
    lctrBisCtx_t * const pBisCtx = pBigCtx->pBisCtx[i];

    if (pBisCtx)
    {
      pBisCtx->chan.chanIdx = LmgrSelectNextChannel(&pBisCtx->chSelInfo, pBigCtx->eventCounter, 0, TRUE);
    }
  }

  /* Select BIG Control channel. */
  pBigCtx->ctrChan.chanIdx = LmgrSelectNextChannel(&pBigCtx->ctrChSelInfo, pBigCtx->eventCounter, 0, TRUE);

  /* Setup BOD for next BIG Event. */
  if (pBigCtx->role == LL_ROLE_SLAVE)
  {
    pBigCtx->bleData.chan.chanIdx = pBigCtx->pBisCtx[0]->chan.chanIdx;
  }
  else /* LL_ROLE_MASTER */
  {
    pBigCtx->bleData.chan.chanIdx = pBigCtx->roleData.mst.pFirstBisCtx->chan.chanIdx;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Remap all BIS channels and BIG Control channels.
 *
 *  \param  pBigCtx     BIG context.
 *  \param  chanMap     New channel map.
 */
/*************************************************************************************************/
void lctrRemapBigChannels(lctrBigCtx_t *pBigCtx, uint64_t chanMap)
{
  /* Select BIS channels. */
  for (unsigned int i = 0; i < pBigCtx->numBis; i++)
  {
    lctrBisCtx_t * const pBisCtx = pBigCtx->pBisCtx[i];

    if (pBisCtx)
    {
      pBisCtx->chSelInfo.chanMask = chanMap;
      LmgrBuildRemapTable(&pBisCtx->chSelInfo);
    }
  }

  /* Select BIG Control channel. */
  pBigCtx->ctrChSelInfo.chanMask = chanMap;
  LmgrBuildRemapTable(&pBigCtx->ctrChSelInfo);
}

/*************************************************************************************************/
/*!
 *  \brief  Queue BIS ISO SDU onto Tx queue.
 *
 *  \param  pBisCtx     BIS context.
 *  \param  pIsoHdr     ISO header.
 *  \param  pIsoSdu     ISO SDU buffer.
 */
/*************************************************************************************************/
void lctrBisTxIsoPduQueue(lctrBisCtx_t *pBisCtx, lctrIsoHdr_t *pIsoHdr, uint8_t *pIsoSdu)
{
  lctrIsoTxBufDesc_t *pDesc;

  if ((pDesc = lctrAllocIsoTxBufDesc()) == NULL)
  {
    LL_TRACE_ERR1("Failed to allocate transmit buffer descriptor: bisHandle=%u", pIsoHdr->handle);
    WsfMsgFree(pIsoSdu);
    if (lmgrPersistCb.sendIsoCompCback &&
        !pBisCtx->test.enabled)
    {
      uint16_t handle = pIsoHdr->handle;
      uint16_t numSdu = 1;

      lmgrPersistCb.sendIsoCompCback(1, &handle, &numSdu);
    }
    lctrIsoSduTxIncAvailBuf();

    lctrNotifyHostHwErrInd(LL_ERROR_CODE_MEM_CAP_EXCEEDED);
    return;
  }

  lctrBisFragmentIsoSdu(pBisCtx, pIsoHdr, pIsoSdu, pDesc);
  WsfMsgEnq(&pBisCtx->roleData.slv.txDataQ, pIsoHdr->handle, (uint8_t *)pDesc);
}

/*************************************************************************************************/
/*!
 *  \brief  Get top element in Tx queue.
 *
 *  \param  pBisCtx      Connection context.
 *  \param  burstIdx     Burst index.
 *  \param  descs        Storage for BB descriptors.
 *
 *  \return Number of BB descriptors.
 */
/*************************************************************************************************/
uint8_t lctrBisTxQueuePeek(lctrBisCtx_t *pBisCtx, uint8_t burstIdx, PalBbBleTxBufDesc_t *descs)
{
  wsfHandlerId_t handlerId;
  lctrIsoTxBufDesc_t *pDesc;
  uint8_t descCnt = 0;
  uint8_t sduCnt = 0;

  if (burstIdx > pBisCtx->pBigCtx->bn)
  {
    /* Advance to the pre-transmission PDU. */
    burstIdx += pBisCtx->pBigCtx->pto * pBisCtx->pBigCtx->bn;
  }

  /* Do not remove from queue until burst completes. */
  while ((pDesc = WsfMsgNPeek(&pBisCtx->roleData.slv.txDataQ, sduCnt++, &handlerId)) != NULL)
  {
    if (pDesc->isoLen > 0)
    {
      unsigned int numFrag = LlMathDivideUint32RoundUp(pDesc->isoLen, pDesc->fragLen);

      if (burstIdx >= numFrag)
      {
        /* Advance to next SDU. */
        burstIdx -= numFrag;
        continue;
      }

      uint8_t  fragCnt  = burstIdx;
      uint16_t fragOffs = pDesc->fragLen * fragCnt;

      if (fragOffs < pDesc->isoLen)
      {
        uint16_t fragSize = WSF_MIN(pDesc->isoLen - fragOffs, pDesc->fragLen);

        descs[0].len  = pDesc->frag[fragCnt].hdrLen;
        descs[0].pBuf = pDesc->frag[fragCnt].hdr;
        descs[1].len  = fragSize;
        descs[1].pBuf = pDesc->pPduBuf + fragOffs;
        descs[2].pBuf = NULL;
        descCnt = 2;
        if (pDesc->frag[fragCnt].trlLen)
        {
          descs[2].len  = pDesc->frag[fragCnt].trlLen;
          descs[2].pBuf = pDesc->frag[fragCnt].trl;
          descCnt = 3;
        }

        /* Exit loop. */
        break;
      }
    }
    else
    {
      /* Zero length PDU. */
      descs[0].len  = pDesc->frag[0].hdrLen;
      descs[0].pBuf = pDesc->frag[0].hdr;
      descs[1].pBuf = NULL;
      descCnt = 1;

      /* Exit loop. */
      break;
    }
  }

  return descCnt;
}

/*************************************************************************************************/
/*!
 *  \brief  Pop top element from Tx queue.
 *
 *  \param  pBisCtx     BIS context.
 *  \param  numFrag     Number of fragments to cleanup.
 */
/*************************************************************************************************/
void lctrBisTxQueuePopCleanup(lctrBisCtx_t *pBisCtx, uint8_t numFrag)
{
  lctrIsoTxBufDesc_t *pDesc;
  wsfHandlerId_t handlerId;

  while (numFrag-- &&
         ((pDesc = WsfMsgPeek(&pBisCtx->roleData.slv.txDataQ, &handlerId)) != NULL))
  {
    pDesc->fragCnt++;

    if ((pDesc->fragLen * pDesc->fragCnt) >= pDesc->isoLen)  /* last fragment */
    {
      WsfMsgDeq(&pBisCtx->roleData.slv.txDataQ, &handlerId);
      WsfMsgFree(pDesc->pIsoSdu);
      lctrFreeIsoTxBufDesc(pDesc);
      lctrIsoSduTxIncAvailBuf();

      if (pBisCtx->roleData.slv.dataPathInCtx.id == LL_ISO_DATA_PATH_HCI)
      {
        /* ISO Test does not send Tx Complete notifications. */
        if (!pBisCtx->test.enabled)
        {
          pBisCtx->roleData.slv.numTxSduComp++;
        }
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a Tx BIS Control PDU buffer.
 *
 *  \param  pduLen      Control PDU length.
 *
 *  \return BIS Control PDU buffer or NULL if out of memory.
 */
/*************************************************************************************************/
uint8_t *lctrBigTxCtrlAlloc(uint8_t pduLen)
{
  uint8_t *pBuf;

  if ((pBuf = WsfBufAlloc(LCTR_Q_ELEM_LEN
                          + sizeof(uint32_t)    /* ReTx data with padding */
                          + LL_DATA_HDR_LEN
                          + sizeof(uint8_t)     /* Opcode */
                          + pduLen)) == NULL)
  {
    return NULL;
  }

  /* Return pointer to start of header. */
  return pBuf + LCTR_Q_ELEM_LEN + sizeof(uint32_t);
}

/*************************************************************************************************/
/*!
 *  \brief  Queue a BIS Control PDU for transmission.
 *
 *  \param  pBigCtx     BIG context.
 *  \param pBuf         BIS Control PDU buffer.
 *  \param numReTx      Number of retransmissions.
 */
/*************************************************************************************************/
void lctrBigTxCtrlQueue(lctrBigCtx_t *pBigCtx, uint8_t *pBuf, uint8_t numReTx)
{
  /* Recover start of buffer. */
  pBuf -= sizeof(uint32_t);

  /* First byte is the retransmission number. */
  pBuf[0] = numReTx;

  /* Recover start of queue element. */
  pBuf -= LCTR_Q_ELEM_LEN;

  WsfQueuePush(&pBigCtx->roleData.slv.txCtrlQ, pBuf);
}

/*************************************************************************************************/
/*!
 *  \brief  Get top element in Tx BIG control queue.
 *
 *  \param  pBigCtx     BIG context.
 *
 *  \return Control PDU buffer or NULL if none available.
 */
/*************************************************************************************************/
uint8_t *lctrBigTxCtrlQueuePeek(lctrBigCtx_t *pBigCtx)
{
  uint8_t *pBuf;

  /* Queue peek. */
  if ((pBuf = pBigCtx->roleData.slv.txCtrlQ.pHead) != NULL)
  {
    /* Return pointer to start of header. */
    return pBuf + LCTR_Q_ELEM_LEN + sizeof(uint32_t);
  }

  /* No more BIS Control PDUs. */
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Pop top element in Tx BIG control queue if retransmissions completed.
 *
 *  \param  pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrBigTxCtrlQueuePop(lctrBigCtx_t *pBigCtx)
{
  uint8_t *pBuf;

  /* Queue peek. */
  pBuf = pBigCtx->roleData.slv.txCtrlQ.pHead;

  if (pBuf)
  {
    /* Recover start buffer. */
    pBuf += LCTR_Q_ELEM_LEN;

    /* Decrease retransmission count. */
    pBuf[0]--;

    if (pBuf[0] == 0)
    {
      /* No more retransmissions; cleanup and skip to next BIS Control PDU. */
      lctrBigTxCtrlQueuePopCleanup(pBigCtx);
      pBigCtx->bcp.cssn++;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Pop top element from Tx BIG control queue.
 *
 *  \param  pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrBigTxCtrlQueuePopCleanup(lctrBigCtx_t *pBigCtx)
{
  uint8_t *pBuf = WsfQueueDeq(&pBigCtx->roleData.slv.txCtrlQ);

  if (pBuf)
  {
    WsfBufFree(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a receive ISO Data PDU buffer.
 *
 *  \return Pointer to the start of the ISO Data PDU buffer.
 */
/*************************************************************************************************/
uint8_t *lctrBisRxIsoDataPduAlloc(void)
{
  const uint32_t allocLen = HCI_ISO_DL_MAX_LEN + pLctrRtCfg->maxIsoSduLen + BB_DATA_PDU_TAILROOM;

  if (lmgrIsoCb.availRxBuf == 0)
  {
    /* Flow control Rx path. */
    return NULL;
  }

  uint8_t *pPdu;

  /* Use LL_ISO_PDU_MAX_LEN to ensure use of data buffers located in the large pool. */
  if ((pPdu = (uint8_t*)WsfMsgAlloc(allocLen)) != NULL)
  {
    /* Return start of ISO Data PDU. */
    pPdu += LCTR_ISO_SDU_START_OFFSET;

    /* Do not claim buffer until SDU is committed. */
    /* lctrIsoDataRxDecAvailBuf(); */  /* SDU committed in lctrBisRxIsoSduEnq(). */
  }

  return pPdu;
}

/*************************************************************************************************/
/*!
 *  \brief  Dequeue a received ISO SDU buffer.
 *
 *  \param  pBisCtx     BIS context.
 *
 *  \return Pointer to the start of the ISO SDU.
 *
 *  \note   Returned pointer must be freed with WsfMsgFree().
 */
/*************************************************************************************************/
uint8_t *lctrBisRxIsoSduDeq(lctrBisCtx_t *pBisCtx)
{
  wsfHandlerId_t bisHandle;
  uint8_t *pSdu = NULL;

  if (pBisCtx->test.enabled)
  {
    pSdu = WsfMsgDeq(&pBisCtx->roleData.mst.dataPathOutCtx.cfg.hci.rxDataQ, &bisHandle);
    return pSdu;
  }

  switch (pBisCtx->roleData.mst.dataPathOutCtx.id)
  {
    case LL_ISO_DATA_PATH_HCI:
      pSdu = WsfMsgDeq(&pBisCtx->roleData.mst.dataPathOutCtx.cfg.hci.rxDataQ, &bisHandle);
      break;
    case LL_ISO_DATA_PATH_VS:
      pSdu = WsfMsgDeq(&pBisCtx->roleData.mst.dataPathOutCtx.cfg.codec.rxDataQ, &bisHandle);
      break;
    default:
      break;
  }

  return pSdu;
}


/*************************************************************************************************/
/*!
 *  \brief  Allocate a receive ISO Data PDU buffer.
 *
 *  \param  pPdu    Pointer to the start of the ISO Data PDU buffer.
 */
/*************************************************************************************************/
void lctrBisRxIsoDataPduFree(uint8_t *pPdu)
{
  /* Recover start of buffer. */
  WsfMsgFree(pPdu - LCTR_ISO_SDU_START_OFFSET);
}

/*************************************************************************************************/
/*!
 *  \brief  Enqueue a receive ISO Data PDU buffer.
 *
 *  \param  pBisCtx     BIS context.
 *  \param  pRxBuf      Received PDU data buffer to queue.
 *  \param  burstIdx    Burst index.
 */
/*************************************************************************************************/
void lctrBisEnqueueRxDataPdu(lctrBisCtx_t *pBisCtx, uint8_t *pRxBuf, uint8_t burstIdx)
{
  WsfMsgEnq(&pBisCtx->roleData.mst.rxDataQ, burstIdx, pRxBuf - LCTR_ISO_SDU_START_OFFSET);
}

/*************************************************************************************************/
/*!
 *  \brief  Pop top element from Tx queue if it matches given \a bnIdx.
 *
 *  \param  pBisCtx     BIS context.
 *  \param  bnIdx       Burst Number index.
 *
 *  \return Pointer to ISO Data PDU.
 */
/*************************************************************************************************/
uint8_t *lctrBisDequeueRxDataPdu(lctrBisCtx_t *pBisCtx, uint8_t burstIdx)
{
  uint8_t *pBuf;

  uint8_t actBurstIdx;

  if (WsfMsgPeek(&pBisCtx->roleData.mst.rxDataQ, &actBurstIdx) == NULL)
  {
    return NULL;
  }

  if (actBurstIdx != burstIdx)
  {
    return NULL;
  }

  pBuf = WsfMsgDeq(&pBisCtx->roleData.mst.rxDataQ, &actBurstIdx);

  return pBuf + LCTR_ISO_SDU_START_OFFSET;
}

/*************************************************************************************************/
/*!
 *  \brief  Pop top element from Tx queue.
 *
 *  \param  pBisCtx     BIS context.
 *
 *  \return Pointer to ISO Data PDU.
 */
/*************************************************************************************************/
uint8_t *lctrBisDequeueRxDataPduTop(lctrBisCtx_t *pBisCtx)
{
  uint8_t *pBuf;
  uint8_t temp8;

  if ((pBuf = WsfMsgDeq(&pBisCtx->roleData.mst.rxDataQ, &temp8)) == NULL)
  {
    return NULL;
  }

  return pBuf + LCTR_ISO_SDU_START_OFFSET;
}

/*************************************************************************************************/
/*!
 *  \brief  Enable BIS Tx test.
 *
 *  \param  pBisCtx     BIS context.
 *  \param  pldType     Payload length type.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t lctrBisTxTest(lctrBisCtx_t *pBisCtx, uint8_t pldType)
{
  uint8_t status = LL_SUCCESS;

  if (pBisCtx->test.enabled || pBisCtx->test.term)
  {
    LL_TRACE_WARN0("Invalid ISO Test state, test mode already enabled");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (lmgrIsoCb.availTxBuf == 0)
  {
    LL_TRACE_WARN0("Out of memory, no ISO Tx buffers available");
    return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
  }

  /* BIS Data PDU generation occurs ever SDU Interval in lctrSlvBigEndOp(). */

  pBisCtx->test.enabled = TRUE;
  pBisCtx->test.term = FALSE;
  pBisCtx->test.util.framed.payloadCtr = 0;
#if (LL_ENABLE_TESTER)
  pBisCtx->test.util.framed.payloadCtr = llTesterCb.isoTxTestNumInit;
#endif
  pBisCtx->test.pldType = pldType;

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief  Enable BIS Rx test.
 *
 *  \param  pBisCtx     BIS context.
 *  \param  pldType     Payload length type.
 *
 *  \return Status error code.
 *
 */
/*************************************************************************************************/
uint8_t lctrBisRxTest(lctrBisCtx_t *pBisCtx, uint8_t pldType)
{
  uint8_t status = LL_SUCCESS;

  if (pBisCtx->test.enabled || pBisCtx->test.term)
  {
    LL_TRACE_WARN0("Invalid ISO Test state, test mode already enabled");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  pBisCtx->test.enabled = TRUE;
  pBisCtx->test.term = FALSE;
  pBisCtx->test.pendInit = TRUE;
  pBisCtx->test.pldType = pldType;
  memset(&pBisCtx->roleData.mst.stats, 0, sizeof(pBisCtx->roleData.mst.stats));

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief  BIS read ISO test counters.
 *
 *  \param  pBisCtx     BIS context.
 *  \param  pStats      Pointer to the statistics block.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrBisReadTestCounters(lctrBisCtx_t *pBisCtx, LlIsoTestCtrs_t *pStats)
{
  if (pBisCtx->test.enabled == FALSE)
  {
    LL_TRACE_WARN0("Invalid ISO Test state, test mode must be enabled");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  *pStats = pBisCtx->roleData.mst.stats;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize connected isochronous memory resources.
 *
 *  \param  pFreeMem    Pointer to free memory.
 *  \param  freeMemSize Size of pFreeMem.
 *
 *  \return Amount of free memory consumed.
 */
/*************************************************************************************************/
uint16_t LctrInitBisMem(uint8_t *pFreeMem, uint32_t freeMemSize)
{
  uint8_t *pAvailMem = pFreeMem;

  /*** Advertising Set Context ***/

  if (((uint32_t)pAvailMem) & 3)
  {
    /* Align to next word. */
    pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
  }

  LL_TRACE_INFO2("    RAM: %u x %u bytes -- BIS context", pLctrRtCfg->maxBis, sizeof(lctrBisCtx_t));

  /* Allocate BIS context memory. */
  pLctrBisTbl = (lctrBisCtx_t *)pAvailMem;
  pAvailMem += sizeof(lctrBisCtx_t) * pLctrRtCfg->maxBis;

  if (((uint32_t)pAvailMem) & 3)
  {
    /* Align to next word. */
    pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
  }

  LL_TRACE_INFO2("    RAM: %u x %u bytes -- BIG context", pLctrRtCfg->maxBig, sizeof(lctrBigCtx_t));

  /* Allocate BIG context memory. */
  pLctrBigTbl = (lctrBigCtx_t *)pAvailMem;
  pAvailMem += sizeof(lctrBigCtx_t) * pLctrRtCfg->maxBig;

  if (((uint32_t)(pAvailMem - pFreeMem)) > freeMemSize)
  {
    LL_TRACE_ERR2("LctrInitBisMem: failed to allocate BIG, need=%u available=%u", (pAvailMem - pFreeMem), freeMemSize);
    WSF_ASSERT(FALSE);
    return 0;
  }

  lmgrPersistCb.bisCtxSize = sizeof(lctrBisCtx_t);
  lmgrPersistCb.bigCtxSize = sizeof(lctrBigCtx_t);

  return (pAvailMem - pFreeMem);
}

/*************************************************************************************************/
/*!
 *  \brief  BIS Tx Data PDU complete handler.
 *
 *  \param  pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrNotifyIsoTxComplete(lctrBigCtx_t *pBigCtx)
{
  uint8_t numHandles = 0;
  uint16_t handle[LL_MAX_BIS] = { 0 };
  uint16_t numSdu[LL_MAX_BIS] = { 0 };

  /* Tally total transmitted SDUs. */
  for (unsigned int i = 0; i < pBigCtx->numBis; i++)
  {
    lctrBisCtx_t * const pBisCtx = pBigCtx->pBisCtx[i];

    if (pBisCtx &&
        (pBisCtx->roleData.slv.dataPathInCtx.id == LL_ISO_DATA_PATH_HCI))
    {
      if (pBisCtx->roleData.slv.numTxSduComp)
      {
        handle[numHandles] = pBisCtx->handle;
        numSdu[numHandles] = pBisCtx->roleData.slv.numTxSduComp;
        pBisCtx->roleData.slv.numTxSduComp = 0;
        numHandles++;
      }
    }
  }

  if (lmgrPersistCb.sendIsoCompCback && numHandles)
  {
    /* Notify host completed SDUs. */
    lmgrPersistCb.sendIsoCompCback(numHandles, handle, numSdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate Group session key.
 *
 *  \param  pGSKD       Group Session Key Diversifier.
 *  \param  pBC         Broadcast Code.
 *  \param  pGSK        Group Session Key result.
 */
/*************************************************************************************************/
void lctrBisCalcGroupSessionKey(const uint8_t *pGSKD, const uint8_t *pBC, uint8_t *pGSK)
{
  static const uint8_t big1[PAL_CRYPTO_LL_KEY_LEN] = { 0x31, 0x47, 0x49, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  static const uint8_t big2[4] = { 0x32, 0x47, 0x49, 0x42 };
  static const uint8_t big3[4] = { 0x33, 0x47, 0x49, 0x42 };
  uint8_t igltk[PAL_CRYPTO_LL_KEY_LEN];
  uint8_t gltk[PAL_CRYPTO_LL_KEY_LEN];
  uint8_t ik[PAL_CRYPTO_LL_KEY_LEN];

  /* IGLTK = h7("BIG1", Broadcast_Code) */
  PalCryptoAesCmac(big1, igltk, pBC, 16);

  /* GLTK = h6(IGLTK, "BIG2") */
  PalCryptoAesCmac(igltk, gltk, big2, 4);

  /* GSK = h8(GLTK, GSKD, "BIG3") */
  PalCryptoAesCmac(pGSKD, ik, gltk, 16);
  PalCryptoAesCmac(ik, pGSK, big3, 4);
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate next subevent index values, using sequential packing.
 *
 *  \param  pBigCtx     BIG context.
 *  \param  pSeCtx      Subevent context.
 *  \param  numSePkts   Total number of subevent packets.
 *
 *  \return TRUE if more subevents pending, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrBisCalcNextIdxSequential(lctrBigCtx_t *pBigCtx, lctrSeCtx_t *pSeCtx, uint8_t numSePkts)
{
  /* Burst loop. */

  pSeCtx->burstIdx++;

  if (pSeCtx->burstIdx >= pBigCtx->bn)      /* Last burst */
  {
    pSeCtx->burstIdx = 0;

    /* Retransmission loop. */

    pSeCtx->repIdx++;

    if (pSeCtx->repIdx >= pBigCtx->irc)     /* Last retransmission */
    {
      pSeCtx->repIdx = 0;

      if (numSePkts < pBigCtx->nse)
      {
        /* Pretransmission loop. */

        pSeCtx->ptIdx++;
        numSePkts++;
      }
      else
      {
        /* BIS loop. */

        pSeCtx->bisEvtIdx++;

        if (pSeCtx->bisEvtIdx >= pBigCtx->numBis)   /* Last BIS */
        {
          /* Control SubEvent */
          return FALSE;
        }
      }
    }
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate next subevent index values, using interleaved packing.
 *
 *  \param  pBigCtx     BIG context.
 *  \param  pSeCtx      Subevent context.
 *  \param  numSePkts   Total number of subevent packets.
 *
 *  \return TRUE if more subevents pending, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrBisCalcNextIdxInterleaved(lctrBigCtx_t *pBigCtx, lctrSeCtx_t *pSeCtx, uint8_t numSePkts)
{
  /* BIS loop. */

  pSeCtx->bisEvtIdx++;

  if (pSeCtx->bisEvtIdx >= pBigCtx->numBis)   /* Last BIS */
  {
    pSeCtx->bisEvtIdx = 0;

    /* Burst loop. */

    pSeCtx->burstIdx++;

    if (pSeCtx->burstIdx >= pBigCtx->bn)      /* Last burst */
    {
      pSeCtx->burstIdx = 0;

      /* Retransmission loop. */

      pSeCtx->repIdx++;

      if (pSeCtx->repIdx >= pBigCtx->irc)     /* Last retransmission */
      {
        pSeCtx->repIdx = 0;

        /* Pretransmission loop. */

        pSeCtx->ptIdx++;
        numSePkts++;

        if (numSePkts > pBigCtx->nse)
        {
          /* Control SubEvent */
          return FALSE;
        }
      }
    }
  }

  return TRUE;
}
