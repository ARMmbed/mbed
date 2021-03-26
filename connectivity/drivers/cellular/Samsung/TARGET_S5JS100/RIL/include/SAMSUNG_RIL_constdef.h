/****************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#ifndef __SAMSUNG_CONSTANT_DEF_H__
#define __SAMSUNG_CONSTANT_DEF_H__


#define RCM_ID                             255
#define TOKEN_INVALID                      0xFFFFFFFF
#define SWAP16(val)     ( (((val) << 8) & 0xFF00) | (((val) >> 8) & 0x00FF) )

enum ToaType {
    RIL_TOA_UNKNOWN =              0x81,
    RIL_TOA_INTERNATIONAL =        0x91
};

// Call & SS
#define MAX_DIAL_NUM                       82
#define MAX_DIAL_NAME                      82
#define MAX_UUS_DATA_LEN                   128
#define MAX_CALL_LIST_NUM                  9
#define MAX_USSD_DATA_LEN                  182
#define MAX_DTMF_LEN                       64
#define MAX_SS_NUM_LEN                     32
#define MAX_CALL_FORWARD_STATUS_NUM        8
#define MAX_SS_RETURN_RESULT               255
#define MAX_EMERGENCY_CALL_NUM             10
#define MAX_EMERGENCY_CALL_NUMBER_LEN      32

// SMS
#define MAX_GSM_SMS_SERVICE_CENTER_ADDR    12
#define MAX_GSM_SMS_TPDU_SIZE              244
#define MAX_CB_MSG                         88
#define MAX_CB_CHANNEL_NUM                 50
#define MAX_BCST_INFO_NUM                  50
#define MAX_BCST_MSG_LEN                   90

//SIM & SAT
#define MAX_SIM_AID_LEN                    16
#define MAX_SIM_APP_LABEL_LEN              32
#define MAX_SIM_APPS_INFO_COUNT            100
#define MAX_SIM_PIN_LEN                    8
#define MAX_SIM_PUK_LEN                    8
#define MAX_SIM_FACILITY_PASSWORD_LEN      39
#define MAX_SIM_IO_PATH_LEN                12
#define MAX_SIM_IO_DATA_LEN                512
#define MAX_SIM_PATH_LEN                   50
#define MAX_SIM_DATA_LEN                   512
#define MAX_SIM_AUTH_REQ_LEN               68
#define MAX_SIM_AUTH_RSP_LEN               69
#define MAX_IMSI_LEN                       15
#define MAX_STK_MSG_LEN                    512
#define MAX_PASSWORD_LEN                   (8+1)
#define MAX_SIM_GBA_AUTH_REQ_LEN           512
#define MAX_SIM_GBA_AUTH_RSP_LEN           260
#define MAX_APDU_DATA_LEN                  (512)
#define MAX_ATR_LEN                        (512)
#define MAX_PB_ENTRY_NUM                   13
#define MAX_PB_FIELD_ENTRY                 32
#define MAX_PB_ENTRY_LEN                   512
#define MAX_VSIM_DATA_LEN                  261
#define MAX_FACILITY_CODE_LEN              3

// DEVICEID & MISC
#define MAX_IMEI_LEN                       15
#define MAX_IMEISV_LEN                     17
#define MAX_MEID_LEN                       14
#define MAX_ESN_LEN                        11
#define MAX_ME_SN_LEN                      (32+1)
#define MAX_BB_SW_VER_LEN                  32
#define MAX_BB_HW_VER_LEN                  32
#define MAX_BB_RF_CAL_DATE_LEN             32
#define MAX_BB_PRODUCT_CODE_LEN            32
#define MAX_BB_MODEL_ID_LEN                17
#define MAX_BB_PRL_ERI_VER_LEN             17
#define MAX_BB_CP_CHIPSET_LEN              16

// PDP & NET
#define MAX_CID_NUM_LEN                    10
#define MAX_PLMN_LEN                       6
#define MAX_SHORT_NAME_LEN                 32
#define MAX_FULL_NAME_LEN                  64
#define MAX_PDP_APN_LEN                    101
#define MAX_PDP_ADDRESS_LEN                20
#define MAX_PCSCF_ADDRESS_LEN              60
#define MAX_PDP_USER_NAME_LEN              50
#define MAX_AUTH_USER_NAME_LEN             50
#define MAX_AUTH_PASSWORD_LEN              50
#define MAX_NET_INFO_COUNT                 16
#define MAX_IPV4_ADDR_LEN                  4
#define MAX_IPV6_ADDR_LEN                  16
#define MAX_IPV4_PREFIX_LEN                32
#define MAX_RTR_SOLICITATIONS              3
#define MAX_IFNAME_LEN                     256
#define MAX_IFPREFIX_LEN                   32
#define MAX_IOCHANNEL_NAME_LEN             32
#define MAX_DATA_CALL_SIZE                 4
#define MAX_ADDRESS_STRING_LEN             256
#define MAX_PREFERRED_PLMN_SIZE            150
#define NET_BAND_MAX            32

// SND

//IMS
#define MAX_PDN_IP_LEN                     16
#define MAX_DELAYED_TIME_REF               8

//WLan SIM auth
#define MAX_AUTH_LEN                       (34)
#define MAX_AUTH_RSP_LEN                   (69)

//MISC
#define RIL_NUM_TX_POWER_LEVELS            5

#ifdef SUPPORT_CDMA
// Call & SS
#define MAX_BURST_DTMF_LEN                 32
#define MAX_BURST_DTMF_ON_OFF_LEN          255
#define MAX_FLASH_LEN                      35
#define MAX_ALPHA_INFO_BUF_LEN             64
#define MAX_NUMBER_INFO_BUFFER_LEN         81
#define MAX_NUMBER_OF_INFO_RECS            10

// SMS
// CDMA SMS MESSAGE SIZE
// Max. 304 Bytes for MO
//     TYPE(1) + Teleservice Id(4) + Destination Address(5 + RIL_CDMA_SMS_ADDRESS_MAX(36))
//     + Bearer Reply Option(3)+ Bearer Data(RIL_CDMA_SMS_BEARER_DATA_MAX(255))
// Max. 348 Bytes for MT
//     TYPE(1) + Teleservice Id(4) + Service Category(4) + Originating Address(5 + RIL_CDMA_SMS_ADDRESS_MAX(36))
//     Origination Subaddress(4 + RIL_CDMA_SMS_SUBADDRESS_MAX(36)) + Bearer Reply Option(3)
//     + Bearer Data(RIL_CDMA_SMS_BEARER_DATA_MAX(255))
#define MAX_CDMA_SMS_MSG_SIZE              348
#define MAX_CDMA_SMS_RUIM_MSG_SIZE         253 // See 3GPP2 C.S0023 3.4.27
#define MAX_CDMA_BCST_INFO_NUM             50

// DEVICEID & MISC
#define MAX_CDMA_MDN_LEN                   15
#define MAX_CDMA_MIN_LEN                   10
#endif // SUPPORT_CDMA
#endif

