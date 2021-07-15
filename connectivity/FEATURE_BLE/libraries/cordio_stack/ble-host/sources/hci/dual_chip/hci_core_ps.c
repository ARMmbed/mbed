/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI core platform-specific module for dual-chip.
 *
 *  Copyright (c) 2009-2018 Arm Ltd. All Rights Reserved.
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
 *
 *  This module implements core platform-dependent HCI features for transmit data path, receive
 *  data path, the 'optimization' API, and the main event handler. This module contains separate
 *  implementations for dual chip and single chip.
 */
/*************************************************************************************************/

#include <string.h>
#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bda.h"
#include "util/bstream.h"
#include "hci_core.h"
#include "hci_tr.h"
#include "hci_cmd.h"
#include "hci_evt.h"
#include "hci_api.h"
#include "hci_main.h"

#include "mbed_wsf_trace.h"

/*************************************************************************************************/
/*!
 *  \brief  HCI core initialization.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreInit(void)
{
  hciCmdInit();
}

/*************************************************************************************************/
/*!
 *  \brief  Handle an HCI Number of Completed Packets event.
 *
 *  \param  pMsg    Message containing the HCI Number of Completed Packets event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreNumCmplPkts(uint8_t *pMsg)
{
  uint8_t         numHandles;
  uint16_t        bufs;
  uint16_t        handle;

  uint8_t         availBufs = 0;
  hciCoreConn_t   *pConn;

  uint8_t         isoAvailBufs = 0;
  hciCoreCis_t    *pCis;
  hciCoreBis_t    *pBis;

  /* parse number of handles */
  BSTREAM_TO_UINT8(numHandles, pMsg);

  /* for each handle in event */
  while (numHandles-- > 0)
  {
    /* parse handle and number of buffers */
    BSTREAM_TO_UINT16(handle, pMsg);
    BSTREAM_TO_UINT16(bufs, pMsg);

    if ((pConn = hciCoreConnByHandle(handle)) != NULL)
    {
      /* decrement outstanding buffer count to controller */
      pConn->outBufs -= (uint8_t) bufs;

      /* decrement queued buffer count for this connection */
      pConn->queuedBufs -= (uint8_t) bufs;

      /* increment available buffer count */
      availBufs += (uint8_t) bufs;

      /* call flow control callback */
      if (pConn->flowDisabled && pConn->queuedBufs <= hciCoreCb.aclQueueLo)
      {
        pConn->flowDisabled = FALSE;
        (*hciCb.flowCback)(handle, FALSE);
      }
    }

    else if ((pCis = isoFcnIfTbl[HCI_CORE_CIS]->findByHandle(handle)) != NULL)
    {
      pCis->txCb.outBufs -= (uint8_t) bufs;

      isoAvailBufs += (uint8_t) bufs;

      /* TODO: Flow control callback once CIS fragmentation is implemented. */
    }

    else if ((pBis = isoFcnIfTbl[HCI_CORE_BIS]->findByHandle(handle)) != NULL)
    {
      if (pBis->pBig->role == HCI_ROLE_MASTER)
      {
        /* Should not happen, but protect the master role data. */
        HCI_TRACE_WARN1("BIS Master received NUM_CMPL_PKTS evt. Handle=%d", pBis->handle);
        isoAvailBufs += (uint8_t) bufs;
        return;
      }

      pBis->roleData.slv.txCb.outBufs -= (uint8_t) bufs;

      isoAvailBufs += (uint8_t) bufs;

      /* TODO: Flow control callback once BIS fragmentation is implemented. */
    }
  }

  /* service ACL TX data path */
  hciCoreTxReady(availBufs);

  /* service ISO TX datapath */
  hciIsoTxReadyCb(isoAvailBufs);
}

/*************************************************************************************************/
/*!
 *  \brief  Send a received HCI event or ACL packet to the HCI event handler.
 *
 *  \param  msgType       Message type:  HCI_ACL_TYPE or HCI_EVT_TYPE.
 *  \param  pCoreRecvMsg  Pointer to received message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreRecv(uint8_t msgType, uint8_t *pCoreRecvMsg)
{
  /* dump event for protocol analysis */
  if (msgType == HCI_EVT_TYPE)
  {
    HCI_PDUMP_EVT(*(pCoreRecvMsg + 1) + HCI_EVT_HDR_LEN, pCoreRecvMsg);
  }
  else if (msgType == HCI_ACL_TYPE)
  {
    HCI_PDUMP_RX_ACL(*(pCoreRecvMsg + 2) + HCI_ACL_HDR_LEN, pCoreRecvMsg);
  }
  else if (msgType == HCI_ISO_TYPE)
  {
    HCI_PDUMP_RX_ISO(*(pCoreRecvMsg + 2) + HCI_ACL_HDR_LEN, pCoreRecvMsg);
  }

  /* queue buffer */
  WsfMsgEnq(&hciCb.rxQueue, (wsfHandlerId_t) msgType, pCoreRecvMsg);

  /* set event */
  WsfSetEvent(hciCb.handlerId, HCI_EVT_RX);
}

/*************************************************************************************************/
/*!
 *  \brief  WSF event handler for core HCI.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciCoreHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
  uint8_t         *pBuf;
  wsfHandlerId_t  handlerId;

  /* Handle message */
  if (pMsg != NULL)
  {
    /* Handle HCI command timeout */
    if (pMsg->event == HCI_MSG_CMD_TIMEOUT)
    {
      hciCmdTimeout(pMsg);
    }
  }
  /* Handle events */
  else if (event & HCI_EVT_RX)
  {
    /* Process rx queue */
    while ((pBuf = WsfMsgDeq(&hciCb.rxQueue, &handlerId)) != NULL)
    {
      /* Handle incoming HCI events */
      if (handlerId == HCI_EVT_TYPE)
      {
        /* Parse/process events */
        hciEvtProcessMsg(pBuf);

        /* Handle events during reset sequence */
        if (hciCb.resetting)
        {
          hciCoreResetSequence(pBuf);
        }

        /* Free buffer */
        WsfMsgFree(pBuf);
      }
      /* Handle ACL data */
      else if (handlerId == HCI_ACL_TYPE)
      {
        /* Reassemble */
        if ((pBuf = hciCoreAclReassembly(pBuf)) != NULL)
        {
          /* Call ACL callback; client will free buffer */
          hciCb.aclCback(pBuf);
        }
      }
      /* Handle ISO data */
      else
      {
        if ((pBuf = hciIsoReassembleCb(pBuf)) != NULL)
        {
          /* Call ISO callback; client will free buffer */
          hciCb.isoCback(pBuf);
        }
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Return a pointer to the BD address of this device.
 *
 *  \return Pointer to the BD address.
 */
/*************************************************************************************************/
uint8_t *HciGetBdAddr(void)
{
  return hciCoreCb.bdAddr;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the white list size.
 *
 *  \return White list size.
 */
/*************************************************************************************************/
uint8_t HciGetWhiteListSize(void)
{
  return hciCoreCb.whiteListSize;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the advertising transmit power.
 *
 *  \return Advertising transmit power.
 */
/*************************************************************************************************/
int8_t HciGetAdvTxPwr(void)
{
  return hciCoreCb.advTxPwr;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the ACL buffer size supported by the controller.
 *
 *  \return ACL buffer size.
 */
/*************************************************************************************************/
uint16_t HciGetBufSize(void)
{
  return hciCoreCb.bufSize;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the number of ACL buffers supported by the controller.
 *
 *  \return Number of ACL buffers.
 */
/*************************************************************************************************/
uint8_t HciGetNumBufs(void)
{
  return hciCoreCb.numBufs;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the states supported by the controller.
 *
 *  \return Pointer to the supported states array.
 */
/*************************************************************************************************/
uint8_t *HciGetSupStates(void)
{
  return hciCoreCb.leStates;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the LE supported features supported by the controller.
 *
 *  \return Supported features.
 */
/*************************************************************************************************/
uint64_t HciGetLeSupFeat(void)
{
  return hciCoreCb.leSupFeat;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the LE supported features supported by the controller.
 *
 *  \return Supported features.
 */
/*************************************************************************************************/
uint32_t HciGetLeSupFeat32(void)
{
  return (uint32_t) hciCoreCb.leSupFeat;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the maximum reassembled RX ACL packet length.
 *
 *  \return ACL packet length.
 */
/*************************************************************************************************/
uint16_t HciGetMaxRxAclLen(void)
{
  return hciCoreCb.maxRxAclLen;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the resolving list size.
 *
 *  \return resolving list size.
 */
/*************************************************************************************************/
uint8_t HciGetResolvingListSize(void)
{
  return hciCoreCb.resListSize;
}

/*************************************************************************************************/
/*!
 *  \brief  Whether LL Privacy is supported.
 *
 *  \return TRUE if LL Privacy is supported. FALSE, otherwise.
 */
/*************************************************************************************************/
bool_t HciLlPrivacySupported(void)
{
  return (hciCoreCb.resListSize > 0) ? TRUE : FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the maximum advertisement (or scan response) data length supported by the Controller.
 *
 *  \return Maximum advertisement data length.
 */
/*************************************************************************************************/
uint16_t HciGetMaxAdvDataLen(void)
{
  return hciCoreCb.maxAdvDataLen;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the maximum number of advertising sets supported by the Controller.
 *
 *  \return Maximum number of advertising sets.
 */
/*************************************************************************************************/
uint8_t HciGetNumSupAdvSets(void)
{
  return hciCoreCb.numSupAdvSets;
}

/*************************************************************************************************/
/*!
 *  \brief  Whether LE Advertising Extensions is supported.
 *
 *  \return TRUE if LE Advertising Extensions is supported. FALSE, otherwise.
 */
/*************************************************************************************************/
bool_t HciLeAdvExtSupported(void)
{
  return (hciCoreCb.numSupAdvSets > 0) ? TRUE : FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the periodic advertising list size.
 *
 *  \return periodic advertising list size.
 */
/*************************************************************************************************/
uint8_t HciGetPerAdvListSize(void)
{
  return hciCoreCb.perAdvListSize;
}

/*************************************************************************************************/
/*!
 *  \brief  Return a pointer to the local version information.
 *
 *  \return Pointer to the local version information.
 */
/*************************************************************************************************/
hciLocalVerInfo_t *HciGetLocalVerInfo(void)
{
  return &hciCoreCb.locVerInfo;
}
