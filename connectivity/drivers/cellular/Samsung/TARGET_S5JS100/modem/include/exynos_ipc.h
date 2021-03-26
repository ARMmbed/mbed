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

#ifndef __EXYNOS_IPC_H__
#define __EXYNOS_IPC_H__

#define EXYNOS_SINGLE_MASK          (0b11000000)
#define EXYNOS_MULTI_START_MASK (0b10000000)
#define EXYNOS_MULTI_LAST_MASK      (0b01000000)

#define EXYNOS_START_MASK           0xABCD
#define EXYNOS_HEADER_SIZE      12


enum exynos_ch_id {
    EXYNOS_CH_ID_MULTIPDP = 0,

    EXYNOS_CH_ID_PDP_0 = 1,    /*rmnet0*/
    EXYNOS_CH_ID_PDP_1,
    EXYNOS_CH_ID_PDP_2,
    EXYNOS_CH_ID_PDP_3,
    EXYNOS_CH_ID_PDP_4,

    EXYNOS_CH_ID_FMT_0 = 245,   /*umts_ipc0*/
    EXYNOS_CH_ID_MAX = 255,
    EXYNOS_CH_ID_FLOW_CTRL = 255
};

typedef struct d_hdr {
    unsigned short start;
    unsigned short frm_seq;
    unsigned short frag_cfg;
    unsigned short msg_len;
    unsigned char ch_id;
    unsigned char ch_seq;
    unsigned short dummy;
} __attribute__((packed)) t_hdr;


typedef union {
    char char_hdr[EXYNOS_HEADER_SIZE];
    unsigned int int_hdr[EXYNOS_HEADER_SIZE / sizeof(unsigned int)];
    t_hdr hdr;
} t_msg_header;


static inline unsigned int exynos_calc_padding_size(unsigned int len)
{
    unsigned int residue = len & 0x3;
    return residue ? (4 - residue) : 0;
}


static inline unsigned int exynos_get_total_len(unsigned int len)
{
    unsigned int pad;

    pad = exynos_calc_padding_size(len) ? exynos_calc_padding_size(len) : 0;
    return len + pad;
}


typedef struct {
    int ch;
    int len;
    unsigned char *data;
    t_msg_header header;
    short pad;
    short dalloc;
} mio_buf;

inline mio_buf *alloc_mio_buf(int len)
{
    mio_buf *mio;

    mio = (mio_buf *)malloc(sizeof(mio_buf));
    if (mio == NULL) {
        return NULL;
    }

    if (len != 0) {
        mio->data = (unsigned char *)malloc(len);
        if (mio->data == NULL) {
            free(mio);
            return NULL;
        }
        mio->dalloc = 1;
    } else {
        mio->dalloc = 0;
    }

    return mio;
}

inline void free_mio_buf(mio_buf *mio)
{
    if (mio->dalloc != 0) {
        free(mio->data);
    }
    free(mio);
}

#endif
