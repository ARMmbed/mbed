/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI transport interface.
 *
 *  Copyright (c) 2009-2018 Arm Ltd. All Rights Reserved.
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
#ifndef HCI_TR_H
#define HCI_TR_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Send a complete HCI ACL packet to the transport.
 *
 *  \param  pContext    Connection context.
 *  \param  pAclData    WSF msg buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrSendAclData(void *pContext, uint8_t *pAclData);

/*************************************************************************************************/
/*!
 *  \brief  Send a complete HCI command to the transport.
 *
 *  \param  pCmdData    WSF msg buffer containing an HCI command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrSendCmd(uint8_t *pCmdData);

/*************************************************************************************************/
/*!
 *  \brief  Initialize HCI transport resources.
 *
 *  \param  port        COM port.
 *  \param  baudRate    Baud rate.
 *  \param  flowControl TRUE if flow control is enabled
 *
 *  \return TRUE if initialization succeeds, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t hciTrInit(uint8_t port, uint32_t baudRate, bool_t flowControl);

/*************************************************************************************************/
/*!
 *  \brief  Close HCI transport resources.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrShutdown(void);

#ifdef __cplusplus
};
#endif

#endif /* HCI_TR_H */
