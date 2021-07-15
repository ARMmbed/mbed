/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI command module implementation file.
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

#include "lhci_int.h"
#include "hci_defs.h"
#include "ll_api.h"
#include "ll_defs.h"
#include "lmgr_api.h"
#include "wsf_msg.h"
#include "wsf_math.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

uint8_t numCis;                               /*!< Number of CIS. */
uint8_t cigID;                                /*!< CIG ID. */
uint16_t cisHandles[LL_MAX_CIS] = {0};        /*!< CIS Handle list. */

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Pack a set CIG parameters event.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  status      Status code.
 *  \param  numHandles  Number of cis handles.
 *  \param  pCisHandles Handle numbers.
 */
/*************************************************************************************************/
static void lhciPackSetCigParamsEvt(uint8_t *pBuf, uint8_t status, uint8_t numHandles, uint16_t *pCisHandles)
{
  UINT8_TO_BSTREAM (pBuf, status);
  UINT8_TO_BSTREAM (pBuf, cigID);
  UINT8_TO_BSTREAM (pBuf, numHandles);

  for (unsigned int i = 0; i < numHandles; i++)
  {
    UINT16_TO_BSTREAM (pBuf, pCisHandles[i]);
  }

  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a remove CIG event.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  status      Status code.
 *
 *  \return Packet length.
 */
/*************************************************************************************************/
static uint8_t lhciPackRemoveCigEvt(uint8_t *pBuf, uint8_t status)
{
  const uint8_t len = LHCI_LEN_LE_REMOVE_CIG;

  UINT8_TO_BSTREAM (pBuf, status);
  UINT8_TO_BSTREAM (pBuf, cigID);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Build and send a command complete event packet.
 *
 *  \param  pCmdHdr     Command HCI header.
 *  \param  status      Status value.
 *  \param  paramLen    Parameter length of the command status event.
 */
/*************************************************************************************************/
static void lhciCisMstSendCmdCmplEvt(LhciHdr_t *pCmdHdr, uint8_t status, uint8_t paramLen)
{
  uint8_t *pBuf;
  uint8_t *pEvtBuf;

  if ((pEvtBuf = lhciAllocCmdCmplEvt(paramLen, pCmdHdr->opCode)) == NULL)
  {
    return;
  }
  pBuf = pEvtBuf;

  switch (pCmdHdr->opCode)
  {
    /* --- command completion with status only parameter --- */

    case HCI_OPCODE_LE_SET_CIG_PARAMS:
    case HCI_OPCODE_LE_SET_CIG_PARAMS_TEST:
    {
      lhciPackSetCigParamsEvt(pBuf, status, numCis, cisHandles);
      break;
    }
    case HCI_OPCODE_LE_REMOVE_CIG:
    {
      lhciPackRemoveCigEvt(pBuf, status);
      break;
    }
    /* --- default --- */

    default:
      break;
  }

  lhciSendCmdCmplEvt(pEvtBuf);
}

/**************************************************************************************************
  External Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Decode an HCI command packet.
 *
 *  \param  pHdr    Decoded packet header.
 *  \param  pBuf    Packed HCI packet buffer.
 *
 *  \return TRUE if opcode handled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lhciMstCisDecodeCmdPkt(LhciHdr_t *pHdr, uint8_t *pBuf)
{
  uint8_t status = HCI_SUCCESS;
  uint8_t paramLen = 0;
  const uint8_t MAX_CIS_COUNT = 16;

  switch (pHdr->opCode)
  {
    case HCI_OPCODE_LE_SET_CIG_PARAMS:
    {
      uint32_t sduIntervalMToS = 0;
      uint32_t sduIntervalSToM = 0;
      uint8_t sca;
      uint8_t packing;
      uint8_t framing;
      uint16_t transLatMToS;
      uint16_t transLatSToM;

      BSTREAM_TO_UINT8(cigID, pBuf);
      BSTREAM_TO_UINT24(sduIntervalMToS, pBuf);
      BSTREAM_TO_UINT24(sduIntervalSToM, pBuf);
      BSTREAM_TO_UINT8(sca, pBuf);
      BSTREAM_TO_UINT8(packing, pBuf);
      BSTREAM_TO_UINT8(framing, pBuf);
      BSTREAM_TO_UINT16(transLatMToS, pBuf);
      BSTREAM_TO_UINT16(transLatSToM, pBuf);
      BSTREAM_TO_UINT8(numCis, pBuf);

      LlCisCigParams_t  cigParam;

      LlCisCisParams_t cisParam[MAX_CIS_COUNT];

      for (unsigned int i = 0; i < WSF_MIN(numCis, MAX_CIS_COUNT); i++)
      {
        BSTREAM_TO_UINT8(cisParam[i].cisId, pBuf);
        BSTREAM_TO_UINT16(cisParam[i].sduSizeMToS, pBuf);
        BSTREAM_TO_UINT16(cisParam[i].sduSizeSToM, pBuf);
        BSTREAM_TO_UINT8(cisParam[i].phyMToS, pBuf);
        BSTREAM_TO_UINT8(cisParam[i].phySToM, pBuf);
        BSTREAM_TO_UINT8(cisParam[i].rteMToS, pBuf);
        BSTREAM_TO_UINT8(cisParam[i].rteSToM, pBuf);
      }

      cigParam.cigId = cigID;
      cigParam.sduIntervalMToS = sduIntervalMToS;
      cigParam.sduIntervalSToM = sduIntervalSToM;
      cigParam.sca = sca;
      cigParam.packing = packing;
      cigParam.framing = framing;
      cigParam.transLatMToS = transLatMToS;
      cigParam.transLatSToM = transLatSToM;
      cigParam.numCis = numCis;
      cigParam.pCisParam = &cisParam[0];

      status = LlSetCigParams(&cigParam, cisHandles);
      paramLen = 3 + numCis * 2;
      break;
    }
    case HCI_OPCODE_LE_SET_CIG_PARAMS_TEST:
    {
      uint32_t sduIntervalMToS = 0;
      uint32_t sduIntervalSToM = 0;
      uint8_t ftMToS;
      uint8_t ftSToM;
      uint16_t isoInterval;
      uint8_t sca;
      uint8_t packing;
      uint8_t framing;

      BSTREAM_TO_UINT8(cigID, pBuf);
      BSTREAM_TO_UINT24(sduIntervalMToS, pBuf);
      BSTREAM_TO_UINT24(sduIntervalSToM, pBuf);
      BSTREAM_TO_UINT8(ftMToS, pBuf);
      BSTREAM_TO_UINT8(ftSToM, pBuf);
      BSTREAM_TO_UINT16(isoInterval, pBuf);
      BSTREAM_TO_UINT8(sca, pBuf);
      BSTREAM_TO_UINT8(packing, pBuf);
      BSTREAM_TO_UINT8(framing, pBuf);
      BSTREAM_TO_UINT8(numCis, pBuf);

      LlCisCigParamsTest_t  cigParam;

      LlCisCigCisParamsTest_t cisParam[LL_MAX_CIS];

      for (unsigned int i = 0; i < WSF_MIN(numCis, LL_MAX_CIS); i++)
      {
        BSTREAM_TO_UINT8(cisParam[i].cisId, pBuf);
        BSTREAM_TO_UINT8(cisParam[i].nse, pBuf);
        BSTREAM_TO_UINT16(cisParam[i].sduSizeMToS, pBuf);
        BSTREAM_TO_UINT16(cisParam[i].sduSizeSToM, pBuf);
        BSTREAM_TO_UINT16(cisParam[i].pduSizeMToS, pBuf);
        BSTREAM_TO_UINT16(cisParam[i].pduSizeSToM, pBuf);
        BSTREAM_TO_UINT8(cisParam[i].phyMToS, pBuf);
        BSTREAM_TO_UINT8(cisParam[i].phySToM, pBuf);
        BSTREAM_TO_UINT8(cisParam[i].bnMToS, pBuf);
        BSTREAM_TO_UINT8(cisParam[i].bnSToM, pBuf);
      }

      cigParam.cigId = cigID;
      cigParam.sduIntervalMToS = sduIntervalMToS;
      cigParam.sduIntervalSToM = sduIntervalSToM;
      cigParam.ftMToS = ftMToS;
      cigParam.ftSToM = ftSToM;
      cigParam.isoInterval = isoInterval;
      cigParam.sca = sca;
      cigParam.packing = packing;
      cigParam.framing = framing;
      cigParam.numCis = numCis;
      cigParam.pCisParam = &cisParam[0];

      status = LlSetCigParamsTest(&cigParam, cisHandles);
      paramLen = 3 + numCis * 2;
      break;
    }
    case HCI_OPCODE_LE_CREATE_CIS:
    {
      BSTREAM_TO_UINT8(numCis, pBuf);
      LlCisCreateCisParams_t createCisParam;
      uint16_t aclHandle[LL_MAX_CIS];
      uint16_t cisHandle[LL_MAX_CIS];

      for (unsigned int i = 0; i < WSF_MIN(numCis, LL_MAX_CIS); i++)
      {
        BSTREAM_TO_UINT16 (cisHandle[i], pBuf);
        BSTREAM_TO_UINT16 (aclHandle[i], pBuf);
      }
      createCisParam.pCisHandle = &cisHandle[0];
      createCisParam.pAclHandle = &aclHandle[0];
      status = LlCreateCis(numCis, &createCisParam);
      paramLen = LHCI_LEN_CMD_STATUS_EVT;
      break;
    }
    case HCI_OPCODE_LE_REMOVE_CIG:
    {
      BSTREAM_TO_UINT8(cigID, pBuf);
      status = LlRemoveCig(cigID);

      paramLen = LHCI_LEN_LE_REMOVE_CIG;

      break;
    }

    default:
      return FALSE;     /* exit dispatcher routine */
  }

  if (paramLen == LHCI_LEN_CMD_STATUS_EVT)
  {
    lhciSendCmdStatusEvt(pHdr, status);
  }
  else if (paramLen > 0)
  {
    lhciCisMstSendCmdCmplEvt(pHdr, status, paramLen);
  }

  return TRUE;
}
