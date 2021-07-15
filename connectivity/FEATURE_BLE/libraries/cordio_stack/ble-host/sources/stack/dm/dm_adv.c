/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Device manager advertising module.
 *
 *  Copyright (c) 2016-2018 Arm Ltd. All Rights Reserved.
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
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "dm_api.h"
#include "dm_adv.h"
#include "dm_main.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* control block */
dmAdvCb_t dmAdvCb;

/*************************************************************************************************/
/*!
 *  \brief  Initialize the advertising CB for a given handle.
 *
 *  \param  advHandle  Advertising handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvCbInit(uint8_t advHandle)
{
  /* initialize control block */
  dmAdvCb.advType[advHandle] = DM_ADV_NONE;
  dmAdvCb.intervalMin[advHandle] = DM_GAP_ADV_SLOW_INT_MIN;
  dmAdvCb.intervalMax[advHandle] = DM_GAP_ADV_SLOW_INT_MAX;
  dmAdvCb.channelMap[advHandle] = DM_ADV_CHAN_ALL;
  dmCb.advFiltPolicy[advHandle] = HCI_ADV_FILT_NONE;
  dmAdvCb.advState[advHandle] = DM_ADV_STATE_IDLE;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the legacy adv module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvInit(void)
{
  uint8_t i;

  /* initialize control block */
  for (i = 0; i < DM_NUM_ADV_SETS; i++)
  {
    dmAdvCbInit(i);
  }

  dmAdvCb.advTimer.handlerId = dmCb.handlerId;
  dmCb.advAddrType = DM_ADDR_PUBLIC;
}

/*************************************************************************************************/
/*!
 *  \brief  Generate an enhanced connection complete event.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  status        Status.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmAdvGenConnCmpl(uint8_t advHandle, uint8_t status)
{
  hciLeConnCmplEvt_t  leConnCmpl;

  /* generate enhanced connection complete event */
  memset(&leConnCmpl, 0, sizeof(leConnCmpl));

  leConnCmpl.hdr.event = HCI_LE_ENHANCED_CONN_CMPL_CBACK_EVT;
  leConnCmpl.hdr.status = leConnCmpl.status = status;
  leConnCmpl.role = DM_ROLE_SLAVE;
  leConnCmpl.addrType = dmAdvCb.peerAddrType[advHandle];
  BdaCpy(leConnCmpl.peerAddr, dmAdvCb.peerAddr[advHandle]);

  /* pass connection complete event to DM connection management module */
  dmDevPassHciEvtToConn((hciEvt_t *) &leConnCmpl);
}

/*************************************************************************************************/
/*!
 *  \brief  Set the advertising parameters using the given advertising type, and peer address.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  advType       Advertising type.
 *  \param  peerAddrType  Peer address type.
 *  \param  pPeerAddr     Peer address.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvConfig(uint8_t advHandle, uint8_t advType, uint8_t peerAddrType, uint8_t *pPeerAddr)
{
  dmAdvApiConfig_t *pMsg;

  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  if ((pMsg = WsfMsgAlloc(sizeof(dmAdvApiSetData_t))) != NULL)
  {
    pMsg->hdr.event = DM_ADV_MSG_API_CONFIG;
    pMsg->advType = advType;
    pMsg->advHandle = advHandle;
    pMsg->peerAddrType = peerAddrType;
    BdaCpy(pMsg->peerAddr, pPeerAddr);
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the advertising or scan response data to the given data.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  op            Data operation.
 *  \param  location      Data location.
 *  \param  len           Length of the data.  Maximum length is 236 bytes.
 *  \param  pData         Pointer to the data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvSetData(uint8_t advHandle, uint8_t op, uint8_t location, uint8_t len, uint8_t *pData)
{
  dmAdvApiSetData_t *pMsg;

  WSF_ASSERT((location == DM_DATA_LOC_SCAN) || (location == DM_DATA_LOC_ADV));
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  if ((pMsg = WsfMsgAlloc(sizeof(dmAdvApiSetData_t) + len)) != NULL)
  {
    pMsg->hdr.event = DM_ADV_MSG_API_SET_DATA;
    pMsg->advHandle = advHandle;
    pMsg->op = op;
    pMsg->location = location;
    pMsg->len = len;
    memcpy(pMsg->pData, pData, len);
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Start advertising using the given advertising set and duration.
 *
 *  \param  numSets       Number of advertising sets to enable.
 *  \param  pAdvHandles   Advertising handles array.
 *  \param  pDuration     Advertising duration (in milliseconds) array.
 *  \param  pMaxEaEvents  Maximum number of extended advertising events array.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvStart(uint8_t numSets, uint8_t *pAdvHandles, uint16_t *pDuration, uint8_t *pMaxEaEvents)
{
  uint8_t i;
  dmAdvApiStart_t *pMsg;

  WSF_ASSERT(numSets <= DM_NUM_ADV_SETS);

  if ((pMsg = WsfMsgAlloc(sizeof(dmAdvApiStart_t))) != NULL)
  {
    pMsg->hdr.event = DM_ADV_MSG_API_START;
    pMsg->numSets = numSets;

    for (i = 0; i < numSets; i++)
    {
      pMsg->advHandle[i] = pAdvHandles[i];
      pMsg->duration[i] = pDuration[i];
      pMsg->maxEaEvents[i] = pMaxEaEvents[i];
    }

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop advertising for the given advertising set. If the number of sets is set to 0
 *          then all advertising sets are disabled.
 *
 *  \param  numSets       Number of advertising sets to disable.
 *  \param  pAdvHandles   Advertising handles array.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvStop(uint8_t numSets, uint8_t *pAdvHandles)
{
  uint8_t i;
  dmAdvApiStop_t *pMsg;

  WSF_ASSERT(numSets <= DM_NUM_ADV_SETS);

  if ((pMsg = WsfMsgAlloc(sizeof(dmAdvApiStop_t))) != NULL)
  {
    pMsg->hdr.event = DM_ADV_MSG_API_STOP;
    pMsg->numSets = numSets;

    for (i = 0; i < numSets; i++)
    {
      pMsg->advHandle[i] = pAdvHandles[i];
    }

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Remove an advertising set.
 *
 *  \param  advHandle     Advertising handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvRemoveAdvSet(uint8_t advHandle)
{
  dmAdvApiRemove_t *pMsg;

  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  if ((pMsg = WsfMsgAlloc(sizeof(dmAdvApiRemove_t))) != NULL)
  {
    pMsg->hdr.event = DM_ADV_MSG_API_REMOVE;
    pMsg->advHandle = advHandle;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Clear advertising sets.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvClearAdvSets(void)
{
  wsfMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(wsfMsgHdr_t))) != NULL)
  {
    pMsg->event = DM_ADV_MSG_API_CLEAR;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the random device address for a given advertising set.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  pAddr         Random device address.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvSetRandAddr(uint8_t advHandle, const uint8_t *pAddr)
{
  dmAdvApiSetRandAddr_t *pMsg;

  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  if ((pMsg = WsfMsgAlloc(sizeof(dmAdvApiSetRandAddr_t))) != NULL)
  {
    pMsg->hdr.event = DM_ADV_MSG_API_SET_RAND_ADDR;
    pMsg->advHandle = advHandle;
    BdaCpy(pMsg->addr, pAddr);
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the minimum and maximum advertising intervals.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  intervalMin   Minimum advertising interval.
 *  \param  intervalMax   Maximum advertising interval.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvSetInterval(uint8_t advHandle, uint16_t intervalMin, uint16_t intervalMax)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmAdvCb.intervalMin[advHandle] = intervalMin;
  dmAdvCb.intervalMax[advHandle] = intervalMax;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Include or exclude certain channels from the advertising channel map.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  channelMap    Advertising channel map.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvSetChannelMap(uint8_t advHandle, uint8_t channelMap)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmAdvCb.channelMap[advHandle] = channelMap;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set the local address type used while advertising.  This function can be used to
 *          configure advertising to use a random address.
 *
 *  \param  addrType      Address type.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvSetAddrType(uint8_t addrType)
{
  WsfTaskLock();
  dmCb.advAddrType = addrType;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set the value of an advertising data element in the given advertising or
 *          scan response data.  If the element already exists in the data then it is replaced
 *          with the new value.  If the element does not exist in the data it is appended
 *          to it, space permitting.
 *
 *  \param  adType        Advertising data element type.
 *  \param  len           Length of the value.  Maximum length is 29 bytes.
 *  \param  pValue        Pointer to the value.
 *  \param  pAdvDataLen   Advertising or scan response data length.  The new length is returned
 *                        in this parameter.
 *  \param  pAdvData      Pointer to advertising or scan response data.
 *  \param  advDataBufLen Length of the advertising or scan response data buffer maintained by
 *                        Application.
 *
 *  \return TRUE if the element was successfully added to the data, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t DmAdvSetAdValue(uint8_t adType, uint8_t len, uint8_t *pValue, uint16_t *pAdvDataLen,
                       uint8_t *pAdvData, uint16_t advDataBufLen)
{
  uint8_t *pElem;
  uint8_t *pNext;
  uint16_t totalLen;
  uint16_t newAdvDataLen;
  bool_t  valueSet = FALSE;

  /* find ad type in data */
  if ((pElem = DmFindAdType(adType, *pAdvDataLen, pAdvData)) != NULL)
  {
    /* if new length equals existing length */
    if ((len + 1) == pElem[DM_AD_LEN_IDX])
    {
      /* copy new ad value to data in existing location */
      memcpy(&pElem[DM_AD_DATA_IDX], pValue, len);
      valueSet = TRUE;
    }
    /* else if new value can replace old value and still fit */
    else
    {
      /* calculate the advertising data length if old element was replaced with new */
      newAdvDataLen = *pAdvDataLen + len + 1 - pElem[DM_AD_LEN_IDX];

      /* if length is ok */
      if (newAdvDataLen <= advDataBufLen)
      {
        /* delete item (then we will replace it) */

        /* get the start of element that follows the element to delete */
        totalLen = pElem[DM_AD_LEN_IDX] + 1;
        pNext = pElem + totalLen;

        /* move data from start of next element to start of current item;
         * length is equal the data that remains after pNext
         */
        memmove(pElem, pNext, *pAdvDataLen - (uint8_t)(pNext - pAdvData));

        /* update length */
        *pAdvDataLen = *pAdvDataLen - totalLen;
      }
    }
  }

  /* if value not set */
  if (!valueSet)
  {
    /* if new value fits */
    if ((*pAdvDataLen + len + 2) <= advDataBufLen)
    {
      /* construct AD item in advertising data */
      pElem = &pAdvData[*pAdvDataLen];
      *pElem++ = len + 1;
      *pElem++ = adType;
      memcpy(pElem, pValue, len);

      /* update length */
      *pAdvDataLen = *pAdvDataLen + len + 2;

      valueSet = TRUE;
    }
  }

  return valueSet;
}

/*************************************************************************************************/
/*!
 *  \brief  Set the device name in the given advertising or scan response data.  If the
 *          device can only fit in the data if it is shortened, the name is shortened
 *          and the AD type is changed to DM_ADV_TYPE_SHORT_NAME.
 *
 *  \param  len           Length of the name.  Maximum length is 29 bytes.
 *  \param  pValue        Pointer to the name in UTF-8 format.
 *  \param  pAdvDataLen   Advertising or scan response data length.  The new length is returned
 *                        in this parameter.
 *  \param  pAdvData      Pointer to advertising or scan response data.
 *  \param  advDataBufLen Length of the advertising or scan response data buffer maintained by
 *                        Application.
 *
 *  \return TRUE if the element was successfully added to the data, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t DmAdvSetName(uint8_t len, uint8_t *pValue, uint16_t *pAdvDataLen, uint8_t *pAdvData,
                    uint16_t advDataBufLen)
{
  uint8_t *pElem;
  uint8_t *pNext;
  uint16_t totalLen;
  uint8_t adType;

  /* find name in data */
  if ((pElem = DmFindAdType(DM_ADV_TYPE_LOCAL_NAME, *pAdvDataLen, pAdvData)) == NULL)
  {
    pElem = DmFindAdType(DM_ADV_TYPE_SHORT_NAME, *pAdvDataLen, pAdvData);
  }

  /* if found delete it */
  if (pElem != NULL)
  {
    /* get the start of element that follows the element to delete */
    totalLen = pElem[DM_AD_LEN_IDX] + 1;
    pNext = pElem + totalLen;

    /* move data from start of next element to start of current item;
     * length is equal the data that remains after pNext
     */
    memmove(pElem, pNext, *pAdvDataLen - (uint8_t)(pNext - pAdvData));

    /* update length */
    *pAdvDataLen = *pAdvDataLen - totalLen;
  }

  /* if name will fit */
  if (*pAdvDataLen <= (advDataBufLen - 2))
  {
    /* if full device name won't fit */
    if ((*pAdvDataLen + len + 2) > advDataBufLen)
    {
      /* adjust length so that it will fit */
      len = (advDataBufLen - 2) - *pAdvDataLen;

      /* set ad type to shortened local name */
      adType = DM_ADV_TYPE_SHORT_NAME;
    }
    else
    {
      adType = DM_ADV_TYPE_LOCAL_NAME;
    }

    /* construct AD item in advertising data */
    pElem = &pAdvData[*pAdvDataLen];
    *pElem++ = len + 1;
    *pElem++ = adType;
    memcpy(pElem, pValue, len);

    /* update length */
    *pAdvDataLen = *pAdvDataLen + len + 2;

    return TRUE;
  }

  return FALSE;
}
