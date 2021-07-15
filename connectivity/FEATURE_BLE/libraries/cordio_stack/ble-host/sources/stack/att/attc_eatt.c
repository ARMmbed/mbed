/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Enhanced ATT (EATT) client module.
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
#include "wsf_timer.h"
#include "wsf_msg.h"
#include "wsf_math.h"
#include "wsf_os.h"
#include "util/bstream.h"
#include "l2c_api.h"
#include "l2c_main.h"
#include "dm_api.h"
#include "eatt_api.h"
#include "att_eatt.h"
#include "att_api.h"
#include "att_defs.h"
#include "att_main.h"
#include "attc_main.h"

/**************************************************************************************************
  Function Prototypes
**************************************************************************************************/

static void eattcL2cCocDataInd(l2cCocEvt_t *pEvt);
static void eattcL2cCocDataCnf(l2cCocEvt_t *pEvt);
static void eattcConnCback(attCcb_t *pCcb, dmEvt_t *pDmEvt);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Interface to ATT */
static const eattFcnIf_t attcFcnIf =
{
  eattcL2cCocDataInd,
  eattcL2cCocDataCnf,
  (attMsgHandler_t) attcMsgCback,
  eattcConnCback
};

/*************************************************************************************************/
/*!
 *  \brief  Get a channel for transmission of an EATT client message.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  dataLen     Length of value data.
 *
 *  \return Slot ID
 */
/*************************************************************************************************/
static uint8_t eattcGetFreeSlot(dmConnId_t connId, uint8_t priority, uint16_t dataLen)
{
  eattConnCb_t *pEattCcb = eattGetConnCb(connId);

  if (pEattCcb)
  {
    if (pEattCfg->pPriorityTbl)
    {
      uint8_t i;

      for (i = 0; i < EATT_CONN_CHAN_MAX; i++)
      {
        uint8_t      slot = i + 1;
        eattChanCb_t *pChanCb = &pEattCcb->pChanCb[i];
      
        if (pChanCb->inUse && (pChanCb->priority >= priority) && (pChanCb->localMtu >= dataLen))
        {
          attcCcb_t *pCcb = attcCcbByConnId(connId, slot);
          attCcb_t  *pAttCcb = attCcbByConnId(connId);

          if (pAttCcb && !(pAttCcb->sccb[slot].control & ATT_CCB_STATUS_CONTEXT_LOCK))
          {
            if (pCcb && (pCcb->outReq.hdr.event == ATTC_MSG_API_NONE))
            {
              EATT_TRACE_INFO1("eattcGetFreeSlot: allocating slot: 0x%x", slot);
          
              /* Reserve the slot until the API function is processed in the ATT */
              pAttCcb->sccb[slot].control |= ATT_CCB_STATUS_CONTEXT_LOCK;

              return slot;
            }
          }
        }
      }
    }
    else
    {
      /* If pEattCfg->pPriorityTbl is NULL, the priority parameter is the L2CAP channel index for this connection. */
      WSF_ASSERT(priority < EATT_CONN_CHAN_MAX);
      return priority + 1;
    }
  }

  return ATT_BEARER_SLOT_ID;
}

/*************************************************************************************************/
/*!
 *  \brief  L2CAP CoC data indication callback function.
 *
 *  \param  pEvt    Pointer to event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattcL2cCocDataInd(l2cCocEvt_t *pEvt)
{
  l2cCocDataInd_t *pDataInd = &pEvt->dataInd;
  uint8_t          opcode;
  attcCcb_t       *pCcb;
  dmConnId_t       connId = (dmConnId_t) pDataInd->hdr.param;
  uint8_t          slot = eattGetSlotId(connId, pDataInd->cid);

  if (slot != ATT_BEARER_SLOT_INVALID)
  {
    /* get connection control block, ignore packet if not found */
    if ((pCcb = attcCcbByConnId(connId, slot)) == NULL)
    {
      return;
    }

    /* parse opcode */
    opcode = *pDataInd->pData;

    /* if response */
    if ((opcode <= ATT_PDU_EXEC_WRITE_RSP) || (opcode == ATT_PDU_READ_MULT_VAR_RSP))
    {
      attcProcRsp(pCcb, pDataInd->dataLen, pDataInd->pData - L2C_PAYLOAD_START);
    }
    /* else if indication or notification */
    else if ((opcode == ATT_PDU_VALUE_NTF) || (opcode == ATT_PDU_VALUE_IND))
    {
      attcProcInd(pCcb, pDataInd->dataLen, pDataInd->pData - L2C_PAYLOAD_START);
    }
    /* else if multiple value notification */
    else if (opcode == ATT_PDU_MULT_VALUE_NTF)
    {
      attcProcMultiVarNtf(pCcb, pDataInd->dataLen, pDataInd->pData - L2C_PAYLOAD_START);
    }
    /* else unknown opcode */
    else
    {
      attSendOpNotSupportedErr(pCcb->pMainCcb, pCcb->slot, opcode);
    }
  }
  else
  {
    ATT_TRACE_WARN0("eattc unexpected data indication");
  }
}

/*************************************************************************************************/
/*!
 *  \brief  L2CAP CoC data confirm callback function.
 *
 *  \param  pEvt    Pointer to event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattcL2cCocDataCnf(l2cCocEvt_t *pEvt)
{
  l2cCocDataCnf_t *pDataCnf = &pEvt->dataCnf;
  attcCcb_t     *pCcb;
  dmConnId_t    connId = (dmConnId_t) pDataCnf->hdr.param;
  uint8_t       slot = eattGetSlotId(connId, pDataCnf->cid);

  if (slot != ATT_BEARER_SLOT_INVALID)
  {
    if (pDataCnf->hdr.status == L2C_COC_DATA_SUCCESS)
    {
      /* get CCB */
      if ((pCcb = attcCcbByConnId(connId, slot)) != NULL)
      {
        /* if confirmation pending try sending now */
        EattcIndConfirm(connId, pDataCnf->cid);

        /* call pending write command callback */
        attcWriteCmdCallback(connId, pCcb, ATT_SUCCESS);
      }
    }
    else
    {
      /* TBD: handle error case */
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Connection callback for ATTC.
 *
 *  \param  pCcb    ATT control block.
 *  \param  pDmEvt  DM callback event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void eattcConnCback(attCcb_t *pCcb, dmEvt_t *pDmEvt)
{
  /* take no action */
}

/*************************************************************************************************/
/*!
 *  \brief  Build and send a WSF message to ATTC.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  msgId       Message ID.
 *  \param  pPkt        Packet parameters.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void eattcSendMsg(dmConnId_t connId, uint8_t priority, uint16_t handle, uint8_t msgId,
                  attcPktParam_t *pPkt, bool_t continuing)
{
  uint8_t     slot;
  uint16_t    dataLen = 0;

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

  /* get the next available channel given the priority and data length */
  slot = eattcGetFreeSlot(connId, priority, dataLen);

  /* if a cid is available */
  if (slot)
  {
    attcApiMsg_t *pMsg;

    /* allocate message buffer */
    if ((pMsg = WsfMsgAlloc(sizeof(attcApiMsg_t))) != NULL)
    {
      EATT_TRACE_INFO2("eattcSendMsg: sending event: 0x%x slot: 0x%x", msgId, slot);

      /* set parameters */
      pMsg->hdr.param = connId;
      pMsg->hdr.status = continuing;
      pMsg->hdr.event = msgId;
      pMsg->pPkt = pPkt;
      pMsg->handle = handle;
      pMsg->slot = slot;

      /* send message */
      WsfMsgSend(attCb.handlerId, pMsg);
    }
    else
    {
      attCcb_t *pAttCcb = attCcbByConnId(connId);

      /* Release the slot */
      if (pAttCcb)
      {
        pAttCcb->sccb[slot].control &= ~ATT_CCB_STATUS_CONTEXT_LOCK;
      }
    }
  }
  else
  {
    /* no EATT slot, pass to ATT */
    attcSendMsg(connId, handle, msgId, pPkt, continuing);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Find Information Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcFindInfoReq(dmConnId_t connId, uint8_t priority, uint16_t startHandle, uint16_t endHandle,
                      bool_t continuing)
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
    eattcSendMsg(connId, priority, startHandle, ATTC_MSG_API_FIND_INFO, pPkt, continuing);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Find By Type Value Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  uuid16      16-bit UUID to find.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcFindByTypeValueReq(dmConnId_t connId, uint8_t priority, uint16_t startHandle, uint16_t endHandle,
                            uint16_t uuid16, uint16_t valueLen, uint8_t *pValue, bool_t continuing)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_FIND_TYPE_REQ_BUF_LEN + valueLen)) != NULL)
  {
    /* set parameters */
    pPkt->len = ATT_FIND_TYPE_REQ_LEN + valueLen;
    pPkt->h.startHandle = startHandle;
    pPkt->h.endHandle = endHandle;

    /* build partial packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_FIND_TYPE_REQ);
    /* skip start and end handle fields */
    p += (2 * sizeof(uint16_t));
    UINT16_TO_BSTREAM(p, uuid16);
    memcpy(p, pValue, valueLen);

    /* send message */
    eattcSendMsg(connId, priority, startHandle, ATTC_MSG_API_FIND_BY_TYPE_VALUE, pPkt, continuing);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read By Type Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  uuidLen     Length of UUID (2 or 16).
 *  \param  pUuid       Pointer to UUID data.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadByTypeReq(dmConnId_t connId, uint8_t priority, uint16_t startHandle, uint16_t endHandle,
                        uint8_t uuidLen, uint8_t *pUuid, bool_t continuing)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_READ_TYPE_REQ_BUF_LEN + uuidLen)) != NULL)
  {
    /* set parameters */
    pPkt->len = ATT_READ_TYPE_REQ_LEN + uuidLen;
    pPkt->h.startHandle = startHandle;
    pPkt->h.endHandle = endHandle;

    /* build partial packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_READ_TYPE_REQ);
    /* skip start and end handle fields */
    p += (2 * sizeof(uint16_t));
    memcpy(p, pUuid, uuidLen);

    /* send message */
    eattcSendMsg(connId, priority, startHandle, ATTC_MSG_API_READ_BY_TYPE, pPkt, continuing);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Request.
 *
 *  \param  connId    DM connection ID.
 *  \param  priority  Operation priority.
 *  \param  handle    Attribute handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadReq(dmConnId_t connId, uint8_t priority, uint16_t handle)
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
    eattcSendMsg(connId, priority, handle, ATTC_MSG_API_READ, pPkt, FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  offset      Read attribute data starting at this offset.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadLongReq(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t offset, bool_t continuing)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_READ_BLOB_REQ_BUF_LEN)) != NULL)
  {
    /* set parameters */
    pPkt->len = ATT_READ_BLOB_REQ_LEN;
    pPkt->o.offset = offset;

    /* build partial packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_READ_BLOB_REQ);
    UINT16_TO_BSTREAM(p, handle);

    /* send message */
    eattcSendMsg(connId, priority, handle, ATTC_MSG_API_READ_LONG, pPkt, continuing);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Multiple Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  numHandles  Number of handles in attribute handle list.
 *  \param  pHandles    List of attribute handles.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadMultipleReq(dmConnId_t connId, uint8_t priority, uint8_t numHandles, uint16_t *pHandles)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;
  uint16_t        handle;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_READ_MULT_REQ_BUF_LEN + (numHandles * sizeof(uint16_t)))) != NULL)
  {
    /* set length */
    pPkt->len = ATT_READ_MULT_REQ_LEN + (numHandles * sizeof(uint16_t));

    /* save first handle */
    handle = pHandles[0];

    /* build packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_READ_MULT_REQ);
    while (numHandles--)
    {
      UINT16_TO_BSTREAM(p, *pHandles);
      pHandles++;
    }

    /* send message */
    eattcSendMsg(connId, priority, handle, ATTC_MSG_API_READ_MULTIPLE, pPkt, FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read By Group Type Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  uuidLen     Length of UUID (2 or 16).
 *  \param  pUuid       Pointer to UUID data.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadByGroupTypeReq(dmConnId_t connId, uint8_t priority, uint16_t startHandle, uint16_t endHandle,
                            uint8_t uuidLen, uint8_t *pUuid, bool_t continuing)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_READ_GROUP_TYPE_REQ_BUF_LEN + uuidLen)) != NULL)
  {
    /* set parameters */
    pPkt->len = ATT_READ_GROUP_TYPE_REQ_LEN + uuidLen;
    pPkt->h.startHandle = startHandle;
    pPkt->h.endHandle = endHandle;

    /* build partial packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_READ_GROUP_TYPE_REQ);
    /* skip start and end handle fields */
    p += (2 * sizeof(uint16_t));
    memcpy(p, pUuid, uuidLen);

    /* send message */
    eattcSendMsg(connId, priority, startHandle, ATTC_MSG_API_READ_BY_GROUP_TYPE, pPkt, continuing);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Write Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcWriteReq(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t valueLen, uint8_t *pValue)
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
    eattcSendMsg(connId, priority, handle, ATTC_MSG_API_WRITE, pPkt, FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Cancel an attribute protocol request in progress.
 *
 *  \param  connId    DM connection ID.
 *  \param  priority  Operation priority.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcCancelReq(dmConnId_t connId, uint8_t priority)
{
  eattcSendMsg(connId, priority, 0, ATTC_MSG_API_CANCEL, NULL, FALSE);
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol indication confirmation.
 *
 *  \param  connId    DM connection ID.
 *  \param  cid       L2Cap channel ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcIndConfirm(dmConnId_t connId, uint16_t cid)
{
  uint8_t      slot = eattGetSlotId(connId, cid);
  attSccb_t   *pSccb = &attCb.ccb[connId-1].sccb[slot];
  uint8_t      cnfPkt[] = {ATT_PDU_VALUE_CNF};

  if (slot != ATT_BEARER_SLOT_INVALID)
  {
    attcCcb_t *pCcb = attcCcbByHandle(connId - 1, slot);

    /* If confirmation is pending */
    if ((pCcb && pSccb->control & ATT_CCB_STATUS_CNF_PENDING) &&
        !(pSccb->control & ATT_CCB_STATUS_FLOW_DISABLED))
    {
        pSccb->control &= ~ATT_CCB_STATUS_CNF_PENDING;
        L2cCocDataReq(cid, ATT_VALUE_CNF_LEN, cnfPkt);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Write Command.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcWriteCmd(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t valueLen, uint8_t *pValue)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_WRITE_CMD_BUF_LEN + valueLen)) != NULL)
  {
    /* set length */
    pPkt->len = ATT_WRITE_CMD_LEN + valueLen;

    /* build packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_WRITE_CMD);
    UINT16_TO_BSTREAM(p, handle);
    memcpy(p, pValue, valueLen);

    /* send message */
    eattcSendMsg(connId, priority, handle, ATTC_MSG_API_WRITE_CMD, pPkt, FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Prepare Write Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  offset      Write attribute data starting at this offset.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *  \param  valueByRef  TRUE if pValue data is accessed by reference rather than copied.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcPrepareWriteReq(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t offset,
                          uint16_t valueLen, uint8_t *pValue, bool_t valueByRef, bool_t continuing)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;
  uint16_t        bufLen;

  if (continuing && valueByRef)
  {
    bufLen = ATT_PREP_WRITE_REQ_BUF_LEN;
  }
  else
  {
    bufLen = ATT_PREP_WRITE_REQ_BUF_LEN + valueLen;
  }

  /* allocate packet and parameter buffer */
  if ((pPkt = attcPrepWriteAllocMsg(bufLen)) != NULL)
  {
    /* set parameters */
    pPkt->pW->len = valueLen;
    pPkt->pW->offset = offset;

    /* build partial packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_PREP_WRITE_REQ);
    UINT16_TO_BSTREAM(p, handle);

    /* skip over offset field */
    p += sizeof(uint16_t);

    /* set value pointer and copy data to packet, if not valueByRef */
    if (continuing && valueByRef)
    {
      pPkt->pW->pValue = pValue;
    }
    else
    {
      memcpy(p, pValue, valueLen);
      pPkt->pW->pValue = p;
    }

    /* send message */
    eattcSendMsg(connId, priority, handle, ATTC_MSG_API_PREP_WRITE, pPkt, continuing);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Execute Write Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  writeAll    TRUE to write all queued writes, FALSE to cancel all queued writes.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcExecuteWriteReq(dmConnId_t connId, uint8_t priority, bool_t writeAll)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(ATT_EXEC_WRITE_REQ_BUF_LEN)) != NULL)
  {
    /* set length */
    pPkt->len = ATT_EXEC_WRITE_REQ_LEN;

    /* build packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_EXEC_WRITE_REQ);
    UINT8_TO_BSTREAM(p, writeAll);

    /* send message */
    eattcSendMsg(connId, priority, 0, ATTC_MSG_API_EXEC_WRITE, pPkt, FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Multiple Variable Length Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  numHandles  The number of handles in pHandles.
 *  \param  pHandles    List of attribute handles to read.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadMultVarLenReq(dmConnId_t connId, uint8_t priority, uint8_t numHandles, uint16_t *pHandles)
{
  attcPktParam_t  *pPkt;
  uint8_t         *p;
  uint8_t         msgLen = ATT_READ_MULT_VAR_REQ_LEN + (sizeof(uint16_t) * numHandles);
  uint8_t i;

  WSF_ASSERT(numHandles >= 2)

  /* allocate packet and parameter buffer */
  if ((pPkt = attMsgAlloc(L2C_PAYLOAD_START + msgLen)) != NULL)
  {
    /* set length */
    pPkt->len = msgLen;

    /* build packet */
    p = (uint8_t *) pPkt + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_READ_MULT_VAR_REQ);

    for (i = 0; i < numHandles; i++)
    {
      UINT16_TO_BSTREAM(p, pHandles[i]);
    }

    /* send message */
    eattcSendMsg(connId, priority, 0, ATTC_MSG_API_READ_MULT_VAR, pPkt, FALSE);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the Enhanced ATT Client.
 *
 *  \return None
 */
 /*************************************************************************************************/
void EattcInit()
{
  /* set up callback interface */
  attCb.pEnClient = &attcFcnIf;
}
