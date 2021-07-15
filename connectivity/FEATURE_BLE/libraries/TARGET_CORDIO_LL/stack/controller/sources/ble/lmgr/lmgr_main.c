/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link manager common implementation file.
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
#include "ll_math.h"
#include "lmgr_api.h"
#include "lhci_api.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include <string.h>

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Link layer manager system control block (persists with resets). */
lmgrPersistCtrlBlk_t lmgrPersistCb;

/*! \brief      Link layer manager control block (clears with resets). */
lmgrCtrlBlk_t lmgrCb;

/*************************************************************************************************/
/*!
 *  \brief      Set default values.
 *
 *  Restore default values to fields that require initial state after reset. Values that
 *  survive reset are unchanged.
 */
/*************************************************************************************************/
void LmgrSetDefaults(void)
{
  /* Ensure reset shuts down resources. */
  WSF_ASSERT(lmgrCb.numConnEnabled == 0);
  WSF_ASSERT(lmgrCb.advEnabled == FALSE);
  WSF_ASSERT(lmgrCb.numExtAdvEnabled == 0);
  WSF_ASSERT(lmgrCb.numScanEnabled == 0);
  WSF_ASSERT(lmgrCb.numInitEnabled == 0);
  WSF_ASSERT(lmgrCb.numWlFilterEnabled == 0);
  WSF_ASSERT(lmgrCb.numPlFilterEnabled == 0);
  WSF_ASSERT(lmgrCb.testEnabled == FALSE);

  memset(&lmgrCb, 0, sizeof(lmgrCb));

  lmgrCb.features = lmgrPersistCb.featuresDefault;

  lmgrCb.opModeFlags =
    /* LL_OP_MODE_FLAG_ENA_VER_LLCP_STARTUP |  */       /* disabled */
    /* LL_OP_MODE_FLAG_SLV_REQ_IMMED_ACK |  */          /* disabled */
    /* LL_OP_MODE_FLAG_BYPASS_CE_GUARD |  */            /* disabled */
    /* LL_OP_MODE_FLAG_MST_RETX_AFTER_RX_NACK |  */     /* disabled */
    /* LL_OP_MODE_FLAG_MST_IGNORE_CP_RSP |  */          /* disabled */
    /* LL_OP_MODE_FLAG_MST_UNCOND_CP_RSP |  */          /* disabled */
    LL_OP_MODE_FLAG_ENA_LEN_LLCP_STARTUP |
    /* LL_OP_MODE_FLAG_REQ_SYM_PHY |  */                /* disabled */
    /* LL_OP_MODE_FLAG_ENA_FEAT_LLCP_STARTUP | */       /* disabled. */
    /* LL_OP_MODE_FLAG_SLV_DELAY_LLCP_STARTUP |  */     /* disabled */
    LL_OP_MODE_FLAG_ENA_SLV_LATENCY_WAKEUP |
    LL_OP_MODE_FLAG_ENA_SLV_AUX_SCAN_RSP_ADI |
    LL_OP_MODE_FLAG_ENA_MST_CIS_NULL_PDU |
    /* LL_OP_MODE_FLAG_ENA_SLV_AUX_IND_ADVA |  */       /* disabled */
    LL_OP_MODE_FLAG_ENA_ADV_CHAN_RAND |
    /* LL_OP_MODE_FLAG_DIS_POWER_MONITOR |  */          /* disabled */
    /* LL_OP_MODE_FLAG_ENA_BIS_RECV_DUP |  */           /* disabled */
    /* LL_OP_MODE_FLAG_ENA_ISO_LOST_NOTIFY | */
    LL_OP_MODE_FLAG_ENA_CH_RPT_LLCP_AFTER_FEAT |
    /* LL_OP_MODE_FLAG_IGNORE_CRC_ERR_TS | */           /* disabled */
    /* LL_OP_MODE_FLAG_FORCE_CIS_CODED_PHY_S2 | */      /* disabled */

    /* diagnostics only */
    LL_OP_MODE_FLAG_ENA_ADV_DLY |
    LL_OP_MODE_FLAG_ENA_SCAN_BACKOFF |
    LL_OP_MODE_FLAG_ENA_WW |
    LL_OP_MODE_FLAG_ENA_SLV_LATENCY |
    LL_OP_MODE_FLAG_ENA_LLCP_TIMER |
    LL_OP_MODE_FLAG_ENA_SUBRATE_CE |
    0;

  lmgrCb.chanClass = LL_CHAN_DATA_ALL;
}

/*************************************************************************************************/
/*!
 *  \brief      Increment reset delay counter.
 */
/*************************************************************************************************/
void LmgrIncResetRefCount(void)
{
  lmgrCb.resetDelayRefCnt++;
}

/*************************************************************************************************/
/*!
 *  \brief      Decrement reset delay counter.
 */
/*************************************************************************************************/
void LmgrDecResetRefCount(void)
{
  WSF_ASSERT(lmgrCb.resetDelayRefCnt);
  lmgrCb.resetDelayRefCnt--;
}

/*************************************************************************************************/
/*!
 *  \brief      Increment whitelist filter enable counter.
 */
/*************************************************************************************************/
void LmgrIncWhitelistRefCount(void)
{
  lmgrCb.numWlFilterEnabled++;
}

/*************************************************************************************************/
/*!
 *  \brief      Decrement whitelist filter enable counter.
 */
/*************************************************************************************************/
void LmgrDecWhitelistRefCount(void)
{
  WSF_ASSERT(lmgrCb.numWlFilterEnabled);
  lmgrCb.numWlFilterEnabled--;
}

/*************************************************************************************************/
/*!
 *  \brief      Increment periodiclist filter enable counter.
 */
/*************************************************************************************************/
void LmgrIncPeriodiclistRefCount(void)
{
  lmgrCb.numPlFilterEnabled++;
}

/*************************************************************************************************/
/*!
 *  \brief      Decrement periodiclist filter enable counter.
 */
/*************************************************************************************************/
void LmgrDecPeriodiclistRefCount(void)
{
  WSF_ASSERT(lmgrCb.numPlFilterEnabled);
  lmgrCb.numPlFilterEnabled--;
}

/*************************************************************************************************/
/*!
 *  \brief  Check that that a given address type is available.
 *
 *  \param  ownAddrType     Own address type.
 *
 *  \return TRUE if value is valid, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LmgrIsAddressTypeAvailable(uint8_t ownAddrType)
{
  bool_t isAddrTypeValid = TRUE;

  switch (ownAddrType)
  {
    case LL_ADDR_RANDOM:
    case LL_ADDR_RANDOM_IDENTITY:
      if (!lmgrCb.bdAddrRndValid)
      {
        /* Random address not available. */
        isAddrTypeValid = FALSE;
      }
      break;
    case LL_ADDR_PUBLIC:
    case LL_ADDR_PUBLIC_IDENTITY:
    case LL_ADDR_ANONYMOUS:
      /* Addresses are always available. */
      break;
    default:
      /* Invalid type. */
      isAddrTypeValid = FALSE;
  }

  return isAddrTypeValid;
}

/*************************************************************************************************/
/*!
 *  \brief      Check that extended commands have not been issued.
 *
 *  \return     TRUE if allowed, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LmgrIsLegacyCommandAllowed(void)
{
  if (lmgrCb.useExtCmds)
  {
    return FALSE;
  }

  lmgrCb.useLegacyCmds = TRUE;
  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Check that legacy commands have not been issued.
 *
 *  \return     TRUE if allowed, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LmgrIsExtCommandAllowed(void)
{
  if (lmgrCb.useLegacyCmds)
  {
    return FALSE;
  }

  lmgrCb.useExtCmds = TRUE;
  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Build channel remapping table.
 *
 *  \param  pChanParam  Channel parameters.
 */
/*************************************************************************************************/
void LmgrBuildRemapTable(lmgrChanParam_t *pChanParam)
{
  unsigned int chanIdx;
  unsigned int numUsedChan = 0;

  for (chanIdx = 0; chanIdx < sizeof(pChanParam->chanRemapTbl); chanIdx++)
  {
    if (pChanParam->chanMask & (UINT64_C(1) << chanIdx))
    {
      pChanParam->chanRemapTbl[numUsedChan++] = chanIdx;
    }
  }

  WSF_ASSERT(numUsedChan);          /* must have at least one channel */

  pChanParam->numUsedChan = numUsedChan;
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate permutation.
 *
 *  \param  v       Input value.
 *
 *  \return Result of permutation calculation.
 */
/*************************************************************************************************/
static inline uint32_t lmgrCalcPerm(uint32_t v)
{
  uint32_t mask;

  /*          abcd efgh ijkl mnop */
  mask = 0x0F0F0F0F;
  v    = ((v << 8) & mask) | (v & ~mask);

  /*     efgh abcd mnop ijkl xxxx */
  mask = mask ^ (mask << 2);
  v    = ((v << 4) & mask) | (v & ~mask);

  /*  gh efcd abop mnkl ijxx xxxx */
  mask = mask ^ (mask << 1);
  v    = ((v << 2) & mask) | (v & ~mask);

  /* hgf edcb apon mlkj ixxx xxxx */
  return v >> 7;
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate MAM (Multiply, Add, Modulo).
 *
 *  \param  a       Input value a.
 *  \param  b       Input value b.
 *
 *  \return Result of MAM.
 */
/*************************************************************************************************/
static inline uint16_t lmgrCalcMAM(uint16_t a, uint16_t b)
{
  /* (17 x a + b) mod 2^16 */
  return ((17 * a) + b) & 0xFFFFF;
}

/*************************************************************************************************/
/*!
 *  \brief  Compute next channel index, algorithm #2.
 *
 *  \param  pChanParam    Channel parameters.
 *  \param  eventCounter  Event Counter.
 *  \param  numSkip       Number of skip.
 *  \param  calSubEvt     TRUE if calculation of subevent is needed.
 *
 *  \return Next data channel index.
 */
/*************************************************************************************************/
uint8_t LmgrSelectNextChannel(lmgrChanParam_t *pChanParam, uint16_t eventCounter, uint16_t numSkip, bool_t calSubEvt)
{
  unsigned int prn;

  /* Ignore parameter since eventCounter is adjusted by client. */
  (void)numSkip;

  /* Pseudo random number */

  prn = eventCounter ^ pChanParam->chIdentifier;
  prn = lmgrCalcPerm(prn);
  prn = lmgrCalcMAM(prn, pChanParam->chIdentifier);
  prn = lmgrCalcPerm(prn);
  prn = lmgrCalcMAM(prn, pChanParam->chIdentifier);
  prn = lmgrCalcPerm(prn);
  prn = lmgrCalcMAM(prn, pChanParam->chIdentifier);

  pChanParam->prnLast = prn;      /* Used by subevent only. */

  unsigned int prn_e = prn ^ pChanParam->chIdentifier;

  /* unmappedChannel */

  uint16_t unmapChan = LL_MATH_MOD_37(prn_e & 0xFFFF);

  /* remappingIndex */

  if (!((UINT64_C(1) << unmapChan) & pChanParam->chanMask))
  {
    /* remappingIndex = (N * prn_e) / 2^16 */
    uint8_t remapIdx = (pChanParam->numUsedChan * prn_e) >> 16;
    if (calSubEvt)
    {
      pChanParam->subEvtIdx = remapIdx;
    }
    return pChanParam->chanRemapTbl[remapIdx];
  }
  else
  {
    if (calSubEvt)
    {
      for (unsigned int chanIdx = 0; chanIdx < sizeof(pChanParam->chanRemapTbl); chanIdx++)
      {
        if (pChanParam->chanRemapTbl[chanIdx] == unmapChan)
        {
          pChanParam->subEvtIdx = chanIdx;
          break;
        }
      }
    }
  }

  return unmapChan;
}

/*************************************************************************************************/
/*!
 *  \brief  Compute next subevent channel index, algorithm #2.
 *
 *  \param  pChanParam    Channel parameters.
 *
 *  \return Next data channel index.
 */
/*************************************************************************************************/
uint8_t LmgrSelectNextSubEvtChannel(lmgrChanParam_t *pChanParam)
{
  uint16_t prn = pChanParam->prnLast;
  uint8_t subEvtIdx = pChanParam->subEvtIdx;
  uint8_t d;

  /* Subevent pseudo random number generator. */
  prn = lmgrCalcPerm(prn);
  prn = lmgrCalcMAM(prn, pChanParam->chIdentifier);   /* prn is prnSubEvent_lu */

  pChanParam->prnLast = prn;

  prn = prn ^ pChanParam->chIdentifier;               /* prn is prnSubEvent_se */

  /* Subevent mapped to used channel index. */
  /* d = max(1, max(min(3, N-5), min(11, (N-10)/2))) */
  d = WSF_MAX(1, WSF_MAX(WSF_MIN(3, (pChanParam->numUsedChan - 5)), WSF_MIN(11, ((pChanParam->numUsedChan - 10) >> 1))));

  /* subEventIndex = (indexofLAstUsedChannel + d + (prnSubEvent_se * (N - 2 * d +1)/ 2 ^16)) mod N */
  uint16_t temp = subEvtIdx + d + ((prn * (pChanParam->numUsedChan - (d << 1) + 1)) >> 16);
  subEvtIdx = temp - (temp / pChanParam->numUsedChan) * pChanParam->numUsedChan;

  pChanParam->subEvtIdx = subEvtIdx;                  /* Update subEvtIdx */

  return pChanParam->chanRemapTbl[subEvtIdx];
}
