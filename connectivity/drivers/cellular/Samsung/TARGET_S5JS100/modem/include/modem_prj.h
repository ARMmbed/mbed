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

#ifndef __MODEM_PRJ_H__
#define __MODEM_PRJ_H__

/* interrupt masks.*/
#define INT_MASK_VALID      0x0080
#define INT_MASK_CMD        0x0040
#define INT_VALID(x)        ((x) & INT_MASK_VALID)
#define INT_CMD_VALID(x)    ((x) & INT_MASK_CMD)
#define INT_NON_CMD(x)      (INT_MASK_VALID | (x))
#define INT_CMD(x)          (INT_MASK_VALID | INT_MASK_CMD | (x))
#define INT_CMD_MASK(x)                 ((x) & 0x1F)

#define STAT_LTE_ACTIVE     0x0020

typedef enum {
    INT_CMD_INIT_START              = 0x1,
    INT_CMD_COLD_BOOT               = 0x2,
    INT_CMD_WARM_BOOT               = 0x3,
    INT_CMD_INIT_END                = 0x2,
    INT_CMD_REQ_ACTIVE              = 0x3,
    INT_CMD_RES_ACTIVE              = 0x4,
    INT_CMD_REQ_TIME_SYNC           = 0x5,
    INT_CMD_REQ_OFF                 = 0x6,
    INT_CMD_CRASH_RESET             = 0x7,
    INT_CMD_PHONE_START             = 0x8,
    INT_CMD_ERR_DISPLAY             = 0x9,
    INT_CMD_CRASH_EXIT              = 0x9,
    INT_CMD_CP_DEEP_SLEEP           = 0xA,
    INT_CMD_NV_REBUILDING           = 0xB,
    INT_CMD_EMER_DOWN               = 0xC,
    INT_CMD_PIF_INIT_DONE           = 0xD,
    INT_CMD_SILENT_NV_REBUILDING    = 0xE,
    INT_CMD_NORMAL_POWER_OFF        = 0xF,
    INT_CMD_MODEM_RELEASE           = 0x10,
    INT_CMD_MAX                     = 0x11,
} MODEM_INT_CMD;

enum direction {
    TX = 0,
    RX,
    MAX_DIR
};


#endif

