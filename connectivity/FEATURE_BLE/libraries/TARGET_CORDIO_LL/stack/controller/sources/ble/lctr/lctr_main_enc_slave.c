/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave encryption implementation file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_conn.h"
#include "lctr_int_conn_slave.h"
#include "lctr_int_enc_slave.h"
#include "lmgr_api.h"
#include "wsf_assert.h"
#include "wsf_trace.h"

/*************************************************************************************************/
/*!
 *  \brief      Process a received data channel PDU in slave role.
 *
 *  \param      pCtx    Connection context.
 *  \param      pBuf    PDU buffer.
 */
/*************************************************************************************************/
static void lctrSlvEncProcessDataPdu(lctrConnCtx_t *pCtx, uint8_t *pBuf)
{
  if (pCtx->enabled)
  {
    uint8_t ctrlResult, encResult;

    if (((ctrlResult = lctrDecodeCtrlPdu(&lctrDataPdu, pBuf, pCtx->role)) == LL_SUCCESS) ||
        ((encResult = lctrDecodeEncPdu(&lctrDataPdu, pBuf, pCtx->role)) == LL_SUCCESS))
    {
#if (LL_ENABLE_TESTER)
      if ((llTesterCb.rxLlcpFilter & (1 << lctrDataPdu.opcode)) != 0)
      {
        return;
      }
#endif
      lctrSlvConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP);
    }
    else if ((ctrlResult == LL_ERROR_CODE_INVALID_LMP_PARAMS) ||
             (encResult == LL_ERROR_CODE_INVALID_LMP_PARAMS))
    {
      lctrSlvConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP_INVALID_PARAM);
    }
    else
    {
      lctrSlvConnExecuteSm(pCtx, LCTR_CONN_MSG_RX_LLCP_UNKNOWN);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for connectable encrypted slave.
 */
/*************************************************************************************************/
void LctrSlvConnEncInit(void)
{
  /* Add LLCP SM handler. */
  lctrSlvLlcpSmTbl[LCTR_LLCP_SM_ENCRYPT] = lctrSlvExecuteEncryptSm;
  lctrSlvLlcpSmTbl[LCTR_LLCP_SM_PING]    = lctrExecutePingSm;

  /* Add control PDU handler. */
  if (!lctrCtrlPduHdlr)
  {
    lctrCtrlPduHdlr = lctrSlvEncProcessDataPdu;
  }

  /* Add packet encryption handlers. */
#if (!(BB_ENABLE_INLINE_ENC_TX && BB_ENABLE_INLINE_DEC_RX))
  lctrInitCipherBlkHdlr = PalCryptoAesEnable;
#endif
#if (!BB_ENABLE_INLINE_ENC_TX)
  lctrPktEncryptHdlr = PalCryptoAesCcmEncrypt;
#else
  lctrSetEncryptPktCountHdlr = PalBbBleSetInlineEncryptPacketCount;
#endif
#if (!BB_ENABLE_INLINE_DEC_RX)
  lctrPktDecryptHdlr = PalCryptoAesCcmDecrypt;
#else
  lctrSetDecryptPktCountHdlr = PalBbBleSetInlineDecryptPacketCount;
#endif

  /* Set supported features. */
  lmgrPersistCb.featuresDefault |= LL_FEAT_ENCRYPTION;
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_4_1)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_LE_PING;
  }

  LctrSlvConnInit();
}

/*************************************************************************************************/
/*!
 *  \brief  Set a new authentication payload timeout.
 *
 *  \param  handle      Connection handle.
 *
 *  \return Timeout value in milliseconds.
 */
/*************************************************************************************************/
uint32_t LctrGetAuthPayloadTimeout(uint16_t handle)
{
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  return pCtx->authTimeoutMs;
}

/*************************************************************************************************/
/*!
 *  \brief  Set a new authentication payload timeout.
 *
 *  \param  handle      Connection handle.
 *  \param  timeoutMs   New timeout value in milliseconds.
 *
 *  \return TRUE if successful, FALSE if not.
 */
/*************************************************************************************************/
bool_t LctrSetAuthPayloadTimeout(uint16_t handle, uint32_t timeoutMs)
{
  lctrConnCtx_t *pCtx = LCTR_GET_CONN_CTX(handle);

  uint32_t pingPeriodMs = lctrCalcPingPeriodMs(pCtx, timeoutMs);

  /* Check parameters. */
  if (pingPeriodMs == timeoutMs)
  {
    LL_TRACE_WARN1("!!! Impractical authentication payload timeoutMs=%u for given connection interval", timeoutMs);
    return FALSE;
  }

  /* Modify operating parameters. */
  pCtx->authTimeoutMs = timeoutMs;
  pCtx->pingPeriodMs  = pingPeriodMs;

  /* Encryption started? */
  if (pCtx->bleData.chan.enc.enaDecrypt)
  {
    /* Reset authentication payload timer. */
    WsfTimerStartMs(&pCtx->tmrAuthTimeout, pCtx->authTimeoutMs);
    WsfTimerStartMs(&pCtx->tmrPingTimeout, pCtx->pingPeriodMs);
  }

  return TRUE;
}
