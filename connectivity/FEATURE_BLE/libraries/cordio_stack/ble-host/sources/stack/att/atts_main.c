/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  ATT server main module.
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
#include "wsf_buf.h"
#include "wsf_msg.h"
#include "util/bstream.h"
#include "util/wstr.h"
#include "att_api.h"
#include "att_main.h"
#include "atts_main.h"
#include "att_uuid.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* ATTS control block */

/**************************************************************************************************
  Function Prototypes
**************************************************************************************************/

static void attsDataCback(uint16_t handle, uint16_t len, uint8_t *pPacket);
static void attsConnCback(attCcb_t *pCcb, dmEvt_t *pDmEvt);
void attsMsgCback(wsfMsgHdr_t *pMsg);
static void attsL2cCtrlCback(wsfMsgHdr_t *pMsg);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Interface to ATT */
static const attFcnIf_t attsFcnIf =
{
  attsDataCback,
  attsL2cCtrlCback,
  (attMsgHandler_t) attsMsgCback,
  attsConnCback
};

/* Minimum PDU lengths, indexed by method */
const uint8_t attsMinPduLen[] =
{
  0,                                  /* ATT_METHOD_ERR */
  ATT_MTU_REQ_LEN,                    /* ATT_METHOD_MTU */
  ATT_FIND_INFO_REQ_LEN,              /* ATT_METHOD_FIND_INFO */
  ATT_FIND_TYPE_REQ_LEN,              /* ATT_METHOD_FIND_TYPE */
  ATT_READ_TYPE_REQ_LEN,              /* ATT_METHOD_READ_TYPE */
  ATT_READ_REQ_LEN,                   /* ATT_METHOD_READ */
  ATT_READ_BLOB_REQ_LEN,              /* ATT_METHOD_READ_BLOB */
  ATT_READ_MULT_REQ_LEN + 4,          /* ATT_METHOD_READ_MULTIPLE */
  ATT_READ_GROUP_TYPE_REQ_LEN,        /* ATT_METHOD_READ_GROUP_TYPE */
  ATT_WRITE_REQ_LEN,                  /* ATT_METHOD_WRITE */
  ATT_WRITE_CMD_LEN,                  /* ATT_METHOD_WRITE_CMD */
  ATT_PREP_WRITE_REQ_LEN,             /* ATT_METHOD_PREPARE_WRITE */
  ATT_EXEC_WRITE_REQ_LEN,             /* ATT_METHOD_EXECUTE_WRITE */
  0,                                  /* ATT_METHOD_VALUE_NTF */
  0,                                  /* ATT_METHOD_VALUE_IND */
  ATT_VALUE_CNF_LEN,                  /* ATT_METHOD_VALUE_CNF */
  0,                                  /* ATT_METHOD_READ_MULT_VAR */
  ATT_SIGNED_WRITE_CMD_LEN            /* ATT_METHOD_SIGNED_WRITE_CMD */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* PDU processing function lookup table, indexed by method */
attsProcFcn_t attsProcFcnTbl[ATT_METHOD_SIGNED_WRITE_CMD+1] =
{
  NULL,                               /* ATT_METHOD_ERR */
  attsProcMtuReq,                     /* ATT_METHOD_MTU */
  attsProcFindInfoReq,                /* ATT_METHOD_FIND_INFO */
  attsProcFindTypeReq,                /* ATT_METHOD_FIND_TYPE */
  attsProcReadTypeReq,                /* ATT_METHOD_READ_TYPE */
  attsProcReadReq,                    /* ATT_METHOD_READ */
  attsProcReadBlobReq,                /* ATT_METHOD_READ_BLOB */
  attsProcReadMultReq,                /* ATT_METHOD_READ_MULT */
  attsProcReadGroupTypeReq,           /* ATT_METHOD_READ_GROUP_TYPE */
  attsProcWrite,                      /* ATT_METHOD_WRITE */
  attsProcWrite,                      /* ATT_METHOD_WRITE_CMD */
  attsProcPrepWriteReq,               /* ATT_METHOD_PREP_WRITE */
  attsProcExecWriteReq,               /* ATT_METHOD_EXEC_WRITE */
  NULL,                               /* ATT_METHOD_VALUE_NTF */
  NULL,                               /* ATT_METHOD_VALUE_IND */
  attsProcValueCnf,                   /* ATT_METHOD_VALUE_CNF */
  attsProcReadMultiVarReq,            /* ATT_METHOD_READ_MULT_VAR */
  NULL                                /* ATT_METHOD_SIGNED_WRITE_CMD */
};

/* Control block */
attsCb_t attsCb;

/*************************************************************************************************/
/*!
 *  \brief  Data callback for ATTS.
 *
 *  \param  handle    The connection handle.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsDataCback(uint16_t handle, uint16_t len, uint8_t *pPacket)
{
  uint8_t       opcode;
  uint8_t       method;
  uint8_t       err;
  attsProcFcn_t procFcn;
  attsCcb_t     *pCcb;
  uint16_t      attHandle;

  /* get connection cb for this handle */
  if ((pCcb = attsCcbByHandle(handle, ATT_BEARER_SLOT_ID)) == NULL)
  {
    return;
  }

  /* parse opcode */
  opcode = *(pPacket + L2C_PAYLOAD_START);

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
  if (pCcb->pMainCcb->sccb[ATT_BEARER_SLOT_ID].control & ATT_CCB_STATUS_RSP_PENDING)
  {
    if (method != ATT_METHOD_VALUE_CNF)
    {
      return;
    }
  }

  /* check client's status to see if server is allowed to process this PDU. */
  err = attsCsfActClientState(pCcb->connId, opcode, pPacket);
  if (err)
  {
    BYTES_TO_UINT16(attHandle, pPacket + L2C_PAYLOAD_START + ATT_HDR_LEN);
  }
  else
  {
    attHandle = ATT_HANDLE_NONE;
  }

#if defined(ATTS_ERROR_TEST) && (ATTS_ERROR_TEST == TRUE)
  if (attCb.errTest != ATT_SUCCESS)
  {
    BYTES_TO_UINT16(attHandle, pPacket + L2C_PAYLOAD_START + ATT_HDR_LEN);
    attErrRsp(pCcb->pMainCcb, ATT_BEARER_SLOT_ID, opcode, attHandle, attCb.errTest);
    return;
  }
#endif

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
        (*procFcn)(pCcb, len, pPacket);
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
    attErrRsp(pCcb->pMainCcb, ATT_BEARER_SLOT_ID, opcode, attHandle, err);
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
static void attsConnCback(attCcb_t *pCcb, dmEvt_t *pDmEvt)
{
  uint8_t i;

  /* if connection closed */
  if (pDmEvt->hdr.event == DM_CONN_CLOSE_IND)
  {
    for (i = 0; i < ATT_BEARER_MAX; i++)
    {
      attsCcb_t *pAttsCb = &attsCb.ccb[pCcb->connId - 1][i];

      /* clear prepare write queue */
      attsClearPrepWrites(pAttsCb);

      /* stop service discovery idle timer, if running */
      if (DmConnCheckIdle(pCcb->connId) & DM_IDLE_ATTS_DISC)
      {
        WsfTimerStop(&pAttsCb->idleTimer);
      }

      pCcb->sccb[i].control &= ~ATT_CCB_STATUS_CONTEXT_LOCK;
    }
  }

  /* pass event to indication interface */
  (*attsCb.pInd->connCback)(pCcb, pDmEvt);
}

/*************************************************************************************************/
/*!
 *  \brief  WSF message handler callback for ATTS.
 *
 *  \param  pMsg  DM callback event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsMsgCback(wsfMsgHdr_t *pMsg)
{
  /* handle service discovery idle timeout */
  if (pMsg->event == ATTS_MSG_IDLE_TIMEOUT)
  {
    /* set channel as idle */
    DmConnSetIdle((dmConnId_t) pMsg->param, DM_IDLE_ATTS_DISC, DM_CONN_IDLE);
  }
  /* pass event to indication interface */
  else if (pMsg->event <= ATTS_MSG_IND_TIMEOUT)
  {
    (*attsCb.pInd->msgCback)(pMsg);
  }
  /* pass event to signed data interface */
  else if (pMsg->event == ATTS_MSG_SIGN_CMAC_CMPL)
  {
    (*attsCb.signMsgCback)(pMsg);
  }
  else if (pMsg->event == ATTS_MSG_DBH_CMAC_CMPL)
  {
    /* handle database hash update */
    attsProcessDatabaseHashUpdate((secCmacMsg_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  L2C control callback for ATTS.
 *
 *  \param  pMsg    Pointer to message structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsL2cCtrlCback(wsfMsgHdr_t *pMsg)
{
  /* pass event to indication interface */
  (*attsCb.pInd->ctrlCback)(pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Clear the prepared write queue.
 *
 *  \param  pCcb    ATT control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsClearPrepWrites(attsCcb_t *pCcb)
{
  void *pBuf;

  while ((pBuf = WsfQueueDeq(&attsCb.prepWriteQueue[pCcb->connId-1])) != NULL)
  {
    WsfBufFree(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set connection idle status to busy for service discovery.
 *
 *  \param  pCcb    ATT control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsDiscBusy(attsCcb_t *pCcb)
{
  if (pAttCfg->discIdleTimeout > 0)
  {
    /* set channel as busy */
    DmConnSetIdle(pCcb->pMainCcb->connId, DM_IDLE_ATTS_DISC, DM_CONN_BUSY);

    /* start service discovery idle timer */
    pCcb->idleTimer.handlerId = attCb.handlerId;
    pCcb->idleTimer.msg.event = ATTS_MSG_IDLE_TIMEOUT;
    pCcb->idleTimer.msg.param = pCcb->pMainCcb->connId;
    WsfTimerStartSec(&pCcb->idleTimer, pAttCfg->discIdleTimeout);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process updated database hash.
 *
 *  \param  pMsg    Event containing the new database hash.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcessDatabaseHashUpdate(secCmacMsg_t *pMsg)
{
  attEvt_t evt;
  attsAttr_t *pAttr;
  attsGroup_t *pGroup;
  uint16_t dbhCharHandle;

  /* send to application */
  evt.hdr.event = ATTS_DB_HASH_CALC_CMPL_IND;
  evt.hdr.status = ATT_SUCCESS;
  evt.hdr.param = DM_CONN_ID_NONE;

  evt.valueLen = ATT_DATABASE_HASH_LEN;
  evt.handle = ATT_HANDLE_NONE;
  evt.continuing = FALSE;
  evt.mtu = 0;

  /* free plain text buffer */
  if (pMsg->pPlainText != NULL)
  {
    WsfBufFree(pMsg->pPlainText);
    pMsg->pPlainText = NULL;
  }


  /* copy in little endian */
  WStrReverse(pMsg->pCiphertext, ATT_DATABASE_HASH_LEN);
  evt.pValue = pMsg->pCiphertext;

  /* find GATT database handle */
  dbhCharHandle = attsFindUuidInRange(ATT_HANDLE_START, ATT_HANDLE_MAX, ATT_16_UUID_LEN,
                                      (uint8_t *) attGattDbhChUuid, &pAttr, &pGroup);

  if (dbhCharHandle != ATT_HANDLE_NONE)
  {
    /* Set hash in service. */
    AttsSetAttr(dbhCharHandle, ATT_DATABASE_HASH_LEN, evt.pValue);
  }

  /* set hash update complete */
  attsCsfSetHashUpdateStatus(FALSE);

  attCb.cback(&evt);
}

/*************************************************************************************************/
/*!
 *  \brief  Check if any clients are pending on a new database hash value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsCheckPendDbHashReadRsp(void)
{
  for (uint8_t i = 0; i < DM_CONN_MAX; i++)
  {
    attCcb_t *pCcb = &attCb.ccb[i];

    if (pCcb->pPendDbHashRsp)
    {
      uint8_t *pBuf;

      /* allocate max size buffer for response */
      if ((pBuf = attMsgAlloc(pCcb->sccb[ATT_BEARER_SLOT_ID].mtu + L2C_PAYLOAD_START)) != NULL)
      {
        uint8_t *p;
        attsAttr_t  *pAttr;
        attsGroup_t *pGroup;

        p = pBuf + L2C_PAYLOAD_START;
        UINT8_TO_BSTREAM(p, ATT_PDU_READ_TYPE_RSP);

        /* set length parameter in response message */
        UINT8_TO_BSTREAM(p, ATT_DATABASE_HASH_LEN + sizeof(uint16_t));

        /* copy result to response message */
        UINT16_TO_BSTREAM(p, pCcb->pPendDbHashRsp->handle);

        if ((pAttr = attsFindByHandle(pCcb->pPendDbHashRsp->handle, &pGroup)) != NULL)
        {
          memcpy(p, pAttr->pValue, *pAttr->pLen);
          p += *pAttr->pLen;

          L2cDataReq(L2C_CID_ATT, pCcb->handle, p - (pBuf + L2C_PAYLOAD_START), pBuf);
        }
        else
        {
          attErrRsp(pCcb, ATT_BEARER_SLOT_ID, ATT_PDU_READ_TYPE_REQ, pCcb->pPendDbHashRsp->startHandle, ATT_ERR_NOT_FOUND);
        }
      }
      else
      {
        attErrRsp(pCcb, ATT_BEARER_SLOT_ID, ATT_PDU_READ_TYPE_REQ, pCcb->pPendDbHashRsp->startHandle, ATT_ERR_RESOURCES);
      }

      /* Free pending state information. */
      WsfBufFree(pCcb->pPendDbHashRsp);
      pCcb->pPendDbHashRsp = NULL;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Determine if attribute is hashable and return length of hashable data.
 *
 *  \param  pAttr    Attribute to check for inclusion in hash.
 *
 *  \return 0 if not hashable, else length of hashable data in bytes.
 *
 *  \note Hashable attributes include Primary Service, Secondary Service, Included Service,
 *        Characteristic Declaration and Characteristic Extended Properties which contribute their
 *        ATT handle, ATT type and ATT value. Hashable attributes also include Characteristic User
 *        Description, Client Characteristic Configuration, Server Characteristic Configuration,
 *        Characteristic Format, and Characteristic Aggreate Format which contribute their ATT
 *        handle and ATT type.
 */
/*************************************************************************************************/
uint16_t attsIsHashableAttr(attsAttr_t *pAttr)
{
  /* Initialize length to 2 for ATT Handle length */
  uint16_t length = 2;
  uint16_t uuid;
  static bool_t isAttrCharVal = FALSE;

  /* Characteristic values are skipped */
  if (isAttrCharVal)
  {
    isAttrCharVal = FALSE;
    return 0;
  }

  BYTES_TO_UINT16(uuid, pAttr->pUuid);
  switch (uuid)
  {
    /* Top cases include Attribute Value length */
    case ATT_UUID_CHARACTERISTIC:
      /* Set the next characteristic in database to be skipped */
      isAttrCharVal = TRUE;
      /* Fallthrough */
    case ATT_UUID_PRIMARY_SERVICE:
    case ATT_UUID_SECONDARY_SERVICE:
    case ATT_UUID_INCLUDE:
    case ATT_UUID_CHARACTERISTIC_EXT:
      length += *pAttr->pLen;
      /* Fallthrough */

    /* All values fall through to include Attribute Type length */
    case ATT_UUID_CHAR_USER_DESC:
    case ATT_UUID_CLIENT_CHAR_CONFIG:
    case ATT_UUID_SERVER_CHAR_CONFIG:
    case ATT_UUID_AGGREGATE_FORMAT:
      if (pAttr->settings & ATTS_SET_UUID_128)
      {
        length += 16;
      }
      else
      {
        length += 2;
      }
      break;

    default:
      length = 0;
      break;
  }

  return length;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the ATTC connection control block connection ID.
 *
 *  \param  connId    Connection ID.
 *
 *  \return Pointer to connection control block or NULL if not in use.
 */
/*************************************************************************************************/
attsCcb_t *attsCcbByConnId(dmConnId_t connId, uint8_t slot)
{
  if (DmConnInUse(connId))
  {
    return &attsCb.ccb[connId - 1][slot];
  }

  ATT_TRACE_WARN1("atts ccb not in use: %d", connId);
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the connection control block for the given handle.
 *
 *  \param  handle    The connection handle.
 *
 *  \return Pointer to connection control block or NULL if not found.
 */
/*************************************************************************************************/
attsCcb_t *attsCcbByHandle(uint16_t handle, uint8_t slot)
{
  dmConnId_t  connId;

  if ((connId = DmConnIdByHandle(handle)) != DM_CONN_ID_NONE)
  {
    return &attsCb.ccb[connId - 1][slot];
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize ATT server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsInit(void)
{
  uint8_t   i, j;
  attsCcb_t *pCcb;

  /* Initialize control block */
  WSF_QUEUE_INIT(&attsCb.groupQueue);
  attsCb.pInd = &attFcnDefault;
  attsCb.signMsgCback = (attMsgHandler_t) attEmptyHandler;

  for (i = 0; i < DM_CONN_MAX; i++)
  {
    for (j = 0; j < ATT_BEARER_MAX; j++)
    {
      pCcb = &attsCb.ccb[i][j];

      /* set pointer to main CCB */
      pCcb->pMainCcb = &attCb.ccb[i];

      pCcb->connId = i + 1;
      pCcb->slot = j;
    }
  }

  /* set up callback interfaces */
  attCb.pServer = &attsFcnIf;
}

/*************************************************************************************************/
/*!
 *  \brief  Register an authorization callback with the attribute server.
 *
 *  \param  cback  Client callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsAuthorRegister(attsAuthorCback_t cback)
{
  attsCb.authorCback = cback;
}

/*************************************************************************************************/
/*!
 *  \brief  Create hash from the database string.
 *
 *  \param  pKey    Key for hashing.
 *  \param  pMsg    Plaintext to hash.
 *  \param  msgLen  Length of Plaintext data.
 *
 *  \return \ref TRUE if successful, \ref FALSE if not.
 */
/*************************************************************************************************/
bool_t AttsHashDatabaseString(uint8_t *pKey, uint8_t *pMsg, uint16_t msgLen)
{
  return SecCmac(pKey, pMsg, msgLen, attCb.handlerId, 0, ATTS_MSG_DBH_CMAC_CMPL);
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate database hash from the GATT database.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCalculateDbHash(void)
{
  uint16_t msgLen = 0;
  uint8_t *pMsg;
  attsGroup_t *pGroup = (attsGroup_t *) attsCb.groupQueue.pHead;

  /* Determine length of message. */
  while (pGroup != NULL)
  {
    uint8_t numAttrs = (pGroup->endHandle - pGroup->startHandle) + 1;

    for (attsAttr_t *pAttr = pGroup->pAttr; numAttrs != 0; numAttrs--, pAttr++)
    {
      msgLen += attsIsHashableAttr(pAttr);
    }

    pGroup = pGroup->pNext;
  }

  /* Allocate buffer for message. */
  if ((pMsg = WsfBufAlloc(msgLen)) != NULL)
  {
    pGroup = (attsGroup_t *)attsCb.groupQueue.pHead;
    uint8_t hashingKey[16] = { 0, };
    uint8_t *p = pMsg;

    /* For each service in services */
    while (pGroup)
    {
      uint16_t attHandle = pGroup->startHandle;

      /* For each attribute in the service */
      for (attsAttr_t *pAttr = pGroup->pAttr; attHandle <= pGroup->endHandle; attHandle++, pAttr++)
      {
        uint16_t valLen;
        uint8_t uuidLen = 2;

        valLen = attsIsHashableAttr(pAttr);
        if (valLen)
        {
          /* Add handle */
          UINT16_TO_BSTREAM(p, attHandle);

          /* Add attribute type*/
          if (pAttr->settings & ATTS_SET_UUID_128)
          {
            memcpy(p, pAttr->pUuid, 16);
            p += 16;
            uuidLen = 16;
          }
          else
          {
            uint16_t uuid;
            BYTES_TO_UINT16(uuid, pAttr->pUuid);
            UINT16_TO_BSTREAM(p,uuid);
          }

          /* Add Attribute value if required */
          if (valLen - (uuidLen + 2))
          {
            memcpy(p, pAttr->pValue, *pAttr->pLen);
            p += *pAttr->pLen;
          }
        }
      }

      pGroup = pGroup->pNext;
    }

    /* Send to CMAC */
    if (AttsHashDatabaseString(hashingKey, pMsg, msgLen))
    {
      return;
    }
  }

  /* Assert on failure to initiate database hash generation. */
  WSF_ASSERT(FALSE);
}

/*************************************************************************************************/
/*!
 *  \brief  Add an attribute group to the attribute server.
 *
 *  \param  pGroup    Pointer to an attribute group structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsAddGroup(attsGroup_t *pGroup)
{
  attsGroup_t   *pElem;
  attsGroup_t   *pPrev = NULL;

  /* task schedule lock */
  WsfTaskLock();

  pElem = (attsGroup_t *) attsCb.groupQueue.pHead;

  /* iterate over queue sorted by increasing handle value */
  while (pElem != NULL)
  {
    if (pGroup->startHandle < pElem->startHandle)
    {
      break;
    }
    pPrev = pElem;
    pElem = pElem->pNext;
  }

  /* insert new group */
  WsfQueueInsert(&attsCb.groupQueue, pGroup, pPrev);

  /* set database hash update status to true until a new hash is generated */
  attsCsfSetHashUpdateStatus(TRUE);

  /* task schedule unlock */
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Remove an attribute group from the attribute server.
 *
 *  \param  startHandle  Start handle of attribute group to be removed.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsRemoveGroup(uint16_t startHandle)
{
  attsGroup_t   *pElem;
  attsGroup_t   *pPrev = NULL;

  /* task schedule lock */
  WsfTaskLock();

  pElem = (attsGroup_t *) attsCb.groupQueue.pHead;

  /* find group in queue */
  while (pElem != NULL)
  {
    if (pElem->startHandle == startHandle)
    {
      break;
    }
    pPrev = pElem;
    pElem = pElem->pNext;
  }

  /* if group found remove from queue */
  if (pElem != NULL)
  {
    WsfQueueRemove(&attsCb.groupQueue, pElem, pPrev);
  }
  else
  {
    ATT_TRACE_WARN1("AttsRemoveGroup start handle not found: 0x%04x", startHandle);
  }

  /* set database hash update status to true until a new hash is generated */
  attsCsfSetHashUpdateStatus(TRUE);

  /* task schedule unlock */
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set an attribute value in the attribute server.
 *
 *  \param  handle    Attribute handle.
 *  \param  valueLen  Attribute length.
 *  \param  pValue    Attribute value.
 *
 *  \return ATT_SUCCESS if successful otherwise error.
 */
/*************************************************************************************************/
uint8_t AttsSetAttr(uint16_t handle, uint16_t valueLen, uint8_t *pValue)
{
  attsAttr_t  *pAttr;
  attsGroup_t *pGroup;
  uint8_t     err = ATT_SUCCESS;

  WsfTaskLock();

  /* find attribute */
  if ((pAttr = attsFindByHandle(handle, &pGroup)) != NULL)
  {
    /* verify write length */
    if (valueLen > pAttr->maxLen)
    {
      err = ATT_ERR_LENGTH;
    }
    else
    {
      /* set attribute value */
      memcpy(pAttr->pValue, pValue, valueLen);

      /* set the length if variable length attribute */
      if ((pAttr->settings & ATTS_SET_VARIABLE_LEN) != 0)
      {
        *(pAttr->pLen) = valueLen;
      }
    }
  }
  /* else attribute not found */
  else
  {
    err = ATT_ERR_NOT_FOUND;
  }

  WsfTaskUnlock();

  return err;
}

/*************************************************************************************************/
/*!
 *  \brief  Get an attribute value in the attribute server.  Tasks should be locked before
 *          calling this function and remain locked until pLen and pValue are no longer used.
 *
 *  \param  handle    Attribute handle.
 *  \param  pLen      Returned attribute length pointer.
 *  \param  pValue    Returned attribute value pointer.
 *
 *  \return ATT_SUCCESS if successful or other error code if failure.
 */
/*************************************************************************************************/
uint8_t AttsGetAttr(uint16_t handle, uint16_t *pLen, uint8_t **pValue)
{
  attsAttr_t  *pAttr;
  attsGroup_t *pGroup;
  uint8_t     err = ATT_SUCCESS;

  /* find attribute */
  if ((pAttr = attsFindByHandle(handle, &pGroup)) != NULL)
  {
    /* set length and value pointers */
    *pLen = *(pAttr->pLen);
    *pValue = pAttr->pValue;
  }
  /* else attribute not found */
  else
  {
    err = ATT_ERR_NOT_FOUND;
  }

  return err;
}

/*************************************************************************************************/
/*!
 *  \brief  For testing purposes only.
 *
 *  \param  status    ATT status
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsErrorTest(uint8_t status)
{
  attCb.errTest = status;
}
