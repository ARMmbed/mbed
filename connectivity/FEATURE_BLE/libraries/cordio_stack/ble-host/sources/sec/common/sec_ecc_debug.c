/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Security ECC implementation using debug keys.
 *
 *  Copyright (c) 2010-2018 Arm Ltd. All Rights Reserved.
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

#if defined(SEC_ECC_CFG)

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "wsf_types.h"
#include "wsf_queue.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "sec_api.h"
#include "sec_main.h"
#include "wsf_buf.h"
#include "hci_api.h"
#include "util/calc128.h"

#if SEC_ECC_CFG == SEC_ECC_CFG_DEBUG

/* Debug Keys */
static const uint8_t debugPrivateKey[] = {0x3f, 0x49, 0xf6, 0xd4,  0xa3, 0xc5, 0x5f, 0x38,
                                          0x74, 0xc9, 0xb3, 0xe3,  0xd2, 0x10, 0x3f, 0x50,
                                          0x4a, 0xff, 0x60, 0x7b,  0xeb, 0x40, 0xb7, 0x99,
                                          0x58, 0x99, 0xb8, 0xa6,  0xcd, 0x3c, 0x1a, 0xbd};

static const uint8_t debugPublicKeyX[] = {0x20, 0xb0, 0x03, 0xd2,  0xf2, 0x97, 0xbe, 0x2c,
                                          0x5e, 0x2c, 0x83, 0xa7,  0xe9, 0xf9, 0xa5, 0xb9,
                                          0xef, 0xf4, 0x91, 0x11,  0xac, 0xf4, 0xfd, 0xdb,
                                          0xcc, 0x03, 0x01, 0x48,  0x0e, 0x35, 0x9d, 0xe6};

static const uint8_t debugPublicKeyY[] = {0xdc, 0x80, 0x9c, 0x49,  0x65, 0x2a, 0xeb, 0x6d,
                                          0x63, 0x32, 0x9a, 0xbf,  0x5a, 0x52, 0x15, 0x5c,
                                          0x76, 0x63, 0x45, 0xc2,  0x8f, 0xed, 0x30, 0x24,
                                          0x74, 0x1c, 0x8e, 0xd0,  0x15, 0x89, 0xd2, 0x8b};

static const uint8_t debugSharedSecret[] = {0x49, 0x4c, 0xfd, 0x99, 0x6f, 0x40, 0x17, 0xf5,
                                            0xb5, 0x48, 0xba, 0x66, 0x99, 0x60, 0x64, 0x08,
                                            0x62, 0x75, 0xd5, 0x1f, 0xe0, 0x8e, 0x56, 0x36,
                                            0xb9, 0x36, 0xd1, 0xe4, 0x57, 0x46, 0x4b, 0xed};

/*************************************************************************************************/
/*!
 *  \brief  Callback for HCI encryption for ECC operations.
 *
 *  \param  pBuf        Pointer to sec queue element.
 *  \param  pEvent      Pointer to HCI event.
 *  \param  handlerId   WSF handler ID.
 *
 *  \return none.
 */
/*************************************************************************************************/
void SecEccHciCback(secQueueBuf_t *pBuf, hciEvt_t *pEvent, wsfHandlerId_t handlerId)
{
}

/*************************************************************************************************/
/*!
 *  \brief  Generate an ECC key.
 *
 *  \param  handlerId   WSF handler ID for client.
 *  \param  param       Optional parameter sent to client's WSF handler.
 *  \param  event       Event for client's WSF handler.
 *
 *  \return TRUE if successful, else FALSE.
 */
/*************************************************************************************************/
bool_t SecEccGenKey(wsfHandlerId_t handlerId, uint16_t param, uint8_t event)
{
  secEccMsg_t *pMsg = WsfMsgAlloc(sizeof(secEccMsg_t));

  if (pMsg)
  {
    /* Generate the keys */
    memcpy(pMsg->data.key.pubKey_x, debugPublicKeyX, SEC_ECC_KEY_LEN);
    memcpy(pMsg->data.key.pubKey_y, debugPublicKeyY, SEC_ECC_KEY_LEN);
    memcpy(pMsg->data.key.privKey, debugPrivateKey, SEC_ECC_KEY_LEN);

    /* Send shared secret to handler */
    pMsg->hdr.event = event;
    pMsg->hdr.param = param;
    pMsg->hdr.status = HCI_SUCCESS;
    WsfMsgSend(handlerId, pMsg);

    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Generate an ECC key.
 *
 *  \param  pKey        ECC Key structure.
 *  \param  handlerId   WSF handler ID for client.
 *  \param  param       Optional parameter sent to client's WSF handler.
 *  \param  event       Event for client's WSF handler.
 *
 *  \return TRUE if successful, else FALSE.
 */
/*************************************************************************************************/
bool_t SecEccGenSharedSecret(secEccKey_t *pKey, wsfHandlerId_t handlerId, uint16_t param, uint8_t event)
{
  secEccMsg_t *pMsg = WsfMsgAlloc(sizeof(secEccMsg_t));

  if (pMsg)
  {
    memcpy(pMsg->data.sharedSecret.secret, debugSharedSecret, SEC_ECC_KEY_LEN);

    /* Send shared secret to handler */
    pMsg->hdr.event = event;
    pMsg->hdr.param = param;
    pMsg->hdr.status = HCI_SUCCESS;
    WsfMsgSend(handlerId, pMsg);

    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Called to initialize ECC security.
 *
 *  \param  None.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SecEccInit()
{

}

 #endif /* SEC_ECC_CFG */
#endif /* defined(SEC_ECC_CFG) */
