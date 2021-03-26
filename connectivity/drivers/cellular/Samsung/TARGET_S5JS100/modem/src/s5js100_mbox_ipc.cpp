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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "rtos/rtos.h"
#include "s5js100_mbox_ipc.h"
#include "s5js100_type.h"
#include "s5js100_error.h"
#include "shmem_save.h"
#include "s5js100_pwr.h"
#include "s5js100.h"

#define S5JS100_MBOX_IPC_DBG_ON     0
#define S5JS100_MBOX_IPC_DBG        if (S5JS100_MBOX_IPC_DBG_ON) printf

/***************************************************************/
/* MCU_IPC Registers part               */
/***************************************************************/
#define S5JS100_MAILBOX_BASE            (0x85023000)
#define EXYNOS_MCU_IPC_MCUCTLR          ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0000))
#define EXYNOS_MCU_IPC_INTGR0           ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0008))
#define EXYNOS_MCU_IPC_INTCR0           ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x000c))
#define EXYNOS_MCU_IPC_INTMR0           ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0010))
#define EXYNOS_MCU_IPC_INTSR0           ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0014))
#define EXYNOS_MCU_IPC_INTMSR0          ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0018))
#define EXYNOS_MCU_IPC_INTGR1           ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x001c))
#define EXYNOS_MCU_IPC_INTCR1           ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0020))
#define EXYNOS_MCU_IPC_INTMR1           ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0024))
#define EXYNOS_MCU_IPC_INTSR1           ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0028))
#define EXYNOS_MCU_IPC_INTMSR1          ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x002c))
#define EXYNOS_MCU_IPC_ISSR0            ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0080))
#define EXYNOS_MCU_IPC_ISSR1            ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0084))
#define EXYNOS_MCU_IPC_ISSR2            ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x0088))
#define EXYNOS_MCU_IPC_ISSR3            ((volatile unsigned int *)(S5JS100_MAILBOX_BASE + 0x008c))

/***************************************************************/
/* MCU_IPC Bit definition part                  */
/***************************************************************/
/* SYSREG Bit definition */
#define MCU_IPC_MCUCTLR_MSWRST  (0)     /* MCUCTRL S/W Reset */

#define MCU_IPC_RX_INT0     (1 << 16)
#define MCU_IPC_RX_INT1     (1 << 17)
#define MCU_IPC_RX_INT2     (1 << 18)
#define MCU_IPC_RX_INT3     (1 << 19)
#define MCU_IPC_RX_INT4     (1 << 20)
#define MCU_IPC_RX_INT5     (1 << 21)
#define MCU_IPC_RX_INT6     (1 << 22)
#define MCU_IPC_RX_INT7     (1 << 23)
#define MCU_IPC_RX_INT8     (1 << 24)
#define MCU_IPC_RX_INT9     (1 << 25)
#define MCU_IPC_RX_INT10    (1 << 26)
#define MCU_IPC_RX_INT11    (1 << 27)
#define MCU_IPC_RX_INT12    (1 << 28)
#define MCU_IPC_RX_INT13    (1 << 29)
#define MCU_IPC_RX_INT14    (1 << 30)
#define MCU_IPC_RX_INT15    (1 << 31)

#define MIF_INT_AP2CP_MSG       0
#define MIF_INT_AP2CP_WAKEUP        1
#define MIF_INT_AP2CP_STATUS        2
#define MIF_INT_AP2CP_ACTIVE        3
#define MIF_INT_AP2CP_DCXO      12

#define MIF_IRQ_CP2AP_MSG       0
#define MIF_IRQ_CP2AP_WAKEUP        1
#define MIF_IRQ_CP2AP_STATUS        2
#define MIF_IRQ_CP2AP_LTE_ACTIVE    4
#define MIF_IRQ_CP2AP_WAKELOCK      8
#define MIF_IRQ_CP2AP_SAVE      11
#define MIF_IRQ_CP2AP_DCXO      12
#define MIF_IRQ_CP2AP_STOP_REQ      13

#define MIF_MBX_AP2CP_MSG       0
#define MIF_MBX_CP2AP_MSG       1
#define MIF_MBX_AP2CP_STATUS        2
#define MIF_MBX_CP2AP_STATUS        3
//#define MIF_MBX_DCXO_MSG      4
#define MIF_MBX_BOOT_MSG        4
#define MIF_MBX_CSADDR_MSG      5
#define MIF_MBX_NVADDR_MSG      6
#define MIF_MBX_CALADDR_MSG     7


#define MAX_MBOX_NUM    64
#define S5JS100_IRQ_MAILBOX_AP_INT      ((IRQn_Type )31)


struct ipc_cp_hd {
    void *data;
    void (*handler)(void *);
} mbox_ipc_cp_hd[16];

static void mbox_ipc_cp_clear_all_interrupt(void)
{
    putreg32(0xFFFF, EXYNOS_MCU_IPC_INTCR1);
}

static void mbox_ipc_cp_clear_all_boxes(void)
{
    unsigned long long i;

    for (i = 0; i < 8; i++) {
        putreg32(0, EXYNOS_MCU_IPC_ISSR0 + i);
    }
}

static void mbox_ipc_cp_handler(int32_t irq, void *context, void *arg)
{
    unsigned int irq_stat, i;

    irq_stat = getreg32(EXYNOS_MCU_IPC_INTSR0) & 0xFFFF0000;

    /* Interrupt Clear */
    putreg32(irq_stat, EXYNOS_MCU_IPC_INTCR0);
    S5JS100_MBOX_IPC_DBG("%s irq_stat=0x%08X\n", __func__, irq_stat);
    irq_stat = irq_stat >> 16;

    for (i = 0; i < 16 && irq_stat; i++) {
        if (irq_stat & (1 << i)) {
            if (mbox_ipc_cp_hd[i].handler != NULL) {
                mbox_ipc_cp_hd[i].handler(mbox_ipc_cp_hd[i].data);
            }
            irq_stat &= ~(1 << (i + 16));
        }
    }
    NVIC_ClearPendingIRQ(S5JS100_IRQ_MAILBOX_AP_INT);
}

extern "C" {
    extern char *get_env(const char *name);
}

void ExynosMboxIpc::mbox_init(void)
{
    unsigned int cs, nv, cal;
    char *epco;

    shmem_get_data(&cs, &nv, &cal);

    /* Request IRQ */
    NVIC_SetVector(S5JS100_IRQ_MAILBOX_AP_INT, (uint32_t)mbox_ipc_cp_handler);
#if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    SCB_InvalidateICache();
#endif

    mbox_ipc_cp_clear_all_interrupt();
    mbox_ipc_cp_clear_all_boxes();

    if (get_bootflag()) {
        mbox_set_value(MIF_MBX_BOOT_MSG, 0);
    } else {
        mbox_set_value(MIF_MBX_BOOT_MSG, 1);
    }

    mbox_set_value(MIF_MBX_CSADDR_MSG, cs);
    mbox_set_value(MIF_MBX_NVADDR_MSG, nv);
    mbox_set_value(MIF_MBX_CALADDR_MSG, cal);

    epco = get_env("ePCO");
    if (epco != NULL && epco[0] == '0') {
        modifyreg32(0x85023088, 0x00000800, 0x00000800);
    } else {
        modifyreg32(0x85023088, 0x00000800, 0x00000000);
    }

    NVIC_EnableIRQ(S5JS100_IRQ_MAILBOX_AP_INT);

}

void ExynosMboxIpc::mbox_deinit(void)
{
    NVIC_DisableIRQ(S5JS100_IRQ_MAILBOX_AP_INT);
}

void ExynosMboxIpc::mbox_sw_reset(void)
{
    unsigned int reg_val;

    reg_val = getreg32(EXYNOS_MCU_IPC_MCUCTLR);
    reg_val |= (0x1 << MCU_IPC_MCUCTLR_MSWRST);

    putreg32(reg_val, EXYNOS_MCU_IPC_MCUCTLR) ;

    while (getreg32(EXYNOS_MCU_IPC_MCUCTLR) & (0x1 << MCU_IPC_MCUCTLR_MSWRST));
}

void ExynosMboxIpc::mbox_update_value(unsigned int mbx_num, unsigned int msg, unsigned int mask, unsigned int pos)
{
    unsigned int val;

    if (mbx_num < MAX_MBOX_NUM) {
        val = mbox_get_value(mbx_num);
        val &= ~(mask << pos);
        val |= (msg & mask) << pos;
        mbox_set_value(mbx_num, val);
    }
}

unsigned int ExynosMboxIpc::mbox_extract_value(unsigned int mbx_num, unsigned int mask, unsigned int pos)
{
    if (mbx_num < MAX_MBOX_NUM) {
        return (mbox_get_value(mbx_num) >> pos) & mask;
    } else {
        return 0;
    }
}

void ExynosMboxIpc::mbox_set_value(unsigned int mbx_num, unsigned int msg)
{
    if (mbx_num < MAX_MBOX_NUM) {
        putreg32(msg, EXYNOS_MCU_IPC_ISSR0 + mbx_num);
    }
}

unsigned int ExynosMboxIpc::mbox_get_value(unsigned int mbx_num)
{
    if (mbx_num < MAX_MBOX_NUM) {
        return getreg32(EXYNOS_MCU_IPC_ISSR0 + mbx_num);
    } else {
        return 0;
    }
}

void ExynosMboxIpc::mbox_set_interrupt(unsigned int int_num)
{
    /* generate interrupt */
    if (int_num < 16) {
        putreg32(0x1 << int_num, EXYNOS_MCU_IPC_INTGR1);
    }
}

void ExynosMboxIpc::mbox_ipc_send_command(unsigned int int_num, unsigned short cmd)
{
    /* write command */
    if (int_num < 16) {
        putreg32(cmd, EXYNOS_MCU_IPC_ISSR0 + (2 * int_num));
    }

    /* generate interrupt */
    mbox_set_interrupt(int_num);
}

int ExynosMboxIpc::mbox_ipc_unregister_handler(unsigned int int_num)
{
    mbox_ipc_cp_hd[int_num].data = NULL;
    mbox_ipc_cp_hd[int_num].handler = NULL;

    return 0;
}

int ExynosMboxIpc::mbox_ipc_register_handler(unsigned int int_num, void (*handler)(void *), void *data)
{
    if ((!handler) || (int_num > 15)) {
        return_error(-EINVAL);
    }

    mbox_ipc_cp_hd[int_num].data = data;
    mbox_ipc_cp_hd[int_num].handler = handler;

    return 0;
}

int ExynosMboxIpc::mbox_attach_isr()
{
    /* Request IRQ */
    NVIC_SetVector(S5JS100_IRQ_MAILBOX_AP_INT, (uint32_t)mbox_ipc_cp_handler);
#if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    SCB_InvalidateICache();
#endif

    mbox_ipc_cp_clear_all_interrupt();
    mbox_ipc_cp_clear_all_boxes();

    NVIC_EnableIRQ(S5JS100_IRQ_MAILBOX_AP_INT);

    return 0;
}

int ExynosMboxIpc::mbox_detach_isr()
{
    NVIC_DisableIRQ(S5JS100_IRQ_MAILBOX_AP_INT);

    return 0;
}

ExynosMboxIpc::ExynosMboxIpc()
{
    mbox.int_ap2cp_msg = MIF_INT_AP2CP_MSG;
    mbox.int_ap2cp_status = MIF_INT_AP2CP_STATUS;
    mbox.int_ap2cp_active = MIF_INT_AP2CP_ACTIVE;

    mbox.irq_cp2ap_msg = MIF_IRQ_CP2AP_MSG;
    mbox.irq_cp2ap_status = MIF_IRQ_CP2AP_STATUS;
    mbox.irq_cp2ap_active = MIF_IRQ_CP2AP_LTE_ACTIVE;

    mbox.mbx_ap2cp_msg = MIF_MBX_AP2CP_MSG;
    mbox.mbx_cp2ap_msg = MIF_MBX_CP2AP_MSG;
    mbox.mbx_ap2cp_status = MIF_MBX_AP2CP_STATUS;
    mbox.mbx_cp2ap_status = MIF_MBX_CP2AP_STATUS;

    mbox.irq_cp2ap_save = MIF_IRQ_CP2AP_SAVE;
    mbox.irq_cp2ap_stop_req = MIF_IRQ_CP2AP_STOP_REQ;

    mbox.save_irq = MIF_IRQ_CP2AP_SAVE;
    mbox.stop_irq = MIF_IRQ_CP2AP_STOP_REQ;
}

ExynosMboxIpc::~ExynosMboxIpc()
{
}

