/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  ATT server optional read PDU processing functions.
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
#include "wsf_buf.h"
#include "wsf_trace.h"
#include "wsf_msg.h"
#include "util/bstream.h"
#include "att_api.h"
#include "att_main.h"
#include "atts_main.h"

/*************************************************************************************************/
/*!
 *  \brief  Find the first attribute within the given handle range with a matching UUID.
 *
 *  \param  startHandle   Starting attribute handle.
 *  \param  endHandle     Ending attribute handle.
 *  \param  uuidLen       UUID length, either 2 or 16.
 *  \param  pUUID         Pointer to UUID.
 *  \param  pAttr         Return value pointer to found attribute.
 *  \param  pAttrGroup    Return value pointer to found attribute's group.
 *
 *  \return Attribute handle or ATT_HANDLE_NONE if not found.
 */
/*************************************************************************************************/
uint16_t attsFindUuidInRange(uint16_t startHandle, uint16_t endHandle, uint8_t uuidLen,
                             uint8_t *pUuid, attsAttr_t **pAttr, attsGroup_t **pAttrGroup)
{
  attsGroup_t *pGroup;

  /* iterate over attribute group list */
  for (pGroup = attsCb.groupQueue.pHead; pGroup != NULL; pGroup = pGroup->pNext)
  {
    /* if start handle is less than group start handle but handle range is within group */
    if ((startHandle < pGroup->startHandle) && (endHandle >= pGroup->startHandle))
    {
      /* set start handle to first handle in group */
      startHandle = pGroup->startHandle;
    }

    /*  if start handle within handle range of group */
    if ((startHandle >= pGroup->startHandle) && (startHandle <= pGroup->endHandle))
    {
      /* compare uuid with each attribute in group */
      *pAttr = &pGroup->pAttr[startHandle - pGroup->startHandle];
      while ((startHandle <= pGroup->endHandle) && (startHandle <= endHandle))
      {
        /* compare uuid in attribute */
        if (attsUuidCmp(*pAttr, uuidLen, pUuid))
        {
          *pAttrGroup = pGroup;
          return startHandle;
        }

        /* special case of max handle value */
        if (startHandle == ATT_HANDLE_MAX)
        {
          break;
        }

        startHandle++;
        (*pAttr)++;
      }
    }
  }

  /* no match found */
  return ATT_HANDLE_NONE;
}

/*************************************************************************************************/
/*!
 *  \brief  Find the handle of the last attribute in a service group.
 *
 *  \param  startHandle   Starting attribute handle of service.
 *
 *  \return Service group end handle.
 */
/*************************************************************************************************/
uint16_t attsFindServiceGroupEnd(uint16_t startHandle)
{
  attsGroup_t   *pGroup;
  attsAttr_t    *pAttr;
  uint16_t      prevHandle;
  uint8_t       primSvcUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_PRIMARY_SERVICE)};
  uint8_t       secSvcUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_SECONDARY_SERVICE)};

  /* special case for max handle */
  if (startHandle == ATT_HANDLE_MAX)
  {
    return ATT_HANDLE_MAX;
  }

  prevHandle = startHandle;
  startHandle++;

  /* iterate over attribute group list */
  for (pGroup = attsCb.groupQueue.pHead; pGroup != NULL; pGroup = pGroup->pNext)
  {
    /* if start handle is less than group start handle */
    if (startHandle < pGroup->startHandle)
    {
      /* set start handle to first handle in group */
      startHandle = pGroup->startHandle;
    }

    /*  if start handle within handle range of group */
    if (startHandle <= pGroup->endHandle)
    {
      /* compare uuid with each attribute in group */
      pAttr = &pGroup->pAttr[startHandle - pGroup->startHandle];
      while (startHandle <= pGroup->endHandle)
      {
        /* compare uuid in attribute to service uuids */
        if (attsUuidCmp(pAttr, ATT_16_UUID_LEN, primSvcUuid) ||
            attsUuidCmp(pAttr, ATT_16_UUID_LEN, secSvcUuid))
        {
          /* found next service; return handle of previous attribute */
          return prevHandle;
        }

        /* special case of max handle value */
        if (startHandle == ATT_HANDLE_MAX)
        {
          return ATT_HANDLE_MAX;
        }

        prevHandle = startHandle;
        startHandle++;
        pAttr++;
      }
    }
  }

  /* next service not found; return 0xFFFF as the last handle in the database */
  return ATT_HANDLE_MAX;
}
/*************************************************************************************************/
/*!
 *  \brief  Process a read blob request PDU.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcReadBlobReq(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  uint8_t     *pBuf;
  uint8_t     *p;
  attsAttr_t  *pAttr;
  attsGroup_t *pGroup;
  uint16_t    handle;
  uint16_t    offset;
  uint16_t    readLen;
  uint8_t     err = ATT_SUCCESS;
  uint16_t    mtu = pCcb->pMainCcb->sccb[pCcb->slot].mtu;

  /* parse handle and offset */
  pPacket += L2C_PAYLOAD_START + ATT_HDR_LEN;
  BSTREAM_TO_UINT16(handle, pPacket);
  BSTREAM_TO_UINT16(offset, pPacket);

  /* find attribute */
  if ((pAttr = attsFindByHandle(handle, &pGroup)) != NULL)
  {
    /* verify permissions */
    if ((err = attsPermissions(pCcb->pMainCcb->connId, ATTS_PERMIT_READ,
                               handle, pAttr->permissions)) != ATT_SUCCESS)
    {
      /* err has been set; fail */
    }
    /* verify offset */
    else if (offset > *pAttr->pLen)
    {
      err = ATT_ERR_OFFSET;
    }
    else
    {
      /* call read callback if desired */
      if ((pAttr->settings & ATTS_SET_READ_CBACK) &&
          (pGroup->readCback != NULL))
      {
        err = (*pGroup->readCback)(pCcb->pMainCcb->connId, handle, ATT_PDU_READ_BLOB_REQ, offset, pAttr);
      }
      /* else check if CCC */
      else if ((pAttr->settings & ATTS_SET_CCC) && (attsCb.cccCback != NULL))
      {
        err = (*attsCb.cccCback)(pCcb->pMainCcb->connId, ATT_METHOD_READ, handle, pAttr->pValue);
      }

      if (err == ATT_SUCCESS)
      {
        /* determine length of data to read */
        readLen = ((*pAttr->pLen - offset) < (mtu - ATT_READ_BLOB_RSP_LEN)) ?
                   (*pAttr->pLen - offset) : (mtu - ATT_READ_BLOB_RSP_LEN);

        /* Allocate response buffer */
        if ((pBuf = attMsgAlloc(L2C_PAYLOAD_START + ATT_READ_BLOB_RSP_LEN + readLen)) != NULL)
        {
          /* build and send PDU */
          p = pBuf + L2C_PAYLOAD_START;
          UINT8_TO_BSTREAM(p, ATT_PDU_READ_BLOB_RSP);
          memcpy(p, (pAttr->pValue + offset), readLen);

          attL2cDataReq(pCcb->pMainCcb, pCcb->slot, (ATT_READ_BLOB_RSP_LEN + readLen), pBuf);
        }
      }
    }
  }
  /* else invalid handle */
  else
  {
    err = ATT_ERR_HANDLE;
  }

  if (err)
  {
    attErrRsp(pCcb->pMainCcb, pCcb->slot, ATT_PDU_READ_BLOB_REQ, handle, err);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process a find by type value request PDU.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcFindTypeReq(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  uint8_t     *pBuf;
  uint8_t     *p;
  uint8_t     *pUuid;
  attsAttr_t  *pAttr;
  attsGroup_t *pGroup;
  uint16_t    startHandle;
  uint16_t    endHandle;
  uint16_t    handle;
  uint16_t    nextHandle;
  uint8_t     err = ATT_SUCCESS;
  uint16_t    mtu = pCcb->pMainCcb->sccb[pCcb->slot].mtu;

  /* parse handles and uuid; pPacket then points to the value in the request */
  pPacket += L2C_PAYLOAD_START + ATT_HDR_LEN;
  BSTREAM_TO_UINT16(startHandle, pPacket);
  BSTREAM_TO_UINT16(endHandle, pPacket);
  pUuid = pPacket;
  pPacket += ATT_16_UUID_LEN;

  /* set len to the value length */
  len -= ATT_FIND_TYPE_REQ_LEN;

  /* verify handles */
  if ((startHandle == 0) || (startHandle > endHandle))
  {
    err = ATT_ERR_HANDLE;
  }

  if (!err)
  {
    /* allocate max size buffer for response */
    if ((pBuf = attMsgAlloc(mtu + L2C_PAYLOAD_START)) != NULL)
    {
      p = pBuf + L2C_PAYLOAD_START;
      UINT8_TO_BSTREAM(p, ATT_PDU_FIND_TYPE_RSP);

      /* find attributes with matching uuid within handle range */
      handle = startHandle;
      while ((handle = attsFindUuidInRange(handle, endHandle, ATT_16_UUID_LEN,
                                           pUuid, &pAttr, &pGroup)) != ATT_HANDLE_NONE)
      {
        /* if value and length matches */
        if ((pAttr->permissions & ATTS_PERMIT_READ) &&
            ((len == 0) ||
             ((len == *pAttr->pLen) && (memcmp(pPacket, pAttr->pValue, len) == 0))))
        {
          /* if uuid in request is for primary service */
          if (pUuid[0] == UINT16_TO_BYTE0(ATT_UUID_PRIMARY_SERVICE) &&
              pUuid[1] == UINT16_TO_BYTE1(ATT_UUID_PRIMARY_SERVICE))
          {
            /* next handle is service group end handle */
            nextHandle = attsFindServiceGroupEnd(handle);
          }
          else
          {
            /* for any other uuid next handle is same as found handle */
            nextHandle = handle;
          }

          /* copy result into response buffer; first check if it fits */
          if ((p + (sizeof(uint16_t) * 2)) <= (pBuf + mtu + L2C_PAYLOAD_START))
          {
            UINT16_TO_BSTREAM(p, handle);
            UINT16_TO_BSTREAM(p, nextHandle);
          }
          else
          {
            /* buffer full, we're done */
            break;
          }
        }
        /* value doesn't match; still need to set next handle */
        else
        {
          nextHandle = handle;
        }

        /* check if handle has reached end */
        if ((nextHandle >= endHandle) || (nextHandle == ATT_HANDLE_MAX))
        {
          break;
        }

        /* try next handle */
        handle = nextHandle + 1;
      }

      /* if no results found set error, free buffer */
      if (p == (pBuf + L2C_PAYLOAD_START + 1))
      {
        WsfMsgFree(pBuf);
        err = ATT_ERR_NOT_FOUND;
      }
    }
    else
    {
      /* buffer allocation failed */
      err = ATT_ERR_RESOURCES;
    }
  }

  /* set channel as busy for service discovery */
  attsDiscBusy(pCcb);

  /* if no error send response, else send error */
  if (!err)
  {
    attL2cDataReq(pCcb->pMainCcb, pCcb->slot, (p - (pBuf + L2C_PAYLOAD_START)), pBuf);
  }
  else
  {
    attErrRsp(pCcb->pMainCcb, pCcb->slot, ATT_PDU_FIND_TYPE_REQ, startHandle, err);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process a read by type request PDU.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcReadTypeReq(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  uint8_t     *pBuf = NULL;
  uint8_t     *p = NULL;
  attsAttr_t  *pAttr;
  attsGroup_t *pGroup;
  uint16_t    startHandle;
  uint16_t    endHandle;
  uint16_t    handle;
  uint8_t     uuidLen;
  uint8_t     attLen;
  uint8_t     cbackErr = ATT_SUCCESS;
  uint8_t     err = ATT_SUCCESS;
  uint16_t    mtu = pCcb->pMainCcb->sccb[pCcb->slot].mtu;

  /* parse handles; pPacket then points to the uuid */
  pPacket += L2C_PAYLOAD_START + ATT_HDR_LEN;
  BSTREAM_TO_UINT16(startHandle, pPacket);
  BSTREAM_TO_UINT16(endHandle, pPacket);

  /* get and verify uuid length */
  uuidLen = len - ATT_READ_TYPE_REQ_LEN;
  if (!((uuidLen == ATT_16_UUID_LEN) || (uuidLen == ATT_128_UUID_LEN)))
  {
    err = ATT_ERR_INVALID_PDU;
  }
  /* verify handles */
  else if ((startHandle == 0) || (startHandle > endHandle))
  {
    err = ATT_ERR_HANDLE;
  }

  if (!err)
  {
    /* find first attribute with matching uuid within handle range */
    handle = attsFindUuidInRange(startHandle, endHandle, uuidLen, pPacket, &pAttr, &pGroup);
    startHandle = handle;

    if (handle == ATT_HANDLE_NONE)
    {
      err = ATT_ERR_NOT_FOUND;
    }
    /* check permissions */
    else if ((err = attsPermissions(pCcb->pMainCcb->connId, ATTS_PERMIT_READ,
                                    handle, pAttr->permissions)) != ATT_SUCCESS)
    {
      /* err is set above */
    }
    /* check if read callback should be called */
    else if ((pAttr->settings & ATTS_SET_READ_CBACK) &&
             (pGroup->readCback != NULL))
    {
      err = (*pGroup->readCback)(pCcb->pMainCcb->connId, handle, ATT_PDU_READ_TYPE_REQ, 0, pAttr);
    }
    /* else check if CCC */
    else if ((pAttr->settings & ATTS_SET_CCC) && (attsCb.cccCback != NULL))
    {
      err = (*attsCb.cccCback)(pCcb->pMainCcb->connId, ATT_METHOD_READ, handle, pAttr->pValue);
    }

    if (err == ATT_SUCCESS)
    {
      /* Check if UUID is the Database Hash Characteristic Value and the value is being
       * re-calculated
       */
      if ((memcmp(pPacket, attGattDbhChUuid, ATT_16_UUID_LEN) == 0) && attsCsfGetHashUpdateStatus())
      {
        /* Store info and return */
        pCcb->pMainCcb->pPendDbHashRsp = WsfBufAlloc(sizeof(attPendDbHashRsp_t));
        if (pCcb->pMainCcb->pPendDbHashRsp)
        {
          pCcb->pMainCcb->pPendDbHashRsp->startHandle = startHandle;
          pCcb->pMainCcb->pPendDbHashRsp->handle = handle;
        }
        else
        {
          attErrRsp(pCcb->pMainCcb, pCcb->slot, ATT_PDU_READ_TYPE_REQ, startHandle, ATT_ERR_RESOURCES);
        }

        return;
      }

      /* allocate max size buffer for response */
      if ((pBuf = attMsgAlloc(mtu + L2C_PAYLOAD_START)) != NULL)
      {
        p = pBuf + L2C_PAYLOAD_START;
        UINT8_TO_BSTREAM(p, ATT_PDU_READ_TYPE_RSP);

        /* get length of this first attribute */
        attLen = (*pAttr->pLen < (mtu - ATT_READ_TYPE_RSP_LEN - sizeof(uint16_t))) ?
                  *pAttr->pLen : (mtu - ATT_READ_TYPE_RSP_LEN - sizeof(uint16_t));

        /* set length parameter in response message */
        UINT8_TO_BSTREAM(p, attLen + sizeof(uint16_t));

        /* copy result to response message */
        UINT16_TO_BSTREAM(p, handle);
        memcpy(p, pAttr->pValue, attLen);
        p += attLen;

        /* look for additional attributes */
        handle++;
        while ((handle = attsFindUuidInRange(handle, endHandle, uuidLen,
                                             pPacket, &pAttr, &pGroup)) != ATT_HANDLE_NONE)
        {
          /* call read callback if desired */
          if ((pAttr->settings & ATTS_SET_READ_CBACK) &&
              (pGroup->readCback != NULL))
          {
            cbackErr = (*pGroup->readCback)(pCcb->pMainCcb->connId, handle, ATT_PDU_READ_TYPE_REQ, 0, pAttr);
          }
          /* else check if CCC */
          else if ((pAttr->settings & ATTS_SET_CCC) && (attsCb.cccCback != NULL))
          {
            cbackErr = (*attsCb.cccCback)(pCcb->pMainCcb->connId, ATT_METHOD_READ, handle, pAttr->pValue);
          }

          /* verify no error from read callback
           * verify length is same as first found attribute
           * verify attribute permissions
           */
          if ((cbackErr == ATT_SUCCESS) &&
              (*pAttr->pLen == attLen) &&
              (attsPermissions(pCcb->pMainCcb->connId, ATTS_PERMIT_READ,
                               handle, pAttr->permissions) == ATT_SUCCESS))
          {
            /* copy result into response buffer; first check if it fits */
            if ((p + attLen + sizeof(uint16_t)) <= (pBuf + mtu + L2C_PAYLOAD_START))
            {
              UINT16_TO_BSTREAM(p, handle);
              memcpy(p, pAttr->pValue, attLen);
              p += attLen;
            }
            else
            {
              /* buffer full, we're done */
              break;
            }
          }
          else
          {
            /* problem with read callback, length, or permissions; send what we've got so far */
            break;
          }

          /* special case of handle at max range */
          if (handle == ATT_HANDLE_MAX)
          {
            break;
          }

          /* try next handle */
          if (++handle > endHandle)
          {
            break;
          }
        }
      }
      else
      {
        /* buffer allocation failed */
        err = ATT_ERR_RESOURCES;
      }
    }
  }

  /* if no error send response, else send error */
  if (!err)
  {
    attL2cDataReq(pCcb->pMainCcb, pCcb->slot, (p - (pBuf + L2C_PAYLOAD_START)), pBuf);
  }
  else
  {
    attErrRsp(pCcb->pMainCcb, pCcb->slot, ATT_PDU_READ_TYPE_REQ, startHandle, err);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process a read multiple request PDU.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcReadMultReq(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  uint8_t     *pBuf;
  uint8_t     *p;
  uint8_t     *pEnd;
  attsAttr_t  *pAttr;
  attsGroup_t *pGroup;
  uint16_t    handle = ATT_HANDLE_NONE;
  uint16_t    readLen;
  uint8_t     err = ATT_SUCCESS;
  uint16_t    mtu = pCcb->pMainCcb->sccb[pCcb->slot].mtu;

  /* points to end of payload */
  pEnd = pPacket + L2C_PAYLOAD_START + len;

  /* points to first handle */
  pPacket += L2C_PAYLOAD_START + ATT_HDR_LEN;

  /* allocate max size buffer for response */
  if ((pBuf = attMsgAlloc(mtu + L2C_PAYLOAD_START)) != NULL)
  {
    p = pBuf + L2C_PAYLOAD_START;
    UINT8_TO_BSTREAM(p, ATT_PDU_READ_MULT_RSP);

    /* while there are handles remaining and there is space in response buffer */
    while (pPacket < pEnd)
    {
      /* parse handle */
      BSTREAM_TO_UINT16(handle, pPacket);

      /* find attribute */
      if ((pAttr = attsFindByHandle(handle, &pGroup)) == NULL)
      {
        err = ATT_ERR_HANDLE;
        break;
      }

      /* verify permissions */
      if ((err = attsPermissions(pCcb->pMainCcb->connId, ATTS_PERMIT_READ,
                                 handle, pAttr->permissions)) != ATT_SUCCESS)
      {
        break;
      }

      /* call read callback if desired */
      if ((pAttr->settings & ATTS_SET_READ_CBACK) &&
          (pGroup->readCback != NULL))
      {
        err = (*pGroup->readCback)(pCcb->pMainCcb->connId, handle, ATT_PDU_READ_MULT_REQ, 0, pAttr);
        if (err != ATT_SUCCESS)
        {
          break;
        }
      }
      /* else check if CCC */
      else if ((pAttr->settings & ATTS_SET_CCC) && (attsCb.cccCback != NULL))
      {
        err = (*attsCb.cccCback)(pCcb->pMainCcb->connId, ATT_METHOD_READ, handle, pAttr->pValue);
        if (err != ATT_SUCCESS)
        {
          break;
        }
      }

      if (p < (pBuf + mtu + L2C_PAYLOAD_START))
      {
        /* calculate remaining space in response buffer */
        readLen = (pBuf + mtu + L2C_PAYLOAD_START) - p;

        /* actual length is minimum of remaining space and attribute length */
        readLen = (*pAttr->pLen < readLen) ? *pAttr->pLen : readLen;

        /* copy attribute to response buffer */
        memcpy(p, pAttr->pValue, readLen);
        p += readLen;
      }
    }
  }
  else
  {
    /* buffer allocation failed */
    err = ATT_ERR_RESOURCES;
  }

  /* if no error send response, else send error */
  if (!err)
  {
    attL2cDataReq(pCcb->pMainCcb, pCcb->slot, (p - (pBuf + L2C_PAYLOAD_START)), pBuf);
  }
  else
  {
    /* discard response buffer */
    if (pBuf != NULL)
    {
      WsfMsgFree(pBuf);
    }

    attErrRsp(pCcb->pMainCcb, pCcb->slot, ATT_PDU_READ_MULT_REQ, handle, err);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process a read group by type request PDU.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcReadGroupTypeReq(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  uint8_t     *pBuf = NULL;
  uint8_t     *p = NULL;
  attsAttr_t  *pAttr;
  attsGroup_t *pGroup;
  uint16_t    startHandle;
  uint16_t    endHandle;
  uint16_t    handle;
  uint8_t     uuidLen;
  uint8_t     attLen;
  uint8_t     err = ATT_SUCCESS;
  uint8_t     primSvcUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_PRIMARY_SERVICE)};
  uint16_t    mtu = pCcb->pMainCcb->sccb[pCcb->slot].mtu;

  /* parse handles; pPacket then points to the uuid */
  pPacket += L2C_PAYLOAD_START + ATT_HDR_LEN;
  BSTREAM_TO_UINT16(startHandle, pPacket);
  BSTREAM_TO_UINT16(endHandle, pPacket);

  /* get and verify uuid length */
  uuidLen = len - ATT_READ_GROUP_TYPE_REQ_LEN;
  if (!((uuidLen == ATT_16_UUID_LEN) || (uuidLen == ATT_128_UUID_LEN)))
  {
    err = ATT_ERR_INVALID_PDU;
  }
  /* verify handles */
  else if ((startHandle == 0) || (startHandle > endHandle))
  {
    err = ATT_ERR_HANDLE;
  }
  /* verify uuid is primary service group */
  else if (!attsUuid16Cmp(primSvcUuid, uuidLen, pPacket))
  {
    err = ATT_ERR_GROUP_TYPE;
  }

  if (!err)
  {
    /* find first attribute with matching uuid within handle range */
    handle = attsFindUuidInRange(startHandle, endHandle, uuidLen, pPacket, &pAttr, &pGroup);

    if (handle == ATT_HANDLE_NONE)
    {
      err = ATT_ERR_NOT_FOUND;
    }
    /* check permissions */
    else if ((err = attsPermissions(pCcb->pMainCcb->connId, ATTS_PERMIT_READ,
                                    handle, pAttr->permissions)) != ATT_SUCCESS)
    {
      startHandle = handle;     /* this handle is returned in error response */
    }
    else
    {
      /* allocate max size buffer for response */
      if ((pBuf = attMsgAlloc(mtu + L2C_PAYLOAD_START)) != NULL)
      {
        p = pBuf + L2C_PAYLOAD_START;
        UINT8_TO_BSTREAM(p, ATT_PDU_READ_GROUP_TYPE_RSP);

        /* get length of this first attribute */
        attLen = (*pAttr->pLen < (mtu - ATT_READ_GROUP_TYPE_RSP_LEN - (2 * sizeof(uint16_t)))) ?
                  *pAttr->pLen : (mtu - ATT_READ_GROUP_TYPE_RSP_LEN - (2 * sizeof(uint16_t)));

        /* set length parameter in response message */
        UINT8_TO_BSTREAM(p, attLen + (2 * sizeof(uint16_t)));

        /* copy handle to response message */
        UINT16_TO_BSTREAM(p, handle);

        /* get end group handle and copy it to response message */
        handle = attsFindServiceGroupEnd(handle);
        UINT16_TO_BSTREAM(p, handle);

        /* copy the attribute value to response message */
        memcpy(p, pAttr->pValue, attLen);
        p += attLen;

        /* look for additional attributes */
        while (TRUE)
        {
          /* special case of handle at max range */
          if (handle == ATT_HANDLE_MAX)
          {
            break;
          }

          /* increment to next handle */
          if (++handle > endHandle)
          {
            break;
          }

          /* find next matching handle */
          if ((handle = attsFindUuidInRange(handle, endHandle, uuidLen,
                                            pPacket, &pAttr, &pGroup)) == ATT_HANDLE_NONE)
          {
            break;
          }

          /* verify length is same as first found attribute
           * verify attribute permissions
           */
          if ((*pAttr->pLen == attLen) &&
              (attsPermissions(pCcb->pMainCcb->connId, ATTS_PERMIT_READ,
                               handle, pAttr->permissions) == ATT_SUCCESS))
          {
            /* copy result into response buffer; first check if it fits */
            if ((p + attLen + (2 * sizeof(uint16_t))) <=
                (pBuf + mtu + L2C_PAYLOAD_START))
            {
              UINT16_TO_BSTREAM(p, handle);
              handle = attsFindServiceGroupEnd(handle);
              UINT16_TO_BSTREAM(p, handle);
              memcpy(p, pAttr->pValue, attLen);
              p += attLen;
            }
            else
            {
              /* buffer full, we're done */
              break;
            }
          }
          else
          {
            /* problem with either length or permissions; send what we've got so far */
            break;
          }
        }
      }
      else
      {
        /* buffer allocation failed */
        err = ATT_ERR_RESOURCES;
      }
    }
  }

  /* set channel as busy for service discovery */
  attsDiscBusy(pCcb);

  /* if no error send response, else send error */
  if (!err)
  {
    attL2cDataReq(pCcb->pMainCcb, pCcb->slot, (p - (pBuf + L2C_PAYLOAD_START)), pBuf);
  }
  else
  {
    attErrRsp(pCcb->pMainCcb, pCcb->slot, ATT_PDU_READ_GROUP_TYPE_REQ, startHandle, err);
  }
}
