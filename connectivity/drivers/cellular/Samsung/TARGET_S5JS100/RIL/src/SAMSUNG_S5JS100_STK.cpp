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

#include "SAMSUNG_S5JS100_Modem_Proxy.h"
#include "SAMSUNG_S5JS100_STK.h"
#include "CellularContext.h"

#include "mbed_trace.h"
#define TRACE_GROUP "STK"

#ifndef SAMSUNG_S5JS100_STK_DBG_ON
#define SAMSUNG_S5JS100_STK_DBG_ON        0
#endif
#define SAMSUNG_S5JS100_STK_DBG        if (SAMSUNG_S5JS100_STK_DBG_ON) tr_info


namespace mbed {
extern SAMSUNG_S5JS100_MODEM_PROXY *gModemProxy;

SAMSUNG_S5JS100_STK *SAMSUNG_S5JS100_STK::GetInstance()
{
    static SAMSUNG_S5JS100_STK *s5js100_stkInstance;

    if (s5js100_stkInstance == NULL) {
        s5js100_stkInstance = new SAMSUNG_S5JS100_STK();
    }
    return s5js100_stkInstance;
}

SAMSUNG_S5JS100_STK::SAMSUNG_S5JS100_STK()
{
    rsp_cb = NULL;
    setup_event_list = NULL;
    setup_event_list_size = 0;
    usat_thread = NULL;
    usat_thread_run = 0;
    tcpudpmode = 0;
    openchannel = 0;
}

SAMSUNG_S5JS100_STK::~SAMSUNG_S5JS100_STK()
{
    if (gModemProxy != NULL) {
        gModemProxy->registerSTK(NULL);
    }
}

int SAMSUNG_S5JS100_STK::RegisterToModemProxy(void)
{
    if (gModemProxy == NULL) {
        return -1;
    }
    gModemProxy->registerSTK(this);
    return 0;
}

void SAMSUNG_S5JS100_STK::OnRequest(STK_msg request, void *data, unsigned int datalen)
{
    if (RegisterToModemProxy() < 0) {
        OnRequestComplete(request, STK_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    switch (request) {
        case SEND_STK_TERMINAL_RSP:
            gModemProxy->sendStkTerminalRsp(data, datalen);
            break;
        case SEND_STK_ENVELOPE_CMD:
            gModemProxy->sendStkEnvelopeCmd(data, datalen);
            break;
        default:
            SAMSUNG_S5JS100_STK_DBG("SAMSUNG_S5JS100_STK::OnRequest Invalid Request (%d)", request);
            break;
    }
}

void SAMSUNG_S5JS100_STK::OnRequestComplete(STK_msg request, STK_Errno e, void *response, unsigned int responselen)
{
    //return ParseMsg(request, e, response, responselen);
    return;
}

void SAMSUNG_S5JS100_STK::OnUnsolicitedResponse(STK_msg unsolResponse, void *data, unsigned int datalen)
{
    return ParseMsg(unsolResponse, STK_E_SUCCESS, data, datalen);
}

typedef struct {
    char *buf;
    unsigned int bufsize;
} recv_buf;
static recv_buf *recv_remain = NULL;
static unsigned int recv_remain_offset = 0;

static recv_buf *alloc_recv_buf(unsigned int size)
{
    recv_buf *ret;

    ret = (recv_buf *)malloc(sizeof(recv_buf));
    if (ret == NULL) {
        return NULL;
    }

    ret->bufsize = size;
    ret->buf = (char *)malloc(size);
    if (ret->buf == NULL) {
        free(ret);
        ret = NULL;
    }

    return ret;
}

static void free_recv_buf(recv_buf *recv)
{
    free(recv->buf);
    free(recv);
}

static Queue<recv_buf, 32> recv_queue;
static Queue<recv_buf, 32> send_queue;
static EventQueue *usat_wqueue;
static Thread *usat_work;
static Mutex usat_mutex;
static unsigned int total_recv_size;

void SAMSUNG_S5JS100_STK::ParseMsg(int stkmsg, STK_Errno e, void *msg, unsigned int msglen)
{
    uicc_proactive_cmd_hrd *cmd_hdr;
    UICC_COMMAND_TYPE cmd_type;
#if SAMSUNG_S5JS100_STK_DBG_ON
    unsigned int i;
    char *tmsg;
    char mm[256] = "";
#endif

    if (e != STK_E_SUCCESS) {
        return;
    }

    if (stkmsg == IND_STK_SESSION_END) {
        tr_debug("SAMSUNG_S5JS100_STK::ParseMsg Session End");
        return;
    }

#if SAMSUNG_S5JS100_STK_DBG_ON
    SAMSUNG_S5JS100_STK_DBG("SAMSUNG_S5JS100_STK::ParseMsg msglen[%d]", msglen);
    tmsg = (char *)msg;
    for (i = 0; i < msglen; i++) {
        sprintf(mm + (i % 16) * 3, "%02X ", tmsg[i]);
        //MODEM_LINK_DEVICE_SHMEM_DBG_ON("%02X ", msg->data[i]);
        if (i % 16 == 15 && i != 0) {
            SAMSUNG_S5JS100_STK_DBG("%s", mm);
        }
    }
    if ((i - 1) % 16 != 15) {
        SAMSUNG_S5JS100_STK_DBG("%s", mm);
    }
#endif

    cmd_hdr = (uicc_proactive_cmd_hrd *)msg;

    if (cmd_hdr->command_tag != BER_TLV_PROACTIVE_COMMAND) {
        tr_error("[ParseMsg] Not BER_TLV_PROACTIVE_COMMAND");
        return;
    }

    /* Parse message */
    if ((cmd_hdr->command_details.tag & 0x7F) == BER_COMP_command_detail_tag) {
        cmd_type = (UICC_COMMAND_TYPE)(cmd_hdr->command_details.type);
    } else if (cmd_hdr->short_length == 0x81) {
        cmd_type = (UICC_COMMAND_TYPE)((char *)msg)[6];
    } else {
        return;
    }

    switch (cmd_type) {
        case UICC_COMMAND_REFRESH:
            proactive_refresh(msg, msglen);
            break;
        case UICC_COMMAND_SETUP_EVENT_LIST:
            proactive_setup_event_list(msg, msglen);
            break;
        case UICC_COMMAND_OPEN_CHANNEL:
            proactive_open_channel(msg, msglen);
            break;
        case UICC_COMMAND_CLOSE_CHANNEL:
            proactive_close_channel(msg, msglen);
            break;
        case UICC_COMMAND_RECEIVE_DATA:
            proactive_receive_data(msg, msglen);
            break;
        case UICC_COMMAND_SEND_DATA:
            proactive_send_data(msg, msglen);
            break;
        case UICC_COMMAND_GET_CHANNEL_STATUS:
            proactive_get_channel_status(msg, msglen);
            break;
        default:
            tr_error("[ParseMsg] Not supported command (0x%02X)", cmd_type);
            break;
    }

    if (rsp_cb != NULL) {
        return rsp_cb(msg, msglen);
    }
}

static int check_device_identy(uicc_device_identities *identy)
{
    if (identy->tag != 0x82 || identy->length != 0x02 ||
            identy->source_device != 0x81) {
        return -1;
    }

    return 0;
}

int SAMSUNG_S5JS100_STK::proactive_refresh(void *msg, unsigned int msglen)
{
    SAMSUNG_S5JS100_STK_DBG("%s", __func__);

    return 0;
}

int SAMSUNG_S5JS100_STK::proactive_setup_event_list(void *msg, unsigned int msglen)
{
    uicc_setup_event_list m;
    unsigned char *b = (unsigned char *)msg;
    unsigned int offset = 0;
    int i;
    char setup_event_list_success[12] = {0x81, 0x03, 0x01, 0x05, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};

    m.hrd = (uicc_proactive_cmd_hrd *)b;
    offset += sizeof(uicc_proactive_cmd_hrd);
    m.device_identy = (uicc_device_identities *)(b + offset);
    offset += m.device_identy->length + 2;
    m.event_list = (uicc_event_list *)(b + offset);

    if (check_device_identy(m.device_identy)) {
        tr_error("%s device_identy fail", __func__);
        return -1;
    }

    if (m.event_list->tag != 0x99) {
        tr_error("%s event_list tag fail", __func__);
        return -1;
    }

    if (setup_event_list != NULL) {
        free(setup_event_list);
        setup_event_list_size = 0;
    }

    setup_event_list = (uicc_event_list_event *)malloc(m.event_list->length);
    setup_event_list_size = m.event_list->length;
    for (i = 0; i < m.event_list->length; i++) {
        setup_event_list[i] = (uicc_event_list_event)(m.event_list->events[i]);
    }


    SAMSUNG_S5JS100_STK_DBG("%s", __func__);

    SAMSUNG_S5JS100_STK::GetInstance()->OnRequest(SEND_STK_TERMINAL_RSP, setup_event_list_success, 12);

    return 0;
}

static char open_channel_success[48] = {
    0x81, 0x03, 0x01, 0x40, 0x01, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00, 0x38, 0x02, 0x81, 0x00,
};
static unsigned int open_channel_success_tr_size = 16;

static char close_channel_success[12] = {0x81, 0x03, 0x01, 0x41, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
static unsigned int close_req = 0;

void SAMSUNG_S5JS100_STK::usat_task(void)
{
    nsapi_size_or_error_t retcode;
    SocketAddress sock_addr;
    SAMSUNG_S5JS100_STK *stk = GetInstance();
    char *tbuf;
    recv_buf *t;
    unsigned int wait_count = 0;

    usat_wqueue = new EventQueue();
    usat_work = new rtos::Thread(osPriorityNormal, 2048, NULL, "usat_work");
    usat_work->start(callback(usat_wqueue, &events::EventQueue::dispatch_forever));

    switch (stk->connection.protocol_type) {
        case 0x02:
        case 0x03:
        case 0x05:
            stk->tcpudpmode = 0;
            break;
        case 0x01:
        case 0x04:
            stk->tcpudpmode = 1;
            break;
        default:
            tr_error("SAMSUNG_S5JS100_STK::usat_task Undefined protocol type (0x%X)\n", stk->connection.protocol_type);
            return;
    }

    stk->connection.context = (CellularContext *)(NetworkInterface::get_default_instance());
    if (stk->connection.apn[0] != 0) {
        stk->connection.context->set_credentials((char *)(stk->connection.apn), (char *)(stk->connection.login), (char *)(stk->connection.password));
        stk->connection.context->connect();
    }

    SAMSUNG_S5JS100_STK_DBG("Open socket ip: %s port:%d\n", (char *)(stk->connection.dest_address), stk->connection.port);
    sock_addr.set_port(stk->connection.port);
    sock_addr.set_ip_address((char *)(stk->connection.dest_address));

    if (stk->tcpudpmode) {
        retcode = stk->connection.udp.open(stk->connection.context);
        while (retcode != NSAPI_ERROR_OK) {
            tr_error("UDPSocket.open() fails & retry, code: %d\n", retcode);
            ThisThread::sleep_for(1000);
            retcode = stk->connection.udp.open(stk->connection.context);
        }
        stk->connection.udp.set_timeout(5000);
        retcode = stk->connection.udp.connect(sock_addr);
        while (retcode != NSAPI_ERROR_OK) {
            tr_error("UDPSocket.connect() fails & retry, code: %d\n", retcode);
            ThisThread::sleep_for(1000);
            retcode = stk->connection.udp.connect(sock_addr);
        }
    } else {
        retcode = stk->connection.tcp.open(stk->connection.context);
        while (retcode != NSAPI_ERROR_OK) {
            tr_error("TCPSocket.open() fails & retry, code: %d\n", retcode);
            ThisThread::sleep_for(1000);
            retcode = stk->connection.tcp.open(stk->connection.context);
        }
        stk->connection.tcp.set_timeout(5000);
        retcode = stk->connection.tcp.connect(sock_addr);
        while (retcode != NSAPI_ERROR_OK) {
            tr_error("TCPSocket.connect() fails & retry, code: %d\n", retcode);
            ThisThread::sleep_for(1000);
            stk->connection.tcp.close();
            stk->connection.tcp.open(stk->connection.context);
            retcode = stk->connection.tcp.connect(sock_addr);
            SAMSUNG_S5JS100_STK_DBG("Retry Result(%d) IP: %s Port: %d", retcode, sock_addr.get_ip_address(), sock_addr.get_port());
        }
        SAMSUNG_S5JS100_STK_DBG("Link(%d) IP: %s Port: %d", retcode, sock_addr.get_ip_address(), sock_addr.get_port());
    }

    stk->OnRequest(SEND_STK_TERMINAL_RSP, open_channel_success, open_channel_success_tr_size);

    tbuf = (char *)malloc(1024);
    if (tbuf == NULL) {
        tr_error("USAT task malloc FAIL!");
        goto taskout;
    }

    usat_mutex.lock();
    stk->openchannel = 1;
    total_recv_size = 0;
    usat_mutex.unlock();

    while (stk->usat_thread_run == 1) {
        if (stk->tcpudpmode) {
            retcode = stk->connection.udp.recv(tbuf, 1024);
        } else {
            retcode = stk->connection.tcp.recv(tbuf, 1024);
        }

        if (retcode < NSAPI_ERROR_OK) {
            if (retcode == NSAPI_ERROR_WOULD_BLOCK) {
                if (stk->connection.context->get_connection_status() == NSAPI_STATUS_DISCONNECTED) {
                    usat_mutex.lock();
                    stk->openchannel = 0;
                    usat_mutex.unlock();
                    stk->envelope_channel_status(0x01, 5, 0x21, (unsigned char *)("0000"));
                    break;
                }
                continue;
            }

            tr_error("STK DATA receive error (%d)", retcode);
            continue;
        }
        wait_count = 0;

        usat_mutex.lock();
        total_recv_size += retcode;
        usat_mutex.unlock();

        while ((t = alloc_recv_buf(retcode)) == NULL) {
            ThisThread::sleep_for(1000);
        }

        memcpy(t->buf, tbuf, retcode);
        t->bufsize = retcode;
        recv_queue.put(t);
        stk->envelope_data_available(0, 0, retcode);
    }

    usat_mutex.lock();
    stk->openchannel = 0;
    usat_mutex.unlock();

    free(tbuf);

taskout:
    if (stk->tcpudpmode) {
        stk->connection.tcp.close();
    } else {
        stk->connection.udp.close();
    }

    if (close_req == 1) {
        close_req = 0;
        stk->OnRequest(SEND_STK_TERMINAL_RSP, close_channel_success, sizeof(close_channel_success));
    }

    if ((stk->connection.apn[0]) != 0) {
        stk->connection.context->disconnect();
    }

    usat_work->terminate();
    delete usat_work;
    delete usat_wqueue;

    SAMSUNG_S5JS100_STK_DBG("USAT TASK END!!");
}

int SAMSUNG_S5JS100_STK::proactive_open_channel(void *msg, unsigned int msglen)
{
    uicc_open_channel_default m;
    unsigned char *b = (unsigned char *)msg;
    unsigned int offset = 0;
    char tbuf[48];
    unsigned int t;

    m.hrd = (uicc_proactive_cmd_hrd *)b;
    offset += sizeof(uicc_proactive_cmd_hrd);
    m.device_identy = (uicc_device_identities *)(b + offset);
    offset += m.device_identy->length + 2;

    m.alpha_identifier = (uicc_alpha_identifier *)(b + offset);
    if ((m.alpha_identifier->tag & 0x7F) == BER_COMP_alpha_identifier_tag) {
        offset += m.alpha_identifier->length + 2;
    } else {
        m.alpha_identifier = NULL;
    }

    m.icon_identifier = (uicc_icon_identifier *)(b + offset);
    if ((m.icon_identifier->tag & 0x7F) == BER_COMP_icon_identifier_tag) {
        offset += m.icon_identifier->length + 2;
    } else {
        m.icon_identifier = NULL;
    }

    m.bearer_description = (uicc_bearer_description *)(b + offset);
    offset += m.bearer_description->length + 2;

    m.buffer_size = (uicc_buffer_size *)(b + offset);
    offset += m.buffer_size->length + 2;

    m.local_address = (uicc_other_address *)(b + offset);
    if ((m.local_address->tag & 0x7F) == BER_COMP_other_address_tag) {
        offset += m.local_address->length + 2;
    } else {
        m.local_address = NULL;
    }

    m.network_access_name = (uicc_network_access_name *)(b + offset);
    if ((m.network_access_name->tag & 0x7F) == BER_COMP_nerwork_access_name_tag) {
        offset += m.network_access_name->length + 2;
    } else {
        m.network_access_name = NULL;
    }

    m.user_login = (uicc_text_string *)(b + offset);
    if ((m.user_login->tag & 0x7F) == BER_COMP_text_string_tag) {
        offset += m.user_login->length + 2;
    } else {
        m.user_login = NULL;
    }

    m.user_password = (uicc_text_string *)(b + offset);
    if ((m.user_password->tag & 0x7F) == BER_COMP_text_string_tag) {
        offset += m.user_password->length + 2;
    } else {
        m.user_password = NULL;
    }

    m.uicc_terminal_interface_transport_level = (uicc_uicc_terminal_interface_transport_level *)(b + offset);
    if ((m.uicc_terminal_interface_transport_level->tag & 0x7F) == BER_COMP_uicc_uicc_terminal_interface_transport_level) {
        offset += m.uicc_terminal_interface_transport_level->length + 2;
    } else {
        m.uicc_terminal_interface_transport_level = NULL;
    }

    m.data_destination_address = (uicc_other_address *)(b + offset);
    if ((m.data_destination_address->tag & 0x7F) == BER_COMP_other_address_tag) {
        offset += m.data_destination_address->length + 2;
    } else {
        m.data_destination_address = NULL;
    }

    m.text_attribute = (uicc_text_attribute *)(b + offset);
    if ((m.text_attribute->tag & 0x7F) == BER_COMP_text_string_tag) {
        offset += m.text_attribute->length + 2;
    } else {
        m.text_attribute = NULL;
    }

    m.frame_identifier = (uicc_frame_identifier *)(b + offset);
    if ((m.frame_identifier->tag & 0x7F) != BER_COMP_frame_identifier_tag) {
        m.frame_identifier = NULL;
    }

    if (check_device_identy(m.device_identy)) {
        tr_error("%s device_identy fail", __func__);
        return -1;
    }

    SAMSUNG_S5JS100_STK_DBG("[%s]", __func__);
    SAMSUNG_S5JS100_STK_DBG("Bearer type   : 0x%X", (m.bearer_description == NULL) ? 0 : m.bearer_description->bearer_type);
    SAMSUNG_S5JS100_STK_DBG("Buffer size   : 0x%X", (m.buffer_size == NULL) ? 0 : m.buffer_size->buffer_size);
    if (m.local_address == NULL) {
        if (m.network_access_name == NULL) {
            SAMSUNG_S5JS100_STK_DBG("Local address : (null)");
            connection.apn[0] = 0;
        } else {
            memcpy(tbuf, (char *)(m.network_access_name->network_access_name), sizeof(tbuf));
            t = 0;
            while (t < m.network_access_name->length && t < sizeof(tbuf)) {
                tbuf[t] = '.';
                t += m.network_access_name->network_access_name[t] + 1;
            }
            t = m.network_access_name->length < sizeof(tbuf) - 1 ? m.network_access_name->length : sizeof(tbuf) - 1;
            tbuf[t] = 0;
            strcpy((char *)(connection.apn), tbuf + 1);

            SAMSUNG_S5JS100_STK_DBG("Network name  : %s", (char *)(connection.apn));
        }
    } else {
        SAMSUNG_S5JS100_STK_DBG("Local address : %d.%d.%d.%d", m.local_address->address[0], m.local_address->address[1], m.local_address->address[2], m.local_address->address[3]);
    }
    memset(tbuf, 0, 32);
    if (m.user_login != NULL) {
        memcpy(tbuf, m.user_login->text_string, m.user_login->length - 1);
    }
    tbuf[31] = 0;
    SAMSUNG_S5JS100_STK_DBG("User login (%d): %s", m.user_login->length, tbuf);

    memset(tbuf, 0, 32);
    if (m.user_password != NULL) {
        memcpy(tbuf, m.user_password->text_string, m.user_password->length - 1);
    }
    tbuf[31] = 0;
    SAMSUNG_S5JS100_STK_DBG("User paswd (%d): %s", m.user_password->length, tbuf);
    SAMSUNG_S5JS100_STK_DBG("protocol type : 0x%X", (m.uicc_terminal_interface_transport_level == NULL) ? 0 : m.uicc_terminal_interface_transport_level->transport_protocol_type);
    SAMSUNG_S5JS100_STK_DBG("port number   : %d", (m.uicc_terminal_interface_transport_level == NULL) ? 0 : (unsigned short)(m.uicc_terminal_interface_transport_level->port_number[0]) << 8 | (unsigned short)(m.uicc_terminal_interface_transport_level->port_number[1]));
    if (m.data_destination_address == NULL) {
        SAMSUNG_S5JS100_STK_DBG("Dest  address : (null)");
    } else {
        SAMSUNG_S5JS100_STK_DBG("Dst addr type : %s", (m.data_destination_address->type_of_address == 0x21) ? "IPV4" : "IPV6");
        if (m.data_destination_address->type_of_address == 0x21) {
            SAMSUNG_S5JS100_STK_DBG("Dest  address : %d.%d.%d.%d", m.data_destination_address->address[0], m.data_destination_address->address[1], m.data_destination_address->address[2], m.data_destination_address->address[3]);
        } else {
            SAMSUNG_S5JS100_STK_DBG("Dest  address : %d.%d.%d.%d.%d.%d", m.data_destination_address->address[0], m.data_destination_address->address[1], m.data_destination_address->address[2], m.data_destination_address->address[3],
                                    m.data_destination_address->address[5], m.data_destination_address->address[6]);
        }
    }

    memcpy(open_channel_success + 16, (char *)(m.bearer_description), m.bearer_description->length + 2 + m.buffer_size->length + 2);
    open_channel_success_tr_size += (m.bearer_description->length + 2 + m.buffer_size->length + 2);

    if (m.uicc_terminal_interface_transport_level != NULL) {
        connection.protocol_type = m.uicc_terminal_interface_transport_level->transport_protocol_type;
        connection.port = (unsigned short)(m.uicc_terminal_interface_transport_level->port_number[0]) << 8 | (unsigned short)(m.uicc_terminal_interface_transport_level->port_number[1]);
    }
    if (m.local_address != NULL) {
        if (m.data_destination_address->type_of_address == 0x21) {
            sprintf((char *)(connection.local_address), "%d.%d.%d.%d",
                    m.local_address->address[0], m.local_address->address[1],
                    m.local_address->address[2], m.local_address->address[3]);
        } else {
            sprintf((char *)(connection.local_address), "%d.%d.%d.%d.%d.%d",
                    m.local_address->address[0], m.local_address->address[1],
                    m.local_address->address[2], m.local_address->address[3],
                    m.local_address->address[5], m.local_address->address[6]);
        }
    }
    if (m.data_destination_address != NULL) {
        if (m.data_destination_address->type_of_address == 0x21) {
            sprintf((char *)(connection.dest_address), "%d.%d.%d.%d",
                    m.data_destination_address->address[0], m.data_destination_address->address[1],
                    m.data_destination_address->address[2], m.data_destination_address->address[3]);
        } else {
            sprintf((char *)(connection.dest_address), "%d.%d.%d.%d.%d.%d",
                    m.data_destination_address->address[0], m.data_destination_address->address[1],
                    m.data_destination_address->address[2], m.data_destination_address->address[3],
                    m.data_destination_address->address[5], m.data_destination_address->address[6]);
        }
    }
    if (m.user_login != NULL) {
        memcpy((char *)(connection.login), (char *)(m.user_login->text_string), m.user_login->length - 1);
        connection.login[m.user_login->length - 1] = 0;
    }
    if (m.user_password != NULL) {
        memcpy((char *)(connection.password), (char *)(m.user_password->text_string), m.user_password->length - 1);
        connection.password[m.user_password->length - 1] = 0;
    }
    connection.received_size = 0;

    open_channel_success[2] = (char)(m.hrd->command_details.number);

    usat_thread_run = 1;
    usat_thread = new rtos::Thread(osPriorityNormal, 2048, NULL, "usat_task");
    usat_thread->start(this->usat_task);

    return 0;
}

int SAMSUNG_S5JS100_STK::proactive_close_channel(void *msg, unsigned int msglen)
{
    uicc_close_channel m;
    unsigned char *b = (unsigned char *)msg;
    unsigned int offset = 0;

    m.hrd = (uicc_proactive_cmd_hrd *)b;
    offset += sizeof(uicc_proactive_cmd_hrd);
    m.device_identy = (uicc_device_identities *)(b + offset);
    offset += m.device_identy->length + 2;
    m.alpha_identifier = (uicc_alpha_identifier *)(b + offset);
    offset += m.alpha_identifier->length + 2;
    m.icon_identifier = (uicc_icon_identifier *)(b + offset);
    offset += m.icon_identifier->length + 2;
    m.text_attribute = (uicc_text_attribute *)(b + offset);
    offset += m.text_attribute->length + 2;
    m.frame_identifier = (uicc_frame_identifier *)(b + offset);

    if (check_device_identy(m.device_identy)) {
        tr_error("%s device_identy fail", __func__);
        return -1;
    }

    close_channel_success[2] = (char)(m.hrd->command_details.number);
    usat_thread_run = 0;
    close_req = 1;

    SAMSUNG_S5JS100_STK_DBG("[%s]", __func__);

    return 0;
}

#define RECV_HRD_SIZE    15
static char receive_data_success[RECV_HRD_SIZE + 256 + 3] = {0x81, 0x03, 0x01, 0x42, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00, 0xb6, 0x81, 0x00};
static void *receive_msg;
static unsigned int receive_msglen;

void receive_work_task(void)
{
    uicc_receive_data m;
    unsigned char *b = (unsigned char *)receive_msg;
    unsigned int offset = 0;
    unsigned int receive_msg_size = 0;
    unsigned recv_length;
    osEvent evt;
    recv_buf *rbuf;
    unsigned int short_length_mode = 0;

    m.hrd = (uicc_proactive_cmd_hrd *)b;
    offset += sizeof(uicc_proactive_cmd_hrd);
    m.device_identy = (uicc_device_identities *)(b + offset);
    offset += m.device_identy->length + 2;

    m.alpha_identifier = (uicc_alpha_identifier *)(b + offset);
    if ((m.alpha_identifier->tag & 0x7F) == BER_COMP_alpha_identifier_tag) {
        offset += m.alpha_identifier->length + 2;
    } else {
        m.alpha_identifier = NULL;
    }

    m.icon_identifier = (uicc_icon_identifier *)(b + offset);
    if ((m.icon_identifier->tag & 0x7F) == BER_COMP_icon_identifier_tag) {
        offset += m.icon_identifier->length + 2;
    } else {
        m.icon_identifier = NULL;
    }

    m.channel_data_length = (uicc_channel_data_length *)(b + offset);
    offset += m.channel_data_length->length + 2;

    m.text_attribute = (uicc_text_attribute *)(b + offset);
    if ((m.text_attribute->tag & 0x7F) == BER_COMP_text_string_tag) {
        offset += m.text_attribute->length + 2;
    } else {
        m.text_attribute = NULL;
    }

    m.frame_identifier = (uicc_frame_identifier *)(b + offset);
    if ((m.frame_identifier->tag & 0x7F) != BER_COMP_frame_identifier_tag) {
        m.frame_identifier = NULL;
    }

    if (check_device_identy(m.device_identy)) {
        tr_error("%s device_identy fail", __func__);
        free(receive_msg);
        return;
    }

    recv_length = m.channel_data_length->channel_data_length;
    receive_data_success[2] = (char)(m.hrd->command_details.number);
    if (recv_length <= 127) {
        receive_data_success[RECV_HRD_SIZE - 2] = (char)(recv_length & 0xFF);
        short_length_mode = 1;
    } else {
        receive_data_success[RECV_HRD_SIZE - 2] = 0x81;
        receive_data_success[RECV_HRD_SIZE - 1] = (char)(recv_length & 0xFF);
    }

    SAMSUNG_S5JS100_STK_DBG("[%s] receive data size (%d)", __func__,  m.channel_data_length->channel_data_length);

    if (recv_remain != NULL) {
        if (recv_remain->bufsize - recv_remain_offset > recv_length) {
            memcpy(receive_data_success + RECV_HRD_SIZE - short_length_mode, recv_remain->buf + recv_remain_offset, recv_length);
            receive_msg_size += recv_length;
            recv_remain_offset += recv_length;
        } else {
            memcpy(receive_data_success + RECV_HRD_SIZE - short_length_mode, recv_remain->buf + recv_remain_offset, recv_remain->bufsize - recv_remain_offset);
            receive_msg_size += recv_remain->bufsize - recv_remain_offset;
            free_recv_buf(recv_remain);
            recv_remain_offset = 0;
            recv_remain = NULL;
        }
    }

    while (receive_msg_size  < recv_length) {
        evt = recv_queue.get(osWaitForever);
        if (evt.status != (osStatus)osEventMessage) {
            break;
        }

        rbuf = (recv_buf *)(evt.value.p);
        if (recv_length >= rbuf->bufsize + receive_msg_size) {
            memcpy(receive_data_success + RECV_HRD_SIZE - short_length_mode + receive_msg_size, rbuf->buf, rbuf->bufsize);
            receive_msg_size += rbuf->bufsize;
            free_recv_buf(rbuf);
            recv_remain_offset = 0;
            recv_remain = NULL;
        } else {
            memcpy(receive_data_success + RECV_HRD_SIZE - short_length_mode + receive_msg_size, rbuf->buf, recv_length - receive_msg_size);
            recv_remain_offset = recv_length - receive_msg_size;
            recv_remain = rbuf;
            receive_msg_size += recv_remain_offset;
        }
    }

    receive_msg_size += RECV_HRD_SIZE - short_length_mode;
    receive_data_success[receive_msg_size] = 0xB7;
    receive_data_success[receive_msg_size + 1] = 0x01;

    usat_mutex.lock();
    total_recv_size -= receive_msg_size - RECV_HRD_SIZE - short_length_mode;

    if (total_recv_size > 0xFF) {
        receive_data_success[receive_msg_size + 2] = 0xFF;
    } else {
        receive_data_success[receive_msg_size + 2] = (char)(total_recv_size & 0xFF);
    }
    usat_mutex.unlock();

    receive_msg_size += 3;

    SAMSUNG_S5JS100_STK::GetInstance()->OnRequest(SEND_STK_TERMINAL_RSP, receive_data_success, receive_msg_size);

    free(receive_msg);
}

int SAMSUNG_S5JS100_STK::proactive_receive_data(void *msg, unsigned int msglen)
{
    while ((receive_msg = (void *)malloc(msglen)) == NULL) {
        ThisThread::sleep_for(1000);
    }
    receive_msglen = msglen;
    memcpy(receive_msg, msg, msglen);
    usat_wqueue->call(receive_work_task);

    return 0;
}

static void *send_msg;
static unsigned int send_msglen;
static char *send_stored_buf = NULL;
static unsigned int send_stored_size = 0;

void send_work_task(void)
{
    uicc_send_data m;
    unsigned char *b = (unsigned char *)send_msg;
    unsigned int offset = 0;
    char send_data_success[15] = {0x81, 0x03, 0x01, 0x43, 0x01, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00, 0xb7, 0x01, 0xff};
    SAMSUNG_S5JS100_STK *stk = SAMSUNG_S5JS100_STK::GetInstance();
    unsigned int channel_status;
    uicc_command_details *details = NULL;

    m.hrd = (uicc_proactive_cmd_hrd *)b;
    offset += sizeof(uicc_proactive_cmd_hrd);
    if (m.hrd->short_length == 0x81) {
        details = (uicc_command_details *)(b + 3);
        offset++;
    } else {
        details = (uicc_command_details *)(b + 2);
    }

    m.device_identy = (uicc_device_identities *)(b + offset);
    offset += m.device_identy->length + 2;

    m.alpha_identifier = (uicc_alpha_identifier *)(b + offset);
    if ((m.alpha_identifier->tag & 0x7F) == BER_COMP_alpha_identifier_tag) {
        offset += m.alpha_identifier->length + 2;
    } else {
        m.alpha_identifier = NULL;
    }

    m.icon_identifier = (uicc_icon_identifier *)(b + offset);
    if ((m.icon_identifier->tag & 0x7F) == BER_COMP_icon_identifier_tag) {
        offset += m.icon_identifier->length + 2;
    } else {
        m.icon_identifier = NULL;
    }

    m.channel_data = (uicc_channel_data *)(b + offset);
    offset += m.channel_data->length + 2;

    m.text_attribute = (uicc_text_attribute *)(b + offset);
    if ((m.text_attribute->tag & 0x7F) == BER_COMP_text_string_tag) {
        offset += m.text_attribute->length + 2;
    } else {
        m.text_attribute = NULL;
    }

    m.frame_identifier = (uicc_frame_identifier *)(b + offset);
    if ((m.frame_identifier->tag & 0x7F) != BER_COMP_frame_identifier_tag) {
        m.frame_identifier = NULL;
    }

    if (check_device_identy(m.device_identy)) {
        tr_error("%s device_identy fail", __func__);
        free(send_msg);
        return;
    }

    send_data_success[4] = details->qualifier;

    if (m.hrd->short_length != 0x81) {
        send_data_success[2] = (char)(m.hrd->command_details.number);
    } else {
        send_data_success[2] = b[5];
    }

    channel_status = 0;
    while (channel_status != 1) {
        usat_mutex.lock();
        channel_status = stk->openchannel;
        usat_mutex.unlock();
        ThisThread::sleep_for(1000);
    }

    if (details->qualifier == 1) {
        if (send_stored_size == 0) {
            if (stk->tcpudpmode) {
                stk->connection.udp.send(m.channel_data->channel_data_string, m.channel_data->length);
            } else {
                stk->connection.tcp.send(m.channel_data->channel_data_string, m.channel_data->length);
            }
        } else {
            memcpy(send_stored_buf + send_stored_size, m.channel_data->channel_data_string, m.channel_data->length);
            send_stored_size += m.channel_data->length;
            if (stk->tcpudpmode) {
                stk->connection.udp.send(send_stored_buf, send_stored_size);
            } else {
                stk->connection.tcp.send(send_stored_buf, send_stored_size);
            }

            free(send_stored_buf);
            send_stored_size = 0;
        }
    } else {
        while ((send_stored_buf = (char *)malloc(4096)) == NULL) {
            ThisThread::sleep_for(1000);
        }
        memcpy(send_stored_buf + send_stored_size, m.channel_data->channel_data_string, m.channel_data->length);
        send_stored_size += m.channel_data->length;
    }
    stk->OnRequest(SEND_STK_TERMINAL_RSP, send_data_success, sizeof(send_data_success));

    free(send_msg);
}

int SAMSUNG_S5JS100_STK::proactive_send_data(void *msg, unsigned int msglen)
{
    while ((send_msg = (void *)malloc(msglen)) == NULL) {
        ThisThread::sleep_for(1000);
    }
    memcpy(send_msg, msg, msglen);
    send_msglen = msglen;
    usat_wqueue->call(send_work_task);

    return 0;
}

int SAMSUNG_S5JS100_STK::proactive_get_channel_status(void *msg, unsigned int msglen)
{
    uicc_get_channel_status m;
    unsigned char *b = (unsigned char *)msg;
    unsigned int offset = 0;
    char get_status_response[16] = {0x81, 0x03, 0x01, 0x44, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00, 0xb8, 0x02, 0x81, 0x00};

    m.hrd = (uicc_proactive_cmd_hrd *)b;
    offset += sizeof(uicc_proactive_cmd_hrd);
    m.device_identy = (uicc_device_identities *)(b + offset);

    if (check_device_identy(m.device_identy)) {
        tr_error("%s device_identy fail", __func__);
        return -1;
    }

    get_status_response[2] = (char)(m.hrd->command_details.number);
    get_status_response[14] = (openchannel == 1) ? 0x81 : 0x01;
    get_status_response[15] = (openchannel == 1) ? 0x00 : 0x05;

    SAMSUNG_S5JS100_STK_DBG("[%s]", __func__);

    SAMSUNG_S5JS100_STK::GetInstance()->OnRequest(SEND_STK_TERMINAL_RSP, get_status_response, sizeof(get_status_response));

    return 0;
}

/* TS 102 223 V12 7.5.10.2 */
int SAMSUNG_S5JS100_STK::envelope_data_available(unsigned char channel_status0, unsigned char channel_status1, unsigned int receive_data_length)
{
    unsigned char total_length = 16;
    static unsigned char envelope_data_available_msg[17] = {
        BER_TLV_EVENT_DOWNLOAD, 14,
        BER_COMP_event_list_tag, 0x01, UICC_EVENT_Data_available,
        BER_COMP_device_identity_tag, 0x02, 0x82, 0x81,
        BER_COMP_channel_status_tag, 0x02, 0x81, 0x00,
        BER_COMP_channel_data_length_tag, 0x01, 0x01,
    };

    if (receive_data_length >= 255) {
        envelope_data_available_msg[15] = 255;
    } else {
        envelope_data_available_msg[15] = receive_data_length;
    }

    SAMSUNG_S5JS100_STK::GetInstance()->OnRequest(SEND_STK_ENVELOPE_CMD, (void *)envelope_data_available_msg, total_length);

    return 0;
}

/* TS 102 223 V12 7.5.11.2 */
int SAMSUNG_S5JS100_STK::envelope_channel_status(unsigned char channel_status0, unsigned char channel_status1, unsigned char addr_type, unsigned char *ipaddr)
{
    unsigned char total_length = 13;
    static unsigned char envelope_channel_status_msg[13] = {
        BER_TLV_EVENT_DOWNLOAD, 11,
        0x99, 0x01, UICC_EVENT_Channel_status,
        0x82, 0x02, 0x82, 0x81,
        0xB8, 0x02, 0x01, 0x00,
    };

    envelope_channel_status_msg[11] = channel_status0;
    envelope_channel_status_msg[12] = channel_status1;

    SAMSUNG_S5JS100_STK::GetInstance()->OnRequest(SEND_STK_ENVELOPE_CMD, (void *)envelope_channel_status_msg, total_length);

    return 0;
}

}
