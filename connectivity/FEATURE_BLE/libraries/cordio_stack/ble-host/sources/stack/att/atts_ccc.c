/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  ATT client characteristic configuration module.
 *
 *  Copyright (c) 2011-2018 Arm Ltd. All Rights Reserved.
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
#include "util/bstream.h"
#include "dm_api.h"
#include "att_api.h"
#include "att_main.h"
#include "atts_main.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Control block */
typedef struct
{
  uint16_t        *pCccTbl[DM_CONN_MAX];  /* Pointer to descriptor value tables */
  attsCccSet_t    *pSet;                  /* Array of CCC descriptor settings */
  attsCccCback_t  cback;                  /* Client callback function */
  uint8_t         setLen;                 /* Length of settings array */
} AttsCccCb_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Control block */
static AttsCccCb_t attsCccCb;

/*************************************************************************************************/
/*!
 *  \brief  Execute the client callback function.
 *
 *  \param  connId      DM connection ID.
 *  \param  idx         Index of descriptor in CCC descriptor handle table.
 *  \param  handle      Attribute handle of the descriptor.
 *  \param  value       Attribute value of the descriptor.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsCccCback(dmConnId_t connId, uint8_t idx, uint16_t handle, uint16_t value)
{
  attsCccEvt_t  evt;

  evt.hdr.event = ATTS_CCC_STATE_IND;
  evt.hdr.param = connId;
  evt.idx = idx;
  evt.handle = handle;
  evt.value = value;

  (*attsCccCb.cback)(&evt);
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate the CCC table for this connection.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return Pointer into the CCC table.
 */
/*************************************************************************************************/
static uint16_t *attsCccAllocTbl(dmConnId_t connId)
{
  WSF_ASSERT((connId > 0) && (connId <= DM_CONN_MAX));

  /* if not already allocated */
  if (attsCccCb.pCccTbl[connId - 1] == NULL)
  {
    WSF_ASSERT(attsCccCb.setLen > 0);

    /* allocate new buffer */
    attsCccCb.pCccTbl[connId - 1] = WsfBufAlloc(attsCccCb.setLen * sizeof(uint16_t));
  }

  return attsCccCb.pCccTbl[connId - 1];
}

/*************************************************************************************************/
/*!
 *  \brief  Get the pointer into the CCC table for this connection.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return Pointer into the CCC table.
 */
/*************************************************************************************************/
static uint16_t *attsCccGetTbl(dmConnId_t connId)
{
  WSF_ASSERT((connId > 0) && (connId <= DM_CONN_MAX));

  return attsCccCb.pCccTbl[connId - 1];
}

/*************************************************************************************************/
/*!
 *  \brief  Free the CCC table for this connection.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void attsCccFreeTbl(dmConnId_t connId)
{
  WSF_ASSERT((connId > 0) && (connId <= DM_CONN_MAX));

  if (attsCccCb.pCccTbl[connId - 1] != NULL)
  {
    WsfBufFree(attsCccCb.pCccTbl[connId - 1]);
    attsCccCb.pCccTbl[connId - 1] = NULL;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Read the value of a client characteristic configuration descriptor.  Note the
 *          value is treated as a little endian byte array.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle of the descriptor.
 *  \param  pValue      The attribute value of the descriptor is copied to this pointer.
 *
 *  \return ATT_SUCCESS if successful otherwise error.
 */
/*************************************************************************************************/
static uint8_t attsCccReadValue(dmConnId_t connId, uint16_t handle, uint8_t *pValue)
{
  attsCccSet_t  *pSet;
  uint16_t      *pTbl;
  uint8_t       i;

  /* find handle in handle array */
  for (pSet = attsCccCb.pSet, i = 0; i < attsCccCb.setLen; i++, pSet++)
  {
    if (pSet->handle == handle)
    {
      break;
    }
  }

  /* if handle not found return error */
  if (i == attsCccCb.setLen)
  {
    return ATT_ERR_NOT_FOUND;
  }

  /* get pointer to the table for this connection */
  if ((pTbl = attsCccGetTbl(connId)) != NULL)
  {
    /* read value */
    UINT16_TO_BSTREAM(pValue, pTbl[i]);

    return ATT_SUCCESS;
  }
  else
  {
    return ATT_ERR_RESOURCES;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Write the value of a client characteristic configuration descriptor.  Note the
 *          value is treated as a little endian byte array.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle of the descriptor.
 *  \param  pValue      Pointer to the attribute value of the descriptor.
 *
 *  \return ATT_SUCCESS if successful otherwise error.
 */
/*************************************************************************************************/
static uint8_t attsCccWriteValue(dmConnId_t connId, uint16_t handle, uint8_t *pValue)
{
  attsCccSet_t  *pSet;
  uint16_t      *pTbl;
  uint8_t       i;
  uint16_t      value;
  uint16_t      prevValue;

  /* find handle in handle array */
  for (pSet = attsCccCb.pSet, i = 0; i < attsCccCb.setLen; i++, pSet++)
  {
    if (pSet->handle == handle)
    {
      break;
    }
  }

  /* if handle not found return error */
  if (i == attsCccCb.setLen)
  {
    return ATT_ERR_NOT_FOUND;
  }

  BYTES_TO_UINT16(value, pValue);

  /* verify value range */
  if (((value != 0) && (value != ATT_CLIENT_CFG_NOTIFY) && (value != ATT_CLIENT_CFG_INDICATE)) ||
      ((value != 0) && ((value & pSet->valueRange) == 0)))
  {
    return ATT_ERR_VALUE_RANGE;
  }

  /* get pointer to the table for this connection */
  if ((pTbl = attsCccGetTbl(connId)) != NULL)
  {
    /* write value */
    prevValue = pTbl[i];
    pTbl[i] = value;

    /* if value changed call callback */
    if (prevValue != value)
    {
      attsCccCback(connId, i, handle, value);
    }

    return ATT_SUCCESS;
  }
  else
  {
    return ATT_ERR_RESOURCES;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  CCC callback function executed by ATTS and CCC read or write.
 *
 *  \param  connId      DM connection ID.
 *  \param  method      Read or write.
 *  \param  handle      Attribute handle of the descriptor.
 *  \param  pValue      The attribute value of the descriptor is copied to this pointer.
 *
 *  \return ATT_SUCCESS if successful otherwise error.
 */
/*************************************************************************************************/
static uint8_t attsCccMainCback(dmConnId_t connId, uint8_t method, uint16_t handle, uint8_t *pValue)
{
  ATT_TRACE_INFO2("attsCccMainCback connId=%d handle=%d", connId, handle);

  if (method == ATT_METHOD_READ)
  {
    return attsCccReadValue(connId, handle, pValue);
  }
  else
  {
    return attsCccWriteValue(connId, handle, pValue);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Register the utility service for managing client characteristic
 *          configuration descriptors.  This function is typically called once on
 *          system initialization.
 *
 *  \param  setLen  Length of settings array.
 *  \param  pSet    Array of CCC descriptor settings.
 *  \param  cback   Client callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCccRegister(uint8_t setLen, attsCccSet_t *pSet, attsCccCback_t cback)
{
  attsCccCb.setLen = setLen;
  attsCccCb.pSet = pSet;
  attsCccCb.cback = cback;

  attsCb.cccCback = attsCccMainCback;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the client characteristic configuration descriptor value table for a
 *          connection.  This function is typically called when a connection is established
 *          or when a device is bonded.
 *
 *  \param  connId      DM connection ID.
 *  \param  pCccTbl     Pointer to the descriptor value array.  The length of the array
 *                      must equal the value of setLen passed to AttsCccRegister().
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCccInitTable(dmConnId_t connId, uint16_t *pCccTbl)
{
  uint8_t   i;
  uint16_t  *pTbl;

  ATT_TRACE_INFO1("AttsCccInitTable connId=%d", connId);

  if ((pTbl = attsCccAllocTbl(connId)) != NULL)
  {
    /* if initializer table is passed in */
    if (pCccTbl != NULL)
    {
      /* initialize table */
      for (i = 0; i < attsCccCb.setLen; i++, pCccTbl++, pTbl++)
      {
        /* copy value */
        *pTbl = *pCccTbl;

        /* execute callback for each nonzero entry in table */
        if (*pCccTbl != 0)
        {
          attsCccCback(connId, i, ATT_HANDLE_NONE, *pCccTbl);
        }
      }
    }
    else
    {
      /* initialize table to zero */
      memset(pTbl, 0, (sizeof(uint16_t) * attsCccCb.setLen));
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Clear and deallocate the client characteristic configuration descriptor value
 *          table for a connection.  This function must be called when a connection is closed.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCccClearTable(dmConnId_t connId)
{
  ATT_TRACE_INFO1("AttsCccClearTable connId=%d", connId);

  attsCccFreeTbl(connId);
}

/*************************************************************************************************/
/*!
 *  \brief  Get the value of a client characteristic configuration descriptor by its index.
 *          If not found, return zero.
 *
 *  \param  connId      DM connection ID.
 *  \param  idx         Index of descriptor in CCC descriptor handle table.
 *
 *  \return Value of the descriptor.
 */
/*************************************************************************************************/
uint16_t AttsCccGet(dmConnId_t connId, uint8_t idx)
{
  uint16_t      *pTbl;

  WSF_ASSERT(idx < attsCccCb.setLen);

  if ((pTbl = attsCccGetTbl(connId)) != NULL)
  {
    /* return value from table */
    return pTbl[idx];
  }
  else
  {
    return 0;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the value of a client characteristic configuration descriptor by its index.
 *
 *  \param  connId      DM connection ID.
 *  \param  idx         Index of descriptor in CCC descriptor handle table.
 *  \param  value       Value of the descriptor.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCccSet(dmConnId_t connId, uint8_t idx, uint16_t value)
{
  uint16_t      *pTbl;

  WSF_ASSERT(idx < attsCccCb.setLen);

  if ((pTbl = attsCccGetTbl(connId)) != NULL)
  {
    pTbl[idx] = value;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check if a client characteristic configuration descriptor is enabled and if
 *          the characteristic's security level has been met.
 *
 *  \param  connId      DM connection ID.
 *  \param  idx         Index of descriptor in CCC descriptor handle table.
 *
 *  \return Value of the descriptor if security level is met, otherwise zero.
 */
/*************************************************************************************************/
uint16_t AttsCccEnabled(dmConnId_t connId, uint8_t idx)
{
  WSF_ASSERT(idx < attsCccCb.setLen);

  /* check security level */
  if (DmConnSecLevel(connId) < attsCccCb.pSet[idx].secLevel)
  {
    return 0;
  }

  /* get value */
  return AttsCccGet(connId, idx);
}

/*************************************************************************************************/
/*!
 *  \brief  Check if a client characteristic configuration descriptor is enabled and if
 *          the characteristic's security level has been met.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle of CCC descriptor.
 *
 *  \return Value of the descriptor if security level is met, otherwise zero.
 */
/*************************************************************************************************/
uint16_t AttsCccEnabledByHandle(dmConnId_t connId, uint16_t handle)
{
  attsCccSet_t  *pSet;
  uint8_t       i;

  /* find handle in handle array */
  for (pSet = attsCccCb.pSet, i = 0; i < attsCccCb.setLen; i++, pSet++)
  {
    if (pSet->handle == handle)
    {
      return AttsCccEnabled(connId, i);
    }
  }

  return 0;
}

/*************************************************************************************************/
/*!
 *  \brief   Get number of CCC entries in table.
 *
 *  \param   None
 *
 *  \return  Number of CCC entries in table.
 */
/*************************************************************************************************/
uint8_t AttsGetCccTableLen(void)
{
  return attsCccCb.setLen;
}
