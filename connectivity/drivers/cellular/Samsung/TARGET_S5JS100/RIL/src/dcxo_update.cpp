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

#include "modem_io_device.h"
#include "s5js100_dcxo.h"
#include "mbed_trace.h"
#include "mbed_error.h"
#define TRACE_GROUP "DCXO"

#ifndef DCXO_UPDATE_DBG_ON
#define DCXO_UPDATE_DBG_ON      0
#endif
#define DCXO_UPDATE_DBG     if (DCXO_UPDATE_DBG_ON) tr_info

bool g_enable_ctb_loop = false;

/****************************************************************************
 * Public Functions
 ****************************************************************************/
static ModemIoDevice *dcxo_mio;
static Full_set_table_t *ctb_tbl;

void dcxo_update_task(mio_buf *buf, void *data)
{
    ModemIoDevice *mio = dcxo_mio;
    unsigned short force_ctb;
    Nl1cHiu_UpdateInd_t pMsg;

    memcpy(&pMsg, buf->data, sizeof(pMsg));
    free_mio_buf(buf);

    switch (pMsg.header.msgType) {
        case DCXO_CTB_FULLSET_UPDATE:
            DCXO_UPDATE_DBG("DCXO_CTB_FULLSET_UPDATE");
            /* set ACK */
            if (pMsg.header.response != 0x0) {
                DCXO_UPDATE_DBG("CTB FULLSET_UPDATE ack/nck field should be 0x0\n");
                return;
            }

            dcxo_ctb_sem_wait();
            memcpy((void *)ctb_tbl, pMsg.payload.setTable, sizeof(Full_set_table_t)*DCXO_TEMPERATURE_RANGE);
            dcxo_ctb_sem_release();

            if (g_enable_ctb_loop != true) {
                g_enable_ctb_loop = true;
                s5js100_dcxo_start_ctb_loop(ctb_tbl);
            }

            /* setting done and response to CP */
            pMsg.header.response = DCXO_RESPONSE_ACK; /* 4B for response */
            break;

        case DCXO_TEMP_REQUEST:
            /* response TSU value */
            pMsg.header.response = s5js100_tsu_get_temperature();/* GET TSU to decimal ? */
            pMsg.header.response1 = s5js100_dcxo_get_coarse_tune_value();/* GET CTB at same time */
            DCXO_UPDATE_DBG("DCXO_TEMP_REQUEST temperature(%d) coarse_tune_value(%d)", pMsg.header.response - 50000, pMsg.header.response1);
            break;

        case DCXO_CTB_FORCE_REQUEST_FOR_FACTORY_CAL:
            DCXO_UPDATE_DBG("DCXO_CTB_FORCE_REQUEST_FOR_FACTORY_CAL");
            /* setting done and response to CP */
            dcxo_ctb_sem_wait();
            force_ctb = pMsg.payload.resolution; /* 2B for force ctb value */
            s5js100_dcxo_set_tune_value((unsigned int)force_ctb, 4096);
            dcxo_ctb_sem_release();
            break;

        case DCXO_GET_CTB_REQUEST:
            pMsg.header.response = s5js100_dcxo_get_coarse_tune_value(); /* 4B for response */
            DCXO_UPDATE_DBG("DCXO_GET_CTB_REQUEST coarse_tune_value(%d)", pMsg.header.response);
            break;

        case DCXO_GET_TSX_INFO_REQUEST:
            pMsg.header.response = S5JS100_TSX_TYPE; /* 4B for response */
            DCXO_UPDATE_DBG("DCXO_GET_TSX_INFO_REQUEST TSX_INFO(%d)", pMsg.header.response);
            break;

        default:
            DCXO_UPDATE_DBG("dcxo msg type wrong..\n");
            return;
    }

    if (pMsg.header.msgType != DCXO_CTB_FORCE_REQUEST_FOR_FACTORY_CAL) {
        mio->write((char *)(&(pMsg.header)), sizeof(UpdateInd_header_t));
    }
}

extern "C" {

    void dcxo_init(void)
    {
        char mio_name[32] = "dcxo";
        ctb_tbl = (Full_set_table_t *)malloc(sizeof(Full_set_table_t) * DCXO_TEMPERATURE_RANGE);

        dcxo_mio = getModemIoDeviceByName(mio_name);
        dcxo_mio->register_ReadCb(dcxo_update_task, NULL);
    }

}

