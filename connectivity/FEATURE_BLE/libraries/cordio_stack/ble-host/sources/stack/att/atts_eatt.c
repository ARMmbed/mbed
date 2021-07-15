/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Enhanced ATT (EATT) server.
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
#include "att_api.h"
#include "att_main.h"
#include "atts_main.h"
#include "eatt_api.h"
#include "att_eatt.h"
#include "svc_core.h"

/**************************************************************************************************
  Function Prototypes
**************************************************************************************************/

static void eattsL2cCocDataInd(l2cCocEvt_t *pEvt);
static void eattsL2cCocDataCnf(l2cCocEvt_t *pEvt);
static void eattsConnCback(attCcb_t *pCcb, dmEvt_t *pDmEvt);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Interface to ATT */
static const eattFcnIf_t attsFcnIf =
{
  eattsL2cCocDataInd,
  eattsL2cCocDataCnf,
  (attMsgHandler_t) attsMsgCback,
  eattsConnCback
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

extern attsProcFcn_t attsProcFcnTbl[];
extern const uint8_t attsMinPduLen[];

/*************************************************************************************************/
/*!
 *  \brief  Check if application callback is pending for indication or a given notification, or
 *          the maximum number of simultaneous notifications has been reached.
 *
 *  \param  pCcb      ATTS ind control block.
 *  \param  opcode    Operation opcode.
 *  \param  handle    Attribute handle.
 *
 *  \return TRUE if app callback's pending or max number of simultaneous notifications reached.
 *          FALSE, otherwise.
 */
/*************************************************************************************************/
static bool_t eattsPendIndNtfHandle(attsCcb_t *pCcb, uint8_t opcode, uint16_t handle)
{
  uint8_t pendNtfs;
  uint8_t i;

  /* if indication */
  if (opcode == ATT_PDU_VALUE_IND)
  {
    /* see if callback pending for indication */
    return (pCcb->pendIndHandle == ATT_HANDLE_NONE) ? FALSE : TRUE;
  }

  /* initialize number of notification callbacks pending */
  pendNtfs = 0;

  for (i = 0; i < ATT_NUM_SIMUL_NTF; i++)
  {
    /* if callback pending for notification */
    if (pCcb->pendNtfHandle[i] != ATT_HANDLE_NONE)
    {
      if (opcode == ATT_PDU_MULT_VALUE_NTF)
      {
        /* Multiple value notification not used with pending notifications. */
        return TRUE;
      }

      /* if callback pending for this handle */
      if (pCcb->pendNtfHandle[i] == handle)
      {
        /* callback pending for this notification */
        return TRUE;
      }

      pendNtfs++;
    }
  }

  /* no callback is pending for this notification but see if the maximum number of simultaneous
     notifications has been reached */
  return (pendNtfs < ATT_NUM_SIMUL_NTF) ? FALSE : TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Get a channel for transmission of an EATT server message.
 *
 *  \param  connId    DM connection ID.
 *  \param  priority  Operation priority.
 *  \param  dataLen   Length of value data.
 *  \param  opcode    Operation opcode.
 *  \param  handle    Attribute handle.
 *
 *  \return Slot ID
 */
/*************************************************************************************************/
static uint8_t eattsGetFreeSlot(dmConnId_t connId, uint8_t priority, uint16_t dataLen,
                                uint8_t opcode, uint16_t handle)
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
          attsCcb_t *pAttsCcb = attsCcbByConnId(connId, slot);
          attCcb_t  *pAttCcb = attCcbByConnId(connId);

          if (pAttCcb && !(pAttCcb->sccb[slot].control & ATT_CCB_STATUS_RSP_PENDING) &&
                         !(pAttCcb->sccb[slot].control & ATT_CCB_STATUS_CONTEXT_LOCK))
          {
            if (pAttsCcb && !(eattsPendIndNtfHandle(pAttsCcb, opcode, handle)))
            {
              EATT_TRACE_INFO1("eattsGetFreeSlot: allocating slot: 0x%x", slot);
            
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
static void eattsL2cCocDataInd(l2cCocEvt_t *pEvt)
{
  l2cCocDataInd_t   *pDataInd = &pEvt->dataInd;
  uint8_t           opcode;
  uint8_t           method;
  uint8_t           err;
  uint16_t          attHandle;
  attsProcFcn_t     procFcn;
  dmConnId_t        connId = (dmConnId_t) pDataInd->hdr.param;
  uint8_t           slot = eattGetSlotId(connId, pDataInd->cid);
  attCcb_t          *pAttCcb = attCcbByConnId(connId);
  attsCcb_t         *pAttsCcb = attsCcbByConnId(connId, slot);
  uint16_t          len = pEvt->dataInd.dataLen;

  if (slot != ATT_BEARER_SLOT_INVALID)
  {
    /* parse opcode */
    opcode = *(pDataInd->pData);

    /* get method */
    if ((opcode <= ATT_PDU_WRITE_REQ) ||
        ((opcode >= ATT_PDU_PREP_WRITE_REQ) && (opcode <= ATT_PDU_VALUE_CNF)))
    {
      method = ATT_OPCODE_2_METHOD(opcode);
    }
    else if (opcode == ATT_PDU_WRITE_CMD)
    {
      method = ATT_METHOD_WRITE_CMD;
    }
    else if (opcode == ATT_PDU_READ_MULT_VAR_REQ)
    {
      method = ATT_METHOD_READ_MULT_VAR;
    }
    else if (opcode == ATT_PDU_SIGNED_WRITE_CMD)
    {
      method = ATT_METHOD_SIGNED_WRITE_CMD;
    }
    else
    {
      method = ATT_METHOD_ERR;
    }

    /* ignore packet if write response is pending. */
    if (pAttCcb->sccb[slot].control & ATT_CCB_STATUS_RSP_PENDING)
    {
      if (method != ATT_METHOD_VALUE_CNF)
      {
        return;
      }
    }

    /* check client's status to see if server is allowed to process this PDU. */
    err = attsCsfActClientState(connId, opcode, pEvt->dataInd.pData - L2C_PAYLOAD_START);
    if (err)
    {
      BYTES_TO_UINT16(attHandle, pEvt->dataInd.pData + ATT_HDR_LEN);
    }
    else
    {
      attHandle = ATT_HANDLE_NONE;
    }

#if defined(ATTS_ERROR_TEST) && (ATTS_ERROR_TEST == TRUE)
    if (attCb.errTest != ATT_SUCCESS)
    {
      attErrRsp(pAttCcb, ATT_BEARER_SLOT_ID, opcode, attHandle, attCb.errTest);
      return;
    }
#endif

    EATT_TRACE_INFO2("eattsL2cCocDataInd: slot: 0x%x, method: %d", slot, method);

    /* if no error process request */
    if (!err)
    {
      /* look up processing function */
      procFcn = attsProcFcnTbl[method];

      /* if method is supported */
      if (procFcn != NULL)
      {
        /* verify length */
        if (len >= attsMinPduLen[method])
        {
          /* execute processing function */
          (*procFcn)(pAttsCcb, len, pEvt->dataInd.pData - L2C_PAYLOAD_START);
          err = 0;
        }
        else
        {
          /* invalid PDU length */
          err = ATT_ERR_INVALID_PDU;
        }
      }
      else
      {
        /* PDU not supported */
        err = ATT_ERR_NOT_SUP;
      }
    }

    /* if there's an error and an error response can be sent for this opcode */
    if (err && (opcode != ATT_PDU_MTU_REQ) && (opcode != ATT_PDU_VALUE_CNF) &&
        ((opcode & ATT_PDU_MASK_COMMAND) == 0))
    {
      attErrRsp(pAttCcb, slot, opcode, attHandle, err);
    }
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
static void eattsL2cCocDataCnf(l2cCocEvt_t *pEvt)
{
  dmConnId_t connId = (dmConnId_t) pEvt->dataCnf.hdr.param;
  uint8_t slot = eattGetSlotId(connId,  pEvt->dataCnf.cid);
  attsCcb_t *pCcb;

  /* note this function is currently only called when flow is enabled */

  if (slot != ATT_BEARER_SLOT_INVALID)
  {
    /* get CCB */
    if ((pCcb = attsCcbByConnId(connId, slot)) != NULL)
    {
      /* call pending indication and notification callback */
      attsIndNtfCallback(connId, pCcb, ATT_SUCCESS);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Connection callback for ATTS.
 *
 *  \param  pCcb    ATT control block.
 *  \param  pDmEvt  DM callback event.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void eattsConnCback(attCcb_t *pCcb, dmEvt_t *pDmEvt)
{
  (void) pCcb;
  (void) pDmEvt;

  /* take no action */
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the length of a list of handle length value tupples.
 *
 *  \param  numTuples   The number of tuples.
 *  \param  pTupleList  Pointer to a list of tuples.
 *
 *  \return length of tupples.
 */
/*************************************************************************************************/
static uint16_t eattMultiNtfLen(uint16_t numTuples, eattTuple_t *pTupleList)
{
  uint8_t i;
  uint16_t len = 0;

  for (i = 0; i < numTuples; i++)
  {
    len += pTupleList[i].len + sizeof(uint16_t) * 2;
  }

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Send multiple attribute protocol Handle Value Notification.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  numTuples   The number of tuples.
 *  \param  pTupleList  Pointer to a list of tuples.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattsMultiValueNtf(dmConnId_t connId, uint8_t priority, uint16_t numTuples, eattTuple_t *pTupleList)
{
  uint16_t       valueLen = eattMultiNtfLen(numTuples, pTupleList);
  uint8_t        slot;
  bool_t         msgSent = FALSE;

  WsfTaskLock();

  slot = eattsGetFreeSlot(connId, priority, valueLen + ATT_MULT_VALUE_NTF_BUF_LEN, ATT_PDU_MULT_VALUE_NTF, 0);

  WsfTaskUnlock();

  if (slot)
  {
    /* Only send notifications and indications if client is aware of any database changes. */
    if (attsCsfIsClientChangeAware(connId, 0))
    {
      attsApiMsg_t  *pMsg;
      uint8_t       *p;
      uint8_t       i;

      /* allocate message buffer */
      if ((pMsg = WsfMsgAlloc(sizeof(attsApiMsg_t))) != NULL)
      {
        /* set parameters */
        pMsg->hdr.param = connId;
        pMsg->hdr.event = ATTS_MSG_API_VALUE_IND_NTF;
        pMsg->slot = slot;
        pMsg->pPkt = attMsgAlloc(ATT_MULT_VALUE_NTF_BUF_LEN + valueLen);

        if (pMsg->pPkt != NULL)
        {
          /* set data length and handle (ind and ntf have same header length) */
          pMsg->pPkt->len = ATT_PDU_MULT_VALUE_NTF_LEN + valueLen;

          /* build packet */
          p = (uint8_t *)pMsg->pPkt + L2C_PAYLOAD_START;
          UINT8_TO_BSTREAM(p, ATT_PDU_MULT_VALUE_NTF);

          for (i = 0; i < numTuples; i++)
          {
            UINT16_TO_BSTREAM(p, pTupleList[i].handle);
            UINT16_TO_BSTREAM(p, pTupleList[i].len);
            memcpy(p, pTupleList[i].pValue, pTupleList[i].len);
            p += pTupleList[i].len;
          }

          /* send message */
          WsfMsgSend(attCb.handlerId, pMsg);
          msgSent = TRUE;
        }
        else
        {
          /* free message buffer if packet buffer alloc failed */
          WsfMsgFree(pMsg);
        }
      }
    }

    if (!msgSent)
    {
      attCcb_t *pAttCcb = attCcbByConnId(connId);

      /* Release the slot */
      if (pAttCcb)
      {
        pAttCcb->sccb[slot].control &= ~ATT_CCB_STATUS_CONTEXT_LOCK;
      }

      /* Failed to send the packet, release the slot. */
      attExecCallback(connId, ATTS_MULT_VALUE_CNF, 0, ATT_ERR_MEMORY, 0);
    }
  }
  else
  {
    /* call callback with no channel available status */
    attExecCallback(connId, ATTS_MULT_VALUE_CNF, 0, ATT_ERR_NO_CHANNEL, 0);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Indication.
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
void EattsHandleValueInd(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t valueLen,
                         uint8_t *pValue)
{
  uint8_t slot = eattsGetFreeSlot(connId, priority, valueLen, ATT_PDU_VALUE_IND, handle);
  attsHandleValueIndNtf(connId, handle, slot, valueLen, pValue, ATT_PDU_VALUE_IND, FALSE);
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Notification.
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
void EattsHandleValueNtf(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t valueLen,
                         uint8_t *pValue)
{
  uint8_t slot = eattsGetFreeSlot(connId, priority, valueLen, ATT_PDU_VALUE_NTF, handle);
  attsHandleValueIndNtf(connId, handle, slot, valueLen, pValue, ATT_PDU_VALUE_NTF, FALSE);
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Indication without copying the attribute
 *          value data.
 *
 *          Note: attribute value buffer 'pValue' must be allocated with AttMsgAlloc().
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
void EattsHandleValueIndZeroCpy(dmConnId_t connId, uint8_t priority, uint16_t handle,
                                uint16_t valueLen, uint8_t *pValue)
{
  uint8_t slot = eattsGetFreeSlot(connId, priority, valueLen, ATT_PDU_VALUE_IND, handle);
  attsHandleValueIndNtf(connId, handle, slot, valueLen, pValue, ATT_PDU_VALUE_IND, TRUE);
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Notification without copying the attribute
 *          value data.
 *
 *          Note: attribute value buffer 'pValue' must be allocated with AttMsgAlloc().
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
void EattsHandleValueNtfZeroCpy(dmConnId_t connId, uint8_t priority, uint16_t handle,
                                uint16_t valueLen, uint8_t *pValue)
{
  uint8_t slot = eattsGetFreeSlot(connId, priority, valueLen, ATT_PDU_VALUE_NTF, handle);
  attsHandleValueIndNtf(connId, handle, slot, valueLen, pValue, ATT_PDU_VALUE_NTF, TRUE);
}

/*************************************************************************************************/
/*!
 *  \brief  Send a response to a pending write request.  For use with ATT_RSP_PENDING.
 *
 *  \param  connId      Connection ID.
 *  \param  slot        EATT channel slot ID.
 *  \param  handle      Attribute handle.
 *  \param  status      Status of the write request.
 *
 *  \return None.
 *
 *  \note   When a higher layer returns ATT_RSP_PENDING to an ATT write callback indicating the
 *          response status is pending, the higher layer must subsequently call this function
 *          with the status of the write request.
 */
/*************************************************************************************************/
void EattsContinueWriteReq(dmConnId_t connId, uint8_t slot, uint16_t handle, uint8_t status)
{
  attCcb_t *pCcb;
  uint8_t  *pBuf;
  uint8_t  *p;

  /* get connection cb for this handle */
  if ((pCcb = attCcbByConnId(connId)) == NULL)
  {
    return;
  }

  if (pCcb->sccb[slot].control & ATT_CCB_STATUS_RSP_PENDING)
  {
    /* clear response pending */
    pCcb->sccb[slot].control &= ~ATT_CCB_STATUS_RSP_PENDING;

    if (status)
    {
      attErrRsp(pCcb, slot, ATT_PDU_WRITE_REQ, handle, status);
    }
    else
    {
      if ((pBuf = attMsgAlloc(L2C_PAYLOAD_START + ATT_WRITE_RSP_LEN)) != NULL)
      {
        /* build and send PDU */
        p = pBuf + L2C_PAYLOAD_START;
        UINT8_TO_BSTREAM(p, ATT_PDU_WRITE_RSP);

        attL2cDataReq(pCcb, slot, ATT_WRITE_RSP_LEN, pBuf);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the Enhanced ATT Server.
 *
 *  \return None
 */
 /*************************************************************************************************/
void EattsInit()
{
  /* set up callback interface */
  attCb.pEnServer = &attsFcnIf;
}
