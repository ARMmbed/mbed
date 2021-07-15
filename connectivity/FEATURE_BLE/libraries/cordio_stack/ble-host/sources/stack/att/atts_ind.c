/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  ATT server indication and notification functions.
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
#include "util/bstream.h"
#include "att_api.h"
#include "att_main.h"
#include "atts_main.h"

/**************************************************************************************************
  Function Prototypes
**************************************************************************************************/

static void attsIndConnCback(attCcb_t *pCcb, dmEvt_t *pDmEvt);
static void attsIndMsgCback(attsApiMsg_t *pMsg);
static void attsIndCtrlCback(wsfMsgHdr_t *pMsg);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Interface to ATT */
static const attFcnIf_t attsIndFcnIf =
{
  attEmptyDataCback,
  attsIndCtrlCback,
  (attMsgHandler_t) attsIndMsgCback,
  attsIndConnCback
};

/*************************************************************************************************/
/*!
 *  \brief  Check if application callback is pending for indication or a given notification, or
 *          the maximum number of simultaneous notifications has been reached.
 *
 *  \param  pCcb    ATTS ind control block.
 *  \param  pPkt    Pointer to packet.
 *
 *  \return TRUE if app callback's pending or max number of simultaneous notifications reached.
 *          FALSE, otherwise.
 */
/*************************************************************************************************/
static bool_t attsPendIndNtfHandle(attsCcb_t *pCcb, attsPktParam_t *pPkt)
{
  uint8_t     opcode;
  uint8_t     pendNtfs;
  uint8_t     i;

  /* extract opcode */
  opcode = *(((uint8_t *) pPkt) + L2C_PAYLOAD_START);

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
      /* if callback pending for this handle */
      if (pCcb->pendNtfHandle[i] == pPkt->handle)
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
 *  \brief  Set pending notification callback for a given handle.
 *
 *  \param  pCcb    ATTS ind control block.
 *  \param  handle  Notification handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsSetPendNtfHandle(attsCcb_t *pCcb, uint16_t handle)
{
  uint8_t     i;

  for (i = 0; i < ATT_NUM_SIMUL_NTF; i++)
  {
    /* if entry free */
    if (pCcb->pendNtfHandle[i] == ATT_HANDLE_NONE)
    {
      /* set pending notification handle */
      pCcb->pendNtfHandle[i] = handle;
      break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Execute application callback function with confirmation event.
 *
 *  \param  connId  DM connection ID.
 *  \param  handle  Attribute handle.
 *  \param  status  Callback event status.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsExecCallback(dmConnId_t connId, uint16_t handle, uint8_t status)
{
  attExecCallback(connId, ATTS_HANDLE_VALUE_CNF, handle, status, 0);
}

/*************************************************************************************************/
/*!
 *  \brief  Call pending indication or/and notification(s) application callback.
 *
 *  \param  connId  DM connection ID.
 *  \param  pCcb    ATTS ind control block.
 *  \param  status  Callback event status.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsIndNtfCallback(dmConnId_t connId, attsCcb_t *pCcb, uint8_t status)
{
  uint8_t     i;

  /* if pending indication callback */
  if (pCcb->pendIndHandle != ATT_HANDLE_NONE)
  {
    /* call indication callback with status */
    attsExecCallback(connId, pCcb->pendIndHandle, status);
    pCcb->pendIndHandle = ATT_HANDLE_NONE;
  }

  /* if any pending notification callback */
  for (i = 0; i < ATT_NUM_SIMUL_NTF; i++)
  {
    if (pCcb->pendNtfHandle[i] != ATT_HANDLE_NONE)
    {
      /* call notification callback with status */
      attsExecCallback(connId, pCcb->pendNtfHandle[i], status);
      pCcb->pendNtfHandle[i] = ATT_HANDLE_NONE;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set up and send an attribute server indication or notification.
 *
 *  \param  pCcb    ATTS ind control block.
 *  \param  connId  DM connection ID.
 *  \param  pPkt    Pointer to packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsSetupMsg(attsCcb_t *pCcb, dmConnId_t connId, uint8_t slot, attsPktParam_t *pPkt)
{
  uint8_t     opcode;
  uint16_t    handle;

  /* extract opcode */
  opcode = *(((uint8_t *) pPkt) + L2C_PAYLOAD_START);

  /* copy handle (it may be overwritten in pPkt) */
  handle = pPkt->handle;

  /* send pdu */
  attL2cDataReq(pCcb->pMainCcb, slot, pPkt->len, (uint8_t *) pPkt);

  /* if indication store handle and start timer */
  if (opcode == ATT_PDU_VALUE_IND)
  {
    pCcb->outIndHandle = pCcb->pendIndHandle = handle;
    pCcb->outIndTimer.msg.event = ATTS_MSG_IND_TIMEOUT;
    pCcb->outIndTimer.msg.param = attMsgParam(pCcb->connId, pCcb->slot);

    WsfTimerStartSec(&pCcb->outIndTimer, pAttCfg->transTimeout);
  }
  /* else if a notification and flow not disabled call callback now */
  else if (!(pCcb->pMainCcb->sccb[slot].control & ATT_CCB_STATUS_FLOW_DISABLED))
  {
    if (opcode == ATT_PDU_MULT_VALUE_NTF)
    {
      attExecCallback(connId, ATTS_MULT_VALUE_CNF, handle, ATT_SUCCESS, 0);
    }
    else
    {
      attsExecCallback(connId, handle, ATT_SUCCESS);
    }
  }
  /* else set pending notification callback for this handle */
  else
  {
    attsSetPendNtfHandle(pCcb, handle);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Connection callback for ATTS indications/notifications.
 *
 *  \param  pCcb    ATT control block.
 *  \param  pDmEvt  DM callback event.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsIndConnCback(attCcb_t *pCcb, dmEvt_t *pDmEvt)
{
  attsCcb_t   *pAttsCcb;
  uint8_t      status;
  uint8_t      i;

  /* if connection opened */
  if (pDmEvt->hdr.event == DM_CONN_OPEN_IND)
  {

  }
  /* if connection closed */
  else if (pDmEvt->hdr.event == DM_CONN_CLOSE_IND)
  {
    /* set status */
    if (pDmEvt->connClose.hdr.status == HCI_SUCCESS)
    {
      status = pDmEvt->connClose.reason + ATT_HCI_ERR_BASE;
    }
    else
    {
      status = pDmEvt->connClose.hdr.status + ATT_HCI_ERR_BASE;
    }

    for (i = 0; i < ATT_BEARER_MAX; i++)
    {
      /* get server control block directly */
      pAttsCcb = &attsCb.ccb[pCcb->connId - 1][i];

      /* if outstanding indication */
      if (pAttsCcb->outIndHandle != ATT_HANDLE_NONE)
      {
        /* stop timer */
        WsfTimerStop(&pAttsCcb->outIndTimer);
        pAttsCcb->outIndHandle = ATT_HANDLE_NONE;
      }

      /* call pending indication and notification callback */
      attsIndNtfCallback(pCcb->connId, pAttsCcb, status);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  WSF message handler callback for ATTS indications/notifications.
 *
 *  \param  pDmEvt  DM callback event.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsIndMsgCback(attsApiMsg_t *pMsg)
{
  attsCcb_t   *pCcb;

  /* if an API message to send packet */
  if (pMsg->hdr.event == ATTS_MSG_API_VALUE_IND_NTF)
  {
    /* get CCB and verify connection still in use */
    if ((pCcb = attsCcbByConnId((dmConnId_t) pMsg->hdr.param, pMsg->slot)) == NULL)
    {
      /* if message has a packet buffer free packet buffer */
      WsfMsgFree(pMsg->pPkt);

      /* ignore if connection not in use */
      return;
    }

    /* Release the EATT context lock */
    pCcb->pMainCcb->sccb[pMsg->slot].control &= ~ATT_CCB_STATUS_CONTEXT_LOCK;

    /* verify no API message already pending */
    if (attsPendIndNtfHandle(pCcb, pMsg->pPkt))
    {
      /* call callback with failure status and free packet buffer */
      attsExecCallback((dmConnId_t) pMsg->hdr.param, pMsg->pPkt->handle, ATT_ERR_OVERFLOW);
      WsfMsgFree(pMsg->pPkt);
    }
    /* otherwise ready to send; set up request */
    else
    {
      attsSetupMsg(pCcb, (dmConnId_t) pMsg->hdr.param, pMsg->slot, pMsg->pPkt);
    }
  }
  /* else if indication timeout */
  else if (pMsg->hdr.event == ATTS_MSG_IND_TIMEOUT)
  {
    dmConnId_t connId;
    uint8_t slot;

    attDecodeMsgParam(pMsg->hdr.param, &connId, &slot);
    pMsg->hdr.param = connId;

    if ((pCcb = attsCcbByConnId(connId, slot)) == NULL)
    {
      /* ignore if connection not in use */
      return;
    }

    /* if outstanding indication */
    if (pCcb->outIndHandle != ATT_HANDLE_NONE)
    {
      /* clear out handle */
      pCcb->outIndHandle = ATT_HANDLE_NONE;

      /* call callback with timeout error */
      attsExecCallback(connId, pCcb->pendIndHandle, ATT_ERR_TIMEOUT);
      pCcb->pendIndHandle = ATT_HANDLE_NONE;
      pCcb->pMainCcb->sccb[pMsg->slot].control |= ATT_CCB_STATUS_TX_TIMEOUT;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  L2CAP control callback.
 *
 *  \param  pMsg    Pointer to message structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsIndCtrlCback(wsfMsgHdr_t *pMsg)
{
  attsCcb_t *pCcb;

  /* note this function is currently only called when flow is enabled */

  /* get CCB */
  if ((pCcb = attsCcbByConnId((dmConnId_t) pMsg->param, ATT_BEARER_SLOT_ID)) != NULL)
  {
    /* call pending indication and notification callback */
    attsIndNtfCallback((dmConnId_t) pMsg->param, pCcb, ATT_SUCCESS);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Indication or Notification.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  slot        EATT/ATT slot ID.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *  \param  opcode      Opcode for notification or indication.
 *  \param  zeroCpy     Whether or not to copy attribute value data into new buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsHandleValueIndNtf(dmConnId_t connId, uint16_t handle, uint8_t slot, uint16_t valueLen,
                                  uint8_t *pValue, uint8_t opcode, bool_t zeroCpy)
{
  attsCcb_t     *pCcb;
  uint16_t       mtu;
  bool_t         transTimedOut;
  bool_t         pktSent = FALSE;

  WsfTaskLock();

  /* get CCB and verify connection still in use */
  if ((pCcb = attsCcbByConnId(connId, slot)) != NULL)
  {
    /* get MTU size */
    mtu = pCcb->pMainCcb->sccb[slot].mtu;
    transTimedOut = !!(pCcb->pMainCcb->sccb[slot].control & ATT_CCB_STATUS_TX_TIMEOUT);
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
      /* Only send notifications and indications if client is aware of any database changes. */
      if (attsCsfIsClientChangeAware(connId, handle))
      {
        /* if packet length is less than or equal to negotiated MTU */
        if ((valueLen + ATT_VALUE_NTF_LEN) <= mtu)
        {
          attsApiMsg_t  *pMsg;
          uint8_t       *p;

          /* allocate message buffer */
          if ((pMsg = WsfMsgAlloc(sizeof(attsApiMsg_t))) != NULL)
          {
            /* set parameters */
            pMsg->hdr.param = connId;
            pMsg->hdr.event = ATTS_MSG_API_VALUE_IND_NTF;
            pMsg->slot = slot;

            if (zeroCpy)
            {
              /* use packet buffer provided */
              pMsg->pPkt = (attsPktParam_t *)(pValue - ATT_VALUE_IND_NTF_BUF_LEN);
            }
            else
            {
              /* allocate packet buffer */
              pMsg->pPkt = attMsgAlloc(ATT_VALUE_IND_NTF_BUF_LEN + valueLen);
            }

            if (pMsg->pPkt != NULL)
            {
              /* set data length and handle (ind and ntf have same header length) */
              pMsg->pPkt->len = ATT_VALUE_IND_LEN + valueLen;
              pMsg->pPkt->handle = handle;

              /* build packet */
              p = (uint8_t *)pMsg->pPkt + L2C_PAYLOAD_START;
              UINT8_TO_BSTREAM(p, opcode);
              UINT16_TO_BSTREAM(p, handle);

              if (!zeroCpy)
              {
                memcpy(p, pValue, valueLen);
              }

              /* send message */
              WsfMsgSend(attCb.handlerId, pMsg);
              pktSent = TRUE;
            }
            else
            {
              /* free message buffer if packet buffer alloc failed */
              WsfMsgFree(pMsg);
            }
          }
        }
        /* packet length exceeds MTU size */
        else
        {
          /* call callback with failure status */
          attsExecCallback(connId, handle, ATT_ERR_MTU_EXCEEDED);
        }
      }
    }
    else
    /* transaction's timed out */
    {
      /* call callback with failure status */
      attsExecCallback(connId, handle, ATT_ERR_TIMEOUT);
    }
  }

  /* if packet wasn't sent and it's a zero-copy packet */
  if (!pktSent)
  {
    attCcb_t *pAttCcb = attCcbByConnId(connId);

    /* Release the slot */
    if (pAttCcb)
    {
      pAttCcb->sccb[slot].control &= ~ATT_CCB_STATUS_CONTEXT_LOCK;
    }

    if (zeroCpy)
    {
      /* free packet buffer provided */
      AttMsgFree(pValue, opcode);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process received handle value confirm packet.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcValueCnf(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  /* if an outstanding indication */
  if (pCcb->outIndHandle != ATT_HANDLE_NONE)
  {
    attsAttr_t  *pAttr;
    attsGroup_t *pGroup;

    /* stop indication timer */
    WsfTimerStop(&pCcb->outIndTimer);

    /* if client is unaware of a database update and this confirmation is for a service change
     * indication, transition client to the change-aware state */
    if (((pAttr = attsFindByHandle(pCcb->outIndHandle, &pGroup)) != NULL) &&
        (memcmp(pAttr->pUuid, attScChUuid, ATT_16_UUID_LEN) == 0) &&
        (AttsCsfGetClientChangeAwareState(pCcb->connId) != ATTS_CLIENT_CHANGE_AWARE))
    {
      AttsCsfSetClientChangeAwareState(pCcb->connId, ATTS_CLIENT_CHANGE_AWARE);
    }

    /* clear outstanding indication */
    pCcb->outIndHandle = ATT_HANDLE_NONE;

    /* call callback if flow control permits */
    if (!(pCcb->pMainCcb->sccb[pCcb->slot].control & ATT_CCB_STATUS_FLOW_DISABLED))
    {
      attsExecCallback(pCcb->connId, pCcb->pendIndHandle, ATT_SUCCESS);
      pCcb->pendIndHandle = ATT_HANDLE_NONE;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize ATT server for indications/notifications.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsIndInit(void)
{
  uint8_t    i, j;
  attsCcb_t  *pCcb;

  /* Initialize control block CCBs */
  for (i = 0; i < DM_CONN_MAX; i++)
  {
    for (j = 0; j < ATT_BEARER_MAX; j++)
    {
      pCcb = &attsCb.ccb[i][j];

      /* initialize timer */
      pCcb->outIndTimer.handlerId = attCb.handlerId;
      pCcb->outIndTimer.msg.param = i + 1;  /* param stores the conn id */
    }
  }

  /* set up callback interface */
  attsCb.pInd = &attsIndFcnIf;
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Indication.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsHandleValueInd(dmConnId_t connId, uint16_t handle, uint16_t valueLen, uint8_t *pValue)
{
  attsHandleValueIndNtf(connId, handle, ATT_BEARER_SLOT_ID, valueLen, pValue, ATT_PDU_VALUE_IND, FALSE);
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Notification.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsHandleValueNtf(dmConnId_t connId, uint16_t handle, uint16_t valueLen, uint8_t *pValue)
{
  attsHandleValueIndNtf(connId, handle, ATT_BEARER_SLOT_ID, valueLen, pValue, ATT_PDU_VALUE_NTF, FALSE);
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Indication without copying the attribute
 *          value data.
 *
 *          Note: attribute value buffer 'pValue' must be allocated with AttMsgAlloc().
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsHandleValueIndZeroCpy(dmConnId_t connId, uint16_t handle, uint16_t valueLen,
                               uint8_t *pValue)
{
  attsHandleValueIndNtf(connId, handle, ATT_BEARER_SLOT_ID, valueLen, pValue, ATT_PDU_VALUE_IND, TRUE);
}

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Notification without copying the attribute
 *          value data.
 *
 *          Note: attribute value buffer 'pValue' must be allocated with AttMsgAlloc().
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsHandleValueNtfZeroCpy(dmConnId_t connId, uint16_t handle, uint16_t valueLen,
                               uint8_t *pValue)
{
  attsHandleValueIndNtf(connId, handle, ATT_BEARER_SLOT_ID, valueLen, pValue, ATT_PDU_VALUE_NTF, TRUE);
}
