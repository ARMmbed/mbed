/*************************************************************************************************/
/*!
 *  \file   wsf_trace.h
 *
 *  \brief  Trace message interface.
 *
 *  Copyright (c) 2009-2019 Arm Ltd. All Rights Reserved.
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
#ifndef WSF_TRACE_H
#define WSF_TRACE_H

#include <stdarg.h>

#if MBED_CONF_CORDIO_TRACE_HCI_PACKETS
#include "mbed_wsf_trace.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup WSF_TRACE_API
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

#ifndef WSF_TRACE_ENABLED
/*! \brief      Trace enable flag (default is disabled, override with compile-time directive). */
#define WSF_TRACE_ENABLED         FALSE
#endif

#ifndef WSF_TOKEN_ENABLED
/*! \brief      Tokenized tracing enable flag (default is disabled, override with compile-time directive). */
#define WSF_TOKEN_ENABLED         FALSE
#endif

#ifndef LL_TRACE_ENABLED
/*! \brief     Trace enabled for controller */
#define LL_TRACE_ENABLED          FALSE
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Token event handler. */
typedef bool_t (*WsfTraceHandler_t)(const uint8_t *pBuf, uint32_t len);

/*! \brief      BT4 Platform trace callback. */
typedef void (*WsfBt4TraceCback_t)(const char *pStr, va_list args);

/**************************************************************************************************
  Function Prototypes
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Output tokenized message.
 *
 *  \param  tok       Token
 *  \param  var       Variable
 */
/*************************************************************************************************/
void WsfToken(uint32_t tok, uint32_t var);

/*************************************************************************************************/
/*!
 *  \brief  Enable trace messages.
 *
 *  \param  enable    TRUE to enable, FALSE to disable
 */
/*************************************************************************************************/
void WsfTraceEnable(bool_t enable);

/*************************************************************************************************/
/*!
 *  \brief  Output trace message.
 *
 *  \param  pStr      Format string
 *  Addition parameters variable arguments to the format string.
 */
/*************************************************************************************************/
void WsfTrace(const char *pStr, ...);

/*************************************************************************************************/
/*!
 *  \brief  Register trace handler.
 *
 *  \param  traceCback  Token event handler.
 *
 *  This routine registers a token callback. This callback is called when the next token event
 *  is ready to be written to the I/O.
 */
/*************************************************************************************************/
void WsfTraceRegisterHandler(WsfTraceHandler_t traceCback);

/*************************************************************************************************/
/*!
 *  \brief  Register BT4 platform trace callback function.
 *
 *  \param  cback    Callback function
 */
/*************************************************************************************************/
void WsfTraceRegister(WsfBt4TraceCback_t cback);

/*************************************************************************************************/
/*!
 *  \brief  Service the trace ring buffer.
 *
 *  \return TRUE if trace messages pending, FALSE otherwise.
 *
 *  This routine is called in the main loop for a "push" type trace systems.
 */
/*************************************************************************************************/
bool_t WsfTokenService(void);

/**************************************************************************************************
  Macros
**************************************************************************************************/

#ifdef TOKEN_GENERATION

#define WSF_TOKEN(subsys, stat, msg)                    \
  __WSF_TOKEN_DEFINE__(                                 \
    /* token:   */     MODULE_ID, __LINE__,             \
    /* origin:  */     __FILE__, subsys,                \
    /* message: */     stat, msg)

#define WSF_TRACE0(subsys, stat, msg)                   WSF_TOKEN(subsys, stat, msg)
#define WSF_TRACE1(subsys, stat, msg, var1)             WSF_TOKEN(subsys, stat, msg)
#define WSF_TRACE2(subsys, stat, msg, var1, var2)       WSF_TOKEN(subsys, stat, msg)
#define WSF_TRACE3(subsys, stat, msg, var1, var2, var3) WSF_TOKEN(subsys, stat, msg)

#elif WSF_TOKEN_ENABLED == TRUE

/** \name Trace macros
 *
 */
/**@{*/
#define WSF_TRACE0(subsys, stat, msg)                   \
  WsfToken(((__LINE__ & 0xFFF) << 16) | MODULE_ID, 0)
#define WSF_TRACE1(subsys, stat, msg, var1)             \
  WsfToken(((__LINE__ & 0xFFF) << 16) | MODULE_ID, (uint32_t)(var1))
#define WSF_TRACE2(subsys, stat, msg, var1, var2)       \
  WsfToken(((__LINE__ & 0xFFF) << 16) | MODULE_ID, (uint32_t)(((var2) << 16) | ((var1) & 0xFFFF)))
#define WSF_TRACE3(subsys, stat, msg, var1, var2, var3) \
  WsfToken(((__LINE__ & 0xFFF) << 16) | MODULE_ID, (uint32_t)((((var3) & 0xFFFF) << 16) | (((var2) & 0xFF) << 8) | ((var1) & 0xFF)))
/**@}*/

#elif WSF_TRACE_ENABLED == TRUE

/** \name Trace macros
 *
 */
/**@{*/
#define WSF_TRACE0(subsys, stat, msg)                   \
  WsfTrace(msg)
#define WSF_TRACE1(subsys, stat, msg, var1)             \
  WsfTrace(msg, var1)
#define WSF_TRACE2(subsys, stat, msg, var1, var2)       \
  WsfTrace(msg, var1, var2)
#define WSF_TRACE3(subsys, stat, msg, var1, var2, var3) \
  WsfTrace(msg, var1, var2, var3)
#define WSF_TRACE4(subsys, stat, msg, var1, var2, var3, var4) \
  WsfTrace(msg, var1, var2, var3, var4)
#define WSF_TRACE5(subsys, stat, msg, var1, var2, var3, var4, var5) \
  WsfTrace(msg, var1, var2, var3, var4, var5)
#define WSF_TRACE6(subsys, stat, msg, var1, var2, var3, var4, var5, var6) \
  WsfTrace(msg, var1, var2, var3, var4, var5, var6)
#define WSF_TRACE7(subsys, stat, msg, var1, var2, var3, var4, var5, var6, var7) \
  WsfTrace(msg, var1, var2, var3, var4, var5, var6, var7)
#define WSF_TRACE8(subsys, stat, msg, var1, var2, var3, var4, var5, var6, var7, var8) \
  WsfTrace(msg, var1, var2, var3, var4, var5, var6, var7, var8)
#define WSF_TRACE9(subsys, stat, msg, var1, var2, var3, var4, var5, var6, var7, var8, var9) \
  WsfTrace(msg, var1, var2, var3, var4, var5, var6, var7, var8, var9)
#define WSF_TRACE12(subsys, stat, msg, var1, var2, var3, var4, var5, var6, var7, var8, var9, var10, var11, var12) \
  WsfTrace(msg, var1, var2, var3, var4, var5, var6, var7, var8, var9, var10, var11, var12)
/**@}*/

#else

/** \name Trace macros
 *
 */
/**@{*/
#define WSF_TRACE0(subsys, stat, msg)
#define WSF_TRACE1(subsys, stat, msg, var1)
#define WSF_TRACE2(subsys, stat, msg, var1, var2)
#define WSF_TRACE3(subsys, stat, msg, var1, var2, var3)
#define WSF_TRACE4(subsys, stat, msg, var1, var2, var3, var4)
#define WSF_TRACE5(subsys, stat, msg, var1, var2, var3, var4, var5)
#define WSF_TRACE6(subsys, stat, msg, var1, var2, var3, var4, var5, var6)
#define WSF_TRACE7(subsys, stat, msg, var1, var2, var3, var4, var5, var6, var7)
#define WSF_TRACE8(subsys, stat, msg, var1, var2, var3, var4, var5, var6, var7, var8)
#define WSF_TRACE9(subsys, stat, msg, var1, var2, var3, var4, var5, var6, var7, var8, var9)
#define WSF_TRACE12(subsys, stat, msg, var1, var2, var3, var4, var5, var6, var7, var8, var9, var10, var11, var12)
/**@}*/
#endif

/*! \brief 0 argument WSF info trace. */
#define WSF_TRACE_INFO0(msg)
/*! \brief 1 argument WSF info trace. */
#define WSF_TRACE_INFO1(msg, var1)
/*! \brief 2 argument WSF info trace. */
#define WSF_TRACE_INFO2(msg, var1, var2)
/*! \brief 3 argument WSF info trace. */
#define WSF_TRACE_INFO3(msg, var1, var2, var3)
/*! \brief 0 argument WSF warning trace. */
#define WSF_TRACE_WARN0(msg)                        WSF_TRACE0("WSF", "WARN", msg)
/*! \brief 1 argument WSF warning trace. */
#define WSF_TRACE_WARN1(msg, var1)                  WSF_TRACE1("WSF", "WARN", msg, var1)
/*! \brief 2 argument WSF warning trace. */
#define WSF_TRACE_WARN2(msg, var1, var2)            WSF_TRACE2("WSF", "WARN", msg, var1, var2)
/*! \brief 3 argument WSF warning trace. */
#define WSF_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE3("WSF", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument WSF error trace. */
#define WSF_TRACE_ERR0(msg)                         WSF_TRACE0("WSF", "ERR",  msg)
/*! \brief 1 argument WSF error trace. */
#define WSF_TRACE_ERR1(msg, var1)                   WSF_TRACE1("WSF", "ERR",  msg, var1)
/*! \brief 2 argument WSF error trace. */
#define WSF_TRACE_ERR2(msg, var1, var2)             WSF_TRACE2("WSF", "ERR",  msg, var1, var2)
/*! \brief 3 argument WSF error trace. */
#define WSF_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE3("WSF", "ERR",  msg, var1, var2, var3)
/*! \brief 0 argument WSF buffer allocation trace. */
#define WSF_TRACE_ALLOC0(msg)
/*! \brief 1 argument WSF buffer allocation trace. */
#define WSF_TRACE_ALLOC1(msg, var1)
/*! \brief 2 argument WSF buffer allocation trace. */
#define WSF_TRACE_ALLOC2(msg, var1, var2)
/*! \brief 3 argument WSF buffer allocation trace. */
#define WSF_TRACE_ALLOC3(msg, var1, var2, var3)
/*! \brief 0 argument WSF buffer free trace. */
#define WSF_TRACE_FREE0(msg)
/*! \brief 1 argument WSF buffer free trace. */
#define WSF_TRACE_FREE1(msg, var1)
/*! \brief 2 argument WSF buffer free trace. */
#define WSF_TRACE_FREE2(msg, var1, var2)
/*! \brief 3 argument WSF buffer free trace. */
#define WSF_TRACE_FREE3(msg, var1, var2, var3)
/*! \brief 0 argument WSF message trace. */
#define WSF_TRACE_MSG0(msg)
/*! \brief 1 argument WSF message trace. */
#define WSF_TRACE_MSG1(msg, var1)
/*! \brief 2 argument WSF message trace. */
#define WSF_TRACE_MSG2(msg, var1, var2)
/*! \brief 3 argument WSF message trace. */
#define WSF_TRACE_MSG3(msg, var1, var2, var3)

/*! \brief 0 argument HCI info trace. */
#define HCI_TRACE_INFO0(msg)                        WSF_TRACE0("HCI", "INFO", msg)
/*! \brief 1 argument HCI info trace. */
#define HCI_TRACE_INFO1(msg, var1)                  WSF_TRACE1("HCI", "INFO", msg, var1)
/*! \brief 2 argument HCI info trace. */
#define HCI_TRACE_INFO2(msg, var1, var2)            WSF_TRACE2("HCI", "INFO", msg, var1, var2)
/*! \brief 3 argument HCI info trace. */
#define HCI_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE3("HCI", "INFO", msg, var1, var2, var3)
/*! \brief 0 argument HCI warning trace. */
#define HCI_TRACE_WARN0(msg)                        WSF_TRACE0("HCI", "WARN", msg)
/*! \brief 1 argument HCI warning trace. */
#define HCI_TRACE_WARN1(msg, var1)                  WSF_TRACE1("HCI", "WARN", msg, var1)
/*! \brief 2 argument HCI warning trace. */
#define HCI_TRACE_WARN2(msg, var1, var2)            WSF_TRACE2("HCI", "WARN", msg, var1, var2)
/*! \brief 3 argument HCI warning trace. */
#define HCI_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE3("HCI", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument HCI error trace. */
#define HCI_TRACE_ERR0(msg)                         WSF_TRACE0("HCI", "ERR",  msg)
/*! \brief 1 argument HCI error trace. */
#define HCI_TRACE_ERR1(msg, var1)                   WSF_TRACE1("HCI", "ERR",  msg, var1)
/*! \brief 2 argument HCI error trace. */
#define HCI_TRACE_ERR2(msg, var1, var2)             WSF_TRACE2("HCI", "ERR",  msg, var1, var2)
/*! \brief 3 argument HCI error trace. */
#define HCI_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE3("HCI", "ERR",  msg, var1, var2, var3)

/*! \brief 0 argument DM info trace. */
#define DM_TRACE_INFO0(msg)                         WSF_TRACE0("DM", "INFO", msg)
/*! \brief 1 argument DM info trace. */
#define DM_TRACE_INFO1(msg, var1)                   WSF_TRACE1("DM", "INFO", msg, var1)
/*! \brief 2 argument DM info trace. */
#define DM_TRACE_INFO2(msg, var1, var2)             WSF_TRACE2("DM", "INFO", msg, var1, var2)
/*! \brief 3 argument DM info trace. */
#define DM_TRACE_INFO3(msg, var1, var2, var3)       WSF_TRACE3("DM", "INFO", msg, var1, var2, var3)
/*! \brief 0 argument DM warning trace. */
#define DM_TRACE_WARN0(msg)                         WSF_TRACE0("DM", "WARN", msg)
/*! \brief 1 argument DM warning trace. */
#define DM_TRACE_WARN1(msg, var1)                   WSF_TRACE1("DM", "WARN", msg, var1)
/*! \brief 2 argument DM warning trace. */
#define DM_TRACE_WARN2(msg, var1, var2)             WSF_TRACE2("DM", "WARN", msg, var1, var2)
/*! \brief 3 argument DM warning trace. */
#define DM_TRACE_WARN3(msg, var1, var2, var3)       WSF_TRACE3("DM", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument DM error trace. */
#define DM_TRACE_ERR0(msg)                          WSF_TRACE0("DM", "ERR",  msg)
/*! \brief 1 argument DM error trace. */
#define DM_TRACE_ERR1(msg, var1)                    WSF_TRACE1("DM", "ERR",  msg, var1)
/*! \brief 2 argument DM error trace. */
#define DM_TRACE_ERR2(msg, var1, var2)              WSF_TRACE2("DM", "ERR",  msg, var1, var2)
/*! \brief 3 argument DM error trace. */
#define DM_TRACE_ERR3(msg, var1, var2, var3)        WSF_TRACE3("DM", "ERR",  msg, var1, var2, var3)
/*! \brief 0 argument DM buffer allocation trace. */
#define DM_TRACE_ALLOC0(msg)                        WSF_TRACE0("DM", "ALLOC", msg)
/*! \brief 1 argument DM buffer allocation trace. */
#define DM_TRACE_ALLOC1(msg, var1)                  WSF_TRACE1("DM", "ALLOC", msg, var1)
/*! \brief 2 argument DM buffer allocation trace. */
#define DM_TRACE_ALLOC2(msg, var1, var2)            WSF_TRACE2("DM", "ALLOC", msg, var1, var2)
/*! \brief 3 argument DM buffer allocation trace. */
#define DM_TRACE_ALLOC3(msg, var1, var2, var3)      WSF_TRACE3("DM", "ALLOC", msg, var1, var2, var3)
/*! \brief 0 argument DM buffer free trace. */
#define DM_TRACE_FREE0(msg)                         WSF_TRACE0("DM", "FREE", msg)
/*! \brief 1 argument DM buffer free trace. */
#define DM_TRACE_FREE1(msg, var1)                   WSF_TRACE1("DM", "FREE", msg, var1)
/*! \brief 2 argument DM buffer free trace. */
#define DM_TRACE_FREE2(msg, var1, var2)             WSF_TRACE2("DM", "FREE", msg, var1, var2)
/*! \brief 3 argument DM buffer free trace. */
#define DM_TRACE_FREE3(msg, var1, var2, var3)       WSF_TRACE3("DM", "FREE", msg, var1, var2, var3)

/*! \brief 0 argument L2C info trace. */
#define L2C_TRACE_INFO0(msg)                        WSF_TRACE0("L2C", "INFO", msg)
/*! \brief 1 argument L2C info trace. */
#define L2C_TRACE_INFO1(msg, var1)                  WSF_TRACE1("L2C", "INFO", msg, var1)
/*! \brief 2 argument L2C info trace. */
#define L2C_TRACE_INFO2(msg, var1, var2)            WSF_TRACE2("L2C", "INFO", msg, var1, var2)
/*! \brief 3 argument L2C info trace. */
#define L2C_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE3("L2C", "INFO", msg, var1, var2, var3)
/*! \brief 0 argument L2C warning trace. */
#define L2C_TRACE_WARN0(msg)                        WSF_TRACE0("L2C", "WARN", msg)
/*! \brief 1 argument L2C warning trace. */
#define L2C_TRACE_WARN1(msg, var1)                  WSF_TRACE1("L2C", "WARN", msg, var1)
/*! \brief 2 argument L2C warning trace. */
#define L2C_TRACE_WARN2(msg, var1, var2)            WSF_TRACE2("L2C", "WARN", msg, var1, var2)
/*! \brief 3 argument L2C warning trace. */
#define L2C_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE3("L2C", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument L2C error trace. */
#define L2C_TRACE_ERR0(msg)                         WSF_TRACE0("L2C", "ERR",  msg)
/*! \brief 1 argument L2C error trace. */
#define L2C_TRACE_ERR1(msg, var1)                   WSF_TRACE1("L2C", "ERR",  msg, var1)
/*! \brief 2 argument L2C error trace. */
#define L2C_TRACE_ERR2(msg, var1, var2)             WSF_TRACE2("L2C", "ERR",  msg, var1, var2)
/*! \brief 3 argument L2C error trace. */
#define L2C_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE3("L2C", "ERR",  msg, var1, var2, var3)

/*! \brief 0 argument ATT info trace. */
#define ATT_TRACE_INFO0(msg)                        WSF_TRACE0("ATT", "INFO", msg)
/*! \brief 1 argument ATT info trace. */
#define ATT_TRACE_INFO1(msg, var1)                  WSF_TRACE1("ATT", "INFO", msg, var1)
/*! \brief 2 argument ATT info trace. */
#define ATT_TRACE_INFO2(msg, var1, var2)            WSF_TRACE2("ATT", "INFO", msg, var1, var2)
/*! \brief 3 argument ATT info trace. */
#define ATT_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE3("ATT", "INFO", msg, var1, var2, var3)
/*! \brief 0 argument ATT warning trace. */
#define ATT_TRACE_WARN0(msg)                        WSF_TRACE0("ATT", "WARN", msg)
/*! \brief 1 argument ATT warning trace. */
#define ATT_TRACE_WARN1(msg, var1)                  WSF_TRACE1("ATT", "WARN", msg, var1)
/*! \brief 2 argument ATT warning trace. */
#define ATT_TRACE_WARN2(msg, var1, var2)            WSF_TRACE2("ATT", "WARN", msg, var1, var2)
/*! \brief 3 argument ATT warning trace. */
#define ATT_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE3("ATT", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument ATT error trace. */
#define ATT_TRACE_ERR0(msg)                         WSF_TRACE0("ATT", "ERR",  msg)
/*! \brief 1 argument ATT error trace. */
#define ATT_TRACE_ERR1(msg, var1)                   WSF_TRACE1("ATT", "ERR",  msg, var1)
/*! \brief 2 argument ATT error trace. */
#define ATT_TRACE_ERR2(msg, var1, var2)             WSF_TRACE2("ATT", "ERR",  msg, var1, var2)
/*! \brief 3 argument ATT error trace. */
#define ATT_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE3("ATT", "ERR",  msg, var1, var2, var3)

/*! \brief 0 argument EATT info trace. */
#define EATT_TRACE_INFO0(msg)                       WSF_TRACE0("EATT", "INFO", msg)
/*! \brief 1 argument EATT info trace. */
#define EATT_TRACE_INFO1(msg, var1)                 WSF_TRACE1("EATT", "INFO", msg, var1)
/*! \brief 2 argument EATT info trace. */
#define EATT_TRACE_INFO2(msg, var1, var2)           WSF_TRACE2("EATT", "INFO", msg, var1, var2)
/*! \brief 3 argument EATT info trace. */
#define EATT_TRACE_INFO3(msg, var1, var2, var3)     WSF_TRACE3("EATT", "INFO", msg, var1, var2, var3)
/*! \brief 0 argument EATT warning trace. */
#define EATT_TRACE_WARN0(msg)                       WSF_TRACE0("EATT", "WARN", msg)
/*! \brief 1 argument EATT warning trace. */
#define EATT_TRACE_WARN1(msg, var1)                 WSF_TRACE1("EATT", "WARN", msg, var1)
/*! \brief 2 argument EATT warning trace. */
#define EATT_TRACE_WARN2(msg, var1, var2)           WSF_TRACE2("EATT", "WARN", msg, var1, var2)
/*! \brief 3 argument EATT warning trace. */
#define EATT_TRACE_WARN3(msg, var1, var2, var3)     WSF_TRACE3("EATT", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument EATT error trace. */
#define EATT_TRACE_ERR0(msg)                        WSF_TRACE0("EATT", "ERR",  msg)
/*! \brief 1 argument EATT error trace. */
#define EATT_TRACE_ERR1(msg, var1)                  WSF_TRACE1("EATT", "ERR",  msg, var1)
/*! \brief 2 argument EATT error trace. */
#define EATT_TRACE_ERR2(msg, var1, var2)            WSF_TRACE2("EATT", "ERR",  msg, var1, var2)
/*! \brief 3 argument EATT error trace. */
#define EATT_TRACE_ERR3(msg, var1, var2, var3)      WSF_TRACE3("EATT", "ERR",  msg, var1, var2, var3)

/*! \brief 0 argument SMP info trace. */
#define SMP_TRACE_INFO0(msg)                        WSF_TRACE0("SMP", "INFO", msg)
/*! \brief 1 argument SMP info trace. */
#define SMP_TRACE_INFO1(msg, var1)                  WSF_TRACE1("SMP", "INFO", msg, var1)
/*! \brief 2 argument SMP info trace. */
#define SMP_TRACE_INFO2(msg, var1, var2)            WSF_TRACE2("SMP", "INFO", msg, var1, var2)
/*! \brief 3 argument SMP info trace. */
#define SMP_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE3("SMP", "INFO", msg, var1, var2, var3)
/*! \brief 0 argument SMP warning trace. */
#define SMP_TRACE_WARN0(msg)                        WSF_TRACE0("SMP", "WARN", msg)
/*! \brief 1 argument SMP warning trace. */
#define SMP_TRACE_WARN1(msg, var1)                  WSF_TRACE1("SMP", "WARN", msg, var1)
/*! \brief 2 argument SMP warning trace. */
#define SMP_TRACE_WARN2(msg, var1, var2)            WSF_TRACE2("SMP", "WARN", msg, var1, var2)
/*! \brief 3 argument SMP warning trace. */
#define SMP_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE3("SMP", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument SMP error trace. */
#define SMP_TRACE_ERR0(msg)                         WSF_TRACE0("SMP", "ERR",  msg)
/*! \brief 1 argument SMP error trace. */
#define SMP_TRACE_ERR1(msg, var1)                   WSF_TRACE1("SMP", "ERR",  msg, var1)
/*! \brief 2 argument SMP error trace. */
#define SMP_TRACE_ERR2(msg, var1, var2)             WSF_TRACE2("SMP", "ERR",  msg, var1, var2)
/*! \brief 3 argument SMP error trace. */
#define SMP_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE3("SMP", "ERR",  msg, var1, var2, var3)

/*! \brief 0 argument App info trace. */
#define APP_TRACE_INFO0(msg)                        WSF_TRACE0("APP", "INFO", msg)
/*! \brief 1 argument App info trace. */
#define APP_TRACE_INFO1(msg, var1)                  WSF_TRACE1("APP", "INFO", msg, var1)
/*! \brief 2 argument App info trace. */
#define APP_TRACE_INFO2(msg, var1, var2)            WSF_TRACE2("APP", "INFO", msg, var1, var2)
/*! \brief 3 argument App info trace. */
#define APP_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE3("APP", "INFO", msg, var1, var2, var3)
/*! \brief 4 argument App info trace. */
#define APP_TRACE_INFO4(msg, var1, var2, var3, var4) \
                                                    WSF_TRACE4(INFO, APP, msg, var1, var2, var3, var4)
/*! \brief 5 argument App info trace. */
#define APP_TRACE_INFO5(msg, var1, var2, var3, var4, var5) \
                                                    WSF_TRACE5(INFO, APP, msg, var1, var2, var3, var4, var5)
/*! \brief 6 argument App info trace. */
#define APP_TRACE_INFO6(msg, var1, var2, var3, var4, var5, var6) \
                                                    WSF_TRACE6(INFO, APP, msg, var1, var2, var3, var4, var5, var6)
/*! \brief 7 argument App info trace. */
#define APP_TRACE_INFO7(msg, var1, var2, var3, var4, var5, var6, var7) \
                                                    WSF_TRACE7(INFO, APP, msg, var1, var2, var3, var4, var5, var6, var7)
/*! \brief 8 argument App info trace. */
#define APP_TRACE_INFO8(msg, var1, var2, var3, var4, var5, var6, var7, var8) \
                                                    WSF_TRACE8(INFO, APP, msg, var1, var2, var3, var4, var5, var6, var7, var8)
/*! \brief 9 argument App info trace. */
#define APP_TRACE_INFO9(msg, var1, var2, var3, var4, var5, var6, var7, var8, var9) \
                                                    WSF_TRACE9(INFO, APP, msg, var1, var2, var3, var4, var5, var6, var7, var8, var9)
/*! \brief 12 argument App info trace. */
#define APP_TRACE_INFO12(msg, var1, var2, var3, var4, var5, var6, var7, var8, var9, var10, var11, var12) \
                                                    WSF_TRACE12(INFO, APP, msg, var1, var2, var3, var4, var5, var6, var7, var8, var9, var10, var11, var12)
/*! \brief 0 argument App warning trace. */
#define APP_TRACE_WARN0(msg)                        WSF_TRACE0("APP", "WARN", msg)
/*! \brief 1 argument App warning trace. */
#define APP_TRACE_WARN1(msg, var1)                  WSF_TRACE1("APP", "WARN", msg, var1)
/*! \brief 2 argument App warning trace. */
#define APP_TRACE_WARN2(msg, var1, var2)            WSF_TRACE2("APP", "WARN", msg, var1, var2)
/*! \brief 3 argument App warning trace. */
#define APP_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE3("APP", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument App error trace. */
#define APP_TRACE_ERR0(msg)                         WSF_TRACE0("APP", "ERR",  msg)
/*! \brief 1 argument App error trace. */
#define APP_TRACE_ERR1(msg, var1)                   WSF_TRACE1("APP", "ERR",   msg, var1)
/*! \brief 2 argument App error trace. */
#define APP_TRACE_ERR2(msg, var1, var2)             WSF_TRACE2("APP", "ERR",   msg, var1, var2)
/*! \brief 3 argument App error trace. */
#define APP_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE3("APP", "ERR",   msg, var1, var2, var3)

#if (LL_TRACE_ENABLED == TRUE)
/*! \brief 0 argument LL info trace. */
#define LL_TRACE_INFO0(msg)                         WSF_TRACE0("LL", "INFO", msg)
/*! \brief 1 argument LL info trace. */
#define LL_TRACE_INFO1(msg, var1)                   WSF_TRACE1("LL", "INFO", msg, var1)
/*! \brief 2 argument LL info trace. */
#define LL_TRACE_INFO2(msg, var1, var2)             WSF_TRACE2("LL", "INFO", msg, var1, var2)
/*! \brief 3 argument LL info trace. */
#define LL_TRACE_INFO3(msg, var1, var2, var3)       WSF_TRACE3("LL", "INFO", msg, var1, var2, var3)
/*! \brief 0 argument LL warning trace. */
#define LL_TRACE_WARN0(msg)                         WSF_TRACE0("LL", "WARN", msg)
/*! \brief 1 argument LL warning trace. */
#define LL_TRACE_WARN1(msg, var1)                   WSF_TRACE1("LL", "WARN", msg, var1)
/*! \brief 2 argument LL warning trace. */
#define LL_TRACE_WARN2(msg, var1, var2)             WSF_TRACE2("LL", "WARN", msg, var1, var2)
/*! \brief 3 argument LL warning trace. */
#define LL_TRACE_WARN3(msg, var1, var2, var3)       WSF_TRACE3("LL", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument LL error trace. */
#define LL_TRACE_ERR0(msg)                          WSF_TRACE0("LL", "ERR",  msg)
/*! \brief 1 argument LL error trace. */
#define LL_TRACE_ERR1(msg, var1)                    WSF_TRACE1("LL", "ERR",  msg, var1)
/*! \brief 2 argument LL error trace. */
#define LL_TRACE_ERR2(msg, var1, var2)              WSF_TRACE2("LL", "ERR",  msg, var1, var2)
/*! \brief 3 argument LL error trace. */
#define LL_TRACE_ERR3(msg, var1, var2, var3)        WSF_TRACE3("LL", "ERR",  msg, var1, var2, var3)
#else   /* LL_TRACE_ENABLED */
#define LL_TRACE_INFO0(msg)
/*! \brief 1 argument LL info trace. */
#define LL_TRACE_INFO1(msg, var1)
/*! \brief 2 argument LL info trace. */
#define LL_TRACE_INFO2(msg, var1, var2)
/*! \brief 3 argument LL info trace. */
#define LL_TRACE_INFO3(msg, var1, var2, var3)
/*! \brief 0 argument LL warning trace. */
#define LL_TRACE_WARN0(msg)
/*! \brief 1 argument LL warning trace. */
#define LL_TRACE_WARN1(msg, var1)
/*! \brief 2 argument LL warning trace. */
#define LL_TRACE_WARN2(msg, var1, var2)
/*! \brief 3 argument LL warning trace. */
#define LL_TRACE_WARN3(msg, var1, var2, var3)
/*! \brief 0 argument LL error trace. */
#define LL_TRACE_ERR0(msg)
/*! \brief 1 argument LL error trace. */
#define LL_TRACE_ERR1(msg, var1)
/*! \brief 2 argument LL error trace. */
#define LL_TRACE_ERR2(msg, var1, var2)
/*! \brief 3 argument LL error trace. */
#define LL_TRACE_ERR3(msg, var1, var2, var3)
#endif  /* LL_TRACE_ENABLED */

#if (AUD_TRACE_ENABLED == TRUE)
/*! \brief 0 argument audio info trace. */
#define AUD_TRACE_INFO0(msg)                        WSF_TRACE0("AUD", "INFO", msg)
/*! \brief 1 argument audio info trace. */
#define AUD_TRACE_INFO1(msg, var1)                  WSF_TRACE1("AUD", "INFO", msg, var1)
/*! \brief 2 argument audio info trace. */
#define AUD_TRACE_INFO2(msg, var1, var2)            WSF_TRACE2("AUD", "INFO", msg, var1, var2)
/*! \brief 3 argument audio info trace. */
#define AUD_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE3("AUD", "INFO", msg, var1, var2, var3)
/*! \brief 0 argument audio warning trace. */
#define AUD_TRACE_WARN0(msg)                        WSF_TRACE0("AUD", "WARN", msg)
/*! \brief 1 argument audio warning trace. */
#define AUD_TRACE_WARN1(msg, var1)                  WSF_TRACE1("AUD", "WARN", msg, var1)
/*! \brief 2 argument audio warning trace. */
#define AUD_TRACE_WARN2(msg, var1, var2)            WSF_TRACE2("AUD", "WARN", msg, var1, var2)
/*! \brief 3 argument audio warning trace. */
#define AUD_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE3("AUD", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument audio error trace. */
#define AUD_TRACE_ERR0(msg)                         WSF_TRACE0("AUD", "ERR",  msg)
/*! \brief 1 argument audio error trace. */
#define AUD_TRACE_ERR1(msg, var1)                   WSF_TRACE1("AUD", "ERR",  msg, var1)
/*! \brief 2 argument audio error trace. */
#define AUD_TRACE_ERR2(msg, var1, var2)             WSF_TRACE2("AUD", "ERR",  msg, var1, var2)
/*! \brief 3 argument audio error trace. */
#define AUD_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE3("AUD", "ERR",  msg, var1, var2, var3)
#else
/*! \brief 0 argument audio info trace. */
#define AUD_TRACE_INFO0(msg)
/*! \brief 1 argument audio info trace. */
#define AUD_TRACE_INFO1(msg, var1)
/*! \brief 2 argument audio info trace. */
#define AUD_TRACE_INFO2(msg, var1, var2)
/*! \brief 3 argument audio info trace. */
#define AUD_TRACE_INFO3(msg, var1, var2, var3)
/*! \brief 0 argument audio warning trace. */
#define AUD_TRACE_WARN0(msg)
/*! \brief 1 argument audio warning trace. */
#define AUD_TRACE_WARN1(msg, var1)
/*! \brief 2 argument audio warning trace. */
#define AUD_TRACE_WARN2(msg, var1, var2)
/*! \brief 3 argument audio warning trace. */
#define AUD_TRACE_WARN3(msg, var1, var2, var3)
/*! \brief 0 argument audio error trace. */
#define AUD_TRACE_ERR0(msg)
/*! \brief 1 argument audio error trace. */
#define AUD_TRACE_ERR1(msg, var1)
/*! \brief 2 argument audio error trace. */
#define AUD_TRACE_ERR2(msg, var1, var2)
/*! \brief 3 argument audio error trace. */
#define AUD_TRACE_ERR3(msg, var1, var2, var3)
#endif  /* AUD_TRACE_ENABLED */

/*! \brief 0 argument MESH info trace. */
#define MESH_TRACE_INFO0(msg)                       WSF_TRACE0("MESH", "INFO", msg)
/*! \brief 1 argument MESH info trace. */
#define MESH_TRACE_INFO1(msg, var1)                 WSF_TRACE1("MESH", "INFO", msg, var1)
/*! \brief 2 argument MESH info trace. */
#define MESH_TRACE_INFO2(msg, var1, var2)           WSF_TRACE2("MESH", "INFO", msg, var1, var2)
/*! \brief 3 argument MESH info trace. */
#define MESH_TRACE_INFO3(msg, var1, var2, var3)     WSF_TRACE3("MESH", "INFO", msg, var1, var2, var3)
/*! \brief 0 argument MESH warning trace. */
#define MESH_TRACE_WARN0(msg)                       WSF_TRACE0("MESH", "WARN", msg)
/*! \brief 1 argument MESH warning trace. */
#define MESH_TRACE_WARN1(msg, var1)                 WSF_TRACE1("MESH", "WARN", msg, var1)
/*! \brief 2 argument MESH warning trace. */
#define MESH_TRACE_WARN2(msg, var1, var2)           WSF_TRACE2("MESH", "WARN", msg, var1, var2)
/*! \brief 3 argument MESH warning trace. */
#define MESH_TRACE_WARN3(msg, var1, var2, var3)     WSF_TRACE3("MESH", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument MESH error trace. */
#define MESH_TRACE_ERR0(msg)                        WSF_TRACE0("MESH", "ERR",  msg)
/*! \brief 1 argument MESH error trace. */
#define MESH_TRACE_ERR1(msg, var1)                  WSF_TRACE1("MESH", "ERR",  msg, var1)
/*! \brief 2 argument MESH error trace. */
#define MESH_TRACE_ERR2(msg, var1, var2)            WSF_TRACE2("MESH", "ERR",  msg, var1, var2)
/*! \brief 3 argument MESH error trace. */
#define MESH_TRACE_ERR3(msg, var1, var2, var3)      WSF_TRACE3("MESH", "ERR",  msg, var1, var2, var3)

/*! \brief 0 argument MMDL info trace. */
#define MMDL_TRACE_INFO0(msg)                       WSF_TRACE0("MMDL", "INFO", msg)
/*! \brief 1 argument MMDL info trace. */
#define MMDL_TRACE_INFO1(msg, var1)                 WSF_TRACE1("MMDL", "INFO", msg, var1)
/*! \brief 2 argument MMDL info trace. */
#define MMDL_TRACE_INFO2(msg, var1, var2)           WSF_TRACE2("MMDL", "INFO", msg, var1, var2)
/*! \brief 3 argument MMDL info trace. */
#define MMDL_TRACE_INFO3(msg, var1, var2, var3)     WSF_TRACE3("MMDL", "INFO", msg, var1, var2, var3)
/*! \brief 0 argument MMDL warning trace. */
#define MMDL_TRACE_WARN0(msg)                       WSF_TRACE0("MMDL", "WARN", msg)
/*! \brief 1 argument MMDL warning trace. */
#define MMDL_TRACE_WARN1(msg, var1)                 WSF_TRACE1("MMDL", "WARN", msg, var1)
/*! \brief 2 argument MMDL warning trace. */
#define MMDL_TRACE_WARN2(msg, var1, var2)           WSF_TRACE2("MMDL", "WARN", msg, var1, var2)
/*! \brief 3 argument MMDL warning trace. */
#define MMDL_TRACE_WARN3(msg, var1, var2, var3)     WSF_TRACE3("MMDL", "WARN", msg, var1, var2, var3)
/*! \brief 0 argument MMDL error trace. */
#define MMDL_TRACE_ERR0(msg)                        WSF_TRACE0("MMDL", "ERR",  msg)
/*! \brief 1 argument MMDL error trace. */
#define MMDL_TRACE_ERR1(msg, var1)                  WSF_TRACE1("MMDL", "ERR",  msg, var1)
/*! \brief 2 argument MMDL error trace. */
#define MMDL_TRACE_ERR2(msg, var1, var2)            WSF_TRACE2("MMDL", "ERR",  msg, var1, var2)
/*! \brief 3 argument MMDL error trace. */
#define MMDL_TRACE_ERR3(msg, var1, var2, var3)      WSF_TRACE3("MMDL", "ERR",  msg, var1, var2, var3)

/*! \brief Enable LL trace. */
#if (WSF_TRACE_ENABLED == TRUE) || (WSF_TOKEN_ENABLED == TRUE)
#define LL_TRACE_ENABLE(ena)                        WsfTraceEnable(ena)
#else
#define LL_TRACE_ENABLE(ena)
#endif

/*! \} */    /* WSF_TRACE_API */

#ifdef __cplusplus
};
#endif

#endif /* WSF_TRACE_H */
