/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI Connected Isochronous Stream (CIS) command module.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_msg.h"
#include "util/bstream.h"
#include "hci_cmd.h"
#include "hci_api.h"
#include "hci_main.h"
#include "hci_core_iso.h"
#include "hci_defs.h"

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set CIG parameters command.
 *
 *  \param      pCigParam    CIG parameters.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetCigParamsCmd(HciCisCigParams_t *pCigParam)
{
  uint8_t *pBuf;
  uint8_t *p;
  HciCisCisParams_t *pCisParam;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_CIG_PARAMS,
                          HCI_LEN_LE_SET_CIG_PARAMS(pCigParam->numCis))) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, pCigParam->cigId);
    UINT24_TO_BSTREAM(p, pCigParam->sduIntervalMToS);
    UINT24_TO_BSTREAM(p, pCigParam->sduIntervalSToM);
    UINT8_TO_BSTREAM(p, pCigParam->sca);
    UINT8_TO_BSTREAM(p, pCigParam->packing);
    UINT8_TO_BSTREAM(p, pCigParam->framing);
    UINT16_TO_BSTREAM(p, pCigParam->transLatMToS);
    UINT16_TO_BSTREAM(p, pCigParam->transLatSToM);
    UINT8_TO_BSTREAM(p, pCigParam->numCis);

    pCisParam = pCigParam->pCisParam;

    for (uint8_t i = pCigParam->numCis; i > 0; i--, pCisParam++)
    {
      UINT8_TO_BSTREAM(p, pCisParam->cisId);
      UINT16_TO_BSTREAM(p, pCisParam->sduSizeMToS);
      UINT16_TO_BSTREAM(p, pCisParam->sduSizeSToM);
      UINT8_TO_BSTREAM(p, pCisParam->phyMToS);
      UINT8_TO_BSTREAM(p, pCisParam->phySToM);
      UINT8_TO_BSTREAM(p, pCisParam->rteMToS);
      UINT8_TO_BSTREAM(p, pCisParam->rteSToM);
    }

    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      HCI LE create CIS command.
 *
 *  \param      numCis            Nunber of CISes.
 *  \param      pCreateCisParam   Parameters for creating connected isochronous stream.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeCreateCisCmd(uint8_t numCis, HciCisCreateCisParams_t *pCreateCisParam)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_CREATE_CIS, HCI_LEN_LE_CREATE_CIS(numCis))) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, numCis);

    for (uint8_t i = 0; i < numCis; i++)
    {
      UINT16_TO_BSTREAM(p, pCreateCisParam->pCisHandle[i]);
      UINT16_TO_BSTREAM(p, pCreateCisParam->pAclHandle[i]);
    }

    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      HCI LE remove CIG command.
 *
 *  \param      cigId        Identifer of a CIG.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeRemoveCigCmd(uint8_t cigId)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_REMOVE_CIG, HCI_LEN_LE_REMOVE_CIG)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, cigId);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      HCI LE accept CIS request command.
 *
 *  \param      handle       Connection handle of the CIS to be accepted.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeAcceptCisReqCmd(uint16_t handle)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_ACCEPT_CIS_REQ, HCI_LEN_LE_ACCEPT_CIS_REQ)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      HCI LE reject CIS request command.
 *
 *  \param      handle       Connection handle of the CIS to be rejected.
 *  \param      reason       Reason the CIS request was rejected.
 *
 *  \return     None.
*/
/*************************************************************************************************/
void HciLeRejectCisReqCmd(uint16_t handle, uint8_t reason)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_REJECT_CIS_REQ, HCI_LEN_LE_REJECT_CIS_REQ)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    UINT8_TO_BSTREAM(p, reason);
    hciCmdSend(pBuf);
  }
}
