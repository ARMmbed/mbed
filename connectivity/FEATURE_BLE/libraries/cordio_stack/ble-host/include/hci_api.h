/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI subsystem API.
 *
 *  Copyright (c) 2009-2019 Arm Ltd. All Rights Reserved.
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
#ifndef HCI_API_H
#define HCI_API_H

#include "wsf_types.h"
#include "hci_defs.h"
#include "wsf_os.h"
#include "util/bda.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \addtogroup STACK_HCI_EVT_API
 *  \{ */

/** \name HCI Internal Event Codes
 * Proprietary HCI event codes for handling HCI events in callbacks.
 */
/**@{*/
#define HCI_RESET_SEQ_CMPL_CBACK_EVT                     0   /*!< \brief Reset sequence complete */
#define HCI_LE_CONN_CMPL_CBACK_EVT                       1   /*!< \brief LE connection complete */
#define HCI_LE_ENHANCED_CONN_CMPL_CBACK_EVT              2   /*!< \brief LE enhanced connection complete */
#define HCI_DISCONNECT_CMPL_CBACK_EVT                    3   /*!< \brief Disconnect complete */
#define HCI_LE_CONN_UPDATE_CMPL_CBACK_EVT                4   /*!< \brief LE connection update complete */
#define HCI_LE_CREATE_CONN_CANCEL_CMD_CMPL_CBACK_EVT     5   /*!< \brief LE create connection cancel command complete */
#define HCI_LE_ADV_REPORT_CBACK_EVT                      6   /*!< \brief LE advertising report */
#define HCI_READ_RSSI_CMD_CMPL_CBACK_EVT                 7   /*!< \brief Read RSSI command complete */
#define HCI_LE_READ_CHAN_MAP_CMD_CMPL_CBACK_EVT          8   /*!< \brief LE Read channel map command complete */
#define HCI_READ_TX_PWR_LVL_CMD_CMPL_CBACK_EVT           9   /*!< \brief Read transmit power level command complete */
#define HCI_READ_REMOTE_VER_INFO_CMPL_CBACK_EVT          10  /*!< \brief Read remote version information complete */
#define HCI_LE_READ_REMOTE_FEAT_CMPL_CBACK_EVT           11  /*!< \brief LE read remote features complete */
#define HCI_LE_LTK_REQ_REPL_CMD_CMPL_CBACK_EVT           12  /*!< \brief LE LTK request reply command complete */
#define HCI_LE_LTK_REQ_NEG_REPL_CMD_CMPL_CBACK_EVT       13  /*!< \brief LE LTK request negative reply command complete */
#define HCI_ENC_KEY_REFRESH_CMPL_CBACK_EVT               14  /*!< \brief Encryption key refresh complete */
#define HCI_ENC_CHANGE_CBACK_EVT                         15  /*!< \brief Encryption change */
#define HCI_LE_LTK_REQ_CBACK_EVT                         16  /*!< \brief LE LTK request */
#define HCI_VENDOR_SPEC_CMD_STATUS_CBACK_EVT             17  /*!< \brief Vendor specific command status */
#define HCI_VENDOR_SPEC_CMD_CMPL_CBACK_EVT               18  /*!< \brief Vendor specific command complete */
#define HCI_VENDOR_SPEC_CBACK_EVT                        19  /*!< \brief Vendor specific */
#define HCI_HW_ERROR_CBACK_EVT                           20  /*!< \brief Hardware error */
#define HCI_LE_ADD_DEV_TO_RES_LIST_CMD_CMPL_CBACK_EVT    21  /*!< \brief LE add device to resolving list command complete */
#define HCI_LE_REM_DEV_FROM_RES_LIST_CMD_CMPL_CBACK_EVT  22  /*!< \brief LE remove device from resolving command complete */
#define HCI_LE_CLEAR_RES_LIST_CMD_CMPL_CBACK_EVT         23  /*!< \brief LE clear resolving list command complete */
#define HCI_LE_READ_PEER_RES_ADDR_CMD_CMPL_CBACK_EVT     24  /*!< \brief LE read peer resolving address command complete */
#define HCI_LE_READ_LOCAL_RES_ADDR_CMD_CMPL_CBACK_EVT    25  /*!< \brief LE read local resolving address command complete */
#define HCI_LE_SET_ADDR_RES_ENABLE_CMD_CMPL_CBACK_EVT    26  /*!< \brief LE set address resolving enable command complete */
#define HCI_LE_ENCRYPT_CMD_CMPL_CBACK_EVT                27  /*!< \brief LE encrypt command complete */
#define HCI_LE_RAND_CMD_CMPL_CBACK_EVT                   28  /*!< \brief LE rand command complete */
#define HCI_LE_REM_CONN_PARAM_REP_CMD_CMPL_CBACK_EVT     29  /*!< \brief LE remote connection parameter request reply complete */
#define HCI_LE_REM_CONN_PARAM_NEG_REP_CMD_CMPL_CBACK_EVT 30  /*!< \brief LE remote connection parameter request negative reply complete */
#define HCI_LE_READ_DEF_DATA_LEN_CMD_CMPL_CBACK_EVT      31  /*!< \brief LE read suggested default data length command complete */
#define HCI_LE_WRITE_DEF_DATA_LEN_CMD_CMPL_CBACK_EVT     32  /*!< \brief LE write suggested default data length command complete */
#define HCI_LE_SET_DATA_LEN_CMD_CMPL_CBACK_EVT           33  /*!< \brief LE set data length command complete */
#define HCI_LE_READ_MAX_DATA_LEN_CMD_CMPL_CBACK_EVT      34  /*!< \brief LE read maximum data length command complete */
#define HCI_LE_REM_CONN_PARAM_REQ_CBACK_EVT              35  /*!< \brief LE remote connection parameter request */
#define HCI_LE_DATA_LEN_CHANGE_CBACK_EVT                 36  /*!< \brief LE data length change */
#define HCI_LE_READ_LOCAL_P256_PUB_KEY_CMPL_CBACK_EVT    37  /*!< \brief LE read local P-256 public key */
#define HCI_LE_GENERATE_DHKEY_CMPL_CBACK_EVT             38  /*!< \brief LE generate DHKey complete */
#define HCI_WRITE_AUTH_PAYLOAD_TO_CMD_CMPL_CBACK_EVT     39  /*!< \brief Write authenticated payload timeout command complete */
#define HCI_AUTH_PAYLOAD_TO_EXPIRED_CBACK_EVT            40  /*!< \brief Authenticated payload timeout expired event */
#define HCI_LE_READ_PHY_CMD_CMPL_CBACK_EVT               41  /*!< \brief LE read phy command complete */
#define HCI_LE_SET_DEF_PHY_CMD_CMPL_CBACK_EVT            42  /*!< \brief LE set default phy command complete */
#define HCI_LE_PHY_UPDATE_CMPL_CBACK_EVT                 43  /*!< \brief LE phy update complete */
#define HCI_LE_EXT_ADV_REPORT_CBACK_EVT                  44  /*!< \brief LE extended advertising report */
#define HCI_LE_SCAN_TIMEOUT_CBACK_EVT                    45  /*!< \brief LE scan timeout event */
#define HCI_LE_ADV_SET_TERM_CBACK_EVT                    46  /*!< \brief LE advertising set terminated event */
#define HCI_LE_SCAN_REQ_RCVD_CBACK_EVT                   47  /*!< \brief LE scan request received event */
#define HCI_LE_PER_ADV_SYNC_EST_CBACK_EVT                48  /*!< \brief LE periodic advertising sync established event */
#define HCI_LE_PER_ADV_REPORT_CBACK_EVT                  49  /*!< \brief LE periodic advertising report event */
#define HCI_LE_PER_ADV_SYNC_LOST_CBACK_EVT               50  /*!< \brief LE periodic advertising synch lost event */
#define HCI_LE_CH_SEL_ALGO_CBACK_EVT                     51  /*!< \brief LE channel selection algorithm event */
#define HCI_LE_SCAN_ENABLE_CMD_CMPL_CBACK_EVT            52  /*!< \brief LE scan enable command complete  */
#define HCI_LE_ADV_ENABLE_CMD_CMPL_CBACK_EVT             53  /*!< \brief LE advertise enable command complete  */
#define HCI_LE_EXT_SCAN_ENABLE_CMD_CMPL_CBACK_EVT        54  /*!< \brief LE extended scan enable command complete  */
#define HCI_LE_EXT_ADV_ENABLE_CMD_CMPL_CBACK_EVT         55  /*!< \brief LE extended advertise enable command complete  */
#define HCI_LE_PER_ADV_ENABLE_CMD_CMPL_CBACK_EVT         56  /*!< \brief LE periodic advertise enable command complete  */
#define HCI_LE_SET_RAND_ADDR_CMD_CMPL_CBACK_EVT          57  /*!< \brief LE set random address command complete  */
#define HCI_LE_PER_SYNC_TRSF_RCVD_CBACK_EVT              58  /*!< \brief LE periodic advertising sync transfer received event */
#define HCI_LE_PER_ADV_SYNC_TRSF_CMD_CMPL_CBACK_EVT      59  /*!< \brief LE periodic advertising sync transfer command complete  */
#define HCI_LE_PER_ADV_SET_INFO_TRSF_CMD_CMPL_CBACK_EVT  60  /*!< \brief LE set periodic advertising set info transfer command complete  */
#define HCI_LE_CONN_IQ_REPORT_CBACK_EVT                  61  /*!< \brief LE connection IQ report event */
#define HCI_LE_CTE_REQ_FAILED_CBACK_EVT                  62  /*!< \brief LE CTE request failed event */
#define HCI_LE_SET_CONN_CTE_RX_PARAMS_CMD_CMPL_CBACK_EVT 63  /*!< \brief LE set connection CTE receive parameters command complete */
#define HCI_LE_SET_CONN_CTE_TX_PARAMS_CMD_CMPL_CBACK_EVT 64  /*!< \brief LE set connection CTE transmit parameters command complete */
#define HCI_LE_CONN_CTE_REQ_ENABLE_CMD_CMPL_CBACK_EVT    65  /*!< \brief LE connection CTE request enable command complete */
#define HCI_LE_CONN_CTE_RSP_ENABLE_CMD_CMPL_CBACK_EVT    66  /*!< \brief LE connection CTE response enable  command complete */
#define HCI_LE_READ_ANTENNA_INFO_CMD_CMPL_CBACK_EVT      67  /*!< \brief LE read antenna information command complete */
#define HCI_LE_CIS_EST_CBACK_EVT                         68  /*!< \brief LE CIS established event */
#define HCI_LE_CIS_REQ_CBACK_EVT                         69  /*!< \brief LE CIS request event */
#define HCI_CIS_DISCONNECT_CMPL_CBACK_EVT                70  /*!< \brief CIS disconnect complete */
#define HCI_LE_REQ_PEER_SCA_CBACK_EVT                    71  /*!< \brief LE Request peer SCA complete */
#define HCI_LE_SET_CIG_PARAMS_CMD_CMPL_CBACK_EVT         72  /*!< \brief LE set CIG parameters command complete */
#define HCI_LE_REMOVE_CIG_CMD_CMPL_CBACK_EVT             73  /*!< \brief LE remove CIG command complete */
#define HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL_CBACK_EVT    74  /*!< \brief LE setup ISO data path command complete */
#define HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL_CBACK_EVT   75  /*!< \brief LE remove ISO data path command complete */
#define HCI_CONFIG_DATA_PATH_CMD_CMPL_CBACK_EVT          76  /*!< \brief Configure data path command complete */
#define HCI_READ_LOCAL_SUP_CODECS_CMD_CMPL_CBACK_EVT     77  /*!< \brief Read local supported codecs command complete */
#define HCI_READ_LOCAL_SUP_CODEC_CAP_CMD_CMPL_CBACK_EVT  78  /*!< \brief Read local supported codec capabilities command complete */
#define HCI_READ_LOCAL_SUP_CTR_DLY_CMD_CMPL_CBACK_EVT    79  /*!< \brief Read local supported controller delay command complete */
#define HCI_LE_CREATE_BIG_CMPL_CBACK_EVT                 80  /*!< \brief LE create BIG complete */
#define HCI_LE_TERM_BIG_CMPL_CBACK_EVT                   81  /*!< \brief LE terminate BIG complete */
#define HCI_LE_BIG_SYNC_EST_CBACK_EVT                    82  /*!< \brief LE BIG sync established */
#define HCI_LE_BIG_SYNC_LOST_CBACK_EVT                   83  /*!< \brief LE BIG sync lost */
#define HCI_LE_BIG_TERM_SYNC_CMPL_CBACK_EVT              84  /*!< \brief LE BIG terminate sync complete */
#define HCI_LE_BIG_INFO_ADV_REPORT_CBACK_EVT             85  /*!< \brief LE BIG Info advertising report */

#if MBED_CONF_CORDIO_ROUTE_UNHANDLED_COMMAND_COMPLETE_EVENTS
#define HCI_UNHANDLED_CMD_CMPL_CBACK_EVT                 86  /*!< \brief Unhandled event */
#endif

#define HCI_LE_MAX_CBACK_EVT                             HCI_LE_BIG_INFO_ADV_REPORT_CBACK_EVT
                                                             /*!< \brief Max callback event ID number  */
/**@}*/

/*! \brief HCI calculate SDU header length. */
#define HCI_GET_SDU_HDR_LEN(useTs)               (HCI_ISO_HDR_LEN + (useTs ? HCI_ISO_DL_MAX_LEN : HCI_ISO_DL_MIN_LEN))

/*! \brief HCI Get base of SDU data buffer. */
#define HCI_GET_SDU_PAYLOAD_BUF(pHdrBuf, useTs)  (pHdrBuf + HCI_GET_SDU_HDR_LEN(useTs))

/*! \brief HCI Get base of SDU Header buffer. */
#define HCI_GET_SDU_HDR_BUF(pDataBuf, useTs)     (pDataBuf - HCI_GET_SDU_HDR_LEN(useTs))

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief LE connection complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;           /*!< \brief Event header */
  uint8_t             status;        /*!< \brief Status. */
  uint16_t            handle;        /*!< \brief Connection handle. */
  uint8_t             role;          /*!< \brief Local connection role. */
  uint8_t             addrType;      /*!< \brief Peer address type. */
  bdAddr_t            peerAddr;      /*!< \brief Peer address. */
  uint16_t            connInterval;  /*!< \brief Connection interval */
  uint16_t            connLatency;   /*!< \brief Connection latency. */
  uint16_t            supTimeout;    /*!< \brief Supervision timeout. */
  uint8_t             clockAccuracy; /*!< \brief Clock accuracy. */

  /* \brief enhanced fields */
  bdAddr_t            localRpa;      /*!< \brief Local RPA. */
  bdAddr_t            peerRpa;       /*!< \brief Peer RPA. */
} hciLeConnCmplEvt_t;

/*! \brief Disconnect complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;           /*!< \brief Event header. */
  uint8_t             status;        /*!< \brief Disconnect complete status. */
  uint16_t            handle;        /*!< \brief Connect handle. */
  uint8_t             reason;        /*!< \brief Reason. */
} hciDisconnectCmplEvt_t;

/*! \brief LE connection update complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;          /*!< \brief Event header. */
  uint8_t             status;       /*!< \brief Status. */
  uint16_t            handle;       /*!< \brief Connection handle. */
  uint16_t            connInterval; /*!< \brief Connection interval. */
  uint16_t            connLatency;  /*!< \brief Connection latency. */
  uint16_t            supTimeout;   /*!< \brief Supervision timeout. */
} hciLeConnUpdateCmplEvt_t;

/*! \brief LE create connection cancel command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
} hciLeCreateConnCancelCmdCmplEvt_t;

/*! \brief LE advertising report event */
typedef struct
{
  wsfMsgHdr_t         hdr;            /*!< \brief Event header. */
  uint8_t             *pData;         /*!< \brief advertising or scan response data. */
  uint8_t             len;            /*!< \brief length of advertising or scan response data. */
  int8_t              rssi;           /*!< \brief RSSI. */
  uint8_t             eventType;      /*!< \brief Advertising event type. */
  uint8_t             addrType;       /*!< \brief Address type. */
  bdAddr_t            addr;           /*!< \brief Device address. */

  /* \brief direct fields */
  uint8_t             directAddrType; /*!< \brief Direct advertising address type. */
  bdAddr_t            directAddr;     /*!< \brief Direct advertising address. */
} hciLeAdvReportEvt_t;

/*! \brief LE extended advertising report */
typedef struct
{
  wsfMsgHdr_t         hdr;            /*!< \brief Event header. */
  uint16_t            eventType;      /*!< \brief Event type. */
  uint8_t             addrType;       /*!< \brief Address type. */
  bdAddr_t            addr;           /*!< \brief Address. */
  uint8_t             priPhy;         /*!< \brief Primary PHY. */
  uint8_t             secPhy;         /*!< \brief Secondary PHY. */
  uint8_t             advSid;         /*!< \brief Advertising SID. */
  int8_t              txPower;        /*!< \brief Tx Power. */
  int8_t              rssi;           /*!< \brief RSSI. */
  int16_t             perAdvInter;    /*!< \brief Periodic advertising interval. */
  uint8_t             directAddrType; /*!< \brief Directed address type. */
  bdAddr_t            directAddr;     /*!< \brief Directed address. */
  uint16_t            len;            /*!< \brief Data buffer length. */
  uint8_t             *pData;         /*!< \brief Data buffer. */
} hciLeExtAdvReportEvt_t;

/*! \brief LE scan timeout */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< \brief Event header. */
} hciLeScanTimeoutEvt_t;

/*! \brief LE advertising set terminated */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< \brief Event header. */
  uint8_t       status;         /*!< \brief Status. */
  uint8_t       advHandle;      /*!< \brief Advertising handle. */
  uint16_t      handle;         /*!< \brief Connection handle. */
  uint8_t       numComplEvts;   /*!< \brief Number of completed extended advertising events. */
} hciLeAdvSetTermEvt_t;

/*! \brief LE scan request received */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< \brief Event header. */
  uint8_t       advHandle;      /*!< \brief Advertising handle. */
  uint8_t       scanAddrType;   /*!< \brief Scanner address type. */
  bdAddr_t      scanAddr;       /*!< \brief Scanner address. */
} hciLeScanReqRcvdEvt_t;

/*! \brief LE periodic advertising sync established */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< \brief Event header. */
  uint8_t       status;         /*!< \brief Status. */
  uint16_t      syncHandle;     /*!< \brief Sync handle. */
  uint8_t       advSid;         /*!< \brief Advertising SID. */
  uint8_t       advAddrType;    /*!< \brief Advertiser address type. */
  bdAddr_t      advAddr;        /*!< \brief Advertiser address. */
  uint8_t       advPhy;         /*!< \brief Advertiser PHY. */
  uint16_t      perAdvInterval; /*!< \brief Periodic advertising interval. */
  uint8_t       clockAccuracy;  /*!< \brief Advertiser clock accuracy. */
} hciLePerAdvSyncEstEvt_t;

/*! \brief LE periodic advertising report */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< \brief Event header. */
  uint16_t      syncHandle;     /*!< \brief Sync handle. */
  uint8_t       txPower;        /*!< \brief Tx power. */
  uint8_t       rssi;           /*!< \brief RSSI. */
  uint8_t       unused;         /*!< \brief Intended to be used in a future feature. */
  uint8_t       status;         /*!< \brief Data status. */
  uint16_t      len;            /*!< \brief Data buffer length. */
  uint8_t       *pData;         /*!< \brief Data buffer. */
} hciLePerAdvReportEvt_t;

/*! \brief LE periodic advertising synch lost */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< \brief Event header. */
  uint16_t      syncHandle;     /*!< \brief Sync handle. */
} hciLePerAdvSyncLostEvt_t;

/*! \brief LE periodic advertising sync transfer received */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< \brief Event header. */
  uint8_t       status;         /*!< \brief Status. */
  uint16_t      connHandle;     /*!< \brief Connection handle. */
  uint16_t      serviceData;    /*!< \brief Service data. */
  uint16_t      syncHandle;     /*!< \brief Sync handle. */
  uint8_t       advSid;         /*!< \brief Advertising SID. */
  uint8_t       advAddrType;    /*!< \brief Advertiser address type. */
  bdAddr_t      advAddr;        /*!< \brief Advertiser address. */
  uint8_t       advPhy;         /*!< \brief Advertiser PHY. */
  uint16_t      perAdvInterval; /*!< \brief Periodic advertising interval. */
  uint8_t       clockAccuracy;  /*!< \brief Advertiser clock accuracy. */
} HciLePerAdvSyncTrsfRcvdEvt_t;

/*! \brief LE channel selection algorithm */
typedef struct
{
  wsfMsgHdr_t   hdr;            /*!< \brief Event header. */
  uint16_t      handle;         /*!< \brief Connection handle. */
  uint8_t       chSelAlgo;      /*!< \brief Channel selection algorithm */
} hciLeChSelAlgoEvt_t;

/*! \brief Read RSSI command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
  int8_t              rssi;   /*!< \brief RSSI. */
} hciReadRssiCmdCmplEvt_t;

/*! \brief LE Read channel map command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                       /*!< \brief Event header. */
  uint8_t             status;                    /*!< \brief Status. */
  uint16_t            handle;                    /*!< \brief Connection handle. */
  uint8_t             chanMap[HCI_CHAN_MAP_LEN]; /*!< \brief channel map. */
} hciReadChanMapCmdCmplEvt_t;

/*! \brief Read transmit power level command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint8_t             handle; /*!< \brief Connection handle. */
  int8_t              pwrLvl; /*!< \brief Tx power level. */
} hciReadTxPwrLvlCmdCmplEvt_t;

/*! \brief Read remote version information complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;        /*!< \brief Event header. */
  uint8_t             status;     /*!< \brief Status. */
  uint16_t            handle;     /*!< \brief Connection handle. */
  uint8_t             version;    /*!< \brief Version. */
  uint16_t            mfrName;    /*!< \brief Manufacturer name. */
  uint16_t            subversion; /*!< \brief Sub-version. */
} hciReadRemoteVerInfoCmplEvt_t;

/*! \brief LE read remote features complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                    /*!< \brief Event header. */
  uint8_t             status;                 /*!< \brief Status. */
  uint16_t            handle;                 /*!< \brief Connection handle. */
  uint8_t             features[HCI_FEAT_LEN]; /*!< \brief Remote features buffer. */
} hciLeReadRemoteFeatCmplEvt_t;

/*! \brief LE LTK request reply command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLeLtkReqReplCmdCmplEvt_t;

/*! \brief LE LTK request negative reply command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLeLtkReqNegReplCmdCmplEvt_t;

/*! \brief Encryption key refresh complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciEncKeyRefreshCmpl_t;

/*! \brief Encryption change event */
typedef struct
{
  wsfMsgHdr_t         hdr;     /*!< \brief Event header. */
  uint8_t             status;  /*!< \brief Status. */
  uint16_t            handle;  /*!< \brief Connection handle. */
  uint8_t             enabled; /*!< \brief Encryption enabled flag. */
} hciEncChangeEvt_t;

/*! \brief LE LTK request event */
typedef struct
{
  wsfMsgHdr_t         hdr;                   /*!< \brief Event header. */
  uint16_t            handle;                /*!< \brief Connection handle. */
  uint8_t             randNum[HCI_RAND_LEN]; /*!< \brief LTK random number. */
  uint16_t            encDiversifier;        /*!< \brief LTK encryption diversifier. */
} hciLeLtkReqEvt_t;

/*! \brief Vendor specific command status event */
typedef struct
{
  wsfMsgHdr_t        hdr;    /*!< \brief Event header. */
  uint16_t           opcode; /*!< \brief Opcode. */
} hciVendorSpecCmdStatusEvt_t;

/*! \brief Vendor specific command complete event */
typedef struct
{
  wsfMsgHdr_t        hdr;      /*!< \brief Event header. */
  uint16_t           opcode;   /*!< \brief Opcode. */
  uint8_t            param[1]; /*!< \brief Operation parameter. */
} hciVendorSpecCmdCmplEvt_t;

/*! \brief Vendor specific event */
typedef struct
{
  wsfMsgHdr_t        hdr;      /*!< \brief Event header. */
  uint8_t            param[1]; /*!< \brief Vendor specific event. */
} hciVendorSpecEvt_t;

/*! \brief Hardware error event */
typedef struct
{
  wsfMsgHdr_t        hdr;  /*!< \brief Event header. */
  uint8_t            code; /*!< \brief Error code. */
} hciHwErrorEvt_t;

/*! \brief LE encrypt command complete event */
typedef struct
{
  wsfMsgHdr_t        hdr;                        /*!< \brief Event header. */
  uint8_t            status;                     /*!< \brief Status. */
  uint8_t            data[HCI_ENCRYPT_DATA_LEN]; /*!< \brief Data. */
} hciLeEncryptCmdCmplEvt_t;

/*! \brief LE rand command complete event */
typedef struct
{
  wsfMsgHdr_t        hdr;                   /*!< \brief Event header. */
  uint8_t            status;                /*!< \brief Status. */
  uint8_t            randNum[HCI_RAND_LEN]; /*!< \brief Random number buffer. */
} hciLeRandCmdCmplEvt_t;

/*! \brief LE remote connection parameter request reply command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLeRemConnParamRepEvt_t;

/*! \brief LE remote connection parameter request negative reply command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLeRemConnParamNegRepEvt_t;

/*! \brief LE read suggested default data len command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint8_t             status;               /*!< \brief Status. */
  uint16_t            suggestedMaxTxOctets; /*!< \brief Suggested maximum Tx octets. */
  uint16_t            suggestedMaxTxTime;   /*!< \brief Suggested maximum Tx time. */
} hciLeReadDefDataLenEvt_t;

/*! \brief LE write suggested default data len command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
} hciLeWriteDefDataLenEvt_t;

/*! \brief LE set data len command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLeSetDataLenEvt_t;

/*! \brief LE read maximum data len command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint8_t             status;               /*!< \brief Status. */
  uint16_t            supportedMaxTxOctets; /*!< \brief Supported maximum Tx octets. */
  uint16_t            supportedMaxTxTime;   /*!< \brief Supported maximum Tx time. */
  uint16_t            supportedMaxRxOctets; /*!< \brief Supported maximum Rx octets. */
  uint16_t            supportedMaxRxTime;   /*!< \brief Supported maximum Rx time. */
} hciLeReadMaxDataLenEvt_t;

/*! \brief LE remote connetion parameter request event */
typedef struct
{
  wsfMsgHdr_t         hdr;         /*!< \brief Event header. */
  uint16_t            handle;      /*!< \brief Connection handle. */
  uint16_t            intervalMin; /*!< \brief Interval minimum. */
  uint16_t            intervalMax; /*!< \brief Interval maximum. */
  uint16_t            latency;     /*!< \brief Connection latency. */
  uint16_t            timeout;     /*!< \brief Connection timeout. */
} hciLeRemConnParamReqEvt_t;

/*! \brief LE data length change event */
typedef struct
{
  wsfMsgHdr_t        hdr;         /*!< \brief Event header. */
  uint16_t           handle;      /*!< \brief Connection handle. */
  uint16_t           maxTxOctets; /*!< \brief Maximum Tx octets. */
  uint16_t           maxTxTime;   /*!< \brief Maximum Tx time. */
  uint16_t           maxRxOctets; /*!< \brief Maximum Rx octets. */
  uint16_t           maxRxTime;   /*!< \brief Maximum Rx time. */
} hciLeDataLenChangeEvt_t;

/*! \brief LE local  p256 ecc key command complete event */
typedef struct
{
  wsfMsgHdr_t        hdr;                   /*!< \brief Event header. */
  uint8_t            status;                /*!< \brief Status. */
  uint8_t            key[HCI_P256_KEY_LEN]; /*!< \brief P-256 public keys. */
} hciLeP256CmplEvt_t;

/*! \brief LE generate DH key command complete event */
typedef struct
{
  wsfMsgHdr_t        hdr;                 /*!< \brief Event header. */
  uint8_t            status;              /*!< \brief Status. */
  uint8_t            key[HCI_DH_KEY_LEN]; /*!< \brief Diffie-Hellman (Share Secret) key. */
} hciLeGenDhKeyEvt_t;

/*! \brief LE read peer resolving address command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                   /*!< \brief Event header. */
  uint8_t             status;                /*!< \brief Status. */
  uint8_t             peerRpa[BDA_ADDR_LEN]; /*!< \brief Peer RPA. */
} hciLeReadPeerResAddrCmdCmplEvt_t;

/*! \brief LE read local resolving address command complete event */
typedef struct
{
  wsfMsgHdr_t        hdr;                    /*!< \brief Event header. */
  uint8_t            status;                 /*!< \brief Status. */
  uint8_t            localRpa[BDA_ADDR_LEN]; /*!< \brief Local RPA. */
} hciLeReadLocalResAddrCmdCmplEvt_t;

/*! \brief LE set address resolving enable command complete event */
typedef struct
{
  wsfMsgHdr_t        hdr;    /*!< \brief Event header. */
  uint8_t            status; /*!< \brief Status. */
} hciLeSetAddrResEnableCmdCmplEvt_t;

/*! \brief LE add device to resolving list command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
} hciLeAddDevToResListCmdCmplEvt_t;

/*! \brief LE remove device from resolving list command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
} hciLeRemDevFromResListCmdCmplEvt_t;

/*! \brief LE clear resolving list command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
} hciLeClearResListCmdCmplEvt_t;

/*! \brief Write authenticated payload to command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciWriteAuthPayloadToCmdCmplEvt_t;

/*! \brief Authenticated payload to expire event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciAuthPayloadToExpiredEvt_t;

/*! \brief LE read PHY command complete event */
  typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
  uint8_t             txPhy;  /*!< \brief Tx PHY. */
  uint8_t             rxPhy;  /*!< \brief Rx PHY. */
} hciLeReadPhyCmdCmplEvt_t;

/*! \brief LE set default PHY command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
} hciLeSetDefPhyCmdCmplEvt_t;

/*! \brief LE PHY update complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Handle. */
  uint8_t             txPhy;  /*!< \brief Tx PHY. */
  uint8_t             rxPhy;  /*!< \brief Rx PHY. */
} hciLePhyUpdateEvt_t;

/*! \brief LE periodic advertising sync transfer command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLePerAdvSyncTrsfCmdCmplEvt_t;

/*! \brief LE set periodic advertising set info transfer command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLePerAdvSetInfoTrsfCmdCmplEvt_t;

/*! \brief LE connection IQ report */
typedef struct
{
  wsfMsgHdr_t   hdr;           /*!< \brief Event header. */
  uint16_t      handle;        /*!< \brief Connection handle. */
  uint8_t       rxPhy;         /*!< \brief Rx Phy. */
  uint8_t       dataChIdx;     /*!< \brief Data Channel Index. */
  int16_t       rssi;          /*!< \brief RSSI. */
  uint8_t       rssiAntennaId; /*!< \brief RSSI Antenna ID. */
  uint8_t       cteType;       /*!< \brief CTE Type. */
  uint8_t       slotDurations; /*!< \brief Slot Durations. */
  uint8_t       pktStatus;     /*!< \brief Packet Status. */
  uint16_t      connEvtCnt;    /*!< \brief Connection Event Counter. */
  uint8_t       sampleCnt;     /*!< \brief Sample Count. */
  int8_t        *pISample;     /*!< \brief List of I Samples. */
  int8_t        *pQSample;     /*!< \brief List of Q Samples. */
} hciLeConnIQReportEvt_t;

/*! \brief LE CTE request failed event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLeCteReqFailedEvt_t;

/*! \brief LE set connection CTE receive parameters command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLeSetConnCteRxParamsCmdCmplEvt_t;

/*! \brief LE set connection CTE transmit parameters command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLeSetConnCteTxParamsCmdCmplEvt_t;

/*! \brief LE connection CTE request enable command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLeConnCteReqEnableCmdCmplEvt_t;

/*! \brief LE connection CTE response enable command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;    /*!< \brief Event header. */
  uint8_t             status; /*!< \brief Status. */
  uint16_t            handle; /*!< \brief Connection handle. */
} hciLeConnCteRspEnableCmdCmplEvt_t;

/*! \brief LE read antenna information command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint8_t             status;               /*!< \brief Status. */
  uint8_t             switchSampleRates;    /*!< \brief Supported Switching Sampling Rates. */
  uint8_t             numAntennae;          /*!< \brief Number of Antennae. */
  uint8_t             switchPatternMaxLen;  /*!< \brief Max Length of Switching Pattern. */
  uint8_t             cteMaxLen;            /*!< \brief Max CTE Length. */
} hciLeReadAntennaInfoCmdCmplEvt_t;

/*! \brief LE CIS established event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint8_t             status;               /*!< \brief Status. */
  uint16_t            cisHandle;            /*!< \brief CIS connection handle. */
  uint32_t            cigSyncDelayUsec;     /*!< \brief CIG synchronization delay in usec. */
  uint32_t            cisSyncDelayUsec;     /*!< \brief CIS synchronization delay in usec. */
  uint32_t            transLatMToSUsec;     /*!< \brief The maximum time, in msec, for transmission of SDUs of all CISes from mater to slave. */
  uint32_t            transLatSToMUsec;     /*!< \brief The maximum time, in msec, for transmission of SDUs of all CISes from slave to master. */
  uint8_t             phyMToS;              /*!< \brief Master to slave PHY. */
  uint8_t             phySToM;              /*!< \brief Slave to master PHY. */
  uint8_t             nse;                  /*!< \brief Number of subevents. */
  uint8_t             bnMToS;               /*!< \brief Burst number master to slave. */
  uint8_t             bnSToM;               /*!< \brief Burst number slave to master. */
  uint8_t             ftMToS;               /*!< \brief Flush timeout master to slave. */
  uint8_t             ftSToM;               /*!< \brief Flush timeout slave to master. */
  uint16_t            maxPduMToS;           /*!< \brief Maximum payload size from master to slave. */
  uint16_t            maxPduSToM;           /*!< \brief Maximum payload size from slave to master. */
  uint16_t            isoInterval;          /*!< \brief Time between two consecutive ISO anchor points. */
} HciLeCisEstEvt_t;

/*! \brief LE CIS request event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint16_t            aclHandle;            /*!< \brief ACL connection handle. */
  uint16_t            cisHandle;            /*!< \brief CIS connection handle. */
  uint8_t             cigId;                /*!< \brief CIG identifier. */
  uint8_t             cisId;                /*!< \brief CIS identifier. */
} HciLeCisReqEvt_t;

/*! \brief LE request peer SCA complete */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint8_t             status;               /*!< \brief Status. */
  uint16_t            handle;               /*!< \brief ACL Connection handle. */
  uint8_t             peerSca;              /*!< \brief Peer SCA. */
} HciLeReqPeerScaCmplEvt_t_t;

/*! \brief LE set CIG parameters command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint8_t             status;               /*!< \brief Status. */
  uint8_t             cigId;                /*!< \brief CIG identifier. */
  uint8_t             numCis;               /*!< \brief Total number of CISes added or modified. */
  uint16_t            cisHandle[HCI_MAX_CIS_COUNT];  /*!< \brief Connection handle of the CISes in the CIG. */
} hciLeSetCigParamsCmdCmplEvt_t;

/*! \brief LE remove CIG command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint8_t             status;               /*!< \brief Status. */
  uint8_t             cigId;                /*!< \brief CIG identifier. */
} hciLeRemoveCigCmdCmplEvt_t;

/*! \brief LE Create BIG complete event */
typedef struct
{
  wsfMsgHdr_t   hdr;                        /*!< \brief Event header. */
  uint8_t       status;                     /*!< \brief Status. */
  uint8_t       bigHandle;                  /*!< \brief BIG handle. */
  uint32_t      syncDelayUsec;              /*!< \brief Synchronization delay in microseconds. */
  uint32_t      transLatUsec;               /*!< \brief Transport latency, in microseconds. */
  uint8_t       phy;                        /*!< \brief Transmit PHY. */
  uint8_t       nse;                        /*!< \brief Number of Sub-Events in each BIS event in the BIG. */
  uint8_t       bn;                         /*!< \brief Number of new payloads in each BIS event. */
  uint8_t       pto;                        /*!< \brief Offset used for pre-transmissions. */
  uint8_t       irc;                        /*!< \brief Number of times a payload is transmitted in a BIS event. */
  uint16_t      maxPdu;                     /*!< \brief Maximum size of the payload. */
  uint16_t      isoInterval;                /*!< \brief Time between two consecutive ISO anchor points. */
  uint8_t       numBis;                     /*!< \brief Number of BIS. */
  uint16_t      bisHandle[HCI_MAX_BIS_COUNT]; /*!< \brief Connection handles of the BIS's. */
} HciLeCreateBigCmplEvt_t;

/*! \brief LE Terminate BIG complete event */
typedef struct
{
  wsfMsgHdr_t   hdr;                        /*!< \brief Event header. */
  uint8_t       bigHandle;                  /*!< \brief BIG handle. */
  uint8_t       reason;                     /*!< \brief Terminate reason. */
} HciLeTerminateBigCmplEvt_t;

/*! \brief LE BIG Terminate Sync complete event */
typedef struct
{
  wsfMsgHdr_t   hdr;                        /*!< \brief Event header. */
  uint8_t       status;                     /*!< \brief Status. */
  uint8_t       bigHandle;                  /*!< \brief BIG handle. */
} HciLeBigTermSyncCmplEvt_t;

/*! \brief LE BIG Sync Established event */
typedef struct
{
  wsfMsgHdr_t   hdr;                        /*!< \brief Event header. */
  uint8_t       status;                     /*!< \brief Status. */
  uint8_t       bigHandle;                  /*!< \brief BIG handle. */
  uint32_t      transLatUsec;               /*!< \brief The maximum time, in microseconds, for transmission of SDUs of all BISes. */
  uint8_t       nse;                        /*!< \brief Number of Sub-Events in each BIS event in the BIG. */
  uint8_t       bn;                         /*!< \brief Number of new payloads in each BIS event. */
  uint8_t       pto;                        /*!< \brief Offset used for pre-transmissions. */
  uint8_t       irc;                        /*!< \brief Number of times a payload is transmitted in a BIS event. */
  uint16_t      maxPdu;                     /*!< \brief Maximum size of the payload. */
  uint16_t      isoInterval;                /*!< \brief Time between two consecutive ISO anchor points. */
  uint8_t       numBis;                     /*!< \brief Number of BIS. */
  uint16_t      bisHandle[HCI_MAX_BIS_COUNT]; /*!< \brief Connection handles of the BIS's. */
} HciLeBigSyncEstEvt_t;

/*! \brief LE BIG sync lost event */
typedef struct
{
  wsfMsgHdr_t   hdr;                        /*!< \brief Event header. */
  uint8_t       bigHandle;                  /*!< \brief BIG handle. */
  uint8_t       reason;                     /*!< \brief Sync lost reason. */
} HciLeBigSyncLostEvt_t;

/*! \brief LE BIG Info Advertising Report event */
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
  uint8_t       phy;            /*!< Transmit PHY. */
  uint8_t       framing;        /*!< Framing mode. */
  bool_t        encrypt;        /*!< Encryption enabled. */
} HciLeBigInfoAdvRptEvt_t;

/*! \brief LE setup ISO data path command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint8_t             status;               /*!< \brief Status. */
  uint16_t            handle;               /*!< \brief Connection handle of the CIS or BIS. */
} hciLeSetupIsoDataPathCmdCmplEvt_t;

/*! \brief LE remove ISO data path command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint8_t             status;               /*!< \brief Status. */
  uint16_t            handle;               /*!< \brief Connection handle of the CIS or BIS. */
} hciLeRemoveIsoDataPathCmdCmplEvt_t;

/*! \brief Config data path command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header. */
  uint8_t             status;               /*!< \brief Status. */
} hciConfigDataPathCmdCmplEvt_t;

/*! \brief Standard codec info block */
typedef struct
{
  uint8_t             codecId;              /*!< \brief Codec ID. */
} HciStdCodecInfo_t;

/*! \brief Vendor-specific codec info block */
typedef struct
{
  uint16_t            compId;               /*!< \brief Company ID. */
  uint16_t            codecId;              /*!< \brief Codec ID. */
} HciVsCodecInfo_t;

/*! \brief Read local supported codecs command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                          /*!< \brief Event header. */
  uint8_t             status;                       /*!< \brief Status. */
  uint8_t             numStdCodecs;                 /*!< \brief Total number of standard codecs supported. */
  HciStdCodecInfo_t   stdCodecs[HCI_MAX_CODEC];     /*!< \brief Standard codecs. */
  uint8_t             stdCodecTrans[HCI_MAX_CODEC]; /*!< \brief Standard codec transport. */
  uint8_t             numVsCodecs;                  /*!< \brief Total number of vendor-specific codecs supported. */
  HciVsCodecInfo_t    vsCodecs[HCI_MAX_CODEC];      /*!< \brief Vendor-specfic codecs. */
  uint8_t             vsCodecTrans[HCI_MAX_CODEC];  /*!< \brief Vendor-specfic codec transport. */
} hciReadLocalSupCodecsCmdCmplEvt_t;

/*! \brief Codec capability block */
typedef struct
{
  uint8_t             len;                          /*!< \brief Length of codec capability. */
  uint8_t             data[HCI_CODEC_CAP_DATA_LEN]; /*!< \brief Codec-specific capability data. */
} HciCodecCap_t;

/*! \brief Read local supported codec capabilities command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                          /*!< \brief Event header. */
  uint8_t             status;                       /*!< \brief Status. */
  uint8_t             numCodecCaps;                 /*!< \bried Number of codec capabilities. */
  HciCodecCap_t       codecCap[HCI_MAX_CODEC];      /*!< \brief Codec capabilities. */
} hciReadLocalSupCodecCapCmdCmplEvt_t;

typedef struct
{
  wsfMsgHdr_t         hdr;                          /*!< \brief Event header. */
  uint8_t             status;                       /*!< \brief Status. */
  uint32_t            minDly;                       /*!< \brief Minimum controller delay. */
  uint32_t            maxDly;                       /*!< \brief Maximum controller delay. */
} hciReadLocalSupCtrDlyCmdCmplEvt_t;

#if MBED_CONF_CORDIO_ROUTE_UNHANDLED_COMMAND_COMPLETE_EVENTS
/*! \brief LE read antenna information command complete event */
typedef struct
{
  wsfMsgHdr_t         hdr;                  /*!< \brief Event header containing the opcode in hdr.param. */
  uint8_t             param[1];             /*!< \brief Unhandled event payload. */
} hciUnhandledCmdCmplEvt_t;
#endif // MBED_CONF_CORDIO_ROUTE_UNHANDLED_COMMAND_COMPLETE_EVENTS

/*! \brief Local version information */
typedef struct
{
  uint8_t             hciVersion;           /*!< \brief HCI version. */
  uint16_t            hciRevision;          /*!< \brief HCI revision. */
  uint8_t             lmpVersion;           /*!< \brief LMP version. */
  uint16_t            manufacturerName;     /*!< \brief Manufacturer name. */
  uint16_t            lmpSubversion;        /*!< \brief LMP Sub-version. */
} hciLocalVerInfo_t;

/*! \brief Union of all event types */
typedef union
{
  wsfMsgHdr_t                         hdr;                         /*!< \brief Event header. */
  wsfMsgHdr_t                         resetSeqCmpl;                /*!< \brief Reset sequence complete. */
  hciLeConnCmplEvt_t                  leConnCmpl;                  /*!< \brief LE connection complete. */
  hciDisconnectCmplEvt_t              disconnectCmpl;              /*!< \brief Disconnect complete. */
  hciLeConnUpdateCmplEvt_t            leConnUpdateCmpl;            /*!< \brief LE connection update complete. */
  hciLeCreateConnCancelCmdCmplEvt_t   leCreateConnCancelCmdCmpl;   /*!< \brief LE create connection cancel command complete. */
  hciLeAdvReportEvt_t                 leAdvReport;                 /*!< \brief LE advertising report. */
  hciReadRssiCmdCmplEvt_t             readRssiCmdCmpl;             /*!< \brief Read RSSI command complete. */
  hciReadChanMapCmdCmplEvt_t          readChanMapCmdCmpl;          /*!< \brief Read channel map command complete. */
  hciReadTxPwrLvlCmdCmplEvt_t         readTxPwrLvlCmdCmpl;         /*!< \brief Read Tx power level command complete. */
  hciReadRemoteVerInfoCmplEvt_t       readRemoteVerInfoCmpl;       /*!< \brief Read remote version information complete. */
  hciLeReadRemoteFeatCmplEvt_t        leReadRemoteFeatCmpl;        /*!< \brief LE read remote feature complete. */
  hciLeLtkReqReplCmdCmplEvt_t         leLtkReqReplCmdCmpl;         /*!< \brief LE LTK request reply command complete. */
  hciLeLtkReqNegReplCmdCmplEvt_t      leLtkReqNegReplCmdCmpl;      /*!< \brief LE LT request negative reply command complete. */
  hciEncKeyRefreshCmpl_t              encKeyRefreshCmpl;           /*!< \brief Encryption key refresh complete. */
  hciEncChangeEvt_t                   encChange;                   /*!< \brief Encryption change. */
  hciLeLtkReqEvt_t                    leLtkReq;                    /*!< \brief LE LTK request. */
  hciVendorSpecCmdStatusEvt_t         vendorSpecCmdStatus;         /*!< \brief Vendor specific command status. */
  hciVendorSpecCmdCmplEvt_t           vendorSpecCmdCmpl;           /*!< \brief Vendor specific command complete. */
  hciVendorSpecEvt_t                  vendorSpec;                  /*!< \brief Vendor specific. */
  hciHwErrorEvt_t                     hwError;                     /*!< \brief Hardware error. */
  hciLeEncryptCmdCmplEvt_t            leEncryptCmdCmpl;            /*!< \brief LE encrypt command complete. */
  hciLeRandCmdCmplEvt_t               leRandCmdCmpl;               /*!< \brief LE random command complete. */
  hciLeReadPeerResAddrCmdCmplEvt_t    leReadPeerResAddrCmdCmpl;    /*!< \brief LE read peer resolvable address command complete. */
  hciLeReadLocalResAddrCmdCmplEvt_t   leReadLocalResAddrCmdCmpl;   /*!< \brief LE read local resolvable address command complete. */
  hciLeSetAddrResEnableCmdCmplEvt_t   leSetAddrResEnableCmdCmpl;   /*!< \brief LE set address resolution enable command complete. */
  hciLeAddDevToResListCmdCmplEvt_t    leAddDevToResListCmdCmpl;    /*!< \brief LE add device to resolving list command complete. */
  hciLeRemDevFromResListCmdCmplEvt_t  leRemDevFromResListCmdCmpl;  /*!< \brief LE remove device from resolving list command complete. */
  hciLeClearResListCmdCmplEvt_t       leClearResListCmdCmpl;       /*!< \brief LE clear resolving list command complete. */
  hciLeRemConnParamRepEvt_t           leRemConnParamRepCmdCmpl;    /*!< \brief LE Remo Connection Parameter Reply Command Complete. */
  hciLeRemConnParamNegRepEvt_t        leRemConnParamNegRepCmdCmpl; /*!< \brief LE Remote Connection Parameter Negative Reply Command Complete. */
  hciLeReadDefDataLenEvt_t            leReadDefDataLenCmdCmpl;     /*!< \brief LE read default data length command complete. */
  hciLeWriteDefDataLenEvt_t           leWriteDefDataLenCmdCmpl;    /*!< \brief LE write default data length command complete. */
  hciLeSetDataLenEvt_t                leSetDataLenCmdCmpl;         /*!< \brief LE set data length command complete. */
  hciLeReadMaxDataLenEvt_t            leReadMaxDataLenCmdCmpl;     /*!< \brief LE read max data length command complete. */
  hciLeRemConnParamReqEvt_t           leRemConnParamReq;           /*!< \brief LE remote connection parameter request. */
  hciLeDataLenChangeEvt_t             leDataLenChange;             /*!< \brief LE data length change. */
  hciLeP256CmplEvt_t                  leP256;                      /*!< \brief LE P-256 */
  hciLeGenDhKeyEvt_t                  leGenDHKey;                  /*!< \brief LE generate Diffie-Hellman key. */
  hciWriteAuthPayloadToCmdCmplEvt_t   writeAuthPayloadToCmdCmpl;   /*!< \brief Write authenticated payload to command complete. */
  hciAuthPayloadToExpiredEvt_t        authPayloadToExpired;        /*!< \brief Authenticated payload to expired. */
  hciLeReadPhyCmdCmplEvt_t            leReadPhyCmdCmpl;            /*!< \brief LE read PHY command complete. */
  hciLeSetDefPhyCmdCmplEvt_t          leSetDefPhyCmdCmpl;          /*!< \brief LE set default PHY command complete. */
  hciLePhyUpdateEvt_t                 lePhyUpdate;                 /*!< \brief LE PHY update. */
  hciLeExtAdvReportEvt_t              leExtAdvReport;              /*!< \brief LE extended advertising report. */
  hciLeScanTimeoutEvt_t               leScanTimeout;               /*!< \brief LE scan timeout. */
  hciLeAdvSetTermEvt_t                leAdvSetTerm;                /*!< \brief LE advertising set terminated. */
  hciLeScanReqRcvdEvt_t               leScanReqRcvd;               /*!< \brief LE scan request received. */
  hciLePerAdvSyncEstEvt_t             lePerAdvSyncEst;             /*!< \brief LE periodic advertising synchronization established. */
  hciLePerAdvReportEvt_t              lePerAdvReport;              /*!< \brief LE periodic advertising report. */
  hciLePerAdvSyncLostEvt_t            lePerAdvSyncLost;            /*!< \brief LE periodic advertising synchronization lost. */
  hciLeChSelAlgoEvt_t                 leChSelAlgo;                 /*!< \brief LE channel select algorithm. */
  HciLePerAdvSyncTrsfRcvdEvt_t        lePerAdvSyncTrsfRcvd;        /*!< \brief LE periodic advertising sync transfer received. */
  hciLePerAdvSyncTrsfCmdCmplEvt_t     lePerAdvSyncTrsfCmdCmpl;     /*!< \brief LE periodic advertising sync transfer command complete. */
  hciLePerAdvSetInfoTrsfCmdCmplEvt_t  lePerAdvSetInfoTrsfCmdCmpl;  /*!< \brief LE set periodic advertising set info transfer command complete. */
  hciLeConnIQReportEvt_t              leConnIQReport;              /*!< \brief LE connection IQ report. */
  hciLeCteReqFailedEvt_t              leCteReqFailed;              /*!< \brief LE CTE request failed. */
  hciLeSetConnCteRxParamsCmdCmplEvt_t leSetConnCteRxParamsCmdCmpl; /*!< \brief LE set connection CTE receive parameters command complete. */
  hciLeSetConnCteTxParamsCmdCmplEvt_t leSetConnCteTxParamsCmdCmpl; /*!< \brief LE set connection CTE transmit parameters command complete. */
  hciLeConnCteReqEnableCmdCmplEvt_t   leConnCteReqEnableCmdCmpl;   /*!< \brief LE connection CTE request enable command complete. */
  hciLeConnCteRspEnableCmdCmplEvt_t   leConnCteRspEnableCmdCmpl;   /*!< \brief LE connection CTE response enable command complete. */
  hciLeReadAntennaInfoCmdCmplEvt_t    leReadAntennaInfoCmdCmpl;    /*!< \brief LE read antenna information command complete. */
  hciLeSetCigParamsCmdCmplEvt_t       leSetCigParamsCmdCmpl;       /*!< \brief LE set CIG parameters command complete. */
  hciLeRemoveCigCmdCmplEvt_t          leRemoveCigCmdCmpl;          /*!< \brief LE remove CIG command complete. */
  HciLeCisEstEvt_t                    leCisEst;                    /*!< \brief LE CIS established. */
  HciLeCisReqEvt_t                    leCisReq;                    /*!< \brief LE CIS request. */
  HciLeReqPeerScaCmplEvt_t_t          leReqPeerSca;                /*!< \brief LE request peer SCA complete. */
  hciLeSetupIsoDataPathCmdCmplEvt_t   leSetupIsoDataPathCmdCmpl;   /*!< \brief LE setup ISO data path command complete. */
  hciLeRemoveIsoDataPathCmdCmplEvt_t  leRemoveIsoDataPathCmdCmpl;  /*!< \brief LE remove ISO data path command complete. */
  hciConfigDataPathCmdCmplEvt_t       configDataPathCmdCmpl;       /*!< \brief Config data path command complete. */
  hciReadLocalSupCodecsCmdCmplEvt_t   readLocalSupCodecsCmdCmpl;   /*!< \brief Read local supported codecs command complete. */
  hciReadLocalSupCodecCapCmdCmplEvt_t readLocalSupCodecCapCmdCmpl; /*!< \brief Read local supported codec capablitlies command complete. */
  hciReadLocalSupCtrDlyCmdCmplEvt_t   readLocalSupCtrDlyCmdCmpl;   /*!< \brief Read local supported controller delay command complete. */
  HciLeCreateBigCmplEvt_t             leCreateBigCmpl;             /*!< \brief LE create BIG complete. */
  HciLeTerminateBigCmplEvt_t          leTerminateBigCmpl;          /*!< \brief LE terminate BIG complete. */
  HciLeBigSyncEstEvt_t                leBigSyncEst;                /*!< \brief LE BIG sync established. */
  HciLeBigSyncLostEvt_t               leBigSyncLost;               /*!< \brief LE BIG sync lost. */
  HciLeBigTermSyncCmplEvt_t           leBigTermSyncCmpl;           /*!< \brief LE BIG terminate sync complete. */
  HciLeBigInfoAdvRptEvt_t             leBigInfoAdvRpt;             /*!< \brief LE BIG info advertising report. */
#if MBED_CONF_CORDIO_ROUTE_UNHANDLED_COMMAND_COMPLETE_EVENTS
  hciUnhandledCmdCmplEvt_t            unhandledCmdCmpl;            /*!< \brief Unhandled events. */
#endif  
} hciEvt_t;

/*! \} */    /* STACK_HCI_EVT_API */

/*! \addtogroup STACK_HCI_CMD_API
 * \{ */

/*! \brief Connection specification type */
typedef struct
{
  uint16_t            connIntervalMin; /*!< \brief Minimum connection interval. */
  uint16_t            connIntervalMax; /*!< \brief Maximum connection interval. */
  uint16_t            connLatency;     /*!< \brief Connection latency. */
  uint16_t            supTimeout;      /*!< \brief Supervision timeout. */
  uint16_t            minCeLen;        /*!< \brief Minimum CE length. */
  uint16_t            maxCeLen;        /*!< \brief Maximum CE length. */
} hciConnSpec_t;

/*! \brief Initiating parameters */
typedef struct
{
  uint8_t             filterPolicy;    /*!< \brief Scan filter policy. */
  uint8_t             ownAddrType;     /*!< \brief Address type used by this device. */
  uint8_t             peerAddrType;    /*!< \brief Address type used for peer device. */
  const uint8_t       *pPeerAddr;      /*!< \brief Address of peer device. */
  uint8_t             initPhys;        /*!< \brief Initiating PHYs. */
} hciExtInitParam_t;

/*! \brief Initiating scan parameters */
typedef struct
{
  uint16_t            scanInterval;    /*!< \brief Scan interval. */
  uint16_t            scanWindow;      /*!< \brief Scan window. */
} hciExtInitScanParam_t;

/*! \brief Extended advertising parameters */
typedef struct
{
  uint16_t            advEventProp;    /*!< \brief Advertising Event Properties. */
  uint32_t            priAdvInterMin;  /*!< \brief Primary Advertising Interval Minimum. */
  uint32_t            priAdvInterMax;  /*!< \brief Primary Advertising Interval Maximum. */
  uint8_t             priAdvChanMap;   /*!< \brief Primary Advertising Channel Map. */
  uint8_t             ownAddrType;     /*!< \brief Own Address Type. */
  uint8_t             peerAddrType;    /*!< \brief Peer Address Type. */
  uint8_t             *pPeerAddr;      /*!< \brief Peer Address. */
  uint8_t             advFiltPolicy;   /*!< \brief Advertising Filter Policy. */
  int8_t              advTxPwr;        /*!< \brief Advertising Tx Power. */
  uint8_t             priAdvPhy;       /*!< \brief Primary Advertising PHY. */
  uint8_t             secAdvMaxSkip;   /*!< \brief Secondary Advertising Maximum Skip. */
  uint8_t             secAdvPhy;       /*!< \brief Secondary Advertising PHY. */
  uint8_t             advSID;          /*!< \brief Advertising SID. */
  uint8_t             scanReqNotifEna; /*!< \brief Scan Request Notification Enable. */
} hciExtAdvParam_t;

/*! \brief Extended advertising enable parameters */
typedef struct
{
  uint8_t             advHandle;       /*!< \brief Advertising handle. */
  uint16_t            duration;        /*!< \brief Advertising duration in 10 ms units. */
  uint8_t             maxEaEvents;     /*!< \brief Maximum number of extended advertising events. */
} hciExtAdvEnableParam_t;

/*! \brief Extended scanning parameters */
typedef struct
{
  uint16_t            scanInterval;    /*!< \brief Scan interval. */
  uint16_t            scanWindow;      /*!< \brief Scan window. */
  uint8_t             scanType;        /*!< \brief Scan type. */
} hciExtScanParam_t;

/*! \brief CIS parameters */
typedef struct
{
  uint8_t             cisId;           /*!< \brief Used to identify a connected isochronous stream. */
  uint16_t            sduSizeMToS;     /*!< \brief Maximum size of a data SDU from the master to the slave. */
  uint16_t            sduSizeSToM;     /*!< \brief Maximum size of a data SDU from the slave to the master. */
  uint8_t             phyMToS;         /*!< \brief PHY to be used for transmission from master to slave. */
  uint8_t             phySToM;         /*!< \brief PHY to be used for transmission from master to slave. */
  uint8_t             rteMToS;         /*!< \brief Maximum number of times every PDU should be retransmitted from the master to slave. */
  uint8_t             rteSToM;         /*!< \brief Maximum number of times every PDU should be retransmitted from the slave to master. */
} HciCisCisParams_t;

/*! \brief CIG parameters */
typedef struct
{
  uint8_t             cigId;           /*!< \brief Used to identify the connected isochronous group. */
  uint32_t            sduIntervalMToS; /*!< \brief The time interval between the start of consecutive SDUs from the master Host. */
  uint32_t            sduIntervalSToM; /*!< \brief The time interval between the start of consecutive SDUs from the slave Host. */
  uint8_t             sca;             /*!< \brief Sleep clock accuracy. */
  uint8_t             packing;         /*!< \brief Packing scheme. */
  uint8_t             framing;         /*!< \brief Indicates the format of CIS Data PDUs.  */
  uint16_t            transLatMToS;    /*!< \brief Maximum time, in milliseconds, for an SDU to be transported from the master Controller to slave Controller. */
  uint16_t            transLatSToM;    /*!< \brief Maximum time, in milliseconds, for an SDU to be transported from the slave Controller to master Controller. */
  uint8_t             numCis;          /*!< \brief Number of CIS to set. */
  HciCisCisParams_t   *pCisParam;      /*!< \brief CIS parameters. */
} HciCisCigParams_t;

/*! \brief CIS create CIS parameters */
typedef struct
{
  uint16_t            *pCisHandle;     /*!< \brief Pointer to the connected isochronous handle array. */
  uint16_t            *pAclHandle;     /*!< \brief Pointer to the asynchronous connection link handle array. */
} HciCisCreateCisParams_t;

/*! \brief BIG Create BIG parameters */
typedef struct
{
  uint8_t       bigHandle;             /*!< \brief Used to identify the BIG. */
  uint8_t       advHandle;             /*!< \brief Used to identify the periodic advertising train. */
  uint8_t       numBis;                /*!< \brief Total number of BISes in the BIG. */
  uint32_t      sduInterUsec;          /*!< \brief Interval, in microseconds, of BIG SDUs. */
  uint16_t      maxSdu;                /*!< \brief Maximum size of an SDU. */
  uint16_t      mtlMs;                 /*!< \brief Maximum time in milliseconds. */
  uint8_t       rtn;                   /*!< \brief Retransmission number. */
  uint8_t       phys;                  /*!< \brief Transmitter PHYs of packets. */
  uint8_t       packing;               /*!< \brief Sequential or Interleaved packing. */
  uint8_t       framing;               /*!< \brief Unframed or Framed. */
  uint8_t       encrypt;               /*!< \brief Unencrypted or Encrypted. */
  uint8_t       bcstCode[HCI_BC_LEN];  /*!< \brief Session key used to encrypt and decrypt BIS payloads. */
} HciCreateBig_t;

/*! \brief BIG Create Sync parameters */
typedef struct
{
  uint8_t       bigHandle;             /*!< \brief Used to identify the BIG. */
  uint16_t      syncHandle;            /*!< \brief Periodic advertising train handle. */
  uint8_t       encrypt;               /*!< \brief Unencrypted or Encrypted. */
  uint8_t       bcstCode[HCI_BC_LEN];  /*!< \brief Session key code for encrypt and decrypt BIS payloads. */
  uint8_t       mse;                   /*!< \brief Maximum number of subevents. */
  uint16_t      bigSyncTimeout;        /*!< \brief Synchronization timeout for the BIS, in the units of 10ms. */
  uint8_t       numBis;                /*!< \brief Total number of BISes in the BIG. */
  uint8_t       bis[HCI_MAX_BIS_COUNT];/*!< \brief List of indices of BISes. */
} HciBigCreateSync_t;

/*! \brief Setup ISO data path parameters */
typedef struct
{
  uint16_t            handle;          /*!< \brief Handle of CIS or BIS. */
  uint8_t             dpDir;           /*!< \brief Data path direction. */
  uint8_t             dpId;            /*!< \brief Data path ID. */
  uint8_t             codingFmt;       /*!< \brief Coding Format. */
  uint16_t            compId;          /*!< \brief Company ID (ignored if 'codingFmt' not 0xFF). */
  uint16_t            vsCodecId;       /*!< \brief Vendor-defined codec ID (ignored if 'codingFmt' not 0xFF). */
  uint32_t            ctrDly;          /*!< \brief Controller delay (in usec). */
  uint8_t             codecConfigLen;  /*!< \brief Codec configuration length. */
  uint8_t             *pCodecConfig;   /*!< \brief Codec configuration. */
} HciIsoSetupDataPath_t;

/*! \brief Configure data path parameters */
typedef struct
{
  uint8_t             dpDir;           /*!< \brief Data path direction. */
  uint8_t             dpId;            /*!< \brief Data path ID. */
  uint8_t             configLen;       /*!< \brief Length of vendor-specific configuration data. */
  uint8_t             *pConfig;        /*!< \brief Vendor-specific configuration data. */
} HciConfigDataPath_t;

/*! \brief Read local supported codec capabilities parameters */
typedef struct
{
  uint8_t             codingFmt;       /*!< \brief Coding Format. */
  uint16_t            compId;          /*!< \brief Company ID (ignored if 'codingFmt' not 0xFF). */
  uint16_t            vsCodecId;       /*!< \brief Vendor-defined codec ID (ignored if 'codingFmt' not 0xFF). */
  uint8_t             transType;       /*!< \brief Logical transport type. */
  uint8_t             direction;       /*!< \brief Direction. */
} HciReadLocalSupCodecCaps_t;

/*! \brief Read local supported controller delay parameters */
typedef struct
{
  uint8_t             codingFmt;       /*!< \brief Coding Format. */
  uint16_t            compId;          /*!< \brief Company ID (ignored if 'codingFmt' not 0xFF). */
  uint16_t            vsCodecId;       /*!< \brief Vendor-defined codec ID (ignored if 'codingFmt' not 0xFF). */
  uint8_t             transType;       /*!< \brief Logical transport type. */
  uint8_t             direction;       /*!< \brief Direction. */
  uint8_t             codecConfigLen;  /*!< \brief Length of codec configuration. */
  uint8_t             *pCodecConfig;   /*!< \brief Codec-specific configuration data. */
} HciReadLocalSupControllerDly_t;

/*! \} */    /* STACK_HCI_CMD_API */

/**************************************************************************************************
  Callback Function Types
**************************************************************************************************/

/*! \addtogroup STACK_HCI_EVT_API
 * \{ */

/*! \brief HCI event callback type.
 *
 *  This callback function sends events from HCI to the stack.
 *
 *  \param  pEvent    Pointer to HCI callback event structure.
 *
 *  \return None.
 */
typedef void (*hciEvtCback_t)(hciEvt_t *pEvent);

/*! \brief HCI security callback type
 *
 *  This callback function sends certain security events from HCI to the stack.
 *  The security events passed in this callback are the LE Rand Command Complete event and the
 *  LE Encrypt Command Complete event.
 *
 *  \param  pEvt    Pointer to HCK callback event structure.
 *
 *  \return None.
 */
typedef void (*hciSecCback_t)(hciEvt_t *pEvent);

/*! \} */    /* STACK_HCI_EVT_API */

/*! \addtogroup STACK_HCI_ACL_API
 * \{ */

/*! \brief HCI ACL callback type
 *
 *  This callback function sends ACL data from HCI to the stack.
 *
 *  \param  pData    WSF buffer containing an ACL packet.
 *
 *  \return None.
 */
typedef void (*hciAclCback_t)(uint8_t *pData);

/*! \brief HCI ISO callback type
 *
 *  This callback function sends ISO data from HCI to the stack.
 *
 *  \param  pData    WSF buffer containing an ISO packet.
 *
 *  \return None.
*/
typedef void (*hciIsoCback_t)(uint8_t *pData);

/*! \brief HCI flow control callback type
 *
 *  This callback function manages flow control in the TX path betrween the stack and HCI.
 *
 *  \param  connId          Connection handle.
 *  \param  flowDisabled    TRUE if flow is disabled.
 *
 *  \return None.
 */
typedef void (*hciFlowCback_t)(uint16_t handle, bool_t flowDisabled);

/*! \} */    /* STACK_HCI_ACL_API */

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*! \addtogroup STACK_HCI_INIT_API
 *  \{ */

/** \name HCI Initialization, Registration, Reset
 *
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Register a callback for HCI events.
 *
 *  \param  evtCback  Callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciEvtRegister(hciEvtCback_t evtCback);

/*************************************************************************************************/
/*!
 *  \brief  Register a callback for certain HCI security events.
 *
 *  \param  secCback  Callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSecRegister(hciSecCback_t secCback);

/*************************************************************************************************/
/*!
 *  \brief  Register callbacks for the HCI data path.
 *
 *  \param  aclCback  ACL data callback function.
 *  \param  flowCback Flow control callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciAclRegister(hciAclCback_t aclCback, hciFlowCback_t flowCback);

/*************************************************************************************************/
/*!
 *  \brief  Register callbacks for the HCI ISO data path.
 *
 *  \param  isoCback  ISO data callback function.
 *  \param  flowCback Flow control callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciIsoRegister(hciIsoCback_t isoCback, hciFlowCback_t flowCback);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an HCI reset sequence.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciResetSequence(void);

/*************************************************************************************************/
/*!
 *  \brief  Vendor-specific controller initialization function.
 *
 *  \param  param    Vendor-specific parameter.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciVsInit(uint8_t param);

/*************************************************************************************************/
/*!
 *  \brief  HCI core initialization.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciCoreInit(void);

/*************************************************************************************************/
/*!
 *  \brief  WSF event handler for core HCI.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciCoreHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg);

/*************************************************************************************************/
/*!
 *  \brief  Set the maximum reassembled RX ACL packet length.  Minimum value is 27.
 *
 *  \param  len     ACL packet length.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSetMaxRxAclLen(uint16_t len);

/*************************************************************************************************/
/*!
 *  \brief  Set TX ACL queue high and low watermarks.
 *
 *  \param  queueHi   Disable flow on a connection when this many ACL buffers are queued.
 *  \param  queueLo   Disable flow on a connection when this many ACL buffers are queued.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSetAclQueueWatermarks(uint8_t queueHi, uint8_t queueLo);

/*************************************************************************************************/
/*!
*  \brief   Set LE supported features configuration mask.
*
*  \param   feat    Feature bit to set or clear
*  \param   flag    TRUE to set feature bit and FALSE to clear it
*
*  \return None.
*/
/*************************************************************************************************/
void HciSetLeSupFeat(uint64_t feat, bool_t flag);

/*************************************************************************************************/
/*!
 *  \brief   Set LE supported features configuration mask.
 *
 *  \param   feat    Feature bit to set or clear
 *  \param   flag    TRUE to set feature bit and FALSE to clear it
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSetLeSupFeat32(uint32_t feat, bool_t flag);
/**@}*/

/*************************************************************************************************/
/*!
 *  \brief  Vendor-specific controller AE initialization function.
 *
 *  \param  param    Vendor-specific parameter.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciVsAeInit(uint8_t param);

/*! \} */    /* STACK_HCI_INIT_API */

/*! \addtogroup STACK_HCI_OPT_API
 * \{ */

/** \name HCI Optimization Interface Functions
 * This is an optimized interface for certain HCI commands that simply read a
 * value.  The stack uses these functions rather than their corresponding
 * functions in the command interface.
 * These functions can only be called after the reset sequence has been completed.
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Return a pointer to the BD address of this device.
 *
 *  \return Pointer to the BD address.
 */
/*************************************************************************************************/
uint8_t *HciGetBdAddr(void);

/*************************************************************************************************/
/*!
 *  \brief  Return the white list size.
 *
 *  \return White list size.
 */
/*************************************************************************************************/
uint8_t HciGetWhiteListSize(void);

/*************************************************************************************************/
/*!
 *  \brief  Return the advertising transmit power.
 *
 *  \return Advertising transmit power.
 */
/*************************************************************************************************/
int8_t HciGetAdvTxPwr(void);

/*************************************************************************************************/
/*!
 *  \brief  Return the ACL buffer size supported by the controller.
 *
 *  \return ACL buffer size.
 */
/*************************************************************************************************/
uint16_t HciGetBufSize(void);

/*************************************************************************************************/
/*!
 *  \brief  Return the number of ACL buffers supported by the controller.
 *
 *  \return Number of ACL buffers.
 */
/*************************************************************************************************/
uint8_t HciGetNumBufs(void);

/*************************************************************************************************/
/*!
 *  \brief  Return the states supported by the controller.
 *
 *  \return Pointer to the supported states array.
 */
/*************************************************************************************************/
uint8_t *HciGetSupStates(void);

/*************************************************************************************************/
/*!
 *  \brief  Return the LE supported features supported by the controller.
 *
 *  \return Supported features.
 */
/*************************************************************************************************/
uint64_t HciGetLeSupFeat(void);

/*************************************************************************************************/
/*!
 *  \brief  Return the LE supported features supported by the controller.
 *
 *  \return Supported features.
 */
/*************************************************************************************************/
uint32_t HciGetLeSupFeat32(void);

/*************************************************************************************************/
/*!
 *  \brief  Get the maximum reassembled RX ACL packet length.
 *
 *  \return ACL packet length.
 */
/*************************************************************************************************/
uint16_t HciGetMaxRxAclLen(void);

/*************************************************************************************************/
/*!
 *  \brief  Return the resolving list size.
 *
 *  \return resolving list size.
 */
/*************************************************************************************************/
uint8_t HciGetResolvingListSize(void);

/*************************************************************************************************/
/*!
*  \brief  Whether LL Privacy is supported.
*
*  \return TRUE if LL Privacy is supported. FALSE, otherwise.
*/
/*************************************************************************************************/
bool_t HciLlPrivacySupported(void);

/*************************************************************************************************/
/*!
*  \brief  Get the maximum advertisement (or scan response) data length supported by the Controller.
*
*  \return Maximum advertisement data length.
*/
/*************************************************************************************************/
uint16_t HciGetMaxAdvDataLen(void);

/*************************************************************************************************/
/*!
*  \brief  Get the maximum number of advertising sets supported by the Controller.
*
*  \return Maximum number of advertising sets.
*/
/*************************************************************************************************/
uint8_t HciGetNumSupAdvSets(void);

/*************************************************************************************************/
/*!
*  \brief  Whether LE Advertising Extensions is supported.
*
*  \return TRUE if LE Advertising Extensions is supported. FALSE, otherwise.
*/
/*************************************************************************************************/
bool_t HciLeAdvExtSupported(void);

/*************************************************************************************************/
/*!
 *  \brief  Return the periodic advertising list size.
 *
 *  \return periodic advertising list size.
 */
/*************************************************************************************************/
uint8_t HciGetPerAdvListSize(void);

/*************************************************************************************************/
/*!
 *  \brief  Return a pointer to the local version information.
 *
 *  \return Pointer to the local version information.
 */
/*************************************************************************************************/
hciLocalVerInfo_t *HciGetLocalVerInfo(void);
/**@}*/

/*! \} */    /* STACK_HCI_OPT_API */

/*! \addtogroup STACK_HCI_ACL_API
 * \{ */

/** \name HCI ACL Data Functions
 * HCI ACL data interface
 */
/**@{*/

/*************************************************************************************************/
/*!
 *  \brief  Send ACL Data from the stack to HCI.
 *
 *  \param  pAclData    WSF buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSendAclData(uint8_t *pAclData);
/**@}*/

/*! \} */    /* STACK_HCI_ACL_API */

/*! \addtogroup STACK_HCI_CMD_API
 * \{ */

/** \name HCI Command Interface Functions
 * HCI commands
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  HCI disconnect command.
 *
 *  \param  handle    Connection handle.
 *  \param  reason    Reason for disconnect.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciDisconnectCmd(uint16_t handle, uint8_t reason);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE add device white list command.
 *
 *  \param  addrType    Address type.
 *  \param  pAddr       Peer address.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeAddDevWhiteListCmd(uint8_t addrType, uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE clear white list command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeClearWhiteListCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI connection update command.
 *
 *  \param  handle       Connection handle.
 *  \param  pConnSpec    Update connection parameters.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeConnUpdateCmd(uint16_t handle, hciConnSpec_t *pConnSpec);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE create connection command.
 *
 *  \param  scanInterval    Scan interval.
 *  \param  scanWindow      Scan window.
 *  \param  filterPolicy    Filter policy.
 *  \param  peerAddrType    Peer address type.
 *  \param  pPeerAddr       Peer address.
 *  \param  ownAddrType     Own address type.
 *  \param  pConnSpec       Connecdtion parameters.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeCreateConnCmd(uint16_t scanInterval, uint16_t scanWindow, uint8_t filterPolicy,
                        uint8_t peerAddrType, uint8_t *pPeerAddr, uint8_t ownAddrType,
                        hciConnSpec_t *pConnSpec);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE create connection cancel command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeCreateConnCancelCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE encrypt command.
 *
 *  \param  pKey     Encryption key.
 *  \param  pData    Data to encrypt.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeEncryptCmd(uint8_t *pKey, uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE long term key request negative reply command.
 *
 *  \param  handle    Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeLtkReqNegReplCmd(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE long term key request reply command.
 *
 *  \param  handle    Connection handle.
 *  \param  pKey      LTK.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeLtkReqReplCmd(uint16_t handle, uint8_t *pKey);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE random command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeRandCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE read advertising TX power command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadAdvTXPowerCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE read buffer size command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadBufSizeCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE read buffer size version 2 command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadBufSizeCmdV2(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE read channel map command.
 *
 *  \param  handle    Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadChanMapCmd(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE read local supported feautre command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadLocalSupFeatCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE read remote feature command.
 *
 *  \param  handle    Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadRemoteFeatCmd(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE read supported states command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadSupStatesCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE read white list size command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadWhiteListSizeCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE remove device white list command.
 *
 *  \param  addrType    Address type.
 *  \param  pAddr       Peer address.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeRemoveDevWhiteListCmd(uint8_t addrType, uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE set advanced enable command.
 *
 *  \param  enable    Enable.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetAdvEnableCmd(uint8_t enable);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE set advertising data command.
 *
 *  \param  len      Length of advertising data.
 *  \param  pData    Advertising data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetAdvDataCmd(uint8_t len, uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE set advertising parameters command.
 *
 *  \param  advIntervalMin    Adveritsing minimum interval.
 *  \param  advIntervalMax    Advertising maximum interval.
 *  \param  advType           Advertising type.
 *  \param  ownAddrType       Own address type.
 *  \param  peerAddrType      Peer address type.
 *  \param  pPeerAddr         Peer address.
 *  \param  advChanMap        Advertising channel map.
 *  \param  advFiltPolicy     Advertising filter policy.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetAdvParamCmd(uint16_t advIntervalMin, uint16_t advIntervalMax, uint8_t advType,
                         uint8_t ownAddrType, uint8_t peerAddrType, uint8_t *pPeerAddr,
                         uint8_t advChanMap, uint8_t advFiltPolicy);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE set event mask command.
 *
 *  \param  pLeEventMask    LE Event mask.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetEventMaskCmd(uint8_t *pLeEventMask);

/*************************************************************************************************/
/*!
 *  \brief  HCI set host channel class command.
 *
 *  \param  pChanMap    Channel map.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetHostChanClassCmd(uint8_t *pChanMap);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE set random address command.
 *
 *  \param  pAddr    Randon address.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetRandAddrCmd(uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE set scan enable command.
 *
 *  \param  enable       Enable.
 *  \param  filterDup    Filter duplicates.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetScanEnableCmd(uint8_t enable, uint8_t filterDup);

/*************************************************************************************************/
/*!
 *  \brief  HCI set scan parameters command.
 *
 *  \param  scanType          Scan type.
 *  \param  scanInterval      Scan interval.
 *  \param  scanWindow        Scan window.
 *  \param  ownAddrType       Own address type.
 *  \param  scanFiltPolicy    Scanning filter policy.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetScanParamCmd(uint8_t scanType, uint16_t scanInterval, uint16_t scanWindow,
                          uint8_t ownAddrType, uint8_t scanFiltPolicy);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE set scan response data.
 *
 *  \param  len      Scan response data length.
 *  \param  pData    Scan response data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetScanRespDataCmd(uint8_t len, uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE start encryption command.
 *
 *  \param  handle         Connection handle.
 *  \param  pRand          Random number.
 *  \param  diversifier    Diversifier.
 *  \param  pKey           Encryption key.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeStartEncryptionCmd(uint16_t handle, uint8_t *pRand, uint16_t diversifier, uint8_t *pKey);

/*************************************************************************************************/
/*!
 *  \brief  HCI read BD address command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciReadBdAddrCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI read buffer size command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciReadBufSizeCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI read local supported feature command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciReadLocalSupFeatCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI read local version info command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciReadLocalVerInfoCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI read remote version info command.
 *
 *  \param  handle    Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciReadRemoteVerInfoCmd(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  HCI read RSSI command.
 *
 *  \param  handle    Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciReadRssiCmd(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  HCI read Tx power level command.
 *
 *  \param  handle    Connection handle.
 *  \param  type      Type.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciReadTxPwrLvlCmd(uint16_t handle, uint8_t type);

/*************************************************************************************************/
/*!
 *  \brief  HCI reset command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciResetCmd(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI set event mask command.
 *
 *  \param  pEventMask    Page 1 of the event mask.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSetEventMaskCmd(uint8_t *pEventMask);

/*************************************************************************************************/
/*!
 *  \brief  HCI set event page 2 mask command.
 *
 *  \param  pEventMask    Page 2 of the event mask.
 *  \return None.
 */
/*************************************************************************************************/
void HciSetEventMaskPage2Cmd(uint8_t *pEventMask);

/*************************************************************************************************/
/*!
 *  \brief  HCI read authenticated payload timeout command.
 *
 *  \param  handle    Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciReadAuthPayloadTimeout(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  HCI write authenticated payload timeout command.
 *
 *  \param  handle     Connection handle.
 *  \param  timeout    Timeout value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciWriteAuthPayloadTimeout(uint16_t handle, uint16_t timeout);

/*************************************************************************************************/
/*!
 *  \brief  HCI add device to resolving list command.
 *
 *  \param  peerAddrType         Peer identity address type.
 *  \param  pPeerIdentityAddr    Peer identity address.
 *  \param  pPeerIrk             Peer IRK.
 *  \param  pLocalIrk            Local IRK.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeAddDeviceToResolvingListCmd(uint8_t peerAddrType, const uint8_t *pPeerIdentityAddr,
                                      const uint8_t *pPeerIrk, const uint8_t *pLocalIrk);

/*************************************************************************************************/
/*!
 *  \brief  HCI remove device from resolving list command.
 *
 *  \param  peerAddrType         Peer identity address type.
 *  \param  pPeerIdentityAddr    Peer identity address.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeRemoveDeviceFromResolvingList(uint8_t peerAddrType, const uint8_t *pPeerIdentityAddr);

/*************************************************************************************************/
/*!
 *  \brief  HCI clear resolving list command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeClearResolvingList(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI read resolving list command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadResolvingListSize(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI read peer resolvable address command.
 *
 *  \param  addrType         Peer identity address type.
 *  \param  pIdentityAddr    Peer identity address.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadPeerResolvableAddr(uint8_t addrType, const uint8_t *pIdentityAddr);

/*************************************************************************************************/
/*!
 *  \brief  HCI read local resolvable address command.
 *
 *  \param  addrType         Peer identity address type.
 *  \param  pIdentityAddr    Peer identity address.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadLocalResolvableAddr(uint8_t addrType, const uint8_t *pIdentityAddr);

/*************************************************************************************************/
/*!
 *  \brief  HCI enable or disable address resolution command.
 *
 *  \param  enable    Set to TRUE to enable address resolution or FALSE to disable address
 *                    resolution.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetAddrResolutionEnable(uint8_t enable);

/*************************************************************************************************/
/*!
 *  \brief  HCI set resolvable private address timeout command.
 *
 *  \param  rpaTimeout    Timeout measured in seconds.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetResolvablePrivateAddrTimeout(uint16_t rpaTimeout);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE set privacy mode command.
 *
 *  \param  addrType    Peer identity address type.
 *  \param  pAddr       Peer identity address.
 *  \param  mode        Privacy mode.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetPrivacyModeCmd(uint8_t addrType, uint8_t *pAddr, uint8_t mode);

/*************************************************************************************************/
/*!
*  \brief  HCI read PHY command.
*
*  \param  handle    Connection handle.
*
*  \return None.
*/
/*************************************************************************************************/
void HciLeReadPhyCmd(uint16_t handle);

/*************************************************************************************************/
/*!
*  \brief  HCI set default PHY command.
*
*  \param  allPhys    All PHYs.
*  \param  txPhys     Tx PHYs.
*  \param  rxPhys     Rx PHYs.
*
*  \return None.
*/
/*************************************************************************************************/
void HciLeSetDefaultPhyCmd(uint8_t allPhys, uint8_t txPhys, uint8_t rxPhys);

/*************************************************************************************************/
/*!
*  \brief  HCI set PHY command.
*
*  \param  handle        Connection handle.
*  \param  allPhys       All PHYs.
*  \param  txPhys        Tx PHYs.
*  \param  rxPhys        Rx PHYs.
*  \param  phyOptions    PHY options.
*
*  \return None.
*/
/*************************************************************************************************/
void HciLeSetPhyCmd(uint16_t handle, uint8_t allPhys, uint8_t txPhys, uint8_t rxPhys, uint16_t phyOptions);

/*************************************************************************************************/
/*!
 *  \brief  HCI vencor specific command.
 *
 *  \param  opcode    Opcode.
 *  \param  len       Length of pData.
 *  \param  pData     Command data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciVendorSpecificCmd(uint16_t opcode, uint8_t len, uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief  HCI Remote Connection Parameter Request Reply.
 *
 *  \param  handle         Connection handle.
 *  \param  intervalMin    Interval minimum.
 *  \param  intervalMax    Interval maximum.
 *  \param  latency        Connection latency.
 *  \param  timeout        Connection timeout.
 *  \param  minCeLen       Minimum connection event length.
 *  \param  maxCeLen       Maximum connection event length.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeRemoteConnParamReqReply(uint16_t handle, uint16_t intervalMin, uint16_t intervalMax, uint16_t latency,
                                  uint16_t timeout, uint16_t minCeLen, uint16_t maxCeLen);

/*************************************************************************************************/
/*!
 *  \brief  HCI Remote Connection Parameter Request Negative Reply.
 *
 *  \param  handle    Connection handle.
 *  \param  reason    Negative reply reason.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeRemoteConnParamReqNegReply(uint16_t handle, uint8_t reason);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE Set Data Length.
 *
 *  \param  handle      Connection handle.
 *  \param  txOctets    Tx octets.
 *  \param  txTime      Tx time.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetDataLen(uint16_t handle, uint16_t txOctets, uint16_t txTime);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE Read Default Data Length.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadDefDataLen(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE Write Default Data Length.
 *
 *  \param  suggestedMaxTxOctets    Suggested maximum Tx octets.
 *  \param  suggestedMaxTxTime      Suggested maximum Tx time.
 *
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeWriteDefDataLen(uint16_t suggestedMaxTxOctets, uint16_t suggestedMaxTxTime);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE Read Local P-256 Public Key.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadLocalP256PubKey(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE Generate DH Key.
 *
 *  \param  pPubKeyX    Public key X-coordinate.
 *  \param  pPubKeyY    Public key Y-coordinate.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeGenerateDHKey(uint8_t *pPubKeyX, uint8_t *pPubKeyY);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE Generate DH Key Version 2.
 *
 *  \param  pPubKeyX    Public key X-coordinate.
 *  \param  pPubKeyY    Public key Y-coordinate.
 *  \param  keyType     Key type.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeGenerateDHKeyV2(uint8_t *pPubKeyX, uint8_t *pPubKeyY, uint8_t keyType);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE Read Maximum Data Length.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadMaxDataLen(void);

/*************************************************************************************************/
/*!
 *  \brief  HCI write authenticated payload timeout command.
 *
 *  \param  handle     Connection handle.
 *  \param  timeout    Timeout value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciWriteAuthPayloadTimeout(uint16_t handle, uint16_t timeout);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE read transmit power command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeReadTxPower(void);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE read RF path compensation command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeReadRfPathComp(void);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE write RF path compensation command.
 *
 *  \param      txPathComp    RF transmit path compensation value.
 *  \param      rxPathComp    RF receive path compensation value.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeWriteRfPathComp(int16_t txPathComp, int16_t rxPathComp);
/**@}*/

/** \name HCI AE Advertiser Interface
 * HCI Advertising Extension functions used by the Advertiser role.
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief      HCI LE set advertising set random device address command.
 *
 *  \param      advHandle    Advertising handle.
 *  \param      pAddr        Random device address.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetAdvSetRandAddrCmd(uint8_t advHandle, const uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set extended advertising parameters command.
 *
 *  \param      advHandle       Advertising handle.
 *  \param      pExtAdvParam    Extended advertising parameters.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetExtAdvParamCmd(uint8_t advHandle, hciExtAdvParam_t *pExtAdvParam);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set extended advertising data command.
 *
 *  \param      advHandle    Advertising handle.
 *  \param      op           Operation.
 *  \param      fragPref     Fragment preference.
 *  \param      len          Data buffer length.
 *  \param      pData        Advertising data buffer.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetExtAdvDataCmd(uint8_t advHandle, uint8_t op, uint8_t fragPref, uint8_t len,
                           const uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set extended scan response data command.
 *
 *  \param      advHandle    Advertising handle.
 *  \param      op           Operation.
 *  \param      fragPref     Fragment preference.
 *  \param      len          Data buffer length.
 *  \param      pData        Scan response data buffer.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetExtScanRespDataCmd(uint8_t advHandle, uint8_t op, uint8_t fragPref, uint8_t len,
                                const uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set extended advertising enable command.
 *
 *  \param      enable          Set to TRUE to enable advertising, FALSE to disable advertising.
 *  \param      numSets         Number of advertising sets.
 *  \param      pEnableParam    Advertising enable parameter array.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetExtAdvEnableCmd(uint8_t enable, uint8_t numSets, hciExtAdvEnableParam_t *pEnableParam);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE read maximum advertising data length command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeReadMaxAdvDataLen(void);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE read number of supported advertising sets command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeReadNumSupAdvSets(void);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE remove advertising set command.
 *
 *  \param      advHandle    Advertising handle.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
void HciLeRemoveAdvSet(uint8_t advHandle);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE clear advertising sets command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeClearAdvSets(void);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set periodic advertising parameters command.
 *
 *  \param      advHandle         Advertising handle.
 *  \param      advIntervalMin    Periodic advertising interval minimum.
 *  \param      advIntervalMax    Periodic advertising interval maximum.
 *  \param      advProps          Periodic advertising properties.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetPerAdvParamCmd(uint8_t advHandle, uint16_t advIntervalMin, uint16_t advIntervalMax,
                            uint16_t advProps);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set periodic advertising data command.
 *
 *  \param      advHandle    Advertising handle.
 *  \param      op           Operation.
 *  \param      len          Data buffer length.
 *  \param      pData        Advertising data buffer.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetPerAdvDataCmd(uint8_t advHandle, uint8_t op, uint8_t len, const uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set periodic advertising enable command.
 *
 *  \param      enable       Set to TRUE to enable advertising, FALSE to disable advertising.
 *  \param      advHandle    Advertising handle.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetPerAdvEnableCmd(uint8_t enable, uint8_t advHandle);
/**@}*/

/** \name HCI AE Scanner Interface
 * HCI Advertising Extension functions used in the Scanner role.
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief      HCI LE set extended scanning parameters command.
 *
 *  \param      ownAddrType       Address type used by this device.
 *  \param      scanFiltPolicy    Scan filter policy.
 *  \param      scanPhys          Scanning PHYs.
 *  \param      pScanParam        Scanning parameter array.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetExtScanParamCmd(uint8_t ownAddrType, uint8_t scanFiltPolicy, uint8_t scanPhys,
                             hciExtScanParam_t *pScanParam);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE extended scan enable command.
 *
 *  \param      enable          Set to TRUE to enable scanning, FALSE to disable scanning.
 *  \param      filterDup       Set to TRUE to filter duplicates.
 *  \param      duration        Duration.
 *  \param      period          Period.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeExtScanEnableCmd(uint8_t enable, uint8_t filterDup, uint16_t duration, uint16_t period);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE extended create connection command.
 *
 *  \param      pInitParam      Initiating parameters.
 *  \param      pScanParam      Initiating scan parameters.
 *  \param      pConnSpec       Connection specification.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeExtCreateConnCmd(hciExtInitParam_t *pInitParam, hciExtInitScanParam_t *pScanParam,
                           hciConnSpec_t *pConnSpec);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE periodic advertising create sync command.
 *
 *  \param      options         options.
 *  \param      advSid          Advertising SID.
 *  \param      advAddrType     Advertiser address type.
 *  \param      pAdvAddr        Advertiser address.
 *  \param      skip            Number of periodic advertising packets that can be skipped after
 *                              successful receive.
 *  \param      syncTimeout     Synchronization timeout.
 *  \param      unused          Reserved for future use (must be zero).
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLePerAdvCreateSyncCmd(uint8_t options, uint8_t advSid, uint8_t advAddrType,
                              uint8_t *pAdvAddr, uint16_t skip, uint16_t syncTimeout, uint8_t unused);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE periodic advertising create sync cancel command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLePerAdvCreateSyncCancelCmd(void);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE periodic advertising terminate sync command.
 *
 *  \param      syncHandle      Sync handle.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLePerAdvTerminateSyncCmd(uint16_t syncHandle);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE add device to periodic advertiser list command.
 *
 *  \param      advAddrType     Advertiser address type.
 *  \param      pAdvAddr        Advertiser address.
 *  \param      advSid          Advertising SID.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeAddDeviceToPerAdvListCmd(uint8_t advAddrType, uint8_t *pAdvAddr, uint8_t advSid);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE remove device from periodic advertiser list command.
 *
 *  \param      advAddrType     Advertiser address type.
 *  \param      pAdvAddr        Advertiser address.
 *  \param      advSid          Advertising SID.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeRemoveDeviceFromPerAdvListCmd(uint8_t advAddrType, uint8_t *pAdvAddr, uint8_t advSid);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE clear periodic advertiser list command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeClearPerAdvListCmd(void);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE read periodic advertiser size command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeReadPerAdvListSizeCmd(void);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set periodic advertising receive enable command.
 *
 *  \param      syncHandle   Periodic sync handle.
 *  \param      enable       TRUE to enable reports, FALSE to disable reports.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetPerAdvRcvEnableCmd(uint16_t syncHandle, uint8_t enable);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE periodic advertising sync transfer command.
 *
 *  \param      connHandle   Connection handle.
 *  \param      serviceData  Service data provided by the host.
 *  \param      syncHandle   Periodic sync handle.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLePerAdvSyncTrsfCmd(uint16_t connHandle, uint16_t serviceData, uint16_t syncHandle);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set periodic advertising set info transfer command.
 *
 *  \param      connHandle   Connection handle.
 *  \param      serviceData  Service data provided by the host.
 *  \param      advHandle    Handle to identify an advertising set.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLePerAdvSetInfoTrsfCmd(uint16_t connHandle, uint16_t serviceData, uint8_t advHandle);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set periodic advertising sync transfer parameters command.
 *
 *  \param      connHandle   Connection handle.
 *  \param      mode         Periodic sync advertising sync transfer mode.
 *  \param      skip         The number of periodic advertising packets that can be skipped after
 *                           a successful receive.
 *  \param      syncTimeout  Synchronization timeout for the periodic advertising.
 *  \param      cteType      Constant tone extension type(Used in AoD/AoA).
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetPerAdvSyncTrsfParamsCmd(uint16_t connHandle, uint8_t mode, uint16_t skip,
                                     uint16_t syncTimeout, uint8_t cteType);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set default periodic advertising sync transfer parameters command.
 *
 *  \param      mode         Periodic sync advertising sync transfer mode.
 *  \param      skip         The number of periodic advertising packets that can be skipped after
 *                           a successful receive.
 *  \param      syncTimeout  Synchronization timeout for the periodic advertising.
 *  \param      cteType      Constant tone extension type(Used in AoD/AoA).
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetDefaultPerAdvSyncTrsfParamsCmd(uint8_t mode, uint16_t skip, uint16_t syncTimeout,
                                            uint8_t cteType);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set connection CTE receive parameters command.
 *
 *  \param      connHandle       Connection handle.
 *  \param      samplingEnable   TRUE to enable Connection IQ sampling, FALSE to disable it.
 *  \param      slotDurations    Switching and sampling slot durations to be used while receiving CTE.
 *  \param      switchPatternLen Number of Antenna IDs in switching pattern.
 *  \param      pAntennaIDs      List of Antenna IDs in switching pattern.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetConnCteRxParamsCmd(uint16_t connHandle, uint8_t samplingEnable, uint8_t slotDurations,
                                uint8_t switchPatternLen, uint8_t *pAntennaIDs);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set connection CTE transmit parameters command.
 *
 *  \param      connHandle       Connection handle.
 *  \param      cteTypeBits      Permitted CTE type bits used for transmitting CTEs requested by peer.
 *  \param      switchPatternLen Number of Antenna IDs in switching pattern.
 *  \param      pAntennaIDs      List of Antenna IDs in switching pattern.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetConnCteTxParamsCmd(uint16_t connHandle, uint8_t cteTypeBits, uint8_t switchPatternLen,
                                uint8_t *pAntennaIDs);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE connection CTE request enable command.
 *
 *  \param      connHandle  Connection handle.
 *  \param      enable      TRUE to enable CTE request for connection, FALSE to disable it.
 *  \param      cteReqInt   CTE request interval.
 *  \param      reqCteLen   Minimum length of CTE being requested in 8 us units.
 *  \param      reqCteType  Requested CTE type.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeConnCteReqEnableCmd(uint16_t connHandle, uint8_t enable, uint16_t cteReqInt,
                              uint8_t reqCteLen, uint8_t reqCteType);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE connection CTE response enable command.
 *
 *  \param      connHandle  Connection handle.
 *  \param      enable      TRUE to enable CTE response for connection, FALSE to disable it.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeConnCteRspEnableCmd(uint16_t connHandle, uint8_t enable);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE read antenna information command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeReadAntennaInfoCmd(void);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set CIG parameters command.
 *
 *  \param      pCigParam    CIG parameters.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetCigParamsCmd(HciCisCigParams_t *pCigParam);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE create CIS command.
 *
 *  \param      numCis            Nunber of CISes.
 *  \param      pCreateCisParam   Parameters for creating connected isochronous stream.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeCreateCisCmd(uint8_t numCis, HciCisCreateCisParams_t *pCreateCisParam);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE accept CIS request command.
 *
 *  \param      connHandle   Connection handle of the CIS to be accepted.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeAcceptCisReqCmd(uint16_t connHandle);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE reject CIS request command.
 *
 *  \param      connHandle   Connection handle of the CIS to be rejected.
 *  \param      reason       Reason the CIS request was rejected.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeRejectCisReqCmd(uint16_t connHandle, uint8_t reason);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE remove CIG command.
 *
 *  \param      cigId        Identifer of a CIG.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeRemoveCigCmd(uint8_t cigId);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE request peer SCA command.
 *
 *  \param      handle    Connection handle.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeRequestPeerScaCmd(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE create BIG command.
 *
 *  \param      pCreateBis    Create BIG parameters.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeCreateBigCmd(HciCreateBig_t *pCreateBig);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE terminate BIG command.
 *
 *  \param      bigHandle     Used to identify the BIG.
 *  \param      reason        Termination reason.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciTerminateBigCmd(uint8_t bigHandle, uint8_t reason);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE BIG create sync command.
 *
 *  \param      pCreateSync     BIG Create Sync parameters.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeBigCreateSyncCmd(HciBigCreateSync_t *pCreateSync);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE BIG terminate sync command.
 *
 *  \param      bigHandle     Used to identify the BIG.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeBigTerminateSync(uint8_t bigHandle);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE enable ISO Tx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pldType     Payload type.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeIsoTxTest(uint16_t handle, uint8_t pldType);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE enable ISO Rx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pldType     Payload type.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeIsoRxTest(uint16_t handle, uint8_t pldType);

/*************************************************************************************************/
/*!
 *  \brief  HCI LE read ISO test counter.
 *
 *  \param      handle      CIS or BIS handle.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeIsoReadTestCounters(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE ISO test end.
 *
 *  \param      handle      CIS or BIS handle.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeIsoTestEnd(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE setup ISO data path command.
 *
 *  \param      pDataPathParam  Parameters for setup ISO data path.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeSetupIsoDataPathCmd(HciIsoSetupDataPath_t *pDataPathParam);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE remove ISO data path command.
 *
 *  \param      handle          Connection handle of the CIS or BIS.
 *  \param      directionBits   Data path direction bits.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciLeRemoveIsoDataPathCmd(uint16_t handle, uint8_t directionBits);

/*************************************************************************************************/
/*!
 *  \brief      HCI configure data path command.
 *
 *  \param      pDataPathParam  Parameters for configuring data path.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciConfigDataPathCmd(HciConfigDataPath_t *pDataPathParam);

/*************************************************************************************************/
/*!
 *  \brief      HCI read local supported codecs command.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciReadLocalSupCodecsCmd(void);

/*************************************************************************************************/
/*!
 *  \brief      HCI read local supported codec capabilities command.
 *
 *  \param      pCodecParam     Parameters to read codec capablilties.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciReadLocalSupCodecCapCmd(HciReadLocalSupCodecCaps_t *pCodecParam);

/*************************************************************************************************/
/*!
 *  \brief      HCI read local supported controller delay command.
 *
 *  \param      pDelayParam     Parameters to read controller delay.
 *
 *  \return     None.
 */
/*************************************************************************************************/
void HciReadLocalSupControllerDlyCmd(HciReadLocalSupControllerDly_t *pDelayParam);

/*************************************************************************************************/
/*!
 *  \brief      HCI LE set host feature command.
 *
 *  \param      bitNum    Bit position in the FeatureSet.
 *  \param      bitVal    Enable or disable feature.
 *
 *  \return     None.
 *
 *  \note Set or clear a bit in the feature controlled by the Host in the Link Layer FeatureSet
 *  stored in the Controller.
 */
/*************************************************************************************************/
void HciLeSetHostFeatureCmd(uint8_t bitNum, bool_t bitVal);

/**@}*/

/*! \} */    /* STACK_HCI_CMD_API */

#ifdef __cplusplus
};
#endif

#endif /* HCI_API_H */
