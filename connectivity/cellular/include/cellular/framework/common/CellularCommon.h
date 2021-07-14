/*
 * Copyright (c) 2018, Arm Limited and affiliates.
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

#ifndef CELLULAR_COMMON_
#define CELLULAR_COMMON_

#include <stdint.h>
#include "netsocket/nsapi_types.h"

const int CELLULAR_RETRY_ARRAY_SIZE = 10;

struct cell_callback_data_t {
    nsapi_error_t error; /* possible error code */
    int status_data;     /* cellular_event_status related enum or other info in int format. Check cellular_event_status comments.*/
    bool final_try;      /* This flag is true if state machine is used and this was the last try. State machine does goes to idle. */
    const void *data;    /* possible extra data in any form. Format specified in cellular_connection_status_t per event if any. */
    cell_callback_data_t()
    {
        error = NSAPI_ERROR_OK;
        status_data = -1;
        final_try = false;
        data = NULL;
    }
};

struct cell_signal_quality_t {
    int rssi;           /* received signal strength */
    int ber;            /* channel bit error rate */
    cell_signal_quality_t()
    {
        rssi = -1;
        ber = -1;
    }
};

/**
 * Cellular specific event changes.
 * Connect and disconnect are handled via NSAPI_EVENT_CONNECTION_STATUS_CHANGE
 * All enum types have struct *cell_callback_data_t in intptr_t with possible error code in cell_callback_data_t.error.
 * Most enum values also have some enum in cell_callback_data_t.enumeration, check comments below.
 */
typedef enum cellular_event_status {
    CellularDeviceReady                     = NSAPI_EVENT_CELLULAR_STATUS_BASE,     /* Modem is powered and ready to receive commands. cell_callback_data_t.status_data will be -1 */
    CellularSIMStatusChanged                = NSAPI_EVENT_CELLULAR_STATUS_BASE + 1, /* SIM state changed. cell_callback_data_t.status_data will be enum SimState. See enum SimState in ../API/CellularSIM.h*/
    CellularRegistrationStatusChanged       = NSAPI_EVENT_CELLULAR_STATUS_BASE + 2, /* Registering status changed. cell_callback_data_t.status_data will be enum RegistrationStatus. See enum RegistrationStatus in ../API/CellularNetwork.h*/
    CellularRegistrationTypeChanged         = NSAPI_EVENT_CELLULAR_STATUS_BASE + 3, /* Registration type changed. cell_callback_data_t.status_data will be enum RegistrationType. See enum RegistrationType in ../API/CellularNetwork.h*/
    CellularCellIDChanged                   = NSAPI_EVENT_CELLULAR_STATUS_BASE + 4, /* Network Cell ID have changed. cell_callback_data_t.status_data will be int cellid*/
    CellularRadioAccessTechnologyChanged    = NSAPI_EVENT_CELLULAR_STATUS_BASE + 5, /* Network roaming status have changed. cell_callback_data_t.status_data will be enum RadioAccessTechnology See enum RadioAccessTechnology in ../API/CellularNetwork.h*/
    CellularAttachNetwork                   = NSAPI_EVENT_CELLULAR_STATUS_BASE + 6, /* cell_callback_data_t.status_data will be enum AttachStatus. See enum AttachStatus in ../API/CellularNetwork.h */
    CellularActivatePDPContext              = NSAPI_EVENT_CELLULAR_STATUS_BASE + 7, /* NSAPI_ERROR_OK in cell_callback_data_t.error on successfully PDP Context activated or negative error */
    CellularSignalQuality                   = NSAPI_EVENT_CELLULAR_STATUS_BASE + 8, /* cell_callback_data_t.error will contains return value when signal quality was queried. data will hold the pointer to cell_signal_quality struct. See possible values from ../API/CellularNetwork.h*/
    CellularStateRetryEvent                 = NSAPI_EVENT_CELLULAR_STATUS_BASE + 9, /* cell_callback_data_t.error contain an error if any. cell_callback_data_t.status_data contains cellular_event_status and it specifies the operation which is retried.
                                                                                       cellular_event_status.data contains current retrycount */
    CellularDeviceTimeout                   = NSAPI_EVENT_CELLULAR_STATUS_BASE + 10,/* cell_callback_data_t.error contain an error or NSAPI_ERROR_OK,
                                                                                       cell_callback_data_t.status_data contains the current cellular_connection_status_t,
                                                                                       cellular_event_status.data contains new timeout value in milliseconds */
    CellularRILATResponse                   = NSAPI_EVENT_CELLULAR_STATUS_BASE + 11, /* Only available when using RIL layer. This is a (multi-)response to RIL command RIL_REQUEST_SET_FORWARDING_AT_COMMAND. Data field contain AT response (char*) and status_data contains length. */
} cellular_connection_status_t;

#endif // CELLULAR_COMMON_
