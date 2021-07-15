/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Arm Ltd. vendor specific HCI command module implementation file.
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

#include "lhci_int.h"
#include "hci_defs.h"
#include "bb_ble_api.h"
#include "ll_api.h"
#include "pal_sys.h"
#include "sch_api.h"
#include "bb_ble_sniffer_api.h"
#include "wsf_assert.h"
#include "wsf_buf.h"
#include "wsf_cs.h"
#include "wsf_heap.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Maximum number of pool items. */
#define LHCI_MAX_POOL                   5

/*! \brief      Get Pool Statistics command complete event length. */
#define LHCI_LEN_GET_POOL_STATS_EVT(n)  (3 + (sizeof(WsfBufPoolStat_t) * (n)))

/*! \brief      Get System Statistics command complete event length. */
#define LHCI_LEN_GET_SYS_STATS_EVT      (2 * sizeof(uint16_t) + 3 * sizeof(uint32_t) + 7 * sizeof(uint16_t) + 2 * sizeof(uint16_t))

/*! \brief      Get context size command complete event length. */
#define LHCI_LEN_GET_CONTEXT_SIZES_EVT  (2 * 14 * sizeof(uint16_t))

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Reset flag. */
bool_t lhciResetPending = FALSE;

/* Bootloader flag. */
bool_t lhciEnterBootloaderPending = FALSE;

/**************************************************************************************************
  Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Decode Packetcraft common vendor specific HCI command packet.
 *
 *  \param  pHdr    Unpacked command header.
 *  \param  pBuf    Packed HCI packet buffer.
 *
 *  Command processing for vendor specific commands.
 *
 *  \return TRUE if opcode handled, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t lhciCommonVsStdDecodeCmdPkt(LhciHdr_t *pHdr, uint8_t *pBuf)
{
  uint8_t status = HCI_SUCCESS;
  uint8_t evtParamLen = 1;      /* default is status field only */

  /* Decode and consume command packet. */
  switch (pHdr->opCode)
  {
    /* --- extended device commands --- */

    case LHCI_OPCODE_VS_SET_OP_FLAGS:
    {
      uint32_t flags;
      bool_t enable;
      BSTREAM_TO_UINT32(flags, pBuf);
      BSTREAM_TO_UINT8 (enable, pBuf);

      status = LlSetOpFlags(flags, enable);
      break;
    }
    case LHCI_OPCODE_VS_SET_EVENT_MASK:
    {
      bool_t   enable;
      uint64_t mask;
      BSTREAM_TO_UINT64(mask, pBuf);
      BSTREAM_TO_UINT8 (enable, pBuf);

      if (mask & ((uint64_t)LHCI_VS_EVT_MASK_SCAN_REPORT_EVT) << LHCI_BYTE_TO_BITS(0))
      {
        LlScanReportEnable(enable);
      }

      if (mask & ((uint64_t)LHCI_VS_EVT_MASK_DIAG_TRACE_EVT) << LHCI_BYTE_TO_BITS(0))
      {
        LL_TRACE_ENABLE(enable);
      }

#if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
      /* TODO: Use function pointer to allow run time configuration of supported versions. */
      if (mask & ((uint64_t)LHCI_VS_EVT_MASK_ISO_EVENT_CMPL_EVT) << LHCI_BYTE_TO_BITS(0))
      {
        LlIsoEventCompleteEnable(enable);
      }
#endif

      break;
    }
    case LHCI_OPCODE_VS_SET_TX_TEST_ERR_PATT:
    {
      uint32_t pattern;

      BSTREAM_TO_UINT32(pattern, pBuf);
      status = LlSetTxTestErrorPattern(pattern);
      break;
    }
    case LHCI_OPCODE_VS_SET_SNIFFER_ENABLE:
#if (BB_SNIFFER_ENABLED == TRUE)
       status = BbBleInitSniffer(pBuf[0], pBuf[1]);
#else
       status = HCI_ERR_UNKNOWN_CMD;
#endif
       break;

    case LHCI_OPCODE_VS_SET_BD_ADDR:
      LlSetBdAddr(pBuf);
      break;
    case LHCI_OPCODE_VS_GET_RAND_ADDR:
      status = LlGetRandAddr(pBuf);
      evtParamLen += sizeof(bdAddr_t);
      break;
    case LHCI_OPCODE_VS_SET_LOCAL_FEAT:
      status = LlSetFeatures(pBuf);
      break;
    case LHCI_OPCODE_VS_SET_DIAG_MODE:
      PalSysSetTrap(pBuf[0]);
      break;
    case LHCI_OPCODE_VS_GET_SYS_STATS:
      evtParamLen += LHCI_LEN_GET_SYS_STATS_EVT;
      break;
    case LHCI_OPCODE_VS_GET_CONTEXT_SIZES:
      evtParamLen += LHCI_LEN_GET_CONTEXT_SIZES_EVT;
      break;
    case LHCI_OPCODE_VS_GET_TEST_STATS:
      evtParamLen += sizeof(BbBleDataPktStats_t);
      break;
    case LHCI_OPCODE_VS_GET_POOL_STATS:
#if WSF_BUF_STATS == TRUE
      evtParamLen += LHCI_LEN_GET_POOL_STATS_EVT(WSF_MIN(WsfBufGetNumPool(), LHCI_MAX_POOL));
#else
      status = HCI_ERR_CMD_DISALLOWED;
#endif
      break;
    case LHCI_OPCODE_VS_GET_PDU_FILT_STATS:
      evtParamLen += sizeof(BbBlePduFiltStats_t);
      break;

    /* --- reset --- */

    case LHCI_OPCODE_VS_SYSTEM_RESET:
      lhciResetPending = TRUE;
      break;

    case LHCI_OPCODE_VS_ENTER_BOOTLOADER:
      lhciEnterBootloaderPending = TRUE;
      break;

    /* --- default --- */

    default:
      return FALSE;       /* exit dispatcher routine */
  }

  uint8_t *pEvtBuf;

  /* Encode and send command complete event packet. */
  if ((pEvtBuf = lhciAllocCmdCmplEvt(evtParamLen, pHdr->opCode)) != NULL)
  {
    pBuf  = pEvtBuf;
    pBuf += lhciPackCmdCompleteEvtStatus(pBuf, status);

    switch (pHdr->opCode)
    {
      /* --- extended device commands --- */

      case LHCI_OPCODE_VS_SET_OP_FLAGS:
      case LHCI_OPCODE_VS_SET_EVENT_MASK:
      case LHCI_OPCODE_VS_SET_BD_ADDR:
      case LHCI_OPCODE_VS_GET_RAND_ADDR:
      case LHCI_OPCODE_VS_SET_TX_TEST_ERR_PATT:
      case LHCI_OPCODE_VS_SET_SNIFFER_ENABLE:
        /* no action */
        break;

      case LHCI_OPCODE_VS_SET_LOCAL_FEAT:
      case LHCI_OPCODE_VS_SET_DIAG_MODE:
        /* no action */
        break;

      case LHCI_OPCODE_VS_GET_SYS_STATS:
      {
        uint16_t stackWatermark = PalSysGetStackUsage();
        UINT16_TO_BSTREAM(pBuf, stackWatermark);
        UINT16_TO_BSTREAM(pBuf, PalSysGetAssertCount());

        uint32_t freeMem = WsfHeapCountAvailable();
        UINT32_TO_BSTREAM(pBuf, freeMem);
        uint32_t usedMem = WsfHeapCountUsed();
        UINT32_TO_BSTREAM(pBuf, usedMem);

        uint16_t connMax = 0;
        uint16_t connCtxSize;
        LlGetConnContextSize((uint8_t *)&connMax, &connCtxSize);
        UINT16_TO_BSTREAM(pBuf, connMax);
        UINT16_TO_BSTREAM(pBuf, connCtxSize);

        #if (WSF_CS_STATS == TRUE)
          uint16_t csWatermarkUsec = WsfCsStatsGetCsWaterMark();
        #else
          uint16_t csWatermarkUsec = 0;
        #endif
        UINT16_TO_BSTREAM(pBuf, csWatermarkUsec);

        uint16_t llHandlerWatermarkUsec = LlStatsGetHandlerWatermarkUsec();
        UINT16_TO_BSTREAM(pBuf, llHandlerWatermarkUsec);
        uint16_t schHandlerWatermarkUsec = SchStatsGetHandlerWatermarkUsec();
        UINT16_TO_BSTREAM(pBuf, schHandlerWatermarkUsec);
        UINT16_TO_BSTREAM(pBuf, lhciHandlerWatermarkUsec);

        break;
      }
      case LHCI_OPCODE_VS_GET_CONTEXT_SIZES:
      {
        uint16_t advSetMax = 0;
        uint16_t advSetCtxSize;
        LlGetAdvSetContextSize((uint8_t *)&advSetMax, &advSetCtxSize);
        UINT16_TO_BSTREAM(pBuf, advSetMax);
        UINT16_TO_BSTREAM(pBuf, advSetCtxSize);

        uint16_t extScanMax = 0;
        uint16_t extScanCtxSize;
        LlGetExtScanContextSize((uint8_t *)&extScanMax, &extScanCtxSize);
        UINT16_TO_BSTREAM(pBuf, extScanMax);
        UINT16_TO_BSTREAM(pBuf, extScanCtxSize);

        uint16_t extInitMax = 0;
        uint16_t extInitCtxSize;
        LlGetExtInitContextSize((uint8_t *)&extInitMax, &extInitCtxSize);
        UINT16_TO_BSTREAM(pBuf, extInitMax);
        UINT16_TO_BSTREAM(pBuf, extInitCtxSize);

        uint16_t perScanMax = 0;
        uint16_t perScanCtxSize;
        LlGetPerScanContextSize((uint8_t *)&perScanMax, &perScanCtxSize);
        UINT16_TO_BSTREAM(pBuf, perScanMax);
        UINT16_TO_BSTREAM(pBuf, perScanCtxSize);

        uint8_t cigMax = 0;
        uint16_t cigCtxSize;
        uint8_t cisMax = 0;
        uint16_t cisCtxSize;
        LlGetCisContextSize(&cigMax, &cigCtxSize, &cisMax, &cisCtxSize);
        UINT16_TO_BSTREAM(pBuf, cigMax);
        UINT16_TO_BSTREAM(pBuf, cigCtxSize);
        UINT16_TO_BSTREAM(pBuf, cisMax);
        UINT16_TO_BSTREAM(pBuf, cisCtxSize);

        uint8_t bigMax = 0;
        uint16_t bigCtxSize;
        uint8_t bisMax = 0;
        uint16_t bisCtxSize;
        LlGetBisContextSize(&bigMax, &bigCtxSize, &bisMax, &bisCtxSize);
        UINT16_TO_BSTREAM(pBuf, bigMax);
        UINT16_TO_BSTREAM(pBuf, bigCtxSize);
        UINT16_TO_BSTREAM(pBuf, bisMax);
        UINT16_TO_BSTREAM(pBuf, bisCtxSize);

        break;
      }
      case LHCI_OPCODE_VS_GET_TEST_STATS:
      {
        BbBleDataPktStats_t stats;
        BbBleGetTestStats(&stats);
        memcpy(pBuf, (uint8_t *)&stats, sizeof(stats));
        break;
      }
#if WSF_BUF_STATS == TRUE
      case LHCI_OPCODE_VS_GET_POOL_STATS:
      {
        unsigned int i;
        WsfBufPoolStat_t stats;
        const uint8_t numPool = WSF_MIN(WsfBufGetNumPool(), LHCI_MAX_POOL);

        UINT8_TO_BSTREAM(pBuf, numPool);

        for (i = 0; i < numPool; i++)
        {
          WsfBufGetPoolStats(&stats, i);
          UINT16_TO_BSTREAM(pBuf, stats.bufSize);
          UINT8_TO_BSTREAM (pBuf, stats.numBuf);
          UINT8_TO_BSTREAM (pBuf, stats.numAlloc);
          UINT8_TO_BSTREAM (pBuf, stats.maxAlloc);
          UINT16_TO_BSTREAM (pBuf, stats.maxReqLen);
        }
        break;
      }
#endif
      case LHCI_OPCODE_VS_GET_PDU_FILT_STATS:
      {
        BbBlePduFiltStats_t stats;
        BbBleGetPduFiltStats(&stats);
        memcpy(pBuf, (uint8_t *)&stats, sizeof(stats));
        break;
      }

      /* --- default --- */

      default:
        break;
    }

    lhciSendCmdCmplEvt(pEvtBuf);
  }

  return TRUE;
}
