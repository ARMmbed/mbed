/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer (LL) Periodic advertising sync transfer interface implementation file.
 *
 *  Copyright (c) 2018 Arm Ltd. All Rights Reserved.
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

#include "lmgr_api.h"
#include "lctr_api_adv_slave_ae.h"
#include "lmgr_api_conn.h"
#include "lctr_api_conn.h"
#include "lctr_api_adv_master_ae.h"
#include "wsf_msg.h"
#include "wsf_trace.h"

/*************************************************************************************************/
/*!
 *  \brief      Enable or disable reports for the periodic advertising sync.
 *
 *  \param      syncHandle        Periodic sync handle.
 *  \param      enable            Enable settings.
 *
 *  \return     Status error code.
 *
 *  Enable or disable reports for the periodic advertising sync.
 */
/*************************************************************************************************/
uint8_t LlSetPeriodicAdvRcvEnable(uint16_t syncHandle, uint8_t enable)
{
  LL_TRACE_INFO2("### LlApi ###  LlSetPeriodicAdvRcvEnable: syncHandle=%u, enable=%u", syncHandle, enable);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      syncHandle > LL_SYNC_MAX_HANDLE)
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      !lctrMstPerIsSyncHandleValid(syncHandle))
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  LctrMstPerSetRcvEnable(syncHandle, enable);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Send sync info about periodic advertising to a connected device.
 *
 *  \param      connHandle        Connection handle.
 *  \param      serviceData       Service data provided by the host.
 *  \param      syncHandle        Periodic sync handle.
 *
 *  \return     Status error code.
 *
 *  Send sync info about about periodic advertising to a connected device.
 */
/*************************************************************************************************/
uint8_t LlPeriodicAdvSyncTransfer(uint16_t connHandle, uint16_t serviceData, uint16_t syncHandle)
{
  LL_TRACE_INFO2("### LlApi ###  LlPeriodicAdvSyncTransfer: connHandle=%u, syncHandle=%u", connHandle, syncHandle);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  return LctrPeriodicAdvSyncTransfer (connHandle, serviceData, syncHandle);
}

/*************************************************************************************************/
/*!
 *  \brief      Send sync info about periodic adv in an advertising set to a connected device.
 *
 *  \param      connHandle        Connection handle.
 *  \param      serviceData       Service data provided by the host.
 *  \param      advHandle         Handle to identify an advertising set.
 *
 *  \return     Status error code.
 *
 *  Send sync info about periodic adv in an advertising set to a connected device.
 */
/*************************************************************************************************/
uint8_t LlPeriodicAdvSetInfoTransfer(uint16_t connHandle, uint16_t serviceData, uint8_t advHandle)
{
  LL_TRACE_INFO2("### LlApi ###  LlPeriodicAdvSetInfoTransfer: connHandle=%u, advHandle=%u", connHandle, advHandle);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  return LctrPeriodicAdvSetInfoTransfer(connHandle, serviceData, advHandle);
}

/*************************************************************************************************/
/*!
 *  \brief      Set periodic advertising sync transfer parameters.
 *
 *  \param      connHandle        Connection handle.
 *  \param      mode              Periodic sync advertising sync transfer mode.
 *  \param      skip              The number of periodic advertising packets that can be skipped after a successful receive.
 *  \param      syncTimeout       Synchronization timeout for the periodic advertising.
 *  \param      cteType           Constant tone extension type(Used in AoD/AoA).
 *
 *  \return     Status error code.
 *
 *  Set periodic advertising sync transfer parameters.
 */
/*************************************************************************************************/
uint8_t LlSetPeriodicAdvSyncTransParams(uint16_t connHandle, uint8_t mode, uint16_t skip, uint16_t syncTimeout, uint8_t cteType)
{
  LL_TRACE_INFO3("### LlApi ###  LlSetPeriodicAdvSyncTransParams: connHandle=%u, mode=%u, skip=%u", connHandle, mode, skip);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((mode > LL_SYNC_TRSF_MAX_MODE) ||
      (skip > LL_SYNC_MAX_SKIP) ||
      (syncTimeout > LL_SYNC_MAX_TIMEOUT) ||
      (syncTimeout < LL_SYNC_MIN_TIMEOUT)))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  return LctrSetPeriodicAdvSyncTransParams(connHandle, mode, skip, syncTimeout, cteType);
}

/*************************************************************************************************/
/*!
 *  \brief      Set default periodic advertising sync transfer parameters.
 *
 *  \param      mode              Periodic sync advertising sync transfer mode.
 *  \param      skip              The number of periodic advertising packets that can be skipped after a successful receive.
 *  \param      syncTimeout       Synchronization timeout for the periodic advertising.
 *  \param      cteType           Constant tone extension type(Used in AoD/AoA).
 *
 *  \return     Status error code.
 *
 *  Set default periodic advertising sync transfer parameters.
 */
/*************************************************************************************************/
uint8_t LlSetDefaultPeriodicAdvSyncTransParams(uint8_t mode, uint16_t skip, uint16_t syncTimeout, uint8_t cteType)
{
  LL_TRACE_INFO3("### LlApi ###  LlSetDefaultPeriodicAdvSyncTransParams: mode=%u, skip=%u, syncTimeout=%u", mode, skip, syncTimeout);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((mode > LL_SYNC_TRSF_MAX_MODE) ||
      (skip > LL_SYNC_MAX_SKIP) ||
      (syncTimeout > LL_SYNC_MAX_TIMEOUT) ||
      (syncTimeout < LL_SYNC_MIN_TIMEOUT)))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  lmgrConnCb.syncMode = mode;
  lmgrConnCb.syncSkip = skip;
  lmgrConnCb.syncTimeout = syncTimeout;

  return LL_SUCCESS;
}
