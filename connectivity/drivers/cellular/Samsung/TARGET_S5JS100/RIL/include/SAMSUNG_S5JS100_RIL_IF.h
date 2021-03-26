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

#ifndef SAMSUNG_S5JS100_RIL_IF_H_
#define SAMSUNG_S5JS100_RIL_IF_H_

#include "mbed.h"
#include "SAMSUNG_RIL_constdef.h"
#include "SAMSUNG_RIL_type.h"

typedef struct RequestInfo {
    unsigned int token;
    RIL_Token t;
    int timeoutTickCount;
    struct RequestInfo *p_next;
} RequestInfo;

namespace mbed {
class SAMSUNG_S5JS100_MODEM_PROXY;


class SAMSUNG_S5JS100_RIL_IF {
private:
    struct RIL_Env mCallback;
    RIL_RadioState mOverallRadioState;
    RequestInfo *mPendingRequests;
    Mutex mMutex;

public:
    SAMSUNG_S5JS100_MODEM_PROXY *mModemProxy;

    SAMSUNG_S5JS100_RIL_IF();
    virtual ~SAMSUNG_S5JS100_RIL_IF();

    RIL_RadioState onStateRequest(void);
    void OnRequest(int request, void *data, unsigned int datalen, RIL_Token t);
    void OnCancel(RIL_Token t);
    bool Init(const struct RIL_Env *env);
    void DeInit(void);

    bool AddToRequestList(unsigned int token, RIL_Token t, unsigned int timeout = 10000);
    RIL_Token FindRequestInfo(unsigned int token);
    void UpdateRadioState(RIL_RadioState radioState);
    int ProcessPendingRequests();
    int ReleaseRequests(void);
    void OnRequestComplete(RIL_Token t, RIL_Errno e, void *response, size_t responselen);
    void OnUnsolicitedResponse(int unsolResponse, const void *data, size_t datalen);

public:
    static SAMSUNG_S5JS100_RIL_IF *GetInstance();
};

} /* namespace mbed */

#endif /* SAMSUNG_S5JS100_RIL_IF_H_ */

