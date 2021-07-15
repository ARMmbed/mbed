/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Controller HCI transport module implementation file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#include "chci_tr.h"
#include "chci_api.h"
#include "hci_defs.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_math.h"
#include "wsf_os.h"
#include "util/bstream.h"
#include <string.h>

#if (CHCI_TR_UART == 1)
#include "pal_uart.h"
#include "pal_sys.h"
#endif

#if (CHCI_TR_CUSTOM == 1)
/* Custom transport */
extern void CustomChciTrInit(uint16_t maxAclLen, uint16_t maxIsoSduLen);
extern void CustomChciTrWrite(uint8_t prot, uint8_t type, uint16_t len, uint8_t *pData);
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Maximum read buffer length allowed by UART DMA. */
#define LHCI_MAX_RD_BUF_LEN     255

/*! \brief      Maximum write buffer length allowed by UART DMA. */
#define LHCI_MAX_WR_BUF_LEN     255

/*! \brief      Maximum read header length. */
#define LHCI_MAX_RD_HDR_LEN     (1 + 4)     /* type + max header length */

/*! \brief      Maximum write buffer length allowed by UART DMA. */
#define LHCI_MAX_DATA_LEN       4096

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Transport control block. */
typedef struct
{
  wsfHandlerId_t              handlerId;            /*!< Handler ID. */
  uint8_t                     *pDataPending;        /*!< Data packet in progress. */
  uint8_t                     typePending;          /*!< Data type in progress. */
  uint8_t                     protPending;          /*!< Protocol in progress. */
  uint8_t                     nextAvailMask;        /*!< Next available mask. */

  uint16_t                    maxAclLen;            /*!< Maximum ACL data length. */
  uint16_t                    maxIsoSduLen;         /*!< Maximum ISO data length. */

  uint32_t                    cmdCount;             /*!< Counter of commands */
  uint32_t                    evtCount;             /*!< Counter of events */

  ChciTrSendHwErrorCback_t    sendHwErrorCback;     /*!< Send HW error callback. */

  struct
  {
    ChciTrRecvCback_t         recvCback;            /*!< Message received callback. */
    ChciTrSendCompleteCback_t sendCompleteCback;    /*!< Message send complete callback. */
    ChciTrServiceCback_t      serviceCback;         /*!< Service callback. */
  } protCbacks[CHCI_TR_PROT_NUM];                   /*!< Callback array indexed by protocol ID. */

  /* Read buffer state. */
  uint8_t                     rxPktState;           /*!< Receive state. */
  uint8_t                     rdHdr[LHCI_MAX_RD_HDR_LEN];   /*!< Read header buffer. */
  uint16_t                    rdBufOffs;            /*!< Write data buffer offset. */
  uint16_t                    rdBufLen;             /*!< Write data buffer length. */
  uint8_t                     *pRdBuf;              /*!< Read data buffer. */

  /* Write buffer state. */
  uint16_t                    wrBufOffs;            /*!< Write data buffer offset. */
  uint16_t                    wrBufLen;             /*!< Write data buffer length. */
  uint8_t                     *pWrBuf;              /*!< Write data buffer. */
} chciTrCtrlBlk_t;

/*! \brief      Send handler. */
typedef void (*chciTrSendHandler_t)(uint8_t *pBuf);

/*! \brief      Transport events. */
enum
{
  CHCI_TR_EVT_SEND_CMPL = (1 << 0)                  /*!< Send complete. */
};

/*! \brief      Receive state. */
enum
{
  CHCI_RX_STATE_IDLE,               /*!< Idle state. */
  CHCI_RX_STATE_TYPE,               /*!< Type parse state. */
  CHCI_RX_STATE_HEADER,             /*!< Header parse state. */
  CHCI_RX_STATE_PAYLOAD,            /*!< Payload parse state. */
  CHCI_RX_STATE_COMPLETE            /*!< Complete state. */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Current event data in progress. */
chciTrCtrlBlk_t chciTrCb;

/*************************************************************************************************/
/*!
 *  \brief  Increment the command and event counters.
 *
 *  \param  type          Type of message.
 */
/*************************************************************************************************/
static void chciTrIncrementCounters(uint8_t type)
{
  if (type == CHCI_TR_TYPE_CMD)
  {
    chciTrCb.cmdCount++;
  }
  else if (type == CHCI_TR_TYPE_EVT)
  {
    chciTrCb.evtCount++;
  }
}

#if (CHCI_TR_UART == 1)

/*************************************************************************************************/
/*!
 *  \brief  Signal a hardware error.
 *
 *  \param  code    Error code.
 */
/*************************************************************************************************/
static void chciTrHwError(uint8_t code)
{
  if (chciTrCb.sendHwErrorCback != NULL)
  {
    chciTrCb.sendHwErrorCback(code);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Setup a read data buffer.
 *
 *  \param  len       Number of bytes to write.
 *  \param  pData     Byte array to write.
 */
/*************************************************************************************************/
static void chciTrRead(uint16_t len, uint8_t *pData)
{
  WSF_ASSERT(len <= LHCI_MAX_RD_BUF_LEN);
#if (CHCI_TR_UART == 1)
  PalUartReadData(PAL_UART_ID_CHCI, pData, len);
#endif
}

/*************************************************************************************************/
/*!
 *  \brief  Receive packet state machine.
 */
/*************************************************************************************************/
static void chciRxPacketSM(void)
{
  /* --- Type State --- */
  if (chciTrCb.rxPktState == CHCI_RX_STATE_TYPE)
  {
    chciTrCb.rxPktState = CHCI_RX_STATE_HEADER;
    /* Determine header length based on packet type. */
    switch (chciTrCb.rdHdr[0])
    {
      case HCI_CMD_TYPE:
        chciTrRead(HCI_CMD_HDR_LEN, &chciTrCb.rdHdr[1]);
        break;
      case HCI_ACL_TYPE:
        chciTrRead(HCI_ACL_HDR_LEN, &chciTrCb.rdHdr[1]);
        break;
      case HCI_ISO_TYPE:
        chciTrRead(HCI_ISO_HDR_LEN, &chciTrCb.rdHdr[1]);
        break;
      case CHCI_15P4_CMD_TYPE:
      case CHCI_15P4_DATA_TYPE:
        chciTrRead(CHCI_15P4_HDR_LEN, &chciTrCb.rdHdr[1]);
        break;
      default:
        /* Invalid byte received. */
        chciTrHwError(CHCI_TR_CODE_INVALID_DATA);
        chciTrCb.rxPktState = CHCI_RX_STATE_IDLE;
    }
  }

  /* --- Header State --- */
  else if (chciTrCb.rxPktState == CHCI_RX_STATE_HEADER)
  {
    uint8_t hdrLen = 0;
    uint16_t dataLen = 0;
    uint16_t allocLen = 0;

    /* Extract data length from header. */
    switch (chciTrCb.rdHdr[0])
    {
      case HCI_CMD_TYPE:
        hdrLen = HCI_CMD_HDR_LEN;
        dataLen = (uint16_t)chciTrCb.rdHdr[3];
        allocLen = dataLen;
        break;
      case HCI_ACL_TYPE:
        hdrLen = HCI_ACL_HDR_LEN;
        BYTES_TO_UINT16(dataLen, &chciTrCb.rdHdr[3]);
        if (dataLen > chciTrCb.maxAclLen)
        {
          chciTrHwError(CHCI_TR_CODE_INVALID_DATA_LEN);
          chciTrCb.rxPktState = CHCI_RX_STATE_IDLE;
          return;
        }
        /* Use data buffers located in the large pool. */
        allocLen = chciTrCb.maxAclLen + CHCI_BUF_TAILROOM;
        break;
      case HCI_ISO_TYPE:
        hdrLen = HCI_ISO_HDR_LEN;
        BYTES_TO_UINT16(dataLen, &chciTrCb.rdHdr[3]);
        if (dataLen > (chciTrCb.maxIsoSduLen + HCI_ISO_DL_MAX_LEN))
        {
          chciTrHwError(CHCI_TR_CODE_INVALID_DATA_LEN);
          chciTrCb.rxPktState = CHCI_RX_STATE_IDLE;
          return;
        }
        /* Use data buffers located in the large pool. */
        allocLen = HCI_ISO_DL_MAX_LEN + chciTrCb.maxIsoSduLen + CHCI_BUF_TAILROOM;
        break;
      case CHCI_15P4_CMD_TYPE:
      case CHCI_15P4_DATA_TYPE:
        hdrLen = CHCI_15P4_HDR_LEN;
        BYTES_TO_UINT16(dataLen, &chciTrCb.rdHdr[2]);
        allocLen = dataLen;
        break;
      default:
        /* already validated in CHCI_RX_STATE_TYPE */
        break;
    }

    if (dataLen > LHCI_MAX_DATA_LEN)
    {
      /* Invalid byte received. */
      chciTrHwError(CHCI_TR_CODE_INVALID_DATA);
      chciTrCb.rxPktState = CHCI_RX_STATE_IDLE;
    }
    else if ((chciTrCb.pRdBuf = (uint8_t *)WsfMsgAlloc(hdrLen + allocLen)) != NULL)
    {
      if (dataLen > 0)
      {
        /* Bound read size to UART DMA maximum length. */
        uint16_t blkLen = WSF_MIN(dataLen, LHCI_MAX_RD_BUF_LEN);

        /* Read additional payload data. */
        chciTrRead(blkLen, chciTrCb.pRdBuf + hdrLen);
        chciTrCb.rdBufLen = hdrLen + dataLen;
        chciTrCb.rdBufOffs = hdrLen + blkLen;
        chciTrCb.rxPktState = CHCI_RX_STATE_PAYLOAD;
      }
      else
      {
        /* No more data. */
        chciTrCb.rxPktState = CHCI_RX_STATE_COMPLETE;
      }
      /* Remove packet type encapsulation. */
      memcpy(chciTrCb.pRdBuf, &chciTrCb.rdHdr[1], hdrLen);
    }
    else
    {
      chciTrHwError(CHCI_TR_CODE_OUT_OF_MEMORY);
      chciTrCb.rxPktState = CHCI_RX_STATE_IDLE;
    }
  }

  /* --- Data State --- */
  else if (chciTrCb.rxPktState == CHCI_RX_STATE_PAYLOAD)
  {
    uint16_t blkLen = WSF_MIN((chciTrCb.rdBufLen - chciTrCb.rdBufOffs), LHCI_MAX_RD_BUF_LEN);
    if (blkLen > 0)
    {
      /* Bound read size to UART DMA maximum length. */
      blkLen = WSF_MIN(blkLen, LHCI_MAX_RD_BUF_LEN);

      /* Read next block. */
      chciTrRead(blkLen, chciTrCb.pRdBuf + chciTrCb.rdBufOffs);
      chciTrCb.rdBufOffs += blkLen;
    }
    else
    {
      chciTrCb.rxPktState = CHCI_RX_STATE_COMPLETE;
    }
  }

  /* --- Complete State --- */
  /* N.B. There is no else-if construct by design. */
  if (chciTrCb.rxPktState == CHCI_RX_STATE_COMPLETE)
  {
    WSF_ASSERT(chciTrCb.pRdBuf);

    switch (chciTrCb.rdHdr[0])
    {
      case HCI_ISO_TYPE:
        chciTrRecv(CHCI_TR_PROT_BLE, CHCI_TR_TYPE_ISO, chciTrCb.pRdBuf);
        break;
      case HCI_ACL_TYPE:
        chciTrRecv(CHCI_TR_PROT_BLE, CHCI_TR_TYPE_ACL, chciTrCb.pRdBuf);
        break;
      case HCI_CMD_TYPE:
        chciTrRecv(CHCI_TR_PROT_BLE, CHCI_TR_TYPE_CMD, chciTrCb.pRdBuf);
        break;
      case CHCI_15P4_CMD_TYPE:
        chciTrRecv(CHCI_TR_PROT_15P4, CHCI_TR_TYPE_CMD, chciTrCb.pRdBuf);
        break;
      case CHCI_15P4_DATA_TYPE:
        chciTrRecv(CHCI_TR_PROT_15P4, CHCI_TR_TYPE_ACL, chciTrCb.pRdBuf);
        break;
      default:
        break;
    }

    chciTrCb.pRdBuf = NULL;

    chciTrCb.rxPktState = CHCI_RX_STATE_IDLE;
  }

  /* --- Idle State --- */
  /* N.B. There is no else-if construct by design. */
  if (chciTrCb.rxPktState == CHCI_RX_STATE_IDLE)
  {
    /* Read packet type. */
    chciTrRead(1, &chciTrCb.rdHdr[0]);
    chciTrCb.rxPktState = CHCI_RX_STATE_TYPE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Tx complete callback.
 */
/*************************************************************************************************/
static void chciTxComplete(void)
{
  uint16_t blkLen = WSF_MIN((chciTrCb.wrBufLen - chciTrCb.wrBufOffs), LHCI_MAX_WR_BUF_LEN);

  if (blkLen)
  {
  #if (CHCI_TR_UART == 1)
    PalUartWriteData(PAL_UART_ID_CHCI, chciTrCb.pWrBuf + chciTrCb.wrBufOffs, blkLen);
  #endif
    chciTrCb.wrBufOffs += blkLen;
  }
  else
  {
    chciTrSendComplete();
  }
}

#endif /* (CHCI_TR_UART == 1) */

/*************************************************************************************************/
/*!
 *  \brief  Write data the driver.
 *
 *  \param  prot      Protocol.
 *  \param  type      Packet type.
 *  \param  len       Number of bytes to write.
 *  \param  pData     Byte array to write.
 *
 *  \note   The type parameter allows the driver layer to prepend the data with a header on the
 *          same write transaction.
 */
/*************************************************************************************************/
static void chciTrWrite(uint8_t prot, uint8_t type, uint16_t len, uint8_t *pData)
{
#if (CHCI_TR_UART == 1)
  /* Initialize write buffer state. */
  chciTrCb.wrBufLen  = len + 1;      /* use wsfMsg_t headroom */
  if (prot == CHCI_TR_PROT_15P4)
  {
    *(pData - 1) = (type == CHCI_TR_TYPE_EVT) ? CHCI_15P4_CMD_TYPE : CHCI_15P4_DATA_TYPE;
  }
  else
  {
    if (type == CHCI_TR_TYPE_EVT)
    {
      *(pData - 1) = HCI_EVT_TYPE;
    }
    else if (type == CHCI_TR_TYPE_ACL)
    {
      *(pData - 1) = HCI_ACL_TYPE;
    }
    else
    {
      /* CHCI_TR_TYPE_ISO */
      *(pData - 1) = HCI_ISO_TYPE;
    }
  }
  chciTrCb.pWrBuf    = pData - 1;
  chciTrCb.wrBufOffs = WSF_MIN(chciTrCb.wrBufLen, LHCI_MAX_WR_BUF_LEN);

  /* Initiate Tx operation. */
  PalUartWriteData(PAL_UART_ID_CHCI, chciTrCb.pWrBuf, chciTrCb.wrBufOffs);
#endif

#if (CHCI_TR_CUSTOM == 1)
  CustomChciTrWrite(prot, type, len, pData);
  chciTrSendComplete();
#endif
}

/*************************************************************************************************/
/*!
 *  \brief  Signal the completion of a message write.
 *
 *  This routine is used for asynchronous write operations. When the driver has completed the
 *  use of the write buffer, this routine is called to free the buffer and release flow control.
 */
/*************************************************************************************************/
void chciTrSendComplete(void)
{
  uint8_t *pBuf = chciTrCb.pDataPending;
  uint8_t type  = chciTrCb.typePending;
  uint8_t prot  = chciTrCb.protPending;

  WSF_ASSERT(chciTrCb.protCbacks[prot].sendCompleteCback != NULL);

  /* Protocol will free buffer. */
  chciTrCb.pDataPending = NULL;
  chciTrCb.protCbacks[prot].sendCompleteCback(type, pBuf);
  WsfSetEvent(chciTrCb.handlerId, CHCI_TR_EVT_SEND_CMPL);
}

/*************************************************************************************************/
/*!
 *  \brief  Get the HCI command and event counts.
 *
 *  \param  pCmdCount       Pointer to uint32_t to hold command count.
 *  \param  pEvtCount       Pointer to uint32_t to hold event count.
 */
/*************************************************************************************************/
void ChciTrGetCmdEvtCounts(uint32_t *pCmdCount, uint32_t *pEvtCount)
{
  *pCmdCount = chciTrCb.cmdCount;
  *pEvtCount = chciTrCb.evtCount;
}

/*************************************************************************************************/
/*!
 *  \brief  Reset the HCI command and event counts to zero.
 */
/*************************************************************************************************/
void ChciTrResetCmdEvtCounts(void)
{
  chciTrCb.cmdCount = 0;
  chciTrCb.evtCount = 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the transport handler.
 *
 *  \param  handlerId       Handler ID.
 *  \param  maxAclLen       Maximum ACL data length.
 *  \param  maxIsoSduLen    Maximum ISO data length.
 */
/*************************************************************************************************/
void ChciTrHandlerInit(wsfHandlerId_t handlerId, uint16_t maxAclLen, uint16_t maxIsoSduLen)
{
  memset(&chciTrCb, 0, sizeof(chciTrCb));
  chciTrCb.handlerId = handlerId;
  chciTrCb.maxAclLen = maxAclLen;
  chciTrCb.maxIsoSduLen = maxIsoSduLen;

#if (CHCI_TR_UART == 1)
  PalUartConfig_t cfg;
  cfg.baud   = UART_BAUD;
  cfg.hwFlow = UART_HWFC;
  cfg.rdCback = chciRxPacketSM;
  cfg.wrCback = chciTxComplete;
  PalUartInit(PAL_UART_ID_CHCI, &cfg);

  /* Start receiver. */
  chciRxPacketSM();
#elif (CHCI_TR_CUSTOM == 1)
  CustomChciTrInit(maxAclLen, maxIsoSduLen);
#else
  (void)chciRxPacketSM;
  (void)chciTxComplete;
#endif
}

/*************************************************************************************************/
/*!
 *  \brief      Controller HCI transport message dispatch handler.
 *
 *  \param      event       WSF event.
 *  \param      pMsg        WSF message.
 */
/*************************************************************************************************/
void ChciTrHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
  if (chciTrCb.pDataPending == NULL)
  {
    uint8_t protCount = 0;
    uint8_t prot      = chciTrCb.protPending;   /* last protocol serviced */

    do
    {
      uint8_t  type = 0;
      uint8_t  *pData = NULL;
      uint16_t len = 0;

      prot = (prot + 1 >= CHCI_TR_PROT_NUM) ? 0 : (prot + 1);

      if ((chciTrCb.protCbacks[prot].serviceCback != NULL) &&
          chciTrCb.protCbacks[prot].serviceCback(&type, &len, &pData))
      {
        chciTrCb.pDataPending = pData;
        chciTrCb.typePending  = type;
        chciTrCb.protPending  = prot;

        chciTrIncrementCounters(type);
        chciTrWrite(prot, type, len, pData);
        break;
      }
    } while (++protCount < CHCI_TR_PROT_NUM);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set callbacks for a protocol.
 *
 *  \param  prot                Protocol.
 *  \param  recvCback           Message received callback.
 *  \param  sendCompleteCback   Message send complete callback.
 *  \param  serviceCback        Service callback.
 */
/*************************************************************************************************/
void ChciTrSetCbacks(uint8_t prot, ChciTrRecvCback_t recvCback, ChciTrSendCompleteCback_t sendCompleteCback,
    ChciTrServiceCback_t serviceCback)
{
  if (prot < CHCI_TR_PROT_NUM)
  {
    chciTrCb.protCbacks[prot].recvCback         = recvCback;
    chciTrCb.protCbacks[prot].sendCompleteCback = sendCompleteCback;
    chciTrCb.protCbacks[prot].serviceCback      = serviceCback;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set send hardware error callback.
 *
 *  \param  sendHwErrorCback    Send hardware error callback.
 */
/*************************************************************************************************/
void ChciTrSetSendHwErrorCback(ChciTrSendHwErrorCback_t sendHwErrorCback)
{
  chciTrCb.sendHwErrorCback = sendHwErrorCback;
}

/*************************************************************************************************/
/*!
 *  \brief  Flag protocol for needing service.
 *
 *  \param  prot                Protocol.
 */
/*************************************************************************************************/
void ChciTrNeedsService(uint8_t prot)
{
  if (chciTrCb.pDataPending == NULL)
  {
    ChciTrHandler(CHCI_TR_EVT_SEND_CMPL, NULL);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Signal the completion of a message receive.
 *
 *  \param  prot    Protocol.
 *  \param  type    Message type.
 *  \param  pBuf    Message.
 */
/*************************************************************************************************/
void chciTrRecv(uint8_t prot, uint8_t type, uint8_t *pBuf)
{
  chciTrIncrementCounters(type);

  if (prot < CHCI_TR_PROT_NUM)
  {
    if (chciTrCb.protCbacks[prot].recvCback != NULL)
    {
      /* Protocol will free buffer. */
      chciTrCb.protCbacks[prot].recvCback(type, pBuf);
      return;
    }
  }

  /* Free buffer that cannot be handled. */
  WsfMsgFree(pBuf);
}
