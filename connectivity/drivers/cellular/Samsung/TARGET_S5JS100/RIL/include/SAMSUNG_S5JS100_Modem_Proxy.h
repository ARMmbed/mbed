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

#ifndef __SAMSUNG_S5JS100_MODEM_PROXY_H__
#define __SAMSUNG_S5JS100_MODEM_PROXY_H__

#include "SAMSUNG_RIL_constdef.h"
#include "SAMSUNG_RIL_type.h"
#include "modem_io_device.h"
#include "SAMSUNG_S5JS100_RIL_IF.h"
#include "SAMSUNG_S5JS100_STK.h"

namespace mbed {

class SAMSUNG_S5JS100_MODEM_PROXY {
protected:
    SAMSUNG_S5JS100_RIL_IF *mRilContext;
    RIL_RadioState mRadioState;
    ModemIoDevice *mio;
    SAMSUNG_S5JS100_STK *stk;

public:
    SAMSUNG_S5JS100_MODEM_PROXY(SAMSUNG_S5JS100_RIL_IF *rilContext);
    ~SAMSUNG_S5JS100_MODEM_PROXY();
    static void CbProxyRead(mio_buf *buf, void *data);
    void registerSTK(SAMSUNG_S5JS100_STK *t)
    {
        stk = t;
    }

public:
    void setRadioPower(void *data, unsigned int datalen, RIL_Token t);
    void getIccCardStatus(void *data, unsigned int datalen, RIL_Token t);
    void getVoiceRegistrationState(void *data, unsigned int datalen, RIL_Token t);
    void getDataRegistrationState(void *data, unsigned int datalen, RIL_Token t);
    void getOperator(void *data, unsigned int datalen, RIL_Token t);
    void setupDataCall(void *data, unsigned int datalen, RIL_Token t);
    void deactivateDataCall(void *data, unsigned int datalen, RIL_Token t);
    void setInitialAttachApn(void *data, unsigned int datalen, RIL_Token t);
    void getDataCallList(void *data, unsigned int datalen, RIL_Token t);
    void getIMSI(void *data, unsigned int datalen, RIL_Token t);
    void getBasebandVersion(void *data, unsigned int datalen, RIL_Token t);
    void getDeviceIdentity(void *data, unsigned int datalen, RIL_Token t);
    void getIMEI(void *data, unsigned int datalen, RIL_Token t);
    void getIMEISV(void *data, unsigned int datalen, RIL_Token t);
    void getSignalStrength(void *data, unsigned int datalen, RIL_Token t);
    void iccIoForApp(void *data, unsigned int datalen, RIL_Token t);
    void sendSms(void *data, unsigned int datalen, RIL_Token t);
    void setPsmEnabled(void *data, unsigned int datalen, RIL_Token t);
    void setPsmTimer(void *data, unsigned int datalen, RIL_Token t);
    void setEdrx(void *data, unsigned int datalen, RIL_Token t);
    void setForwardingAtCommand(void *data, unsigned int datalen, RIL_Token t);
    void smsAck(void *data, unsigned int datalen, RIL_Token t);
    void smsAckWithPdu(void *data, unsigned int datalen, RIL_Token t);
    void getSmscAddress(void *data, unsigned int datalen, RIL_Token t);
    void setEngMode(void *data, unsigned int datalen, RIL_Token t);
    void setScrLine(void *data, unsigned int datalen, RIL_Token t);
    void getNetworkSelectionMode(void *data, unsigned int datalen, RIL_Token t);
    void setNetworkSelectionAutomatic(void *data, unsigned int datalen, RIL_Token t);
    void setNetworkSelectionManual(void *data, unsigned int datalen, RIL_Token t);
    void setPdnIpAddress(void *data, unsigned int datalen, RIL_Token t);
    void sendStkTerminalRsp(void *data, unsigned int datalen);
    void sendStkEnvelopeCmd(void *data, unsigned int datalen);
    void setDebugTrace(void *data, unsigned int datalen, RIL_Token t);

    void setRadioPowerResponse(void *data, unsigned int datalen);
    void getIccCardStatusResponse(void *data, unsigned int datalen);
    void getVoiceRegistrationStateResponse(void *data, unsigned int datalen);
    void getDataRegistrationStateResponse(void *data, unsigned int datalen);
    void getOperatorResponse(void *data, unsigned int datalen);
    void setupDataCallResponse(void *data, unsigned int datalen);
    void deactivateDataCallResponse(void *data, unsigned int datalen);
    void setInitialAttachApnResponse(void *data, unsigned int datalen);
    void getDataCallListResponse(void *data, unsigned int datalen);
    void getIMSIResponse(void *data, unsigned int datalen);
    void getBasebandVersionResponse(void *data, unsigned int datalen);
    void getDeviceIdentityResponse(void *data, unsigned int datalen);
    void getIMEIResponse(void *data, unsigned int datalen);
    void getIMEISVResponse(void *data, unsigned int datalen);
    void getSignalStrengthResponse(void *data, unsigned int datalen);
    void iccIoForAppResponse(void *data, unsigned int datalen);
    void sendSmsResponse(void *data, unsigned int datalen);
    void setPsmEnabledResponse(void *data, unsigned int datalen);
    void setPsmTimerResponse(void *data, unsigned int datalen);
    void setEdrxResponse(void *data, unsigned int datalen);
    void setForwardingAtCommandResponse(void *data, unsigned int datalen);
    void smsAckResponse(void *data, unsigned int datalen);
    void smsAckWithPduResponse(void *data, unsigned int datalen);
    void getSmscAddressResponse(void *data, unsigned int datalen);
    void setEngModeResponse(void *data, unsigned int datalen);
    void setScrLineResponse(void *data, unsigned int datalen);
    void getNetworkSelectionModeResponse(void *data, unsigned int datalen);
    void setNetworkSelectionAutomaticResponse(void *data, unsigned int datalen);
    void setNetworkSelectionManualResponse(void *data, unsigned int datalen);
    void setPdnIpAddressResponse(void *data, unsigned int datalen);
    void sendStkTerminalRspResponse(void *data, unsigned int datalen);
    void sendStkEnvelopeCmdResponse(void *data, unsigned int datalen);
    void setDebugTraceResponse(void *data, unsigned int datalen);

    void radioReadyInd(void *data, unsigned int datalen);
    void radioStateChangedInd(void *data, unsigned int datalen);
    void networkStateChangedInd(void *data, unsigned int datalen);
    void dataCallListChangedInd(void *data, unsigned int datalen);
    void simStatusChangedInd(void *data, unsigned int datalen);
    void currentSignalStrengthInd(void *data, unsigned int datalen);
    void nitzUpdatedInd(void *data, unsigned int datalen);
    void psmTimerChangedInd(void *data, unsigned int datalen);
    void psmStatusChangedInd(void *data, unsigned int datalen);
    void edrxChangedInd(void *data, unsigned int datalen);
    void forwardingAtCommandInd(void *data, unsigned int datalen);
    void onIncomingNewSms(void *data, unsigned int datalen);
    void onIncomingNewSmsStatusReport(void *data, unsigned int datalen);
    void displayEngInd(void *data, unsigned int datalen);
    void iccid_info(void *data, unsigned int datalen);
    void simRefreshInd(void *data, unsigned int datalen);
    void stkProactiveCommandInd(void *data, unsigned int datalen);

protected:
    int SendToModem(void *data, unsigned int datalen);
    int ProcessResponse(void *data, unsigned int datalen);
    void PrintHexdump(void *data, unsigned int datalen);
};

}
#endif // __SAMSUNG_S5JS100_MODEM_PROXY_H__
