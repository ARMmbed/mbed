/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer (LL) connection interface implementation file.
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

#include "ll_api.h"
#include "lctr_api_conn.h"
#include "lmgr_api.h"
#include "lmgr_api_conn.h"
#include "bb_ble_api.h"
#include "pal_radio.h"
#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief      Get RSSI of a connection.
 *
 *  \param      handle          Connection handle.
 *  \param      pRssi           RSSI value.
 *
 *  \return     Status error code.
 *
 *  Get the current RSSI of a connection.
 */
/*************************************************************************************************/
uint8_t LlGetRssi(uint16_t handle, int8_t *pRssi)
{
  uint8_t result = LL_SUCCESS;

  WSF_CS_INIT(cs);
  WSF_CS_ENTER(cs);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    *pRssi = LL_RSSI_NOT_AVAIL;
    result = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }
  else
  {
    *pRssi = LctrGetRssi(handle);
  }

  WSF_CS_EXIT(cs);

  LL_TRACE_INFO2("### LlApi ###  LlGetRssi, handle=%u, rssi=%d", handle, *pRssi);

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Get connection's TX power level.
 *
 *  \param      handle          Connection handle.
 *  \param      type            Power level type.
 *  \param      pLevel          Transmit power level.
 *
 *  \return     Status error code.
 *
 *  Get the TX power of a connection.
 */
/*************************************************************************************************/
uint8_t LlGetTxPowerLevel(uint16_t handle, uint8_t type, int8_t *pLevel)
{
  uint8_t result = LL_SUCCESS;

  WSF_CS_INIT(cs);
  WSF_CS_ENTER(cs);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    *pLevel = LL_MIN_TX_PWR_LVL;
    result = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }
  else if ((LL_API_PARAM_CHECK == 1) &&
          (type > LL_TX_PWR_LVL_TYPE_MAX))
  {
    *pLevel = LL_MIN_TX_PWR_LVL;
    result = LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }
  else
  {
    switch (type)
    {
      case LL_TX_PWR_LVL_TYPE_CURRENT:
        *pLevel = LctrGetTxPowerLevel(handle);
        break;
      case LL_TX_PWR_LVL_TYPE_MAX:
      default:
        *pLevel = LL_MAX_TX_PWR_LVL;
        break;
    }
  }

  WSF_CS_EXIT(cs);

  LL_TRACE_INFO3("### LlApi ###  LlGetTxPowerLevel, handle=%u, type, level=%d", handle, type, *pLevel);

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Get connection's enhanced TX power level and max txPower.
 *
 *  \param      handle          Connection handle.
 *  \param      phy             PHY.
 *  \param      pCurPwr         Current transmit power level.
 *  \param      pMaxPwr         Max power level.
 *
 *  \return     Status error code.
 *
 */
/*************************************************************************************************/
uint8_t LlGetEnhTxPowerLevel(uint16_t handle, uint8_t phy, int8_t *pCurPwr, int8_t *pMaxPwr)
{
  uint8_t result = LL_SUCCESS;

  WSF_CS_INIT(cs);
  WSF_CS_ENTER(cs);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    result = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }
  else if ((phy <= LL_PHY_NONE) || (phy > LL_PC_PHY_TOTAL))
  {
    result = LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }
  else
  {
    int8_t minPwr; /* Used for getting max power only .*/
    PalRadioGetSupTxPower(&minPwr, pMaxPwr);
    *pCurPwr = PalRadioGetActualTxPower(LctrGetPhyTxPowerLevel(handle, phy), FALSE);
  }

  WSF_CS_EXIT(cs);

  LL_TRACE_INFO3("### LlApi ###  LlGetEnhancedTxPowerLevel, handle=%u, phy=%u, curPower=%d", handle, phy, *pCurPwr);

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Set connection's TX power level (All PHYs).
 *
 *  \param      handle          Connection handle.
 *  \param      level           Transmit power level.
 *
 *  \return     Status error code.
 *
 *  Set the TX power of a connection (All PHYs).
 */
/*************************************************************************************************/
uint8_t LlSetAllPhyTxPowerLevel(uint16_t handle, int8_t level)
{
  uint8_t result = LL_SUCCESS;

  LL_TRACE_INFO2("### LlApi ###  LlSetTxPowerLevel, handle=%u, level=%d", handle, level);

  WSF_CS_INIT(cs);
  WSF_CS_ENTER(cs);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    result = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }
  else
  {
    LctrSetTxPowerLevel(handle, level);
  }

  WSF_CS_EXIT(cs);

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Set connection's TX power level for a PHY.
 *
 *  \param      handle          Connection handle.
 *  \param      level           Transmit power level.
 *
 *  \return     Status error code.
 *
 *  Set the TX power of a connection for a PHY.
 */
/*************************************************************************************************/
uint8_t LlSetPhyTxPowerLevel(uint16_t handle, int8_t level, uint8_t phy)
{
  uint8_t result = LL_SUCCESS;

  LL_TRACE_INFO3("### LlApi ###  LlSetPhyTxPowerLevel, handle=%u, level=%d, phy=%u", handle, level, phy);

  WSF_CS_INIT(cs);
  WSF_CS_ENTER(cs);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    result = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }
  else if (phy > LL_PC_PHY_TOTAL)
  {
    result = LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }
  else
  {
    LctrSetPhyTxPowerLevel(handle, level, phy);
  }

  WSF_CS_EXIT(cs);

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Get connection's channel map.
 *
 *  \param      handle          Connection handle.
 *  \param      pChanMap        Channel map.
 *
 *  \return     Status error code.
 *
 *  Get the current channel map of a connection.
 */
/*************************************************************************************************/
uint8_t LlGetChannelMap(uint16_t handle, uint8_t *pChanMap)
{
  uint64_t chanMap;
  uint8_t result = LL_SUCCESS;

  LL_TRACE_INFO1("### LlApi ###  LlGetChannelMap, handle=%u", handle);

  WSF_CS_INIT(cs);
  WSF_CS_ENTER(cs);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    result = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }
  else
  {
    chanMap = LctrGetChannelMap(handle);
    UINT40_TO_BSTREAM(pChanMap, chanMap);
  }

  WSF_CS_EXIT(cs);

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Set connection operational mode flags.
 *
 *  \param      flags   Flags.
 *  \param      enable  TRUE to set flags or FALSE to clear flags.
 *
 *  \return     Status error code.
 *
 *  Set mode flags governing LL operations of a given connection.
 */
/*************************************************************************************************/
uint8_t LlSetConnOpFlags(uint16_t handle, uint32_t flags, bool_t enable)
{
  const uint32_t flagAll =
    LL_OP_MODE_FLAG_SLV_REQ_IMMED_ACK |
    LL_OP_MODE_FLAG_BYPASS_CE_GUARD |
    LL_OP_MODE_FLAG_MST_RETX_AFTER_RX_NACK |
    LL_OP_MODE_FLAG_MST_IGNORE_CP_RSP |
    LL_OP_MODE_FLAG_MST_UNCOND_CP_RSP |
    LL_OP_MODE_FLAG_REQ_SYM_PHY |
    LL_OP_MODE_FLAG_ENA_SLV_LATENCY_WAKEUP |
    LL_OP_MODE_FLAG_ENA_WW |
    LL_OP_MODE_FLAG_ENA_SLV_LATENCY |
    LL_OP_MODE_FLAG_ENA_LLCP_TIMER |
    LL_OP_MODE_FLAG_IGNORE_CRC_ERR_TS |
    LL_OP_MODE_FLAG_ENA_SUBRATE_CE;

  LL_TRACE_INFO2("### LlApi ###  LlSetConnFlags flag=%x enable=%d", flags, enable);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((~flagAll & flags) != 0))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  LctrSetConnOpFlags(handle, flags, enable);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Disconnect a connection.
 *
 *  \param      handle          Connection handle.
 *  \param      reason          Disconnect reason.
 *
 *  \return     Status error code.
 *
 *  Disconnect a connection.
 */
/*************************************************************************************************/
uint8_t LlDisconnect(uint16_t handle, uint8_t reason)
{
  lctrDisconnect_t *pMsg;

  LL_TRACE_INFO2("### LlApi ###  LlDisconnect, handle=%u, reason=%u", handle, reason);

  if ((LL_API_PARAM_CHECK == 1) &&
       (handle >= (LCTR_MAX_HANDLE_INDEX)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (handle < pLctrRtCfg->maxConn))
  {
    /* Handle for ACL is not enabled. */
    if (!LctrIsConnHandleEnabled(handle))
    {
      return LL_ERROR_CODE_UNKNOWN_CONN_ID;
    }
  }

  if (handle < pLctrRtCfg->maxConn)
  {
    /* ACL handle */
    if ((pMsg = (lctrDisconnect_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
    {
      pMsg->hdr.handle = handle;
      pMsg->hdr.dispId = LCTR_DISP_CONN;
      pMsg->hdr.event  = LCTR_CONN_MSG_API_DISCONNECT;
      pMsg->reason     = reason;

      /* Explicitly define a non-cis handle to avoid disconnecting CIS. */
      ((lctrCisDisc_t *) pMsg)->cisHandle = handle;

      WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
    }
  }
  else
  {
    /* CIS handle */

    /* Return error if CIS is not established. */
    if (LctrIsCisConnHandleEnabled(handle) == FALSE)
    {
      return LL_ERROR_CODE_CMD_DISALLOWED;
    }

    if ((pMsg = (lctrDisconnect_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
    {
      pMsg->hdr.handle = handle;
      pMsg->hdr.dispId = LCTR_DISP_CIS;
      pMsg->hdr.event  = LCTR_CONN_MSG_API_DISCONNECT;
      pMsg->reason     = reason;

      WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
    }
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Update connection parameters.
 *
 *  \param      handle          Connection handle.
 *  \param      pConnSpec       New connection specification.
 *
 *  \return     Status error code.
 *
 *  Update the connection parameters of a connection.
 */
/*************************************************************************************************/
uint8_t LlConnUpdate(uint16_t handle, const LlConnSpec_t *pConnSpec)
{
  lctrConnUpdate_t *pMsg;
  uint8_t status;

  WSF_ASSERT(pConnSpec);                    /* not NULL */

  LL_TRACE_INFO1("### LlApi ###  LlConnUpdate, handle=%u", handle);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  if (LctrIsProcActPended(handle, LCTR_CONN_MSG_API_CONN_UPDATE) == TRUE)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((LctrGetRole(handle) == LL_ROLE_SLAVE) &&
       ((lmgrCb.features & LL_FEAT_CONN_PARAM_REQ_PROC) == 0)))
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((status = LctrValidateConnSpec(pConnSpec)) != LL_SUCCESS))
  {
    return status;
  }

  if ((pMsg = (lctrConnUpdate_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.handle = handle;
    pMsg->hdr.dispId = LCTR_DISP_CONN;
    pMsg->hdr.event  = LCTR_CONN_MSG_API_CONN_UPDATE;

    pMsg->connSpec = *pConnSpec;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Remote connection parameter request reply.
 *
 *  \param      handle          Connection handle.
 *  \param      pConnSpec       New connection specification.
 *
 *  \return     Status error code.
 *
 *  Reply to a connection parameter request.
 */
/*************************************************************************************************/
uint8_t LlRemoteConnParamReqReply(uint16_t handle, const LlConnSpec_t *pConnSpec)
{
  lctrConnParamReply_t *pMsg;
  uint8_t status;

  WSF_ASSERT(pConnSpec);                    /* not NULL */

  LL_TRACE_INFO1("### LlApi ###  LlRemoteConnParamReqReply, handle=%u", handle);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      !LctrIsWaitingForReply(handle, LCTR_HOST_REPLY_CONN_PARAM_REQ))
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((status = LctrValidateConnSpec(pConnSpec)) != LL_SUCCESS))
  {
    return status;
  }

  if ((pMsg = (lctrConnParamReply_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.handle = handle;
    pMsg->hdr.dispId = LCTR_DISP_CONN;
    pMsg->hdr.event  = LCTR_CONN_MSG_API_CONN_PARAM_REPLY;

    pMsg->connSpec = *pConnSpec;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Remote connection parameter request negative reply.
 *
 *  \param      handle          Connection handle.
 *  \param      reason          Reason code.
 *
 *  \return  Status error code.
 *
 *  Negative reply to a connection parameter request.
 */
/*************************************************************************************************/
uint8_t LlRemoteConnParamReqNegReply(uint16_t handle, uint8_t reason)
{
  lctrConnParamNegReply_t *pMsg;

  LL_TRACE_INFO2("### LlApi ###  LlRemoteConnParamReqNegReply, handle=%u, reason=%u", handle, reason);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      !LctrIsWaitingForReply(handle, LCTR_HOST_REPLY_CONN_PARAM_REQ))
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((pMsg = (lctrConnParamNegReply_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.handle = handle;
    pMsg->hdr.dispId = LCTR_DISP_CONN;
    pMsg->hdr.event  = LCTR_CONN_MSG_API_CONN_PARAM_NEG_REPLY;

    pMsg->reason = reason;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Read remote version information.
 *
 *  \param      handle          Connection handle.
 *
 *  \return     Status error code.
 *
 *  Read the version information of the remote device.
 */
/*************************************************************************************************/
uint8_t LlReadRemoteVerInfo(uint16_t handle)
{
  lctrMsgHdr_t *pMsg;

  LL_TRACE_INFO1("### LlApi ###  LlReadRemoteVerInfo, handle=%u", handle);


  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  if (LctrIsProcActPended(handle, LCTR_CONN_MSG_API_REMOTE_VERSION) == TRUE)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((pMsg = (lctrMsgHdr_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->handle = handle;
    pMsg->dispId = LCTR_DISP_CONN;
    pMsg->event  = LCTR_CONN_MSG_API_REMOTE_VERSION;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set data length.
 *
 *  \param      handle          Connection handle.
 *  \param      txLen           Maximum number of payload bytes for a Data PDU
 *  \param      txTime          Maximum microseconds for a Data PDU
 *
 *  \return     Status error code.
 *
 *  Preferred maximum microseconds that the local Controller should use to transmit a
 *  single Link Layer Data Channel PDU.
 */
/*************************************************************************************************/
uint8_t LlSetDataLen(uint16_t handle, uint16_t txLen, uint16_t txTime)
{
  LL_TRACE_INFO1("### LlApi ###  LlSetDataLen: handle=%u", handle);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  if (LctrIsProcActPended(handle, LCTR_CONN_MSG_API_DATA_LEN_CHANGE) == TRUE)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((txLen < LL_MAX_DATA_LEN_MIN) ||
       (txLen > LL_MAX_DATA_LEN_ABS_MAX) ||
       (txTime < LL_MAX_DATA_TIME_MIN) ||
       ((pLctrRtCfg->btVer <= LL_VER_BT_CORE_SPEC_4_2) && (txTime > LL_MAX_DATA_TIME_ABS_MAX_1M)) ||
       ((pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_0) && (txTime > LL_MAX_DATA_TIME_ABS_MAX))))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  lctrDataLengthChange_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.handle = handle;
    pMsg->hdr.dispId = LCTR_DISP_CONN;
    pMsg->hdr.event = LCTR_CONN_MSG_API_DATA_LEN_CHANGE;

    pMsg->maxTxLen = WSF_MIN(txLen, LCTR_MAX_DATA_LEN_MAX);
    pMsg->maxTxTime = txTime;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Read default data length.
 *
 *  \param      pMaxTxLen       Maximum number of payload bytes for a Data PDU
 *  \param      pMaxTxTime      Maximum microseconds for a Data PDU
 *
 *  Suggested length and microseconds that the local Controller should use to transmit a
 *  single Link Layer Data Channel PDU.
 */
/*************************************************************************************************/
void LlReadDefaultDataLen(uint16_t *pMaxTxLen, uint16_t *pMaxTxTime)
{
  LL_TRACE_INFO0("### LlApi ###  LlReadDefaultDataLen");

  WSF_ASSERT(pMaxTxLen);
  WSF_ASSERT(pMaxTxTime);

  *pMaxTxLen = lmgrConnCb.maxTxLen;
  *pMaxTxTime = lmgrConnCb.maxTxTime;
}

/*************************************************************************************************/
/*!
 *  \brief      Write default data length.
 *
 *  \param      maxTxLen        Maximum number of payload bytes for a Data PDU
 *  \param      maxTxTime       Maximum microseconds for a Data PDU
 *
 *  \return     Status error code.
 *
 *  Suggested length and microseconds that the local Controller should use to transmit a
 *  single Link Layer Data Channel PDU.
 */
/*************************************************************************************************/
uint8_t LlWriteDefaultDataLen(uint16_t maxTxLen, uint16_t maxTxTime)
{
  LL_TRACE_INFO0("### LlApi ###  LlWriteDefaultDataLen");

  if ((LL_API_PARAM_CHECK == 1) &&
      ((maxTxLen < LL_MAX_DATA_LEN_MIN) ||
       (maxTxLen > LL_MAX_DATA_LEN_ABS_MAX) ||
       (maxTxTime < LL_MAX_DATA_TIME_MIN) ||
       ((pLctrRtCfg->btVer <= LL_VER_BT_CORE_SPEC_4_2) && (maxTxTime > LL_MAX_DATA_TIME_ABS_MAX_1M)) ||
       ((pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_0) && (maxTxTime > LL_MAX_DATA_TIME_ABS_MAX))))
  {
    return LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (maxTxLen > WSF_MIN(pLctrRtCfg->maxAclLen, LCTR_MAX_DATA_LEN_MAX)))
  {
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }

  lmgrConnCb.maxTxLen = maxTxLen;
  lmgrConnCb.maxTxTime = maxTxTime;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Read maximum data length.
 *
 *  \param      pMaxTxLen       Maximum number of payload bytes for a Tx Data PDU
 *  \param      pMaxTxTime      Maximum microseconds for a Tx Data PDU
 *  \param      pMaxRxLen       Maximum number of payload bytes for a Rx Data PDU
 *  \param      pMaxRxTime      Maximum microseconds for a Rx Data PDU
 *
 *  Read the Controller's maximum supported payload octets and packet duration times for
 *  transmission and reception.
 */
/*************************************************************************************************/
void LlReadMaximumDataLen(uint16_t *pMaxTxLen, uint16_t *pMaxTxTime, uint16_t *pMaxRxLen, uint16_t *pMaxRxTime)
{
  LL_TRACE_INFO0("### LlApi ###  LlReadMaximumDataLen");

  WSF_ASSERT(pMaxTxLen);
  WSF_ASSERT(pMaxTxTime);

  *pMaxTxLen  = WSF_MIN(pLctrRtCfg->maxAclLen, LCTR_MAX_DATA_LEN_MAX);
  *pMaxTxTime = LL_DATA_LEN_TO_TIME_1M(*pMaxTxLen, TRUE);
  *pMaxRxLen  = *pMaxTxLen;
  *pMaxRxTime = *pMaxTxTime;
}

/*************************************************************************************************/
/*!
 *  \brief      Set local minimum number of used channels.
 *
 *  \param      phys            Bitmask for the PHYs.
 *  \param      pwrThres        Power threshold.
 *  \param      minUsedCh       Minimum number of used channels.
 *
 *  \return     Status error code.
 *
 *  Set local minimum number of used channels.
 */
/*************************************************************************************************/
uint8_t LlSetLocalMinUsedChan(uint8_t phys, int8_t pwrThres, uint8_t minUsedCh)
{
  lctrSetMinUsedChan_t *pMsg;

  LL_TRACE_INFO0("### LlApi ###  LlSetLocalMinUsedChan");

  if ((LL_API_PARAM_CHECK == 1) &&
      ((lmgrCb.features & LL_FEAT_MIN_NUM_USED_CHAN) == 0))
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }
  else if ((LL_API_PARAM_CHECK == 1) &&
           ((phys == 0) ||
            (phys >= (1 << LL_MAX_PHYS)) ||
            (minUsedCh > LL_MAX_NUM_CHAN_DATA) ||
            (minUsedCh < LL_MIN_NUM_CHAN_DATA)))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  /* Save the local minimum number of channels for the PHYs. */
  for (unsigned i = 0; i < LL_MAX_PHYS; i++)
  {
    if ((phys & (1 << i)) == (1 << i))
    {
      lmgrCb.powerThreshold[i] = pwrThres;
      lmgrCb.localMinUsedChan[i] = minUsedCh;
    }
  }
  uint16_t handle;

  /* If slave, start the LL_SET_MIN_USED_CHAN procedure to inform the peer localMinUsedChan. */
  for (handle = 0; handle < pLctrRtCfg->maxConn; handle++)
  {
    if ((LctrIsConnHandleEnabled(handle)) &&
        (LctrGetRole(handle) == LL_ROLE_SLAVE))
    {
      if (LctrIsProcActPended(handle, LCTR_CONN_MSG_API_SET_MIN_USED_CHAN) == TRUE)
      {
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }

      if ((pMsg = (lctrSetMinUsedChan_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        pMsg->hdr.handle = handle;
        pMsg->hdr.dispId = LCTR_DISP_CONN;
        pMsg->hdr.event  = LCTR_CONN_MSG_API_SET_MIN_USED_CHAN;
        pMsg->phys = phys;
        pMsg->minUsedChan = minUsedCh;

        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
      }
    }
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Get peer minimum number of used channels.
 *
 *  \param      handle              Connection handle.
 *  \param      pPeerMinUsedChan    Storage for the peer minimum number of used channels.
 *
 *  \return     Status error code.
 *
 *  Get peer minimum number of used channels.
 */
/*************************************************************************************************/
uint8_t LlGetPeerMinUsedChan(uint16_t handle, uint8_t *pPeerMinUsedChan)
{
  uint8_t result = LL_SUCCESS;

  LL_TRACE_INFO1("### LlApi ###  LlGetPeerMinUsedChan, handle=%u", handle);

  WSF_CS_INIT(cs);
  WSF_CS_ENTER(cs);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
        !LctrIsConnHandleEnabled(handle)))
  {
    result = LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }
  else if ((LL_API_PARAM_CHECK == 1) &&
           (((lmgrCb.features & LL_FEAT_MIN_NUM_USED_CHAN) == 0) ||
            (LctrGetRole(handle) != LL_ROLE_MASTER)))
  {
    result = LL_ERROR_CODE_CMD_DISALLOWED;
  }
  else
  {
    LctrGetPeerMinUsedChan(handle, pPeerMinUsedChan);
  }

  WSF_CS_EXIT(cs);

  return result;
}

/*************************************************************************************************/
/*!
 *  \brief      Get the maximum ACL buffers size.
 *
 *  \return     Maximum buffers size in bytes.
 */
/*************************************************************************************************/
uint16_t LlGetAclMaxSize(void)
{
  LL_TRACE_INFO1("### LlApi ###  LlGetAclMaxSize, maxBufSize=%u", pLctrRtCfg->maxAclLen);

  return pLctrRtCfg->maxAclLen;
}

/*************************************************************************************************/
/*!
 *  \brief      Get the number of buffers in the LL ACL transmit queue.
 *
 *  \return     Number of buffers.
 */
/*************************************************************************************************/
uint8_t LlGetAclTxBufs(void)
{
  LL_TRACE_INFO1("### LlApi ###  LlGetAclTxBufs, numBufs=%u", pLctrRtCfg->numTxBufs);

  return pLctrRtCfg->numTxBufs;
}

/*************************************************************************************************/
/*!
 *  \brief      Get the number of buffers in the LL ACL receive queue.
 *
 *  \return     Number of buffers.
 */
/*************************************************************************************************/
uint8_t LlGetAclRxBufs(void)
{
  LL_TRACE_INFO1("### LlApi ###  LlGetAclRxBufs, numBufs=%u", pLctrRtCfg->numRxBufs);

  return pLctrRtCfg->numRxBufs;
}

/*************************************************************************************************/
/*!
 *  \brief      Send an ACL data packet.
 *
 *  \param      pData   Data buffer
 *
 *  Send an ACL data packet. pData points to an ACL buffer formatted according to [1]; the host
 *  must set the connection handle, flags, and length fields in the buffer.
 */
/*************************************************************************************************/
void LlSendAclData(uint8_t *pData)
{
  if (lmgrConnCb.availTxBuf == 0)
  {
    LL_TRACE_WARN0("!!! ACL flow control detected; dropping Tx data PDU");

    /* Drop packet due to out of Tx buffers condition. */
    WsfMsgFree(pData);
    return;
  }

  LctrTxAcl(pData);
}

/*************************************************************************************************/
/*!
 *  \brief      Receive an ACL data packet
 *
 *  \return     Data buffer.
 *
 *  Receive an ACL data packet.  This function returns a pointer to an ACL buffer formatted
 *  according to [1].  The host must parse the header to determine the connection handle, flags,
 *  and length fields.  If no ACL buffers are available this function returns NULL.
 *
 *  The host must deallocate the buffer by calling WsfMsgFree() and then calling
 *  LlRecvAclDataComplete().
 */
/*************************************************************************************************/
uint8_t *LlRecvAclData(void)
{
  return LctrRxAcl();
}

/*************************************************************************************************/
/*!
 *  \brief      Indicate that received ACL data buffer has been deallocated
 *
 *  \param      numBufs     Number of completed packets.
 *
 *  Indicate that received ACL data buffer has been deallocated.
 */
/*************************************************************************************************/
void LlRecvAclDataComplete(uint8_t numBufs)
{
  LctrRxAclComplete(numBufs);
}

/*************************************************************************************************/
/*!
 *  \brief      Used to read the sleep clock accuracy of the peer device.
 *
 *  \param      handle          Connection handle.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlRequestPeerSca(uint16_t handle)
{
  lctrScaReq_t *pMsg;

  LL_TRACE_INFO1("### LlApi ###  LlRequestPeerSca, handle=%u", handle);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }
  else if ((LL_API_PARAM_CHECK == 1) &&
           ((LctrGetUsedFeatures(handle) & LL_FEAT_SCA_UPDATE) == 0))
  {
    return LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE;
  }

  if ((pMsg = (lctrScaReq_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.handle = handle;
    pMsg->hdr.dispId = LCTR_DISP_CONN;
    pMsg->hdr.event  = LCTR_CONN_MSG_API_REQ_PEER_SCA;
    pMsg->action     = LL_MODIFY_SCA_NO_ACTION;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Modify the sleep clock accuracy
 *
 *  \param      action   Increase or decrease the sleep clock accuracy
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlModifySleepClockAccuracy(uint8_t action)
{
  LL_TRACE_INFO1("### LlApi ### LlRequestPeerSca, Action=%u", action);

  uint8_t status = LL_SUCCESS;
  lctrScaReq_t *pMsg;

  if ((LL_API_PARAM_CHECK == 1) &&
      (action > LL_MODIFY_SCA_LESS_ACCURATE))
  {
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  if ((status = LctrValidateModifyScaParam(action)) != LL_SUCCESS)
  {
    LL_TRACE_WARN1("Modify SCA failed. Status=%u", status);
  }
  else /* status = LL_SUCCESS */
  {
    /* Update lmgrCb sca for future connections. */
    switch (action)
    {
      case LL_MODIFY_SCA_MORE_ACCURATE:
        lmgrCb.scaMod++;
        break;
      case LL_MODIFY_SCA_LESS_ACCURATE:
        lmgrCb.scaMod--;
        break;

      default:
        break;
    }
  }

  /* We will send out the request even if we reach the limit,
   * In case we need to update the connection's sca with the lmgr's sca.
   */
  uint16_t handle;
  for (handle = 0; handle < pLctrRtCfg->maxConn; handle++)
  {
    if (LctrIsConnHandleEnabled(handle))
    {
      if ((LctrGetUsedFeatures(handle) & LL_FEAT_SCA_UPDATE) == 0)
      {
        continue;
      }

      if ((pMsg = (lctrScaReq_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        pMsg->hdr.handle = handle;
        pMsg->hdr.dispId = LCTR_DISP_CONN;
        pMsg->hdr.event  = LCTR_CONN_MSG_API_REQ_PEER_SCA;
        pMsg->action     = action;
        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
      }
    }
  }

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      Request change to or read peer txPower
 *
 *  \param      handle          Connection handle.
 *  \param      delta           Requested change.
 *  \param      phy             Phy this change requests the change on.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlPowerCtrlReq(uint16_t handle, int8_t delta, uint8_t phy)
{
  lctrMsgPwrCtrlReq_t *pMsg;

  LL_TRACE_INFO3("### LlApi ###  LlPowerCtrlReq, handle=%u, delta=%u, phy=%u", handle, delta, phy);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }
  if ((LL_API_PARAM_CHECK == 1) &&
      (phy > LL_PC_PHY_TOTAL))
  {
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }
  else if ((LL_API_PARAM_CHECK == 1) &&
           ((LctrGetUsedFeatures(handle) & LL_FEAT_POWER_CONTROL_REQUEST) == 0))
  {
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }

  if ((pMsg = (lctrMsgPwrCtrlReq_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.handle = handle;
    pMsg->hdr.dispId = LCTR_DISP_CONN;
    pMsg->hdr.event  = LCTR_CONN_MSG_API_PWR_CTRL_REQ;
    pMsg->delta      = delta;
    pMsg->phy        = phy;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
  else
  {
    return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set transmit power change reporting enable.
 *
 *  \param      handle          Connection handle.
 *  \param      enableLocal     Enable local reporting.
 *  \param      enableRemote    Enable remote reporting.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlSetTxPowerReporting(uint16_t handle, uint8_t enableLocal, uint8_t enableRemote)
{
  LL_TRACE_INFO3("### LlApi ###  LlSetTxPowerReporting, handle=%u, localEnable=%u, remoteEnable=%u", handle, enableLocal, enableRemote);

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  return lctrSetTxPowerReporting(handle, enableLocal, enableRemote);
}
