/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI Broadcast Isochronous Stream (BIS) command module.
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
#include "wsf_math.h"
#include "util/bstream.h"
#include "hci_cmd.h"
#include "hci_api.h"
#include "hci_main.h"

/*************************************************************************************************/
/*!
 *  \brief      HCI LE create BIG command.
 *
 *  \param      pCreateBis    Create BIG parameters.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeCreateBigCmd(HciCreateBig_t *pCreateBig)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_CREATE_BIG, HCI_LEN_LE_CREATE_BIS)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, pCreateBig->bigHandle);
    UINT8_TO_BSTREAM(p, pCreateBig->advHandle);
    UINT8_TO_BSTREAM(p, pCreateBig->numBis);
    UINT24_TO_BSTREAM(p, pCreateBig->sduInterUsec);
    UINT16_TO_BSTREAM(p, pCreateBig->maxSdu);
    UINT16_TO_BSTREAM(p, pCreateBig->mtlMs);
    UINT8_TO_BSTREAM(p, pCreateBig->rtn);
    UINT8_TO_BSTREAM(p, pCreateBig->phys);
    UINT8_TO_BSTREAM(p, pCreateBig->packing);
    UINT8_TO_BSTREAM(p, pCreateBig->framing);
    UINT8_TO_BSTREAM(p, pCreateBig->encrypt);
    memcpy(p, pCreateBig->bcstCode, sizeof(pCreateBig->bcstCode));

    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      HCI LE terminate BIG command.
 *
 *  \param      bigHandle     Used to identify the BIG.
 *  \param      reason        Termination reason.
 *
 *  \return     None.
 */
 /*************************************************************************************************/
void HciTerminateBigCmd(uint8_t bigHandle, uint8_t reason)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_TERMINATE_BIG, HCI_LEN_LE_REMOVE_CIG)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, bigHandle);
    UINT8_TO_BSTREAM(p, reason);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
*  \brief      HCI LE BIG create sync command.
*
*  \param      pCreateSync     BIG Create Sync parameters.
*
*  \return     Status error code.
*/
/*************************************************************************************************/
void HciLeBigCreateSyncCmd(HciBigCreateSync_t *pCreateSync)
{
  uint8_t *pBuf;
  uint8_t *p;
  uint8_t numBis = WSF_MIN(pCreateSync->numBis, sizeof(pCreateSync->bis));

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_BIG_CREATE_SYNC, HCI_LEN_LE_BIG_CREATE_SYNC(numBis))) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, pCreateSync->bigHandle);
    UINT16_TO_BSTREAM(p, pCreateSync->syncHandle);
    UINT8_TO_BSTREAM(p, pCreateSync->encrypt);
    memcpy(p, pCreateSync->bcstCode, sizeof(pCreateSync->bcstCode));
    p += HCI_BC_LEN;
    UINT8_TO_BSTREAM(p, pCreateSync->mse);
    UINT16_TO_BSTREAM(p, pCreateSync->bigSyncTimeout);
    UINT8_TO_BSTREAM(p, pCreateSync->numBis);
    memcpy(p, pCreateSync->bis, numBis);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      HCI LE BIG terminate sync command.
 *
 *  \param      bigHandle     Used to identify the BIG.
 *
 *  \return     None.
*/
/*************************************************************************************************/
void HciLeBigTerminateSync(uint8_t bigHandle)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_BIG_TERMINATE_SYNC, HCI_LEN_LE_BIG_TERMINATE_SYNC)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, bigHandle);
    hciCmdSend(pBuf);
  }
}
