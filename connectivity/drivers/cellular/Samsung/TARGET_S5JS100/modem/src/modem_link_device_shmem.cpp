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
#include "modem_link_device_shmem.h"
#include "modem_io_device.h"
#include "modem_prj.h"
#include "s5js100_error.h"
#include "s5js100_type.h"

#define SZ_64K       0x00010000

#ifndef IPCRAM_PADDR
#define IPCRAM_PADDR 0x10000000
#endif
#ifndef IPC_REGION_SIZE
#define IPC_REGION_SIZE SZ_64K
#endif
#if IPC_REGION_SIZE != SZ_64K
#error "Check IPC_REGION_SIZE! Refer to struct shmem_64KB_phys_map"
#endif

#include "mbed_trace.h"
#define TRACE_GROUP "SMEM"

#ifndef MODEM_LINK_DEVICE_SHMEM_DBG_ON
#define MODEM_LINK_DEVICE_SHMEM_DBG_ON      0
#endif
#define MODEM_LINK_DEVICE_SHMEM_DBG     if (MODEM_LINK_DEVICE_SHMEM_DBG_ON) tr_info

#define SHMEM_CIRC_BUFFER_ERROR         (-10)




static void shmem_irq_handler(void *data);
static void shmem_save_irq_handler(void *data);
static void shmem_stop_req_irq_handler(void *data);

ShmemLinkDevice *pShmemLinkDevice = new ShmemLinkDevice();



int ShmemIpcDevice::get_txq_space(void)
{
    int head = *txq.head;
    int tail = *txq.tail;
    int space;

    if ((head >= txq.size) || (tail >= txq.size)) {
        return SHMEM_CIRC_BUFFER_ERROR;
    }

    space = tail - head - 1;
    if (space < 0) {
        space += txq.size;
    }

    return space;
}

void ShmemIpcDevice::write_ipc_to_txq(mio_buf *mio)
{
    unsigned int qsize = txq.size;
    unsigned int in = *txq.head;
    unsigned int *buff = (unsigned int *)txq.buff;
    t_msg_header *header = &(mio->header);
    unsigned int *src = (unsigned int *)(mio->data);
    unsigned int len = mio->len;
    unsigned int pad = mio->pad;
    unsigned int i, j;

    header->hdr.frm_seq = ++header_cnt;

    /* Write HEADER to the TXQ, assume that with padding aligned on 4 bytes */
    i = in >> 2;
    for (j = 0; j < EXYNOS_HEADER_SIZE / 4; j++) {
        buff[i] = header->int_hdr[j];
        i ++;
        if (i ==  qsize >> 2) {
            i = 0;
        }
        MBED_ASSERT(i < qsize >> 2);
    }

    in += EXYNOS_HEADER_SIZE;
    if (in >= qsize) {
        in -= qsize;
    }

    /* Write data to the TXQ, assume that with padding aligned on 4 bytes */
    i = in >> 2;
    for (j = 0; j < (len + pad) / 4; j ++) {
        buff[i] = src[j];
        i ++;
        if (i ==  qsize >> 2) {
            i = 0;
        }
        MBED_ASSERT(i < qsize >> 2);
    }

    in += len + pad;
    if (in >= qsize) {
        in -= qsize;
    }

    *txq.head = in;
}

int ShmemIpcDevice::xmit_ipc_msg(mio_buf *msg)
{
//    shmem_circ_status circ;
    int space;
    int copied = 0;
    bool chk_nospc = false;
#if MODEM_LINK_DEVICE_SHMEM_DBG_ON
    int i;
    char mm[96] = "";
#endif

    MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d", __func__, __LINE__);
#if MODEM_LINK_DEVICE_SHMEM_DBG_ON
    MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d ch[%d] len[%d]", __func__, __LINE__, msg->ch, msg->len);
    for (i = 0; i < msg->len; i++) {
        sprintf(mm + (i % 16) * 3, "%02X ", msg->data[i]);
        //MODEM_LINK_DEVICE_SHMEM_DBG("%02X ", msg->data[i]);
        if (i % 16 == 15 && i != 0) {
            MODEM_LINK_DEVICE_SHMEM_DBG("%s", mm);
        }
    }
    if ((i - 1) % 16 != 15) {
        MODEM_LINK_DEVICE_SHMEM_DBG("%s", mm);
    }
#endif


    while (1) {
        /* Acquire the spin lock for a TXQ */
        tx_lock->lock();

        /* Get the size of free space in the TXQ */
        space = get_txq_space();
        if (space < 0) {
            /* TXQ ERROR */
            copied = -EIO;
            tx_lock->unlock();
            return copied;//break;
        }

        /* TO DO - check free memory space if it fits all headres !!!
        Check the free space size,
          - FMT : comparing with mxb->len
          - RAW : check used buffer size  */
        chk_nospc = (space < msg->len) ? true : false;

        if (chk_nospc) {
            /* TO DO - implement this back pressure to operate properly. */
            tr_error("No TX space for ch[%d] %s:%d\n", msg->ch, __func__, __LINE__);
            /* Set res_required flag for the "dev" */
            res_required = 1;
            /* Take the mxb back to the mxb_txq */
            //mbuf_queue_head(txq, mxb);
            tx_lock->unlock();
            ThisThread::sleep_for(100);
        } else {
            break;
        }
    }

    /* TX only when there is enough space in the TXQ */
    write_ipc_to_txq(msg);
    copied += msg->len;
    free_mio_buf(msg);

    /* Release the spin lock */
    tx_lock->unlock();

    return copied;
}

int ShmemIpcDevice::send(mio_buf *buf)
{
    int ret;

    MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d", __func__, __LINE__);
    ret = xmit_ipc_msg(buf);
    if (ret > 0) {
        MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d", __func__, __LINE__);
        pShmemLinkDevice->send_int2cp(INT_NON_CMD(mask_send));
    }

    return 0;
}

void ShmemIpcDevice::send_cmd(unsigned short cmd)
{
    ShmemLinkDevice *sld = pShmemLinkDevice;

    sld->send_int2cp(cmd);
}

void ShmemIpcDevice::save_release(void)
{
    ShmemLinkDevice *sld = pShmemLinkDevice;

    sld->save_sem_release();
}

int ShmemIpcDevice::ioctl(unsigned int cmd, unsigned long arg)
{
    return 0;
}

int ShmemIpcDevice::check_security(unsigned long arg)
{
    return 0;
}

int ShmemIpcDevice::sec_init(unsigned long arg)
{
    return 0;
}


unsigned int ShmemLinkDevice::get_mbx_cp2ap_msg(void)
{
    return mbox_get_value(mbox.mbx_cp2ap_msg);
}

unsigned int ShmemLinkDevice::get_mbx_ap2cp_msg(void)
{
    return mbox_get_value(mbox.mbx_ap2cp_msg);
}

unsigned int ShmemLinkDevice::get_mbx_cp2ap_status(void)
{
    return mbox_get_value(mbox.mbx_cp2ap_status);
}

unsigned int ShmemLinkDevice::get_mbx_ap2cp_status(void)
{
    return mbox_get_value(mbox.mbx_ap2cp_status);
}

unsigned int ShmemLinkDevice::get_mbx_magic(void)
{
    unsigned int ret = getreg32(shmem_magic);
    return ret;
}

unsigned int ShmemLinkDevice::get_mbx_access(void)
{
    unsigned int ret = getreg32(shmem_access);
    return ret;
}

void ShmemLinkDevice::send_int2cp(unsigned short mask)
{
    mbox_set_value(mbox.mbx_ap2cp_msg, mask);
    mbox_set_interrupt(mbox.int_ap2cp_msg);
}

void ShmemLinkDevice::save_sem_release()
{
    save_sem->release();
}

int ShmemLinkDevice::init(void)
{
    if (get_mbx_magic() == SHM_IPC_MAGIC && get_mbx_access() == 1 && state == STATE_ONLINE) {
        return 0;    // Already initialized
    }

    putreg32(0, shmem_access);
    putreg32(SHM_IPC_MAGIC, shmem_magic);
    putreg32(1, shmem_access);
    state = STATE_ONLINE;

    return 0;
}


ShmemLinkDevice::ShmemLinkDevice()
{
    ipc_device[IPC_FMT] = ShmemIpcFmtDevice::getInstance();
    ipc_device[IPC_RAW] = ShmemIpcRawDevice::getInstance();

    shmem_size = IPC_REGION_SIZE;
    shmem_base = (void *)IPCRAM_PADDR;
    shmem_magic = (unsigned int *) & (((struct shmem_64KB_phys_map *)IPCRAM_PADDR)->magic);
    shmem_access = (unsigned int *) & (((struct shmem_64KB_phys_map *)IPCRAM_PADDR)->access);
    memset(shmem_base, 0, shmem_size);

    /* Initialize locks, completions, and bottom halves */
    save_sem = new Semaphore(0, 1);
    stop_sem = new Semaphore(0, 1);
    ipc_rx_sem = new Semaphore(0, 512);


    mbox_ipc_register_handler(mbox.irq_cp2ap_msg, shmem_irq_handler, NULL);
    mbox_ipc_register_handler(mbox.save_irq, shmem_save_irq_handler, NULL);
    mbox_ipc_register_handler(mbox.stop_irq, shmem_stop_req_irq_handler, NULL);
}

ShmemLinkDevice::~ShmemLinkDevice()
{
    mbed_error_printf("need to add code here ,%s\n\r", __func__);
}

int ShmemLinkDevice::ShmemLinkDevice_stop(void)
{
    NVIC_DisableIRQ(S5JS100_IRQ_MCPU_WDT);
    NVIC_ClearPendingIRQ(S5JS100_IRQ_MCPU_WDT);

    NVIC_DisableIRQ(S5JS100_IRQ_MAILBOX_AP_INT);

    mcpu_reset();

    return 0;
}

void ShmemLinkDevice::mcpu_wd_task(void)
{
//    ShmemLinkDevice *ld = pShmemLinkDevice;
    tr_error("MCPU WD");
    ShmemLinkDevice_restart();
}

static void cp_wd_handler(int32_t irq, void *context, void *arg)
{
    pShmemLinkDevice->modem_link_device_wqueue->call(pShmemLinkDevice, &ShmemLinkDevice::mcpu_wd_task);
    NVIC_DisableIRQ(S5JS100_IRQ_MCPU_WDT);
    NVIC_ClearPendingIRQ(S5JS100_IRQ_MCPU_WDT);
}


void ShmemLinkDevice::mcpu_failed_to_start(void)
{
//    ShmemLinkDevice *ld = pShmemLinkDevice;
    mcpu_failed_to_start_id = 0;

    tr_error("MCPU Failed to start");
#if 0
    ld->ShmemLinkDevice_restart();
#else
    NVIC_DisableIRQ(S5JS100_IRQ_MCPU_WDT);
    NVIC_ClearPendingIRQ(S5JS100_IRQ_MCPU_WDT);
    NVIC_DisableIRQ(S5JS100_IRQ_MAILBOX_AP_INT);
    mcpu_reset();

    mbox_init();
    putreg32(0, shmem_access);
    putreg32(0, shmem_magic);
    setstate(STATE_OFFLINE);

    mcpu_init(MCPU_CP);

    NVIC_SetVector(S5JS100_IRQ_MCPU_WDT, (uint32_t)cp_wd_handler);
    NVIC_EnableIRQ(S5JS100_IRQ_MCPU_WDT);

    mcpu_failed_to_start_id = modem_link_device_wqueue->call_in(3000, this, &ShmemLinkDevice::mcpu_failed_to_start);
    tr_info("MCPU Restart try ... %x", mcpu_failed_to_start_id);
#endif
}


/* To avoid race conditions start of RX/TX threads should be done
*  only after constructor is finished and ponter on new object is returned
*/

int ShmemLinkDevice::ShmemLinkDevice_start(void)
{
    shmem_start = 1;
    static int init = 0;

    NVIC_SetVector(S5JS100_IRQ_MCPU_WDT, (uint32_t)cp_wd_handler);
    NVIC_EnableIRQ(S5JS100_IRQ_MCPU_WDT);

    if (!init) {
        init = 1;
        MODEM_LINK_DEVICE_SHMEM_DBG("%s\n", __func__);
        msgrxwork = new rtos::Thread(osPriorityNormal, 2048, NULL, "msgrxwork");
        msgrxwork->start(callback(this, &ShmemLinkDevice::msg_rx_work));

        modem_link_device_wqueue = new EventQueue(4 * EVENTS_EVENT_SIZE, NULL);
        irqwork = new rtos::Thread(osPriorityNormal, 2048, NULL, "ipc_rx_task");
        irqwork->start(callback(modem_link_device_wqueue, &events::EventQueue::dispatch_forever));
    }
    mcpu_failed_to_start_id = modem_link_device_wqueue->call_in(3000, this, &ShmemLinkDevice::mcpu_failed_to_start);
    tr_info("modem start %x", mcpu_failed_to_start_id);

    return 0;
}

int ShmemLinkDevice::ShmemLinkDevice_restart(void)
{

    /*
    To emulate:
        "AT+FCRASHDMP=1" => generate CRASH
        "AT+FCRASHDMP=2" => generate WDOG
    */
#if 0
    NVIC_DisableIRQ(S5JS100_IRQ_MCPU_WDT);
    NVIC_ClearPendingIRQ(S5JS100_IRQ_MCPU_WDT);
    NVIC_DisableIRQ(S5JS100_IRQ_MAILBOX_AP_INT);
    mcpu_reset();
    /* TODO add code to restart RIL, Stack, etc ... */





    /* Start of Modem */
    mbox_init();
    putreg32(0, shmem_access);
    putreg32(0, shmem_magic);
    state = STATE_OFFLINE;

    mcpu_init(MCPU_CP);

    NVIC_SetVector(S5JS100_IRQ_MCPU_WDT, (uint32_t)cp_wd_handler);
    NVIC_EnableIRQ(S5JS100_IRQ_MCPU_WDT);

    /* Add extra code to complete restart */


#endif
    // Notify App about failure ...

    if ((shmem_start) && (fail_cb)) {
        fail_cb();
    }

    shmem_start = 0;
    return 0;
}

int ShmemLinkDevice::init_shmem_save(void)
{
    int i;

    if (get_mbx_magic() == SHM_SAVE_MAGIC && get_mbx_access() == 1) {
        return 0;
    }

    putreg32(0, shmem_access);

    for (i = 0; i < MAX_IPC_DEV; i++) {
        putreg32(0, ipc_device[i]->txq.head);
        putreg32(0, ipc_device[i]->txq.tail);
        putreg32(0, ipc_device[i]->rxq.head);
        putreg32(0, ipc_device[i]->rxq.tail);
        ipc_device[i]->res_required = 0;
    }

    putreg32(SHM_SAVE_MAGIC, shmem_magic);
    putreg32(1, shmem_access);

    if (get_mbx_magic() != SHM_SAVE_MAGIC || get_mbx_access() != 1) {
        return -13;
    }

    state = STATE_NV_CS_SAVE;

    return 0;
}


void ShmemIpcDevice::init(void)
{
    putreg32(0, txq.head);
    putreg32(0, txq.tail);
    putreg32(0, rxq.head);
    putreg32(0, rxq.tail);

    res_required = 0;
}


void ShmemIpcDevice::done_req_ack(void)
{
    //MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d\n", __func__, __LINE__);

    if (req_ack_rcvd < 0) {
        req_ack_rcvd = 0;
    }

    if (req_ack_rcvd == 0) {
        return;
    }

    pShmemLinkDevice->send_int2cp(INT_NON_CMD(mask_res_ack));
    req_ack_rcvd--;

}

void ShmemIpcDevice::recv_res_ack(unsigned short intr)
{
    /*
    This method is used to nodify about receiving response
    from CP on req_ack sent earlier to confirm CPis alive
    In fact we never sent is, so nothing to receive.
    */
    if (intr & mask_res_ack) {
        /* We should not get here!!! */
        while (1);
    }
}

void ShmemIpcDevice::recv_req_ack(unsigned short intr)
{
    //MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d\n", __func__, __LINE__);

    if (intr & mask_req_ack) {
        req_ack_rcvd++;
    }
}


ShmemIpcFmtDevice::ShmemIpcFmtDevice()
{
    struct shmem_64KB_phys_map *map;

    map = (struct shmem_64KB_phys_map *)IPCRAM_PADDR;

    strncpy(name, "FMT", 15);

    txq.head = &(map->fmt_tx_head);
    txq.tail = &(map->fmt_tx_tail);
    txq.buff = map->fmt_tx_buff;
    txq.size = SHM_64K_FMT_TX_BUFF_SZ;

    rxq.head = &(map->fmt_rx_head);
    rxq.tail = &(map->fmt_rx_tail);
    rxq.buff = map->fmt_rx_buff;
    rxq.size = SHM_64K_FMT_RX_BUFF_SZ;

    mask_req_ack = INT_MASK_REQ_ACK_F;
    mask_res_ack = INT_MASK_RES_ACK_F;
    mask_send = INT_MASK_SEND_F;

    res_required = 0;

    tx_lock = new Mutex("IPC_FMT");
    req_ack_cmpl = new Semaphore(1, 1);

    header_cnt = 0;
}

ShmemIpcFmtDevice *ShmemIpcFmtDevice::getInstance(void)
{
    static ShmemIpcFmtDevice *s_ipcfmtdevice;

    if (s_ipcfmtdevice == NULL) {
        s_ipcfmtdevice = new ShmemIpcFmtDevice();
    }

    return s_ipcfmtdevice;
}

int ShmemIpcDevice::rx_ipc_frames(void)
{
    unsigned int i;

    /* buffer indeces are corrupted*/
    if ((*rxq.head >= rxq.size) || (*rxq.tail >= rxq.size)) {
        return -5;
    }
    /* nothing to read */
    if (*rxq.head == *rxq.tail) {
        return 0;
    }

    ShmemLinkDevice *ld = pShmemLinkDevice;

    /**
     * variables for the status of the circular queue
     */
    t_msg_header hdr;

    /**
     * variables for RX processing
     */
    int rcvd;   /* size of data in the RXQ or error */
    int rest;   /* size of the rest data        */
    int out;   /* index to the start of current frame  */
    unsigned int tot;    /* total length including padding data  */

    rcvd = *rxq.head - *rxq.tail;
    if (rcvd < 0) {
        rcvd += rxq.size;
    }

    rest = rcvd;
    out = *rxq.tail;

    while (rest > 0) {
        mio_buf *msg;

        if (rest < EXYNOS_HEADER_SIZE) {
            return -75;
        }

        for (i = 0; i < EXYNOS_HEADER_SIZE / sizeof(unsigned int); i ++) {
            hdr.int_hdr[i] = rxq.buff[out / sizeof(unsigned int)];
            out += sizeof(unsigned int);
            if (out >= rxq.size) {
                out = 0;
            }
        }

        /* Check the config field in the header */
        if (hdr.hdr.start != EXYNOS_START_MASK) {
            goto bad_msg;
        }

        /* Verify the total length of the frame (data + padding) */
        tot = exynos_get_total_len(hdr.hdr.msg_len);

        if (tot > rest) {
            goto bad_msg;
        }

        msg = alloc_mio_buf(tot - EXYNOS_HEADER_SIZE);
        for (i = 0; i < (tot - EXYNOS_HEADER_SIZE) / 4; i ++) {
            ((unsigned int *)(msg->data))[i] = rxq.buff[out / sizeof(unsigned int)];
            out += 4;
            if (out >= rxq.size) {
                out = 0;
            }
        }


        msg->ch = hdr.hdr.ch_id;

        if (msg->ch == 254) {
            msg->len = tot - EXYNOS_HEADER_SIZE;
        } else {
            /* Use actual frame len to pass real payload length to upper layer */
            msg->len = hdr.hdr.msg_len - EXYNOS_HEADER_SIZE;
        }

        ld->ipc_rxq.put(msg);
        ld->ipc_rx_sem->release();

        /* Calculate new out value */
        rest -= tot;

    }

    /* Update tail (out) pointer to empty out the RXQ */
    *rxq.tail = out;

    return rcvd;

bad_msg:
#ifdef CONFIG_DEBUG_MODEM_IF
    //mifdbg("SHMEM: ERR! rcvd:%d tot:%d rest:%d\n", rcvd, tot, rest);
#endif
    return -74;
}

ShmemIpcRawDevice::ShmemIpcRawDevice()
{
    struct shmem_64KB_phys_map *map;

    map = (struct shmem_64KB_phys_map *)IPCRAM_PADDR;

    memmove(name, "RAW", strlen("RAW"));

    txq.head = &(map->raw_tx_head);
    txq.tail = &(map->raw_tx_tail);
    txq.buff = map->raw_tx_buff;
    txq.size = SHM_64K_RAW_TX_BUFF_SZ;

    rxq.head = &(map->raw_rx_head);
    rxq.tail = &(map->raw_rx_tail);
    rxq.buff = map->raw_rx_buff;
    rxq.size = SHM_64K_RAW_RX_BUFF_SZ;

    mask_req_ack = INT_MASK_REQ_ACK_R;
    mask_res_ack = INT_MASK_RES_ACK_R;
    mask_send = INT_MASK_SEND_R;

    res_required = 0;

    req_ack_cmpl = new Semaphore(1, 1);
    tx_lock = new Mutex("IPC_RAW");

    header_cnt = 0;
}

ShmemIpcRawDevice *ShmemIpcRawDevice::getInstance(void)
{
    static ShmemIpcRawDevice *s_ipclawdevice;

    if (s_ipclawdevice == NULL) {
        s_ipclawdevice = new ShmemIpcRawDevice();
    }

    return s_ipclawdevice;
}


/*--------------------------------*/

/**
 * cmd_phone_start_handler
 *
 * Handles the PHONE_START command from a CP.
 */
void ShmemLinkDevice::rx_cmd_phone_start(void)
{
    int i;

    init();

    for (i = 0; i < MAX_IPC_DEV; i++) {
        ipc_device[i]->init();
    }

    send_int2cp(INT_CMD(INT_CMD_INIT_END));

    if (!(get_mbx_cp2ap_status() & STAT_LTE_ACTIVE)) {
        if (mcpu_failed_to_start_id != 0) {
            modem_link_device_wqueue->cancel(mcpu_failed_to_start_id);
            mcpu_failed_to_start_id = 0;
        }
        modem_link_device_wqueue->call(this, &ShmemLinkDevice::mcpu_failed_to_start);
        tr_error("Modem is not ready, CP2AP status %x", get_mbx_cp2ap_status());
    }
}

void ShmemLinkDevice::rx_cmd_cp_crash(void)
{

    tr_error("CP Crash");
    ShmemLinkDevice_restart();
}

void ShmemLinkDevice::cmd_handler(unsigned int cmd)
{

    switch (INT_CMD_MASK(cmd)) {

        case INT_CMD_PHONE_START:
            rx_cmd_phone_start();
            break;
        case INT_CMD_CRASH_EXIT:
            rx_cmd_cp_crash();
            break;
        default:
            //mifdbg("SHMEM: unknown command 0x%04X\n", cmd);
            break;
    }
}


static void shmem_irq_handler(void *data)
{
    pShmemLinkDevice->modem_link_device_wqueue->call(pShmemLinkDevice, &ShmemLinkDevice::ipc_rx_task);
}

void ShmemLinkDevice::ipc_shmem_save_task(void)
{
    ShmemLinkDevice *ld = pShmemLinkDevice;
    mio_buf *msg;

    msg = alloc_mio_buf(12);
    if (msg == NULL) {
        return;
    }

    ld->init_shmem_save();

    msg->ch = 243;
    strcpy((char *)(msg->data), "shmem_start");

    ld->ipc_rxq.put(msg);
    ld->ipc_rx_sem->release();
}

static void shmem_save_irq_handler(void *data)
{
    pShmemLinkDevice->modem_link_device_wqueue->call(pShmemLinkDevice, &ShmemLinkDevice::ipc_shmem_save_task);
}

static void shmem_stop_req_irq_handler(void *data)
{
    /* DO SOMETHING here */
}

void ShmemLinkDevice::msg_handler(void)
{
    ShmemLinkDevice *ld = pShmemLinkDevice;
    ShmemIpcDevice *ipc;
    int i;
    int ret;
    unsigned int magic;

    //MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d  head[] = {%d, %d}, tail[] = {%d, %d}\n", __func__, __LINE__, head[0], head[1], tail[0], tail[1]);

    /* Is IPC active ? */
    if (ld->get_mbx_access() != 1) {
        system_halt();
    }
    magic = ld->get_mbx_magic();
    if (magic != SHM_IPC_MAGIC && magic != SHM_SAVE_MAGIC) {
        system_halt();
    }

    /* Skip RX processing if there is no data in the RXQ */
    /* Read data in the RXQ */
    /* Process REQ_ACK (At this point, the RXQ may be empty.) */
    for (i = 0; i < MAX_IPC_DEV; i++) {
        ipc = ld->ipc_device[i];
        ret = ipc->rx_ipc_frames();

        /* TODO review below case and handle it properly!!! */
        /* In case of error just return. Shall we do aything here? Shall we report crash???*/
        if (ret < 0)
            while (1);

        ipc->done_req_ack();
        if (mcpu_failed_to_start_id != 0) {
            modem_link_device_wqueue->cancel(mcpu_failed_to_start_id);
            mcpu_failed_to_start_id = 0;
        }
    }
}

void ShmemLinkDevice::ipc_rx_task(void)
{
    ShmemLinkDevice *ld = pShmemLinkDevice;
    unsigned int int2ap;
    int i;

    /* get_shmem_status */
    int2ap = ld->get_mbx_cp2ap_msg();

    /* Process a SHMEM command */
    if (INT_CMD_VALID(int2ap)) {
        cmd_handler(int2ap);
        return;
    }

    if (INT_VALID(int2ap)) {
        /* Make CP happy with our fake responses we are alive ... */
        /* However CP never sends it ... only SEND_R and SEND_F */
        /* Check and receive RES_ACK from CP */
        /* Check and receive REQ_ACK from CP */
        for (i = 0; i < MAX_IPC_DEV; i++) {
            ld->ipc_device[i]->recv_res_ack(int2ap);
            ld->ipc_device[i]->recv_req_ack(int2ap);
        }

        msg_handler();
    }
}

void ShmemLinkDevice::msg_rx_work(void)
{
    ShmemLinkDevice *ld;
    ModemIoDevice *iodev;
    mio_buf *msg;
    osEvent evt;
#if MODEM_LINK_DEVICE_SHMEM_DBG_ON
    int i;
    char mm[96] = "";
#endif

    while (1) {
        ld = pShmemLinkDevice;
        ld->ipc_rx_sem->try_acquire();

        while (!(ld->ipc_rxq.empty())) {
            MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d", __func__, __LINE__);
            evt = ld->ipc_rxq.get(osWaitForever);

            if (evt.status != (osStatus)osEventMessage) {
                continue;
            }

            msg = (mio_buf *)(evt.value.p);
#if MODEM_LINK_DEVICE_SHMEM_DBG_ON
            MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d ch[%d] len[%d]", __func__, __LINE__, msg->ch, msg->len);
            for (i = 0; i < msg->len; i++) {
                sprintf(mm + (i % 16) * 3, "%02X ", msg->data[i]);
                //MODEM_LINK_DEVICE_SHMEM_DBG("%02X ", msg->data[i]);
                if (i % 16 == 15 && i != 0) {
                    MODEM_LINK_DEVICE_SHMEM_DBG("%s", mm);
                }
            }
            if ((i - 1) % 16 != 15) {
                MODEM_LINK_DEVICE_SHMEM_DBG("%s", mm);
            }
            MODEM_LINK_DEVICE_SHMEM_DBG("\n");
#endif
            MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d ch:%d", __func__, __LINE__, msg->ch);
            iodev = getModemIoDeviceById(msg->ch);
            if (iodev != NULL) {
                MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d [%s]", __func__, __LINE__, iodev->name);
                iodev->IoReadCallback(msg);
            } else {
                free_mio_buf(msg);
            }
            MODEM_LINK_DEVICE_SHMEM_DBG("%s:%d", __func__, __LINE__);
        }
    }
}
