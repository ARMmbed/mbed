/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI Constant Tone Extension (CTE) command module.
 *
 *  Copyright (c) 2018 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019 Packetcraft, Inc.
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

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set connection CTE receive parameters command.
 *
 *  \param      connHandle       Connection handle.
 *  \param      samplingEnable   TRUE to enable Connection IQ sampling, FALSE to disable it.
 *  \param      slotDurations    Switching and sampling slot durations to be used while receiving CTE.
 *  \param      switchPatternLen Number of Antenna IDs in switching pattern.
 *  \param      pAntennaIDs      List of Antenna IDs in switching pattern.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetConnCteRxParamsCmd(uint16_t connHandle, uint8_t samplingEnable, uint8_t slotDurations,
                                uint8_t switchPatternLen, uint8_t *pAntennaIDs)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_CONN_CTE_RX_PARAMS,
                          HCI_LEN_LE_SET_CONN_CTE_RX_PARAMS(switchPatternLen))) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, connHandle);
    UINT8_TO_BSTREAM(p, samplingEnable);
    UINT8_TO_BSTREAM(p, slotDurations);
    UINT8_TO_BSTREAM(p, switchPatternLen);
    memcpy(p, pAntennaIDs, switchPatternLen);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set connection CTE transmit parameters command.
 *
 *  \param      connHandle       Connection handle.
 *  \param      cteTypeBits      Permitted CTE type bits used for transmitting CTEs requested by peer.
 *  \param      switchPatternLen Number of Antenna IDs in switching pattern.
 *  \param      pAntennaIDs      List of Antenna IDs in switching pattern.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetConnCteTxParamsCmd(uint16_t connHandle, uint8_t cteTypeBits, uint8_t switchPatternLen,
                                uint8_t *pAntennaIDs)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_CONN_CTE_TX_PARAMS,
                          HCI_LEN_LE_SET_CONN_CTE_TX_PARAMS(switchPatternLen))) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, connHandle);
    UINT8_TO_BSTREAM(p, cteTypeBits);
    UINT8_TO_BSTREAM(p, switchPatternLen);
    memcpy(p, pAntennaIDs, switchPatternLen);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
  *  \brief      HCI LE connection CTE request enable command.
  *
  *  \param      connHandle  Connection handle.
  *  \param      enable      TRUE to enable CTE request for connection, FALSE to disable it.
  *  \param      cteReqInt   CTE request interval.
  *  \param      reqCteLen   Minimum length of CTE being requested in 8 us units.
  *  \param      reqCteType  Requested CTE type.
  *
  *  \return     None.
  */
/*************************************************************************************************/
void HciLeConnCteReqEnableCmd(uint16_t connHandle, uint8_t enable, uint16_t cteReqInt,
                              uint8_t reqCteLen, uint8_t reqCteType)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_CONN_CTE_REQ_ENABLE,
                          HCI_LEN_LE_CONN_CTE_REQ_ENABLE)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, connHandle);
    UINT8_TO_BSTREAM(p, enable);
    UINT16_TO_BSTREAM(p, cteReqInt);
    UINT8_TO_BSTREAM(p, reqCteLen);
    UINT8_TO_BSTREAM(p, reqCteType);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      HCI LE connection CTE response enable command.
 *
 *  \param      connHandle  Connection handle.
 *  \param      enable      TRUE to enable CTE response for connection, FALSE to disable it.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeConnCteRspEnableCmd(uint16_t connHandle, uint8_t enable)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_CONN_CTE_RSP_ENABLE,
                          HCI_LEN_LE_CONN_CTE_RSP_ENABLE)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, connHandle);
    UINT8_TO_BSTREAM(p, enable);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      HCI LE read antenna information command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeReadAntennaInfoCmd(void)
{
  uint8_t *pBuf;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_READ_ANTENNA_INFO, HCI_LEN_LE_READ_ANTENNA_INFO)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}
