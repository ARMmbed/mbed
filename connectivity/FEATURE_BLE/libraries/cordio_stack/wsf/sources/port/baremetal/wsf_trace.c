/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Trace message implementation.
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

#include "wsf_types.h"
#include "wsf_trace.h"

#include "wsf_bufio.h"
#include "wsf_buf.h"
#include "util/print.h"
#include "pal_sys.h"
#include <stdarg.h>
#include "wsf_assert.h"
#include "wsf_cs.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Length of newline and carriage return suffix. */
#define WSF_TRACE_SUFFIX_LEN           2

#ifndef WSF_PRINTF_MAX_LEN
/*! \brief      Maximum length of a trace string. */
#define WSF_PRINTF_MAX_LEN             128
#endif

#ifndef WSF_TOKEN_RING_BUF_SIZE
/*! \brief      Size of token ring buffer (multiple of 2^N). */
#define WSF_TOKEN_RING_BUF_SIZE        64
#endif

/*! \brief      Ring buffer flow control condition detected. */
#define WSF_TOKEN_FLAG_FLOW_CTRL       (1 << 28)

/*! UART TX buffer size. */
#ifndef WSF_TRACE_BUFIO_BUFFER_SIZE
#define WSF_TRACE_BUFIO_BUFFER_SIZE    2048U
#endif

/**************************************************************************************************
  Data types
**************************************************************************************************/

/*! \brief Trace control block. */
struct
{
  WsfTraceHandler_t sendMsgCback;       /*!< Send trace message callback. */
  uint32_t numDropMsg;                  /*!< Number of dropped messages since last successful. */
  bool_t enabled;                       /*!< Tracing state. */

#if WSF_TOKEN_ENABLED == TRUE
  union
  {
    struct
    {
      uint32_t token;                   /*!< Token. */
      uint32_t param;                   /*!< Parameter. */
    } v;                                /*!< Value accessor. */
    /* Use native packing for speed. Host will resolve endian. */
    uint8_t buf[8];                     /*!< Buffer accessor. */
  } ringBuf[WSF_TOKEN_RING_BUF_SIZE];   /*!< Token message ring buffer. */
  uint32_t prodIdx;                     /*!< Ring buffer producer index. */
  uint32_t consIdx;                     /*!< Ring buffer consumer index. */
#endif
} wsfTraceCb;

#if WSF_TOKEN_ENABLED == TRUE
/*************************************************************************************************/
/*!
 *  \brief  Store tokenized message.
 *
 *  \param  tok      Message token.
 *  \param  param    Message parameter.
 */
/*************************************************************************************************/
void WsfToken(uint32_t tok, uint32_t param)
{
  static uint32_t flags = 0;

  if (!wsfTraceCb.enabled)
  {
    return;
  }

  WSF_CS_INIT(cs);
  WSF_CS_ENTER(cs);

  uint32_t prodIdx = (wsfTraceCb.prodIdx + 1) & (WSF_TOKEN_RING_BUF_SIZE - 1);

  if (prodIdx != wsfTraceCb.consIdx)
  {
    wsfTraceCb.ringBuf[wsfTraceCb.prodIdx].v.token = tok | flags;
    wsfTraceCb.ringBuf[wsfTraceCb.prodIdx].v.param = param;
    wsfTraceCb.prodIdx = prodIdx;
    flags = 0;
  }
  else
  {
    flags = WSF_TOKEN_FLAG_FLOW_CTRL;
  }

  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief  Service the trace ring buffer.
 *
 *  \return TRUE if trace messages pending, FALSE otherwise.
 *
 *  This routine is called in the main loop for a "push" type trace systems.
 */
/*************************************************************************************************/
bool_t WsfTokenService(void)
{
  WSF_ASSERT(wsfTraceCb.sendMsgCback);

  if (wsfTraceCb.consIdx != wsfTraceCb.prodIdx)
  {
    /* Advance consumer counter only if write successful. */
    if (wsfTraceCb.sendMsgCback((uint8_t *)&wsfTraceCb.ringBuf[wsfTraceCb.consIdx], sizeof(wsfTraceCb.ringBuf[0])))
    {
      wsfTraceCb.consIdx = (wsfTraceCb.consIdx + 1) & (WSF_TOKEN_RING_BUF_SIZE - 1);
    }
    return TRUE;
  }

  return FALSE;
}
#endif

#if WSF_TRACE_ENABLED == TRUE
/*************************************************************************************************/
/*!
 *  \brief  Create overflow message.
 *
 *  \param  pBuf    Buffer to store output in.
 *  \param  pStr    Format string.
 *  Addition parameters variable arguments to the format string.
 *
 *  \return Length in bytes of overflow message.
 */
/*************************************************************************************************/
uint8_t wsfTraceOverFlowMessage(char *pBuf, const char *pStr, ...)
{
  uint8_t len;
  va_list args;

  va_start(args, pStr);

  len = PrintVsn(pBuf, WSF_PRINTF_MAX_LEN, pStr, args);

  va_end(args);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Output trace message.
 *
 *  \param  pStr    Format string
 *  Addition parameters variable arguments to the format string.
 */
/*************************************************************************************************/
void WsfTrace(const char *pStr, ...)
{
  if (!wsfTraceCb.enabled)
  {
    /* Discard message when disabled. */
    return;
  }

  if (wsfTraceCb.sendMsgCback != NULL)
  {
    uint32_t len;
    va_list  args;
    /* Use heap memory to ease stack utilization. */
    static char buf[WSF_PRINTF_MAX_LEN];

    va_start(args, pStr);

    /* Dropped message notification. */
    if (wsfTraceCb.numDropMsg)
    {
      static char dropMsg[] = ">>> Trace buffer overflowed; %u message(s) lost <<<\r\n";

      len = wsfTraceOverFlowMessage(buf, dropMsg, wsfTraceCb.numDropMsg);

      if (wsfTraceCb.sendMsgCback((uint8_t *)buf, len) == FALSE)
      {
        wsfTraceCb.numDropMsg++;

        /* Trace I/O flow control continues. */
        return;
      }

      wsfTraceCb.numDropMsg = 0;
    }

    /* Format message. */
    len = PrintVsn(buf, WSF_PRINTF_MAX_LEN - WSF_TRACE_SUFFIX_LEN, pStr, args);
#if !defined(WSF_TRACE_LF_DISABLE) || (WSF_TRACE_LF_DISABLE == FALSE)
    buf[len++] = '\r';
#endif
    buf[len++] = '\n';

    /* Deliver message. */
    if (wsfTraceCb.sendMsgCback((uint8_t *)buf, (uint8_t)len) == FALSE)
    {
      /* Trace I/O flow controlled; drop message. */
      wsfTraceCb.numDropMsg = 1;
    }
  }
}
#endif

/*************************************************************************************************/
/*!
 *  \brief  Enable trace messages.
 *
 *  \param  enable    TRUE to enable, FALSE to disable
 */
/*************************************************************************************************/
void WsfTraceEnable(bool_t enable)
{
  WSF_ASSERT(wsfTraceCb.sendMsgCback);
  wsfTraceCb.enabled = enable;

/*
 * TOKEN | TRACE | Action
 *     0 |     0 | No tracing of any kind
 *     0 |     1 | Tracing through buffered UART
 *     1 |     0 | Tokenized tracing through CHCI
 *     1 |     1 | Tokenized tracing through buffered UART
 */
}

/*************************************************************************************************/
/*!
 *  \brief  Register trace handler.
 *
 *  \param  traceCback    Token event handler.
 *
 *  This routine registers trace output handler. This callback is called when the trace data is
 *  ready for writing.
 */
/*************************************************************************************************/
void WsfTraceRegisterHandler(WsfTraceHandler_t traceCback)
{
  wsfTraceCb.sendMsgCback = traceCback;
}
