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

#ifndef __SAMSUNG_RIL_TYPES_H__
#define __SAMSUNG_RIL_TYPES_H__

#include "SAMSUNG_S5JS100_RIL_HEADER.h"

#define MAX_IMSI_LEN             15
#define MAX_IMEI_LEN             15
#define MAX_IMEISV_LEN           17
#define MAX_BASEBAND_VERSION_LEN 32
#define MAX_LONG_EONS_LEN        64
#define MAX_SHORT_EONS_LEN       32
#define MAX_NUMERIC_LEN          6
#define MAX_AID_LEN              32
#define MAX_SMSC_PDU_SIZE        24
#define MAX_SMS_PDU_SIZE         488
#define MAX_GSM_SMS_TPDU_SIZE    244
#define MAX_SIM_IO_PAYLOAD_LEN   (1024+1)

typedef unsigned int        UINT;

typedef enum {
    RIL_NOT_REG_AND_NOT_SEARCHING = 0,           // Not registered, MT is not currently searching
    // a new operator to register
    RIL_REG_HOME = 1,                            // Registered, home network
    RIL_NOT_REG_AND_SEARCHING = 2,               // Not registered, but MT is currently searching
    // a new operator to register
    RIL_REG_DENIED = 3,                          // Registration denied
    RIL_UNKNOWN = 4,                             // Unknown
    RIL_REG_ROAMING = 5,                         // Registered, roaming
    RIL_NOT_REG_AND_EMERGENCY_AVAILABLE_AND_NOT_SEARCHING = 10,   // Same as
    // RIL_NOT_REG_AND_NOT_SEARCHING but indicates that
    // emergency calls are enabled.
    RIL_NOT_REG_AND_EMERGENCY_AVAILABLE_AND_SEARCHING = 12,  // Same as RIL_NOT_REG_AND_SEARCHING
    // but indicates that
    // emergency calls are enabled.
    RIL_REG_DENIED_AND_EMERGENCY_AVAILABLE = 13, // Same as REG_DENIED but indicates that
    // emergency calls are enabled.
    RIL_UNKNOWN_AND_EMERGENCY_AVAILABLE = 14,    // Same as UNKNOWN but indicates that
    // emergency calls are enabled.
} RIL_RegState;

typedef struct {
    int serial;
    int status;
} ResponseInfo;

typedef struct {
    int regState;
    int rat;
    int reasonForDenial;
} VoiceRegistrationState;

typedef struct {
    int regState;
    int rat;
    int reasonDataDenied;
    int maxDataCalls;
    int tac;
    int pcid;
    int cid;
} DataRegistrationState;

typedef struct {
    char longAlpha[MAX_LONG_EONS_LEN + 1];
    char shortAlpha[MAX_SHORT_EONS_LEN + 1];
    char numeric[MAX_NUMERIC_LEN + 1];
} NetworkOperatorInfo;

typedef struct {
    int status;
    int suggestedRetryTime;
    int cid;
    int active;
    int protocol;
    char addresses[256];
    char dnses[256];
    char gateways[256];
} SetupDataCallResult;

typedef struct {
    RIL_CardState cardState;
    RIL_AppState appState;
    RIL_AppType appType;
    char aid[MAX_AID_LEN + 1];
} CardStatus;

struct GsmSignalStrength {
    int32_t signalStrength;              // Valid values are (0-61, 99) as defined in
    // TS 27.007 8.69
    int32_t bitErrorRate;                // bit error rate (0-7, 99) as defined in TS 27.007 8.5
};

struct LteSignalStrength {
    int32_t signalStrength;              // Valid values are (0-31, 99) as defined in
    // TS 27.007 8.5
    int32_t rsrp;                        // The current Reference Signal Receive Power in dBm
    // multipled by -1.
    // Range: 44 to 140 dBm
    // INT_MAX: 0x7FFFFFFF denotes invalid value.
    // Reference: 3GPP TS 36.133 9.1.4
    int32_t rsrq;                        // The current Reference Signal Receive Quality in dB
    // multiplied by -1.
    // Range: 20 to 3 dB.
    // INT_MAX: 0x7FFFFFFF denotes invalid value.
    // Reference: 3GPP TS 36.133 9.1.7
    int32_t rssnr;                        // The current reference signal signal-to-noise ratio in
};

typedef struct {
    GsmSignalStrength gw;
    LteSignalStrength lte;
} SignalStrength;

typedef struct {
    int sw1;
    int sw2;
    char payload[MAX_SIM_IO_PAYLOAD_LEN];
} IccIoResult;

typedef struct {
    int status;
    UINT t3412_accepted;
    UINT t3324_accepted;
} PsmTimerInfo;

typedef struct {
    int status;
    int mode_accepted;
    int ptw_accepted;
    int t_edrx_accepted;
} EdrxChangedResult;

typedef struct {
    char smscPdu[MAX_SMSC_PDU_SIZE + 1];
    char pdu[MAX_SMS_PDU_SIZE + 1];
} GsmSmsMessage;

struct SendSmsResult {
    int messageRef;
    char ackPDU[MAX_SMS_PDU_SIZE + 1];
    int errorCode;
};

typedef struct {
    int success;
    char ackPdu[MAX_GSM_SMS_TPDU_SIZE + 1];
} SmsAckIncomingGsmSmsWithPdu;

#endif // __SAMSUNG_RIL_TYPES_H__
