/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  ATT client mandatory PDU processing functions.
 *
 *  Copyright (c) 2009-2019 Arm Ltd. All Rights Reserved.
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
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "wsf_msg.h"
#include "wsf_math.h"
#include "wsf_timer.h"
#include "util/bstream.h"
#include "att_api.h"
#include "att_main.h"
#include "attc_main.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Table of response processing functions */
static const attcProcRsp_t attcProcRspTbl[] =
{
  attcProcErrRsp,               /* ATT_METHOD_ERR */
  attcProcMtuRsp,               /* ATT_METHOD_MTU */
  attcProcFindOrReadRsp,        /* ATT_METHOD_FIND_INFO */
  attcProcFindByTypeRsp,        /* ATT_METHOD_FIND_TYPE */
  attcProcFindOrReadRsp,        /* ATT_METHOD_READ_TYPE */
  attcProcReadRsp,              /* ATT_METHOD_READ */
  attcProcReadLongRsp,          /* ATT_METHOD_READ_BLOB */
  attcProcReadRsp,              /* ATT_METHOD_READ_MULTIPLE */
  attcProcFindOrReadRsp,        /* ATT_METHOD_READ_GROUP_TYPE */
  attcProcWriteRsp,             /* ATT_METHOD_WRITE */
  NULL,                         /* ATT_METHOD_WRITE_CMD */
  attcProcPrepWriteRsp,         /* ATT_METHOD_PREPARE_WRITE */
  attcProcWriteRsp,             /* ATT_METHOD_EXECUTE_WRITE */
  NULL,                         /* unused */
  NULL,                         /* unused */
  NULL,                         /* unused */
  attcProcReadMultVarRsp        /* ATT_METHOD_READ_MULT_VAR */
};

/*************************************************************************************************/
/*!
 *  \brief  Process received Error response packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *  \param  pEvt      Pointer to callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attcProcErrRsp(attcCcb_t *pCcb, uint16_t len, uint8_t *pPacket, attEvt_t *pEvt)
{
  uint8_t *p;

  p =  pPacket + L2C_PAYLOAD_START + ATT_HDR_LEN;

  /* set callback event from stored method */
  pEvt->hdr.event = pCcb->outReq.hdr.event;

  /* ignore request opcode in the error response */
  p++;

  /* if request was a read or write with a specific handle */
  if (pEvt->hdr.event == ATTC_READ_RSP || pEvt->hdr.event == ATTC_READ_LONG_RSP ||
      pEvt->hdr.event == ATTC_WRITE_RSP || pEvt->hdr.event == ATTC_PREPARE_WRITE_RSP)
  {
    /* ignore handle in the error response; callback will use stored handle from request */
    p += 2;
  }
  else
  {
    /* set handle from packet */
    BSTREAM_TO_UINT16(pEvt->handle, p);
  }

  /* set status from error code in packet, but verify it's not 'success' */
  BSTREAM_TO_UINT8(pEvt->hdr.status, p);
  if (pEvt->hdr.status == ATT_SUCCESS)
  {
    pEvt->hdr.status = ATT_ERR_UNDEFINED;
  }

  /* no parameters so clear length */
  pEvt->valueLen = 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Process received MTU response packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *  \param  pEvt      Pointer to callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attcProcMtuRsp(attcCcb_t *pCcb, uint16_t len, uint8_t *pPacket, attEvt_t *pEvt)
{
  uint16_t  mtu;

  BYTES_TO_UINT16(mtu, pPacket + L2C_PAYLOAD_START + ATT_HDR_LEN);

  /* verify */
  if (mtu < ATT_DEFAULT_MTU)
  {
    mtu = ATT_DEFAULT_MTU;
  }

  /* set mtu for the connection */
  attSetMtu(pCcb->pMainCcb, pCcb->slot, mtu, WSF_MIN(pAttCfg->mtu, (HciGetMaxRxAclLen() - L2C_HDR_LEN)));
}

/*************************************************************************************************/
/*!
 *  \brief  Process received Find Information, Read By Type, or Read By Group Type
 *          response packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *  \param  pEvt      Pointer to callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attcProcFindOrReadRsp(attcCcb_t *pCcb, uint16_t len, uint8_t *pPacket, attEvt_t *pEvt)
{
  uint8_t   *p;
  uint8_t   *pEnd;
  uint16_t  handle;
  uint16_t  nextHandle;
  uint16_t  prevHandle;
  uint8_t   paramLen;

  p = pPacket + L2C_PAYLOAD_START + ATT_HDR_LEN;
  pEnd = pPacket + L2C_PAYLOAD_START + len;

  /* parameter length depends on packet type */
  if (pCcb->outReq.hdr.event == ATTC_MSG_API_FIND_INFO)
  {
    /* length in find info response is coded by UUID */
    if (*p++ == ATT_FIND_HANDLE_16_UUID)
    {
      paramLen = ATT_16_UUID_LEN;
    }
    else
    {
      paramLen = ATT_128_UUID_LEN;
    }
  }
  else if (pCcb->outReq.hdr.event == ATTC_MSG_API_READ_BY_TYPE)
  {
    /* length in read by type response is handle plus parameter length */
    paramLen = *p++ - 2;
  }
  else
  {
    /* length in read by group type response is two handles plus parameter length */
    paramLen = *p++ - (2 * 2);
  }

  /* get and verify all handles */
  nextHandle = pCcb->outReqParams.h.startHandle;
  while (p < pEnd)
  {
    /* get and compare handle */
    BSTREAM_TO_UINT16(handle, p);
    if (handle == 0 || nextHandle == 0 || handle < nextHandle ||
        handle > pCcb->outReqParams.h.endHandle)
    {
      pEvt->hdr.status = ATT_ERR_INVALID_RSP;
      break;
    }

    /* if read by group type response get second handle */
    if (pCcb->outReq.hdr.event == ATTC_MSG_API_READ_BY_GROUP_TYPE)
    {
      prevHandle = handle;
      BSTREAM_TO_UINT16(handle, p);
      if (handle == 0 || handle < prevHandle || handle < nextHandle ||
          handle > pCcb->outReqParams.h.endHandle)
      {
        pEvt->hdr.status = ATT_ERR_INVALID_RSP;
        break;
      }
    }

    /* set next expected handle, with special case for max handle */
    if (handle == ATT_HANDLE_MAX)
    {
      nextHandle = 0;
    }
    else
    {
      nextHandle = handle + 1;
    }

    /* skip over parameter */
    p += paramLen;

    /* check for truncated response */
    if (p > pEnd)
    {
      pEvt->hdr.status = ATT_ERR_INVALID_RSP;
      break;
    }
  }

  /* if response was correct */
  if (pEvt->hdr.status == ATT_SUCCESS)
  {
    /* if continuing */
    if (pCcb->outReq.hdr.status == ATTC_CONTINUING)
    {
      /* if all handles read */
      if (nextHandle == 0 || nextHandle == (pCcb->outReqParams.h.endHandle + 1))
      {
        /* we're done */
        pCcb->outReq.hdr.status = ATTC_NOT_CONTINUING;
      }
      /* else set up for next request */
      else
      {
        pCcb->outReqParams.h.startHandle = nextHandle;
        pCcb->outReq.handle = nextHandle;
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process received Read response packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *  \param  pEvt      Pointer to callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attcProcReadRsp(attcCcb_t *pCcb, uint16_t len, uint8_t *pPacket, attEvt_t *pEvt)
{
  /* nothing to process */
}

/*************************************************************************************************/
/*!
 *  \brief  Process received Write response packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *  \param  pEvt      Pointer to callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attcProcWriteRsp(attcCcb_t *pCcb, uint16_t len, uint8_t *pPacket, attEvt_t *pEvt)
{
  /* no parameters so clear length */
  pEvt->valueLen = 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Process received read multiple variable length response packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *  \param  pEvt      Pointer to callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attcProcReadMultVarRsp(attcCcb_t *pCcb, uint16_t len, uint8_t *pPacket, attEvt_t *pEvt)
{
  /* nothing to process */
}

/*************************************************************************************************/
/*!
 *  \brief  Process received multiple variable len notification packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attcProcMultiVarNtf(attcCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  attEvt_t    evt;
  uint8_t     *p;

  p = pPacket + L2C_PAYLOAD_START;

  /* parse packet and set callback event struct */
  evt.hdr.event = ATT_OPCODE_2_METHOD(*p++);
  evt.pValue = p;
  evt.valueLen = len - ATT_HDR_LEN;
  evt.hdr.param = pCcb->pMainCcb->connId;
  evt.hdr.status = ATT_SUCCESS;
  evt.continuing = FALSE;

  /* verify handle and call callback */
  if (attCb.cback)
  {
    (*attCb.cback)(&evt);
  }

  /* mark confirm as pending; will be sent when flow enabled or application sends it. */
  pCcb->pMainCcb->sccb[pCcb->slot].control |= ATT_CCB_STATUS_CNF_PENDING;
}

/*************************************************************************************************/
/*!
 *  \brief  Process received response packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attcProcRsp(attcCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  attEvt_t    evt;

  /* if no request in progress ignore response */
  if (pCcb->outReq.hdr.event == ATTC_MSG_API_NONE)
  {
    return;
  }

  /* get method */
  evt.hdr.event = ATT_OPCODE_2_METHOD(*(pPacket + L2C_PAYLOAD_START));

  /* if response method is not error and does not match stored method ignore response */
  if ((evt.hdr.event != ATT_METHOD_ERR) && (evt.hdr.event != pCcb->outReq.hdr.event))
  {
    return;
  }

  /* stop request timer */
  WsfTimerStop(&pCcb->outReqTimer);

  /* initialize event structure then process response */
  evt.pValue = pPacket + L2C_PAYLOAD_START + ATT_HDR_LEN;
  evt.valueLen = len - ATT_HDR_LEN;
  evt.handle = pCcb->outReq.handle;
  evt.hdr.status = ATT_SUCCESS;
  (*attcProcRspTbl[evt.hdr.event])(pCcb, len, pPacket, &evt);

  /* if not continuing or status is not success */
  if ((pCcb->outReq.hdr.status == ATTC_NOT_CONTINUING) || (evt.hdr.status != ATT_SUCCESS))
  {
    /* we're not sending another request so clear the out req */
    pCcb->outReq.hdr.event = ATTC_MSG_API_NONE;
    attcFreePkt(&pCcb->outReq);
  }

  /* call callback (if not mtu rsp) */
  if ((evt.hdr.event != ATT_METHOD_MTU) && attCb.cback)
  {
    /* set additional parameters and call callback */
    evt.continuing = pCcb->outReq.hdr.status;   /* continuing flag */
    evt.hdr.param = pCcb->outReq.hdr.param;     /* connId */
    (*attCb.cback)(&evt);
  }

  /* if no flow control */
  if (!(pCcb->pMainCcb->sccb[pCcb->slot].control & ATT_CCB_STATUS_FLOW_DISABLED))
  {
    /* if out req ready */
    if (pCcb->outReq.pPkt != NULL)
    {
      /* build and send request */
      attcSendReq(pCcb);
    }
    /* else if api is on deck */
    else if ((pCcb->slot == ATT_BEARER_SLOT_ID) &&
             (attcCb.onDeck[pCcb->connId-1].hdr.event != ATTC_MSG_API_NONE))
    {
      /* set up and send request */
      attcSetupReq(pCcb, &attcCb.onDeck[pCcb->connId-1]);

      /* clear on deck */
      attcCb.onDeck[pCcb->connId-1].hdr.event = ATTC_MSG_API_NONE;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process received indication or notification packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attcProcInd(attcCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  attEvt_t    evt;
  uint8_t     *p;
  uint8_t     *pPkt;

  p = pPacket + L2C_PAYLOAD_START;

  /* parse packet and set callback event struct */
  evt.hdr.event = ATT_OPCODE_2_METHOD(*p++);
  BSTREAM_TO_UINT16(evt.handle, p);
  evt.pValue = p;
  evt.valueLen = len - ATT_HDR_LEN - 2;
  evt.hdr.param = pCcb->pMainCcb->connId;
  evt.hdr.status = ATT_SUCCESS;
  evt.continuing = FALSE;

  /* verify handle and call callback */
  if ((evt.handle != 0) && attCb.cback)
  {
    (*attCb.cback)(&evt);
  }

  /* if indication send confirm */
  if (attcCb.autoCnf && (evt.hdr.event == ATT_METHOD_VALUE_IND))
  {
    if (!(pCcb->pMainCcb->sccb[pCcb->slot].control & ATT_CCB_STATUS_FLOW_DISABLED))
    {
      if ((pPkt = attMsgAlloc(ATT_VALUE_CNF_LEN + L2C_PAYLOAD_START)) != NULL)
      {
        *(pPkt + L2C_PAYLOAD_START) = ATT_PDU_VALUE_CNF;
        L2cDataReq(L2C_CID_ATT, pCcb->pMainCcb->handle, ATT_VALUE_CNF_LEN, pPkt);
      }
    }

    return;
  }

  /* mark confirm as pending; will be sent when flow enabled or application sends it. */
  pCcb->pMainCcb->sccb[pCcb->slot].control |= ATT_CCB_STATUS_CNF_PENDING;
}

/*************************************************************************************************/
/*!
 *  \brief  Build and send a WSF message to ATTC.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  msgId       Message ID.
 *  \param  pPkt        Packet parameters.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attcSendMsg(dmConnId_t connId, uint16_t handle, uint8_t msgId, attcPktParam_t *pPkt, bool_t continuing)
{
  attcCcb_t   *pCcb;
  uint16_t    mtu;
  bool_t      transTimedOut;

  WsfTaskLock();

  /* get CCB and verify connection still in use */
  if ((pCcb = attcCcbByConnId(connId, ATT_BEARER_SLOT_ID)) != NULL)
  {
    /* get MTU size */
    mtu = pCcb->pMainCcb->sccb[ATT_BEARER_SLOT_ID].mtu;
    transTimedOut = !!(pCcb->pMainCcb->sccb[ATT_BEARER_SLOT_ID].control & ATT_CCB_STATUS_TX_TIMEOUT);
  }
  /* else connection not in use */
  else
  {
    /* MTU size unknown */
    mtu = 0;
    transTimedOut = FALSE;
  }

  WsfTaskUnlock();

  /* if MTU size known for connection */
  if (mtu > 0)
  {
    /* if no transaction's timed out */
    if (!transTimedOut)
    {
      uint16_t dataLen = 0;

      /* if packet is not null then find out its length */
      if (pPkt != NULL)
      {
        /* if not prepare write request */
        if (msgId != ATTC_MSG_API_PREP_WRITE)
        {
          dataLen = pPkt->len;
        }
        /* else prepare write request */
        else
        {
          /* if not continuing */
          if (!continuing)
          {
            /* single prepare write request */
            dataLen = ATT_PREP_WRITE_REQ_LEN + pPkt->pW->len;
          }
          /* else will be sent as multiple prepare write requests */
        }
      }

      /* if packet length is less than or equal to negotiated MTU */
      if (dataLen <= mtu)
      {
        attcApiMsg_t *pMsg;

        /* allocate message buffer */
        if ((pMsg = WsfMsgAlloc(sizeof(attcApiMsg_t))) != NULL)
        {
          /* set parameters */
          pMsg->hdr.param = connId;
          pMsg->hdr.status = continuing;
          pMsg->hdr.event = msgId;
          pMsg->pPkt = pPkt;
          pMsg->handle = handle;
          pMsg->slot = ATT_BEARER_SLOT_ID;

          /* send message */
          WsfMsgSend(attCb.handlerId, pMsg);
          return;
        }
      }
      /* else packet length exceeds MTU size */
      else
      {
        /* call callback with failure status */
        attcExecCallback(connId, msgId, handle, ATT_ERR_MTU_EXCEEDED);
      }
    }
    else
    /* transaction's timed out */
    {
      /* call callback with failure status */
      attcExecCallback(connId, msgId, handle, ATT_ERR_TIMEOUT);
    }
  }

  /* alloc failed, transaction's timed out or packet length exceeded MTU size; free packet buffer */
  if (pPkt != NULL)
  {
    WsfMsgFree(pPkt);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Find Information Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcFindInfoReq(dmConnId_t connId, uint16_t startHandle, uint16_t endHandle, bool_t continuing)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_FIND_INFO_REQ_BUF_LEN)) != NULL)
  {
    /* set parameters */
    pPkt->len = ATT_FIND_INFO_REQ_LEN;
    pPkt->h.startHandle = startHandle;
    pPkt->h.endHandle = endHandle;

    /* build partial packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_FIND_INFO_REQ);

    /* send message */
    attcSendMsg(connId, startHandle, ATTC_MSG_API_FIND_INFO, pPkt, continuing);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Request.
 *
 *  \param  connId    DM connection ID.
 *  \param  handle    Attribute handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcReadReq(dmConnId_t connId, uint16_t handle)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_READ_REQ_BUF_LEN)) != NULL)
  {
    /* set length */
    pPkt->len = ATT_READ_REQ_LEN;

    /* build packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_READ_REQ);
    UINT16_TO_BSTREAM(p, handle);

    /* send message */
    attcSendMsg(connId, handle, ATTC_MSG_API_READ, pPkt, FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Write Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcWriteReq(dmConnId_t connId, uint16_t handle, uint16_t valueLen, uint8_t *pValue)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_WRITE_REQ_BUF_LEN + valueLen)) != NULL)
  {
    /* set length */
    pPkt->len = ATT_WRITE_REQ_LEN + valueLen;

    /* build packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_WRITE_REQ);
    UINT16_TO_BSTREAM(p, handle);
    memcpy(p, pValue, valueLen);

    /* send message */
    attcSendMsg(connId, handle, ATTC_MSG_API_WRITE, pPkt, FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Cancel an attribute protocol request in progress.
 *
 *  \param  connId    DM connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcCancelReq(dmConnId_t connId)
{
  attcSendMsg(connId, 0, ATTC_MSG_API_CANCEL, NULL, FALSE);
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Exchange MTU Request.
 *
 *  \param  connId    DM connection ID.
 *  \param  mtu       Attribute protocol MTU.
 *
 *  \return None.
 *
 *  \note   The Exchange MTU Request will be initiated automatically on a master connection.
 *
 *  \note   This API can be used by the application to initiate an Exchange MTU Request on slave
 *          connections.
 */
/*************************************************************************************************/
void AttcMtuReq(dmConnId_t connId, uint16_t mtu)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_MTU_REQ_BUF_LEN)) != NULL)
  {
    /* set length */
    pPkt->len = ATT_MTU_REQ_LEN;

    /* build packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_MTU_REQ);
    UINT16_TO_BSTREAM(p, mtu);

    /* send message */
    attcSendMsg(connId, 0, ATTC_MSG_API_MTU, pPkt, FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol indication confirmation.
 *
 *  \param  connId    DM connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcIndConfirm(dmConnId_t connId)
{
  attcCcb_t   *pCcb;
  uint8_t     *pPkt;

  pCcb = attcCcbByHandle(connId - 1, ATT_BEARER_SLOT_ID);

  /* If confirmation is pending */
  if (pCcb && (pCcb->pMainCcb->sccb[ATT_BEARER_SLOT_ID].control & ATT_CCB_STATUS_CNF_PENDING) &&
      !(pCcb->pMainCcb->sccb[ATT_BEARER_SLOT_ID].control & ATT_CCB_STATUS_FLOW_DISABLED))
  {
    if ((pPkt = attMsgAlloc(ATT_VALUE_CNF_LEN + L2C_PAYLOAD_START)) != NULL)
    {
      pCcb->pMainCcb->sccb[ATT_BEARER_SLOT_ID].control &= ~ATT_CCB_STATUS_CNF_PENDING;

      *(pPkt + L2C_PAYLOAD_START) = ATT_PDU_VALUE_CNF;
      L2cDataReq(L2C_CID_ATT, pCcb->pMainCcb->handle, ATT_VALUE_CNF_LEN, pPkt);
    }
  }
}
