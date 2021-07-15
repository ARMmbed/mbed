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
#include "lctr_int_iso.h"
#include "lctr_int_conn.h"
#include "lctr_int.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "bb_api.h"
#include "pal_bb.h"
#include "pal_crypto.h"
#include "wsf_assert.h"
#include "wsf_buf.h"
#include "wsf_msg.h"
#include "wsf_math.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Globals
**************************************************************************************************/

/*! \brief      CIS context table. */
lctrCisCtx_t *pLctrCisTbl;

/*! \brief      CIG context table. */
lctrCigCtx_t *pLctrCigTbl;

/*! \brief      CIS message. */
lctrCisMsg_t *pLctrCisMsg;

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Update channel parameter.
 *
 *  \param      pCisCtx   CIS context.
 *  \param      chanMask  Channel mask.
 */
/*************************************************************************************************/
static void LctrCisUpdateChanParam(lctrCisCtx_t *pCisCtx, uint64_t chanMask)
{
  LL_TRACE_INFO1("LctrCisUpdateChanParam cisHandle=%u", pCisCtx->cisHandle);
  LL_TRACE_INFO1("LctrCisUpdateChanParam, chanMask=%u", chanMask);

  pCisCtx->chanParam.chanMask = chanMask;
  LmgrBuildRemapTable(&pCisCtx->chanParam);
  pCisCtx->chIdx = LmgrSelectNextChannel(&pCisCtx->chanParam, pCisCtx->cisEvtCounter, 0, TRUE);
  pCisCtx->nextSubEvtChanIdx = LmgrSelectNextSubEvtChannel(&pCisCtx->chanParam);
}

/**************************************************************************************************
  External Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Allocate a CIS context.
 *
 *  \param      pCigCtx   Pointer to the CIG context.
 *
 *  \return     CIS context or NULL if at capacity.
 */
/*************************************************************************************************/
lctrCisCtx_t *lctrAllocCisCtx(lctrCigCtx_t *pCigCtx)
{
  for (uint16_t i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if (!pCisCtx->enabled)
    {
      memset(pCisCtx, 0, sizeof(lctrCisCtx_t));

      pCisCtx->enabled = TRUE;
      pCisCtx->cisHandle = LCTR_FIRST_CIS_HANDLE + i;

      /* Setup LLCP timer. */
      pCisCtx->tmrProcRsp.handlerId = lmgrPersistCb.handlerId;
      lctrMsgHdr_t *pMsg = (lctrMsgHdr_t *)&pCisCtx->tmrProcRsp.msg;
      pMsg->handle = pCisCtx->aclHandle;
      pMsg->dispId = LCTR_DISP_CONN;
      pMsg->event = LCTR_CONN_TMR_CIS_LLCP_RSP_EXP;

      /* Setup supervision timer. */
      pCisCtx->tmrSupTimeout.handlerId = lmgrPersistCb.handlerId;
      pMsg = (lctrMsgHdr_t *)&pCisCtx->tmrSupTimeout.msg;
      pMsg->handle = pCisCtx->cisHandle;
      pMsg->dispId = LCTR_DISP_CIS;
      pMsg->event = LCTR_CONN_TERM_SUP_TIMEOUT;

      /* Initialize data path info. */
      pCisCtx->dataPathInCtx.id = LL_ISO_DATA_PATH_DISABLED;
      pCisCtx->dataPathOutCtx.id = LL_ISO_DATA_PATH_DISABLED;

      /* TODO: Update this value when needed. */
      pCisCtx->transLatUsec = LL_ISO_TRANSPORT_LAT_MIN;

      pCisCtx->cigId = pCigCtx->cigId;    /* Save the CIG ID. */

      return pCisCtx;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Cleanup context, clean CIG context if necessary for slave.
 *
 *  \param      pCisCtx       CIS context to free.
 */
/*************************************************************************************************/
void lctrCleanupCtx(lctrCisCtx_t *pCisCtx)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  WSF_ASSERT(pCigCtx);

  uint8_t *pBuf;
  uint8_t numTxBufs;
  wsfHandlerId_t handlerId;

  pCisCtx->cisEvtCounter = 0;
  pCisCtx->firstFromPeer = FALSE;
  pCisCtx->connEst = FALSE;

  /* Must do one at a time in case one datapath is not used. */
  LctrRemoveIsoDataPath(pCisCtx->cisHandle, LL_ISO_DATA_PATH_INPUT_BIT);
  LctrRemoveIsoDataPath(pCisCtx->cisHandle, LL_ISO_DATA_PATH_OUTPUT_BIT);

  /* Flush remaining transmit packets. */
  numTxBufs = lctrCisTxQueueClear(pCisCtx);

  /* Flush remaining transmit packets. */
  while ((pBuf = WsfMsgDeq(&pCisCtx->txIsoQ, &handlerId)) != NULL)
  {
    lctrDataTxIncAvailBuf();
    numTxBufs++;

    WsfMsgFree(pBuf);
  }

  /* Flush remaining Tx/Rx flush timeout list. */
  lctrCisFtListClear(&pCisCtx->txFtParamList);
  lctrCisFtListClear(&pCisCtx->rxFtParamList);

  /* Cleanup timers. */
  WsfTimerStop(&pCisCtx->tmrSupTimeout);
  WsfTimerStop(&pCisCtx->tmrProcRsp);

  if (lctrCisIsHeadCis(&pCigCtx->list, pCisCtx) == TRUE)
  {
    pCigCtx->headCisRmved = TRUE;
    pCigCtx->firstRxStartTsUsec = pCisCtx->data.slv.firstRxStartTsUsec;

    if (pCigCtx->packing == LL_PACKING_INTERLEAVED)
    {
      pCigCtx->offsetUsec = pCisCtx->delayUsec;
    }
    else
    {
      pCigCtx->offsetUsec = pCisCtx->subIntervUsec * pCisCtx->nse;
    }
  }

  if (pCisCtx->role == LL_ROLE_SLAVE)
  {
    lctrFreeCisCtx(pCisCtx);

    (void)lctrCisRemove(&pCigCtx->list, pCisCtx);    /* Result could be FALSE since BOD might not be in list. */

    if (lctrCisIsListEmpty(&pCigCtx->list))
    {
      lctrFreeCigCtx(pCigCtx);
    }
  }
  else
  {
    /* CIS context for master will not be freed until CIG is removed. */

    (void)lctrCisRemove(&pCigCtx->list, pCisCtx);    /* Result could be FALSE since BOD might not be in list. */

    if (lctrCisIsListEmpty(&pCigCtx->list))
    {
      /* Refresh CIG context to get ready for re-starting CIS. */
      pCigCtx->headCisRmved = FALSE;
      pCigCtx->isBodBuilt = FALSE;
      pCigCtx->isBodStarted = FALSE;

      if (pCigCtx->isRmAdded == TRUE)
      {
        SchRmRemove(LCTR_GET_CIG_RM_HANDLE(pCigCtx));
        pCigCtx->isRmAdded = FALSE;
      }
    }

    /* CIG is removed through host command, unless resetted. */

    if ((lctrResetEnabled == TRUE))
    {
      lctrFreeCisCtx(pCisCtx);

      if (lctrCisIsListEmpty(&pCigCtx->list))
      {
        lctrFreeCigCtx(pCigCtx);
      }
    }
  }

  /* Send CIS message to continue reset. This message will not be processed since the CIS context has been cleared. */
  if (lctrResetEnabled)
  {
    lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_CLOSED);
  }

  void *pIsoBuf;
  while ((pIsoBuf = WsfMsgDeq(&pCisCtx->isoalTxCtx.pendingSduQ, &handlerId)) != NULL)
  {
    WsfMsgFree(pIsoBuf);
  }

  lctrIsoalRxDataPathClear(&pCisCtx->isoalRxCtx, pCisCtx->framing);
}

/*************************************************************************************************/
/*!
 *  \brief      Free a  CIS context.
 *
 *  \param      pCisCtx       CIS context to free.
 */
/*************************************************************************************************/
void lctrFreeCisCtx(lctrCisCtx_t *pCisCtx)
{
  WSF_ASSERT(pCisCtx->enabled);
  pCisCtx->enabled = FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Find a CIS by handle.
 *
 *  \param      cisHandle      CIS handle.
 *
 *  \return     CIS or NULL if not found.
 */
/*************************************************************************************************/
lctrCisCtx_t *lctrFindCisByHandle(uint16_t cisHandle)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if ((pCisCtx->cisHandle == cisHandle) &&
        (pCisCtx->enabled))
    {
      return pCisCtx;
    }
  }
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Find a CIS by CIG ID and CIS ID.
 *
 *  \param      cigId       CIG identifier.
 *  \param      cisId       CIS identifier.
 *
 *  \return     CIS or NULL if not found.
 */
/*************************************************************************************************/
lctrCisCtx_t *lctrFindCisById(uint8_t cigId, uint8_t cisId)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if ((pCisCtx->enabled) &&
        (pCisCtx->cigId == cigId) &&
        (pCisCtx->cisId == cisId))
    {
      return pCisCtx;
    }
  }
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Allocate a CIG context.
 *
 *  \param      cigId  CIG ID.
 *
 *  \return     CIG context or NULL if at capacity.
 */
/*************************************************************************************************/
lctrCigCtx_t *lctrAllocCigCtx(uint8_t cigId)
{
  for (uint16_t i = 0; i < pLctrRtCfg->maxCig; i++)
  {
    lctrCigCtx_t *pCigCtx = &pLctrCigTbl[i];

    if (!pCigCtx->enabled)
    {
      memset(pCigCtx, 0, sizeof(lctrCigCtx_t));

      /* pCigCtx->numCisEsted = 0; */
      pCigCtx->enabled = TRUE;
      pCigCtx->cigId = cigId;
      pCigCtx->cigHandle = i;

      /* Initialize the CIS list. */
      pCigCtx->list.numNodes = 0;
      pCigCtx->list.pHead = NULL;
      pCigCtx->list.pTail = NULL;

      LmgrIncResetRefCount();

      /* Enable BB. */
      BbStart(BB_PROT_BLE);

      return pCigCtx;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Free a  CIG context.
 *
 *  \param      pCigCtx       CIG context to free.
 */
/*************************************************************************************************/
void lctrFreeCigCtx(lctrCigCtx_t *pCigCtx)
{
  WSF_ASSERT(pCigCtx->enabled);

  pCigCtx->enabled = FALSE;
  pCigCtx->roleData.slv.lastActiveEvent = 0;
  pCigCtx->isBodBuilt = FALSE;
  pCigCtx->isBodStarted = FALSE;
  pCigCtx->numCisEsted = 0;

  LmgrDecResetRefCount();

  /* Disable BB. */
  BbStop(BB_PROT_BLE);
}

/*************************************************************************************************/
/*!
 *  \brief      Find a CIG by CIG ID.
 *
 *  \param      cigId       CIG identifier.
 *
 *  \return     CIG or NULL if not found.
 */
/*************************************************************************************************/
lctrCigCtx_t *lctrFindCigById(uint8_t cigId)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxCig; i++)
  {
    lctrCigCtx_t *pCigCtx = &pLctrCigTbl[i];

    if ((pCigCtx->enabled) &&
        (pCigCtx->cigId == cigId))
    {
      return pCigCtx;
    }
  }
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of available CIG contexts.
 *
 *  \return     Number of available CIS contexts.
 */
/*************************************************************************************************/
uint8_t lctrGetNumAvailCigCtx()
{
  uint8_t count = 0;

  for (unsigned int i = 0; i < pLctrRtCfg->maxCig; i++)
  {
    lctrCigCtx_t *pCigCtx = &pLctrCigTbl[i];

    if (pCigCtx->enabled == FALSE)
    {
      count++;
    }
  }

  return count;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize connected isochronous stream memory resources.
 *
 *  \param  pFreeMem    Pointer to free memory.
 *  \param  freeMemSize Size of pFreeMem.
 *
 *  \return Amount of free memory consumed.
 */
/*************************************************************************************************/
uint16_t LctrInitCisMem(uint8_t *pFreeMem, uint32_t freeMemSize)
{
  uint8_t *pAvailMem = pFreeMem;

  /*** Advertising Set Context ***/

  if (((uint32_t)pAvailMem) & 3)
  {
    /* Align to next word. */
    pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
  }

  LL_TRACE_INFO2("    RAM: %u x %u bytes -- CIS context", pLctrRtCfg->maxCis, sizeof(lctrCisCtx_t));

  /* Allocate CIS context memory. */
  pLctrCisTbl = (lctrCisCtx_t *)pAvailMem;
  pAvailMem += sizeof(lctrCisCtx_t) * pLctrRtCfg->maxCis;

  if (((uint32_t)pAvailMem) & 3)
  {
    /* Align to next word. */
    pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
  }

  LL_TRACE_INFO2("    RAM: %u x %u bytes -- CIG context", pLctrRtCfg->maxCig, sizeof(lctrCigCtx_t));

  /* Allocate CIG context memory. */
  pLctrCigTbl = (lctrCigCtx_t *)pAvailMem;
  pAvailMem += sizeof(lctrCigCtx_t) * pLctrRtCfg->maxCig;

  if (((uint32_t)(pAvailMem - pFreeMem)) > freeMemSize)
  {
    LL_TRACE_ERR2("LctrInitCisMem: failed to allocate CIS, need=%u available=%u", (pAvailMem - pFreeMem), freeMemSize);
    WSF_ASSERT(FALSE);
    return 0;
  }

  lmgrPersistCb.cisCtxSize = sizeof(lctrCisCtx_t);
  lmgrPersistCb.cigCtxSize = sizeof(lctrCigCtx_t);

  return (pAvailMem - pFreeMem);
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize flush timeout parameters.
 *
 *  \param      pFtParam      Pointer to the flush parameter.
 *  \param      bn            Burst number.
 *  \param      ft            Flush time.
 *  \param      nse           Number of subevent.
 */
/*************************************************************************************************/
void lctrCisInitFtParam(lctrFtParam_t *pFtParam, uint8_t bn, uint8_t ft, uint8_t nse)
{
  pFtParam->pduRcved = FALSE;
  pFtParam->pduAcked = FALSE;
  pFtParam->pduCounter = 0;
  pFtParam->subEvtCounter = 0;
  pFtParam->bn = bn;
  pFtParam->intervalTotal = ft;
  pFtParam->intervalCounter = 0;

  if (bn == 1)
  {
    /* BN = 1, numSubEvtFt[0] = BN * NSE */
    pFtParam->lastSubEvtFt[0] = nse;
    pFtParam->isPduDone[0] = FALSE;
    pFtParam->pduType[0] = LCTR_CIS_PDU_DEFAULT;
  }
  else
  {
    /* BN > 1, i = 0, numSubEvtFt[i] = FLOOR(NSE/BN) + MOD(NSE, BN) */
    uint8_t floor = nse / bn;

    pFtParam->lastSubEvtFt[0] = floor + (nse - (floor * bn));
    pFtParam->isPduDone[0] = FALSE;

    /* BN > 1, i > 0, numSubEvtFt[i] = floor */
    for (unsigned int i = 1; i < WSF_MIN(bn, LCTR_MAX_BN); i++)
    {
      pFtParam->lastSubEvtFt[i] = floor + pFtParam->lastSubEvtFt[i-1];
      pFtParam->isPduDone[i] = FALSE;
      pFtParam->pduType[i] = LCTR_CIS_PDU_DEFAULT;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check if the CIS is established or not.
 *
 *  \param      pCisCtx      CIS context.
 *
 *  \return     TRUE if CIS is established.
 */
/*************************************************************************************************/
bool_t lctrIsCisEst(lctrCisCtx_t *pCisCtx)
{
  return (pCisCtx->state == LCTR_CIS_STATE_EST) ? TRUE : FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of available CIS contexts.
 *
 *  \return     Number of available CIS contexts.
 */
/*************************************************************************************************/
uint8_t lctrGetNumAvailCisCtx()
{
  uint8_t count = 0;

  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if (pCisCtx->enabled == FALSE)
    {
      count++;
    }
  }

  return count;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of CIS contexts that are already set up(enabled).
 *
 *  \param      pSetCigParam      Set CIG parameters.
 *
 *  \return     Number of CIS contexts that are already set up(enabled).
 */
/*************************************************************************************************/
uint8_t lctrGetNumEnabledCisCtx(LlCisCigParams_t *pSetCigParam)
{
  uint8_t count = 0;

  for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
  {
    if (lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[i].cisId))
    {
      count++;
    }
  }

  return count;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of CIS contexts that are enabled(in use).
 *
 *  \param      pSetCigParam      Set CIG parameters.
 *
 *  \return     Number of CIS contexts that are already set up(enabled).
 */
/*************************************************************************************************/
uint8_t lctrGetNumEnabledCisCtxTest(LlCisCigParamsTest_t *pSetCigParam)
{
  uint8_t count = 0;

  for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
  {
    if (lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[i].cisId))
    {
      count++;
    }
  }

  return count;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of CIS contexts that are already established.
 *
 *  \param      pSetCigParam      Set CIG parameters.
 *
 *  \return     Number of CIS contexts that are established.
 */
/*************************************************************************************************/
uint8_t lctrGetNumEstCisCtx(LlCisCigParams_t *pSetCigParam)
{
  uint8_t count = 0;

  for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
  {
    lctrCisCtx_t *pCisCtx;

    if ((pCisCtx = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[i].cisId)))
    {
      if (lctrIsCisEst(pCisCtx))
      {
        count++;
      }
    }
  }

  return count;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of CIS contexts that are already established(enabled) for testing.
 *
 *  \param      pSetCigParam      Set CIG parameters.
 *
 *  \return     Number of CIS contexts that are established.
 */
/*************************************************************************************************/
uint8_t lctrGetNumEstCisCtxTest(LlCisCigParamsTest_t *pSetCigParam)
{
  uint8_t count = 0;

  for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
  {
    lctrCisCtx_t *pCisCtx;

    if ((pCisCtx = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[i].cisId)))
    {
      if (lctrIsCisEst(pCisCtx))
      {
        count++;
      }
    }
  }

  return count;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of CIS contexts that are already established(enabled) in a CIG.
 *
 *  \param      pCigCtx       CIG context.
 *
 *  \return     Number of CIS contexts that are established.
 */
/*************************************************************************************************/
uint8_t lctrGetNumEstCisCtxByCigCtx(lctrCigCtx_t *pCigCtx)
{
  WSF_ASSERT(pCigCtx);

  return pCigCtx->numCisEsted;
}

/*************************************************************************************************/
/*!
 *  \brief      CIS message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
void lctrCisDisp(lctrCisMsg_t *pMsg)
{
  lctrCisCtx_t *pCisCtx;

  if (pMsg->hdr.dispId != LCTR_DISP_BCST)
  {
    WSF_ASSERT(pMsg->hdr.handle < (LCTR_FIRST_BIS_HANDLE)); /* The beginning of BIS handles is the upper bound of CIS handles. */

    if ((pCisCtx = lctrFindCisByHandle(pMsg->hdr.handle)) != NULL)
    {
      pLctrCisMsg = pMsg;

      lctrCisExecuteSm(pCisCtx, pMsg->hdr.event);
    }
  }
  else
  {
    /* Clear CIG handles. */
    lctrCleanupCigCtx();
  }
}

/*************************************************************************************************/
/*!
 *  \brief  CIS conflict resolution handler.
 *
 *  \param  pNewOp      New BOD.
 *  \param  pExistOp    Existing BOD.
 *
 *  \return Prioritized BOD.
 *
 *  Prioritize BLE connection at most risk for termination.
 */
/*************************************************************************************************/
BbOpDesc_t *lctrCisResolveConflict(BbOpDesc_t *pNewOp, BbOpDesc_t *pExistOp)
{
  /* TODO Add conflict resolution handler */

  return pExistOp;
}

/*************************************************************************************************/
/*!
 *  \brief      Set up the channel parameter.
 *
 *  \param      pCisCtx   CIS context.
 *  \param      chanMask  Channel mask.
 */
/*************************************************************************************************/
void lctrCisSetupChanParam(lctrCisCtx_t *pCisCtx, uint64_t chanMask)
{
  pCisCtx->chanParam.chanMask = chanMask;
  LmgrBuildRemapTable(&pCisCtx->chanParam);
  pCisCtx->chanParam.usedChSel = LL_CH_SEL_2;
  pCisCtx->chanParam.chIdentifier = (pCisCtx->accessAddr >> 16) ^
                                    (pCisCtx->accessAddr >> 0);
  pCisCtx->chIdx = LmgrSelectNextChannel(&pCisCtx->chanParam, pCisCtx->cisEvtCounter, 0, TRUE);
  pCisCtx->nextSubEvtChanIdx = LmgrSelectNextSubEvtChannel(&pCisCtx->chanParam);
  pCisCtx->subEvtCounter = 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Update channel map triggered by host or peer.
 *
 *  \param      aclHandle   ACL handle.
 */
/*************************************************************************************************/
void LctrCisUpdateChanMap(uint16_t aclHandle)
{
  LL_TRACE_INFO1("LctrCisUpdateChanMap aclHandle=%u", aclHandle);

  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if ((pCisCtx->aclHandle == aclHandle) &&
        (pCisCtx->enabled))
    {
      lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

      LctrCisUpdateChanParam(pCisCtx, pCtx->chanMask);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize cipher block for CIS.
 *
 *  \param  pCisCtx      Connection context.
 */
/*************************************************************************************************/
void lctrCisSetupEncrypt(lctrCisCtx_t *pCisCtx)
{
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);
  WSF_ASSERT(pCtx);

  PalCryptoEnc_t * const pEnc = &pCisCtx->bleData.chan.enc;

  PalCryptoAesEcb(pCtx->ltk, pEnc->sk, pCtx->skd);

  memcpy(pEnc->iv, pCtx->iv, sizeof(pEnc->iv));
  uint8_t *pTemp, accAddr[4];
  pTemp = accAddr;
  UINT32_TO_BSTREAM(pTemp, pCisCtx->accessAddr)
  pEnc->iv[0] ^= accAddr[0];
  pEnc->iv[1] ^= accAddr[1];
  pEnc->iv[2] ^= accAddr[2];
  pEnc->iv[3] ^= accAddr[3];
  pEnc->dir = (pCisCtx->role == LL_ROLE_MASTER) ? 1 : 0;     /* master = 1; slave = 0 */
  pEnc->type = PAL_BB_TYPE_CIS;

  if (lctrInitCipherBlkHdlr)
  {
    lctrInitCipherBlkHdlr(pEnc, pCisCtx->cisHandle, pEnc->dir);
  }

  pEnc->enaEncrypt = pCtx->bleData.chan.enc.enaEncrypt;
  pEnc->enaDecrypt = pCtx->bleData.chan.enc.enaDecrypt;
}

/**************************************************************************************************
  Utility functions to manipulate the CIS linked list.
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Create a node.
 *
 *  \param      pCisCtx   CIS context.
 *
 *  \return     Pointer to the node or NULL if creation fails.
 */
/*************************************************************************************************/
static lctrCisNode_t * lctrCisCreateNode(lctrCisCtx_t *pCisCtx)
{
  lctrCisNode_t *pNew = NULL;

  if ((pNew = (lctrCisNode_t *)WsfBufAlloc(sizeof(lctrCisNode_t))) != NULL)
  {
    pNew->pNext = NULL;
    pNew->pCisCtx = pCisCtx;
  }

  return pNew;
}

/*************************************************************************************************/
/*!
 *  \brief      Insert at the head of the list.
 *
 *  \param      pList     Pointer to the list.
 *  \param      pCisCtx   CIS context.
 *
 *  \return     True if successfully insert at the head, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisInsertHead(lctrCisList_t *pList, lctrCisCtx_t *pCisCtx)
{
  lctrCisNode_t *pTemp = NULL;

  if ((pTemp = lctrCisCreateNode(pCisCtx)) == NULL)
  {
    return FALSE;
  }

  if (lctrCisIsListEmpty(pList))
  {
    /* List is empty. */
    pList->pHead = pList->pTail = pTemp;
  }
  else
  {
    /* List is not empty. */
    pTemp->pNext = pList->pHead;
    pList->pHead = pTemp;
  }
  pList->numNodes++;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Insert at the tail of the list.
 *
 *  \param      pList     Pointer to the list.
 *  \param      pCisCtx   CIS context.
 *
 *  \return     True if successfully insert at the head, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisInsertTail(lctrCisList_t *pList, lctrCisCtx_t *pCisCtx)
{
  lctrCisNode_t *pTemp = NULL;

  if ((pTemp = lctrCisCreateNode(pCisCtx)) == NULL)
  {
    return FALSE;
  }

  if (lctrCisIsListEmpty(pList))
  {
    /* List is empty. */
    pList->pHead = pList->pTail = pTemp;
  }
  else
  {
    /* List is not empty. */
    pList->pTail->pNext = pTemp;
    pList->pTail = pTemp;
  }
  pList->numNodes++;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Remove at the head of the list.
 *
 *  \param      pList     Pointer to the list.
 *
 *  \return     True if successfully remove at the head, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisRemoveHead(lctrCisList_t *pList)
{
  lctrCisNode_t *pTemp = NULL;

  if (lctrCisIsListEmpty(pList))
  {
    /* List is empty. */
    return FALSE;
  }
  else
  {
    /* List is not empty. */
    pTemp = pList->pHead;
    pList->pHead = pTemp->pNext;
    WsfBufFree(pTemp);
  }
  pList->numNodes--;

  if (lctrCisIsListEmpty(pList))
  {
    pList->pTail = NULL;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Remove at the tail of the list.
 *
 *  \param      pList     Pointer to the list.
 *
 *  \return     True if successfully remove at the tail, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisRemoveTail(lctrCisList_t *pList)
{
  lctrCisNode_t *pTemp = NULL;

  if (lctrCisIsListEmpty(pList))
  {
    /* List is empty. */
    return FALSE;
  }
  else if (pList->numNodes == 1)
  {
    /* List has only one node. */
    pTemp = pList->pTail;
    WsfBufFree(pTemp);
    pList->pHead = pList->pTail = NULL;
  }
  else
  {
    /* List has more than one node. */
    pTemp = pList->pHead;

    /* Traverse to the second last. */
    while (pTemp->pNext->pNext != NULL)
    {
      pTemp = pTemp->pNext;
    }

    WsfBufFree(pTemp->pNext); /* Free the tail. */
    pTemp->pNext = NULL;
    pList->pTail = pTemp;
  }
  pList->numNodes--;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Remove at the middle of the list.
 *
 *  \param      pList     Pointer to the list.
 *  \param      pCisCtx   Pointer to the CIS context to remove
 *
 *  \return     True if successfully remove at the head, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisRemoveMiddle(lctrCisList_t *pList, lctrCisCtx_t *pCisCtx)
{
  lctrCisNode_t *pTemp = NULL;
  lctrCisNode_t *pRemove = NULL;

  if (lctrCisIsListEmpty(pList))
  {
    /* List is empty. */
    return FALSE;
  }
  else
  {
    /* List is not empty. */
    pTemp = pList->pHead;

    while (pTemp->pNext != pList->pTail)
    {
      if (pTemp->pNext->pCisCtx == pCisCtx)
      {
        pRemove = pTemp->pNext;
        pTemp->pNext = pTemp->pNext->pNext;
        WsfBufFree(pRemove);
        pList->numNodes--;
        return TRUE;
      }
      pTemp = pTemp->pNext;
    }
  }

  /* Cannot fine the CIS context in the linked list.  */
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Remove a CIS context from the list.
 *
 *  \param      pList     Pointer to the list.
 *  \param      pCisCtx   Pointer to the CIS context to remove
 *
 *  \return     True if successfully remove at the head, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisRemove(lctrCisList_t *pList, lctrCisCtx_t *pCisCtx)
{
  if (lctrCisIsListEmpty(pList))
  {
    /* List is empty. */
    return FALSE;
  }

  if (pList->pHead->pCisCtx == pCisCtx)
  {
    return lctrCisRemoveHead(pList);
  }
  else if (pList->pTail->pCisCtx == pCisCtx)
  {
    return lctrCisRemoveTail(pList);
  }
  else
  {
    return lctrCisRemoveMiddle(pList, pCisCtx);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check if the list is empty or not.
 *
 *  \param      pList     Pointer to the list.
 *
 *  \return     True if successfully remove at the tail, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisIsListEmpty(lctrCisList_t *pList)
{
  return (pList->numNodes == 0) ? TRUE : FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the number of elements in the list.
 *
 *  \param      pList     Pointer to the list.
 *
 *  \return     Number of elements in the list.
 */
/*************************************************************************************************/
uint8_t lctrCisGetListCount(lctrCisList_t *pList)
{
  return pList->numNodes;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the CIS context at the head of the linked list.
 *
 *  \param      pList     Pointer to the list.
 *
 *  \return     NULL or pointer to the CIS context at the head of the linked list.
 */
/*************************************************************************************************/
lctrCisCtx_t * lctrCisGetHeadCis(lctrCisList_t *pList)
{
  if (lctrCisIsListEmpty(pList))
  {
    return NULL;
  }

  return pList->pHead->pCisCtx;
}

/*************************************************************************************************/
/*!
 *  \brief      Check if the input context is the head in the linked list.
 *
 *  \param      pList     Pointer to the list.
 *  \param      pCisCtx   Pointer to the CIS context.
 *
 *  \return     TRUE if the input context is the head in the linked list, FALSE otherwise
 */
/*************************************************************************************************/
bool_t lctrCisIsHeadCis(lctrCisList_t *pList, lctrCisCtx_t *pCisCtx)
{
  if (lctrCisIsListEmpty(pList))
  {
    return FALSE;
  }

  return (pList->pHead->pCisCtx == pCisCtx ? TRUE : FALSE);
}

/*************************************************************************************************/
/*!
 *  \brief      Return the next CIS context after the current CIS context in the linked list.
 *
 *  \param      pList       Pointer to the list.
 *  \param      pCurCisCtx  Current CIS.
 *
 *  \return     NULL or pointer to the next CIS context after the current CIS context in the linked list.
 */
/*************************************************************************************************/
lctrCisCtx_t * lctrCisGetNextCis(lctrCisList_t *pList, lctrCisCtx_t *pCurCisCtx)
{
  if (lctrCisIsListEmpty(pList))
  {
    return NULL;
  }

  lctrCisNode_t *pTempNode = pList->pHead;

  while (pTempNode)
  {
    if (pTempNode->pCisCtx == pCurCisCtx)
    {
      if (pTempNode->pNext)
      {
        return pTempNode->pNext->pCisCtx;
      }
      else
      {
        return NULL;
      }
    }

    pTempNode = pTempNode->pNext;
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief      Return the next CIS context after the current CIS context in the linked list.
 *
 *  \param      pList       Pointer to the list.
 *  \param      pCurCisCtx  Current CIS.
 *
 *  \return     NULL or pointer to the next CIS context after the current CIS context in the linked list.
 */
/*************************************************************************************************/
lctrCisCtx_t * lctrCisGetPreCis(lctrCisList_t *pList, lctrCisCtx_t *pCurCisCtx)
{
  if (lctrCisIsListEmpty(pList))
  {
    return NULL;
  }

  lctrCisNode_t *pTempNode = pList->pHead;

  while (pTempNode->pNext)
  {
    if (pTempNode->pNext->pCisCtx == pCurCisCtx)
    {
      return pTempNode->pCisCtx;
    }
    pTempNode = pTempNode->pNext;
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Check whether all the interleaved CISs in the CIG are done or not.
 *
 *  \param  pList        Pointer to the CIS list in the CIG.
 *
 *  \return TRUE if all the CISs in the CIG are done, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisAreCisCtxDone(lctrCisList_t *pList)
{
  lctrCisNode_t *pTempNode = pList->pHead;

  while (pTempNode)
  {
    if (pTempNode->pCisCtx->cisDone == FALSE)
    {
      return FALSE;
    }

    pTempNode = pTempNode->pNext;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Clear the cisDone flag for all the CISs in the CIG.
 *
 *  \param  pList        Pointer to the CIS list in the CIG.
 */
/*************************************************************************************************/
void lctrCisClearCisDone(lctrCisList_t *pList)
{
  lctrCisNode_t *pTempNode = pList->pHead;

  while (pTempNode)
  {
    pTempNode->pCisCtx->cisDone = FALSE;
    pTempNode = pTempNode->pNext;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the cisDone flag for all the CISs after the CIS, including the current one.
 *
 *  \param  pList        Pointer to the CIS list in the CIG.
 *  \param  pCurCisCtx   Current CIS.
 */
/*************************************************************************************************/
void lctrCisSetCisDone(lctrCisList_t *pList, lctrCisCtx_t *pCurCisCtx)
{
  lctrCisNode_t *pTgtNode = NULL, *pTempNode = pList->pHead;

  while (pTempNode)
  {
    if (pTempNode->pCisCtx == pCurCisCtx)
    {
      pTgtNode = pTempNode;
      break;
    }
    pTempNode = pTempNode->pNext;
  }

  if (pTgtNode == NULL)
  {
    return;
  }
  else
  {
    pTempNode = pTgtNode;

    while (pTempNode)
    {
      pTempNode->pCisCtx->cisDone = TRUE;
      pTempNode = pTempNode->pNext;
    }
  }
}

/**************************************************************************************************
  Utility functions to manipulate the CIS Tx/Rx Ft list.
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Create a flush timeout node.
 *
 *  \param      pFtParam   Flush timeout parameters.
 *
 *  \return     Pointer to the node or NULL if creation fails.
 */
/*************************************************************************************************/
lctrFtParamNode_t * lctrCisFtCreateFtParamNode(lctrFtParam_t *pFtParam)
{
  lctrFtParamNode_t *pNew = NULL;

  if ((pNew = (lctrFtParamNode_t *)WsfBufAlloc(sizeof(lctrFtParamNode_t))) != NULL)
  {
    pNew->pNext = NULL;
    memcpy(&pNew->ftParam, pFtParam, sizeof(lctrFtParam_t));
  }

  return pNew;
}

/*************************************************************************************************/
/*!
 *  \brief      Insert at the head of the list.
 *
 *  \param      pList     Pointer to the list.
 *  \param      pFtParam  Flush timeout parameters.
 *
 *  \return     True if successfully insert at the head, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisFtInsertHead(lctrFtParamList_t *pList, lctrFtParam_t *pFtParam)
{
  lctrFtParamNode_t *pTemp = NULL;

  if ((pTemp = lctrCisFtCreateFtParamNode(pFtParam)) == NULL)
  {
    return FALSE;
  }

  if (lctrCisFtIsListEmpty(pList))
  {
    /* List is empty. */
    pList->pHead = pList->pTail = pTemp;
  }
  else
  {
    /* List is not empty. */
    pTemp->pNext = pList->pHead;
    pList->pHead = pTemp;
  }
  pList->numNodes++;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Insert at the tail of the list.
 *
 *  \param      pList     Pointer to the list.
 *  \param      pFtParam  Flush timeout parameters.
 *
 *  \return     True if successfully insert at the head, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisFtInsertTail(lctrFtParamList_t *pList, lctrFtParam_t *pFtParam)
{
  lctrFtParamNode_t *pTemp = NULL;

  if ((pTemp = lctrCisFtCreateFtParamNode(pFtParam)) == NULL)
  {
    return FALSE;
  }

  if (lctrCisFtIsListEmpty(pList))
  {
    /* List is empty. */
    pList->pHead = pList->pTail = pTemp;
  }
  else
  {
    /* List is not empty. */
    pList->pTail->pNext = pTemp;
    pList->pTail = pTemp;
  }
  pList->numNodes++;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Remove at the head of the list.
 *
 *  \param      pList     Pointer to the list.
 *
 *  \return     True if successfully remove at the head, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisFtRemoveHead(lctrFtParamList_t *pList)
{
  lctrFtParamNode_t *pTemp = NULL;

  if (lctrCisFtIsListEmpty(pList))
  {
    /* List is empty. */
    return FALSE;
  }
  else
  {
    /* List is not empty. */
    pTemp = pList->pHead;
    pList->pHead = pTemp->pNext;
    WsfBufFree(pTemp);
  }
  pList->numNodes--;

  if (lctrCisFtIsListEmpty(pList))
  {
    pList->pTail = NULL;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Remove at the tail of the list.
 *
 *  \param      pList     Pointer to the list.
 *
 *  \return     True if successfully remove at the tail, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisFtRemoveTail(lctrFtParamList_t *pList)
{
  lctrFtParamNode_t *pTemp = NULL;

  if (lctrCisFtIsListEmpty(pList))
  {
    /* List is empty. */
    return FALSE;
  }
  else if (pList->numNodes == 1)
  {
    /* List has only one node. */
    pTemp = pList->pTail;
    WsfBufFree(pTemp);
    pList->pHead = pList->pTail = NULL;
  }
  else
  {
    /* List has more than one node. */
    pTemp = pList->pHead;

    /* Traverse to the second last. */
    while (pTemp->pNext->pNext != NULL)
    {
      pTemp = pTemp->pNext;
    }

    WsfBufFree(pTemp->pNext); /* Free the tail. */
    pTemp->pNext = NULL;
    pList->pTail = pTemp;
  }
  pList->numNodes--;

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Clear the flush timeout list.
 *
 *  \param      pList     Pointer to the list.
 */
/*************************************************************************************************/
void lctrCisFtListClear(lctrFtParamList_t *pList)
{
  if (lctrCisFtIsListEmpty(pList))
  {
    /* List is empty. */
    return;
  }

  lctrFtParamNode_t *pTemp = pList->pHead;

  while (pTemp)
  {
    lctrCisFtRemoveHead(pList);
    pTemp = pTemp->pNext;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Check if the list is empty or not.
 *
 *  \param      pList     Pointer to the list.
 *
 *  \return     True if successfully remove at the tail, False otherwise.
 */
/*************************************************************************************************/
bool_t lctrCisFtIsListEmpty(lctrFtParamList_t *pList)
{
  return pList->numNodes == 0 ? TRUE : FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Set flags for CIS link termination.
 *
 *  \param  aclHandle    ACL connection handle.
 *
 *  \return TRUE if CIS is terminated, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrCheckForCisLinkTerm(uint16_t aclHandle)
{
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(aclHandle);
  lctrCisCtx_t *pCisCtx = lctrFindCisByHandle(pCtx->llcpCisHandle);

  if (pCisCtx == NULL)
  {
    return FALSE;
  }

  if (pCisCtx->termState == LCTR_CIS_TERM_STATE_TERMINATING)
  {
    /* Peer device is LL_TERMINATE_IND initiator. */
    if (pCtx->termAckReqd)                  /* Tx Ack required after Rx of LL_TERMINATE_IND */
    {
      pCtx->termAckReqd = FALSE;

      if (pCtx->ackAfterCtrlPdu)            /*     guarantee Ack Tx'ed */
      {
        lctrSendCisLlcpMsg(pCisCtx, LCTR_CIS_TERM_EVENT_CIS_TERM);
        return TRUE;
      }
    }
    /* Local device is LL_CIS_TERMINATE_IND initiator. */
    else if ((pCtx->llcpState == LCTR_LLCP_STATE_IDLE) || /* LL_TERMINATE_IND not pending */
             (pCtx->txArqQ.pHead == NULL))                /* guarantee LL_TERMINATE_IND is Ack'ed */
                                                          /*     i.e. "WsfQueueEmpty(&pCtx->txArqQ)" (optimized for ISR) */
    {
      lctrSendCisLlcpMsg(pCisCtx, LCTR_CIS_TERM_EVENT_CIS_TERM);
      return TRUE;
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Check if there is a CIS established for the ACL indicated by the aclHandle.
 *
 *  \param  aclHandle    ACL connection handle.
 *
 *  \return TRUE if there is a CIS established, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrCheckIsCisEstAcl(uint16_t aclHandle)
{
  for (uint16_t i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if ((pCisCtx->enabled == TRUE) &&
        (pCisCtx->aclHandle == aclHandle) &&
        lctrIsCisEst(pCisCtx))

    {
      return TRUE;
    }
  }
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Check if CIS is established indicated by the cisHandle.
 *
 *  \param  cisHandle    CIS connection handle.
 *
 *  \return TRUE if CIS is established, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lctrCheckIsCisEstCis(uint16_t cisHandle)
{
  lctrCisCtx_t *pCisCtx = lctrFindCisByHandle(cisHandle);

  if ((pCisCtx->enabled == TRUE) &&
      lctrIsCisEst(pCisCtx))
  {
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Start LLCP timer.
 *
 *  \param      pCtx    ACL Connection context.
 *  \param      pCisCtx CIS Context.
 */
/*************************************************************************************************/
void lctrCisStartLlcpTimer(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  if (lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_ENA_LLCP_TIMER))
  {
    WsfTimerStartSec(&pCisCtx->tmrProcRsp, LL_T_PRT_SEC);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Stop LLCP timer.
 *
 *  \param      pCtx    ACL Connection context.
 *  \param      pCisCtx CIS Context.
 */
/*************************************************************************************************/
void lctrCisStopLlcpTimer(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  if (lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_ENA_LLCP_TIMER))
  {
    WsfTimerStop(&pCisCtx->tmrProcRsp);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Compute the duration in microseconds of an subevent for sequential packing.
 *
 *  \param      phyMToS     PHY master to slave.
 *  \param      phySToM     PHY slave to master.
 *  \param      plMToS      Payload master to slave.
 *  \param      plSToM      Payload master to slave.
 *
 *  \return     Time in microseconds for the packet to be transferred on the medium.
 */
/*************************************************************************************************/
uint32_t lctrCisCalcSubEvtDurationUsecSeq(uint8_t phyMToS, uint8_t phySToM, uint8_t plMToS, uint8_t plSToM)
{
  uint32_t duration = 0;

  /* MIC is always included since the scheduling is done when a CIS in the CIG is enabled for the first time
   * and the encryption mode of other CISs in the CIG are unknown. */
  switch (phyMToS)
  {
    case BB_PHY_BLE_1M:
      duration += LL_DATA_LEN_TO_TIME_1M(plMToS, TRUE);
      break;
    case BB_PHY_BLE_2M:
      duration += LL_DATA_LEN_TO_TIME_2M(plMToS, TRUE);
      break;
    case BB_PHY_BLE_CODED:
      if (lmgrGetOpFlag(LL_OP_MODE_FLAG_FORCE_CIS_CODED_PHY_S2))
      {
        duration += LL_DATA_LEN_TO_TIME_CODED_S2(plMToS, TRUE);
      }
      else
      {
        duration += LL_DATA_LEN_TO_TIME_CODED_S8(plMToS, TRUE);
      }
      break;
  }

  duration += LL_BLE_TIFS_US;

  /* MIC is always included since the scheduling is done when a CIS in the CIG is enabled for the first time
   * and the encryption mode of other CISs in the CIG are unknown. */
  switch (phySToM)
  {
    case BB_PHY_BLE_1M:
      duration += LL_DATA_LEN_TO_TIME_1M(plSToM, TRUE);
      break;
    case BB_PHY_BLE_2M:
      duration += LL_DATA_LEN_TO_TIME_2M(plSToM, TRUE);
      break;
    case BB_PHY_BLE_CODED:
      if (lmgrGetOpFlag(LL_OP_MODE_FLAG_FORCE_CIS_CODED_PHY_S2))
      {
        duration += LL_DATA_LEN_TO_TIME_CODED_S2(plSToM, TRUE);
      }
      else
      {
        duration += LL_DATA_LEN_TO_TIME_CODED_S8(plSToM, TRUE);
      }
      break;
  }

  duration += LL_BLE_TMSS_US;

  duration += pLctrRtCfg->cisSubEvtSpaceDelay;   /* Add runtime config delay. */

  return duration;
}

/*************************************************************************************************/
/*!
 *  \brief      Compute the duration in microseconds of an sub event for interleaved packing.
 *
 *  \param      pSetCigParam  CIG parameters.
 *
 *  \return     Time in microseconds for the packet to be transferred on the medium.
 *
 *  \note       This value includes preamble, access address, CRC and T_IFS.
 */
/*************************************************************************************************/
uint32_t lctrCisCalcSubEvtDurationUsecInter(LlCisCigParams_t *pSetCigParam)
{
  uint32_t duration = 0;

  lctrCisCtx_t *pCisCtx;

  for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
  {
    pCisCtx = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[i].cisId);

    duration += lctrCisCalcSubEvtDurationUsecSeq(pCisCtx->phyMToS, pCisCtx->phySToM,
                                                 pCisCtx->localDataPdu.maxTxLen, pCisCtx->localDataPdu.maxRxLen);
  }
  return duration;
}

/*************************************************************************************************/
/*!
 *  \brief      Set default values for CIS.
 */
/*************************************************************************************************/
void lctrCisDefaults(void)
{
  memset(pLctrCisTbl, 0, sizeof(lctrCisCtx_t) * pLctrRtCfg->maxCis);
  memset(pLctrCigTbl, 0, sizeof(lctrCigCtx_t) * pLctrRtCfg->maxCig);
}

/*************************************************************************************************/
/*!
 *  \brief      Generate and send a test payload.
 *
 *  \param     pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisTxTestPayloadHandler(lctrCisCtx_t * pCisCtx)
{
  uint16_t maxSdu = (pCisCtx->role == LL_ROLE_MASTER) ? pCisCtx->sduSizeMToS : pCisCtx->sduSizeSToM;
  uint8_t * pIsoBuf;
  uint32_t pktCtr;
  if ((pCisCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED) && (pCisCtx->testTxPktCtr == 0))
  {
    pCisCtx->testTxPktCtr = pCisCtx->txPktCounter;
  }

  pktCtr = pCisCtx->testTxPktCtr++;
  pIsoBuf = lctrGenerateIsoTestData(pCisCtx->cisHandle, pCisCtx->testPldType, maxSdu, pktCtr);

  if (lmgrIsoCb.availTxBuf == 0)
  {
    LL_TRACE_WARN0("!!! ISO flow control detected; dropping Tx data PDU");

    /* Drop packet due to out of Tx buffers condition. */
    WsfMsgFree(pIsoBuf);
    pIsoBuf = NULL; /* Returned below. */
  }

  if (pIsoBuf == NULL)
  {
    /* Restore last payload counter since packet could not be generated. */
    pCisCtx->testTxPktCtr--;
    return;
  }

  LctrTxIso(pIsoBuf);
}

/*************************************************************************************************/
/*!
 *  \brief     Generate a lost/invalid SDU in case of flushed SDU.
 *
 *  \param     pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisCheckUnframedFlush(lctrCisCtx_t *pCisCtx)
{
  /*** Send up error SDU ***/
  uint8_t handlerId;
  uint8_t *pSdu;
  uint8_t numHandles = 0;
  uint16_t handles[1] = { pCisCtx->cisHandle };
  uint16_t numSdu[1] = { 0 };

  /* NULL signals lost PDU; release previously stored fragments. */
  if (lctrRecombineRxUnframedSdu(&pCisCtx->isoalRxCtx, NULL))
  {
    while ((pSdu = WsfMsgDeq(&pCisCtx->isoalRxCtx.data.unframed.pendSduQ, &handlerId)) != NULL)
    {
      /* Enqueue SDU for processing. */
      if (!lctrIsoRxConnEnq(&pCisCtx->dataPathOutCtx, pCisCtx->cisHandle,
                            pCisCtx->rxPktCounter, pSdu))
      {
        /* The buffer was not freed, so free it now. */
        WsfMsgFree(pSdu);
      }

      numHandles = 1;
      numSdu[0]++;
    }
  }
  else
  {
    if (lmgrGetOpFlag(LL_OP_MODE_FLAG_ENA_ISO_LOST_NOTIFY) &&
       (((pCisCtx->role == LL_ROLE_SLAVE) && (pCisCtx->data.slv.rxFromMaster == FALSE)) ||
        ((pCisCtx->role == LL_ROLE_MASTER) && (pCisCtx->data.mst.rxFromSlave == FALSE))))
    {
      /* No prior fragments; send lost SDU. */
      if ((pSdu = lctrRxSduAlloc()) != NULL)
      {
        /* TODO: This code path increases this twice: once in lctrRxSduAlloc and lctrIsoRxConnEnq.
         * Change to only increase once.
         */
        lctrIsoDataRxIncAvailBuf(1);

        lctrIsoHdr_t isoHdr =
        {
          /* ISO header */
          .handle = pCisCtx->cisHandle,
          .pb     = LCTR_PB_COMP,
          .tsFlag = FALSE,
          .len    = 0,

          /* Data load */
          .ts     = 0,
          .pktSn  = pCisCtx->rxPktCounter,
          .sduLen = 0,
          .ps     = LCTR_PS_LOST
        };

        lctrIsoPackHdr(pSdu, &isoHdr);

        /* Enqueue SDU for processing. */
        if (!lctrIsoRxConnEnq(&pCisCtx->dataPathOutCtx, pCisCtx->cisHandle,
                              pCisCtx->rxPktCounter, pSdu))
        {
          /* The buffer was not freed, so free it now. */
          WsfMsgFree(pSdu);
        }
        numHandles = 1;
        numSdu[0]++;
      }
      else
      {
        LL_TRACE_WARN0("SDU Could not be allocated.");
      }
    }
  }

  if (lmgrPersistCb.recvIsoPendCback && numHandles)
  {
    /* Notify host received SDUs. */
    lmgrPersistCb.recvIsoPendCback(numHandles, handles, numSdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Service function for CIS power control.
 *
 *  \param      pConnCtx  Connection context.
 *
*************************************************************************************************/
void lctrCisServicePowerMonitor(lctrConnCtx_t *pConnCtx)
{
  if (!(pConnCtx->usedFeatSet & LL_FEAT_POWER_CONTROL_REQUEST) ||
      lmgrGetOpFlag(LL_OP_MODE_FLAG_DIS_POWER_MONITOR))
  {
    pConnCtx->monitoringState = LCTR_PC_MONITOR_DISABLED;
    return;
  }

  int8_t sendReqDelta = 0;
  lctrCisCtx_t *pCisCtx = NULL;

  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    pCisCtx = &pLctrCisTbl[i];

    if ((pCisCtx->aclHandle == LCTR_GET_CONN_HANDLE(pConnCtx)) &&
        (pCisCtx->enabled) &&  (pCisCtx->connEst))
    {
      if (((pCisCtx->role == LL_ROLE_SLAVE) && (pCisCtx->phyMToS == pConnCtx->bleData.chan.rxPhy)) ||
          ((pCisCtx->role == LL_ROLE_MASTER) && (pCisCtx->phySToM == pConnCtx->bleData.chan.rxPhy)))
      {
        /* This power is already managed by ACL, so no need to run management on CIS. */
        return;
      }

      goto CisManageRssiPower;
    }
  }
  return;

CisManageRssiPower:

  lctrRssiAddAveragePoint(&pConnCtx->cisRunAvg, -((int8_t) (pConnCtx->cisAccumulatedRssi / pConnCtx->cisTotalAccumulatedRssi)));
  pConnCtx->cisAccumulatedRssi = 0;
  pConnCtx->cisTotalAccumulatedRssi = 0;

  if (pConnCtx->cisRunAvg.avgCount >= LL_PC_TBL_LEN)
  {
    int8_t averageRunning = lctrRssiGetAverage(&pConnCtx->cisRunAvg);

    if ((averageRunning > pConnCtx->pclMonitorParam.autoMonitor.highThreshold) &&
        !(pConnCtx->peerPwrLimits & LL_PWR_CONTROL_LIMIT_MIN_BIT))
    {
      sendReqDelta = -(pConnCtx->pclMonitorParam.autoMonitor.requestVal);
    }
    else if ((averageRunning < pConnCtx->pclMonitorParam.autoMonitor.lowThreshold) &&
             !(pConnCtx->peerPwrLimits & LL_PWR_CONTROL_LIMIT_MAX_BIT))
    {
      sendReqDelta = pConnCtx->pclMonitorParam.autoMonitor.requestVal;
    }
  }

  if (sendReqDelta != 0)
  {
    if ((pConnCtx->llcpActiveProc == LCTR_PROC_PWR_CTRL) ||
        (pConnCtx->llcpPendMask & (1 << LCTR_PROC_PWR_CTRL)))
    {
      LL_TRACE_WARN0("Power control LLCP already ongoing or pended upon attempt to start new power control process.");
      return;
    }

    lctrMsgPwrCtrlReq_t *pMsg;
    uint8_t phy = ((pCisCtx->role == LL_ROLE_MASTER) ? pCisCtx->phySToM : pCisCtx->phyMToS);

    /* Decide to request S8 or S2 PHY. */
    phy += (((phy == LL_PHY_LE_CODED) &&
            (pCisCtx->bleData.chan.initTxPhyOptions == LL_PHY_OPTIONS_S2_PREFERRED)) ? 1 : 0);

    if ((pMsg = (lctrMsgPwrCtrlReq_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
    {
      pMsg->hdr.handle = LCTR_GET_CONN_HANDLE(pConnCtx);
      pMsg->hdr.dispId = LCTR_DISP_CONN;
      pMsg->hdr.event  = LCTR_CONN_MSG_API_PWR_CTRL_REQ;
      pMsg->delta      = sendReqDelta;
      pMsg->phy        = phy;

      WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
    }

    pConnCtx->cisRunAvg.avgCount = 0;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Cleanup CIG contexts.
 */
/*************************************************************************************************/
void lctrCleanupCigCtx()
{
  lctrCigCtx_t *pCigCtx;
  for (uint8_t i = 0; i < pLctrRtCfg->maxCig; i++)
  {
    pCigCtx = &pLctrCigTbl[i];
    if (pCigCtx->enabled)
    {
      if (pCigCtx->numCisEsted == 0)
      {
        lctrFreeCigCtx(pCigCtx);
      }
      else
      {
        SchRemove(&pCigCtx->cigBod);
      }
    }
  }
}
