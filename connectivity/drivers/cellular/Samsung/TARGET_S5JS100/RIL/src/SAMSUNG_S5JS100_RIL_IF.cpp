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

#include "SAMSUNG_S5JS100_RIL_IF.h"
#include "SAMSUNG_S5JS100_STK.h"
#include "SAMSUNG_S5JS100_Modem_Proxy.h"
#include "modem_link_device_shmem.h"
#include "s5js100_pwr.h"

#include "mbed_trace.h"
#define TRACE_GROUP "SRIL"

#ifndef SAMSUNG_S5JS100_PPP_RIL_DBG_ON
#define SAMSUNG_S5JS100_PPP_RIL_DBG_ON        0
#endif
#define SAMSUNG_S5JS100_PPP_RIL_DBG        if (SAMSUNG_S5JS100_PPP_RIL_DBG_ON) tr_info

namespace mbed {

extern "C" {
    extern void dcxo_init(void);
    extern void shmem_save_init(void);
    extern void s5js100_modem_start(void);
    extern void s5js100_modem_stop(void);
#ifdef SAMSUNG_S5JS100_IF5_EN
#if SAMSUNG_S5JS100_IF5_EN==1
    extern void if5_init(void);
#endif
#endif
}

static EventQueue *rilif_wqueue = NULL;
static Thread *rilifwork = NULL;
static struct RIL_Env rilcallback;
static Semaphore rilifwork_sem(1, 512);

struct {
    RIL_Token t;
    RIL_Errno e;
    void *response;
    size_t responselen;
} sync_res_msg;

int first_RIL_REQUEST_SET_INITIAL_ATTACH_APN = 1;

static void rilifwork_func(void)
{
    rilcallback.OnRequestComplete(sync_res_msg.t, sync_res_msg.e, sync_res_msg.response, sync_res_msg.responselen);
    rilifwork_sem.release();
}

void sync_resposne(RIL_Token t, RIL_Errno e, void *response, size_t responselen)
{
    rilifwork_sem.acquire();
    sync_res_msg.t = t;
    sync_res_msg.e = e;
    sync_res_msg.response = response;
    sync_res_msg.responselen = responselen;
    rilif_wqueue->call(rilifwork_func);
}

RIL_RadioState SAMSUNG_S5JS100_RIL_IF::onStateRequest(void)
{
    return mOverallRadioState;
}

SAMSUNG_S5JS100_RIL_IF *SAMSUNG_S5JS100_RIL_IF::GetInstance()
{
    static SAMSUNG_S5JS100_RIL_IF *s5js100_rilInstance;

    if (s5js100_rilInstance == NULL) {
        s5js100_rilInstance = new SAMSUNG_S5JS100_RIL_IF();
    }
    return s5js100_rilInstance;
}

void restart_cb(void)
{
    tr_info("RESTART Modem!!!");
    mbed::SAMSUNG_S5JS100_RIL_IF::GetInstance()->OnUnsolicitedResponse(RIL_UNSOL_RIL_RESTART_REQUIRED, NULL, 0);
}

bool SAMSUNG_S5JS100_RIL_IF::Init(const struct RIL_Env *env)
{
    if (env == NULL) {
        tr_info("Invalid RIL_Env pointer");
        return false;
    }

    tr_info("MODEM IPC ready..");
    s5js100_modem_start();

    mOverallRadioState = RADIO_STATE_UNAVAILABLE;
    mCallback = *env;
    if (mModemProxy == NULL) {
        mModemProxy = new SAMSUNG_S5JS100_MODEM_PROXY(this);
        mModemProxy->registerSTK(SAMSUNG_S5JS100_STK::GetInstance());
    }

    if (rilif_wqueue == NULL) {
        rilif_wqueue = new EventQueue();
        rilifwork = new rtos::Thread(osPriorityNormal, 2048, NULL, "rilifwork");
        rilifwork->start(callback(rilif_wqueue, &events::EventQueue::dispatch_forever));
    }
    rilcallback = *env;

    pShmemLinkDevice->fail_cb = restart_cb;
    pShmemLinkDevice->mbox_init();

    dcxo_init();
    shmem_save_init();
#ifdef SAMSUNG_S5JS100_IF5_EN
#if SAMSUNG_S5JS100_IF5_EN==1
    if5_init();
#endif
#endif
    mcpu_init(MCPU_CP);

    return true;
}

void SAMSUNG_S5JS100_RIL_IF::DeInit(void)
{
    mOverallRadioState = RADIO_STATE_UNAVAILABLE;
    mPendingRequests = NULL;
    mModemProxy = NULL;

    pShmemLinkDevice->mbox_deinit();
    tr_info("MODEM IPC stop..");
    s5js100_modem_stop();

    mcpu_reset();

    mMutex.lock();
    RequestInfo **ppCur = &mPendingRequests;
    while (*ppCur != NULL) {
        RequestInfo *tmp = *ppCur;
        *ppCur = (*ppCur)->p_next;
        delete tmp;
    }
    mMutex.unlock();
}

SAMSUNG_S5JS100_RIL_IF::SAMSUNG_S5JS100_RIL_IF()
{
    mOverallRadioState = RADIO_STATE_UNAVAILABLE;
    mPendingRequests = NULL;
    mModemProxy = NULL;
}

SAMSUNG_S5JS100_RIL_IF::~SAMSUNG_S5JS100_RIL_IF()
{
    DeInit();
    delete mModemProxy;
}

void SAMSUNG_S5JS100_RIL_IF::OnRequest(int request, void *data, unsigned int datalen, RIL_Token t)
{
    SAMSUNG_S5JS100_PPP_RIL_DBG("SAMSUNG_S5JS100_PPP_RIL::OnRequest request(%d) data(%p) datalen(%u)\n", request, data, datalen);
    if (mModemProxy == NULL) {
        tr_info("%s ModemProxy is not initialized.", __FUNCTION__);
        OnRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return ;
    }

    if (mOverallRadioState == RADIO_STATE_UNAVAILABLE) {
        tr_info("%s OverallRadioState is RADIO_STATE_UNAVAILABLE.", __FUNCTION__);
        OnRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return ;
    }

    switch (request) {
        case RIL_REQUEST_RADIO_POWER:
            //mModemProxy->setRadioPower(data, datalen, t);
            sync_resposne(t, RIL_E_SUCCESS, NULL, 0);
            break;
        case RIL_REQUEST_GET_SIM_STATUS:
            mModemProxy->getIccCardStatus(data, datalen, t);
            break;
        case RIL_REQUEST_VOICE_REGISTRATION_STATE:
            mModemProxy->getVoiceRegistrationState(data, datalen, t);
            break;
        case RIL_REQUEST_DATA_REGISTRATION_STATE:
            mModemProxy->getDataRegistrationState(data, datalen, t);
            break;
        case RIL_REQUEST_OPERATOR:
            mModemProxy->getOperator(data, datalen, t);
            break;
        case RIL_REQUEST_SETUP_DATA_CALL:
            mModemProxy->setupDataCall(data, datalen, t);
            break;
        case RIL_REQUEST_DEACTIVATE_DATA_CALL:
            mModemProxy->deactivateDataCall(data, datalen, t);
            break;
        case RIL_REQUEST_SET_INITIAL_ATTACH_APN:
            if (first_RIL_REQUEST_SET_INITIAL_ATTACH_APN == 1) {
                mModemProxy->setInitialAttachApn(data, datalen, t);
                first_RIL_REQUEST_SET_INITIAL_ATTACH_APN = 0;
            } else {
                sync_resposne(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        case RIL_REQUEST_DATA_CALL_LIST:
            mModemProxy->getDataCallList(data, datalen, t);
            break;
        case RIL_REQUEST_GET_IMSI:
            mModemProxy->getIMSI(data, datalen, t);
            break;
        case RIL_REQUEST_BASEBAND_VERSION:
            mModemProxy->getBasebandVersion(data, datalen, t);
            break;
        case RIL_REQUEST_DEVICE_IDENTITY:
            mModemProxy->getDeviceIdentity(data, datalen, t);
            break;
        case RIL_REQUEST_GET_IMEI:
            mModemProxy->getIMEI(data, datalen, t);
            break;
        case RIL_REQUEST_GET_IMEISV:
            mModemProxy->getIMEISV(data, datalen, t);
            break;
        case RIL_REQUEST_SIGNAL_STRENGTH:
            mModemProxy->getSignalStrength(data, datalen, t);
            break;
        case RIL_REQUEST_SIM_IO:
            mModemProxy->iccIoForApp(data, datalen, t);
            break;
        case RIL_REQUEST_SEND_SMS:
            mModemProxy->sendSms(data, datalen, t);
            break;
        case RIL_REQUEST_PSM_ENABLED:
            mModemProxy->setPsmEnabled(data, datalen, t);
            break;
        case RIL_REQUEST_PSM_TIMER:
            mModemProxy->setPsmTimer(data, datalen, t);
            break;
        case RIL_REQUEST_SET_EDRX:
            mModemProxy->setEdrx(data, datalen, t);
            break;
        case RIL_REQUEST_SET_FORWARDING_AT_COMMAND:
            mModemProxy->setForwardingAtCommand(data, datalen, t);
            break;
        case RIL_REQUEST_SMS_ACKNOWLEDGE:
            mModemProxy->smsAck(data, datalen, t);
            break;
        case RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU:
            mModemProxy->smsAckWithPdu(data, datalen, t);
            break;
        case RIL_REQUEST_GET_SMSC_ADDRESS:
            mModemProxy->getSmscAddress(data, datalen, t);
            break;
        case RIL_REQUEST_SET_ENG_MODE:
            mModemProxy->setEngMode(data, datalen, t);
            break;
        case RIL_REQUEST_SET_SCR_LINE:
            mModemProxy->setScrLine(data, datalen, t);
            break;
        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
            mModemProxy->getNetworkSelectionMode(data, datalen, t);
            break;
        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:
            mModemProxy->setNetworkSelectionAutomatic(data, datalen, t);
            break;
        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:
            mModemProxy->setNetworkSelectionManual(data, datalen, t);
            break;
        case RIL_REQUEST_SET_PDN_IP_ADDR:
            mModemProxy->setPdnIpAddress(data, datalen, t);
            break;
        case RIL_REQUEST_SET_DEBUG_TRACE:
            mModemProxy->setDebugTrace(data, datalen, t);
            break;
        default:
            //RilLogW("%s Unsupported request %d", __FUNCTION__, request);
            OnRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
            return ;
    }
}

void SAMSUNG_S5JS100_RIL_IF::OnCancel(RIL_Token t)
{
    RIL_Token ret = (RIL_Token)0;

    mMutex.lock();
    RequestInfo **pendingRequestsHook = &mPendingRequests;
    for (RequestInfo **ppCur = pendingRequestsHook ; *ppCur != NULL ; ppCur = &((*ppCur)->p_next)) {
        if ((*ppCur)->t == t) {
            RequestInfo *tmp = *ppCur;
            ret = tmp->t;
            *ppCur = (*ppCur)->p_next;
            delete tmp;
            break;
        }
    } // end for ~
    mMutex.unlock();

    if (ret == NULL) {
        return;
    }

    OnRequestComplete(ret, RIL_E_CANCELLED, NULL, 0);
}

void SAMSUNG_S5JS100_RIL_IF::UpdateRadioState(RIL_RadioState radioState)
{
    RIL_RadioState oldState = mOverallRadioState;
    mOverallRadioState = radioState;
    if ((oldState == RADIO_STATE_ON) && (radioState == RADIO_STATE_UNAVAILABLE)) {
        //int ret = ReleaseRequests();
        ReleaseRequests();
        //RilLogV("Release Requests(%d)", ret);
    }
}

bool SAMSUNG_S5JS100_RIL_IF::AddToRequestList(unsigned int token, RIL_Token t, unsigned int timeout/* = 10000*/)
{
    RequestInfo *pRI = new RequestInfo;
    if (pRI != NULL) {
        RequestInfo **pendingRequestsHook = &mPendingRequests;
        pRI->token = token;
        pRI->t = t;
//        pRI->timeoutTickCount = (int)((timeout < RequestTimeoutMonitor::MAX_TIMEOUT_TICK ? timeout : RequestTimeoutMonitor::MAX_TIMEOUT_TICK)  / RequestTimeoutMonitor::TIMEOUT_TICK) - 1;
        if (pRI->timeoutTickCount <= 0) {
            pRI->timeoutTickCount = 1;
        }
        mMutex.lock();
        pRI->p_next = *pendingRequestsHook;
        *pendingRequestsHook = pRI;
        mMutex.unlock();
        //RilLogV("%s token(%d) RIL_Token(%p)", __FUNCTION__, token, t);
        return true;
    }
    return false;
}

RIL_Token SAMSUNG_S5JS100_RIL_IF::FindRequestInfo(unsigned int token)
{
    RIL_Token ret = (RIL_Token)0;
    mMutex.lock();
    RequestInfo **pendingRequestsHook = &mPendingRequests;
    for (RequestInfo **ppCur = pendingRequestsHook ; *ppCur != NULL ; ppCur = &((*ppCur)->p_next)) {
        if ((*ppCur)->token == token) {
            RequestInfo *tmp = *ppCur;
            ret = tmp->t;
            *ppCur = (*ppCur)->p_next;
            delete tmp;
            break;
        }
    } // end for ~
    mMutex.unlock();
    if (ret != 0) {
        //RilLogV("%s found token(%d) RIL_Token(%p)", __FUNCTION__, token, ret);
    } else {
        //RilLogW("%s not existed token (%d)", __FUNCTION__, token);
    }

    return ret;
}

int SAMSUNG_S5JS100_RIL_IF::ProcessPendingRequests()
{
    int ret = 0;
    RequestInfo **pendingRequestsHook = &mPendingRequests;
    mMutex.lock();
    for (RequestInfo **ppCur = pendingRequestsHook ; *ppCur != NULL ; ppCur = &((*ppCur)->p_next)) {
        if (--(*ppCur)->timeoutTickCount < 0) {
            ret++;
            RequestInfo *tmp = *ppCur;
            *ppCur = (*ppCur)->p_next;
            if (tmp != NULL) {
                //RilLogV("%s OnRequestComplete: token(0x%04d) RIL_Token(%p)", __FUNCTION__, tmp->token, tmp->t);
                OnRequestComplete(tmp->t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            delete tmp;

            // end of list
            if ((*ppCur) == NULL) {
                break;
            }
        }
    } // end for ~
    mMutex.unlock();
    if (ret > 0) {
        //RilLogV("%s %d pending requests had been timeout!", __FUNCTION__, ret);
    }

    return ret;
}

int SAMSUNG_S5JS100_RIL_IF::ReleaseRequests()
{
    int ret = 0;
    RequestInfo **pendingRequestsHook = &mPendingRequests;
    mMutex.lock();
    for (RequestInfo **ppCur = pendingRequestsHook ; *ppCur != NULL ; ppCur = &((*ppCur)->p_next)) {
        ret++;
        RequestInfo *tmp = *ppCur;
        *ppCur = (*ppCur)->p_next;
        if (tmp != NULL) {
            //RilLogV("%s OnRequestComplete: token(0x%04d) RIL_Token(%p)", __FUNCTION__, tmp->token, tmp->t);
            OnRequestComplete(tmp->t, RIL_E_GENERIC_FAILURE, NULL, 0);
        }
        delete tmp;

        // end of list
        if ((*ppCur) == NULL) {
            break;
        }
    } // end for ~
    mMutex.unlock();

    return ret;
}

void SAMSUNG_S5JS100_RIL_IF::OnRequestComplete(RIL_Token t, RIL_Errno e, void *response, size_t responselen)
{
    SAMSUNG_S5JS100_PPP_RIL_DBG("SAMSUNG_S5JS100_RIL_IF::OnRequestComplete RIL_Token(%p) RIL_Errno(%d) response(%p) responselen(%d)\n", t, e, response, responselen);
    mCallback.OnRequestComplete(t, e, response, responselen);
}

void SAMSUNG_S5JS100_RIL_IF::OnUnsolicitedResponse(int unsolResponse, const void *data, size_t datalen)
{
    SAMSUNG_S5JS100_PPP_RIL_DBG("SAMSUNG_S5JS100_RIL_IF::OnUnsolicitedResponse unsolResponse(%d) data(%p) datalen(%u)\n", unsolResponse, data, datalen);
    mCallback.OnUnsolicitedResponse(unsolResponse, data, datalen);
}

#ifdef __cplusplus
extern "C" {
#endif

void onRequest(int request, void *data, size_t datalen, RIL_Token t)
{
    return mbed::SAMSUNG_S5JS100_RIL_IF::GetInstance()->OnRequest(request, data, datalen, t);
}

RIL_RadioState onStateRequest()
{
    return mbed::SAMSUNG_S5JS100_RIL_IF::GetInstance()->onStateRequest();
}

void onCancel(RIL_Token t)
{
    return mbed::SAMSUNG_S5JS100_RIL_IF::GetInstance()->OnCancel(t);
}

const char *getVersion(void)
{
    return "12";
}

/**
 * RIL callbacks exposed to RILD daemon
 */
static const RIL_RadioFunctions s_callbacks = {
    12,
    onRequest,
    onStateRequest,
    NULL,
    onCancel,
    getVersion
};

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
    mbed::SAMSUNG_S5JS100_RIL_IF *inst = mbed::SAMSUNG_S5JS100_RIL_IF::GetInstance();
    if (!inst->Init(env)) {
        return NULL;
    }
    return &s_callbacks;
}


void RIL_Reinit(void)
{
    mbed::SAMSUNG_S5JS100_RIL_IF *inst = mbed::SAMSUNG_S5JS100_RIL_IF::GetInstance();
    inst->mModemProxy = new SAMSUNG_S5JS100_MODEM_PROXY(inst);
    inst->mModemProxy->registerSTK(SAMSUNG_S5JS100_STK::GetInstance());
    pShmemLinkDevice->mbox_init();
    dcxo_init();
    shmem_save_init();
    mcpu_init(MCPU_CP);
}


void RIL_Deinit(void)
{
    mbed::SAMSUNG_S5JS100_RIL_IF *inst = mbed::SAMSUNG_S5JS100_RIL_IF::GetInstance();

    inst->DeInit();
    first_RIL_REQUEST_SET_INITIAL_ATTACH_APN = 1;
}

#ifdef __cplusplus
}
#endif

} /* namespace mbed */
