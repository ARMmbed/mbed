/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  ATT client optional signed PDU processing functions.
 *
 *  Copyright (c) 2011-2019 Arm Ltd. All Rights Reserved.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_buf.h"
#include "wsf_msg.h"
#include "sec_api.h"
#include "util/bstream.h"
#include "util/calc128.h"
#include "util/wstr.h"
#include "att_api.h"
#include "att_main.h"
#include "attc_main.h"
#include "att_sign.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* ATTC signed PDU control block */
typedef struct
{
  attcApiMsg_t  msg;
} attcSignCb_t;

/* Message parameters */
typedef union
{
  wsfMsgHdr_t     hdr;
  attcApiMsg_t    api;
} attcSignMsg_t;

/**************************************************************************************************
  Function Prototypes
**************************************************************************************************/

static void attcSignCloseCback(attcCcb_t *pCcb, uint8_t status);
static void attcSignMsgCback(attcCcb_t *pCcb, attcSignMsg_t *pMsg);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Interface to ATT */
static const attcSignFcnIf_t attcSignFcnIf =
{
  (attcSignMsgCback_t) attcSignMsgCback,
  attcSignCloseCback
};

/* Control block */
static attcSignCb_t *pAttcSignCb[DM_CONN_MAX];

/*************************************************************************************************/
/*!
 *  \brief  Allocate a signing control block.
 *
 *  \param  connId    Connection ID.
 *
 *  \return Pointer to control block.
 */
/*************************************************************************************************/
static attcSignCb_t *attcSignCbAlloc(dmConnId_t connId)
{
  WSF_ASSERT((connId > 0) && (connId <= DM_CONN_MAX));
  WSF_ASSERT(pAttcSignCb[connId - 1] == NULL);

  return (pAttcSignCb[connId - 1] = WsfBufAlloc(sizeof(attcSignCb_t)));
}
/*************************************************************************************************/
/*!
 *  \brief  Free a signing control block.
 *
 *  \param  connId    Connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attcSignCbFree(dmConnId_t connId)
{
  WsfBufFree(pAttcSignCb[connId - 1]);
  pAttcSignCb[connId - 1] = NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the signing control block for the connection ID.
 *
 *  \param  connId    Connection ID.
 *
 *  \return Pointer to control block.
 */
/*************************************************************************************************/
static attcSignCb_t *attcSignCbByConnId(dmConnId_t connId)
{
  WSF_ASSERT((connId > 0) && (connId <= DM_CONN_MAX));

  return pAttcSignCb[connId - 1];
}

/*************************************************************************************************/
/*!
 *  \brief  Connection close callback for ATTC signed PDU processing.
 *
 *  \param  pCcb    ATT control block.
 *  \param  status  Connection close status.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attcSignCloseCback(attcCcb_t *pCcb, uint8_t status)
{
  attcSignCb_t  *pCb;

  /* if connection closed while processing in progress */
  if ((pCb = attcSignCbByConnId(pCcb->connId)) != NULL)
  {

    /* message free buffer */
    if (pCb->msg.hdr.event != ATTC_MSG_API_NONE)
    {
      attcReqClear(pCcb->connId, &pCb->msg, status);
    }

    attcSignCbFree(pCcb->pMainCcb->connId);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Message handler callback for ATTC signed PDU processing.
 *
 *  \param  pCcb  ATT control block.
 *  \param  pMsg  ATTC message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attcSignMsgCback(attcCcb_t *pCcb, attcSignMsg_t *pMsg)
{
  attcSignCb_t *pCb;
  
  /* cannot assume pCcb parameter is valid for signed writes */
  if ((pCcb = attcCcbByConnId((dmConnId_t) pMsg->hdr.param, ATT_BEARER_SLOT_ID)) == NULL)
  {
    /* connection not in use */
    return;
  }

  if (pMsg->hdr.event == ATTC_MSG_API_SIGNED_WRITE_CMD)
  {
    /* adjust msg event id for later use with app callback */
    pMsg->hdr.event = ATTC_MSG_API_WRITE_CMD;

    /* verify no API request already waiting on deck or in progress,
     * and no signed write already in progress
     */
    if ((attcCb.onDeck[pCcb->connId-1].hdr.event != ATTC_MSG_API_NONE) ||
        (pCcb->outReq.hdr.event > ATTC_MSG_API_MTU) ||
        (attcSignCbByConnId((dmConnId_t) pMsg->hdr.param) != NULL))
    {
      /* free request and call callback with failure status */
      attcReqClear(pCcb->connId, &pMsg->api, ATT_ERR_OVERFLOW);
      return;
    }

    /* allocate signing control block */
    if ((pCb = attcSignCbAlloc((dmConnId_t) pMsg->hdr.param)) != NULL)
    {
      /* store message */
      pCb->msg = pMsg->api;

      uint16_t cmacTxtLen = pCb->msg.pPkt->len - ATT_AUTH_SIG_LEN + sizeof(uint32_t);
      uint8_t *pCmacText = WsfBufAlloc(cmacTxtLen);

      if (pCmacText)
      {
        uint8_t revLocalCsrk[SEC_CMAC_KEY_LEN] = {0};

        WStrReverseCpy(revLocalCsrk, DmSecGetLocalCsrk(), SEC_CMAC_KEY_LEN);
        WStrReverseCpy(pCmacText, (uint8_t *)pCb->msg.pPkt + L2C_PAYLOAD_START, cmacTxtLen);

        if (SecCmac(revLocalCsrk, pCmacText, cmacTxtLen, attCb.handlerId,
                    (dmConnId_t) pMsg->hdr.param, ATTC_MSG_CMAC_CMPL))
        {
          return;
        }
        else
        {
          WsfBufFree(pCmacText);
        }
      }
    }
    else
    {
      /* allocation failure */
      attcReqClear(pCcb->connId, &pMsg->api, ATT_ERR_UNDEFINED);
      return;
    }
  }
  else if (pMsg->hdr.event == ATTC_MSG_CMAC_CMPL)
  {
    secCmacMsg_t *pCmacMsg = (secCmacMsg_t*) pMsg;
    WsfBufFree(pCmacMsg->pPlainText);

    if ((pCb = attcSignCbByConnId((dmConnId_t) pMsg->hdr.param)) != NULL)
    {
      /* append signature to packet */
      WStrReverseCpy((uint8_t *) pCb->msg.pPkt +
                    (L2C_PAYLOAD_START + pCb->msg.pPkt->len - ATT_CMAC_RESULT_LEN),
                    pCmacMsg->pCiphertext, ATT_CMAC_RESULT_LEN);

      /* if MTU request in progress or flow controlled */
      if (pCcb->outReq.hdr.event == ATTC_MSG_API_MTU ||
          pCcb->pMainCcb->sccb[ATT_BEARER_SLOT_ID].control & ATT_CCB_STATUS_FLOW_DISABLED)
      {
        /* put request "on deck" for processing later */
        attcCb.onDeck[pCcb->connId-1] = pCb->msg;
      }
      /* otherwise ready to send */
      else
      {
        attcSetupReq(pCcb, &pCb->msg);
      }

      /* we're done-- free control block */
      attcSignCbFree((dmConnId_t) pMsg->hdr.param);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol signed Write Command.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  signCounter Value of the sign counter.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcSignedWriteCmd(dmConnId_t connId, uint16_t handle, uint32_t signCounter,
                        uint16_t valueLen, uint8_t *pValue)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;
      
  /* if connection already encrypted with security mode 1 level 2 or higher */
  if (DmConnSecLevel(connId) > DM_SEC_LEVEL_NONE)
  {
    /* use write command instead */
    AttcWriteCmd(connId, handle, valueLen, pValue);
    return;
  }

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_SIGNED_WRITE_CMD_BUF_LEN + valueLen)) != NULL)
  {
    /* set length */
    pPkt->len = ATT_SIGNED_WRITE_CMD_LEN + valueLen;

    /* build packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_SIGNED_WRITE_CMD);
    UINT16_TO_BSTREAM(p, handle);
    memcpy(p, pValue, valueLen);
    p += valueLen;
    UINT32_TO_BSTREAM(p, signCounter);

    /* send message */
    attcSendMsg(connId, handle, ATTC_MSG_API_SIGNED_WRITE_CMD, pPkt, FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize ATT client for data signing.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcSignInit(void)
{
  /* set up callback interface */
  attcCb.pSign = &attcSignFcnIf;
}
