/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  ATT server mandatory PDU processing functions.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "wsf_msg.h"
#include "wsf_math.h"
#include "util/bstream.h"
#include "att_api.h"
#include "att_main.h"
#include "atts_main.h"
#include "svc_core.h"

/*************************************************************************************************/
/*!
 *  \brief  Compare the given attribute's UUID to the given UUID.
 *
 *  \param  pAttr   Pointer to attribute.
 *  \param  uuidLen UUID length, either 2 or 16.
 *  \param  pUuid   Pointer to UUID.
 *
 *  \return TRUE of UUIDs match, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t attsUuidCmp(attsAttr_t *pAttr, uint8_t uuidLen, uint8_t *pUuid)
{
  /* if both uuids are the same length */
  if ((((pAttr->settings & ATTS_SET_UUID_128) == 0) && (uuidLen == ATT_16_UUID_LEN)) ||
      (((pAttr->settings & ATTS_SET_UUID_128) != 0) &&  (uuidLen == ATT_128_UUID_LEN)))
  {
    /* simply compare the data */
    return (memcmp(pAttr->pUuid, pUuid, uuidLen) == 0);
  }
  /* else we need to convert one of the uuids */
  else if (((pAttr->settings & ATTS_SET_UUID_128) == 0) && (uuidLen == ATT_128_UUID_LEN))
  {
    return attUuidCmp16to128(pAttr->pUuid, pUuid);
  }
  else
  {
    return attUuidCmp16to128(pUuid, pAttr->pUuid);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Compare the given 16 bit UUID to the given UUID.
 *
 *  \param  pUuid16   Pointer to the 16 bit UUID.
 *  \param  uuidLen   UUID length, either 2 or 16.
 *  \param  pUuid     Pointer to UUID.
 *
 *  \return TRUE of UUIDs match, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t attsUuid16Cmp(uint8_t *pUuid16, uint8_t uuidLen, uint8_t *pUuid)
{
  if (uuidLen == ATT_16_UUID_LEN)
  {
    return ((pUuid16[0] == pUuid[0]) && (pUuid16[1] == pUuid[1]));
  }
  else
  {
    return attUuidCmp16to128(pUuid16, pUuid);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Find an attribute with the given handle.
 *
 *  \param  handle      Attribute handle.
 *  \param  pAttrGroup  Return value pointer to found attribute's group.
 *
 *  \return Pointer to attribute if found, othewise NULL.
 */
/*************************************************************************************************/
attsAttr_t *attsFindByHandle(uint16_t handle, attsGroup_t **pAttrGroup)
{
  attsGroup_t   *pGroup;

  /* iterate over attribute group list */
  for (pGroup = attsCb.groupQueue.pHead; pGroup != NULL; pGroup = pGroup->pNext)
  {
    /*  if start handle within handle range of group */
    if ((handle >= pGroup->startHandle) && (handle <= pGroup->endHandle))
    {
      /* index by handle into attribute array to return attribute */
      *pAttrGroup = pGroup;
      return &pGroup->pAttr[handle - pGroup->startHandle];
    }
  }

  /* handle not found */
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Find the first attribute within the given handle range.
 *
 *  \param  startHandle   Starting attribute handle.
 *  \param  endHandle     Ending attribute handle.
 *  \param  pAttr         Return value pointer to found attribute.
 *
 *  \return Attribute handle or ATT_HANDLE_NONE if not found.
 */
/*************************************************************************************************/
uint16_t attsFindInRange(uint16_t startHandle, uint16_t endHandle, attsAttr_t **pAttr)
{
  attsGroup_t   *pGroup;

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
      /* index by handle into attribute array to return attribute */
      *pAttr = &pGroup->pAttr[startHandle - pGroup->startHandle];
      return startHandle;
    }
  }

  /* handle within range not found */
  return ATT_HANDLE_NONE;
}

/*************************************************************************************************/
/*!
 *  \brief  Perform required permission and security checks when reading or writing an attribute.
 *
 *  \param  connId      Connection ID.
 *  \param  permit      Either ATTS_PERMIT_READ or ATTS_PERMIT_WRITE.
 *  \param  handle      Attribute handle.
 *  \param  permissions Attribute permissions.
 *
 *  \return ATT_SUCCESS if successful or error code on failure.
 */
/*************************************************************************************************/
uint8_t attsPermissions(dmConnId_t connId, uint8_t permit, uint16_t handle, uint8_t permissions)
{
  uint8_t secLevel;

  /* verify read or write permissions */
  if (!(permissions & permit))
  {
    return (permit == ATTS_PERMIT_READ) ? ATT_ERR_READ : ATT_ERR_WRITE;
  }

  /* convert write permissions to read permissions for easier masking */
  if (permit == ATTS_PERMIT_WRITE)
  {
    permissions >>= 4;
  }

  /* if no security requirements return quickly */
  if ((permissions & (ATTS_PERMIT_READ_AUTH | ATTS_PERMIT_READ_AUTHORIZ | ATTS_PERMIT_READ_ENC)) == 0)
  {
    return ATT_SUCCESS;
  }

  /* get security level for this connection */
  secLevel = DmConnSecLevel(connId);

  /* check if encryption required */
  if ((permissions & ATTS_PERMIT_READ_ENC) && (secLevel == DM_SEC_LEVEL_NONE))
  {
    return ATT_ERR_AUTH;
  }

  /* check if encryption required with authenticated key */
  if (((permissions & (ATTS_PERMIT_READ_AUTH | ATTS_PERMIT_READ_ENC)) ==
       (ATTS_PERMIT_READ_AUTH | ATTS_PERMIT_READ_ENC)) && (secLevel < DM_SEC_LEVEL_ENC_AUTH))
  {
    return ATT_ERR_AUTH;
  }

  /* authorization check */
  if (permissions & ATTS_PERMIT_READ_AUTHORIZ)
  {
    if (attsCb.authorCback == NULL)
    {
      return ATT_ERR_AUTHOR;
    }
    else
    {
      return (*attsCb.authorCback)(connId, permit, handle);
    }
  }
  else
  {
    return ATT_SUCCESS;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process an MTU request PDU.
 *
 *  \param  pCcb     Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcMtuReq(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  uint8_t   *p;
  uint16_t  mtu;
  uint16_t  localMtu;
  uint8_t   *pRsp;
  uint8_t   features;

  /* Verify Server supported features permits MTU exchange */
  AttsCsfGetFeatures(pCcb->connId, &features, sizeof(uint8_t));
  
  ATT_TRACE_WARN1("attsProcMtuReq features 0x%02x", features);

  if (features & ATTS_CSF_EATT_BEARER)
  {
    /* Client set the EATT supported feature.  MTU Exchange not permitted */
    attErrRsp(pCcb->pMainCcb, pCcb->slot, ATT_PDU_MTU_REQ, 0, ATT_ERR_NOT_SUP);
  }
  else
  {
    p = pPacket + L2C_PAYLOAD_START + ATT_HDR_LEN;

    /* parse mtu */
    BYTES_TO_UINT16(mtu, p);

    /* verify */
    if (mtu < ATT_DEFAULT_MTU)
    {
      mtu = ATT_DEFAULT_MTU;
    }

    /* get desired MTU */
    localMtu = WSF_MIN(pAttCfg->mtu, (HciGetMaxRxAclLen() - L2C_HDR_LEN));

    /* send response */
    if ((pRsp = attMsgAlloc(L2C_PAYLOAD_START + ATT_MTU_RSP_LEN)) != NULL)
    {
      p = pRsp + L2C_PAYLOAD_START;
      UINT8_TO_BSTREAM(p, ATT_PDU_MTU_RSP);
      UINT16_TO_BSTREAM(p, localMtu);

      attL2cDataReq(pCcb->pMainCcb, pCcb->slot, ATT_MTU_RSP_LEN, pRsp);
    }

    /* set mtu for the connection */
    attSetMtu(pCcb->pMainCcb, pCcb->slot, mtu, localMtu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process a find information request PDU.
 *
 *  \param  pCcb     Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcFindInfoReq(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  uint8_t     *pBuf;
  uint8_t     *p;
  attsAttr_t  *pAttr;
  uint16_t    startHandle;
  uint16_t    endHandle;
  uint16_t    handle;
  uint8_t     err = ATT_SUCCESS;
  uint16_t    mtu = pCcb->pMainCcb->sccb[pCcb->slot].mtu;

  /* parse handles */
  pPacket += L2C_PAYLOAD_START + ATT_HDR_LEN;
  BSTREAM_TO_UINT16(startHandle, pPacket);
  BSTREAM_TO_UINT16(endHandle, pPacket);

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
      UINT8_TO_BSTREAM(p, ATT_PDU_FIND_INFO_RSP);

      /* set result format */
      UINT8_TO_BSTREAM(p, ATT_FIND_HANDLE_16_UUID);

      /* find attributes within handle range */
      handle = startHandle;
      while ((handle = attsFindInRange(handle, endHandle, &pAttr)) != ATT_HANDLE_NONE)
      {
        /* copy handle and UUID into response buffer */

        /* if 128 bit UUID */
        if (pAttr->settings & ATTS_SET_UUID_128)
        {
          /* if this is the first result */
          if (p == (pBuf + L2C_PAYLOAD_START + 2))
          {
            p--;
            UINT8_TO_BSTREAM(p, ATT_FIND_HANDLE_128_UUID);
            UINT16_TO_BSTREAM(p, handle);
            memcpy(p, pAttr->pUuid, ATT_128_UUID_LEN);
            p += ATT_128_UUID_LEN;
          }
          break;
        }
        /* else 16 bit UUID */
        else
        {
          /* check if result fits */
          if ((p + ATT_16_UUID_LEN + sizeof(uint16_t)) <=
              (pBuf + mtu + L2C_PAYLOAD_START))
          {
            /* copy result */
            UINT16_TO_BSTREAM(p, handle);
            UINT8_TO_BSTREAM(p, pAttr->pUuid[0]);
            UINT8_TO_BSTREAM(p, pAttr->pUuid[1]);
          }
          else
          {
            /* response buffer full, we're done */
            break;
          }
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

      /* if no results found set error, free buffer */
      if (p == (pBuf + L2C_PAYLOAD_START + 2))
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
    attErrRsp(pCcb->pMainCcb, pCcb->slot, ATT_PDU_FIND_INFO_REQ, startHandle, err);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process a read request PDU.
 *
 *  \param  pCcb     Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcReadReq(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  uint8_t     *pBuf;
  uint8_t     *p;
  attsAttr_t  *pAttr;
  attsGroup_t *pGroup;
  uint16_t    handle;
  uint16_t    readLen;
  uint8_t     err = ATT_SUCCESS;
  uint16_t    mtu = pCcb->pMainCcb->sccb[pCcb->slot].mtu;

  /* parse handle */
  pPacket += L2C_PAYLOAD_START + ATT_HDR_LEN;
  BSTREAM_TO_UINT16(handle, pPacket);

  /* find attribute */
  if ((pAttr = attsFindByHandle(handle, &pGroup)) != NULL)
  {
    /* verify permissions */
    if ((err = attsPermissions(pCcb->pMainCcb->connId, ATTS_PERMIT_READ,
                               handle, pAttr->permissions)) == ATT_SUCCESS)
    {
      /* call read callback if desired */
      if ((pAttr->settings & ATTS_SET_READ_CBACK) &&
          (pGroup->readCback != NULL))
      {
        err = (*pGroup->readCback)(pCcb->pMainCcb->connId, handle, ATT_PDU_READ_REQ, 0, pAttr);
      }
      /* else check if CCC */
      else if ((pAttr->settings & ATTS_SET_CCC) && (attsCb.cccCback != NULL))
      {
        err = (*attsCb.cccCback)(pCcb->pMainCcb->connId, ATT_METHOD_READ, handle, pAttr->pValue);
      }

      if (err == ATT_SUCCESS)
      {
        /* determine length of data to read */
        readLen = (*pAttr->pLen < (mtu - ATT_READ_RSP_LEN)) ?
                   *pAttr->pLen : (mtu - ATT_READ_RSP_LEN);

        /* Allocate response buffer */
        if ((pBuf = attMsgAlloc(L2C_PAYLOAD_START + ATT_READ_RSP_LEN + readLen)) != NULL)
        {
          /* build and send PDU */
          p = pBuf + L2C_PAYLOAD_START;
          UINT8_TO_BSTREAM(p, ATT_PDU_READ_RSP);
          memcpy(p, pAttr->pValue, readLen);

          attL2cDataReq(pCcb->pMainCcb, pCcb->slot, (ATT_READ_RSP_LEN + readLen), pBuf);
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
    attErrRsp(pCcb->pMainCcb, pCcb->slot, ATT_PDU_READ_REQ, handle, err);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process a read multi variable length request PDU.
 *
 *  \param  pCcb      Connection control block.
 *  \param  len       The length of the L2CAP payload data in pPacket.
 *  \param  pPacket   A buffer containing the packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsProcReadMultiVarReq(attsCcb_t *pCcb, uint16_t len, uint8_t *pPacket)
{
  uint8_t       *pBuf;
  uint8_t       *p;
  attsAttr_t    *pAttr;
  attsGroup_t   *pGroup;
  uint16_t      handle = ATT_HANDLE_NONE;
  uint16_t      rspLen = 0;
  uint8_t       err = ATT_SUCCESS;
  uint16_t      mtu = pCcb->pMainCcb->sccb[pCcb->slot].mtu;
  const uint8_t hdrLen = L2C_PAYLOAD_START + ATT_HDR_LEN;

  pPacket += L2C_PAYLOAD_START + ATT_HDR_LEN;
  len -= ATT_HDR_LEN;
  
  if ((pBuf = attMsgAlloc(L2C_PAYLOAD_START + mtu)) != NULL)
  {
    p = pBuf + hdrLen;

    while (len > 0)
    {
      /* parse handle */
      BSTREAM_TO_UINT16(handle, pPacket);
      len -= sizeof(uint16_t);

      /* find attribute */
      if ((pAttr = attsFindByHandle(handle, &pGroup)) != NULL)
      {
        /* verify permissions */
        if ((err = attsPermissions(pCcb->pMainCcb->connId, ATTS_PERMIT_READ,
                                   handle, pAttr->permissions)) == ATT_SUCCESS)
        {
          /* call read callback if desired */
          if ((pAttr->settings & ATTS_SET_READ_CBACK) && (pGroup->readCback != NULL))
          {
            err = (*pGroup->readCback)(pCcb->pMainCcb->connId, handle, ATT_PDU_READ_MULT_VAR_REQ, 0, pAttr);
          }
          /* else check if CCC */
          else if ((pAttr->settings & ATTS_SET_CCC) && (attsCb.cccCback != NULL))
          {
            err = (*attsCb.cccCback)(pCcb->pMainCcb->connId, ATT_METHOD_READ, handle, pAttr->pValue);
          }

          if (err == ATT_SUCCESS)
          {
            /* determine length of data to read */
            uint16_t readLen = (*pAttr->pLen < (mtu - rspLen - hdrLen)) ?
                                *pAttr->pLen : (mtu - rspLen - hdrLen);

            UINT16_TO_BSTREAM(p, readLen);
            memcpy(p, pAttr->pValue, readLen);
            p += readLen;
            rspLen += readLen + sizeof(uint16_t);

            /* if truncating, then we are done */
            if (readLen < *pAttr->pLen)
            {
              break;
            }
          }
          else
          {
            break;
          }
        }
      }
      else
      {
        err = ATT_ERR_HANDLE;
        break;
      }
    }

    if (err == ATT_SUCCESS)
    {
      /* complete and send PDU */
      p = pBuf + L2C_PAYLOAD_START;
      UINT8_TO_BSTREAM(p, ATT_PDU_READ_MULT_VAR_RSP);

      attL2cDataReq(pCcb->pMainCcb, pCcb->slot, (ATT_READ_MULT_VAR_RSP_LEN + rspLen), pBuf);
    }
    else
    {
      AttMsgFree(pBuf, ATT_PDU_READ_MULT_VAR_RSP);
    }
  }

  if (err)
  {
    attErrRsp(pCcb->pMainCcb, pCcb->slot, ATT_PDU_READ_MULT_VAR_REQ, handle, err);
  }
}
