/*************************************************************************************************/
/*!
 *  \file   hci_tr.c
 *
 *  \brief  HCI transport implementation for Nordic.
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

#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "wsf_assert.h"
#include "util/bstream.h"
#include "hci_api.h"
#include "hci_core.h"
#include "hci_defs.h"
#include "hci_drv.h"
#include <stdio.h>
#include <string.h>

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* PORTING: EXACTLE removed as replaced by zero copy hci driver in mbedos */
uint16_t hci_mbed_os_drv_write(uint8_t type, uint16_t len, uint8_t *pData);

/**************************************************************************************************
  Data Types
**************************************************************************************************/

typedef enum
{
  HCI_RX_STATE_IDLE,
  HCI_RX_STATE_HEADER,
  HCI_RX_STATE_DATA,
  HCI_RX_STATE_COMPLETE
} hciRxState_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     hciTrSendAclData
 *
 *  \brief  Send a complete HCI ACL packet to the transport.
 *
 *  \param  pContext Connection context.
 *  \param  pData    WSF msg buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrSendAclData(void *pContext, uint8_t *pData)
{
  /* PORTING: sending and fragmenting done by mbed-os */
  uint16_t len;

  /* transmit ACL header and data */
  BYTES_TO_UINT16(len, (pData + 2))
  len += HCI_ACL_HDR_LEN;

  if (hci_mbed_os_drv_write(HCI_ACL_TYPE, len, pData) == len)
  {
#if CORDIO_ZERO_COPY_HCI
    /* pData is not freed as the hci_mbed_os_drv_write took ownership of the WSF buffer */
#else
    /* free buffer */
    hciCoreTxAclComplete(pContext, pData);
#endif // CORDIO_ZERO_COPY_HCI
  }
}

/*************************************************************************************************/
/*!
 *  \fn     hciTrSendCmd
 *
 *  \brief  Send a complete HCI command to the transport.
 *
 *  \param  pData    WSF msg buffer containing an HCI command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrSendCmd(uint8_t *pData)
{
  /* PORTING: sending done by mbed-os */
  uint16_t   len;

  /* get length */
  len = pData[2] + HCI_CMD_HDR_LEN;

  /* transmit ACL header and data */
  if (hci_mbed_os_drv_write(HCI_CMD_TYPE, len, pData) == len)
  {
#if CORDIO_ZERO_COPY_HCI
    /* pData is not freed as the hci_mbed_os_drv_write took ownership of the WSF buffer */
#else
    /* free command buffer */
    WsfMsgFree(pData);
#endif // CORDIO_ZERO_COPY_HCI
  }
}

/*************************************************************************************************/
/*!
 *  \fn     hciSerialRxIncoming
 *
 *  \brief  Receive function.  Gets called by external code when bytes are received.
 *
 *  \param  pBuf   Pointer to buffer of incoming bytes.
 *  \param  len    Number of bytes in incoming buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrSerialRxIncoming(uint8_t *pBuf, uint8_t len)
{
  static uint8_t    stateRx = HCI_RX_STATE_IDLE;
  static uint8_t    pktIndRx;
  static uint16_t   iRx;
  static uint8_t    hdrRx[HCI_ACL_HDR_LEN];
  static uint8_t    *pPktRx;
  static uint8_t    *pDataRx;

  uint8_t dataByte;

  /* loop until all bytes of incoming buffer are handled */
  while (len--)
  {
    /* read single byte from incoming buffer and advance to next byte */
    dataByte = *pBuf++;

    /* --- Idle State --- */
    if (stateRx == HCI_RX_STATE_IDLE)
    {
      /* save the packet type */
      pktIndRx = dataByte;
      iRx      = 0;
      stateRx  = HCI_RX_STATE_HEADER;
    }

    /* --- Header State --- */
    else if (stateRx == HCI_RX_STATE_HEADER)
    {
      uint8_t  hdrLen = 0;
      uint16_t dataLen = 0;

      /* copy current byte into the temp header buffer */
      hdrRx[iRx++] = dataByte;

      /* determine header length based on packet type */
      switch (pktIndRx)
      {
        case HCI_CMD_TYPE:
          hdrLen = HCI_CMD_HDR_LEN;
          break;
        case HCI_ACL_TYPE:
          hdrLen = HCI_ACL_HDR_LEN;
          break;
        case HCI_EVT_TYPE:
          hdrLen = HCI_EVT_HDR_LEN;
          break;
        default:
          /* invalid packet type */
          WSF_ASSERT(0);
          break;
      }

      /* see if entire header has been read */
      if (iRx == hdrLen)
      {
        /* extract data length from header */
        switch (pktIndRx)
        {
          case HCI_CMD_TYPE:
            dataLen = hdrRx[2];
            break;
          case HCI_ACL_TYPE:
            BYTES_TO_UINT16(dataLen, &hdrRx[2]);
            break;
          case HCI_EVT_TYPE:
            dataLen = hdrRx[1];
            break;
          default:
            break;
        }

        /* allocate data buffer to hold entire packet */
        if (pktIndRx == HCI_ACL_TYPE)
        {
          pPktRx = (uint8_t*)WsfMsgDataAlloc(hdrLen + dataLen, 0);
        }
        else
        {
          pPktRx = (uint8_t*)WsfMsgAlloc(hdrLen + dataLen);
        }

        if (pPktRx != NULL)
        {
          pDataRx = pPktRx;

          /* copy header into data packet (note: memcpy is not so portable) */
          {
            uint8_t  i;
            for (i = 0; i < hdrLen; i++)
            {
              *pDataRx++ = hdrRx[i];
            }
          }

          /* save number of bytes left to read */
          iRx = dataLen;
          if (iRx == 0)
          {
            stateRx = HCI_RX_STATE_COMPLETE;
          }
          else
          {
            stateRx = HCI_RX_STATE_DATA;
          }
        }
        else
        {
          WSF_ASSERT(0); /* allocate falied */
        }

      }
    }

    /* --- Data State --- */
    else if (stateRx == HCI_RX_STATE_DATA)
    {
      /* write incoming byte to allocated buffer */
      *pDataRx++ = dataByte;

      /* determine if entire packet has been read */
      iRx--;
      if (iRx == 0)
      {
        stateRx = HCI_RX_STATE_COMPLETE;
      }
    }

    /* --- Complete State --- */
    /* ( Note Well!  There is no else-if construct by design. ) */
    if (stateRx == HCI_RX_STATE_COMPLETE)
    {
      /* deliver data */
      if (pPktRx != NULL)
      {
        hciCoreRecv(pktIndRx, pPktRx);
      }

      /* reset state machine */
      stateRx = HCI_RX_STATE_IDLE;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     hciTrInit
 *
 *  \brief  Initialize HCI transport resources.
 *
 *  \param  port        COM port.
 *  \param  baudRate    Baud rate.
 *  \param  flowControl TRUE if flow control is enabled
 *
 *  \return TRUE if initialization succeeds, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t hciTrInit( uint8_t port, uint32_t baudRate, bool_t flowControl )
{
  /* Parameters are not used */
  (void) port;
  (void) baudRate;
  (void) flowControl;

  return (FALSE);
}

/*************************************************************************************************/
/*!
 *  \fn     hcrTrShutdown
 *
 *  \brief  Close HCI transport resources.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrShutdown(void)
{

}
