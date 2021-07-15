/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave extended advertising operation builder implementation file.
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

#include "lctr_api_adv_slave_ae.h"
#include "lctr_int_adv_slave_ae.h"
#include "lctr_int_adv_slave.h"
#include "lctr_pdu_adv_ae.h"
#include "lctr_pdu_adv.h"
#include "lctr_int.h"
#include "lctr_api.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "bb_api.h"
#include "pal_bb.h"
#include "pal_bb_ble.h"
#include "pal_radio.h"
#include "bb_ble_api_reslist.h"
#include "wsf_assert.h"
#include "wsf_buf.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>
#include "lctr_int_conn.h"

/**************************************************************************************************
  Globals
**************************************************************************************************/
/*! \brief      Advertising set table. */
lctrAdvSet_t *pLctrAdvSetTbl;                   /* TODO: share memory with legacy lctrSlvAdv */

/*! \brief      Common storage for task message data (parameter passing to action routines). */
LctrExtAdvMsg_t *pLctrSlvExtAdvMsg;

/*! \brief      Common storage for task message data (parameter passing to action routines). */
LctrPerAdvMsg_t *pLctrSlvPerAdvMsg;

/*! \brief      Extended advertising operational context. */
lctrSlvExtAdvCtx_t lctrSlvExtAdv;

/*! \brief      Extended advertising data buffer table. */
static uint8_t *lctrExtAdvDataBufTbl[LL_MAX_ADV_SETS];

/*! \brief      Periodic advertising data buffer table. */
static uint8_t *lctrPerAdvDataBufTbl[LL_MAX_ADV_SETS];

/*! \brief      Slave ACAD message. */
lctrAcadSlvMsg_t *pLctrAcadSlvMsg;

/**************************************************************************************************
  Internal functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Reset ACAD params
 */
/*************************************************************************************************/
static void lctrSlvAcadResetHandler(void)
{
  lctrAdvSet_t *pAdvSet;

  for (unsigned int handle = 0; handle < pLctrRtCfg->maxAdvSets; handle++)
  {
    if ((pAdvSet = lctrFindAdvSet(handle)) != NULL)
    {
      memset(pAdvSet->acadParams, 0, sizeof(pAdvSet->acadParams));
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  ACAD dispatch handler
 *
 *  \param  pMsg       Message to be handled.
 */
/*************************************************************************************************/
static void lctrSlvAcadDisp(lctrAcadSlvMsg_t *pMsg)
{
  lctrAdvSet_t *pAdvSet;

  LL_TRACE_INFO2("lctrSlvAcadDisp: handle=%u, event=%u", pMsg->hdr.handle, pMsg->hdr.event);

  if (pMsg->hdr.dispId != LCTR_DISP_BCST)
  {
    if ((pAdvSet = lctrFindAdvSet(pMsg->hdr.handle)) != NULL)
    {
      pLctrAcadSlvMsg = pMsg;

      lctrSlvAcadExecuteSm(pAdvSet, pMsg->hdr.event);
    }
  }

  /* Currently, the only broadcast message is reset. */
  else
  {
    lctrSlvAcadResetHandler();
  }
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
static inline uint32_t lctrCalcPerm(uint32_t v)
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
static inline uint16_t lctrCalcMAM(uint16_t a, uint16_t b)
{
  /* (17 x a + b) mod 2^16 */
  return ((17 * a) + b) & 0xFFFFF;
}

/*************************************************************************************************/
/*!
 *  \brief      Allocate an advertising set.
 *
 *  \param      handle      Advertising handle.
 *
 *  \return     Advertising set or NULL if at capacity.
 */
/*************************************************************************************************/
static lctrAdvSet_t *lctrAllocAdvSet(uint8_t handle)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxAdvSets; i++)
  {
    lctrAdvSet_t *pAdvSet = &pLctrAdvSetTbl[i];

    if (!pAdvSet->enabled)
    {
      memset(pAdvSet, 0, sizeof(lctrAdvSet_t));

      pAdvSet->enabled = TRUE;
      pAdvSet->handle = handle;

      /* Defaults. */
      pAdvSet->advDataFragLen = WSF_MAX(pLctrRtCfg->defExtAdvDataFrag, 1);

      /* Extended adv and scan response data buffer use is mutually exclusive; they share the same storage. */
      pAdvSet->advData.pBuf = lctrExtAdvDataBufTbl[i];
      pAdvSet->advData.ready = TRUE;
      pAdvSet->scanRspData.pBuf = lctrExtAdvDataBufTbl[i];
      pAdvSet->scanRspData.ready = TRUE;
      pAdvSet->perAdvData.pBuf = lctrPerAdvDataBufTbl[i];
      pAdvSet->perAdvData.ready = TRUE;

      /* Setup duration timer. */
      pAdvSet->tmrAdvDur.handlerId = lmgrPersistCb.handlerId;
      lctrMsgHdr_t *pMsg = (lctrMsgHdr_t *)&pAdvSet->tmrAdvDur.msg;
      pMsg->handle = pAdvSet->handle;
      pMsg->dispId = LCTR_DISP_EXT_ADV;
      pMsg->event = LCTR_EXT_ADV_MSG_TMR_DUR_EXP;

      LmgrIncResetRefCount();

      return pAdvSet;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Free an advertising set.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrFreeAdvSet(lctrAdvSet_t *pAdvSet)
{
  WSF_ASSERT(pAdvSet->enabled);
  pAdvSet->enabled = FALSE;
  LmgrDecResetRefCount();
}

/*************************************************************************************************/
/*!
 *  \brief      Find an advertising set data.
 *
 *  \param      handle      Advertising handle.
 *
 *  \return     Advertising set or NULL if not found.
 */
/*************************************************************************************************/
lctrAdvSet_t *lctrFindAdvSet(uint8_t handle)
{
  unsigned int i;
  for (i = 0; i < pLctrRtCfg->maxAdvSets; i++)
  {
    lctrAdvSet_t *pAdvSet = &pLctrAdvSetTbl[i];

    if ((pAdvSet->handle == handle) &&
        (pAdvSet->enabled))
    {
      return pAdvSet;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Update auxiliary channel.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrSelectNextAuxChannel(lctrAdvSet_t *pAdvSet)
{
  do
  {

    pAdvSet->auxChHopInc = lctrComputeHopInc();
    pAdvSet->auxChIdx = LL_MATH_MOD_37(pAdvSet->auxChIdx + pAdvSet->auxChHopInc);
  }
  while (!(lmgrCb.chanClass & ((uint64_t) 1 << pAdvSet->auxChIdx)));
}

/*************************************************************************************************/
/*!
 *  \brief      Update periodic channel.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrSelectNextPerChannel(lctrAdvSet_t *pAdvSet)
{
  do
  {
    pAdvSet->perParam.perChHopInc = lctrComputeHopInc();
    pAdvSet->perParam.perChIdx = LL_MATH_MOD_37(pAdvSet->perParam.perChIdx + pAdvSet->perParam.perChHopInc);
  }
  while (!(lmgrCb.chanClass & ((uint64_t) 1 << pAdvSet->perParam.perChIdx)));
}

/*************************************************************************************************/
/*!
 *  \brief  Compute next channel index, algorithm #2.
 *
 *  \param  pChanParam    Channel parameters.
 *  \param  eventCounter  Event Counter.
 *
 *  \return Next data channel index.
 */
/*************************************************************************************************/
uint8_t lctrPeriodicSelectNextChannel(lmgrChanParam_t *pChanParam, uint16_t eventCounter)
{
  unsigned int prn;

  /* Pseudo random number */

  prn = eventCounter ^ pChanParam->chIdentifier;
  prn = lctrCalcPerm(prn);
  prn = lctrCalcMAM(prn, pChanParam->chIdentifier);
  prn = lctrCalcPerm(prn);
  prn = lctrCalcMAM(prn, pChanParam->chIdentifier);
  prn = lctrCalcPerm(prn);
  prn = lctrCalcMAM(prn, pChanParam->chIdentifier);
  /* uint16_t prn_s = prn; */
  unsigned int prn_e = prn ^ pChanParam->chIdentifier;

  /* unmappedChannel */

  uint16_t unmapChan = LL_MATH_MOD_37(prn_e & 0xFFFF);

  /* remappingIndex */

  if (!((UINT64_C(1) << unmapChan) & pChanParam->chanMask))
  {
    /* remappingIndex = (N * prn_e) / 2^16 */
    uint8_t remapIdx = (pChanParam->numUsedChan * prn_e) >> 16;
    return pChanParam->chanRemapTbl[remapIdx];
  }

  return unmapChan;
}

/*************************************************************************************************/
/*!
 *  \brief      Extended advertising reset handler.
 */
/*************************************************************************************************/
static void lctrSlvExtAdvResetHandler(void)
{
  BbBleAdvSlaveInit();
  BbBleAuxAdvSlaveInit();
  LctrSlvExtAdvDefaults();
}

/*************************************************************************************************/
/*!
 *  \brief      Slave extended advertising message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrSlvExtAdvDisp(LctrExtAdvMsg_t *pMsg)
{
  pLctrSlvExtAdvMsg = pMsg;

  lctrAdvSet_t *pAdvSet;

  if (pMsg->hdr.dispId != LCTR_DISP_BCST)
  {
    if ((pAdvSet = lctrFindAdvSet(pMsg->hdr.handle)) != NULL)
    {
      lctrSlvExtAdvExecuteSm(pAdvSet, pMsg->hdr.event);
    }
  }
  else
  {
    for (unsigned int i = 0; i < pLctrRtCfg->maxAdvSets; i++)
    {
      pAdvSet = &pLctrAdvSetTbl[i];
      if (pAdvSet->enabled)
      {
        lctrSlvExtAdvExecuteSm(pAdvSet, pMsg->hdr.event);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Scan request post-processing.
 */
/*************************************************************************************************/
static void lctrSlvRxExtScanReq(void)
{
  LlScanReqRcvdInd_t *pScanReqRcvd;
  wsfHandlerId_t handlerId;

  while ((pScanReqRcvd = WsfMsgDeq(&lctrSlvExtAdv.rxScanReqQ, &handlerId)) != NULL)
  {
    LmgrSendEvent((LlEvt_t *)&pScanReqRcvd);
    WsfMsgFree(pScanReqRcvd);
  }

  /* Handle legacy scan request. */
  lctrSlvRxScanReq();
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended advertising or scan response data.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pDataBuf    Advertising or scan response data buffer descriptor.
 *  \param      fragOp      Fragment operation.
 *  \param      fragPref    Fragment preference.
 *  \param      fragLen     Fragment data buffer length.
 *  \param      pFragBuf    Fragment data buffer.
 *
 *  \return     Status error code.
 *
 *  Set extended advertising data data.
 */
/*************************************************************************************************/
static uint8_t lctrSetExtAdvDataSm(lctrAdvSet_t *pAdvSet, lctrAdvDataBuf_t *pDataBuf, uint8_t fragOp,
                                   uint8_t fragPref, uint8_t fragLen, const uint8_t *pFragBuf)
{
  switch (fragOp)
  {
    case LL_ADV_DATA_OP_FRAG_INTER:
    case LL_ADV_DATA_OP_FRAG_LAST:
      if (pDataBuf->ready)
      {
        LL_TRACE_WARN2("Operation=%u missing a preceding LL_ADV_DATA_OP_FRAG_FIRST, handle=%u", fragOp, pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      if ((pDataBuf->len + fragLen) > pLctrRtCfg->maxExtAdvDataLen)
      {
        /* Discard buffer. */
        pDataBuf->len = 0;
        pDataBuf->ready = FALSE;
        LL_TRACE_WARN2("Fragments exceeded maximum buffer length -- discarding buffer, maxExtAdvDataLen=%u, handle=%u", pLctrRtCfg->maxExtAdvDataLen, pAdvSet->handle);
        return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
      }
      /* Fallthrough */
    case LL_ADV_DATA_OP_FRAG_FIRST:
      if (pAdvSet->state != LCTR_EXT_ADV_STATE_DISABLED)    /* e.g. advertising enabled */
      {
        LL_TRACE_WARN2("Operation=%u not permitted when advertising enabled, handle=%u", fragOp, pAdvSet->handle);
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
      if (fragLen == 0)
      {
        LL_TRACE_WARN2("Fragments for operation=%u must have data, handle=%u", fragOp, pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      break;
    case LL_ADV_DATA_OP_COMP:
      /*** Complete single fragment buffer (no reassembly required) while advertising is enabled. ***/
      if (pAdvSet->state == LCTR_EXT_ADV_STATE_ENABLED)
      {
        /* BOD's are already running. Data will be updated in the end callback of the BOD's. */
        pDataBuf->alt.ext.len = fragLen;
        memcpy(pDataBuf->alt.ext.buf, pFragBuf, fragLen);
        pDataBuf->alt.ext.did = lctrCalcDID(pFragBuf, fragLen);
        pDataBuf->alt.ext.fragPref = fragPref;
        pDataBuf->alt.ext.modified = TRUE;

        /* Renew BOD's to make the data updated immediately if possible. */
        if (SchIsBodCancellable(&pAdvSet->advBod) &&
            ((pAdvSet->auxBodUsed == FALSE) || SchIsBodCancellable(&pAdvSet->auxAdvBod)))
        {
          /* Remove BOD's; remove completes in abortCback(). */
          if (pAdvSet->auxBodUsed)
          {
            SchRemove(&pAdvSet->auxAdvBod);
          }
          SchRemove(&pAdvSet->advBod);
        }

        return LL_SUCCESS;
      }
      break;
    case LL_ADV_DATA_OP_UNCHANGED:
      if (pAdvSet->state == LCTR_EXT_ADV_STATE_DISABLED)
      {
        LL_TRACE_WARN1("Operation=LL_ADV_DATA_OP_UNCHANGED not permitted when advertising disabled, handle=%u", pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      if (fragLen > 0)
      {
        LL_TRACE_WARN1("Operation=LL_ADV_DATA_OP_UNCHANGED not permitted with data fragment, handle=%u", pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      if (pDataBuf->len == 0)
      {
        LL_TRACE_WARN1("Operation=LL_ADV_DATA_OP_UNCHANGED not permitted when no data buffer present, handle=%u", pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      break;
    default:
      LL_TRACE_WARN2("Invalid operation=%u, handle=%u", fragOp, pAdvSet->handle);
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  /*** Reassemble data buffer ***/

  switch (fragOp)
  {
    case LL_ADV_DATA_OP_FRAG_INTER:
      /* Append buffer. */
      memcpy(pDataBuf->pBuf + pDataBuf->len, pFragBuf, fragLen);
      pDataBuf->len += fragLen;
      pDataBuf->ready = FALSE;
      break;
    case LL_ADV_DATA_OP_FRAG_FIRST:
      /* New buffer (discard old buffer). */
      memcpy(pDataBuf->pBuf, pFragBuf, fragLen);
      pDataBuf->len = fragLen;
      pDataBuf->ready = FALSE;
      break;
    case LL_ADV_DATA_OP_FRAG_LAST:
      /* Append buffer. */
      memcpy(pDataBuf->pBuf + pDataBuf->len, pFragBuf, fragLen);
      pDataBuf->len += fragLen;
      pAdvSet->param.advDID = lctrCalcDID(pFragBuf, fragLen);
      pDataBuf->fragPref = fragPref;
      pDataBuf->ready = TRUE;
      break;
    case LL_ADV_DATA_OP_COMP:
      /* New buffer (discard old buffer). */
      memcpy(pDataBuf->pBuf, pFragBuf, fragLen);
      pDataBuf->len = fragLen;
      pAdvSet->param.advDID = lctrCalcDID(pFragBuf, fragLen);
      pDataBuf->fragPref = fragPref;
      pDataBuf->ready = TRUE;
      break;
    case LL_ADV_DATA_OP_UNCHANGED:
      /* Same buffer. */
      pAdvSet->param.advDID = lctrCalcDID(pFragBuf, fragLen);
      pDataBuf->fragPref = fragPref;
      pDataBuf->ready = TRUE;
      break;
    default:
      /* coverity[dead_error_begin] */
      break;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set periodic advertising data.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pDataBuf    Advertising or scan response data buffer descriptor.
 *  \param      fragOp      Fragment operation.
 *  \param      fragLen     Fragment data buffer length.
 *  \param      pFragBuf    Fragment data buffer.
 *
 *  \return     Status error code.
 *
 *  Set extended advertising data data.
 */
/*************************************************************************************************/
static uint8_t lctrSetPerAdvDataSm(lctrAdvSet_t *pAdvSet, lctrAdvDataBuf_t *pDataBuf, uint8_t fragOp,
                                   uint8_t fragLen, const uint8_t *pFragBuf)
{
  switch (fragOp)
  {
    case LL_ADV_DATA_OP_FRAG_INTER:
    case LL_ADV_DATA_OP_FRAG_LAST:
      if (pDataBuf->ready)
      {
        LL_TRACE_WARN2("Operation=%u missing a preceding LL_ADV_DATA_OP_FRAG_FIRST, handle=%u", fragOp, pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      if ((pDataBuf->len + fragLen) > pLctrRtCfg->maxExtAdvDataLen)
      {
        /* Discard buffer. */
        pDataBuf->len = 0;
        pDataBuf->ready = FALSE;
        LL_TRACE_WARN2("Fragments exceeded maximum buffer length -- discarding buffer, maxExtAdvDataLen=%u, handle=%u", pLctrRtCfg->maxExtAdvDataLen, pAdvSet->handle);
        return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
      }
      /* Fallthrough */
    case LL_ADV_DATA_OP_FRAG_FIRST:
      if (pAdvSet->perParam.perState != LCTR_PER_ADV_STATE_DISABLED)    /* e.g. advertising enabled */
      {
        LL_TRACE_WARN2("Operation=%u not permitted when periodic advertising enabled, handle=%u", fragOp, pAdvSet->handle);
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
      if (fragLen == 0)
      {
        LL_TRACE_WARN2("Fragments for operation=%u must have data, handle=%u", fragOp, pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      break;
    case LL_ADV_DATA_OP_COMP:
      /*** Complete single fragment buffer (no reassembly required) while advertising is enabled. ***/
      if (pAdvSet->perParam.perState == LCTR_PER_ADV_STATE_ENABLED)
      {
        pDataBuf->alt.ext.len = fragLen;
        memcpy(pDataBuf->alt.ext.buf, pFragBuf, fragLen);
        pDataBuf->alt.ext.did = lctrCalcDID(pFragBuf, fragLen);
        pDataBuf->alt.ext.modified = TRUE;

        return LL_SUCCESS;
      }
      break;
    case LL_ADV_DATA_OP_UNCHANGED:
      if (pAdvSet->perParam.perState == LCTR_PER_ADV_STATE_DISABLED)
      {
        LL_TRACE_WARN1("Operation=LL_ADV_DATA_OP_UNCHANGED not permitted when periodic advertising disabled, handle=%u", pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      if (fragLen > 0)
      {
        LL_TRACE_WARN1("Operation=LL_ADV_DATA_OP_UNCHANGED not permitted with data fragment, handle=%u", pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      if (pDataBuf->len == 0)
      {
        LL_TRACE_WARN1("Operation=LL_ADV_DATA_OP_UNCHANGED not permitted when no data buffer present, handle=%u", pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      break;
    default:
      LL_TRACE_WARN2("Invalid operation=%u, handle=%u", fragOp, pAdvSet->handle);
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  /*** Reassemble data buffer ***/
  /* coverity[dead_error_condition] */
  switch (fragOp)
  {
    case LL_ADV_DATA_OP_FRAG_INTER:
      /* Append buffer. */
      memcpy(pDataBuf->pBuf + pDataBuf->len, pFragBuf, fragLen);
      pDataBuf->len += fragLen;
      pDataBuf->ready = FALSE;
      break;
    case LL_ADV_DATA_OP_FRAG_FIRST:
      /* New buffer (discard old buffer). */
      memcpy(pDataBuf->pBuf, pFragBuf, fragLen);
      pDataBuf->len = fragLen;
      pDataBuf->ready = FALSE;
      break;
    case LL_ADV_DATA_OP_FRAG_LAST:
      /* Append buffer. */
      memcpy(pDataBuf->pBuf + pDataBuf->len, pFragBuf, fragLen);
      pDataBuf->len += fragLen;
      pAdvSet->perParam.advDID = lctrCalcDID(pFragBuf, fragLen);
      pDataBuf->ready = TRUE;
      break;
    case LL_ADV_DATA_OP_COMP:
      /* New buffer (discard old buffer). */
      memcpy(pDataBuf->pBuf, pFragBuf, fragLen);
      pDataBuf->len = fragLen;
      pAdvSet->perParam.advDID = lctrCalcDID(pFragBuf, fragLen);
      pDataBuf->ready = TRUE;
      break;
    case LL_ADV_DATA_OP_UNCHANGED:
      /* Same buffer. */
      pAdvSet->perParam.advDID = lctrCalcDID(pFragBuf, fragLen);
      pDataBuf->ready = TRUE;
      break;
    default:
      /* coverity[dead_error_begin] */
      break;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set legacy advertising or scan response data.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pDataBuf    Advertising data buffer descriptor.
 *  \param      fragOp      Fragment operation.
 *  \param      len         Fragment data buffer length.
 *  \param      pData       Fragment data buffer.
 *
 *  \return     Status error code.
 *
 *  Set extended advertising data data.
 */
/*************************************************************************************************/
static uint8_t lctrSetLegacyAdvDataSm(lctrAdvSet_t *pAdvSet, lctrAdvDataBuf_t *pDataBuf,
                                      uint8_t fragOp, uint8_t len, const uint8_t *pData)
{
  if (fragOp != LL_ADV_DATA_OP_COMP)
  {
    LL_TRACE_WARN1("Legacy advertising does not accept data fragments, handle=%u", pAdvSet->handle);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  if (len > LL_ADVBU_MAX_LEN)
  {
    LL_TRACE_WARN2("Exceeded maximum legacy data buffer size=%u, handle=%u", LL_ADVBU_MAX_LEN, pAdvSet->handle);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  /* Legacy buffers always stored in cache (exclusive buffer). */
  pDataBuf->alt.legacy.len = len;
  memcpy(pDataBuf->alt.legacy.buf, pData, len);

  if (pAdvSet->state == LCTR_EXT_ADV_STATE_ENABLED)
  {
    /* Always complete. */
    pDataBuf->ready = TRUE;
  }

  pDataBuf->alt.legacy.modified = TRUE;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Build extended advertising (primary channel) operation.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      maxStartMs  Maximum allowable start time in milliseconds, or 0 for no maximum.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t lctrSlvExtAdvBuildOp(lctrAdvSet_t *pAdvSet, uint32_t maxStartMs)
{
  BbOpDesc_t * const pOp = &pAdvSet->advBod;
  BbBleData_t * const pBle = &pAdvSet->bleData;
  BbBleSlvAdvEvent_t * const pAdv = &pBle->op.slvAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_MOVEABLE;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrSlvExtAdvEndOp;
  pOp->abortCback = lctrSlvExtAdvAbortOp;
  pOp->pCtx = pAdvSet;

  /*** BLE General Setup ***/

  pBle->chan.opType = BB_BLE_OP_SLV_ADV_EVENT;

  /* pBle->chan.chanIdx = 0; */     /* overridden by BB */
  if (pAdvSet->param.advTxPwr == HCI_TX_PWR_NO_PREFERENCE)
  {
    pBle->chan.txPower  = lmgrCb.advTxPwr;
  }
  else
  {
    pBle->chan.txPower  = pAdvSet->param.advTxPwr;
  }
  pBle->chan.accAddr    = LL_ADV_ACCESS_ADDR;
  pBle->chan.crcInit    = LL_ADV_CRC_INIT;
  pBle->chan.txPhy      = pAdvSet->param.priAdvPhy;
  pBle->chan.rxPhy      = pAdvSet->param.priAdvPhy;

  /* Set PHY options to default behavior for ADV_EXT_IND. */
  pBle->chan.initTxPhyOptions = lmgrSlvAdvCb.defTxPhyOpts;

#if (LL_ENABLE_TESTER == TRUE)
  pBle->chan.accAddrRx = llTesterCb.advAccessAddrRx ^ pBle->chan.accAddr;
  pBle->chan.accAddrTx = llTesterCb.advAccessAddrTx ^ pBle->chan.accAddr;
  pBle->chan.crcInitRx = llTesterCb.advCrcInitRx ^ pBle->chan.crcInit;
  pBle->chan.crcInitTx = llTesterCb.advCrcInitTx ^ pBle->chan.crcInit;
#endif

  pBle->pduFilt.wlPduTypeFilt = 0;

  pBle->pduFilt.pduTypeFilt = (1 << LL_PDU_SCAN_REQ) | (1 << LL_PDU_CONNECT_IND);

  /* Activate white list only for undirected advertisement. */
  if ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_DIRECT_ADV_BIT) == 0)
  {
    if (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_SCAN_ADV_BIT)
    {
      if (pAdvSet->param.advFiltPolicy & LL_ADV_FILTER_SCAN_WL_BIT)
      {
        pBle->pduFilt.wlPduTypeFilt |= (1 << LL_PDU_SCAN_REQ);
      }
    }

    if (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_CONN_ADV_BIT)
    {
      if (pAdvSet->param.advFiltPolicy & LL_ADV_FILTER_CONN_WL_BIT)
      {
        pBle->pduFilt.wlPduTypeFilt |= (1 << LL_PDU_CONNECT_IND);
      }
    }
  }
  else
  {
    /* Always match peer address in PDU for directed advertising. */
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_MATCH_ENA);
  }

  /*** BLE Advertising Setup: Tx advertising packet ***/

  pAdv->advChMap = pAdvSet->param.priAdvChanMap;

  pAdv->txAdvSetupCback = lctrSlvTxSetupExtAdvHandler;

  if (lmgrGetOpFlag(LL_OP_MODE_FLAG_ENA_ADV_CHAN_RAND))
  {
    pAdv->firstAdvChIdx = LlMathRandNum() & LL_NUM_CHAN_ADV;
  }
  else
  {
    pAdv->firstAdvChIdx = 0;
  }

  if (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT)
  {
    const uint8_t LEGACY_HIGH_DUTY = (LL_ADV_EVT_PROP_LEGACY_ADV_BIT | LL_ADV_EVT_PROP_HIGH_DUTY_ADV_BIT |
                                      LL_ADV_EVT_PROP_DIRECT_ADV_BIT | LL_ADV_EVT_PROP_CONN_ADV_BIT);
    /* Advertising interval is determined by LL for high duty cycle directed adv. */
    if ((pAdvSet->param.advEventProp & LEGACY_HIGH_DUTY) == LEGACY_HIGH_DUTY )
    {
      pAdv->firstAdvChIdx = 0;  /* High duty cycle always start from channel 37. */
      pAdvSet->param.priAdvInterMinUsec = 0;
      pAdvSet->param.priAdvInterMaxUsec = BB_BLE_TO_US(LL_DIR_ADV_INTER_TICKS);
      pAdvSet->param.priAdvTermCntDownUsec = pLctrSlvExtAdvMsg->enable.durMs * 1000;
    }

    pAdv->txAdvLen = lctrPackLegacyAdvPdu(pAdvSet, pAdvSet->advHdrBuf);
  }
  else
  {
    pAdv->txAdvLen = lctrPackAdvExtIndPdu(pAdvSet, pAdvSet->advHdrBuf, FALSE);
  }
  pAdv->pTxAdvBuf = pAdvSet->advHdrBuf;

#if (LL_ENABLE_TESTER)
  if (llTesterCb.txAdvPduLen)
  {
    pAdv->pTxAdvBuf = llTesterCb.txAdvPdu;
    pAdv->txAdvLen = llTesterCb.txAdvPduLen;
  }
#endif

  /* Primary channel scan */
  if ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) &&
      /* Either connectable or scannable. */
      (pAdvSet->param.advEventProp & (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT)))
  {
    /*** BLE Advertising Setup: Rx scan request packet ***/

    lctrMsgHdr_t *pMsg;

    if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t) + BB_REQ_PDU_MAX_LEN)) != NULL)
    {
      pAdv->pRxReqBuf = (uint8_t *)(pMsg + 1);      /* hide header */
      pAdv->rxReqCback = lctrSlvRxLegacyReqHandler;
      pAdv->rxReqPostCback = lctrSlvRxLegacyReqPostProcessHandler;
    }
    else
    {
      LL_TRACE_ERR0("Could not allocate request buffer");
      return LL_ERROR_CODE_UNSPECIFIED_ERROR;
    }

    /*** BLE Advertising Setup: Tx scan response packet ***/

    if (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_SCAN_ADV_BIT)
    {
      pAdv->txRspLen = lctrPackLegacyScanRspPdu(pAdvSet, pAdvSet->scanRspHdrBuf);
      pAdv->pTxRspBuf = pAdvSet->scanRspHdrBuf;
    }

#if (LL_ENABLE_TESTER == TRUE)
    if (llTesterCb.txScanRspPduLen)
    {
      pAdv->txRspLen = llTesterCb.txScanRspPduLen;
      pAdv->pTxRspBuf = llTesterCb.txScanRspPdu;
    }
#endif
  }

  /*** Commit operation ***/

  /* Setup auxiliary channel before primary channel operation calls lctrSlvTxSetupExtAdvHandler(). */
  lctrSelectNextAuxChannel(pAdvSet);

  SchBleCalcAdvOpDuration(pOp, 0);

  if (pLctrSlvExtAdvMsg->enable.durMs)
  {
    pOp->dueUsec = PalBbGetCurrentTime() + BbGetSchSetupDelayUs();

    uint32_t maxTime = maxStartMs * 1000;
    if (!SchInsertEarlyAsPossible(pOp, 0, maxTime))
    {
      LL_TRACE_WARN1("Could not enable ExtAdv operation within given durMs=%u", maxStartMs);
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }
  }
  else
  {
    SchInsertNextAvailable(pOp);
  }

  /* Advertising offloading to auxiliary channel. */
  if (pAdvSet->pExtAdvAuxPtr)
  {
    switch (pAdvSet->param.advEventProp & (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT))
    {
      case LL_ADV_EVT_PROP_NON_CONN_NON_SCAN:
        /* Setup auxiliary non-connectable, non-scannable operation. */
        lctrSlvAuxNonConnNonScanBuildOp(pAdvSet);
        pAdvSet->auxBodUsed = TRUE;
        break;
      case LL_ADV_EVT_PROP_SCAN_ADV_BIT:
        /* Setup auxiliary scan operation. */
        lctrSlvAuxScanBuildOp(pAdvSet);
        pAdvSet->auxBodUsed = TRUE;
        break;
      case LL_ADV_EVT_PROP_CONN_ADV_BIT:
        if (pLctrRtCfg->maxConn > 0)
        {
          /* Setup auxiliary connect operation. */
          lctrSlvAuxConnBuildOp(pAdvSet);
          pAdvSet->auxBodUsed = TRUE;
        }
        else
        {
          LL_TRACE_ERR0("Extended connectable advertising not supported");
          return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
        }
        break;
      case LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT:
      default:
        LL_TRACE_ERR0("Connectable and scannable extended advertising not valid");
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }

    /* Update AuxPtr now that AUX packet is ready. */
    lctrSlvTxSetupExtAdvHandler(pOp, pOp->dueUsec);
  }

  LL_TRACE_INFO1("    >>> ExtAdv started, handle=%u <<<", pAdvSet->handle);
  LL_TRACE_INFO1("                        priAdvInterMinUsec=%u", pAdvSet->param.priAdvInterMinUsec);
  LL_TRACE_INFO1("                        priAdvInterMaxUsec=%u", pAdvSet->param.priAdvInterMaxUsec);
  LL_TRACE_INFO1("                        dueUsec=%u", pOp->dueUsec);
  LL_TRACE_INFO1("                        pAdvBod=0x%08x", pOp);
  LL_TRACE_INFO1("                        pAuxAdvBod=0x%08x", &pAdvSet->auxAdvBod);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Reschedule auxiliary operation.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pOp         Auxiliary BOD.
 */
/*************************************************************************************************/
void lctrSlvAuxRescheduleOp(lctrAdvSet_t *pAdvSet, BbOpDesc_t * const pOp)
{
  uint32_t auxOffsUsec = pAdvSet->advBod.minDurUsec +
                         WSF_MAX(BbGetSchSetupDelayUs(), LL_BLE_MAFS_US) +
                         WSF_MAX(pAdvSet->auxDelayUsec, pLctrRtCfg->auxDelayUsec);

  auxOffsUsec = WSF_MIN(auxOffsUsec, LL_AUX_PTR_MAX_USEC);

  /* Round up auxOffsetUsec if necessary. */
  auxOffsUsec = SchBleGetAlignedAuxOffsUsec(auxOffsUsec);

  /* Update BOD duration because extended header length might have been updated. */
  SchBleCalcAdvOpDuration(pOp, (pAdvSet->advData.fragPref == LL_ADV_DATA_FRAG_ALLOW) ? pAdvSet->advDataFragLen : 0);

  if (pAdvSet->auxSkipInterUsec == 0)
  {
    pOp->dueUsec = pAdvSet->advBod.dueUsec;

    do
    {
      /* No delay after primary channel operation. */
      if (SchInsertEarlyAsPossible(pOp, auxOffsUsec + pLctrRtCfg->auxPtrOffsetUsec, pAdvSet->param.priAdvInterMaxUsec))
      {
        break;
      }

      LL_TRACE_WARN1("!!! AuxAdv schedule conflict at dueUsec=%u", pOp->dueUsec);
      LL_TRACE_WARN1("!!!                             handle=%u", pAdvSet->handle);
      /* Try next advertising interval. */
      pOp->dueUsec += pAdvSet->param.priAdvInterMaxUsec;

    } while (TRUE);
  }
  else
  {
    /* Using EXT_ADV_IND due time as a reference ensures AuxOffset value does not overflow. */
    pOp->dueUsec = pAdvSet->advBod.dueUsec;

    do
    {
      /* Link multiple primary channel operations. */
      if (SchInsertLateAsPossible(pOp, auxOffsUsec + pLctrRtCfg->auxPtrOffsetUsec, pAdvSet->auxSkipInterUsec))
      {
        break;
      }

      /* Try next skip interval. */
      pOp->dueUsec += pAdvSet->auxSkipInterUsec;

      /* TODO restart advertising at next skip interval or use large skip? */

      LL_TRACE_WARN1("!!! AuxAdv schedule conflict at dueUsec=%u", pOp->dueUsec);
      LL_TRACE_WARN1("!!!                             handle=%u", pAdvSet->handle);

    } while (TRUE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Commit auxiliary operation.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pOp         Auxiliary BOD.
 */
/*************************************************************************************************/
static void lctrSlvAuxCommitOp(lctrAdvSet_t *pAdvSet, BbOpDesc_t * const pOp)
{
  SchBleCalcAdvOpDuration(pOp, (pAdvSet->advData.fragPref == LL_ADV_DATA_FRAG_ALLOW) ? pAdvSet->advDataFragLen : 0);

  if (pAdvSet->param.secAdvMaxSkip)
  {
    uint32_t advEvtDurUsec = pAdvSet->advBod.minDurUsec;

    uint32_t skipTimeUsec = pAdvSet->param.secAdvMaxSkip *                        /* number of skip events */
                            (pAdvSet->param.priAdvInterMinUsec +                  /* minimum interval */
                            ((LL_MAX_ADV_DLY_MS >> 1) * 1000)) +                  /* average advDelay */
                            (LL_MAX_ADV_DLY_MS * 1000);                           /* ensure no overlap due to advDelay */
    skipTimeUsec = WSF_MIN(skipTimeUsec, (LCTR_AUX_PTR_MAX_OFFSET * 300));        /* limit maximum */

    pAdvSet->auxSkipInterUsec = WSF_MAX(skipTimeUsec, advEvtDurUsec);             /* ensure minimum */

  }

  lctrSlvAuxRescheduleOp(pAdvSet, pOp);
}

/*************************************************************************************************/
/*!
 *  \brief  Build auxiliary non-connectable and non-scannable operation.
 *
 *  \param  pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrSlvAuxNonConnNonScanBuildOp(lctrAdvSet_t *pAdvSet)
{
  /* Must be non-connectable, non-scannable. */
  WSF_ASSERT((pAdvSet->param.advEventProp & (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT)) == 0);

  BbOpDesc_t * const pOp = &pAdvSet->auxAdvBod;
  BbBleData_t * const pBle = &pAdvSet->auxBleData;
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBle->op.slvAuxAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_FIXED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrSlvAuxAdvEndOp;
  pOp->abortCback = lctrSlvAuxAdvEndOp;
  pOp->pCtx = pAdvSet;
  pOp->pDataLen = &(pAdvSet->advData.len);

  /*** BLE General Setup ***/

  pBle->chan.opType = BB_BLE_OP_SLV_AUX_ADV_EVENT;

  pBle->chan.chanIdx    = pAdvSet->auxChIdx;
  if (pAdvSet->param.advTxPwr == HCI_TX_PWR_NO_PREFERENCE)
  {
    pBle->chan.txPower  = lmgrCb.advTxPwr;
  }
  else
  {
    pBle->chan.txPower  = pAdvSet->param.advTxPwr;
  }
  pBle->chan.accAddr    = LL_ADV_ACCESS_ADDR;
  pBle->chan.crcInit    = LL_ADV_CRC_INIT;
  pBle->chan.txPhy      = pAdvSet->param.secAdvPhy;
  pBle->chan.rxPhy      = pAdvSet->param.secAdvPhy;

  /* Set PHY options to default behavior for AUX_ADV_IND. */
  pBle->chan.initTxPhyOptions = lmgrSlvAdvCb.defTxPhyOpts;

#if (LL_ENABLE_TESTER == TRUE)
  pBle->chan.accAddrRx = llTesterCb.auxAccessAddrRx ^ pBle->chan.accAddr;
  pBle->chan.accAddrTx = llTesterCb.auxAccessAddrTx ^ pBle->chan.accAddr;
  pBle->chan.crcInitRx = llTesterCb.auxCrcInitRx ^ pBle->chan.crcInit;
  pBle->chan.crcInitTx = llTesterCb.auxCrcInitTx ^ pBle->chan.crcInit;
#endif

  /*** BLE Advertising Setup: Tx advertising packet ***/

  pAuxAdv->txAuxSetupCback = lctrSlvTxSetupAuxAdvDataHandler;

  pAdvSet->advData.txOffs = 0;
  pAuxAdv->txAuxAdvPdu[0].pBuf = pAdvSet->auxAdvHdrBuf;
  /* pAuxAdv->txAuxAdvPdu[0].len = 0; */        /* Cleared in memset(). */
  /* Auxiliary advertising buffer setup dynamically in lctrSlvTxSetupAuxAdvDataHandler(). */
  /* pAuxAdv->txAuxAdvPdu[1].pBuf = NULL; */
  /* pAuxAdv->txAuxAdvPdu[1].len = 0; */

  /*** BLE Advertising Setup: Rx request packet ***/

  pAuxAdv->pRxAuxReqBuf = NULL;
  pAuxAdv->rxAuxReqCback = NULL;

  /*** BLE Advertising Setup: Tx scan response packet ***/

  /* pAuxAdv->txAuxRspPdu[0].len = 0; */        /* Cleared in memset(). */
  /* pAuxAdv->txAuxRspPdu[0].pBuf = NULL; */
  /* pAuxAdv->txAuxRspPdu[1].len = 0; */
  /* pAuxAdv->txAuxRspPdu[1].pBuf = NULL; */

  /*** BLE Advertising Setup: Tx chain indication packet ***/

  pAuxAdv->txAuxChainPdu[0].pBuf = pAdvSet->auxAdvHdrBuf;
  /* Chain buffer setup dynamically in lctrSlvTxSetupAuxAdvDataHandler(). */
  /* pAuxAdv->txAuxChainPdu[0].len = 0; */      /* Cleared in memset(). */
  /* pAuxAdv->txAuxChainPdu[1].pBuf = NULL; */
  /* pAuxAdv->txAuxChainPdu[1].len = 0; */

  /*** Commit operation ***/

  lctrSlvAuxCommitOp(pAdvSet, pOp);
}

/*************************************************************************************************/
/*!
 *  \brief  Build auxiliary scannable operation.
 *
 *  \param  pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrSlvAuxScanBuildOp(lctrAdvSet_t *pAdvSet)
{
  BbOpDesc_t * const pOp = &pAdvSet->auxAdvBod;
  BbBleData_t * const pBle = &pAdvSet->auxBleData;
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBle->op.slvAuxAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_MOVEABLE_PREFERRED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrSlvAuxAdvEndOp;
  pOp->abortCback = lctrSlvAuxAdvEndOp;
  pOp->pCtx = pAdvSet;
  pOp->pDataLen = &(pAdvSet->scanRspData.len);

  /*** BLE General Setup ***/

  pBle->chan.opType = BB_BLE_OP_SLV_AUX_ADV_EVENT;

  pBle->chan.chanIdx    = pAdvSet->auxChIdx;
  if (pAdvSet->param.advTxPwr == HCI_TX_PWR_NO_PREFERENCE)
  {
    pBle->chan.txPower  = lmgrCb.advTxPwr;
  }
  else
  {
    pBle->chan.txPower  = pAdvSet->param.advTxPwr;
  }
  pBle->chan.accAddr    = LL_ADV_ACCESS_ADDR;
  pBle->chan.crcInit    = LL_ADV_CRC_INIT;
  pBle->chan.txPhy      = pAdvSet->param.secAdvPhy;
  pBle->chan.rxPhy      = pAdvSet->param.secAdvPhy;

   /* Set PHY options to default behavior for AUX_ADV_IND. */
  pBle->chan.initTxPhyOptions = lmgrSlvAdvCb.defTxPhyOpts;

#if (LL_ENABLE_TESTER == TRUE)
  pBle->chan.accAddrRx = llTesterCb.auxAccessAddrRx ^ pBle->chan.accAddr;
  pBle->chan.accAddrTx = llTesterCb.auxAccessAddrTx ^ pBle->chan.accAddr;
  pBle->chan.crcInitRx = llTesterCb.auxCrcInitRx ^ pBle->chan.crcInit;
  pBle->chan.crcInitTx = llTesterCb.auxCrcInitTx ^ pBle->chan.crcInit;
#endif

  pBle->pduFilt.wlPduTypeFilt = 0;
  pBle->pduFilt.pduTypeFilt = (1 << LL_PDU_AUX_SCAN_REQ);

  /* Activate white list only for undirected advertisement. */
  if ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_DIRECT_ADV_BIT) == 0)
  {
    if (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_SCAN_ADV_BIT)
    {
      if (pAdvSet->param.advFiltPolicy & LL_ADV_FILTER_SCAN_WL_BIT)
      {
        pBle->pduFilt.wlPduTypeFilt |= (1 << LL_PDU_AUX_SCAN_REQ);
      }
    }
  }

  /*** BLE Advertising Setup: Tx advertising packet ***/

  pAdvSet->advData.txOffs = 0;
  pAuxAdv->txAuxAdvPdu[0].pBuf = pAdvSet->auxAdvHdrBuf;
  pAuxAdv->txAuxAdvPdu[0].len = lctrPackAuxAdvIndPdu(pAdvSet, pAdvSet->auxAdvHdrBuf, NULL, FALSE);
  /* pAuxAdv->txAuxAdvPdu[1].pBuf = NULL; */    /* Cleared in memset(). */
  /* pAuxAdv->txAuxAdvPdu[1].len = 0; */

  /*** BLE Advertising Setup: Rx request packet ***/

  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t) + LL_ADV_HDR_LEN + LL_SCAN_REQ_PDU_LEN)) != NULL)
  {
    pAuxAdv->pRxAuxReqBuf = (uint8_t *)(pMsg + 1);      /* hide header */
    pAuxAdv->rxAuxReqCback = lctrSlvRxAuxScanReqHandler;
    pAuxAdv->rxAuxReqPostCback = lctrSlvRxAuxScanReqPostProcessHandler;
  }
  else
  {
    LL_TRACE_ERR0("Could not allocate request buffer");
    pAdvSet->termReason = LL_ERROR_CODE_UNSPECIFIED_ERROR;
    lctrSendAdvSetMsg(pAdvSet, LCTR_EXT_ADV_MSG_TERMINATE);
  }

  /*** BLE Advertising Setup: Tx scan response packet ***/

  pAdvSet->scanRspData.txOffs = 0;
  pAuxAdv->txAuxRspPdu[0].pBuf = pAdvSet->auxRspHdrBuf;
  /* Scan response buffer setup dynamically in lctrSlvTxSetupAuxScanRspDataHandler(). */
  /* pAuxAdv->txAuxRspPdu[0].len = 0; */        /* Cleared in memset(). */
  /* pAuxAdv->txAuxRspPdu[1].pBuf = NULL; */
  /* pAuxAdv->txAuxRspPdu[1].len = 0; */

  /*** BLE Advertising Setup: Tx chain indication packet ***/

  pAuxAdv->txAuxSetupCback = lctrSlvTxSetupAuxScanRspDataHandler;

  pAuxAdv->txAuxChainPdu[0].pBuf = pAdvSet->auxRspHdrBuf;
  /* Chain buffer setup dynamically in lctrSlvTxSetupAuxScanRspDataHandler(). */
  /* pAuxAdv->txAuxChainPdu[0].len = 0; */      /* Cleared in memset(). */
  /* pAuxAdv->txAuxChainPdu[1].pBuf = NULL; */
  /* pAuxAdv->txAuxChainPdu[1].len = 0; */

  /*** Commit operation ***/

  pAdvSet->auxBleData.chan.chanIdx = pAdvSet->auxChIdx;
  lctrSlvAuxCommitOp(pAdvSet, pOp);
}

/*************************************************************************************************/
/*!
 *  \brief  Build auxiliary connectable operation.
 *
 *  \param  pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrSlvAuxConnBuildOp(lctrAdvSet_t *pAdvSet)
{
  BbOpDesc_t * const pOp = &pAdvSet->auxAdvBod;
  BbBleData_t * const pBle = &pAdvSet->auxBleData;
  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pBle->op.slvAuxAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_MOVEABLE_PREFERRED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrSlvAuxAdvEndOp;
  pOp->abortCback = lctrSlvAuxAdvEndOp;
  pOp->pCtx = pAdvSet;
  pOp->pDataLen = &(pAdvSet->advData.len);

  /*** BLE General Setup ***/

  pBle->chan.opType = BB_BLE_OP_SLV_AUX_ADV_EVENT;

  pBle->chan.chanIdx    = pAdvSet->auxChIdx;
  if (pAdvSet->param.advTxPwr == HCI_TX_PWR_NO_PREFERENCE)
  {
    pBle->chan.txPower  = lmgrCb.advTxPwr;
  }
  else
  {
    pBle->chan.txPower  = pAdvSet->param.advTxPwr;
  }
  pBle->chan.accAddr    = LL_ADV_ACCESS_ADDR;
  pBle->chan.crcInit    = LL_ADV_CRC_INIT;
  pBle->chan.txPhy      = pAdvSet->param.secAdvPhy;
  pBle->chan.rxPhy      = pAdvSet->param.secAdvPhy;

  /* Set PHY options to default behavior for AUX_ADV_IND. */
  pBle->chan.initTxPhyOptions = lmgrSlvAdvCb.defTxPhyOpts;

#if (LL_ENABLE_TESTER == TRUE)
  pBle->chan.accAddrRx = llTesterCb.auxAccessAddrRx ^ pBle->chan.accAddr;
  pBle->chan.accAddrTx = llTesterCb.auxAccessAddrTx ^ pBle->chan.accAddr;
  pBle->chan.crcInitRx = llTesterCb.auxCrcInitRx ^ pBle->chan.crcInit;
  pBle->chan.crcInitTx = llTesterCb.auxCrcInitTx ^ pBle->chan.crcInit;
#endif

  pBle->pduFilt.wlPduTypeFilt = 0;
  pBle->pduFilt.pduTypeFilt = (1 << LL_PDU_AUX_CONNECT_REQ);

  /* Activate white list only for undirected advertisement. */
  if ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_DIRECT_ADV_BIT) == 0)
  {
    if (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_CONN_ADV_BIT)
    {
      if (pAdvSet->param.advFiltPolicy & LL_ADV_FILTER_CONN_WL_BIT)
      {
        pBle->pduFilt.wlPduTypeFilt |= (1 << LL_PDU_AUX_CONNECT_REQ);
      }
    }
  }

  /*** BLE Advertising Setup: Tx advertising packet ***/

  pAdvSet->advData.txOffs = 0;
  pAuxAdv->txAuxAdvPdu[0].pBuf = pAdvSet->auxAdvHdrBuf;
  pAuxAdv->txAuxAdvPdu[0].len = lctrPackAuxAdvIndPdu(pAdvSet, pAdvSet->auxAdvHdrBuf, &pAdvSet->advData, FALSE);
  /* Only complete AdvData allowed AUX_ADV_IND (CHAIN_IND not allowed). */
  pAuxAdv->txAuxAdvPdu[1].pBuf = pAdvSet->advData.pBuf;
  pAuxAdv->txAuxAdvPdu[1].len = pAdvSet->advData.len;

  /*** BLE Advertising Setup: Rx request packet ***/

  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t) + LL_ADV_HDR_LEN + LL_CONN_IND_PDU_LEN)) != NULL)
  {
    pAuxAdv->pRxAuxReqBuf = (uint8_t *)(pMsg + 1);      /* hide header */
    pAuxAdv->rxAuxReqCback = lctrSlvRxAuxConnReqHandler;
  }
  else
  {
    LL_TRACE_ERR0("Could not allocate request buffer");
    pAdvSet->termReason = LL_ERROR_CODE_UNSPECIFIED_ERROR;
    lctrSendAdvSetMsg(pAdvSet, LCTR_EXT_ADV_MSG_TERMINATE);
  }

  /*** BLE Advertising Setup: Tx scan response packet ***/

  pAuxAdv->txAuxRspPdu[0].pBuf = pAdvSet->auxRspHdrBuf;
  pAuxAdv->txAuxRspPdu[0].len = lctrPackAuxConnRspPdu(pAdvSet, pAdvSet->auxRspHdrBuf, FALSE);
  /* pAuxAdv->txAuxRspPdu[1].pBuf = NULL; */    /* Cleared in memset(). */
  /* pAuxAdv->txAuxRspPdu[1].len = 0; */

  /*** BLE Advertising Setup: Tx chain indication packet ***/

  /* pAuxAdv->txAuxSetupCback = NULL; */        /* Cleared in memset(). */

  /* pAuxAdv->txAuxChainPdu[0].pBuf = NULL; */
  /* pAuxAdv->txAuxChainPdu[0].len = 0; */
  /* pAuxAdv->txAuxChainPdu[1].pBuf = NULL; */
  /* pAuxAdv->txAuxChainPdu[1].len = 0; */

  /*** Commit operation ***/

  lctrSlvAuxCommitOp(pAdvSet, pOp);
}

/*************************************************************************************************/
/*!
 *  \brief  Host channel class update handler for Periodic Advertising.
 *
 *  \param  chanMap     Updated channel map.
 *
 *  \return Status code.
 */
/*************************************************************************************************/
static uint8_t lctrSlvPeriodicChClassUpdate(uint64_t chanMap)
{
  lctrChanMapUpdate_t *pMsg;

  /* Update for periodic adv sets */
  for (uint8_t perAdvHandle = 0; perAdvHandle < pLctrRtCfg->maxAdvSets; perAdvHandle++)
  {
    if (LctrPerAdvEnabled(perAdvHandle) == TRUE)
    {
      if ((pMsg = (lctrChanMapUpdate_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        pMsg->hdr.handle = (uint16_t)perAdvHandle;
        pMsg->hdr.dispId = LCTR_DISP_ACAD;
        pMsg->hdr.event  = LCTR_ACAD_MSG_CHAN_UPDATE;
        pMsg->chanMap = chanMap;

        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
      }
      else
      {
        LL_TRACE_ERR0("lctrSlvPeriodicChClassUpdate: out of message buffers");
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
    }
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize link layer controller resources for advertising slave.
 */
/*************************************************************************************************/
void LctrSlvExtAdvInit(void)
{
  /* Add extended advertising reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_EXT_ADV] = lctrSlvExtAdvResetHandler;

  /* Add extended advertising task message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_EXT_ADV] = (LctrMsgDisp_t)lctrSlvExtAdvDisp;

  /* Add advertising task event handlers. */
  lctrEventHdlrTbl[LCTR_EVENT_RX_SCAN_REQ] = lctrSlvRxExtScanReq;

  /* Add ACAD message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_ACAD] = (LctrMsgDisp_t)lctrSlvAcadDisp;

  lctrRegisterChClassHandler(lctrSlvPeriodicChClassUpdate);

  /* Set supported features. */
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_0)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_LE_EXT_ADV;
  }

  LctrSlvExtAdvDefaults();
}

/*************************************************************************************************/
/*!
 *  \brief  Set default values for advertising slave.
 */
/*************************************************************************************************/
void LctrSlvExtAdvDefaults(void)
{
  memset(pLctrAdvSetTbl, 0, (sizeof(lctrAdvSet_t) * pLctrRtCfg->maxAdvSets));
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize advertising set memory resources.
 *
 *  \param  pFreeMem    Pointer to free memory.
 *  \param  freeMemSize Size of pFreeMem.
 *
 *  \return Amount of free memory consumed.
 */
/*************************************************************************************************/
uint16_t LctrInitAdvSetMem(uint8_t *pFreeMem, uint32_t freeMemSize)
{
  uint8_t *pAvailMem = pFreeMem;

  /*** Advertising Set Context ***/

  if (((uint32_t)pAvailMem) & 3)
  {
    /* Align to next word. */
    pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
  }

  LL_TRACE_INFO2("    RAM: %u x %u bytes -- advertising set context", pLctrRtCfg->maxAdvSets, sizeof(lctrAdvSet_t));

  /* Allocate context memory. */
  pLctrAdvSetTbl = (lctrAdvSet_t *)pAvailMem;
  pAvailMem += sizeof(lctrAdvSet_t) * pLctrRtCfg->maxAdvSets;

  /* Allocate adv/scan rsp buffer memory. */
  for (unsigned int i = 0; i < pLctrRtCfg->maxAdvSets; i++)
  {
    if (((uint32_t)pAvailMem) & 3)
    {
      /* Align to next word. */
      pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
    }

    lctrExtAdvDataBufTbl[i] = pAvailMem;
    pAvailMem += pLctrRtCfg->maxExtAdvDataLen;
  }

  /* Allocate periodic adv buffer memory. */
  for (unsigned int i = 0; i < pLctrRtCfg->maxAdvSets; i++)
  {
    if (((uint32_t)pAvailMem) & 3)
    {
      /* Align to next word. */
      pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
    }

    lctrPerAdvDataBufTbl[i] = pAvailMem;
    pAvailMem += pLctrRtCfg->maxExtAdvDataLen;
  }

  if (((uint32_t)(pAvailMem - pFreeMem)) > freeMemSize)
  {
    LL_TRACE_ERR2("LctrInitAdvSetMem: failed to allocate advertising set context, need=%u available=%u", (pAvailMem - pFreeMem), freeMemSize);
    WSF_ASSERT(FALSE);
    return 0;
  }

  lmgrPersistCb.advSetCtxSize = sizeof(lctrAdvSet_t);

  return (pAvailMem - pFreeMem);
}

/*************************************************************************************************/
/*!
 *  \brief  Is set extended advertising enable parameter valid
 *
 *  \param  enable      True if enable extented adv, False otherwise.
 *  \param  pEnaParam   Set extended adv enable parameters.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrIsExtAdvEnableParamValid(uint8_t enable, LlExtAdvEnableParam_t *pEnaParam)
{
  if (enable == FALSE)
  {
    /* Skip validation if enable is False. */
    return LL_SUCCESS;
  }

  lctrAdvSet_t *pAdvSet;
  if ((pAdvSet = lctrFindAdvSet(pEnaParam->handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  const uint8_t LEGACY_HIGH_DUTY = (LL_ADV_EVT_PROP_LEGACY_ADV_BIT | LL_ADV_EVT_PROP_HIGH_DUTY_ADV_BIT |
                                    LL_ADV_EVT_PROP_DIRECT_ADV_BIT | LL_ADV_EVT_PROP_CONN_ADV_BIT);
  const uint8_t HIGH_DUTY_MAX_DURATION = 128;   /* unit in 10ms. */
  /* Advertising interval is determined by LL for high duty cycle directed adv. */
  if ((pAdvSet->param.advEventProp & LEGACY_HIGH_DUTY) == LEGACY_HIGH_DUTY )
  {
    if ((pEnaParam->duration > HIGH_DUTY_MAX_DURATION) ||
        (pEnaParam->duration == 0))
    {
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Is advertising set ready for enable?
 *
 *  \param  handle      Advertising handle.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrIsExtAdvEnableReady(uint8_t handle)
{
  lctrAdvSet_t *pAdvSet;
  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  if (!pAdvSet->advData.ready || !pAdvSet->scanRspData.ready)
  {
    LL_TRACE_WARN1("Incomplete AdvData or ScanRspData buffer, handle=%u", handle);
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) == 0) &&
      (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_SCAN_ADV_BIT) &&
      (pAdvSet->scanRspData.len == 0))
  {
    LL_TRACE_WARN1("No scan response data for scannable ext advertising, handle=%u", handle);
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  switch (pAdvSet->param.ownAddrType)
  {
    case LL_ADDR_RANDOM:
    case LL_ADDR_RANDOM_IDENTITY:
      if (!pAdvSet->bdAddrRndValid)
      {
        LL_TRACE_WARN1("Random address not available, handle=%u", handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      break;
    default:
      break;
  }

  if (((pAdvSet->param.advEventProp & (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_LEGACY_ADV_BIT)) ==
      (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_LEGACY_ADV_BIT)) &&                               /* Extended advertising with legacy PDU. */
      (pAdvSet->advData.len > LL_ADVBU_MAX_LEN))
  {
    LL_TRACE_WARN1("AdvData too large with legacy connectable advertising, handle=%u", handle);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }
  else if (((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_CONN_ADV_BIT) ==
            LL_ADV_EVT_PROP_CONN_ADV_BIT) &&                                                            /* Extended advertising PDU. */
           (pAdvSet->advData.len > (LL_EXT_ADVB_MAX_LEN - LL_ADV_HDR_LEN - LL_EXT_ADV_HDR_MAX_LEN)))    /* Assume maximum advertising header length.  */
  {
    LL_TRACE_WARN1("AdvData too large with extended connectable advertising, handle=%u", handle);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Check if the advertising handle is valid.
 *
 *  \param  handle      Advertising handle.
 *
 *  \return TRUE if valid, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t LctrIsAdvHandleValid(uint8_t handle)
{
  return (lctrFindAdvSet(handle) != NULL);
}

/*************************************************************************************************/
/*!
 *  \brief  Get extended advertising TX power level.
 *
 *  \param  handle          Advertising handle.
 *  \param  pLevel          Transmit power level.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrGetExtAdvTxPowerLevel(uint16_t handle, int8_t *pLevel)
{
  lctrAdvSet_t *pAdvSet;
  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }
  *pLevel = PalRadioGetActualTxPower(pAdvSet->param.advTxPwr, FALSE);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Get a list of active advertising handles.
 *
 *  \param  pHandles    Table to store advertising handles.
 *
 *  \return Number of active advertising handles.
 */
/*************************************************************************************************/
uint8_t LctrGetAdvHandles(uint8_t pHandles[LL_MAX_ADV_SETS])
{
  unsigned int numHandles = 0;

  unsigned int i;
  for (i = 0; i < pLctrRtCfg->maxAdvSets; i++)
  {
    lctrAdvSet_t *pAdvSet = &pLctrAdvSetTbl[i];

    if (pAdvSet->enabled)
    {
      pHandles[numHandles++] = pAdvSet->handle;
    }
  }

  return numHandles;
}

/*************************************************************************************************/
/*!
 *  \brief  Set advertising set random device address.
 *
 *  \param  handle      Advertising handle.
 *  \param  pAddr       Random Bluetooth device address.
 *
 *  \return Status error code.
 *
 *  Set the random address to be used by a advertising set.
 */
/*************************************************************************************************/
uint8_t LctrSetExtAdvSetRandAddr(uint8_t handle, const uint8_t *pAddr)
{
  WSF_CS_INIT(cs);

  lctrAdvSet_t *pAdvSet;
  uint64_t bdAddr;
  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  if ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_CONN_ADV_BIT) &&
      (pAdvSet->state != LCTR_EXT_ADV_STATE_DISABLED))
  {
    LL_TRACE_WARN1("Cannot modify AdvSet's random address when connectable advertising, handle=%u", handle);
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  BSTREAM_TO_BDA64(bdAddr, pAddr);
  /* Check BD Random Address type. */
  switch (bdAddr & LL_RAND_ADDR_TYPE_MASK)
  {
    case LL_RAND_ADDR_TYPE_STATIC:
    case LL_RAND_ADDR_TYPE_RPA:
    case LL_RAND_ADDR_TYPE_NRPA:
      /* Valid types. */
      break;
    default:
      LL_TRACE_WARN0("Invalid random address type");
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  WSF_CS_ENTER(cs);
  pAdvSet->bdAddrRnd = bdAddr;
  pAdvSet->bdAddrRndValid = TRUE;
  pAdvSet->bdAddrRndMod = TRUE;

  WSF_CS_EXIT(cs);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Choose Advertising Address for a advertising set.
 *
 *  \param      pPduHdr     PDU header.
 *  \param      pBle        BLE operation data.
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrChooseSetAdvA(lctrAdvbPduHdr_t *pPduHdr, BbBleData_t * const pBle, lctrAdvSet_t *pAdvSet)
{
  /* TODO: Update with privacy implementation: Create context fields in pAdvSet instead of using lmgrSlvAdvCb. */

  /* Choose advertiser's address type. */
  lmgrSlvAdvCb.ownAddrType = pAdvSet->param.ownAddrType & ~LL_ADDR_IDENTITY_BIT;  /* Fallback to non-RPA. */
  lmgrSlvAdvCb.localRpa = 0;

  /* Fallback onto non-RPA if local IRK is zero or peer address is not in resolving list. */
  if ((lmgrSlvAdvCb.ownAddrType == LL_ADDR_RANDOM_BIT) &&
      (pAdvSet->bdAddrRndValid))
  {
    pPduHdr->txAddrRnd = TRUE;
    pAdvSet->advA = pAdvSet->bdAddrRnd;
    pAdvSet->bdAddrRndMod = FALSE;
  }
  else
  {
    pPduHdr->txAddrRnd = FALSE;
    pAdvSet->advA = lmgrPersistCb.bdAddr;
  }

  /* Generate local RPA for advertisement. */
  if (pAdvSet->param.ownAddrType & LL_ADDR_IDENTITY_BIT)
  {
    uint64_t localRpa;

    if (BbBleResListReadLocal(pAdvSet->param.peerAddrType, pAdvSet->param.peerAddr, &localRpa))
    {
      /* Actually using RPA.  Even though this is a RPA, any received local address will be strictly matched (never resolved). */
      pPduHdr->txAddrRnd = TRUE;
      pAdvSet->advA = localRpa;
      lmgrSlvAdvCb.ownAddrType = pAdvSet->param.ownAddrType;
      lmgrSlvAdvCb.localRpa    = localRpa;  /* Save local RPA for connection complete event. */
    }
  }

  /* Always match local address in PDU to advertiser's address. */
  pBle->pduFilt.localAddrMatch = pAdvSet->advA;
  if (pPduHdr->txAddrRnd)
  {
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_MATCH_RAND);
  }
  BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, LOCAL_ADDR_MATCH_ENA);

  /* Sometimes match peer address, but always match to command parameters. */
  pBle->pduFilt.peerAddrMatch = pAdvSet->param.peerAddr;
  if (pAdvSet->param.peerAddrType)
  {
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_MATCH_RAND);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Choose target address for a advertising set.
 *
 *  \param  pPduHdr         PDU header.
 *  \param  pBle            BLE data.
 *  \param  pAdvSet         Advertising set.
 */
/*************************************************************************************************/
void lctrChooseSetPeerA(lctrAdvbPduHdr_t *pPduHdr, BbBleData_t * const pBle, lctrAdvSet_t *pAdvSet)
{
  pPduHdr->rxAddrRnd = pAdvSet->param.peerAddrType;
  pAdvSet->tgtA = pAdvSet->param.peerAddr;

  if (lmgrCb.addrResEna)
  {
    /* Attempt to generate RPA for peer. */
    if (pAdvSet->param.ownAddrType & LL_ADDR_IDENTITY_BIT)
    {
      uint64_t peerRpa = 0;

      if (BbBleResListReadPeer(pAdvSet->param.peerAddrType, pAdvSet->param.peerAddr, &peerRpa) == FALSE)
      {
        BbBleResListGeneratePeer(pAdvSet->param.peerAddrType, pAdvSet->param.peerAddr, &peerRpa);
      }

      if (peerRpa != 0)
      {
        pPduHdr->rxAddrRnd  = TRUE;
        pAdvSet->tgtA    = peerRpa;
      }
    }
    /* Resolve peer RPAs whether or not a RPA was generated for peer. */
    BB_BLE_PDU_FILT_SET_FLAG(&pBle->pduFilt, PEER_ADDR_RES_ENA);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Get advertising set random device address.
 *
 *  \param  handle      Advertising handle.
 *  \param  pAddr       Random Bluetooth device address.
 *
 *  \return Status error code.
 *
 *  Get the random address to be used by a advertising set.
 */
/*************************************************************************************************/
uint8_t LctrGetExtAdvSetRandAddr(uint8_t handle, uint8_t *pAddr)
{
  lctrAdvSet_t *pAdvSet;
  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  BDA64_TO_BSTREAM(pAddr, pAdvSet->bdAddrRnd);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Set extended advertising parameters.
 *
 *  \param  handle          Advertising handle.
 *  \param  pExtAdvParam    Extended advertising parameters.
 *
 *  \return Status error code.
 *
 *  Set extended advertising parameters.
 *
 *  \note   This function must only be called when advertising for this set is disabled.
 */
/*************************************************************************************************/
uint8_t LctrSetExtAdvParam(uint8_t handle, LlExtAdvParam_t *pExtAdvParam)
{
  lctrAdvSet_t *pAdvSet;
  bool_t propUpdate = TRUE;

  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    if ((pAdvSet = lctrAllocAdvSet(handle)) == NULL)
    {
      return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
    }
    propUpdate = FALSE;
  }

  if (pAdvSet->state != LCTR_EXT_ADV_STATE_DISABLED)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (propUpdate && (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT))
  {
    /* Transition from legacy to extended advertising; copy from exclusive buffer. */
    pAdvSet->advData.len = pAdvSet->advData.alt.legacy.len;
    memcpy(pAdvSet->advData.pBuf, pAdvSet->advData.alt.legacy.buf, pAdvSet->advData.alt.legacy.len);
    pAdvSet->scanRspData.len = pAdvSet->scanRspData.alt.legacy.len;
    memcpy(pAdvSet->scanRspData.pBuf, pAdvSet->scanRspData.alt.legacy.buf, pAdvSet->scanRspData.alt.legacy.len);
  }

  if ((pExtAdvParam->advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) == 0)
  {
    if (pExtAdvParam->advEventProp & LL_ADV_EVT_PROP_CONN_ADV_BIT)
    {
      if (pAdvSet->scanRspData.len)
      {
        LL_TRACE_WARN1("Invalid scan response data buffer set with connectable advertising, handle=%u", pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
    }
    else if (pExtAdvParam->advEventProp & LL_ADV_EVT_PROP_SCAN_ADV_BIT)
    {
      if (pAdvSet->advData.len)
      {
        LL_TRACE_WARN1("Invalid advertising data buffer set with scannable advertising, handle=%u", pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
    }
    else /* ((pExtAdvParam->advEventProp & (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT)) == 0) */
    {
      if (pAdvSet->scanRspData.len)
      {
        LL_TRACE_WARN1("Invalid scan response data buffer set with non-scannable and non-connectable advertising, handle=%u", pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
    }
  }
  else /* (pExtAdvParam->advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) */
  {
    if (propUpdate && ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) == 0))
    {
      /* Transition from extended to legacy. */
      if (pAdvSet->advData.len > LL_ADVBU_MAX_LEN)
      {
        LL_TRACE_WARN2("Invalid advertising data buffer length=%u, handle=%u", pAdvSet->advData.len, pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      if (pAdvSet->scanRspData.len > LL_ADVBU_MAX_LEN)
      {
        LL_TRACE_WARN2("Invalid scan response data buffer length=%u, handle=%u", pAdvSet->scanRspData.len, pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }

      /* Copy to exclusive buffer. */
      pAdvSet->advData.alt.legacy.len = pAdvSet->advData.len;
      memcpy(pAdvSet->advData.alt.legacy.buf, pAdvSet->advData.pBuf, pAdvSet->advData.len);
      pAdvSet->scanRspData.alt.legacy.len = pAdvSet->scanRspData.len;
      memcpy(pAdvSet->scanRspData.alt.legacy.buf, pAdvSet->scanRspData.pBuf, pAdvSet->scanRspData.len);
    }
    else
    {
      if (pAdvSet->advData.len > LL_ADVBU_MAX_LEN)
      {
        LL_TRACE_WARN2("Invalid advertising data buffer length=%u, handle=%u", pAdvSet->advData.len, pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      if (pAdvSet->scanRspData.len > LL_ADVBU_MAX_LEN)
      {
        LL_TRACE_WARN2("Invalid scan response data buffer length=%u, handle=%u", pAdvSet->scanRspData.len, pAdvSet->handle);
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
    }
  }

  pAdvSet->param.advEventProp       = pExtAdvParam->advEventProp;
  pAdvSet->param.priAdvInterMinUsec = BB_BLE_TO_US(pExtAdvParam->priAdvInterMin);
  pAdvSet->param.priAdvInterMaxUsec = BB_BLE_TO_US(pExtAdvParam->priAdvInterMax);
  pAdvSet->param.priAdvChanMap      = pExtAdvParam->priAdvChanMap;
  pAdvSet->param.ownAddrType        = pExtAdvParam->ownAddrType;
  pAdvSet->param.peerAddrType       = pExtAdvParam->peerAddrType;
  pAdvSet->param.peerAddr           = BstreamToBda64(pExtAdvParam->pPeerAddr);
  pAdvSet->param.advFiltPolicy      = pExtAdvParam->advFiltPolicy;
  pAdvSet->param.advTxPwr           = pExtAdvParam->advTxPwr;
  pAdvSet->param.priAdvPhy          = pExtAdvParam->priAdvPhy;
  pAdvSet->param.secAdvMaxSkip      = pExtAdvParam->secAdvMaxSkip;
  pAdvSet->param.secAdvPhy          = pExtAdvParam->secAdvPhy;
  pAdvSet->param.advSID             = pExtAdvParam->advSID;
  pAdvSet->param.scanReqNotifEna    = pExtAdvParam->scanReqNotifEna;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Set extended advertising data.
 *
 *  \param  handle      Advertising handle.
 *  \param  op          Operation.
 *  \param  fragPref    Fragment preference.
 *  \param  len         Data buffer length.
 *  \param  pData       Advertising data buffer.
 *
 *  \return Status error code.
 *
 *  Set extended advertising data data.
 */
/*************************************************************************************************/
uint8_t LctrSetExtAdvData(uint8_t handle, uint8_t op, uint8_t fragPref, uint8_t len, const uint8_t *pData)
{
  uint8_t result;

  lctrAdvSet_t *pAdvSet;
  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  if ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) == 0)
  {
    if (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_SCAN_ADV_BIT)
    {
      LL_TRACE_WARN1("Cannot add advertising data buffer with scannable advertising, handle=%u", pAdvSet->handle);
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }

    if ((len > 0) && (pAdvSet->scanRspData.len > 0))
    {
      LL_TRACE_WARN1("Extended advertising data buffer already in use, handle=%u", pAdvSet->handle);
      return LL_ERROR_CODE_CMD_DISALLOWED;
    }

    result = lctrSetExtAdvDataSm(pAdvSet, &pAdvSet->advData, op, fragPref, len, pData);
  }
  else
  {
    if ((result = lctrSetLegacyAdvDataSm(pAdvSet, &pAdvSet->advData, op, len, pData)) == LL_SUCCESS)
    {
      pAdvSet->advData.fragPref = LL_ADV_DATA_FRAG_DISALLOW;
    }
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief  Get channel map of periodic sdvertiser.
 *
 *  \param  handle          Advertising handle.
 *
 *  \return Channel map in 64bit int format.
 *
 */
/*************************************************************************************************/
uint64_t LctrGetPerAdvChanMap(uint8_t handle)
{
  lctrAdvSet_t *pAdvSet;
  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return 0;
  }

  return pAdvSet->perParam.perChanParam.chanMask;
}

/*************************************************************************************************/
/*!
 *  \brief  Set periodic advertising parameters.
 *
 *  \param  handle          Advertising handle.
 *  \param  pPerAdvParam    Periodic advertising parameters.
 *
 *  \return Status error code.
 *
 *  Set periodic advertising parameters.
 *
 */
/*************************************************************************************************/
uint8_t LctrSetPeriodicAdvParam(uint8_t handle, LlPerAdvParam_t *pPerAdvParam)
{
  lctrAdvSet_t *pAdvSet;

  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  if (pAdvSet->perParam.perAdvEnabled == TRUE)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  uint16_t mafOffset = WSF_MAX(pAdvSet->auxDelayUsec, pLctrRtCfg->auxDelayUsec);
  uint32_t worstCaseUsec = SchBleCalcPerAdvDurationUsec(pAdvSet->param.secAdvPhy,
                                               0,
                                               mafOffset,
                                               pAdvSet->perAdvData.len,
                                               TRUE,
                                               BB_PHY_OPTIONS_BLE_S8);

  if (worstCaseUsec > LCTR_PER_INTER_TO_US(pPerAdvParam->perAdvInterMax))
  {
    return LL_ERROR_CODE_PKT_TOO_LONG;
  }

  /* Anonymous advertising is not allowed for periodic advertising. */
  if (pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_OMIT_AA_BIT)
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  pAdvSet->perParam.advInterMinUsec = LCTR_PER_INTER_TO_US(pPerAdvParam->perAdvInterMin);    /* Convert parameter to bb ticks */
  pAdvSet->perParam.advInterMaxUsec = LCTR_PER_INTER_TO_US(pPerAdvParam->perAdvInterMax);    /* Convert parameter to bb ticks */
  pAdvSet->perParam.advEventProp = pPerAdvParam->perAdvProp;
  pAdvSet->perParam.advParamReady = TRUE;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Set periodic advertising data.
 *
 *  \param  handle      Advertising handle.
 *  \param  op          Operation.
 *  \param  len         Data buffer length.
 *  \param  pData       Periodic advertising data buffer.
 *
 *  \return Status error code.
 *
 *  Set periodic advertising data.
 */
/*************************************************************************************************/
uint8_t LctrSetPeriodicAdvData(uint8_t handle, uint8_t op, uint8_t len, const uint8_t *pData)
{
  uint8_t result;
  lctrAdvSet_t *pAdvSet;

  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

   if (pAdvSet->perParam.advParamReady == FALSE) /* AdvSet is not configured for Periodic Advertising. */
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

   if ((op != LL_ADV_DATA_OP_COMP) && (op != LL_ADV_DATA_OP_UNCHANGED))
  {
    if (len == 0)
    {
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }

    if (pAdvSet->perParam.perAdvEnabled)
    {
      return LL_ERROR_CODE_CMD_DISALLOWED;
    }
  }

  uint32_t worstCaseUsec;
  uint16_t mafOffset = WSF_MAX(pAdvSet->auxDelayUsec, pLctrRtCfg->auxDelayUsec);
  uint32_t maxInterval = (pAdvSet->perParam.perAdvEnabled) ? (pAdvSet->perParam.perAdvInterUsec) : (pAdvSet->perParam.advInterMaxUsec);

  if (op == LL_ADV_DATA_OP_FRAG_INTER || op == LL_ADV_DATA_OP_FRAG_LAST) /* Intermediate chained data. */
  {
    worstCaseUsec = SchBleCalcPerAdvDurationUsec(pAdvSet->param.secAdvPhy,
                                                 0,
                                                 mafOffset,
                                                 len + pAdvSet->perAdvData.len,
                                                 TRUE,
                                                 BB_PHY_OPTIONS_BLE_S8);
  }
  else /* Len is complete data. */
  {
    worstCaseUsec = SchBleCalcPerAdvDurationUsec(pAdvSet->param.secAdvPhy,
                                                 0,
                                                 mafOffset,
                                                 len,
                                                 TRUE,
                                                 BB_PHY_OPTIONS_BLE_S8);
  }

  if (worstCaseUsec > maxInterval)
  {
    return LL_ERROR_CODE_PKT_TOO_LONG;
  }

  if ((result = lctrSetPerAdvDataSm(pAdvSet, &pAdvSet->perAdvData, op, len, pData)) == LL_SUCCESS)
  {
    pAdvSet->perAdvData.fragPref = LL_ADV_DATA_FRAG_DISALLOW;
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief  Enable/disable periodic advertising.
 *
 *  \param  handle          Advertising handle.
 *  \param  enable          Set periodic advertising enabled/disabled.
 *
 *  Enable/disable periodic advertising.
 */
/*************************************************************************************************/
void LctrSetPeriodicAdvEnable(uint8_t handle, bool_t enable)
{
  lctrAdvSet_t *pAdvSet;
  bool_t enableAdv = enable & LL_PER_ADV_ENABLE_ADV_ENABLE_BIT;
  bool_t enableAdi = (enable & LL_PER_ADV_ENABLE_ADI_ENABLE_BIT) >> 1;

  if ((enableAdv == TRUE) && (enableAdi == TRUE) &&
      !(lmgrCb.features & LL_FEAT_PER_ADV_ADI_SUP))
  {
    LmgrSendPeriodicAdvEnableCnf(handle, LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE);
    return;
  }

  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    LmgrSendPeriodicAdvEnableCnf(handle, LL_ERROR_CODE_UNKNOWN_ADV_ID);
    return;
  }

  if ((pAdvSet->perParam.advParamReady == FALSE) ||                                                     /* Periodic advertising parameters shall be set. */
      ((enableAdv == TRUE) && (pAdvSet->perAdvData.ready == FALSE)) ||                                  /* Periodic advertising data shall be complete. */
      (pAdvSet->param.advEventProp & (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT |     /* Only non-connectable and non-scannable is allowed. */
                                      LL_ADV_EVT_PROP_HIGH_DUTY_ADV_BIT | LL_ADV_EVT_PROP_OMIT_AA_BIT)))/* No high duty cycle, No anonymous advertising. */
  {
    LmgrSendPeriodicAdvEnableCnf(handle, LL_ERROR_CODE_CMD_DISALLOWED);
    return;
  }

  pAdvSet->perParam.enableAdi = enableAdi;

  LctrPerAdvEnableMsg_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.handle = handle;
    pMsg->hdr.dispId = LCTR_DISP_PER_ADV;
    pMsg->hdr.event  = enableAdv ? LCTR_PER_ADV_MSG_START : LCTR_PER_ADV_MSG_STOP;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set extended scan response data.
 *
 *  \param  handle      Advertising handle.
 *  \param  op          Operation.
 *  \param  fragPref    Fragment preference.
 *  \param  len         Data buffer length.
 *  \param  pData       Scan response data buffer.
 *
 *  \return Status error code.
 *
 *  Set extended scan response data.
 */
/*************************************************************************************************/
uint8_t LctrSetExtScanRespData(uint8_t handle, uint8_t op, uint8_t fragPref, uint8_t len, const uint8_t *pData)
{
  uint8_t result;

  lctrAdvSet_t *pAdvSet;
  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  if (op == LL_ADV_DATA_OP_UNCHANGED)
  {
    LL_TRACE_WARN2("Invalid operation=%u, handle=%u", op, pAdvSet->handle);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  if ((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) == 0)
  {
    if (((pAdvSet->param.advEventProp & LL_ADV_EVT_PROP_SCAN_ADV_BIT) == 0) &&
        (len > 0))
    {
      LL_TRACE_WARN1("Cannot add scan response data buffer with non-scannable advertising, handle=%u", pAdvSet->handle);
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }
    else
    {
      if ((len == 0) && (op != LL_ADV_DATA_OP_COMP))
      {
        LL_TRACE_WARN1("Zero-length scan response data is not valid with scannable advertising, handle=%u", pAdvSet->handle);
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
    }

    if ((len > 0) && (pAdvSet->advData.len > 0))
    {
      LL_TRACE_WARN1("Extended advertising data buffer already in use, handle=%u", pAdvSet->handle);
      return LL_ERROR_CODE_CMD_DISALLOWED;
    }

    result = lctrSetExtAdvDataSm(pAdvSet, &pAdvSet->scanRspData, op, fragPref, len, pData);
  }
  else
  {
    if ((result = lctrSetLegacyAdvDataSm(pAdvSet, &pAdvSet->scanRspData, op, len, pData)) == LL_SUCCESS)
    {
      /* Fragmentation disabled for legacy. */
      pAdvSet->scanRspData.fragPref = LL_ADV_DATA_FRAG_DISALLOW;
    }
  }

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief  Remove advertising set.
 *
 *  \param  handle      Advertising handle.
 *
 *  \return Status error code.
 *
 *  Removes the an advertising set from the LL.
 */
/*************************************************************************************************/
uint8_t LctrRemoveAdvSet(uint8_t handle)
{
  lctrAdvSet_t *pAdvSet;

  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  if ((pAdvSet->state != LCTR_EXT_ADV_STATE_DISABLED) ||
      (pAdvSet->perParam.perAdvEnabled == TRUE))
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (pAdvSet->removeCback)
  {
    pAdvSet->removeCback(handle);
  }

  lctrFreeAdvSet(pAdvSet);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Clear advertising sets.
 *
 *  \return Status error code.
 *
 *  Remove all existing advertising sets from the LL.
 */
/*************************************************************************************************/
uint8_t LctrClearAdvSets(void)
{
  unsigned int i;

  for (i = 0; i < pLctrRtCfg->maxAdvSets; i++)
  {
    if ((pLctrAdvSetTbl[i].state != LCTR_EXT_ADV_STATE_DISABLED) ||
        (pLctrAdvSetTbl[i].perParam.perAdvEnabled == TRUE))
    {
      return LL_ERROR_CODE_CMD_DISALLOWED;
    }
  }

  for (i = 0; i < pLctrRtCfg->maxAdvSets; i++)
  {
    if (pLctrAdvSetTbl[i].enabled)
    {
      lctrFreeAdvSet(&pLctrAdvSetTbl[i]);
    }
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Set auxiliary packet offset delay.
 *
 *  \param  handle      Advertising handle.
 *  \param  delayUsec   Additional time in microseconds. "0" to disable.
 *
 *  \return Status error code.
 *
 *  Additional delay given to auxiliary packets specified by AuxPtr. Offset values are
 *  limited by the advertising interval.
 */
/*************************************************************************************************/
uint8_t LctrSetAuxOffsetDelay(uint8_t handle, uint32_t delayUsec)
{
  WSF_CS_INIT(cs);

  lctrAdvSet_t *pAdvSet;

  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  WSF_CS_ENTER(cs);
  pAdvSet->auxDelayUsec = delayUsec;
  WSF_CS_EXIT(cs);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended advertising data fragmentation length.
 *
 *  \param      handle      Advertising handle.
 *  \param      fragLen     Fragmentation length.
 *
 *  \return     Status error code.
 *
 *  Fragmentation size for Advertising Data and Scan Response Data when selected by the host.
 */
/*************************************************************************************************/
uint8_t LctrSetExtAdvDataFragLen(uint8_t handle, uint8_t fragLen)
{
  WSF_CS_INIT(cs);

  lctrAdvSet_t *pAdvSet;

  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  if ((fragLen < 1) || (fragLen > LL_EXT_ADVBU_MAX_LEN))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  WSF_CS_ENTER(cs);
  pAdvSet->advDataFragLen = fragLen;
  WSF_CS_EXIT(cs);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended advertising transmit PHY options.
 *
 *  \param      handle      Advertising handle.
 *  \param      priPhyOpts  Primary advertising channel PHY options.
 *  \param      secPhyOpts  Secondary advertising channel PHY options.
 *
 *  \return     Status error code.
 *
 *  PHY options for extended advertising transmissions. New values are applied dynamically.
 */
/*************************************************************************************************/
uint8_t LctrSetExtAdvTxPhyOptions(uint8_t handle, uint8_t priPhyOpts, uint8_t secPhyOpts)
{
  WSF_CS_INIT(cs);

  lctrAdvSet_t *pAdvSet;

  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  WSF_CS_ENTER(cs);
  pAdvSet->bleData.chan.tifsTxPhyOptions = priPhyOpts;
  pAdvSet->auxBleData.chan.tifsTxPhyOptions = secPhyOpts;
  WSF_CS_EXIT(cs);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal extended advertising subsystem message.
 *
 *  \param  pAdvSet     Advertising set.
 *  \param  event       Extended advertising event.
 */
/*************************************************************************************************/
void lctrSendAdvSetMsg(lctrAdvSet_t *pAdvSet, uint8_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t))) != NULL)
  {
    pMsg->handle = pAdvSet->handle;
    pMsg->dispId = LCTR_DISP_EXT_ADV;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Slave periodic advertising message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrSlvPeriodicAdvDisp(LctrPerAdvMsg_t *pMsg)
{
  pLctrSlvPerAdvMsg = pMsg;

  lctrAdvSet_t *pAdvSet;

  if (pMsg->hdr.dispId != LCTR_DISP_BCST)
  {
    if ((pAdvSet = lctrFindAdvSet(pMsg->hdr.handle)) != NULL)
    {
      lctrSlvPeriodicAdvExecuteSm(pAdvSet, pMsg->hdr.event);
    }
  }
  else
  {
    for (unsigned int i = 0; i < pLctrRtCfg->maxAdvSets; i++)
    {
      pAdvSet = &pLctrAdvSetTbl[i];
      if (pAdvSet->enabled)
      {
        lctrSlvPeriodicAdvExecuteSm(pAdvSet, pMsg->hdr.event);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal periodic advertising subsystem message.
 *
 *  \param  pAdvSet     Advertising set.
 *  \param  event       Periodic advertising event.
 */
/*************************************************************************************************/
void lctrSendPeriodicAdvSetMsg(lctrAdvSet_t *pAdvSet, uint8_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t))) != NULL)
  {
    pMsg->handle = pAdvSet->handle;
    pMsg->dispId = LCTR_DISP_PER_ADV;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Get status of periodic adv handle.
 *
 *  \param  handle     Advertising handle.
 *
 *  \return True if periodic advertising is running on that handle, False if not
 */
/*************************************************************************************************/
bool_t LctrIsPerAdvEnabled(uint8_t handle)
{
  lctrAdvSet_t *pAdvSet;

  if ((pAdvSet = lctrFindAdvSet(handle)) == NULL)
  {
    return FALSE;
  }

  return ((pAdvSet->perParam.perAdvEnabled == TRUE) &&
         (pAdvSet->perParam.perState == LCTR_PER_ADV_STATE_ENABLED));
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize link layer controller resources for periodic advertising slave.
 */
/*************************************************************************************************/
void LctrSlvPeriodicAdvInit(void)
{
  /* Add periodic advertising reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_PER_ADV] = NULL;   /* Not needed. */

  /* Add extended advertising task message dispatchers. */
  lctrMsgDispTbl[LCTR_DISP_PER_ADV] = (LctrMsgDisp_t)lctrSlvPeriodicAdvDisp;

  /* Add periodic advertising check to function pointer */
  LctrPerAdvEnabled = LctrIsPerAdvEnabled;

  /* Set supported features. */
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_0)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_LE_PER_ADV;
  }

  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_SYDNEY)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_PER_ADV_ADI_SUP;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Commit periodic operation.
 *
 *  \param      pAdvSet     Advertising set.
 *  \param      pOp         Periodic BOD.
 */
/*************************************************************************************************/
static void lctrSlvPeriodicCommitOp(lctrAdvSet_t *pAdvSet, BbOpDesc_t * const pOp)
{
  const uint32_t curTime = PalBbGetCurrentTime();
  uint32_t offsetUsec = SchRmGetOffsetUsec(pAdvSet->perParam.perAdvInterUsec,
                                           LL_MAX_CONN + LCTR_GET_EXT_ADV_INDEX(pAdvSet), curTime);   /* RM is shared by connections and periodic advertising. */

  pOp->dueUsec = curTime + offsetUsec;

  /*** Commit operation ***/
  uint32_t anchorPointUsec = pOp->dueUsec;
  uint16_t numIntervals = 0;

  while (TRUE)
  {
    if (SchInsertAtDueTime(pOp, NULL))
    {
      break;
    }

    LL_TRACE_WARN1("!!! Establish periodic advertising schedule conflict handle=%u", pAdvSet->handle);

    numIntervals += 1;
    pAdvSet->perParam.perEventCounter += 1;

    uint32_t perInterUsec = numIntervals * pAdvSet->perParam.perAdvInterUsec;

    /* Advance to next interval. */
    pOp->dueUsec = anchorPointUsec + perInterUsec;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Build periodic advertising operation.
 *
 *  \param  pAdvSet     Advertising set.
 *
 *  \return Error status code.
 */
/*************************************************************************************************/
uint8_t lctrSlvPeriodicAdvBuildOp(lctrAdvSet_t *pAdvSet)
{
  BbOpDesc_t * const pOp = &pAdvSet->perParam.perAdvBod;
  BbBleData_t * const pBle = &pAdvSet->perParam.perBleData;
  BbBleSlvAuxAdvEvent_t * const pPerAdv = &pBle->op.slvPerAdv;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_FIXED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrSlvPeriodicAdvEndOp;
  pOp->abortCback = lctrSlvPeriodicAdvAbortOp;
  pOp->pCtx = pAdvSet;
  pOp->pDataLen = &(pAdvSet->perAdvData.len);

  /*** BLE General Setup ***/
  pAdvSet->perParam.perChHopInc = lctrComputeHopInc();

  pBle->chan.opType = BB_BLE_OP_SLV_PER_ADV_EVENT;

  pBle->chan.chanIdx    = pAdvSet->perParam.perChIdx;
  if (pAdvSet->param.advTxPwr == HCI_TX_PWR_NO_PREFERENCE)
  {
    pBle->chan.txPower  = lmgrCb.advTxPwr;
  }
  else
  {
    pBle->chan.txPower  = pAdvSet->param.advTxPwr;
  }
  pBle->chan.accAddr    = pAdvSet->perParam.perAccessAddr;
  pBle->chan.crcInit    = lctrComputeCrcInit();
  pBle->chan.txPhy      = pAdvSet->param.secAdvPhy;
  pBle->chan.rxPhy      = pAdvSet->param.secAdvPhy;

  /* Set PHY options to default behavior for AUX_SYNC_IND. */
  pBle->chan.initTxPhyOptions = lmgrSlvAdvCb.defTxPhyOpts;

#if (LL_ENABLE_TESTER == TRUE)
  /* TODO If tester ability if necessary. */
  pBle->chan.accAddrRx = llTesterCb.auxAccessAddrRx ^ pBle->chan.accAddr;
  pBle->chan.accAddrTx = llTesterCb.auxAccessAddrTx ^ pBle->chan.accAddr;
  pBle->chan.crcInitRx = llTesterCb.auxCrcInitRx ^ pBle->chan.crcInit;
  pBle->chan.crcInitTx = llTesterCb.auxCrcInitTx ^ pBle->chan.crcInit;
#endif

  /*** BLE Advertising Setup: Tx advertising packet ***/

  pPerAdv->txAuxSetupCback = lctrSlvTxSetupPeriodicAdvDataHandler;

  pAdvSet->perAdvData.txOffs = 0;
  pPerAdv->txAuxAdvPdu[0].pBuf = pAdvSet->perAdvHdrBuf;
  /* Auxiliary advertising buffer setup dynamically in lctrSlvTxSetupAuxAdvDataHandler(). */
  /* pAuxAdv->txAuxAdvPdu[1].pBuf = NULL; */
  /* pAuxAdv->txAuxAdvPdu[1].len = 0; */

  /*** BLE Advertising Setup: Rx request packet ***/

  /* pPerAdv->pRxAuxReqBuf = NULL; */       /* Cleared in alloc. */
  /* pPerAdv->rxAuxReqCback = NULL; */      /* Cleared in alloc. */

  /*** BLE Advertising Setup: Tx chain indication packet ***/

  pPerAdv->txAuxChainPdu[0].pBuf = pAdvSet->perAdvHdrBuf;
  /* Chain buffer setup dynamically in lctrSlvTxSetupAuxAdvDataHandler(). */

  /*** Commit operation ***/
  SchBleCalcAdvOpDuration(pOp, 0);
  uint32_t interMinUsec = pAdvSet->perParam.advInterMinUsec;
  uint32_t interMaxUsec = pAdvSet->perParam.advInterMaxUsec;
  uint32_t durUsec = pOp->minDurUsec;
  uint32_t perInterUsec;

  /* Make sure the worst case advertising duration can make the intervals. */
  uint16_t mafOffset = WSF_MAX(pAdvSet->auxDelayUsec, pLctrRtCfg->auxDelayUsec);
  uint32_t worstCaseUsec = SchBleCalcPerAdvDurationUsec(pAdvSet->param.secAdvPhy,
                                                        0,
                                                        mafOffset,
                                                        pAdvSet->perAdvData.len,
                                                        TRUE,
                                                        BB_PHY_OPTIONS_BLE_S8);

  if (worstCaseUsec > interMaxUsec)
  {
    return LL_ERROR_CODE_PKT_TOO_LONG;
  }

  /* If the minimum interval is too fast, set minimum to be the worst case. */
  if (worstCaseUsec > interMinUsec)
  {
    interMinUsec = worstCaseUsec;
  }

  /* Max interval is preferred in resource manager. */
  if (!SchRmAdd(LCTR_GET_PER_RM_HANDLE(pAdvSet), SCH_RM_PREF_CAPACITY, interMinUsec, interMaxUsec, durUsec, &perInterUsec, lctrGetPerRefTime))
  {
    return LL_ERROR_CODE_LIMIT_REACHED;
  }

  pAdvSet->perParam.perAdvInterUsec = perInterUsec;
  lctrSlvPeriodicCommitOp(pAdvSet, pOp);

  LL_TRACE_INFO2("    >>> PerAdv started, AdvSet.handle=%u perChHopInc=%u <<<", pAdvSet->handle, pAdvSet->perParam.perChHopInc);
  LL_TRACE_INFO1("                        perInterUsec=%u", perInterUsec);
  LL_TRACE_INFO1("                        perAccessAddr=0x%08x", pAdvSet->perParam.perAccessAddr);
  LL_TRACE_INFO1("                        pPerAdvBod=0x%08x", pOp);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Get reference time (due time) of the periodic advertising.
 *
 *  \param      perHandle   Periodic advertising handle.
 *  \param      pDurUsec    Pointer to duration of the BOD.
 *
 *  \return     Due time in microseconds of the periodic advertising handle.
 */
/*************************************************************************************************/
uint32_t lctrGetPerRefTime(uint8_t perHandle, uint32_t *pDurUsec)
{
  uint32_t refTime = 0;
  lctrAdvSet_t *pAdvSet;

  WSF_ASSERT(perHandle >= LL_MAX_CONN);

  pAdvSet = &pLctrAdvSetTbl[perHandle - LL_MAX_CONN];
  if (pAdvSet->perParam.perState == LCTR_PER_ADV_STATE_ENABLED)
  {
    refTime = pAdvSet->perParam.perAdvBod.dueUsec;
    if (pDurUsec)
    {
      *pDurUsec = pAdvSet->perParam.perAdvBod.minDurUsec;
    }
  }

  return refTime;
}
