/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller connected isochronous stream master operation builder implementation file.
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
#include "lctr_int_cis_master.h"
#include "lctr_int_cis_slave.h"
#include "lctr_int_conn_master.h"
#include "lctr_int.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "bb_api.h"
#include "pal_bb.h"
#include "wsf_assert.h"
#include "wsf_buf.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Globals
**************************************************************************************************/

/*! \brief      Create CIS pending parameters. */
lctrCreateCisPend_t lctrMstCreateCisPend;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/
extern uint8_t lctrChoosePreferredPhy(uint8_t val);

/**************************************************************************************************
  Internal functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      CIS master reset handler.
 */
/*************************************************************************************************/
static void lctrCisMstResetHandler(void)
{
  BbBleCisSlaveInit();
  BbBleCisMasterInit();
  lctrCisDefaults();
  LmgrMstCisInit();
}

/*************************************************************************************************/
/*!
 *  \brief      Master Tx data pending task event handler.
 */
/*************************************************************************************************/
static void lctrIsoTxPendingHandler(void)
{
  unsigned int i;

  for (i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if ((pCisCtx->enabled) &&
        !WsfQueueEmpty(&pCisCtx->txArqQ))
    {
      lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
      SchReload(&pCigCtx->cigBod);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Host channel class update handler for CIS master.
 *
 *  \param  chanMap     Updated channel map.
 *
 *  \return Status code.
 */
/*************************************************************************************************/
static uint8_t lctrMstCisChClassUpdate(uint64_t chanMap)
{
  uint16_t handle;

  /* Update for connections */
  for (handle = 0; handle < pLctrRtCfg->maxConn; handle++)
  {
    if ((LctrIsConnHandleEnabled(handle)) &&
        (LctrGetRole(handle) == LL_ROLE_MASTER))
    {
      /* Update the channel map for CIS master as well. */
      LctrCisUpdateChanMap(handle);
    }
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Check whether the parameters for the CIG can be scheduled or not.
 *
 *  \param      pCigCtx   CIG context.
 *
 *  \return     TRUE if schedule is successful, otherwise FALSE.
 */
/*************************************************************************************************/
static bool_t lctrCisMstCheckCigParams(lctrCigCtx_t *pCigCtx)
{
  /* Check whether the CIG could be scheduled within the interval. */
  if (pCigCtx->cigSyncDelayUsec >= (uint32_t)(LCTR_ISO_INT_TO_US(pCigCtx->isoInterval) - LL_BLE_TMSS_US))
  {
    return FALSE;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Compute the duration in microseconds of an sub event for interleaved packing for
 *              testing purpose.
 *
 *  \param      pSetCigParamTest  CIG parameter for test.
 *
 *  \return     Time in microseconds for the packet to be transferred on the medium.
 *
 *  \note       This value includes preamble, access address, CRC and T_IFS.
 */
/*************************************************************************************************/
static uint32_t lctrCalcSubEvtDurationUsecInterTest(LlCisCigParamsTest_t *pSetCigParamTest)
{
  uint32_t duration = 0;

  lctrCisCtx_t *pCisCtx;

  for (unsigned int i = 0; i < pSetCigParamTest->numCis; i++)
  {
    pCisCtx = lctrFindCisById(pSetCigParamTest->cigId, pSetCigParamTest->pCisParam[i].cisId);

    duration += lctrCisCalcSubEvtDurationUsecSeq(pCisCtx->phyMToS, pCisCtx->phySToM,
                                                 pCisCtx->localDataPdu.maxTxLen, pCisCtx->localDataPdu.maxRxLen);
  }
  return duration;
}

/*************************************************************************************************/
/*!
 *  \brief      Set up a new CIS or update an existing CIS.
 *
 *  \param      pCisCtx           CIS context.
 *  \param      pSetCigParam      CIG parameters.
 *  \param      pCisParam         CIS parameters.
 */
/*************************************************************************************************/
static void lctrSetCis(lctrCisCtx_t *pCisCtx, LlCisCigParams_t *pSetCigParam, LlCisCisParams_t *pCisParam)
{
  pCisCtx->cisId = pCisParam->cisId;
  pCisCtx->sca = pSetCigParam->sca;
  pCisCtx->packing = pSetCigParam->packing;
  pCisCtx->framing = pSetCigParam->framing;
  pCisCtx->accessAddr = lctrComputeAccessAddr();

  if (pCisCtx->framing == LL_ISO_PDU_TYPE_UNFRAMED)
  {
    if (pSetCigParam->sduIntervalMToS <= 1250)
    {
      pSetCigParam->sduIntervalMToS = 1250;
    }
    if (pSetCigParam->sduIntervalSToM <= 1250)
    {
      pSetCigParam->sduIntervalSToM = 1250;
    }

    pCisCtx->isoInterval = LL_MATH_DIV_1250(pSetCigParam->sduIntervalMToS);    /* Make sure PDU interval the same as the SDU interval. Assume sduIntervalMToS equals sduIntervalSToM */
    pCisCtx->sduIntervalMToS = pSetCigParam->sduIntervalMToS;
    pCisCtx->sduIntervalSToM = pSetCigParam->sduIntervalSToM;
    pCisCtx->localDataPdu.maxTxLen = WSF_MIN(pCisParam->sduSizeMToS, pLctrRtCfg->maxIsoPduLen);
    pCisCtx->localDataPdu.maxRxLen = WSF_MIN(pCisParam->sduSizeSToM, pLctrRtCfg->maxIsoPduLen);
    pCisCtx->sduSizeMToS = pCisParam->sduSizeMToS;
    pCisCtx->sduSizeSToM = pCisParam->sduSizeSToM;
    pCisCtx->phyMToS = lctrPhysBitToPhy(lctrChoosePreferredPhy(pCisParam->phyMToS));
    pCisCtx->phySToM = lctrPhysBitToPhy(lctrChoosePreferredPhy(pCisParam->phySToM));
    pCisCtx->ftMToS = LlMathDivideUint32(pSetCigParam->transLatMToS * 1000, LCTR_ISO_INT_TO_US(pCisCtx->isoInterval));
    pCisCtx->ftSToM = LlMathDivideUint32(pSetCigParam->transLatSToM * 1000, LCTR_ISO_INT_TO_US(pCisCtx->isoInterval));

    if (pCisCtx->localDataPdu.maxTxLen == 0)
    {
      pCisCtx->bnMToS = 0;
    }
    else if (pCisCtx->sduSizeMToS / pCisCtx->localDataPdu.maxTxLen * pCisCtx->localDataPdu.maxTxLen < pCisCtx->sduSizeMToS)
    {
      pCisCtx->bnMToS = pCisCtx->sduSizeMToS / pCisCtx->localDataPdu.maxTxLen + 1;
    }
    else
    {
      pCisCtx->bnMToS = pCisCtx->sduSizeMToS / pCisCtx->localDataPdu.maxTxLen;
    }

    if (pCisCtx->localDataPdu.maxRxLen == 0)
    {
      pCisCtx->bnSToM = 0;
    }
    else if (pCisCtx->sduSizeSToM / pCisCtx->localDataPdu.maxRxLen * pCisCtx->localDataPdu.maxRxLen < pCisCtx->sduSizeSToM)
    {
      pCisCtx->bnSToM = pCisCtx->sduSizeSToM / pCisCtx->localDataPdu.maxRxLen + 1;
    }
    else
    {
      pCisCtx->bnSToM = pCisCtx->sduSizeSToM / pCisCtx->localDataPdu.maxRxLen;
    }

    pCisCtx->nse = WSF_MAX(pCisCtx->bnSToM, pCisCtx->bnMToS) + WSF_MAX(pCisParam->rteMToS, pCisParam->rteSToM);

    LL_TRACE_INFO1("lctrSetCis, maxTxLen=%d", pCisCtx->localDataPdu.maxTxLen);
    LL_TRACE_INFO1("lctrSetCis, sduMToS=%d", pCisCtx->sduSizeMToS);
    LL_TRACE_INFO1("lctrSetCis, bnMToS=%d", pCisCtx->bnMToS);

  }
  else /* LL_ISO_PDU_TYPE_FRAMED */
  {
    if (pSetCigParam->sduIntervalMToS <= 1250)
    {
      pSetCigParam->sduIntervalMToS = 1250;
    }
    if (pSetCigParam->sduIntervalSToM <= 1250)
    {
      pSetCigParam->sduIntervalSToM = 1250;
    }

    pCisCtx->isoInterval = LL_MATH_DIV_1250(pSetCigParam->sduIntervalMToS);    /* Make sure PDU interval the same as the SDU interval. Assume sduIntervalMToS equals sduIntervalSToM */
    pCisCtx->sduIntervalMToS = pSetCigParam->sduIntervalMToS;
    pCisCtx->sduIntervalSToM = pSetCigParam->sduIntervalSToM;
    pCisCtx->localDataPdu.maxTxLen = WSF_MIN(pCisParam->sduSizeMToS, pLctrRtCfg->maxIsoPduLen);
    pCisCtx->localDataPdu.maxRxLen = WSF_MIN(pCisParam->sduSizeSToM, pLctrRtCfg->maxIsoPduLen);
    pCisCtx->sduSizeMToS = pCisParam->sduSizeMToS;
    pCisCtx->sduSizeSToM = pCisParam->sduSizeSToM;
    pCisCtx->phyMToS = lctrPhysBitToPhy(lctrChoosePreferredPhy(pCisParam->phyMToS));
    pCisCtx->phySToM = lctrPhysBitToPhy(lctrChoosePreferredPhy(pCisParam->phySToM));
    pCisCtx->ftMToS = LlMathDivideUint32(pSetCigParam->transLatMToS * 1000, LCTR_ISO_INT_TO_US(pCisCtx->isoInterval));
    pCisCtx->ftSToM = LlMathDivideUint32(pSetCigParam->transLatSToM * 1000, LCTR_ISO_INT_TO_US(pCisCtx->isoInterval));

    if (pCisCtx->localDataPdu.maxTxLen == 0)
    {
      pCisCtx->bnMToS = 0;
    }
    else if (pCisCtx->sduSizeMToS / pCisCtx->localDataPdu.maxTxLen * pCisCtx->localDataPdu.maxTxLen < pCisCtx->sduSizeMToS)
    {
      pCisCtx->bnMToS = pCisCtx->sduSizeMToS / pCisCtx->localDataPdu.maxTxLen + 1;
    }
    else
    {
      pCisCtx->bnMToS = pCisCtx->sduSizeMToS / pCisCtx->localDataPdu.maxTxLen;
    }

    if (pCisCtx->localDataPdu.maxRxLen == 0)
    {
      pCisCtx->bnSToM = 0;
    }
    else if (pCisCtx->sduSizeSToM / pCisCtx->localDataPdu.maxRxLen * pCisCtx->localDataPdu.maxRxLen < pCisCtx->sduSizeSToM)
    {
      pCisCtx->bnSToM = pCisCtx->sduSizeSToM / pCisCtx->localDataPdu.maxRxLen + 1;
    }
    else
    {
      pCisCtx->bnSToM = pCisCtx->sduSizeSToM / pCisCtx->localDataPdu.maxRxLen;
    }

    pCisCtx->nse = WSF_MAX(pCisCtx->bnSToM, pCisCtx->bnMToS) + WSF_MAX(pCisParam->rteMToS, pCisParam->rteSToM);

    LL_TRACE_INFO1("lctrSetCis, maxTxLen=%d", pCisCtx->localDataPdu.maxTxLen);
    LL_TRACE_INFO1("lctrSetCis, sduMToS=%d", pCisCtx->sduSizeMToS);
    LL_TRACE_INFO1("lctrSetCis, bnMToS=%d", pCisCtx->bnMToS);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set up a new CIG.
 *
 *  \param      pCigCtx           CIG context.
 *  \param      pSetCigParam      CIG parameters.
 */
/*************************************************************************************************/
static void lctrSetCig(lctrCigCtx_t *pCigCtx, LlCisCigParams_t *pSetCigParam)
{
  lctrCisCtx_t *pCisCtx, *pCisCtxTemp, *pCisCtxFirst;

  /* Calculate subevent interval and offset for each CIS. */
  for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
  {
    pCisCtx = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[i].cisId);

    WSF_ASSERT(pCisCtx);

    if (pSetCigParam->packing == LL_PACKING_INTERLEAVED)
    {
      pCisCtx->subIntervUsec = lctrCisCalcSubEvtDurationUsecInter(pSetCigParam);
      pCisCtx->delayUsec = lctrCisCalcSubEvtDurationUsecSeq(pCisCtx->phyMToS, pCisCtx->phySToM,
                                                            pCisCtx->localDataPdu.maxTxLen, pCisCtx->localDataPdu.maxRxLen);
    }
    else
    {
      /* LL_PACKING_SEQUENTIAL */
      pCisCtx->subIntervUsec = lctrCisCalcSubEvtDurationUsecSeq(pCisCtx->phyMToS, pCisCtx->phySToM,
                                                                pCisCtx->localDataPdu.maxTxLen, pCisCtx->localDataPdu.maxRxLen);
      pCisCtx->delayUsec = pCisCtx->subIntervUsec;
    }

    /* The formula is (2 * CIS_interval * (master_sca + slave_sca) / 1000000) */
    WSF_ASSERT((uint64_t)pCisCtx->subIntervUsec > WSF_MIN(LL_BLE_TMSS_US, LL_MATH_DIV_10E6(2 * LCTR_ISO_INT_TO_US(pCisCtx->isoInterval) * ((uint64_t)lctrCalcTotalAccuracy(pSetCigParam->sca)))));
    LL_TRACE_INFO1("lctrSetCig,     subIntervUsec=%d",      pCisCtx->subIntervUsec);
    LL_TRACE_INFO1("lctrSetCig,     delayUsec=%d",          pCisCtx->delayUsec);
  }

  /* Calculate CIG sync delay and CIS sync delay. */
  if (pSetCigParam->packing == LL_PACKING_INTERLEAVED)
  {
    pCisCtxFirst = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[0].cisId);
    WSF_ASSERT(pCisCtxFirst);

    pCigCtx->cigSyncDelayUsec = pCisCtxFirst->cigSyncDelayUsec = pCisCtxFirst->cisSyncDelayUsec = pCisCtxFirst->subIntervUsec * pCisCtxFirst->nse;
    pCigCtx->isoInterval = pCisCtxFirst->isoInterval;    /* CISs have the same ISO interval and it is the same as CIG. */

    for (unsigned int i = 1; i < pSetCigParam->numCis; i++)
    {
      pCisCtx = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[i].cisId);

      WSF_ASSERT(pCisCtx);

      uint32_t durationUs = 0;

      for (unsigned int j = 0; j < i; j++)
      {
        pCisCtxTemp = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[j].cisId);
        durationUs += lctrCisCalcSubEvtDurationUsecSeq(pCisCtxTemp->phyMToS, pCisCtxTemp->phySToM,
                                                       pCisCtxTemp->localDataPdu.maxTxLen, pCisCtxTemp->localDataPdu.maxRxLen);
      }
      pCisCtx->cisSyncDelayUsec = pCisCtxFirst->cisSyncDelayUsec - durationUs;
      pCisCtx->cigSyncDelayUsec = pCisCtxFirst->cisSyncDelayUsec;   /* CIG_SYNC_DELAY = CIS_SYNC_DELAY[0] */
    }
  }
  else
  {
    /* LL_PACKING_SEQUENTIAL */
    pCisCtxFirst = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[0].cisId);

    WSF_ASSERT(pCisCtxFirst);

    for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
    {
      pCisCtx = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[i].cisId);

      WSF_ASSERT(pCisCtx);
      pCisCtx->cisSyncDelayUsec = 0;

      for (unsigned int j = i; j < pSetCigParam->numCis; j++)
      {
        pCisCtxTemp = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[j].cisId);

        pCisCtx->cisSyncDelayUsec += (pCisCtxTemp->subIntervUsec * pCisCtxTemp->nse);
      }

      pCisCtx->cigSyncDelayUsec = pCisCtxFirst->cisSyncDelayUsec;   /* CIG_SYNC_DELAY = CIS_SYNC_DELAY[0] */
      LL_TRACE_INFO1("LL_PACKING_SEQUENTIAL, cisSyncDelayUsec=%d", pCisCtx->cisSyncDelayUsec);
    }
    pCigCtx->cigSyncDelayUsec = pCisCtxFirst->cisSyncDelayUsec;    /* CIG_SYNC_DELAY = CIS_SYNC_DELAY[0] */
    pCigCtx->isoInterval = pCisCtxFirst->isoInterval;              /* CISs have the same ISO interval and it is the same as CIG. */
  }

  pCigCtx->packing = pSetCigParam->packing;
  pCigCtx->isValid = lctrCisMstCheckCigParams(pCigCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Set up a new CIG for testing purpose.
 *
 *  \param      pCigCtx           CIS context.
 *  \param      pSetCigParamTest  CIG test parameters.
 */
/*************************************************************************************************/
static void lctrSetCigTest(lctrCigCtx_t *pCigCtx, LlCisCigParamsTest_t *pSetCigParamTest)
{
  lctrCisCtx_t *pCisCtx, *pCisCtxTemp, *pCisCtxFirst;

  /* Calculate subevent interval for each CIS. */
  for (unsigned int i = 0; i < pSetCigParamTest->numCis; i++)
  {
    pCisCtx = lctrFindCisById(pSetCigParamTest->cigId, pSetCigParamTest->pCisParam[i].cisId);

    WSF_ASSERT(pCisCtx);

    if (pSetCigParamTest->packing == LL_PACKING_INTERLEAVED)
    {
      pCisCtx->subIntervUsec = lctrCalcSubEvtDurationUsecInterTest(pSetCigParamTest);
      pCisCtx->delayUsec = lctrCisCalcSubEvtDurationUsecSeq(pCisCtx->phyMToS, pCisCtx->phySToM,
                                                            pCisCtx->localDataPdu.maxTxLen, pCisCtx->localDataPdu.maxRxLen);
    }
    else
    {
      /* LL_PACKING_SEQUENTIAL */
      pCisCtx->subIntervUsec = lctrCisCalcSubEvtDurationUsecSeq(pCisCtx->phyMToS, pCisCtx->phySToM,
                                                                pCisCtx->localDataPdu.maxTxLen, pCisCtx->localDataPdu.maxRxLen);
      pCisCtx->delayUsec = pCisCtx->subIntervUsec;
    }

    /* The formula is (2 * CIS_interva *(master_sca + slave_sca) / 1000000) */
    WSF_ASSERT((uint64_t)pCisCtx->subIntervUsec > WSF_MIN(LL_BLE_TMSS_US, LL_MATH_DIV_10E6(2 * LCTR_ISO_INT_TO_US(pCisCtx->isoInterval) * ((uint64_t)lctrCalcTotalAccuracy(pSetCigParamTest->sca)))));
  }

  /* Calculate CIG sync delay and CIS sync delay. */
  if (pSetCigParamTest->packing == LL_PACKING_INTERLEAVED)
  {
    pCisCtxFirst = lctrFindCisById(pSetCigParamTest->cigId, pSetCigParamTest->pCisParam[0].cisId);
    WSF_ASSERT(pCisCtxFirst);

    pCigCtx->cigSyncDelayUsec = pCisCtxFirst->cigSyncDelayUsec = pCisCtxFirst->cisSyncDelayUsec = pCisCtxFirst->subIntervUsec * pCisCtxFirst->nse;
    pCigCtx->isoInterval = pCisCtxFirst->isoInterval;    /* CISs have the same ISO interval and it is the same as CIG. */

    for (unsigned int i = 1; i < pSetCigParamTest->numCis; i++)
    {
      pCisCtx = lctrFindCisById(pSetCigParamTest->cigId, pSetCigParamTest->pCisParam[i].cisId);

      WSF_ASSERT(pCisCtx);

      uint32_t durationUs = 0;

      for (unsigned int j = 0; j < i; j++)
      {
        pCisCtxTemp = lctrFindCisById(pSetCigParamTest->cigId, pSetCigParamTest->pCisParam[j].cisId);
        durationUs += lctrCisCalcSubEvtDurationUsecSeq(pCisCtxTemp->phyMToS, pCisCtxTemp->phySToM,
                                                       pCisCtxTemp->localDataPdu.maxTxLen, pCisCtxTemp->localDataPdu.maxRxLen);
      }
      pCisCtx->cisSyncDelayUsec = pCisCtxFirst->cisSyncDelayUsec - durationUs;
      pCisCtx->cigSyncDelayUsec = pCisCtxFirst->cisSyncDelayUsec;   /* CIG_SYNC_DELAY = CIS_SYNC_DELAY[0] */
    }
  }
  else
  {
    pCisCtxFirst = lctrFindCisById(pSetCigParamTest->cigId, pSetCigParamTest->pCisParam[0].cisId);

    WSF_ASSERT(pCisCtxFirst);

    /* LL_PACKING_SEQUENTIAL */
    for (unsigned int i = 0; i < pSetCigParamTest->numCis; i++)
    {
      pCisCtx = lctrFindCisById(pSetCigParamTest->cigId, pSetCigParamTest->pCisParam[i].cisId);

      WSF_ASSERT(pCisCtx);

      for (unsigned int j = i; j < pSetCigParamTest->numCis; j++)
      {
        pCisCtxTemp = lctrFindCisById(pSetCigParamTest->cigId, pSetCigParamTest->pCisParam[j].cisId);

        pCisCtx->cisSyncDelayUsec += (pCisCtxTemp->subIntervUsec * pCisCtxTemp->nse);
      }

      pCisCtx->cigSyncDelayUsec = pCisCtxFirst->cisSyncDelayUsec;   /* CIG_SYNC_DELAY = CIS_SYNC_DELAY[0] */
    }
    pCigCtx->cigSyncDelayUsec = pCisCtxFirst->cisSyncDelayUsec;             /* CIG_SYNC_DELAY = CIS_SYNC_DELAY[0] */
    pCigCtx->isoInterval = pCisCtxFirst->isoInterval;                       /* CISs have the same ISO interval and it is the same as CIG. */
  }

  pCigCtx->packing = pSetCigParamTest->packing;
  pCigCtx->isValid = lctrCisMstCheckCigParams(pCigCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Set up a new CIS or update an existing CIS for testing purpose.
 *
 *  \param      pCisCtx               CIS context.
 *  \param      pSetCigParamTest      CIG parameters.
 *  \param      pCigCisParamTest      CIS parameters.
 */
/*************************************************************************************************/
static void lctrSetCisTest(lctrCisCtx_t *pCisCtx, LlCisCigParamsTest_t *pSetCigParamTest,
                           LlCisCigCisParamsTest_t *pCigCisParamTest)
{
  pCisCtx->sduIntervalMToS = pSetCigParamTest->sduIntervalMToS;
  pCisCtx->sduIntervalSToM = pSetCigParamTest->sduIntervalSToM;
  pCisCtx->ftMToS = pSetCigParamTest->ftMToS;
  pCisCtx->ftSToM = pSetCigParamTest->ftSToM;
  pCisCtx->isoInterval = pSetCigParamTest->isoInterval;
  pCisCtx->sca = pSetCigParamTest->sca;
  pCisCtx->packing = pSetCigParamTest->packing;
  pCisCtx->framing = pSetCigParamTest->framing;

  pCisCtx->cisId = pCigCisParamTest->cisId;
  pCisCtx->nse = pCigCisParamTest->nse;
  pCisCtx->sduSizeMToS = pCigCisParamTest->sduSizeMToS;
  pCisCtx->sduSizeSToM = pCigCisParamTest->sduSizeSToM;
  pCisCtx->phyMToS = lctrPhysBitToPhy(lctrChoosePreferredPhy(pCigCisParamTest->phyMToS));
  pCisCtx->phySToM = lctrPhysBitToPhy(lctrChoosePreferredPhy(pCigCisParamTest->phySToM));
  pCisCtx->bnMToS = pCigCisParamTest->bnMToS;
  pCisCtx->bnSToM = pCigCisParamTest->bnSToM;

  pCisCtx->accessAddr = lctrComputeAccessAddr();
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);
  WSF_ASSERT(pCtx);
  pCisCtx->crcInit = pCtx->crcInit;
  pCisCtx->supTimeoutMs = pCtx->supTimeoutMs;

  pCisCtx->localDataPdu.maxTxLen = pCigCisParamTest->pduSizeMToS;
  pCisCtx->localDataPdu.maxRxLen = pCigCisParamTest->pduSizeSToM;
}

/*************************************************************************************************/
/*!
 *  \brief  Set CIG parameters.
 *
 *  \param  pSetCigParam    CIG parameters.
 *  \param  pCisHandles     Returned CIS handles.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrSetCigParam(LlCisCigParams_t *pSetCigParam, uint16_t *pCisHandles)
{
  lctrCigCtx_t *pCigCtx;

  if ((pCigCtx = lctrFindCigById(pSetCigParam->cigId)) == NULL)
  {
    if ((pCigCtx = lctrAllocCigCtx(pSetCigParam->cigId)) == NULL)
    {
      LL_TRACE_WARN0("LctrSetCigParam, there is no more CIG context");
      return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
    }
  }

  if (pCigCtx->numCisEsted > 0)
  {
    LL_TRACE_WARN0("LctrSetCigParam, there is an established CIS");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (lmgrCisMstCb.createCisPend == TRUE)
  {
    LL_TRACE_WARN0("LctrSetCigParam, there is a pending CIS");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  lctrCisCtx_t *pCisCtx;

  uint8_t numEnableCis = lctrGetNumEnabledCisCtx(pSetCigParam);

  if (numEnableCis == 0)
  {
    /* Set up new CIS contexts. */
    if (pSetCigParam->numCis > lctrGetNumAvailCisCtx())
    {
      LL_TRACE_WARN0("LctrSetCigParam, there is no more CIS context");
      return LL_ERROR_CODE_CONN_LIMIT_EXCEEDED;
    }
    else
    {
      for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
      {
        pCisCtx = lctrAllocCisCtx(pCigCtx);
        pCisCtx->role = LL_ROLE_MASTER;
        lctrSetCis(pCisCtx, pSetCigParam, &pSetCigParam->pCisParam[i]);
        pCisHandles[i] = pCisCtx->cisHandle;
      }
      lctrSetCig(pCigCtx, pSetCigParam);

      /* Invalid CIG parameters. Attempt adjusting of CIG parameters. */
      bool_t paramsChanged = TRUE;
      if (pCigCtx->isValid == FALSE)
      {
        LL_TRACE_WARN1("LctrSetCigParam, invalid parameters in cigID=%u; adjust parameters", pSetCigParam->cigId);
      }
      while ((pCigCtx->isValid == FALSE) && (paramsChanged == TRUE))
      {
        paramsChanged = FALSE;
        for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
        {
          /* Try to adjust CIS RTE to lower than host-reccomended value. */
          pCisCtx = lctrFindCisByHandle(pCisHandles[i]);
          if (pSetCigParam->pCisParam[i].rteMToS > 0)
          {
            pSetCigParam->pCisParam[i].rteMToS--;
            paramsChanged = TRUE;
          }
          if (pSetCigParam->pCisParam[i].rteSToM > 0)
          {
            pSetCigParam->pCisParam[i].rteSToM--;
            paramsChanged = TRUE;
          }

          /* Re-apply parameters. */
          lctrSetCis(pCisCtx, pSetCigParam, &pSetCigParam->pCisParam[i]);
          lctrSetCig(pCigCtx, pSetCigParam);
          if (pCigCtx->isValid)
          {
            break;
          }
        }
      }
    }
  }
  else
  {
    if ((pSetCigParam->numCis - numEnableCis) > lctrGetNumAvailCisCtx())
    {
      return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
    }
    else
    {
      for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
      {
        if ((pCisCtx = lctrFindCisById(pSetCigParam->cigId, pSetCigParam->pCisParam[i].cisId)) != NULL)
        {
          /* Update the context which is already set up. */
          lctrSetCis(pCisCtx, pSetCigParam, &pSetCigParam->pCisParam[i]);
        }
        else
        {
          /* Allocate new context, no allocation error, already checked. */
          pCisCtx = lctrAllocCisCtx(pCigCtx);
          pCisCtx->role = LL_ROLE_MASTER;
          lctrSetCis(pCisCtx, pSetCigParam, &pSetCigParam->pCisParam[i]);
        }
        pCisHandles[i] = pCisCtx->cisHandle;
      }
      lctrSetCig(pCigCtx, pSetCigParam);
    }

    /* Invalid CIG parameters. Attempt adjusting of CIG parameters. */
    bool_t paramsChanged = TRUE;
    if (pCigCtx->isValid == FALSE)
    {
      LL_TRACE_WARN1("LctrSetCigParam: invalid parameters in cigID=%u; adjust parameters", pSetCigParam->cigId);
    }
    while ((pCigCtx->isValid == FALSE) && (paramsChanged == TRUE))
    {
      paramsChanged = FALSE;
      for (unsigned int i = 0; i < pSetCigParam->numCis; i++)
      {
        /* Try to adjust CIS RTE to lower than host-reccomended value. */
        pCisCtx = lctrFindCisByHandle(pCisHandles[i]);
        if (pSetCigParam->pCisParam[i].rteMToS > 0)
        {
          pSetCigParam->pCisParam[i].rteMToS--;
          paramsChanged = TRUE;
        }
        if (pSetCigParam->pCisParam[i].rteSToM > 0)
        {
          pSetCigParam->pCisParam[i].rteSToM--;
          paramsChanged = TRUE;
        }

        /* Re-apply parameters. */
        lctrSetCis(pCisCtx, pSetCigParam, &pSetCigParam->pCisParam[i]);
        lctrSetCig(pCigCtx, pSetCigParam);
        if (pCigCtx->isValid)
        {
          break;
        }
      }
    }
  }

  pCigCtx->roleData.mst.numCis = pSetCigParam->numCis;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Set CIG parameters for testing purpose.
 *
 *  \param  pSetCigParamTest    CIG parameters for testing.
 *  \param  pCisHandles         Returned CIS handles.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrSetCigParamTest(LlCisCigParamsTest_t *pSetCigParamTest, uint16_t *pCisHandles)
{
  lctrCigCtx_t *pCigCtx;

  if ((pCigCtx = lctrFindCigById(pSetCigParamTest->cigId)) == NULL)
  {
    if ((pCigCtx = lctrAllocCigCtx(pSetCigParamTest->cigId)) == NULL)
    {
      LL_TRACE_WARN0("LctrSetCigParamTest, there is no more CIG context");
      return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
    }
  }

  if (pCigCtx->numCisEsted > 0)
  {
    LL_TRACE_WARN0("LctrSetCigParamTest, there is an established CIS");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  lctrCisCtx_t *pCisCtx;

  uint8_t numEnableCis = lctrGetNumEnabledCisCtxTest(pSetCigParamTest);

  if (numEnableCis == 0)
  {
    /* Set up new CIS contexts. */
    if (pSetCigParamTest->numCis > lctrGetNumAvailCisCtx())
    {
      LL_TRACE_WARN0("LctrSetCigParamTest, there is no more CIS context");
      return LL_ERROR_CODE_CONN_LIMIT_EXCEEDED;
    }
    else
    {
      for (unsigned int i = 0; i < pSetCigParamTest->numCis; i++)
      {
        pCisCtx = lctrAllocCisCtx(pCigCtx);
        pCisCtx->role = LL_ROLE_MASTER;
        lctrSetCisTest(pCisCtx, pSetCigParamTest, &pSetCigParamTest->pCisParam[i]);
        pCisHandles[i] = pCisCtx->cisHandle;
      }
      lctrSetCigTest(pCigCtx, pSetCigParamTest);
    }
  }
  else
  {
    if ((pSetCigParamTest->numCis - numEnableCis) > lctrGetNumAvailCisCtx())
    {
      return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
    }
    else
    {
      for (unsigned int i = 0; i < pSetCigParamTest->numCis; i++)
      {
        if ((pCisCtx = lctrFindCisById(pSetCigParamTest->cigId, pSetCigParamTest->pCisParam[i].cisId)) != NULL)
        {
          /* Update the context which is already set up. */
          lctrSetCisTest(pCisCtx, pSetCigParamTest, &pSetCigParamTest->pCisParam[i]);
        }
        else
        {
          /* Allocate new context, no allocation error, already checked. */
          pCisCtx = lctrAllocCisCtx(pCigCtx);
          pCisCtx->role = LL_ROLE_MASTER;
          lctrSetCisTest(pCisCtx, pSetCigParamTest, &pSetCigParamTest->pCisParam[i]);
        }
        pCisHandles[i] = pCisCtx->cisHandle;
      }
      lctrSetCigTest(pCigCtx, pSetCigParamTest);
    }
  }

  pCigCtx->roleData.mst.numCis = pSetCigParamTest->numCis;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Remove CIG.
 *
 *  \param  cigId           Identifier for CIG.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrRemoveCig(uint8_t cigId)
{
  lctrCigCtx_t *pCigCtx;

  if ((pCigCtx = lctrFindCigById(cigId)) == NULL)
  {
    LL_TRACE_WARN1("LctrRemoveCig, invalid CIG_ID=%d.", cigId);
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  /* Return LL_ERROR_CODE_CMD_DISALLOWED if anyone of the CIS is pending or slave role in the CIG. */
  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if ((pCisCtx->enabled == TRUE) &&
        (pCisCtx->cigId == cigId))
    {
      if (pCisCtx->role == LL_ROLE_SLAVE)
      {
        LL_TRACE_WARN0("LctrRemoveCig, invalid role");
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }

      if (pCisCtx->isCisReqPend == TRUE)
      {
        LL_TRACE_WARN0("LctrRemoveCig, there is a pending CIS");
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
    }
  }

  if (lctrGetNumEstCisCtxByCigCtx(pCigCtx) > 0)
  {
    LL_TRACE_WARN0("LctrRemoveCig, there is an established CIS");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];

    if ((pCisCtx->enabled == TRUE) &&
        (pCisCtx->cigId == cigId) &&
        (pCisCtx->role == LL_ROLE_MASTER))
    {
      lctrFreeCisCtx(pCisCtx);
    }
  }

  lctrFreeCigCtx(pCigCtx);

  return LL_SUCCESS;
}



/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for CIS master.
 */
/*************************************************************************************************/
void LctrMstCisInit(void)
{
  /* Add reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_CIS] = lctrCisMstResetHandler;

  /* Add CIS message dispatcher. */
  lctrMsgDispTbl[LCTR_DISP_CIS] = (LctrMsgDisp_t)lctrCisDisp;

  /* Add CIS function pointers */
  LctrUpdateCisChanMapFn = LctrCisUpdateChanMap;
  lctrRegisterChClassHandler(lctrMstCisChClassUpdate);
  lctrCisServicePowerMonitorFn = lctrCisServicePowerMonitor;

  /* Add CIS event handlers. */
  lctrEventHdlrTbl[LCTR_EVENT_CIS_TX_PENDING]  = lctrIsoTxPendingHandler;
  lctrEventHdlrTbl[LCTR_EVENT_CIS_RX_PENDING]  = lctrCisRxPendingHandler;
  lctrEventHdlrTbl[LCTR_EVENT_ISO_TX_COMPLETE] = lctrIsoTxCompletedHandler;

  /* Add LLCP SM handler. */
  lctrMstLlcpSmTbl[LCTR_LLCP_SM_CIS_EST]      = lctrMstLlcpExecuteCisEstSm;
  lctrMstLlcpSmTbl[LCTR_LLCP_SM_CIS_TERM]     = lctrLlcpExecuteCisTermSm;

  lctrCisDefaults();
  LmgrMstCisInit();

  /* Set supported features. */
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_1)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_CIS_MASTER_ROLE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Create CIS.
 *
 *  \param  numCis              Number of streams to create.
 *  \param  pCreateCisParam     CIS parameters.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrCreateCis(uint8_t numCis, LlCisCreateCisParams_t *pCreateCisParam)
{
  /* Return error if the previous create CIS is pending. */
  if (lmgrCisMstCb.createCisPend == TRUE)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  /* Make sure handle is valid and CIS is not yet established. */
  for (unsigned int i = 0; i < numCis; i++)
  {
    lctrCisCtx_t *pCisCtx;

    if ((pCisCtx = lctrFindCisByHandle(pCreateCisParam->pCisHandle[i])) == NULL)
    {
      LL_TRACE_WARN0("LctrCreateCis, invalid CIS handle");
      return LL_ERROR_CODE_UNKNOWN_CONN_ID;
    }

    if (lctrIsCisEst(pCisCtx) == TRUE)
    {
      LL_TRACE_WARN0("LctrCreateCis, CIS is already established");
      return LL_ERROR_CODE_ACL_CONN_ALREADY_EXISTS;
    }

    pCisCtx->isClosing = FALSE;
  }

  lmgrCisMstCb.createCisPend = TRUE;

  lctrMstCreateCisPend.numCis = numCis;
  /* Save the handles */
  for (unsigned int i = 0; i < numCis; i++)
  {
    lctrMstCreateCisPend.cisHandle[i] = pCreateCisParam->pCisHandle[i];
    lctrMstCreateCisPend.aclHandle[i] = pCreateCisParam->pAclHandle[i];
    lctrMstCreateCisPend.isCreateCisDone[i] = FALSE;
  }

  /* Only start first CIS establishment procedure. */
  lctrCreateCis_t *pMsg;

  if ((pMsg = (lctrCreateCis_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.handle = lctrMstCreateCisPend.aclHandle[0];
    pMsg->hdr.dispId = LCTR_DISP_CONN;
    pMsg->hdr.event = LCTR_CONN_MSG_API_CIS_REQ;

    pMsg->cisHandle =  lctrMstCreateCisPend.cisHandle[0];
    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Build a CIG operation.
 *
 *  \param  pCigCtx     CIG context.
 */
/*************************************************************************************************/
void lctrMstCisBuildCigOp(lctrCigCtx_t *pCigCtx)
{
  /* Pre-resolve common structures for efficient access. */
  BbOpDesc_t * const pOp = &pCigCtx->cigBod;

  /* When build the BOD, always setup channel use the first CIS context in the CIG. */
  lctrCisCtx_t  *pCisCtx = lctrCisGetHeadCis(&pCigCtx->list);
  pCigCtx->pCisCtx = pCisCtx;

  BbBleData_t * const pBle = &pCisCtx->bleData;
  BbBleMstCisEvent_t * const pCis = &pBle->op.mstCis;
  lctrConnCtx_t * pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));
  memset(pCis, 0, sizeof(BbBleMstCisEvent_t));

  /*** CIS context setup ***/

  /* pFirstCisCtx->cisEvtCounter = 0; */            /* cleared in alloc. */
  /* pFirstCisCtx->txHdr.sn = 0; */                 /* cleared in alloc */
  /* pFirstCisCtx->txHdr.nesn = 0; */               /* cleared in alloc */

  /*** BLE general setup ***/

  pBle->chan.opType = BB_BLE_OP_MST_CIS_EVENT;

  pBle->chan.chanIdx = pCisCtx->chIdx;         /* Set in the lctrCisSetupChanParam. */
  pBle->chan.txPower = pLctrRtCfg->defTxPwrLvl;
  pBle->chan.accAddr = pCisCtx->accessAddr;
  pBle->chan.crcInit = pCisCtx->crcInit;
  pBle->chan.txPhy = pCisCtx->phyMToS;
  pBle->chan.rxPhy = pCisCtx->phySToM;;
  pBle->chan.peerTxStableModIdx = TRUE;
  pBle->chan.peerRxStableModIdx = TRUE;

  if (lmgrGetOpFlag(LL_OP_MODE_FLAG_FORCE_CIS_CODED_PHY_S2))
  {
    /* Force Coded PHY option to S2. */
    pBle->chan.initTxPhyOptions = BB_PHY_OPTIONS_BLE_S2;
    pBle->chan.tifsTxPhyOptions = BB_PHY_OPTIONS_BLE_S2;
  }
  else
  {
    /* Set PHY options to mirror ACL connection option. */
    pBle->chan.initTxPhyOptions = pConnCtx->bleData.chan.initTxPhyOptions;
    pBle->chan.tifsTxPhyOptions = pConnCtx->bleData.chan.tifsTxPhyOptions;
  }

#if (LL_ENABLE_TESTER)
  pBle->chan.accAddrRx = pCisCtx->accessAddr ^ llTesterCb.cisAccessAddrRx;
  pBle->chan.accAddrTx = pCisCtx->accessAddr ^ llTesterCb.cisAccessAddrRx;
  pBle->chan.crcInitRx = pCisCtx->crcInit    ^ llTesterCb.cisCrcInitRx;
  pBle->chan.crcInitTx = pCisCtx->crcInit    ^ llTesterCb.cisCrcInitTx;
#endif

  /* pBle->chan.enc.enaEncrypt = FALSE; */  /* cleared in alloc */
  /* pBle->chan.enc.enaDecrypt = FALSE; */
  pBle->chan.enc.enaAuth = TRUE;
  pBle->chan.enc.nonceMode = PAL_BB_NONCE_MODE_EXT64_CNTR;
  pBle->chan.enc.pTxPktCounter = &pCisCtx->txPktCounter;
  pBle->chan.enc.pRxPktCounter = &pCisCtx->rxPktCounter;

  /*** General setup ***/

  pOp->minDurUsec = pCisCtx->subIntervUsec * WSF_MAX(pCisCtx->bnMToS, pCisCtx->bnSToM); /* Guarantee at least Max BN */
  pOp->maxDurUsec = pCigCtx->cigSyncDelayUsec;

  /* pOp->due = 0 */  /* set in lctrMstCisCigOpCommit() */
  pOp->reschPolicy = BB_RESCH_FIXED_PREFERRED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrMstCisCigEndOp;
  pOp->abortCback = lctrMstCisCigAbortOp;
  pOp->pCtx = pCigCtx;

  /*** BLE stream setup ***/

  pCis->checkContOpCback = lctrMstCisCheckContOp;
  pCis->checkContOpPostCback = lctrMstCisCheckContOpPostCback;
  pCis->execCback = lctrMstCisCigBeginOp;
  pCis->contExecCback = lctrMstCisCigContOp;
  pCis->postSubEvtCback = lctrMstCisCigPostSubEvt;
  pCis->cancelCback = lctrMstCisCigCleanupOp;
  pCis->txDataCback = lctrMstCisCigTxCompletion;
  pCis->rxDataCback = lctrMstCisCigRxCompletion;

  /*** Commit operation ***/

  /* Postponed in lctrMstCisCigOpCommit(). */
  pCigCtx->isBodBuilt = TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Build a CIG operation.
 *
 *  \param  pCisCtx     CIS context.
 */
/*************************************************************************************************/
void lctrMstCisBuildCisData(lctrCisCtx_t *pCisCtx)
{
  BbBleData_t * const pBle = &pCisCtx->bleData;
  BbBleMstCisEvent_t * const pCis = &pBle->op.mstCis;
  lctrConnCtx_t * pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

  memset(pBle, 0, sizeof(BbBleData_t));
  memset(pCis, 0, sizeof(BbBleMstCisEvent_t));

  /*** CIS context setup ***/

  /* pCisCtx->cisEvtCounter = 0; */            /* cleared in alloc. */
  /* pCisCtx->txHdr.sn = 0; */                 /* cleared in alloc */
  /* pCisCtx->txHdr.nesn = 0; */               /* cleared in alloc */

  /*** BLE general setup ***/

  pBle->chan.opType = BB_BLE_OP_MST_CIS_EVENT;

  pBle->chan.chanIdx = pCisCtx->chIdx;         /* Set in the lctrCisSetupChanParam. */
  pBle->chan.txPower = pLctrRtCfg->defTxPwrLvl;
  pBle->chan.accAddr = pCisCtx->accessAddr;
  pBle->chan.crcInit = pCisCtx->crcInit;
  pBle->chan.txPhy = pCisCtx->phyMToS;
  pBle->chan.rxPhy = pCisCtx->phySToM;;
  pBle->chan.peerTxStableModIdx = TRUE;
  pBle->chan.peerRxStableModIdx = TRUE;

  if (lmgrGetOpFlag(LL_OP_MODE_FLAG_FORCE_CIS_CODED_PHY_S2))
  {
    /* Force Coded PHY option to S2. */
    pBle->chan.initTxPhyOptions = BB_PHY_OPTIONS_BLE_S2;
    pBle->chan.tifsTxPhyOptions = BB_PHY_OPTIONS_BLE_S2;
  }
  else
  {
    /* Set PHY options to mirror ACL connection option. */
    pBle->chan.initTxPhyOptions = pConnCtx->bleData.chan.initTxPhyOptions;
    pBle->chan.tifsTxPhyOptions = pConnCtx->bleData.chan.tifsTxPhyOptions;
  }

#if (LL_ENABLE_TESTER)
  pBle->chan.accAddrRx = pCisCtx->accessAddr ^ llTesterCb.cisAccessAddrRx;
  pBle->chan.accAddrTx = pCisCtx->accessAddr ^ llTesterCb.cisAccessAddrRx;
  pBle->chan.crcInitRx = pCisCtx->crcInit    ^ llTesterCb.cisCrcInitRx;
  pBle->chan.crcInitTx = pCisCtx->crcInit    ^ llTesterCb.cisCrcInitTx;
#endif

  /* pBle->chan.enc.enaEncrypt = FALSE; */  /* cleared in alloc */
  /* pBle->chan.enc.enaDecrypt = FALSE; */
  pBle->chan.enc.enaAuth = TRUE;
  pBle->chan.enc.nonceMode = PAL_BB_NONCE_MODE_EXT64_CNTR;
  pBle->chan.enc.pTxPktCounter = &pCisCtx->txPktCounter;
  pBle->chan.enc.pRxPktCounter = &pCisCtx->rxPktCounter;

  /*** BLE stream setup ***/

  pCis->checkContOpCback = lctrMstCisCheckContOp;
  pCis->checkContOpPostCback = lctrMstCisCheckContOpPostCback;
  pCis->execCback = lctrMstCisCigBeginOp;
  pCis->contExecCback = lctrMstCisCigContOp;
  pCis->postSubEvtCback = lctrMstCisCigPostSubEvt;
  pCis->cancelCback = lctrMstCisCigCleanupOp;
  pCis->txDataCback = lctrMstCisCigTxCompletion;
  pCis->rxDataCback = lctrMstCisCigRxCompletion;
}

/*************************************************************************************************/
/*!
 *  \brief  Commit a CIG operation.
 *
 *  \param  pCigCtx     CIG context.
 *  \param  pCtx        ACL connection context.
 *  \param  pCisCtx     CIS connection context.
 */
/*************************************************************************************************/
void lctrMstCisCigOpCommit(lctrCigCtx_t *pCigCtx, lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  BbOpDesc_t * const pOp = &pCigCtx->cigBod;
  BbOpDesc_t *pConnBod = &pCtx->connBod;
  uint32_t refTime;

  if (pCisCtx->ceRef <= pCtx->eventCounter)
  {
    /* Recalculate the CE ref if it is already past. */
    pCisCtx->ceRef = pCtx->eventCounter +
                     ((LL_MIN_INSTANT + 1 +     /* +1 for next CE */
                       pCtx->maxLatency) *      /* ensure slave will listen to this packet */
                      pCtx->ecu.srFactor);      /* include subrating factor */

    refTime = pConnBod->dueUsec + (pCisCtx->ceRef - pCtx->eventCounter) * LCTR_CONN_IND_US(pCtx->connInterval);

    pCisCtx->offsetUsec = SchRmGetOffsetUsec(LCTR_ISO_INT_TO_US(pCigCtx->isoInterval),
                                             LCTR_GET_CIG_RM_HANDLE(pCigCtx), refTime);
  }
  else
  {
    refTime = pConnBod->dueUsec + (pCisCtx->ceRef - pCtx->eventCounter) * LCTR_CONN_IND_US(pCtx->connInterval);
  }

  pOp->dueUsec = refTime + pCisCtx->offsetUsec;

  (void)SchInsertAtDueTime(pOp, lctrCisResolveConflict);    /* CIS has the highest priority so scheduling will never fail. */

  pCigCtx->isBodStarted = TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Get reference time(due time) of the CIG BOD.
 *
 *  \param      rmHandle      Reservation manager handle of CIG BOD.
 *  \param      pDurUsec      Pointer to duration of the CIG BOD.
 *
 *  \return     Due time in microseconds of the CIG BOD.
 */
/*************************************************************************************************/
uint32_t lctrGetCigRefTime(uint8_t rmHandle, uint32_t *pDurUsec)
{
  uint32_t refTime = 0;
  lctrCigCtx_t *pCigCtx = LCTR_GET_CIG_RM_CTX(rmHandle);

  WSF_ASSERT(pCigCtx);

  if (pCigCtx->isBodStarted)
  {
    refTime = pCigCtx->cigBod.dueUsec;
    if (pDurUsec)
    {
      *pDurUsec = pCigCtx->cigBod.minDurUsec;
    }
  }

  return refTime;
}

/*************************************************************************************************/
/*!
 *  \brief  Create CIS procedure finishes.
 *
 *  \param  pCisCtx     CIS connection context.
 */
/*************************************************************************************************/
void lctrMstCreateCisDone(lctrCisCtx_t *pCisCtx)
{
  /* Update the create CIS pending data structure. */
  for (unsigned int i = 0; i < lctrMstCreateCisPend.numCis; i++)
  {
    if (pCisCtx->cisHandle == lctrMstCreateCisPend.cisHandle[i])
    {
      lctrMstCreateCisPend.isCreateCisDone[i] = TRUE;
      break;
    }
  }

  /* Continue creating next CIS. */
  for (unsigned int i = 0; i < lctrMstCreateCisPend.numCis; i++)
  {
    if (lctrMstCreateCisPend.isCreateCisDone[i] == FALSE)
    {
      lctrCreateCis_t *pMsg;

      if ((pMsg = (lctrCreateCis_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        pMsg->hdr.handle = lctrMstCreateCisPend.aclHandle[i];
        pMsg->hdr.dispId = LCTR_DISP_CONN;
        pMsg->hdr.event = LCTR_CONN_MSG_API_CIS_REQ;

        pMsg->cisHandle =  lctrMstCreateCisPend.cisHandle[i];
        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
      }
      return;
    }
  }

  /* No create CIS is pending. */
  lmgrCisMstCb.createCisPend = FALSE;
}
