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

#ifndef __MODEM_IO_DEVICE_H__
#define __MODEM_IO_DEVICE_H__


#include "pbuf.h"
#include "exynos_ipc.h"
#include "modem_link_device_shmem.h"

typedef enum {
    LINK_MODE_OFFLINE,
    LINK_MODE_RELEASED,
    LINK_MODE_IPC,
} link_mode;


class ModemIoDevice {
public:
    char name[32];
    int id;
    ShmemIpcDevice *ipc;
    link_mode mode;
    unsigned char ch_cnt;
    void (*read_cb)(mio_buf *, void *);
    void *cb_data;

public:
    virtual void IoReadCallback(mio_buf *) { }
    virtual void open(void) { }
    virtual void close(void) { }
    virtual int read(char *buffer, int buflen)
    {
        return 0;
    }
    int write(const char *buffer, int buflen);
    virtual int poll(int setup)
    {
        return 0;
    }
    int ioctl(int cmd, unsigned long arg);
    void register_ReadCb(void (*cb)(mio_buf *, void *), void *data)
    {
        read_cb = cb;
        cb_data = data;
    }

    unsigned short exynos_build_fr_config(unsigned int count);
    void exynos_build_header(unsigned char *buff, unsigned short cfg, unsigned char ctl, int count);
};

class IoDevMisc : public ModemIoDevice {
public:
    Queue<mio_buf, 32> rxq;
    mio_buf remain;
    Semaphore *r_sem;

public:
    IoDevMisc(char *io_name, int ch, ShmemIpcDevice *ipc_dev)
    {
        strncpy(name, io_name, 31);
        id = ch;
        ipc = ipc_dev;
        mode = LINK_MODE_OFFLINE;
        ch_cnt = 0;
        read_cb = NULL;
    }
    void IoReadCallback(mio_buf *);
    void  open(void) { }
    void close(void) { }
    int read(char *buffer, int buflen);
    int poll(int setup);
};

class IoDevNet : public ModemIoDevice {
public:
    IoDevNet(char *io_name, int ch, ShmemIpcDevice *ipc_dev)
    {
        strncpy(name, io_name, 31);
        id = ch;
        ipc = ipc_dev;
        mode = LINK_MODE_OFFLINE;
        ch_cnt = 0;
        read_cb = NULL;
    }
    void IoReadCallback(mio_buf *);
    void  open(void) { }
    void close(void) { }
    int read(char *buffer, int buflen);
    int poll(int setup)
    {
        return 0;
    }
};


#define IOC_VOID            0x20000000UL    /* no parameters */
#define _IO(x, y)           (IOC_VOID|((x)<<8)|(y))

#define IOCTL_MODEM_ON          _IO('o', 0x19)
#define IOCTL_MODEM_OFF         _IO('o', 0x20)
#define IOCTL_MODEM_RESET       _IO('o', 0x21)
#define IOCTL_MODEM_BOOT_ON     _IO('o', 0x22)
#define IOCTL_MODEM_BOOT_OFF        _IO('o', 0x23)
#define IOCTL_MODEM_BOOT_DONE       _IO('o', 0x24)
#define IOCTL_MODEM_SAVE_RELEASE    _IO('o', 0x38)
#define IOCTL_MODEM_START_SAVE      _IO('o', 0x39)

#define IOCTL_MODEM_PROTOCOL_SUSPEND    _IO('o', 0x25)
#define IOCTL_MODEM_PROTOCOL_RESUME _IO('o', 0x26)

#define IOCTL_MODEM_STATUS      _IO('o', 0x27)
#define IOCTL_MODEM_DL_START        _IO('o', 0x28)
#define IOCTL_MODEM_FW_UPDATE       _IO('o', 0x29)

#define IOCTL_MODEM_NET_SUSPEND     _IO('o', 0x30)
#define IOCTL_MODEM_NET_RESUME      _IO('o', 0x31)

#define IOCTL_MODEM_DUMP_START      _IO('o', 0x32)
#define IOCTL_MODEM_DUMP_UPDATE     _IO('o', 0x33)
#define IOCTL_MODEM_FORCE_CRASH_EXIT    _IO('o', 0x34)
#define IOCTL_MODEM_CP_UPLOAD       _IO('o', 0x35)
#define IOCTL_MODEM_DUMP_RESET      _IO('o', 0x36)

#define IOCTL_MODEM_SWITCH_MODEM    _IO('o', 0x37)

#define IOCTL_MODEM_RAMDUMP_START   _IO('o', 0xCE)
#define IOCTL_MODEM_RAMDUMP_STOP    _IO('o', 0xCF)

#define IOCTL_MODEM_XMIT_BOOT       _IO('o', 0x40)
#define IOCTL_MODEM_GET_SHMEM_INFO  _IO('o', 0x41)

/* ioctl command for IPC Logger */
#define IOCTL_MIF_LOG_DUMP      _IO('o', 0x51)

#define IOCTL_SHMEM_FULL_DUMP       _IO('o', 0x54)  /* For shmem dump */
#define IOCTL_VSS_FULL_DUMP     _IO('o', 0x57)  /* For vss dump */
#define IOCTL_ACPM_FULL_DUMP        _IO('o', 0x58)  /* For acpm memory dump */

/* ioctcl command for fast CP Boot */
#define IOCTL_SEC_CP_INIT       _IO('o', 0x61)
#define IOCTL_CHECK_SECURITY        _IO('o', 0x62)
#define IOCTL_XMIT_BIN          _IO('o', 0x63)


extern void registerModemIoDevice(ModemIoDevice *io);
extern ModemIoDevice *getModemIoDeviceByName(char *devname);
extern ModemIoDevice *getModemIoDeviceById(int ch);


#endif

