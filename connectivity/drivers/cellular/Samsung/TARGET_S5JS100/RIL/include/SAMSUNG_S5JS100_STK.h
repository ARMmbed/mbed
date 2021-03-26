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

#ifndef __SAMSUNG_S5JS100_STK_H__
#define __SAMSUNG_S5JS100_STK_H__

#include "usat_des.h"
#include "CellularContext.h"

namespace mbed {

typedef enum {
    SEND_STK_TERMINAL_RSP = 0,
    SEND_STK_ENVELOPE_CMD = 1,

    STK_REPONSE_OFFSET = 1000,
    RSP_SEND_STK_TERMINAL_RSP = SEND_STK_TERMINAL_RSP + STK_REPONSE_OFFSET,
    RSP_SEND_STK_ENVELOPE_CMD = SEND_STK_ENVELOPE_CMD + STK_REPONSE_OFFSET,

    IND_SIM_REFRESH = 2000,
    IND_STK_PROACTIVE_COMMAND = 2001,
    IND_STK_SESSION_END = 2002,
} STK_msg;

typedef enum {
    STK_E_SUCCESS = 0,

    STK_E_GENERIC_FAILURE = 1,
} STK_Errno;

typedef struct {
    CellularContext *context;
    TCPSocket tcp;
    UDPSocket udp;
    unsigned char protocol_type;
    unsigned short port;
    unsigned char apn[48];
    unsigned char local_address[24];
    unsigned char dest_address[24];
    unsigned char login[48];
    unsigned char password[48];
    unsigned int received_size;
} usat_connection;

class SAMSUNG_S5JS100_STK {
private:
    int RegisterToModemProxy(void);
    int proactive_refresh(void *msg, unsigned int msglen);
    int proactive_setup_event_list(void *msg, unsigned int msglen);
    int proactive_open_channel(void *msg, unsigned int msglen);
    int proactive_close_channel(void *msg, unsigned int msglen);
    int proactive_receive_data(void *msg, unsigned int msglen);
    int proactive_send_data(void *msg, unsigned int msglen);
    int proactive_get_channel_status(void *msg, unsigned int msglen);
    int envelope_data_available(unsigned char channel_status0, unsigned char channel_status1, unsigned int receive_data_length);
    int envelope_channel_status(unsigned char channel_status0, unsigned char channel_status1, unsigned char addr_type, unsigned char *ipaddr);

    static void usat_task(void);

public:
    uicc_event_list_event *setup_event_list;
    int setup_event_list_size;
    void (*rsp_cb)(void *msg, unsigned int msglen);
    Thread *usat_thread;
    unsigned int usat_thread_run;
    usat_connection connection;
    int tcpudpmode;
    int openchannel;

public:
    SAMSUNG_S5JS100_STK();
    ~SAMSUNG_S5JS100_STK();

    void RegisterRspCB(void (*cb)(void *msg, unsigned int msglen))
    {
        rsp_cb = cb;
    }

    void OnRequest(STK_msg request, void *data, unsigned int datalen);
    void OnRequestComplete(STK_msg t, STK_Errno e, void *response, unsigned int responselen);
    void OnUnsolicitedResponse(STK_msg unsolResponse, void *data, unsigned int datalen);

    void ParseMsg(int stk_msg, STK_Errno e, void *data, unsigned int datalen);

    static SAMSUNG_S5JS100_STK *GetInstance();
};

}

#endif
