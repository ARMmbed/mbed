/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer interface file.
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

#ifndef LL_API_H
#define LL_API_H

#include "wsf_types.h"
#include "wsf_os.h"
#include "util/bda.h"
#include "cfg_mac_ble.h"
#include "ll_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief  Version number. */
#define LL_VER_NUM          21015

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      The following status values are used in the LL API. */
typedef enum
{
  LL_SUCCESS                                            = 0x00,
  LL_ERROR_CODE_UNKNOWN_HCI_CMD                         = 0x01,
  LL_ERROR_CODE_UNKNOWN_CONN_ID                         = 0x02,
  LL_ERROR_CODE_HW_FAILURE                              = 0x03,
  LL_ERROR_CODE_PAGE_TIMEOUT                            = 0x04,
  LL_ERROR_CODE_AUTH_FAILURE                            = 0x05,
  LL_ERROR_CODE_PIN_KEY_MISSING                         = 0x06,
  LL_ERROR_CODE_MEM_CAP_EXCEEDED                        = 0x07,
  LL_ERROR_CODE_CONN_TIMEOUT                            = 0x08,
  LL_ERROR_CODE_CONN_LIMIT_EXCEEDED                     = 0x09,
  LL_ERROR_CODE_SYNCH_CONN_LIMIT_EXCEEDED               = 0x0A,
  LL_ERROR_CODE_ACL_CONN_ALREADY_EXISTS                 = 0x0B,
  LL_ERROR_CODE_CMD_DISALLOWED                          = 0x0C,
  LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES              = 0x0D,
  LL_ERROR_CODE_CONN_REJECTED_SECURITY_REASONS          = 0x0E,
  LL_ERROR_CODE_CONN_REJECTED_UNACCEPTABLE_BDADDR       = 0x0F,
  LL_ERROR_CODE_CONN_ACCEPT_TIMEOUT_EXCEEDED            = 0x10,
  LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE         = 0x11,
  LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS                  = 0x12,
  LL_ERROR_CODE_REMOTE_USER_TERM_CONN                   = 0x13,
  LL_ERROR_CODE_REMOTE_DEVICE_TERM_CONN_LOW_RESOURCES   = 0x14,
  LL_ERROR_CODE_REMOTE_DEVICE_TERM_CONN_POWER_OFF       = 0x15,
  LL_ERROR_CODE_CONN_TERM_BY_LOCAL_HOST                 = 0x16,
  LL_ERROR_CODE_REPEATED_ATTEMPTS                       = 0x17,
  LL_ERROR_CODE_PAIRING_NOT_ALLOWED                     = 0x18,
  LL_ERROR_CODE_UNKNOWN_LMP_PDU                         = 0x19,
  LL_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE              = 0x1A,
  LL_ERROR_CODE_SCO_OFFSET_REJ                          = 0x1B,
  LL_ERROR_CODE_SCO_INTERVAL_REJ                        = 0x1C,
  LL_ERROR_CODE_SCO_AIR_MODE_REJ                        = 0x1D,
  LL_ERROR_CODE_INVALID_LMP_PARAMS                      = 0x1E,
  LL_ERROR_CODE_UNSPECIFIED_ERROR                       = 0x1F,
  LL_ERROR_CODE_UNSUPPORTED_LMP_PARAM_VAL               = 0x20,
  LL_ERROR_CODE_ROLE_CHANGE_NOT_ALLOWED                 = 0x21,
  LL_ERROR_CODE_LMP_LL_RESP_TIMEOUT                     = 0x22,
  LL_ERROR_CODE_LMP_ERR_TRANSACTION_COLLISION           = 0x23,
  LL_ERROR_CODE_LMP_PDU_NOT_ALLOWED                     = 0x24,
  LL_ERROR_CODE_ENCRYPT_MODE_NOT_ACCEPTABLE             = 0x25,
  LL_ERROR_CODE_LINK_KEY_CAN_NOT_BE_CHANGED             = 0x26,
  LL_ERROR_CODE_REQ_QOS_NOT_SUPPORTED                   = 0x27,
  LL_ERROR_CODE_INSTANT_PASSED                          = 0x28,
  LL_ERROR_CODE_PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED     = 0x29,
  LL_ERROR_CODE_DIFFERENT_TRANSACTION_COLLISION         = 0x2A,
  LL_ERROR_CODE_RESERVED1                               = 0x2B,
  LL_ERROR_CODE_QOS_UNACCEPTABLE_PARAM                  = 0x2C,
  LL_ERROR_CODE_QOS_REJ                                 = 0x2D,
  LL_ERROR_CODE_CHAN_ASSESSMENT_NOT_SUPPORTED           = 0x2E,
  LL_ERROR_CODE_INSUFFICIENT_SECURITY                   = 0x2F,
  LL_ERROR_CODE_PARAM_OUT_OF_MANDATORY_RANGE            = 0x30,
  LL_ERROR_CODE_RESERVED2                               = 0x31,
  LL_ERROR_CODE_ROLE_SWITCH_PENDING                     = 0x32,
  LL_ERROR_CODE_RESERVED3                               = 0x33,
  LL_ERROR_CODE_RESERVED_SLOT_VIOLATION                 = 0x34,
  LL_ERROR_CODE_ROLE_SWITCH_FAILED                      = 0x35,
  LL_ERROR_CODE_EXTENDED_INQUIRY_RESP_TOO_LARGE         = 0x36,
  LL_ERROR_CODE_SIMPLE_PAIRING_NOT_SUPPORTED_BY_HOST    = 0x37,
  LL_ERROR_CODE_HOST_BUSY_PAIRING                       = 0x38,
  LL_ERROR_CODE_CONN_REJ_NO_SUITABLE_CHAN_FOUND         = 0x39,
  LL_ERROR_CODE_CONTROLLER_BUSY                         = 0x3A,
  LL_ERROR_CODE_UNACCEPTABLE_CONN_INTERVAL              = 0x3B,
  LL_ERROR_CODE_ADV_TIMEOUT                             = 0x3C,
  LL_ERROR_CODE_CONN_TERM_MIC_FAILURE                   = 0x3D,
  LL_ERROR_CODE_CONN_FAILED_TO_ESTABLISH                = 0x3E,
  LL_ERROR_CODE_MAC_CONN_FAILED                         = 0x3F,
  LL_ERROR_CODE_COARSE_CLK_ADJ_REJ                      = 0x40,
  LL_ERROR_CODE_TYPE0_SUBMAP_NOT_DEF                    = 0x41,
  LL_ERROR_CODE_UNKNOWN_ADV_ID                          = 0x42,
  LL_ERROR_CODE_LIMIT_REACHED                           = 0x43,
  LL_ERROR_CODE_OP_CANCELLED_BY_HOST                    = 0x44,
  LL_ERROR_CODE_PKT_TOO_LONG                            = 0x45
} LlStatus_t;

/*! \addtogroup LL_API_INIT
 *  \{ */

/*! \brief      LL runtime configuration parameters. */
typedef struct
{
  /* Device */
  uint16_t  compId;                 /*!< Company ID (default to ARM Ltd. ID). */
  uint16_t  implRev;                /*!< Implementation revision number. */
  uint8_t   btVer;                  /*!< Core specification implementation level (LL_VER_BT_CORE_SPEC_4_2). */
  uint32_t  _align32;               /*!< Unused. Align next field to word boundary. */
  /* Advertiser */
  uint8_t   maxAdvSets;             /*!< Maximum number of advertising sets. */
  uint8_t   maxAdvReports;          /*!< Maximum number of pending legacy or extended advertising reports. */
  uint16_t  maxExtAdvDataLen;       /*!< Maximum extended advertising data size. */
  uint8_t   defExtAdvDataFrag;      /*!< Default extended advertising data fragmentation size. */
  uint16_t  auxDelayUsec;           /*!< Auxiliary Offset delay above T_MAFS in microseconds. */
  uint16_t  auxPtrOffsetUsec;       /*!< Delay of auxiliary packet in microseconds from the time specified by auxPtr. */
  /* Scanner */
  uint8_t   maxScanReqRcvdEvt;      /*!< Maximum scan request received events. */
  uint16_t  maxExtScanDataLen;      /*!< Maximum extended scan data size. */
  /* Connection */
  uint8_t   maxConn;                /*!< Maximum number of connections. */
  uint8_t   numTxBufs;              /*!< Default number of transmit buffers. */
  uint8_t   numRxBufs;              /*!< Default number of receive buffers. */
  uint16_t  maxAclLen;              /*!< Maximum ACL buffer size. */
  int8_t    defTxPwrLvl;            /*!< Default Tx power level for connections. */
  uint8_t   ceJitterUsec;           /*!< Allowable CE jitter on a slave (account for master's sleep clock resolution). */
  /* ISO */
  uint8_t   numIsoTxBuf;            /*!< Default number of ISO transmit buffers. */
  uint8_t   numIsoRxBuf;            /*!< Default number of ISO receive buffers. */
  uint16_t  maxIsoSduLen;           /*!< Maximum ISO SDU size between host and controller. */
  uint16_t  maxIsoPduLen;           /*!< Maximum ISO Data PDU buffer size. */
  /* CIS */
  uint8_t   maxCig;                 /*!< Maximum number of CIG. */
  uint8_t   maxCis;                 /*!< Maximum number of CIS, it is shared by the CIGs. */
  uint16_t  cisSubEvtSpaceDelay;    /*!< Subevent spacing above T_MSS in microsecond. */
  /* BIS */
  uint8_t   maxBig;                 /*!< Maximum number of BIG. */
  uint8_t   maxBis;                 /*!< Maximum number of BIS. */
  /* DTM */
  uint16_t  dtmRxSyncMs;            /*!< DTM Rx synchronization window in milliseconds. */
  /* PHY */
  bool_t    phy2mSup;               /*!< 2M PHY supported. */
  bool_t    phyCodedSup;            /*!< Coded PHY supported. */
  bool_t    stableModIdxTxSup;      /*!< Tx stable modulation index supported. */
  bool_t    stableModIdxRxSup;      /*!< Rx stable modulation index supported. */
  /* Power control */
  int8_t   pcHighThreshold;         /*!< High RSSI threshold for power monitoring. */
  int8_t   pcLowThreshold;          /*!< Low RSSI threshold for power monitoring. */
  /* Channel classification reporting. */
  uint8_t   chClassIntSpacing;      /*!< Interval spacing of channel classification reporting. */
} LlRtCfg_t;

/*! \} */    /* LL_API_INIT */

/*! \addtogroup LL_API_DEVICE
 *  \{ */

/* The supported state bitmask indicates the LE states supported by the LL. */
#define LL_SUP_STATE_NON_CONN_ADV                  (UINT64_C(1) <<  0)    /*!< Non-connectable Advertising State supported. */
#define LL_SUP_STATE_SCAN_ADV                      (UINT64_C(1) <<  1)    /*!< Scannable Advertising State supported. */
#define LL_SUP_STATE_CONN_ADV                      (UINT64_C(1) <<  2)    /*!< Connectable Advertising State supported. */
#define LL_SUP_STATE_HI_DUTY_DIR_ADV               (UINT64_C(1) <<  3)    /*!< High Duty Cycle Directed Advertising State supported. */
#define LL_SUP_STATE_PASS_SCAN                     (UINT64_C(1) <<  4)    /*!< Passive Scanning State supported. */
#define LL_SUP_STATE_ACT_SCAN                      (UINT64_C(1) <<  5)    /*!< Active Scanning State supported. */
#define LL_SUP_STATE_INIT                          (UINT64_C(1) <<  6)    /*!< Initiating State supported. Connection State in the Master Role supported is also supported. */
#define LL_SUP_STATE_CONN_SLV                      (UINT64_C(1) <<  7)    /*!< Connection State in the Slave Role supported. */
#define LL_SUP_STATE_NON_CONN_ADV_AND_PASS_SCAN    (UINT64_C(1) <<  8)    /*!< Non-connectable Advertising State and Passive Scanning State combination supported. */
#define LL_SUP_STATE_SCAN_ADV_AND_PASS_SCAN        (UINT64_C(1) <<  9)    /*!< Scannable Advertising State and Passive Scanning State combination supported. */
#define LL_SUP_STATE_CONN_ADV_AND_PASS_SCAN        (UINT64_C(1) << 10)    /*!< Connectable Advertising State and Passive Scanning State combination supported. */
#define LL_SUP_STATE_HI_DUTY_DIR_ADV_AND_PASS_SCAN (UINT64_C(1) << 11)    /*!< Directed Advertising State and Passive Scanning State combination supported. */
#define LL_SUP_STATE_NON_CONN_ADV_AND_ACT_SCAN     (UINT64_C(1) << 12)    /*!< Non-connectable Advertising State and Active Scanning State combination supported. */
#define LL_SUP_STATE_SCAN_ADV_AND_ACT_SCAN         (UINT64_C(1) << 13)    /*!< Scannable Advertising State and Active Scanning State combination supported. */
#define LL_SUP_STATE_CONN_ADV_AND_ACT_SCAN         (UINT64_C(1) << 14)    /*!< Connectable Advertising State and Active Scanning State combination supported. */
#define LL_SUP_STATE_HI_DUTY_DIR_ADV_ACT_SCAN      (UINT64_C(1) << 15)    /*!< Directed Advertising State and Active Scanning State combination supported. */
#define LL_SUP_STATE_NON_CONN_ADV_AND_INIT         (UINT64_C(1) << 16)    /*!< Non-connectable Advertising State and Initiating State combination supported. */
#define LL_SUP_STATE_SCAN_ADV_AND_INIT             (UINT64_C(1) << 17)    /*!< Scannable Advertising State and Initiating State combination supported */
#define LL_SUP_STATE_NON_CONN_ADV_MST              (UINT64_C(1) << 18)    /*!< Non-connectable Advertising State and Master Role combination supported. */
#define LL_SUP_STATE_SCAN_ADV_MST                  (UINT64_C(1) << 19)    /*!< Scannable Advertising State and Master Role combination supported. */
#define LL_SUP_STATE_NON_CONN_ADV_SLV              (UINT64_C(1) << 20)    /*!< Non-connectable Advertising State and Slave Role combination supported. */
#define LL_SUP_STATE_SCAN_ADV_SLV                  (UINT64_C(1) << 21)    /*!< Scannable Advertising State and Slave Role combination supported. */
#define LL_SUP_STATE_PASS_SCAN_AND_INIT            (UINT64_C(1) << 22)    /*!< Passive Scanning State and Initiating State combination supported. */
#define LL_SUP_STATE_ACT_SCAN_AND_INIT             (UINT64_C(1) << 23)    /*!< Active Scanning State and Initiating State combination supported. */
#define LL_SUP_STATE_PASS_SCAN_MST                 (UINT64_C(1) << 24)    /*!< Passive Scanning State and Master Role combination supported. */
#define LL_SUP_STATE_ACT_SCAN_MST                  (UINT64_C(1) << 25)    /*!< Active Scanning State and Master Role combination supported. */
#define LL_SUP_STATE_PASS_SCAN_SLV                 (UINT64_C(1) << 26)    /*!< Passive Scanning state and Slave Role combination supported. */
#define LL_SUP_STATE_ACT_SCAN_SLV                  (UINT64_C(1) << 27)    /*!< Active Scanning state and Slave Role combination supported. */
#define LL_SUP_STATE_INIT_MST                      (UINT64_C(1) << 28)    /*!< Initiating State and Master Role combination supported. Master Role and Master Role combination is also supported. */
#define LL_SUP_STATE_LO_DUTY_DIR_ADV               (UINT64_C(1) << 29)    /*!< Low Duty Cycle Directed Advertising State. */
#define LL_SUP_STATE_LO_DUTY_DIR_ADV_AND_PASS_SCAN (UINT64_C(1) << 30)    /*!< Low Duty Cycle Directed Advertising and Passive Scanning State combination supported. */
#define LL_SUP_STATE_LO_DUTY_DIR_ADV_AND_ACT_SCAN  (UINT64_C(1) << 31)    /*!< Low Duty Cycle Directed Advertising and Active Scanning State combination supported. */
#define LL_SUP_STATE_CONN_ADV_AND_INIT             (UINT64_C(1) << 32)    /*!< Connectable Advertising State and Initiating State combination supported. */
#define LL_SUP_STATE_HI_DUTY_DIR_ADV_AND_INIT      (UINT64_C(1) << 33)    /*!< High Duty Cycle Directed Advertising and Initiating combination supported. */
#define LL_SUP_STATE_LO_DUTY_DIR_ADV_AND_INIT      (UINT64_C(1) << 34)    /*!< Low Duty Cycle Directed Advertising and Initiating combination supported. */
#define LL_SUP_STATE_CONN_ADV_MST                  (UINT64_C(1) << 35)    /*!< Connectable Advertising State and Master Role combination supported. */
#define LL_SUP_STATE_HI_DUTY_DIR_ADV_MST           (UINT64_C(1) << 36)    /*!< High Duty Cycle Directed Advertising and Master Role combination supported. */
#define LL_SUP_STATE_LO_DUTY_DIR_ADV_MST           (UINT64_C(1) << 37)    /*!< Low Duty Cycle Directed Advertising and Master Role combination supported. */
#define LL_SUP_STATE_CONN_ADV_SLV                  (UINT64_C(1) << 38)    /*!< Connectable Advertising State and Slave Role combination supported. */
#define LL_SUP_STATE_HI_DUTY_DIR_ADV_SLV           (UINT64_C(1) << 39)    /*!< High Duty Cycle Directed Advertising and Slave Role combination supported. */
#define LL_SUP_STATE_LO_DUTY_DIR_ADV_SLV           (UINT64_C(1) << 40)    /*!< Low Duty Cycle Directed Advertising and Slave Role combination supported. */
#define LL_SUP_STATE_INIT_SLV                      (UINT64_C(1) << 41)    /*!< Initiating State and Slave Role combination. */

/*! The features bitmask indicates the LE features supported by the LL. */

/* --- Core Spec 4.0 --- */
#define LL_FEAT_ENCRYPTION                          (UINT64_C(1) <<  0)   /*!< Encryption supported. */
/* --- Core Spec 4.2 --- */
#define LL_FEAT_CONN_PARAM_REQ_PROC                 (UINT64_C(1) <<  1)   /*!< Connection Parameters Request Procedure supported. */
#define LL_FEAT_EXT_REJECT_IND                      (UINT64_C(1) <<  2)   /*!< Extended Reject Indication supported. */
#define LL_FEAT_SLV_INIT_FEAT_EXCH                  (UINT64_C(1) <<  3)   /*!< Slave-Initiated Features Exchange supported. */
#define LL_FEAT_LE_PING                             (UINT64_C(1) <<  4)   /*!< LE Ping supported. */
#define LL_FEAT_DATA_LEN_EXT                        (UINT64_C(1) <<  5)   /*!< Data Length Extension supported. */
#define LL_FEAT_PRIVACY                             (UINT64_C(1) <<  6)   /*!< LL Privacy supported. */
#define LL_FEAT_EXT_SCAN_FILT_POLICY                (UINT64_C(1) <<  7)   /*!< Extended Scan Filter Policy supported. */
/* --- Core Spec 5.0 --- */
#define LL_FEAT_LE_2M_PHY                           (UINT64_C(1) <<  8)   /*!< LE 2M PHY supported. */
#define LL_FEAT_STABLE_MOD_IDX_TRANSMITTER          (UINT64_C(1) <<  9)   /*!< Stable Modulation Index - Transmitter supported. */
#define LL_FEAT_STABLE_MOD_IDX_RECEIVER             (UINT64_C(1) << 10)   /*!< Stable Modulation Index - Receiver supported. */
#define LL_FEAT_LE_CODED_PHY                        (UINT64_C(1) << 11)   /*!< LE Coded PHY supported. */
#define LL_FEAT_LE_EXT_ADV                          (UINT64_C(1) << 12)   /*!< LE Extended Advertising supported. */
#define LL_FEAT_LE_PER_ADV                          (UINT64_C(1) << 13)   /*!< LE Periodic Advertising supported. */
#define LL_FEAT_CH_SEL_2                            (UINT64_C(1) << 14)   /*!< Channel Selection Algorithm #2 supported. */
#define LL_FEAT_LE_POWER_CLASS_1                    (UINT64_C(1) << 15)   /*!< LE Power Class 1 supported. */
#define LL_FEAT_MIN_NUM_USED_CHAN                   (UINT64_C(1) << 16)   /*!< Minimum Number of Used Channels supported. */
/* --- Core Spec 5.1 --- */
#define LL_FEAT_CONN_CTE_REQ                        (UINT64_C(1) << 17)   /*!< Connection CTE Request supported */
#define LL_FEAT_CONN_CTE_RSP                        (UINT64_C(1) << 18)   /*!< Connection CTE Response supported */
#define LL_FEAT_CONNLESS_CTE_TRANS                  (UINT64_C(1) << 19)   /*!< Connectionless CTE Transmitter supported */
#define LL_FEAT_CONNLESS_CTE_RECV                   (UINT64_C(1) << 20)   /*!< Connectionless CTE Receiver supported */
#define LL_FEAT_ANTENNA_SWITCH_AOD                  (UINT64_C(1) << 21)   /*!< Anetenna Switching during CTE Transmission (AoD) supported */
#define LL_FEAT_ANTENNA_SWITCH_AOA                  (UINT64_C(1) << 22)   /*!< Anetenna Switching during CTE Reception (AoA) supported */
#define LL_FEAT_RECV_CTE                            (UINT64_C(1) << 23)   /*!< Receive Constant Tone Extension supported */
#define LL_FEAT_PAST_SENDER                         (UINT64_C(1) << 24)   /*!< Periodic Advertising Sync Transfer – Sender supported. */
#define LL_FEAT_PAST_RECIPIENT                      (UINT64_C(1) << 25)   /*!< Periodic Advertising Sync Transfer – Recipient supported. */
#define LL_FEAT_SCA_UPDATE                          (UINT64_C(1) << 26)   /*!< Sleep Clock Accuracy Updates supported. */
#define LL_FEAT_REMOTE_PUB_KEY_VALIDATION           (UINT64_C(1) << 27)   /*!< Remote Public Key Validation supported. */
/* --- Core Spec 5.2 --- */
#define LL_FEAT_CIS_MASTER_ROLE                     (UINT64_C(1) << 28)   /*!< Connected Isochronous Stream Master Role supported. */
#define LL_FEAT_CIS_SLAVE_ROLE                      (UINT64_C(1) << 29)   /*!< Connected Isochronous Stream Slave Role supported. */
#define LL_FEAT_ISO_BROADCASTER                     (UINT64_C(1) << 30)   /*!< Isochronous Broadcaster Role supported. */
#define LL_FEAT_ISO_SYNC                            (UINT64_C(1) << 31)   /*!< Isochronous Synchronized Receiver Role supported. */
#define LL_FEAT_ISO_HOST_SUPPORT                    (UINT64_C(1) << 32)   /*!< Host support for ISO Channels. */
#define LL_FEAT_POWER_CONTROL_REQUEST               (UINT64_C(1) << 33)   /*!< Power control requests supported. */
#define LL_FEAT_POWER_CHANGE_IND                    (UINT64_C(1) << 34)   /*!< Power control power change indication supported. */
#define LL_FEAT_PATH_LOSS_MONITOR                   (UINT64_C(1) << 35)   /*!< Path loss monitoring supported. */
/* --- Core Spec Sydney --- */
#define LL_FEAT_PER_ADV_ADI_SUP                     (UINT64_C(1) << 36)   /*!< Periodic advertising ADI field supported. */
#define LL_FEAT_CONN_SUBRATE                        (UINT64_C(1) << 37)   /*!< Connection subrating supported. */
#define LL_FEAT_CONN_SUBRATE_HOST_SUPPORT           (UINT64_C(1) << 38)   /*!< Host support for connection subrating. */
#define LL_FEAT_CHANNEL_CLASSIFICATION              (UINT64_C(1) << 39)   /*!< Channel classification supported. */
/* Bits 56 - 62 are RFU for testing. */

/*! \brief      Feature bits controlled by the host. */
#define LL_HOST_CONTROLLED_FEAT                     (LL_FEAT_ISO_HOST_SUPPORT | LL_FEAT_CONN_SUBRATE_HOST_SUPPORT)

/*! \brief      All feature mask. */
#define LL_FEAT_ALL_MASK                            (UINT64_C(0x000000FFFFFFFFFF))

/*! \brief      This parameter identifies the device role. */
typedef enum
{
  LL_ROLE_MASTER                = 0,            /*!< Role is master. */
  LL_ROLE_SLAVE                 = 1             /*!< Role is slave. */
} LlRole_t;

/*! \brief      Operational mode flags. */
enum
{
  LL_OP_MODE_FLAG_ENA_VER_LLCP_STARTUP        = (1 << 0),   /*!< Perform version exchange LLCP at connection establishment. */
  LL_OP_MODE_FLAG_SLV_REQ_IMMED_ACK           = (1 << 1),   /*!< MD bit set if data transmitted. */
  LL_OP_MODE_FLAG_BYPASS_CE_GUARD             = (1 << 2),   /*!< Bypass end of CE guard. */
  LL_OP_MODE_FLAG_MST_RETX_AFTER_RX_NACK      = (1 << 3),   /*!< Master retransmits after receiving NACK. */
  LL_OP_MODE_FLAG_MST_IGNORE_CP_RSP           = (1 << 4),   /*!< Master ignores LL_CONNECTION_PARAM_RSP. */
  LL_OP_MODE_FLAG_MST_UNCOND_CP_RSP           = (1 << 5),   /*!< Master unconditionally accepts LL_CONNECTION_PARAM_RSP. */
                                                            /*!<   (LL_OP_MODE_FLAG_MST_IGNORE_CP_RSP must be cleared). */
  LL_OP_MODE_FLAG_ENA_LEN_LLCP_STARTUP        = (1 << 6),   /*!< Perform data length update LLCP at connection establishment. */
  LL_OP_MODE_FLAG_REQ_SYM_PHY                 = (1 << 7),   /*!< Require symmetric PHYs for connection. */
  LL_OP_MODE_FLAG_ENA_FEAT_LLCP_STARTUP       = (1 << 8),   /*!< Perform feature exchange LLCP at connection establishment. */
  LL_OP_MODE_FLAG_SLV_DELAY_LLCP_STARTUP      = (1 << 9),   /*!< Slave delays LLCP startup procedures. */
  LL_OP_MODE_FLAG_ENA_SLV_LATENCY_WAKEUP      = (1 << 10),  /*!< Enable slave latency wake up upon data pending. */
  LL_OP_MODE_FLAG_ENA_SLV_AUX_SCAN_RSP_ADI    = (1 << 11),  /*!< Enable ADI field for aux scan. */
  LL_OP_MODE_FLAG_ENA_MST_CIS_NULL_PDU        = (1 << 12),  /*!< Enable CIS master sends additional NULL PDU for acknowledge scheme. */
  LL_OP_MODE_FLAG_ENA_SLV_AUX_IND_ADVA        = (1 << 13),  /*!< AdvA will be included in AUX_ADV_IND instead of ADV_EXT_IND. */
  LL_OP_MODE_FLAG_ENA_ADV_CHAN_RAND           = (1 << 14),  /*!< Enable advertising channel randomization. */
  LL_OP_MODE_FLAG_DIS_POWER_MONITOR           = (1 << 15),  /*!< Disable power monitoring. */
  LL_OP_MODE_FLAG_ENA_BIS_RECV_DUP            = (1 << 16),  /*!< Enable BIS receive duplicates. */
  LL_OP_MODE_FLAG_ENA_ISO_LOST_NOTIFY         = (1 << 17),  /*!< Enable HCI ISO lost SDU notification. */
  LL_OP_MODE_FLAG_ENA_CH_RPT_LLCP_AFTER_FEAT  = (1 << 18),  /*!< Perform channel report LLCP after feature exchange. */
  LL_OP_MODE_FLAG_IGNORE_CRC_ERR_TS           = (1 << 19),  /*!< Ignore timestamp of Rx packet with CRC error. */
  LL_OP_MODE_FLAG_FORCE_CIS_CODED_PHY_S2      = (1 << 20),  /*!< Force CIS to use Coded PHY with S2 option. */
  /* diagnostics only */
  LL_OP_MODE_FLAG_ENA_ADV_DLY                 = (1 << 24),  /*!< Enable advertising delay. */
  LL_OP_MODE_FLAG_ENA_SCAN_BACKOFF            = (1 << 25),  /*!< Enable scan backoff. */
  LL_OP_MODE_FLAG_ENA_WW                      = (1 << 26),  /*!< Enable window widening. */
  LL_OP_MODE_FLAG_ENA_SLV_LATENCY             = (1 << 27),  /*!< Enable slave latency. */
  LL_OP_MODE_FLAG_ENA_LLCP_TIMER              = (1 << 28),  /*!< Enable LLCP timer. */
  LL_OP_MODE_FLAG_ENA_SUBRATE_CE              = (1 << 29)   /*!< Enable subrating connection events. */
};

/*! \} */    /* LL_API_DEVICE */

/*! \addtogroup LL_API_BROADCAST
 *  \{ */

/*! \brief      The advertising type indicates the connectable and discoverable nature of the advertising packets transmitted by a device. */
enum
{
  LL_ADV_CONN_UNDIRECT          = 0,            /*!< Connectable undirected advertising.  Peer devices can scan and connect to this device. */
  LL_ADV_CONN_DIRECT_HIGH_DUTY  = 1,            /*!< Connectable directed advertising, high duty cycle.  Only a specified peer device can connect to this device. */
  LL_ADV_SCAN_UNDIRECT          = 2,            /*!< Scannable undirected advertising.  Peer devices can scan this device but cannot connect. */
  LL_ADV_NONCONN_UNDIRECT       = 3,            /*!< Non-connectable undirected advertising.  Peer devices cannot scan or connect to this device. */
  LL_ADV_CONN_DIRECT_LOW_DUTY   = 4             /*!< Connectable directed advertising, low duty cycle.  Only a specified peer device can connect to this device. */
};

/*! \brief      The address type indicates whether an address is public or random. */
enum
{
  LL_ADDR_PUBLIC                = 0,            /*!< Public address. */
  LL_ADDR_RANDOM                = 1,            /*!< Random address. */
  LL_ADDR_PUBLIC_IDENTITY       = 2,            /*!< Public identity address. */
  LL_ADDR_RANDOM_IDENTITY       = 3,            /*!< Random (static) identity address. */
  LL_ADDR_RANDOM_UNRESOLVABLE   = 0xFE,         /*!< Un-resolvable random address. */
  LL_ADDR_ANONYMOUS             = 0xFF          /*!< Anonymous advertiser. */
};

#define LL_ADDR_RANDOM_BIT      LL_ADDR_RANDOM            /*!< Address type random or public bit. */
#define LL_ADDR_IDENTITY_BIT    LL_ADDR_PUBLIC_IDENTITY   /*!< Address type identity bit. */

/*! \brief      Advertising channel bit. */
enum
{
  LL_ADV_CHAN_37_BIT            = (1 << 0),     /*!< Advertising channel 37. */
  LL_ADV_CHAN_38_BIT            = (1 << 1),     /*!< Advertising channel 38. */
  LL_ADV_CHAN_39_BIT            = (1 << 2),     /*!< Advertising channel 39. */
  LL_ADV_CHAN_ALL               = 0x7,          /*!< All advertising channels. */
};

/*! \brief      Advertising filter policy. */
enum
{
  LL_ADV_FILTER_NONE            = 0,            /*!< Scan from any device. */
  LL_ADV_FILTER_SCAN_WL_BIT     = 1,            /*!< Scan from White List only. */
  LL_ADV_FILTER_CONN_WL_BIT     = 2,            /*!< Connect from While List only. */
  LL_ADV_FILTER_WL_ONLY         = 3             /*!< Scan and connect from While List only. */
};

/*! \brief      Advertising event properties. */
enum
{
  LL_ADV_EVT_PROP_CONN_ADV_BIT      = (1 << 0), /*!< Connectable advertising. */
  LL_ADV_EVT_PROP_SCAN_ADV_BIT      = (1 << 1), /*!< Scannable advertising. */
  LL_ADV_EVT_PROP_DIRECT_ADV_BIT    = (1 << 2), /*!< Directed advertising. */
  LL_ADV_EVT_PROP_HIGH_DUTY_ADV_BIT = (1 << 3), /*!< High Duty Cycle advertising. */
  LL_ADV_EVT_PROP_LEGACY_ADV_BIT    = (1 << 4), /*!< Use legacy advertising PDUs. */
  LL_ADV_EVT_PROP_OMIT_AA_BIT       = (1 << 5), /*!< Omit advertiser's address from all PDUs (anonymous advertising). */
  LL_ADV_EVT_PROP_TX_PWR_BIT        = (1 << 6)  /*!< Include TxPower in the advertising PDU. */
};

#define LL_ADV_EVT_PROP_NON_CONN_NON_SCAN   0   /*!< Non-connectable and non-scannable advertising. */

/*! \brief      Extended advertising parameters. */
typedef struct
{
  uint16_t      advEventProp;       /*!< Advertising Event Properties. */
  uint32_t      priAdvInterMin;     /*!< Primary Advertising Interval Minimum. */
  uint32_t      priAdvInterMax;     /*!< Primary Advertising Interval Maximum. */
  uint8_t       priAdvChanMap;      /*!< Primary Advertising Channel Map. */
  uint8_t       ownAddrType;        /*!< Own Address Type. */
  uint8_t       peerAddrType;       /*!< Peer Address Type. */
  const uint8_t *pPeerAddr;         /*!< Peer Address. */
  uint8_t       advFiltPolicy;      /*!< Advertising Filter Policy. */
  int8_t        advTxPwr;           /*!< Advertising Tx Power. */
  uint8_t       priAdvPhy;          /*!< Primary Advertising PHY. */
  uint8_t       secAdvMaxSkip;      /*!< Secondary Advertising Maximum Skip. */
  uint8_t       secAdvPhy;          /*!< Secondary Advertising PHY. */
  uint8_t       advSID;             /*!< Advertising SID. */
  uint8_t       scanReqNotifEna;    /*!< Scan Request Notification Enable. */
} LlExtAdvParam_t;

/*! \brief      Extended advertising enable parameters. */
typedef struct
{
  uint8_t       handle;             /*!< Advertising handle. */
  uint16_t      duration;           /*!< Duration. */
  uint8_t       numEvents;          /*!< Maximum number of extended advertising events. */
} LlExtAdvEnableParam_t;

/*! \brief      Periodic advertising parameters. */
typedef struct
{
  uint16_t      perAdvInterMin;     /*!< Periodic Advertising Interval Minimum. */
  uint16_t      perAdvInterMax;     /*!< Periodic Advertising Interval Maximum. */
  uint16_t      perAdvProp;         /*!< Periodic Advertising Properties. */
} LlPerAdvParam_t;

/*! \brief       Advertising data operation. */
enum
{
  LL_ADV_DATA_OP_FRAG_INTER     = 0,            /*!< Intermediate fragment. */
  LL_ADV_DATA_OP_FRAG_FIRST     = 1,            /*!< First fragment. */
  LL_ADV_DATA_OP_FRAG_LAST      = 2,            /*!< Last fragment. */
  LL_ADV_DATA_OP_COMP           = 3,            /*!< Complete extended advertising data. */
  LL_ADV_DATA_OP_UNCHANGED      = 4             /*!< Unchanged data (just update the Advertising DID). */
};

/*! \brief       Advertising data fragment preference. */
enum
{
  LL_ADV_DATA_FRAG_ALLOW        = 0,            /*!< Controller may fragment all Host advertising data. */
  LL_ADV_DATA_FRAG_DISALLOW     = 1             /*!< Controller should not fragment nor minimize fragmentation of Host advertising data. */
};

/*! \} */    /* LL_API_BROADCAST */

/*! \addtogroup LL_API_OBSERVER
 *  \{ */

/*! \brief      Type of scan. */
enum
{
  LL_SCAN_PASSIVE               = 0,            /*!< Passive scanning. */
  LL_SCAN_ACTIVE                = 1             /*!< Active scanning. */
};

/*! \brief      Init filter policy. */
enum
{
  LL_INIT_FILTER_PEER          = 0,             /*!< Filter to only Peer addr and addr type. */
  LL_INIT_FILTER_WL            = 1,             /*!< Filter to only whitelist. */
  LL_INIT_FILTER_TOTAL                          /*!< Total number of filter policies. */
};

/*! \brief      Scan filter policy. */
enum
{
  LL_SCAN_FILTER_NONE           = 0,            /*!< Accept all advertising packets. */
  LL_SCAN_FILTER_WL_BIT         = 1,            /*!< Accept from While List only. */
  LL_SCAN_FILTER_RES_INIT_BIT   = 2,            /*!< Accept directed advertisements with RPAs. */
  LL_SCAN_FILTER_WL_OR_RES_INIT = 3             /*!< Accept from White List or directed advertisements with RPAs. */
};

/*! \brief      Periodic scan filter policy. */
enum
{
  LL_PER_SCAN_FILTER_NONE       = 0,            /*!< Use advSID, advAddrType and advAddr to filter. */
  LL_PER_SCAN_FILTER_PL_BIT     = 1,            /*!< Use the periodic advertiser list. */
};

/*! \brief      Scan parameters. */
typedef struct
{
  uint16_t      scanInterval;                   /*!< Scan interval. */
  uint16_t      scanWindow;                     /*!< Scan window. */
  uint8_t       scanType;                       /*!< Scan type. */
  uint8_t       ownAddrType;                    /*!< Address type used by this device. */
  uint8_t       scanFiltPolicy;                 /*!< Scan filter policy. */
} LlScanParam_t;

/*! \brief      Extended scan parameters. */
typedef struct
{
  uint16_t      scanInterval;                   /*!< Scan interval. */
  uint16_t      scanWindow;                     /*!< Scan window. */
  uint8_t       scanType;                       /*!< Scan type. */
} LlExtScanParam_t;

/*! \brief      Scan filter modes for duplicate report. */
enum
{
  LL_SCAN_FILTER_DUP_DISABLE            = 0x00, /*!< Duplicate filtering disabled. */
  LL_SCAN_FILTER_DUP_ENABLE_ONCE        = 0x01, /*!< Duplicate filtering enabled. */
  LL_SCAN_FILTER_DUP_ENABLE_PERIODIC    = 0x02  /*!< Duplicate filtering enabled, reset for each scan period. */
};

/*! \brief       Advertising report event types. */
enum
{
  LL_RPT_TYPE_ADV_IND           = 0x00,         /*!< Connectable undirected advertising (ADV_IND). */
  LL_RPT_TYPE_ADV_DIRECT_IND    = 0x01,         /*!< Connectable directed advertising (ADV_DIRECT_IND). */
  LL_RPT_TYPE_ADV_SCAN_IND      = 0x02,         /*!< Scannable undirected advertising (ADV_SCAN_IND). */
  LL_RPT_TYPE_ADV_NONCONN_IND   = 0x03,         /*!< Non connectable undirected advertising (ADV_NONCONN_IND). */
  LL_RPT_TYPE_SCAN_RSP          = 0x04          /*!< Scan Response (SCAN_RSP). */
};

/*! \brief      Periodic advertising create sync options valid bits. */
#define LL_PER_ADV_CREATE_SYNC_OPTIONS_BITS   0x07

/*! \brief      Periodic advertising enable bits. */
enum
{
  LL_PER_ADV_ENABLE_ADV_ENABLE_BIT      = (1 << 0),   /*!< Enable bit for periodic advertising enable command. */
  LL_PER_ADV_ENABLE_ADI_ENABLE_BIT      = (1 << 1)    /*!< Enable bit for periodic advertising ADI inclusion. */
};

/*! \brief      Periodic advertising create sync command. */
typedef struct
{
  uint8_t   options;        /*!< Options. */
  uint8_t   advSID;         /*!< Advertising SID. */
  uint8_t   advAddrType;    /*!< Advertiser Address Type. */
  uint8_t   *pAdvAddr;      /*!< Advertiser Address. */
  uint16_t  skip;           /*!< Skip. */
  uint16_t  syncTimeOut;    /*!< Synchronization Timeout. */
} LlPerAdvCreateSyncCmd_t;

/*! \brief      Device in periodic advertiser list */
typedef struct
{
  uint8_t   advAddrType;    /*!< Advertiser Address Type. */
  uint8_t   *pAdvAddr;      /*!< Advertiser Address. */
  uint8_t   advSID;         /*!< Advertising SID. */
} LlDevicePerAdvList_t;

/*! \} */    /* LL_API_OBSERVER */

/*! \addtogroup LL_API_CONN
 *  \{ */

/*! \brief      Master clock accuracy. */
enum
{
  LL_MCA_500_PPM                = 0x00,         /*!< 500 ppm */
  LL_MCA_250_PPM                = 0x01,         /*!< 250 ppm */
  LL_MCA_150_PPM                = 0x02,         /*!< 150 ppm */
  LL_MCA_100_PPM                = 0x03,         /*!< 100 ppm */
  LL_MCA_75_PPM                 = 0x04,         /*!< 75 ppm */
  LL_MCA_50_PPM                 = 0x05,         /*!< 50 ppm */
  LL_MCA_30_PPM                 = 0x06,         /*!< 30 ppm */
  LL_MCA_20_PPM                 = 0x07          /*!< 20 ppm */
};

/*! \brief      PHYS specification. */
enum
{
  LL_PHYS_NONE                  = 0,            /*!< No selected PHY. */
  LL_PHYS_LE_1M_BIT             = (1 << 0),     /*!< LE 1M PHY. */
  LL_PHYS_LE_2M_BIT             = (1 << 1),     /*!< LE 2M PHY. */
  LL_PHYS_LE_CODED_BIT          = (1 << 2),     /*!< LE Coded PHY. */
};

/*! \brief      All PHYs preference. */
enum
{
  LL_ALL_PHY_ALL_PREFERENCES    = 0,            /*!< All PHY preferences. */
  LL_ALL_PHY_TX_PREFERENCE_BIT  = (1 << 0),     /*!< Set if no Tx PHY preference. */
  LL_ALL_PHY_RX_PREFERENCE_BIT  = (1 << 1)      /*!< Set if no Rx PHY preference. */
};

/*! \brief      PHY options. */
enum
{
  LL_PHY_OPTIONS_NONE           = 0,            /*!< No preferences. */
  LL_PHY_OPTIONS_S2_PREFERRED   = 1,            /*!< S=2 coding preferred when transmitting on LE Coded PHY. */
  LL_PHY_OPTIONS_S8_PREFERRED   = 2,            /*!< S=8 coding preferred when transmitting on LE Coded PHY. */
};

/*! \brief      PHY types. */
typedef enum
{
  LL_PHY_NONE                   = 0,                 /*!< PHY not selected. */
  LL_PHY_LE_1M                  = 1,                 /*!< LE 1M PHY. */
  LL_PHY_LE_2M                  = 2,                 /*!< LE 2M PHY. */
  LL_PHY_LE_CODED               = 3,                 /*!< LE Coded PHY. */
} LlPhy_t;

/*! \brief     Power Control managed PHYs. */
enum
{
  LL_PC_PHY_1M           = 1,
  LL_PC_PHY_2M           = 2,
  LL_PC_PHY_CODED_S8     = 3,
  LL_PC_PHY_CODED_S2     = 4,

  LL_PC_PHY_TOTAL        = LL_PC_PHY_CODED_S2,
  LL_PC_PHY_INVALID      = 0xFF
};

/*! \brief    Power control PHY bits. */
enum
{
  LL_PC_1M_BIT             = (1 << 0),     /*!< LE 1M PHY bit. */
  LL_PC_2M_BIT             = (1 << 1),     /*!< LE 2M PHY bit. */
  LL_PC_CODED_S8_BIT       = (1 << 2),     /*!< LE Coded S8 PHY bit. */
  LL_PC_CODED_S2_BIT       = (1 << 3),     /*!< LE Coded S2 PHY bit. */
  LL_PC_MAX_BIT            = LL_PC_CODED_S2_BIT,

  LL_PC_ALL_BITS = LL_PC_1M_BIT | LL_PC_2M_BIT | LL_PC_CODED_S8_BIT | LL_PC_CODED_S8_BIT
};

/*! \brief      PCL path loss monitoring unused high threshold value. */
#define LL_PC_PATH_LOSS_UNUSED_HIGH_THRESHOLD      0xFF

/*! \brief      Privacy modes. */
enum
{
  LL_PRIV_MODE_NETWORK          = 0,            /*!< Network privacy mode. */
  LL_PRIV_MODE_DEVICE           = 1,            /*!< Device privacy mode. */
};

/*! \brief      Initiating parameters (\a LlExtCreateConn()). */
typedef struct
{
  uint16_t      scanInterval;                   /*!< Scan interval. */
  uint16_t      scanWindow;                     /*!< Scan window. */
  uint8_t       filterPolicy;                   /*!< Scan filter policy. */
  uint8_t       ownAddrType;                    /*!< Address type used by this device. */
  uint8_t       peerAddrType;                   /*!< Address type used for peer device. */
  const uint8_t *pPeerAddr;                     /*!< Address of peer device. */
} LlInitParam_t;

/*! \brief      Initiating parameters (\a LlExtCreateConn()). */
typedef struct
{
  uint8_t       filterPolicy;                   /*!< Scan filter policy. */
  uint8_t       ownAddrType;                    /*!< Address type used by this device. */
  uint8_t       peerAddrType;                   /*!< Address type used for peer device. */
  const uint8_t *pPeerAddr;                     /*!< Address of peer device. */
  uint8_t       initPhys;                       /*!< Initiating PHYs. */
} LlExtInitParam_t;

/*! \brief      Initiating scan parameters (\a LlExtCreateConn()). */
typedef struct
{
  uint16_t      scanInterval;                   /*!< Scan interval. */
  uint16_t      scanWindow;                     /*!< Scan window. */
} LlExtInitScanParam_t;

/*! \brief      Connection specification (\a LlCreateConn(), \a LlConnUpdate() and \a LlExtCreateConn()). */
typedef struct
{
  uint16_t      connIntervalMin;                /*!< Minimum connection interval. */
  uint16_t      connIntervalMax;                /*!< Maximum connection interval. */
  uint16_t      connLatency;                    /*!< Connection latency. */
  uint16_t      supTimeout;                     /*!< Supervision timeout. */
  uint16_t      minCeLen;                       /*!< Minimum CE length. */
  uint16_t      maxCeLen;                       /*!< Maximum CE length. */
} LlConnSpec_t;

/*! \brief      Channel selection algorithm methods. */
enum
{
  LL_CH_SEL_1                   = 0,            /*!< Channel selection #1. */
  LL_CH_SEL_2                   = 1             /*!< Channel selection #2. */
};

/*! \brief      Tx power level type. */
enum
{
  LL_TX_PWR_LVL_TYPE_CURRENT    = 0x00,         /*!< Current transmit power level. */
  LL_TX_PWR_LVL_TYPE_MAX        = 0x01          /*!< Maximum transmit power level. */
};

/*! \} */    /* LL_API_CONN */

/*! \addtogroup LL_API_ISO
 *  \{ */

/*! \brief      Packing scheme. */
typedef enum
{
  LL_PACKING_SEQUENTIAL         = 0,            /*!< Sequential. */
  LL_PACKING_INTERLEAVED        = 1             /*!< Interleaved. */
} LlPacking_t;

/*! \brief      CIS parameters. */
typedef struct
{
  uint8_t       cisId;                          /*!< Used to identify a connected isochronous stream. */
  uint16_t      sduSizeMToS;                    /*!< Maximum size of a data SDU from the master to the slave. */
  uint16_t      sduSizeSToM;                    /*!< Maximum size of a data SDU from the slave to the master. */
  uint8_t       phyMToS;                        /*!< PHY to be used for transmission from master to slave. */
  uint8_t       phySToM;                        /*!< PHY to be used for transmission from master to slave. */
  uint8_t       rteMToS;                        /*!< Maximum number of times every PDU should be retransmitted from the master to slave. */
  uint8_t       rteSToM;                        /*!< Maximum number of times every PDU should be retransmitted from the slave to master. */
} LlCisCisParams_t;

/*! \brief      CIG parameters. */
typedef struct
{
  uint8_t       cigId;                          /*!< Used to identify the connected isochronous group. */
  uint32_t      sduIntervalMToS;                /*!< The time interval between the start of consecutive SDUs from the master Host. */
  uint32_t      sduIntervalSToM;                /*!< The time interval between the start of consecutive SDUs from the slave Host. */
  uint8_t       sca;                            /*!< Sleep clock accuracy. */
  uint8_t       packing;                        /*!< Packing scheme. */
  LlFraming_t   framing:8;                      /*!< Indicates the format of CIS Data PDUs.  */
  uint16_t      transLatMToS;                   /*!< Maximum time in microseconds between the transmissions of a Data PDU from the Link Layer of the master to the reception of the same Data PDU in the Link Layer of the slave. */
  uint16_t      transLatSToM;                   /*!< Maximum time in microseconds between the transmissions of a Data PDU from the Link Layer of the slave to the reception of the same Data PDU in the Link Layer of the master. */
  uint8_t       numCis;                         /*!< Number of CIS to set. */
  LlCisCisParams_t *pCisParam;                  /*!< CIS parameters. */
} LlCisCigParams_t;

/*! \brief      CIG test CIS parameters. */
typedef struct
{
  uint8_t       cisId;                          /*!< CIS identifier. */
  uint8_t       nse;                            /*!< Maximum number of subevent in each interval on CIS. */
  uint16_t      sduSizeMToS;                    /*!< Maximum size of a data SDU from the master to the slave. */
  uint16_t      sduSizeSToM;                    /*!< Maximum size of a data SDU from the slave to the master. */
  uint16_t      pduSizeMToS;                    /*!< Maximum size of payload from master to slave. */
  uint16_t      pduSizeSToM;                    /*!< Maximum size of payload from slave to master. */
  uint8_t       phyMToS;                        /*!< Master to slave PHY. */
  uint8_t       phySToM;                        /*!< Slave to master PHY. */
  uint8_t       bnMToS;                         /*!< Master to slave burst number. */
  uint8_t       bnSToM;                         /*!< Slave to master burst number. */
} LlCisCigCisParamsTest_t;

/*! \brief      CIG test CIG parameters. */
typedef struct
{
  uint8_t       cigId;                          /*!< CIG identifier. */
  uint32_t      sduIntervalMToS;                /*!< The time interval between the start of consecutive SDUs from the master Host. */
  uint32_t      sduIntervalSToM;                /*!< The time interval between the start of consecutive SDUs from the slave Host. */
  uint8_t       ftMToS;                         /*!< The flush timeout in multiples of ISO_Interval for each payload sent from the master to slave. */
  uint8_t       ftSToM;                         /*!< The flush timeout in multiples of ISO_Interval for each payload sent from the slave to master. */
  uint16_t      isoInterval;                    /*!< The time duration of the isochronous PDU interval. */
  uint8_t       sca;                            /*!< Sleep clock accuracy. */
  uint8_t       packing;                        /*!< Packing scheme. */
  LlFraming_t   framing:8;                      /*!< Indicates the format of CIS Data PDUs.  */
  uint8_t       numCis;                         /*!< Number of CIS. */
  LlCisCigCisParamsTest_t *pCisParam;           /*!< CIS parameters. */
} LlCisCigParamsTest_t;

/*! \brief      Create CIS parameters. */
typedef struct
{
  uint16_t      *pCisHandle;                    /*!< Pointer to the connected isochronous handle array. */
  uint16_t      *pAclHandle;                    /*!< Pointer to the asynchronous connection link handle array. */
} LlCisCreateCisParams_t;

/*! \brief       ISO data path direction. */
typedef enum
{
  LL_ISO_DATA_DIR_INPUT         = 0x00,         /*!< Input data path. */
  LL_ISO_DATA_DIR_OUTPUT        = 0x01          /*!< Output data path. */
} LlIsoDataPathDir_t;

/*! \brief       ISO data path. */
typedef enum
{
  LL_ISO_DATA_PATH_HCI          = 0x00,         /*!< HCI data path. */
  LL_ISO_DATA_PATH_VS           = 0x01,         /*!< Vendor Specific data path. */

  LL_ISO_DATA_PATH_TOTAL,                       /*!< Total number of data path methods. */

  LL_ISO_DATA_PATH_DISABLED     = 0xFF,         /*!< Data path is disabled. */
} LlIsoDataPath_t;

/*! \brief      ISO data path direction bit. */
enum
{
  LL_ISO_DATA_PATH_INPUT_BIT    = (1 << 0),     /*!< Data path input bit. */
  LL_ISO_DATA_PATH_OUTPUT_BIT   = (1 << 1)      /*!< Data path output bit. */
};

/*! \brief      LE setup ISO Data Path command. */
typedef struct
{
  LlIsoDataPathDir_t dpDir:8;       /*!< Data path direction. */
  uint8_t       dpId;               /*!< Data path ID. */
  uint8_t       vsCfgLen;           /*!< Length of \a pVsCfg. */
  const uint8_t *pVsCfg;            /*!< VS configuration buffer. */
} LlIsoConfigDataPath_t;

/*! \brief      LE setup ISO Data Path command. */
typedef struct
{
  uint16_t      handle;             /*!< Handle of CIS or BIS. */
  LlIsoDataPathDir_t dpDir:8;       /*!< Data path direction. */
  uint8_t       dpId;               /*!< Data path ID. */
  uint8_t       codecFormat;        /*!< Codec Format. */
  uint16_t      codecCompId;        /*!< Codec Company ID. */
  uint16_t      codecId;            /*!< Codec ID. */
  uint32_t      ctrDly;             /*!< Controller delay. */
  uint8_t       codecConfigLen;     /*!< Codec configuration length. */
  const uint8_t *pCodecConfig;      /*!< Codec configuration. */
} LlIsoSetupDataPath_t;

/*! \brief      BIG Create BIG command. */
typedef struct
{
  uint8_t       bigHandle;          /*!< Used to identify the BIG. */
  uint8_t       advHandle;          /*!< Used to identify the periodic advertising train. */
  uint8_t       numBis;             /*!< Total number of BISes in the BIG. */
  uint32_t      sduInterUsec;       /*!< Interval, in microseconds, of BIG SDUs. */
  uint8_t       bcstCode[LL_BC_LEN];/*!< Session key used to encrypt and decrypt BIS payloads. */
  uint16_t      maxSdu;             /*!< Maximum size of an SDU. */
  uint16_t      mtlMs;              /*!< Maximum time in milliseconds. */
  uint8_t       rtn;                /*!< Retransmitted number. */
  uint8_t       phys;               /*!< Transmitter PHYs of packets. */
  uint8_t       packing;            /*!< Sequential or Interleaved packing. */
  LlFraming_t   framing:8;          /*!< Unframed or Framed. */
  uint8_t       encrypt;            /*!< Unencrypted or Encrypted. */
} LlCreateBig_t;

/*! \brief      BIG Create BIG Test command. */
typedef struct
{
  uint8_t       bigHandle;          /*!< Used to identify the BIG. */
  uint8_t       advHandle;          /*!< Used to identify the periodic advertising train. */
  uint8_t       numBis;             /*!< Total number of BISes in the BIG. */
  uint32_t      sduInterUsec;       /*!< Interval in microseconds of BIG SDUs. */
  uint8_t       bcstCode[LL_BC_LEN];/*!< Code used to derive the session key. */
  uint16_t      isoInter;           /*!< Duration of an isochronous interval for BIG PDUs in 1.25ms unit. */
  uint8_t       nse;                /*!< Total number of subevents in each interval of each BIS in the BIG. */
  uint16_t      maxSdu;             /*!< Maximum size of a SDU. */
  uint16_t      maxPdu;             /*!< Maximum size of payload. */
  uint8_t       phys;               /*!< Transmitter PHYs of packets. */
  uint8_t       packing;            /*!< Sequential or Interleaved packing. */
  LlFraming_t   framing:8;          /*!< Unframed or Framed. */
  uint8_t       bn;                 /*!< Number of new payloads in each interval for each BIS. */
  uint8_t       irc;                /*!< Number of times the scheduled payload(s) are transmitted in a given event. */
  uint8_t       pto;                /*!< Offset used for pre-transmissions. */
  uint8_t       encrypt;            /*!< Unencrypted or Encrypted. */
} LlCreateBigTest_t;

/*! \brief      BIG Create Sync command. */
typedef struct
{
  uint8_t       bigHandle;          /*!< Used to identify the BIG. */
  uint16_t      syncHandle;         /*!< Periodic advertising train handle. */
  uint8_t       encrypt;            /*!< Unencrypted or Encrypted. */
  uint8_t       bcstCode[LL_BC_LEN];/*!< Session key code for encrypt and decrypt BIS payloads. */
  uint8_t       mse;                /*!< Maximum number of subevents. */
  uint16_t      bigSyncTimeout;     /*!< Synchronization timeout for the BIS, in the units of 10ms. */
  uint8_t       numBis;             /*!< Total number of BISes in the BIG. */
  uint8_t       bis[LL_MAX_BIS];    /*!< List of indices of BISes. */
} LlBigCreateSync_t;

/*! \} */    /* LL_API_ISO */

/*! \addtogroup LL_API_ECU
 *  \{ */

/*! \brief      Subrate parameter. */
typedef struct
{
  uint16_t      srMin;              /*!< Subrate minimum value. */
  uint16_t      srMax;              /*!< Subrate maximum value. */
  uint16_t      maxLatency;         /*!< Maximum latency. */
  uint16_t      contNum;            /*!< Continuation number. */
  uint16_t      svt;                /*!< Supervision timeout in 10ms units. */
} LlSubrateParam_t;

/*! \} */    /* LL_API_ECU */

/*! \addtogroup LL_API_ENCRYPT
 *  \{ */

/*! \brief       Nonce mode. */
enum
{
  LL_NONCE_MODE_PKT_CNTR        = 0x00,         /*!< Packet counter nonce mode (default). */
  LL_NONCE_MODE_EVT_CNTR        = 0x01          /*!< Connection event counter mode. */
};

/*! \brief      Encryption mode data structure used in LlGetEncMode() and LlSetEncMode(). */
typedef struct
{
  bool_t        enaAuth;            /*!< Enable authentication. */
  bool_t        nonceMode;          /*!< Nonce mode. */
} LlEncMode_t;

/*! \} */    /* LL_API_ENCRYPT */

/*! \addtogroup LL_API_TEST
 *  \{ */

/*! \brief      Test packet payload type. */
enum
{
  LL_TEST_PKT_TYPE_PRBS9        = 0x00,         /*!< Pseudo-Random bit sequence 9. */
  LL_TEST_PKT_TYPE_0F           = 0x01,         /*!< 00001111'b packet payload type. */
  LL_TEST_PKT_TYPE_55           = 0x02,         /*!< 01010101'b packet payload type. */
  LL_TEST_PKT_TYPE_PRBS15       = 0x03,         /*!< Pseudo-Random bit sequence 15. */
  LL_TEST_PKT_TYPE_FF           = 0x04,         /*!< 11111111'b packet payload type. */
  LL_TEST_PKT_TYPE_00           = 0x05,         /*!< 00000000'b packet payload type. */
  LL_TEST_PKT_TYPE_F0           = 0x06,         /*!< 11110000'b packet payload type. */
  LL_TEST_PKT_TYPE_AA           = 0x07          /*!< 10101010'b packet payload type. */
};

/*! \brief      Test PHY type. */
enum
{
  LL_TEST_PHY_LE_1M             = 0x01,         /*!< LE 1M PHY. */
  LL_TEST_PHY_LE_2M             = 0x02,         /*!< LE 2M PHY. */
  LL_TEST_PHY_LE_CODED          = 0x03,         /*!< LE Coded PHY (data coding unspecified). */
  LL_TEST_PHY_LE_CODED_S8       = 0x03,         /*!< LE Coded PHY with S=8 data coding. */
  LL_TEST_PHY_LE_CODED_S2       = 0x04          /*!< LE Coded PHY with S=2 data coding. */
};

/*! \brief      Test modulation index. */
enum
{
  LL_TEST_MOD_IDX_STANDARD      = 0x00,         /*!< Standard modulation index. */
  LL_TEST_MOD_IDX_STABLE        = 0x01          /*!< Stable modulation index. */
};

/*! \brief      Test report data. */
typedef struct
{
  uint16_t      numTx;          /*!< Total transmit packet count. */
  uint16_t      numRxSuccess;   /*!< Successfully received packet count. */
  uint16_t      numRxCrcError;  /*!< CRC failed packet count. */
  uint16_t      numRxTimeout;   /*!< Receive timeout count. */
} LlTestReport_t;

/*! \brief      ISO test packet payload type. */
typedef enum
{
  LL_ISO_PLD_TYPE_ZERO_LEN      = 0x00,         /*!< Zero length payload. */
  LL_ISO_PLD_TYPE_VAR_LEN       = 0x01,         /*!< Variable length payload. */
  LL_ISO_PLD_TYPE_MAX_LEN       = 0x02          /*!< Maximum length payload. */
} LlIsoPldType_t;

/*! \brief      ISO test counter data. */
typedef struct
{
  uint32_t      numSuccess;     /*!< Received good packet count. */
  uint32_t      numMissed;      /*!< Received missed packet count. */
  uint32_t      numFailed;      /*!< Received failed packet count. */
} LlIsoTestCtrs_t;

/*! \brief      ISO link quality statistics. */
typedef struct
{
  uint32_t txUnAckPkt;          /*!< Unacked packets. */
  uint32_t txFlushedPkt;        /*!< Flushed packets. */
  uint32_t txLastSubEventPkt;   /*!< The CIS slave transmit CIS Data PDU on it's last subevent. */
  uint32_t retransmitPkt;       /*!< Retransmitted packets. */
  uint32_t crcErrPkt;           /*!< CRC error packets. */
  uint32_t rxUnreceivedPkt;     /*!< Packets unreceived by flush point. */
  uint32_t duplicatePkt;        /*!< Retransmitted CIS data PDUs. */
} LlIsoLinkQual_t;

/*! \} */    /* LL_API_TEST */

/*! \addtogroup LL_API_EVENT
 *  \{ */

/*! \brief      Link control callback interface events */
enum
{
  LL_ERROR_IND,                 /*!< Unrecoverable LL or radio error occurred (vendor specific). */
  /* --- Core Spec 4.0 --- */
  LL_RESET_CNF,                 /*!< Reset complete. */
  LL_ADV_REPORT_IND,            /*!< Advertising report. */
  LL_ADV_ENABLE_CNF,            /*!< Advertising enable/disable complete. */
  LL_SCAN_ENABLE_CNF,           /*!< Scan enable/disable complete. */
  LL_CONN_IND,                  /*!< Connection complete. */
  LL_DISCONNECT_IND,            /*!< Disconnect complete. */
  LL_CONN_UPDATE_IND,           /*!< Connection update complete. */
  LL_CREATE_CONN_CANCEL_CNF,    /*!< Create connection cancel status. */
  LL_READ_REMOTE_VER_INFO_CNF,  /*!< Read remote version information complete. */
  LL_READ_REMOTE_FEAT_CNF,      /*!< Read remote features complete. */
  LL_ENC_CHANGE_IND,            /*!< Encryption change. */
  LL_ENC_KEY_REFRESH_IND,       /*!< Key refresh. */
  LL_LTK_REQ_IND,               /*!< LTK request. */
  LL_LTK_REQ_NEG_REPLY_CNF,     /*!< LTK request negative reply status. */
  LL_LTK_REQ_REPLY_CNF,         /*!< LTK request reply status. */
  /* --- Core Spec 4.2 --- */
  LL_REM_CONN_PARAM_IND,        /*!< Remote connection parameter change. */
  LL_AUTH_PAYLOAD_TIMEOUT_IND,  /*!< Authentication payload timeout expired. */
  LL_DATA_LEN_CHANGE_IND,       /*!< Data length changed. */
  LL_READ_LOCAL_P256_PUB_KEY_CMPL_IND, /*!< Read local P-256 public key complete. */
  LL_GENERATE_DHKEY_CMPL_IND,          /*!< Generate Diffie-Hellman key complete. */
  LL_SCAN_REPORT_IND,           /*!< Legacy scan report (vendor specific). */
  /* --- Core Spec 5.0 --- */
  LL_PHY_UPDATE_IND,            /*!< LE PHY update complete. */
  LL_EXT_ADV_REPORT_IND,        /*!< Extended advertising report. */
  LL_EXT_SCAN_ENABLE_CNF,       /*!< Extended scan enable/disable complete. */
  LL_SCAN_TIMEOUT_IND,          /*!< Scan timeout. */
  LL_SCAN_REQ_RCVD_IND,         /*!< Scan request received. */
  LL_EXT_ADV_ENABLE_CNF,        /*!< Extended advertising enable/disable complete. */
  LL_ADV_SET_TERM_IND,          /*!< Advertising set terminated complete. */
  LL_PER_ADV_ENABLE_CNF,        /*!< Periodic advertising enable/disable complete. */
  LL_PER_ADV_SYNC_ESTD_IND,     /*!< Periodic scanning synchronization established. */
  LL_PER_ADV_SYNC_LOST_IND,     /*!< Periodic scanning synchronization lost. */
  LL_PER_ADV_REPORT_IND,        /*!< Periodic scanning report. */
  LL_CH_SEL_ALGO_IND,           /*!< Channel selection algorithm. */
  /* --- Core Spec 5.1 --- */
  LL_CONNLESS_IQ_REPORT_IND,    /*!< LE connectionless IQ report received. */
  LL_CONN_IQ_REPORT_IND,        /*!< LE connection IQ report received. */
  LL_CTE_REQ_FAILED_IND,        /*!< LE CTE request failed received. */
  LL_PER_SYNC_TRSF_RCVD_IND,    /*!< LE periodic advertising sync transfer received. */
  /* --- Core Spec 5.2 --- */
  LL_CIS_EST_IND,               /*!< CIS established event. */
  LL_CIS_REQ_IND,               /*!< CIS request event. */
  LL_CREATE_BIG_CNF,            /*!< Create BIG complete. */
  LL_TERM_BIG_IND,              /*!< Terminate BIG complete. */
  LL_BIG_TERM_SYNC_CNF,         /*!< BIG Terminate Sync complete. */
  LL_BIG_SYNC_EST_IND,          /*!< BIG sync established event. */
  LL_BIG_SYNC_LOST_IND,         /*!< BIG sync lost event. */
  LL_REQ_PEER_SCA_IND,          /*!< Request peer SCA complete. */
  LL_TX_POWER_REPORTING_IND,    /*!< LL txPower change report received. */
  LL_PATH_LOSS_REPORTING_IND,   /*!< Path loss reporting event. */
  LL_ISO_EVT_CMPL_IND,          /*!< ISO Event complete event. */
  LL_BIG_INFO_ADV_REPORT_IND,   /*!< BIG Info advertising report event. */
  /* --- Core Spec Sydney --- */
  LL_SUBRATE_CHANGE_IND         /*!< Enhanced connection update complete. */
};

/*! \brief      Advertising report indication */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       *pData;         /*!< Data buffer. */
  uint8_t       len;            /*!< Data buffer length. */
  int8_t        rssi;           /*!< RSSI. */
  uint8_t       eventType;      /*!< Event type. */
  uint8_t       addrType;       /*!< Address type. */
  bdAddr_t      addr;           /*!< Address. */
  /* --- direct fields --- */
  uint8_t       directAddrType; /*!< Direct address type. */
  bdAddr_t      directAddr;     /*!< Direct address. */
} LlAdvReportInd_t;

/*! \brief      Connect indication */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
  uint8_t       role;           /*!< Role of this device. */
  uint8_t       addrType;       /*!< Address type. */
  bdAddr_t      peerAddr;       /*!< Peer address. */
  uint16_t      connInterval;   /*!< Connection interval. */
  uint16_t      connLatency;    /*!< Connection latency. */
  uint16_t      supTimeout;     /*!< Supervision timeout. */
  uint8_t       clockAccuracy;  /*!< Clock accuracy. */
  /* --- enhanced fields --- */
  bdAddr_t      localRpa;       /*!< Local resolvable private address. */
  bdAddr_t      peerRpa;        /*!< Peer resolvable private address. */
} LlConnInd_t;

/*! \brief      Disconnect indication */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
  uint8_t       reason;         /*!< Reason code. */
} LlDisconnectInd_t;

/*! \brief      Connect update indication */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
  uint16_t      connInterval;   /*!< Connection interval. */
  uint16_t      connLatency;    /*!< Connection latency. */
  uint16_t      supTimeout;     /*!< Supervision timeout. */
} LlConnUpdateInd_t;

/*! \brief      Connection parameter change indication */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      handle;         /*!< Connection handle. */
  uint16_t      connIntervalMin;/*!< Minimum connection interval. */
  uint16_t      connIntervalMax;/*!< Maximum connection interval. */
  uint16_t      connLatency;    /*!< Connection latency. */
  uint16_t      supTimeout;     /*!< Supervision timeout. */
} LlRemConnParamInd_t;

/*! \brief      Create connection cancel confirm */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
} LlCreateConnCancelCnf_t;

/*! \brief      Read remote version information confirm */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
  uint8_t       version;        /*!< Bluetooth specification version. */
  uint16_t      mfrName;        /*!< Manufacturer ID. */
  uint16_t      subversion;     /*!< Subversion. */
} LlReadRemoteVerInfoCnf_t;

#define LL_FEAT_LEN             8       /*!< Length of features byte array */

/*! \brief      Read remote feature confirm */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
  uint8_t       features[LL_FEAT_LEN];  /*!< Features. */
} LlReadRemoteFeatCnf_t;

/*! \brief      Encryption change indication */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
  bool_t        enabled;        /*!< Encryption enabled. */
  uint8_t       encKeySize;     /*!< Encryption key size. */
} LlEncChangeInd_t;

/*! \brief      Key refresh indication */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
} LlEncKeyRefreshInd_t;

/*! \brief      LTK request indication */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      handle;         /*!< Connection handle. */
  uint8_t       randNum[LL_RAND_LEN];   /*!< Random number. */
  uint16_t      encDiversifier; /*!< Encryption diversifier. */
} LlLtkReqInd_t;

/*! \brief      LTK request reply confirm */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
} LlLtkReqReplyCnf_t;

/*! \brief      LTK request negative reply */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
} LlLtkReqNegReplyCnf_t;

/*! \brief      Authentication payload timeout expired indication */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      handle;         /*!< Connection handle. */
} LlAuthPayloadTimeoutInd_t;

/*! \brief      Data length change */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      handle;         /*!< Connection handle. */
  uint16_t      maxTxLen;       /*!< Maximum transmit length. */
  uint16_t      maxTxTime;      /*!< Maximum transmit time in microseconds. */
  uint16_t      maxRxLen;       /*!< Maximum receive length. */
  uint16_t      maxRxTime;      /*!< Maximum receive time in microseconds. */
} LlDataLenChangeInd_t;

/*! \brief      Read local P-256 key pair complete */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint8_t       pubKey_x[LL_ECC_KEY_LEN];  /*!< Public key x-coordinate. */
  uint8_t       pubKey_y[LL_ECC_KEY_LEN];  /*!< Public key y-coordinate. */
} LlReadLocalP256PubKeyInd_t;

/*! \brief      Generate Diffie-Hellman key complete */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint8_t       dhKey[LL_ECC_KEY_LEN];     /*!< Diffie-Hellman key. */
} LlGenerateDhKeyInd_t;

/*! \brief      PHY update complete. */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
  uint8_t       txPhy;          /*!< Transceiver PHY. */
  uint8_t       rxPhy;          /*!< Receiver PHY. */
} LlPhyUpdateInd_t;

/*! \brief      HW error */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       code;           /*!< Code. */
} LlHwErrorInd_t;

/*! \brief      Scan report */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       peerAddrType;   /*!< Peer address type. */
  uint64_t      peerAddr;       /*!< Peer address. */
  uint64_t      peerRpa;        /*!< Peer RPA. */
} LlScanReportInd_t;

/*! \brief      Extended advertising enable */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint8_t       handle;         /*!< Advertising handle. */
} LlExtAdvEnableCnf_t;

/*! \brief      Periodic advertising enable */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint8_t       handle;         /*!< Advertising handle. */
} LlPerAdvEnableCnf_t;

/*! \brief      Extended advertising report event types. */
enum
{
  LL_RPT_EVT_CONN_ADV_BIT       = (1 << 0),     /*!< Connectable advertising event bit. */
  LL_RPT_EVT_SCAN_ADV_BIT       = (1 << 1),     /*!< Scannable advertising event bit. */
  LL_RPT_EVT_DIRECT_ADV_BIT     = (1 << 2),     /*!< Directed advertising event bit. */
  LL_RPT_EVT_SCAN_RSP_BIT       = (1 << 3),     /*!< Scan response event bit. */
  LL_RPT_EVT_LEGACY_ADV_BIT     = (1 << 4),     /*!< Legacy advertising PDU event bit. */
};

/*! \brief      Extended advertising report data status. */
enum
{
  LL_RPT_DATA_CMPL              = 0x00,         /*!< Data complete. */
  LL_RPT_DATA_INC_MORE          = 0x01,         /*!< Data incomplete, more date to come. */
  LL_RPT_DATA_INC_TRUNC         = 0x02          /*!< Data incomplete, data truncated, no more date to come. */
};

/*! \brief      Special SID values. */
enum
{
  LL_SID_NO_ADI                 = 0xFF          /*!< No ADI field in the PDU. */
};

/*! \brief      Extended advertising report */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      eventType;      /*!< Event type. */
  uint8_t       addrType;       /*!< Address type. */
  bdAddr_t      addr;           /*!< Address. */
  uint8_t       priPhy;         /*!< Primary PHY. */
  uint8_t       secPhy;         /*!< Secondary PHY. */
  uint8_t       advSID;         /*!< Advertising SID. */
  int8_t        txPwr;          /*!< Tx Power. */
  int8_t        rssi;           /*!< RSSI. */
  int16_t       perAdvInter;    /*!< Periodic advertising interval. */
  uint8_t       directAddrType; /*!< Direct address type. */
  bdAddr_t      directAddr;     /*!< Direct address. */
  uint16_t      len;            /*!< Data buffer length. */
  const uint8_t *pData;         /*!< Data buffer. */
} LlExtAdvReportInd_t;

/*! \brief      Extended scan enable confirm */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
} LlExtScanEnableCnf_t;

/*! \brief      Advertising set terminated */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint8_t       advHandle;      /*!< Advertising handle. */
  uint16_t      connHandle;     /*!< Connection handle. */
  uint8_t       numCmplAdvEvt;  /*!< Number of completed advertising events. */
} LlAdvSetTermInd_t;

/*! \brief      Scan request received */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       handle;         /*!< Advertising handle. */
  uint8_t       scanAddrType;   /*!< Scanner address type. */
  bdAddr_t      scanAddr;       /*!< Scanner address. */
} LlScanReqRcvdInd_t;

/*! \brief      Used channel selection indication */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      handle;         /*!< Connection handle. */
  uint8_t       usedChSel;      /*!< Used channel selection. */
} LlChSelInd_t;

/*! \brief     LE periodic advertising synchronization established */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      syncHandle;     /*!< Sync handle. */
  uint8_t       advSID;         /*!< Advertising SID. */
  uint8_t       addrType;       /*!< Advertiser address type. */
  bdAddr_t      addr;           /*!< Advertiser address. */
  uint8_t       advPhy;         /*!< Advertiser PHY. */
  uint16_t      perAdvInterval; /*!< Periodic advertising interval. */
  uint8_t       advClkAccuracy; /*!< Advertiser clock accuracy. */
} LlPerAdvSyncEstdCnf_t;

/*! \brief     LE periodic advertising report */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      syncHandle;     /*!< Sync handle. */
  int8_t        txPwr;          /*!< Tx Power. */
  int8_t        rssi;           /*!< RSSI. */
  uint8_t       unused;         /*!< Future use. */
  uint8_t       dataStatus;     /*!< Data status. */
  uint16_t      len;            /*!< Data buffer length. */
  const uint8_t *pData;         /*!< Data buffer. */
} LlPerAdvReportInd_t;

/*! \brief     LE periodic advertising sync lost */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      syncHandle;     /*!< Sync handle. */
} LlPerAdvSyncLostInd_t;

/*! \brief     LE periodic advertising sync transfer received */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      connHandle;     /*!< Connection handle. */
  uint16_t      serviceData;    /*!< Service data. */
  uint16_t      syncHandle;     /*!< Sync handle. */
  uint8_t       advSID;         /*!< Advertising SID. */
  uint8_t       addrType;       /*!< Advertiser address type. */
  bdAddr_t      addr;           /*!< Advertiser address. */
  uint8_t       advPhy;         /*!< Advertiser PHY. */
  uint16_t      perAdvInterval; /*!< Periodic advertising interval. */
  uint8_t       advClkAccuracy; /*!< Advertiser clock accuracy. */
} LlPerSyncTrsfRcvdInd_t;

/*! \brief      CIS established event */
typedef struct
{
  wsfMsgHdr_t   hdr;              /*!< Event header. */
  uint8_t       status;           /*!< Status. */
  uint16_t      cisHandle;        /*!< CIS handle. */
  uint32_t      cigSyncDelayUsec; /*!< CIG synchronization delay in usec. */
  uint32_t      cisSyncDelayUsec; /*!< CIS synchronization delay in usec. */
  uint32_t      transLatUsecMToS; /*!< The maximum time, in microseconds, for transmission of SDUs of all CISes from master to slave. */
  uint32_t      transLatUsecSToM; /*!< The maximum time, in microseconds, for transmission of SDUs of all CISes from slave to master. */
  uint8_t       phyMToS;          /*!< Master to slave PHY. */
  uint8_t       phySToM;          /*!< Slave to master PHY. */
  uint8_t       nse;              /*!< Number of subevents. */
  uint8_t       bnMToS;           /*!< Burst number master to slave. */
  uint8_t       bnSToM;           /*!< Burst number slave to master. */
  uint8_t       ftMToS;           /*!< Flush timeout master to slave. */
  uint8_t       ftSToM;           /*!< Flush timeout slave to master. */
  uint16_t      maxPduMToS;       /*!< Max pdu master to slave. */
  uint16_t      maxPduSToM;       /*!< Max pdu slave to master. */
  uint16_t      isoInterval;      /*!< Time between two consecutive ISO anchor points. */
} LlCisEstInd_t;

/*! \brief      CIS request event */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      aclHandle;      /*!< ACL handle. */
  uint16_t      cisHandle;      /*!< ACL handle. */
  uint8_t       cigId;          /*!< CIG identifier. */
  uint8_t       cisId;          /*!< CIS identifier. */
} LlCisReqInd_t;

/*! \brief      Create BIG complete event */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint8_t       bigHandle;      /*!< BIG handle. */
  uint32_t      syncDelayUsec;  /*!< Synchronization delay in microseconds. */
  uint32_t      transLatUsec;   /*!< Transport latency, in microseconds. */
  uint8_t       phy;            /*!< Transmit PHY. */
  uint8_t       nse;            /*!< Number of Sub-Events in each BIS event in the BIG. */
  uint8_t       bn;             /*!< Number of new payloads in each BIS event. */
  uint8_t       pto;            /*!< Offset used for pre-transmissions. */
  uint8_t       irc;            /*!< Number of times a payload is transmitted in a BIS event. */
  uint16_t      maxPdu;         /*!< Maximum size of the payload. */
  uint16_t      isoInterval;    /*!< Time between two consecutive ISO anchor points. */
  uint8_t       numBis;         /*!< Number of BIS. */
  uint16_t      bisHandle[LL_MAX_BIS];  /*!< Connection handles of the BIS's. */
} LlCreateBigCnf_t;

/*! \brief      Terminate BIG complete event */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       bigHandle;      /*!< BIG handle. */
  uint8_t       reason;         /*!< Terminate reason. */
} LlTerminateBigInd_t;

/*! \brief      BIG Terminate complete event */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint8_t       bigHandle;      /*!< BIG handle. */
} LlBigTermSyncCnf_t;

/*! \brief      Create BIG complete (Sync Established) event */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint8_t       bigHandle;      /*!< BIG handle. */
  uint32_t      transLatUsec;   /*!< The maximum time, in microseconds, for transmission of SDUs of all BISes. */
  uint8_t       nse;            /*!< Number of Sub-Events in each BIS event in the BIG. */
  uint8_t       bn;             /*!< Number of new payloads in each BIS event. */
  uint8_t       pto;            /*!< Offset used for pre-transmissions. */
  uint8_t       irc;            /*!< Number of times a payload is transmitted in a BIS event. */
  uint16_t      maxPdu;         /*!< Maximum size of the payload. */
  uint16_t      isoInterval;    /*!< Time between two consecutive ISO anchor points. */
  uint8_t       numBis;         /*!< Number of BIS. */
  uint16_t      bisHandle[LL_MAX_BIS];  /*!< Connection handles of the BIS's. */
} LlBigSyncEstInd_t;

/*! \brief      BIG sync lost event */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       bigHandle;      /*!< BIG handle. */
  uint8_t       reason;         /*!< Sync lost reason. */
} LlBigSyncLostInd_t;

/*! \brief     LE request peer SCA complete */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      connHandle;     /*!< Connection handle. */
  uint8_t       peerSca;        /*!< Peer SCA. */
} LlPeerScaCnf_t;

/*! \brief      LE power reporting indication. */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      connHandle;     /*!< Connection handle. */
  uint8_t       reason;         /*!< Report reason. */
  uint8_t       phy;            /*!< PHY. */
  int8_t        txPower;        /*!< txPower. */
  uint8_t       txPowerLimits;  /*!< Transmit power level limit flags. */
  int8_t        delta;          /*!< Change from previous txPower. */
} LlPowerReportInd_t;

/*! \brief      VS ISO Event complete event */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       handle;         /*!< ISO handle. */
  uint32_t      evtCtr;         /*!< Event counter. */
} LlIsoEventCmplInd_t;

/*! \brief      BIG Info Advertising Report event */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      syncHandle;     /*!< Sync handle identifying the periodic advertising train. */
  uint8_t       numBis;         /*!< Number of BIS. */
  uint8_t       nse;            /*!< Number of Sub-Events in each BIS event in the BIG. */
  uint16_t      isoInterv;      /*!< ISO interval. */
  uint8_t       bn;             /*!< Number of new payloads in each BIS event. */
  uint8_t       pto;            /*!< Offset used for pre-transmissions. */
  uint8_t       irc;            /*!< Number of times a payload is transmitted in a BIS event. */
  uint16_t      maxPdu;         /*!< Maximum size of the PDU. */
  uint32_t      sduInterv;      /*!< SDU interval. */
  uint16_t      maxSdu;         /*!< Maximum size of the SDU. */
  LlPhy_t       phy;            /*!< Transmit PHY. */
  LlFraming_t   framing;        /*!< Framing mode. */
  bool_t        encrypt;        /*!< Encryption enabled. */
} LlBigInfoAdvRptInd_t;

/*! \brief      LE Path loss reporting event. */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint16_t      connHandle;     /*!< Connection handle. */
  uint8_t       curPathLoss;    /*!< Current path loss. */
  uint8_t       zoneEntered;    /*!< Zone entered. */
} LlPathLossThresholdInd_t;

/*! \brief      Subrate change event. */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< Event header. */
  uint8_t       status;         /*!< Status. */
  uint16_t      handle;         /*!< Connection handle. */
  uint16_t      srFactor;       /*!< Subrate factor. */
  uint16_t      perLatency;     /*!< Peripheral latency. */
  uint16_t      contNum;        /*!< Continuation number. */
  uint16_t      svt;            /*!< Supervision timeout in 10ms units. */
} LlSubrateChangeInd_t;

/*! \brief      Union of all event types */
typedef union
{
  wsfMsgHdr_t               hdr;                    /*!< Event header. */
  LlHwErrorInd_t            hwErrorInd;             /*!< Unrecoverable LL or radio error occurred. */
  /* --- Core Spec 4.0 --- */
  LlAdvReportInd_t          advReportInd;           /*!< LE advertising report. */
  LlConnInd_t               connInd;                /*!< LE connection complete. */
  LlDisconnectInd_t         disconnectInd;          /*!< LE disconnect complete. */
  LlConnUpdateInd_t         connUpdateInd;          /*!< LE connection update complete. */
  LlCreateConnCancelCnf_t   createConnCancelCnf;    /*!< LE create connection cancel status. */
  LlReadRemoteVerInfoCnf_t  readRemoteVerInfoCnf;   /*!< Read remote version information complete. */
  LlReadRemoteFeatCnf_t     readRemoteFeatCnf;      /*!< LE read remote features complete. */
  LlEncChangeInd_t          encChangeInd;           /*!< Encryption change. */
  LlEncKeyRefreshInd_t      keyRefreshInd;          /*!< Key refresh. */
  LlLtkReqInd_t             ltkReqInd;              /*!< LE LTK request. */
  LlLtkReqNegReplyCnf_t     ltkReqNegReplyCnf;      /*!< LTK request negative reply status. */
  LlLtkReqReplyCnf_t        ltkReqReplyCnf;         /*!< LTK request reply status. */
  /* --- Core Spec 4.2 --- */
  LlRemConnParamInd_t       remConnParamInd;        /*!< LE remote connection parameter request. */
  LlAuthPayloadTimeoutInd_t authPayloadTimeoutInd;  /*!< Authentication payload timeout. */
  LlDataLenChangeInd_t      dataLenChangeInd;       /*!< Data length changed. */
  LlReadLocalP256PubKeyInd_t readLocalP256PubKeyInd; /*!< Read local P-256 public key complete. */
  LlGenerateDhKeyInd_t      generateDhKeyInd;       /*!< Generate Diffie-Hellman key complete. */
  LlScanReportInd_t         scanReportInd;          /*!< Scan report. */
  /* --- Core Spec 5.0 --- */
  LlPhyUpdateInd_t          phyUpdateInd;           /*!< PHY update complete. */
  LlExtAdvReportInd_t       extAdvReportInd;        /*!< LE extended advertising report. */
  LlExtScanEnableCnf_t      extScanEnableCnf;       /*!< LE extended scan enable completed. */
  LlScanReqRcvdInd_t        scanReqRcvdInd;         /*!< LE scan request received. */
  LlExtAdvEnableCnf_t       extAdvEnableCnf;        /*!< LE extended advertising enable complete. */
  LlAdvSetTermInd_t         advSetTermInd;          /*!< LE advertising set terminated. */
  LlChSelInd_t              usedChSelInd;           /*!< Used channel selection. */
  LlPerAdvEnableCnf_t       perAdvEnableCnf;        /*!< LE periodic advertising enable complete. */
  LlPerAdvSyncEstdCnf_t     perAdvSyncEstdCnf;      /*!< LE periodic advertising sync established. */
  LlPerAdvReportInd_t       perAdvReportInd;        /*!< LE periodic advertising report. */
  LlPerAdvSyncLostInd_t     perAdvSyncLostInd;      /*!< LE periodic advertising sync lost. */
  /* --- Core Spec 5.1 --- */
  LlPerSyncTrsfRcvdInd_t    perASyncTrsfRcvdInd;    /*!< LE periodic advertising sync transfer received. */
  /* --- Core Spec 5.2 --- */
  LlCisEstInd_t             cisEstInd;              /*!< LE CIS established. */
  LlCisReqInd_t             cisReqInd;              /*!< LE CIS request. */
  LlCreateBigCnf_t          createBigCnf;           /*!< LE create BIG complete. */
  LlTerminateBigInd_t       termBigInd;             /*!< LE terminate BIG complete. */
  LlBigTermSyncCnf_t        bigTermSyncCnf;         /*!< LE BIG terminate sync. */
  LlBigSyncEstInd_t         bigSyncEstInd;          /*!< LE BIG sync established. */
  LlBigSyncLostInd_t        bigSyncLostInd;         /*!< LE BIG sync lost. */
  LlPeerScaCnf_t            peerScaCnf;             /*!< LE request peer SCA complete. */
  LlPowerReportInd_t        powerRptInd;            /*!< LE transmit power reporting indication. */
  LlIsoEventCmplInd_t       isoEvtCmplInd;          /*!< ISO Event complete. */
  LlBigInfoAdvRptInd_t      bigInfoInd;             /*!< LE Big Info indication. */
  LlPathLossThresholdInd_t  pathLossInd;            /*!< LE Path loss threshold reporting indication. */
  /* --- Core Spec Sydney --- */
  LlSubrateChangeInd_t      subrateChangeInd;       /*!< Subrate change event. */
} LlEvt_t;

/*! \brief      Event callback */
typedef bool_t (*llEvtCback_t)(LlEvt_t *pEvent);

/*! \brief      ACL callback */
typedef void (*llAclCback_t)(uint16_t handle, uint8_t numBufs);

/*! \brief      ISO callback */
typedef void (*llIsoCback_t)(uint8_t numHandles, uint16_t *pHandle, uint16_t *pNumPkts);

/*! \} */    /* LL_API_EVENT */

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*! \addtogroup LL_API_INIT
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Get default runtime configuration values.
 *
 *  \param      pCfg            Pointer to runtime configuration parameters.
 *
 *  This function returns default value for the LL subsystem's runtime configurations.
 */
/*************************************************************************************************/
void LlGetDefaultRunTimeCfg(LlRtCfg_t *pCfg);

/*************************************************************************************************/
/*!
 *  \brief      Initialize runtime configuration.
 *
 *  \param      pCfg            Pointer to runtime configuration parameters (data must be static).
 *
 *  This function initializes the LL subsystem's runtime configuration.
 *
 *  \note       This routine must be called only once before any other initialization routines.
 */
/*************************************************************************************************/
void LlInitRunTimeCfg(const LlRtCfg_t *pCfg);

/*************************************************************************************************/
/*!
 *  \brief      Initialize memory for connections.
 *
 *  \param      pFreeMem        Pointer to free memory.
 *  \param      freeMemSize     Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 *
 *  This function allocates memory for connections.
 *
 *  \note       This routine must be called after LlInitRunTimeCfg() but only once before any
 *              other initialization routines.
 */
/*************************************************************************************************/
uint16_t LlInitConnMem(uint8_t *pFreeMem, uint32_t freeMemSize);

/*************************************************************************************************/
/*!
 *  \brief      Initialize memory for extended advertising.
 *
 *  \param      pFreeMem        Pointer to free memory.
 *  \param      freeMemSize     Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 *
 *  This function allocates memory for extended advertising.
 *
 *  \note       This routine must be called after LlInitRunTimeCfg() but only once before any
 *              other initialization routines.
 */
/*************************************************************************************************/
uint16_t LlInitExtAdvMem(uint8_t *pFreeMem, uint32_t freeMemSize);

/*************************************************************************************************/
/*!
 *  \brief      Initialize memory for extended scanning.
 *
 *  \param      pFreeMem        Pointer to free memory.
 *  \param      freeMemSize     Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 *
 *  This function allocates memory for extended scanning.
 *
 *  \note       This routine must be called after LlInitRunTimeCfg() but only once before any
 *              other initialization routines.
 */
/*************************************************************************************************/
uint16_t LlInitExtScanMem(uint8_t *pFreeMem, uint32_t freeMemSize);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation as an advertising slave.
 *
 *  This function initializes the LL subsystem for use as an advertising slave.
 */
/*************************************************************************************************/
void LlAdvSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation for extended advertising slave.
 *
 *  This function initializes the LL subsystem for use as an extended advertising slave.
 */
/*************************************************************************************************/
void LlExtAdvSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation as a connectable slave.
 *
 *  This function initializes the LL subsystem for use as an advertising and connectable slave.
 */
/*************************************************************************************************/
void LlConnSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation as a encryptable connectable slave.
 *
 *  This function initializes the LL subsystem for use as an advertising and encryptable
 *  connectable slave.
 */
/*************************************************************************************************/
void LlEncConnSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation as a scanning master.
 *
 *  This function initializes the LL subsystem for use as a scanning master.
 */
/*************************************************************************************************/
void LlScanMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation for extended scanning master.
 *
 *  This function initializes the LL subsystem for use as an extended scanning master.
 */
/*************************************************************************************************/
void LlExtScanMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation as an initiating master.
 *
 *  This function initializes the LL subsystem for use as an initiating master.
 */
/*************************************************************************************************/
void LlInitMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation as an extended initiating master.
 *
 *  This function initializes the LL subsystem for use as an initiating master.
 */
/*************************************************************************************************/
void LlExtInitMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation as a connectable master.
 *
 *  This function initializes the LL subsystem for use as a scanning and initiating master.
 */
/*************************************************************************************************/
void LlConnMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation as a encryptable connectable slave.
 *
 *  This function initializes the LL subsystem for use as an advertising and encryptable
 *  connectable slave.
 */
/*************************************************************************************************/
void LlEncConnMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation with privacy.
 *
 *  This function initializes the LL subsystem for use with privacy.
 */
/*************************************************************************************************/
void LlPrivInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for secure connections.
 *
 *  This function initializes the LL subsystem for secure connections.
 */
/*************************************************************************************************/
void LlScInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for PHY features (slave).
 *
 *  This function initializes the LL subsystem for slave PHY features.
 */
/*************************************************************************************************/
void LlPhySlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for PHY features (master).
 *
 *  This function initializes the LL subsystem for master PHY features.
 */
/*************************************************************************************************/
void LlPhyMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for secure connections.
 *
 *  This function initializes the LL subsystem for secure connections.
 */
/*************************************************************************************************/
void LlChannelSelection2Init(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for test modes.
 *
 *  This function initializes the LL subsystem for test modes.
 */
/*************************************************************************************************/
void LlTestInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem with task handler.
 *
 *  \param      handlerId  WSF handler ID.
 *
 *  This function initializes the LL subsystem.  It is called once upon system initialization.
 *  It must be called before any other function in the LL API is called.
 */
/*************************************************************************************************/
void LlHandlerInit(wsfHandlerId_t handlerId);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation for power control.
 *
 *  This function initializes the LL subsystem for power control.
 */
/*************************************************************************************************/
void LlPowerControlInit(void);

/*************************************************************************************************/
/*!
 *  \brief      LL message dispatch handler.
 *
 *  \param      event       WSF event.
 *  \param      pMsg        WSF message.
 */
/*************************************************************************************************/
void LlHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg);

/*************************************************************************************************/
/*!
 *  \brief      Reset LL subsystem.
 *
 * Reset the LL subsystem.  All active connections are closed and all radio procedures such as
 * scanning or advertising are terminated.
 */
/*************************************************************************************************/
void LlReset(void);

/*************************************************************************************************/
/*!
 *  \brief      Register LL event handler.
 *
 *  \param      evtCback        Client callback function.
 *
 *  This function is called by a client to register for LL events.
 */
/*************************************************************************************************/
void LlEvtRegister(llEvtCback_t evtCback);

/*************************************************************************************************/
/*!
 *  \brief      Register ACL handler.
 *
 *  \param      sendCompCback   Client ACL send complete callback function.
 *  \param      recvPendCback   Client ACL receive pending callback function.
 *
 *  This function is called by a client to register for ACL data.
 */
/*************************************************************************************************/
void LlAclRegister(llAclCback_t sendCompCback, llAclCback_t recvPendCback);

/*************************************************************************************************/
/*!
 *  \brief      Register ISO handler.
 *
 *  \param      sendIsoCompCback   Client ISO send complete callback function.
 *  \param      recvIsoPendCback   Client ISO receive pending callback function.
 *
 *  This function is called by a client to register for ISO data.
 */
/*************************************************************************************************/
void LlIsoRegister(llIsoCback_t sendIsoCompCback, llIsoCback_t recvIsoPendCback);

/*! \} */    /* LL_API_INIT */

/*! \addtogroup LL_API_DEVICE
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Set Bluetooth device address
 *
 *  \param      pAddr       Bluetooth device address.
 *
 *  Set the BD address to be used by LL.
 */
/*************************************************************************************************/
void LlSetBdAddr(const uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \brief      Get Bluetooth device address
 *
 *  \param      pAddr       Bluetooth device address.
 *
 *  Get the BD address currently used by LL or all zeros if address is not set.
 */
/*************************************************************************************************/
void LlGetBdAddr(uint8_t *pAddr);

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
uint8_t LlSetRandAddr(const uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \brief      Get random device address.
 *
 *  \param      pAddr       Random Bluetooth device address.
 *
 *  \return     Status error code.
 *
 *  Get the random address currently used by LL or all zeros if address is not set.
 */
/*************************************************************************************************/
uint8_t LlGetRandAddr(uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \brief      Get versions
 *
 *  \param      pCompId     Company ID.
 *  \param      pBtVer      Bluetooth version.
 *  \param      pImplRev    Implementation revision.
 */
/*************************************************************************************************/
void LlGetVersion(uint16_t *pCompId, uint8_t *pBtVer, uint16_t *pImplRev);

/*************************************************************************************************/
/*!
 *  \brief      Get supported states.
 *
 *  \param      pStates     Supported states bitmask.
 *
 *  Return the states supported by the LL.
 */
/*************************************************************************************************/
void LlGetSupStates(uint8_t *pStates);

/*************************************************************************************************/
/*!
 *  \brief      Get features.
 *
 *  \param      pFeatures   Supported features bitmask.
 *
 *  Return the LE features supported by the LL.
 */
/*************************************************************************************************/
void LlGetFeatures(uint8_t *pFeatures);

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
uint8_t LlSetFeatures(const uint8_t *pFeatures);

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
uint8_t LlSetHostFeatures(uint8_t bitNum, bool_t bitVal);

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
uint8_t LlGetRandNum(uint8_t *pRandNum);

/*************************************************************************************************/
/*!
 *  \brief      Get white list size.
 *
 *  \return     Total number of white list entries.
 *
 *  Read the white list capacity supported by the LL.
 */
/*************************************************************************************************/
uint8_t LlGetWhitelistSize(void);

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
uint8_t LlClearWhitelist(void);

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
uint8_t LlAddDeviceToWhitelist(uint8_t addrType, bdAddr_t pAddr);

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
uint8_t LlRemoveDeviceFromWhitelist(uint8_t addrType, bdAddr_t pAddr);

/*************************************************************************************************/
/*!
 *  \brief      Add device to resolving list.
 *
 *  \param      peerAddrType        Peer identity address type.
 *  \param      pPeerIdentityAddr   Peer identity address.
 *  \param      pPeerIrk            Peer IRK.
 *  \param      pLocalIrk           Local IRK.
 *
 *  \return     Status error code.
 *
 *  Add device to resolving list.
 */
/*************************************************************************************************/
uint8_t LlAddDeviceToResolvingList(uint8_t peerAddrType, const uint8_t *pPeerIdentityAddr, const uint8_t *pPeerIrk, const uint8_t *pLocalIrk);

/*************************************************************************************************/
/*!
 *  \brief      Remove device from resolving list.
 *
 *  \param      peerAddrType        Peer identity address type.
 *  \param      pPeerIdentityAddr   Peer identity address.
 *
 *  \return     Status error code.
 *
 *  Remove device from resolving list.
 */
/*************************************************************************************************/
uint8_t LlRemoveDeviceFromResolvingList(uint8_t peerAddrType, const uint8_t *pPeerIdentityAddr);

/*************************************************************************************************/
/*!
 *  \brief      Clear resolving list.
 *
 *  \return     Status error code.
 *
 *  Clear resolving list.
 */
/*************************************************************************************************/
uint8_t LlClearResolvingList(void);

/*************************************************************************************************/
/*!
 *  \brief      Read resolving list size.
 *
 *  \param      pSize             Storage for resolving list size.
 *
 *  \return     Status error code.
 *
 *  Read number of address translation entries that can be stored in the resolving list.
 */
/*************************************************************************************************/
uint8_t LlReadResolvingListSize(uint8_t *pSize);

/*************************************************************************************************/
/*!
 *  \brief      Read peer resolvable address.
 *
 *  \param      addrType        Peer identity address type.
 *  \param      pIdentityAddr   Peer identity address.
 *  \param      pRpa            Storage for peer resolvable private address
 *
 *  \return     Status error code.
 *
 *  Get the peer resolvable private address that is currently being used for the peer identity
 *  address.
 */
/*************************************************************************************************/
uint8_t LlReadPeerResolvableAddr(uint8_t addrType, const uint8_t *pIdentityAddr, uint8_t *pRpa);

/*************************************************************************************************/
/*!
 *  \brief      Read local resolvable address.
 *
 *  \param      addrType        Peer identity address type.
 *  \param      pIdentityAddr   Peer identity address.
 *  \param      pRpa            Storage for peer resolvable private address
 *
 *  \return     Status error code.
 *
 *  Get the local resolvable private address that is currently being used for the peer identity
 *  address.
 */
/*************************************************************************************************/
uint8_t LlReadLocalResolvableAddr(uint8_t addrType, const uint8_t *pIdentityAddr, uint8_t *pRpa);

/*************************************************************************************************/
/*!
 *  \brief      Enable or disable address resolution.
 *
 *  \param      enable      Set to TRUE to enable address resolution or FALSE to disable address
 *                          resolution.
 *
 *  \return     Status error code.
 *
 *  Enable or disable address resolution so that received local or peer resolvable private
 *  addresses are resolved.
 */
/*************************************************************************************************/
uint8_t LlSetAddrResolutionEnable(uint8_t enable);

/*************************************************************************************************/
/*!
 *  \brief      Set resolvable private address timeout.
 *
 *  \param      rpaTimeout    Timeout measured in seconds.
 *
 *  \return     Status error code.
 *
 *  Set the time period between automatic generation of new resolvable private addresses.
 */
/*************************************************************************************************/
uint8_t LlSetResolvablePrivateAddrTimeout(uint16_t rpaTimeout);

/*************************************************************************************************/
/*!
 *  \brief      Set privacy mode.
 *
 *  \param      peerAddrType        Peer identity address type.
 *  \param      pPeerIdentityAddr   Peer identity address.
 *  \param      privMode            Privacy mode.
 *
 *  \return     Status error code.
 *
 *  Allow the host to specify the privacy mode to be used for a given entry on the resolving list.
 */
/*************************************************************************************************/
uint8_t LlSetPrivacyMode(uint8_t peerAddrType, const uint8_t *pPeerIdentityAddr, uint8_t privMode);

/*************************************************************************************************/
/*!
 *  \brief      Generate a P-256 public/private key pair.
 *
 *  \return     Status error code.
 *
 *  Generate a P-256 public/private key pair.  If another ECC operation (P-256 key pair or Diffie-
 *  Hellman key generation) is ongoing, an error will be returned.
 */
/*************************************************************************************************/
uint8_t LlGenerateP256KeyPair(void);

/*************************************************************************************************/
/*!
 *  \brief      Generate a Diffie-Hellman key.
 *
 *  \param      pubKey_x  Remote public key x-coordinate.
 *  \param      pubKey_y  Remote public key y-coordinate.
 *
 *  \return     Status error code.
 *
 *  Generate a Diffie-Hellman key from a remote public key and the local private key.  If another
 *  ECC operation (P-256 key pair or Diffie-Hellman key generation) is ongoing, an error will be
 *  returned.
 */
/*************************************************************************************************/
uint8_t LlGenerateDhKey(const uint8_t pubKey_x[LL_ECC_KEY_LEN], const uint8_t pubKey_y[LL_ECC_KEY_LEN]);

/*************************************************************************************************/
/*!
 *  \brief      Generate a Diffie-Hellman key.
 *
 *  \param      pubKey_x  Remote public key x-coordinate.
 *  \param      pubKey_y  Remote public key y-coordinate.
 *  \param      keyType   debug enable
 *
 *  \return     Status error code.
 *
 *  Generate a Diffie-Hellman key from a remote public key and the local private key.  If another
 *  ECC operation (P-256 key pair or Diffie-Hellman key generation) is ongoing, an error will be
 *  returned. If keyType == 1, debug keys will be used in place for the keys.
 */
/*************************************************************************************************/
uint8_t LlGenerateDhKeyV2(uint8_t pubKey_x[LL_ECC_KEY_LEN], uint8_t pubKey_y[LL_ECC_KEY_LEN], uint8_t keyType);

/*************************************************************************************************/
/*!
 *  \brief      Set P-256 private key for debug purposes.
 *
 *  \param      privKey   Private key, or all zeros to clear set private key.
 *
 *  \return     Status error code.
 *
 *  Set P-256 private key or clear set private key.  The private key will be used for generate key
 *  pairs and Diffie-Hellman keys until cleared.
 */
/*************************************************************************************************/
uint8_t LlSetP256PrivateKey(const uint8_t privKey[LL_ECC_KEY_LEN]);

/*************************************************************************************************/
/*!
 *  \brief      Set validate public key mode.
 *
 *  \param      validateMode   ALT1 or ALT2.
 *
 *  \return     Status error code.
 *
 */
/*************************************************************************************************/
uint8_t LlSetValidatePublicKeyMode(uint8_t validateMode);

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
uint8_t LlSetChannelClass(const uint8_t *pChanMap);

/*************************************************************************************************/
/*!
 *  \brief      Set operational mode flags.
 *
 *  \param      flags   Flags.
 *  \param      enable  TRUE to set flags or FALSE to clear flags.
 *
 *  \return     Status error code.
 *
 *  Set mode flags governing LL operations.
 */
/*************************************************************************************************/
uint8_t LlSetOpFlags(uint32_t flags, bool_t enable);

/*************************************************************************************************/
/*!
 *  \brief      Set the default Ext adv TX PHY options.
 *
 *  \param      phyOptions  PHY options.
 *
 *  Set the default TX PHY options for extended adv slave primary and secondary channel.
 */
/*************************************************************************************************/
void LlSetDefaultExtAdvTxPhyOptions(const uint8_t phyOptions);

/*! \} */    /* LL_API_DEVICE */

/*! \addtogroup LL_API_BROADCAST
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Set advertising transmit power.
 *
 *  \param      advTxPwr        Advertising transmit power level.
 *
 *  Set the advertising transmit power.
 */
/*************************************************************************************************/
void LlSetAdvTxPower(int8_t advTxPwr);

/*************************************************************************************************/
/*!
 *  \brief      Get advertising transmit power.
 *
 *  \param      pAdvTxPwr       Advertising transmit power level.
 *
 *  \return     Status error code.
 *
 *  Return the advertising transmit power.
 */
/*************************************************************************************************/
uint8_t LlGetAdvTxPower(int8_t *pAdvTxPwr);

/*************************************************************************************************/
/*!
 *  \brief      Set advertising parameter.
 *
 *  \param      advIntervalMin  Minimum advertising interval.
 *  \param      advIntervalMax  Maximum advertising interval.
 *  \param      advType         Advertising type.
 *  \param      ownAddrType     Address type used by this device.
 *  \param      peerAddrType    Address type of peer device.  Only used for directed advertising.
 *  \param      pPeerAddr       Address of peer device.  Only used for directed advertising.
 *  \param      advChanMap      Advertising channel map.
 *  \param      advFiltPolicy   Advertising filter policy.
 *
 *  \return     Status error code.
 *
 *  Set advertising parameters.
 *
 *  \note       This function must only be called when advertising is disabled.
 */
/*************************************************************************************************/
uint8_t LlSetAdvParam(uint16_t advIntervalMin, uint16_t advIntervalMax, uint8_t advType,
                      uint8_t ownAddrType, uint8_t peerAddrType, const uint8_t *pPeerAddr,
                      uint8_t advChanMap, uint8_t advFiltPolicy);

/*************************************************************************************************/
/*!
 *  \brief      Set advertising data.
 *
 *  \param      len     Data buffer length.
 *  \param      pData   Advertising data buffer.
 *
 *  \return     Status error code.
 *
 *  Set advertising data data.
 */
/*************************************************************************************************/
uint8_t LlSetAdvData(uint8_t len, const uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief      Set scan response data.
 *
 *  \param      len     Data buffer length.
 *  \param      pData   Scan response data buffer.
 *
 *  \return     Status error code.
 *
 *  Set scan response data.
 */
/*************************************************************************************************/
uint8_t LlSetScanRespData(uint8_t len, const uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief      Advertising enable.
 *
 *  \param      enable          Set to TRUE to enable advertising, FALSE to disable advertising.

 *  Enable or disable advertising.
 */
/*************************************************************************************************/
void LlAdvEnable(uint8_t enable);

/*************************************************************************************************/
/*!
 *  \brief      Set advertising set random device address.
 *
 *  \param      handle      Advertising handle.
 *  \param      pAddr       Random Bluetooth device address.
 *
 *  \return     Status error code.
 *
 *  Set the random address to be used by a advertising set.
 */
/*************************************************************************************************/
uint8_t LlSetAdvSetRandAddr(uint8_t handle, const uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \brief      Get periodic channel map.
 *
 *  \param      handle      Periodic advertiser/scanner handle.
 *  \param      isAdv       TRUE for periodic advertiser, FALSE for periodic scanner.
 *
 *  \return     64-bit formatted channel mask.
 *
 *  Get the 64-bit channel mask of the channel map
 */
/*************************************************************************************************/
uint64_t LlGetPeriodicChanMap(uint16_t handle, bool_t isAdv);

/*************************************************************************************************/
/*!
 *  \brief      Get advertising set random device address.
 *
 *  \param      handle      Advertising handle.
 *  \param      pAddr       Random Bluetooth device address.
 *
 *  \return     Status error code.
 *
 *  Get the random address to be used by a advertising set.
 */
/*************************************************************************************************/
uint8_t LlGetAdvSetRandAddr(uint8_t handle, uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \brief      Set extended advertising parameters.
 *
 *  \param      handle          Advertising handle.
 *  \param      pExtAdvParam    Extended advertising parameters.
 *
 *  \return     Status error code.
 *
 *  Set extended advertising parameters.
 *
 *  \note       This function must only be called when advertising for this set is disabled.
 */
/*************************************************************************************************/
uint8_t LlSetExtAdvParam(uint8_t handle, LlExtAdvParam_t *pExtAdvParam);

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
uint8_t LlGetExtAdvTxPowerLevel(uint16_t handle, int8_t *pLevel);

/*************************************************************************************************/
/*!
 *  \brief      Set extended advertising data.
 *
 *  \param      handle      Advertising handle.
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
uint8_t LlSetExtAdvData(uint8_t handle, uint8_t op, uint8_t fragPref, uint8_t len, const uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief      Set extended scan response data.
 *
 *  \param      handle      Advertising handle.
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
uint8_t LlSetExtScanRespData(uint8_t handle, uint8_t op, uint8_t fragPref, uint8_t len, const uint8_t *pData);

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
void LlExtAdvEnable(uint8_t enable, uint8_t numAdvSets, LlExtAdvEnableParam_t enaParam[]);

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
uint8_t LlReadMaxAdvDataLen(uint16_t *pLen);

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
uint8_t LlReadNumSupAdvSets(uint8_t *pNumSets);

/*************************************************************************************************/
/*!
 *  \brief      Remove advertising set.
 *
 *  \param      handle      Advertising handle.
 *
 *  \return     Status error code.
 *
 *  Removes the an advertising set from the LL.
 */
/*************************************************************************************************/
uint8_t LlRemoveAdvSet(uint8_t handle);

/*************************************************************************************************/
/*!
 *  \brief      Clear advertising sets.
 *
 *  \return     Status error code.
 *
 *  Remove all existing advertising sets from the LL.
 */
/*************************************************************************************************/
uint8_t LlClearAdvSets(void);

/*************************************************************************************************/
/*!
 *  \brief      Set periodic advertising parameters.
 *
 *  \param      handle          Advertising handle.
 *  \param      pPerAdvParam    Periodic advertising parameters.
 *
 *  \return     Status error code.
 *
 *  Set periodic advertising parameters.
 *
 *  \note       This function must only be called when advertising handle exists.
 */
/*************************************************************************************************/
uint8_t LlSetPeriodicAdvParam(uint8_t handle, LlPerAdvParam_t *pPerAdvParam);

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
 *  Set periodic advertising data.
 */
/*************************************************************************************************/
uint8_t LlSetPeriodicAdvData(uint8_t handle, uint8_t op, uint8_t len, const uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief      Set periodic advertising enable.
 *
 *  \param      enable      TRUE to enable advertising, FALSE to disable advertising.
 *  \param      handle      Advertising handle.
 *
 *  \return     Status error code.
 *
 *  Enable or disable periodic advertising.
 */
/*************************************************************************************************/
void LlSetPeriodicAdvEnable(uint8_t enable, uint8_t handle);

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
uint8_t LlSetAuxOffsetDelay(uint8_t handle, uint32_t delayUsec);

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
uint8_t LlSetExtAdvDataFragLen(uint8_t handle, uint8_t fragLen);

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
uint8_t LlSetExtAdvTxPhyOptions(uint8_t handle, uint8_t priPhyOpts, uint8_t secPhyOpts);

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
void LlReadSupTxPower(int8_t *pMinTxPwr, int8_t *pMaxTxPwr);

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
void LlReadRfPathComp(int16_t *pTxPathComp, int16_t *pRxPathComp);

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
uint8_t LlWriteRfPathComp(int16_t txPathComp, int16_t rxPathComp);

/*************************************************************************************************/
/*!
 *  \brief      Scan report enable.
 *
 *  \param      enable          Set to TRUE to enable scan reports, FALSE to disable scan reports.
 *
 *  Enable or disable reports about the scanners from which an advertiser receives scan requests.
 */
/*************************************************************************************************/
void LlScanReportEnable(uint8_t enable);

/*! \} */    /* LL_API_BROADCAST */

/*! \addtogroup LL_API_OBSERVER
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Set scan channel map.
 *
 *  \param      chanMap         Scan channel map.
 *
 *  \return     Status error code.
 *
 *  Set scan channel map.
 *
 *  \note       This function must only be called when scanning is disabled.
 */
/*************************************************************************************************/
uint8_t LlSetSetScanChanMap(uint8_t chanMap);

/*************************************************************************************************/
/*!
 *  \brief      Set scan parameters.
 *
 *  \param      pParam          Scan parameters.
 *
 *  \return     Status error code.
 *
 *  Set scan parameters.
 *
 *  \note       This function must only be called when scanning is disabled.
 */
/*************************************************************************************************/
uint8_t LlSetScanParam(const LlScanParam_t *pParam);

/*************************************************************************************************/
/*!
 *  \brief      Scan enable.
 *
 *  \param      enable          Set to TRUE to enable scanning, FALSE to disable scanning.
 *  \param      filterDup       Set to TRUE to filter duplicates.
 *
 *  Enable or disable scanning.  This function is only used when operating in master role.
 */
/*************************************************************************************************/
void LlScanEnable(uint8_t enable, uint8_t filterDup);

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
uint8_t LlSetExtScanParam(uint8_t ownAddrType, uint8_t scanFiltPolicy, uint8_t scanPhys, const LlExtScanParam_t param[]);

/*************************************************************************************************/
/*!
 *  \brief      Extended scan enable.
 *
 *  \param      enable          Set to TRUE to enable scanning, FALSE to disable scanning.
 *  \param      filterDup       Set to TRUE to filter duplicates.
 *  \param      duration        Duration.
 *  \param      period          Period.
 *
 *  Enable or disable extended scanning.
 */
/*************************************************************************************************/
void LlExtScanEnable(uint8_t enable, uint8_t filterDup, uint16_t duration, uint16_t period);

/*************************************************************************************************/
/*!
 *  \brief      Create synchronization of periodic advertising.
 *
 *  \param      pParam          Create sync parameters.
 *
 *  \return     Status error code.
 *
 *  Create synchronization of periodic advertising.
 */
/*************************************************************************************************/
uint8_t LlPeriodicAdvCreateSync(const LlPerAdvCreateSyncCmd_t *pParam);

/*************************************************************************************************/
/*!
 *  \brief      Cancel pending synchronization of periodic advertising.
 *
 *  \return     Status error code.
 *
 *  Cancel pending synchronization of periodic advertising.
 */
/*************************************************************************************************/
uint8_t LlPeriodicAdvCreateSyncCancel(void);

/*************************************************************************************************/
/*!
 *  \brief      Stop synchronization of periodic advertising.
 *
 *  \param      syncHandle      Sync handle.
 *
 *  \return     Status error code.
 *
 *  Stop synchronization of periodic advertising.
 */
/*************************************************************************************************/
uint8_t LlPeriodicAdvTerminateSync(uint16_t syncHandle);

/*************************************************************************************************/
/*!
 *  \brief      Add device to periodic advertiser list.
 *
 *  \param      pParam          Add to periodic list parameters.
 *
 *  \return     Status error code.
 *
 *  Add device to periodic advertiser list.
 */
/*************************************************************************************************/
uint8_t LlAddDeviceToPeriodicAdvList(const LlDevicePerAdvList_t *pParam);

/*************************************************************************************************/
/*!
 *  \brief      Remove device from periodic advertiser list command.
 *
 *  \param      pParam          Remove from periodic list parameters.
 *
 *  \return     Status error code.
 *
 *
 */
/*************************************************************************************************/
uint8_t LlRemoveDeviceFromPeriodicAdvList(const LlDevicePerAdvList_t *pParam);

/*************************************************************************************************/
/*!
 *  \brief      Clear all devices in periodic advertiser list command.
 *
 *  \return     Status error code.
 *
 *  Clear all devices in periodic advertiser list command.
 */
/*************************************************************************************************/
uint8_t LlClearPeriodicAdvList(void);

/*************************************************************************************************/
/*!
 *  \brief      Read total number of devices in periodic advertiser list command.
 *
 *  \param      pListSize       Return size value of periodic advertiser list
 *
 *  \return     Status error code.
 *
 *  Read total number of devices in periodic advertiser list command.
 */
/*************************************************************************************************/
uint8_t LlReadPeriodicAdvListSize(uint8_t *pListSize);

/*************************************************************************************************/
/*!
 *  \brief      Enable or disable reports for the periodic advertising sync.
 *
 *  \param      syncHandle        Periodic sync handle.
 *  \param      enable            1 to enable reports, 0 to disable reports.
 *
 *  \return     Status error code.
 *
 *  Enable or disable reports for the periodic advertising sync.
 */
/*************************************************************************************************/
uint8_t LlSetPeriodicAdvRcvEnable(uint16_t syncHandle, uint8_t enable);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for PAST(Periodic advertising sync transfer).
 *
 *  This function initializes the LL subsystem for PAST(Periodic advertising sync transfer).
 */
/*************************************************************************************************/
void LlPastInit(void);

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
 *  Send sync info about periodic advertising to a connected device.
 */
/*************************************************************************************************/
uint8_t LlPeriodicAdvSyncTransfer(uint16_t connHandle, uint16_t serviceData, uint16_t syncHandle);

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
uint8_t LlPeriodicAdvSetInfoTransfer(uint16_t connHandle, uint16_t serviceData, uint8_t advHandle);

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
uint8_t LlSetPeriodicAdvSyncTransParams(uint16_t connHandle, uint8_t mode, uint16_t skip, uint16_t syncTimeout, uint8_t cteType);

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
uint8_t LlSetDefaultPeriodicAdvSyncTransParams(uint8_t mode, uint16_t skip, uint16_t syncTimeout, uint8_t cteType);

/*! \} */    /* LL_API_OBSERVER */

/*! \addtogroup LL_API_CONN
 *  \{ */

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
uint8_t LlDisconnect(uint16_t handle, uint8_t reason);

/*************************************************************************************************/
/*!
 *  \brief      Set connection's operational mode flags.
 *
 *  \param      handle  Connection handle.
 *  \param      flags   Flags.
 *  \param      enable  TRUE to set flags or FALSE to clear flags.
 *
 *  \return     Status error code.
 *
 *  Set connection's operational mode flags governing LL operations.
 */
/*************************************************************************************************/
uint8_t LlSetConnOpFlags(uint16_t handle, uint32_t flags, bool_t enable);

/*************************************************************************************************/
/*!
 *  \brief      Read remote features.
 *
 *  \param      handle          Connection handle.
 *
 *  \return     Status error code.
 *
 *  Read the link layer features of the remote device.
 */
/*************************************************************************************************/
uint8_t LlReadRemoteFeat(uint16_t handle);

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
uint8_t LlReadRemoteVerInfo(uint16_t handle);

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
uint8_t LlGetRssi(uint16_t handle, int8_t *pRssi);

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
uint8_t LlGetTxPowerLevel(uint16_t handle, uint8_t type, int8_t *pLevel);

/*************************************************************************************************/
/*!
 *  \brief      Get connection's enhanced TX power level and max txPower.
 *
 *  \param      handle          Connection handle.
 *  \param      phy             PHY.
 *  \param      pCurPwr         Transmit power level.
 *  \param      pMaxPwr         Max power level.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlGetEnhTxPowerLevel(uint16_t handle, uint8_t phy, int8_t *pCurPwr, int8_t *pMaxPwr);

/*************************************************************************************************/
/*!
 *  \brief      Set connection's TX power level (all PHYs).
 *
 *  \param      handle          Connection handle.
 *  \param      level           Transmit power level.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlSetAllPhyTxPowerLevel(uint16_t handle, int8_t level);

/*************************************************************************************************/
/*!
 *  \brief      Set connection's TX power level for a PHY.
 *
 *  \param      handle          Connection handle.
 *  \param      level           Transmit power level.
 *  \param      phy             PHY txPower to change.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlSetPhyTxPowerLevel(uint16_t handle, int8_t level, uint8_t phy);

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
uint8_t LlConnUpdate(uint16_t handle, const LlConnSpec_t *pConnSpec);

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
uint8_t LlRemoteConnParamReqReply(uint16_t handle, const LlConnSpec_t *pConnSpec);

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
uint8_t LlRemoteConnParamReqNegReply(uint16_t handle, uint8_t reason);

/*************************************************************************************************/
/*!
 *  \brief      Set connection's channel map.
 *
 *  \param      handle          Connection handle.
 *  \param      pChanMap        Channel map.
 *
 *  \return     Status error code.
 *
 *  Set the channel map of a connection.
 */
/*************************************************************************************************/
uint8_t LlSetChannelMap(uint16_t handle, const uint8_t *pChanMap);

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
uint8_t LlGetChannelMap(uint16_t handle, uint8_t *pChanMap);

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
uint8_t LlSetDataLen(uint16_t handle, uint16_t txLen, uint16_t txTime);

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
void LlReadDefaultDataLen(uint16_t *pMaxTxLen, uint16_t *pMaxTxTime);

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
uint8_t LlWriteDefaultDataLen(uint16_t maxTxLen, uint16_t maxTxTime);

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
void LlReadMaximumDataLen(uint16_t *pMaxTxLen, uint16_t *pMaxTxTime, uint16_t *pMaxRxLen, uint16_t *pMaxRxTime);

/*************************************************************************************************/
/*!
 *  \brief      Read current transmitter PHY and receive PHY.
 *
 *  \param      handle            Connection handle.
 *  \param      pTxPhy            Storage for transmitter PHY.
 *  \param      pRxPhy            Storage for receiver PHY.
 *
 *  \return     Status error code.
 *
 *  Read current transmitter PHY and receive PHY.
 */
/*************************************************************************************************/
uint8_t LlReadPhy(uint16_t handle, uint8_t *pTxPhy, uint8_t *pRxPhy);

/*************************************************************************************************/
/*!
 *  \brief      Set default PHYs.
 *
 *  \param      allPhys           All PHYs preferences.
 *  \param      txPhys            Preferred transmitter PHYs.
 *  \param      rxPhys            Preferred receiver PHYs.
 *
 *  \return     Status error code.
 *
 *  Specify the preferred values for the transmitter PHY and receiver PHY to be used for all
 *  subsequent connections over the LE transport.
 */
/*************************************************************************************************/
uint8_t LlSetDefaultPhy(uint8_t allPhys, uint8_t txPhys, uint8_t rxPhys);

/*************************************************************************************************/
/*!
 *  \brief      Validate if specified PHYs are supported by LL.
 *
 *  \param      txPhys            Preferred transmitter PHYs.
 *  \param      rxPhys            Preferred receiver PHYs.
 *
 *  \return     TRUE if all specified PHYs are supported.
 */
/*************************************************************************************************/
bool_t llValidatePhySupport(uint8_t txPhys, uint8_t rxPhys);

/*************************************************************************************************/
/*!
 *  \brief      Set PHY for a connection.
 *
 *  \param      handle            Connection handle.
 *  \param      allPhys           All PHYs preferences.
 *  \param      txPhys            Preferred transmitter PHYs.
 *  \param      rxPhys            Preferred receiver PHYs.
 *  \param      phyOptions        PHY options.
 *
 *  \return     Status error code.
 *
 *  Set PHY preferences for a given connection.  The controller might not be able to make the
 *  change (e.g., because the peer does not support the requested PHY) or may decide that the
 *  current PHY is preferable.
 */
/*************************************************************************************************/
uint8_t LlSetPhy(uint16_t handle, uint8_t allPhys, uint8_t txPhys, uint8_t rxPhys, uint16_t phyOptions);

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
uint8_t LlSetLocalMinUsedChan(uint8_t phys, int8_t pwrThres, uint8_t minUsedCh);

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
uint8_t LlGetPeerMinUsedChan(uint16_t handle, uint8_t *pPeerMinUsedChan);

/*************************************************************************************************/
/*!
 *  \brief      Used to read the sleep clock accuracy of the peer device.
 *
 *  \param      handle       Handle of the ACL.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlRequestPeerSca(uint16_t handle);

/*! \} */    /* LL_API_CONN */

/*! \addtogroup LL_API_CENTRAL
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Create connection.
 *
 *  \param      pInitParam      Initiating parameters.
 *  \param      pConnSpec       Connection specification.
 *
 *  \return     Status error code.
 *
 *  Create a connection to the specified peer address with the specified connection parameters.
 *  This function is only when operating in master role.
 */
/*************************************************************************************************/
uint8_t LlCreateConn(const LlInitParam_t *pInitParam, const LlConnSpec_t *pConnSpec);

/*************************************************************************************************/
/*!
 *  \brief      Extended create connection.
 *
 *  \param      pInitParam      Initiating parameters.
 *  \param      scanParam       Scan parameters table indexed by PHY.
 *  \param      connSpec        Connection specification table indexed by PHY.
 *
 *  \return     Status error code.
 *
 *  Create a connection to the specified peer address with the specified connection parameters.
 *  This function is only when operating in master role.
 */
/*************************************************************************************************/
uint8_t LlExtCreateConn(const LlExtInitParam_t *pInitParam, const LlExtInitScanParam_t scanParam[], const LlConnSpec_t connSpec[]);

/*************************************************************************************************/
/*!
 *  \brief      Cancel a create connection operation.
 *
 *  Cancel a connection before it is established.  This function is only used when operating
 *  in master role.
 */
/*************************************************************************************************/
void LlCreateConnCancel(void);

/*! \} */    /* LL_API_CENTRAL */

/*! \addtogroup LL_API_ENCRYPT
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Encrypt data.
 *
 *  \param      pKey            Encryption key.
 *  \param      pData           16 bytes of plain text data.
 *
 *  \return     Status error code.
 *
 *  Request the LL to encryption a block of data in place.
 */
/*************************************************************************************************/
uint8_t LlEncrypt(uint8_t *pKey, uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief      Start encryption.
 *
 *  \param      handle          Connection handle.
 *  \param      pRand           Pointer to the random number.
 *  \param      diversifier     Diversifier value.
 *  \param      pKey            Pointer to the encryption key.
 *
 *  \return     Status error code.
 *
 *  Start or restart link layer encryption on a connection.  This function is only used when
 *  operating in master role.
 */
/*************************************************************************************************/
uint8_t LlStartEncryption(uint16_t handle, const uint8_t *pRand, uint16_t diversifier, const uint8_t *pKey);

/*************************************************************************************************/
/*!
 *  \brief      Reply to a LTK request.
 *
 *  \param      handle          Connection handle.
 *  \param      pKey            Pointer to new key.
 *
 *  \return     Status error code.
 *
 *  Provide the requested LTK encryption key.  This function is only used when operating in
 *  slave mode.
 */
/*************************************************************************************************/
uint8_t LlLtkReqReply(uint16_t handle, const uint8_t *pKey);

/*************************************************************************************************/
/*!
 *  \brief      Negative reply to a LTK request.
 *
 *  \param      handle          Connection handle.
 *
 *  \return     Status error code.
 *
 *  Requested LTK encryption key not available.  This function is only used when operating in
 *  slave mode.
 */
/*************************************************************************************************/
uint8_t LlLtkReqNegReply(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief      Read authenticated payload timeout value.
 *
 *  \param      handle          Connection handle.
 *  \param      pTimeout        Pointer to timeout value.
 *
 *  \return     Status error code.
 *
 *  Read authenticated payload timeout value for the given handle.
 */
/*************************************************************************************************/
uint8_t LlReadAuthPayloadTimeout(uint16_t handle, uint16_t *pTimeout);

/*************************************************************************************************/
/*!
 *  \brief      Write authenticated payload timeout value.
 *
 *  \param      handle          Connection handle.
 *  \param      timeout         New timeout value.
 *
 *  \return     Status error code.
 *
 *  Write new authenticated payload timeout value for the given handle.
 */
/*************************************************************************************************/
uint8_t LlWriteAuthPayloadTimeout(uint16_t handle, uint16_t timeout);

/*************************************************************************************************/
/*!
 *  \brief      Get encryption mode used in a connection.
 *
 *  \param      handle          Connection handle.
 *  \param      pMode           Storage for encryption mode.
 *
 *  \return     Status error code.
 *
 *  Get the encryption mode used by a connection.
 */
/*************************************************************************************************/
uint8_t LlGetEncMode(uint16_t handle, LlEncMode_t *pMode);

/*************************************************************************************************/
/*!
 *  \brief      Set encryption mode used in a connection.
 *
 *  \param      handle          Connection handle.
 *  \param      pMode           New encryption mode.
 *
 *  \return     Status error code.
 *
 *  Set the encryption mode used by a connection. Must be called before encryption is started or
 *  when encryption is paused.
 */
/*************************************************************************************************/
uint8_t LlSetEncMode(uint16_t handle, const LlEncMode_t *pMode);

/*! \} */    /* LL_API_ENCRYPT */

/*! \addtogroup LL_API_DATA
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Get the maximum ACL buffers size.
 *
 *  \return     Maximum buffers size in bytes.
 */
/*************************************************************************************************/
uint16_t LlGetAclMaxSize(void);

/*************************************************************************************************/
/*!
 *  \brief      Get the number of buffers in the LL ACL transmit queue.
 *
 *  \return     Number of buffers.
 */
/*************************************************************************************************/
uint8_t LlGetAclTxBufs(void);

/*************************************************************************************************/
/*!
 *  \brief      Get the number of buffers in the LL ACL receive queue.
 *
 *  \return     Number of buffers.
 */
/*************************************************************************************************/
uint8_t LlGetAclRxBufs(void);

/*************************************************************************************************/
/*!
 *  \brief      Get the maximum ISO buffers size between host and controller.
 *
 *  \return     Maximum buffers size in bytes.
 */
/*************************************************************************************************/
uint16_t LlGetIsoMaxBufSize(void);

/*************************************************************************************************/
/*!
 *  \brief      Get the number of buffers in the LL ISO transmit queue.
 *
 *  \return     Number of buffers.
 */
/*************************************************************************************************/
uint8_t LlGetIsoTxBufs(void);

/*************************************************************************************************/
/*!
 *  \brief      Get the number of buffers in the LL ISO receive queue.
 *
 *  \return     Number of buffers.
 */
/*************************************************************************************************/
uint8_t LlGetIsoRxBufs(void);

/*************************************************************************************************/
/*!
 *  \brief      Read ISO link Quality stats.
 *
 *  \param      handle  connection handle.
 *  \param      pStats  link quality statistics.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlReadIsoLinkQual(uint16_t handle, LlIsoLinkQual_t * pStats);

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
void LlSendAclData(uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief      Receive an ACL data packet
 *
 *  \return     Data buffer.
 *
 *  Receive an ACL data packet. If no ACL buffers are available this function returns NULL.
 *
 *  \note       The host must deallocate the buffer by calling WsfMsgFree() and call LlRecvBufCmpl() to
 *              update LL accounting.
 */
/*************************************************************************************************/
uint8_t *LlRecvAclData(void);

/*************************************************************************************************/
/*!
 *  \brief      Indicate that received ACL data buffer has been deallocated
 *
 *  \param      numBufs     Number of completed packets.
 *
 *  Indicate that received ACL data buffer has been deallocated.
 */
/*************************************************************************************************/
void LlRecvAclDataComplete(uint8_t numBufs);

/*! \} */    /* LL_API_DATA */

/*! \addtogroup LL_API_TEST
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Enter transmit test mode.
 *
 *  \param      rfChan      RF channel number, i.e. "rfChan = (F - 2402) / 2)".
 *  \param      len         Length of test data.
 *  \param      pktType     Test packet payload type.
 *  \param      numPkt      Auto terminate after number of packets, 0 for infinite.
 *
 *  \return     Status error code.
 *
 *  Start the transmit test mode on the given channel.
 */
/*************************************************************************************************/
uint8_t LlTxTest(uint8_t rfChan, uint8_t len, uint8_t pktType, uint16_t numPkt);

/*************************************************************************************************/
/*!
 *  \brief      Enter receive test mode.
 *
 *  \param      rfChan      RF channel number, i.e. "rfChan = (F - 2402) / 2)".
 *  \param      numPkt      Auto terminate after number of successful packets, 0 for infinite.
 *
 *  \return     Status error code.
 *
 *  Start the receive test mode on the given channel.
 */
/*************************************************************************************************/
uint8_t LlRxTest(uint8_t rfChan, uint16_t numPkt);

/*************************************************************************************************/
/*!
 *  \brief      Enter enhanced transmit test mode.
 *
 *  \param      rfChan      RF channel number, i.e. "rfChan = (F - 2402) / 2)".
 *  \param      len         Length of test data.
 *  \param      pktType     Test packet payload type.
 *  \param      phy         Transmitter PHY.
 *  \param      numPkt      Auto terminate after number of packets, 0 for infinite.
 *
 *  \return     Status error code.
 *
 *  Start the transmit test mode on the given channel.
 */
/*************************************************************************************************/
uint8_t LlEnhancedTxTest(uint8_t rfChan, uint8_t len, uint8_t pktType, uint8_t phy, uint16_t numPkt);

/*************************************************************************************************/
/*!
 *  \brief      Enter enhanced receive test mode.
 *
 *  \param      rfChan      RF channel number, i.e. "rfChan = (F - 2402) / 2)".
 *  \param      phy         Receiver PHY.
 *  \param      modIdx      Modulation index.
 *  \param      numPkt      Auto terminate after number of successful packets, 0 for infinite.
 *
 *  \return     Status error code.
 *
 *  Start the receive test mode on the given channel.
 */
/*************************************************************************************************/
uint8_t LlEnhancedRxTest(uint8_t rfChan, uint8_t phy, uint8_t modIdx, uint16_t numPkt);

/*************************************************************************************************/
/*!
 *  \brief      End test mode.
 *
 *  \param      pRpt        Report return buffer.
 *
 *  \return     Status error code.
 *
 *  End test mode and return the report.
 */
/*************************************************************************************************/
uint8_t LlEndTest(LlTestReport_t *pRpt);

/*************************************************************************************************/
/*!
 *  \brief      Set pattern of errors for Tx test mode.
 *
 *  \param      pattern   Error pattern (1s = no error; 0s = CRC failure).
 *
 *  \return     Status error code.
 *
 *  Set pattern of errors for Tx test mode.
 *
 *  \note       The error pattern must be set after the Tx test is started.
 */
/*************************************************************************************************/
uint8_t LlSetTxTestErrorPattern(uint32_t pattern);

/*************************************************************************************************/
/*!
 *  \brief      Modify the sleep clock accuracy.
 *
 *  \param      action   Increase or decrease the sleep clock accuracy.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlModifySleepClockAccuracy(uint8_t action);

/*************************************************************************************************/
/*!
 *  \brief      Enable ISO Tx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pldType     Payload type.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlIsoTxTest(uint16_t handle, uint8_t pldType);

/*************************************************************************************************/
/*!
 *  \brief      Enable ISO Rx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pldType     Payload type.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlIsoRxTest(uint16_t handle, uint8_t pldType);

/*************************************************************************************************/
/*!
 *  \brief      ISO read test counters.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pCtr        Test Counter.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlIsoReadTestCounter(uint16_t handle, LlIsoTestCtrs_t *pCtr);

/*************************************************************************************************/
/*!
 *  \brief      Terminate ISO Tx or Rx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pCtr        Test counters.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlIsoTestEnd(uint16_t handle, LlIsoTestCtrs_t *pCtr);

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
uint8_t LlSetTxPowerReporting(uint16_t handle, uint8_t enableLocal, uint8_t enableRemote);

/*************************************************************************************************/
/*!
 *  \brief  Set enable state for power monitoring.
 *
 *  \param  handle  Handle identifier for connection.
 *  \param  enable  Enable status for power monitor.
 *
 *  \return Status error code.
 *
 *  \note  Path loss must be disabled.
 */
/*************************************************************************************************/
uint8_t LlSetPowerMonitorEnable(uint16_t handle, bool_t enable);

/*************************************************************************************************/
/*!
 *  \brief  Set path loss monitoring parameters.
 *
 *  \param  handle      Handle identifier for connection.
 *  \param  highThresh  High extreme threshold.
 *  \param  highHyst    High extreme hysteresis.
 *  \param  lowThresh   Low extreme threshold.
 *  \param  lowHyst     Low extreme hysteresis.
 *  \param  minTime     Minimum time spent to trigger event generation.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LlSetPathLossReportingParams(uint16_t handle, uint8_t highThresh, uint8_t highHyst, uint8_t lowThresh, uint8_t lowHyst, uint16_t minTime);

/*************************************************************************************************/
/*!
 *  \brief      Set path loss enable.
 *
 *  \param      handle          Connection handle.
 *  \param      enable          Enable flag.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlSetPathLossReportingEnable(uint16_t handle, uint8_t enable);

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
uint8_t LlPowerCtrlReq(uint16_t handle, int8_t delta, uint8_t phy);

/*************************************************************************************************/
/*!
 *  \brief      ISO event complete enable.
 *
 *  \param      enable          Set to TRUE to enable, FALSE to disable.
 *
 *  Enable or disable reports about the scanners from which an advertiser receives scan requests.
 */
/*************************************************************************************************/
void LlIsoEventCompleteEnable(uint8_t enable);

/*! \} */    /* LL_API_TEST */

/*! \addtogroup LL_API_DIAG
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Get advertising set context size.
 *
 *  \param      pMaxAdvSets     Buffer to return the maximum number of advertising sets.
 *  \param      pAdvSetCtxSize  Buffer to return the size in bytes of the advertising set context.
 *
 *  Return the advertising set context sizes.
 */
/*************************************************************************************************/
void LlGetAdvSetContextSize(uint8_t *pMaxAdvSets, uint16_t *pAdvSetCtxSize);

/*************************************************************************************************/
/*!
 *  \brief      Get connection context size.
 *
 *  \param      pMaxConn        Buffer to return the maximum number of connections.
 *  \param      pConnCtxSize    Buffer to return the size in bytes of the connection context.
 *
 *  Return the connection context sizes.
 */
/*************************************************************************************************/
void LlGetConnContextSize(uint8_t *pMaxConn, uint16_t *pConnCtxSize);

/*************************************************************************************************/
/*!
 *  \brief      Get extended scan context size.
 *
 *  \param      pMaxExtScan     Buffer to return the maximum number of extended scanners.
 *  \param      pExtScanCtxSize Buffer to return the size in bytes of the extended scanner context.
 *
 *  Return the advertising set context sizes.
 */
/*************************************************************************************************/
void LlGetExtScanContextSize(uint8_t *pMaxExtScan, uint16_t *pExtScanCtxSize);

/*************************************************************************************************/
/*!
 *  \brief      Get extended initiator context size.
 *
 *  \param      pMaxExtInit     Buffer to return the maximum number of extended initiators.
 *  \param      pExtInitCtxSize Buffer to return the size in bytes of the extended initiator context.
 *
 *  Return the advertising set context sizes.
 */
/*************************************************************************************************/
void LlGetExtInitContextSize(uint8_t *pMaxExtInit, uint16_t *pExtInitCtxSize);

/*************************************************************************************************/
/*!
 *  \brief      Get periodic scan context size.
 *
 *  \param      pMaxPerScan     Buffer to return the maximum number of periodic scanners.
 *  \param      pPerScanCtxSize Buffer to return the size in bytes of the periodic scanner context.
 *
 *  Return the advertising set context sizes.
 */
/*************************************************************************************************/
void LlGetPerScanContextSize(uint8_t *pMaxPerScan, uint16_t *pPerScanCtxSize);

/*************************************************************************************************/
/*!
 *  \brief      Get CIS context size.
 *
 *  \param      pMaxCig         Buffer to return the maximum number of CIG.
 *  \param      pCigCtxSize     Buffer to return the size in bytes of the CIG context.
 *  \param      pMaxCis         Buffer to return the maximum number of CIS.
 *  \param      pCisCtxSize     Buffer to return the size in bytes of the CIS context.
 *
 *  Return the connection context sizes.
 */
/*************************************************************************************************/
void LlGetCisContextSize(uint8_t *pMaxCig, uint16_t *pCigCtxSize, uint8_t *pMaxCis, uint16_t *pCisCtxSize);

/*************************************************************************************************/
/*!
 *  \brief      Get BIS context size.
 *
 *  \param      pMaxBig         Buffer to return the maximum number of BIG.
 *  \param      pBigCtxSize     Buffer to return the size in bytes of the BIG context.
 *  \param      pMaxBis         Buffer to return the maximum number of BIS.
 *  \param      pBisCtxSize     Buffer to return the size in bytes of the BIS context.
 *
 *  Return the connection context sizes.
 */
/*************************************************************************************************/
void LlGetBisContextSize(uint8_t *pMaxBig, uint16_t *pBigCtxSize, uint8_t *pMaxBis, uint16_t *pBisCtxSize);

/*************************************************************************************************/
/*!
 *  \brief      Get the LL handler watermark level.
 *
 *  \return     Watermark level in microseconds.
 */
/*************************************************************************************************/
uint16_t LlStatsGetHandlerWatermarkUsec(void);

/*! \} */    /* LL_API_DIAG */

/*! \addtogroup LL_API_ISO
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Used by a broadcaster host to command is used to create one or more BISes of a BIG
 *              in the controller.
 *
 *  \param      pCreateBig      Create BIG parameters.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlCreateBig(LlCreateBig_t *pCreateBig);

/*************************************************************************************************/
/*!
 *  \brief      Used by a broadcaster host to command is used to create one or more BISes of a BIG
 *              in the ISO test mode.
 *
 *  \param      pCreateBigTest  Create BIG Test parameters.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlCreateBigTest(LlCreateBigTest_t *pCreateBigTest);

/*************************************************************************************************/
/*!
 *  \brief      Used to terminate the transmission of all BISes of a BIG, or to cancel the process
 *              of creating a BIG using the HCI_LE_Create_BIG command from the Isochronous Broadcaster.
 *
 *  \param      bigHandle     Used to identify the BIG.
 *  \param      reason        Termination reason.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlTerminateBig(uint8_t bigHandle, uint8_t reason);

/*************************************************************************************************/
/*!
 *  \brief      Used to synchronize and receive PDUs from one or more BISes within a BIG.
 *
 *  \param      pCreateSync     BIG Create Sync parameters.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlBigCreateSync(LlBigCreateSync_t *pCreateSync);

/*************************************************************************************************/
/*!
 *  \brief      Used to stop synchronization with the BIG or to cancel the process of synchronizing
 *              to BISes invoked by the HCI_LE_BIG_Create_Sync command
 *
 *  \param      bigHandle     Used to identify the BIG.
 */
/*************************************************************************************************/
void LlBigTerminateSync(uint8_t bigHandle);

/*************************************************************************************************/
/*!
 *  \brief      Used by a master host to set the parameters of all connected isochronous streams
 *              associated with a connected isochronous group in the controller.
 *
 *  \param      pCigParam        CIG parameters.
 *  \param      pCisHandles      Return buffer for the connected isochronous handles.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlSetCigParams(LlCisCigParams_t *pCigParam, uint16_t *pCisHandles);

/*************************************************************************************************/
/*!
 *  \brief      Used by a master Host to set the parameters of all connected isochronous streams
 *              associated with a connected isochronous group in the controller for testing purpose.
 *
 *  \param      pSetCigParamTest    CIG test parameters.
 *  \param      pCisHandles         Return buffer for the connected isochronous handles.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlSetCigParamsTest(LlCisCigParamsTest_t *pSetCigParamTest, uint16_t *pCisHandles);

/*************************************************************************************************/
/*!
 *  \brief      Used by the master host to establish one or more connected isochronous streams.
 *
 *  \param      numCis          Number of connected isochronous streams.
 *  \param      pCreateCisParam Parameters for create connected isochronous stream.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlCreateCis(uint8_t numCis, LlCisCreateCisParams_t *pCreateCisParam);

/*************************************************************************************************/
/*!
 *  \brief      Removes all the connected isochronous streams which have previously been set up.
 *
 *  \param      cigId           Identifier of the connected isochronous group.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlRemoveCig(uint8_t cigId);

/*************************************************************************************************/
/*!
 *  \brief      Used by the slave host to inform the controller to accept the request for the
 *              connected isochronous stream.
 *
 *  \param      cisHandle       Handle of the connected isochronous stream.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlAcceptCisReq(uint16_t cisHandle);

/*************************************************************************************************/
/*!
 *  \brief      Used by the slave host to inform the controller to reject the request for the
 *              connected isochronous stream.
 *
 *  \param      cisHandle       Handle of the connected isochronous stream.
 *  \param      reason          Indicate the reason for rejecting the request.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlRejectCisReq(uint16_t cisHandle, uint8_t reason);

/*************************************************************************************************/
/*!
 *  \brief      Read the Time_Stamp and Time_Offset of a transmitted ISO_SDU identified by the
 *              Packet_Sequence_Number on a CIS or BIS identified by the Connection_Handle.
 *
 *  \param      handle      BIS or CIS handle.
 *  \param      pPktSn      Packet sequence number.
 *  \param      pTs         Timestamp.
 *  \param      pTimeOffs   Time offset.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlReadIsoTxSync(uint16_t handle, uint16_t *pPktSn, uint32_t *pTs, uint32_t *pTimeOffs);

/*************************************************************************************************/
/*!
 * \brief       Used to request the Controller to configure the data transport path in a given
 *              direction between the Controller and the Host.
 *
 * \param       pConfigDataPath  Parameters for configure data path.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlConfigureDataPath(LlIsoConfigDataPath_t *pConfigDataPath);

/*************************************************************************************************/
/*!
 *  \brief      Used to identify and enable the isochronous data path between the host and the
 *              controller for each connected isochronous or broadcast isochronous stream.
 *
 *  \param      pSetupDataPath   Parameters for setup ISO data path.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlSetupIsoDataPath(LlIsoSetupDataPath_t *pSetupDataPath);

/*************************************************************************************************/
/*!
 *  \brief      Used to remove the isochronous data path associated with the
 *              connected isochronous or broadcast isochronous stream.
 *
 *  \param      handle      Handle for CIS or BIS.
 *  \param      dpDir       Direction of data path to remove.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlRemoveIsoDataPath(uint16_t handle, uint8_t dpDir);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation for slave connected isochronous stream.
 *
 *  This function initializes the LL subsystem for use as a slave connected isochronous stream.
 */
/*************************************************************************************************/
void LlCisSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation for master connected isochronous stream.
 *
 *  This function initializes the LL subsystem for use as a master connected isochronous stream.
 */
/*************************************************************************************************/
void LlCisMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize memory for CIS.
 *
 *  \param      pFreeMem        Pointer to free memory.
 *  \param      freeMemSize     Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 *
 *  This function allocates memory for CIS.
 *
 *  \note       This routine must be called after LlInitRunTimeCfg() but only once before any
 *              other initialization routines.
 */
/*************************************************************************************************/
uint16_t LlInitCisMem(uint8_t *pFreeMem, uint32_t freeMemSize);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation for slave broadcast isochronous stream.
 *
 *  This function initializes the LL subsystem for use as a slave broadcast isochronous stream.
 */
/*************************************************************************************************/
void LlBisSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for operation for master broadcast isochronous stream.
 *
 *  This function initializes the LL subsystem for use as a master broadcast isochronous stream.
 */
/*************************************************************************************************/
void LlBisMasterInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Initialize memory for BIS.
 *
 *  \param      pFreeMem        Pointer to free memory.
 *  \param      freeMemSize     Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 *
 *  This function allocates memory for BIS.
 *
 *  \note       This routine must be called after LlInitRunTimeCfg() but only once before any
 *              other initialization routines.
 */
/*************************************************************************************************/
uint16_t LlInitBisMem(uint8_t *pFreeMem, uint32_t freeMemSize);

/*************************************************************************************************/
/*!
 *  \brief      Initialize memory for ISO.
 *
 *  \param      pFreeMem        Pointer to free memory.
 *  \param      freeMemSize     Size of pFreeMem.
 *
 *  \return     Amount of free memory consumed.
 *
 *  This function allocates memory for CIS.
 *
 *  \note       This routine must be called after LlInitRunTimeCfg() but only once before any
 *              other initialization routines.
 */
/*************************************************************************************************/
uint16_t LlInitIsoMem(uint8_t *pFreeMem, uint32_t freeMemSize);

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL for use with an audio codec.
 *
 *  Assign codec functions for direct LL audio streaming.
 */
/*************************************************************************************************/
void LlInitCodec(void);

/*************************************************************************************************/
/*!
 *  \brief      Send an ISO data packet.
 *
 *  \param      pData   Data buffer
 *
 *  Send an ISO data packet. pData points to an ISO buffer formatted according to [1]; the host
 *  must set the connection handle, flags, and length fields in the buffer.
 */
/*************************************************************************************************/
void LlSendIsoData(uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief      Receive an ISO data packet
 *
 *  \return     Data buffer.
 *
 *  Receive an ISO data packet. If no ISO buffers are available this function returns NULL.
 *
 *  \note       The host must deallocate the buffer by calling WsfMsgFree() and call LlRecvBufCmpl() to
 *              update LL accounting.
 */
/*************************************************************************************************/
uint8_t *LlRecvIsoData(void);

/*************************************************************************************************/
/*!
 *  \brief      Indicate that received ISO data buffer has been deallocated
 *
 *  \param      numBufs     Number of completed packets.
 *
 *  Indicate that received ISO data buffer has been deallocated.
 */
/*************************************************************************************************/
void LlRecvIsoDataComplete(uint8_t numBufs);

/*! \} */    /* LL_API_ISO */

/*! \addtogroup LL_API_ECU
 *  \{ */

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL subsystem for enhanced connection update.
 *
 *  This function initializes the LL subsystem for use with enhanced connection updates.
 */
/*************************************************************************************************/
void LlEnhConnUpdateInit(void);

/*************************************************************************************************/
/*!
 *  \brief      Set the initial values for subrating requests.
 *
 *  \param      pSubrate        Subrate parameter.
 *
 *  \return     Status error code.
 *
 *  Set the initial values for the acceptable parameters for subrating requests for all future ACL
 *  connections where the Controller is the Central. This command does not affect any existing
 *  connection.
 */
/*************************************************************************************************/
uint8_t LlSetDefaultSubrate(LlSubrateParam_t *pSubrate);

/*************************************************************************************************/
/*!
 *  \brief      Request subrate change.
 *
 *  \param      handle          Connection handle.
 *  \param      pSubrate        Subrate parameter.
 *
 *  Used by a Central or a Peripheral to request a change to the subrating factor and/or other
 *  parameters applied to an existing connection using the Connection Subrate Update procedure.
 */
/*************************************************************************************************/
void LlSubrateReq(uint16_t handle, LlSubrateParam_t *pSubrate);

/*! \} */    /* LL_API_ECU */

#ifdef __cplusplus
};
#endif

#endif /* LL_API_H */
