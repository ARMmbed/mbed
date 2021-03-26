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

#ifndef __EXYNOS_MBOX_IPC_H__
#define __EXYNOS_MBOX_IPC_H__

#ifndef NUM_OF_MBOX_IPC
#define NUM_OF_MBOX_IPC     1
#endif
#if NUM_OF_MBOX_IPC == 0
#error "wrong NUM_OF_MBOX_IPC"
#endif

class ExynosMboxIpc {
public:
    struct {
        unsigned int mbx_ap2cp_msg;
        unsigned int mbx_cp2ap_msg;
        unsigned int mbx_ap2cp_status;
        unsigned int mbx_cp2ap_status;

        int int_ap2cp_msg;
        int int_ap2cp_active;
        int int_ap2cp_status;

        int irq_cp2ap_msg;
        int irq_cp2ap_active;
        int irq_cp2ap_status;

        int irq_cp2ap_save;
        int irq_cp2ap_stop_req;

        int save_irq;
        int stop_irq;
    } mbox;

public:
    void mbox_init(void);
    void mbox_deinit(void);
    void mbox_sw_reset(void);
    void mbox_update_value(unsigned int mbx_num, unsigned int msg, unsigned int mask, unsigned int pos);
    unsigned int mbox_extract_value(unsigned int mbx_num, unsigned int mask, unsigned int pos);
    void mbox_set_value(unsigned int mbx_num, unsigned int msg);
    unsigned int mbox_get_value(unsigned int mbx_num);
    void mbox_ipc_send_command(unsigned int int_num, unsigned short cmd);
    int mbox_ipc_unregister_handler(unsigned int int_num);
    int mbox_ipc_register_handler(unsigned int int_num, void (*handler)(void *), void *data);
    void mbox_set_interrupt(unsigned int int_num);
    int mbox_attach_isr();
    int mbox_detach_isr();

public:
    ExynosMboxIpc();
    ~ExynosMboxIpc();
};

#endif

