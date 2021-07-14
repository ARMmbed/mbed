/*
 * Copyright (c) 2020, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef RIL_ADAPTATION_H_
#define RIL_ADAPTATION_H_

#include "ril.h"
#include "nsapi.h"
#include "rtos/ConditionVariable.h"
#include "rtos/Mutex.h"

namespace mbed {

class RIL_CellularDevice;

/** struct to identify RIL request and all the data needed to make successful requests */
struct ril_token_t {
    int token_id = 0;
    int request_id = 0;
    nsapi_error_t response_error = NSAPI_ERROR_OK;
    Callback<void(ril_token_t *, RIL_Errno, void *, size_t)> cb;

    rtos::Mutex *cond_mutex = nullptr;
    rtos::ConditionVariable *cond_var = nullptr;
};

class RILAdaptation {
public:
    RILAdaptation();
    virtual ~RILAdaptation();

    /** Device must create this class first and set itself here in constructor so it's there when
     *  the first requests kick in.
     *
     *  @param device   pointer to device instance
     */
    void set_device(RIL_CellularDevice *device);

    /** Initializes RILD
     *
     *  @return NSAPI_ERROR_OK for success
     *          NSAPI_ERROR_DEVICE_ERROR if error
     */
    virtual nsapi_error_t init_ril();

    /** Deinitializes RILD.
     *
     *  @return NSAPI_ERROR_OK for success,
     *          NSAPI_ERROR_DEVICE_ERROR if error
     */
    virtual nsapi_error_t deinit_ril();

    /** Getter for RIL_CellularDevice instance.
     *
     *  @return pointer to RIL_CellularDevice instance
     */
    RIL_CellularDevice *get_device() const;

    /** Send request to RILD. Asynchronous call. On request complete/cancel the callback will called.
     *
     *  @param request  RIL request id, for example RIL_REQUEST_SIGNAL_STRENGTH
     *  @param data     Data needed for RIL request
     *  @param data_len Length of the data
     *  @param callback Callback to call when request is completed/cancelled
     *  @param cond_mutex Optional pointer to mutex held by condition variable cond_var
     *  @param cond_var   Optional condition variable to be included in RIL request token
     *
     *  @return         pointer to token which identifies the request and holds data. Token is owned by this class and it will
     *                  be deleted after token callback function is called. Caller is responsible about deleting possible
     *                  data inside the token as it's request specific
     */
    ril_token_t *send_request(int request, void *data, size_t data_len, Callback<void(ril_token_t *, RIL_Errno, void *, size_t)> callback,
                              rtos::Mutex *cond_mutex, rtos::ConditionVariable *cond_var);

    /** Synchronous call to get radio state from RILD
     *
     *  @return enum RIL_RadioState on success. NULL if init_ril() methods has not been called successfully
     */
    RIL_RadioState get_radio_state();

    /** Synchronous methods to get RILD version.
     *
     *  @return RILD version or NULL if init_ril() methods has not been called successfully
     */
    const char *get_version(void);

    /** Cancels the wanted RIL request. callback specified in param token is still called but with error RIL_E_CANCELLED
     *
     *  @param token which identifies the RIL request to be cancelled
     */
    void cancel_request(ril_token_t *token);

#if MBED_CONF_MBED_TRACE_ENABLE
    virtual const char *get_ril_name(int request);
#endif

private:
    /** Get singleton instance of class RILAdaptation
     *
     *  @return pointer to singleton instance of class RILAdaptation
     */
    static RILAdaptation *get_instance();

    static void request_complete(RIL_Token t, RIL_Errno e, void *response, size_t response_len);
    static void unsolicited_response(int response_id, const void *data, size_t data_len);
    static void request_ack(RIL_Token t);
    static void request_timed_callback(RIL_TimedCallback callback, void *param, const struct timeval *relative_time);

    RIL_CellularDevice *_device;
    const RIL_RadioFunctions *_radio_funcs;
    int _ril_token_id;
};

} /* namespace mbed */

#endif /* RIL_ADAPTATION_H_ */
