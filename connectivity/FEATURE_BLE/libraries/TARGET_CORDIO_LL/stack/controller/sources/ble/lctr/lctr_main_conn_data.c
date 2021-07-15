/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller data path implementation file.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_conn_slave.h"
#include "lctr_int_conn.h"
#include "lctr_pdu_conn.h"
#include "lmgr_api_conn.h"
#include "ll_math.h"
#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include "hci_defs.h"
#include "pal_bb.h"
#include <string.h>

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Data PDU start offset in a buffer. */
#define LCTR_DATA_TX_PDU_START_OFFSET   0

/*! \brief      Fragment header maximum length. */
#define LCTR_FRAG_HDR_MAX_LEN           LL_DATA_HDR_LEN

/*! \brief      Fragment trailer maximum length. */
#define LCTR_FRAG_TRL_MAX_LEN           LL_DATA_MIC_LEN

/**************************************************************************************************
  Data Types
**************************************************************************************************/

#ifdef LCTR_CONN_NO_TIFS_REASSEMBLY
/*! \brief      Transmit buffer descriptor. */
typedef struct
{
  uint16_t aclLen;                        /*!< ACL PDU length. */
  uint16_t fragLen;                       /*!< Fragmentation length. */
  uint8_t  fragCnt;                       /*!< Current fragmentation Tx count. */
  uint32_t pad;                           /*!< Padding to guarantee alignment. */
  uint8_t  data[];                        /*!< Data fragments. */
} lctrTxBufDesc_t;
#else
/*! \brief      Transmit buffer descriptor. */
typedef struct
{
  uint8_t *pAclPdu;                       /*!< ACL PDU. */
  uint16_t aclLen;                        /*!< ACL PDU length. */
  uint8_t  fragLen;                       /*!< Fragmentation length. */
  uint8_t  fragCnt;                       /*!< Current fragmentation Tx count. */
  struct
  {
    uint8_t hdrLen;                       /*!< Data PDU header length. */
    uint8_t trlLen;                       /*!< Data PDU trailer length. */
    uint8_t hdr[LCTR_FRAG_HDR_MAX_LEN];   /*!< Data PDU header. */
    uint8_t trl[LCTR_FRAG_TRL_MAX_LEN];   /*!< Data PDU trailer (i.e., MIC). */
  } frag[];                               /*!< Fragmented Data PDU packet data. */
} lctrTxBufDesc_t;
#endif

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Free list of Tx buffer descriptors. */
static wsfQueue_t lctrTxBufDescQ;

/*! \brief      Completed transmit buffer. */
static uint8_t *pLctrTxCompBuf;

/*! \brief      Completed transmit buffer handle ID. */
static wsfHandlerId_t lctrTxCompBufHandlerId;

/*************************************************************************************************/
/*!
 *  \brief      Check to see whether it is necessary to abort slave latency when slave
 *              latency is not zero and there is data to send.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static void lctrCheckAbortSlvLatency(lctrConnCtx_t *pCtx)
{
  if ((pCtx->role == LL_ROLE_MASTER) ||
      !lctrGetConnOpFlag(pCtx, LL_OP_MODE_FLAG_ENA_SLV_LATENCY_WAKEUP))
  {
    return;
  }

  if (pCtx->maxLatency)
  {
    const uint32_t curTime = PalBbGetCurrentTime();
    uint32_t connIntervalUsec = LCTR_CONN_IND_US(pCtx->connInterval);
    BbOpDesc_t *pOp = &pCtx->connBod;

    if (BbGetTargetTimeDelta(pOp->dueUsec, curTime) > connIntervalUsec)
    {
      /* If the connection BOD is due in the future and after the next immediate anchor point,
       * set the flag to adjust the connection BOD later. */
      pCtx->data.slv.abortSlvLatency = TRUE;

      /* Remove the connection BOD, it will be rescheduled in the end callback. */
      SchRemove(pOp);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize the transmit memory resources.
 *
 *  \param      pFreeMem    Pointer to free memory.
 *  \param      freeMemSize Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 */
/*************************************************************************************************/
uint16_t lctrTxInitMem(uint8_t *pFreeMem, uint32_t freeMemSize)
{
  pLctrTxCompBuf = NULL;

  uint8_t *pAvailMem = pFreeMem;

  const uint16_t maxNumFrag = LL_MATH_DIV_27(pLctrRtCfg->maxAclLen + (LL_MAX_DATA_LEN_MIN - 1));
#ifdef LCTR_CONN_NO_TIFS_REASSEMBLY
  const uint16_t fragAlign = 4;
  const uint16_t descSize = sizeof(lctrTxBufDesc_t) + (maxNumFrag * (LCTR_FRAG_HDR_MAX_LEN + LCTR_FRAG_TRL_MAX_LEN + LL_MAX_DATA_LEN_MIN + fragAlign));
#else
  const uint16_t descSize = sizeof(lctrTxBufDesc_t) + (maxNumFrag * sizeof(((lctrTxBufDesc_t *)0)->frag[0]));
#endif

  LL_TRACE_INFO2("    RAM: %u x %u bytes -- Tx buffer descriptors", pLctrRtCfg->numTxBufs, sizeof(void *) + descSize);

  lctrTxBufDescQ.pHead = NULL;
  lctrTxBufDescQ.pTail = NULL;

  unsigned int i;

  for (i = 0; i < pLctrRtCfg->numTxBufs; i++)
  {
    lctrTxBufDesc_t *pDesc;

    if (((uint32_t)pAvailMem) & 3)
    {
      /* Align to next word. */
      pAvailMem = (uint8_t *)(((uint32_t)pAvailMem & ~3) + sizeof(uint32_t));
    }

    /* Allocate memory. */
    pDesc = (lctrTxBufDesc_t *)pAvailMem;
    pAvailMem += (2 * sizeof(uint32_t)) + descSize;   /* wsfMsg_t header is at most 2 words */

    if (((uint32_t)(pAvailMem - pFreeMem)) > freeMemSize)
    {
      return 0;
    }

    /* Add to free list. */
    WsfQueueEnq(&lctrTxBufDescQ, pDesc);
  }

  return (pAvailMem - pFreeMem);
}

/*************************************************************************************************/
/*!
 *  \brief      Number of bytes that can be transmitted within time on LE 1M PHY.
 *
 *  \param      timeUsec    Time in microseconds.
 *
 *  \return     Number of bytes.
 */
/*************************************************************************************************/
static uint16_t lctrMaxNumBytesWithinUsec1M(uint16_t timeUsec)
{
  if (timeUsec < (LL_MIN_PKT_TIME_US_1M + (LL_DATA_MIC_LEN << 3)))
  {
    return 0;
  }
  /* (timePayload / 8) - bytesMic */
  return ((timeUsec - LL_MIN_PKT_TIME_US_1M) >> 3) - LL_DATA_MIC_LEN;
}

/*************************************************************************************************/
/*!
 *  \brief      Number of bytes that can be transmitted within time on LE 2M PHY.
 *
 *  \param      timeUsec    Time in microseconds.
 *
 *  \return     Number of bytes.
 */
/*************************************************************************************************/
static uint16_t lctrMaxNumBytesWithinUsec2M(uint16_t timeUsec)
{
  if (timeUsec < (LL_MIN_PKT_TIME_US_2M + (LL_DATA_MIC_LEN << 2)))
  {
    return 0;
  }
  /* (timePayload / 4) - bytesMic */

  return ((timeUsec - LL_MIN_PKT_TIME_US_2M) >> 2) - LL_DATA_MIC_LEN;
}

/*************************************************************************************************/
/*!
 *  \brief      Number of bytes that can be transmitted within time on LE Coded PHY.
 *
 *  \param      timeUsec    Time in microseconds.
 *
 *  \return     Number of bytes.
 */
/*************************************************************************************************/
static uint16_t lctrMaxNumBytesWithinUsecCoded(uint16_t timeUsec)
{
  if (timeUsec < (LL_MIN_PKT_TIME_US_CODED_S8 + (LL_DATA_MIC_LEN << 6)))
  {
    return 0;
  }
  /* (timePayload / 64) - bytesMic */
  return ((timeUsec - LL_MIN_PKT_TIME_US_CODED_S8) >> 6) - LL_DATA_MIC_LEN;
}

/*************************************************************************************************/
/*!
 *  \brief      Set packet time restriction.
 *
 *  \param      pCtx    Connection context.
 *  \param      txPhys  Transmit PHYs for PHY update in progress.
 */
/*************************************************************************************************/
void lctrSetPacketTimeRestriction(lctrConnCtx_t *pCtx, uint8_t txPhys)
{
  pCtx->txPhysPending = txPhys;
}

/*************************************************************************************************/
/*!
 *  \brief      Remove packet time restriction.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrRemovePacketTimeRestriction(lctrConnCtx_t *pCtx)
{
  pCtx->txPhysPending = 0;
}

/*************************************************************************************************/
/*!
 *  \brief      Get maximum connection duration.
 *
 *  \param      phy       Transmitter or Receiver PHY.
 *  \param      maxLen    Maximum transmit or receive length.
 *  \param      maxTime   Maximum transmit or receive time.
 *
 *  \return     Maximum connection duration in microseconds.
 */
/*************************************************************************************************/
static inline uint16_t lctrGetMaxConnDurationUsec(uint8_t phy, uint16_t maxLen, uint16_t maxTime)
{
  uint16_t maxDur;

  switch (phy)
  {
    default:
    case BB_PHY_BLE_1M:
      maxDur = WSF_MIN(LL_DATA_LEN_TO_TIME_1M(maxLen, TRUE), maxTime);
      break;
    case BB_PHY_BLE_2M:
      maxDur = WSF_MIN(LL_DATA_LEN_TO_TIME_2M(maxLen, TRUE), maxTime);
      break;
    case BB_PHY_BLE_CODED:
      /* maximum time may be less than minimum packet for coded PHY */
      maxDur = WSF_MIN(LL_DATA_LEN_TO_TIME_CODED_S8(maxLen, TRUE),
                       WSF_MAX(maxTime,
                               LL_DATA_LEN_TO_TIME_CODED_S8(LL_MAX_DATA_LEN_MIN, TRUE)));
      break;
  }

  return maxDur;
}

/*************************************************************************************************/
/*!
 *  \brief      Calculate connection duration.
 *
 *  \param      pCtx      Connection context.
 *  \param      pDataLen  Data lengths.
 *
 *  \return     Connection duration in microseconds.
 */
/*************************************************************************************************/
uint16_t lctrCalcConnDurationUsec(lctrConnCtx_t *pCtx, const lctrDataLen_t *pDataLen)
{
  uint16_t connDur = LL_BLE_TIFS_US +
                     lctrGetMaxConnDurationUsec(pCtx->bleData.chan.txPhy, pDataLen->maxTxLen, pDataLen->maxTxTime) +
                     lctrGetMaxConnDurationUsec(pCtx->bleData.chan.rxPhy, pDataLen->maxRxLen, pDataLen->maxRxTime);

  return connDur;
}

/*************************************************************************************************/
/*!
 *  \brief      Tx fragment length for connection.
 *
 *  \param      pCtx    Connection context.
 *
 *  \return     Tx fragment length for connection.
 */
/*************************************************************************************************/
uint16_t lctrTxFragLen(lctrConnCtx_t *pCtx)
{
  uint8_t txPhy = pCtx->bleData.chan.txPhy;
  uint16_t numBytes;

  if (pCtx->txPhysPending != 0)
  {
    switch (txPhy)
    {
      case BB_PHY_BLE_2M:
        if (pCtx->txPhysPending & LL_PHYS_LE_1M_BIT)
        {
          txPhy = BB_PHY_BLE_1M;
        }
        /* Fallthrough */
      case BB_PHY_BLE_1M:
        if (pCtx->txPhysPending & LL_PHYS_LE_CODED_BIT)
        {
          txPhy = BB_PHY_BLE_CODED;
        }
        /* Fallthrough */
      case BB_PHY_BLE_CODED:
        /* no slower PHYs */
        break;
      default:
        break;
    }
  }

  switch (txPhy)
  {
    default:
    case BB_PHY_BLE_1M:
      numBytes = lctrMaxNumBytesWithinUsec1M(pCtx->effDataPdu.maxTxTime);
      break;
    case BB_PHY_BLE_2M:
      numBytes = lctrMaxNumBytesWithinUsec2M(pCtx->effDataPdu.maxTxTime);
      break;
    case BB_PHY_BLE_CODED:
      numBytes = lctrMaxNumBytesWithinUsecCoded(pCtx->effDataPdu.maxTxTime);
      break;
  }
  return WSF_MIN(pCtx->effDataPdu.maxTxLen, WSF_MAX(LL_MAX_DATA_LEN_MIN, numBytes));
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a Tx buffer descriptor.
 *
 *  \return A Tx buffer descriptor, NULL if allocation fails.
 */
/*************************************************************************************************/
static lctrTxBufDesc_t *lctrAllocConnTxBufDesc(void)
{
  uint8_t *pElem;

  if ((pElem = WsfQueueDeq(&lctrTxBufDescQ)) == NULL)
  {
    return NULL;
  }

  pElem += (2 * sizeof(uint32_t));   /* hide header */

  return (lctrTxBufDesc_t *)pElem;
}

/*************************************************************************************************/
/*!
 *  \brief  Free a Tx buffer descriptor.
 *
 *  \param  pDesc       Pointer to a Tx buffer descriptor.
 */
/*************************************************************************************************/
static void lctrFreeConnTxBufDesc(lctrTxBufDesc_t *pDesc)
{
  uint8_t *pElem = (uint8_t *)pDesc;
  pElem -= (2 * sizeof(uint32_t));   /* recover header */

  WsfQueueEnq(&lctrTxBufDescQ, pElem);
}

/*************************************************************************************************/
/*!
 *  \brief  Assemble data PDU.
 *
 *  \param  pCtx        Connection context.
 *  \param  pAclHdr     ACL header.
 *  \param  pBuf        Buffer to pack the Data PDU header.
 */
/*************************************************************************************************/
static void lctrAssembleDataPdu(lctrConnCtx_t *pCtx, lctrAclHdr_t *pAclHdr, uint8_t *pBuf)
{
  /* All additional fields must be zero'ed since flow control bits will be or'ed in at transmit. */
  lctrDataPduHdr_t dataHdr = { 0 };

  /* Assemble data PDU. */
  switch (pAclHdr->pktBound)
  {
    case LCTR_PB_START_NON_AUTO_FLUSH:
      dataHdr.llid = LL_LLID_START_PDU;
      pCtx->forceStartPdu = FALSE;
      break;

    case LCTR_PB_CONT_FRAG:
      dataHdr.llid = LL_LLID_CONT_PDU;
      /* If the next data Pdu is forced to a start pdu, change pdu type to start. */
      if (pCtx->forceStartPdu == TRUE)
      {
        dataHdr.llid = LL_LLID_START_PDU;
        pCtx->forceStartPdu = FALSE;
      }
      break;

    case LCTR_PB_VS_DATA:
    default:
      dataHdr.llid = LL_LLID_VS_PDU;
      break;
  }

  dataHdr.len = pAclHdr->len;

  lctrPackDataPduHdr(pBuf, &dataHdr);
}

/*************************************************************************************************/
/*!
 *  \brief  Queue data PDU onto ARQ queue.
 *
 *  \param  pCtx        Connection context.
 *  \param  fragLen     Fragment length.
 *  \param  pAclHdr     ACL header.
 *  \param  pAclBuf     ACL buffer.
 */
/*************************************************************************************************/
void lctrTxDataPduQueue(lctrConnCtx_t *pCtx, uint16_t fragLen, lctrAclHdr_t *pAclHdr, uint8_t *pAclBuf)
{
  uint16_t fragOffset = 0;
  uint16_t aclLen = pAclHdr->len;

  lctrTxBufDesc_t *pDesc;

  if ((pDesc = lctrAllocConnTxBufDesc()) == NULL)
  {
    LL_TRACE_ERR1("Failed to allocate transmit buffer descriptor: connHandle=%u", pAclHdr->connHandle);
    WsfMsgFree(pAclBuf);
    lmgrPersistCb.sendCompCback(pAclHdr->connHandle, 1);
    lctrDataTxIncAvailBuf();

    lctrNotifyHostHwErrInd(LL_ERROR_CODE_MEM_CAP_EXCEEDED);
    return;
  }

  if (fragLen >= pAclHdr->len)
  {
    fragLen = pAclHdr->len;
  }

  pDesc->aclLen  = pAclHdr->len;
  pDesc->fragLen = fragLen;
  pDesc->fragCnt = 0;

#ifdef LCTR_CONN_NO_TIFS_REASSEMBLY
  if (pDesc->fragLen != pAclHdr->len)
  {
    /* Adjust fragment length for alignment. */
    const uint16_t fragAlign = 4;
    const uint16_t fragOff   = ((LL_DATA_HDR_LEN + fragLen) % fragAlign);
    if (fragOff != 0)
    {
      fragLen        -= fragOff;
      pDesc->fragLen  = fragLen;
    }
  }

  pAclBuf += HCI_ACL_HDR_LEN;

  uint8_t *pData = &pDesc->data[LL_DATA_HDR_LEN];
  uint8_t *pAclBufStart = pAclBuf;
  pDesc->aclLen = 0;

  while (fragOffset < aclLen)
  {
    const uint16_t dataRem = aclLen - fragOffset;
    const uint16_t fragSize = WSF_MIN(dataRem, fragLen);
    uint8_t *pHdr = pData - LL_DATA_HDR_LEN;
    uint8_t *pMic = pData + fragSize;

    memcpy(pData, pAclBuf, fragSize);

    pAclHdr->len = fragSize;
    lctrAssembleDataPdu(pCtx, pAclHdr, pHdr);

    if (lctrPktEncryptHdlr && lctrPktEncryptHdlr(&pCtx->bleData.chan.enc, pHdr, pData, pMic))
    {
      pData         += LL_DATA_HDR_LEN + fragSize + LL_DATA_MIC_LEN;
      pDesc->fragLen = LL_DATA_HDR_LEN + fragLen  + LL_DATA_MIC_LEN;
      pDesc->aclLen += LL_DATA_HDR_LEN + fragSize + LL_DATA_MIC_LEN;

#if (LL_ENABLE_TESTER)
      pMic[0] ^= (llTesterCb.pktMic >>  0) & 0xFF;
      pMic[1] ^= (llTesterCb.pktMic >>  8) & 0xFF;
      pMic[2] ^= (llTesterCb.pktMic >> 16) & 0xFF;
      pMic[3] ^= (llTesterCb.pktMic >> 24) & 0xFF;
#endif
    }
    else
    {
      pData         += LL_DATA_HDR_LEN + fragSize;
      pDesc->fragLen = LL_DATA_HDR_LEN + fragLen;
      pDesc->aclLen += LL_DATA_HDR_LEN + fragSize;
    }

    pAclHdr->pktBound = LCTR_PB_CONT_FRAG; /* modify for remaining fragments. */

    fragOffset += fragSize;
    pAclBuf    += fragSize;
  }

  WsfMsgFree(pAclBufStart - HCI_ACL_HDR_LEN);
#else

  pAclBuf += HCI_ACL_HDR_LEN;

  uint8_t fragCnt = 0;
  pDesc->pAclPdu = pAclBuf;

  while (fragOffset < aclLen)
  {
    const uint16_t dataRem = aclLen - fragOffset;
    const uint16_t fragSize = WSF_MIN(dataRem, fragLen);

    pAclHdr->len = fragSize;
    lctrAssembleDataPdu(pCtx, pAclHdr, pDesc->frag[fragCnt].hdr);
    pDesc->frag[fragCnt].hdrLen = LL_DATA_HDR_LEN;

    if (lctrPktEncryptHdlr && lctrPktEncryptHdlr(&pCtx->bleData.chan.enc, pDesc->frag[fragCnt].hdr, pAclBuf, pDesc->frag[fragCnt].trl))
    {
      pDesc->frag[fragCnt].trlLen = LL_DATA_MIC_LEN;

#if (LL_ENABLE_TESTER)
      pDesc->frag[fragCnt].trl[0] ^= (llTesterCb.pktMic >>  0) & 0xFF;
      pDesc->frag[fragCnt].trl[1] ^= (llTesterCb.pktMic >>  8) & 0xFF;
      pDesc->frag[fragCnt].trl[2] ^= (llTesterCb.pktMic >> 16) & 0xFF;
      pDesc->frag[fragCnt].trl[3] ^= (llTesterCb.pktMic >> 24) & 0xFF;
#endif
    }
    else
    {
      pDesc->frag[fragCnt].trlLen = 0;
    }

    pAclHdr->pktBound = LCTR_PB_CONT_FRAG; /* modify for remaining fragments. */

    fragOffset += fragSize;
    pAclBuf += fragSize;
    fragCnt++;
  }
#endif

  WsfMsgEnq(&pCtx->txArqQ, pAclHdr->connHandle, (uint8_t *)pDesc);

  lctrCheckAbortSlvLatency(pCtx);
}

/*************************************************************************************************/
/*!
 *  \brief  Queue control PDU onto ARQ queue.
 *
 *  \param  pCtx    Connection context.
 *  \param  pBuf    Data PDU buffer.
 */
/*************************************************************************************************/
void lctrTxCtrlPduQueue(lctrConnCtx_t *pCtx, uint8_t *pBuf)
{
  pCtx->numTxPendCtrlPdu++;

#if (LL_ENABLE_TESTER)
  uint16_t connHandle = LCTR_GET_CONN_HANDLE(pCtx);
#endif

  pBuf -= (LCTR_DATA_TX_PDU_START_OFFSET + LL_DATA_HDR_LEN);

#if (LL_ENABLE_TESTER)
  uint8_t opcode = pBuf[LCTR_DATA_TX_PDU_START_OFFSET + LL_DATA_HDR_LEN];

  if (llTesterCb.dataTriggerEnabled &&
      (llTesterCb.dataTriggerPdu == opcode) &&
      !llTesterCb.dataTriggerAfter)
  {
    lctrForceTxData(connHandle);
  }

  if ((opcode < 32) && ((llTesterCb.txLlcpFilter & (1 << opcode)) != 0))
  {
    WsfMsgFree(pBuf);
    return;
  }
#endif

  if (lctrPktEncryptHdlr)
  {
    uint8_t *pHdr  = pBuf + LCTR_DATA_TX_PDU_START_OFFSET;
    uint8_t *pData = pHdr + LL_DATA_HDR_LEN;
    uint8_t *pMic  = pData + pHdr[LCTR_DATA_PDU_LEN_OFFSET];
    lctrPktEncryptHdlr(&pCtx->bleData.chan.enc, pHdr, pData, pMic);
  }

  WsfMsgEnq(&pCtx->txArqQ, LCTR_CTRL_DATA_HANDLE, pBuf);
  WsfSetEvent(lmgrPersistCb.handlerId, (1 << LCTR_EVENT_TX_PENDING));

  lctrCheckAbortSlvLatency(pCtx);

#if (LL_ENABLE_TESTER)
  if (llTesterCb.dataTriggerEnabled &&
      (llTesterCb.dataTriggerPdu == opcode) &&
      llTesterCb.dataTriggerAfter)
  {
    lctrForceTxData(connHandle);
  }
#endif
}

/*************************************************************************************************/
/*!
 *  \brief  Assemble control PDU.
 *
 *  \param  pBuf        Buffer to pack the data PDU header.
 *  \param  len         Length of buffer.
 *
 *  \return Data PDU header length.
 */
/*************************************************************************************************/
static uint8_t lctrAssembleCtrlPdu(uint8_t *pBuf, uint8_t len)
{
  /* All additional fields must be zero'ed since flow control bits will be or'ed in at transmit. */
  lctrDataPduHdr_t dataHdr =
  {
      .llid = LL_LLID_CTRL_PDU,
      .len  = len
  };

  return lctrPackDataPduHdr(pBuf, &dataHdr);
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a transmit LLCP PDU buffer.
 *
 *  \param  pduLen      Length of the PDU to allocate.
 *
 *  \return Pointer to the start of the PDU data buffer.
 */
/*************************************************************************************************/
uint8_t *lctrTxCtrlPduAlloc(uint8_t pduLen)
{
  uint8_t *pPdu;

  if ((pPdu = WsfMsgAlloc(LCTR_DATA_TX_PDU_START_OFFSET + LCTR_DATA_PDU_LEN(pduLen))) != NULL)
  {
    pPdu += LCTR_DATA_TX_PDU_START_OFFSET;
    pPdu += lctrAssembleCtrlPdu(pPdu, pduLen);
  }

  return pPdu;
}

/*************************************************************************************************/
/*!
 *  \brief  Get top element in Tx queue.
 *
 *  \param  pCtx      Connection context.
 *  \param  descs     Storage for BB descriptors.
 *  \param  pMd       Storage for MD (TRUE if top element is not only element).
 *
 *  \return Number of BB descriptors.
 */
/*************************************************************************************************/
uint8_t lctrTxQueuePeek(lctrConnCtx_t *pCtx, PalBbBleTxBufDesc_t *descs, bool_t *pMd)
{
  wsfHandlerId_t handlerId;
  uint8_t *pTxBuf;
  uint8_t descCnt = 0;
  bool_t md = FALSE;

  /* Do not remove from ARQ until acknowledged by peer. */
  pTxBuf = WsfMsgPeek(&pCtx->txArqQ, &handlerId);
  if (pTxBuf != NULL)
  {
    md = !WsfIsQueueDepthOne(&pCtx->txArqQ);

    /*** Send Data PDU ***/

    if (handlerId != LCTR_CTRL_DATA_HANDLE)
    {
      lctrTxBufDesc_t *pDesc = (lctrTxBufDesc_t *)pTxBuf;
      uint8_t  fragCnt  = pDesc->fragCnt;
      uint16_t fragSize = pDesc->fragLen;
      uint16_t fragOff  = fragSize * fragCnt;

      if (fragOff + fragSize > pDesc->aclLen)
      {
        fragSize = pDesc->aclLen - fragOff;
      }
#ifdef LCTR_CONN_NO_TIFS_REASSEMBLY
      descs[0].len  = fragSize;
      descs[0].pBuf = &pDesc->data[fragOff];
      descCnt = 1;
#else
      descs[0].len  = pDesc->frag[fragCnt].hdrLen;
      descs[0].pBuf = pDesc->frag[fragCnt].hdr;
      descs[1].len  = fragSize;
      descs[1].pBuf = pDesc->pAclPdu + fragOff;
      descCnt = 2;
      if (pDesc->frag[fragCnt].trlLen)
      {
        descs[2].len  = pDesc->frag[fragCnt].trlLen;
        descs[2].pBuf = pDesc->frag[fragCnt].trl;
        descCnt = 3;
      }
#endif
      md = md || ((fragOff + fragSize) < pDesc->aclLen);
    }
    else
    {
      /* Adjust message buffer to the start of the data PDU. */
      pTxBuf += LCTR_DATA_TX_PDU_START_OFFSET;

      descs[0].len  = LL_DATA_HDR_LEN + pTxBuf[LCTR_DATA_PDU_LEN_OFFSET];
      descs[0].pBuf = pTxBuf;
      descCnt = 1;
    }
  }

  *pMd = md;
  return descCnt;
}

/*************************************************************************************************/
/*!
 *  \brief  Pop top element from Tx queue.
 *
 *  \param  pCtx      Connection context.
 *
 *  \return TRUE if element popped.
 */
/*************************************************************************************************/
bool_t lctrTxQueuePop(lctrConnCtx_t *pCtx)
{
  wsfHandlerId_t handlerId;
  uint8_t *pBuf;

  WSF_ASSERT(pLctrTxCompBuf == NULL);

  /* Remove last transmitted PDU. */
  if ((pBuf = WsfMsgPeek(&pCtx->txArqQ, &handlerId)) != NULL)
  {
    if (handlerId != LCTR_CTRL_DATA_HANDLE)
    {
      lctrTxBufDesc_t *pDesc = (lctrTxBufDesc_t *)pBuf;
      uint16_t fragSize = pDesc->fragLen;

      if ((fragSize * (pDesc->fragCnt + 1)) >= pDesc->aclLen)  /* last fragment */
      {
        /* Store buffer for post setup cleanup. */
        pLctrTxCompBuf = pBuf;
        lctrTxCompBufHandlerId = handlerId;

        WsfMsgDeq(&pCtx->txArqQ, &handlerId);
      }
      else
      {
        /* Move to next fragment. */
        pDesc->fragCnt++;
      }
    }
    else
    {
      /* Store buffer for post setup cleanup. */
      pLctrTxCompBuf = pBuf;
      lctrTxCompBufHandlerId = handlerId;

      WsfMsgDeq(&pCtx->txArqQ, &handlerId);
    }

    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Pop top element from Tx queue.
 *
 *  \param  pCtx      Connection context.
 */
/*************************************************************************************************/
void lctrTxQueuePopCleanup(lctrConnCtx_t *pCtx)
{
  if (pLctrTxCompBuf)
  {
    if (lctrTxCompBufHandlerId != LCTR_CTRL_DATA_HANDLE)
    {
      lctrTxBufDesc_t *pDesc = (lctrTxBufDesc_t *)pLctrTxCompBuf;

#ifndef LCTR_CONN_NO_TIFS_REASSEMBLY
      WsfMsgFree(pDesc->pAclPdu - HCI_ACL_HDR_LEN);
#endif
      lctrFreeConnTxBufDesc(pDesc);
      lctrDataTxIncAvailBuf();
      pCtx->numTxComp++;
    }
    else
    {
      WsfMsgFree(pLctrTxCompBuf);
    }

    pLctrTxCompBuf = NULL;
    WsfSetEvent(lmgrPersistCb.handlerId, (1 << LCTR_EVENT_TX_COMPLETE));
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Clear Tx queue.
 *
 *  \param  pCtx        Connection context.
 *
 *  \return Number of freed buffers.
 */
/*************************************************************************************************/
uint8_t lctrTxQueueClear(lctrConnCtx_t *pCtx)
{
  uint8_t *pBuf;
  uint8_t numTxBufs = 0;
  wsfHandlerId_t handlerId;

  while ((pBuf = WsfMsgDeq(&pCtx->txArqQ, &handlerId)) != NULL)
  {
    if (handlerId != LCTR_CTRL_DATA_HANDLE)
    {
      lctrTxBufDesc_t *pDesc = (lctrTxBufDesc_t *)pBuf;

#ifndef LCTR_CONN_NO_TIFS_REASSEMBLY
      WsfMsgFree(pDesc->pAclPdu - HCI_ACL_HDR_LEN);
#endif
      lctrFreeConnTxBufDesc(pDesc);

      lctrDataTxIncAvailBuf();
      numTxBufs++;
    }
    else
    {
      WsfMsgFree(pBuf);
    }
  }

  return numTxBufs;
}

/*************************************************************************************************/
/*!
 *  \brief  Allocate a receive data PDU buffer.
 *
 *  \param  maxRxLen    Maximum receive length.
 *
 *  \return Pointer to the start of the PDU data buffer.
 */
/*************************************************************************************************/
uint8_t *lctrRxPduAlloc(uint16_t maxRxLen)
{
  /* LCTR_DATA_PDU_MAX_LEN includes LL_DATA_MIC_LEN if required. */
  const uint16_t allocLen = WSF_MAX(BB_FIXED_DATA_PKT_LEN, LCTR_DATA_PDU_LEN(maxRxLen)) + LCTR_DATA_PDU_START_OFFSET;

  uint8_t *pBuf;

  /* Include ACL header headroom. */
  if ((pBuf = WsfMsgAlloc(HCI_ACL_HDR_LEN + allocLen)) != NULL)
  {
    /* Return start of data PDU. */
    pBuf += LCTR_DATA_PDU_START_OFFSET;
  }

  return pBuf;
}

/*************************************************************************************************/
/*!
 *  \brief  Free a receive data PDU buffer.
 *
 *  \param  pBuf        PDU data buffer to free.
 */
/*************************************************************************************************/
void lctrRxPduFree(uint8_t *pBuf)
{
  /* Recover headroom, assume buffer starts at the beginning of the data PDU. */
  pBuf -= LCTR_DATA_PDU_START_OFFSET;

  WsfMsgFree(pBuf);
}

/*************************************************************************************************/
/*!
 *  \brief  Enqueue a receive data PDU buffer.
 *
 *  \param  pBuf            PDU data buffer to queue.
 *  \param  eventCounter    Event counter.
 *  \param  connHandle      Connection handle.
 */
/*************************************************************************************************/
void lctrRxEnq(uint8_t *pBuf, uint16_t eventCounter, uint16_t connHandle)
{
  /* Stamp packet with event counter. */
  pBuf -= LCTR_DATA_PDU_START_OFFSET;
  UINT16_TO_BUF(pBuf, eventCounter);

  /* Queue LE Data PDU. */
  WsfMsgEnq(&lmgrConnCb.rxDataQ, connHandle, pBuf);
  WsfSetEvent(lmgrPersistCb.handlerId, (1 << LCTR_EVENT_RX_PENDING));
}

/*************************************************************************************************/
/*!
 *  \brief  Dequeue a receive data PDU buffer.
 *
 *  \param  pConnHandle     Storage for connection handle.
 *
 *  \return Pointer to the start of the PDU data buffer.
 */
/*************************************************************************************************/
uint8_t *lctrRxDeq(uint16_t *pConnHandle)
{
  uint8_t *pBuf;
  wsfHandlerId_t handlerId;

  if ((pBuf = WsfMsgDeq(&lmgrConnCb.rxDataQ, &handlerId)) != NULL)
  {
    /* Return start of data PDU. */
    pBuf += LCTR_DATA_PDU_START_OFFSET;

    *pConnHandle = (uint16_t)handlerId;
  }
  return pBuf;
}

/*************************************************************************************************/
/*!
 *  \brief  Enqueue a receive data PDU buffer for a connection.
 *
 *  \param  pCtx            Connection context.
 *  \param  pBuf            PDU data buffer to queue.
 */
/*************************************************************************************************/
void lctrRxConnEnq(lctrConnCtx_t *pCtx, uint8_t *pBuf)
{
  WsfMsgEnq(&pCtx->rxDataQ, LCTR_GET_CONN_HANDLE(pCtx), pBuf - LCTR_DATA_PDU_START_OFFSET);
  pCtx->numRxPend++;
}

/*************************************************************************************************/
/*!
 *  \brief  Dequeue a receive data PDU buffer for a connection as a ACL message.
 *
 *  \param  pCtx            Connection context.
 *
 *  \return Pointer to the start of the ACL message.
 *
 *  \note   Returned pointer will be freed with WsfMsgFree().
 */
/*************************************************************************************************/
uint8_t *lctrRxConnDeqAcl(lctrConnCtx_t *pCtx)
{
  uint8_t *pAclBuf;
  wsfHandlerId_t connHandle;

  if ((pAclBuf = WsfMsgDeq(&pCtx->rxDataQ, &connHandle)) != NULL)
  {
    /* lctrRxPduFree(pRxBuf); */        /* Freed in LHCI, etc. */
    /* lctrDataRxIncAvailBuf(); */      /* in LctrRxComplete(). */

    uint8_t * const pDataBuf = pAclBuf + LCTR_DATA_PDU_START_OFFSET;
    lctrAclHdr_t aclHdr;
    uint8_t llid = pDataBuf[LCTR_DATA_PDU_FC_OFFSET] & LL_DATA_HDR_LLID_MSK;

    /*** Assemble ACL packet. ***/

    aclHdr.connHandle = connHandle;
    aclHdr.len = pDataBuf[LCTR_DATA_PDU_LEN_OFFSET];

    switch (llid)
    {
      case LL_LLID_START_PDU:
        aclHdr.pktBound = LCTR_PB_START_AUTO_FLUSH;
        break;
      case LL_LLID_CONT_PDU:
        aclHdr.pktBound = LCTR_PB_CONT_FRAG;
        break;
      case LL_LLID_VS_PDU:
      default:
        aclHdr.pktBound = LCTR_PB_VS_DATA;
        break;
    }

    lctrPackAclHdr(pAclBuf, &aclHdr);
  }

  return pAclBuf;
}

/*************************************************************************************************/
/*!
 *  \brief  Clear receive queue for a connection.
 *
 *  \param  pCtx            Connection context.
 *
 *  \return Number of freed buffers.
 */
/*************************************************************************************************/
uint8_t lctrRxConnClear(lctrConnCtx_t *pCtx)
{
  uint8_t *pBuf;
  uint8_t numRxBufs = 0;
  wsfHandlerId_t handlerId;

  /* Flush remaining receive packets. */
  while ((pBuf = WsfMsgDeq(&pCtx->rxDataQ, &handlerId)) != NULL)
  {
    WsfMsgFree(pBuf);
    numRxBufs++;
  }

  return numRxBufs;
}
