/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      BLE MAC system configuration.
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

#ifndef CFG_MAC_BLE_H
#define CFG_MAC_BLE_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*** LL ***/

#ifndef LL_API_PARAM_CHECK
#define LL_API_PARAM_CHECK      1       /*!< Check API parameters. */
#endif

#ifndef LL_MAX_CONN
#define LL_MAX_CONN             4       /*!< Absolute maximum number of connections (maximum is 32). */
#endif

#ifndef LL_MAX_FRAG
#define LL_MAX_FRAG             8       /*!< Maximum number of Data PDU fragments. */
#endif

#ifndef LL_NUM_ADV_FILT
#define LL_NUM_ADV_FILT         8       /*!< Table size for advertising filter. */
#endif

#ifndef LL_MAX_ADV_SETS
#define LL_MAX_ADV_SETS         6       /*!< Absolute maximum number of advertising sets. */
#endif

#ifndef LL_MAX_PHYS
#define LL_MAX_PHYS             3       /*!< Absolute maximum number of PHYs supported. */
#endif

#ifndef LL_MAX_PER_SCAN
#define LL_MAX_PER_SCAN         6       /*!< Absolute maximum number of periodic scanners (maximum is 32). */
#endif

#ifndef LL_ENABLE_TESTER
#define LL_ENABLE_TESTER        0       /*!< Enable LL tester extensions. */
#endif

#ifndef LL_ENABLE_CALIBRATION
#define LL_ENABLE_CALIBRATION   0       /*!< Enable LL calibration extensions. */
#endif

#ifndef LL_MAX_CIG
#define LL_MAX_CIG              2       /*!< Absolute maximum number of connected isochronous groups. */
#endif

#ifndef LL_MAX_CIS
#define LL_MAX_CIS              6       /*!< Absolute maximum number of connected isochronous streams, it is shared by CIGs. */
#endif

#ifndef LL_MAX_BIG
#define LL_MAX_BIG              2       /*!< Absolute maximum number of broadcast isochronous groups. */
#endif

#ifndef LL_MAX_BIS
#define LL_MAX_BIS              6       /*!< Absolute maximum number of broadcast isochronous streams, it is shared by BIGs. */
#endif

#ifndef LL_MAX_BN
#define LL_MAX_BN               8       /*!< Absolute maximum number of bursts. */
#endif

#ifndef LL_PC_TBL_POW
#define LL_PC_TBL_POW           2       /*!< Running average power control array sizes, must be a power of 2. */
#endif

#ifndef LL_PC_SERVICE_MS
#define LL_PC_SERVICE_MS     1000       /*!< Power control service intervals in milliseconds. */
#endif

#ifndef LL_PC_REQ_CHANGE_DBM
#define LL_PC_REQ_CHANGE_DBM    5       /*!< Request of increase/decrease in power value in units of dBm. */
#endif

#ifndef LHCI_ENABLE_VS
#define LHCI_ENABLE_VS          1       /*!< Enable vendor specific command processing. */
#endif

/*** Scheduler ***/

#ifndef SCH_RM_MAX_RSVN
#define SCH_RM_MAX_RSVN         (LL_MAX_CONN + LL_MAX_ADV_SETS + LL_MAX_CIG + LL_MAX_BIG) /*!< Maximum number of reservations (maximum is 32). */
#endif

/*** Baseband ***/

#ifndef BB_DATA_PLD_MAX_LEN
#define BB_DATA_PLD_MAX_LEN     251     /*!< Maximum data channel payload length (valid range 27 to 251, exclude MIC). */
#endif

#ifndef BB_ADV_PLD_MAX_LEN
#define BB_ADV_PLD_MAX_LEN      (BB_DATA_PLD_MAX_LEN + 4)
                                        /*!< Maximum advertising channel payload length (valid range 95 to 255). */
#endif

#ifndef BB_FIXED_DATA_PKT_LEN
#define BB_FIXED_DATA_PKT_LEN   0       /*!< Fixed packet length required for BB data receive buffers (0 to disable). */
#endif

#ifndef BB_FIXED_ADVB_PKT_LEN
#define BB_FIXED_ADVB_PKT_LEN   0       /*!< Fixed packet length required for BB advertisement receive buffers (0 to disable). */
#endif

#ifndef BB_DATA_PDU_TAILROOM
#define BB_DATA_PDU_TAILROOM    4       /*!< Extra byte allocation required for BB operations (i.e. MIC). */
#endif

#ifndef BB_ENABLE_INLINE_ENC_TX
#define BB_ENABLE_INLINE_ENC_TX 0       /*!< Enable inline encryption mode. Not available for all projects. */
#endif

#ifndef BB_ENABLE_INLINE_DEC_RX
#define BB_ENABLE_INLINE_DEC_RX 0       /*!< Enable inline decryption mode. Not available for all projects. */
#endif

#ifndef BB_SYM_PHY_REQ
#define BB_SYM_PHY_REQ          0       /*!< Symmetric PHY required on connections. */
#endif

#ifdef __cplusplus
};
#endif

#endif /* CFG_MAC_BLE_H */
