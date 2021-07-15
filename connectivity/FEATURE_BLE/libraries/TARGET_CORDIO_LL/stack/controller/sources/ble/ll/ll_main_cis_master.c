/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer (LL) master control interface implementation file.
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

#include "ll_api.h"
#if (LL_ENABLE_TESTER)
#include "ll_tester_api.h"
#endif
#include "lmgr_api.h"
#include "lmgr_api_cis_master.h"
#include "lctr_api_cis_master.h"
#include "lctr_api_conn.h"
#include "util/bstream.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief      Check if the CIG parameters are valid or not.
 *
 *  \param      pCigParam    Set CIG parameters.
 *
 *  \return     True if parameter is valid, False otherwise.
 */
/*************************************************************************************************/
static bool_t LlIsCigParamsValid(LlCisCigParams_t *pCigParam)
{
  const uint8_t MAX_CIG_ID = 0xEF;
  const uint32_t MIN_SDU_INTERVAL = 0x000FF;
  const uint32_t MAX_SDU_INTERVAL = 0xFFFFF;
  const uint8_t MAX_SCA = 0x07;
  const uint8_t MAX_PACKING = 0x01;
  const uint8_t MAX_FRAMING = 0x01;
  const uint16_t MIN_TRANS_LATENCY = 0x0005;
  const uint16_t MAX_TRANS_LATENCY = 0x0FA0;
  const uint8_t MIN_NUM_CIS = 0x01;
  const uint8_t MAX_NUM_CIS = 0x10;
  const uint8_t MAX_CIS_ID = 0xEF;
  const uint16_t MAX_SDU_SIZE = 0x0FFF;
  const uint8_t MAX_RTE = 0x0F;

  if (pCigParam->cigId > MAX_CIG_ID)
  {
    return FALSE;
  }
  if (pCigParam->sduIntervalMToS != pCigParam->sduIntervalSToM)
  {
    /* For now, sduIntervalMToS shall be equal to the sduIntervalMToS */
    LL_TRACE_INFO0("LlIsCigParamsValid sduIntervalMToS != sduIntervalSToM");
    return FALSE;
  }
  if ((pCigParam->sduIntervalMToS < MIN_SDU_INTERVAL) || (pCigParam->sduIntervalMToS > MAX_SDU_INTERVAL))
  {
    LL_TRACE_INFO0("LlIsCigParamsValid Invalid sduIntervalMToS");
    return FALSE;
  }
  if ((pCigParam->sduIntervalSToM < MIN_SDU_INTERVAL) || (pCigParam->sduIntervalSToM > MAX_SDU_INTERVAL))
  {
    LL_TRACE_INFO0("LlIsCigParamsValid Invalid sduIntervalSToM");
    return FALSE;
  }
  if (pCigParam->sca > MAX_SCA)
  {
    LL_TRACE_INFO0("LlIsCigParamsValid Invalid sca");
    return FALSE;
  }
  if (pCigParam->packing > MAX_PACKING)
  {
    LL_TRACE_INFO0("LlIsCigParamsValid Invalid packing");
    return FALSE;
  }
  if (pCigParam->framing > MAX_FRAMING)
  {
    LL_TRACE_INFO0("LlIsCigParamsValid Invalid framing");
    return FALSE;
  }
  if ((pCigParam->transLatMToS < MIN_TRANS_LATENCY) || (pCigParam->transLatMToS > MAX_TRANS_LATENCY))
  {
    LL_TRACE_INFO0("LlIsCigParamsValid Invalid transLatMToS");
    return FALSE;
  }
  if ((pCigParam->transLatSToM < MIN_TRANS_LATENCY) || (pCigParam->transLatSToM > MAX_TRANS_LATENCY))
  {
    LL_TRACE_INFO0("LlIsCigParamsValid Invalid transLatSToM");
    return FALSE;
  }
  if ((pCigParam->numCis < MIN_NUM_CIS) || (pCigParam->numCis > MAX_NUM_CIS))
  {
    LL_TRACE_INFO0("LlIsCigParamsValid Invalid numCis");
    return FALSE;
  }

  for (unsigned int i = 0; i < pCigParam->numCis; i++)
  {
    LlCisCisParams_t *pCisParam = &pCigParam->pCisParam[i];

    if (pCisParam->cisId > MAX_CIS_ID)
    {
      LL_TRACE_INFO0("LlIsCigParamsValid Invalid cisId");
      return FALSE;
    }
    if (pCisParam->sduSizeMToS > MAX_SDU_SIZE)
    {
      LL_TRACE_INFO0("LlIsCigParamsValid Invalid sduSizeMToS");
      return FALSE;
    }
    if (pCisParam->sduSizeSToM > MAX_SDU_SIZE)
    {
      LL_TRACE_INFO0("LlIsCigParamsValid Invalid sduSizeSToM");
      return FALSE;
    }
    if (pCisParam->phyMToS == 0)
    {
      LL_TRACE_INFO0("LlIsCigParamsValid Invalid phyMToS");
      return FALSE;
    }
    if (pCisParam->phySToM == 0)
    {
      LL_TRACE_INFO0("LlIsCigParamsValid Invalid phySToM");
      return FALSE;
    }
    if (pCisParam->rteMToS > MAX_RTE)
    {
      LL_TRACE_WARN0("LlIsCigParamsValid rteMToS out of controller range, setting to max");
      pCisParam->rteMToS = MAX_RTE;
    }
    if (pCisParam->rteSToM > MAX_RTE)
    {
      LL_TRACE_WARN0("LlIsCigParamsValid rteSToM out of controller range, setting to max");
      pCisParam->rteSToM = MAX_RTE;
    }
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Validate all CIS PHY parameters.
 *
 *  \param      pCigParam    CIG parameters.
 *
 *  \return     True if all PHY are valid, false otherwise.
 */
/*************************************************************************************************/
static bool_t LlIsCigPhyParamsValid(LlCisCigParams_t *pCigParam)
{
  for (unsigned int i = 0; i < pCigParam->numCis; i++)
  {
    const uint8_t phyMask = LL_PHYS_LE_1M_BIT | LL_PHYS_LE_2M_BIT | LL_PHYS_LE_CODED_BIT;
    LlCisCisParams_t *pCisParam = &pCigParam->pCisParam[i];

    if (!llValidatePhySupport(pCisParam->phyMToS, pCisParam->phySToM))
    {
      return FALSE;
    }

    if (((pCisParam->phyMToS & ~phyMask) != 0) ||         /* reserved bits set in phyMToS */
        ((pCisParam->phySToM & ~phyMask) != 0))           /* reserved bits set in phySToM */
    {
      return FALSE;
    }

    if (BB_SYM_PHY_REQ &&
        (pCisParam->phyMToS != pCisParam->phySToM))
    {
      return FALSE;
    }
  }
  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Check if the CIG test parameters are valid or not.
 *
 *  \param      pSetCigParamTest    Set CIG parameters.
 *
 *  \return     True if parameter is valid, False otherwise.
 */
/*************************************************************************************************/
static bool_t LlIsCigTestParamsValid(LlCisCigParamsTest_t *pSetCigParamTest)
{
  /* All the values below are in the valid range. */
  const uint8_t MAX_CIG_ID = 0xEF;
  const uint32_t MIN_SDU_INTERVAL = 0x000FF;
  const uint32_t MAX_SDU_INTERVAL = 0xFFFFF;
  const uint8_t MAX_SCA = 0x07;
  const uint8_t MAX_PACKING = 0x01;
  const uint8_t MAX_FRAMING = 0x01;
  const uint8_t MIN_FT = 0x01;
  const uint8_t MAX_FT = 0x1F;
  const uint16_t MIN_ISO_INTERVAL = 0x0004;
  const uint16_t MAX_ISO_INTERVAL = 0x0C80;
  const uint8_t MIN_NUM_CIS = 0x01;
  const uint8_t MAX_NUM_CIS = 0x10;
  const uint8_t MAX_CIS_ID = 0xEF;
  const uint16_t MAX_SDU_SIZE = 0x0FFF;
  const uint16_t MAX_PDU_SIZE = 0x0FFB;
  const uint8_t MAX_BN = 0x0F;

  if (pSetCigParamTest->cigId > MAX_CIG_ID)
  {
    return FALSE;
  }
  if ((pSetCigParamTest->sduIntervalMToS < MIN_SDU_INTERVAL) || (pSetCigParamTest->sduIntervalMToS > MAX_SDU_INTERVAL))
  {
    return FALSE;
  }
  if ((pSetCigParamTest->sduIntervalSToM < MIN_SDU_INTERVAL) || (pSetCigParamTest->sduIntervalSToM > MAX_SDU_INTERVAL))
  {
    return FALSE;
  }
  if ((pSetCigParamTest->ftMToS < MIN_FT) || (pSetCigParamTest->ftMToS > MAX_FT))
  {
    return FALSE;
  }
  if ((pSetCigParamTest->ftSToM < MIN_FT) || (pSetCigParamTest->ftSToM > MAX_FT))
  {
    return FALSE;
  }
  if ((pSetCigParamTest->isoInterval < MIN_ISO_INTERVAL) || (pSetCigParamTest->isoInterval > MAX_ISO_INTERVAL))
  {
    return FALSE;
  }
  if (pSetCigParamTest->sca > MAX_SCA)
  {
    return FALSE;
  }
  if (pSetCigParamTest->packing > MAX_PACKING)
  {
    return FALSE;
  }
  if (pSetCigParamTest->framing > MAX_FRAMING)
  {
    return FALSE;
  }
  if ((pSetCigParamTest->numCis < MIN_NUM_CIS) || (pSetCigParamTest->numCis > MAX_NUM_CIS))
  {
    return FALSE;
  }

  for (unsigned int i = 0; i < pSetCigParamTest->numCis; i++)
  {
    LlCisCigCisParamsTest_t *pCisParam = &pSetCigParamTest->pCisParam[i];

    if (pCisParam->cisId > MAX_CIS_ID)
    {
      return FALSE;
    }
    if (pCisParam->sduSizeMToS > MAX_SDU_SIZE)
    {
      return FALSE;
    }
    if (pCisParam->sduSizeSToM > MAX_SDU_SIZE)
    {
      return FALSE;
    }
    if (pCisParam->pduSizeMToS > MAX_PDU_SIZE)
    {
      return FALSE;
    }
    if (pCisParam->pduSizeSToM > MAX_PDU_SIZE)
    {
      return FALSE;
    }
    if (pCisParam->phyMToS == 0)
    {
      return FALSE;
    }
    if (pCisParam->phySToM == 0)
    {
      return FALSE;
    }
    if (pCisParam->bnMToS > MAX_BN)
    {
      return FALSE;
    }
    if (pCisParam->bnSToM > MAX_BN)
    {
      return FALSE;
    }
  }


  if (pSetCigParamTest->pCisParam->bnMToS > pSetCigParamTest->pCisParam->nse ||
      pSetCigParamTest->pCisParam->bnSToM > pSetCigParamTest->pCisParam->nse)
  {
    return FALSE;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Validate all test cis param PHYs.
 *
 *  \param      pSetCigParamTest    CIG parameters.
 *
 *  \return     True if all PHY are valid, false otherwise.
 */
/*************************************************************************************************/
static bool_t LlIsCigPhyTestParamsValid(LlCisCigParamsTest_t *pSetCigParamTest)
{
  for (unsigned int i = 0; i < pSetCigParamTest->numCis; i++)
  {
    const uint8_t phyMask = LL_PHYS_LE_1M_BIT | LL_PHYS_LE_2M_BIT | LL_PHYS_LE_CODED_BIT;
    LlCisCigCisParamsTest_t *pCisParam = &pSetCigParamTest->pCisParam[i];

    if (!llValidatePhySupport(pCisParam->phyMToS, pCisParam->phySToM))
    {
      return FALSE;
    }

    if (((pCisParam->phyMToS & ~phyMask) != 0) ||         /* reserved bits set in phyMToS */
        ((pCisParam->phySToM & ~phyMask) != 0))           /* reserved bits set in phySToM */
    {
      return FALSE;
    }

    if (BB_SYM_PHY_REQ &&
        (pCisParam->phyMToS != pCisParam->phySToM))
    {
      return FALSE;
    }
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Used by a master host to set the parameters of all connected isochronous streams associated with a connected isochronous group in the controller.
 *
 *  \param      pCigParam    CIG parameters.
 *  \param      pCisHandles  Return buffer for the connected isochronous stream handles.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlSetCigParams(LlCisCigParams_t *pCigParam, uint16_t *pCisHandles)
{
  uint8_t status = LL_SUCCESS;

  LL_TRACE_INFO1("### LlApi ###  LlSetCigParams numCis=%u", pCigParam->numCis);

  memset(pCisHandles, 0, pCigParam->numCis * sizeof(uint16_t));

  LL_TRACE_INFO1("### LlApi ###  LlSetCigParams cigId=%u", pCigParam->cigId);
  LL_TRACE_INFO1("### LlApi ###  LlSetCigParams sduIntervalMToS=%u", pCigParam->sduIntervalMToS);
  LL_TRACE_INFO1("### LlApi ###  LlSetCigParams sduIntervalSToM=%u", pCigParam->sduIntervalSToM);
  LL_TRACE_INFO2("### LlApi ###  LlSetCigParams packing=%u, framing=%u", pCigParam->packing, pCigParam->framing);
  LL_TRACE_INFO1("### LlApi ###  LlSetCigParams numCis=%u", pCigParam->numCis);

  if ((LL_API_PARAM_CHECK == 1) &&
      (LlIsCigParamsValid(pCigParam) == FALSE))
  {
    LL_TRACE_INFO0("### LlApi ###  LlSetCigParams Invalid Parameters");
    return LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (LlIsCigPhyParamsValid(pCigParam) == FALSE))
  {
    LL_TRACE_INFO0("### LlApi ###  LlSetCigParams Invalid PHY: Not supported by controller");
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }

  status = LctrSetCigParam(pCigParam, pCisHandles);

  /* Add the check If the Host attempts to set Connected Isochronous Stream parameters that exceed the maximum supported Connected Isochronous Stream parameters in the Controller, the Controller shall return the error code Unsupported Feature or Parameter Value (0x11).*/

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      Used by a master Host to set the parameters of all connected isochronous streams
 *              associated with a connected isochronous group in the controller for testing purpose.
 *
 *  \param      pSetCigParamTest    CIG test parameters.
 *  \param      pCisHandles         Return buffer for the connected isochronous stream handles.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlSetCigParamsTest(LlCisCigParamsTest_t *pSetCigParamTest, uint16_t *pCisHandles)
{
  uint8_t status = LL_SUCCESS;

  LL_TRACE_INFO1("### LlApi ###  LlSetCigParamsTest numCis=%u", pSetCigParamTest->numCis);

  memset(pCisHandles, 0, pSetCigParamTest->numCis * sizeof(uint16_t));

  if ((LL_API_PARAM_CHECK == 1) &&
      (LlIsCigTestParamsValid(pSetCigParamTest) == FALSE))
  {
    LL_TRACE_INFO0("### LlApi ###  LlSetCigParamsTest Invalid Parameters");
    return LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (LlIsCigPhyTestParamsValid(pSetCigParamTest) == FALSE))
  {
    LL_TRACE_INFO0("### LlApi ###  LlSetCigParams Invalid PHY: Not supported by controller");
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }

  status = LctrSetCigParamTest(pSetCigParamTest, pCisHandles);

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      Used by the master host to establish one or more connected isochronous streams.
 *
 *  \param      numCis          Number of connected isochronous streams.
 *  \param      pCreateCisParam Parameters for create connected isochronous stream.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlCreateCis(uint8_t numCis, LlCisCreateCisParams_t *pCreateCisParam)
{
  uint8_t status = LL_SUCCESS;

  LL_TRACE_INFO1("### LlApi ###  LlCreateCis numCis=%u", numCis);

  if ((lmgrCb.features & LL_FEAT_ISO_HOST_SUPPORT) == 0)
  {
    LL_TRACE_INFO0("### LlApi ###  LlCreateCis ISO host support bit is not enabled");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1))
  {
    for (unsigned int i = 0; i < numCis; i++)
    {
      if ((pCreateCisParam->pAclHandle[i] >= pLctrRtCfg->maxConn) ||
          !LctrIsConnHandleEnabled(pCreateCisParam->pAclHandle[i]))
      {
        return LL_ERROR_CODE_UNKNOWN_CONN_ID;
      }

      if (LctrGetRole(pCreateCisParam->pAclHandle[i]) != LL_ROLE_MASTER)
      {
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
    }
  }
  status = LctrCreateCis(numCis, pCreateCisParam);

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      Removes all the connected isochronous streams which have been set up.
 *
 *  \param      cigId           Identifier of the connected isochronous group.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlRemoveCig(uint8_t cigId)
{
  uint8_t status = LL_SUCCESS;

  LL_TRACE_INFO1("### LlApi ###  LlRemoveCig CIG_ID=%u", cigId);

  status = LctrRemoveCig(cigId);

  return status;
}
