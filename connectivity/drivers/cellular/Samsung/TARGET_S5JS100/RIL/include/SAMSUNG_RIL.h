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

#ifndef __VENDOR_H__
#define __VENDOR_H__

#include "ril.h"
#include "SAMSUNG_RIL_type.h"

class IRadioResponse;
class IRadioIndication;

class IRadio {
public:
    virtual ~IRadio() {}
    virtual void setRadioPower(bool on) = 0;
    virtual void setResponse(IRadioResponse *radioResponse, IRadioIndication *radioIndication) = 0;
    virtual void getIccCardStatus() = 0;
    virtual void getVoiceRegistrationState() = 0;
    virtual void getDataRegistrationState() = 0;
    virtual void getOperator() = 0;
    virtual void setupDataCall(int rat, int profile, const char *apn, const char *username, const char *password,
                               int authType, const char *protocol) = 0;
    virtual void deactivateDataCall(int cid, int reason) = 0;
    virtual void setInitialAttachApn(const char *apn, const char *user, const char *password,
                                     int authType, const char *protocol, int type) = 0;
    virtual void getDataCallList() = 0;
    virtual void getIMSI() = 0;
    virtual void getBasebandVersion() = 0;
    virtual void getDeviceIdentity() = 0;
    virtual void getIMEI() = 0;
    virtual void getIMEISV() = 0;
    virtual void getSignalStrength() = 0;
    virtual int iccIoForApp(int command, int fileid, const char *path, int p1, int p2, int p3,
                            const char *data, const char *pin2, const char *aid) = 0;
    virtual int sendSms(GsmSmsMessage *message);
    virtual void setPsmEnabled(int status) = 0;
    virtual void setPsmTimer(UINT t3412, UINT t3324) = 0;
    virtual void setEdrx(int ptw, int t_edrx) = 0;
    virtual void setForwardingAtCommand(char *command) = 0;
    virtual int acknowledgeLastIncomingGsmSms(int result, int failcause) = 0;
    virtual int acknowledgeIncomingGsmSmsWithPdu(int success, const char *ackPdu) = 0;
    virtual int getSmscAddress() = 0;
    virtual void setEngMode(int mode) = 0;
    virtual void setScrLine(int line) = 0;
};

class IRadioResponse {
public:
    virtual ~IRadioResponse() {}
    virtual void setRadioPowerResponse(ResponseInfo *responseInfo) = 0;
    virtual void getIccCardStatusResponse(ResponseInfo *responseInfo, CardStatus *cardStatus) = 0;
    virtual void getVoiceRegistrationStateResponse(ResponseInfo *responseInfo, const VoiceRegistrationState *voiceRegState) = 0;
    virtual void getDataRegistrationStateResponse(ResponseInfo *responseInfo, const DataRegistrationState *dataRegState) = 0;
    virtual void getOperatorResponse(ResponseInfo *responseInfo, const char *longName, const char *shortName, const char *numeric) = 0;
    virtual void setupDataCallResponse(ResponseInfo *responseInfo, const SetupDataCallResult *setupDataCallResult) = 0;
    virtual void deactivateDataCallResponse(ResponseInfo *responseInfo) = 0;
    virtual void setInitialAttachApnResponse(ResponseInfo *responseInfo) = 0;
    virtual void getDataCallListResponse(ResponseInfo *responseInfo, const SetupDataCallResult *setupDataCallResult, int size) = 0;
    virtual void getIMSIResponse(ResponseInfo *responseInfo, const char *imsi) = 0;
    virtual void getBasebandVersionResponse(ResponseInfo *responseInfo, const char *version) = 0;
    virtual void getDeviceIdentityResponse(ResponseInfo *responseInfo, const char *imei, const char *imeisv, const char *esn, const char *meid) = 0;
    virtual void getIMEIResponse(ResponseInfo *responseInfo, const char *imei) = 0;
    virtual void getIMEISVResponse(ResponseInfo *responseInfo, const char *imeisv) = 0;
    virtual void getSignalStrengthResponse(ResponseInfo *responseInfo, const SignalStrength *sigStrength) = 0;
    virtual void iccIoForAppResponse(ResponseInfo *responseInfo, IccIoResult *iccIo) = 0;
    virtual void sendSmsResponse(ResponseInfo *responseInfo, SendSmsResult *sendSmsResult);
    virtual void setPsmEnabledResponse(ResponseInfo *responseInfo) = 0;
    virtual void setPsmTimerResponse(ResponseInfo *responseInfo) = 0;
    virtual void setEdrxResponse(ResponseInfo *responseInfo) = 0;
    virtual void setForwardingAtCommandResponse(ResponseInfo *responseInfo) = 0;
    virtual void acknowledgeLastIncomingGsmSmsResponse(ResponseInfo *responseInfo) = 0;
    virtual void acknowledgeIncomingGsmSmsWithPduResponse(ResponseInfo *responseInfo) = 0;
    virtual void getSmscAddressResponse(ResponseInfo *responseInfo, char *sca) = 0;
    virtual void setEngModeResponse(ResponseInfo *responseInfo) = 0;
    virtual void setScrLineResponse(ResponseInfo *responseInfo) = 0;
};

class IRadioIndication {
public:
    virtual ~IRadioIndication() {}
    virtual void radioStateChangedInd(int radioState) = 0;
    virtual void networkStateChangedInd() = 0;
    virtual void dataCallListChangedInd(const SetupDataCallResult *setupDataCallResult, int size) = 0;
    virtual void simStatusChangedInd() = 0;
    virtual void currentSignalStrengthInd(const SignalStrength *sigStrength) = 0;
    virtual void nitzUpdatedInd(const char *time) = 0;
    virtual void psmTimerChangedInd(const PsmTimerInfo *psmTimerInfo) = 0;
    virtual void psmStatusChangedInd(int status) = 0;
    virtual void edrxChangedInd(const EdrxChangedResult *edrxChangedResult) = 0;
    virtual void forwardingAtCommandInd(char *command) = 0;
    virtual void newSmsInd(char *pdu) = 0;
    virtual void newSmsStatusReportInd(char *pdu) = 0;
    virtual void displayEngInd(char *data, int datalen) = 0;
};

#endif // __VENDOR_H__
