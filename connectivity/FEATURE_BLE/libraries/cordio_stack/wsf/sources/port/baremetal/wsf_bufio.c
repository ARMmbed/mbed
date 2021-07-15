/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  WSF buffer IO for UART driver.
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

#include <string.h>

#include "wsf_types.h"
#include "wsf_bufio.h"

#include "wsf_cs.h"
#include "wsf_trace.h"
#include "wsf_os.h"

#include "pal_uart.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief    TX structure */
typedef struct
{
  uint8_t      *pBuf;                              /*!< UART TX buffer pointer */
  uint16_t     size;                               /*!< UART TX buffer size */
  uint16_t     in;                                 /*!< UART TX buffer in index */
  uint16_t     out;                                /*!< UART TX buffer out index */
  uint16_t     crt;                                /*!< UART TX current number of bytes sent */
} WsfBufIoUartTx_t;

/*! \brief    RX structure */
typedef struct
{
  WsfBufIoUartRxCback_t   cback;                   /*!< UART RX callback. */
  uint8_t                 buf[1];                  /*!< UART RX buffer */
} WsfBufIoUartRx_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! \brief    Control block. */
static struct
{
  WsfBufIoUartTx_t         tx;             /*!< Platform UART TX structure */
  WsfBufIoUartRx_t         rx;             /*!< Platform UART RX structure */
  bool_t                   initialized;    /*!< UART RX is initialized */
} WsfBufIoCb = {0};

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Start transmit.
 *
 *  \param  len Length to transmit.
 */
/*************************************************************************************************/
static void wsfBufIoUartTxStart(uint16_t len)
{
  /* If TX buffer overrun happens, send bytes only till the end of the buffer. */
  if (WsfBufIoCb.tx.out + len > WsfBufIoCb.tx.size)
  {
    len = WsfBufIoCb.tx.size - WsfBufIoCb.tx.out;
  }

  /* Check if TX length exceeds 1 octet size from UART driver. */
  if (len <= 0xFF)
  {
    WsfBufIoCb.tx.crt = len;
    PalUartWriteData(PAL_UART_ID_TERMINAL, WsfBufIoCb.tx.pBuf + WsfBufIoCb.tx.out, (uint16_t)len);
  }
  else
  {
	  WsfBufIoCb.tx.crt = 0xFF;
    PalUartWriteData(PAL_UART_ID_TERMINAL, WsfBufIoCb.tx.pBuf + WsfBufIoCb.tx.out, 0xFF);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Get number of characters in Tx buffer.
 *
 *  \return Number of characters in Tx buffer.
 */
/*************************************************************************************************/
static uint16_t wsfBufIoUartTxBufCount(void)
{
  uint16_t count;

  if (WsfBufIoCb.tx.in >= WsfBufIoCb.tx.out)
  {
    count = WsfBufIoCb.tx.in - WsfBufIoCb.tx.out;
  }
  else
  {
    count = WsfBufIoCb.tx.size - WsfBufIoCb.tx.out + WsfBufIoCb.tx.in;
  }

  return count;
}

/*************************************************************************************************/
/*!
 *  \brief  Rx handler.
 */
/*************************************************************************************************/
static void wsfBufIoUartRxHandler(void)
{
  if (WsfBufIoCb.rx.cback)
  {
    WsfBufIoCb.rx.cback(WsfBufIoCb.rx.buf[0]);
  }
  /* Read next byte. */
  PalUartReadData(PAL_UART_ID_TERMINAL, WsfBufIoCb.rx.buf, 1);
}

/*************************************************************************************************/
/*!
 *  \brief  Tx handler.
 */
/*************************************************************************************************/
static void wsfBufIoUartTxHandler(void)
{
  uint16_t count;

  WSF_CS_INIT(cs);
  WSF_CS_ENTER(cs);

  if (WsfBufIoCb.tx.out + WsfBufIoCb.tx.crt >= WsfBufIoCb.tx.size)
  {
    WsfBufIoCb.tx.out = WsfBufIoCb.tx.crt - (WsfBufIoCb.tx.size - WsfBufIoCb.tx.out);
  }
  else
  {
    WsfBufIoCb.tx.out += WsfBufIoCb.tx.crt;
  }

  WsfBufIoCb.tx.crt = 0;

  count = wsfBufIoUartTxBufCount();

  if (count != 0)
  {
    wsfBufIoUartTxStart(count);
  }

  WSF_CS_EXIT(cs);
}

/**************************************************************************************************
  Global Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Initialize the UART.
 *
 *  \param  pBuf    Tx Buffer pointer.
 *  \param  size    Length of buffer.
 *
 *  \return memory used.
 */
/*************************************************************************************************/
uint32_t WsfBufIoUartInit(void *pBuf, uint32_t size)
{
  /* Skip initialization if it is already done. */
  if (WsfBufIoCb.initialized)
  {
    return 0;
  }

  WsfBufIoCb.tx.pBuf = (uint8_t *)pBuf;
  WsfBufIoCb.tx.size = size;

  PalUartConfig_t cfg;
  cfg.baud   = 115200;
  cfg.hwFlow = FALSE;
  cfg.rdCback = wsfBufIoUartRxHandler;
  cfg.wrCback = wsfBufIoUartTxHandler;

  PalUartInit(PAL_UART_ID_TERMINAL, &cfg);

  /* Start UART RX. */
  PalUartReadData(PAL_UART_ID_TERMINAL, WsfBufIoCb.rx.buf, 1);

  WsfBufIoCb.initialized = TRUE;
  return WsfBufIoCb.tx.size;
}

/*************************************************************************************************/
/*!
 *  \brief  Register the UART RX callback.
 *
 *  \param  rxCback  Callback function for UART RX.
 */
/*************************************************************************************************/
void WsfBufIoUartRegister(WsfBufIoUartRxCback_t rxCback)
{
  if (rxCback != NULL)
  {
    WsfBufIoCb.rx.cback = rxCback;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Transmit buffer on UART.
 *
 *  \param  pBuf    Buffer to transmit.
 *  \param  len     Length of buffer in octets.
 *
 *  \return if write successfully.
 */
/*************************************************************************************************/
bool_t WsfBufIoWrite(const uint8_t *pBuf, uint32_t len)
{
  uint16_t end;
  uint32_t retValue = TRUE;

  WSF_CS_INIT(cs);

  WSF_CS_ENTER(cs);

  /* Check if there is enough space in TX buffer */
  if ((WsfBufIoCb.tx.size - wsfBufIoUartTxBufCount() >= (uint16_t)len) &&
      (WsfBufIoCb.tx.size > WsfBufIoCb.tx.crt))
  {
    /* Compute the space available till the end of the buffer. */
    end = WsfBufIoCb.tx.size - WsfBufIoCb.tx.in;

    /* Check if overrun should happen. */
    if (end < len)
    {
      /* Copy the bytes till the end of the buffer. */
      memcpy(WsfBufIoCb.tx.pBuf + WsfBufIoCb.tx.in, pBuf, end);
      /* Copy the remaining bytes. */
      memcpy(WsfBufIoCb.tx.pBuf, pBuf + end, len - end);
      /* Increment input count */
      WsfBufIoCb.tx.in = len - end;
    }
    else
    {
      /* Enough space till the end of the buffer. Just copy. */
      memcpy(WsfBufIoCb.tx.pBuf + WsfBufIoCb.tx.in, pBuf, len);
      WsfBufIoCb.tx.in += len;
    }

    if (WsfBufIoCb.tx.in == WsfBufIoCb.tx.size)
    {
      WsfBufIoCb.tx.in = 0;
    }

    /* Check if UART TX is idle. */
    if (PalUartGetState(PAL_UART_ID_TERMINAL) == PAL_UART_STATE_READY)
    {
      /* Start TX */
      wsfBufIoUartTxStart(len);
    }
  }
  else
  {
    retValue = FALSE;
  }

  WSF_CS_EXIT(cs);

  return retValue;
}
