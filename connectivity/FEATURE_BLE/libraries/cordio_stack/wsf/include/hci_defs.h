/*************************************************************************************************/
/*!
 *  \file   hci_defs.h
 *
 *  \brief  HCI constants and definitions from the Bluetooth specification.
 *
 *  Copyright (c) 2009-2019 ARM Ltd. All Rights Reserved.
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
#ifndef HCI_DEFS_H
#define HCI_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup STACK_HCI_API
 *  \{ */

/** \name Packet definitions
 *
 */
/**@{*/
#define HCI_CMD_HDR_LEN                              3       /*!< Command packet header length */
#define HCI_ACL_HDR_LEN                              4       /*!< ACL packet header length */
#define HCI_ISO_HDR_LEN                              4       /*!< ISO packet header length */
#define HCI_EVT_HDR_LEN                              2       /*!< Event packet header length */
#define HCI_EVT_PARAM_MAX_LEN                        255     /*!< Maximum length of event packet parameters */
#define HCI_ACL_DEFAULT_LEN                          27      /*!< Default maximum ACL packet length */
#define HCI_PB_FLAG_MASK                             0x3000  /*!< ACL packet boundary flag mask */
#define HCI_PB_START_H2C                             0x0000  /*!< Packet boundary flag, start, host-to-controller */
#define HCI_PB_CONTINUE                              0x1000  /*!< Packet boundary flag, continue */
#define HCI_PB_START_C2H                             0x2000  /*!< Packet boundary flag, start, controller-to-host */
#define HCI_HANDLE_MASK                              0x0FFF  /*!< Mask for handle bits in ACL packet */
#define HCI_HANDLE_NONE                              0xFFFF  /*!< Value for invalid handle */
#define HCI_ISO_GROUP_ID_NONE                        0xFF    /*!< Value for invalid CIG/BIG ID */


/* ISO Header. */
#define HCI_TS_FLAG_MASK                             (1 << 14)
                                                             /*!< Timestamp flag mask for ISO packets. */
#define HCI_DATA_LOAD_LEN_MASK                       0x3FFF  /*!< HCI Data load length. */

/* ISO Data load. */
#define HCI_ISO_DL_MIN_LEN                           4       /*!< ISO Data Load header minimum length */
#define HCI_ISO_DL_MAX_LEN                           8       /*!< ISO Data Load header maximum length */
#define HCI_ISO_TS_LEN                               4       /*!< ISO Data Load timestamp length */
#define HCI_ISO_DL_SDU_LEN_MASK                      0x0FFF  /*!< HCI SDU Length mask. */
#define HCI_ISO_DL_PS_MASK                           0xC000  /*!< HCI Packet status mask. */

/**@}*/

/** \name Packet types
 *
 */
#define HCI_CMD_TYPE                                0x01      /*!< HCI command packet */
#define HCI_ACL_TYPE                                0x02      /*!< HCI ACL data packet */
#define HCI_EVT_TYPE                                0x04      /*!< HCI event packet */
#define HCI_ISO_TYPE                                0x05      /*!< HCI ISO data packet */
/**@}*/

/** \name Error codes
 *
 */
/**@{*/
#define HCI_SUCCESS                                  0x00    /*!< Success */
#define HCI_ERR_UNKNOWN_CMD                          0x01    /*!< Unknown HCI command */
#define HCI_ERR_UNKNOWN_HANDLE                       0x02    /*!< Unknown connection identifier */
#define HCI_ERR_HARDWARE_FAILURE                     0x03    /*!< Hardware failure */
#define HCI_ERR_PAGE_TIMEOUT                         0x04    /*!< Page timeout */
#define HCI_ERR_AUTH_FAILURE                         0x05    /*!< Authentication failure */
#define HCI_ERR_KEY_MISSING                          0x06    /*!< PIN or key missing */
#define HCI_ERR_MEMORY_EXCEEDED                      0x07    /*!< Memory capacity exceeded */
#define HCI_ERR_CONN_TIMEOUT                         0x08    /*!< Connection timeout */
#define HCI_ERR_CONN_LIMIT                           0x09    /*!< Connection limit exceeded */
#define HCI_ERR_SYNCH_CONN_LIMIT                     0x0A    /*!< Synchronous connection limit exceeded */
#define HCI_ERR_ACL_CONN_EXISTS                      0x0B    /*!< ACL connection already exists */
#define HCI_ERR_CMD_DISALLOWED                       0x0C    /*!< Command disallowed */
#define HCI_ERR_REJ_RESOURCES                        0x0D    /*!< Connection rejected limited resources */
#define HCI_ERR_REJ_SECURITY                         0x0E    /*!< Connection rejected security reasons */
#define HCI_ERR_REJ_BD_ADDR                          0x0F    /*!< Connection rejected unacceptable BD_ADDR */
#define HCI_ERR_ACCEPT_TIMEOUT                       0x10    /*!< Connection accept timeout exceeded */
#define HCI_ERR_UNSUP_FEAT                           0x11    /*!< Unsupported feature or parameter value */
#define HCI_ERR_INVALID_PARAM                        0x12    /*!< Invalid HCI command parameters */
#define HCI_ERR_REMOTE_TERMINATED                    0x13    /*!< Remote user terminated connection */
#define HCI_ERR_REMOTE_RESOURCES                     0x14    /*!< Remote device low resources */
#define HCI_ERR_REMOTE_POWER_OFF                     0x15    /*!< Remote device power off */
#define HCI_ERR_LOCAL_TERMINATED                     0x16    /*!< Connection terminated by local host */
#define HCI_ERR_REPEATED_ATTEMPTS                    0x17    /*!< Repeated attempts */
#define HCI_ERR_PAIRING_NOT_ALLOWED                  0x18    /*!< Pairing not allowed */
#define HCI_ERR_UNKNOWN_LMP_PDU                      0x19    /*!< Unknown LMP PDU */
#define HCI_ERR_UNSUP_REMOTE_FEAT                    0x1A    /*!< Unsupported remote feature */
#define HCI_ERR_SCO_OFFSET                           0x1B    /*!< SCO offset rejected */
#define HCI_ERR_SCO_INTERVAL                         0x1C    /*!< SCO interval rejected */
#define HCI_ERR_SCO_MODE                             0x1D    /*!< SCO air mode rejected */
#define HCI_ERR_LMP_PARAM                            0x1E    /*!< Invalid LMP parameters */
#define HCI_ERR_UNSPECIFIED                          0x1F    /*!< Unspecified error */
#define HCI_ERR_UNSUP_LMP_PARAM                      0x20    /*!< Unsupported LMP parameter value */
#define HCI_ERR_ROLE_CHANGE                          0x21    /*!< Role change not allowed */
#define HCI_ERR_LL_RESP_TIMEOUT                      0x22    /*!< LL response timeout */
#define HCI_ERR_LMP_COLLISION                        0x23    /*!< LMP error transaction collision */
#define HCI_ERR_LMP_PDU                              0x24    /*!< LMP pdu not allowed */
#define HCI_ERR_ENCRYPT_MODE                         0x25    /*!< Encryption mode not acceptable */
#define HCI_ERR_LINK_KEY                             0x26    /*!< Link key can not be changed */
#define HCI_ERR_UNSUP_QOS                            0x27    /*!< Requested qos not supported */
#define HCI_ERR_INSTANT_PASSED                       0x28    /*!< Instant passed */
#define HCI_ERR_UNSUP_UNIT_KEY                       0x29    /*!< Pairing with unit key not supported */
#define HCI_ERR_TRANSACT_COLLISION                   0x2A    /*!< Different transaction collision */
#define HCI_ERR_CHANNEL_CLASS                        0x2E    /*!< Channel classification not supported */
#define HCI_ERR_MEMORY                               0x2F    /*!< Insufficient security */
#define HCI_ERR_PARAMETER_RANGE                      0x30    /*!< Parameter out of mandatory range */
#define HCI_ERR_ROLE_SWITCH_PEND                     0x32    /*!< Role switch pending */
#define HCI_ERR_RESERVED_SLOT                        0x34    /*!< Reserved slot violation */
#define HCI_ERR_ROLE_SWITCH                          0x35    /*!< Role switch failed */
#define HCI_ERR_INQ_TOO_LARGE                        0x36    /*!< Extended inquiry response too large */
#define HCI_ERR_UNSUP_SSP                            0x37    /*!< Secure simple pairing not supported by host */
#define HCI_ERR_HOST_BUSY_PAIRING                    0x38    /*!< Host busy - pairing */
#define HCI_ERR_NO_CHANNEL                           0x39    /*!< Connection rejected no suitable channel */
#define HCI_ERR_CONTROLLER_BUSY                      0x3A    /*!< Controller busy */
#define HCI_ERR_CONN_INTERVAL                        0x3B    /*!< Unacceptable connection interval */
#define HCI_ERR_ADV_TIMEOUT                          0x3C    /*!< Advertising timeout */
#define HCI_ERR_MIC_FAILURE                          0x3D    /*!< Connection terminated due to MIC failure */
#define HCI_ERR_CONN_FAIL                            0x3E    /*!< Connection failed to be established */
#define HCI_ERR_MAC_CONN_FAIL                        0x3F    /*!< MAC connection failed */
#define HCI_ERR_COARSE_CLK_ADJ_REJ                   0x40    /*!< Coarse clock adjustment rejected */
#define HCI_ERR_TYPE0_SUBMAP_NOT_DEF                 0x41    /*!< Type0 submap not defined */
#define HCI_ERR_UNKNOWN_ADV_ID                       0x42    /*!< Unknown advertising identifier */
#define HCI_ERR_LIMIT_REACHED                        0x43    /*!< Limit reached */
#define HCI_ERR_OP_CANCELLED_BY_HOST                 0x44    /*!< Operation cancelled by host */
/* Version 5.1 */
#define HCI_ERR_PKT_TOO_LONG                         0x45    /*!< Packet too long */
/**@}*/

/** \name Command groups
 *
 */
/**@{*/
#define HCI_OGF_NOP                                  0x00    /*!< No operation */
#define HCI_OGF_LINK_CONTROL                         0x01    /*!< Link control */
#define HCI_OGF_LINK_POLICY                          0x02    /*!< Link policy */
#define HCI_OGF_CONTROLLER                           0x03    /*!< Controller and baseband */
#define HCI_OGF_INFORMATIONAL                        0x04    /*!< Informational parameters */
#define HCI_OGF_STATUS                               0x05    /*!< Status parameters */
#define HCI_OGF_TESTING                              0x06    /*!< Testing */
#define HCI_OGF_LE_CONTROLLER                        0x08    /*!< LE controller */
#define HCI_OGF_VENDOR_SPEC                          0x3F    /*!< Vendor specific */
/**@}*/

/** \name NOP command
 *
 */
/**@{*/
#define HCI_OCF_NOP                                  0x00
/**@}*/

/** \name Link control commands
 *
 */
/**@{*/
#define HCI_OCF_DISCONNECT                           0x06
#define HCI_OCF_READ_REMOTE_VER_INFO                 0x1D
/**@}*/

/* Link policy commands (none used for LE) */

/** \name Controller and baseband commands
 *
 */
/**@{*/
#define HCI_OCF_SET_EVENT_MASK                       0x01
#define HCI_OCF_RESET                                0x03
#define HCI_OCF_READ_TX_PWR_LVL                      0x2D
#define HCI_OCF_SET_CONTROLLER_TO_HOST_FC            0x31
#define HCI_OCF_HOST_BUFFER_SIZE                     0x33
#define HCI_OCF_HOST_NUM_CMPL_PKTS                   0x35
#define HCI_OCF_READ_AFH_CHAN_ASSESMENT_MODE         0x48
#define HCI_OCF_WRITE_AFH_CHAN_ASSESSMENT_MODE       0x49
/* Version 4.1 */
#define HCI_OCF_SET_EVENT_MASK_PAGE2                 0x63
#define HCI_OCF_READ_AUTH_PAYLOAD_TO                 0x7B
#define HCI_OCF_WRITE_AUTH_PAYLOAD_TO                0x7C
/* Version 5.2 */
#define HCI_OCF_CONFIG_DATA_PATH                     0x83
/**@}*/

/** \name Informational commands
 *
 */
/**@{*/
#define HCI_OCF_READ_LOCAL_VER_INFO                  0x01
#define HCI_OCF_READ_LOCAL_SUP_CMDS                  0x02
#define HCI_OCF_READ_LOCAL_SUP_FEAT                  0x03
#define HCI_OCF_READ_BUF_SIZE                        0x05
#define HCI_OCF_READ_BD_ADDR                         0x09

/* Version 5.2 */
#define HCI_OCF_READ_LOCAL_SUP_CODECS                0x0D
#define HCI_OCF_READ_LOCAL_SUP_CODEC_CAP             0x0E
#define HCI_OCF_READ_LOCAL_SUP_CONTROLLER_DLY        0x0F
/**@}*/

/** \name Status commands
 *
 */
/**@{*/
#define HCI_OCF_READ_RSSI                            0x05
/**@}*/

/** \name LE controller commands
 *
 */
/**@{*/
#define HCI_OCF_LE_SET_EVENT_MASK                    0x01
#define HCI_OCF_LE_READ_BUF_SIZE                     0x02
#define HCI_OCF_LE_READ_LOCAL_SUP_FEAT               0x03
#define HCI_OCF_LE_SET_RAND_ADDR                     0x05
#define HCI_OCF_LE_SET_ADV_PARAM                     0x06
#define HCI_OCF_LE_READ_ADV_TX_POWER                 0x07
#define HCI_OCF_LE_SET_ADV_DATA                      0x08
#define HCI_OCF_LE_SET_SCAN_RESP_DATA                0x09
#define HCI_OCF_LE_SET_ADV_ENABLE                    0x0A
#define HCI_OCF_LE_SET_SCAN_PARAM                    0x0B
#define HCI_OCF_LE_SET_SCAN_ENABLE                   0x0C
#define HCI_OCF_LE_CREATE_CONN                       0x0D
#define HCI_OCF_LE_CREATE_CONN_CANCEL                0x0E
#define HCI_OCF_LE_READ_WHITE_LIST_SIZE              0x0F
#define HCI_OCF_LE_CLEAR_WHITE_LIST                  0x10
#define HCI_OCF_LE_ADD_DEV_WHITE_LIST                0x11
#define HCI_OCF_LE_REMOVE_DEV_WHITE_LIST             0x12
#define HCI_OCF_LE_CONN_UPDATE                       0x13
#define HCI_OCF_LE_SET_HOST_CHAN_CLASS               0x14
#define HCI_OCF_LE_READ_CHAN_MAP                     0x15
#define HCI_OCF_LE_READ_REMOTE_FEAT                  0x16
#define HCI_OCF_LE_ENCRYPT                           0x17
#define HCI_OCF_LE_RAND                              0x18
#define HCI_OCF_LE_START_ENCRYPTION                  0x19
#define HCI_OCF_LE_LTK_REQ_REPL                      0x1A
#define HCI_OCF_LE_LTK_REQ_NEG_REPL                  0x1B
#define HCI_OCF_LE_READ_SUP_STATES                   0x1C
#define HCI_OCF_LE_RECEIVER_TEST                     0x1D
#define HCI_OCF_LE_TRANSMITTER_TEST                  0x1E
#define HCI_OCF_LE_TEST_END                          0x1F
/* Version 4.1 */
#define HCI_OCF_LE_REM_CONN_PARAM_REP                0x20
#define HCI_OCF_LE_REM_CONN_PARAM_NEG_REP            0x21
/* Version 4.2 */
#define HCI_OCF_LE_SET_DATA_LEN                      0x22
#define HCI_OCF_LE_READ_DEF_DATA_LEN                 0x23
#define HCI_OCF_LE_WRITE_DEF_DATA_LEN                0x24
#define HCI_OCF_LE_READ_LOCAL_P256_PUB_KEY           0x25
#define HCI_OCF_LE_GENERATE_DHKEY                    0x26
#define HCI_OCF_LE_ADD_DEV_RES_LIST                  0x27
#define HCI_OCF_LE_REMOVE_DEV_RES_LIST               0x28
#define HCI_OCF_LE_CLEAR_RES_LIST                    0x29
#define HCI_OCF_LE_READ_RES_LIST_SIZE                0x2A
#define HCI_OCF_LE_READ_PEER_RES_ADDR                0x2B
#define HCI_OCF_LE_READ_LOCAL_RES_ADDR               0x2C
#define HCI_OCF_LE_SET_ADDR_RES_ENABLE               0x2D
#define HCI_OCF_LE_SET_RES_PRIV_ADDR_TO              0x2E
#define HCI_OCF_LE_READ_MAX_DATA_LEN                 0x2F
/* Version 5.0 */
#define HCI_OCF_LE_READ_PHY                          0x30
#define HCI_OCF_LE_SET_DEF_PHY                       0x31
#define HCI_OCF_LE_SET_PHY                           0x32
#define HCI_OCF_LE_ENHANCED_RECEIVER_TEST            0x33
#define HCI_OCF_LE_ENHANCED_TRANSMITTER_TEST         0x34
#define HCI_OCF_LE_SET_ADV_SET_RAND_ADDR             0x35
#define HCI_OCF_LE_SET_EXT_ADV_PARAM                 0x36
#define HCI_OCF_LE_SET_EXT_ADV_DATA                  0x37
#define HCI_OCF_LE_SET_EXT_SCAN_RESP_DATA            0x38
#define HCI_OCF_LE_SET_EXT_ADV_ENABLE                0x39
#define HCI_OCF_LE_READ_MAX_ADV_DATA_LEN             0x3A
#define HCI_OCF_LE_READ_NUM_SUP_ADV_SETS             0x3B
#define HCI_OCF_LE_REMOVE_ADV_SET                    0x3C
#define HCI_OCF_LE_CLEAR_ADV_SETS                    0x3D
#define HCI_OCF_LE_SET_PER_ADV_PARAM                 0x3E
#define HCI_OCF_LE_SET_PER_ADV_DATA                  0x3F
#define HCI_OCF_LE_SET_PER_ADV_ENABLE                0x40
#define HCI_OCF_LE_SET_EXT_SCAN_PARAM                0x41
#define HCI_OCF_LE_SET_EXT_SCAN_ENABLE               0x42
#define HCI_OCF_LE_EXT_CREATE_CONN                   0x43
#define HCI_OCF_LE_PER_ADV_CREATE_SYNC               0x44
#define HCI_OCF_LE_PER_ADV_CREATE_SYNC_CANCEL        0x45
#define HCI_OCF_LE_PER_ADV_TERM_SYNC                 0x46
#define HCI_OCF_LE_ADD_DEV_PER_ADV_LIST              0x47
#define HCI_OCF_LE_REMOVE_DEV_PER_ADV_LIST           0x48
#define HCI_OCF_LE_CLEAR_PER_ADV_LIST                0x49
#define HCI_OCF_LE_READ_PER_ADV_LIST_SIZE            0x4A
#define HCI_OCF_LE_READ_TX_POWER                     0x4B
#define HCI_OCF_LE_READ_RF_PATH_COMP                 0x4C
#define HCI_OCF_LE_WRITE_RF_PATH_COMP                0x4D
#define HCI_OCF_LE_SET_PRIVACY_MODE                  0x4E
/* Version 5.1 */
#define HCI_OCF_LE_RECEIVER_TEST_V3                  0x4F
#define HCI_OCF_LE_TRANSMITTER_TEST_V3               0x50
#define HCI_OCF_LE_SET_CONNLESS_CTE_TX_PARAMS        0x51
#define HCI_OCF_LE_SET_CONNLESS_CTE_TX_ENABLE        0x52
#define HCI_OCF_LE_SET_CONNLESS_IQ_SAMP_ENABLE       0x53
#define HCI_OCF_LE_SET_CONN_CTE_RX_PARAMS            0x54
#define HCI_OCF_LE_SET_CONN_CTE_TX_PARAMS            0x55
#define HCI_OCF_LE_CONN_CTE_REQ_ENABLE               0x56
#define HCI_OCF_LE_CONN_CTE_RSP_ENABLE               0x57
#define HCI_OCF_LE_READ_ANTENNA_INFO                 0x58
#define HCI_OCF_LE_SET_PER_ADV_RCV_ENABLE            0x59
#define HCI_OCF_LE_PER_ADV_SYNC_TRANSFER             0x5A
#define HCI_OCF_LE_PER_ADV_SET_INFO_TRANSFER         0x5B
#define HCI_OCF_LE_SET_PAST_PARAM                    0x5C
#define HCI_OCF_LE_SET_DEFAULT_PAST_PARAM            0x5D
#define HCI_OCF_LE_GENERATE_DHKEY_V2                 0x5E
#define HCI_OCF_LE_MODIFY_SLEEP_CLK_ACC              0x5F
/* Version 5.2 */
#define HCI_OCF_LE_READ_BUF_SIZE_V2                  0x60
#define HCI_OCF_LE_READ_ISO_TX_SYNC                  0x61
#define HCI_OCF_LE_SET_CIG_PARAMS                    0x62
#define HCI_OCF_LE_SET_CIG_PARAMS_TEST               0x63
#define HCI_OCF_LE_CREATE_CIS                        0x64
#define HCI_OCF_LE_REMOVE_CIG                        0x65
#define HCI_OCF_LE_ACCEPT_CIS_REQ                    0x66
#define HCI_OCF_LE_REJECT_CIS_REQ                    0x67
#define HCI_OCF_LE_CREATE_BIG                        0x68
#define HCI_OCF_LE_CREATE_BIG_TEST                   0x69
#define HCI_OCF_LE_TERMINATE_BIG                     0x6A
#define HCI_OCF_LE_BIG_CREATE_SYNC                   0x6B
#define HCI_OCF_LE_BIG_TERMINATE_SYNC                0x6C
#define HCI_OCF_LE_REQUEST_PEER_SCA                  0x6D
#define HCI_OCF_LE_SETUP_ISO_DATA_PATH               0x6E
#define HCI_OCF_LE_REMOVE_ISO_DATA_PATH              0x6F
#define HCI_OCF_LE_ISO_TX_TEST                       0x70
#define HCI_OCF_LE_ISO_RX_TEST                       0x71
#define HCI_OCF_LE_ISO_READ_TEST_COUNTERS            0x72
#define HCI_OCF_LE_ISO_TEST_END                      0x73
#define HCI_OCF_LE_SET_HOST_FEATURE                  0x74
#define HCI_OCF_LE_READ_ISO_LINK_QUAL                0x75
#define HCI_OCF_LE_READ_ENHANCED_TX_POWER            0x76
#define HCI_OCF_LE_READ_REMOTE_TX_POWER              0x77
#define HCI_OCF_LE_SET_PATH_LOSS_REPORTING_PARAMS    0x78
#define HCI_OCF_LE_SET_PATH_LOSS_REPORTING_ENABLE    0x79
#define HCI_OCF_LE_SET_TX_POWER_REPORT_ENABLE        0x7A
/* Version Sydney */
#define HCI_OCF_LE_SET_DEF_SUBRATE                   0x7D
#define HCI_OCF_LE_SUBRATE_REQ                       0x7E
/**@}*/

/** \name Opcode manipulation macros
 *
 */
/**@{*/
#define HCI_OPCODE(ogf, ocf)                         (((ogf) << 10) + (ocf))
#define HCI_OGF(opcode)                              ((opcode) >> 10)
#define HCI_OCF(opcode)                              ((opcode) & 0x03FF)
/**@}*/

/** \name Command opcodes
 *
 */
/**@{*/
#define HCI_OPCODE_NOP                               HCI_OPCODE(HCI_OGF_NOP, HCI_OCF_NOP)

#define HCI_OPCODE_DISCONNECT                        HCI_OPCODE(HCI_OGF_LINK_CONTROL, HCI_OCF_DISCONNECT)
#define HCI_OPCODE_READ_REMOTE_VER_INFO              HCI_OPCODE(HCI_OGF_LINK_CONTROL, HCI_OCF_READ_REMOTE_VER_INFO)

#define HCI_OPCODE_SET_EVENT_MASK                    HCI_OPCODE(HCI_OGF_CONTROLLER, HCI_OCF_SET_EVENT_MASK)
#define HCI_OPCODE_RESET                             HCI_OPCODE(HCI_OGF_CONTROLLER, HCI_OCF_RESET)
#define HCI_OPCODE_READ_AFH_CHAN_ASSESMENT_MODE      HCI_OPCODE(HCI_OGF_CONTROLLER, HCI_OCF_READ_AFH_CHAN_ASSESMENT_MODE)
#define HCI_OPCODE_WRITE_AFH_CHAN_ASSESSMENT_MODE    HCI_OPCODE(HCI_OGF_CONTROLLER, HCI_OCF_WRITE_AFH_CHAN_ASSESSMENT_MODE)
#define HCI_OPCODE_READ_TX_PWR_LVL                   HCI_OPCODE(HCI_OGF_CONTROLLER, HCI_OCF_READ_TX_PWR_LVL)
#define HCI_OPCODE_SET_EVENT_MASK_PAGE2              HCI_OPCODE(HCI_OGF_CONTROLLER, HCI_OCF_SET_EVENT_MASK_PAGE2)
#define HCI_OPCODE_READ_AUTH_PAYLOAD_TO              HCI_OPCODE(HCI_OGF_CONTROLLER, HCI_OCF_READ_AUTH_PAYLOAD_TO)
#define HCI_OPCODE_WRITE_AUTH_PAYLOAD_TO             HCI_OPCODE(HCI_OGF_CONTROLLER, HCI_OCF_WRITE_AUTH_PAYLOAD_TO)
#define HCI_OPCODE_CONFIG_DATA_PATH                  HCI_OPCODE(HCI_OGF_CONTROLLER, HCI_OCF_CONFIG_DATA_PATH)

#define HCI_OPCODE_READ_LOCAL_VER_INFO               HCI_OPCODE(HCI_OGF_INFORMATIONAL, HCI_OCF_READ_LOCAL_VER_INFO)
#define HCI_OPCODE_READ_LOCAL_SUP_CMDS               HCI_OPCODE(HCI_OGF_INFORMATIONAL, HCI_OCF_READ_LOCAL_SUP_CMDS)
#define HCI_OPCODE_READ_LOCAL_SUP_FEAT               HCI_OPCODE(HCI_OGF_INFORMATIONAL, HCI_OCF_READ_LOCAL_SUP_FEAT)
#define HCI_OPCODE_READ_BUF_SIZE                     HCI_OPCODE(HCI_OGF_INFORMATIONAL, HCI_OCF_READ_BUF_SIZE)
#define HCI_OPCODE_READ_BD_ADDR                      HCI_OPCODE(HCI_OGF_INFORMATIONAL, HCI_OCF_READ_BD_ADDR)
#define HCI_OPCODE_READ_LOCAL_SUP_CODECS             HCI_OPCODE(HCI_OGF_INFORMATIONAL, HCI_OCF_READ_LOCAL_SUP_CODECS)
#define HCI_OPCODE_READ_LOCAL_SUP_CODEC_CAP          HCI_OPCODE(HCI_OGF_INFORMATIONAL, HCI_OCF_READ_LOCAL_SUP_CODEC_CAP)
#define HCI_OPCODE_READ_LOCAL_SUP_CONTROLLER_DLY     HCI_OPCODE(HCI_OGF_INFORMATIONAL, HCI_OCF_READ_LOCAL_SUP_CONTROLLER_DLY)

#define HCI_OPCODE_READ_RSSI                         HCI_OPCODE(HCI_OGF_STATUS, HCI_OCF_READ_RSSI)

#define HCI_OPCODE_LE_SET_EVENT_MASK                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_EVENT_MASK)
#define HCI_OPCODE_LE_READ_BUF_SIZE                  HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_BUF_SIZE)
#define HCI_OPCODE_LE_READ_LOCAL_SUP_FEAT            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_LOCAL_SUP_FEAT)
#define HCI_OPCODE_LE_SET_RAND_ADDR                  HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_RAND_ADDR)
#define HCI_OPCODE_LE_SET_ADV_PARAM                  HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_ADV_PARAM)
#define HCI_OPCODE_LE_READ_ADV_TX_POWER              HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_ADV_TX_POWER)
#define HCI_OPCODE_LE_SET_ADV_DATA                   HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_ADV_DATA)
#define HCI_OPCODE_LE_SET_SCAN_RESP_DATA             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_SCAN_RESP_DATA)
#define HCI_OPCODE_LE_SET_ADV_ENABLE                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_ADV_ENABLE)
#define HCI_OPCODE_LE_SET_SCAN_PARAM                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_SCAN_PARAM)
#define HCI_OPCODE_LE_SET_SCAN_ENABLE                HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_SCAN_ENABLE)
#define HCI_OPCODE_LE_CREATE_CONN                    HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CREATE_CONN)
#define HCI_OPCODE_LE_CREATE_CONN_CANCEL             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CREATE_CONN_CANCEL)
#define HCI_OPCODE_LE_READ_WHITE_LIST_SIZE           HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_WHITE_LIST_SIZE)
#define HCI_OPCODE_LE_CLEAR_WHITE_LIST               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CLEAR_WHITE_LIST)
#define HCI_OPCODE_LE_ADD_DEV_WHITE_LIST             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ADD_DEV_WHITE_LIST)
#define HCI_OPCODE_LE_REMOVE_DEV_WHITE_LIST          HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_REMOVE_DEV_WHITE_LIST)
#define HCI_OPCODE_LE_CONN_UPDATE                    HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CONN_UPDATE)
#define HCI_OPCODE_LE_SET_HOST_CHAN_CLASS            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_HOST_CHAN_CLASS)
#define HCI_OPCODE_LE_READ_CHAN_MAP                  HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_CHAN_MAP)
#define HCI_OPCODE_LE_READ_REMOTE_FEAT               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_REMOTE_FEAT)
#define HCI_OPCODE_LE_ENCRYPT                        HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ENCRYPT)
#define HCI_OPCODE_LE_RAND                           HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_RAND)
#define HCI_OPCODE_LE_START_ENCRYPTION               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_START_ENCRYPTION)
#define HCI_OPCODE_LE_LTK_REQ_REPL                   HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_LTK_REQ_REPL)
#define HCI_OPCODE_LE_LTK_REQ_NEG_REPL               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_LTK_REQ_NEG_REPL)
#define HCI_OPCODE_LE_READ_SUP_STATES                HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_SUP_STATES)
#define HCI_OPCODE_LE_RECEIVER_TEST                  HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_RECEIVER_TEST)
#define HCI_OPCODE_LE_TRANSMITTER_TEST               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_TRANSMITTER_TEST)
#define HCI_OPCODE_LE_TEST_END                       HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_TEST_END)
/* Version 4.1 */
#define HCI_OPCODE_LE_REM_CONN_PARAM_REP             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_REM_CONN_PARAM_REP)
#define HCI_OPCODE_LE_REM_CONN_PARAM_NEG_REP         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_REM_CONN_PARAM_NEG_REP)
/* Version 4.2 */
#define HCI_OPCODE_LE_SET_DATA_LEN                   HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_DATA_LEN)
#define HCI_OPCODE_LE_READ_DEF_DATA_LEN              HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_DEF_DATA_LEN)
#define HCI_OPCODE_LE_WRITE_DEF_DATA_LEN             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_WRITE_DEF_DATA_LEN)
#define HCI_OPCODE_LE_READ_LOCAL_P256_PUB_KEY        HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_LOCAL_P256_PUB_KEY)
#define HCI_OPCODE_LE_GENERATE_DHKEY                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_GENERATE_DHKEY)
#define HCI_OPCODE_LE_ADD_DEV_RES_LIST               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ADD_DEV_RES_LIST)
#define HCI_OPCODE_LE_REMOVE_DEV_RES_LIST            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_REMOVE_DEV_RES_LIST)
#define HCI_OPCODE_LE_CLEAR_RES_LIST                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CLEAR_RES_LIST)
#define HCI_OPCODE_LE_READ_RES_LIST_SIZE             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_RES_LIST_SIZE)
#define HCI_OPCODE_LE_READ_PEER_RES_ADDR             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_PEER_RES_ADDR)
#define HCI_OPCODE_LE_READ_LOCAL_RES_ADDR            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_LOCAL_RES_ADDR)
#define HCI_OPCODE_LE_SET_ADDR_RES_ENABLE            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_ADDR_RES_ENABLE)
#define HCI_OPCODE_LE_SET_RES_PRIV_ADDR_TO           HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_RES_PRIV_ADDR_TO)
#define HCI_OPCODE_LE_READ_MAX_DATA_LEN              HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_MAX_DATA_LEN)
/* Version 5.0 */
#define HCI_OPCODE_LE_READ_PHY                       HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_PHY)
#define HCI_OPCODE_LE_SET_DEF_PHY                    HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_DEF_PHY)
#define HCI_OPCODE_LE_SET_PHY                        HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_PHY)
#define HCI_OPCODE_LE_ENHANCED_RECEIVER_TEST         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ENHANCED_RECEIVER_TEST)
#define HCI_OPCODE_LE_ENHANCED_TRANSMITTER_TEST      HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ENHANCED_TRANSMITTER_TEST)
#define HCI_OPCODE_LE_SET_ADV_SET_RAND_ADDR          HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_ADV_SET_RAND_ADDR)
#define HCI_OPCODE_LE_SET_EXT_ADV_PARAM              HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_EXT_ADV_PARAM)
#define HCI_OPCODE_LE_SET_EXT_ADV_DATA               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_EXT_ADV_DATA)
#define HCI_OPCODE_LE_SET_EXT_SCAN_RESP_DATA         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_EXT_SCAN_RESP_DATA)
#define HCI_OPCODE_LE_SET_EXT_ADV_ENABLE             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_EXT_ADV_ENABLE)
#define HCI_OPCODE_LE_READ_MAX_ADV_DATA_LEN          HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_MAX_ADV_DATA_LEN)
#define HCI_OPCODE_LE_READ_NUM_SUP_ADV_SETS          HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_NUM_SUP_ADV_SETS)
#define HCI_OPCODE_LE_REMOVE_ADV_SET                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_REMOVE_ADV_SET)
#define HCI_OPCODE_LE_CLEAR_ADV_SETS                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CLEAR_ADV_SETS)
#define HCI_OPCODE_LE_SET_PER_ADV_PARAM              HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_PER_ADV_PARAM)
#define HCI_OPCODE_LE_SET_PER_ADV_DATA               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_PER_ADV_DATA)
#define HCI_OPCODE_LE_SET_PER_ADV_ENABLE             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_PER_ADV_ENABLE)
#define HCI_OPCODE_LE_SET_EXT_SCAN_PARAM             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_EXT_SCAN_PARAM)
#define HCI_OPCODE_LE_SET_EXT_SCAN_ENABLE            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_EXT_SCAN_ENABLE)
#define HCI_OPCODE_LE_EXT_CREATE_CONN                HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_EXT_CREATE_CONN)
#define HCI_OPCODE_LE_PER_ADV_CREATE_SYNC            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_PER_ADV_CREATE_SYNC)
#define HCI_OPCODE_LE_PER_ADV_CREATE_SYNC_CANCEL     HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_PER_ADV_CREATE_SYNC_CANCEL)
#define HCI_OPCODE_LE_PER_ADV_TERMINATE_SYNC         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_PER_ADV_TERM_SYNC)
#define HCI_OPCODE_LE_ADD_DEV_PER_ADV_LIST           HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ADD_DEV_PER_ADV_LIST)
#define HCI_OPCODE_LE_REMOVE_DEV_PER_ADV_LIST        HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_REMOVE_DEV_PER_ADV_LIST)
#define HCI_OPCODE_LE_CLEAR_PER_ADV_LIST             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CLEAR_PER_ADV_LIST)
#define HCI_OPCODE_LE_READ_PER_ADV_LIST_SIZE         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_PER_ADV_LIST_SIZE)
#define HCI_OPCODE_LE_READ_TX_POWER                  HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_TX_POWER)
#define HCI_OPCODE_LE_WRITE_RF_PATH_COMP             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_WRITE_RF_PATH_COMP)
#define HCI_OPCODE_LE_READ_RF_PATH_COMP              HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_RF_PATH_COMP)
#define HCI_OPCODE_LE_SET_PRIVACY_MODE               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_PRIVACY_MODE)
/* Version 5.1 */
#define HCI_OPCODE_LE_RECEIVER_TEST_V3               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_RECEIVER_TEST_V3)
#define HCI_OPCODE_LE_TRANSMITTER_TEST_V3            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_TRANSMITTER_TEST_V3)
#define HCI_OPCODE_LE_SET_CONNLESS_CTE_TX_PARAMS     HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_CONNLESS_CTE_TX_PARAMS)
#define HCI_OPCODE_LE_SET_CONNLESS_CTE_TX_ENABLE     HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_CONNLESS_CTE_TX_ENABLE)
#define HCI_OPCODE_LE_SET_CONNLESS_IQ_SAMP_ENABLE    HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_CONNLESS_IQ_SAMP_ENABLE)
#define HCI_OPCODE_LE_SET_CONN_CTE_RX_PARAMS         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_CONN_CTE_RX_PARAMS)
#define HCI_OPCODE_LE_SET_CONN_CTE_TX_PARAMS         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_CONN_CTE_TX_PARAMS)
#define HCI_OPCODE_LE_CONN_CTE_REQ_ENABLE            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CONN_CTE_REQ_ENABLE)
#define HCI_OPCODE_LE_CONN_CTE_RSP_ENABLE            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CONN_CTE_RSP_ENABLE)
#define HCI_OPCODE_LE_READ_ANTENNA_INFO              HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_ANTENNA_INFO)
#define HCI_OPCODE_LE_SET_PER_ADV_RCV_ENABLE         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_PER_ADV_RCV_ENABLE)
#define HCI_OPCODE_LE_PER_ADV_SYNC_TRANSFER          HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_PER_ADV_SYNC_TRANSFER)
#define HCI_OPCODE_LE_PER_ADV_SET_INFO_TRANSFER      HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_PER_ADV_SET_INFO_TRANSFER)
#define HCI_OPCODE_LE_SET_PAST_PARAM                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_PAST_PARAM)
#define HCI_OPCODE_LE_SET_DEFAULT_PAST_PARAM         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_DEFAULT_PAST_PARAM)
#define HCI_OPCODE_LE_GENERATE_DHKEY_V2              HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_GENERATE_DHKEY_V2)
#define HCI_OPCODE_LE_MODIFY_SLEEP_CLK_ACC           HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_MODIFY_SLEEP_CLK_ACC)
/* Version 5.2 */
#define HCI_OPCODE_LE_READ_BUF_SIZE_V2               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_BUF_SIZE_V2)
#define HCI_OPCODE_LE_READ_ISO_TX_SYNC               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_ISO_TX_SYNC)
#define HCI_OPCODE_LE_SET_CIG_PARAMS                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_CIG_PARAMS)
#define HCI_OPCODE_LE_SET_CIG_PARAMS_TEST            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_CIG_PARAMS_TEST)
#define HCI_OPCODE_LE_CREATE_CIS                     HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CREATE_CIS)
#define HCI_OPCODE_LE_REMOVE_CIG                     HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_REMOVE_CIG)
#define HCI_OPCODE_LE_ACCEPT_CIS_REQ                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ACCEPT_CIS_REQ)
#define HCI_OPCODE_LE_REJECT_CIS_REQ                 HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_REJECT_CIS_REQ)
#define HCI_OPCODE_LE_CREATE_BIG                     HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CREATE_BIG)
#define HCI_OPCODE_LE_CREATE_BIG_TEST                HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_CREATE_BIG_TEST)
#define HCI_OPCODE_LE_TERMINATE_BIG                  HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_TERMINATE_BIG)
#define HCI_OPCODE_LE_BIG_CREATE_SYNC                HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_BIG_CREATE_SYNC)
#define HCI_OPCODE_LE_BIG_TERMINATE_SYNC             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_BIG_TERMINATE_SYNC)
#define HCI_OPCODE_LE_REQUEST_PEER_SCA               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_REQUEST_PEER_SCA)
#define HCI_OPCODE_LE_SETUP_ISO_DATA_PATH            HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SETUP_ISO_DATA_PATH)
#define HCI_OPCODE_LE_REMOVE_ISO_DATA_PATH           HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_REMOVE_ISO_DATA_PATH)
#define HCI_OPCODE_LE_ISO_TX_TEST                    HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ISO_TX_TEST)
#define HCI_OPCODE_LE_ISO_RX_TEST                    HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ISO_RX_TEST)
#define HCI_OPCODE_LE_ISO_READ_TEST_COUNTERS         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ISO_READ_TEST_COUNTERS)
#define HCI_OPCODE_LE_ISO_TEST_END                   HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_ISO_TEST_END)
#define HCI_OPCODE_LE_SET_HOST_FEATURE               HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_HOST_FEATURE)
#define HCI_OPCODE_LE_READ_ISO_LINK_QUAL             HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_ISO_LINK_QUAL)
#define HCI_OPCODE_LE_READ_ENHANCED_TX_POWER         HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_ENHANCED_TX_POWER)
#define HCI_OPCODE_LE_READ_REMOTE_TX_POWER           HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_READ_REMOTE_TX_POWER)
#define HCI_OPCODE_LE_SET_PATH_LOSS_REPORTING_PARAMS HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_PATH_LOSS_REPORTING_PARAMS)
#define HCI_OPCODE_LE_SET_PATH_LOSS_REPORTING_ENABLE HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_PATH_LOSS_REPORTING_ENABLE)
#define HCI_OPCODE_LE_SET_TX_POWER_REPORT_ENABLE     HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_TX_POWER_REPORT_ENABLE)
/* Version Sydney */
#define HCI_OPCODE_LE_SET_DEF_SUBRATE                HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SET_DEF_SUBRATE)
#define HCI_OPCODE_LE_SUBRATE_REQ                    HCI_OPCODE(HCI_OGF_LE_CONTROLLER, HCI_OCF_LE_SUBRATE_REQ)

/**@}*/

/** \name Packetcraft Vendor Specific
 *
 */
/**@{*/
#define HCI_OPCODE_LE_VS_ENABLE_READ_FEAT_ON_CONN    ((uint16_t)(0xfff3))
/**@}*/

/** \name Command parameter lengths
 *
 */
/**@{*/
#define HCI_LEN_NOP                                  0

#define HCI_LEN_DISCONNECT                           3
#define HCI_LEN_READ_REMOTE_VER_INFO                 2

#define HCI_LEN_SET_EVENT_MASK                       8
#define HCI_LEN_SET_EVENT_MASK_PAGE2                 8
#define HCI_LEN_RESET                                0
#define HCI_LEN_READ_TX_PWR_LVL                      3
#define HCI_LEN_SET_CONTROLLER_TO_HOST_FC            1
#define HCI_LEN_HOST_BUFFER_SIZE                     8
#define HCI_LEN_HOST_NUM_CMPL_PKTS                   1
#define HCI_LEN_CONFIG_DATA_PATH(cLen)               (3 + (cLen))

#define HCI_LEN_READ_LOCAL_VER_INFO                  0
#define HCI_LEN_READ_LOCAL_SUP_CMDS                  0
#define HCI_LEN_READ_LOCAL_SUP_FEAT                  0
#define HCI_LEN_READ_BUF_SIZE                        0
#define HCI_LEN_READ_BD_ADDR                         0
#define HCI_LEN_READ_LOCAL_SUP_CODECS                0
#define HCI_LEN_READ_LOCAL_SUP_CODEC_CAP             7
#define HCI_LEN_READ_LOCAL_SUP_CONTROLLER_DLY(ccLen) (8 + (ccLen))

#define HCI_LEN_READ_RSSI                            2
#define HCI_LEN_READ_AUTH_PAYLOAD_TO                 2
#define HCI_LEN_WRITE_AUTH_PAYLOAD_TO                4

#define HCI_LEN_LE_SET_EVENT_MASK                    8
#define HCI_LEN_LE_READ_BUF_SIZE                     0
#define HCI_LEN_LE_READ_LOCAL_SUP_FEAT               0
#define HCI_LEN_LE_SET_RAND_ADDR                     6
#define HCI_LEN_LE_SET_ADV_PARAM                     15
#define HCI_LEN_LE_READ_ADV_TX_POWER                 0
#define HCI_LEN_LE_SET_ADV_DATA                      32
#define HCI_LEN_LE_SET_SCAN_RESP_DATA                32
#define HCI_LEN_LE_SET_ADV_ENABLE                    1
#define HCI_LEN_LE_SET_SCAN_PARAM                    7
#define HCI_LEN_LE_SET_SCAN_ENABLE                   2
#define HCI_LEN_LE_CREATE_CONN                       25
#define HCI_LEN_LE_CREATE_CONN_CANCEL                0
#define HCI_LEN_LE_READ_WHITE_LIST_SIZE              0
#define HCI_LEN_LE_CLEAR_WHITE_LIST                  0
#define HCI_LEN_LE_ADD_DEV_WHITE_LIST                7
#define HCI_LEN_LE_REMOVE_DEV_WHITE_LIST             7
#define HCI_LEN_LE_CONN_UPDATE                       14
#define HCI_LEN_LE_SET_HOST_CHAN_CLASS               5
#define HCI_LEN_LE_READ_CHAN_MAP                     2
#define HCI_LEN_LE_READ_REMOTE_FEAT                  2
#define HCI_LEN_LE_ENCRYPT                           32
#define HCI_LEN_LE_RAND                              0
#define HCI_LEN_LE_START_ENCRYPTION                  28
#define HCI_LEN_LE_LTK_REQ_REPL                      18
#define HCI_LEN_LE_LTK_REQ_NEG_REPL                  2
#define HCI_LEN_LE_READ_SUP_STATES                   0
#define HCI_LEN_LE_RECEIVER_TEST                     1
#define HCI_LEN_LE_TRANSMITTER_TEST                  3
#define HCI_LEN_LE_TEST_END                          0
/* Version 4.1 */
#define HCI_LEN_LE_REM_CONN_PARAM_REP                14
#define HCI_LEN_LE_REM_CONN_PARAM_NEG_REP            3
/* Version 4.2 */
#define HCI_LEN_LE_SET_DATA_LEN                      6
#define HCI_LEN_LE_READ_DEF_DATA_LEN                 0
#define HCI_LEN_LE_WRITE_DEF_DATA_LEN                4
#define HCI_LEN_LE_READ_LOCAL_P256_PUB_KEY           0
#define HCI_LEN_LE_GENERATE_DHKEY                    64
#define HCI_LEN_LE_ADD_DEV_RES_LIST                  39
#define HCI_LEN_LE_REMOVE_DEV_RES_LIST               7
#define HCI_LEN_LE_CLEAR_RES_LIST                    0
#define HCI_LEN_LE_READ_RES_LIST_SIZE                0
#define HCI_LEN_LE_READ_PEER_RES_ADDR                7
#define HCI_LEN_LE_READ_LOCAL_RES_ADDR               7
#define HCI_LEN_LE_SET_ADDR_RES_ENABLE               1
#define HCI_LEN_LE_SET_RES_PRIV_ADDR_TO              2
#define HCI_LEN_LE_READ_MAX_DATA_LEN                 0
/* Version 5.0 */
#define HCI_LEN_LE_READ_PHY                          2
#define HCI_LEN_LE_SET_DEF_PHY                       3
#define HCI_LEN_LE_SET_PHY                           7
#define HCI_LEN_LE_ENHANCED_RECEIVER_TEST            3
#define HCI_LEN_LE_ENHANCED_TRANSMITTER_TEST         4
#define HCI_LEN_LE_SET_ADV_SET_RAND_ADDR             7
#define HCI_LEN_LE_SET_EXT_ADV_PARAM                 25
#define HCI_LEN_LE_SET_EXT_ADV_DATA(len)             (4 + (len))
#define HCI_LEN_LE_SET_EXT_SCAN_RESP_DATA(len)       (4 + (len))
#define HCI_LEN_LE_EXT_ADV_ENABLE(numSets)           (2 + (4 * (numSets)))
#define HCI_LEN_LE_READ_MAX_ADV_DATA_LEN             0
#define HCI_LEN_LE_READ_NUM_OF_SUP_ADV_SETS          0
#define HCI_LEN_LE_REMOVE_ADV_SET                    1
#define HCI_LEN_LE_CLEAR_ADV_SETS                    0
#define HCI_LEN_LE_SET_PER_ADV_PARAM                 7
#define HCI_LEN_LE_SET_PER_ADV_DATA(len)             (3 + (len))
#define HCI_LEN_LE_SET_PER_ADV_ENABLE                2
#define HCI_LEN_LE_SET_EXT_SCAN_PARAM(numPhys)       (3 + (5 * (numPhys)))
#define HCI_LEN_LE_SET_EXT_SCAN_ENABLE               6
#define HCI_LEN_LE_EXT_CREATE_CONN(numPhys)          (10 + (16 * (numPhys)))
#define HCI_LEN_LE_PER_ADV_CREATE_SYNC               14
#define HCI_LEN_LE_PER_ADV_CREATE_SYNC_CANCEL        0
#define HCI_LEN_LE_PER_ADV_TERMINATE_SYNC            2
#define HCI_LEN_LE_ADD_DEV_PER_ADV_LIST              8
#define HCI_LEN_LE_REMOVE_DEV_PER_ADV_LIST           8
#define HCI_LEN_LE_CLEAR_PER_ADV_LIST                0
#define HCI_LEN_LE_READ_PER_ADV_LIST_SIZE            0
#define HCI_LEN_LE_READ_TX_POWER                     0
#define HCI_LEN_LE_READ_RF_PATH_COMP                 0
#define HCI_LEN_LE_WRITE_RF_PATH_COMP                4
#define HCI_LEN_LE_SET_PRIVACY_MODE                  8
/* Version 5.1 */
#define HCI_LEN_LE_SET_CONN_CTE_RX_PARAMS(spLen)     (5 + (spLen))
#define HCI_LEN_LE_SET_CONN_CTE_TX_PARAMS(spLen)     (4 + (spLen))
#define HCI_LEN_LE_CONN_CTE_REQ_ENABLE               7
#define HCI_LEN_LE_CONN_CTE_RSP_ENABLE               3
#define HCI_LEN_LE_READ_ANTENNA_INFO                 0
#define HCI_LEN_LE_SET_PER_ADV_RCV_ENABLE            3
#define HCI_LEN_LE_PER_ADV_SYNC_TRANSFER             6
#define HCI_LEN_LE_PER_ADV_SET_INFO_TRANSFER         5
#define HCI_LEN_LE_SET_PAST_PARAM                    8
#define HCI_LEN_LE_SET_DEFAULT_PAST_PARAM            6
#define HCI_LEN_LE_GENERATE_DHKEY_V2                 65
/* Version 5.2 */
#define HCI_LEN_LE_SET_CIG_PARAMS(numCis)            (15 + (9 * (numCis)))
#define HCI_LEN_LE_CREATE_CIS(numCis)                (1 + (4 * (numCis)))
#define HCI_LEN_LE_REMOVE_CIG                        1
#define HCI_LEN_LE_ACCEPT_CIS_REQ                    2
#define HCI_LEN_LE_REJECT_CIS_REQ                    3
#define HCI_LEN_LE_REQUEST_PEER_SCA                  2
#define HCI_LEN_LE_CREATE_BIS                        (15 + HCI_BC_LEN)
#define HCI_LEN_LE_TERMINATE_BIG                     2
#define HCI_LEN_LE_BIG_CREATE_SYNC(numBis)           (8 + HCI_BC_LEN + (numBis))
#define HCI_LEN_LE_BIG_TERMINATE_SYNC                1
#define HCI_LEN_LE_SETUP_ISO_DATA_PATH(ccLen)        (13 + (ccLen))
#define HCI_LEN_LE_REMOVE_ISO_DATA_PATH              3
#define HCI_LEN_LE_ISO_TX_TEST                       3
#define HCI_LEN_LE_ISO_RX_TEST                       3
#define HCI_LEN_LE_ISO_READ_TEST_COUNTERS            2
#define HCI_LEN_LE_ISO_TEST_END                      2
#define HCI_LEN_LE_SET_HOST_FEATURE                  2
/* Version Sydney */
#define HCI_LEN_LE_SET_DEF_SUBRATE                   1
/**@}*/

/** \name Events
 *
 */
/**@{*/
#define HCI_DISCONNECT_CMPL_EVT                      0x05
#define HCI_ENC_CHANGE_EVT                           0x08
#define HCI_READ_REMOTE_VER_INFO_CMPL_EVT            0x0C
#define HCI_CMD_CMPL_EVT                             0x0E
#define HCI_CMD_STATUS_EVT                           0x0F
#define HCI_HW_ERROR_EVT                             0x10
#define HCI_NUM_CMPL_PKTS_EVT                        0x13
#define HCI_DATA_BUF_OVERFLOW_EVT                    0x1A
#define HCI_ENC_KEY_REFRESH_CMPL_EVT                 0x30
#define HCI_LE_META_EVT                              0x3E
#define HCI_AUTH_PAYLOAD_TIMEOUT_EVT                 0x57
#define HCI_ENC_CHANGE_V2_EVT                        0x59
#define HCI_VENDOR_SPEC_EVT                          0xFF
/**@}*/

/** \name LE Subevents
 *
 */
/**@{*/
#define HCI_LE_CONN_CMPL_EVT                         0x01
#define HCI_LE_ADV_REPORT_EVT                        0x02
#define HCI_LE_CONN_UPDATE_CMPL_EVT                  0x03
#define HCI_LE_READ_REMOTE_FEAT_CMPL_EVT             0x04
#define HCI_LE_LTK_REQ_EVT                           0x05
/* Version 4.1 */
#define HCI_LE_REM_CONN_PARAM_REQ_EVT                0x06
/* Version 4.2 */
#define HCI_LE_DATA_LEN_CHANGE_EVT                   0x07
#define HCI_LE_READ_LOCAL_P256_PUB_KEY_CMPL_EVT      0x08
#define HCI_LE_GENERATE_DHKEY_CMPL_EVT               0x09
#define HCI_LE_ENHANCED_CONN_CMPL_EVT                0x0A
#define HCI_LE_DIRECT_ADV_REPORT_EVT                 0x0B
/* Version 5.0 */
#define HCI_LE_PHY_UPDATE_CMPL_EVT                   0x0C
#define HCI_LE_EXT_ADV_REPORT_EVT                    0x0D
#define HCI_LE_PER_ADV_SYNC_EST_EVT                  0x0E
#define HCI_LE_PER_ADV_REPORT_EVT                    0x0F
#define HCI_LE_PER_ADV_SYNC_LOST_EVT                 0x10
#define HCI_LE_SCAN_TIMEOUT_EVT                      0x11
#define HCI_LE_ADV_SET_TERM_EVT                      0x12
#define HCI_LE_SCAN_REQ_RCVD_EVT                     0x13
#define HCI_LE_CH_SEL_ALGO_EVT                       0x14
/* Version 5.1 */
#define HCI_LE_CONNLESS_IQ_REPORT_EVT                0x15
#define HCI_LE_CONN_IQ_REPORT_EVT                    0x16
#define HCI_LE_CTE_REQ_FAILED_EVT                    0x17
#define HCI_LE_PER_SYNC_TRSF_RCVD_EVT                0x18
/* Version 5.2 */
#define HCI_LE_CIS_EST_EVT                           0x19
#define HCI_LE_CIS_REQ_EVT                           0x1A
#define HCI_LE_CREATE_BIG_CMPL_EVT                   0x1B
#define HCI_LE_TERMINATE_BIG_CMPL_EVT                0x1C
#define HCI_LE_BIG_SYNC_EST_EVT                      0x1D
#define HCI_LE_BIG_SYNC_LOST_EVT                     0x1E
#define HCI_LE_REQ_PEER_SCA_CMPLT_EVT                0x1F
#define HCI_LE_PATH_LOSS_REPORT_EVT                  0x20
#define HCI_LE_POWER_REPORT_EVT                      0x21
#define HCI_LE_BIG_INFO_ADV_REPORT_EVT               0x22
/* Version Sydney */
#define HCI_LE_SUBRATE_CHANGE_EVT                    0x23
/**@}*/

/** \name Event parameter lengths
 *
 */
/**@{*/
#define HCI_LEN_DISCONNECT_CMPL                      4       /*!< Disconnect event length. */
#define HCI_LEN_READ_REMOTE_VER_INFO_CMPL            8       /*!< Read remove version info complete event length. */
#define HCI_LEN_CMD_CMPL                             3       /*!< Command complete event length. */
#define HCI_LEN_CMD_STATUS                           4       /*!< Command status event length. */
#define HCI_LEN_HW_ERR                               1       /*!< Hardware error event length. */
#define HCI_LEN_NUM_CMPL_PKTS(numHdls)               (1 + (4 * numHdls))    /*!< Number of completed packets event length. */
#define HCI_LEN_ENC_CHANGE                           4       /*!< Encryption change event length. */
#define HCI_LEN_ENC_KEY_REFRESH_CMPL                 3       /*!< Encryption key refresh complete event length. */
#define HCI_LEN_LE_CONN_CMPL                         19      /*!< Connection complete event length. */
#define HCI_LEN_LE_ADV_RPT_MIN                       12      /*!< Advertising report event minimum length. */
#define HCI_LEN_LE_CONN_UPDATE_CMPL                  10      /*!< Connection update complete event length. */
#define HCI_LEN_LE_READ_REMOTE_FEAT_CMPL             12      /*!< Read remote feature event length. */
#define HCI_LEN_LE_LTK_REQ                           13      /*!< LTK request event length. */
/* Version 4.1 */
#define HCI_LEN_LE_REM_CONN_PARAM_REQ                11      /*!< Remote connection parameter event length. */
#define HCI_LEN_LE_DATA_LEN_CHANGE                   11      /*!< Data length change event length. */
#define HCI_LEN_LE_READ_PUB_KEY_CMPL                 66      /*!< Read local P256 public key compete event length. */
#define HCI_LEN_LE_GEN_DHKEY_CMPL                    34      /*!< Generate DH key complete event length. */
#define HCI_LEN_LE_ENHANCED_CONN_CMPL                31      /*!< Enhanced connection complete event length. */
#define HCI_LEN_LE_DIRECT_ADV_REPORT                 18      /*!< Direct advertising report event length. */
#define HCI_LEN_AUTH_PAYLOAD_TIMEOUT                 2       /*!< Authenticated payload timeout event length. */
/* Version 5.0 */
#define HCI_LEN_LE_PHY_UPDATE_CMPL                   6       /*!< PHY update complete event length. */
#define HCI_LEN_LE_CH_SEL_ALGO                       4       /*!< Channel selection algorithm event length. */
#define HCI_LEN_LE_PHY_UPDATE_CMPL                   6       /*!< PHY update complete event length. */
#define HCI_LEN_LE_EXT_ADV_REPORT_MIN                26      /*!< Extended advertising report minimum length. */
#define HCI_LEN_LE_PER_ADV_SYNC_EST                  16      /*!< Periodic advertising sync established event length. */
#define HCI_LEN_LE_PER_ADV_REPORT                    8       /*!< Periodic advertising report event length. */
#define HCI_LEN_LE_PER_ADV_SYNC_LOST                 3       /*!< Periodic advertising sync lost event length. */
#define HCI_LEN_LE_SCAN_TIMEOUT                      1       /*!< Scan timeout event length. */
#define HCI_LEN_LE_ADV_SET_TERM                      6       /*!< Advertising set terminated event length. */
#define HCI_LEN_LE_SCAN_REQ_RCVD                     9       /*!< Scan request received event length. */
/* Version 5.1 */
#define HCI_LEN_LE_PER_SYNC_TRSF_RCVT                20      /*!< Periodic advertising sync transfer received event length. */
/* Version 5.2 */
#define HCI_LEN_LE_CIS_EST                           29      /*!< CIS established event length. */
#define HCI_LEN_LE_CIS_REQ                           7       /*!< CIS request event length. */
#define HCI_LEN_LE_PEER_SCA_CMPL                     5       /*!< Request peer SCA complete event length. */
#define HCI_LEN_LE_CREATE_BIG_CMPL(numBis)           (19 + (2 * numBis))    /*!< Create BIG complete event length. */
#define HCI_LEN_LE_TERMINATE_BIG_CMPL                3       /*!< Terminate BIG complete event length. */
#define HCI_LEN_LE_BIG_SYNC_EST(numBis)              (15 + (2 * numBis))    /*!< BIG sync established event length. */
#define HCI_LEN_LE_BIG_SYNC_LOST                     3       /*!< BIG sync lost event length. */
#define HCI_LEN_LE_POWER_REPORT                      9       /*!< Power reporting event length. */
#define HCI_LEN_LE_PATH_LOSS_ZONE                    5       /*!< Path loss reporting event length. */
#define HCI_LEN_LE_BIG_INFO_ADV_REPORT               20      /*!< BIG Info advertising report length. */
/* Version Sydney */
#define HCI_LEN_ENC_CHANGE_V2                        5       /*!< Encryption change (V2) event length. */
#define HCI_LEN_LE_SUBRATE_CHANGE                    12      /*!< Subrate change event length. */
/**@}*/

/** \name Supported commands
 *
 */
/**@{*/
#define HCI_SUP_DISCONNECT                           0x20    /*!< Byte 0 */
#define HCI_SUP_READ_REMOTE_VER_INFO                 0x80    /*!< Byte 2 */
#define HCI_SUP_SET_EVENT_MASK                       0x40    /*!< Byte 5 */
#define HCI_SUP_RESET                                0x80    /*!< Byte 5 */
#define HCI_SUP_READ_TX_PWR_LVL                      0x04    /*!< Byte 10 */
#define HCI_SUP_READ_LOCAL_VER_INFO                  0x08    /*!< Byte 14 */
#define HCI_SUP_READ_LOCAL_SUP_FEAT                  0x20    /*!< Byte 14 */
#define HCI_SUP_READ_BD_ADDR                         0x02    /*!< Byte 15 */
#define HCI_SUP_READ_RSSI                            0x20    /*!< Byte 15 */
#define HCI_SUP_SET_EVENT_MASK_PAGE2                 0x04    /*!< Byte 22 */
#define HCI_SUP_LE_SET_EVENT_MASK                    0x01    /*!< Byte 25 */
#define HCI_SUP_LE_READ_BUF_SIZE                     0x02    /*!< Byte 25 */
#define HCI_SUP_LE_READ_LOCAL_SUP_FEAT               0x04    /*!< Byte 25 */
#define HCI_SUP_LE_SET_RAND_ADDR                     0x10    /*!< Byte 25 */
#define HCI_SUP_LE_SET_ADV_PARAM                     0x20    /*!< Byte 25 */
#define HCI_SUP_LE_READ_ADV_TX_POWER                 0x40    /*!< Byte 25 */
#define HCI_SUP_LE_SET_ADV_DATA                      0x80    /*!< Byte 25 */
#define HCI_SUP_LE_SET_SCAN_RESP_DATA                0x01    /*!< Byte 26 */
#define HCI_SUP_LE_SET_ADV_ENABLE                    0x02    /*!< Byte 26 */
#define HCI_SUP_LE_SET_SCAN_PARAM                    0x04    /*!< Byte 26 */
#define HCI_SUP_LE_SET_SCAN_ENABLE                   0x08    /*!< Byte 26 */
#define HCI_SUP_LE_CREATE_CONN                       0x10    /*!< Byte 26 */
#define HCI_SUP_LE_CREATE_CONN_CANCEL                0x20    /*!< Byte 26 */
#define HCI_SUP_LE_READ_WHITE_LIST_SIZE              0x40    /*!< Byte 26 */
#define HCI_SUP_LE_CLEAR_WHITE_LIST                  0x80    /*!< Byte 26 */
#define HCI_SUP_LE_ADD_DEV_WHITE_LIST                0x01    /*!< Byte 27 */
#define HCI_SUP_LE_REMOVE_DEV_WHITE_LIST             0x02    /*!< Byte 27 */
#define HCI_SUP_LE_CONN_UPDATE                       0x04    /*!< Byte 27 */
#define HCI_SUP_LE_SET_HOST_CHAN_CLASS               0x08    /*!< Byte 27 */
#define HCI_SUP_LE_READ_CHAN_MAP                     0x10    /*!< Byte 27 */
#define HCI_SUP_LE_READ_REMOTE_FEAT                  0x20    /*!< Byte 27 */
#define HCI_SUP_LE_ENCRYPT                           0x40    /*!< Byte 27 */
#define HCI_SUP_LE_RAND                              0x80    /*!< Byte 27 */
#define HCI_SUP_LE_START_ENCRYPTION                  0x01    /*!< Byte 28 */
#define HCI_SUP_LE_LTK_REQ_REPL                      0x02    /*!< Byte 28 */
#define HCI_SUP_LE_LTK_REQ_NEG_REPL                  0x04    /*!< Byte 28 */
#define HCI_SUP_LE_READ_SUP_STATES                   0x08    /*!< Byte 28 */
#define HCI_SUP_LE_RECEIVER_TEST                     0x10    /*!< Byte 28 */
#define HCI_SUP_LE_TRANSMITTER_TEST                  0x20    /*!< Byte 28 */
#define HCI_SUP_LE_TEST_END                          0x40    /*!< Byte 28 */
#define HCI_SUP_READ_AUTH_PAYLOAD_TO                 0x10    /*!< Byte 32 */
#define HCI_SUP_WRITE_AUTH_PAYLOAD_TO                0x20    /*!< Byte 32 */
/* Version 4.1 */
#define HCI_SUP_LE_REM_CONN_PARAM_REQ_REPL           0x10    /*!< Byte 33 */
#define HCI_SUP_LE_REM_CONN_PARAM_REQ_NEG_REPL       0x20    /*!< Byte 33 */
/* Version 4.2 */
#define HCI_SUP_LE_SET_DATA_LEN                      0x40    /*!< Byte 33 */
#define HCI_SUP_LE_READ_DEF_DATA_LEN                 0x80    /*!< Byte 33 */
#define HCI_SUP_LE_WRITE_DEF_DATA_LEN                0x01    /*!< Byte 34 */
#define HCI_SUP_LE_READ_LOCAL_P256_PUB_KEY           0x02    /*!< Byte 34 */
#define HCI_SUP_LE_GENERATE_DHKEY                    0x04    /*!< Byte 34 */
#define HCI_SUP_LE_ADD_DEV_RES_LIST_EVT              0x08    /*!< Byte 34 */
#define HCI_SUP_LE_REMOVE_DEV_RES_LIST               0x10    /*!< Byte 34 */
#define HCI_SUP_LE_CLEAR_RES_LIST                    0x20    /*!< Byte 34 */
#define HCI_SUP_LE_READ_RES_LIST_SIZE                0x40    /*!< Byte 34 */
#define HCI_SUP_LE_READ_PEER_RES_ADDR                0x80    /*!< Byte 34 */
#define HCI_SUP_LE_READ_LOCAL_RES_ADDR               0x01    /*!< Byte 35 */
#define HCI_SUP_LE_SET_ADDR_RES_ENABLE               0x02    /*!< Byte 35 */
#define HCI_SUP_LE_SET_RES_PRIV_ADDR_TO              0x04    /*!< Byte 35 */
#define HCI_SUP_LE_READ_MAX_DATA_LEN                 0x08    /*!< Byte 35 */
/* Version 5.0 */
#define HCI_SUP_LE_READ_PHY                          0x10    /*!< Byte 35 */
#define HCI_SUP_LE_SET_DEF_PHY                       0x20    /*!< Byte 35 */
#define HCI_SUP_LE_SET_PHY                           0x40    /*!< Byte 35 */
#define HCI_SUP_LE_ENHANCED_RECEIVER_TEST            0x80    /*!< Byte 35 */
#define HCI_SUP_LE_ENHANCED_TRANSMITTER_TEST         0x01    /*!< Byte 36 */
#define HCI_SUP_LE_SET_ADV_SET_RAND_ADDR             0x02    /*!< Byte 36 */
#define HCI_SUP_LE_SET_EXT_ADV_PARAM                 0x04    /*!< Byte 36 */
#define HCI_SUP_LE_SET_EXT_ADV_DATA                  0x08    /*!< Byte 36 */
#define HCI_SUP_LE_SET_EXT_SCAN_RESP_DATA            0x10    /*!< Byte 36 */
#define HCI_SUP_LE_SET_EXT_ADV_ENABLE                0x20    /*!< Byte 36 */
#define HCI_SUP_LE_READ_MAX_ADV_DATA_LEN             0x40    /*!< Byte 36 */
#define HCI_SUP_LE_READ_NUM_OF_SUP_ADV_SETS          0x80    /*!< Byte 36 */
#define HCI_SUP_LE_REMOVE_ADV_SET                    0x01    /*!< Byte 37 */
#define HCI_SUP_LE_CLEAR_ADV_SETS                    0x02    /*!< Byte 37 */
#define HCI_SUP_LE_SET_PER_ADV_PARAM                 0x04    /*!< Byte 37 */
#define HCI_SUP_LE_SET_PER_ADV_DATA                  0x08    /*!< Byte 37 */
#define HCI_SUP_LE_SET_PER_ADV_ENABLE                0x10    /*!< Byte 37 */
#define HCI_SUP_LE_SET_EXT_SCAN_PARAM                0x20    /*!< Byte 37 */
#define HCI_SUP_LE_SET_EXT_SCAN_ENABLE               0x40    /*!< Byte 37 */
#define HCI_SUP_LE_EXT_CREATE_CONN                   0x80    /*!< Byte 37 */
#define HCI_SUP_LE_PER_ADV_CREATE_SYNC               0x01    /*!< Byte 38 */
#define HCI_SUP_LE_PER_ADV_CREATE_SYNC_CANCEL        0x02    /*!< Byte 38 */
#define HCI_SUP_LE_PER_ADV_TERMINATE_SYNC            0x04    /*!< Byte 38 */
#define HCI_SUP_LE_ADD_DEV_PER_ADV_LIST              0x08    /*!< Byte 38 */
#define HCI_SUP_LE_REMOVE_DEV_PER_ADV_LIST           0x10    /*!< Byte 38 */
#define HCI_SUP_LE_CLEAR_PER_ADV_LIST                0x20    /*!< Byte 38 */
#define HCI_SUP_LE_READ_PER_ADV_LIST_SIZE            0x40    /*!< Byte 38 */
#define HCI_SUP_LE_READ_TX_POWER                     0x80    /*!< Byte 38 */
#define HCI_SUP_LE_READ_RF_PATH_COMP                 0x01    /*!< Byte 39 */
#define HCI_SUP_LE_WRITE_RF_PATH_COMP                0x02    /*!< Byte 39 */
#define HCI_SUP_LE_SET_PRIVACY_MODE                  0x04    /*!< Byte 39 */
/* Version 5.1 */
#define HCI_SUP_LE_RECEIVER_TEST_V3                  0x08    /*!< Byte 39 */
#define HCI_SUP_LE_TRANSMITTER_TEST_V3               0x10    /*!< Byte 39 */
#define HCI_SUP_LE_SET_CONNLESS_CTE_TX_PARAMS        0x20    /*!< Byte 39 */
#define HCI_SUP_LE_SET_CONNLESS_CTE_TX_ENABLE        0x40    /*!< Byte 39 */
#define HCI_SUP_LE_SET_CONNLESS_IQ_SAMP_ENABLE       0x80    /*!< Byte 39 */
#define HCI_SUP_LE_SET_CONN_CTE_RX_PARAMS            0x01    /*!< Byte 40 */
#define HCI_SUP_LE_SET_CONN_CTE_TX_PARAMS            0x02    /*!< Byte 40 */
#define HCI_SUP_LE_CONN_CTE_REQ_ENABLE               0x04    /*!< Byte 40 */
#define HCI_SUP_LE_CONN_CTE_RSP_ENABLE               0x08    /*!< Byte 40 */
#define HCI_SUP_LE_READ_ANTENNA_INFO                 0x10    /*!< Byte 40 */
#define HCI_SUP_LE_SET_PER_ADV_RCV_ENABLE            0x20    /*!< Byte 40 */
#define HCI_SUP_LE_PER_ADV_SYNC_TRANSFER             0x40    /*!< Byte 40 */
#define HCI_SUP_LE_PER_ADV_SET_INFO_TRANSFER         0x80    /*!< Byte 40 */
#define HCI_SUP_LE_SET_PAST_PARAM                    0x01    /*!< Byte 41 */
#define HCI_SUP_LE_SET_DEFAULT_PAST_PARAM            0x02    /*!< Byte 41 */
#define HCI_SUP_LE_GENERATE_DHKEY_V2                 0x04    /*!< Byte 41 */
#define HCI_SUP_LE_MODIFY_SLEEP_CLK_ACCURACY         0x10    /*!< Byte 41 */
/* Version 5.2 */
#define HCI_SUP_LE_READ_BUF_SIZE_V2                  0x20    /*!< Byte 41 */
#define HCI_SUP_LE_READ_ISO_TX_SYNC                  0x40    /*!< Byte 41 */
#define HCI_SUP_LE_SET_CIG_PARAM                     0x80    /*!< Byte 41 */
#define HCI_SUP_LE_SET_CIG_PARAM_TEST                0x01    /*!< Byte 42 */
#define HCI_SUP_LE_CREATE_CIS                        0x02    /*!< Byte 42 */
#define HCI_SUP_LE_REMOVE_CIG                        0x04    /*!< Byte 42 */
#define HCI_SUP_LE_ACCEPT_CIS_REQ                    0x08    /*!< Byte 42 */
#define HCI_SUP_LE_REJECT_CIS_REQ                    0x10    /*!< Byte 42 */
#define HCI_SUP_LE_CREATE_BIG                        0x20    /*!< Byte 42 */
#define HCI_SUP_LE_CREATE_BIG_TEST                   0x40    /*!< Byte 42 */
#define HCI_SUP_LE_TERMINATE_BIG                     0x80    /*!< Byte 42 */
#define HCI_SUP_LE_BIG_CREATE_SYNC                   0x01    /*!< Byte 43 */
#define HCI_SUP_LE_BIG_TERMINATE_SYNC                0x02    /*!< Byte 43 */
#define HCI_SUP_LE_REQ_PEER_SCA                      0x04    /*!< Byte 43 */
#define HCI_SUP_LE_SETUP_ISO_DATA_PATH               0x08    /*!< Byte 43 */
#define HCI_SUP_LE_REMOVE_ISO_DATA_PATH              0x10    /*!< Byte 43 */
#define HCI_SUP_LE_ISO_TRANSMIT_TEST                 0x20    /*!< Byte 43 */
#define HCI_SUP_LE_ISO_RECEIVE_TEST                  0x40    /*!< Byte 43 */
#define HCI_SUP_LE_ISO_READ_TEST_COUNTERS            0x80    /*!< Byte 43 */
#define HCI_SUP_LE_ISO_TEST_END                      0x01    /*!< Byte 44 */
#define HCI_SUP_LE_SET_HOST_FEATURE                  0x02    /*!< Byte 44 */
#define HCI_SUP_LE_READ_ISO_LINK_QUALITY             0x04    /*!< Byte 44 */
#define HCI_SUP_LE_ENH_READ_TX_POWER_LEVEL           0x08    /*!< Byte 44 */
#define HCI_SUP_LE_READ_REMOTE_TX_POWER_LEVEL        0x10    /*!< Byte 44 */
#define HCI_SUP_LE_SET_PATH_LOSS_REPORT_PARAM        0x20    /*!< Byte 44 */
#define HCI_SUP_LE_SET_PATH_LOSS_REPORT_ENABLE       0x40    /*!< Byte 44 */
#define HCI_SUP_LE_SET_TX_POWER_REPORT_ENABLE        0x80    /*!< Byte 44 */
#define HCI_SUP_LE_TRANSMITTER_TEST_V4               0x01    /*!< Byte 45 */
#define HCI_SUP_READ_LOCAL_SUP_CODECS_V2             0x02    /*!< Byte 45 */
#define HCI_SUP_READ_LOCAL_SUP_CODEC_CAP             0x04    /*!< Byte 45 */
#define HCI_SUP_READ_LOCAL_SUP_CTR_DLY               0x08    /*!< Byte 45 */
#define HCI_SUP_CONFIG_DATA_PATH                     0x10    /*!< Byte 45 */
/* Version Sydney */
#define HCI_SUP_LE_SET_DEF_SUBRATE                   0x01    /*!< Byte 46 */
#define HCI_SUP_LE_SET_SUBRATE_REQ                   0x02    /*!< Byte 46 */

#define HCI_SUP_CMD_LEN                              64      /*!< Byte length of support cmd field. */
/**@}*/

/** \name Event mask
 *
 */
/**@{*/
#define HCI_EVT_MASK_DISCONNECT_CMPL                 0x10    /*!< Byte 0 */
#define HCI_EVT_MASK_ENC_CHANGE                      0x80    /*!< Byte 0 */
#define HCI_EVT_MASK_READ_REMOTE_VER_INFO_CMPL       0x08    /*!< Byte 1 */
#define HCI_EVT_MASK_HW_ERROR                        0x80    /*!< Byte 1 */
#define HCI_EVT_MASK_DATA_BUF_OVERFLOW               0x02    /*!< Byte 3 */
#define HCI_EVT_MASK_ENC_KEY_REFRESH_CMPL            0x80    /*!< Byte 5 */
#define HCI_EVT_MASK_LE_META                         0x20    /*!< Byte 7 */
/**@}*/

/** \name Event mask page 2
 *
 */
/**@{*/
#define HCI_EVT_MASK_AUTH_PAYLOAD_TIMEOUT            0x80    /*!< Byte 2 */
#define HCI_EVT_MASK_ENC_CHANGE_V2                   0x02    /*!< Byte 3 */
/**@}*/

/** \name LE event mask
 *
 */
/**@{*/
#define HCI_EVT_MASK_LE_CONN_CMPL_EVT                0x01    /*!< Byte 0 */
#define HCI_EVT_MASK_LE_ADV_REPORT_EVT               0x02    /*!< Byte 0 */
#define HCI_EVT_MASK_LE_CONN_UPDATE_CMPL_EVT         0x04    /*!< Byte 0 */
#define HCI_EVT_MASK_LE_READ_REMOTE_FEAT_CMPL_EVT    0x08    /*!< Byte 0 */
#define HCI_EVT_MASK_LE_LTK_REQ_EVT                  0x10    /*!< Byte 0 */
/* Version 4.1 */
#define HCI_EVT_MASK_LE_REMOTE_CONN_PARAM_REQ_EVT    0x20    /*!< Byte 0 */
/* Version 4.2 */
#define HCI_EVT_MASK_LE_DATA_LEN_CHANGE_EVT          0x40    /*!< Byte 0 */
#define HCI_EVT_MASK_LE_READ_LOCAL_P256_PUB_KEY_CMPL 0x80    /*!< Byte 0 */
#define HCI_EVT_MASK_LE_GENERATE_DHKEY_CMPL          0x01    /*!< Byte 1 */
#define HCI_EVT_MASK_LE_ENHANCED_CONN_CMPL_EVT       0x02    /*!< Byte 1 */
#define HCI_EVT_MASK_LE_DIRECT_ADV_REPORT_EVT        0x04    /*!< Byte 1 */
/* Version 5.0 */
#define HCI_EVT_MASK_LE_PHY_UPDATE_CMPL_EVT          0x08    /*!< Byte 1 */
#define HCI_EVT_MASK_LE_EXT_ADV_REPORT_EVT           0x10    /*!< Byte 1 */
#define HCI_EVT_MASK_LE_PER_ADV_SYNC_EST_EVT         0x20    /*!< Byte 1 */
#define HCI_EVT_MASK_LE_PER_ADV_REPORT_EVT           0x40    /*!< Byte 1 */
#define HCI_EVT_MASK_LE_PER_ADV_SYNC_LOST_EVT        0x80    /*!< Byte 1 */
#define HCI_EVT_MASK_LE_SCAN_TIMEOUT_EVT             0x01    /*!< Byte 2 */
#define HCI_EVT_MASK_LE_ADV_SET_TERM_EVT             0x02    /*!< Byte 2 */
#define HCI_EVT_MASK_LE_SCAN_REQ_RCVD_EVT            0x04    /*!< Byte 2 */
#define HCI_EVT_MASK_LE_CH_SEL_ALGO_EVT              0x08    /*!< Byte 2 (Bit 19) */
/* Version 5.1 */
#define HCI_EVT_MASK_LE_CONNLESS_IQ_REPORT_EVT       0x10    /*!< Byte 2 */
#define HCI_EVT_MASK_LE_CONN_IQ_REPORT_EVT           0x20    /*!< Byte 2 */
#define HCI_EVT_MASK_LE_CTE_REQ_FAILED_EVT           0x40    /*!< Byte 2 */
#define HCI_EVT_MASK_LE_PER_SYNC_TRSF_RCVT_EVT       0x80    /*!< Byte 2 (Bit 23) */
/* Version 5.2 */
#define HCI_EVT_MASK_LE_CIS_EST_EVT                  0x01    /*!< Byte 3 (Bit 24) */
#define HCI_EVT_MASK_LE_CIS_REQ_EVT                  0x02    /*!< Byte 3 */
#define HCI_EVT_MASK_LE_CREATE_BIG_CMPL_EVT          0x04    /*!< Byte 3 */
#define HCI_EVT_MASK_LE_TERMINATE_BIG_CMPL_EVT       0x08    /*!< Byte 3 */
#define HCI_EVT_MASK_LE_BIG_SYNC_EST_EVT             0x10    /*!< Byte 3 */
#define HCI_EVT_MASK_LE_BIG_SYNC_LOST_EVT            0x20    /*!< Byte 3 */
#define HCI_EVT_MASK_LE_PEER_SCA_CMPL_EVT            0x40    /*!< Byte 3 */
#define HCI_EVT_MASK_LE_PATH_LOSS_REPORT_EVT         0x80    /*!< Byte 3 */
#define HCI_EVT_MASK_LE_TX_POWER_REPORT_EVT          0x01    /*!< Byte 4 (Bit 32)*/
#define HCI_EVT_MASK_LE_BIG_INFO_ADV_RPT_EVT         0x02    /*!< Byte 4 */
/* Version Sydney */
#define HCI_EVT_MASK_LE_SUBRATE_CHANGE_EVT           0x04    /*!< Byte 4 */
/**@}*/

/** \name LE supported features
 *
 */
/**@{*/
/* Version 4.0 */
#define HCI_LE_SUP_FEAT_ENCRYPTION                   0x0000000000000001  /*!< Encryption supported */
/* Version 4.1 */
#define HCI_LE_SUP_FEAT_CONN_PARAM_REQ_PROC          0x0000000000000002  /*!< Connection Parameters Request Procedure supported */
#define HCI_LE_SUP_FEAT_EXT_REJECT_IND               0x0000000000000004  /*!< Extended Reject Indication supported */
#define HCI_LE_SUP_FEAT_SLV_INIT_FEAT_EXCH           0x0000000000000008  /*!< Slave-Initiated Features Exchange supported */
#define HCI_LE_SUP_FEAT_LE_PING                      0x0000000000000010  /*!< LE Ping supported */
/* Version 4.2 */
#define HCI_LE_SUP_FEAT_DATA_LEN_EXT                 0x0000000000000020  /*!< Data Length Extension supported */
#define HCI_LE_SUP_FEAT_PRIVACY                      0x0000000000000040  /*!< LL Privacy supported */
#define HCI_LE_SUP_FEAT_EXT_SCAN_FILT_POLICY         0x0000000000000080  /*!< Extended Scan Filter Policy supported */
/* Version 5.0 */
#define HCI_LE_SUP_FEAT_LE_2M_PHY                    0x0000000000000100  /*!< LE 2M PHY supported */
#define HCI_LE_SUP_FEAT_STABLE_MOD_IDX_TRANSMITTER   0x0000000000000200  /*!< Stable Modulation Index - Transmitter supported */
#define HCI_LE_SUP_FEAT_STABLE_MOD_IDX_RECEIVER      0x0000000000000400  /*!< Stable Modulation Index - Receiver supported */
#define HCI_LE_SUP_FEAT_LE_CODED_PHY                 0x0000000000000800  /*!< LE Coded PHY supported */
#define HCI_LE_SUP_FEAT_LE_EXT_ADV                   0x0000000000001000  /*!< LE Extended Advertising supported */
#define HCI_LE_SUP_FEAT_LE_PER_ADV                   0x0000000000002000  /*!< LE Periodic Advertising supported */
#define HCI_LE_SUP_FEAT_CH_SEL_2                     0x0000000000004000  /*!< Channel Selection Algorithm #2 supported */
#define HCI_LE_SUP_FEAT_LE_POWER_CLASS_1             0x0000000000008000  /*!< LE Power Class 1 supported */
#define HCI_LE_SUP_FEAT_MIN_NUN_USED_CHAN            0x0000000000010000  /*!< Minimum Number of Used Channels Procedure supported */
/* Version 5.1 */
#define HCI_LE_SUP_FEAT_CONN_CTE_REQ                 0x0000000000020000  /*!< Connection CTE Request supported */
#define HCI_LE_SUP_FEAT_CONN_CTE_RSP                 0x0000000000040000  /*!< Connection CTE Response supported */
#define HCI_LE_SUP_FEAT_CONNLESS_CTE_TRANS           0x0000000000080000  /*!< Connectionless CTE Transmitter supported */
#define HCI_LE_SUP_FEAT_CONNLESS_CTE_RECV            0x0000000000100000  /*!< Connectionless CTE Receiver supported */
#define HCI_LE_SUP_FEAT_ANTENNA_SWITCH_AOD           0x0000000000200000  /*!< Anetenna Switching during CTE Transmission (AoD) supported */
#define HCI_LE_SUP_FEAT_ANTENNA_SWITCH_AOA           0x0000000000400000  /*!< Anetenna Switching during CTE Reception (AoA) supported */
#define HCI_LE_SUP_FEAT_RECV_CTE                     0x0000000000800000  /*!< Receive Constant Tone Extension supported */
#define HCI_LE_SUP_FEAT_PAST_SENDER                  0x0000000001000000  /*!< Periodic Advertising Sync Transfer Sender supported */
#define HCI_LE_SUP_FEAT_PAST_RECIPIENT               0x0000000002000000  /*!< Periodic Advertising Sync Transfer Recipient supported */
#define HCI_LE_SUP_FEAT_SCA_UPDATE                   0x0000000004000000  /*!< Sleep Clock Accuracy Update supported */
#define HCI_LE_SUP_FEAT_REMOTE_PUB_KEY_VALIDATION    0x0000000008000000  /*!< Remote Public Key Validation supported */
/* Version 5.2 */
#define HCI_LE_SUP_FEAT_CIS_MASTER                   0x0000000010000000  /*!< Connected Isochronous Master Role supported */
#define HCI_LE_SUP_FEAT_CIS_SLAVE                    0x0000000020000000  /*!< Connected Isochronous Slave Role supported */
#define HCI_LE_SUP_FEAT_ISO_BROADCASTER              0x0000000040000000  /*!< Isochronous Broadcaster Role supported */
#define HCI_LE_SUP_FEAT_ISO_SYNC_RECEIVER            0x0000000080000000  /*!< Isochronous Synchronized Receiver Role supported */
#define HCI_LE_SUP_FEAT_ISO_HOST_SUPPORT             0x0000000100000000  /*!< Host support for ISO Channels */
#define HCI_LE_SUP_FEAT_POWER_CONTROL_REQUEST        0x0000000200000000  /*!< Power control requests supported */
#define HCI_LE_SUP_FEAT_POWER_CHANGE_IND             0x0000000400000000  /*!< Power control power change indication supported */
#define HCI_LE_SUP_FEAT_PATH_LOSS_MONITOR            0x0000000800000000  /*!< Path loss monitoring supported */
/**@}*/

/** \name LE feature bit positon in FeatureSet stored in the Controller
*
*/
/**@{*/
#define HCI_LE_FEAT_BIT_ISO_HOST_SUPPORT             32  /*!< Host support for ISO Channels */
/**@}*/

/** \name Advertising command parameters
 *
 */
/**@{*/
#define HCI_ADV_MIN_INTERVAL                         0x0020  /*!< Minimum advertising interval */
#define HCI_ADV_MAX_INTERVAL                         0x4000  /*!< Maximum advertising interval */
#define HCI_ADV_DIRECTED_MAX_DURATION                0x0500  /*!< Maximum high duty cycle connectable directed advertising duration */
#define HCI_ADV_TYPE_CONN_UNDIRECT                   0x00    /*!< Connectable undirected advertising */
#define HCI_ADV_TYPE_CONN_DIRECT                     0x01    /*!< Connectable directed high duty cycle advertising */
#define HCI_ADV_TYPE_DISC_UNDIRECT                   0x02    /*!< Discoverable undirected advertising */
#define HCI_ADV_TYPE_NONCONN_UNDIRECT                0x03    /*!< Nonconnectable undirected advertising */
#define HCI_ADV_TYPE_CONN_DIRECT_LO_DUTY             0x04    /*!< Connectable directed low duty cycle advertising */
#define HCI_ADV_CHAN_37                              0x01    /*!< Advertising channel 37 */
#define HCI_ADV_CHAN_38                              0x02    /*!< Advertising channel 38 */
#define HCI_ADV_CHAN_39                              0x04    /*!< Advertising channel 39 */
#define HCI_ADV_FILT_NONE                            0x00    /*!< No scan request or connection filtering */
#define HCI_ADV_FILT_SCAN                            0x01    /*!< White list filters scan requests */
#define HCI_ADV_FILT_CONN                            0x02    /*!< White list filters connections */
#define HCI_ADV_FILT_ALL                             0x03    /*!< White list filters scan req. and conn. */
/**@}*/

/** \name Scan command parameters
 *
 */
/**@{*/
#define HCI_SCAN_TYPE_PASSIVE                        0       /*!< Passive scan */
#define HCI_SCAN_TYPE_ACTIVE                         1       /*!< Active scan */
#define HCI_SCAN_INTERVAL_MIN                        0x0004  /*!< Minimum scan interval */
#define HCI_SCAN_INTERVAL_MAX                        0x4000  /*!< Maximum scan interval */
#define HCI_SCAN_INTERVAL_DEFAULT                    0x0010  /*!< Default scan interval */
#define HCI_SCAN_WINDOW_MIN                          0x0004  /*!< Minimum scan window */
#define HCI_SCAN_WINDOW_MAX                          0x4000  /*!< Maximum scan window */
#define HCI_SCAN_WINDOW_DEFAULT                      0x0010  /*!< Default scan window */
/**@}*/

/** \name Connection command parameters
 *
 */
/**@{*/
#define HCI_CONN_INTERVAL_MIN                        0x0006  /*!< Minimum connection interval */
#define HCI_CONN_INTERVAL_MAX                        0x0C80  /*!< Maximum connection interval */
#define HCI_CONN_LATENCY_MAX                         0x01F3  /*!< Maximum connection latency */
#define HCI_SUP_TIMEOUT_MIN                          0x000A  /*!< Minimum supervision timeout */
#define HCI_SUP_TIMEOUT_MAX                          0x0C80  /*!< Maximum supervision timeout */
/**@}*/

/** \name Connection event parameters
 *
 */
/**@{*/
#define HCI_ROLE_MASTER                              0       /*!< Role is master */
#define HCI_ROLE_SLAVE                               1       /*!< Role is slave */
#define HCI_CLOCK_500PPM                             0x00    /*!< 500 ppm clock accuracy */
#define HCI_CLOCK_250PPM                             0x01    /*!< 250 ppm clock accuracy */
#define HCI_CLOCK_150PPM                             0x02    /*!< 150 ppm clock accuracy */
#define HCI_CLOCK_100PPM                             0x03    /*!< 100 ppm clock accuracy */
#define HCI_CLOCK_75PPM                              0x04    /*!< 75 ppm clock accuracy */
#define HCI_CLOCK_50PPM                              0x05    /*!< 50 ppm clock accuracy */
#define HCI_CLOCK_30PPM                              0x06    /*!< 30 ppm clock accuracy */
#define HCI_CLOCK_20PPM                              0x07    /*!< 20 ppm clock accuracy */
/**@}*/

/** \name Advertising report event parameters
 *
 */
/**@{*/
#define HCI_ADV_CONN_UNDIRECT                        0x00    /*!< Connectable undirected advertising */
#define HCI_ADV_CONN_DIRECT                          0x01    /*!< Connectable directed advertising */
#define HCI_ADV_DISC_UNDIRECT                        0x02    /*!< Discoverable undirected advertising */
#define HCI_ADV_NONCONN_UNDIRECT                     0x03    /*!< Non-connectable undirected advertising */
#define HCI_ADV_SCAN_RESPONSE                        0x04    /*!< Scan response */
/**@}*/

/** \name Extended advertising data operations
 *
 */
/**@{*/
#define HCI_ADV_DATA_OP_FRAG_INTER                   0x00    /*!< Intermediate fragment */
#define HCI_ADV_DATA_OP_FRAG_FIRST                   0x01    /*!< First fragment */
#define HCI_ADV_DATA_OP_FRAG_LAST                    0x02    /*!< Last fragment */
#define HCI_ADV_DATA_OP_COMP_FRAG                    0x03    /*!< Complete extended advertising data */
#define HCI_ADV_DATA_OP_UNCHANGED_DATA               0x04    /*!< Unchanged data (just update Advertising DID) */
/**@}*/

/** \name Advertising data fragment preference
 *
 */
/**@{*/
#define HCI_ADV_DATA_FRAG_PREF_FRAG                  0x00    /*!< Controller may fragment all Host advertising data */
#define HCI_ADV_DATA_FRAG_PREF_NO_FRAG               0x01    /*!< Controller should not fragment nor minimize fragmentation of Host advertising data */
/**@}*/

/** \name Number of advertising sets
 *
 */
/**@{*/
#define HCI_ADV_NUM_SETS_ALL_DISABLE                 0x00    /*!< Disable all advertising sets */
/**@}*/

/** \name Maximum number of scanning or initiating PHYs
 *
 */
/**@{*/
#define HCI_MAX_NUM_PHYS                             3       /*!< Maximum number of scanning or initiating PHYs */
/**@}*/

/** \name Advertising PHY values
 *
 */
/**@{*/
#define HCI_ADV_PHY_LE_1M                            0x01    /*!< LE 1M PHY */
#define HCI_ADV_PHY_LE_2M                            0x02    /*!< LE 2M PHY */
#define HCI_ADV_PHY_LE_CODED                         0x03    /*!< LE Coded PHY */
/**@}*/

/** \name Scanner PHY value bits
 *
 */
/**@{*/
#define HCI_SCAN_PHY_LE_1M_BIT                       (1<<0)  /*!< LE 1M PHY */
#define HCI_SCAN_PHY_LE_2M_BIT                       (1<<1)  /*!< LE 2M PHY */
#define HCI_SCAN_PHY_LE_CODED_BIT                    (1<<2)  /*!< LE Coded PHY  */
/**@}*/

/** \name Initiator PHY value bits
 *
 */
/**@{*/
#define HCI_INIT_PHY_LE_1M_BIT                       (1<<0)  /*!< LE 1M PHY */
#define HCI_INIT_PHY_LE_2M_BIT                       (1<<1)  /*!< LE 2M PHY */
#define HCI_INIT_PHY_LE_CODED_BIT                    (1<<2)  /*!< LE Coded PHY  */
/**@}*/

/** \name Transmitter PHY value bits
*
*/
/**@{*/
#define HCI_TRANS_PHY_LE_1M_BIT                      (1<<0)  /*!< LE 1M PHY */
#define HCI_TRANS_PHY_LE_2M_BIT                      (1<<1)  /*!< LE 2M PHY */
#define HCI_TRABS_PHY_LE_CODED_BIT                   (1<<2)  /*!< LE Coded PHY  */
/**@}*/

/** \name Advertising event properties type bits
 *
 */
/**@{*/
#define HCI_ADV_PROP_CONN_ADV_BIT                    (1<<0)  /*!< Connectable advertising bit */
#define HCI_ADV_PROP_SCAN_ADV_BIT                    (1<<1)  /*!< Scannable advertising bit */
#define HCI_ADV_PROP_DIRECT_ADV_BIT                  (1<<2)  /*!< Directed advertising bit */
#define HCI_ADV_PROP_CONN_DIRECT_ADV_BIT             (1<<3)  /*!< High duty cycle connectable directed advertising bit */
#define HCI_ADV_PROP_USE_LEG_PDU_BIT                 (1<<4)  /*!< Use legacy advertising PDUs bit */
#define HCI_ADV_PROP_OMIT_ADV_ADDR_BIT               (1<<5)  /*!< Omit advertiser's address from all PDUs (anonymous advertising) bit */
#define HCI_ADV_PROP_INC_TX_PWR_BIT                  (1<<6)  /*!< Include TxPower in extended header of advertising PDU bit */
/**@}*/

/** \name Advertising event properties for legacy PDUs
 *
 */
/**@{*/
#define HCI_ADV_PROP_LEG_CONN_UNDIRECT               0x13    /*!< Connectable and scannable undirected advertising (00010011b) */
#define HCI_ADV_PROP_LEG_CONN_DIRECT                 0x1D    /*!< Connectable directed high duty cycle advertising (00011101b) */
#define HCI_ADV_PROP_LEG_SCAN_UNDIRECT               0x12    /*!< Scannable undirected advertising (00010010b) */
#define HCI_ADV_PROP_LEG_NONCONN_UNDIRECT            0x10    /*!< Non-connectable and non-scannable undirected advertising (00010000b) */
#define HCI_ADV_PROP_LEG_CONN_DIRECT_LO_DUTY         0x15    /*!< Connectable directed low duty cycle advertising (00010101b) */
/**@}*/

/** \name Extended advertising report event type bits
 *
 */
/**@{*/
#define HCI_ADV_RPT_CONN_ADV_BIT                     (1<<0)  /*!< Connectable advertising event bit */
#define HCI_ADV_RPT_SCAN_ADV_BIT                     (1<<1)  /*!< Scannable advertising event bit */
#define HCI_ADV_RPT_DIRECT_ADV_BIT                   (1<<2)  /*!< Directed advertising event bit */
#define HCI_ADV_RPT_SCAN_RSP_BIT                     (1<<3)  /*!< Scan response event bit */
#define HCI_ADV_RPT_LEG_ADV_BIT                      (1<<4)  /*!< Legacy advertising PDU event bit */
#define HCI_ADV_RPT_DATA_STATUS_BITS                 (3<<5)  /*!< Data status bits */
/**@}*/

/** \name Advertising report event types for legacy PDUs
 *
 */
/**@{*/
#define HCI_ADV_RPT_LEG_CONN_UNDIRECT                0x13    /*!< Connectable and scannable undirected advertising (0010011b) */
#define HCI_ADV_RPT_LEG_CONN_DIRECT                  0x15    /*!< Connectable directed advertising (0010101b) */
#define HCI_ADV_RPT_LEG_SCAN_UNDIRECT                0x12    /*!< Scannable undirected advertising (0010010b) */
#define HCI_ADV_RPT_LEG_NONCONN_UNDIRECT             0x10    /*!< Non-connectable and non-scannable undirected advertising (0010000b) */
#define HCI_ADV_RPT_LEG_CONN_UNDIRECT_SCAN_RSP       0x1B    /*!< Scan response to connectable and scannable undirected advertising (0011011b) */
#define HCI_ADV_RPT_LEG_SCAN_UNDIRECT_SCAN_RSP       0x1A    /*!< Scan response to scannable undirected advertising (0011010b) */
/**@}*/

/** \name Advertising report data status
 *
 */
/**@{*/
#define HCI_ADV_RPT_DATA_CMPL                        0x00    /*!< Data complete */
#define HCI_ADV_RPT_DATA_INCMPL_MORE                 0x01    /*!< Data incomplete, more date to come */
#define HCI_ADV_RPT_DATA_INCMPL_TRUNC                0x02    /*!< Data incomplete, data truncated, no more date to come */
/**@}*/

/** \name Extended advertising report event primary PHY values
 *
 */
/**@{*/
#define HCI_ADV_RPT_PHY_PRIM_LE_1M                   0x01    /*!< Advertiser PHY is LE 1M */
#define HCI_ADV_RPT_PHY_PRIM_LE_CODED                0x03    /*!< Advertiser PHY is LE Coded */
/**@}*/

/** \name Extended advertising report event seconday PHY values
 *
 */
/**@{*/
#define HCI_ADV_RPT_PHY_SEC_NONE                     0x00    /*!< No packets on seconday advertising channel */
#define HCI_ADV_RPT_PHY_SEC_LE_1M                    0x01    /*!< Advertiser PHY is LE 1M */
#define HCI_ADV_RPT_PHY_SEC_LE_2M                    0x02    /*!< Advertiser PHY is LE 2M */
#define HCI_ADV_RPT_PHY_SEC_LE_CODED                 0x03    /*!< Advertiser PHY is LE Coded */
/**@}*/

/** \name Channel selection algorithm used
 *
 */
/**@{*/
#define HCI_CH_SEL_ALGO_1                            0x00   /*!< LE channel selection algorithm #1 used */
#define HCI_CH_SEL_ALGO_2                            0x01   /*!< LE channel selection algorithm #2 used */
/**@}*/

/** \name KeyType parameters
 *
 */
/**@{*/
#define HCI_PRIVATE_KEY_GENERATED                    0x00    /*!< Use generated private key */
#define HCI_PRIVATE_KEY_DEBUG                        0x01    /*!< Use debug private key */
/**@{*/

/**@{*/
/** \name Minimum number of used channels
 *
 */
/**@{*/
#define HCI_MIN_NUM_OF_USED_CHAN                     8      /*!< Minimum number of used channels */
/**@}*/

/** \name Synchronization timeout for the periodic advertising
*
*/
/**@{*/
#define HCI_SYNC_MIN_TIMEOUT                         0x000A  /*!< Minimum synchronization timeout */
#define HCI_SYNC_MAX_TIMEOUT                         0x4000  /*!< Maximum synchronization timeout */
/**@}*/

/** \name Maximum synchronization skip
*
*/
/**@{*/
#define HCI_SYNC_MAX_SKIP                            0x01F3  /*!< Maximum synchronization skip */
/**@}*/

/** \name Maximum synchronization handle
*
*/
/**@{*/
#define HCI_SYNC_MAX_HANDLE                          0x0EFF  /*!< Maximum synchronization handle */
/**@}*/

/** \name Periodic sync transfer receive mode
*
*/
/**@{*/
#define HCI_SYNC_TRSF_MODE_OFF                       0x00   /*!< Periodic sync transfer receive is disabled */
#define HCI_SYNC_TRSF_MODE_REP_DISABLED              0x01   /*!< Periodic sync transfer receive is enabled, report event is disabled */
#define HCI_SYNC_TRSF_MODE_REP_ENABLED               0x02   /*!< Periodic sync transfer receive is enabled, report event is enabled */
/**@}*/

/** \name Periodic advertising create sync options bits
*
*/
/**@{*/
#define HCI_OPTIONS_FILT_POLICY_BIT                  (1<<0)  /*!< filter policy bit */
#define HCI_OPTIONS_INIT_RPT_ENABLE_BIT              (1<<1)  /*!< initial periodic advertisement reporting bit */
/**@}*/

/** \name Misc command parameters
 *
 */
/**@{*/
#define HCI_READ_TX_PWR_CURRENT                      0       /*!< Read current tx power */
#define HCI_READ_TX_PWR_MAX                          1       /*!< Read maximum tx power */
#define HCI_TX_PWR_MIN                               -30     /*!< Minimum tx power dBm */
#define HCI_TX_PWR_MAX                               20      /*!< Maximum tx power dBm */
#define HCI_TX_PWR_NO_PREFERENCE                     127     /*!< Tx power no preference */
#define HCI_VERSION                                  6       /*!< HCI specification version */
#define HCI_RSSI_MIN                                 -127    /*!< Minimum RSSI dBm */
#define HCI_RSSI_MAX                                 20      /*!< Maximum RSSI dBm */
#define HCI_ADDR_TYPE_PUBLIC                         0       /*!< Public device address */
#define HCI_ADDR_TYPE_RANDOM                         1       /*!< Random device address */
#define HCI_ADDR_TYPE_PUBLIC_IDENTITY                2       /*!< Public identity address */
#define HCI_ADDR_TYPE_RANDOM_IDENTITY                3       /*!< Random identity address */
#define HCI_ADDR_TYPE_ANONYMOUS                      0xFF    /*!< Anonymous device address */
#define HCI_FILT_NONE                                0       /*!< Accept all advertising packets */
#define HCI_FILT_WHITE_LIST                          1       /*!< Accept from While List only */
#define HCI_FILT_RES_INIT                            2       /*!< Accept directed advertisements with RPAs */
#define HCI_FILT_WHITE_LIST_RES_INIT                 3       /*!< Accept from White List or directed advertisements with RPAs */
#define HCI_FILT_PER_ADV_PARAM                       0       /*!< Listen to advertiser specified by create sync command parameters */
#define HCI_FILT_PER_ADV_LIST                        1       /*!< Listen to advertiser from Periodic Advertiser List only */
#define HCI_ROLE_MASTER                              0       /*!< Role is master */
#define HCI_ROLE_SLAVE                               1       /*!< Role is slave */
#define HCI_PRIV_MODE_NETWORK                        0x00    /*!< Network privacy mode (default) */
#define HCI_PRIV_MODE_DEVICE                         0x01    /*!< Device privacy mode */
/**@}*/

/** \name PHY types
 *
 */
/**@{*/
#define HCI_PHY_NONE                                 0x00    /*!< No selected PHY */
#define HCI_PHY_LE_1M_BIT                            (1<<0)  /*!< LE 1M PHY */
#define HCI_PHY_LE_2M_BIT                            (1<<1)  /*!< LE 2M PHY */
#define HCI_PHY_LE_CODED_BIT                         (1<<2)  /*!< LE Coded PHY */
/**@}*/

/** \name All PHYs preference
 *
 */
/**@{*/
#define HCI_ALL_PHY_ALL_PREFERENCES                  0x00    /*!< All PHY preferences */
#define HCI_ALL_PHY_TX_PREFERENCE_BIT                (1<<0)  /*!< Tx PHY preference */
#define HCI_ALL_PHY_RX_PREFERENCE_BIT                (1<<1)  /*!< Rx PHY preference */
/**@}*/

/** \name PHY options
 *
 */
/**@{*/
#define HCI_PHY_OPTIONS_NONE                         0x00    /*!< No preferences */
#define HCI_PHY_OPTIONS_S2_PREFERRED                 0x01    /*!< S=2 coding preferred when transmitting on LE Coded PHY */
#define HCI_PHY_OPTIONS_S8_PREFERRED                 0x02    /*!< S=8 coding preferred when transmitting on LE Coded PHY */
/**@}*/

/** \name CTE Slot Durations
*
*/
/**@{*/
#define HCI_CTE_SLOT_DURATION_NONE                   0x00    /*!< No switching and sampling */
#define HCI_CTE_SLOT_DURATION_1_US                   0x01    /*!< Switching and sampling slots are 1 us each */
#define HCI_CTE_SLOT_DURATION_2_US                   0x02    /*!< Switching and sampling slots are 2 us each */
/**@}*/

/** \name Permitted CTE Type bits
*
*/
/**@{*/
#define HCI_CTE_TYPE_PERMIT_AOA_RSP_BIT              (1<<0)  /*!< Allow AoA Constant Tone Extension Response */
#define HCI_CTE_TYPE_PERMIT_AOD_RSP_1_US_BIT         (1<<1)  /*!< Allow AoD Constant Tone Extension Response with 1 us slots */
#define HCI_CTE_TYPE_PERMIT_AOD_RSP_2_US_BIT         (1<<2)  /*!< Allow AoD Constant Tone Extension Response with 2 us slots */
/**@}*/

/** \name Requested CTE Types
*
*/
/**@{*/
#define HCI_CTE_TYPE_REQ_AOA                         0x00    /*!< AoA Constant Tone Extension */
#define HCI_CTE_TYPE_REQ_AOD_1_US                    0x01    /*!< AoD Constant Tone Extension with 1 us slots */
#define HCI_CTE_TYPE_REQ_AOD_2_US                    0x02    /*!< AoD Constant Tone Extension with 2 us slots */
/**@}*/

/** \name Sync CTE Type bits
*
*/
/**@{*/
#define HCI_SYNC_CTE_TYPE_DONT_SYNC_AOA_BIT          (1<<0)  /*!< Do not sync to packets with an AoA Constant Tone Extension */
#define HCI_SYNC_CTE_TYPE_DONT_SYNC_AOD_1_US_BIT     (1<<1)  /*!< Do not sync to packets with an AoD Constant Tone Extension with 1 us slots */
#define HCI_SYNC_CTE_TYPE_DONT_SYNC_AOD_2_US_BIT     (1<<2)  /*!< Do not sync to packets with an AoD Constant Tone Extension with 2 us slots */
#define HCI_SYNC_CTE_TYPE_DONT_SYNC_TYPE_3_BIT       (1<<3)  /*!< Do not sync to packets with a type 3 Constant Tone Extension */
/**@}*/

/** \name CTE Type bits
*
*/
/**@{*/
#define HCI_CTE_TYPE_DONT_SYNC_AOA_BIT               (1<<0)  /*!< Do not sync to packets with an AoA Constant Tone Extension */
#define HCI_CTE_TYPE_DONT_SYNC_AOD_1_US_BIT          (1<<1)  /*!< Do not sync to packets with an AoD Constant Tone Extension with 1 us slots */
#define HCI_CTE_TYPE_DONT_SYNC_AOD_2_US_BIT          (1<<2)  /*!< Do not sync to packets with an AoD Constant Tone Extension with 2 us slots */
#define HCI_CTE_TYPE_DONT_SYNC_WITHOUT_CTE_BIT       (1<<4)  /*!< Do not sync to packets without Constant Tone Extension */
/**@}*/

/** \name Bluetooth core specification versions
 *
 */
/**@{*/
#define HCI_VER_BT_CORE_SPEC_4_0                     0x06    /*!< Bluetooth core specification 4.0 */
#define HCI_VER_BT_CORE_SPEC_4_1                     0x07    /*!< Bluetooth core specification 4.1 */
#define HCI_VER_BT_CORE_SPEC_4_2                     0x08    /*!< Bluetooth core specification 4.2 */
#define HCI_VER_BT_CORE_SPEC_5_0                     0x09    /*!< Bluetooth core specification 5.0 */
#define HCI_VER_BT_CORE_SPEC_5_1                     0x0A    /*!< Bluetooth core specification 5.1 */
#define HCI_VER_BT_CORE_SPEC_5_2                     0x0B    /*!< Bluetooth core specification 5.2 */
/**@}*/

#ifndef HCI_VER_BT
# define HCI_VER_BT HCI_VER_BT_CORE_SPEC_5_1
#endif

/** \name Parameter lengths
 *
 */
/**@{*/
#define HCI_EVT_MASK_LEN                             8       /*!< Length of event mask byte array */
#define HCI_EVT_MASK_PAGE_2_LEN                      8       /*!< Length of event mask page 2 byte array */
#define HCI_LE_EVT_MASK_LEN                          8       /*!< Length of LE event mask byte array */
#define HCI_FEAT_LEN                                 8       /*!< Length of features byte array */
#define HCI_ADV_DATA_LEN                             31      /*!< Length of advertising data */
#define HCI_SCAN_DATA_LEN                            31      /*!< Length of scan response data */
#define HCI_EXT_ADV_DATA_LEN                         251     /*!< Length of extended advertising data */
#define HCI_EXT_ADV_CONN_DATA_LEN                    191     /*!< Length of extended connectable advertising data */
#define HCI_PER_ADV_DATA_LEN                         252     /*!< Length of periodic advertising data */
#define HCI_EXT_ADV_RPT_DATA_LEN                     229     /*!< Length of extended advertising report data */
#define HCI_PER_ADV_RPT_DATA_LEN                     247     /*!< Length of periodic advertising report data */
#define HCI_CHAN_MAP_LEN                             5       /*!< Length of channel map byte array */
#define HCI_KEY_LEN                                  16      /*!< Length of encryption key */
#define HCI_ENCRYPT_DATA_LEN                         16      /*!< Length of data used in encryption */
#define HCI_RAND_LEN                                 8       /*!< Length of random number */
#define HCI_LE_STATES_LEN                            8       /*!< Length of LE states byte array */
#define HCI_P256_KEY_LEN                             64      /*!< Length of P256 key */
#define HCI_DH_KEY_LEN                               32      /*!< Length of DH Key */
#define HCI_BC_LEN                                   16      /*!< Broadcast code length */

#define HCI_EXT_ADV_RPT_DATA_LEN_OFFSET              23      /*!< Length field offset of extended advertising report data */
#define HCI_PER_ADV_RPT_DATA_LEN_OFFSET              6       /*!< Length field offset of periodic advertising report data */
/**@}*/

/** \name Maximum Advertising Handle and SID
*
*/
/**@{*/
#define HCI_MAX_ADV_HANDLE                           0xEF    /*!< Maximum advertising handle */
#define HCI_MAX_ADV_SID                              0x0F    /*!< Maximum advertising SID */
/**@}*/

/** \name Number of Antenna IDs in Switching Pattern
*
*/
/**@{*/
#define HCI_MIN_NUM_ANTENNA_IDS                      2       /*!< Minimum number of Antenna IDs in switching pattern */
#define HCI_MAX_NUM_ANTENNA_IDS                      75      /*!< Maximum number of Antenna IDs in switching pattern */
/**@}*/

/** \name IQ Report Sample Counts
*
*/
/**@{*/
#define HCI_IQ_RPT_SAMPLE_CNT_MIN                    9       /*!< Minimum number of sample pairs in IQ report */
#define HCI_IQ_RPT_SAMPLE_CNT_MAX                    82      /*!< Maximum number of sample pairs in IQ report */

#define HCI_CONN_IQ_RPT_SAMPLE_CNT_OFFSET            12      /*!< Sample count field offset of connection IQ report */
/**@}*/

/** \name CIG Count
*
*/
/**@{*/
#define HCI_MAX_CIG_COUNT                            2       /*!< Maximum count for CIG */
/**@}*/

/** \name CIS Count
*
*/
/**@{*/
#define HCI_MAX_CIS_COUNT                            6       /*!< Maximum count for CIS */
/**@}*/

/** \name BIG Count
*
*/
/**@{*/
#define HCI_MAX_BIG_COUNT                            2       /*!< Maximum count for BIG */
/**@}*/

/** \name BIS Count
*
*/
/**@{*/
#define HCI_MAX_BIS_COUNT                            6       /*!< Maximum count for BIS */
/**@}*/

/** \name CIG IDs
*
*/
/**@{*/
#define HCI_MIN_CIG_ID                               0x00    /*!< Minimum value for CIG ID. */
#define HCI_MAX_CIG_ID                               0xEF    /*!< Maximum value for CIG ID. */
/**@}*/

/** \name CIS IDs
*
*/
/**@{*/
#define HCI_MIN_CIS_ID                               0x00    /*!< Minimum value for CIS ID. */
#define HCI_MAX_CIS_ID                               0xEF    /*!< Maximum value for CIS ID. */
/**@}*/

/** \name Packing Scheme
*
*/
/**@{*/
#define HCI_PACKING_SEQUENTIAL                       0x00    /*!< Sequential */
#define HCI_PACKING_INTERLEAVED                      0x01    /*!< Interleaved */
/**@}*/

/** \name Framing
*
*/
/**@{*/
#define HCI_FRAMING_UNFRAMED                         0x00    /*!< Unframed */
#define HCI_FRAMING_FRAMED                           0x01    /*!< Framed */
/**@}*/

/** \name Slave Clock Accuracy
*
*/
/**@{*/
#define HCI_MIN_SCA                                  0x00    /*!< Minimum value for SCA. */
#define HCI_MAX_SCA                                  0x07    /*!< Maximum value for SCA. */

/** \name SDU Size
*
*/
/**@{*/
#define HCI_MIN_SDU_SIZE                             0x0000  /*!< Minimum value for SDU size. */
#define HCI_MAX_SDU_SIZE                             0x0FFF  /*!< Maximum value for SDU size. */
/**@}*/

/** \name SDU Interval
*
*/
/**@{*/
#define HCI_MIN_SDU_INTERV                           0x0000FF /*!< Minimum value for SDU interval. */
#define HCI_MAX_SDU_INTERV                           0x0FFFFF /*!< Maximum value for SDU interval. */
#define HCI_DEFAULT_SDU_INTERV                       0x004E20 /*!< Default value for SDU interval. */
/**@}*/

/** \name CIS Transport Latency
*
*/
/**@{*/
#define HCI_MIN_CIS_TRANS_LAT                        0x0005  /*!< Minimum value for CIS transport latency. */
#define HCI_MAX_CIS_TRANS_LAT                        0x0FA0  /*!< Maximum value for CIS transport latency. */
#define HCI_DEFAULT_CIS_TRANS_LAT                    0x0028  /*!< Default value for CIS transport latency. */
/**@}*/

/** \name CIS Flush Time
*
*/
/**@{*/
#define HCI_MIN_CIS_FT                               0x01    /*!< Minimum value for CIS flush time. */
#define HCI_MAX_CIS_FT                               0xFF    /*!< Maximum value for CIS flush time. */
/**@}*/

/** \name CIS Burst Number
*
*/
/**@{*/
#define HCI_MIN_CIS_BN                               0x00    /*!< Minimum value for CIS burst number. */
#define HCI_MAX_CIS_BN                               0x0F    /*!< Maximum value for CIS burst number. */
/**@}*/

/** \name CIS Retransmission Number
*
*/
/**@{*/
#define HCI_MIN_CIS_RTN                              0x00    /*!< Minimum value for CIS retransmission number. */
#define HCI_MAX_CIS_RTN                              0x0F    /*!< Maximum value for CIS retransmission number. */
/**@}*/

/** \name ISO Data Path Direction
*
*/
/**@{*/
#define HCI_ISO_DATA_DIR_INPUT                       0       /*!< Input (Host to Controller) data path. */
#define HCI_ISO_DATA_DIR_OUTPUT                      1       /*!< Output (Controller to Host) data path. */
/**@}*/

/** \name ISO Data Path Direction Bit
*
*/
/**@{*/
#define HCI_ISO_DATA_PATH_INPUT_BIT                  (1<<HCI_ISO_DATA_DIR_INPUT)  /*!< Data path input bit. */
#define HCI_ISO_DATA_PATH_OUTPUT_BIT                 (1<<HCI_ISO_DATA_DIR_OUTPUT) /*!< Data path output bit. */
/**@}*/

/** \name ISO Data Path ID
*
*/
/**@{*/
#define HCI_ISO_DATA_PATH_HCI                        0x00    /*!< HCI data path. */
#define HCI_ISO_DATA_PATH_VS                         0x01    /*!< Vendor Specific. */
#define HCI_ISO_DATA_PATH_DISABLED                   0xFF    /*!< Data path is disabled. */
/**@}*/

/** \name ISO test packet payload type
*
*/
/**@{*/
#define HCI_ISO_ISO_PLD_TYPE_ZERO_LEN                0x00    /*!< Zero length payload. */
#define HCI_ISO_ISO_PLD_TYPE_VAR_LEN                 0x01    /*!< Variable length payload. */
#define HCI_ISO_ISO_PLD_TYPE_MAX_LEN                 0x02    /*!< Maximum length payload. */
/**@}*/

/** \name Maximum number of codecs
*
*/
/**@{*/
#define HCI_MAX_CODEC                                5       /*!< Maximum number of codecs to read from the Controller. */
/**@}*/

/** \name Maximum length of codec-specific capability data
*
*/
/**@{*/
#define HCI_CODEC_CAP_DATA_LEN                       4       /*!< Maximum length of codec-specific capability data. */
/**@}*/

/** \name Codec transport types
*
*/
/**@{*/
#define HCI_CODEC_TRANS_CIS_BIT                      (1<<2)  /*!< Codec supported over LE CIS. */
#define HCI_CODEC_TRANS_BIS_BIT                      (1<<3)  /*!< Codec supported over LE BIS. */
/**@}*/

/** \name ISO Header Packet Boundary
*
*/
/**@{*/
#define HCI_ISO_HDR_PB_START_FRAG                    0x00    /*!< Start fragment of a fragmented SDU. */
#define HCI_ISO_HDR_PB_CONT_FRAG                     0x01    /*!< Continuation fragment of a fragmented SDU. */
#define HCI_ISO_HDR_PB_COMP_FRAG                     0x02    /*!< Complete SDU. */
#define HCI_ISO_HDR_PB_END_FRAG                      0x03    /*!< The end fragment of a fragmented SDU. */
/**@}*/

/** \name ISOAL Segmentation Header Start/Continuation Bit
*
*/
/**@{*/
#define HCI_ISOAL_SEG_HDR_SC_START                   0x00    /*!< ISOAL segmentation header start bit. */
#define HCI_ISOAL_SEG_HDR_SC_CONT                    0x01    /*!< ISOAL segmentation header continue bit. */
/**@}*/

/** \name Company ID
 *
 */
/**@{*/
#define HCI_ID_PACKETCRAFT                           0x07E8  /*!< Packetcraft Inc. company ID */
/**@}*/

/** \name Manufacturer location in Local version
 *
 */
/**@{*/
#define HCI_LOCAL_VER_MANUFACTURER_POS               4       /*!< Manufacturer location in local version */
/**@}*/

/** \name Coding Format Assigned Numbers
 *
 */
/**@{*/
#define HCI_ID_LC3                                   0x06    /*!< LC3 ID */
#define HCI_ID_VS                                    0xFF    /*!< Vendor specific ID */
/**@}*/

/** \name Coding Format Assigned Numbers
 *
 */
/**@{*/
#define HCI_CODEC_TRANSPORT_CIS                      0x02    /*!< Codec supported over LE CIS */
#define HCI_CODEC_TRANSPORT_BIS                      0x03    /*!< Codec supported over LE BIS */
/**@}*/

/* \} */    /* STACK_HCI_API */

#ifdef __cplusplus
};
#endif

#endif /* HCI_DEFS_H */
