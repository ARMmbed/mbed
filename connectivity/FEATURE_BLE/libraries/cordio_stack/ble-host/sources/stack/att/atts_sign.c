/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  ATT server signed PDU processing functions.
 *
 *  Copyright (c) 2011-2019 Arm Ltd. All Rights Reserved.
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
#include "wsf_buf.h"
#include "wsf_queue.h"
#include "wsf_msg.h"
#include "sec_api.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include "util/calc128.h"
#include "util/wstr.h"
#include "att_api.h"
#include "att_main.h"
#include "atts_main.h"
#include "att_sign.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Signed data buffer structure */
typedef struct attsSignBuf_tag
{
  struct attsSignBuf_tag  *pNext;             /* pointer to next in queue */
  attCcb_t                *pCcb;              /* ATT CCB associated with the packet */
  uint16_t                handle;             /* ATT handle */
  uint16_t                writeLen;           /* length of data to write */
  dmConnId_t              connId;             /* Connection ID associated with the packet */
  uint8_t                 packet[1];          /* packet */
} attsSignBuf_t;

/* ATTS signed PDU connection control block */
typedef struct
{
  uint32_t                signCounter;        /* sign counter for this connection */
  uint8_t                 *pCsrk;             /* signing key (CSRK) for this connection */
  bool_t                  authen;             /* TRUE if CSRK was received on authenticated connection */
  attsSignBuf_t           *pBuf;              /* current data being processed */
} attsSignCcb_t;

/* ATTS signed PDU control block */
typedef struct
{
  attsSignCcb_t           ccb[DM_CONN_MAX];
  wsfQueue_t              msgQueue;
} attsSignCb_t;


/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Control block */
static attsSignCb_t attsSignCb;

/*************************************************************************************************/
/*!
 *  \brief  Return the signing connection control block for the connection ID.
 *
 *  \param  connId    Connection ID.
 *
 *  \return Pointer to control block.
 */
/*************************************************************************************************/
static attsSignCcb_t *attsSignCcbByConnId(dmConnId_t connId)
{
  WSF_ASSERT((connId > 0) && (connId <= DM_CONN_MAX));

  return &attsSignCb.ccb[connId - 1];
}

/*************************************************************************************************/
/*!
 *  \brief  Perform required permission and security checks for a signed write command.
 *
 *  \param  connId      Connection ID.
 *  \param  handle      Attribute handle.
 *  \param  permissions Attribute permissions.
 *  \param  pSignCcb    Pointer to ATTS signed PDU connection control block.
 *
 *  \return ATT_SUCCESS if successful or error code on failure.
 */
/*************************************************************************************************/
static uint8_t attsSignedWritePermissions(dmConnId_t connId, uint16_t handle, uint8_t permissions,
                                          attsSignCcb_t *pSignCcb)
{
  /* verify write permissions */
  if ((permissions & (ATTS_PERMIT_WRITE | ATTS_PERMIT_WRITE_ENC)) != 
      (ATTS_PERMIT_WRITE | ATTS_PERMIT_WRITE_ENC))
  {
    return ATT_ERR_WRITE;
  }

  /* if peer CSRK not present */
  if (pSignCcb->pCsrk == NULL)
  {
    return ATT_ERR_AUTH;
  }

  /* check if signed write required with authenticated key but key not authenticated */
  if ((permissions & ATTS_PERMIT_WRITE_AUTH) && (pSignCcb->authen == FALSE))
  {
    return ATT_ERR_AUTH;
  }

  /* authorization check */
  if (permissions & ATTS_PERMIT_WRITE_AUTHORIZ)
  {
    if (attsCb.authorCback == NULL)
    {
      return ATT_ERR_AUTHOR;
    }

    return (*attsCb.authorCback)(connId, ATTS_PERMIT_WRITE, handle);
  }

  return ATT_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Start processing of data for signed write.
 *
 *  \param  pCcb      Signed data CCB.
 *  \param  pBuf      Signed data buffer structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsSignedWriteStart(attsSignCcb_t *pCcb, attsSignBuf_t *pBuf)
{
  if (pCcb->pCsrk != NULL)
  {
    uint16_t cmacTxtLen = pBuf->writeLen + ATT_WRITE_CMD_LEN + sizeof(uint32_t);
    uint8_t *pCmacTxt = WsfBufAlloc(cmacTxtLen);

    pCcb->pBuf = pBuf;

    if (pCmacTxt)
    {
      uint8_t revPeerCsrk[SEC_CMAC_KEY_LEN] = {0};

      WStrReverseCpy(revPeerCsrk, pCcb->pCsrk, SEC_CMAC_KEY_LEN);
      WStrReverseCpy(pCmacTxt, pBuf->packet, cmacTxtLen);

      if (SecCmac(revPeerCsrk, pCmacTxt, cmacTxtLen, attCb.handlerId,
                  pBuf->connId, ATTS_MSG_SIGN_CMAC_CMPL))
      {
        return;
      }

      WsfBufFree(pCmacTxt);
    }
  }
  else
  {
    /* no CSRK */
    ATT_TRACE_WARN0("ATTS CSRK not set");
  }

  /* free buffer */
  WsfBufFree(pBuf);
}

/*************************************************************************************************/
/*!
 *  \brief  Process a signed write command PDU.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsProcSignedWrite(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  uint8_t       *p;
  attsAttr_t    *pAttr;
  attsGroup_t   *pGroup;
  attsSignCcb_t *pSignCcb;
  attsSignBuf_t *pBuf;
  uint16_t      handle;
  uint16_t      writeLen;

  /* signed write should not be received on encrypted link */
  if (DmConnSecLevel(pCcb->connId) > DM_SEC_LEVEL_NONE)
  {
    return;
  }

  /* parse handle, calculate write length */
  p = pPacket + L2C_PAYLOAD_START + ATT_HDR_LEN;
  BSTREAM_TO_UINT16(handle, p);
  writeLen = len - ATT_SIGNED_WRITE_CMD_LEN;

  /* find attribute */
  if ((pAttr = attsFindByHandle(handle, &pGroup)) != NULL)
  {
    /* verify signed write is permitted */
    if ((pAttr->settings & ATTS_SET_ALLOW_SIGNED) == 0)
     {
       return;
     }

    /* verify that csrk is present */
    if (attsSignCcbByConnId(pCcb->pMainCcb->connId)->pCsrk == NULL) {
      return;
    }

    /* verify basic permissions */
    if ((pAttr->permissions & (ATTS_PERMIT_WRITE | ATTS_PERMIT_WRITE_ENC)) == 0)
     {
       return;
     }

    /* verify authentication */
    if ((pAttr->permissions & ATTS_PERMIT_WRITE_AUTH) &&
        (attsSignCcbByConnId(pCcb->pMainCcb->connId)->authen == 0))
    {
      return;
    }

    /* Note: authorization not verified at this stage as it is reserved for lesc
       writes; authorization occurs latter when the write cb is called */

     /* verify write length, fixed length */
    if (((pAttr->settings & ATTS_SET_VARIABLE_LEN) == 0) &&
              (writeLen != pAttr->maxLen))
     {
       return;
     }

     /* verify write length, variable length */
    if (((pAttr->settings & ATTS_SET_VARIABLE_LEN) != 0) &&
              (writeLen > pAttr->maxLen))
     {
       return;
     }

    /* allocate buffer to store packet and parameters */
    if ((pBuf = WsfBufAlloc(sizeof(attsSignBuf_t) - 1 + len)) != NULL)
     {
      /* initialize buffer */
      pBuf->pCcb = pCcb->pMainCcb;
      pBuf->handle = handle;
      pBuf->writeLen = writeLen;
      pBuf->connId = pCcb->connId;
      memcpy(pBuf->packet, (pPacket + L2C_PAYLOAD_START), len);
 
      /* check if a signed write is already in progress */
      pSignCcb = attsSignCcbByConnId(pCcb->connId);
 
      if (pSignCcb->pBuf != NULL)
      {
        /* signed write in progress; queue packet */
        WsfQueueEnq(&attsSignCb.msgQueue, pBuf);
      }
      else
      {
        /* start signed data processing */
        attsSignedWriteStart(pSignCcb, pBuf);
       }
     }
   }
}

/*************************************************************************************************/
/*!
 *  \brief  Message handler callback for ATTS signed PDU processing.
 *
 *  \param  pMsg  ATTS message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsSignMsgCback(secCmacMsg_t *pMsg)
{
  attsSignCcb_t *pCcb;
  attsSignBuf_t *pBuf;
  attsAttr_t    *pAttr;
  attsGroup_t   *pGroup;
  uint32_t      signCounter;

  if (pMsg->hdr.event == ATTS_MSG_SIGN_CMAC_CMPL)
  { 
    uint8_t signature[ATT_CMAC_RESULT_LEN] = {0};

    pCcb = attsSignCcbByConnId((dmConnId_t) pMsg->hdr.param);
    pBuf = pCcb->pBuf;

    WStrReverseCpy(signature, pMsg->pCiphertext, ATT_CMAC_RESULT_LEN);

    /* if signature ok */
    if (memcmp(signature, pBuf->packet + pBuf->writeLen + ATT_WRITE_CMD_LEN + 4, ATT_CMAC_RESULT_LEN) == 0)
    {
      /* check sign counter */
      BYTES_TO_UINT32(signCounter, (pBuf->packet + pBuf->writeLen + ATT_WRITE_CMD_LEN));

      if (signCounter >= pCcb->signCounter)
      {
        /* update sign counter */
        pCcb->signCounter = signCounter + 1;

        /* perform write: */

        /* find attribute */
        if ((pAttr = attsFindByHandle(pBuf->handle, &pGroup)) != NULL)
        {
          /* if write callback is desired */
          if ((pAttr->settings & ATTS_SET_WRITE_CBACK) &&
              (pGroup->writeCback != NULL))
          {
            /* write value via write callback */
            (*pGroup->writeCback)(pBuf->connId, pBuf->handle, ATT_PDU_SIGNED_WRITE_CMD,
                                  0, pBuf->writeLen, (pBuf->packet + ATT_WRITE_CMD_LEN), pAttr);
          }
          /* else check if CCC */
          else if ((pAttr->settings & ATTS_SET_CCC) && (attsCb.cccCback != NULL))
          {
            (*attsCb.cccCback)(pBuf->connId, ATT_METHOD_WRITE, pBuf->handle,
                                (pBuf->packet + ATT_WRITE_CMD_LEN));
          }
          else
          {
            /* write attribute value */
            memcpy(pAttr->pValue, (pBuf->packet + ATT_WRITE_CMD_LEN), pBuf->writeLen);

            /* write the length if variable length attribute */
            if ((pAttr->settings & ATTS_SET_VARIABLE_LEN) != 0)
            {
              *(pAttr->pLen) = pBuf->writeLen;
            }
          }
        }
      }
      else
      {
        ATT_TRACE_WARN0("Signed write counter failed");
      }
    }
    else
    {
      ATT_TRACE_WARN0("Signed write sig failed");
    }

    /* we're done-- free parameter buffer */
    WsfBufFree(pBuf);

    WsfBufFree(pMsg->pPlainText);

    /* process next signed write in queue, if any */
    if ((pBuf = WsfQueueDeq(&attsSignCb.msgQueue)) != NULL)
    {
      pCcb = attsSignCcbByConnId(pBuf->connId);
      attsSignedWriteStart(pCcb, pBuf);
    }
    else
    {
      pCcb->pBuf = NULL;
    }
  }
}


/*************************************************************************************************/
/*!
 *  \brief  Initialize ATT server for data signing.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsSignInit(void)
{
  /* initialize control block */
  WSF_QUEUE_INIT(&attsSignCb.msgQueue);

  /* set up callback interface */
  attsCb.signMsgCback = (attMsgHandler_t) attsSignMsgCback;
  attsProcFcnTbl[ATT_METHOD_SIGNED_WRITE_CMD] = attsProcSignedWrite;
}

/*************************************************************************************************/
/*!
 *  \brief  Set the peer's data signing key on this connection.  This function
 *          is typically called from the ATT connection callback when the connection is
 *          established.  The caller is responsible for maintaining the memory that
 *          contains the key.
 *
 *  \param  connId      DM connection ID.
 *  \param  pCsrk       Pointer to data signing key (CSRK).
 *  \param  authen      Whether CSRK was received on authenticated connection.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsSetCsrk(dmConnId_t connId, uint8_t *pCsrk, bool_t authen)
{
  attsSignCcb_t *pSignCcb = attsSignCcbByConnId(connId);

  pSignCcb->pCsrk = pCsrk;
  pSignCcb->authen = authen;
}

/*************************************************************************************************/
/*!
 *  \brief  Set the peer's sign counter on this connection.  This function
 *          is typically called from the ATT connection callback when the connection is
 *          established.  ATT maintains the value of the sign counter internally and
 *          sets the value when a signed packet is successfully received.
 *
 *  \param  connId      DM connection ID.
 *  \param  signCounter Sign counter.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsSetSignCounter(dmConnId_t connId, uint32_t signCounter)
{
  attsSignCcbByConnId(connId)->signCounter = signCounter;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the current value peer's sign counter on this connection.  This function
 *          is typically called from the ATT connection callback when the connection is
 *          closed so the application can store the sign counter for use on future
 *          connections.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return Sign counter.
 */
/*************************************************************************************************/
uint32_t AttsGetSignCounter(dmConnId_t connId)
{
  return attsSignCcbByConnId(connId)->signCounter;
}
