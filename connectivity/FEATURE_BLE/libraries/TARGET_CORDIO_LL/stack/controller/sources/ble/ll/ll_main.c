/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer (LL) slave parameter interface implementation file.
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
#include "lctr_api.h"
#include "lctr_api_conn.h"
#include "lctr_api_adv_slave_ae.h"
#include "lctr_api_adv_master_ae.h"
#include "lctr_api_adv_master.h"
#include "lctr_api_init_master.h"
#include "lctr_api_init_master_ae.h"
#include "lmgr_api_adv_slave.h"
#include "lmgr_api.h"
#include "bb_ble_api.h"
#include "bb_ble_api_whitelist.h"
#include "pal_bb_ble.h"
#include "pal_radio.h"
#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_trace.h"
#include "wsf_msg.h"
#include "util/bstream.h"
#include <string.h>

/*************************************************************************************************
 Globals
*************************************************************************************************/

/*! \brief      Check if periodic adv is enabled (ae functionality). */
LctrIsPerAdvEnabledFn_t LctrPerAdvEnabled; /*!< Lctr Per Adv Enabled check function (defined if AE supported). */

/*! \brief      Update the channel map for CIS. */
LctrUpdateCisChanMapFn_t LctrUpdateCisChanMapFn; /*!< Lctr Per Adv Enabled check function (defined if AE supported). */

/*! \brief      Check is Ext Scan is enabled (ae functionality). */
LctrExtCheckFn_t LctrMstExtScanEnabled;

/*! \brief      Check is Ext Init is enabled (ae functionality). */
LctrExtCheckFn_t LctrMstExtInitEnabled;

/*************************************************************************************************/
/*!
 *  \brief      Set Bluetooth device address
 *
 *  \param      pAddr       Bluetooth device address.
 *
 *  Set the BD address to be used by LL.
 */
/*************************************************************************************************/
void LlSetBdAddr(const uint8_t *pAddr)
{
  LL_TRACE_INFO0("### LlApi ###  LlSetBdAddr");

  LL_TRACE_INFO3("Static BDA[5:3]=%02x:%02x:%02x", pAddr[5], pAddr[4], pAddr[3]);
  LL_TRACE_INFO3("       BDA[2:0]=%02x:%02x:%02x", pAddr[2], pAddr[1], pAddr[0]);

  BSTREAM_TO_BDA64(lmgrPersistCb.bdAddr, pAddr);
}

/*************************************************************************************************/
/*!
 *  \brief      Get Bluetooth device address
 *
 *  \param      pAddr       Bluetooth device address.
 *
 *  Get the BD address currently used by LL or all zeros if address is not set.
 */
/*************************************************************************************************/
void LlGetBdAddr(uint8_t *pAddr)
{
  WSF_ASSERT(pAddr);

  LL_TRACE_INFO0("### LlApi ###  LlGetBdAddr");

  BDA64_TO_BSTREAM(pAddr, lmgrPersistCb.bdAddr);
}

/*************************************************************************************************/
/*!
 *  \brief      Set random device address.
 *
 *  \param      pAddr       Random Bluetooth device address.
 *
 *  \return     Status.
 *
 *  Set the random address to be used by LL.
 */
/*************************************************************************************************/
uint8_t LlSetRandAddr(const uint8_t *pAddr)
{
  LL_TRACE_INFO0("### LlApi ###  LlSetRandAddr");

  LL_TRACE_INFO3("Private BDA[5:3]=%02x:%02x:%02x", pAddr[5], pAddr[4], pAddr[3]);
  LL_TRACE_INFO3("        BDA[2:0]=%02x:%02x:%02x", pAddr[2], pAddr[1], pAddr[0]);

  WSF_CS_INIT(cs);

  WSF_ASSERT(pAddr);

  /* Legacy Advertising */
  if ((lmgrCb.advEnabled &&
    ((lmgrSlvAdvCb.advParam.ownAddrType & 0x01))))
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  /* Scanning. */
  if (lmgrCb.numScanEnabled)
  {
    /* Legacy Scanning. */
    if (LctrMstScanIsEnabled())
    {
      return LL_ERROR_CODE_CMD_DISALLOWED;
    }

    /* Extended Scanning. */
    if (LctrMstExtScanEnabled)
    {
      for (int scanPhy = 0; scanPhy < LCTR_SCAN_PHY_TOTAL; scanPhy++)
      {
        if (LctrMstExtScanEnabled(scanPhy))
        {
          return LL_ERROR_CODE_CMD_DISALLOWED;
        }
      }
    }
  }

  /* Initiating */
  if (lmgrCb.numInitEnabled)
  {
    /* Legacy init. */
    if (LctrMstInitIsEnabled())
    {
      return LL_ERROR_CODE_CMD_DISALLOWED;
    }

    /* Extended Initiating */
    if (LctrMstExtInitEnabled)
    {
      for (int scanPhy = 0; scanPhy < LCTR_SCAN_PHY_TOTAL; scanPhy++)
      {
        if (LctrMstExtInitEnabled(scanPhy))
        {
          return LL_ERROR_CODE_CMD_DISALLOWED;
        }
      }
    }
  }

  uint64_t bdAddr;
  BSTREAM_TO_BDA64(bdAddr, pAddr);

  /* Check BD Random Address type. */
  switch (bdAddr & LL_RAND_ADDR_TYPE_MASK)
  {
    case LL_RAND_ADDR_TYPE_STATIC:
    case LL_RAND_ADDR_TYPE_RPA:
    case LL_RAND_ADDR_TYPE_NRPA:
      /* Valid types. */
      break;
    default:
      LL_TRACE_WARN0("Invalid random address type");
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  WSF_CS_ENTER(cs);
  lmgrCb.bdAddrRnd = bdAddr;
  lmgrCb.bdAddrRndValid = TRUE;
  lmgrCb.bdAddrRndModAdv = lmgrCb.bdAddrRndModScan = TRUE;
  WSF_CS_EXIT(cs);
  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Get random device address
 *
 *  \param      pAddr       Random Bluetooth device address.
 *
 *  \return     Status error code.
 *
 *  Get the random address currently used by LL or all zeros if address is not set.
 */
/*************************************************************************************************/
uint8_t LlGetRandAddr(uint8_t *pAddr)
{
  LL_TRACE_INFO0("### LlApi ###  LlGetRandAddr");

  WSF_ASSERT(pAddr);

  if (!lmgrCb.bdAddrRndValid)
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  BDA64_TO_BSTREAM(pAddr, lmgrCb.bdAddrRnd);
  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Get versions
 *
 *  \param      pCompId     Company ID.
 *  \param      pBtVer      Bluetooth version.
 *  \param      pImplRev    Implementation revision.
 */
/*************************************************************************************************/
void LlGetVersion(uint16_t *pCompId, uint8_t *pBtVer, uint16_t *pImplRev)
{
  LL_TRACE_INFO0("### LlApi ###  LlGetVersion");

  *pCompId  = pLctrRtCfg->compId;
  *pBtVer   = pLctrRtCfg->btVer;
  *pImplRev = pLctrRtCfg->implRev;
}

/*************************************************************************************************/
/*!
 *  \brief      Get supported states.
 *
 *  \param      pStates     Supported states bitmask.
 *
 *  Return the states supported by the LL.
 */
/*************************************************************************************************/
void LlGetSupStates(uint8_t *pStates)
{
  LL_TRACE_INFO0("### LlApi ###  LlGetSupStates");

  WSF_ASSERT(pStates);

  UINT64_TO_BSTREAM(pStates, lmgrPersistCb.supStates);
}

/*************************************************************************************************/
/*!
 *  \brief      Get features.
 *
 *  \param      pFeatures   Supported features bitmask.
 *
 *  Return the LE features supported by the LL.
 */
/*************************************************************************************************/
void LlGetFeatures(uint8_t *pFeatures)
{
  LL_TRACE_INFO0("### LlApi ###  LlGetFeatures");

  WSF_ASSERT(pFeatures);

  UINT64_TO_BSTREAM(pFeatures, lmgrCb.features);
}

/*************************************************************************************************/
/*!
 *  \brief      Set features.
 *
 *  \param      pFeatures   Supported features bitmask.
 *
 *  \return     Status error code.
 *
 *  \note       This function must only be called when controller is not connected to another
 *              device.
 *
 *  Set the LE features supported by the LL.
 */
/*************************************************************************************************/
uint8_t LlSetFeatures(const uint8_t *pFeatures)
{
  uint64_t newMask;

  WSF_ASSERT(pFeatures);

  LL_TRACE_INFO0("### LlApi ###  LlSetFeatures");

  BSTREAM_TO_UINT64(newMask, pFeatures);

  if ((LL_API_PARAM_CHECK == 1) &&
      ((~LL_FEAT_ALL_MASK & newMask) != 0))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  lmgrCb.features = newMask;

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set operational mode flags.
 *
 *  \param      flags   Flags.
 *  \param      enable  TRUE to enable flags, FALSE to disable.
 *
 *  \return     Status error code.
 *
 *  Set mode flags governing LL operations.
 */
/*************************************************************************************************/
uint8_t LlSetOpFlags(uint32_t flags, bool_t enable)
{
  const uint32_t allFlags =
    LL_OP_MODE_FLAG_ENA_VER_LLCP_STARTUP |
    LL_OP_MODE_FLAG_SLV_REQ_IMMED_ACK |
    LL_OP_MODE_FLAG_BYPASS_CE_GUARD |
    LL_OP_MODE_FLAG_MST_RETX_AFTER_RX_NACK |
    LL_OP_MODE_FLAG_MST_IGNORE_CP_RSP |
    LL_OP_MODE_FLAG_MST_UNCOND_CP_RSP |
    LL_OP_MODE_FLAG_ENA_LEN_LLCP_STARTUP |
    LL_OP_MODE_FLAG_REQ_SYM_PHY |
    LL_OP_MODE_FLAG_ENA_FEAT_LLCP_STARTUP |
    LL_OP_MODE_FLAG_SLV_DELAY_LLCP_STARTUP |
    LL_OP_MODE_FLAG_ENA_SLV_LATENCY_WAKEUP |
    LL_OP_MODE_FLAG_ENA_SLV_AUX_SCAN_RSP_ADI |
    LL_OP_MODE_FLAG_ENA_MST_CIS_NULL_PDU |
    LL_OP_MODE_FLAG_ENA_SLV_AUX_IND_ADVA |
    LL_OP_MODE_FLAG_ENA_ADV_CHAN_RAND |
    LL_OP_MODE_FLAG_DIS_POWER_MONITOR |
    LL_OP_MODE_FLAG_ENA_ISO_LOST_NOTIFY |
    LL_OP_MODE_FLAG_ENA_CH_RPT_LLCP_AFTER_FEAT |
    LL_OP_MODE_FLAG_IGNORE_CRC_ERR_TS |
    LL_OP_MODE_FLAG_FORCE_CIS_CODED_PHY_S2 |
    /* diagnostics only */
    LL_OP_MODE_FLAG_ENA_ADV_DLY |
    LL_OP_MODE_FLAG_ENA_SCAN_BACKOFF |
    LL_OP_MODE_FLAG_ENA_WW |
    LL_OP_MODE_FLAG_ENA_SLV_LATENCY |
    LL_OP_MODE_FLAG_ENA_LLCP_TIMER |
    LL_OP_MODE_FLAG_ENA_SUBRATE_CE;

  LL_TRACE_INFO2("### LlApi ###  LlSetOpFlags flag=%x enable=%d", flags, enable);

  if ((LL_API_PARAM_CHECK == 1) &&
      ((~allFlags & flags) != 0))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  if (enable)
  {
    lmgrCb.opModeFlags |= flags;
  }
  else
  {
    lmgrCb.opModeFlags &= ~flags;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Get white list size.
 *
 *  \return     Total number of white list entries.
 *
 *  Read the white list capacity supported by the LL.
 */
/*************************************************************************************************/
uint8_t LlGetWhitelistSize(void)
{
  LL_TRACE_INFO0("### LlApi ###  LlGetWhitelistSize");

  return BbBleWhiteListGetSize();
}

/*************************************************************************************************/
/*!
 *  \brief      Clear all white list entries.
 *
 *  \return     Status error code.
 *
 *  Clear all white list entries stored in the LL.
 *
 *  \note       This function must only be called when advertising or scan is disabled
 *              and not initiating.
 */
/*************************************************************************************************/
uint8_t LlClearWhitelist(void)
{
  LL_TRACE_INFO0("### LlApi ###  LlClearWhitelist");

  if (lmgrCb.numWlFilterEnabled)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  BbBleWhiteListClear();

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Add device to the white list.
 *
 *  \param      addrType    Address type.
 *  \param      pAddr       Bluetooth device address.
 *
 *  \return     Status error code.
 *
 *  Adds the given address to the white list stored in the LL.
 *
 *  \note       This function must only be called when advertising or scan is disabled
 *              and not initiating.
 */
/*************************************************************************************************/
uint8_t LlAddDeviceToWhitelist(uint8_t addrType, bdAddr_t pAddr)
{
  LL_TRACE_INFO0("### LlApi ###  LlAddDeviceToWhitelist");

  if (LL_API_PARAM_CHECK == 1)
  {
    if (((pLctrRtCfg->btVer <= LL_VER_BT_CORE_SPEC_4_2) &&
        (addrType > LL_ADDR_RANDOM)) ||
      ((pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_0) &&
        (addrType > LL_ADDR_RANDOM) && (addrType != LL_ADDR_ANONYMOUS)))
    {
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }
  }
  if (lmgrCb.numWlFilterEnabled)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (addrType == LL_ADDR_ANONYMOUS)
  {
    BbBleWhiteListAddAnonymous();
  }
  else
  {
    uint64_t addr = BstreamToBda64(pAddr);
    if (!BbBleWhiteListAdd(addrType, addr))
    {
      return LL_ERROR_CODE_MEM_CAP_EXCEEDED;
    }
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Remove device from the white list.
 *
 *  \param      addrType    Address type.
 *  \param      pAddr       Bluetooth device address.
 *
 *  \return     Status error code.
 *
 *  Removes the given address from the white list stored in the LL.
 *
 *  \note       This function must only be called when advertising or scan is disabled
 *              and not initiating.
 */
/*************************************************************************************************/
uint8_t LlRemoveDeviceFromWhitelist(uint8_t addrType, bdAddr_t pAddr)
{
  LL_TRACE_INFO0("### LlApi ###  LlRemoveDeviceFromWhitelist");

  if (LL_API_PARAM_CHECK == 1)
  {
    if (((pLctrRtCfg->btVer <= LL_VER_BT_CORE_SPEC_4_2) &&
          (addrType > LL_ADDR_RANDOM)) ||
        ((pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_0) &&
          (addrType > LL_ADDR_RANDOM) && (addrType != LL_ADDR_ANONYMOUS)))
    {
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }
  }
  if (lmgrCb.numWlFilterEnabled)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (addrType == LL_ADDR_ANONYMOUS)
  {
    BbBleWhiteListRemoveAnonymous();
  }
  else
  {
    uint64_t addr = BstreamToBda64(pAddr);
    if (!BbBleWhiteListRemove(addrType, addr))
    {
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set host feature.
 *
 *  \param      bitNum      Bit position in the FeatureSet.
 *  \param      bitVal      Enable or disable feature.
 *
 *  \return     Status error code.
 *
 *  Set or clear a bit in the feature controlled by the Host in the Link Layer FeatureSet
 *  stored in the Controller.
 */
/*************************************************************************************************/
uint8_t LlSetHostFeatures(uint8_t bitNum, bool_t bitVal)
{
  LL_TRACE_INFO2("### LlApi ###  LlSetHostFeatures, bit=%d value=%d", bitNum, bitVal);

  if ((LL_HOST_CONTROLLED_FEAT & (UINT64_C(1) << bitNum)) == 0)
  {
    return LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE;
  }

  if (lmgrCb.numConnEnabled)
  {
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  lmgrCb.features = (lmgrCb.features & ~(UINT64_C(1) << bitNum)) | (((uint64_t) bitVal) << bitNum);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Get random number.
 *
 *  \param      pRandNum        Buffer to store 8 bytes random data.
 *
 *  \return     Status error code.
 *
 *  Request the LL to generate a random number.
 */
/*************************************************************************************************/
uint8_t LlGetRandNum(uint8_t *pRandNum)
{
  WSF_ASSERT(pRandNum);

  LL_TRACE_INFO0("### LlApi ###  LlGetRandNum");

  /* Return 8 bytes of random data. */
  PalCryptoGenerateRandomNumber(pRandNum, sizeof(uint64_t) / sizeof(uint8_t));

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Read supported transmit power.
 *
 *  \param      pMinTxPwr   Return buffer for minimum transmit power.
 *  \param      pMaxTxPwr   Return buffer for maximum transmit power.
 *
 *  Read the minimum and maximum transmit powers supported by the LL.
 */
/*************************************************************************************************/
void LlReadSupTxPower(int8_t *pMinTxPwr, int8_t *pMaxTxPwr)
{
  WSF_ASSERT(pMinTxPwr && pMaxTxPwr);

  PalRadioGetSupTxPower(pMinTxPwr, pMaxTxPwr);
}

/*************************************************************************************************/
/*!
 *  \brief      Read RF path compensation.
 *
 *  \param      pTxPathComp     Return buffer for RF transmit path compensation value.
 *  \param      pRxPathComp     Return buffer for RF receive path compensation value.
 *
 *  Read the RF Path Compensation Values parameter used in the Tx Power Level and RSSI calculation.
 */
/*************************************************************************************************/
void LlReadRfPathComp(int16_t *pTxPathComp, int16_t *pRxPathComp)
{
  LL_TRACE_INFO0("### LlApi ###  LlReadRfPathComp");

  WSF_ASSERT(pTxPathComp && pRxPathComp);
  PalRadioReadRfPathComp(pTxPathComp, pRxPathComp);
}

/*************************************************************************************************/
/*!
 *  \brief      Write RF path compensation.
 *
 *  \param      txPathComp      RF transmit path compensation value.
 *  \param      rxPathComp      RF receive path compensation value.
 *
 *  \return     Status error code.
 *
 *  Indicate the RF path gain or loss between the RF transceiver and the antenna contributed by
 *  intermediate components.
 */
/*************************************************************************************************/
uint8_t LlWriteRfPathComp(int16_t txPathComp, int16_t rxPathComp)
{
  LL_TRACE_INFO0("### LlApi ###  LlWriteRfPathComp");

  if (!PalRadioWriteRfPathComp(txPathComp, rxPathComp))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Set channel class.
 *
 *  \param      pChanMap        Channel map (0=bad, 1=usable).
 *
 *  \return     Status error code.
 *
 *  Set the channel class. At least 2 bits must be set.
 */
/*************************************************************************************************/
uint8_t LlSetChannelClass(const uint8_t *pChanMap)
{
  uint64_t chanMap;

  LL_TRACE_INFO0("### LlApi ###  LlSetChannelClass");

  BSTREAM_TO_UINT40(chanMap, pChanMap);

  if ((LL_API_PARAM_CHECK == 1) &&
      ((LlMathGetNumBitsSet(chanMap) < 2) ||
       ((chanMap & ~LL_CHAN_DATA_ALL) != 0)))
  {
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  return LctrSetChannelClass(chanMap);
}
