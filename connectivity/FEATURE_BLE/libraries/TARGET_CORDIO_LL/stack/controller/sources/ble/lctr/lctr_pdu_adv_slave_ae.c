/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave extended advertising operation builder implementation file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#include "ll_api.h"
#include "lctr_pdu_adv_ae.h"
#include "lctr_pdu_adv.h"
#include "lctr_int_adv_slave.h"
#include "lctr_int_conn_master.h"
#include "sch_api_ble.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include "pal_radio.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*! \brief      Assert minimum advertising payload length can contain a legacy length payload. */
WSF_CT_ASSERT(BB_ADV_PLD_MAX_LEN > (LL_EXT_ADV_HDR_MAX_LEN + LL_ADVBU_MAX_LEN));

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! \brief Packing lookup table; local use only. */
static const lctrPackAcad_t lctrPackAcad[LCTR_ACAD_NUM_ID] =
{
  lctrPackAcadChanMapUpd, /*!< Channel map update. */
  lctrPackAcadBigInfo     /*!< BIG Info. */
};

/*************************************************************************************************/
/*!
 *  \brief      Pack Sync info field.
 *
 *  \param      pAdvSet         Advertising set.
 *  \param      pSyncInfo       Packed SyncInfo field.
 */
/*************************************************************************************************/
static void lctrPackSyncInfo(lctrAdvSet_t *pAdvSet, uint8_t *pSyncInfo)
{
  uint8_t offsUnits;
  uint8_t offsAdjust = 0;
  uint16_t offs;
  uint32_t offsUsec, tempDue;
  BbOpDesc_t *pAuxOp = &pAdvSet->auxAdvBod;
  BbOpDesc_t *pPerOp = &pAdvSet->perParam.perAdvBod;
  BbBleData_t *pBle = &pAdvSet->perParam.perBleData;

  tempDue = pPerOp->dueUsec;

  while (TRUE)
  {
    /* If sync PDU is due in the future and at least MAFS apart, update offset. */
    if (BbGetTargetTimeDelta(tempDue, pAuxOp->dueUsec) > LL_BLE_MAFS_US)
    {
      offsUsec = BbGetTargetTimeDelta(tempDue, pAuxOp->dueUsec);
      break;
    }

    tempDue += pAdvSet->perParam.perAdvInterUsec;
  }

  if (offsUsec < LL_30_USEC_OFFS_MAX_USEC)
  {
    offsUnits = LCTR_OFFS_UNITS_30_USEC;
    offs = LL_MATH_DIV_30(offsUsec);
  }
  else
  {
    if (offsUsec >= LL_SYNC_OFFS_ADJUST_USEC)
    {
      offsAdjust = 1;
      offsUsec -= LL_SYNC_OFFS_ADJUST_USEC;
    }

    offsUnits = LCTR_OFFS_UNITS_300_USEC;
    offs = LL_MATH_DIV_300(offsUsec);
  }

  UINT16_TO_BUF(pSyncInfo, offs |                                   /* Sync packet offset. */
                           (offsUnits << 13) |                      /* Offset units. */
                           (offsAdjust << 14));                     /* Offset adjust. */
  UINT16_TO_BUF(&pSyncInfo[2],                                      /* Interval. */
                LCTR_PER_INTER_TO_MS(pAdvSet->perParam.perAdvInterUsec));
  uint64_t temp =
     ((pAdvSet->perParam.perChanParam.chanMask & LL_CHAN_DATA_ALL) |/* Channel Map. */
     ((uint64_t)lctrComputeSca() << 37));                           /* SCA. */
  UINT40_TO_BUF(&pSyncInfo[4], temp);
  UINT32_TO_BUF(&pSyncInfo[9], pBle->chan.accAddr);                 /* Access address. */
  UINT24_TO_BUF(&pSyncInfo[13], pBle->chan.crcInit);                /* CRCInit. */
  UINT16_TO_BUF(&pSyncInfo[16], pAdvSet->perParam.perEventCounter); /* Event counter. */
}

/*************************************************************************************************/
/*!
 *  \brief      Pack Advertising Header and Extended Advertising Headers.
 *
 *  \param      pAdvSet         Advertising set.
 *  \param      manExtHdrFlags  Mandatory extended header flags.
 *  \param      optExtHdrFlags  Optional extended header flags.
 *  \param      advMode         advMode field.
 *  \param      pPduHdr         Unpacked PDU header.
 *  \param      pDataBuf        Advertising data buffer descriptor (ignored if NULL).
 *  \param      pPduBuf         Buffer to store packed header.
 *  \param      commExtAdvPdu   Common extended advertising PDU type.
 *  \param      isPeriodic      This header is part of periodic PDUs or not.
 *
 *  \return     Combined Adv and ExtAdv headers length.
 */
/*************************************************************************************************/
static uint8_t lctrPackExtAdvHeader(lctrAdvSet_t *pAdvSet, uint8_t manExtHdrFlags, uint8_t optExtHdrFlags,
                                    uint8_t advMode, lctrAdvbPduHdr_t *pPduHdr, lctrAdvDataBuf_t *pDataBuf,
                                    uint8_t *pPduBuf, uint8_t commExtAdvPdu, bool_t isPeriodic)
{
  uint8_t extHdrFlags = 0;
  pPduHdr->len = 0;
  uint8_t *pAuxPtr = NULL;

  uint8_t *pExtHdrBuf = pPduBuf + LL_ADV_HDR_LEN + LCTR_EXT_HDR_CMN_LEN;
  uint8_t *pBuf = pExtHdrBuf + LCTR_EXT_HDR_FLAG_LEN;

  /* Determine the superior PDU. */
  if (commExtAdvPdu == LCTR_PDU_AUX_CHAIN_IND)
  {
    if (isPeriodic)
    {
      commExtAdvPdu = LCTR_PDU_AUX_SYNC_IND;
    }
    else
    {
      commExtAdvPdu = LCTR_PDU_AUX_ADV_IND;
    }
  }

  if ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_OMIT_AA_BIT) == 0)
  {
    if ((manExtHdrFlags & LL_EXT_HDR_ADV_ADDR_BIT) ||
        ((optExtHdrFlags & LL_EXT_HDR_ADV_ADDR_BIT) && (!(pAdvSet->extHdrFlags & LL_EXT_HDR_ADV_ADDR_BIT))))
    {
      /* Pack AdvA */
      extHdrFlags |= LL_EXT_HDR_ADV_ADDR_BIT;
      BDA64_TO_BSTREAM(pBuf, pAdvSet->advA);
#if (LL_ENABLE_TESTER == TRUE)
      if ((llTesterCb.extHdr.pduMatchMask & (1 << pPduHdr->pduType)) &&
          (llTesterCb.extHdr.modifyMask & LL_EXT_HDR_ADV_ADDR_BIT))
      {
        Bda64ToBstream(pBuf - BDA_ADDR_LEN, llTesterCb.extHdr.AdvA);
      }
#endif
    }
  }

  if ((manExtHdrFlags & LL_EXT_HDR_TGT_ADDR_BIT) ||
      ((optExtHdrFlags & LL_EXT_HDR_TGT_ADDR_BIT) && (!(pAdvSet->extHdrFlags & LL_EXT_HDR_TGT_ADDR_BIT))))
  {
    /* Pack TargetA */
    extHdrFlags |= LL_EXT_HDR_TGT_ADDR_BIT;
    BDA64_TO_BSTREAM(pBuf, pAdvSet->tgtA);

#if (LL_ENABLE_TESTER == TRUE)
    if ((llTesterCb.extHdr.pduMatchMask & (1 << pPduHdr->pduType)) &&
        (llTesterCb.extHdr.modifyMask & LL_EXT_HDR_TGT_ADDR_BIT))
    {
      Bda64ToBstream(pBuf - BDA_ADDR_LEN, llTesterCb.extHdr.TargetA);
    }
#endif
  }

  if ((manExtHdrFlags & LL_EXT_HDR_ADI_BIT) ||
      ((optExtHdrFlags & LL_EXT_HDR_ADI_BIT) &&
       (!(pAdvSet->extHdrFlags & LL_EXT_HDR_ADI_BIT) || (isPeriodic && pAdvSet->perParam.enableAdi))))
  {
    /* Pack AdvDataInfo */
    extHdrFlags |= LL_EXT_HDR_ADI_BIT;
    uint8_t did = isPeriodic ? pAdvSet->perParam.advDID : pAdvSet->param.advDID;
    UINT16_TO_BSTREAM(pBuf, (pAdvSet->param.advSID << 12) | ((did & 0x0FFF) << 0));

#if (LL_ENABLE_TESTER == TRUE)
    if ((llTesterCb.extHdr.pduMatchMask & (1 << pPduHdr->pduType)) &&
        (llTesterCb.extHdr.modifyMask & LL_EXT_HDR_ADI_BIT))
    {
      UINT16_TO_BUF(pBuf - sizeof(uint16_t), llTesterCb.extHdr.ADI);
    }
#endif
  }

  unsigned int availDataLen = LL_EXT_ADVB_MAX_LEN - (pBuf - pPduBuf);
  bool_t syncInfoNeeded = FALSE;
  if ((manExtHdrFlags & LL_EXT_HDR_SYNC_INFO_BIT) ||
      ((optExtHdrFlags & LL_EXT_HDR_SYNC_INFO_BIT) && (!(pAdvSet->extHdrFlags & LL_EXT_HDR_SYNC_INFO_BIT))))
  {
    syncInfoNeeded = TRUE;
    /* Reserve packet space now, pack later. */
    availDataLen -= LL_SYNC_INFO_LEN;
  }

  bool_t txPwrNeeded = FALSE;
  /* Tx Power field is never mandatory; skip check. */
  if ((optExtHdrFlags & LL_EXT_HDR_TX_PWR_BIT) &&
      (((commExtAdvPdu == LCTR_PDU_AUX_SYNC_IND) && (pAdvSet->perParam.advEventProp & LL_EXT_HDR_TX_PWR_BIT)) ||
       ((commExtAdvPdu != LCTR_PDU_AUX_SYNC_IND) && (pAdvSet->param.advEventProp & LL_EXT_HDR_TX_PWR_BIT))))
  {
    txPwrNeeded = TRUE;
    /* Reserve packet space now, pack later. */
    availDataLen -= LCTR_TX_POWER_LEN;
  }

  uint16_t remDataLen = 0;
  if (pDataBuf)
  {
    remDataLen = pDataBuf->len - pDataBuf->txOffs;
    if ((optExtHdrFlags & LL_EXT_HDR_AUX_PTR_BIT) &&  /* Allow fragmentation only if AUX_CHAIN_IND is allowed. */
        (pDataBuf->fragPref == LL_ADV_DATA_FRAG_ALLOW))
    {
      /* Adjust to fragment size. */
      availDataLen = WSF_MIN(pAdvSet->advDataFragLen, availDataLen);
    }
  }

  if ((manExtHdrFlags & LL_EXT_HDR_AUX_PTR_BIT) ||
      ((optExtHdrFlags & LL_EXT_HDR_AUX_PTR_BIT) && (remDataLen > availDataLen)))
  {
    /* Pack AuxPtr. */
    extHdrFlags |= LL_EXT_HDR_AUX_PTR_BIT;
    pAuxPtr = pBuf;

    /* Skip AuxPtr field; contents filled when values are known. */
    pBuf += LCTR_AUX_PTR_LEN;
  }

  if (syncInfoNeeded)
  {
    /* Pack SyncInfo */
    extHdrFlags |= LL_EXT_HDR_SYNC_INFO_BIT;
    uint8_t *pSyncInfo = pBuf;
    lctrPackSyncInfo(pAdvSet, pSyncInfo);
    pBuf += LL_SYNC_INFO_LEN;
  }

  if (txPwrNeeded)
  {
    /* Pack TxPower */
    extHdrFlags |= LL_EXT_HDR_TX_PWR_BIT;
    int8_t actTxPwr = PalRadioGetActualTxPower(pAdvSet->bleData.chan.txPower, TRUE);
#if (LL_ENABLE_TESTER == TRUE)
    if ((llTesterCb.extHdr.pduMatchMask & (1 << pPduHdr->pduType)) &&
        (llTesterCb.extHdr.modifyMask & LL_EXT_HDR_TX_PWR_BIT))
    {
      actTxPwr = llTesterCb.extHdr.TxPower;
    }
#endif
    UINT8_TO_BSTREAM(pBuf, (uint8_t)actTxPwr);
  }

  /* ACAD assembly. */
  bool_t acadPresent = FALSE;
  if (commExtAdvPdu == LCTR_PDU_AUX_SYNC_IND)
  {
    for (unsigned int acadId = 0; acadId < LCTR_ACAD_NUM_ID; acadId++)
    {
      if (pAdvSet->acadParams[acadId].hdr.state == LCTR_ACAD_STATE_ENABLED)
      {
        uint8_t acadLen = LL_ACAD_OPCODE_LEN + LL_ACAD_LEN_FIELD_LEN
                          + pAdvSet->acadParams[acadId].hdr.len;

        uint8_t availExtHdrLen = LL_EXT_ADV_HDR_MAX_LEN - (pBuf - pExtHdrBuf);

        /* Only add ACAD if space available. */
        if (acadLen <= availExtHdrLen)
        {
          acadPresent = TRUE;

          /* Pack ACAD immediately after the Extended Header. */
          pBuf += lctrPackAcad[acadId](pAdvSet, pBuf);
        }
      }
    }
  }

  if (!extHdrFlags && !acadPresent)
  {
    /* Recover optional ExtHdrFlag field since it is not used. */
    pBuf = pExtHdrBuf;
  }

  /* Assign AdvData to this PDU, defer buffer assembly to DMA gather. */
  uint8_t advDataLen = 0;
  if (remDataLen)
  {
    const unsigned int maxAvailDataLen = LL_EXT_ADVB_MAX_LEN - (pBuf - pPduBuf);
    availDataLen = WSF_MIN(maxAvailDataLen, availDataLen);  /* Limit to maximum packet size. */
    advDataLen = WSF_MIN(remDataLen, availDataLen);         /* Reduce to remaining data. */

    /* Commit AdvData fragment to this PDU. */
    pDataBuf->txOffs += advDataLen;
  }

  /* Pack Advertising Header now that Extended Advertising Header length is known. */
  uint16_t extHdrLen = pBuf - pExtHdrBuf;
  pPduHdr->len = LCTR_EXT_HDR_CMN_LEN + extHdrLen + advDataLen;

  lctrPackAdvbPduHdr(pPduBuf, pPduHdr);

  /* Pack Extended Advertising header; unconditionally write even if unused. */
  pPduBuf[LL_ADV_HDR_LEN + 0] = extHdrLen |         /* Extended Header Length */
                                (advMode << 6);     /* AdvMode */
  pPduBuf[LL_ADV_HDR_LEN + 1] = extHdrFlags;        /* Extended Header Flags */

  switch (commExtAdvPdu)
  {
    case LCTR_PDU_ADV_EXT_IND:
      pAdvSet->auxOffsUsec = 0;
      pAdvSet->extHdrFlags = extHdrFlags;
      /* AUX Offset field set dynamically in lctrSlvTxSetupExtAdvHandler() after AuxBod is scheduled. */
      pAdvSet->pExtAdvAuxPtr = pAuxPtr;
      break;

    case LCTR_PDU_AUX_ADV_IND:
      /* Compute auxiliary PDU AuxPtr fields (PDU is an auxiliary PDU). */
      pAdvSet->auxOffsUsec = 0;
      if (pAuxPtr)
      {
        BbBleData_t * const pBle = &pAdvSet->auxBleData;
        /* Now the PDU length/duration is known, complete AuxPtr fields for next CHAIN_IND. */
        pAdvSet->auxOffsUsec = SchBleCalcAuxPktDurationUsec(pBle->chan.txPhy, pBle->chan.initTxPhyOptions, LL_ADV_HDR_LEN + pPduHdr->len) +
        WSF_MAX(pAdvSet->auxDelayUsec, pLctrRtCfg->auxDelayUsec);
        pAdvSet->auxOffsUsec = WSF_MIN(pAdvSet->auxOffsUsec, LL_AUX_PTR_MAX_USEC);

        /* Round up auxOffsetUsec if necessary. */
        pAdvSet->auxOffsUsec = SchBleGetAlignedAuxOffsUsec(pAdvSet->auxOffsUsec);

        lctrPackAuxPtr(pAdvSet, pAdvSet->auxOffsUsec, pAdvSet->auxChIdx, pAuxPtr);
      }
      break;

    case LCTR_PDU_AUX_SCAN_RSP:
    case LCTR_PDU_AUX_CONNECT_RSP:
      pAdvSet->auxOffsUsec = 0;
      /* Compute auxiliary PDU AuxPtr fields (PDU is an auxiliary PDU). */
      if (pAuxPtr)
      {
        BbBleData_t * const pBle = &pAdvSet->auxBleData;

        /* Now the PDU length/duration is known, complete AuxPtr fields for next CHAIN_IND. */
        pAdvSet->auxOffsUsec = SchBleCalcAuxPktDurationUsec(pBle->chan.txPhy, (pBle->chan.tifsTxPhyOptions != BB_PHY_OPTIONS_DEFAULT) ? pBle->chan.tifsTxPhyOptions : pBle->op.slvAuxAdv.auxRxPhyOptions, LL_ADV_HDR_LEN + pPduHdr->len) +
                               WSF_MAX(pAdvSet->auxDelayUsec, pLctrRtCfg->auxDelayUsec);
        pAdvSet->auxOffsUsec = WSF_MIN(pAdvSet->auxOffsUsec, LL_AUX_PTR_MAX_USEC);

        /* Round up auxOffsetUsec if necessary. */
        pAdvSet->auxOffsUsec = SchBleGetAlignedAuxOffsUsec(pAdvSet->auxOffsUsec);

        lctrPackAuxPtr(pAdvSet, pAdvSet->auxOffsUsec, pAdvSet->auxChIdx, pAuxPtr);
      }
      break;

    case LCTR_PDU_AUX_SYNC_IND:
      pAdvSet->perParam.perOffsUsec = 0;
      /* Compute auxiliary PDU AuxPtr fields (PDU is an auxiliary PDU). */
      if (pAuxPtr)
      {
        BbBleData_t * const pBle = &pAdvSet->auxBleData;

        /* Now the PDU length/duration is known, complete AuxPtr fields for next CHAIN_IND. */
        pAdvSet->perParam.perOffsUsec = SchBleCalcAuxPktDurationUsec(pBle->chan.txPhy, pBle->chan.initTxPhyOptions, LL_ADV_HDR_LEN + pPduHdr->len) +
                                        WSF_MAX(pAdvSet->auxDelayUsec, pLctrRtCfg->auxDelayUsec);
        pAdvSet->perParam.perOffsUsec = WSF_MIN(pAdvSet->perParam.perOffsUsec, LL_AUX_PTR_MAX_USEC);

        lctrPackAuxPtr(pAdvSet, pAdvSet->perParam.perOffsUsec, pAdvSet->perParam.perChIdx, pAuxPtr);
      }
      break;
  }

  return pBuf - pPduBuf;
}

/*************************************************************************************************/
/*!
 *  \brief      Pack an ADV_EXT_IND PDU
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pPduBuf     Buffer to store packed PDU.
 *  \param      isPeriodic  This header is part of periodic advertising or not.
 *
 *  \return     PDU header buffer length.
 */
/*************************************************************************************************/
uint8_t lctrPackAdvExtIndPdu(lctrAdvSet_t *pAdvSet, uint8_t *pPduBuf, bool_t isPeriodic)
{
  BbBleData_t * const pBle = &pAdvSet->bleData;

  lctrAdvbPduHdr_t pduHdr = { 0 };
  pduHdr.pduType = LL_PDU_ADV_EXT_IND;

  uint8_t manExtHdrFlags = 0;
  uint8_t optExtHdrFlags = 0;

  uint16_t prop = pAdvSet->param.advEventProp &
                  (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT | LL_ADV_EVT_PROP_DIRECT_ADV_BIT);

  /*** Specification defined behavior ***/

  switch (prop)
  {
    /* Non-Connectable / Non-Scannable */
    case ( /* ------------------- */ 0 | /* ------------------- */ 0 | /* ------------------ */ 0 ):
      manExtHdrFlags = ((pAdvSet->advData.len >  0) ? LL_EXT_HDR_ADI_BIT : 0) |
                       ((pAdvSet->advData.len >  0) ? LL_EXT_HDR_AUX_PTR_BIT : 0);
      optExtHdrFlags = ((pAdvSet->param.priAdvPhy == LL_PHY_LE_1M) ? LL_EXT_HDR_ADV_ADDR_BIT : 0) |
                       ((pAdvSet->param.priAdvPhy == LL_PHY_LE_1M) ? LL_EXT_HDR_TX_PWR_BIT : 0) |
                       ((pAdvSet->advData.len == 0) ? LL_EXT_HDR_TX_PWR_BIT : 0);

      /* Pack aux pointer and ADI if periodic advertising is enabled. */
      if ((pAdvSet->perParam.perAdvEnabled == TRUE) && (pAdvSet->didPerUpdate == FALSE))
      {
        manExtHdrFlags |= LL_EXT_HDR_ADI_BIT;
        manExtHdrFlags |= LL_EXT_HDR_AUX_PTR_BIT;
      }

      if (!(manExtHdrFlags & LL_EXT_HDR_AUX_PTR_BIT))
      {
        /* Advertising address is mandatory if auxiliary pointer is NULL. */
        manExtHdrFlags |= LL_EXT_HDR_ADV_ADDR_BIT;
      }
      else
      {
        /* Make AdvA included in AUX_ADV_IND instead of ADV_EXT_IND. */
        if (lmgrGetOpFlag(LL_OP_MODE_FLAG_ENA_SLV_AUX_IND_ADVA) == TRUE)
        {
          optExtHdrFlags &= ~LL_EXT_HDR_ADV_ADDR_BIT;
        }
      }
      break;

    case ( /* ------------------- */ 0 | /* ------------------- */ 0 | LL_ADV_EVT_PROP_DIRECT_ADV_BIT ):
      manExtHdrFlags = ((pAdvSet->advData.len >  0) ? LL_EXT_HDR_ADI_BIT : 0) |
                       ((pAdvSet->advData.len >  0) ? LL_EXT_HDR_AUX_PTR_BIT : 0);
      optExtHdrFlags = ((pAdvSet->param.priAdvPhy == LL_PHY_LE_1M) ? LL_EXT_HDR_ADV_ADDR_BIT : 0) |
                       ((pAdvSet->param.priAdvPhy == LL_PHY_LE_1M) ? LL_EXT_HDR_TGT_ADDR_BIT : 0) |
                       ((pAdvSet->param.priAdvPhy == LL_PHY_LE_1M) ? LL_EXT_HDR_TX_PWR_BIT : 0) |
                       ((pAdvSet->advData.len == 0) ? LL_EXT_HDR_TX_PWR_BIT : 0);
      /* Pack aux pointer and ADI if periodic advertising is enabled. */
      if ((pAdvSet->perParam.perAdvEnabled == TRUE) && (pAdvSet->didPerUpdate == FALSE))
      {
        manExtHdrFlags |= LL_EXT_HDR_ADI_BIT;
        manExtHdrFlags |= LL_EXT_HDR_AUX_PTR_BIT;
      }

      if (!(manExtHdrFlags & LL_EXT_HDR_AUX_PTR_BIT))
      {
        /* Advertising address and target address are mandatory if auxiliary pointer is NULL. */
        manExtHdrFlags |= LL_EXT_HDR_ADV_ADDR_BIT;
        manExtHdrFlags |= LL_EXT_HDR_TGT_ADDR_BIT;
      }
      else
      {
        /* Make AdvA included in AUX_ADV_IND instead of ADV_EXT_IND. */
        if (lmgrGetOpFlag(LL_OP_MODE_FLAG_ENA_SLV_AUX_IND_ADVA) == TRUE)
        {
          optExtHdrFlags &= ~LL_EXT_HDR_ADV_ADDR_BIT;
        }
      }
      break;

    /* Connectable */
    case (LL_ADV_EVT_PROP_CONN_ADV_BIT | /* ------------------- */ 0 | /* ------------------ */ 0):
    case (LL_ADV_EVT_PROP_CONN_ADV_BIT | /* ------------------- */ 0 | LL_ADV_EVT_PROP_DIRECT_ADV_BIT):
      manExtHdrFlags = LL_EXT_HDR_ADI_BIT |
                       LL_EXT_HDR_AUX_PTR_BIT;
      optExtHdrFlags = ((pAdvSet->param.priAdvPhy == LL_PHY_LE_1M) ? LL_EXT_HDR_TX_PWR_BIT : 0);
      break;

    /* Scannable */
    case (/* ------------------- */ 0 | LL_ADV_EVT_PROP_SCAN_ADV_BIT | /* ------------------ */ 0):
    case (/* ------------------- */ 0 | LL_ADV_EVT_PROP_SCAN_ADV_BIT | LL_ADV_EVT_PROP_DIRECT_ADV_BIT):
      manExtHdrFlags = LL_EXT_HDR_ADI_BIT |
                       LL_EXT_HDR_AUX_PTR_BIT;
      optExtHdrFlags = ((pAdvSet->param.priAdvPhy == LL_PHY_LE_1M) ? LL_EXT_HDR_TX_PWR_BIT : 0);
      break;

    default:
      manExtHdrFlags = 0;
      break;
  }

  if (manExtHdrFlags & LL_EXT_HDR_AUX_PTR_BIT)
  {
    /* No need to include TxPower when auxiliary packet has this field. */
    optExtHdrFlags &= ~LL_EXT_HDR_TX_PWR_BIT;
  }

  /*** Host allowed options ***/
  lctrChooseSetAdvA(&pduHdr, pBle, pAdvSet);
  lctrChooseSetPeerA(&pduHdr, pBle, pAdvSet);

  uint8_t advMode = ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_CONN_ADV_BIT) ? LCTR_ADV_MODE_CONN_BIT : 0) |
                    ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_SCAN_ADV_BIT) ? LCTR_ADV_MODE_SCAN_BIT : 0);

  pAdvSet->extHdrFlags = 0;
  return lctrPackExtAdvHeader(pAdvSet, manExtHdrFlags, optExtHdrFlags,
                              advMode, &pduHdr, NULL,
                              pPduBuf, LCTR_PDU_ADV_EXT_IND, isPeriodic);
}

/*************************************************************************************************/
/*!
 *  \brief      Pack an AUX_ADV_IND PDU.
 *
 *  \param      pAdvSet         Advertising set.
 *  \param      pPduBuf         Buffer to store packed PDU.
 *  \param      pAdvData        Advertising data or scan response data descriptor.
 *  \param      isPeriodic      This header is part of periodic advertising or not.
 *
 *  \return     PDU header buffer length.
 */
/*************************************************************************************************/
uint8_t lctrPackAuxAdvIndPdu(lctrAdvSet_t *pAdvSet, uint8_t *pPduBuf, lctrAdvDataBuf_t *pAdvData,
                             bool_t isPeriodic)
{
  lctrAdvbPduHdr_t pduHdr = { 0 };
  pduHdr.pduType = LL_PDU_AUX_ADV_IND;
  BbBleData_t * const pBle = &pAdvSet->auxBleData;

  uint8_t manExtHdrFlags = 0;
  uint8_t optExtHdrFlags = 0;

  uint16_t prop = pAdvSet->param.advEventProp &
                  (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT | LL_ADV_EVT_PROP_DIRECT_ADV_BIT);

  /*** Specification defined behavior ***/

  switch (prop)
  {
    /* Non-Connectable / Non-Scannable */
    case ( /* ------------------- */ 0 | /* ------------------- */ 0 | /* ------------------ */ 0):
      manExtHdrFlags = LL_EXT_HDR_ADI_BIT;
      optExtHdrFlags = ((!(pAdvSet->extHdrFlags & LL_EXT_HDR_ADV_ADDR_BIT)) ? LL_EXT_HDR_ADV_ADDR_BIT : 0) |
                       LL_EXT_HDR_AUX_PTR_BIT |
                       LL_EXT_HDR_TX_PWR_BIT;
      /* Pack aux pointer if periodic adv is enabled. */
      if ((pAdvSet->perParam.perAdvEnabled == TRUE) && (pAdvSet->didPerUpdate == FALSE))
      {
        /* For synchronization, sync info is applied only after Data ID is updated in ext adv and aux adv is updated. */
        optExtHdrFlags |= LL_EXT_HDR_SYNC_INFO_BIT;
      }
      break;
    case ( /* ------------------- */ 0 | /* ------------------- */ 0 | LL_ADV_EVT_PROP_DIRECT_ADV_BIT):
      manExtHdrFlags = ((!(pAdvSet->extHdrFlags & LL_EXT_HDR_TGT_ADDR_BIT)) ? LL_EXT_HDR_TGT_ADDR_BIT : 0) |
                       LL_EXT_HDR_ADI_BIT;
      optExtHdrFlags = ((!(pAdvSet->extHdrFlags & LL_EXT_HDR_ADV_ADDR_BIT)) ? LL_EXT_HDR_ADV_ADDR_BIT : 0) |
                       LL_EXT_HDR_AUX_PTR_BIT |
                       LL_EXT_HDR_TX_PWR_BIT;
      /* Pack aux pointer if periodic adv is enabled. */
      if ((pAdvSet->perParam.perAdvEnabled == TRUE) && (pAdvSet->didPerUpdate == FALSE))
      {
        optExtHdrFlags |= LL_EXT_HDR_SYNC_INFO_BIT;
      }
      break;

    /* Connectable */
    case (LL_ADV_EVT_PROP_CONN_ADV_BIT | /* ------------------- */ 0 | /* ------------------ */ 0):
      manExtHdrFlags = LL_EXT_HDR_ADV_ADDR_BIT |
                       LL_EXT_HDR_ADI_BIT;
      optExtHdrFlags = LL_EXT_HDR_TX_PWR_BIT;
      break;
    case (LL_ADV_EVT_PROP_CONN_ADV_BIT | /* ------------------- */ 0 | LL_ADV_EVT_PROP_DIRECT_ADV_BIT):
      manExtHdrFlags = LL_EXT_HDR_ADV_ADDR_BIT |
                       LL_EXT_HDR_TGT_ADDR_BIT |
                       LL_EXT_HDR_ADI_BIT;
      optExtHdrFlags = LL_EXT_HDR_TX_PWR_BIT;
      break;

    /* Scannable */
    case (/* ------------------- */ 0 | LL_ADV_EVT_PROP_SCAN_ADV_BIT | /* ------------------ */ 0):
      manExtHdrFlags = LL_EXT_HDR_ADV_ADDR_BIT |
                       LL_EXT_HDR_ADI_BIT;
      optExtHdrFlags = LL_EXT_HDR_TX_PWR_BIT;
      break;
    case (/* ------------------- */ 0 | LL_ADV_EVT_PROP_SCAN_ADV_BIT | LL_ADV_EVT_PROP_DIRECT_ADV_BIT):
      manExtHdrFlags = LL_EXT_HDR_ADV_ADDR_BIT |
                       LL_EXT_HDR_TGT_ADDR_BIT |
                       LL_EXT_HDR_ADI_BIT;
      optExtHdrFlags = LL_EXT_HDR_ADI_BIT |
                       LL_EXT_HDR_TX_PWR_BIT;
      break;

    default:
      manExtHdrFlags = 0;
      break;
  }

  /*** Host allowed options ***/

  lctrChooseSetAdvA(&pduHdr, pBle, pAdvSet);
  lctrChooseSetPeerA(&pduHdr, pBle, pAdvSet);

  uint8_t advMode = ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_CONN_ADV_BIT) ? LCTR_ADV_MODE_CONN_BIT : 0) |
                    ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_SCAN_ADV_BIT) ? LCTR_ADV_MODE_SCAN_BIT : 0);

  return lctrPackExtAdvHeader(pAdvSet, manExtHdrFlags, optExtHdrFlags,
                              advMode, &pduHdr, pAdvData,
                              pPduBuf, LCTR_PDU_AUX_ADV_IND, isPeriodic);
}

/*************************************************************************************************/
/*!
 *  \brief      Pack an AUX_SCAN_RSP PDU.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pPduBuf     Buffer to store packed PDU.
 *  \param      isPeriodic  This header is part of periodic advertising or not.
 *
 *  \return     PDU header buffer length.
 */
/*************************************************************************************************/
uint8_t lctrPackAuxScanRspPdu(lctrAdvSet_t *pAdvSet, uint8_t *pPduBuf, bool_t isPeriodic)
{
  BbBleData_t * const pBle = &pAdvSet->auxBleData;
  lctrAdvbPduHdr_t pduHdr = { 0 };
  pduHdr.pduType = LL_PDU_AUX_SCAN_RSP;

  /*** Specification defined behavior ***/

  uint8_t manExtHdrFlags = LL_EXT_HDR_ADV_ADDR_BIT |
                           (lmgrGetOpFlag(LL_OP_MODE_FLAG_ENA_SLV_AUX_SCAN_RSP_ADI) ? LL_EXT_HDR_ADI_BIT : 0);
  uint8_t optExtHdrFlags = LL_EXT_HDR_AUX_PTR_BIT |
                           LL_EXT_HDR_TX_PWR_BIT;

  lctrChooseSetAdvA(&pduHdr, pBle, pAdvSet);
  lctrChooseSetPeerA(&pduHdr, pBle, pAdvSet);

  return lctrPackExtAdvHeader(pAdvSet, manExtHdrFlags, optExtHdrFlags,
                              0, &pduHdr, &pAdvSet->scanRspData,
                              pPduBuf, LCTR_PDU_AUX_SCAN_RSP, isPeriodic);
}

/*************************************************************************************************/
/*!
 *  \brief      Pack an AUX_CHAIN_IND PDU.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pPduBuf     Buffer to store packed PDU.
 *  \param      pAdvData    Advertising or Scan Response data.
 *  \param      isPeriodic  This header is part of periodic advertising or not.
 *
 *  \return     PDU header buffer length.
 */
/*************************************************************************************************/
uint8_t lctrPackAuxChainIndPdu(lctrAdvSet_t *pAdvSet, uint8_t *pPduBuf, lctrAdvDataBuf_t *pAdvData,
                               bool_t isPeriodic)
{
  lctrAdvbPduHdr_t pduHdr = { 0 };
  pduHdr.pduType = LL_PDU_AUX_CHAIN_IND;

  /*** Specification defined behavior ***/

  uint8_t manExtHdrFlags = LL_EXT_HDR_ADI_BIT;
  uint8_t optExtHdrFlags = LL_EXT_HDR_AUX_PTR_BIT;

  /* AUX_CHAIN_IND following AUX_SYNC_IND or AUX_SCAN_RSP should not have ADI field. */
  if (isPeriodic || (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_SCAN_ADV_BIT))
  {
    manExtHdrFlags &= ~LL_EXT_HDR_ADI_BIT;
  }

  return lctrPackExtAdvHeader(pAdvSet, manExtHdrFlags, optExtHdrFlags,
                              0, &pduHdr, pAdvData,
                              pPduBuf, LCTR_PDU_AUX_CHAIN_IND, isPeriodic);
}

/*************************************************************************************************/
/*!
 *  \brief      Pack an AUX_CONNECT_RSP PDU.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pPduBuf     Buffer to store packed PDU.
 *  \param      isPeriodic  This header is part of periodic advertising or not.
 *
 *  \return     PDU header buffer length.
 */
/*************************************************************************************************/
uint8_t lctrPackAuxConnRspPdu(lctrAdvSet_t *pAdvSet, uint8_t *pPduBuf, bool_t isPeriodic)
{
  BbBleData_t * const pBle = &pAdvSet->auxBleData;

  pAdvSet->rspPduHdr.pduType = LL_PDU_AUX_CONNECT_RSP;

  /*** Specification defined behavior ***/

  uint8_t manExtHdrFlags = LL_EXT_HDR_ADV_ADDR_BIT |
                           LL_EXT_HDR_TGT_ADDR_BIT;

  lctrChooseSetAdvA(&pAdvSet->rspPduHdr, pBle, pAdvSet);
  lctrChooseSetPeerA(&pAdvSet->rspPduHdr, pBle, pAdvSet);

  return lctrPackExtAdvHeader(pAdvSet, manExtHdrFlags, 0,
                              0, &pAdvSet->rspPduHdr, NULL,
                              pPduBuf, LCTR_PDU_AUX_CONNECT_RSP, isPeriodic);
}

/*************************************************************************************************/
/*!
 *  \brief      Pack AuxPtr field.
 *
 *  \param      pAdvSet         Advertising set.
 *  \param      offsUsec        Auxiliary offset in microseconds.
 *  \param      chIdx           Channel index.
 *  \param      pAuxPtr         Packed AuxPtr buffer.
 */
/*************************************************************************************************/
void lctrPackAuxPtr(lctrAdvSet_t const *pAdvSet, uint32_t offsUsec, uint8_t chIdx, uint8_t *pAuxPtr)
{
  uint8_t ca = (BbGetClockAccuracy() <= 50) ? LCTR_CLK_ACC_0_50_PPM : LCTR_CLK_ACC_51_500_PPM;
  uint8_t offsUnits;
  uint16_t auxOffset;

  if (offsUsec < LL_30_USEC_OFFS_MAX_USEC)
  {
    offsUnits = LCTR_OFFS_UNITS_30_USEC;
    auxOffset = LL_MATH_DIV_30(offsUsec);
  }
  else
  {
    offsUnits = LCTR_OFFS_UNITS_300_USEC;
    auxOffset = LL_MATH_DIV_300(offsUsec);
  }

  if (auxOffset > LCTR_AUX_PTR_MAX_OFFSET)
  {
    LL_TRACE_ERR1("AUX Offset exceeds max allowed value: %u", auxOffset);
  }

  pAuxPtr[0] = chIdx |                  /* LL Channel */
               (ca << 6) |              /* CA */
               (offsUnits << 7);        /* Offset Units */

  /* Map used PHY. */
  uint8_t usedPhy;
  switch (pAdvSet->param.secAdvPhy)
  {
    case LL_PHY_LE_2M:
      usedPhy = 1;
      break;
    case LL_PHY_LE_CODED:
      usedPhy = 2;
      break;
    default:
    case LL_PHY_LE_1M:
      usedPhy = 0;
      break;
  }

  UINT16_TO_BUF(&pAuxPtr[1], auxOffset | (usedPhy << LCTR_AUX_OFFS_USED_PHY_SHIFT));
}

/*************************************************************************************************/
/*!
 *  \brief      Pack a legacy Advertising PDU.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pPduBuf     Buffer to store packed PDU.
 *
 *  \return     PDU header buffer length.
 */
/*************************************************************************************************/
uint8_t lctrPackLegacyAdvPdu(lctrAdvSet_t *pAdvSet, uint8_t *pPduBuf)
{
  uint8_t *pBuf = pPduBuf + LL_ADV_HDR_LEN;
  lctrAdvbPduHdr_t pduHdr = { 0 };

  BbBleData_t * const pBle = &pAdvSet->bleData;

  lctrChooseSetAdvA(&pduHdr, pBle, pAdvSet);
  lctrChooseSetPeerA(&pduHdr, pBle, pAdvSet);

  uint16_t prop = pAdvSet->param.advEventProp &
                  (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT | LL_ADV_EVT_PROP_DIRECT_ADV_BIT);

  switch (prop)
  {
    case ( LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT | /* ------------------ */ 0 ):
      pduHdr.pduType = LL_PDU_ADV_IND;
      break;
    case ( LL_ADV_EVT_PROP_CONN_ADV_BIT | /* -------------------- */ 0 | LL_ADV_EVT_PROP_DIRECT_ADV_BIT ):
      pduHdr.pduType = LL_PDU_ADV_DIRECT_IND;
      break;
    case ( /* -------------------- */ 0 | LL_ADV_EVT_PROP_SCAN_ADV_BIT | /* ------------------ */ 0 ):
      pduHdr.pduType = LL_PDU_ADV_SCAN_IND;
      break;
    case ( /* -------------------- */ 0 | /* -------------------- */ 0 | /* ------------------ */ 0 ):
    default:
      pduHdr.pduType = LL_PDU_ADV_NONCONN_IND;
      break;
  }

  /* Pack AdvA */
  BDA64_TO_BSTREAM(pBuf, pAdvSet->advA);

  /* Pack TargetA. */
  switch (pduHdr.pduType)
  {
    case LL_PDU_ADV_DIRECT_IND:
    {
      /* Always match peer address in PDU to initiator's address for directed advertising. */
      BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_MATCH_ENA);
      BDA64_TO_BSTREAM(pBuf, pAdvSet->tgtA);
      break;
    }
    case LL_PDU_ADV_IND:
    case LL_PDU_ADV_NONCONN_IND:
    case LL_PDU_ADV_SCAN_IND:
    default:
      break;
  }

  /* Pack AdvData. */
  uint8_t advDataLen = pAdvSet->advData.alt.legacy.len;
  switch (pduHdr.pduType)
  {
    case LL_PDU_ADV_IND:
    case LL_PDU_ADV_NONCONN_IND:
    case LL_PDU_ADV_SCAN_IND:
      if (pAdvSet->advData.alt.legacy.len > 0)
      {
        /* Legacy buffers always stored in cache (exclusive buffer). */
        memcpy(pBuf, pAdvSet->advData.alt.legacy.buf, WSF_MIN(pAdvSet->advData.alt.legacy.len, LL_ADVBU_MAX_LEN));
      }
      break;
    case LL_PDU_ADV_DIRECT_IND:
    default:
      /* Not allowed. */
      advDataLen = 0;
      break;
  }

  /* Pack header. */
  if ((lmgrCb.features & LL_FEAT_CH_SEL_2) &&
      ((pduHdr.pduType == LL_PDU_ADV_IND) || (pduHdr.pduType == LL_PDU_ADV_DIRECT_IND)))
  {
    pduHdr.chSel = LL_CH_SEL_2;
  }
  /* else use LL_CH_SEL_1 */
  pduHdr.len = pBuf - (pPduBuf + LL_ADV_HDR_LEN) + advDataLen;
  lctrPackAdvbPduHdr(pPduBuf, &pduHdr);

  return LL_ADV_HDR_LEN + pduHdr.len;
}

/*************************************************************************************************/
/*!
 *  \brief      Pack legacy SCAN_RSP PDU.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pPduBuf     Buffer to store packed PDU.
 *
 *  \return     PDU header buffer length.
 */
/*************************************************************************************************/
uint8_t lctrPackLegacyScanRspPdu(lctrAdvSet_t *pAdvSet, uint8_t *pPduBuf)
{
  uint8_t *pBuf = pPduBuf;
  lctrAdvbPduHdr_t pduHdr = { 0 };

  pduHdr.pduType = LL_PDU_SCAN_RSP;

  switch (pAdvSet->param.ownAddrType)
  {
    case LL_ADDR_RANDOM:
    case LL_ADDR_RANDOM_IDENTITY:
      pduHdr.txAddrRnd = LL_ADDR_RANDOM;
      break;
    case LL_ADDR_PUBLIC_IDENTITY:
      if (BDA64_ADDR_IS_RPA(pAdvSet->advA))
      {
        pduHdr.txAddrRnd = LL_ADDR_RANDOM;
      }
      else
      {
        pduHdr.txAddrRnd = LL_ADDR_PUBLIC;
      }
      break;
    case LL_ADDR_PUBLIC:
    default:
      pduHdr.txAddrRnd = LL_ADDR_PUBLIC;
      break;
  }
  pduHdr.len = LL_SCAN_PREFIX_LEN + pAdvSet->scanRspData.alt.legacy.len;

  pBuf += lctrPackAdvbPduHdr(pBuf, &pduHdr);
  BDA64_TO_BSTREAM(pBuf, pAdvSet->advA);
  if (pAdvSet->scanRspData.alt.legacy.len > 0)
  {
    /* Legacy buffers always stored in cache (exclusive buffer). */
    memcpy(pBuf, pAdvSet->scanRspData.alt.legacy.buf, WSF_MIN(pAdvSet->scanRspData.alt.legacy.len, LL_ADVBU_MAX_LEN));
  }

  return LL_ADV_HDR_LEN + pduHdr.len;
}

/*************************************************************************************************/
/*!
 *  \brief      Pack an AUX_SYNC_IND PDU.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pPduBuf     Buffer to store packed PDU.
 *  \param      pAdvData    Advertising or Scan Response data.
 *  \param      isPeriodic  This header is part of periodic PDUs or not.
 *
 *  \return     PDU header buffer length.
 */
/*************************************************************************************************/
uint8_t lctrPackSyncIndPdu(lctrAdvSet_t *pAdvSet, uint8_t *pPduBuf, lctrAdvDataBuf_t *pAdvData, bool_t isPeriodic)
{
  lctrAdvbPduHdr_t pduHdr = { 0 };
  pduHdr.pduType = LL_PDU_AUX_SYNC_IND;

  /*** Specification defined behavior ***/

  uint8_t manExtHdrFlags = 0;
  uint8_t optExtHdrFlags = LL_EXT_HDR_AUX_PTR_BIT |
                           ((lmgrCb.features & LL_FEAT_PER_ADV_ADI_SUP) ? LL_EXT_HDR_ADI_BIT : 0) |
                           ((pAdvSet->perParam.advEventProp & LL_EXT_HDR_TX_PWR_BIT) ? LL_EXT_HDR_TX_PWR_BIT : 0);

  return lctrPackExtAdvHeader(pAdvSet, manExtHdrFlags, optExtHdrFlags,
                              0, &pduHdr, pAdvData,
                              pPduBuf, LCTR_PDU_AUX_SYNC_IND, isPeriodic);
}

/*************************************************************************************************/
/*!
 *  \brief      Pack Channel Map Update ACAD.
 *
 *  \param      pAdvSet         Advertising set.
 *  \param      pBuf            Packed data buffer.
 *
 *  \return     Length of ACAD.
 */
/*************************************************************************************************/
uint8_t lctrPackAcadChanMapUpd(lctrAdvSet_t *pAdvSet, uint8_t *pBuf)
{
  const uint8_t len = LL_ACAD_LEN_FIELD_LEN + LL_ACAD_CHAN_MAP_UPD_LEN;

  LctrAcadChanMapUpd_t *pChanMapUpd = &pAdvSet->acadParams[LCTR_ACAD_ID_CHAN_MAP_UPDATE].chanMapUpdate;

  /* Pack ACAD */
  UINT8_TO_BSTREAM (pBuf, LL_ACAD_CHAN_MAP_UPD_LEN);
  UINT8_TO_BSTREAM (pBuf, LL_ACAD_OPCODE_CHAN_MAP_UPD);
  UINT40_TO_BSTREAM(pBuf, pChanMapUpd->chanMask);
  UINT16_TO_BSTREAM(pBuf, pChanMapUpd->instant);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief      Pack BIG Info ACAD.
 *
 *  \param      pAdvSet         Advertising set.
 *  \param      pBuf            Packed data buffer.
 *
 *  \return     Length of ACAD.
 */
/*************************************************************************************************/
uint8_t lctrPackAcadBigInfo(lctrAdvSet_t *pAdvSet, uint8_t *pBuf)
{
  LctrAcadBigInfo_t *pBigInfo = &pAdvSet->acadParams[LCTR_ACAD_ID_BIG_INFO].bigInfo;

  const uint8_t len = LL_ACAD_LEN_FIELD_LEN + LL_ACAD_OPCODE_LEN + pBigInfo->hdr.len;

  /* Convert PHY value to BIG Info enumeration. */
  uint8_t bigPhy = pBigInfo->phy - 1;

  /* Pack ACAD */
  UINT8_TO_BSTREAM (pBuf, pBigInfo->hdr.len + LL_ACAD_LEN_FIELD_LEN);
  UINT8_TO_BSTREAM (pBuf, LL_ACAD_OPCODE_BIG_INFO);

  UINT32_TO_BSTREAM(pBuf, ((pBigInfo->bigOffs         & 0x3FFF) <<  0)    |
                          ((pBigInfo->bigOffsUnits    & 0x0001) << 14)    |
                          ((pBigInfo->isoInter                ) << 15)    |
                          ((pBigInfo->numBis                  ) << 27));
  UINT32_TO_BSTREAM(pBuf, ((pBigInfo->nse                     ) <<  0)    |
                          ((pBigInfo->bn                      ) <<  5)    |
                          ((pBigInfo->subEvtInterUsec         ) <<  8)    |
                          ((pBigInfo->pto                     ) << 28));
  UINT32_TO_BSTREAM(pBuf, ((pBigInfo->bisSpaceUsec            ) <<  0)    |
                          ((pBigInfo->irc                     ) << 20)    |
                          ((pBigInfo->maxPdu                  ) << 24));
  UINT8_TO_BSTREAM (pBuf, ((0 /* RFU */                       ) <<  0));
  UINT32_TO_BSTREAM(pBuf, ((pBigInfo->seedAccAddr             ) <<  0));
  UINT32_TO_BSTREAM(pBuf, ((pBigInfo->sduInterUsec            ) <<  0)    |
                          ((pBigInfo->maxSdu                  ) << 20));
  UINT16_TO_BSTREAM(pBuf, ((pBigInfo->baseCrcInit             ) <<  0));
  UINT40_TO_BSTREAM(pBuf, ((pBigInfo->chanMap                 ) <<  0)    |
                (((uint64_t)bigPhy                            ) << 37));
  UINT40_TO_BSTREAM(pBuf, ((pBigInfo->bisPldCtr & UINT64_C(0x7FFFFFFFFF)) <<  0) |
                          ((pBigInfo->framing   & UINT64_C(1))            << 39));
  if (pBigInfo->encrypt)
  {
    memcpy(pBuf, pBigInfo->giv, LL_GIV_LEN);
    pBuf += LL_GIV_LEN;
    memcpy(pBuf, pBigInfo->gskd, LL_GSKD_LEN);
    pBuf += LL_GSKD_LEN;
  }

  return len;
}
