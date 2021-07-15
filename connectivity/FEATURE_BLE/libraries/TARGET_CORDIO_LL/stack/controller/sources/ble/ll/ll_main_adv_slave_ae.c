/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer (LL) master control interface implementation file.
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
#include "ll_math.h"
#include "lctr_api_adv_slave_ae.h"
#include "lctr_api_adv_master_ae.h"
#include "lmgr_api_adv_slave.h"
#include "lmgr_api_adv_slave_ae.h"
#include "bb_ble_api.h"
#include "bb_ble_api_op.h"
#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief      Set advertising set random device address.
 *
 *  \param      handle      Advertising set ID.
 *  \param      pAddr       Random Bluetooth device address.
 *
 *  \return     Status error code.
 *
 *  Set the random address to be used by a advertising set.
 */
/*************************************************************************************************/
uint8_t LlSetAdvSetRandAddr(uint8_t handle, const uint8_t *pAddr)
{
  LL_TRACE_INFO1("### LlApi ###  LlSetAdvSetRandAddr, handle=%u", handle);

  LL_TRACE_INFO3("Private BDA[5:3]=%02x:%02x:%02x", pAddr[5], pAddr[4], pAddr[3]);
  LL_TRACE_INFO3("        BDA[2:0]=%02x:%02x:%02x", pAddr[2], pAddr[1], pAddr[0]);

  WSF_ASSERT(pAddr);

  return LctrSetExtAdvSetRandAddr(handle, pAddr);
}

/*************************************************************************************************/
/*!
 *  \brief  Read periodic channel map for slave or master
 *
 *  \param  handle        handle.
 *  \param  isAdv         Handle is an advertiser.
 *
 *  \return Channel map, packed into a 64bit int
 *
 *   Note: Consider moving to ll_main_ae.c (as of now, no file for this exists).
 */
/*************************************************************************************************/
uint64_t LlGetPeriodicChanMap(uint16_t handle, bool_t isAdv)
{
  LL_TRACE_INFO2("### LlApi ###  LlGetPeriodicChanMap, handle=%u | isAdv=%u", handle, isAdv);

  return isAdv ? LctrGetPerAdvChanMap((uint8_t) handle) : LctrGetPerScanChanMap(handle);
}

/*************************************************************************************************/
/*!
 *  \brief      Get advertising set random device address.
 *
 *  \param      handle      Advertising set ID.
 *  \param      pAddr       Random Bluetooth device address.
 *
 *  \return     Status error code.
 *
 *  Get the random address to be used by a advertising set.
 */
/*************************************************************************************************/
uint8_t LlGetAdvSetRandAddr(uint8_t handle, uint8_t *pAddr)
{
  LL_TRACE_INFO1("### LlApi ###  LlGetAdvSetRandAddr, handle=%u", handle);

  WSF_ASSERT(pAddr);

  return LctrGetExtAdvSetRandAddr(handle, pAddr);
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended advertising parameters.
 *
 *  \param      handle          Advertising set ID.
 *  \param      pExtAdvParam    Extended advertising parameters.
 *
 *  \return     Status error code.
 *
 *  Set extended advertising parameters.
 *
 *  \note       This function must only be called when advertising for this set is disabled.
 */
/*************************************************************************************************/
uint8_t LlSetExtAdvParam(uint8_t handle, LlExtAdvParam_t *pExtAdvParam)
{
  const uint16_t validAdvEventProp = 0x007F;
  const uint32_t validAdvInterMin = 0x000020;
  const uint16_t highDutyConnDirectAdvProp = LL_ADV_EVT_PROP_LEGACY_ADV_BIT |
                                             LL_ADV_EVT_PROP_HIGH_DUTY_ADV_BIT |
                                             LL_ADV_EVT_PROP_DIRECT_ADV_BIT |
                                             LL_ADV_EVT_PROP_CONN_ADV_BIT;

  /* Maximum extended advertising interval is half the maximum BB clock rollover time. */
  const uint32_t bbDurMaxUsec = (BbGetBbTimerBoundaryUs() >> 1) + 1;
  const uint32_t validAdvInterMax = WSF_MIN(LL_MATH_DIV_625(bbDurMaxUsec), 0xFFFFFF);

  LL_TRACE_INFO2("### LlApi ###  LlSetExtAdvParam, handle=%u, advEventProp=0x%04x", handle, pExtAdvParam->advEventProp);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }
  if ((LL_API_PARAM_CHECK == 1) &&
      (handle > LL_MAX_ADV_HANDLE))
  {
    return LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE;
  }
  if ((LL_API_PARAM_CHECK == 1) &&
      ((pExtAdvParam->priAdvInterMax < pExtAdvParam->priAdvInterMin) ||
       /* Ignore unused intervals for high duty cycle connectable directed advertising. */
       ((pExtAdvParam->advEventProp != highDutyConnDirectAdvProp) &&
        ((pExtAdvParam->priAdvInterMin < validAdvInterMin) ||
         (pExtAdvParam->priAdvInterMax < validAdvInterMin)))))
  {
    LL_TRACE_WARN1("Invalid Advertising Interval values, handle=%u", handle);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }
  if ((LL_API_PARAM_CHECK == 1) &&
      /* Ignore unused intervals for high duty cycle connectable directed advertising. */
      ((pExtAdvParam->advEventProp != highDutyConnDirectAdvProp) &&
       ((pExtAdvParam->priAdvInterMin > validAdvInterMax) ||
        (pExtAdvParam->priAdvInterMax > validAdvInterMax))))
  {
    LL_TRACE_WARN1("Unsupported Advertising Interval values, handle=%u", handle);
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }
  if ((LL_API_PARAM_CHECK == 1) &&
      ((pExtAdvParam->advEventProp & ~validAdvEventProp) ||
       ((pExtAdvParam->advEventProp & (LL_ADV_EVT_PROP_LEGACY_ADV_BIT | LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT)) ==
           (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT)) ||
       ((pExtAdvParam->advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) &&
           (pExtAdvParam->advEventProp & (LL_ADV_EVT_PROP_OMIT_AA_BIT | LL_ADV_EVT_PROP_TX_PWR_BIT)))))
  {
    LL_TRACE_WARN1("Invalid Advertising Event Properties, handle=%u", handle);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }
  if ((LL_API_PARAM_CHECK == 1) &&
      ((pExtAdvParam->advEventProp & LL_ADV_EVT_PROP_OMIT_AA_BIT) &&
       (pExtAdvParam->advEventProp & (LL_ADV_EVT_PROP_CONN_ADV_BIT | LL_ADV_EVT_PROP_SCAN_ADV_BIT))))
  {
    LL_TRACE_WARN1("Only non-connectable and non-scannable advertising can do anonymous advertising, handle=%u", handle);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }
  if ((LL_API_PARAM_CHECK == 1) &&
      ((((pExtAdvParam->advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) != 0) &&
         (pExtAdvParam->priAdvPhy != LL_PHY_LE_1M)) ||
       (((pExtAdvParam->advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) == 0) &&
         (pExtAdvParam->priAdvPhy != LL_PHY_LE_1M) && (pExtAdvParam->priAdvPhy != LL_PHY_LE_CODED)) ||
       (((pExtAdvParam->advEventProp & LL_ADV_EVT_PROP_LEGACY_ADV_BIT) == 0) &&
         (pExtAdvParam->secAdvPhy != LL_PHY_LE_1M) && (pExtAdvParam->secAdvPhy != LL_PHY_LE_2M) && (pExtAdvParam->secAdvPhy != LL_PHY_LE_CODED))))
  {
    LL_TRACE_WARN1("Invalid PHY for legacy advertising, handle=%u", handle);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }
  if ((LL_API_PARAM_CHECK == 1) &&
      (((pExtAdvParam->secAdvPhy == LL_PHY_LE_2M) && ((lmgrCb.features & LL_FEAT_LE_2M_PHY) == 0)) ||
      (((pExtAdvParam->priAdvPhy == LL_PHY_LE_CODED) || (pExtAdvParam->secAdvPhy == LL_PHY_LE_CODED)) && ((lmgrCb.features & LL_FEAT_LE_CODED_PHY) == 0))))
  {
    LL_TRACE_WARN1("Unsupported PHY, handle=%u", handle);
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }

  return LctrSetExtAdvParam(handle, pExtAdvParam);
}

/*************************************************************************************************/
/*!
 *  \brief      Get extended advertising TX power level.
 *
 *  \param      handle          Advertising handle.
 *  \param      pLevel          Transmit power level.
 *
 *  \return     Status error code.
 *
 *  Get the TX power of an advertising set.
 */
/*************************************************************************************************/
uint8_t LlGetExtAdvTxPowerLevel(uint16_t handle, int8_t *pLevel)
{
  *pLevel = -127;
  return LctrGetExtAdvTxPowerLevel(handle, pLevel);
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended advertising data.
 *
 *  \param      handle      Advertising set ID.
 *  \param      op          Operation.
 *  \param      fragPref    Fragment preference.
 *  \param      len         Data buffer length.
 *  \param      pData       Advertising data buffer.
 *
 *  \return     Status error code.
 *
 *  Set extended advertising data data.
 */
/*************************************************************************************************/
uint8_t LlSetExtAdvData(uint8_t handle, uint8_t op, uint8_t fragPref, uint8_t len, const uint8_t *pData)
{
  LL_TRACE_INFO2("### LlApi ###  LlSetExtAdvData, handle=%u, len=%u", handle, len);

  WSF_ASSERT(pData);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  return LctrSetExtAdvData(handle, op, fragPref, len, pData);
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended scan response data.
 *
 *  \param      handle      Advertising set ID.
 *  \param      op          Operation.
 *  \param      fragPref    Fragment preference.
 *  \param      len         Data buffer length.
 *  \param      pData       Scan response data buffer.
 *
 *  \return     Status error code.
 *
 *  Set extended scan response data.
 */
/*************************************************************************************************/
uint8_t LlSetExtScanRespData(uint8_t handle, uint8_t op, uint8_t fragPref, uint8_t len, const uint8_t *pData)
{
  LL_TRACE_INFO2("### LlApi ###  LlSetExtScanRespData, handle=%u, len=%u", handle, len);

  WSF_ASSERT(pData);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  return LctrSetExtScanRespData(handle, op, fragPref, len, pData);
}

/*************************************************************************************************/
/*!
 *  \brief      Extended advertising enable.
 *
 *  \param      enable      Set to TRUE to enable advertising, FALSE to disable advertising.
 *  \param      numAdvSets  Number of elements in enaParam[].
 *  \param      enaParam    Enable parameter table.
 *
 *  Enable or disable extended advertising.
 */
/*************************************************************************************************/
void LlExtAdvEnable(uint8_t enable, uint8_t numAdvSets, LlExtAdvEnableParam_t enaParam[])
{
  LL_TRACE_INFO2("### LlApi ###  LlExtAdvEnable: enable=%u, numAdvSets=%u", enable, numAdvSets);

  if (lmgrCb.extAdvEnaDelayCnt != 0)
  {
    LL_TRACE_WARN0("Overlapped enable request");
    LmgrSendExtAdvEnableCnf(0, LL_ERROR_CODE_CMD_DISALLOWED);
  }

  lmgrCb.advSetEnaStatus = LL_SUCCESS;

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    lmgrCb.extAdvEnaDelayCnt = 1;
    LmgrSendExtAdvEnableCnf(0, LL_ERROR_CODE_CMD_DISALLOWED);
    return;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (((numAdvSets > LL_MAX_ADV_SETS)) ||
       ((numAdvSets == 0) && enable)))
  {
    lmgrCb.extAdvEnaDelayCnt = 1;
    LmgrSendExtAdvEnableCnf(0, LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS);
    return;
  }

  for (unsigned int i = 0; i < numAdvSets; i++)
  {
    uint8_t status;
    if (((status = LctrIsExtAdvEnableReady(enaParam[i].handle)) != LL_SUCCESS) ||
        ((status = LctrIsExtAdvEnableParamValid(enable, &enaParam[i])) != LL_SUCCESS))
    {
      lmgrCb.extAdvEnaDelayCnt = 1;
      LmgrSendExtAdvEnableCnf(enaParam[i].handle, status);
      return;
    }
  }

  if (numAdvSets > 0)
  {
    for (unsigned int i = 0; i < numAdvSets; i++)
    {
      LctrExtAdvEnableMsg_t *pMsg;

      if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        pMsg->hdr.handle = enaParam[i].handle;
        pMsg->hdr.dispId = LCTR_DISP_EXT_ADV;
        pMsg->hdr.event  = enable ? LCTR_EXT_ADV_MSG_START : LCTR_EXT_ADV_MSG_STOP;
        pMsg->durMs      = enaParam[i].duration * 10;
        pMsg->maxEvents  = enaParam[i].numEvents;

        /* Delay enable confirm event until all advertising sets respond. */
        lmgrCb.extAdvEnaDelayCnt++;

        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
      }
    }
  }
  else /* (numAdvSets == 0) */
  {
    /* N.B. Parameter check guarantees enable is FALSE. */

    uint8_t numHandles;
    uint8_t handles[LL_MAX_ADV_SETS] = { 0 };

    numHandles = LctrGetAdvHandles(handles);

    for (unsigned int i = 0; i < numHandles; i++)
    {
      LctrExtAdvEnableMsg_t *pMsg;

      if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        pMsg->hdr.handle = handles[i];
        pMsg->hdr.dispId = LCTR_DISP_EXT_ADV;
        pMsg->hdr.event  = LCTR_EXT_ADV_MSG_STOP;
        pMsg->durMs      = 0;
        pMsg->maxEvents  = 0;

        /* Delay enable confirm event until all advertising sets respond. */
        lmgrCb.extAdvEnaDelayCnt++;

        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Read maximum advertising data length.
 *
 *  \param      pLen        Return buffer for Maximum data length.
 *
 *  \return     Status error code.
 *
 *  Read maximum advertising data length.
 */
/*************************************************************************************************/
uint8_t LlReadMaxAdvDataLen(uint16_t *pLen)
{
  LL_TRACE_INFO0("### LlApi ###  LlReadMaxAdvDataLen");

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  WSF_ASSERT(pLen);
  *pLen = WSF_MIN(pLctrRtCfg->maxExtAdvDataLen, LL_MAX_ADV_DATA_LEN);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Read number of supported advertising sets.
 *
 *  \param      pNumSets    Return buffer for number of advertising sets.
 *
 *  \return     Status error code.
 *
 *  Read number of supported advertising sets.
 */
/*************************************************************************************************/
uint8_t LlReadNumSupAdvSets(uint8_t *pNumSets)
{
  LL_TRACE_INFO1("### LlApi ###  LlReadNumSupAdvSets: numSets=%u", pLctrRtCfg->maxAdvSets);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  WSF_ASSERT(pNumSets);
  *pNumSets = pLctrRtCfg->maxAdvSets;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Remove advertising set.
 *
 *  \param      handle      Advertising set ID.
 *
 *  \return     Status error code.
 *
 *  Removes the an advertising set from the LL.
 */
/*************************************************************************************************/
uint8_t LlRemoveAdvSet(uint8_t handle)
{
  LL_TRACE_INFO1("### LlApi ###  LlRemoveAdvSet: handle=%u", handle);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  return LctrRemoveAdvSet(handle);
}

/*************************************************************************************************/
/*!
 *  \brief      Clear advertising sets.
 *
 *  \return     Status error code.
 *
 *  Remove all existing advertising sets from the LL.
 */
/*************************************************************************************************/
uint8_t LlClearAdvSets(void)
{
  LL_TRACE_INFO0("### LlApi ###  LlClearAdvSets");

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  return LctrClearAdvSets();
}

/*************************************************************************************************/
/*!
 *  \brief      Set periodic advertising parameters.
 *
 *  \param      handle          Advertising set ID.
 *  \param      pPerAdvParam   Periodic advertising parameters.
 *
 *  \return     Status error code.
 *
 *  Set periodic advertising parameters.
 *
 *  \note       This function must only be called when advertising handle exists.
 */
/*************************************************************************************************/
uint8_t LlSetPeriodicAdvParam(uint8_t handle, LlPerAdvParam_t *pPerAdvParam)
{
  LL_TRACE_INFO2("### LlApi ###  LlSetPeriodicAdvParam, handle=%u, perAdvProp=0x%04x", handle, pPerAdvParam->perAdvProp);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (handle > LL_MAX_ADV_HANDLE))
  {
    return LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((pPerAdvParam->perAdvInterMax < pPerAdvParam->perAdvInterMin) ||
       (pPerAdvParam->perAdvInterMin < LL_PER_ADV_INT_MIN)))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  return LctrSetPeriodicAdvParam(handle, pPerAdvParam);
}
/*************************************************************************************************/
/*!
 *  \brief      Set periodic advertising data.
 *
 *  \param      handle      Advertising handle.
 *  \param      op          Operation.
 *  \param      len         Data buffer length.
 *  \param      pData       Advertising data buffer.
 *
 *  \return     Status error code.
 *
 *  Set extended advertising data data.
 */
/*************************************************************************************************/
uint8_t LlSetPeriodicAdvData(uint8_t handle, uint8_t op, uint8_t len, const uint8_t *pData)
{
  LL_TRACE_INFO2("### LlApi ###  LlSetPeriodicAdvData, handle=%u, len=%u", handle, len);

  WSF_ASSERT(pData);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (handle > LL_MAX_ADV_HANDLE))
  {
    return LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (op > LL_ADV_DATA_OP_UNCHANGED))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  return LctrSetPeriodicAdvData(handle, op, len, pData);
}

/*************************************************************************************************/
/*!
 *  \brief      Set periodic advertising enable.
 *
 *  \param      handle      Advertising handle.
 *  \param      enable      Set to TRUE to enable advertising, FALSE to disable advertising.
 *
 *  Enable or disable periodic advertising.
 */
/*************************************************************************************************/
void LlSetPeriodicAdvEnable(uint8_t enable, uint8_t handle)
{
  LL_TRACE_INFO2("### LlApi ###  LlSetPeriodicAdvEnable: enable=%u, handle=%u", enable, handle);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    LmgrSendPeriodicAdvEnableCnf(handle, LL_ERROR_CODE_CMD_DISALLOWED);
    return;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (handle > LL_MAX_ADV_HANDLE))
  {
    LmgrSendPeriodicAdvEnableCnf(handle, LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE);
    return;
  }

  LctrSetPeriodicAdvEnable(handle, enable);
}

/*************************************************************************************************/
/*!
 *  \brief      Set auxiliary packet offset delay.
 *
 *  \param      handle      Advertising handle.
 *  \param      delayUsec   Additional time in microseconds. "0" to disable.
 *
 *  \return     Status error code.
 *
 *  Additional delay given to auxiliary packets specified by AuxPtr. Offset values are
 *  limited by the advertising interval.
 */
/*************************************************************************************************/
uint8_t LlSetAuxOffsetDelay(uint8_t handle, uint32_t delayUsec)
{
  LL_TRACE_INFO2("### LlApi ###  LlSetAuxOffsetDelay: handle=%u, delayUsec=%u", handle, delayUsec);

  return LctrSetAuxOffsetDelay(handle, delayUsec);
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended advertising data fragmentation length.
 *
 *  \param      handle      Advertising handle.
 *  \param      fragLen     Fragmentation length.
 *
 *  \return     Status error code.
 *
 *  Fragmentation size for Advertising Data and Scan Response Data when selected by the host.
 */
/*************************************************************************************************/
uint8_t LlSetExtAdvDataFragLen(uint8_t handle, uint8_t fragLen)
{
  LL_TRACE_INFO2("### LlApi ###  LlSetExtAdvDataFragLen: handle=%u, fragLen=%u", handle, fragLen);

  return LctrSetExtAdvDataFragLen(handle, fragLen);
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended advertising transmit PHY options.
 *
 *  \param      handle      Advertising handle.
 *  \param      priPhyOpts  Primary advertising channel PHY options.
 *  \param      secPhyOpts  Secondary advertising channel PHY options.
 *
 *  \return     Status error code.
 *
 *  PHY options for extended advertising transmissions. New values are applied dynamically.
 */
/*************************************************************************************************/
uint8_t LlSetExtAdvTxPhyOptions(uint8_t handle, uint8_t priPhyOpts, uint8_t secPhyOpts)
{
  LL_TRACE_INFO3("### LlApi ###  LlSetExtAdvTxPhyOptions: handle=%u, priPhyOpts=%u, secPhyOpts=%u", handle, priPhyOpts, secPhyOpts);

  if (LL_API_PARAM_CHECK == 1)
  {
    switch (priPhyOpts)
    {
      case LL_PHY_OPTIONS_S2_PREFERRED:
      case LL_PHY_OPTIONS_S8_PREFERRED:
        /* Allowed options. */
        break;
      default:
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }

    switch (secPhyOpts)
    {
      case LL_PHY_OPTIONS_S2_PREFERRED:
      case LL_PHY_OPTIONS_S8_PREFERRED:
        /* Allowed options. */
        break;
      default:
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }
  }

  return LctrSetExtAdvTxPhyOptions(handle, priPhyOpts, secPhyOpts);
}

/*************************************************************************************************/
/*!
 *  \brief      Set the default Ext adv TX PHY options.
 *
 *  \param      phyOptions  PHY options.
 *
 *  Set the default TX PHY options for extended adv slave primary and secondary channel.
 */
/*************************************************************************************************/
void LlSetDefaultExtAdvTxPhyOptions(const uint8_t phyOptions)
{
  LL_TRACE_INFO1("### LlApi ###  LlSetDefaultExtAdvTxPhyOptions phyOptions=%x", phyOptions);

  lmgrSlvAdvCb.defTxPhyOpts = phyOptions;
}
