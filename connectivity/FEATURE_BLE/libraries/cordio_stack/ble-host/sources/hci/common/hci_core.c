/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI core module, platform independent functions.
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
 * This module implements core platform independent HCI features for transmit data path,
 * fragmentation, reassembly, and connection management.
 */
/*************************************************************************************************/

#include <string.h>
#include "wsf_types.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "wsf_assert.h"
#include "util/bda.h"
#include "util/bstream.h"
#include "hci_core.h"
#include "hci_tr.h"
#include "hci_cmd.h"
#include "hci_api.h"
#include "hci_main.h"
#include "l2c_defs.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Default ACL buffer flow control watermark levels */
#ifndef HCI_ACL_QUEUE_HI
#define HCI_ACL_QUEUE_HI          5             /* Disable flow when this many buffers queued */
#endif
#ifndef HCI_ACL_QUEUE_LO
#define HCI_ACL_QUEUE_LO          1             /* Enable flow when this many buffers queued */
#endif

/* Default maximum ACL packet size for reassembly */
#ifndef HCI_MAX_RX_ACL_LEN
#define HCI_MAX_RX_ACL_LEN        HCI_ACL_DEFAULT_LEN
#endif

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void hciCoreIsoNullOpenCloseFunction(uint16_t handle);
static void *hciCoreNullFindByHandleFunction(uint16_t handle);
static uint8_t *hciCoreNullAssembleIsoFunction(uint8_t *pData);
static void hciCoreNullOpenGroupFunction(uint8_t id, uint8_t count, uint16_t *pHandles, uint8_t role);
static void hciCoreNullCloseGroupFUnction(uint8_t id);
static void hciCoreNullIsoTxReady(uint8_t bufs);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Event mask */
const uint8_t hciEventMask[HCI_EVT_MASK_LEN] =
{
  HCI_EVT_MASK_DISCONNECT_CMPL |                  /* Byte 0 */
  HCI_EVT_MASK_ENC_CHANGE,                        /* Byte 0 */
  HCI_EVT_MASK_READ_REMOTE_VER_INFO_CMPL |        /* Byte 1 */
  HCI_EVT_MASK_HW_ERROR,                          /* Byte 1 */
  0,                                              /* Byte 2 */
  HCI_EVT_MASK_DATA_BUF_OVERFLOW,                 /* Byte 3 */
  0,                                              /* Byte 4 */
  HCI_EVT_MASK_ENC_KEY_REFRESH_CMPL,              /* Byte 5 */
  0,                                              /* Byte 6 */
  HCI_EVT_MASK_LE_META                            /* Byte 7 */
};

/* LE event mask */
const uint8_t hciLeEventMask[HCI_LE_EVT_MASK_LEN] =
{
  HCI_EVT_MASK_LE_CONN_CMPL_EVT |                 /* Byte 0 */
  HCI_EVT_MASK_LE_ADV_REPORT_EVT |                /* Byte 0 */
  HCI_EVT_MASK_LE_CONN_UPDATE_CMPL_EVT |          /* Byte 0 */
  HCI_EVT_MASK_LE_READ_REMOTE_FEAT_CMPL_EVT |     /* Byte 0 */
  HCI_EVT_MASK_LE_LTK_REQ_EVT |                   /* Byte 0 */
  HCI_EVT_MASK_LE_REMOTE_CONN_PARAM_REQ_EVT |     /* Byte 0 */
  HCI_EVT_MASK_LE_DATA_LEN_CHANGE_EVT |           /* Byte 0 */
  HCI_EVT_MASK_LE_READ_LOCAL_P256_PUB_KEY_CMPL,   /* Byte 0 */
  HCI_EVT_MASK_LE_GENERATE_DHKEY_CMPL |           /* Byte 1 */
  HCI_EVT_MASK_LE_ENHANCED_CONN_CMPL_EVT |        /* Byte 1 */
  HCI_EVT_MASK_LE_DIRECT_ADV_REPORT_EVT |         /* Byte 1 */
  HCI_EVT_MASK_LE_PHY_UPDATE_CMPL_EVT |           /* Byte 1 */
  HCI_EVT_MASK_LE_EXT_ADV_REPORT_EVT |            /* Byte 1 */
  HCI_EVT_MASK_LE_PER_ADV_SYNC_EST_EVT |          /* Byte 1 */
  HCI_EVT_MASK_LE_PER_ADV_REPORT_EVT |            /* Byte 1 */
  HCI_EVT_MASK_LE_PER_ADV_SYNC_LOST_EVT,          /* Byte 1 */
  HCI_EVT_MASK_LE_SCAN_TIMEOUT_EVT |              /* Byte 2 */
  HCI_EVT_MASK_LE_ADV_SET_TERM_EVT |              /* Byte 2 */
  HCI_EVT_MASK_LE_SCAN_REQ_RCVD_EVT |             /* Byte 2 */
  HCI_EVT_MASK_LE_CH_SEL_ALGO_EVT |               /* Byte 2 */
  HCI_EVT_MASK_LE_CONN_IQ_REPORT_EVT |            /* Byte 2 */
  HCI_EVT_MASK_LE_CTE_REQ_FAILED_EVT |            /* Byte 2 */
  HCI_EVT_MASK_LE_PER_SYNC_TRSF_RCVT_EVT,         /* Byte 2 */
  HCI_EVT_MASK_LE_CIS_EST_EVT |                   /* Byte 3 */
  HCI_EVT_MASK_LE_CIS_REQ_EVT |                   /* Byte 3 */
  HCI_EVT_MASK_LE_CREATE_BIG_CMPL_EVT|            /* Byte 3 */
  HCI_EVT_MASK_LE_TERMINATE_BIG_CMPL_EVT |        /* Byte 3 */
  HCI_EVT_MASK_LE_BIG_SYNC_EST_EVT |              /* Byte 3 */
  HCI_EVT_MASK_LE_BIG_SYNC_LOST_EVT |             /* Byte 3 */
  HCI_EVT_MASK_LE_PEER_SCA_CMPL_EVT |             /* Byte 3 */
  HCI_EVT_MASK_LE_PATH_LOSS_REPORT_EVT,           /* Byte 3 */
  HCI_EVT_MASK_LE_TX_POWER_REPORT_EVT |           /* Byte 4 */
  HCI_EVT_MASK_LE_BIG_INFO_ADV_RPT_EVT,           /* Byte 4 */
  0,                                              /* Byte 5 */
  0,                                              /* Byte 6 */
  0                                               /* Byte 7 */
};

/* event mask page 2 */
const uint8_t hciEventMaskPage2[HCI_EVT_MASK_PAGE_2_LEN] =
{
  0,                                              /* Byte 0 */
  0,                                              /* Byte 1 */
  HCI_EVT_MASK_AUTH_PAYLOAD_TIMEOUT,              /* Byte 2 */
  0,                                              /* Byte 3 */
  0,                                              /* Byte 4 */
  0,                                              /* Byte 5 */
  0,                                              /* Byte 6 */
  0                                               /* Byte 7 */
};

/* LE supported features configuration mask */
uint64_t hciLeSupFeatCfg =
  HCI_LE_SUP_FEAT_ENCRYPTION                 |    /* LE Encryption */
  HCI_LE_SUP_FEAT_CONN_PARAM_REQ_PROC        |    /* Connection Parameters Request Procedure */
  HCI_LE_SUP_FEAT_EXT_REJECT_IND             |    /* Extended Reject Indication */
  HCI_LE_SUP_FEAT_SLV_INIT_FEAT_EXCH         |    /* Slave-initiated Features Exchange */
  HCI_LE_SUP_FEAT_LE_PING                    |    /* LE Ping */
  HCI_LE_SUP_FEAT_DATA_LEN_EXT               |    /* LE Data Packet Length Extension */
  HCI_LE_SUP_FEAT_PRIVACY                    |    /* LL Privacy */
  HCI_LE_SUP_FEAT_EXT_SCAN_FILT_POLICY       |    /* Extended Scanner Filter Policies */
  HCI_LE_SUP_FEAT_STABLE_MOD_IDX_TRANSMITTER |    /* Stable Modulation Index - Transmitter supported */
  HCI_LE_SUP_FEAT_STABLE_MOD_IDX_RECEIVER;        /* Stable Modulation Index - Receiver supported */

/* Default ISO function inteface */
static const isoFcnIf_t isoFcnDefault =
{
  hciCoreIsoNullOpenCloseFunction,
  hciCoreIsoNullOpenCloseFunction,
  hciCoreNullFindByHandleFunction,
  hciCoreNullOpenGroupFunction,
  hciCoreNullCloseGroupFUnction
};

/* Component function interface table indexed ISO type */
isoFcnIf_t *isoFcnIfTbl[HCI_CORE_TOTAL_ISO_TYPES] =
{
  (isoFcnIf_t *) &isoFcnDefault,                /* HCI_CORE_CIS */
  (isoFcnIf_t *) &isoFcnDefault,                /* HCI_CORE_BIS */
};

/* Assembling received ISO data */
hciIsoReassembleCb_t hciIsoReassembleCb = hciCoreNullAssembleIsoFunction;

/* Servicing ISO TX data path */
hciIsoTxReadyCb_t hciIsoTxReadyCb = hciCoreNullIsoTxReady;

/* Control block */
hciCoreCb_t hciCoreCb;

/*************************************************************************************************/
/*!
 *  \brief  Allocate a connection structure.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void hciCoreConnAlloc(uint16_t handle)
{
  uint8_t         i;
  hciCoreConn_t   *pConn = hciCoreCb.conn;

  /* find available connection struct */
  for (i = DM_CONN_MAX; i > 0; i--, pConn++)
  {
    if (pConn->handle == HCI_HANDLE_NONE)
    {
      /* allocate and initialize */
      pConn->handle = handle;
      pConn->flowDisabled = FALSE;
      pConn->outBufs = 0;
      pConn->queuedBufs = 0;

      return;
    }
  }

  HCI_TRACE_WARN0("HCI conn struct alloc failure");
}

/*************************************************************************************************/
/*!
 *  \brief  Free a connection structure.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void hciCoreConnFree(uint16_t handle)
{
  uint8_t         i;
  hciCoreConn_t   *pConn = hciCoreCb.conn;

  /* find connection struct */
  for (i = DM_CONN_MAX; i > 0; i--, pConn++)
  {
    if (pConn->handle == handle)
    {
      /* free any fragmenting ACL packet */
      if (pConn->pTxAclPkt != NULL)
      {
        WsfMsgFree(pConn->pTxAclPkt);
        pConn->pTxAclPkt = NULL;
      }
      pConn->fragmenting = FALSE;

      if (pConn->pRxAclPkt != NULL)
      {
        WsfMsgFree(pConn->pRxAclPkt);
        pConn->pRxAclPkt = NULL;
      }

      /* free structure */
      pConn->handle = HCI_HANDLE_NONE;

      /* optional: iterate through tx ACL queue and free any buffers with this handle */

      /* outstanding buffers are now available; service TX data path */
      hciCoreTxReady(pConn->outBufs);

      return;
    }
  }

  HCI_TRACE_WARN1("hciCoreConnFree handle not found:%u", handle);
}

/*************************************************************************************************/
/*!
 *  \brief  Get a connection structure by handle
 *
 *  \param  handle  Connection handle.
 *
 *  \return Pointer to connection structure or NULL if not found.
 */
/*************************************************************************************************/
hciCoreConn_t *hciCoreConnByHandle(uint16_t handle)
{
  uint8_t         i;
  hciCoreConn_t   *pConn = hciCoreCb.conn;

  /* find available connection struct */
  for (i = DM_CONN_MAX; i > 0; i--, pConn++)
  {
    if (pConn->handle == handle)
    {
      return pConn;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the next connection structure with a packet fragment to send.
 *
 *  \return Pointer to connection structure or NULL if not found.
 */
/*************************************************************************************************/
static hciCoreConn_t *hciCoreNextConnFragment(void)
{
  uint8_t         i;
  hciCoreConn_t   *pConn = hciCoreCb.conn;

  /* find connection struct */
  for (i = DM_CONN_MAX; i > 0; i--, pConn++)
  {
    if (pConn->handle != HCI_HANDLE_NONE && pConn->fragmenting)
    {
      return pConn;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Perform internal processing on HCI connection open.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreConnOpen(uint16_t handle)
{
  /* allocate connection structure */
  hciCoreConnAlloc(handle);
}

/*************************************************************************************************/
/*!
 *  \brief  Perform internal processing on HCI connection close.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreConnClose(uint16_t handle)
{
  /* free connection structure */
  hciCoreConnFree(handle);
}

/*************************************************************************************************/
/*!
 *  \brief  Send ACL data to transport.
 *
 *  \param  pConn    Pointer to connection structure.
 *  \param  pData    WSF buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreSendAclData(hciCoreConn_t *pConn, uint8_t *pData)
{
  /* increment outstanding buf count for handle */
  pConn->outBufs++;

  /* send to transport */
  hciTrSendAclData(pConn, pData);

  /* decrement available buffer count */
  if (hciCoreCb.availBufs > 0)
  {
    hciCoreCb.availBufs--;
  }
  else
  {
    HCI_TRACE_WARN0("hciCoreSendAclData availBufs=0");
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Service the TX data path.
 *
 *  \param  bufs    Number of new buffers now available.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreTxReady(uint8_t bufs)
{
  uint8_t         *pData;
  wsfHandlerId_t  handlerId;
  uint16_t        handle;
  uint16_t        len;
  hciCoreConn_t   *pConn;

  hciCoreCb.availBufs = WSF_MIN(hciCoreCb.availBufs + bufs, hciCoreCb.numBufs);

  /* service ACL data queue and send as many buffers as we can */
  while (hciCoreCb.availBufs > 0)
  {
    /* send continuation of any fragments first */
    if (hciCoreTxAclContinue(NULL) == FALSE)
    {
      /* if no fragments then check for any queued ACL data */
      if ((pData = WsfMsgDeq(&hciCoreCb.aclQueue, &handlerId)) != NULL)
      {
        /* parse handle and length */
        BYTES_TO_UINT16(handle, pData);
        BYTES_TO_UINT16(len, &pData[2]);

        /* look up conn structure and send data */
        if ((pConn = hciCoreConnByHandle(handle)) != NULL)
        {
          hciCoreTxAclStart(pConn, len, pData);
        }
        /* handle not found, connection must be closed */
        else
        {
          /* discard buffer */
          WsfMsgFree(pData);

          HCI_TRACE_WARN1("hciCoreTxReady discarding buffer, handle=%u", handle);
        }
      }
      else
      {
        /* no fragments or queued data to send; we're done */
        break;
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send ACL packets, start of packet.
 *
 *  \param  pConn    Pointer to connection structure.
 *  \param  len      ACL packet length.
 *  \param  pData    WSF buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreTxAclStart(hciCoreConn_t *pConn, uint16_t len, uint8_t *pData)
{
  uint16_t hciLen;

  /* make sure not already fragmenting on this connection */
  WSF_ASSERT(pConn->fragmenting == FALSE);

  hciLen = HciGetBufSize();

  HCI_TRACE_INFO1("hciCoreTxAclStart len=%u", len);

  /* if acl len > controller acl buf len */
  if (len > hciLen)
  {
    /* store remaining acl len = acl len - hci acl buf len */
    pConn->txAclRemLen = len - hciLen;

    /* store position for next fragment */
    pConn->pNextTxFrag = pData + hciLen;

    /* store information required for fragmentation */
    pConn->pTxAclPkt = pData;
    pConn->fragmenting = TRUE;

    /* set acl len in packet to hci acl buf len */
    UINT16_TO_BUF(&pData[2], hciLen);

    /* send the packet */
    hciCoreSendAclData(pConn, pData);

    /* send additional fragments while there are HCI buffers available */
    while ((hciCoreCb.availBufs > 0) && hciCoreTxAclContinue(pConn));
  }
  else
  {
    /* no fragmentation, just send the packet */
    hciCoreSendAclData(pConn, pData);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send ACL packets, continuation of fragmented packets.
 *
 *  \param  pConn    Pointer to connection structure.  If set non-NULL, then a fragment is
 *                   sent from this connection structure.  If NULL the function finds the next
 *                   connection structure with a fragment to be sent.
 *
 *  \return TRUE if packet sent, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t hciCoreTxAclContinue(hciCoreConn_t *pConn)
{
  uint16_t aclLen;

  if (pConn == NULL)
  {
    pConn = hciCoreNextConnFragment();
  }

  if (pConn != NULL)
  {
    /* get next fragment length */
    aclLen = (pConn->txAclRemLen < HciGetBufSize()) ? pConn->txAclRemLen : HciGetBufSize();

    if (aclLen > 0)
    {
      /* decrement remaining length */
      pConn->txAclRemLen -= aclLen;

      /* set handle in packet with continuation bit set */
      UINT16_TO_BUF(pConn->pNextTxFrag, (pConn->handle | HCI_PB_CONTINUE));

      /* set acl len in packet */
      UINT16_TO_BUF(&(pConn->pNextTxFrag[2]), aclLen);

      HCI_TRACE_INFO2("hciCoreTxAclContinue aclLen=%u remLen=%u", aclLen, pConn->txAclRemLen);

      /* send the packet */
      hciCoreSendAclData(pConn, pConn->pNextTxFrag);

      /* set up pointer to next fragment */
      if (pConn->txAclRemLen > 0)
      {
        pConn->pNextTxFrag += aclLen;
      }

      return TRUE;
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  This function is called from the HCI transport layer when transmission of an ACL
 *          packet is complete.
 *
 *  \param  pConn    Pointer to connection structure.
 *  \param  pData    WSF buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreTxAclComplete(hciCoreConn_t *pConn, uint8_t *pData)
{
  /* if fragmenting */
  if (pConn->fragmenting)
  {
    /* check if all fragments sent */
    if (pConn->txAclRemLen == 0)
    {
      /* free original buffer */
      WsfMsgFree(pConn->pTxAclPkt);
      pConn->pTxAclPkt = NULL;
      pConn->fragmenting = FALSE;
      HCI_TRACE_INFO0("hciCoreTxAclComplete free pTxAclPkt");
    }
  }
  else if (pData != NULL)
  {
    WsfMsgFree(pData);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Reassemble an ACL packet.
 *
 *  \param  pData   Input ACL packet.
 *
 *  \return pointer to ACL packet to send, or NULL if no packet to send.
 */
/*************************************************************************************************/
uint8_t *hciCoreAclReassembly(uint8_t *pData)
{
  hciCoreConn_t *pConn;
  uint8_t       *pDataRtn = NULL;
  uint16_t      handle;
  uint16_t      aclLen;
  uint16_t      l2cLen;
  uint16_t      pbf;
  bool_t        freeData = TRUE;

  BYTES_TO_UINT16(handle, pData);
  pbf = handle & HCI_PB_FLAG_MASK;
  handle &= HCI_HANDLE_MASK;
  BYTES_TO_UINT16(aclLen, &pData[2]);

  /* look up connection */
  if ((pConn = hciCoreConnByHandle(handle)) != NULL)
  {
    /* if this is a start packet */
    if (pbf == HCI_PB_START_C2H)
    {
      /* if currently reassembled packet not complete */
      if (pConn->pRxAclPkt != NULL)
      {
        /* discard currently reassembled packet */
        WsfMsgFree(pConn->pRxAclPkt);
        pConn->pRxAclPkt = NULL;
        HCI_TRACE_WARN1("disarded hci rx pkt handle=0x%04x", handle);
      }

      /* read l2cap length */
      if (aclLen >= L2C_HDR_LEN)
      {
        BYTES_TO_UINT16(l2cLen, &pData[4]);

        /* check length vs. configured maximum */
        if ((l2cLen + L2C_HDR_LEN) > hciCoreCb.maxRxAclLen)
        {
          HCI_TRACE_WARN2("l2c len=0x%04x to large for reassembly - max: 0x%04x", l2cLen, hciCoreCb.maxRxAclLen - L2C_HDR_LEN);
        }
        /* if reassembly required */
        else if ((l2cLen + L2C_HDR_LEN) > aclLen)
        {
          /* allocate buffer to store complete l2cap packet */
          if ((pConn->pRxAclPkt = WsfMsgDataAlloc(l2cLen + L2C_HDR_LEN + HCI_ACL_HDR_LEN, 0)) != NULL)
          {
            /* store buffer for reassembly */
            pConn->pNextRxFrag = pConn->pRxAclPkt;

            /* build acl header and copy data */
            UINT16_TO_BSTREAM(pConn->pNextRxFrag, handle);
            UINT16_TO_BSTREAM(pConn->pNextRxFrag, l2cLen + L2C_HDR_LEN);
            memcpy(pConn->pNextRxFrag, &pData[4], aclLen);
            pConn->pNextRxFrag += aclLen;

            /* store remaining length */
            pConn->rxAclRemLen = l2cLen + L2C_HDR_LEN - aclLen;
          }
          else
          {
            /* alloc failed; discard */
            HCI_TRACE_WARN1("reassembly alloc failed len=%u", (l2cLen + L2C_HDR_LEN + HCI_ACL_HDR_LEN));
          }
        }
        else
        {
          /* no reassembly required, pData is ready to go */
          pDataRtn = pData;
          freeData = FALSE;
        }
      }
      else
      {
        /* invalid l2cap packet; discard */
        HCI_TRACE_WARN1("invalid l2c pkt aclLen=%u", aclLen);
      }
    }
    /* else if this is a continuation packet */
    else if (pbf == HCI_PB_CONTINUE)
    {
      /* if expecting a continuation */
      if (pConn->pRxAclPkt != NULL)
      {
        if (aclLen <= pConn->rxAclRemLen)
        {
          /* copy data to start of next fragment */
          memcpy(pConn->pNextRxFrag, &pData[HCI_ACL_HDR_LEN], aclLen);
          pConn->pNextRxFrag += aclLen;

          /* update remaining length */
          pConn->rxAclRemLen -= aclLen;

          /* if reassembly complete return reassembled packet */
          if (pConn->rxAclRemLen == 0)
          {
            pDataRtn = pConn->pRxAclPkt;
            pConn->pRxAclPkt = NULL;
          }
        }
        else
        {
          HCI_TRACE_WARN2("continuation pkt too long len=%u RemLen=%u", aclLen, pConn->rxAclRemLen);
        }
      }
      else
      {
        HCI_TRACE_WARN1("unexpected continuation pkt handle=0x%04x", handle);
      }
    }
    /* else unknown packet type */
    else
    {
      HCI_TRACE_WARN1("unknown pb flags=0x%04x", pbf);
    }
  }
  else
  {
    /* connection not found */
    HCI_TRACE_WARN1("pkt rcvd on unknown handle=0x%04x", (handle & HCI_HANDLE_MASK));
  }

  if (freeData)
  {
    WsfMsgFree(pData);
  }

  return pDataRtn;
}

/*************************************************************************************************/
/*!
 *  \brief  Check if a TX ACL packet is being fragmented.
 *
 *  \param  pContext Connection context.
 *
 *  \return TRUE if fragmenting a TX ACL packet, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t hciCoreTxAclDataFragmented(hciCoreConn_t *pConn)
{
  return pConn->fragmenting;
}

/*************************************************************************************************/
/*!
 *  \brief  HCI core initialization.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciCoreInit(void)
{
  uint8_t   i;

  WSF_QUEUE_INIT(&hciCoreCb.aclQueue);

  for (i = 0; i < DM_CONN_MAX; i++)
  {
    hciCoreCb.conn[i].handle = HCI_HANDLE_NONE;
  }

  hciCoreCb.maxRxAclLen = HCI_MAX_RX_ACL_LEN;
  hciCoreCb.aclQueueHi = HCI_ACL_QUEUE_HI;
  hciCoreCb.aclQueueLo = HCI_ACL_QUEUE_LO;
  hciCoreCb.extResetSeq = NULL;

  hciCoreInit();
}

/*************************************************************************************************/
/*!
 *  \brief  Initiate an HCI reset sequence.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciResetSequence(void)
{
  /* set resetting state */
  hciCb.resetting = TRUE;

  /* start the reset sequence */
  hciCoreResetStart();
}

/*************************************************************************************************/
/*!
 *  \brief  Set the maximum reassembled RX ACL packet length.  Minimum value is 27.
 *
 *  \param  len     ACL packet length.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSetMaxRxAclLen(uint16_t len)
{
  hciCoreCb.maxRxAclLen = len;
}

/*************************************************************************************************/
/*!
 *  \brief  Set TX ACL queue high and low watermarks.
 *
 *  \param  queueHi   Disable flow on a connection when this many ACL buffers are queued.
 *          queueLo   Disable flow on a connection when this many ACL buffers are queued.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSetAclQueueWatermarks(uint8_t queueHi, uint8_t queueLo)
{
  hciCoreCb.aclQueueHi = queueHi;
  hciCoreCb.aclQueueLo = queueLo;
}

/*************************************************************************************************/
/*!
*  \brief   Set LE supported features configuration mask.
*
*  \param   feat    Feature bit to set or clear
*  \param   flag    TRUE to set feature bit and FALSE to clear it
*
*  \return None.
*/
/*************************************************************************************************/
void HciSetLeSupFeat(uint64_t feat, bool_t flag)
{
  /* if asked to include feature */
  if (flag)
  {
    /* set feature bit */
    hciLeSupFeatCfg |= feat;
  }
  else
  {
    /* clear feature bit */
    hciLeSupFeatCfg &= ~feat;
  }
}

/*************************************************************************************************/
/*!
 *  \brief   Set LE supported features configuration mask.
 *
 *  \param   feat    Feature bit to set or clear
 *  \param   flag    TRUE to set feature bit and FALSE to clear it
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSetLeSupFeat32(uint32_t feat, bool_t flag)
{
  /* if asked to include feature */
  if (flag)
  {
    /* set feature bit */
    hciLeSupFeatCfg |= feat;
  }
  else
  {
    /* clear feature bit */
    hciLeSupFeatCfg &= ~feat;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send data from the stack to HCI.
 *
 *  \param  pData    WSF buffer containing an ACL packet
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSendAclData(uint8_t *pData)
{
  uint16_t        handle;
  uint16_t        len;
  hciCoreConn_t   *pConn;

  /* parse handle and length */
  BYTES_TO_UINT16(handle, pData);
  BYTES_TO_UINT16(len, &pData[2]);

  /* look up connection structure */
  if ((pConn = hciCoreConnByHandle(handle)) != NULL)
  {
    /* if queue empty, buffers available and no HCI fragmentation in progress */
    if (WsfQueueEmpty(&hciCoreCb.aclQueue) && (hciCoreCb.availBufs > 0) && (pConn->fragmenting == FALSE))
    {
      /* send data */
      hciCoreTxAclStart(pConn, len, pData);
    }
    else
    {
      /* queue data - message handler ID 'handerId' not used */
      WsfMsgEnq(&hciCoreCb.aclQueue, 0, pData);
    }

    /* increment buffer queue count for this connection with consideration for HCI fragmentation */
    pConn->queuedBufs += ((len - 1) / HciGetBufSize()) + 1;

    /* manage flow control to stack */
    if (pConn->queuedBufs >= hciCoreCb.aclQueueHi && pConn->flowDisabled == FALSE)
    {
      pConn->flowDisabled = TRUE;
      (*hciCb.flowCback)(handle, TRUE);
    }
  }
  /* connection not found, connection must be closed */
  else
  {
    /* discard buffer */
    WsfMsgFree(pData);

    HCI_TRACE_WARN1("HciSendAclData discarding buffer, handle=%u", handle);
  }
}

/**************************************************************************************************
  ISO EMPTY FUNCTIONS
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Default empty function for closing ISO control blocks.
 *
 *  \param  handle  ISO handle
 *
 *  \return None.
 */
/*************************************************************************************************/
static void hciCoreIsoNullOpenCloseFunction(uint16_t handle)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Default empty function for finding ISO handle.
 *
 *  \param  handle  ISO handle
 *
 *  \return Pointer to CIS connection structure or NULL if not found.
 */
/*************************************************************************************************/
static void *hciCoreNullFindByHandleFunction(uint16_t handle)
{
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Default empty function for opening a ISO group
 *
 *  \param  id          Group identifier.
 *  \param  count       Count of handles associated.
 *  \param  pHandles    Associated handles.
 *  \param  role        Role of group.
 *
 */
/*************************************************************************************************/
static void hciCoreNullOpenGroupFunction(uint8_t id, uint8_t count, uint16_t *pHandles, uint8_t role)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Default empty function for closing a ISO group
 *
 *  \param  id          Group identifier.
 *
 */
/*************************************************************************************************/
static void hciCoreNullCloseGroupFUnction(uint8_t id)
{
  return;
}
/*************************************************************************************************/
/*!
 *  \brief  Default empty function for assembling received ISO data.
 *
 *  \param  handle  ISO handle
 *
 *  \return pointer to ISO packet to send, or NULL if no packet to send.
 */
/*************************************************************************************************/
static uint8_t *hciCoreNullAssembleIsoFunction(uint8_t *pData)
{
  /* discard buffer */
  WsfMsgFree(pData);

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Default empty function for servicing the ISO TX data path.
 *
 *  \param  handle  ISO handle
 *
 *  \return None.
 */
/*************************************************************************************************/
static void hciCoreNullIsoTxReady(uint8_t bufs)
{
  return;
}
