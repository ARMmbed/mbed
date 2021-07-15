/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer constant definitions.
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

#ifndef LL_DEFS_H
#define LL_DEFS_H

#include "wsf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*** Version ***/

#define LL_VER_BT_CORE_SPEC_4_0     0x06    /*!< Bluetooth core specification 4.0 */
#define LL_VER_BT_CORE_SPEC_4_1     0x07    /*!< Bluetooth core specification 4.1 */
#define LL_VER_BT_CORE_SPEC_4_2     0x08    /*!< Bluetooth core specification 4.2 */
#define LL_VER_BT_CORE_SPEC_5_0     0x09    /*!< Bluetooth core specification 5.0 */
#define LL_VER_BT_CORE_SPEC_5_1     0x0A    /*!< Bluetooth core specification 5.1 */
#define LL_VER_BT_CORE_SPEC_5_2     0x0B    /*!< Bluetooth core specification 5.2 */
#define LL_VER_BT_CORE_SPEC_SYDNEY  0x0C    /*!< Bluetooth core specification Sydney */

#define LL_COMP_ID_ARM              0x005F  /*!< ARM Ltd. company ID. */

/*** Common ***/

#define LL_RSSI_MIN                 -127    /*!< Minimum RSSI value. */
#define LL_RSSI_MAX                 20      /*!< Maximum RSSI value. */
#define LL_RSSI_NOT_AVAIL           127     /*!< RSSI is not available. */

#define LL_CRC_LEN                  3       /*!< CRC length. */
#define LL_AA_LEN                   4       /*!< Access address length. */
#define LL_PREAMBLE_LEN_1M          1       /*!< Preamble length (LE 1M PHY). */
#define LL_PREAMBLE_LEN_2M          2       /*!< Preamble length (LE 2M PHY). */
#define LL_PREAMBLE_LEN_CODED_BITS  10      /*!< Preamble length (LE Coded PHY). */
#define LL_CI_LEN_BITS              2       /*!< Coding indicator length (LE Coded PHY). */
#define LL_TERM1_LEN_BITS           3       /*!< TERM1 length (LE Coded PHY). */
#define LL_TERM2_LEN_BITS           3       /*!< TERM2 length (LE Coded PHY). */

#define LL_RAND_ADDR_TYPE_MASK      UINT64_C(0xC00000000000)    /*!< BD Random Address type mask. */
#define LL_RAND_ADDR_TYPE_STATIC    UINT64_C(0xC00000000000)    /*!< Static Random Address type. */
#define LL_RAND_ADDR_TYPE_RPA       UINT64_C(0x400000000000)    /*!< Resolvable Private Address type. */
#define LL_RAND_ADDR_TYPE_NRPA      UINT64_C(0x000000000000)    /*!< Non-Resolvable Private Address type. */

/*** Advertising PDU ***/

/*! \brief      Advertising channel PDU types. */
enum
{
  /* --- Core Spec 4.0 --- */
  LL_PDU_ADV_IND                = 0,    /*!< Connectable undirected advertising PDU. */
  LL_PDU_ADV_DIRECT_IND         = 1,    /*!< Connectable directed advertising PDU. */
  LL_PDU_ADV_NONCONN_IND        = 2,    /*!< Non-connectable undirected advertising PDU. */
  LL_PDU_SCAN_REQ               = 3,    /*!< Scan request PDU. */
  LL_PDU_SCAN_RSP               = 4,    /*!< Scan response PDU. */
  LL_PDU_CONNECT_IND            = 5,    /*!< Connect indication PDU. */
  LL_PDU_ADV_SCAN_IND           = 6,    /*!< Scannable undirected advertising PDU. */
  /* --- Core Spec 5.0 --- */
  LL_PDU_AUX_SCAN_REQ           = 3,    /*!< Auxiliary scan request PDU. */
  LL_PDU_AUX_CONNECT_REQ        = 5,    /*!< Auxiliary connect request PDU. */
  LL_PDU_ADV_EXT_IND            = 7,    /*!< Extended advertising PDU. */
  LL_PDU_AUX_ADV_IND            = 7,    /*!< Auxiliary advertising PDU. */
  LL_PDU_AUX_SCAN_RSP           = 7,    /*!< Auxiliary scan response PDU. */
  LL_PDU_AUX_SYNC_IND           = 7,    /*!< Auxiliary synchronize PDU. */
  LL_PDU_AUX_CHAIN_IND          = 7,    /*!< Auxiliary chain PDU. */
  LL_PDU_AUX_CONNECT_RSP        = 8,    /*!< Auxiliary connect response PDU. */
};

#define LL_SCAN_REQ_PDU_LEN     12      /*!< Size of a scan request PDU. */
#define LL_CONN_IND_PDU_LEN     34      /*!< Size of a connect indication PDU. */
#define LL_CONN_RSP_PDU_LEN     14      /*!< Size of an auxiliary connect response PDU. */

#define LL_CHAN_ADV_MIN_IDX     37      /*!< Minimum advertising channel index. */
#define LL_CHAN_ADV_MAX_IDX     39      /*!< Maximum advertising channel index. */
#define LL_NUM_CHAN_ADV         3       /*!< Total number of advertising channels. */

#define LL_ADVBU_MAX_LEN        31      /*!< Maximum advertising channel host data length. */
#define LL_ADVB_MAX_LEN         39      /*!< Maximum advertising channel PDU length. */
#define LL_ADVB_MIN_LEN         (LL_ADVB_MAX_LEN - LL_ADVBU_MAX_LEN)    /*!< Minimum advertising channel packet length. */
#define LL_ADVB_MAX_TIME_1M     ((LL_BLE_US_PER_BYTE_1M * (LL_ADVB_MAX_LEN - LL_ADV_HDR_LEN)) + LL_MIN_PKT_TIME_US_1M)
                                        /*!< Maximum time for a 1M advertising channel PDU. */
#define LL_ADVB_MAX_TIME_2M     ((LL_BLE_US_PER_BYTE_2M * (LL_ADVB_MAX_LEN - LL_ADV_HDR_LEN)) + LL_MIN_PKT_TIME_US_2M)
                                        /*!< Maximum time for a 2M advertising channel PDU. */
#define LL_ADVB_MAX_TIME_S2     ((LL_BLE_US_PER_BYTE_CODED_S2 * (LL_ADVB_MAX_LEN - LL_ADV_HDR_LEN)) + LL_MIN_PKT_TIME_US_CODED_S2)
                                        /*!< Maximum time for a Coded S2 advertising channel PDU. */
#define LL_ADVB_MAX_TIME_S8     ((LL_BLE_US_PER_BYTE_CODED_S8 * (LL_ADVB_MAX_LEN - LL_ADV_HDR_LEN)) + LL_MIN_PKT_TIME_US_CODED_S8)
                                        /*!< Maximum time for a Coded S8 advertising channel PDU. */

#define LL_ADV_PKT_MAX_USEC     LL_ADVB_MAX_TIME_1M /*!< Maximum time in microseconds for an advertising packet. */
#define LL_SCAN_REQ_MAX_USEC    ((8 * (LL_ADV_PREFIX_LEN + LL_SCAN_PREFIX_LEN)) + LL_MIN_PKT_TIME_US_1M)
                                                    /*!< Maximum time in microseconds for a scan request packet. */
#define LL_SCAN_RSP_MAX_USEC    LL_ADVB_MAX_TIME_1M /*!< Maximum time in microseconds for a scan response packet. */

#define LL_ADV_HDR_LEN          2       /*!< Advertising channel header length. */
#define LL_ADV_HDR_TYPE_OFFS    0       /*!< Advertising header type offset. */
#define LL_ADV_HDR_TYPE_MSK     0x0F    /*!< Advertising header type mask. */
#define LL_ADV_HDR_LEN_OFFS     1       /*!< Advertising header length offset. */
#define LL_ADV_HDR_LEN_MSK      0x3F    /*!< Advertising header length mask for 4.2. */
#define LL_ADV_HDR_CP_MSK       0x40    /*!< Advertising header CP Mask for 5.1. */
#define LL_ADV_EXT_HDR_LEN_MSK  0xFF    /*!< Advertising extension header length mask for 5.0. */
#define LL_ADV_PREFIX_LEN       6       /*!< Advertising PDU payload prefix length (AdvA). */
#define LL_SCAN_PREFIX_LEN      6       /*!< Scan request/response PDU payload prefix length (AdvA). */

#define LL_ADV_ACCESS_ADDR      UINT32_C(0x8E89BED6)    /*!< Advertising channel access address. */
#define LL_ADV_CRC_INIT         UINT32_C(0x555555)      /*!< Advertising CRC initial value. */

#define LL_DIR_ADV_INTER_TICKS  6       /*!< Advertising interval between directed advertising events (3.75 ms). */
#define LL_DIR_ADV_DUR_TICKS    2048    /*!< Maximum high duty cycle directed advertising duration (1.28 seconds). */

/*! \brief      Extended header bit definition. */
enum
{
  LL_EXT_HDR_ADV_ADDR_BIT   = (1 << 0), /*!< Extended header AdvA bit. */
  LL_EXT_HDR_TGT_ADDR_BIT   = (1 << 1), /*!< Extended header TargetA bit. */
  LL_EXT_HDR_CTE_INFO_BIT   = (1 << 2), /*!< Extended header CTEInfo bit. */
  LL_EXT_HDR_ADI_BIT        = (1 << 3), /*!< Extended header AdvDataInfo bit. */
  LL_EXT_HDR_AUX_PTR_BIT    = (1 << 4), /*!< Extended header AuxPtr bit. */
  LL_EXT_HDR_SYNC_INFO_BIT  = (1 << 5), /*!< Extended header SyncInfo bit. */
  LL_EXT_HDR_TX_PWR_BIT     = (1 << 6), /*!< Extended header TxPower bit. */
};

#define LL_MAX_ADV_HANDLE       0xEF    /*!< Maximum advertising handle. */
#define LL_MAX_ADV_SID          0x0F    /*!< Maximum advertising SID */

#define LL_DID_NOT_PRESENT      0xFF    /*!< Periodic advertising data DID was not present. */

#define LL_EXT_ADV_HDR_MIN_LEN      1       /*!< Minimum extended advertising header length (ExtHdrLen and AdvMode fields). */
#define LL_EXT_ADV_HDR_MAX_LEN      64      /*!< Maximum extended advertising header length (ExtHdrLen, AdvMode fields and Extended header). */
#define LL_EXT_HDR_FLAG_LEN         1       /*!< Length of extended header flag field */
#define LL_EXT_ADVBU_MAX_LEN        251     /*!< Maximum extended advertising channel PDU host data length. */
#define LL_EXT_ADVB_MAX_LEN         257     /*!< Maximum extended advertising channel PDU length. */
#define LL_EXT_ADVB_NORMAL_LEN      50      /*!< Normal extended advertising channel PDU length. */

#define LL_EXT_HDR_ACAD_MAX_LEN     LL_EXT_ADV_HDR_MAX_LEN - LL_EXT_ADV_HDR_MIN_LEN - LL_EXT_HDR_FLAG_LEN /*!< Maximum possible ACAD length (Max extended header minus ExtHdrLen, AdvMode, and extended header flag field. */

#define LL_EXT_ADVB_MAX_TIME_1M     ((LL_BLE_US_PER_BYTE_1M * (LL_EXT_ADVB_MAX_LEN - LL_ADV_HDR_LEN)) + LL_MIN_PKT_TIME_US_1M)
                                        /*!< Maximum time for a 1M advertising channel PDU. */
#define LL_EXT_ADVB_MAX_TIME_2M     ((LL_BLE_US_PER_BYTE_2M * (LL_EXT_ADVB_MAX_LEN - LL_ADV_HDR_LEN)) + LL_MIN_PKT_TIME_US_2M)
                                        /*!< Maximum time for a 2M advertising channel PDU. */
#define LL_EXT_ADVB_MAX_TIME_S2     ((LL_BLE_US_PER_BYTE_CODED_S2 * (LL_EXT_ADVB_MAX_LEN - LL_ADV_HDR_LEN)) + LL_MIN_PKT_TIME_US_CODED_S2)
                                        /*!< Maximum time for a Coded S2 advertising channel PDU. */
#define LL_EXT_ADVB_MAX_TIME_S8     ((LL_BLE_US_PER_BYTE_CODED_S8 * (LL_EXT_ADVB_MAX_LEN - LL_ADV_HDR_LEN)) + LL_MIN_PKT_TIME_US_CODED_S8)
                                        /*!< Maximum time for a Coded S8 advertising channel PDU. */
#define LL_EXT_ADVB_NORMAL_TIME_S8  ((LL_BLE_US_PER_BYTE_CODED_S8 * (LL_EXT_ADVB_NORMAL_LEN - LL_ADV_HDR_LEN)) + LL_MIN_PKT_TIME_US_CODED_S8)
                                        /*!< Time for a Coded S8 advertising channel PDU with normal length. */

#define LL_AUX_PTR_MAX_USEC     2457300 /*!< Maximum AuxPtr offset value in microseconds. */

#define LL_SYNC_MIN_TIMEOUT     0x000A  /*!< Minimum synchronization timeout. */
#define LL_SYNC_MAX_TIMEOUT     0x4000  /*!< Maximum synchronization timeout. */
#define LL_SYNC_MAX_SKIP        0x01F3  /*!< Maximum synchronization skip. */
#define LL_SYNC_MAX_HANDLE      0x0EFF  /*!< Maximum synchronization handle. */

#define LL_PER_ADV_INT_MIN      0x0006  /*!< Minimum periodic advertising interval. */

#define LL_SYNC_OFFS_ADJUST_USEC  2457600   /*!< Sync offset adjust of 2.4576 seconds. */
#define LL_SYNC_INFO_LEN        18      /*!< Size of SyncInfo field. */

/*! \brief      Periodic sync transfer receive mode. */
enum
{
  LL_SYNC_TRSF_MODE_OFF           = 0,    /*!< Periodic sync transfer receive is disabled. */
  LL_SYNC_TRSF_MODE_REP_DISABLED  = 1,    /*!< Periodic sync transfer receive is enabled, report event is disabled. */
  LL_SYNC_TRSF_MODE_REP_ENABLED   = 2,    /*!< Periodic sync transfer receive is enabled, report event is enabled. */
  LL_SYNC_TRSF_MODE_DUP_FILTERED  = 3,    /*!< Periodic sync transfer receive is enabled, report event is enabled, duplicates will be filtered if ADI field is present. */
  LL_SYNC_TRSF_MAX_MODE           = LL_SYNC_TRSF_MODE_DUP_FILTERED
};

/*** Data PDU ***/

/*! \brief      Data channel LL Control PDU types. */
enum
{
  /* --- Core Spec 4.0 --- */
  LL_PDU_CONN_UPDATE_IND        = 0x00, /*!< Connection update indication PDU. */
  LL_PDU_CHANNEL_MAP_IND        = 0x01, /*!< Channel map indication PDU. */
  LL_PDU_TERMINATE_IND          = 0x02, /*!< Terminate indication PDU. */
  LL_PDU_ENC_REQ                = 0x03, /*!< Encryption request PDU. */
  LL_PDU_ENC_RSP                = 0x04, /*!< Encryption response PDU. */
  LL_PDU_START_ENC_REQ          = 0x05, /*!< Start encryption request PDU. */
  LL_PDU_START_ENC_RSP          = 0x06, /*!< Start encryption response PDU. */
  LL_PDU_UNKNOWN_RSP            = 0x07, /*!< Unknown response PDU. */
  LL_PDU_FEATURE_REQ            = 0x08, /*!< Feature request PDU. */
  LL_PDU_FEATURE_RSP            = 0x09, /*!< Feature response PDU. */
  LL_PDU_PAUSE_ENC_REQ          = 0x0A, /*!< Pause encryption request PDU. */
  LL_PDU_PAUSE_ENC_RSP          = 0x0B, /*!< Pause encryption response PDU. */
  LL_PDU_VERSION_IND            = 0x0C, /*!< Version indication PDU. */
  LL_PDU_REJECT_IND             = 0x0D, /*!< Reject indication PDU. */
  /* --- Core Spec 4.2 --- */
  LL_PDU_SLV_FEATURE_REQ        = 0x0E, /*!< Slave feature request PDU. */
  LL_PDU_CONN_PARAM_REQ         = 0x0F, /*!< Connection parameter request PDU. */
  LL_PDU_CONN_PARAM_RSP         = 0x10, /*!< Connection parameter response PDU. */
  LL_PDU_REJECT_EXT_IND         = 0x11, /*!< Reject extended indication PDU. */
  LL_PDU_PING_REQ               = 0x12, /*!< Ping request PDU. */
  LL_PDU_PING_RSP               = 0x13, /*!< Ping response PDU. */
  LL_PDU_LENGTH_REQ             = 0x14, /*!< Data length request PDU. */
  LL_PDU_LENGTH_RSP             = 0x15, /*!< Data length response PDU. */
  /* --- Core Spec 5.0 --- */
  LL_PDU_PHY_REQ                = 0x16, /*!< PHY request PDU. */
  LL_PDU_PHY_RSP                = 0x17, /*!< PHY response PDU. */
  LL_PDU_PHY_UPDATE_IND         = 0x18, /*!< PHY update indication PDU. */
  LL_PDU_MIN_USED_CHAN_IND      = 0x19, /*!< Minimum used channels indication PDU. */
  /* --- Core Spec 5.1 --- */
  /* 0x1A 0x1B for AOA AOD, 0x1C for PAST 0x20 RFU */
  LL_PDU_PERIODIC_SYNC_IND      = 0x1C, /*!< Periodic sync indication PDU. */
  /* --- Core Spec 5.2 --- */
  LL_PDU_PEER_SCA_REQ           = 0x1D, /*!< Peer SCA request PDU. */
  LL_PDU_PEER_SCA_RSP           = 0x1E, /*!< Peer SCA response PDU. */
  LL_PDU_CIS_REQ                = 0x1F, /*!< CIS request PDU. */
  LL_PDU_CIS_RSP                = 0x20, /*!< CIS response PDU. */
  LL_PDU_CIS_IND                = 0x21, /*!< CIS indication PDU. */
  LL_PDU_CIS_TERM_IND           = 0x22, /*!< CIS terminate indication PDU. */
  LL_PDU_PWR_CTRL_REQ           = 0x23, /*!< Power Control request. */
  LL_PDU_PWR_CTRL_RSP           = 0x24, /*!< Power Control response. */
  LL_PDU_PWR_CHANGE_IND         = 0x25, /*!< Transmit power change indication PDU. */
  /* --- Core Spec Sydney --- */
  LL_PDU_SUBRATE_REQ            = 0x26, /*!< Connection subrate request PDU. */
  LL_PDU_SUBRATE_IND            = 0x27, /*!< Connection subrate indication PDU. */
  LL_PDU_CH_REPORTING_IND       = 0x28, /*!< Channel class reporting indication PDU. */
  LL_PDU_CH_STATUS_IND          = 0x29, /*!< Channel status indication PDU. */

  LL_PDU_UNSPECIFIED            = 0xFF  /*!< Unspecified PDU. */
};

/* Data PDU length */

/* --- Core Spec 4.0 --- */
#define LL_CONN_UPD_IND_PDU_LEN       12      /*!< Connection update indication PDU length. */
#define LL_CHAN_MAP_IND_PDU_LEN       8       /*!< Channel map indication PDU length. */
#define LL_TERMINATE_IND_PDU_LEN      2       /*!< Terminate indication PDU length. */
#define LL_ENC_REQ_LEN                23      /*!< Encryption request PDU length. */
#define LL_ENC_RSP_LEN                13      /*!< Encryption response PDU length. */
#define LL_START_ENC_LEN              1       /*!< Start encryption request/response PDU length. */
#define LL_UNKNOWN_RSP_LEN            2       /*!< Unknown response PDU length. */
#define LL_FEATURE_PDU_LEN            9       /*!< Feature request/response PDU length. */
#define LL_PAUSE_ENC_LEN              1       /*!< Pause encryption request/response PDU length. */
#define LL_VERSION_IND_PDU_LEN        6       /*!< Version indication PDU length. */
#define LL_REJECT_IND_PDU_LEN         2       /*!< Reject indication PDU length. */
/* --- Core Spec 4.2 --- */
#define LL_CONN_PARAM_PDU_LEN         24      /*!< Connection parameter request or response PDU length. */
#define LL_REJECT_EXT_IND_PDU_LEN     3       /*!< Reject extended indication PDU length. */
#define LL_PING_PDU_LEN               1       /*!< Ping request/response PDU length. */
#define LL_DATA_LEN_PDU_LEN           9       /*!< Data length request or response PDU length. */
/* --- Core Spec 5.0 --- */
#define LL_PHY_PDU_LEN                3       /*!< PHY request/response PDU length. */
#define LL_PHY_UPD_IND_PDU_LEN        5       /*!< PHY update indication PDU length. */
#define LL_MIN_USED_CHAN_PDU_LEN      3       /*!< Minimum used channels indication PDU length. */
/* --- Core Spec 5.1 --- */
#define LL_PERIODIC_SYNC_PDU_LEN      35      /*!< Periodic sync indication PDU length. */
/* --- Core Spec 5.2 --- */
#define LL_PEER_SCA_REQ_LEN           2       /*!< Peer SCA request PDU length. */
#define LL_PEER_SCA_RSP_LEN           2       /*!< Peer SCA response PDU length. */
#define LL_CIS_REQ_LEN                36      /*!< CIS request PDU length. */
#define LL_CIS_RSP_LEN                9       /*!< CIS response PDU length. */
#define LL_CIS_IND_LEN                16      /*!< CIS indication PDU length. */
#define LL_CIS_TERM_LEN               4       /*!< CIS termination PDU length. */
#define LL_CIS_SDU_CONFIG_REQ_LEN     13      /*!< CIS SDU config request PDU length. */
#define LL_CIS_SDU_CONFIG_RSP_LEN     4       /*!< CIS SDU config response PDU length. */
#define LL_PWR_CTRL_REQ_LEN           4       /*!< Power Control request PDU length. */
#define LL_PWR_CTRL_RSP_LEN           5       /*!< Power Control response PDU length. */
#define LL_PWR_CHANGE_IND_LEN         5       /*!< Power Indication PDU length. */

#define LL_EMPTY_PDU_LEN              2       /*!< Length of an empty data PDU. */

#define LL_DATA_HDR_LEN               2       /*!< Data channel header length. */
#define LL_DATA_HDR_MAX_LEN           4       /*!< Data channel header max length (with CTE field). */
#define LL_DATA_MIC_LEN               4       /*!< Data channel PDU MIC length. */

#define LL_DATA_HDR_LLID_MSK          0x03    /*!< Data PDU LLID mask. */
#define LL_DATA_HDR_LEN_MSK           0xFF    /*!< Data header length mask. BLE 4.2 data len extension allows 8 bits. */

#define LL_MAX_NUM_CHAN_DATA          37      /*!< Maximum number of used data channels. */
#define LL_MIN_NUM_CHAN_DATA          2       /*!< Minimum number of used data channels. */
/* --- Core Spec Sydney --- */
#define LL_SUBRATE_REQ_LEN            11      /*!< LL Connection subrating request PDU length. */
#define LL_SUBRATE_IND_LEN            11      /*!< LL Connection subrating indication PDU length. */
#define LL_CH_REPORTING_LEN           4       /*!< Channel class reporting indication PDU length. */
#define LL_CH_STATUS_LEN              11      /*!< Channel status indication PDU length. */

/*! \brief      Data PDU LLID types. */
enum
{
  LL_LLID_VS_PDU                    = 0x00,   /*!< Vendor specific PDU. */
  /* N.B. next two enumerations intentionally use identical values. */
  LL_LLID_EMPTY_PDU                 = 0x01,   /*!< Empty PDU. */
  LL_LLID_CONT_PDU                  = 0x01,   /*!< Data PDU: continuation fragment of an L2CAP message. */
  LL_LLID_START_PDU                 = 0x02,   /*!< Data PDU: start of an L2CAP message or a complete L2CAP message with no fragmentation. */
  LL_LLID_CTRL_PDU                  = 0x03,   /*!< Control PDU. */
};

/*** Encryption ***/

#define LL_ECC_KEY_LEN                32      /*!< ECC key length. */

#define LL_DEF_RES_ADDR_TO_SEC        900     /*!< Default resolvable address timeout in seconds. */

#define LL_RAND_LEN                   8       /*!< Length of random number */
#define LL_KEY_LEN                    16      /*!< Encryption key length. */
#define LL_SKD_LEN                    LL_KEY_LEN      /*!< Session key diversifier length. */
#define LL_IV_LEN                     8       /*!< Initialization vector length. */
#define LL_BC_LEN                     LL_KEY_LEN      /*!< Broadcast code length. */
#define LL_GIV_LEN                    8       /*!< GIV length. */
#define LL_GSKD_LEN                   16      /*!< GSKD code length. */

#define LL_DEF_AUTH_TO_MS             30000   /*!< Default authentication timeout in milliseconds. */

/*** LLCP ***/

#define LL_DATA_LEN_TO_TIME_1M(len, enc)        ((LL_BLE_US_PER_BYTE_1M * ((len) + ((enc) ? LL_DATA_MIC_LEN : 0))) + LL_MIN_PKT_TIME_US_1M)
                                              /*!< Convert data length to time. */
#define LL_DATA_LEN_TO_TIME_2M(len, enc)        ((LL_BLE_US_PER_BYTE_2M * ((len) + ((enc) ? LL_DATA_MIC_LEN : 0))) + LL_MIN_PKT_TIME_US_2M)
                                              /*!< Convert data length to time. */
#define LL_DATA_LEN_TO_TIME_CODED_S8(len, enc)  ((LL_BLE_US_PER_BYTE_CODED_S8 * ((len) + ((enc) ? LL_DATA_MIC_LEN : 0))) + LL_MIN_PKT_TIME_US_CODED_S8)
                                              /*!< Convert data length to time. */
#define LL_DATA_LEN_TO_TIME_CODED_S2(len, enc)  ((LL_BLE_US_PER_BYTE_CODED_S2 * ((len) + ((enc) ? LL_DATA_MIC_LEN : 0))) + LL_MIN_PKT_TIME_US_CODED_S2)
                                              /*!< Convert data length to time. */

#define LL_MIN_INSTANT                6       /*!< Minimum number of CE to apply a CONN_UPD or CHAN_MAP. */

#define LL_MAX_ADV_DATA_LEN           1650    /*!< Maximum advertising data length. */

#define LL_MAX_DATA_LEN_MIN           27      /*!< Minimum value for maximum Data PDU length */
#define LL_MAX_DATA_LEN_ABS_MAX       251     /*!< Absolute maximum limit for maximum Data PDU length */

#define LL_MAX_DATA_TIME_MIN          328     /*!< Minimum value for maximum Data PDU time */
#define LL_MAX_DATA_TIME_ABS_MAX      17040   /*!< Absolute maximum limit for maximum Data PDU time */
#define LL_MAX_DATA_TIME_ABS_MAX_1M   2120    /*!< Absolute maximum limit for maximum Data PDU time (LE 1M PHY). Core Spec Table 4.5: 2128 if CTE is supported, else 2120. */
#define LL_MAX_DATA_TIME_ABS_MIN_CODED  2704  /*!< Absolute minimum limit for maximum Data PDU time (CODED PHY) */

#define LL_T_PRT_SEC                  40      /*!< LLCP procedure response timeout in seconds. */

#define LL_MAX_ADV_DLY_MS             10      /*!< Maximum advertising delay in milliseconds. */

#define LL_MIN_CONN_INTERVAL          6       /*!< Minimum value for connection interval. */
#define LL_MAX_CONN_INTERVAL          3200    /*!< Maximum value for connection interval. */

#define LL_MIN_TX_WIN_SIZE            1       /*!< Minimum value for transmit window size. */
#define LL_MAX_TX_WIN_SIZE            8       /*!< Maximum value for transmit window size. */

#define LL_MAX_CONN_LATENCY           499     /*!< Maximum value for connection slave latency. */

#define LL_MIN_SUP_TIMEOUT            10      /*!< Minimum value for connection supervision timeout. */
#define LL_MAX_SUP_TIMEOUT            3200    /*!< Maximum value for connection supervision timeout. */

#define LL_MIN_POWER_THRESHOLD        -128    /*!< Minimum value for power threshold. */
#define LL_MAX_POWER_THRESHOLD        127     /*!< Maximum value for power threshold. */

#define LL_MAX_PHYS                   3       /*!< Number of LE PHYs. */
#define LL_ALL_PHYS_MSK               0x7     /*!< All supported LE PHYs mask. */

/*** ISO ***/

#define LL_ISO_DATA_HDR_LEN           2       /*!< ISO Data PDU header length. */
#define LL_ISO_DATA_PLD_MAX_LEN       251     /*!< Maximum ISO Data PDU payload length */
#define LL_ISO_PDU_MAX_LEN            (HCI_ISO_HDR_LEN + BB_DATA_PLD_MAX_LEN + BB_DATA_PDU_TAILROOM)
                                              /*!< Maximum ISO Data PDU length. */

#define LL_ISO_SEG_HDR_LEN            2       /*!< Segmentation header length. */
#define LL_ISO_SEG_TO_LEN             3       /*!< Segmentation Time Offset length. */

#define LL_MAX_CIS_COUNT              0x10    /*!< Maximum count for CIS. */

#define LL_MIN_CIG_ID                 0x00    /*!< Minimum value for CIG ID. */
#define LL_MAX_CIG_ID                 0xEF    /*!< Maximum value for CIG ID. */

#define LL_MIN_CIS_ID                 0x00    /*!< Minimum value for CIS ID. */
#define LL_MAX_CIS_ID                 0xEF    /*!< Maximum value for CIS ID. */

#define LL_MIN_ISO_INTERV             0x0004  /*!< Minimum value for ISO interval. */
#define LL_MAX_ISO_INTERV             0x0C80  /*!< Maximum value for ISO interval. */

#define LL_MIN_ISOAL_PDU_TYPE         0x00    /*!< Minimum value for ISOAL PDU type. */
#define LL_MAX_ISOAL_PDU_TYPE         0x01    /*!< Maximum value for ISOAL PDU type. */

#define LL_MIN_SDU_SIZE               0x000   /*!< Minimum value for SDU size. */
#define LL_MAX_SDU_SIZE               0xFFF   /*!< Maximum value for SDU size. */

#define LL_MIN_SDU_INTERV             0x000FF /*!< Minimum value for SDU interval. */
#define LL_MAX_SDU_INTERV             0xFFFFF /*!< Maximum value for SDU interval. */

#define LL_MIN_CIS_NSE                0x01    /*!< Minimum value for CIS number of subevent. */
#define LL_MAX_CIS_NSE                0x1F    /*!< Maximum value for CIS number of subevent. */

#define LL_MIN_CIS_PL                 0x0000  /*!< Minimum value for CIS payload. */
#define LL_MAX_CIS_PL                 0x0FFB  /*!< Maximum value for CIS payload. */

#define LL_MIN_CIS_TRANS_LAT          0x0005  /*!< Minimum value for CIS transport latency. */
#define LL_MAX_CIS_TRANS_LAT          0x0FA0  /*!< Maximum value for CIS transport latency. */

#define LL_MIN_CIS_PHY_BIT            0x00    /*!< Minimum value for CIS PHY bit. */
#define LL_MAX_CIS_PHY_BIT            0x02    /*!< Maximum value for CIS PHY bit. */

#define LL_MIN_CIS_FT                 0x01    /*!< Minimum value for CIS flush time. */
#define LL_MAX_CIS_FT                 0x1F    /*!< Maximum value for CIS flush time. */

#define LL_MIN_CIS_BN                 0x00    /*!< Minimum value for CIS burst number. */
#define LL_MAX_CIS_BN                 0x10    /*!< Maximum value for CIS burst number. */

#define LL_MIN_CIS_RTN                0x00    /*!< Minimum value for CIS retransmission number. */
#define LL_MAX_CIS_RTN                0x0F    /*!< Maximum value for CIS retransmission number. */

/*! \brief      ISO PDU LLID types. */
typedef enum
{
  /* ISO */
  LL_LLID_ISO_UNF_END_PDU           = 0x00,   /*!< Unframed ISO Data PDU; end fragment of a SDU. */
  LL_LLID_ISO_UNF_CONT_PDU          = 0x01,   /*!< Unframed ISO Data PDU; start or continuation fragment of a SDU. */
  LL_LLID_ISO_FRA_PDU               = 0x02,   /*!< Framed ISO Data PDU; one or more segments of a SDU. */
  LL_LLID_ISO_EMPTY_PDU             = 0x01,   /*!< ISO Empty PDU. */

  /* BIS */
  LL_LLID_BIG_CTRL_PDU              = 0x03    /*!< BIG Control PDU. */
} LlIsoLlid_t;

/*! \brief      ISO PDU type. */
typedef enum
{
  LL_ISO_PDU_TYPE_UNFRAMED          = 0x00,   /*!< Unframed PDU type. */
  LL_ISO_PDU_TYPE_FRAMED            = 0x01    /*!< Framed PDU type. */
} LlFraming_t;

/*! \brief      ISO test payload types. */
enum
{
  LL_ISO_TEST_PL_LEN_ZERO           = 0x00,   /*!< Zero length test payload */
  LL_ISO_TEST_PL_LEN_VAR            = 0x01,   /*!< Variable length test payload */
  LL_ISO_TEST_PL_LEN_MAX            = 0x02    /*!< Maximum length test payload */
};

/*! \brief      ISOAL SDU assembly states. */
enum
{
  LL_ISO_SDU_STATE_NEW              = 0x00,   /*!< New SDU state. */
  LL_ISO_SDU_STATE_CONT             = 0x01    /*!< SDU pending state. */
};

#define LL_ISO_TEST_VAR_MIN_LEN       4 /*!< Minimum length test payload. */

#define LL_ISO_TRANSPORT_LAT_MIN      0x0000EA /*!< Minimum transport latency. */

/*** DTM ***/

#define LL_DTM_HDR_LEN                2                       /*!< Direct Test Mode PDU header length. */
#define LL_DTM_SYNC_WORD              UINT32_C(0x71764129)    /*!< Direct Test Mode sync word. */
#define LL_DTM_CRC_INIT               UINT32_C(0x555555)      /*!< Direct Test Mode CRC initial value. */
#define LL_DTM_MAX_INT_US             12500   /*!< Maximum time interval between packets in microseconds. */
#define LL_DTM_PDU_ABS_MAX_LEN        255     /*!< Absolute maximum DTM PDU length. */
#define LL_DTM_MAX_CHAN_IDX           39      /*!< Maximum channel index. */

/*** Baseband ***/

#define LL_CHAN_DATA_MIN_IDX          0       /*!< Minimum data channel index. */
#define LL_CHAN_DATA_MAX_IDX          36      /*!< Maximum data channel index. */
#define LL_CHAN_DATA_ALL              UINT64_C(0x0000001FFFFFFFFF)    /*!< Maximum data channel index. */

#define LL_BLE_BIT_PER_US             1       /*!< BLE PHY rate. */
#define LL_BLE_US_PER_BYTE_1M         8       /*!< BLE PHY speed (LE 1M PHY). */
#define LL_BLE_US_PER_BYTE_2M         4       /*!< BLE PHY speed (LE 2M PHY). */
#define LL_BLE_US_PER_BYTE_CODED_S8   64      /*!< BLE PHY speed (LE Coded PHY, S=8). */
#define LL_BLE_US_PER_BIT_CODED_S8    8       /*!< BLE PHY speed (LE Coded PHY, S=8). */
#define LL_BLE_US_PER_BYTE_CODED_S2   16      /*!< BLE PHY speed (LE Coded PHY, S=2). */
#define LL_BLE_US_PER_BIT_CODED_S2    2       /*!< BLE PHY speed (LE Coded PHY, S=2). */
#define LL_BLE_TIFS_US                150     /*!< BLE inter-frame space. */
#define LL_BLE_MAFS_US                300     /*!< BLE minimum AUX frame space. */
#define LL_BLE_US_PER_TICK            625     /*!< Microseconds per BLE tick. */
#define LL_BLE_TMSS_US                150     /*!< BLE minimum subevent space. */

#define LL_MIN_PKT_TIME_US_1M         80      /*!< Minimum packet time (i.e. empty PDU) on LE 1M PHY. */
#define LL_MIN_PKT_TIME_US_2M         44      /*!< Minimum packet time (i.e. empty PDU) on LE 2M PHY. */
#define LL_MIN_PKT_TIME_US_CODED_S8   720     /*!< Minimum packet time (i.e. empty PDU) on LE Coded PHY, S=8. */
#define LL_MIN_PKT_TIME_US_CODED_S2   462     /*!< Minimum packet time (i.e. empty PDU) on LE Coded PHY, S=2. */

#define LL_MIN_ADV_TX_PWR_LVL         -20     /*!< Minimum Tx power level for advertising. */
#define LL_MAX_ADV_TX_PWR_LVL         10      /*!< Maximum Tx power level for advertising. */

#define LL_MIN_TX_PWR_LVL             -30     /*!< Minimum Tx power level for connections. */
#define LL_MAX_TX_PWR_LVL             20      /*!< Maximum Tx power level for connections. */

#define LL_MAX_TIFS_DEVIATION         2       /*!< Maximum TIFS deviation in microseconds. */

#define LL_WW_RX_DEVIATION_USEC       16      /*!< Rx deviation in microseconds for window widening. */

#define LL_30_USEC_OFFS_MAX_USEC      245730  /*!< Maximum value for 30 microseconds offset unit in microseconds. */

/*** ACAD ***/

/*! \brief      ACAD opcodes. */
enum
{
  LL_ACAD_OPCODE_CHAN_MAP_UPD = 0x28,         /*!< Opcode for ACAD Channel Map Update Indication. */
  LL_ACAD_OPCODE_BIG_INFO     = 0x2C          /*!< Opcode for ACAD BIG Info. */
};

#define LL_ACAD_OPCODE_LEN            1       /*!< Length of a single ACAD opcode. */
#define LL_ACAD_LEN_FIELD_LEN         1       /*!< Length of ACAD length field */
#define LL_ACAD_DATA_FIELD_MAX_LEN    (LL_EXT_HDR_ACAD_MAX_LEN - LL_ACAD_OPCODE_LEN - LL_ACAD_LEN_FIELD_LEN)
                                              /*!< Length of max ACAD field length without opcode and length field */
#define LL_ACAD_CHAN_MAP_UPD_LEN      8       /*!< Length of ACAD update periodic channel map data field. */
#define LL_ACAD_BIG_INFO_UNENCRPT_LEN 33      /*!< Length of ACAD for an unencrypted BIG Info. */
#define LL_ACAD_BIG_INFO_ENCRPT_LEN   57      /*!< Length of ACAD for an encrypted BIG Info. */

/*** BIG Control PDUs ***/

/*! \brief      BIG Control opcodes. */
enum
{
  LL_BIG_OPCODE_CHAN_MAP_IND = 0x00,          /*!< Opcode for BIG Channel Map Indication. */
  LL_BIG_OPCODE_BIG_TERM_IND = 0x01,          /*!< Opcode for BIG Terminate Indication. */
  LL_BIG_OPCODE_MAX                           /*!< Maximum number of BIG opcodes. */
};

#define LL_BIG_OPCODE_LEN             1       /*!< Length of a BIG Control PDU opcode. */

#define LL_BIG_CHAN_MAP_IND_PDU_LEN   7       /*!< BIG Channel Map indication PDU length. */
#define LL_BIG_TERMINATE_IND_PDU_LEN  3       /*!< BIG Terminate indication PDU length. */

#define LL_BIG_MIN_INSTANT            6       /*!< Minimum number of BIG Events to apply a BIG Control PDU. */

/*** Modify Sleep Clock Accuracy ***/

/*! \brief      Action parameter. */
enum
{
  LL_MODIFY_SCA_MORE_ACCURATE = 0x00,         /*!< Modify to more accurate clock accuracy. */
  LL_MODIFY_SCA_LESS_ACCURATE = 0x01,         /*!< Modify to less accurate clock accuracy. */
  LL_MODIFY_SCA_NO_ACTION                     /*!< No action (Used for request sca tester command). */
};

#define LL_SCA_MIN_INDEX              0       /*!< Index for lowest accuracy clock. */
#define LL_SCA_MAX_INDEX              7       /*!< Index for highest accuracy clock. */

/*** Power Control ***/

/*! \brief      Power control Limit field. */
#define LL_PWR_CONTROL_LIMIT_MIN_BIT  (1 << 0)  /*!< txPower is at it's minimum limit. */
#define LL_PWR_CONTROL_LIMIT_MAX_BIT  (1 << 1)  /*!< txPower is at it's maximum limit. */

/*! \brief       Power control APR field. */
#define LL_PWR_CTRL_APR_UNDEF         0xFF    /*!< Undefined power reduction limit. */

/*! \brief       Power control txPower field. */
#define LL_PWR_CTRL_TXPOWER_MAX         0x7F  /*!< txPower symbol for maximum txPower. */
#define LL_PWR_CTRL_TXPOWER_MIN         0x7E  /*!< txPower symbol for minimum txPower. */
#define LL_PWR_CTRL_TXPOWER_UNAVAILABLE 127   /*!< Power is unavailable for this PHY. */
#define LL_PWR_CTRL_TXPOWER_UNMANAGED   126   /*!< Power is unmanaged for this PHY. */

/*! \brief   Path loss monitoring zones. */
enum
{
  LL_PATH_LOSS_ZONE_LOW = 0x00,               /*!< Low path loss zone. */
  LL_PATH_LOSS_ZONE_MID,                      /*!< Medium path loss zone. */
  LL_PATH_LOSS_ZONE_HIGH                      /*!< High path loss zone. */
};

/*! brief    Power control reporting reasons table. */
enum
{
  LL_POWER_REPORT_REASON_LOCAL,               /*!< Local power change report. */
  LL_POWER_REPORT_REASON_REMOTE,              /*!< Remote power change report. */
  LL_POWER_REPORT_REASON_READ_REMOTE          /*!< Remote transmit power read report. */
};

/*** ISO ***/

#define LL_ISOAL_SEG_HDR_MASK_SC      0x01    /*!< Mask for segmentation header start/continuation bit. */
#define LL_ISOAL_SEG_HDR_MASK_CMPLT   0x02    /*!< Mask for segmentation header complete bit. */

/*! \brief  Codec transport types. */
enum
{
  LL_CODEC_TRANS_CIS_BIT = (1 << 2),          /*!< Codec supported over LE CIS. */
  LL_CODEC_TRANS_BIS_BIT = (1 << 3)           /*!< Codec supported over LE BIS. */
};

/*** Channel classification ***/

#define LL_CH_RPT_SPACING_MIN            5    /*!< Minimum value for minimum spacing and max delay parameter. */
#define LL_CH_RPT_SPACING_DEFAULT       10    /*!< Default spacing for channel reporting indication. */
#define LL_CH_RPT_SPACING_MAX          150    /*!< Maximum value for minimum spacing and max delay parameter. */

#define LL_CH_CLASS_BAD                  3    /*!< Channel classification is bad. */
#define LL_CH_CLASS_UNKNOWN              0    /*!< Channel classification is unknown. */

/*! \brief      Minimum and maximum time between LL_CHANNEL_STATUS_IND reports in 200ms units. */
enum
{
  LCTR_MIN_CH_STATUS_INTERVAL        = 5,     /*!< Channel class reporting minimum (1 second). */
  LCTR_MAX_CH_STATUS_INTERVAL        = 150,   /*!< Channel class reporting maximum (30 seconds). */
};

/*** Enhanced connection update ***/

/*! \brief      Minimum and maximum subrate. */
enum
{
  LCTR_MIN_SUBRATE                   = 1,     /*!< Minimum subrate. */
  LCTR_MAX_SUBRATE                   = 500    /*!< Maximum subrate. */
};

#ifdef __cplusplus
};
#endif

#endif /* LL_DEFS_H */
