/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer (LL) master control interface implementation file.
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

#include "ll_api.h"
#include "lctr_api_adv_master_ae.h"
#include "lmgr_api.h"
#include "lmgr_api_adv_master_ae.h"
#include "bb_ble_api_periodiclist.h"
#include "hci_defs.h"
#include "util/bstream.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_trace.h"

/*************************************************************************************************/
/*!
 *  \brief      Validate scan parameter.
 *
 *  \param      pParam          Scan parameters.
 *
 *  \return     TRUE if parameters are valid, FALSE otherwise.
 */
/*************************************************************************************************/
static bool_t llValidateScanParam(const LlExtScanParam_t *pParam)
{
  const uint16_t rangeMin = 0x0004;         /*  2.5 ms */
  /* const uint16_t rangeMax = 0xFFFF; */   /* 40.959375 ms */
  const uint8_t scanTypeMax = LL_SCAN_ACTIVE;

  if ((LL_API_PARAM_CHECK == 1) &&
     ((pParam->scanInterval < pParam->scanWindow) || (pParam->scanWindow < rangeMin) ||
      (pParam->scanType > scanTypeMax)))
  {
    return FALSE;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Set extended scanning parameters.
 *
 *  \param      ownAddrType     Address type used by this device.
 *  \param      scanFiltPolicy  Scan filter policy.
 *  \param      scanPhys        Scanning PHYs bitmask.
 *  \param      param           Scanning parameter table indexed by PHY.
 *
 *  \return     Status error code.
 *
 *  Set the extended scan parameters to be used on the primary advertising channel.
 */
/*************************************************************************************************/
uint8_t LlSetExtScanParam(uint8_t ownAddrType, uint8_t scanFiltPolicy, uint8_t scanPhys, const LlExtScanParam_t param[])
{
  const uint8_t scanFiltPolicyMax = ((lmgrCb.features & LL_FEAT_EXT_SCAN_FILT_POLICY) != 0) ? LL_SCAN_FILTER_WL_OR_RES_INIT : LL_SCAN_FILTER_WL_BIT;
  const uint8_t ownAddrTypeMax = ((lmgrCb.features & LL_FEAT_PRIVACY) != 0) ? LL_ADDR_RANDOM_IDENTITY : LL_ADDR_RANDOM;
  uint8_t supportedPhys = LL_PHYS_LE_1M_BIT;
  supportedPhys |= (lmgrCb.features & LL_FEAT_LE_CODED_PHY) ? LL_PHYS_LE_CODED_BIT : 0;

  LL_TRACE_INFO1("### LlApi ###  LlSetExtScanParam, scanPhys=0x%02x", scanPhys);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (lmgrCb.numScanEnabled || lmgrCb.numInitEnabled)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (scanPhys & ~supportedPhys))
  {
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
     ((ownAddrType > ownAddrTypeMax) ||
       (scanFiltPolicy > scanFiltPolicyMax) ||
       (scanPhys == 0)))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  if (LL_API_PARAM_CHECK == 1)
  {
    unsigned int i = 0;
    if (scanPhys & LL_PHYS_LE_1M_BIT)
    {
      if (!llValidateScanParam(&param[i]))
      {
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      i++;
    }
    if (scanPhys & LL_PHYS_LE_CODED_BIT)
    {
      if (!llValidateScanParam(&param[i]))
      {
        return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
      }
      i++;
    }
  }

  unsigned int i = 0;
  if (scanPhys & LL_PHYS_LE_1M_BIT)
  {
    LctrMstExtScanSetScanPhy(LCTR_SCAN_PHY_1M);
    LctrMstExtScanSetParam(LCTR_SCAN_PHY_1M, ownAddrType, scanFiltPolicy, &param[i++]);
  }
  else
  {
    LctrMstExtScanClearScanPhy(LCTR_SCAN_PHY_1M);
  }
  if (scanPhys & LL_PHYS_LE_CODED_BIT)
  {
    LctrMstExtScanSetScanPhy(LCTR_SCAN_PHY_CODED);
    LctrMstExtScanSetParam(LCTR_SCAN_PHY_CODED, ownAddrType, scanFiltPolicy, &param[i++]);
  }
  else
  {
    LctrMstExtScanClearScanPhy(LCTR_SCAN_PHY_CODED);
  }
  lmgrCb.numExtScanPhys = i;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Extended scan enable.
 *
 *  \param      enable          Set to TRUE to enable scanning, FALSE to disable scanning.
 *  \param      filterDup       Filter duplicate report mode.
 *  \param      duration        Duration.
 *  \param      period          Period.
 *
 *  Enable or disable extended scanning.
 */
/*************************************************************************************************/
void LlExtScanEnable(uint8_t enable, uint8_t filterDup, uint16_t duration, uint16_t period)
{
  const unsigned int durMsPerUnit = 10;
  const unsigned int perMsPerUnit = 1280;
  const unsigned int filterDupMax = LL_SCAN_FILTER_DUP_ENABLE_PERIODIC;

  lctrExtScanEnableMsg_t *pMsg;
  uint32_t durMs = duration * durMsPerUnit;
  uint32_t perMs = period * perMsPerUnit;

  LL_TRACE_INFO2("### LlApi ###  LlExtScanEnable: enable=%u, filterDup=%u", enable, filterDup);

  WSF_ASSERT(lmgrCb.extScanEnaDelayCnt == 0);
  lmgrCb.extScanEnaDelayCnt = lmgrCb.numExtScanPhys;
  lmgrCb.extScanEnaStatus = LL_SUCCESS;

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    lmgrCb.extScanEnaDelayCnt = 1;
    LmgrSendExtScanEnableCnf(LL_ERROR_CODE_CMD_DISALLOWED);
    return;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (enable == TRUE))
  {
    if (filterDup > filterDupMax)
    {
      lmgrCb.extScanEnaDelayCnt = 1;
      LmgrSendExtScanEnableCnf(LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS);
      return;
    }

    if ((filterDup == LL_SCAN_FILTER_DUP_ENABLE_PERIODIC) &&
         ((perMs == 0) || (durMs == 0)))
    {
      lmgrCb.extScanEnaDelayCnt = 1;
      LmgrSendExtScanEnableCnf(LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS);
      return;
    }

    /* Ensure period > duration. */
    if (((perMs != 0) && (durMs != 0)) &&
         (durMs >= perMs))
    {
      lmgrCb.extScanEnaDelayCnt = 1;
      LmgrSendExtScanEnableCnf(LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS);
      return;
    }
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      (!LctrMstExtScanValidateParam()))
  {
    lmgrCb.extScanEnaDelayCnt = 1;
    LmgrSendExtScanEnableCnf(LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS);
    return;
  }

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    /* pMsg->hdr.handle = -1; */     /* Subsystem broadcast message */
    pMsg->hdr.dispId = LCTR_DISP_EXT_SCAN;
    pMsg->hdr.event = enable ? LCTR_EXT_SCAN_MSG_DISCOVER_ENABLE : LCTR_EXT_SCAN_MSG_DISCOVER_DISABLE;

    pMsg->filtDup = filterDup;
    pMsg->durMs = durMs;
    pMsg->perMs = perMs;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Create synchronization of periodic advertising.
 *
 *  \param      pParam          Create sync parameters.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlPeriodicAdvCreateSync(const LlPerAdvCreateSyncCmd_t *pParam)
{
  LL_TRACE_INFO1("### LlApi ###  LlPeriodicAdvCreateSync advSID=%u", pParam->advSID);

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((pParam->advAddrType > LL_ADDR_RANDOM) ||
      (pParam->advSID > LL_MAX_ADV_SID) ||
      (pParam->skip > LL_SYNC_MAX_SKIP) ||
      (pParam->options & ~LL_PER_ADV_CREATE_SYNC_OPTIONS_BITS) ||
      (pParam->syncTimeOut > LL_SYNC_MAX_TIMEOUT) ||
      (pParam->syncTimeOut < LL_SYNC_MIN_TIMEOUT)))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  if (LctrMstPerIsSyncDisabled() == FALSE)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (LctrMstPerIsSync(pParam->advSID, pParam->advAddrType, BstreamToBda64(pParam->pAdvAddr)))
  {
    return LL_ERROR_CODE_ACL_CONN_ALREADY_EXISTS;
  }

  if (lctrMstPerGetNumPerScanCtx() >= LL_MAX_PER_SCAN)
  {
    return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
  }

  lctrPerCreateSyncMsg_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.dispId = LCTR_DISP_PER_CREATE_SYNC;
    pMsg->hdr.event  = LCTR_CREATE_SYNC_MSG_CREATE;

    pMsg->advAddr = BstreamToBda64(pParam->pAdvAddr);
    pMsg->advAddrType = pParam->advAddrType;
    pMsg->advSID = pParam->advSID;
    pMsg->filterPolicy = pParam->options & 0x01;
    pMsg->repDisabled = (pParam->options >> 1) & 0x01;
    pMsg->dupFilterEnable = ((pMsg->repDisabled) ? 0 : ((pParam->options >> 2) & 0x01));
    pMsg->skip = pParam->skip;
    pMsg->syncTimeOut = pParam->syncTimeOut;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Cancel pending synchronization of periodic advertising.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlPeriodicAdvCreateSyncCancel(void)
{
  LL_TRACE_INFO0("### LlApi ###  LlPeriodicAdvCreateSyncCancel");

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  /* Command is disallowed if there is no create sync pending. */
  if (!LctrMstPerIsSyncPending())
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  LctrPerScanMsg_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.dispId = LCTR_DISP_PER_CREATE_SYNC;
    pMsg->hdr.event  = LCTR_CREATE_SYNC_MSG_CANCEL;
    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Stop synchronization of periodic advertising.
 *
 *  \param      syncHandle      Sync handle.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlPeriodicAdvTerminateSync(uint16_t syncHandle)
{
  LL_TRACE_INFO1("### LlApi ###  LlPeriodicAdvTerminateSync, syncHandle=0x%02x", syncHandle);

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

  if (LctrMstPerIsSyncPending())
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  LctrPerScanMsg_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.dispId = LCTR_DISP_PER_SCAN;
    pMsg->hdr.event  = LCTR_PER_SCAN_MSG_TERMINATE;
    pMsg->hdr.handle = syncHandle;
    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Add device to periodic advertiser list.
 *
 *  \param      pParam          Add to periodic advertiser list parameters.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlAddDeviceToPeriodicAdvList(const LlDevicePerAdvList_t *pParam)
{
  LL_TRACE_INFO0("### LlApi ###  LlAddDeviceToPeriodicAdvList");

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (lmgrCb.numPlFilterEnabled)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((pParam->advAddrType > LL_ADDR_RANDOM) ||
       (pParam->advSID > LL_MAX_ADV_SID)))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  if (LctrMstPerIsSyncPending())
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  uint64_t addr = BstreamToBda64(pParam->pAdvAddr);

  if (BbBlePeriodicListCheckAddr(pParam->advAddrType, addr, pParam->advSID))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  if (!BbBlePeriodicListAdd(pParam->advAddrType, addr, pParam->advSID))
  {
    return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Remove device from periodic advertiser list command.
 *
 *  \param      pParam          Remove from periodic advertiser list parameters..
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlRemoveDeviceFromPeriodicAdvList(const LlDevicePerAdvList_t *pParam)
{
  LL_TRACE_INFO0("### LlApi ###  LlRemoveDeviceFromPeriodicAdvList");

  if ((LL_API_PARAM_CHECK == 1) &&
      !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (lmgrCb.numPlFilterEnabled)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
      ((pParam->advAddrType > LL_ADDR_RANDOM) ||
       (pParam->advSID > LL_MAX_ADV_SID)))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  if (LctrMstPerIsSyncPending())
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  uint64_t addr = BstreamToBda64(pParam->pAdvAddr);
  if (!BbBlePeriodicListRemove(pParam->advAddrType, addr, pParam->advSID))
  {
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Clear all devices in the periodic advertiser list.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlClearPeriodicAdvList(void)
{
  LL_TRACE_INFO0("### LlApi ###  LlClearPeriodicAdvList");
  if ((LL_API_PARAM_CHECK == 1) &&
       !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (lmgrCb.numPlFilterEnabled)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (LctrMstPerIsSyncPending())
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  BbBlePeriodicListClear();

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Read total number of devices in the periodic advertiser list.
 *
 *  \param      pListSize       Return size value of periodic advertiser list
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlReadPeriodicAdvListSize(uint8_t *pListSize)
{
  LL_TRACE_INFO0("### LlApi ###  LlReadPeriodicAdvListSize");

  if ((LL_API_PARAM_CHECK == 1) &&
       !LmgrIsExtCommandAllowed())
  {
    LL_TRACE_WARN0("Legacy Advertising/Scanning operation enabled; extended commands not available");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  *pListSize = BbBlePeriodicListGetSize();

  return LL_SUCCESS;
}

