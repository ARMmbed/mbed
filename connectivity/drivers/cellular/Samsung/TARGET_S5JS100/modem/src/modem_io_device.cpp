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

#include <string.h>
#include "modem_io_device.h"
#include "modem_prj.h"
#include "modem_link_device_shmem.h"
#include "mbed_trace.h"
#include "rtos/rtos.h"

#define TRACE_GROUP "IODEV"

#ifndef MODEM_IO_DEVICE_DBG_ON
#define MODEM_IO_DEVICE_DBG_ON      0
#endif
#define MODEM_IO_DEVICE_DBG     if (MODEM_IO_DEVICE_DBG_ON) tr_info

#define MODEM_IO_DEVICE_MAX sizeof(modem_io_deivce_list)/sizeof(ModemIoDevice *)

ShmemIpcFmtDevice *ipc_fmt_dev = ShmemIpcFmtDevice::getInstance();
ShmemIpcRawDevice *ipc_raw_dev = ShmemIpcRawDevice::getInstance();

IoDevMisc   IoDev_ModemProxy((char *)"ModemProxy",  245,    ipc_fmt_dev);
IoDevNet    IoDev_rmnet1((char *)"rmnet1",  1,  ipc_raw_dev);
IoDevMisc   IoDev_shmem_save((char *)"shmem_save",  243,    ipc_raw_dev);
IoDevMisc   IoDev_dcxo((char *)"dcxo",      244,    ipc_raw_dev);
IoDevMisc   IoDev_if5((char *)"if5",        242,    ipc_raw_dev);
IoDevNet    IoDev_rmnet2((char *)"rmnet2",  2,  ipc_raw_dev);
IoDevNet    IoDev_rmnet3((char *)"rmnet3",  3,  ipc_raw_dev);
IoDevNet    IoDev_rmnet4((char *)"rmnet4",  4,  ipc_raw_dev);
IoDevMisc   IoDev_rmnet6((char *)"rmnet6",  6,  ipc_raw_dev);
IoDevMisc   IoDev_rmnet7((char *)"rmnet7",  7,  ipc_raw_dev);
IoDevMisc   IoDev_rmnet8((char *)"rmnet8",  8,  ipc_raw_dev);
IoDevMisc   IoDev_rmnet9((char *)"rmnet9",  9,  ipc_raw_dev);


ModemIoDevice *modem_io_deivce_list[] = {
    &IoDev_ModemProxy,
    &IoDev_rmnet1,
    &IoDev_shmem_save,
    &IoDev_dcxo,
    &IoDev_if5,
    &IoDev_rmnet2,
    &IoDev_rmnet3,
    &IoDev_rmnet4,
    &IoDev_rmnet6,
    &IoDev_rmnet7,
    &IoDev_rmnet8,
    &IoDev_rmnet9,
};

ModemIoDevice *getModemIoDeviceByName(char *name)
{
    unsigned int i;

    for (i = 0; i < MODEM_IO_DEVICE_MAX; i++) {
        if (modem_io_deivce_list[i] && !strncmp(modem_io_deivce_list[i]->name, name, 31)) {
            return modem_io_deivce_list[i];
        }
    }
    return NULL;
}

ModemIoDevice *getModemIoDeviceById(int ch)
{
    unsigned int i;

    for (i = 0; i < MODEM_IO_DEVICE_MAX; i++) {
        if (modem_io_deivce_list[i] != NULL) {
            if (modem_io_deivce_list[i]->id == ch) {
                return modem_io_deivce_list[i];
            }
        }
    }

    return NULL;
}

int ModemIoDevice::ioctl(int cmd, unsigned long arg)
{
    return 0;
}

unsigned short ModemIoDevice::exynos_build_fr_config(unsigned int count)
{
    unsigned short fr_cfg = 0;

    if ((count + EXYNOS_HEADER_SIZE) <= 0xFFFF) {
        fr_cfg |= (EXYNOS_SINGLE_MASK << 8);
    }

    return fr_cfg;
}

int ModemIoDevice::write(const char *buf, int count)
{
    mio_buf *mio;

    MODEM_IO_DEVICE_DBG("%s:%d count(%d)\n", __func__, __LINE__, count);


    mio = (mio_buf *)alloc_mio_buf(0);
    if (!mio) {
        return 0;
    }

    mio->header.hdr.start = EXYNOS_START_MASK;
    mio->header.hdr.frm_seq = 0; //Handled later in IPC
    mio->header.hdr.frag_cfg = exynos_build_fr_config(count);
    mio->header.hdr.msg_len = (unsigned short)(EXYNOS_HEADER_SIZE + count);
    mio->header.hdr.ch_id = id;
    mio->header.hdr.ch_seq = ++ch_cnt;


    mio->data = (unsigned char *)buf;
    mio->len = count;
    mio->ch = id;
    mio->pad = exynos_calc_padding_size(EXYNOS_HEADER_SIZE + count);

    ipc->send(mio);
    return count;

}

void IoDevMisc::IoReadCallback(mio_buf *buf)
{
    MODEM_IO_DEVICE_DBG("%s:%d\n", __func__, __LINE__);
    if (read_cb != NULL) {
        MODEM_IO_DEVICE_DBG("%s:%d name: %s\n", __func__, __LINE__, name);
        read_cb(buf, cb_data);
        return;
    }
    MODEM_IO_DEVICE_DBG("%s:%d\n", __func__, __LINE__);

    if (rxq.full()) {
        free_mio_buf(buf);
        return;
    }
    MODEM_IO_DEVICE_DBG("%s:%d\n", __func__, __LINE__);

    rxq.put(buf);
}

int IoDevMisc::read(char *buf, int count)
{
    int copied = 0;
    int tmp = 0;
    osEvent evt;
    mio_buf *mio;
    int diff;

    if (remain.len) {
        tmp = remain.len > count ? count : remain.len;
        memcpy(buf, remain.data, tmp);
        remain.len -= tmp;
        if (remain.len == 0) {
            free(remain.data);
        }
        count -= tmp;
        if (!count) {
            return tmp;
        }
    }

    if (rxq.empty()) {
        return tmp;
    }

    evt = rxq.get(osWaitForever);

    if (evt.status != (osStatus)osEventMessage) {
        return tmp;
    }

    mio = (mio_buf *)(evt.value.p);
    copied += mio->len > count ? count : mio->len;
    memcpy(buf, mio->data, copied);

    diff = mio->len - copied;
    if (diff > 0) {
        remain.data = (unsigned char *)malloc(diff);
        if (remain.data) {
            memcpy(remain.data, mio->data + copied, diff);
            remain.len = diff;
        }
    }

    free(mio->data);
    free(mio);

    return copied + tmp;
}

int IoDevMisc::poll(int setup)
{
    MODEM_IO_DEVICE_DBG("%s:%d\n", __func__, __LINE__);
    r_sem->try_acquire_for(osWaitForever);

    return 0;
}

void IoDevNet::IoReadCallback(mio_buf *buf)
{
    MODEM_IO_DEVICE_DBG("%s:%d\n", __func__, __LINE__);
    if (read_cb != NULL) {
        MODEM_IO_DEVICE_DBG("%s:%d\n", __func__, __LINE__);
        read_cb(buf, cb_data);
        free_mio_buf(buf);
        return;
    }
    MODEM_IO_DEVICE_DBG("%s:%d\n", __func__, __LINE__);
    free_mio_buf(buf);
}

int IoDevNet::read(char *buf, int count)
{
    return 0;
}

